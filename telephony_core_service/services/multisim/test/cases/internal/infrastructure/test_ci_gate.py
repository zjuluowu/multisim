import copy,json,sys,unittest
from pathlib import Path
T=next(p for p in Path(__file__).resolve().parents if (p/"scripts/_paths.py").is_file());sys.path.insert(0,str(T/'scripts'))
from ci_gate import evaluate
class Gate(unittest.TestCase):
 def setUp(self):
  known=json.loads((T/'cases/data/known_failures.json').read_text())['known_failures'];self.rows=[]
  for r in json.loads((T/'docs/Full_Replacement_Coverage.json').read_text())['cases']:
   name=r['new_id'];self.rows.append({'id':name,'status':'FAIL' if name in known else 'PASS','exit_code':1 if name in known else 0,'valid_xml':True,'details':{'failures':copy.deepcopy(known[name]['classifier_failures']) if name in known else [],'properties':dict.fromkeys(['business_assertions_before_cleanup','strict_ril_expectations_verified','object_lifetime','initialization_ready_idle'],'PASS')}})
 def test_known_failures_remain_raw_failures(self):
  r=evaluate(self.rows,[]);self.assertEqual(r['status'],'PASS');self.assertEqual(r['raw_test_status'],'FAIL');self.assertEqual(r['failed'],5)
 def test_new_failure_rejected(self):
  self.rows.append({'id':'New.Regression','status':'FAIL','exit_code':1,'valid_xml':True});self.assertEqual(evaluate(self.rows,[])['status'],'FAIL')
 def test_sanitizer_does_not_match_known_failure(self):
  row=next(r for r in self.rows if r['status']=='FAIL');row['unsafe_runtime']=True;self.assertEqual(evaluate(self.rows,[])['status'],'FAIL')
 def test_missing_case_rejected(self):
  self.rows.pop();self.assertEqual(evaluate(self.rows,[])['status'],'FAIL')
 def test_missing_xml_rejected(self):
  self.rows[0]['valid_xml']=False;self.assertEqual(evaluate(self.rows,[])['status'],'FAIL')
 def test_extra_assertion_is_not_known(self):
  row=next(r for r in self.rows if r['status']=='FAIL');row['details']['failures'].append('new assertion');self.assertEqual(evaluate(self.rows,[])['status'],'FAIL')
 def test_skip_is_not_pass(self):
  self.rows[0]['status']='UNSUPPORTED';self.assertEqual(evaluate(self.rows,[])['status'],'FAIL')
 def test_block_is_not_pass(self):
  self.rows[0]['status']='BLOCKED';self.assertEqual(evaluate(self.rows,[])['status'],'FAIL')
 def test_cleanup_cannot_be_hidden_in_business_pass(self):
  row=next(r for r in self.rows if '.C0Core' in r['id']);row['details']['properties']['object_lifetime']='FAIL';self.assertEqual(evaluate(self.rows,[])['status'],'FAIL')
 def test_duplicate_cases_rejected(self):
  self.rows.append(self.rows[0]);self.assertEqual(evaluate(self.rows,[])['status'],'FAIL')
if __name__=='__main__':unittest.main()
