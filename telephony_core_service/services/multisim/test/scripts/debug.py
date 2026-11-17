#!/usr/bin/env python3
"""Debug exactly one discovered GoogleTest with isolated coverage output."""
import argparse,hashlib,json,os,platform,subprocess,tempfile
from pathlib import Path
from run_gtest import discover
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binary',type=Path,required=True);p.add_argument('--filter',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--breakpoint',default='OHOS::Telephony::SimStateManager::UnlockPin');a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True);session=Path(tempfile.mkdtemp(prefix='debug-',dir=a.output.resolve()));env=dict(os.environ,LLVM_PROFILE_FILE=str(session/'coverage-%p.profraw'),LD_LIBRARY_PATH=str(a.binary.resolve().parent)+':'+os.environ.get('LD_LIBRARY_PATH',''))
cases,_=discover(a.binary.resolve(),a.filter,env,60)
if len(cases)!=1:raise SystemExit('Debug requires one selected case')
cmd=['lldb','--batch','-o','settings set target.disable-aslr false','-o','breakpoint set -n '+a.breakpoint,'-o','run','-o','thread backtrace','-o','frame variable slotId','-o','breakpoint disable 1','-o','continue','--',str(a.binary.resolve()),'--gtest_filter='+cases[0]]
with (session/'lldb.log').open('w') as log:r=subprocess.run(cmd,env=env,cwd=session,stdout=log,stderr=subprocess.STDOUT)
(session/'result.json').write_text(json.dumps({'command':cmd,'exit_code':r.returncode,'profile':env['LLVM_PROFILE_FILE'],'binary_sha256':hashlib.sha256(a.binary.read_bytes()).hexdigest(),'platform':platform.platform()},indent=2)+'\n');print(session)
text=(session/'lldb.log').read_text()
passed=r.returncode==0 and 'slotId = 1' in text and '1 test.' in text and 'exited with status = 0' in text
raise SystemExit(0 if passed else 1)
