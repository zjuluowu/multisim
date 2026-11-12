#!/usr/bin/env python3
"""Verify the five-group move against committed input, without accepting semantic drift."""
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
    manifest = json.loads((Path(__file__).parent / 'directory_migration_20261112.json').read_text())
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
    from check_structure import module_files
    actual = {p.relative_to(source).as_posix() for p in module_files(source)}
    if len(set(outputs)) != len(rows) or set(outputs) != actual:
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
        if Path(name).suffix in {'.h', '.cpp'} or name == 'BUILD.gn':
            expected = original
            for old, new in sorted(moves.items(), key=lambda pair: -len(pair[0])):
                expected = expected.replace(old.encode(), new.encode())
            current = (source / target).read_bytes()
            if expected != current:
                raise ValueError('Directory changed bytes beyond mapped paths: ' + name)
            if Path(name).suffix in {'.h', '.cpp'} and token_sequence(original.decode()) != token_sequence(current.decode()):
                raise ValueError('Directory changed C++ behavior/type layout: ' + name)
    print(f'directory migration: PASS ({len(rows)} inputs; exact mapped C++/GN bytes; no target policy change)', flush=True)
