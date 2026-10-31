#!/usr/bin/env python3
"""Check the closed multisim skeleton's textual includes and GN source boundaries.

This is deliberately a source/target check, not a C++ semantic or production
write-ownership proof. All conditional includes are checked conservatively.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "services/multisim"
SUFFIXES = {".h", ".hpp", ".cpp", ".cc", ".cxx"}
PURE_TYPES = {"topology/api/account_types.h", "assignment/api/assignment_types.h",
              "assignment/api/primary_execution.h"}
CAPABILITY_CONTRACTS = {"runtime/state/pin_requests.h", "runtime/records/ef_requests.h",
                        "runtime/euicc/profile_requests.h"}
PROTOCOLS = {
    "topology": {"discovery_observation_ledger", "activation_progress", "show_number_plan"},
    "assignment": {"primary_switch_plan", "account_role_protocol"},
}
CONTRACTS = {"topology_contracts", "assignment_contracts", "runtime_contracts"}
TYPES = {"topology_types", "assignment_types"}
VALUES = {"sim_values", "request_protocol"}


def category(name):
    path = Path(name)
    owner = path.parts[0]
    if name in PURE_TYPES:
        return owner + "_types"
    if name in CAPABILITY_CONTRACTS or name.startswith("runtime/api/"):
        return "runtime_contracts"
    if owner == "foundation":
        if name.endswith("runtime_operation_store.h"):
            return "request_mechanism"
        return "request_protocol" if "orchestration" in path.parts else "sim_values"
    if owner in ("topology", "assignment"):
        if "api" in path.parts:
            return owner + "_contracts"
        if "domain" in path.parts or (owner == "topology" and "accounts" in path.parts):
            return owner + "_domain"
        if "adapters" in path.parts:
            return owner + "_adapters"
        return owner + ("_protocols" if path.stem in PROTOCOLS[owner] else "_cases")
    if owner == "runtime":
        if path.parts[1] in ("session", "resource", "records"):
            return "runtime_mechanisms"
        return "runtime_cases" if path.parts[1] == "euicc" else "runtime_core"
    if owner == "compatibility":
        return "compatibility_contracts" if "api" in path.parts else "compatibility_core"
    if owner == "assembly":
        return "assembly_core" if path.name in ("sim_module.h", "sim_module_factory.cpp") else "assembly_contracts"
    if owner == "include":
        return "repository_exports"
    raise ValueError("Source outside the implemented target layout: " + name)


def allowed_edge(user, provider):
    if user == provider:
        return True
    if user == "sim_values":
        return False
    if user == "request_protocol":
        return provider == "sim_values"
    if user == "request_mechanism":
        return provider in VALUES
    if user in TYPES:
        return provider in TYPES | {"sim_values"}
    if user in CONTRACTS:
        return provider in CONTRACTS | TYPES | VALUES
    if user in {"compatibility_contracts", "assembly_contracts"}:
        return provider in CONTRACTS | TYPES | VALUES | {"compatibility_contracts"}
    if user.endswith("_domain"):
        return provider in {"sim_values", user.replace("_domain", "_types")}
    if user.startswith(("topology_", "assignment_")):
        owner = user.split("_", 1)[0]
        common = CONTRACTS | TYPES | VALUES
        if user.endswith("_adapters"):
            return provider in common
        return provider in common | {"request_mechanism", owner + "_domain", owner + "_protocols"}
    if user in {"runtime_core", "runtime_cases", "runtime_mechanisms"}:
        return provider in CONTRACTS | TYPES | VALUES | {
            "request_mechanism", "runtime_mechanisms", "runtime_cases"}
    if user == "compatibility_core":
        return provider in CONTRACTS | TYPES | VALUES | {"compatibility_contracts"}
    if user == "repository_exports":
        return provider == "assembly_contracts"
    return user == "assembly_core"


def without_comments(text):
    # Keep strings intact; comment-looking bytes inside a literal are not comments.
    return re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/',
                  lambda m: "" if m[0].startswith(("//", "/*")) else m[0], text)


def reject_cycles(graph, kind):
    active, done, chain = set(), set(), []

    def visit(node):
        if node in active:
            raise ValueError(kind + " cycle: " + " -> ".join(chain[chain.index(node):] + [node]))
        if node in done:
            return
        active.add(node)
        chain.append(node)
        for dependency in sorted(graph.get(node, ())):
            visit(dependency)
        chain.pop()
        active.remove(node)
        done.add(node)

    for node in sorted(graph):
        visit(node)


def gn_targets(source):
    # BUILD.gn intentionally uses literal lists, so this small checker stays auditable.
    build = without_comments((source / "BUILD.gn").read_text())
    build = re.sub(r"(?m)#.*$", "", build)
    if re.search(r"\b(?:sources|public|deps|public_deps)\s*[+-]=", build):
        raise ValueError("Unsupported dynamic GN list mutation; keep literal boundary lists")
    result = {}
    blocks = re.findall(r'source_set\("(\w+)"\)\s*\{([^{}]*)\}', build)
    if len(blocks) != len(re.findall(r'\bsource_set\s*\(', build)):
        raise ValueError("Unsupported non-literal GN source set")
    for name, body in blocks:
        lists = {}
        for field in ("sources", "public", "deps", "public_deps"):
            assignments = re.findall(r"\b" + field + r"\s*=", body)
            values = re.findall(r"\b" + field + r"\s*=\s*\[([^]]*)\]", body)
            if len(assignments) > 1 or len(values) != len(assignments):
                raise ValueError("Unsupported non-literal/duplicate GN list: " + name + ": " + field)
            content = values[0] if values else ""
            if re.sub(r'"[^"$]*"|[\s,]', "", content):
                raise ValueError("Unsupported GN list expression: " + name + ": " + field)
            lists[field] = re.findall(r'"([^"]+)"', content)
        if name in result or not re.search(r"\bcheck_includes\s*=\s*true\b", body):
            raise ValueError("Duplicate target or missing check_includes: " + name)
        if not re.search(r"\btestonly\s*=\s*true\b", body):
            raise ValueError("Skeleton targets must remain testonly: " + name)
        result[name] = lists
    if not result:
        raise ValueError("No literal skeleton source sets found")
    groups = re.findall(r'group\("skeleton"\)\s*\{([^{}]*)\}', build)
    dependencies = re.findall(r'\bdeps\s*=\s*\[([^]]*)\]', groups[0]) if len(groups) == 1 else []
    if len(dependencies) != 1 or set(re.findall(r'"([^"]+)"', dependencies[0])) != {":" + t for t in result}:
        raise ValueError("GN skeleton group must cover every source set exactly")
    return result


def check(source=SOURCE):
    files = {p.relative_to(source).as_posix(): p for p in source.rglob("*") if p.suffix in SUFFIXES}
    targets = gn_targets(source)
    membership = Counter()
    target_graph = {}
    for target, data in targets.items():
        for name in data["sources"]:
            if name not in files or category(name) != target:
                raise ValueError(f"GN source misclassified or missing: {target}: {name}")
            membership[name] += 1
        if set(data["public"]) - set(data["sources"]):
            raise ValueError("GN public header outside its source set: " + target)
        target_graph[target] = set()
        for dep in data["deps"] + data["public_deps"]:
            if not dep.startswith(":") or dep[1:] not in targets or not allowed_edge(target, dep[1:]):
                raise ValueError(f"Forbidden GN dependency: {target} -> {dep}")
            target_graph[target].add(dep[1:])
    if set(files) != set(membership) or any(count != 1 for count in membership.values()):
        raise ValueError("Each C++ source/header must belong to exactly one GN target")
    reject_cycles(target_graph, "GN target")

    def dependency_visible(user, provider):
        pending = list(target_graph[user])
        visited = set()
        while pending:
            target = pending.pop()
            if target == provider:
                return True
            if target not in visited:
                visited.add(target)
                pending.extend(dep[1:] for dep in targets[target]["public_deps"])
        return False

    graph = {}
    all_code = []
    for name, path in sorted(files.items()):
        # C++ removes line splices before recognizing comments and directives.
        code = without_comments(re.sub(r"\\\r?\n", "", path.read_text()))
        all_code.append(code)
        graph[name] = set()
        for directive in re.findall(r'^\s*#\s*include\b([^\n]*)', code, re.M):
            literal = re.fullmatch(r'\s*(["<])([^">]+)[">]\s*', directive)
            if not literal:
                raise ValueError(f"Unsupported non-literal include: {name}: {directive.strip()}")
            delimiter, include = literal.groups()
            if delimiter == "<":
                # The closed skeleton has only standard-library angle includes.
                if include not in {"algorithm", "array", "cassert", "cstddef", "cstdint", "functional",
                                   "limits", "map", "memory", "optional", "set", "string", "tuple",
                                   "utility", "variant", "vector"}:
                    raise ValueError(f"Unverified platform/angle include: {name}: {include}")
                continue
            candidates = [(path.parent / include).resolve(), (source / include).resolve()]
            resolved = next((p for p in candidates if p.is_file()), None)
            if resolved is None or not resolved.is_relative_to(source.resolve()):
                raise ValueError(f"Missing or out-of-scope quoted include: {name}: {include}")
            provider = resolved.relative_to(source.resolve()).as_posix()
            if provider not in files or not allowed_edge(category(name), category(provider)):
                raise ValueError(f"Forbidden include: {name} -> {provider}")
            user_target, provider_target = category(name), category(provider)
            if user_target != provider_target:
                if provider not in targets[provider_target]["public"]:
                    raise ValueError(f"Non-public GN header: {name} -> {provider}")
                if not dependency_visible(user_target, provider_target):
                    raise ValueError(f"GN dependency missing for include: {name} -> {provider}")
            graph[name].add(provider)
    reject_cycles(graph, "include")
    code = "\n".join(all_code)
    for symbol in ("RequestId", "StepRef", "DefaultRole", "AccountFieldRevisions", "SubscriptionId", "SlotId",
                   "RuntimeOperationStore", "AccountRegistry", "AssignmentAggregate", "TopologyService",
                   "AssignmentService", "CardRuntime", "SessionRegistry", "ResourceBarrier", "LoadSession",
                   "LocalCardRequests"):
        definitions = re.findall(r'\b(?:enum\s+class|class|struct)\s+' + symbol + r'\b[^;{]*\{', code)
        if len(definitions) != 1:
            raise ValueError(f"Expected one definition of {symbol}, found {len(definitions)}")
    return {"files": len(files), "includes": sum(map(len, graph.values())), "targets": len(targets)}


def token_sequence(text):
    code = re.sub(r"(?m)^\s*#.*$", "", without_comments(text))
    return re.findall(r'"(?:\\.|[^"\\])*"|\w+|[^\s\w]', code)


def type_definitions(text):
    tokens = token_sequence(text)
    definitions = Counter()
    for index, token in enumerate(tokens[:-1]):
        if token not in ("class", "struct") or not re.fullmatch(r"\w+", tokens[index + 1]):
            continue
        cursor = index + 2
        while cursor < len(tokens) and tokens[cursor] not in ("{", ";"):
            cursor += 1
        if cursor == len(tokens) or tokens[cursor] != "{":
            continue  # Forward declaration; it has no layout to preserve.
        depth, end = 1, cursor + 1
        while depth and end < len(tokens):
            depth += (tokens[end] == "{") - (tokens[end] == "}")
            end += 1
        start = index - 1 if index and tokens[index - 1] == "enum" else index
        definitions[tuple(tokens[start:end])] += 1
    return definitions


def check_migration():
    manifest = json.loads((Path(__file__).parent / "source_migration.json").read_text())
    repo = ROOT.parent
    baseline, prefix = manifest["baseline"], manifest["source_root"] + "/"
    original = subprocess.check_output(["git", "ls-tree", "-rz", "--name-only", baseline, "--", prefix], cwd=repo)
    paths = {p.decode()[len(prefix):] for p in original.split(b"\0") if p}
    if paths != {entry["old"] for entry in manifest["files"]}:
        raise ValueError("Migration map does not cover exactly the baseline files")
    for entry in manifest["files"]:
        before = subprocess.check_output(["git", "show", baseline + ":" + prefix + entry["old"]], cwd=repo)
        if hashlib.sha256(before).hexdigest() != entry["sha256"]:
            raise ValueError("Baseline digest mismatch: " + entry["old"])
        text = before.decode()
        # The only intentional existing call-expression changes expose opaque API entries.
        if entry["old"] == "compatibility/legacy_sim_facade.cpp":
            text = text.replace("assignment_.StartPrimarySwitch(issued)",
                                "StartLegacyPrimarySwitch(assignment_, issued)")
            text = text.replace("activation_.StartOrdinaryActivation(issued)",
                                "StartLegacyOrdinaryActivation(activation_, issued)")
        after = "\n".join((SOURCE / name).read_text() for name in entry["destinations"])
        missing = Counter(token_sequence(text)) - Counter(token_sequence(after))
        if missing:
            raise ValueError(f"Original code tokens missing from {entry['old']}: {dict(missing)}")
        if type_definitions(text) - type_definitions(after):
            raise ValueError("Original type definition/member order changed: " + entry["old"])
        if entry["old"].endswith(".cpp"):
            after = re.sub(r'RequestId StartLegacy(?:OrdinaryActivation|PrimarySwitch)\([^{}]*\)\s*'
                           r'\{ return entry\.Start(?:OrdinaryActivation|PrimarySwitch)\(command\); \}', "", after)
            if token_sequence(text) != token_sequence(after):
                raise ValueError("Original implementation token order changed: " + entry["old"])
    print(f"migration: PASS ({len(paths)} original files, type definitions and implementation token order)", flush=True)
    revision = manifest["layout_revision"]
    tracked = subprocess.check_output(["git", "ls-tree", "-rz", "--name-only", revision["baseline"], "--", prefix], cwd=repo)
    inputs = {p.decode()[len(prefix):] for p in tracked.split(b"\0") if p}
    entries = revision["files"]
    if inputs != {entry["old"] for entry in entries} or len(inputs) != len(entries):
        raise ValueError("Layout revision must map every input file exactly once")
    actual = {p.relative_to(SOURCE).as_posix() for p in SOURCE.rglob("*") if p.is_file()}
    if actual != {entry["target"] for entry in entries}:
        raise ValueError("Layout revision destinations differ from actual files")
    for entry in entries:
        before = subprocess.check_output(["git", "show", revision["baseline"] + ":" + prefix + entry["old"]], cwd=repo)
        if hashlib.sha256(before).hexdigest() != entry["sha256"]:
            raise ValueError("Layout input digest mismatch: " + entry["old"])
        if Path(entry["old"]).suffix in SUFFIXES:
            after = (SOURCE / entry["target"]).read_text()
            # Includes are checked separately; preserve all remaining tokens, including header bodies.
            if token_sequence(before.decode()) != token_sequence(after):
                raise ValueError("Layout changed code tokens: " + entry["old"])
    print(f"layout revision: PASS ({len(entries)} files; C++ tokens unchanged outside directives)", flush=True)



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--migration", action="store_true", help="Also verify the recorded baseline mapping")
    args = parser.parse_args()
    try:
        result = check()
        print("structure: PASS " + json.dumps(result, sort_keys=True) + " (textual includes/targets only)")
        if args.migration:
            check_migration()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, "structure: FAIL: " + str(error) + "\n")


if __name__ == "__main__":
    main()
