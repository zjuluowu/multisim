#!/usr/bin/env python3
"""Verify every frozen source input, relocated output and immutable business asset."""
import ast
from io import BytesIO
import hashlib
import json
from pathlib import Path
import subprocess
from _paths import TEST, REPO, CORE


def current_output_digests(rows):
    """Keep the frozen migration snapshot, with explicit tooling-only follow-ups."""
    followups = json.loads((TEST / 'docs/Test_Architecture_Followups_20261116.json').read_text())
    allowed = {'cases/architecture/README.md', 'cases/architecture/check_directory_migration.py',
               'cases/architecture/check_structure.py', 'cases/architecture/test_structure.py',
               'scripts/check_relocation.py'}
    updates = followups['files']
    if followups['schema_version'] != 1 or set(updates) != allowed:
        raise RuntimeError('Follow-ups must cover exactly the authorized documentation/tooling changes')
    indexed = {r['new']: r for r in rows}
    prefix = str(TEST.relative_to(REPO)) + '/'
    specs = [followups['baseline'] + ':' + prefix + name for name in updates]
    raw = subprocess.check_output(['git', 'cat-file', '--batch'],
                                  input=('\n'.join(specs) + '\n').encode(), cwd=REPO)
    stream = BytesIO(raw)
    for name, update in updates.items():
        header = stream.readline().decode().split()
        if len(header) != 3 or header[1] != 'blob':
            raise RuntimeError('Missing committed follow-up input: ' + name)
        before = stream.read(int(header[2]))
        if (stream.read(1) != b'\n' or not update['reason']
                or hashlib.sha256(before).hexdigest() != indexed[name]['after_sha256']
                or update['previous_sha256'] != indexed[name]['after_sha256']):
            raise RuntimeError('Follow-up input differs from frozen migration output: ' + name)
    result = {name: update['current_sha256'] for name, update in updates.items()}
    sync = json.loads((TEST / 'docs/Upstream_Sync_Test_Adaptation.json').read_text())
    allowed_sync = {'framework/host/BUILD.gn', 'scripts/run_g2a_query.py',
                    'cases/architecture/test_g2a_source_tables.py',
                    'scripts/check_test_integrity.py', 'README.md',
                    'cases/primary_slot/unscripted_sim_manager.h',
                    'framework/host/stubs/include/compile_declarations.h',
                    'cases/architecture/open_closed_contract_rules.cpp'}
    if set(sync['files']) != allowed_sync:
        raise RuntimeError('Unexpected upstream adaptation scope')
    for name, update in sync['files'].items():
        before = subprocess.check_output(['git', 'show', sync['baseline'] + ':' + prefix + name], cwd=REPO)
        if hashlib.sha256(before).hexdigest() != update['previous_sha256'] or not update['reason']:
            raise RuntimeError('Upstream adaptation input mismatch: ' + name)
        result[name] = update['current_sha256']
    replacement = json.loads((TEST / 'docs/Full_Replacement_Evolution.json').read_text())
    if set(replacement['files']) != {'framework/host/BUILD.gn', 'scripts/check_test_integrity.py', 'README.md'}:
        raise RuntimeError('Unexpected full-replacement bootstrap scope')
    for name, update in replacement['files'].items():
        before = subprocess.check_output(['git', 'show', replacement['baseline'] + ':' + prefix + name], cwd=REPO)
        if hashlib.sha256(before).hexdigest() != update['previous_sha256'] or not update['reason']:
            raise RuntimeError('Full-replacement bootstrap input mismatch: ' + name)
        result[name] = update['current_sha256']
    return result


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
    current_digests = current_output_digests(rows)
    for row in rows:
        path = TEST / row['new']
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != current_digests.get(row['new'], row['after_sha256']):
            raise RuntimeError('Output drift: ' + row['new'])
        # Business tests/Fake/Adapter/Golden, all JSON and all evidence keep their bytes.
        if row['new'] == 'cases/architecture/open_closed_contract_rules.cpp':
            expected = originals[row['old']].replace(b'static_assert(RADIO_ESIM_ENABLED_PROFILE_NUM == 1129);',
                b'static_assert(RADIO_ESIM_UPDATE_START == 1129);\nstatic_assert(RADIO_ESIM_UPDATE_END == 1130);\nstatic_assert(RADIO_ESIM_ENABLED_PROFILE_NUM == 1131);')
            if path.read_bytes() != expected:
                raise RuntimeError('Declaration expectations exceed upstream event insertion')
            continue
        if row['new'] == 'framework/host/stubs/include/compile_declarations.h':
            expected = originals[row['old']].replace(b'    bool WriteInt32(int32_t);',
                b'    bool WriteUint32(uint32_t); bool ReadUint32(uint32_t&);\n    bool WriteInt32(int32_t);')
            if path.read_bytes() != expected:
                raise RuntimeError('Parcel declaration adaptation exceeds two unsigned methods')
            continue
        if row['new'] == 'cases/primary_slot/unscripted_sim_manager.h':
            from generate_manager_seam import generate
            if path.read_text() != generate():
                raise RuntimeError('Upstream interface seam differs from fail-closed generator')
            continue
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
    print(f'integrity: PASS ({len(rows)} input/output mappings; {immutable} immutable assets; '
          f'{len(current_digests)} explicit tooling/documentation follow-ups)')


if __name__ == '__main__':
    check()
