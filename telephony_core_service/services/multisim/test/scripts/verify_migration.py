#!/usr/bin/env python3
"""Verify every historical runtime case without dropping raw observations."""
import argparse,json,re,tarfile,xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path
from _paths import TEST

def failure(text):return re.sub(r'^[^\n]*\.(?:h|hpp|cpp|cc):\d+\n','',text,flags=re.M).strip()
def is_reviewed_repair(mapping, old, new, actual_failures, differences, policy):
 if not policy:return False
 return (policy.get('old_id')==mapping['old_id'] and policy.get('origin')==mapping['origin']
         and old['status']==policy.get('before_status')=='FAIL'
         and new['status']==policy.get('after_status')=='PASS'
         and new.get('exit_code')==0 and new.get('valid_xml') is True
         and not new.get('unsafe_runtime',True) and not actual_failures
         and set(differences)<=set(policy.get('allowed_observation_changes',[])))

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--reports',nargs='+',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 baseline=TEST/'docs/archive/full_replacement_20261116';original={}
 for group in ['host_baseline','real_primary','all_baseline']:
  d=json.loads((baseline/group/'baseline.json').read_text())
  for r in d['cases']:original[(group,r['id'],Path(r['report']).stem)]=r
 current={}
 for path in a.reports:
  data=json.loads(path.read_text())
  for r in data['cases']:
   if r['id'] in current:raise RuntimeError('Duplicate report case '+r['id'])
   current[r['id']]=r
 rows=[]
 mappings=json.loads((TEST/'docs/Full_Replacement_Coverage.json').read_text())['cases']
 policy_path=TEST/'cases/data/accepted_case_repairs.json'
 policies=json.loads(policy_path.read_text())['repairs'] if policy_path.is_file() else {}
 if not set(policies)<={m['new_id'] for m in mappings}:raise RuntimeError('Unknown repair case in comparison policy')
 for mapping in mappings:
  new=current.get(mapping['new_id']);row=dict(mapping,current_status=new['status'] if new else 'NOT_RUN',comparison='NOT_RUN')
  if not new:rows.append(row);continue
  if mapping['old_id'] is None:row['comparison']='NEW_OR_PREVIOUSLY_UNEXECUTED';rows.append(row);continue
  candidates=[r for (g,name,stem),r in original.items() if g==mapping['origin'] and name==mapping['old_id'] and (not mapping.get('variant') or stem.endswith('-'+mapping['variant']))]
  if len(candidates)!=1:raise RuntimeError('Original baseline ambiguity '+str(mapping))
  old=candidates[0];case=next(ET.parse(Path(new['report']).parent/new['xml']).getroot().iter('testcase'))
  actual_failures=[failure(f.text or '') for f in case.findall('failure')];old_failures=[failure(f) for f in old['failures']]
  props={p.get('name'):p.get('value') for p in case.findall('properties/property')}
  keys=['observations','external_calls','pending_responses','real_instances_created','real_instances_destroyed']
  differences={k:[old['properties'].get(k),props.get(k)] for k in keys if old['properties'].get(k)!=props.get(k)}
  row['raw_observation_differences']=differences;row['failure_equal']=actual_failures==old_failures
  equal=new['status']==old['status'] and row['failure_equal']
  row['comparison']='MATCH' if equal and not differences else 'DIFFERENT'
  if row['comparison']=='DIFFERENT' and is_reviewed_repair(mapping,old,new,actual_failures,differences,policies.get(mapping['new_id'])):
   row['authorized_repair']=True;row['historical_status']=old['status'];row['adjudication']=policies[mapping['new_id']]['reason']
  if equal and mapping['old_id']=='SessionLifecycle.INIT_DUAL_002_ReversedEnumeration' and set(differences)=={'external_calls'}:
   before,after=differences['external_calls']
   if Counter(before.split(';'))==Counter(after.split(';')):
    row['contract_equivalent']=True;row['adjudication']='Per-slot asynchronous queues do not impose a cross-slot request order; reverse response order and actual state observations stay identical. Raw total order retained.'
  rows.append(row)
 result={'rows':rows,'counts':dict(Counter(r['comparison'] for r in rows)),'unmapped_new_cases':sorted(set(current)-{r['new_id'] for r in rows})}
 result['status']='FAIL' if any(r['comparison']=='NOT_RUN' or (r['comparison']=='DIFFERENT' and not r.get('contract_equivalent') and not r.get('authorized_repair')) for r in rows) else 'PASS'
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(result['status'],result['counts']);return 0 if result['status']=='PASS' else 1
if __name__=='__main__':raise SystemExit(main())
