#!/usr/bin/env python3
"""Apply the reviewed SIM-028 candidate ONLY to a new isolated local clone."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys


def canonical(data):
    return data.replace(b'\r\n', b'\n')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', required=True, type=Path)
    parser.add_argument('--gtest-root', required=True, type=Path)
    parser.add_argument('--openssl-root', required=True, type=Path)
    parser.add_argument('--runtime-root', required=True, type=Path)
    parser.add_argument('--repository', type=Path,
                        help='Actual source workspace; defaults to the Git root containing this kit')
    parser.add_argument('--prepare-only', action='store_true', help='Verify and apply the candidate in isolation without building or running')
    parser.add_argument('--all', action='store_true', help='Execute all tests, including the five retained failures')
    parser.add_argument('--filter', default='RealSimPrimary.C1ThreePhasesWithStaleCheckResponseRejected')
    parser.add_argument('--sanitizer', choices=['', 'address', 'thread'], default='')
    args = parser.parse_args()
    kit = Path(__file__).resolve().parent
    root = args.repository.resolve() if args.repository else Path(subprocess.check_output(
        ['git', '-C', str(kit), 'rev-parse', '--show-toplevel'], text=True).strip())
    destination = args.destination.resolve()
    if destination.exists() or destination == root or root in destination.parents:
        parser.error('Destination must be a NEW directory outside the source workspace')
    manifest = json.loads((kit / 'changed-files.json').read_text())
    for row in manifest:
        path = root / row['path']
        if row['added']:
            if path.exists():
                parser.error('Candidate path already exists in input: ' + row['path'])
        elif digest(canonical(path.read_bytes())) != row['base_canonical_sha256']:
            parser.error('Input differs from reviewed baseline: ' + row['path'])
    destination.mkdir(parents=True)
    clone = destination / 'repo'
    subprocess.run(['git', 'clone', '--no-hardlinks', '--quiet', str(root), str(clone)], check=True)
    # Git clone alone omits user changes. Overlay the ACTUAL tracked/untracked
    # inputs, excluding ignored build outputs; do not mutate the original tree.
    paths = subprocess.check_output(['git', '-C', str(root), 'ls-files', '-c', '-o',
                                     '--exclude-standard', '-z']).split(b'\0')
    input_hashes = {}
    for encoded in paths:
        if not encoded:
            continue
        relative = encoded.decode()
        source = root / relative
        if not source.is_file():
            continue
        target = clone / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        input_hashes[relative] = digest(source.read_bytes())
        if digest(target.read_bytes()) != input_hashes[relative]:
            raise RuntimeError('Overlay hash mismatch: ' + relative)
    # Normalize only patched files INSIDE the isolated clone. The patch and
    # candidate manifest use canonical LF; current-workspace bytes stay intact.
    for row in manifest:
        if not row['added']:
            target = clone / row['path']
            target.write_bytes(canonical(target.read_bytes()))
    for name in ['production-stop.patch', 'host-tests.patch']:
        command = ['git', 'apply', str(kit / name)]
        subprocess.run(command[:2] + ['--check'] + command[2:], cwd=clone, check=True)
        subprocess.run(command, cwd=clone, check=True)
    for row in manifest:
        if digest(canonical((clone / row['path']).read_bytes())) != row['candidate_canonical_sha256']:
            raise RuntimeError('Patched candidate hash mismatch: ' + row['path'])
    command = [sys.executable, '-B', str(clone / 'telephony_core_service/services/multisim/test/systemtest/tools/host.py'),
               'all' if args.all else 'selected', '--with-real-primary',
               '--target', 'sim_primary_slot_real_test', '--out', str(clone / '.host-out'),
               '--gtest-root', str(args.gtest_root.resolve()), '--openssl-root', str(args.openssl_root.resolve()),
               '--runtime-root', str(args.runtime_root.resolve()), '--sanitizer', args.sanitizer]
    if not args.all:
        command += ['--filter', args.filter]
    provenance = {'source_head': subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip(),
                  'source': str(root), 'clone': str(clone), 'overlay_sha256': input_hashes, 'command': command}
    (destination / 'isolation.json').write_text(json.dumps(provenance, indent=2) + '\n')
    print('Isolated candidate:', clone, flush=True)
    if args.prepare_only:
        print('PREPARED ONLY: no build or test executed', flush=True)
        return 0
    result = subprocess.run(command, cwd=clone)
    # Preserve actual failures; --all intentionally returns 1 for the known five.
    return result.returncode


if __name__ == '__main__':
    sys.exit(main())
