#!/usr/bin/env python3
"""Controlled production-source mutations, isolated builds, exact tests, recovery."""
import argparse,fcntl,hashlib,json,os,subprocess
from pathlib import Path
from _paths import TEST,CORE
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
with (a.out/'.build.lock').open('a') as lock:
 fcntl.flock(lock.fileno(),fcntl.LOCK_SH)
 config=json.loads((a.out/'replacement.json').read_text())
# Keep mutation-linked ELF files out of the ordinary Host and editor output.
a.out=a.out/'mutation-build';a.out.mkdir(parents=True,exist_ok=True)
build_lock=(a.out/'.build.lock').open('a');fcntl.flock(build_lock.fileno(),fcntl.LOCK_EX)
gnargs=' '.join(k+'='+json.dumps(v) for k,v in config.items());rows=[]
def run(name,cmd,expected=0):
 with (a.output/(name+'.log')).open('w') as log:r=subprocess.run(list(map(str,cmd)),stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,LLVM_PROFILE_FILE=str(a.output.resolve()/(name+'-%p.profraw'))))
 rows.append({'name':name,'command':list(map(str,cmd)),'exit_code':r.returncode,'expected_exit_code':expected});(a.output/'result.json').write_text(json.dumps({'commands':rows,'status':'RUNNING'},indent=2)+'\n')
 if r.returncode!=expected:raise RuntimeError(name+' unexpected exit '+str(r.returncode))
def build(name,extra=''):
 run(name+'-gn',['gn','gen',a.out/'build','--root='+str(TEST/'framework/internal/host'),'--args='+gnargs+extra]);run(name+'-build',['ninja','-C',a.out/'build','-j','3','sim_test_host'])
# Establish the normal binary from current source before fingerprinting it.
build('baseline')
originals={name:hashlib.sha256((CORE/'services/sim/src'/name).read_bytes()).hexdigest() for name in ['sim_state_handle.cpp','sim_state_manager.cpp']}
normal=a.out/'build/sim_test_host';before=hashlib.sha256(normal.read_bytes()).hexdigest()
try:
 source=(CORE/'services/sim/src/sim_state_handle.cpp').read_text();needle='case ICC_CONTENT_READY:\n            externalState_ = SimState::SIM_STATE_READY;';assert source.count(needle)==1
 mutant=a.output.resolve()/'sim_state_handle_mutant.cpp';mutant.write_text(source.replace(needle,needle.replace('SIM_STATE_READY;','SIM_STATE_LOCKED;')))
 build('ready',' sim_state_source_override='+json.dumps(str(mutant)))
 run('ready-detected',[normal,'--gtest_filter=SimTest.LIFE_001','--gtest_output=xml:'+str(a.output.resolve()/'ready.xml')],1)
 source=(CORE/'services/sim/src/sim_state_manager.cpp').read_text();start=source.index('int32_t SimStateManager::UnlockPuk(');end=source.index('int32_t SimStateManager::AlterPin(',start);body=source[start:end];assert body.count('PinOperationType::PIN_ALTER')==1
 mutant=a.output.resolve()/'sim_state_manager_mutant.cpp';mutant.write_text(source[:start]+body.replace('PinOperationType::PIN_ALTER','PinOperationType::PIN_ENABLE')+source[end:])
 build('puk',' sim_manager_source_override='+json.dumps(str(mutant)))
 run('puk-detected',[normal,'--gtest_filter=PinMemoryContract.PukSuccessUsesNewPinAndPinAlterOperation','--gtest_output=xml:'+str(a.output.resolve()/'puk.xml')],1)
finally:build('restore')
for name in ['SimTest.LIFE_001','PinMemoryContract.PukSuccessUsesNewPinAndPinAlterOperation']:
 run('recovery-'+name,[normal,'--gtest_filter='+name,'--gtest_output=xml:'+str(a.output.resolve()/('recovery-'+name+'.xml'))])
unchanged=all(hashlib.sha256((CORE/'services/sim/src'/n).read_bytes()).hexdigest()==v for n,v in originals.items());after=hashlib.sha256(normal.read_bytes()).hexdigest();same=before==after
(a.output/'result.json').write_text(json.dumps({'commands':rows,'status':'PASS' if unchanged and same else 'FAIL','binary_before_sha256':before,'binary_after_sha256':after,'mutations':'DETECTED','production_sources_unchanged':unchanged,'normal_binary_restored':same},indent=2)+'\n')
if not unchanged or not same:raise RuntimeError('Mutation recovery changed production source or the baseline binary; see result.json')
