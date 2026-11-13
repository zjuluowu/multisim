#!/usr/bin/env python3
"""Apply a reviewed patch ONLY to a fresh outside-repository source copy, then reuse Host regression."""

import sys as _sys
from pathlib import Path as _Path
_TEST_ROOT = next(p for p in _Path(__file__).resolve().parents if (p / "scripts/_paths.py").is_file())
_sys.path.insert(0, str(_TEST_ROOT / "scripts"))
from _paths import TEST as _TEST, CORE as _CORE, REPO as _REPO, locate, guard_asset
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

HERE = (_TEST / 'cases/primary_slot/lifecycle')
SYSTEM = _TEST / "framework/host"
CORE = _CORE
REPO = _REPO
sys.path.insert(0, str((_TEST / 'scripts')))
from host import command, write


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--patch', type=Path, default=(_TEST / 'cases/primary_slot/lifecycle/pairing_fixes.proposed.patch'))
    parser.add_argument('--config', type=Path, default=REPO / '.host-out/host.json')
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(REPO) or output in REPO.parents:
        parser.error('Verification output must be outside the repository')
    output.mkdir(parents=True, exist_ok=True)
    session = Path(tempfile.mkdtemp(prefix='pairing-review-', dir=output))
    source = session / 'source'
    source.mkdir()
    copied = source / CORE.name
    patch = args.patch.resolve(strict=True)
    paths = subprocess.check_output(['git', 'ls-files', '-c', '-o', '--exclude-standard', '-z', '--', CORE.name], cwd=REPO).decode().rstrip('\0').split('\0')
    migration = json.loads((_TEST / 'docs/Test_Architecture_Migration.json').read_text())
    retired = {'telephony_core_service/services/multisim/test/' + r['old'] for r in migration['files'] if r['old'] != r['new']}
    paths = sorted(set(name for name in paths if (REPO / name).is_file() or name not in retired))
    original = {}
    for name in paths:
        src = REPO / name
        if not src.is_file(): raise RuntimeError('Tracked input missing: ' + name)
        original[name] = sha(src)
        dst = source / name
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
    write(session / 'source-inputs.json', original)
    shutil.copyfile(patch, session / 'reviewed.patch')
    commands = []
    result = {'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(),
              'status': 'BLOCKED', 'source_copy': str(copied), 'patch_sha256': sha(patch),
              'commands': commands, 'original_tree_modified': False, 'cases': []}
    def run(argv, label, cwd):
        code = command(argv, session / (label + '.log'), cwd)
        commands.append({'argv': list(map(str, argv)), 'cwd': str(cwd), 'exit': code, 'log': label + '.log'})
        write(session / 'result.json', result)
        return code
    if run(['git', 'apply', '--check', '--ignore-whitespace', patch], 'patch-check', source): return 2
    if run(['git', 'apply', '--ignore-whitespace', patch], 'patch-apply-isolated', source): return 2
    changed = [p for p, h in original.items() if sha(source / p) != h]
    result['patched_files'] = {p: {'original': original[p], 'patched': sha(source / p)} for p in changed}
    config = json.loads(args.config.read_text())
    argv = [sys.executable, '-B', (_TEST / 'scripts/host.py'), 'all', '--with-real-primary',
            '--out', session / 'host', '--production-root', copied,
            '--gtest-root', config['gtest_root'], '--openssl-root', config['openssl_root'],
            '--runtime-root', config['sim028_runtime_root']]
    code = run(argv, 'host-regression', REPO)
    result['host_exit'] = code
    latest = session / 'host/latest.json'
    if latest.exists():
        host = json.loads(latest.read_text())
        result['host_result'] = host
        artifacts = Path(host['artifacts'])
        for group in host['groups']:
            for c in ET.parse(artifacts / (group['name'] + '.xml')).getroot().findall('.//testcase'):
                properties = {p.get('name'): p.get('value') for p in c.findall('./properties/property')}
                result['cases'].append({'name': c.get('classname') + '.' + c.get('name'),
                    'gtest': 'FAIL' if c.find('failure') is not None else 'PASS',
                    'status': 'BLOCKED' if properties.get('resource_lifetime') == 'BLOCKED' else
                        ('FAIL' if c.find('failure') is not None else 'PASS'), 'properties': properties})
        result['counts'] = {k: sum(g[k] for g in host['groups']) for k in ['tests', 'passed', 'failed']}
        result['status'] = host['status']
    changed_originals = [p for p, h in original.items() if sha(REPO / p) != h]
    result['original_tree_modified'] = bool(changed_originals)
    result['unexpected_original_changes'] = changed_originals
    write(session / 'result.json', result)
    write(output / 'latest.json', result)
    print(json.dumps({'status': result['status'], 'counts': result.get('counts'), 'artifacts': str(session),
                      'original_tree_modified': result['original_tree_modified']}, indent=2))
    if changed_originals: return 2
    return int(result['status'] != 'PASS')


if __name__ == '__main__':
    raise SystemExit(main())
