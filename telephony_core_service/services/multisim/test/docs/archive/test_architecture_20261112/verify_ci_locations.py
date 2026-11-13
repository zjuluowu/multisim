#!/usr/bin/env python3
"""Check real old/new Host XML against unchanged CI signatures, preserving raw FAIL."""
import argparse
import copy
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

root = next(p for p in Path(__file__).resolve().parents if (p / 'docs/Test_Architecture_Migration.json').is_file())
sys.path.insert(0, str(root / 'scripts'))
import ci_sim028 as ci
parser = argparse.ArgumentParser()
parser.add_argument('--before', type=Path, required=True)
parser.add_argument('--after', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()


def evaluate(path):
    host = json.loads(path.read_text())
    rows = []
    for group in host['groups']:
        for t in ET.parse(Path(host['artifacts']) / (group['name'] + '.xml')).getroot().findall('.//testcase'):
            failures = t.findall('failure')
            rows.append({'name': t.get('classname') + '.' + t.get('name'),
                         'status': 'FAIL' if failures else 'PASS', 'exit_code': group['exit_code'],
                         'valid_xml': t.get('status') == 'run' and t.find('skipped') is None,
                         'properties': {p.get('name'): p.get('value') for p in t.findall('./properties/property')},
                         'messages': [ci.normalize(f.get('message', '')) for f in failures]})
    expected = [r['name'] for r in rows]
    result = ci.evaluate(rows, expected, ci.POLICY['known_failures'])
    if result['status'] != 'PASS' or result['raw_test_status'] != 'FAIL' or (result['passed'], result['failed']) != (59, 5):
        raise RuntimeError('Real Host policy/counting drift: ' + json.dumps(result))
    bad = copy.deepcopy(rows)
    next(r for r in bad if r['status'] == 'FAIL')['messages'].append('an additional independent regression')
    if ci.evaluate(bad, expected, ci.POLICY['known_failures'])['status'] != 'FAIL':
        raise RuntimeError('Additional assertion was incorrectly accepted')
    return result


before, after = evaluate(args.before), evaluate(args.after)
if before != after:
    raise RuntimeError('CI classification changed across the directory move')
args.output.write_text(json.dumps({'status': 'PASS', 'raw_host': '59 PASS / 5 FAIL',
    'scope': '64 existing Host cases, not the frozen 96-case candidate',
    'known_signatures': 'identical', 'extra_assertion_negative_control': 'rejected',
    'full_candidate_admission': 'NOT_VALIDATED_BY_THIS_CHECK'}, indent=2) + '\n')
print('CI source locations: PASS (real 64-case XML; raw 59/5 and all exact failures preserved)')
