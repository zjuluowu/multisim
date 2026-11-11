#!/usr/bin/env python3
"""Audit real primary-slot entry execution; whole-component acceptance remains blocked."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

SYSTEM = Path(__file__).resolve().parents[1]
CORE = SYSTEM.parents[3]
REPO = CORE.parent
sys.path.insert(0, str(SYSTEM / 'tools'))
from host import command, dependencies
from build_clock import await_build_timestamps

SOURCE = 'services/core/src/core_service_sim.cpp'
SOURCES = [SOURCE, 'utils/common/src/telephony_permission.cpp',
           'common/capability_mgr/src/multi_sims_capability_manager.cpp']
REQUIRED_CHAIN = [
    ('CoreServiceStub', 'OnRemoteRequest', 'services/core/src/core_service_stub.cpp'),
    ('CoreServiceStub', 'OnSetPrimarySlotId', 'services/core/src/core_service_stub.cpp'),
    ('CoreService', 'SetPrimarySlotId', 'services/core/src/core_service.cpp'),
    ('CoreServiceSim', 'SetPrimarySlotId', SOURCE),
    ('SimManager', 'SetPrimarySlotId', 'services/sim/src/sim_manager.cpp'),
    ('MultiSimController', 'SetPrimarySlotId', 'services/sim/src/multi_sim_controller.cpp'),
    ('RadioProtocolController', 'SetRadioProtocol', 'services/sim/src/radio_protocol_controller.cpp'),
    ('TelRilManager', 'SetRadioProtocol', 'services/tel_ril/src/tel_ril_manager.cpp'),
    ('TelRilSim', 'SetRadioProtocol', 'services/tel_ril/src/tel_ril_sim.cpp'),
]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=REPO / '.host-out/primary-slot-audit')
    parser.add_argument('--gtest-root', default=os.environ.get('SIM_GTEST_ROOT'))
    parser.add_argument('--entry-only', action='store_true', help='Explicitly accept only the internal ISimManager seam')
    parser.add_argument('--mutation-check', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve()
    if out == SYSTEM or SYSTEM in out.parents or out == REPO or out in REPO.parents:
        parser.error('Output must be a dedicated directory outside test sources')
    out.mkdir(parents=True, exist_ok=True)
    lock = (out / 'host.lock').open('w')
    fcntl.flock(lock, fcntl.LOCK_EX)
    session = Path(tempfile.mkdtemp(prefix='execution-', dir=out))
    result = {'status': 'BLOCKED', 'entry_contract_status': 'NOT_RUN', 'executed': False,
              'whole_component_chain': 'BLOCKED', 'whole_chain_executed': False,
              'reason': 'No real Stub/CoreService/SimManager/Controller/Radio/TelRil closure; entry seam is partial',
              'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(),
              'sources': {p: sha(CORE / p) for p in SOURCES}, 'commands': [], 'artifacts': str(session),
              'counts': {'tests': 0, 'PASS': 0, 'FAIL': 0},
              'boundary': 'AccessToken/IPC identity/parameter service external doubles; internal ISimManager seam',
              'NOT_RUN': ['CoreServiceStub IPC dispatch/reply', 'CoreService::SetPrimarySlotId',
                          'SimManager::SetPrimarySlotId', 'MultiSimController A/B/C0/C1',
                          'RadioProtocolController', 'TelRil/HDI', 'new multisim primary switch']}

    def save():
        text = json.dumps(result, ensure_ascii=False, indent=2) + '\n'
        (session / 'result.json').write_text(text)
        (out / 'latest.json').write_text(text)

    def run(argv, label, allowed=(0,), env=None):
        if label == 'coverage':
            print('+ llvm-cov-18 export (saved to coverage.log)', flush=True)
            with (session / 'coverage.log').open('w') as stream:
                completed = subprocess.run(list(map(str, argv)), cwd=REPO, stdout=stream,
                                           stderr=subprocess.PIPE, timeout=90, text=True)
            code = completed.returncode
            if code:
                print(completed.stderr, file=sys.stderr)
        else:
            code = command(argv, session / (label + '.log'), REPO, env)
        result['commands'].append({'argv': list(map(str, argv)), 'exit_code': code, 'log': label + '.log'})
        save()
        if code not in allowed:
            raise RuntimeError(label + ' failed with exit ' + str(code))
        return code

    save()
    try:
        saved = REPO / '.host-out/host.json'
        configured = json.loads(saved.read_text()).get('gtest_root') if saved.exists() else None
        gtest = dependencies(args.gtest_root or configured)
        result['dependency_root'] = str(gtest)
        run([sys.executable, '-B', SYSTEM / 'primary_slot/generate_manager_seam.py', '--check'], 'interface')
        result['validation_inputs'] = {str(p.relative_to(CORE)): sha(p)
            for p in sorted((SYSTEM / 'primary_slot').rglob('*')) if p.is_file()}
        for p in [SYSTEM / 'BUILD.gn', SYSTEM / 'gn/BUILDCONFIG.gn', Path(__file__).resolve()]:
            result['validation_inputs'][str(p.relative_to(CORE))] = sha(p)
        await_build_timestamps(SYSTEM)
        build = out / 'build'

        def compile_target(label, replacement=''):
            gnargs = ' '.join(k + '=' + json.dumps(v) for k, v in [
                ('production_root', str(CORE)), ('gtest_root', str(gtest)),
                ('host_debug', True), ('primary_slot_core_source', replacement)])
            run(['gn', 'gen', build, '--root=' + str(SYSTEM), '--export-compile-commands', '--args=' + gnargs], label + '-gn')
            run(['ninja', '-C', build, '-j', '4', 'sim_primary_slot_entry_test'], label + '-build')

        binary = build / 'sim_primary_slot_entry_test'

        def execute(label):
            xml = session / (label + '.xml')
            profile = session / (label + '.profraw')
            env = dict(os.environ, LLVM_PROFILE_FILE=str(profile))
            code = run([binary, '--gtest_output=xml:' + str(xml)], label, (0, 1), env)
            cases = ET.parse(xml).getroot().findall('.//testcase')
            if len(cases) != 14 or any(c.find('skipped') is not None or c.get('status') == 'notrun' for c in cases):
                raise RuntimeError('Expected all 14 entry cases with no skips')
            failed = sum(c.find('failure') is not None for c in cases)
            if (code == 0) != (failed == 0):
                raise RuntimeError('GoogleTest exit/XML mismatch')
            return failed, profile

        compile_target('normal')
        result['binary_sha256'] = sha(binary)
        failed, profile = execute('normal')
        result['executed'] = True
        result['counts'] = {'tests': 14, 'PASS': 14 - failed, 'FAIL': failed}
        if failed:
            raise RuntimeError('Entry contract has assertion failures')
        profdata = session / 'normal.profdata'
        run(['llvm-profdata-18', 'merge', '-sparse', profile, '-o', profdata], 'merge')
        run(['llvm-cov-18', 'export', binary, '-instr-profile=' + str(profdata)], 'coverage')
        coverage = json.loads((session / 'coverage.log').read_text())
        functions = [f for block in coverage['data'] for f in block['functions']]
        audits = []
        for owner, method, source in [
            ('CoreServiceSim', 'SetPrimarySlotId', SOURCE),
            ('TelephonyPermission', 'CheckCallerIsSystemApp', SOURCES[1]),
            ('TelephonyPermission', 'CheckPermission', SOURCES[1]),
            ('MultiSimsCapabilityMgr', 'IsMultiSimsCapabilitySupported', SOURCES[2])]:
            token = str(len(owner)) + owner + str(len(method)) + method
            matches = [f for f in functions if token in f['name'] and f['count'] > 0 and
                       any(Path(name).resolve() == CORE / source for name in f['filenames'])]
            if not matches:
                raise RuntimeError('Missing real production execution: ' + owner + '::' + method)
            audits.append({'method': owner + '::' + method, 'source': source,
                           'executions': sum(f['count'] for f in matches)})
            if owner == 'CoreServiceSim':
                target = matches[0]
                if len(target['branches']) != 6 or any(b[4] == 0 or b[5] == 0 for b in target['branches']):
                    raise RuntimeError('Primary entry requires both outcomes of all six guards')
                if any(r[4] == 0 for r in target['regions'] if r[7] == 0):
                    raise RuntimeError('Primary entry has an unexecuted code region')
                result['entry_branches'] = target['branches']
                result['entry_code_regions'] = target['regions']
        result['production_execution'] = audits
        result['whole_chain_execution_audit'] = []
        for owner, method, source in REQUIRED_CHAIN:
            token = str(len(owner)) + owner + str(len(method)) + method
            count = sum(f['count'] for f in functions if token in f['name'] and
                        any(Path(name).resolve() == CORE / source for name in f['filenames']))
            result['whole_chain_execution_audit'].append(
                {'method': owner + '::' + method, 'source': source, 'executions': count,
                 'status': 'PASS' if count > 0 else 'NOT_RUN'})
        result['missing_chain_functions'] = [x['method'] for x in result['whole_chain_execution_audit']
                                             if x['executions'] == 0]
        # Aggregate function counts alone would still not prove one scenario's
        # Stub->RIL causality or coverage of A/B/C0/C1. No full-chain claim here.
        result['whole_chain_scenario_causality'] = 'NOT_RUN'
        if args.mutation_check:
            original = (CORE / SOURCE).read_text()
            anchor = 'return simManager_->SetPrimarySlotId(slotId, true);'
            if original.count(anchor) != 1:
                raise RuntimeError('Mutation anchor drift')
            mutated = session / 'core_service_sim_mutated.cpp'
            mutated.write_text(original.replace(anchor, 'return simManager_->SetPrimarySlotId(slotId, false);'))
            try:
                compile_target('mutation', str(mutated))
                mutation_failures, _ = execute('mutation')
                result['mutation'] = {'detected': mutation_failures > 0, 'assertion_failures': mutation_failures,
                                      'change': 'userSet true -> false in outside-repository source copy'}
                if not mutation_failures:
                    raise RuntimeError('Production forwarding mutation was not detected')
            finally:
                compile_target('restore')
            restored_failures, _ = execute('restored')
            if restored_failures:
                raise RuntimeError('Restored production entry failed')
            result['restored_binary_sha256'] = sha(binary)
        result['entry_contract_status'] = 'PASS'
        result['status'] = 'PASS' if args.entry_only else 'BLOCKED'
        save()
        return 0 if args.entry_only else 2
    except Exception as error:
        result['error'] = str(error)
        result['status'] = 'FAIL' if result['executed'] else 'BLOCKED'
        save()
        return 1 if result['executed'] else 2
    finally:
        print(json.dumps({k: result.get(k) for k in ['status', 'entry_contract_status', 'whole_component_chain', 'counts', 'error']}))
        print('Evidence: ' + str(session))


if __name__ == '__main__':
    sys.exit(main())
