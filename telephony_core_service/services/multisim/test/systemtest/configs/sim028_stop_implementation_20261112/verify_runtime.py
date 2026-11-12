#!/usr/bin/env python3
"""Execute the existing linked GTests in fresh processes; preserve every failure."""
import argparse
import fnmatch
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import xml.etree.ElementTree as ET


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--binary', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--rounds', type=int, default=1)
    p.add_argument('--critical-only', action='store_true')
    p.add_argument('--strong-ril', action='store_true')
    p.add_argument('--timeout', type=int, default=120)
    a = p.parse_args()
    binary = a.binary.resolve()
    output = a.output.resolve()
    if not binary.is_file() or a.rounds < 1 or output.exists():
        p.error('Use an existing Linux binary, positive rounds and a NEW output directory')
    output.mkdir(parents=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith('SIM_')}
    env['SIM_HOST_BOOT_SLOT_COUNT'] = '2'
    env['LLVM_PROFILE_FILE'] = str(output / 'current.profraw')
    if a.strong_ril:
        env['SIM_HOST_STRONG_RIL_SUBSCRIPTIONS'] = '1'
    listing = subprocess.run([str(binary), '--gtest_list_tests'], env=env, cwd=output,
                             capture_output=True, text=True)
    (output / 'discovery.log').write_text(listing.stdout + listing.stderr)
    if listing.returncode:
        return listing.returncode
    names, suite = [], ''
    for line in listing.stdout.splitlines():
        line = line.split('#')[0].rstrip()
        if line.startswith('  '):
            names.append(suite + line.strip())
        elif line and not line.startswith('Running'):
            suite = line.strip()
    if a.critical_only:
        names = [n for n in names if n.startswith(('ShutdownContract.', 'ShutdownExtra.')) or n in (
            'RealSimPrimary.C1ThreePhasesWithStaleCheckResponseRejected',
            'RealSimPrimary.BDirectRilSuccessCommitsAfterReply')]
    if not names:
        raise RuntimeError('Zero selected tests')
    result = {'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
              'environment': {k: env[k] for k in ('SIM_HOST_BOOT_SLOT_COUNT',
                              'SIM_HOST_STRONG_RIL_SUBSCRIPTIONS') if k in env}, 'cases': []}
    for round_id in range(a.rounds):
        for case_id, name in enumerate(names):
            label = f'round-{round_id}-case-{case_id}'
            xml = output / (label + '.xml')
            command = [str(binary), '--gtest_filter=' + name, '--gtest_output=xml:' + str(xml)]
            started = time.monotonic()
            with (output / (label + '.log')).open('w') as log:
                try:
                    code = subprocess.run(command, cwd=output, env=env, stdout=log,
                                          stderr=subprocess.STDOUT, timeout=a.timeout).returncode
                except subprocess.TimeoutExpired:
                    code = 124
            valid = False
            if xml.exists():
                cases = ET.parse(xml).getroot().findall('.//testcase')
                valid = len(cases) == 1 and all(not t.findall('failure') and not t.findall('skipped')
                                               and t.get('status') != 'notrun' for t in cases)
            result['cases'].append({'round': round_id, 'name': name, 'command': command,
                                    'exit_code': code, 'valid_xml': valid,
                                    'status': 'PASS' if code == 0 and valid else 'FAIL',
                                    'seconds': time.monotonic() - started})
            (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    result['exit_code'] = int(any(t['status'] != 'PASS' for t in result['cases']))
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(f"Executed {len(result['cases'])}; failed {sum(t['status'] == 'FAIL' for t in result['cases'])}")
    return result['exit_code']


if __name__ == '__main__':
    sys.exit(main())
