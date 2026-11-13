#!/usr/bin/env python3
"""Compile future GTest Stop branches with a declaration-only isolated header; never link/run fake Stop."""

import sys as _sys
from pathlib import Path as _Path
_TEST_ROOT = next(p for p in _Path(__file__).resolve().parents if (p / "scripts/_paths.py").is_file())
_sys.path.insert(0, str(_TEST_ROOT / "scripts"))
from _paths import TEST as _TEST, CORE as _CORE, REPO as _REPO, locate, guard_asset
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-commands', type=Path, required=True)
    parser.add_argument('--header', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    repo = _REPO
    output = args.output.resolve()
    if output.is_relative_to(repo) or output in repo.parents:
        parser.error('Use an outside-repository output directory')
    output.mkdir(parents=True, exist_ok=True)
    session = Path(tempfile.mkdtemp(prefix='signature-', dir=output))
    include = session / 'include'; include.mkdir()
    header = args.header.resolve(strict=True)
    before = header.read_bytes()
    source = before.decode()
    position = source.index('public:', source.index('class SimManager')) + len('public:')
    methods = '\n    // Declaration-only verification overlay, no implementation.\n    bool Stop();\n    bool IsStopping() const;\n    bool IsStopped() const;\n'
    overlay = include / 'sim_manager.h'
    overlay.write_text(source[:position] + methods + source[position:])
    entry = next(e for e in json.loads(args.compile_commands.read_text())
                 if e['file'].endswith('/shutdown_contract_test.cpp'))
    original = entry.get('arguments') or shlex.split(entry['command'])
    filtered = []; index = 0
    while index < len(original):
        if original[index] in ['-o', '-MF']: index += 2; continue
        if original[index] in ['-c', '-MMD']: index += 1; continue
        filtered.append(original[index]); index += 1
    argv = [filtered[0], '-I' + str(include), '-fsyntax-only'] + filtered[1:]
    with (session / 'compile.log').open('w') as stream:
        compiled = subprocess.run(argv, cwd=entry['directory'], stdout=stream, stderr=subprocess.STDOUT)
    unchanged = header.read_bytes() == before
    report = {'scope': 'declaration-only compile: all future Stop test branches instantiate; no Stop linked or executed',
              'command': argv, 'directory': entry['directory'], 'exit': compiled.returncode,
              'status': 'PASS' if compiled.returncode == 0 and unchanged else 'FAIL',
              'header': str(header), 'original_header_sha256': hashlib.sha256(before).hexdigest(),
              'overlay_sha256': hashlib.sha256(overlay.read_bytes()).hexdigest(), 'original_unchanged': unchanged}
    (session / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    (output / 'latest.json').write_text(json.dumps(dict(report, artifacts=str(session)), indent=2) + '\n')
    print(json.dumps(dict(report, artifacts=str(session)), indent=2))
    return int(report['status'] != 'PASS')


if __name__ == '__main__':
    raise SystemExit(main())
