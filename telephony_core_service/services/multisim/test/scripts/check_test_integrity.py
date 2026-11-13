#!/usr/bin/env python3
"""Verify every frozen source input, relocated output and immutable business asset."""
import ast
from io import BytesIO
import hashlib
import json
from pathlib import Path
import subprocess
from _paths import TEST, REPO, CORE


def check():
    manifest = json.loads((TEST / 'docs/Test_Architecture_Migration.json').read_text())
    rows = manifest['files']
    prefix = str(TEST.relative_to(REPO)) + '/'
    actual = subprocess.check_output(['git', 'ls-tree', '-rz', '--name-only', manifest['baseline'], '--', prefix], cwd=REPO).decode().rstrip('\0').split('\0')
    expected = {p[len(prefix):] for p in actual}
    if expected != {r['old'] for r in rows} or len(rows) != len(expected):
        raise RuntimeError('Migration does not cover every committed test input once')
    if len({r['new'] for r in rows}) != len(rows):
        raise RuntimeError('Duplicate migration destination')
    specs = [manifest['baseline'] + ':' + prefix + r['old'] for r in rows]
    raw = subprocess.check_output(['git', 'cat-file', '--batch'], input=('\n'.join(specs) + '\n').encode(), cwd=REPO)
    stream = BytesIO(raw)
    originals = {}
    for row in rows:
        header = stream.readline().decode().split()
        if len(header) != 3 or header[1] != 'blob':
            raise RuntimeError('Missing frozen Git input: ' + row['old'])
        data = stream.read(int(header[2]))
        if stream.read(1) != b'\n' or hashlib.sha256(data).hexdigest() != row['before_sha256']:
            raise RuntimeError('Frozen Git digest mismatch: ' + row['old'])
        originals[row['old']] = data
    # The classifier determines business vs cleanup failure and must retain its exact logic.
    def function_tree(text, name):
        return ast.dump(next(n for n in ast.parse(text).body if isinstance(n, ast.FunctionDef) and n.name == name), include_attributes=False)
    before = originals['systemtest/scripts/run_legacy_primary_contract.py'].decode()
    after = (TEST / 'scripts/run_legacy_primary_contract.py').read_text()
    if function_tree(before, 'classify') != function_tree(after, 'classify'):
        raise RuntimeError('Legacy business/lifecycle classification changed')
    if function_tree(originals['systemtest/tools/ci_sim028.py'].decode(), 'evaluate') != function_tree((TEST / 'scripts/ci_sim028.py').read_text(), 'evaluate'):
        raise RuntimeError('CI admission/counting changed')
    if originals['sim_guard/oracle/primary_oracle.py'] != (TEST / 'cases/primary_slot/oracle/primary_oracle.py').read_bytes():
        raise RuntimeError('Primary business Oracle changed')
    immutable = 0
    for row in rows:
        path = TEST / row['new']
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != row['after_sha256']:
            raise RuntimeError('Output drift: ' + row['new'])
        # Business tests/Fake/Adapter/Golden, all JSON and all evidence keep their bytes.
        frozen = path.suffix in {'.cpp', '.h', '.hpp', '.json'} or row['new'].startswith('docs/archive/')
        if frozen and row['old'] != 'systemtest/primary_slot/runtime_include/hisysevent.h':
            if digest != row['before_sha256']:
                raise RuntimeError('Immutable business/evidence asset changed: ' + row['old'])
            immutable += 1
    # The sole changed C++ header only relocates its include of the identical platform stub.
    old = subprocess.check_output(['git', 'show', manifest['baseline'] + ':' + prefix + 'systemtest/primary_slot/runtime_include/hisysevent.h'], cwd=REPO)
    now = (TEST / 'cases/primary_slot/runtime_include/hisysevent.h').read_bytes()
    if old.replace(b'../../stubs/include/hisysevent.h', b'../../../framework/host/stubs/include/hisysevent.h') != now:
        raise RuntimeError('Primary platform include changed beyond the mapped path')
    # Framework headers may not include a concrete business case.
    for path in (TEST / 'framework').rglob('*'):
        if path.suffix in {'.cpp', '.h'}:
            for line in path.read_text().splitlines():
                if line.lstrip().startswith('#include') and '/cases/' in line:
                    raise RuntimeError('Framework depends on case: ' + str(path))
    print(f'integrity: PASS ({len(rows)} input/output mappings; {immutable} immutable assets)')


if __name__ == '__main__':
    check()
