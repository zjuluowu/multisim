#!/usr/bin/env python3
"""Incremental Linux IDE entry over the existing GN root and frozen SIM guard."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import signal
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

SYSTEM = Path(__file__).resolve().parents[1]
CORE = SYSTEM.parents[3]
REPO = CORE.parent
GUARD = SYSTEM.parent / 'sim_guard'
sys.path[:0] = [str(SYSTEM / 'scripts'), str(GUARD)]
from run_g2a_query import digest_tree
from build_clock import await_build_timestamps
from run_sim_guard import check_semantics, PRODUCTION

TARGETS = ['sim_guard_legacy', 'sim_guard_pin_memory', 'platform_adapter_test', 'sim_component_test',
           'sim_primary_slot_entry_test']


class BuildFailed(RuntimeError):
    pass


def write(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')


def command(argv, log, cwd, env=None, timeout=900):
    print('+ ' + ' '.join(map(str, argv)), flush=True)
    with log.open('w') as stream:
        process = subprocess.Popen(list(map(str, argv)), cwd=cwd, env=env,
                                   stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
            stream.write('\nTIMEOUT\n')
            code = 124
    print(log.read_text(), end='', flush=True)
    return code


def dependencies(root):
    if not root:
        raise RuntimeError('Set SIM_GTEST_ROOT or configure --gtest-root to the pinned source; see HOST.md')
    root = Path(root).resolve()
    lock = json.loads((SYSTEM / 'configs/dependency_lock.json').read_text())
    if digest_tree(root) != lock['googletest']['tree_sha256']:
        raise RuntimeError('GoogleTest tree mismatch; use prepare_dependencies.py with the locked archive')
    for entry in lock['platform_headers']:
        if hashlib.sha256((SYSTEM / entry['path']).read_bytes()).hexdigest() != entry['sha256']:
            raise RuntimeError('Platform header drift: ' + entry['path'])
    check_semantics()
    return root


def configure(args, out):
    if platform.system() != 'Linux' or platform.machine() != 'x86_64':
        raise RuntimeError('Host requires Linux x86_64; open this workspace through Remote-WSL')
    for tool in ['gn', 'ninja', 'clang++', 'ld.gold', 'ar']:
        location = shutil.which(tool)
        if not location or location.endswith('.exe') or location.startswith('/mnt/'):
            raise RuntimeError('Missing native Linux tool: ' + tool)
    saved = out / 'host.json'
    previous = json.loads(saved.read_text()) if saved.exists() else {}
    root = dependencies(args.gtest_root or os.environ.get('SIM_GTEST_ROOT') or previous.get('gtest_root'))
    await_build_timestamps(SYSTEM)
    gnargs = ' '.join(f'{key}={json.dumps(value)}' for key, value in
                      [('gtest_root', str(root)), ('production_root', str(CORE)), ('host_debug', True)])
    code = command(['gn', 'gen', out / 'build', '--root=' + str(SYSTEM),
                    '--export-compile-commands', '--args=' + gnargs], out / 'configure.log', REPO)
    if code:
        raise BuildFailed('GN failed; see configure.log')
    write(saved, {'gtest_root': str(root), 'production_root': str(CORE), 'host_debug': True})


def build(args, out):
    configure(args, out)
    code = command(['ninja', '-C', out / 'build', '-j', args.jobs, *TARGETS], out / 'build.log', REPO)
    if code:
        raise BuildFailed('Ninja failed; see build.log')


def run_tests(args, out):
    build(args, out)
    runs = out / 'runs'
    runs.mkdir(exist_ok=True)
    import tempfile
    session = Path(tempfile.mkdtemp(prefix='run-', dir=runs))
    result = {'status': 'BLOCKED', 'head': subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(), 'groups': [],
        'scope': 'legacy_card_state_component_and_primary_entry_internal_seam', 'artifacts': str(session),
        'primary_whole_component_chain': 'BLOCKED',
        'uncovered': ['full_Core_Init', 'SimManager_OnInit', 'account', 'default_roles', 'persistence', 'eSIM', 'IPC', 'device'],
        'production_sources': {p: hashlib.sha256((CORE / p).read_bytes()).hexdigest() for p in PRODUCTION + [
            'services/core/src/core_service_sim.cpp', 'utils/common/src/telephony_permission.cpp',
            'common/capability_mgr/src/multi_sims_capability_manager.cpp']}}
    write(session / 'result.json', result)
    groups = [('compatibility', TARGETS[0], '*-*DEFECT_*'),
              ('known_defects', TARGETS[0], '*DEFECT_*'),
              ('pin_memory', TARGETS[1], '*'), ('infrastructure', TARGETS[2], '*'),
              ('capacity', TARGETS[3], '*'), ('primary_entry_internal_seam', TARGETS[4], '*')]
    if args.action == 'selected':
        groups = [('selected', args.target, args.filter)]
    failed = False
    catalog = json.loads((GUARD / 'traceability/round2_catalog.json').read_text())
    for name, target, test_filter in groups:
        xml = session / (name + '.xml')
        env = dict(os.environ, LLVM_PROFILE_FILE=str(session / (name + '-%p.profraw')))
        # Explicit scenario inputs; never inherit fault-injection controls from a shell.
        for key in list(env):
            if key.startswith('SIM_') and key != 'SIM_GTEST_ROOT':
                del env[key]
        code = command([out / 'build' / target, '--gtest_filter=' + test_filter,
                        '--gtest_output=xml:' + str(xml)], session / (name + '.log'), session, env, 90)
        group = {'name': name, 'exit_code': code, 'status': 'FAIL', 'tests': 0, 'passed': 0, 'failed': 0,
                 'binary_sha256': hashlib.sha256((out / 'build' / target).read_bytes()).hexdigest(),
                 'filter': test_filter}
        if xml.exists():
            cases = ET.parse(xml).getroot().findall('.//testcase')
            group['tests'] = len(cases)
            group['failed'] = sum(c.find('failure') is not None for c in cases)
            skipped = sum(c.find('skipped') is not None or c.get('status') == 'notrun' for c in cases)
            group['skipped'] = skipped
            group['passed'] = len(cases) - group['failed'] - skipped
            names = {c.get('classname') + '.' + c.get('name') for c in cases}
            expected = None
            if name == 'compatibility':
                expected = set(catalog['baseline_compatibility']) | {c['test'] for c in catalog['extension_compatibility']}
            if name == 'known_defects':
                expected = {c['test'] for c in catalog['defects']}
            metadata_valid = True
            if name == 'infrastructure':
                registered = json.loads((SYSTEM / 'configs/infrastructure_cases.json').read_text())['cases']
                observed = [{p.get('name'): p.get('value') for p in c.findall('./properties/property')} for c in cases]
                metadata_valid = len(cases) == len(registered) and sorted(
                    (p.get('case_id', ''), p.get('oracle_id', '')) for p in observed) == sorted(
                    (p['case_id'], p['oracle_id']) for p in registered)
            expected_count = {'pin_memory': 4, 'capacity': 1, 'primary_entry_internal_seam': 14}.get(name)
            collection_valid = (expected is None or (names == expected and len(cases) == len(expected)))
            collection_valid &= expected_count is None or len(cases) == expected_count
            if cases and not skipped and metadata_valid and collection_valid and code == 0 and group['failed'] == 0:
                group['status'] = 'PASS'
        failed |= group['status'] != 'PASS'
        result['groups'].append(group)
        write(session / 'result.json', result)
    result['status'] = 'FAIL' if failed else 'PASS'
    write(session / 'result.json', result)
    write(out / 'latest.json', result)
    print(json.dumps(result['groups'], indent=2))
    print('Evidence: ' + str(session))
    return int(failed)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['configure', 'build', 'all', 'selected', 'clean', 'debug'])
    parser.add_argument('--out', type=Path, default=REPO / '.host-out')
    parser.add_argument('--gtest-root')
    parser.add_argument('--jobs', default='4')
    parser.add_argument('--target', choices=TARGETS, default=TARGETS[0])
    parser.add_argument('--filter', default='*')
    args = parser.parse_args()
    out = args.out.resolve()
    if out == REPO or out in REPO.parents or out == SYSTEM or SYSTEM in out.parents:
        parser.error('Output must be a dedicated build directory, outside the test sources')
    out.mkdir(parents=True, exist_ok=True)
    # Tasks, F5 and terminal commands can overlap; never run two Ninjas on one output.
    lock_stream = (out / 'host.lock').open('w')
    fcntl.flock(lock_stream, fcntl.LOCK_EX)
    try:
        if args.action == 'configure':
            configure(args, out)
        elif args.action == 'build':
            build(args, out)
        elif args.action == 'debug':
            build(args, out)
            return subprocess.call(['lldb', '--', str(out / 'build' / args.target),
                                    '--gtest_filter=' + args.filter], cwd=out)
        elif args.action == 'clean':
            # Ninja only removes its generated outputs. Never recursively delete a user directory.
            if not (out / 'host.json').exists():
                raise RuntimeError('Refuse clean: no host.json from this entry')
            return command(['ninja', '-C', out / 'build', '-t', 'clean'], out / 'clean.log', REPO)
        else:
            return run_tests(args, out)
    except (RuntimeError, OSError, ValueError) as error:
        status = 'FAIL' if isinstance(error, BuildFailed) else 'BLOCKED'
        write(out / 'error.json', {'status': status, 'action': args.action, 'error': str(error), 'time': time.time()})
        print(status + ': ' + str(error), file=sys.stderr)
        return 1 if status == 'FAIL' else 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
