#!/usr/bin/env python3
"""Verify exact upstream bytes and explicitly fingerprinted local repairs."""
import argparse
import hashlib
import io
import json
import subprocess
import tarfile
from _paths import CORE, REPO, TEST

def check():
    spec = json.loads((TEST / 'docs/Upstream_Sync_Test_Adaptation.json').read_text())
    for name, item in spec['reference_sources'].items():
        original = subprocess.check_output(['git', 'show', item['commit'] + ':' + item['source']], cwd=REPO)
        if hashlib.sha256(original).hexdigest() != item['sha256'] or (TEST / name).read_bytes() != original:
            raise RuntimeError('Test reference provenance differs: ' + name)
    raw = subprocess.check_output(['git', 'archive', spec['upstream']], cwd=REPO)
    checked = []
    repairs = spec.get("local_repairs", {})
    verified_repairs = []
    with tarfile.open(fileobj=io.BytesIO(raw)) as archive:
        for entry in archive:
            if not entry.isfile() or entry.name == 'AGENTS.md' or entry.name.startswith('docs/'):
                continue
            expected = archive.extractfile(entry).read()
            path = CORE / entry.name
            repair = repairs.get(entry.name)
            if repair:
                patch = TEST / repair['patch']
                if (hashlib.sha256(expected).hexdigest() != repair['upstream_sha256'] or
                        not patch.is_file() or
                        hashlib.sha256(patch.read_bytes()).hexdigest() != repair['patch_sha256'] or
                        not path.is_file() or
                        hashlib.sha256(path.read_bytes()).hexdigest() != repair['repaired_sha256']):
                    raise RuntimeError('Local repair provenance differs: ' + entry.name)
                verified_repairs.append(entry.name)
                continue
            if not path.is_file() or path.read_bytes() != expected:
                raise RuntimeError('Upstream file differs: ' + entry.name)
            checked.append({'path': entry.name, 'sha256': hashlib.sha256(expected).hexdigest()})
    if set(verified_repairs) != set(repairs):
        raise RuntimeError('Local repair does not name an upstream source')
    for name in ['include/legacy_sim_query_binding.h', 'src/legacy_sim_query_binding.cpp']:
        if (CORE / 'services/core' / name).exists():
            raise RuntimeError('Production binding remains: ' + name)
    return {'upstream': spec['upstream'], 'status': 'PASS', 'files': checked, 'local_repairs': verified_repairs,
            'excluded': ['AGENTS.md', 'docs/'], 'scope': 'exact upstream bytes except fingerprinted local repairs; excludes local instructions and documentation'}
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output')
    args = parser.parse_args()
    result = check()
    if args.output:
        from pathlib import Path
        Path(args.output).write_text(json.dumps(result, indent=2) + '\n')
    print('upstream sync: PASS (' + str(len(result['files'])) + ' exact files; ' + str(len(result['local_repairs'])) + ' fingerprinted local repairs; ' + result['upstream'] + ')')
