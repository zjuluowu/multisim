#!/usr/bin/env python3
"""Run the existing GN real-SIM target, one scenario per process, with auditable evidence."""
import argparse
import hashlib
import fcntl
import json
import os
import shutil
import shlex
import difflib
from pathlib import Path
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
SYSTEM = Path(__file__).resolve().parents[1]
CORE = SYSTEM.parents[3]
REPO = CORE.parent
sys.path.insert(0, str(SYSTEM / 'tools'))
from host import command, dependencies, write
from build_clock import await_build_timestamps


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--gtest-root', type=Path, required=True)
    parser.add_argument('--openssl-root', type=Path, required=True)
    parser.add_argument('--runtime-root', type=Path, required=True)
    parser.add_argument('--filter', default='RealSimPrimary.*')
    parser.add_argument('--mutation-check', action='store_true')
    args = parser.parse_args()
    output = args.output.resolve()
    if output == REPO or output in REPO.parents or output == SYSTEM or SYSTEM in output.parents:
        parser.error('Use a dedicated output directory outside test sources')
    output.mkdir(parents=True, exist_ok=True)
    session = Path(tempfile.mkdtemp(prefix='execution-', dir=output))
    build = args.build_dir.resolve() if args.build_dir else session / 'build'
    if build == REPO or build in REPO.parents or build == SYSTEM or SYSTEM in build.parents:
        parser.error('Build directory must be dedicated and outside test sources')
    build.mkdir(parents=True, exist_ok=True)
    lock_stream = (build / '.sim-build.lock').open('w')
    fcntl.flock(lock_stream, fcntl.LOCK_EX)
    root = dependencies(args.gtest_root)
    native = json.loads((args.runtime_root / 'runtime_dependencies.json').read_text())
    for name, digest in native['archives'].items():
        if sha(args.runtime_root / name) != digest:
            raise RuntimeError('Native dependency drift: ' + name)
    values = {'production_root': str(CORE), 'gtest_root': str(root),
              'openssl_root': str(args.openssl_root.resolve()),
              'sim028_runtime_root': str(args.runtime_root.resolve())}
    await_build_timestamps(SYSTEM)
    gnargs = ' '.join(k + '=' + json.dumps(v) for k, v in values.items())
    commands = []
    def run(argv, name, env=None):
        code = command(argv, session / (name + '.log'), REPO, env)
        commands.append({'argv': list(map(str, argv)), 'exit_code': code, 'log': name + '.log'})
        return code
    result = {'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(),
              'status': 'BLOCKED', 'artifacts': str(session), 'commands': commands,
              'worktree': subprocess.check_output(['git', 'status', '--short'], cwd=REPO, text=True),
              'cases': [], 'native_dependencies': native,
              'provider_contract': json.loads((SYSTEM / 'primary_slot/provider/contract.json').read_text()),
              'layers': {k: 'NOT_RUN' for k in ['entry_contract', 'real_sim_manager',
                  'real_multi_sim_controller', 'branch_A', 'branch_B', 'branch_C0', 'branch_C1',
                  'real_core_service', 'stub_dispatch', 'host_ipc_boundary', 'real_ohos_ipc',
                  'ril_hdi_boundary', 'whole_component_chain']}}
    for key in ['real_core_service', 'stub_dispatch', 'host_ipc_boundary', 'real_ohos_ipc',
                'ril_hdi_boundary', 'whole_component_chain']:
        result['layers'][key] = 'BLOCKED'
    write(session / 'result.json', result)
    if run(['gn', 'gen', build, '--root=' + str(SYSTEM), '--export-compile-commands',
            '--args=' + gnargs], 'configure') or run(['ninja', '-C', build, '-j4', 'sim_primary_slot_real_test'], 'build'):
        write(session / 'result.json', result)
        return 2
    binary = build / 'sim_primary_slot_real_test'
    result['binary_sha256'] = sha(binary)
    target_commands = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands',
                                               'sim_primary_slot_real_test'], text=True)
    (session / 'target-commands.log').write_text(target_commands)
    source_paths = set()
    for line in target_commands.splitlines():
        argv = shlex.split(line)
        if '-c' in argv:
            source_paths.add((build / argv[argv.index('-c') + 1]).resolve())
    result['production_sources'] = {str(path.relative_to(CORE)): sha(path) for path in sorted(source_paths)
        if str(path).startswith(str(CORE) + '/') and '/services/multisim/test/' not in str(path)}
    env = dict(os.environ, SIM_HOST_BOOT_SLOT_COUNT='2', LLVM_PROFILE_FILE=str(session / 'discovery-%p.profraw'))
    run([binary, '--gtest_list_tests', '--gtest_filter=' + args.filter], 'list', env)
    names, suite = [], ''
    for line in (session / 'list.log').read_text().splitlines():
        line = line.split('#')[0].rstrip()
        if line and not line.startswith(' '):
            suite = line
        elif line.strip():
            names.append(suite + line.strip())
    for index, name in enumerate(names):
        label = 'case-' + str(index)
        xml = session / (label + '.xml')
        profile = session / (label + '.profraw')
        env['LLVM_PROFILE_FILE'] = str(profile)
        code = run([binary, '--gtest_filter=' + name, '--gtest_output=xml:' + str(xml)], label, env)
        case = {'name': name, 'status': 'FAIL', 'exit_code': code, 'xml': xml.name, 'executed_functions': []}
        if xml.exists():
            cases = ET.parse(xml).getroot().findall('.//testcase')
            case['tests'] = len(cases)
            case['failed'] = sum(c.find('failure') is not None for c in cases)
            case['properties'] = {p.get('name'): p.get('value') for p in cases[0].findall('./properties/property')} if len(cases) == 1 else {}
            case['failures'] = [f.text for c in cases for f in c.findall('failure')]
            if len(cases) == 1 and code == 0 and case['failed'] == 0 and cases[0].find('skipped') is None:
                case['status'] = 'PASS'
        if profile.exists():
            profdata = session / (label + '.profdata')
            if run(['llvm-profdata-18', 'merge', '-sparse', profile, '-o', profdata], label + '-merge') == 0:
                coverage = session / (label + '-coverage.json')
                with coverage.open('w') as stream:
                    subprocess.run(['llvm-cov-18', 'export', binary, '-instr-profile=' + str(profdata)], stdout=stream, check=True)
                functions = json.loads(coverage.read_text())['data'][0]['functions']
                for function in functions:
                    if function['count'] and any('services/sim/src/' in f or 'services/core/src/core_service_sim.cpp' in f for f in function['filenames']):
                        case['executed_functions'].append({'name': function['name'], 'count': function['count'], 'files': function['filenames']})
        props = case.get('properties', {})
        cleanup_only = bool(case.get('failures')) and all('REAL_GRAPH_LIFETIME_NOT_DRAINED' in f or
            'REAL_MANAGER_LIFETIME_NOT_DRAINED' in f for f in case.get('failures', []))
        case['business_status'] = ('PASS' if props.get('business_assertions_before_cleanup') == 'PASS'
            and props.get('strict_ril_expectations_verified') == 'PASS'
            and (case['status'] == 'PASS' or cleanup_only) else 'NOT_RUN')
        result['cases'].append(case)
        write(session / 'result.json', result)
    result['status'] = 'PASS' if names and all(c['status'] == 'PASS' for c in result['cases']) else 'FAIL'
    result['counts'] = {'tests': len(names), 'passed': sum(c['status'] == 'PASS' for c in result['cases']),
                        'failed': sum(c['status'] != 'PASS' for c in result['cases'])}
    for case in result['cases']:
        if case['status'] != 'PASS':
            for branch in ['B', 'C0', 'C1']:
                if ('.' + branch) in case['name']:
                    result['layers']['branch_' + branch] = 'FAIL'
            continue
        functions = '\n'.join(f['name'] for f in case['executed_functions'])
        for key, method in [('real_sim_manager', 'SimManager16SetPrimarySlotId'),
                            ('real_multi_sim_controller', 'MultiSimController16SetPrimarySlotId')]:
            if method in functions:
                result['layers'][key] = 'PASS'
        for branch in ['B', 'C0', 'C1']:
            if ('.' + branch) in case['name']:
                result['layers']['branch_' + branch] = 'PASS'
    result['lifecycle'] = {'status': 'FAIL' if any('REAL_GRAPH_LIFETIME_NOT_DRAINED' in '\n'.join(c.get('failures', [])) for c in result['cases']) else 'NOT_RUN'}
    result['business_scenario_causality'] = {
        'status': 'PASS' if any(c.get('business_status') == 'PASS' and
            all(method in '\n'.join(f['name'] for f in c['executed_functions']) for method in
                ['CoreServiceSim16SetPrimarySlotId', 'SimManager16SetPrimarySlotId',
                 'MultiSimController16SetPrimarySlotId']) for c in result['cases']) else 'FAIL',
        'scope': 'one request through CoreServiceSim/real Manager/Controller and its actual branch endpoint; lifetime failures remain FAIL',
        'whole_component_chain': 'BLOCKED'}
    result['limits'] = ['Production full-manager subscription graph remains live after manager destruction; no leak suppression', 'CoreServiceSim uses the public real SimManager attachment; CoreService/Stub not assembled',
        'RIL interface, DataShare I/O, parameters, notification transport and runtime services are Host boundaries',
        'No OHOS Binder/SA runtime or real RIL/HDI/device integration; no persistence durability claim']
    if args.mutation_check:
        source = CORE / 'services/sim/src/multi_sim_controller.cpp'
        original = source.read_bytes()
        old = b'SetParameter(PRIMARY_SLOTID_KEY.c_str(), std::to_string(slotId).c_str());'
        new = b'SetParameter(PRIMARY_SLOTID_KEY.c_str(), std::to_string(slotId + 1).c_str());'
        if original.count(old) != 1:
            raise RuntimeError('Mutation source pattern drift')
        mutant_source = session / 'mutation/multi_sim_controller.cpp'
        mutant_source.parent.mkdir()
        mutant_source.write_bytes(original.replace(old, new, 1))
        (session / 'mutation/change.patch').write_text(''.join(difflib.unified_diff(
            original.decode().splitlines(True), mutant_source.read_bytes().decode().splitlines(True),
            fromfile='original/multi_sim_controller.cpp', tofile='mutant/multi_sim_controller.cpp')))
        # Sibling output permits immutable original object reuse; GN/Ninja rebuild
        # the altered source and link a separate binary. Original output never changes.
        mutant_build = Path(tempfile.mkdtemp(prefix='sim028-mutant-', dir=build.parent))
        shutil.copytree(build, mutant_build, dirs_exist_ok=True)
        mutant_args = gnargs + ' primary_slot_controller_source=' + json.dumps(str(mutant_source))
        configured = run(['gn', 'gen', mutant_build, '--root=' + str(SYSTEM),
                          '--export-compile-commands', '--args=' + mutant_args], 'mutation-configure')
        compiled = run(['ninja', '-C', mutant_build, '-j4', 'sim_primary_slot_real_test'], 'mutation-build') if configured == 0 else 2
        mutant_xml = session / 'mutation.xml'
        mutation = {'status': 'BLOCKED', 'build_dir': str(mutant_build), 'change': 'primary slot parameter slotId -> slotId+1',
                    'production_source_sha256': hashlib.sha256(original).hexdigest(), 'build_exit_code': compiled}
        baseline = next((c for c in result['cases'] if c['name'] == 'RealSimPrimary.C0CoreToRealManagerAndController'), None)
        if compiled == 0:
            env['LLVM_PROFILE_FILE'] = str(session / 'mutation.profraw')
            code = run([mutant_build / 'sim_primary_slot_real_test',
                        '--gtest_filter=RealSimPrimary.C0CoreToRealManagerAndController',
                        '--gtest_output=xml:' + str(mutant_xml)], 'mutation-run', env)
            cases = ET.parse(mutant_xml).getroot().findall('.//testcase') if mutant_xml.exists() else []
            failures = [f.text for c in cases for f in c.findall('failure')]
            detected = (baseline is not None and baseline.get('properties', {}).get('business_assertions_before_cleanup') == 'PASS'
                        and len(cases) == 1 and code != 0 and any('PRIMARY_COMMIT_TARGET' in f for f in failures)
                        and not any('PRIMARY_COMMIT_TARGET' in f for f in baseline.get('failures', [])))
            mutation.update({'status': 'PASS' if detected else 'FAIL', 'detected': detected, 'exit_code': code,
                             'failures': failures, 'binary_sha256': sha(mutant_build / 'sim_primary_slot_real_test')})
        mutation['production_unchanged'] = source.read_bytes() == original
        mutation['original_binary_unchanged'] = sha(binary) == result['binary_sha256']
        if not mutation['production_unchanged'] or not mutation['original_binary_unchanged']:
            raise RuntimeError('Original input changed during negative control')
        result['mutation'] = mutation
    if result.get('mutation', {}).get('status') in ['FAIL', 'BLOCKED']:
        result['status'] = 'FAIL'
    write(session / 'result.json', result)
    write(output / 'latest.json', result)
    print(json.dumps({'status': result['status'], 'counts': result['counts'], 'layers': result['layers'], 'artifacts': str(session)}, indent=2))
    return int(result['status'] != 'PASS')

if __name__ == '__main__':
    raise SystemExit(main())
