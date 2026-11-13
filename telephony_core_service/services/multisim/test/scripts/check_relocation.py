#!/usr/bin/env python3
"""Preserve the original 2026-11-09 relocation checks across the test layout move."""
import hashlib
import json
from _paths import TEST, CORE, locate


def check():
    manifest = json.loads(locate('relocation_manifest.json').read_text())
    followups = json.loads(locate('relocation_followups.json').read_text())['files']
    migration = json.loads((TEST / 'docs/Test_Architecture_Migration.json').read_text())
    current = {r['old']: r for r in migration['files']}
    known = {entry['new'] for entry in manifest['files']}
    if set(followups) - known:
        raise RuntimeError('Follow-up references an unknown relocated input')
    for old in manifest['moves']:
        if (CORE / old).exists():
            raise RuntimeError('Retired engineering path still exists: ' + old)
    immutable = evolved = 0
    prefix = 'services/multisim/test/'
    for entry in manifest['files']:
        if entry['new'].startswith(prefix):
            row = current[entry['new'].removeprefix(prefix)]
            path = TEST / row['new']
        else:
            path = CORE / entry['new']
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            row = {'before_sha256': digest, 'after_sha256': digest}
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        update = followups.get(entry['new'])
        expected = update['current_sha256'] if update else entry['after_sha256']
        if row['before_sha256'] != expected or actual != row['after_sha256']:
            raise RuntimeError('Relocated input differs: ' + entry['new'])
        if update:
            if update['relocation_sha256'] != entry['after_sha256'] or not update['reason']:
                raise RuntimeError('Authorized follow-up differs: ' + entry['new'])
            evolved += 1
        elif path.suffix in {'.h', '.hpp', '.cpp', '.json'}:
            if row['before_sha256'] != entry['before_sha256']:
                raise RuntimeError('Assertion/Fake/evidence changed: ' + entry['new'])
            immutable += 1
    print(f'relocation: PASS ({len(manifest["files"])} files; '
          f'{immutable} original C++/JSON assets; {evolved} explicit follow-ups; exact current mapping)')


if __name__ == '__main__':
    check()
