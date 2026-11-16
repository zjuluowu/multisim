#!/usr/bin/env python3
"""Compare migrated cases against raw original XML, retaining unmigrated rows."""
import argparse
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET


def read_cases(directory):
    result = {}
    for path in directory.glob('*.xml'):
        for case in ET.parse(path).getroot().iter('testcase'):
            name = case.get('classname') + '.' + case.get('name')
            if name in result:
                raise RuntimeError('Duplicate case in input reports: ' + name)
            result[name] = {'status': 'FAIL' if case.find('failure') is not None else
                            'UNSUPPORTED' if case.find('skipped') is not None else
                            'NOT_RUN' if case.get('status') == 'notrun' else 'PASS',
                            'failures': [re.sub(r'^.*?:\d+\n', '', f.get('message', ''))
                                         for f in case.findall('failure')],
                            'properties': {p.get('name'): p.get('value') for p in case.findall('properties/property')}}
    return result


def replacement_id(old):
    if old.startswith('ContractDriver.'):
        return old.replace('ContractDriver.', 'SimAdvancedTest.', 1)
    if old.startswith('Compatibility/Lifecycle.PublicCardState/'):
        return 'SimTest.' + old.rsplit('/', 1)[1]
    return old


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', required=True, type=Path)
    parser.add_argument('--after', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    before, after = read_cases(args.before), read_cases(args.after)
    if not before or not after:
        parser.error('Both inputs must contain raw XML')
    rows = []
    observation_keys = ('observations', 'external_calls', 'pending_responses',
                        'real_instances_created', 'real_instances_destroyed')
    for old, original in sorted(before.items()):
        new = replacement_id(old)
        migrated = after.get(new)
        row = {'old': old, 'new': new, 'before': original, 'after': migrated,
               'status': 'NOT_MIGRATED'}
        if migrated:
            row['result_equal'] = original['status'] == migrated['status']
            row['failure_equal'] = original['failures'] == migrated['failures']
            row['observation_differences'] = {key: [original['properties'].get(key), migrated['properties'].get(key)]
                for key in observation_keys if original['properties'].get(key) != migrated['properties'].get(key)}
            row['status'] = 'MATCH' if row['result_equal'] and row['failure_equal'] and not row['observation_differences'] else 'DIFFERENT'
        rows.append(row)
    result = {'scope': 'Observed result/failure/trace equality; does not alone establish source-path or mutation equivalence',
              'rows': rows, 'new_cases': sorted(set(after) - {replacement_id(old) for old in before}),
              'counts': {key: sum(r['status'] == key for r in rows) for key in ('MATCH', 'DIFFERENT', 'NOT_MIGRATED')}}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(result['counts']))
    return 1 if result['counts']['DIFFERENT'] else 2 if result['counts']['NOT_MIGRATED'] else 0


if __name__ == '__main__':
    raise SystemExit(main())
