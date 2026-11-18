#!/usr/bin/env python3
"""Exercise direct normal and externally signaled crash exits against a private pre-existing sentinel."""
import argparse,hashlib,json,os,platform,resource,subprocess,tempfile,selectors,signal,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binary',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
session=Path(tempfile.mkdtemp(prefix='profile-safety-',dir=a.output.resolve()));rows=[]
for mode in ['unset','relative','crash']:
 directory=session/mode;directory.mkdir();sentinel=directory/'default.profraw';sentinel.write_bytes(b'pre-existing user profile sentinel\x00\xff');expected=hashlib.sha256(sentinel.read_bytes()).hexdigest()
 env=dict(os.environ);env.pop('LLVM_PROFILE_FILE',None)
 if mode!='unset':env['LLVM_PROFILE_FILE']='default.profraw'
 name='SimTest.DualSimPinUnlock'
 cmd=[str(a.binary.resolve()),'--gtest_filter='+name]+(['--gtest_repeat=1000000'] if mode=='crash' else [])
 def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 crash_started=False
 with (session/(mode+'.log')).open('wb') as log:
  if mode!='crash':
   r=subprocess.run(cmd,cwd=directory,env=env,stdout=log,stderr=subprocess.STDOUT,preexec_fn=no_core)
  else:
   # Signal a healthy case after GoogleTest starts: corrected business defects
   # must never be used as a permanent artificial crash source.
   child=subprocess.Popen(cmd,cwd=directory,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,preexec_fn=no_core)
   try:
    with selectors.DefaultSelector() as ready:
     ready.register(child.stdout,selectors.EVENT_READ);seen=b'';deadline=time.monotonic()+10
     while time.monotonic()<deadline and child.poll() is None:
      if not ready.select(max(0,deadline-time.monotonic())):break
      chunk=os.read(child.stdout.fileno(),65536)
      if not chunk:break
      log.write(chunk);seen=(seen+chunk)[-131072:]
      if b'[ RUN      ] '+name.encode() in seen:
       crash_started=True;child.send_signal(signal.SIGABRT);break
    if not crash_started:child.kill()
    remaining,_=child.communicate(timeout=10);log.write(remaining)
    r=subprocess.CompletedProcess(cmd,child.returncode)
   finally:
    if child.poll() is None:child.kill();child.wait()

 unchanged=hashlib.sha256(sentinel.read_bytes()).hexdigest()==expected
 rows.append({'mode':mode,'command':cmd,'exit_code':r.returncode,'sentinel_unchanged':unchanged,'crash_started':crash_started if mode=='crash' else None,'status':'PASS' if unchanged and (r.returncode==0 if mode!='crash' else crash_started and r.returncode==-signal.SIGABRT) else 'FAIL'})
result={'binary_sha256':hashlib.sha256(a.binary.read_bytes()).hexdigest(),'platform':platform.platform(),'status':'PASS' if all(r['status']=='PASS' for r in rows) else 'FAIL','cases':rows};(session/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));raise SystemExit(0 if result['status']=='PASS' else 1)
