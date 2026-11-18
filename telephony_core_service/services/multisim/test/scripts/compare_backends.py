#!/usr/bin/env python3
"""Normalize and compare independently executed Legacy/Target reports."""
import argparse
import json
from pathlib import Path

DEFAULT_FIELDS = ('status', 'scenario', 'oracle', 'primary_role', 'default_data_role',
                  'parameters', 'notifications', 'external_calls', 'retries')


def normalized(case):
    return {key: case.get(key) for key in DEFAULT_FIELDS if key in case}


def compare(left, right):
    index = lambda cases: {case.get('scenario') or case.get('test') or case.get('name'): normalized(case)
                           for case in cases}
    a, b = index(left), index(right)
    scenarios = sorted(set(a) & set(b))
    differences = []
    for scenario in scenarios:
        if a[scenario] != b[scenario]:
            fields = sorted(set(a[scenario]) | set(b[scenario]))
            differences.append({'scenario': scenario, 'fields': [
                {'field': field, 'legacy': a[scenario].get(field), 'target': b[scenario].get(field)}
                for field in fields if a[scenario].get(field) != b[scenario].get(field)]})
    return scenarios, differences


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--legacy', type=Path, required=True)
    parser.add_argument('--target', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    left, right = json.loads(args.legacy.read_text()), json.loads(args.target.read_text())
    scenarios, differences = compare(left.get('cases', []), right.get('cases', []))
    statuses = (left.get('status'), right.get('status'))
    status = 'FAIL' if 'FAIL' in statuses or differences else \
             'BLOCKED' if any(s in ('BLOCKED', 'NOT_RUN', 'UNSUPPORTED') for s in statuses) else 'PASS'
    result = {'legacy': left.get('status'), 'target': right.get('status'),
              'oracle': 'PASS' if status == 'PASS' and left.get('oracle_status', 'PASS') == 'PASS' and
                                   right.get('oracle_status', 'PASS') == 'PASS' else \
                        ('FAIL' if status == 'FAIL' else 'BLOCKED'),
              'legacy_target': 'FAIL' if differences else ('BLOCKED' if status != 'PASS' or not scenarios else 'PASS'),
              'overall': status, 'scenarios': scenarios, 'differences': differences}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n')
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0 if status == 'PASS' else 2 if status == 'BLOCKED' else 1


if __name__ == '__main__':
    raise SystemExit(main())
