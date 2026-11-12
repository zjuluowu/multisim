#!/usr/bin/env python3
"""Run unchanged legacy GTests; report business evidence separately, preserve raw failures.

This is an evidence adapter, not a replacement GTest suite or a leak workaround.
Each case still owns a process using the existing host.py runner.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

SYSTEM = Path(__file__).resolve().parents[1]
CORE = SYSTEM.parents[3]
REPO = CORE.parent
GOLDEN = SYSTEM / 'primary_slot/legacy_contract/golden.json'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def classify(case, returncode, log):
    """Only an exact cleanup-only failure may coexist with business PASS."""
    props = {p.get('name'): p.get('value') for p in case.findall('./properties/property')}
    failures = [f.get('message', '') + (f.text or '') for f in case.findall('failure')]
    cleanup_pattern = re.compile(
        r'^.*(?:/|^)real_scenario\.h:\d+\nValue of: '
        r'(?:SimHost::AwaitSchedulerStopped\(3s\)|retiring\.expired\(\))\n'
        r'  Actual: false\nExpected: true\nREAL_(?:GRAPH|MANAGER)_LIFETIME_NOT_DRAINED\n?$')
    clean_failure = bool(failures) and all(
        cleanup_pattern.fullmatch(f.get('message', '')) for f in case.findall('failure'))
    completed = (case.get('status') == 'run' and case.get('result') == 'completed' and
                 case.find('skipped') is None)
    unsafe = any(s in log for s in ('AddressSanitizer:', 'ThreadSanitizer:',
                                    'runtime error:', 'Segmentation fault', 'Aborted'))
    full_pass = completed and returncode == 0 and not failures and not unsafe
    # Signals, timeout, missing/contradictory XML and other assertion failures fail closed.
    business_pass = full_pass or (completed and returncode == 1 and clean_failure and not unsafe and
        props.get('business_assertions_before_cleanup') == 'PASS' and
        props.get('strict_ril_expectations_verified') == 'PASS' and
        props.get('object_lifetime') == 'FAIL')
    precondition_failure = (completed and returncode == 1 and not unsafe and bool(failures) and
        not clean_failure and 'business_assertions_before_cleanup' not in props and
        'TRACE input request=' not in log)
    bypassed = props.get('initialization_ready_idle') == 'NOT_RUN'
    if bypassed:
        business_pass = False
    return {
        'gtest': 'PASS' if full_pass else 'FAIL',
        'business': 'PASS' if business_pass else 'NOT_RUN' if precondition_failure or bypassed else 'FAIL',
        'lifecycle': props.get('object_lifetime', 'PASS' if full_pass else 'NOT_RUN'),
        'classification': 'diagnostic_barrier_not_run' if bypassed else
                          'cleanup_only' if business_pass and clean_failure else
                          'full_pass' if full_pass else 'precondition_failure' if precondition_failure else
                          'business_or_execution_failure',
        'properties': props, 'failures': failures,
        'post_destruction_callback_safety': 'NOT_VERIFIED' if clean_failure else 'NOT_RUN',
    }


def parallel_runs(args, output, host_out, names):
    """One GN/Ninja build, then the same seven GTests in isolated processes.

    Parallelism is between complete rounds, never shared SIM objects or mocks.
    Original timeouts and real 45-second timer remain unchanged.
    """
    build = [sys.executable, '-B', str(SYSTEM / 'tools/host.py'), 'build',
             '--out', str(host_out), '--production-root', str(CORE),
             '--target', 'sim_primary_slot_real_test']
    with (output / 'build.log').open('w') as stream:
        code = subprocess.run(build, cwd=REPO, stdout=stream, stderr=subprocess.STDOUT).returncode
    if code:
        raise RuntimeError(f'Build failed with exit {code}; see build.log')
    binary = host_out / 'build/sim_primary_slot_real_test'
    env = {k: v for k, v in os.environ.items() if not k.startswith(('SIM_', 'SIM028_'))}
    env['SIM_HOST_BOOT_SLOT_COUNT'] = '2'
    listing_env = dict(env, LLVM_PROFILE_FILE=str(output / 'discovery-%p.profraw'))
    listing = subprocess.check_output([binary, '--gtest_list_tests', '--gtest_filter=RealSimPrimary.*'],
                                      env=listing_env, text=True)
    discovered, suite = [], ''
    for line in listing.splitlines():
        if line and not line.startswith(' '):
            suite = line.strip()
        elif line.strip():
            discovered.append(suite + line.strip().split('#')[0].strip())
    if set(discovered) != set(names) or len(discovered) != 7:
        raise RuntimeError('Full seven-case discovery required; refuse reduced coverage')
    def round_run(repetition):
        dest = output / f'run-{repetition}'
        dest.mkdir()
        order = ('native-arrival', 'reverse', 'present-first')[repetition % 3]
        round_env = dict(env, SIM028_INIT_ORDER=order, SIM028_INIT_MODEL=args.init_model)
        cases = []
        for index, name in enumerate(discovered):
            xml = dest / f'selected-{index}.xml'
            log = dest / f'selected-{index}.log'
            argv = [str(binary), '--gtest_filter=' + name, '--gtest_output=xml:' + str(xml)]
            round_env['LLVM_PROFILE_FILE'] = str(dest / f'case-{index}-%p.profraw')
            with log.open('w') as stream:
                try:
                    code = subprocess.run(argv, env=round_env, cwd=dest, stdout=stream,
                                          stderr=subprocess.STDOUT, timeout=90).returncode
                except subprocess.TimeoutExpired:
                    code = 124
            elements = ET.parse(xml).getroot().findall('.//testcase') if xml.exists() else []
            if len(elements) == 1 and elements[0].get('classname') + '.' + elements[0].get('name') == name:
                outcome = classify(elements[0], code, log.read_text())
            else:
                outcome = {'gtest': 'FAIL', 'business': 'FAIL', 'lifecycle': 'NOT_RUN',
                           'classification': 'missing_or_wrong_xml', 'properties': {}}
            cases.append(dict(id=names[name], test=name, command=argv, raw_exit=code,
                              xml=str(xml.relative_to(output)),
                              process_ids_from_llvm_profile=[int(p.stem.split('-')[-1])
                                  for p in dest.glob(f'case-{index}-*.profraw')], **outcome))
        result = {'repetition': repetition, 'initialization_order': order, 'initialization_model': args.init_model,
                  'raw_exit': max(c['raw_exit'] if c['raw_exit'] >= 0 else 1 for c in cases), 'cases': cases}
        (dest / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        return result
    runs = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        pending = [pool.submit(round_run, index) for index in range(args.repeat)]
        for task in as_completed(pending):
            run = task.result()
            runs.append(run)
            (output / 'progress.json').write_text(json.dumps(runs, indent=2) + '\n')
            print(f'round {run["repetition"] + 1} order={run["initialization_order"]}: '
                  f'business {sum(c["business"] == "PASS" for c in run["cases"])}/7, raw exit {run["raw_exit"]}', flush=True)
    return sorted(runs, key=lambda r: r['repetition']), max(r['raw_exit'] for r in runs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host-out', type=Path, default=REPO / '.host-out')
    parser.add_argument('--output', type=Path, required=True, help='New evidence directory (never overwritten)')
    parser.add_argument('--repeat', type=int, default=1)
    parser.add_argument('--jobs', type=int, default=1, help='Independent round processes; one shared build first')
    parser.add_argument('--init-model', choices=['single-active', 'dual-active-primary0'], default='dual-active-primary0')
    args = parser.parse_args()
    if args.repeat < 1 or args.jobs < 1:
        parser.error('--repeat and --jobs must be positive')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    host_out = args.host_out.resolve()
    config = json.loads((host_out / 'host.json').read_text())
    if Path(config['production_root']).resolve() != CORE.resolve():
        raise RuntimeError('Legacy baseline requires the current unpatched production root')
    golden = json.loads(GOLDEN.read_text())
    scenarios = golden['legacy_reference_scenarios'] if args.init_model == 'single-active' and 'legacy_reference_scenarios' in golden else golden['scenarios']
    names = {s['test']: s['id'] for s in scenarios if s.get('test')}
    protected = {p: digest(p) for folder in ('services/sim', 'services/core', 'services/tel_ril',
        'services/telephony_ext_wrapper', 'utils/common') for p in (CORE / folder).rglob('*') if p.is_file()}
    result = {
        'schema_version': 1, 'adapter': 'Legacy/services/sim/unpatched',
        'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(),
        'golden_sha256': digest(GOLDEN), 'host_config': config,
        'scope': 'Host business assertions under audited external response fixtures; not OHOS golden truth',
        'whole_gtest_status': 'NOT_RUN', 'business_status': 'NOT_RUN', 'runs': [],
        'native_ohos': 'BLOCKED', 'target_adapter': 'NOT_IMPLEMENTED',
        'adapter_sha256': digest(Path(__file__)),
        'initialization_model': args.init_model,
    }
    raw_exit = 0
    if args.jobs > 1:
        result['runs'], raw_exit = parallel_runs(args, output, host_out, names)
        result['jobs'] = args.jobs
    for repetition in range(args.repeat if args.jobs == 1 else 0):
        argv = [sys.executable, '-B', str(SYSTEM / 'tools/host.py'), 'selected',
                '--out', str(host_out), '--production-root', str(CORE),
                '--target', 'sim_primary_slot_real_test', '--filter', 'RealSimPrimary.*']
        argv.extend(['--init-model', args.init_model])
        with (output / f'runner-{repetition}.log').open('w') as log:
            code = subprocess.run(argv, cwd=REPO, stdout=log, stderr=subprocess.STDOUT).returncode
        raw_exit = max(raw_exit, code if code >= 0 else 1)
        raw = json.loads((host_out / 'latest.json').read_text())
        session = Path(raw['artifacts'])
        # Never reuse stale evidence if build/discovery failed before writing latest.json.
        runner_log = (output / f'runner-{repetition}.log').read_text()
        if 'Evidence: ' + str(session) not in runner_log or raw['head'] != result['head']:
            raise RuntimeError('Runner did not complete this execution; inspect raw runner log')
        dest = output / f'run-{repetition}'
        dest.mkdir()
        for source in session.iterdir():
            if source.suffix in ('.log', '.xml', '.json'):
                shutil.copy2(source, dest / source.name)
        cases = []
        for group in raw['groups']:
            xml = dest / (group['name'] + '.xml')
            log = (dest / (group['name'] + '.log')).read_text()
            elements = ET.parse(xml).getroot().findall('.//testcase')
            if len(elements) != 1:
                raise RuntimeError('Expected exactly one isolated GTest case')
            element = elements[0]
            name = element.get('classname') + '.' + element.get('name')
            trace = [line for line in log.splitlines() if line.startswith('TRACE ')]
            cases.append(dict(id=names.get(name), test=name, raw_exit=group['exit_code'],
                              xml=str(xml.relative_to(output)), publisher_and_request_trace=trace,
                              trace_scope='Publisher calls/parameter boundary/RIL-interface request logs; no consumer or physical modem observation',
                              **classify(element, group['exit_code'], log)))
        if {c['test'] for c in cases} != set(names) or len(cases) != len(names):
            raise RuntimeError('Golden scenario discovery mismatch')
        result['runs'].append({'command': argv, 'raw_exit': code, 'cases': cases})
        print(f'run {repetition + 1}: business {sum(c["business"] == "PASS" for c in cases)}/{len(cases)}, '
              f'GTest {sum(c["gtest"] == "PASS" for c in cases)}/{len(cases)}, raw exit {code}', flush=True)
    result['production_bytes_unchanged'] = all(digest(p) == sha for p, sha in protected.items())
    result['production_sha256'] = {str(p.relative_to(CORE)): sha for p, sha in protected.items()}
    binary = host_out / 'build/sim_primary_slot_real_test'
    result['binary_sha256'] = digest(binary)
    result['toolchain'] = {name: subprocess.check_output([name, '--version'], text=True).splitlines()[0]
                           for name in ('clang++', 'gn', 'ninja', 'python3')}
    result['build_commands'] = subprocess.check_output(
        ['ninja', '-C', str(host_out / 'build'), '-t', 'commands', 'sim_primary_slot_real_test'], text=True)
    all_cases = [c for run in result['runs'] for c in run['cases']]
    result['whole_gtest_status'] = 'PASS' if all(c['gtest'] == 'PASS' for c in all_cases) else 'FAIL'
    result['business_status'] = 'PASS' if all(c['business'] == 'PASS' for c in all_cases) else 'FAIL'
    result['raw_process_exit'] = raw_exit
    result['exit_code'] = raw_exit or int(not result['production_bytes_unchanged'] or result['business_status'] != 'PASS')
    (output / 'validation.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(f'Evidence: {output}; exit {result["exit_code"]} (raw GTest failures preserved)')
    return result['exit_code']


if __name__ == '__main__':
    sys.exit(main())
