#!/usr/bin/env python3
"""Verify the recorded SIM refactor checkpoint without changing the worktree.

Exit codes: 0 verified; 2 missing/invalid state or checkpoint file;
3 unavailable Git repository/remote/ref; 4 ancestry/content mismatch;
5 an explicitly requested fetch failed. This never runs a business test.
"""

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
from urllib.parse import urlsplit, urlunsplit


STATE_PATH = "telephony_core_service/docs/SIM_Refactor_Execution_State.md"
MARKER = "<!-- sim-refactor-recovery-v1 -->"
SHA = re.compile(r"[0-9a-f]{40}\Z")
SHA256 = re.compile(r"[0-9a-f]{64}\Z")


class RecoveryError(Exception):
    def __init__(self, exit_code, code, detail):
        super().__init__(detail)
        self.exit_code = exit_code
        self.code = code


def redact_url(value):
    """Report a remote address without accidentally printing URL credentials."""
    if "://" not in value:
        return value
    parsed = urlsplit(value)
    if "@" not in parsed.netloc:
        return value
    host = parsed.netloc.rsplit("@", 1)[1]
    return urlunsplit((parsed.scheme, "[redacted]@" + host, parsed.path, parsed.query, parsed.fragment))


def git(repo, *arguments):
    try:
        return subprocess.run(
            ["git", "--no-optional-locks", "-C", str(repo), *arguments],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
        )
    except FileNotFoundError as error:
        raise RecoveryError(3, "git_missing", "Git is required to verify the checkpoint") from error


def git_text(repo, *arguments, error_code="git_unavailable"):
    command = git(repo, *arguments)
    if command.returncode:
        raise RecoveryError(3, error_code, "Git command failed: " + " ".join(arguments))
    return command.stdout.decode("utf-8", errors="replace").strip()


def repository_root():
    # The checkout can live anywhere; no previous /tmp worktree is consulted.
    return Path(git_text(Path(__file__).resolve().parent, "rev-parse", "--show-toplevel"))


def repository_path(value, field):
    if not isinstance(value, str) or not value or "\\" in value:
        raise RecoveryError(2, "invalid_repository_path", field + " must be a repository-relative POSIX path")
    path = PurePosixPath(value)
    if path.is_absolute() or ".." in path.parts or value.startswith("-"):
        raise RecoveryError(2, "invalid_repository_path", field + " must stay inside the repository")
    return value


def load_state(repo):
    path = repo / STATE_PATH
    if not path.is_file():
        raise RecoveryError(2, "execution_state_missing", STATE_PATH + " is absent from this checkout")
    content = path.read_text(encoding="utf-8")
    if content.count(MARKER) != 1:
        raise RecoveryError(2, "state_marker_invalid", "Expected exactly one " + MARKER)
    section = content.split(MARKER, 1)[1]
    block = re.match(r"\s*```json\s*\n(.*?)\n```", section, flags=re.DOTALL)
    if not block:
        raise RecoveryError(2, "state_json_missing", "The marker must be followed by a fenced JSON block")
    try:
        state = json.loads(block.group(1))
    except (ValueError, TypeError) as error:
        raise RecoveryError(2, "state_json_invalid", str(error)) from error
    if not isinstance(state, dict) or type(state.get("schema_version")) is not int or state["schema_version"] != 1:
        raise RecoveryError(2, "state_schema_invalid", "Expected schema_version=1")
    for field in ("main_baseline", "g2a_base", "g2a_commit"):
        if not isinstance(state.get(field), str) or not SHA.fullmatch(state[field]):
            raise RecoveryError(2, "state_commit_invalid", field + " must be a full, lowercase commit SHA")
    branch = state.get("g2a_branch")
    if not isinstance(branch, str) or not branch:
        raise RecoveryError(2, "state_branch_missing", "g2a_branch must name the remote work branch")
    state["evidence_path"] = repository_path(state.get("evidence_path"), "evidence_path")
    required = state.get("required_code_paths")
    if not isinstance(required, list) or not required:
        raise RecoveryError(2, "state_code_paths_missing", "required_code_paths must be a nonempty list")
    state["required_code_paths"] = [repository_path(item, "required_code_paths") for item in required]
    if len(set(state["required_code_paths"])) != len(state["required_code_paths"]):
        raise RecoveryError(2, "state_code_paths_duplicate", "required_code_paths contains duplicates")
    return state


def resolve_commit(repo, reference):
    return git_text(repo, "rev-parse", "--verify", reference + "^{commit}", error_code="commit_or_ref_missing")


def contains(repo, ancestor, descendant):
    command = git(repo, "merge-base", "--is-ancestor", ancestor, descendant)
    if command.returncode not in (0, 1):
        raise RecoveryError(3, "ancestry_unavailable", "Cannot compare the recorded commits")
    return command.returncode == 0


def checkpoint_bytes(repo, reference, path):
    command = git(repo, "show", reference + ":" + path)
    if command.returncode:
        raise RecoveryError(2, "checkpoint_file_missing", path + " is absent at " + reference)
    return command.stdout


def verify_evidence(repo, state, branch_ref):
    commit = state["g2a_commit"]
    missing_tip_paths = []
    for path in state["required_code_paths"]:
        checkpoint_bytes(repo, commit, path)
        if git(repo, "show", branch_ref + ":" + path).returncode:
            missing_tip_paths.append(path)
    raw = checkpoint_bytes(repo, commit, state["evidence_path"])
    try:
        evidence = json.loads(raw)
    except (ValueError, TypeError) as error:
        raise RecoveryError(2, "evidence_json_invalid", str(error)) from error
    if not isinstance(evidence, dict):
        raise RecoveryError(2, "evidence_schema_invalid", "Checkpoint evidence must be a JSON object")
    sources = evidence.get("production_sources")
    if not isinstance(sources, list) or len(sources) != 5:
        raise RecoveryError(2, "evidence_sources_invalid", "This checkpoint must record exactly five production_sources")
    verified = []
    tip_sources_match = True
    seen = set()
    for item in sources:
        if not isinstance(item, dict):
            raise RecoveryError(2, "evidence_source_invalid", "Each production source needs path and sha256")
        path = repository_path(item.get("path"), "production_sources.path")
        # Result runners record paths relative to the Core component. The saved
        # evidence may instead use full repository-relative paths.
        repo_path = path if path.startswith("telephony_core_service/") else "telephony_core_service/" + path
        expected = item.get("sha256")
        if repo_path in seen or not isinstance(expected, str) or not SHA256.fullmatch(expected):
            raise RecoveryError(2, "evidence_source_invalid", "Duplicate source or invalid sha256: " + repo_path)
        seen.add(repo_path)
        actual = hashlib.sha256(checkpoint_bytes(repo, commit, repo_path)).hexdigest()
        if actual != expected:
            raise RecoveryError(4, "source_digest_mismatch", repo_path + " differs from the saved checkpoint evidence")
        tip_source = git(repo, "show", branch_ref + ":" + repo_path)
        if tip_source.returncode or hashlib.sha256(tip_source.stdout).hexdigest() != expected:
            tip_sources_match = False
        verified.append(repo_path)
    # Later branch work must remain recoverable, while its tests cannot silently
    # inherit the older checkpoint's results.
    tip_evidence = git(repo, "show", branch_ref + ":" + state["evidence_path"])
    evidence_matches = not tip_evidence.returncode and tip_evidence.stdout == raw
    changed = git(repo, "diff", "--name-only", "-z", commit, branch_ref, "--",
                  "telephony_core_service/services", "telephony_core_service/interfaces",
                  "telephony_core_service/BUILD.gn", "telephony_core_service/test")
    if changed.returncode:
        raise RecoveryError(3, "checkpoint_diff_unavailable", "Cannot compare checkpoint code and validation inputs with the branch tip")
    changed_paths = [path for path in changed.stdout.decode("utf-8", errors="replace").split("\0") if path]
    return {
        "verified_production_sources": verified,
        "branch_tip_sources_match_checkpoint": tip_sources_match,
        "branch_tip_evidence_matches_checkpoint": evidence_matches,
        "branch_tip_missing_code_paths": missing_tip_paths,
        "changed_code_or_validation_paths": changed_paths,
        "fresh_validation_required": not tip_sources_match or not evidence_matches or bool(missing_tip_paths) or bool(changed_paths),
    }


def check(args, result):
    repo = repository_root()
    result["repository_root"] = str(repo)
    result["head"] = resolve_commit(repo, "HEAD")
    branch = git(repo, "symbolic-ref", "--short", "-q", "HEAD")
    result["current_branch"] = branch.stdout.decode().strip() if branch.returncode == 0 else None
    status = git_text(repo, "status", "--porcelain=v1", "--untracked-files=all")
    result["dirty"] = bool(status)
    result["worktree_change_count"] = len(status.splitlines()) if status else 0
    state = load_state(repo)
    remote_url = git_text(repo, "remote", "get-url", args.remote, error_code="remote_missing")
    result["remote"] = args.remote
    result["remote_url"] = redact_url(remote_url)
    main_ref = "refs/remotes/" + args.remote + "/main"
    work_ref = "refs/remotes/" + args.remote + "/" + state["g2a_branch"]
    for reference in (main_ref, work_ref):
        if git(repo, "check-ref-format", reference).returncode:
            raise RecoveryError(2, "remote_ref_invalid", "Invalid remote or work branch name")
    if args.fetch:
        # Explicit refspecs also support a fresh --single-branch clone. No
        # checkout/reset/stash or source-file operation is performed.
        fetched = git(repo, "fetch", "--no-tags", args.remote,
                      "refs/heads/main:" + main_ref,
                      "refs/heads/" + state["g2a_branch"] + ":" + work_ref)
        if fetched.returncode:
            raise RecoveryError(5, "fetch_failed", "Explicit checkpoint fetch failed; check network, access and branch existence")
        result["remote_refs_refreshed"] = True
    main = resolve_commit(repo, main_ref)
    work = resolve_commit(repo, work_ref)
    for field in ("main_baseline", "g2a_base", "g2a_commit"):
        resolve_commit(repo, state[field])
    if not contains(repo, state["main_baseline"], main):
        raise RecoveryError(4, "main_baseline_not_reachable", "Remote-tracking main does not contain the recorded baseline")
    for field in ("g2a_base", "g2a_commit"):
        if not contains(repo, state[field], work):
            raise RecoveryError(4, "checkpoint_not_reachable", "Remote-tracking work branch does not contain " + field)
    if not contains(repo, state["g2a_base"], state["g2a_commit"]):
        raise RecoveryError(4, "checkpoint_base_mismatch", "The recorded G2a commit does not contain its base")
    result["main_head"] = main
    result["g2a_branch"] = state["g2a_branch"]
    result["g2a_branch_head"] = work
    result["g2a_commit"] = state["g2a_commit"]
    result["main_contains_g2a"] = contains(repo, state["g2a_commit"], main)
    result["current_checkout_contains_main_baseline"] = contains(repo, state["main_baseline"], result["head"])
    result["current_checkout_contains_g2a_checkpoint"] = contains(repo, state["g2a_commit"], result["head"])
    result.update(verify_evidence(repo, state, work_ref))
    result["required_code_paths_verified"] = len(state["required_code_paths"])
    result["evidence_path"] = state["evidence_path"]
    result["status"] = "CHECKPOINT_VERIFIED"
    result["notes"] = [
        "Git reachability and saved source digests were verified; no current runtime test was executed.",
        "main_contains_g2a=false is normal while product acceptance keeps the code on its work branch.",
    ]
    if not args.fetch:
        result["notes"].append("Remote-tracking refs are cached; use --fetch to request a fresh remote check.")
    if result["fresh_validation_required"]:
        result["notes"].append("The branch tip differs from the recorded checkpoint; validate the tip before reusing historical results, or recover the pinned commit.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--remote", default="origin", help="existing Git remote (default: origin)")
    parser.add_argument("--fetch", action="store_true", help="explicitly refresh main and checkpoint tracking refs")
    args = parser.parse_args()
    result = {"schema_version": 1, "status": "ERROR", "remote_refs_refreshed": False,
              "current_runtime_test_executed": False}
    exit_code = 0
    try:
        check(args, result)
    except RecoveryError as error:
        exit_code = error.exit_code
        result["error"] = {"code": error.code, "detail": str(error)}
    except (OSError, ValueError, UnicodeError) as error:
        exit_code = 2
        result["error"] = {"code": "local_read_failure", "detail": str(error)}
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
