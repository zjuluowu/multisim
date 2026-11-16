#!/usr/bin/env python3
"""Verify upstream-owned files byte for byte; local docs and refactor assets remain separate."""
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
    with tarfile.open(fileobj=io.BytesIO(raw)) as archive:
        for entry in archive:
            if not entry.isfile() or entry.name == 'AGENTS.md' or entry.name.startswith('docs/'):
                continue
            expected = archive.extractfile(entry).read()
            path = CORE / entry.name
            if not path.is_file() or path.read_bytes() != expected:
                raise RuntimeError('Upstream file differs: ' + entry.name)
            checked.append({'path': entry.name, 'sha256': hashlib.sha256(expected).hexdigest()})
    for name in ['include/legacy_sim_query_binding.h', 'src/legacy_sim_query_binding.cpp']:
        if (CORE / 'services/core' / name).exists():
            raise RuntimeError('Production binding remains: ' + name)
    return {'upstream': spec['upstream'], 'status': 'PASS', 'files': checked,
            'excluded': ['AGENTS.md', 'docs/'], 'scope': 'all upstream-owned regular files except local instructions and documentation'}
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output')
    args = parser.parse_args()
    result = check()
    if args.output:
        from pathlib import Path
        Path(args.output).write_text(json.dumps(result, indent=2) + '\n')
    print('upstream sync: PASS (' + str(len(result['files'])) + ' exact files; ' + result['upstream'] + ')')
