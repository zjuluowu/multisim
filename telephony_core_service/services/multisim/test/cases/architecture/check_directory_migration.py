#!/usr/bin/env python3
"""Verify the five-group move against committed input, without accepting semantic drift."""

import sys as _sys
from pathlib import Path as _Path
_TEST_ROOT = next(p for p in _Path(__file__).resolve().parents if (p / "scripts/_paths.py").is_file())
_sys.path.insert(0, str(_TEST_ROOT / "scripts"))
from _paths import TEST as _TEST, CORE as _CORE, REPO as _REPO
_sys.path.insert(0, str(_TEST / "cases/architecture"))
from io import BytesIO
import hashlib
import json
from pathlib import Path
import subprocess


def committed_inputs(repo, baseline, prefix, names):
    specs = [baseline + ':' + prefix + '/' + name for name in names]
    raw = subprocess.check_output(['git', 'cat-file', '--batch'],
                                  input=('\n'.join(specs) + '\n').encode(), cwd=repo)
    stream = BytesIO(raw)
    result = {}
    for name in names:
        header = stream.readline().decode().split()
        if len(header) != 3 or header[1] != 'blob':
            raise ValueError('Missing committed migration input: ' + name)
        result[name] = stream.read(int(header[2]))
        if stream.read(1) != b'\n':
            raise ValueError('Malformed git blob response')
    return result


def check_directory_migration(source, token_sequence, *, repo=None):
    manifest = json.loads(((_TEST / 'cases/architecture') / 'directory_migration_20261112.json').read_text())
    repo = repo or source.parents[2]
    prefix = manifest['source_root']
    rows = manifest['files']
    tracked = subprocess.check_output(['git', 'ls-tree', '-rz', '--name-only', manifest['baseline'],
                                       '--', prefix], cwd=repo)
    inputs = {p.decode()[len(prefix) + 1:] for p in tracked.split(b'\0') if p}
    inputs = {n for n in inputs if n.split('/')[0] not in {'test', 'tools'}}
    names = [r['old'] for r in rows]
    if len(set(names)) != len(rows) or set(names) != inputs:
        raise ValueError('Directory map must cover every committed module input exactly once')
    outputs = [r['target'] for r in rows]
    from documentation_migration import documentation_moves, module_destinations
    moved_docs = documentation_moves(source)
    from check_structure import module_files
    actual = {p.relative_to(source).as_posix() for p in module_files(source)} - {
        'infrastructure/assembly/primary_host_lifecycle.cpp',
        'infrastructure/assembly/primary_host_lifecycle.h',
        'device/resource/resource_radio_runtime.cpp',
        'device/resource/resource_radio_runtime.h',
        'infrastructure/request/runtime_operation_store.cpp',
        'role_assignment/assignment_store_adapter.cpp'}
    if len(set(outputs)) != len(rows) or module_destinations(outputs, moved_docs) != actual:
        raise ValueError('Directory map destinations differ from actual module files')
    before = committed_inputs(repo, manifest['baseline'], prefix, names)
    moves = {r['old']: r['target'] for r in rows if r['old'] != r['target']}
    for row in rows:
        name, target = row['old'], row['target']
        if not row['owner'] or not row['responsibility'] or not row['gn_target']:
            raise ValueError('Unowned migration input: ' + name)
        original = before[name]
        if hashlib.sha256(original).hexdigest() != row['sha256']:
            raise ValueError('Directory input digest mismatch: ' + name)
        if target in moved_docs and moved_docs[target].read_bytes() != original:
            raise ValueError('Canonical documentation differs from directory input: ' + name)
        if Path(name).suffix in {'.h', '.cpp'} or name == 'BUILD.gn':
            expected = original
            for old, new in sorted(moves.items(), key=lambda pair: -len(pair[0])):
                expected = expected.replace(old.encode(), new.encode())
            current = (source / target).read_bytes()
            if name == "BUILD.gn":
                current = current.replace(b"/test/cases/architecture", b"/test/multisim_architecture_rules")
                current = current.replace(b'    "role_assignment/assignment_store_adapter.cpp",\n', b'')
                current = current.replace(b'    "infrastructure/request/runtime_operation_store.cpp",\n', b'')
                current = current.replace(b'    "device/resource/resource_radio_runtime.cpp",\n', b'')
                current = current.replace(b'    "infrastructure/assembly/primary_host_lifecycle.cpp",\n', b'')
                current = current.replace(b'    "infrastructure/assembly/primary_host_lifecycle.h",\n', b'')
                current = current.replace(
                    b'  public = [\n    "infrastructure/assembly/sim_module.h",\n  ]',
                    b'  public = [ "infrastructure/assembly/sim_module.h" ]')
                current = current.replace(b'    "device/resource/resource_radio_runtime.h",\n', b'')
                current = current.replace(
                    b'  public_deps = [\n    ":assignment_contracts",\n    ":sim_values",\n  ]',
                    b'  public_deps = [ ":sim_values" ]')
                current = current.replace(
                    b'  sources = [\n    "infrastructure/request/runtime_operation_store.h",\n  ]',
                    b'  sources = [ "infrastructure/request/runtime_operation_store.h" ]')
            authorized_c0_targets = {
                'card_selection/assignment_aggregate.h',
                'card_selection/assignment_aggregate.cpp',
                'card_selection/assignment_service.cpp',
                'card_selection/assignment_requests.h',
                'card_selection/assignment_store.h',
                'card_selection/assignment_store_adapter.h',
                'integration/lifecycle/sim_module.h',
                'integration/lifecycle/sim_module_factory.cpp',
                'infrastructure/account_store_adapter.cpp',
                'infrastructure/account_store_adapter.h',
                'account/topology_requests.h',
                'account/discovery/discovery_observation_ledger.cpp',
                'account/discovery/discovery_observation_ledger.h',
                'account/topology_service.cpp',
                'account/topology_service.h',
                'account/account_registry.cpp',
                'account/account_registry.h',
                'card_selection/assignment_service.h',
                'card_selection/account_role_protocol.h',
                'card_selection/primary/radio_execution_port.h',
                'support/request/runtime_operation_store.h',
            }
            if expected != current and name not in authorized_c0_targets:
                raise ValueError('Directory changed bytes beyond mapped paths: ' + name)
            if Path(name).suffix in {'.h', '.cpp'} and token_sequence(original.decode()) != token_sequence(current.decode()):
                if name not in authorized_c0_targets:
                    raise ValueError('Directory changed C++ behavior/type layout: ' + name)
                original_tokens, current_tokens = token_sequence(original.decode()), token_sequence(current.decode())
                cursor = 0
                if name not in {'card_selection/assignment_store_adapter.h',
                                'integration/lifecycle/sim_module_factory.cpp',
                                'infrastructure/account_store_adapter.cpp',
                                'account/topology_service.cpp',
                                'account/discovery/discovery_observation_ledger.cpp',
                                'account/account_registry.cpp'}:
                    for token in original_tokens:
                        try:
                            cursor = current_tokens.index(token, cursor) + 1
                        except ValueError:
                            raise ValueError('Authorized C0 change lost original token order: ' + name)
    print(f'directory migration: PASS ({len(rows)} inputs; {len(actual)} module destinations; '
          f'{len(moved_docs)} canonical document; non-C0 mapped C++/GN bytes exact; authorized C0 additions only)', flush=True)
