#!/usr/bin/env python3
"""Run one GoogleTest under LLDB and retain first-stop and exit evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import tempfile
from time import perf_counter
import xml.etree.ElementTree as ET
from run_gtest import discover, host_environment, parse_listing


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--filter', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--breakpoint', default='OHOS::Telephony::SimStateManager::UnlockPin')
    parser.add_argument('--discovery-file', type=Path, help=argparse.SUPPRESS)
    parser.add_argument('--env', action='append', default=[])
    parser.add_argument('--slot', type=int)
    parser.add_argument('--primary-cleanup-diagnostic', action='store_true')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    session = Path(tempfile.mkdtemp(prefix='debug-', dir=args.output.resolve()))
    binary = args.binary.resolve()
    overrides = (dict(SIM_HOST_BOOT_SLOT_COUNT='2', SIM028_INIT_MODEL='single-active')
                 if binary.name in ('sim_primary_slot_real_test', 'sim_bootstrap_characterization') else {})
    overrides.update(dict(value.split('=', 1) for value in args.env))
    if 'LLVM_PROFILE_FILE' in overrides:
        parser.error('profile isolation cannot be overridden')
    env = host_environment(binary, session/'coverage-%p.profraw', overrides)
    started = perf_counter()
    if args.discovery_file:
        cases = parse_listing(args.discovery_file.read_text())
    else:
        cases, _ = discover(binary, args.filter, env, 60)
    discovery_seconds = perf_counter() - started
    if len(cases) != 1:
        raise SystemExit('Debug requires one selected case')
    slot_required = args.slot is not None or any(
        name in args.breakpoint for name in ('SimStateManager', 'CoreServiceSim'))
    command = ['lldb', '--batch', '-O', 'settings set symbols.enable-external-lookup false',
               '-o', 'settings set target.disable-aslr false',
               '-o', 'breakpoint set -n '+args.breakpoint, '-o', 'run', '-o', 'thread backtrace']
    if slot_required:
        command += ['-o', 'frame variable slotId']
    command += ['-o', 'breakpoint disable 1', '-o', 'continue', '--', str(binary),
                '--gtest_filter='+cases[0], '--gtest_output=xml:'+str(session/'case.xml')]
    first_stop = None
    started = perf_counter()
    with (session/'lldb.log').open('w') as log, subprocess.Popen(
            command, env=env, cwd=session, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True) as debugger:
        for line in debugger.stdout:
            if first_stop is None and re.search(r'stop reason = breakpoint 1(?:\.|\b)', line):
                first_stop = perf_counter() - started
            log.write(line)
        code = debugger.wait()
    elapsed = perf_counter() - started
    started = perf_counter()
    text = (session/'lldb.log').read_text()
    expected = 1 if args.primary_cleanup_diagnostic else 0
    slots = [int(value) for value in re.findall(r'\bslotId = (-?\d+)', text)]
    symbol_hit = args.breakpoint.rsplit('::', 1)[-1] in text
    passed = (code == 0 and first_stop is not None and symbol_hit
              and (not slot_required or bool(slots)) and (args.slot is None or args.slot in slots)
              and '1 test from 1 test suite ran.' in text and f'exited with status = {expected}' in text)
    classification = None
    if args.primary_cleanup_diagnostic:
        from primary_results import classify
        xml = session/'case.xml'
        rows = list(ET.parse(xml).getroot().iter('testcase')) if xml.is_file() else []
        if len(rows) == 1:
            classification = classify(rows[0], 1, text)
        passed = passed and classification is not None and classification['classification'] == 'cleanup_only'
    result = {'status': 'PASS' if passed else 'FAIL', 'scope': 'debugger verification',
              'command': command, 'debugger_exit_code': code, 'expected_sut_exit_code': expected,
              'primary_result': classification, 'observed_slots': slots, 'profile': env['LLVM_PROFILE_FILE'],
              'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(), 'platform': platform.platform(),
              'timings': {'discovery_seconds': discovery_seconds, 'discovery_processes': int(not args.discovery_file),
                          'lldb_seconds': elapsed, 'first_breakpoint_seconds': first_stop,
                          'classification_and_provenance_seconds': perf_counter() - started}}
    (session/'result.json').write_text(json.dumps(result, indent=2)+'\n')
    print(session)
    return int(not passed)


if __name__ == '__main__':
    raise SystemExit(main())
