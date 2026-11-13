#!/usr/bin/env python3
"""Explicit offline preparation; no network access or implicit downloads."""

import sys as _sys
from pathlib import Path as _Path
_TEST_ROOT = next(p for p in _Path(__file__).resolve().parents if (p / "scripts/_paths.py").is_file())
_sys.path.insert(0, str(_TEST_ROOT / "scripts"))
from _paths import TEST as _TEST, CORE as _CORE, REPO as _REPO, locate, guard_asset
import argparse
import hashlib
import json
from pathlib import Path
import tarfile

parser = argparse.ArgumentParser()
parser.add_argument("--archive", required=True)
parser.add_argument("--destination", required=True)
args = parser.parse_args()
lock = json.loads(((_TEST / 'framework/host') / "configs/dependency_lock.json").read_text())["googletest"]
archive = Path(args.archive)
if hashlib.sha256(archive.read_bytes()).hexdigest() != lock["archive_sha256"]:
    raise SystemExit("Pinned GoogleTest archive SHA256 mismatch")
destination = Path(args.destination).resolve()
destination.mkdir(parents=True, exist_ok=True)
with tarfile.open(archive) as source:
    source.extractall(destination, filter="data")
print(destination / ("googletest-" + lock["commit"]))
