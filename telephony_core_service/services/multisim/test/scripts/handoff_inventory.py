#!/usr/bin/env python3
"""Measure the tracked M9 source tree and the observed Host build/test closure."""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re
import subprocess

from _paths import CORE, REPO, TEST

SOURCE_SUFFIXES = {'.h', '.hpp', '.cpp', '.cc', '.c'}
DOMAINS = {'account', 'card', 'device', 'infrastructure', 'role_assignment'}
# Count a physical line when it contains C/C++ tokens other than comments.
LEXEME = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[^\s]', re.S)


def source_size(text):
    code_lines = set()
    offset = 0
    line = 1
    for value in LEXEME.finditer(text):
        line += text.count('\n', offset, value.start())
        token = value.group()
        if not token.startswith(('//', '/*')):
            code_lines.update(range(line, line + token.count('\n') + 1))
        line += token.count('\n')
        offset = value.end()
    return {'files': 1, 'physical': len(text.splitlines()), 'code': len(code_lines)}


def summarize(rows):
    directories = defaultdict(Counter)
    for row in rows:
        path = Path(row['path'])
        for parent in path.parents:
            directories[parent.as_posix()].update(row['size'])
    return {path: dict(count) for path, count in sorted(directories.items())}


def observed_cases(report):
    data = json.loads(report.read_text())
    if data.get('status') != 'PASS':
        raise ValueError(f'Accepted full Host report required: {report}')
    return {row['id'] for row in data['cases'] if row['status'] == 'PASS'}


def build_inventory(database):
    compiled = set()
    for row in json.loads(database.read_text()):
        command = row.get('command', ' '.join(row.get('arguments', [])))
        if 'libsim_target_production_host.' not in command:
            continue
        file = (Path(row['directory']) / row['file']).resolve()
        compiled.add(file.relative_to(CORE / 'services/multisim').as_posix())
    manifest = (CORE / 'sim_target_sources.gni').read_text()
    expected = set(re.findall(r'\$sim_target_root/([^"\n]+\.cpp)', manifest))
    if not expected or compiled != expected:
        raise ValueError(f'Target Host compile closure differs: missing={sorted(expected - compiled)}, '
                         f'unexpected={sorted(compiled - expected)}')
    return compiled


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-db', type=Path, required=True)
    parser.add_argument('--legacy', type=Path)
    parser.add_argument('--target', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    module = CORE / 'services/multisim'
    tracked = subprocess.check_output(['git', 'ls-files', '-z', '--', str(module)], cwd=REPO).decode().split('\0')
    host_sources = build_inventory(args.compile_db)
    declarations = set(re.findall(r'"([^"\n]+\.(?:cpp|h))"', (module / 'BUILD.gn').read_text()))
    production, tests = [], []
    for name in filter(None, tracked):
        file = REPO / name
        relative = file.relative_to(module)
        if file.suffix not in SOURCE_SUFFIXES:
            continue
        domain = relative.parts[0]
        row = {'path': relative.as_posix(), 'size': source_size(file.read_text())}
        if domain in DOMAINS:
            row['build'] = ('host-object' if row['path'] in host_sources else
                            'logical-gn' if row['path'] in declarations else 'undeclared')
            production.append(row)
        elif domain == 'test' and relative.parts[1] in {'cases', 'framework'}:
            tests.append(row | {'path': Path(*relative.parts[1:]).as_posix()})
    if not host_sources <= {row['path'] for row in production}:
        raise ValueError('Target compile database references sources outside the tracked production inventory')
    result = {'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(),
              'production': production, 'production_by_directory': summarize(production),
              'tests_by_directory': summarize(tests)}
    if args.legacy and args.target:
        legacy, target = observed_cases(args.legacy), observed_cases(args.target)
        matrix = json.loads((TEST / 'config/backend_capabilities.json').read_text())['scenarios']
        common = {row['id'] for row in matrix if row['differential'] == 'SUPPORTED' and
                  row['legacy']['test'] in legacy and row['target']['test'] in target}
        result['backend_registrations'] = {'legacy': sorted(legacy), 'target': sorted(target),
                                          'mapped_common_semantics': sorted(common)}
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n')


if __name__ == '__main__':
    main()
