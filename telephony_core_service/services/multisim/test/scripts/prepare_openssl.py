#!/usr/bin/env python3
"""Explicit offline extraction of checksum-pinned OpenSSL development headers."""

import sys as _sys
from pathlib import Path as _Path
_TEST_ROOT = next(p for p in _Path(__file__).resolve().parents if (p / "scripts/_paths.py").is_file())
_sys.path.insert(0, str(_TEST_ROOT / "scripts"))
from _paths import TEST as _TEST, CORE as _CORE, REPO as _REPO, locate, guard_asset
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

SYSTEM = (_TEST / 'framework/host')
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--archive', required=True)
parser.add_argument('--destination', required=True)
args = parser.parse_args()
archive = Path(args.archive).resolve()
destination = Path(args.destination).resolve()
if destination == SYSTEM or SYSTEM in destination.parents:
    parser.error('Dependencies must be outside source directory')
lock = json.loads(((_TEST / 'framework/host/configs/dependency_lock.json')).read_text())['openssl_headers']
if hashlib.sha256(archive.read_bytes()).hexdigest() != lock['archive_sha256']:
    raise SystemExit('OpenSSL package checksum mismatch')
if destination.exists():
    raise SystemExit('Use a new, empty destination for explicit dependency preparation')
subprocess.run(['dpkg-deb', '-x', str(archive), str(destination)], check=True)
(destination / 'sim_dependency.json').write_text(json.dumps(lock, indent=2) + '\n')
print('Prepared pinned OpenSSL headers; no system package installed')
