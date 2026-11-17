#!/usr/bin/env python3
"""Native product build and HDC isolated execution; never substitute Host results."""
import argparse,hashlib,json,os,re,shlex,shutil,subprocess,tempfile,uuid
from pathlib import Path
from _paths import TEST,CORE
from run_gtest import classify

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--ohos-root',type=Path);p.add_argument('--binary',type=Path);p.add_argument('--component',action='store_true');p.add_argument('--native-target',choices=['sim_test_device','sim_test_device_component','sim_test_device_capacity','sim_test_device_primary_entry','sim_test_device_primary','sim_test_device_bootstrap','sim_test_device_query','sim_test_device_query_upstream']);p.add_argument('--library',type=Path);p.add_argument('--resource-file',type=Path);p.add_argument('--filter',default='*');a=p.parse_args();a.native_target=a.native_target or ('sim_test_device_component' if a.component else 'sim_test_device');a.component=a.native_target!='sim_test_device'
 a.output.mkdir(parents=True,exist_ok=True);result={'target':a.native_target,'platform':'OpenHarmony','level':'isolated_component' if a.component else 'live_SA_IPC_integration','status':'NOT_RUN','commands':[],'cases':[]}
 def finish(code,reason=None):
  if reason:result['reason']=reason
  (a.output/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k not in ['commands','cases']},ensure_ascii=False));return code
 def run(args,timeout=120):
  r=subprocess.run(args,text=True,capture_output=True,timeout=timeout,env=dict(os.environ,LLVM_PROFILE_FILE=str(a.output.resolve()/'tool-%p.profraw')));result['commands'].append({'command':args,'exit_code':r.returncode,'stdout':r.stdout,'stderr':r.stderr});return r
 if a.ohos_root:
  root=a.ohos_root.resolve();mounted=root/'base/telephony/core_service'
  if not (root/'build/test.gni').is_file() or mounted.resolve()!=CORE.resolve() or not shutil.which('hb'):
   result['status']='BLOCKED';return finish(2,'Matching OHOS tree/build/test.gni, mounted current component and hb are required')
  label='//base/telephony/core_service/services/multisim/test:'+a.native_target
  with (a.output/'native-build.log').open('w') as log:r=subprocess.run(['hb','build','-T',label],cwd=root,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,LLVM_PROFILE_FILE=str(a.output.resolve()/'build-%p.profraw')))
  result['build']={'target':label,'exit_code':r.returncode,'status':'PASS' if r.returncode==0 else 'FAIL'}
  if r.returncode:result['status']='FAIL';return finish(1,'Native product build failed; see native-build.log')
 if not a.binary or not a.binary.is_file() or not shutil.which('hdc'):
  result['status']='BLOCKED';return finish(2,'Native executable and HDC/device unavailable; no device execution performed')
 result['binary_sha256']=hashlib.sha256(a.binary.read_bytes()).hexdigest()
 remote='/data/local/tmp/sim-test-'+uuid.uuid4().hex
 if run(['hdc','shell','mkdir','-p',remote+'/profiles']).returncode or run(['hdc','file','send',str(a.binary.resolve()),remote+'/sim_test']).returncode:
  result['status']='BLOCKED';return finish(2,'Device deployment failed')
 run(['hdc','shell','chmod','700',remote+'/sim_test'])
 if a.library and run(['hdc','file','send',str(a.library.resolve()),remote+'/libtelephony_ext_service.z.so']).returncode:
  result['status']='BLOCKED';return finish(2,'Native query library deployment failed')
 env='LD_LIBRARY_PATH='+shlex.quote(remote)+' LLVM_PROFILE_FILE='+shlex.quote(remote+'/profiles/coverage-%p.profraw')
 if a.resource_file:
  if run(['hdc','file','send',str(a.resource_file.resolve()),remote+'/resources']).returncode:result['status']='BLOCKED';return finish(2,'Device resource declaration deployment failed')
  run(['hdc','shell','chmod','600',remote+'/resources'])
  env+=' SIM_DEVICE_RESOURCE_FILE='+shlex.quote(remote+'/resources')
 listing=run(['hdc','shell',env+' '+remote+'/sim_test --gtest_list_tests --gtest_filter='+shlex.quote(a.filter)])
 suite='';names=[]
 for raw in listing.stdout.splitlines():
  line=raw.split('#',1)[0].rstrip()
  if line and not line.startswith(' ') and line.endswith('.'):suite=line
  elif line.startswith('  ') and suite:names.append(suite+line.strip())
 if not names or len(names)!=len(set(names)):result['status']='FAIL';return finish(1,'Device discovery failed or matched no tests')
 (a.output/'discovery.txt').write_text(listing.stdout)
 for i,name in enumerate(names):
  xml=a.output/f'{i:04}.xml';remote_xml=remote+f'/{i:04}.xml'
  command=env+' '+remote+'/sim_test --gtest_filter='+shlex.quote(name)+' --gtest_output=xml:'+remote_xml+' & p=$!; echo $p > '+remote+'/pid; wait $p; s=$?; echo SIM_TEST_EXIT=$s'
  try:
   r=run(['hdc','shell',command],150);markers=re.findall(r'^SIM_TEST_EXIT=(-?\d+)\s*$',r.stdout,re.M);code=int(markers[-1]) if len(markers)==1 and r.returncode==0 else 125
   (a.output/f'{i:04}.log').write_text(r.stdout+r.stderr);run(['hdc','file','recv',remote_xml,str(xml)])
   status,reason=classify(xml,name,code)
  except subprocess.TimeoutExpired:
   cleanup='p=$(cat '+remote+'/pid); case $p in *[!0-9]*|"") exit 1;; esac; if [ "$(readlink /proc/$p/exe)" = '+shlex.quote(remote+'/sim_test')+' ]; then kill -KILL $p; fi'
   run(['hdc','shell',cleanup]);result['cases'].append({'id':name,'status':'FAIL','exit_code':124,'reason':'Device timeout; owned process terminated; resource restoration NOT_VERIFIED'})
   result['status']='FAIL';result['remaining_cases']='NOT_RUN';return finish(1,'Device execution timeout; do not continue with unverified resources')
  result['cases'].append({'id':name,'status':status,'reason':reason,'exit_code':code,'xml':xml.name})
 statuses={r['status'] for r in result['cases']};result['status']='FAIL' if 'FAIL' in statuses else 'BLOCKED' if statuses & {'BLOCKED','NOT_RUN'} else 'UNSUPPORTED' if 'UNSUPPORTED' in statuses else 'PASS'
 # Profile/log files stay in the unique remote run directory for diagnostics.
 result['device_artifacts']=remote;run(['hdc','file','recv',remote+'/profiles',str(a.output/'profiles')])
 return finish(0 if result['status']=='PASS' else 1 if result['status']=='FAIL' else 2)
if __name__=='__main__':raise SystemExit(main())
