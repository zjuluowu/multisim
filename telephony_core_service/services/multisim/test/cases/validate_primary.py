"""Business assertions over actual Primary measurements, independent of runners."""
import json
from pathlib import Path
from primary_expectations import evaluate, compare
DATA=Path(__file__).parent/'data'
def validate(rows):
    catalog=json.loads((DATA/'primary_inputs.json').read_text())
    golden=json.loads((DATA/'primary_golden.json').read_text())
    results=[]
    for scenario in catalog['scenarios']:
        if 'legacy_test' not in scenario:continue
        match=[r for r in rows if r['id']==scenario['legacy_test']]
        if not match:continue
        row=match[0];props=row.get('details',{}).get('properties',{})
        if scenario['branch']!='initialization failure boundary' and props.get('initialization_model')!='dual-active-primary0':
            results.append({'id':scenario['id'],'status':'BLOCKED','reason':'This shared rich contract requires the frozen dual-active-primary0 precondition; raw legacy GTest business assertions remain authoritative for single-active','gtest':row['status']})
            continue
        value=props.get('primary_observation')
        if value:
            result=evaluate(scenario,json.loads(value))
        elif scenario['branch']=='initialization failure boundary':
            result=evaluate(scenario,{'api_symbol':props.get('primary_direct_no_ril_symbol'),'max_sim_count':int(props.get('primary_max_sim_count','0'))})
        else:result={'status':'FAIL','differences':['No actual production measurement']}
        # Frozen Golden stays as a traceable external contract; never regenerate it.
        results.append(dict(result,id=scenario['id'],gtest=row['status']))
    return results
