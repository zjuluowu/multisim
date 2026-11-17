#!/usr/bin/env python3
"""Collect framework/contract/CI/architecture technical tests using one protocol."""
import argparse,json,os,subprocess,sys
from pathlib import Path
from _paths import TEST,REPO
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
commands=[[sys.executable,'-B','-m','unittest','discover','-s',str(TEST/'cases/internal/infrastructure'),'-p','test_*.py','-v'],[sys.executable,'-B',str(TEST/'cases/architecture/test_g2a_source_tables.py'),'-v']]
rows=[]
for i,cmd in enumerate(commands):
 with (a.output/f'{i}.log').open('w') as stream:r=subprocess.run(cmd,cwd=REPO,stdout=stream,stderr=subprocess.STDOUT,env=dict(os.environ,LLVM_PROFILE_FILE=str(a.output/f'technical-{i}-%p.profraw')))
 rows.append({'command':cmd,'exit_code':r.returncode,'status':'PASS' if r.returncode==0 else 'FAIL'})
(a.output/'result.json').write_text(json.dumps({'commands':rows,'status':'PASS' if all(x['exit_code']==0 for x in rows) else 'FAIL'},indent=2)+'\n')
raise SystemExit(0 if all(x['exit_code']==0 for x in rows) else 1)
