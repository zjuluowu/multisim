#!/usr/bin/env python3
"""Single build/discovery/process/report entry for all SIM test slices."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile
from _paths import TEST, REPO

HOST = ['sim_test_host', 'sim_component_test', 'sim_primary_slot_entry_test', 'sim_g2a_query_test', 'sim_upstream_query_test']
REAL = ['sim_primary_slot_real_test', 'sim_bootstrap_characterization']


def run_architecture_stage(session, run):
    """Run outside the source repository and retain artifacts in this report."""
    destination = session / 'architecture'
    options = ['--with-gn', '--migration', '--output']
    if not destination.resolve().is_relative_to(REPO.resolve()):
        return run('architecture', 'run_architecture.py', [*options, destination])
    with tempfile.TemporaryDirectory(prefix='sim-architecture-') as temporary:
        output = Path(temporary) / 'architecture'
        try:
            return run('architecture', 'run_architecture.py', [*options, output])
        finally:
            if output.exists():
                shutil.copytree(output, destination, dirs_exist_ok=True)
            (session / 'architecture-location.json').write_text(json.dumps({
                'execution_directory': str(output), 'retained_directory': 'architecture',
                'execution_directory_temporary': True,
            }, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['all','build','editor','run','list','debug','technical','architecture','integrity','mutation','device'], nargs='?', default='all')
    parser.add_argument('--out', type=Path, default=REPO/'.host-out')
    parser.add_argument('--output', type=Path, default=REPO/'.host-out/results')
    parser.add_argument('--target', nargs='+', choices=HOST+REAL)
    parser.add_argument('--filter', default='*')
    parser.add_argument('--breakpoint')
    parser.add_argument('--slot',type=int)
    parser.add_argument('--primary-cleanup-diagnostic',action='store_true')
    parser.add_argument('--gtest-root')
    parser.add_argument('--openssl-root')
    parser.add_argument('--runtime-root')
    parser.add_argument('--jobs', default='3')
    parser.add_argument('--ci', action='store_true')
    parser.add_argument('--init-model', choices=['single-active','dual-active-primary0'], default='single-active')
    parser.add_argument('--ohos-root', type=Path)
    parser.add_argument('--device-binary', type=Path)
    parser.add_argument('--device-component', action='store_true')
    parser.add_argument('--resource-file', type=Path)
    parser.add_argument('--native-target')
    parser.add_argument('--library', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    session = Path(tempfile.mkdtemp(prefix='sim-', dir=args.output.resolve()))
    protected = REPO/'default.profraw'
    original = hashlib.sha256(protected.read_bytes()).hexdigest() if protected.is_file() else None
    result = {'head': subprocess.check_output(['git','rev-parse','HEAD'],cwd=REPO,text=True).strip(),
              'started_utc':datetime.now(timezone.utc).isoformat(),'commands':[], 'cases':[], 'status':'NOT_RUN'}
    def save():
        (session/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    def run(name, script, options=()):
        cmd=[sys.executable,'-B',str(TEST/'scripts'/script),*map(str,options)]
        with (session/(name+'.log')).open('w') as log:
            code=subprocess.run(cmd,cwd=REPO,env=dict(os.environ,LLVM_PROFILE_FILE=str(session/(name+'-%p.profraw'))),stdout=log,stderr=subprocess.STDOUT).returncode
        row={'name':name,'command':cmd,'exit_code':code,'status':'PASS' if code==0 else 'BLOCKED' if code==2 else 'FAIL','log':name+'.log'}
        result['commands'].append(row);save();print(name+': '+row['status'],flush=True)
        return code
    try:
        if args.action in ['all','build','editor','run','list','debug']:
            if run('case-discovery','update_case_sources.py'):return 1
        if args.action in ['all','technical','integrity','architecture']:
            if args.action in ['all','integrity']:run('integrity','check_test_integrity.py')
            if args.action in ['all','architecture']:run_architecture_stage(session, run)
            if args.action in ['all','technical']:run('technical','technical.py',['--output',session/'technical'])
        if args.action in ['all','build','editor','run','list','debug']:
            targets=args.target or (HOST+REAL)
            for target in targets:
                build=['--out',args.out,'--target',target,'--jobs',args.jobs]
                for flag,value in [('--gtest-root',args.gtest_root),('--openssl-root',args.openssl_root),('--runtime-root',args.runtime_root)]:
                    if value:build += [flag,value]
                if run(target+'-build','build_replacement.py',build):continue
                if args.action in ['build','editor']:continue
                if args.action in ['run','list','debug']:
                    from run_gtest import discover
                    selected,_=discover((args.out/'build'/target).resolve(),args.filter,dict(os.environ,LLVM_PROFILE_FILE=str(session/'discovery-%p.profraw'),LD_LIBRARY_PATH=str((args.out/'build').resolve())),60,allow_empty=True)
                    result.setdefault('discovery',{})[target]=selected;save()
                    if args.action=='list':
                        for name in selected:print(name,flush=True)
                    if not selected:
                        result['commands'].append({'name':target,'status':'NOT_RUN','exit_code':None,'reason':'filter_did_not_select_this_binary'});save();continue
                if args.action=='debug':
                    debug=['--binary',args.out/'build'/target,'--filter',args.filter,'--output',session/target]
                    if args.breakpoint:debug+=['--breakpoint',args.breakpoint]
                    if args.slot is not None:debug+=['--slot',str(args.slot)]
                    if args.primary_cleanup_diagnostic:debug+=['--primary-cleanup-diagnostic']
                    run(target+'-debug','debug.py',debug);continue
                options=['--binary',args.out/'build'/target,'--output',session/target,'--filter',args.filter]
                if args.action=='list':options+=['--list']
                if target in REAL:options += ['--env','SIM_HOST_BOOT_SLOT_COUNT=2','--env','SIM028_INIT_MODEL='+args.init_model]
                run(target,'run_gtest.py',options)
                reports=list((session/target).glob('gtest-*/result.json'))
                if len(reports)!=1:continue
                child=json.loads(reports[0].read_text())
                for case in child['cases']:case.update(target=target,report=str(reports[0]),scope='host_component')
                result['cases'].extend(child['cases']);save()
                slices={'sim_test_host':'state','sim_primary_slot_real_test':'primary','sim_g2a_query_test':'query','sim_upstream_query_test':'upstream'}
                if args.action=='all' and target in slices:
                    run(target+'-execution','audit_replacement_execution.py',['--slice',slices[target],'--binary',args.out/'build'/target,'--profiles',reports[0].parent,'--output',session/(target+'-execution')])
        if args.action=='editor':
            run('editor-discovery','check_editor.py',['--out',args.out,'--output',session/'editor'])
        if args.action=='all':
            if run('capability-probe-build','build_replacement.py',['--out',args.out,'--target','sim_capability_probe'])==0:
                run('capability-negative-controls','check_capabilities.py',['--binary',args.out/'build/sim_capability_probe','--output',session/'capabilities'])
            run('device-static','check_device.py')
            run('migration-equivalence','verify_migration.py',['--reports',session/'result.json','--output',session/'comparison.json'])
        if args.action in ['all','mutation']:run('mutation','mutations.py',['--out',args.out,'--output',session/'mutation'])
        if args.action=='all':
            run('profile-safety','profile_safety.py',['--binary',args.out/'build/sim_test_host','--output',session/'profile-safety'])
        if args.action=='device':
            options=['--output',session/'device','--filter',args.filter]
            if args.ohos_root:options+=['--ohos-root',args.ohos_root]
            if args.device_binary:options+=['--binary',args.device_binary]
            if args.device_component:options+=['--component']
            if args.resource_file:options+=['--resource-file',args.resource_file]
            if args.native_target:options+=['--native-target',args.native_target]
            if args.library:options+=['--library',args.library]
            run('device','device.py',options)
            report=session/'device/result.json'
            if report.is_file():
                native=json.loads(report.read_text());result['platform_result']=native
                expected={'PASS':0,'FAIL':1,'BLOCKED':2,'UNSUPPORTED':2}.get(native['status'])
                result['commands'][-1]['status']=native['status'] if expected==result['commands'][-1]['exit_code'] else 'FAIL'

        statuses={r['status'] for r in result['commands']}
        result['status']='FAIL' if 'FAIL' in statuses else 'BLOCKED' if 'BLOCKED' in statuses else 'UNSUPPORTED' if 'UNSUPPORTED' in statuses or (args.action=='run' and not result['cases']) or (args.action=='debug' and not any(result.get('discovery',{}).values())) else 'PASS'
        if args.action=='list' and result['status']=='PASS':result['status']='NOT_RUN'
        if args.ci:
            from ci_gate import evaluate
            result['admission']=evaluate(result['cases'],result['commands'])
        if result['cases']:
            sys.path.insert(0,str(TEST/'cases/internal'))
            from validate_primary import validate
            result['primary_contracts']=validate(result['cases'])
            if any(row['status']=='FAIL' for row in result['primary_contracts']):result['status']='FAIL'
        if args.ci:
            return 1 if result['admission']['status']=='FAIL' or any(r['status']=='FAIL' for r in result.get('primary_contracts',[])) else 0
        return 1 if result['status']=='FAIL' else 2 if result['status'] in ['BLOCKED','UNSUPPORTED'] else 0
    finally:
        result['protected_profile_unchanged']=original==(hashlib.sha256(protected.read_bytes()).hexdigest() if protected.is_file() else None)
        if not result['protected_profile_unchanged']:result['status']='FAIL'
        result['finished_utc']=datetime.now(timezone.utc).isoformat();save();print('Report: '+str(session/'result.json'),flush=True)
        if not result['protected_profile_unchanged']: return 1

if __name__=='__main__':raise SystemExit(main())
