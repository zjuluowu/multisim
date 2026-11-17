#!/usr/bin/env python3
"""Debug exactly one discovered GoogleTest with isolated coverage output."""
import argparse,hashlib,json,os,platform,subprocess,tempfile,re,xml.etree.ElementTree as ET
from pathlib import Path
from run_gtest import discover
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binary',type=Path,required=True);p.add_argument('--filter',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--breakpoint',default='OHOS::Telephony::SimStateManager::UnlockPin');p.add_argument('--slot',type=int);p.add_argument('--primary-cleanup-diagnostic',action='store_true');a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True);session=Path(tempfile.mkdtemp(prefix='debug-',dir=a.output.resolve()));env=dict(os.environ,LLVM_PROFILE_FILE=str(session/'coverage-%p.profraw'),LD_LIBRARY_PATH=str(a.binary.resolve().parent)+':'+os.environ.get('LD_LIBRARY_PATH',''),SIM_HOST_BOOT_SLOT_COUNT='2',SIM028_INIT_MODEL='single-active')
cases,_=discover(a.binary.resolve(),a.filter,env,60)
if len(cases)!=1:raise SystemExit('Debug requires one selected case')
cmd=['lldb','--batch','-o','settings set target.disable-aslr false','-o','breakpoint set -n '+a.breakpoint,'-o','run','-o','thread backtrace','-o','frame variable slotId','-o','breakpoint disable 1','-o','continue','--',str(a.binary.resolve()),'--gtest_filter='+cases[0],'--gtest_output=xml:'+str(session/'case.xml')]
with (session/'lldb.log').open('w') as log:r=subprocess.run(cmd,env=env,cwd=session,stdout=log,stderr=subprocess.STDOUT)
text=(session/'lldb.log').read_text()
expected=1 if a.primary_cleanup_diagnostic else 0
slots=[int(n) for n in re.findall(r'\bslotId = (-?\d+)',text)]
passed=r.returncode==0 and bool(slots) and (a.slot is None or a.slot in slots) and '1 test from 1 test suite ran.' in text and ('exited with status = '+str(expected)) in text
classification=None
if a.primary_cleanup_diagnostic:
    from primary_results import classify
    cases_xml=list(ET.parse(session/'case.xml').getroot().iter('testcase')) if (session/'case.xml').is_file() else []
    if len(cases_xml)==1:classification=classify(cases_xml[0],1,text)
    passed=passed and classification is not None and classification['classification']=='cleanup_only'
result={'status':'PASS' if passed else 'FAIL','scope':'debugger verification, not business/CI admission','command':cmd,'debugger_exit_code':r.returncode,'expected_sut_exit_code':expected,'primary_result':classification,'observed_slots':slots,'profile':env['LLVM_PROFILE_FILE'],'binary_sha256':hashlib.sha256(a.binary.read_bytes()).hexdigest(),'platform':platform.platform()}
(session/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(session)
raise SystemExit(0 if passed else 1)
