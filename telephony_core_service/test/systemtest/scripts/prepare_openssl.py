#!/usr/bin/env python3
"""Explicit offline extraction of checksum-pinned OpenSSL development headers."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

SYSTEM = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--archive', required=True)
parser.add_argument('--destination', required=True)
args = parser.parse_args()
archive = Path(args.archive).resolve()
destination = Path(args.destination).resolve()
if destination == SYSTEM or SYSTEM in destination.parents:
    parser.error('Dependencies must be outside source directory')
lock = json.loads((SYSTEM / 'configs/dependency_lock.json').read_text())['openssl_headers']
if hashlib.sha256(archive.read_bytes()).hexdigest() != lock['archive_sha256']:
    raise SystemExit('OpenSSL package checksum mismatch')
if destination.exists():
    raise SystemExit('Use a new, empty destination for explicit dependency preparation')
subprocess.run(['dpkg-deb', '-x', str(archive), str(destination)], check=True)
(destination / 'sim_dependency.json').write_text(json.dumps(lock, indent=2) + '\n')
print('Prepared pinned OpenSSL headers; no system package installed')
