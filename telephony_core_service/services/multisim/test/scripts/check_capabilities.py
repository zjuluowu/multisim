#!/usr/bin/env python3
"""Negative controls: unavailable resources abort a case and still clean up."""
import argparse,json,os,subprocess,sys
from pathlib import Path
from _paths import TEST
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binary',required=True,type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
cmd=[sys.executable,'-B',str(TEST/'scripts/run_gtest.py'),'--binary',str(a.binary),'--output',str(a.output/'raw')]
with (a.output/'execution.log').open('w') as stream:r=subprocess.run(cmd,stdout=stream,stderr=subprocess.STDOUT,env=dict(os.environ,LLVM_PROFILE_FILE=str(a.output.resolve()/'probe-%p.profraw')))
reports=list((a.output/'raw').glob('gtest-*/result.json'));assert len(reports)==1
rows=json.loads(reports[0].read_text())['cases'];states={row['id']:row['status'] for row in rows}
expected={'SkipFlow.MissingMutationIsUnsupported':'UNSUPPORTED','SkipFlow.MissingServiceIsBlocked':'BLOCKED'}
passed=r.returncode==2 and states==expected and all(row['valid_xml'] and row['details']['properties'].get('cleanup_after_skip')=='PASS' for row in rows)
(a.output/'result.json').write_text(json.dumps({'status':'PASS' if passed else 'FAIL','scope':'technical negative controls; neither skipped case is a business PASS','raw_states':states,'runner_exit_code':r.returncode,'cleanup_verified':passed},indent=2)+'\n')
raise SystemExit(0 if passed else 1)
