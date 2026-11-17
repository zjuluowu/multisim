#!/usr/bin/env python3
"""Exercise direct normal/crash/debug exits against a private pre-existing sentinel."""
import argparse,hashlib,json,os,platform,resource,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binary',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
session=Path(tempfile.mkdtemp(prefix='profile-safety-',dir=a.output.resolve()));rows=[]
for mode in ['unset','relative','crash']:
 directory=session/mode;directory.mkdir();sentinel=directory/'default.profraw';sentinel.write_bytes(b'pre-existing user profile sentinel\x00\xff');expected=hashlib.sha256(sentinel.read_bytes()).hexdigest()
 env=dict(os.environ);env.pop('LLVM_PROFILE_FILE',None)
 if mode!='unset':env['LLVM_PROFILE_FILE']='default.profraw'
 name='SimTest.DualSimPinUnlock' if mode!='crash' else 'SimAdvancedTest.DEFECT_001_StaleReadyAfterRemoval'
 cmd=[str(a.binary.resolve()),'--gtest_filter='+name]+(['--gtest_break_on_failure'] if mode=='crash' else [])
 def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 with (session/(mode+'.log')).open('w') as log:r=subprocess.run(cmd,cwd=directory,env=env,stdout=log,stderr=subprocess.STDOUT,preexec_fn=no_core)
 unchanged=hashlib.sha256(sentinel.read_bytes()).hexdigest()==expected
 rows.append({'mode':mode,'command':cmd,'exit_code':r.returncode,'sentinel_unchanged':unchanged,'status':'PASS' if unchanged and (r.returncode==0 if mode!='crash' else r.returncode<0) else 'FAIL'})
result={'binary_sha256':hashlib.sha256(a.binary.read_bytes()).hexdigest(),'platform':platform.platform(),'status':'PASS' if all(r['status']=='PASS' for r in rows) else 'FAIL','cases':rows};(session/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));raise SystemExit(0 if result['status']=='PASS' else 1)
