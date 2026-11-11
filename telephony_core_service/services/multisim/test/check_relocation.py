#!/usr/bin/env python3
"""Verify the test relocation, frozen assets and retired engineering locations."""
import hashlib
import json
from pathlib import Path


def check():
    tests = Path(__file__).resolve().parent
    core = tests.parents[2]
    manifest = json.loads((tests / "relocation_manifest.json").read_text())
    # Keep the original relocation evidence immutable. Later authorized engineering
    # changes have explicit old/new digests and reasons, rather than rewriting history.
    followups = json.loads((tests / "relocation_followups.json").read_text())["files"]
    known = {entry["new"] for entry in manifest["files"]}
    if set(followups) - known:
        raise RuntimeError("Follow-up references an unknown relocated input")
    for old in manifest["moves"]:
        if (core / old).exists():
            raise RuntimeError("Retired engineering path still exists: " + old)
    immutable = 0
    evolved = 0
    for entry in manifest["files"]:
        path = core / entry["new"]
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        update = followups.get(entry["new"])
        if update:
            if (update["relocation_sha256"] != entry["after_sha256"] or
                    actual != update["current_sha256"] or not update["reason"]):
                raise RuntimeError("Authorized follow-up differs: " + entry["new"])
            evolved += 1
            continue
        if actual != entry["after_sha256"]:
            raise RuntimeError("Relocated input differs: " + entry["new"])
        if path.suffix in {".h", ".hpp", ".cpp", ".json"}:
            if actual != entry["before_sha256"]:
                raise RuntimeError("Assertion/Fake/evidence changed: " + entry["new"])
            immutable += 1
    print(f"relocation: PASS ({len(manifest['files'])} files; "
          f"{immutable} C++/JSON assets byte-identical; {evolved} explicit follow-ups)")


if __name__ == "__main__":
    check()
