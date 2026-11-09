#!/usr/bin/env python3
"""Verify the test relocation, frozen assets and retired engineering locations."""
import hashlib
import json
from pathlib import Path


def check():
    tests = Path(__file__).resolve().parent
    core = tests.parents[2]
    manifest = json.loads((tests / "relocation_manifest.json").read_text())
    for old in manifest["moves"]:
        if (core / old).exists():
            raise RuntimeError("Retired engineering path still exists: " + old)
    immutable = 0
    for entry in manifest["files"]:
        path = core / entry["new"]
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != entry["after_sha256"]:
            raise RuntimeError("Relocated input differs: " + entry["new"])
        if path.suffix in {".h", ".hpp", ".cpp", ".json"}:
            if actual != entry["before_sha256"]:
                raise RuntimeError("Assertion/Fake/evidence changed: " + entry["new"])
            immutable += 1
    print(f"relocation: PASS ({len(manifest['files'])} files; "
          f"{immutable} C++/JSON assets byte-identical)")


if __name__ == "__main__":
    check()
