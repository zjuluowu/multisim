"""Enforce reviewed repairs as passes while retaining immutable historical signatures."""
import hashlib,json,re
from pathlib import Path
from _paths import TEST

def signature(text):
    return re.sub(r'^[^\n]*\.(?:h|hpp|cpp|cc):\d+\n','',text,flags=re.M).strip()

def load_known():
    # Historical signatures extracted from immutable original XML; resolved
    # entries are required to pass by the separate active repair policy. Runtime
    # classification must not open a historical runner or evidence archive.
    data=(TEST/'cases/data/known_failures.json').read_bytes()
    if hashlib.sha256(data).hexdigest()!='baeb7f854a119e67d2846f886839cf6b7af78ae8d808e4e92bf3d4d66c54423d':
        raise RuntimeError('Known-failure policy differs from the reviewed original five signatures')
    actual=json.loads(data)['known_failures']
    if len(actual)!=5:raise RuntimeError('Expected exactly five known signatures')
    return actual

def load_resolved():
    path = TEST/'cases/data/resolved_failures.json'
    if not path.is_file():
        raise RuntimeError('Resolved repair policy missing; historical failure acceptance cannot be restored')
    names = json.loads(path.read_text())['resolved']
    if (not isinstance(names, list) or not all(isinstance(name, str) for name in names) or
            len(names) != len(set(names)) or set(names) != set(load_known())):
        raise RuntimeError('Resolved policy must retain all five reviewed repairs exactly once')
    return set(names)

def evaluate(rows,commands,resolved=None):
    known=load_known()
    resolved=load_resolved() if resolved is None else set(resolved)
    reasons=[];observed=[]
    names=[r['id'] for r in rows]
    if len(names)!=len(set(names)):reasons.append('duplicate_cases')
    coverage=TEST/'docs/Full_Replacement_Coverage.json'
    if not coverage.is_file():reasons.append('coverage_manifest_missing')
    else:
        required={r['new_id'] for r in json.loads(coverage.read_text())['cases']}
        for name in sorted(required-set(names)):reasons.append('missing_case:'+name)
    for row in rows:
        name=row['id'];status=row['status']
        if not row.get('valid_xml'):reasons.append('invalid_xml:'+name)
        if row.get('unsafe_runtime'):reasons.append('unsafe_runtime:'+name)
        if status=='PASS' and name.startswith('RealSimPrimary.') and 'InitializesReal' not in name:
            props=row.get('details',{}).get('properties',{})
            if any(props.get(k)!='PASS' for k in ['business_assertions_before_cleanup','strict_ril_expectations_verified','object_lifetime','initialization_ready_idle']):
                reasons.append('primary_evidence_missing:'+name)
        if name in resolved:
            if status != "PASS" or row.get("exit_code") != 0 or row.get("details",{}).get("failures"):
                reasons.append("resolved_defect_regressed:"+name)
        elif name in known:
            failures=[signature(f) for f in row.get('details',{}).get('failures',[])]
            # Classifier stores message and body; both must match their fixed pair.
            if status!='FAIL' or row.get('exit_code')!=1 or failures!=known[name]['classifier_failures']:
                reasons.append('known_signature_changed:'+name)
            else:observed.append(name)
        elif status!='PASS':reasons.append('new_or_unaccepted_failure:'+name)
    for command in commands:
        if command['status']!='PASS' and not (command['name'] in {r.get('target') for r in rows}):
            reasons.append('command_'+command['status'].lower()+':'+command['name'])
    return {'status':'FAIL' if reasons else 'PASS','raw_test_status':'FAIL' if any(r['status']=='FAIL' for r in rows) else 'PASS',
            'known_failures':observed,'reasons':reasons,'passed':sum(r['status']=='PASS' for r in rows),'failed':sum(r['status']=='FAIL' for r in rows)}
