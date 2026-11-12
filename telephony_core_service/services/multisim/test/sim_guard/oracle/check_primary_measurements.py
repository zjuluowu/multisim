#!/usr/bin/env python3
"""Technical oracle mutation checks on copied real observations, never a Target run."""
import argparse
import copy
import json
from pathlib import Path
from primary_oracle import evaluate, compare, PRIMARY, ICCID


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = json.loads(args.evidence.read_text())
    catalog = json.loads((Path(__file__).resolve().parents[1] / 'scenarios/primary_slot.json').read_text())
    scenarios = {s['id']: s for s in catalog['scenarios']}
    cases = {c['id']: c for c in report['cases']}
    checks = []
    def find(part):
        return next(c for c in cases.values() if part in c['id'])
    def mutated(name, case, change):
        observed = copy.deepcopy(case['observation']); change(observed)
        result = evaluate(scenarios[case['id']], observed)
        checks.append({'check': name, 'status': 'PASS' if result['status'] == 'FAIL' else 'FAIL',
                       'rejection': result.get('differences')})
    success, direct, error = find('C1-SUCCESS'), find('B-SUCCESS'), find('CHECK-ERROR')
    mutated('wrong requested slot', direct, lambda o: next(e['fields'] for e in o['events'] if e['kind'] == 'api_input').update(slot='0'))
    mutated('wrong final public role', success, lambda o: o['public'].update(primary='0'))
    mutated('wrong rollback ICCID', error, lambda o: o['parameters'].update({ICCID: 'wrong'}))
    def reuse(o):
        for e in o['events']:
            if 'session' in e['fields']: e['fields']['session'] = '7'
    mutated('reused retry session', error, reuse)
    def early(o):
        events = o['events']; index = next(i for i,e in enumerate(events) if e['kind'] == 'parameter_write' and e['fields']['key'] == PRIMARY)
        write = events.pop(index); index = next(i for i,e in enumerate(events) if e['kind'] == 'ril_response'); events.insert(index,write)
    mutated('primary commit before dedicated reply', direct, early)
    def missing(o): o['events'] = [e for e in o['events'] if e['kind'] != 'ril_response']
    mutated('missing protocol response evidence', success, missing)
    def wrong_notify(o):
        for e in o['events']:
            if e['kind']=='ril_response' and e['fields'].get('phase')=='3':e['fields']['phase']='2'
    mutated('UPDATE ACK substituted for NOTIFY', success, wrong_notify)
    for name, change in [('absolute session renumbering', lambda o: [e['fields'].update(session=str(int(e['fields']['session'])+10000)) for e in o['events'] if 'session' in e['fields']]), ('within-phase slot request order', lambda o: None)]:
        observed = copy.deepcopy(success['observation'])
        if name == 'within-phase slot request order':
            positions = [i for i,e in enumerate(observed['events']) if e['kind']=='ril_protocol'];a,b=positions[:2];observed['events'][a],observed['events'][b]=observed['events'][b],observed['events'][a]
        else: change(observed)
        left=evaluate(scenarios[success['id']],success['observation']);right=evaluate(scenarios[success['id']],observed)
        result=compare(left,right);checks.append({'check':name,'status':'PASS' if result['status']=='PASS' else 'FAIL'})
    result={'technical_only':True,'target_business_execution':'NOT_RUN','checks':checks}
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
    return int(any(c['status']!='PASS' for c in checks))


if __name__ == '__main__':
    raise SystemExit(main())
