"""Accept only exact five known signatures; raw failures never become passes."""
import hashlib,json,re
from pathlib import Path
from _paths import TEST

def signature(text):
    return re.sub(r'^[^\n]*\.(?:h|hpp|cpp|cc):\d+\n','',text,flags=re.M).strip()

def load_known():
    # Reviewed active policy, extracted from immutable original XML. Runtime
    # classification must not open a historical runner or evidence archive.
    data=(TEST/'cases/data/known_failures.json').read_bytes()
    if hashlib.sha256(data).hexdigest()!='baeb7f854a119e67d2846f886839cf6b7af78ae8d808e4e92bf3d4d66c54423d':
        raise RuntimeError('Known-failure policy differs from the reviewed original five signatures')
    actual=json.loads(data)['known_failures']
    if len(actual)!=5:raise RuntimeError('Expected exactly five known signatures')
    return actual

def evaluate(rows,commands):
    known=load_known()
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
        if name in known:
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
