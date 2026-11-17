"""Historical admission policy checks retained as pure regression functions."""
import copy,json,re,xml.etree.ElementTree as ET
from pathlib import Path
from _paths import TEST
POLICY = json.loads((TEST/'cases/data/admission_policy.json').read_text())
_LOCATION_ORIGINS = {'telephony_core_service/services/multisim/test/'+r['new']: 'telephony_core_service/services/multisim/test/'+r['old'] for r in json.loads((TEST/'docs/Test_Architecture_Migration.json').read_text())['files'] if Path(r['old']).suffix in ('.cpp','.h','.hpp')}
def normalize(message):
    message = re.sub(r'^[^\n]*?telephony_core_service/', 'telephony_core_service/', message, flags=re.M)
    return re.sub(r'^([^:\n]+)(?=:\d+\b)', lambda match: _LOCATION_ORIGINS.get(match.group(1), match.group(1)), message, flags=re.M)
def parse_discovery(text):
    suite, names = '', []
    for line in text.splitlines():
        line = line.split('#')[0].rstrip()
        if line.startswith('  ') and suite:
            names.append(suite + line.strip())
        elif line and not line.startswith(' ') and line.endswith('.'):
            suite = line
    return names

def read_case(path, requested, process_code):
    row = {'name': requested, 'exit_code': process_code, 'status': 'FAIL', 'messages': [], 'valid_xml': False}
    try:
        cases = ET.parse(path).getroot().findall('.//testcase')
    except (ET.ParseError, OSError):
        return row
    if len(cases) != 1:
        return row
    t = cases[0]
    actual = t.get('classname', '') + '.' + t.get('name', '')
    skipped = bool(t.findall('skipped')) or t.get('status') == 'notrun' or t.get('result') == 'suppressed'
    row.update(valid_xml=actual == requested and not skipped, actual_name=actual, skipped=skipped,
               messages=[normalize(f.get('message', f.text or '')) for f in t.findall('failure')],
               properties={p.get('name'): p.get('value') for p in t.findall('./properties/property')})
    if row['valid_xml'] and not row['messages'] and process_code == 0:
        row['status'] = 'PASS'
    return row

def evaluate(rows, expected, known):
    reasons = []
    names = [r['name'] for r in rows]
    if len(names) != len(set(names)):
        reasons.append('duplicate_cases')
    if set(names) != set(expected):
        reasons.append('collection_mismatch')
    observed_known = []
    for r in rows:
        name = r['name']
        if not r.get('valid_xml') or r.get('skipped'):
            reasons.append('invalid_or_skipped:' + name)
        for key, value in POLICY.get('required_properties', {}).get(name, {}).items():
            if r.get('properties', {}).get(key) != value:
                reasons.append('metadata_mismatch:' + name + ':' + key)
        if name in known:
            observed_known.append(name)
            if (r['status'] != 'FAIL' or r['exit_code'] != 1 or
                    r.get('messages') != known[name]['failure_messages']):
                reasons.append('known_failure_changed:' + name)
        elif r['status'] != 'PASS' or r['exit_code'] != 0:
            reasons.append('new_regression:' + name)
    # This gate NEVER rewrites raw test statuses or XML failures.
    return {'status': 'FAIL' if reasons else 'PASS', 'reasons': reasons,
            'raw_test_status': 'FAIL' if any(r['status'] == 'FAIL' for r in rows) else 'PASS',
            'passed': sum(r['status'] == 'PASS' for r in rows),
            'failed': sum(r['status'] == 'FAIL' for r in rows),
            'known_failures': [copy.deepcopy(r) for r in rows if r['name'] in known]}
