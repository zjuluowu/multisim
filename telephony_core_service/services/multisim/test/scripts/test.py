#!/usr/bin/env python3
"""Unified orchestration; child reports and their original exit codes remain authoritative."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from _paths import TEST, REPO

SUITES = ('all', 'architecture', 'host', 'compat', 'defect', 'pin', 'g2a',
          'primary', 'target', 'technical', 'integrity')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite', choices=SUITES, default='all')
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--gtest-root', default=os.environ.get('SIM_GTEST_ROOT'))
    parser.add_argument('--openssl-root')
    parser.add_argument('--runtime-root')
    parser.add_argument('--mutation-check', action='store_true')
    parser.add_argument('--jobs', default='3')
    args = parser.parse_args()
    output = args.output.resolve()
    if output == REPO or REPO in output.parents or output in REPO.parents:
        parser.error('--output must be a dedicated directory outside the repository')
    output.mkdir(parents=True, exist_ok=True)
    session = Path(tempfile.mkdtemp(prefix='test-', dir=output))
    rows = []

    def run(name, script, options=()):
        command = [sys.executable, '-B', str(TEST / script), *map(str, options)]
        env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1',
                   LLVM_PROFILE_FILE=str(session / (name + '-%p.profraw')))
        with (session / (name + '.log')).open('w') as log:
            code = subprocess.run(command, cwd=REPO, env=env,
                                  stdout=log, stderr=subprocess.STDOUT).returncode
        row = {'suite': name, 'command': command, 'exit_code': code,
               'status': 'PASS' if code == 0 else 'BLOCKED' if code == 2 else 'FAIL',
               'log': str(session / (name + '.log'))}
        rows.append(row)
        (session / 'result.json').write_text(json.dumps({'commands': rows,
            'counting': 'Child reports retain business/GTest/defect and mutation counts; commands are not business cases.'}, indent=2) + '\n')
        print(f'{name}: {row["status"]} (exit {code}); {row["log"]}', flush=True)
        return code

    selected = ['integrity', 'architecture', 'technical', 'host', 'g2a', 'primary', 'target'] if args.suite == 'all' else [args.suite]
    if args.suite == 'all' and args.mutation_check:
        selected += ['compat', 'pin']
    dependency = ['--gtest-root', args.gtest_root] if args.gtest_root else []
    mutation = ['--mutation-check'] if args.mutation_check else []
    for name in selected:
        dest = session / name
        if name == 'architecture':
            run(name, 'scripts/run_architecture.py', ['--with-gn', '--migration', '--output', dest])
        elif name == 'integrity':
            run(name, 'scripts/check_test_integrity.py')
            run('relocation', 'scripts/check_relocation.py')
        elif name == 'technical':
            for key, script in [('source-tables', 'cases/architecture/test_g2a_source_tables.py'),
                                ('oracle', 'cases/primary_slot/oracle/test_primary_oracle.py'),
                                ('classifier', 'cases/infrastructure/test_legacy_primary_contract.py'),
                                ('ci-policy', 'cases/infrastructure/test_ci_sim028.py')]:
                run(key, script, ['-v'])
        elif name == 'host':
            run(name, 'scripts/host.py', ['all', '--out', dest, '--jobs', args.jobs, *dependency])
        elif name in ('compat', 'defect'):
            run(name, 'scripts/run_sim_guard.py', ['--suite', name, '--output', dest, *dependency, *mutation])
        elif name == 'pin':
            run(name, 'scripts/run_pin_memory_contract.py', ['--output', dest, *dependency, *mutation])
        elif name == 'g2a':
            run(name, 'scripts/run_g2a_query.py', ['--output', dest, *dependency])
        elif name == 'target':
            run(name, 'scripts/run_primary_contract.py', ['--adapter', 'target', '--output', dest])
        elif name == 'primary':
            options = ['configure', '--out', dest / 'host', '--target', 'sim_primary_slot_real_test', *dependency]
            for flag, value in [('--openssl-root', args.openssl_root), ('--runtime-root', args.runtime_root)]:
                if value:
                    options += [flag, value]
            if run('primary-prepare', 'scripts/host.py', options) == 0:
                run(name, 'scripts/run_primary_contract.py',
                    ['--adapter', 'legacy', '--host-out', dest / 'host', '--output', dest / 'results'])
    failed = any(r['status'] == 'FAIL' for r in rows)
    blocked = any(r['status'] == 'BLOCKED' for r in rows)
    print('Reports: ' + str(session))
    return 1 if failed else 2 if blocked else 0


if __name__ == '__main__':
    sys.exit(main())
