#!/usr/bin/env python3
"""Discover GoogleTests and execute each in a fresh process, preserving raw evidence."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from primary_results import classify as classify_primary

REPO = Path(__file__).resolve().parents[5]


def discover(binary, pattern, env, timeout, allow_empty=False):
    completed = subprocess.run([str(binary), '--gtest_list_tests', '--gtest_filter=' + pattern],
                               text=True, capture_output=True, env=env, timeout=timeout)
    if completed.returncode:
        raise RuntimeError('Test discovery failed: ' + completed.stdout + completed.stderr)
    suite = ''
    cases = []
    for raw in completed.stdout.splitlines():
        line = raw.split('#', 1)[0].rstrip()
        if not line:
            continue
        if not line.startswith(' ') and line.endswith('.'):
            suite = line
        elif line.startswith('  ') and suite:
            cases.append(suite + line.strip())
    if (not cases and not allow_empty) or len(cases) != len(set(cases)):
        raise RuntimeError('No tests matched, or discovery produced duplicate IDs')
    return cases, completed.stdout


def classify(xml, expected, exit_code):
    if exit_code == 124:
        return 'FAIL', 'Process timed out'
    if not xml.exists():
        return 'FAIL', 'Missing GoogleTest XML'
    try:
        root = ET.parse(xml).getroot()
    except ET.ParseError as error:
        return 'FAIL', 'Invalid XML: ' + str(error)
    cases = list(root.iter('testcase'))
    if len(cases) != 1:
        return 'FAIL', f'Expected one executed test, received {len(cases)}'
    case = cases[0]
    actual = case.get('classname', '') + '.' + case.get('name', '')
    if actual != expected:
        return 'FAIL', 'Executed ID differs from selected ID: ' + actual
    if exit_code or case.find('failure') is not None:
        return 'FAIL', '\n'.join(n.text or n.get('message', '') for n in case.findall('failure')) or f'Exit {exit_code}'
    if case.get('status') == 'notrun':
        return 'NOT_RUN', 'GoogleTest did not execute the selected case'
    skipped = case.find('skipped')
    if skipped is not None:
        return ('BLOCKED' if 'BLOCKED:' in (skipped.text or skipped.get('message', '')) else 'UNSUPPORTED'), skipped.text or skipped.get('message', 'Platform capability unavailable')
    return 'PASS', ''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--filter', default='*')
    parser.add_argument('--timeout', type=float, default=60)
    parser.add_argument('--list', action='store_true')
    parser.add_argument('--env', action='append', default=[], metavar='KEY=VALUE')
    parser.add_argument('--input', action='append', default=[], type=Path,
                        help='Additional external input, such as a controlled mutation source')
    args = parser.parse_args()
    binary = args.binary.resolve()
    if args.timeout <= 0:
        parser.error('--timeout must be positive')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    session = Path(tempfile.mkdtemp(prefix='gtest-', dir=output))
    env = dict(os.environ, LLVM_PROFILE_FILE=str(session / 'coverage-%p.profraw'),
               LD_LIBRARY_PATH=str(binary.parent) + ':' + os.environ.get('LD_LIBRARY_PATH', ''))
    for value in args.env:
        key, value = value.split('=', 1)
        if key == 'LLVM_PROFILE_FILE': parser.error('profile isolation cannot be overridden')
        env[key] = value
    protected = REPO / 'default.profraw'
    protected_sha = hashlib.sha256(protected.read_bytes()).hexdigest() if protected.is_file() else None
    if not binary.is_file() or not os.access(binary, os.X_OK):
        report = session / 'result.json'
        report.write_text(json.dumps({'status': 'BLOCKED', 'target': binary.name,
            'platform': platform.platform(), 'head': subprocess.check_output(
                ['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(),
            'reason': 'Test executable is missing or not executable: ' + str(binary),
            'exit_code': 2, 'cases': []}, indent=2) + '\n')
        print('Report: ' + str(report), flush=True)
        return 2
    result = {'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(),
              'platform': platform.platform(), 'target': binary.name, 'binary': str(binary),
              'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
              'started_utc': datetime.now(timezone.utc).isoformat(), 'cases': [], 'status': 'NOT_RUN'}
    gnargs = binary.parent / 'args.gn'
    if gnargs.is_file():
        result['gn_args'] = gnargs.read_text()
    result['external_inputs'] = {str(path.resolve()): hashlib.sha256(path.read_bytes()).hexdigest()
                                 for path in args.input}
    result['git_status'] = subprocess.check_output(['git', 'status', '--short'], cwd=REPO, text=True)
    inputs = subprocess.check_output(['git', 'ls-files', '-c', '-o', '--exclude-standard', '-z',
                                      '--', 'telephony_core_service'], cwd=REPO).split(b'\0')
    result['source_sha256'] = {}
    for encoded in inputs:
        if not encoded:
            continue
        path = REPO / os.fsdecode(encoded)
        if '/docs/archive/' in path.as_posix() or path.suffix not in ('.cpp', '.h', '.gn', '.gni', '.py'):
            continue
        if path.is_file():
            result['source_sha256'][path.relative_to(REPO).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    report = session / 'result.json'

    def save():
        temporary = report.with_suffix('.tmp')
        temporary.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
        temporary.replace(report)

    save()
    try:
        cases, listing = discover(binary, args.filter, env, args.timeout)
        (session / 'discovery.txt').write_text(listing)
        if args.list:
            print(listing, end='')
            result['status'] = 'NOT_RUN'
            result['reason'] = 'Discovery only'
            return 0
        for index, case in enumerate(cases):
            xml = session / f'{index:04}.xml'
            log = session / f'{index:04}.log'
            command = [str(binary), '--gtest_filter=' + case, '--gtest_output=xml:' + str(xml)]
            with log.open('w') as stream:
                process = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT,
                                           env=env, cwd=session, start_new_session=True)
                try:
                    code = process.wait(timeout=args.timeout)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait()
                    code = 124
            status, reason = classify(xml, case, code)
            details = {}
            if xml.is_file():
                try:
                    nodes = list(ET.parse(xml).getroot().iter('testcase'))
                    if len(nodes) == 1 and nodes[0].get('classname', '') + '.' + nodes[0].get('name', '') == case:
                        details = classify_primary(nodes[0], code, log.read_text())
                except (ET.ParseError, StopIteration):
                    pass
            unsafe = any(marker in log.read_text() for marker in ['AddressSanitizer:', 'ThreadSanitizer:', 'runtime error:', 'Segmentation fault', 'Aborted'])
            if unsafe:
                status, reason = 'FAIL', 'Unsafe runtime diagnostic: ' + reason
            result['cases'].append({'unsafe_runtime': unsafe, 'valid_xml': bool(details), 'details': details, 'id': case, 'status': status, 'exit_code': code, 'reason': reason,
                                    'command': command, 'xml': xml.name, 'log': log.name})
            print(f'{case}: {status} (exit {code})', flush=True)
            save()
        statuses = {row['status'] for row in result['cases']}
        result['status'] = 'FAIL' if 'FAIL' in statuses else 'BLOCKED' if 'BLOCKED' in statuses else 'UNSUPPORTED' if statuses - {'PASS'} else 'PASS'
        return 1 if result['status'] == 'FAIL' else 2 if result['status'] != 'PASS' else 0
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        result['status'] = 'FAIL'
        result['reason'] = str(error)
        return 1
    finally:
        result['protected_profile_unchanged'] = protected_sha == (hashlib.sha256(protected.read_bytes()).hexdigest() if protected.is_file() else None)
        if not result['protected_profile_unchanged']:
            result['status'] = 'FAIL'
            result['reason'] = 'Protected pre-existing profile changed'
        result['finished_utc'] = datetime.now(timezone.utc).isoformat()
        save()
        print('Report: ' + str(report), flush=True)
        if not result['protected_profile_unchanged']: return 1


if __name__ == '__main__':
    raise SystemExit(main())
