#!/usr/bin/env python3
"""Require positive production execution evidence for the migrated state slice."""
import argparse
import json
from pathlib import Path
import subprocess

REQUIRED = {
    'SimStateManager': ['Init', 'GetSimState', 'HasSimCard', 'UnlockPin', 'UnlockPuk', 'RefreshSimState'],
    'SimStateHandle': ['ProcessEvent', 'GetSimCardData', 'ProcessIccCardState', 'SimStateEscape', 'GetUnlockResult'],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--slice', choices=['state','primary','query','upstream'], default='state')
    parser.add_argument('--binary', required=True, type=Path)
    parser.add_argument('--profiles', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    profiles = sorted(args.profiles.glob('*.profraw'))
    if not profiles:
        parser.error('No execution profiles')
    args.output.mkdir(parents=True, exist_ok=True)
    merged = args.output / 'execution.profdata'
    subprocess.run(['llvm-profdata', 'merge', '-sparse', *map(str, profiles), '-o', str(merged)], check=True)
    raw = subprocess.check_output(['llvm-cov', 'export', str(args.binary), '--instr-profile=' + str(merged)])
    (args.output / 'coverage.json').write_bytes(raw)
    functions = [function for data in json.loads(raw)['data'] for function in data['functions']]
    rows = []
    required = REQUIRED if args.slice == 'state' else ({'CoreServiceSim':['SetPrimarySlotId'], 'SimManager':['SetPrimarySlotId', 'Stop'], 'MultiSimController':['SetPrimarySlotId']} if args.slice == 'primary' else ({'CoreServiceSim':['GetRealSimCount'], 'SimManager':['GetRealSimCount'], 'TelephonyExtWrapper':['GetRealSimCountExt'], 'LegacySimQueryBinding':['Read','ReadLegacyRealSimCount'], 'RealSimCountQuery':['Read']} if args.slice == 'query' else {'CoreServiceSim':['GetRealSimCount'], 'SimManager':['GetRealSimCount'], 'TelephonyExtWrapper':['GetRealSimCountExt']}))
    for owner, methods in required.items():
        for method in methods:
            needle = str(len(owner)) + owner + str(len(method)) + method + 'E'
            matched = [f for f in functions if needle in f['name'] and any(
                ('/services/sim/src/' in name or '/services/core/src/' in name or '/framework/internal/host/g2a_reference/' in name or '/services/multisim/infrastructure/compatibility/api/' in name or '/services/telephony_ext_wrapper/src/' in name) for name in f['filenames'])]
            count = sum(f['count'] for f in matched)
            rows.append({'owner': owner, 'method': method, 'count': count,
                         'files': sorted({name for f in matched for name in f['filenames']}),
                         'status': 'PASS' if count > 0 else 'FAIL'})
    result = {'scope': 'Positive execution of selected '+args.slice+' production methods; not full SIM coverage',
              'methods': rows, 'status': 'PASS' if all(r['status'] == 'PASS' for r in rows) else 'FAIL'}
    (args.output / 'execution.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    return 0 if result['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
