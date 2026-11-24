#!/usr/bin/env python3
"""Single build/discovery/process/report entry for all SIM test slices."""
import argparse
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile
from time import perf_counter
from _paths import TEST, REPO
from backend_selector import (expanded_backends, capability_report, gtest_filter,
                               resolve, scenario_for_test, selected_scenarios)
from compare_backends import compare_reports

from build_replacement import LEGACY, REAL, TARGET, PROBE, exact_case, file_identity, literal_cases
from run_gtest import discover, host_environment

HOST = [target for target in LEGACY if target not in REAL]
BACKEND_TARGETS = {'legacy': LEGACY, 'target': TARGET}


def build_plan(args, auto_route):
    backends = expanded_backends(args.backend)
    targets = args.target or [target for backend in backends for target in BACKEND_TARGETS[backend]]
    if args.target or args.action not in ('run', 'list', 'debug') or not args.build_if_missing:
        return targets, []
    if auto_route:
        return LEGACY + TARGET, [{'targets': LEGACY + TARGET, 'fallback': targets, 'filter': args.filter}]
    groups = []
    for backend in backends:
        pattern = gtest_filter(backend, args.filter, args.platform)
        if literal_cases(pattern) and len(BACKEND_TARGETS[backend]) > 1:
            groups.append({'targets': BACKEND_TARGETS[backend], 'filter': pattern})
        else:
            groups.append({'targets': BACKEND_TARGETS[backend], 'filter': '*'})
    return targets, groups if any(group['filter'] != '*' for group in groups) else []



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
    parser.add_argument('--out', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--backend', choices=['legacy','target','both'])
    parser.add_argument('--platform', choices=['host','device'])
    parser.add_argument('--backend-config', type=Path, default=TEST/'config/test_backend.json')
    parser.add_argument('--build-if-missing', '--incremental-build', dest='build_if_missing',
                        action=argparse.BooleanOptionalAction,
                        help='Check and update selected binaries every run; disabling explicitly reuses existing binaries')
    parser.add_argument('--full-evidence', action='store_true', help='Hash sources and binaries for a local run')
    parser.add_argument('--debug-build', action='store_true', help='Use a separate -O0, no-LTO Host build')
    parser.add_argument('--compare-with-oracle', action=argparse.BooleanOptionalAction)
    parser.add_argument('--target', nargs='+', choices=LEGACY+TARGET)
    parser.add_argument('--filter', default='*')
    parser.add_argument('--breakpoint')
    parser.add_argument('--slot',type=int)
    parser.add_argument('--primary-cleanup-diagnostic',action='store_true')
    parser.add_argument('--gtest-root')
    parser.add_argument('--openssl-root')
    parser.add_argument('--runtime-root')
    parser.add_argument('--jobs', default='3')
    parser.add_argument('--ci', action='store_true')
    parser.add_argument('--init-model', choices=['single-active','dual-active-primary0'])
    parser.add_argument('--ohos-root', type=Path)
    parser.add_argument('--device-binary', type=Path)
    parser.add_argument('--device-component', action='store_true')
    parser.add_argument('--resource-file', type=Path)
    parser.add_argument('--native-target')
    parser.add_argument('--library', type=Path)
    args = parser.parse_args()
    start = perf_counter()
    explicit_backend = args.backend is not None
    config = resolve(args, args.backend_config)
    if args.target and not explicit_backend:
        owners = [backend for backend, targets in BACKEND_TARGETS.items() if set(args.target).intersection(targets)]
        args.backend = config['backend'] = owners[0] if len(owners) == 1 else 'both'
    auto_route = (not explicit_backend and not args.target and config['backend'] == 'legacy'
                  and args.backend_config == TEST/'config/test_backend.json'
                  and args.action in ('run', 'list', 'debug') and exact_case(args.filter))
    if args.ci and (args.action != 'all' or config['backend'] != 'legacy' or args.target or args.filter != '*'):
        parser.error('--ci requires the complete Legacy admission: all --backend legacy --ci')
    if args.target:
        allowed = {target for backend in expanded_backends(config['backend']) for target in BACKEND_TARGETS[backend]}
        if set(args.target) - allowed:
            parser.error('Requested target does not belong to the selected backend')
    if args.debug_build and (args.action in ('all', 'mutation', 'editor') or args.ci):
        parser.error('--debug-build is for local build, run, list and debug; use the standard profile for verification')
    if args.init_model is None:
        args.init_model = ('dual-active-primary0' if config['backend'] == 'both'
                           or args.filter.endswith('-DUAL-PRIMARY0') else 'single-active')
    args.backend = config['backend']
    args.platform = config['platform']
    args.build_if_missing = config['build_if_missing']
    args.compare_with_oracle = config['compare_with_oracle']
    args.output = args.output or (REPO/config['output_dir']/'results')
    args.out = (args.out or REPO/config['output_dir']).resolve()
    build_options = ['--out', args.out, '--jobs', args.jobs]
    if args.debug_build:
        build_options += ['--debug-build']
        args.out /= 'debug'
    for flag, value in [('--gtest-root', args.gtest_root), ('--openssl-root', args.openssl_root),
                        ('--runtime-root', args.runtime_root)]:
        if value:
            build_options += [flag, value]
    target_list, route = build_plan(args, auto_route)
    resolution_seconds = perf_counter() - start
    args.output.mkdir(parents=True, exist_ok=True)
    session = Path(tempfile.mkdtemp(prefix='sim-', dir=args.output.resolve()))
    protected = REPO/'default.profraw'
    original = hashlib.sha256(protected.read_bytes()).hexdigest() if protected.is_file() else None
    result = {'head': subprocess.check_output(['git','rev-parse','HEAD'],cwd=REPO,text=True).strip(),
              'started_utc':datetime.now(timezone.utc).isoformat(),'commands':[], 'cases':[],
              'status':'NOT_RUN', 'backend_config':config, 'init_model':args.init_model,
              'timings':{'initial_resolution_seconds':resolution_seconds,'discovery':[]},
              'capabilities':{backend:capability_report(backend,args.platform,args.filter)
                             for backend in expanded_backends(args.backend)}}
    report_seconds = 0
    execution_lock = None
    def save():
        nonlocal report_seconds
        started = perf_counter()
        result['timings']['report_seconds_before_write'] = report_seconds
        path = session/'result.tmp'
        path.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
        path.replace(session/'result.json')
        report_seconds += perf_counter() - started
    def run(name, script, options=()):
        cmd=[sys.executable,'-B',str(TEST/'scripts'/script),*map(str,options)]
        started = perf_counter()
        with (session/(name+'.log')).open('w') as log:
            code=subprocess.run(cmd,cwd=REPO,env=dict(os.environ,LLVM_PROFILE_FILE=str(session/(name+'-%p.profraw'))),stdout=log,stderr=subprocess.STDOUT).returncode
        row={'name':name,'command':cmd,'exit_code':code,'status':'PASS' if code==0 else 'BLOCKED' if code==2 else 'FAIL','log':name+'.log','seconds':perf_counter()-started}
        result['commands'].append(row);save();print(name+': '+row['status'],flush=True)
        return code
    try:
        if args.action in ['all','build','editor','run','list','debug']:
            if args.platform == 'device' and args.action != 'device':
                result['commands'].append({'name':'device-routing','status':'BLOCKED','exit_code':2,
                    'reason':'Use action=device; Host binaries are not OpenHarmony product binaries'});save()
                return 2
            selected_targets = [target for target in target_list
                                if auto_route or any(target in BACKEND_TARGETS[backend]
                                                     for backend in expanded_backends(args.backend))]
            active_backends = expanded_backends(args.backend)
            for backend in active_backends:
                if not auto_route and not set(selected_targets).intersection(BACKEND_TARGETS[backend]):
                    result['commands'].append({'name':backend+'-target-selection','status':'BLOCKED','exit_code':2,
                        'reason':'requested target does not belong to this backend'})
            must_build = args.action in ('build','editor','all') or args.build_if_missing
            if selected_targets and must_build:
                build_targets = selected_targets[:]
                if args.action == 'all' and 'legacy' in active_backends:
                    build_targets.append(PROBE)
                trace = session/'build.json'
                options = [*build_options, '--target', *build_targets, '--result', trace]
                if args.action not in ('all','editor') and not args.full_evidence and args.backend != 'both':
                    options += ['--incremental']
                if route:
                    options += ['--route', json.dumps(route)]
                build_exit = run('host-build', 'build_replacement.py', options)
                if trace.is_file():
                    result['build'] = json.loads(trace.read_text())
                if build_exit:
                    result['status'] = 'FAIL'
                    print(result.get('build', {}).get('error', 'Host build failed') +
                          '; see ' + str(session/'host-build.log'), file=sys.stderr)
                    return 1
                selected_targets = [target for target in result['build']['targets'] if target != PROBE]
            if selected_targets and args.action != 'build':
                wait_start = perf_counter()
                execution_lock = (args.out/'.build.lock').open('a')
                fcntl.flock(execution_lock.fileno(), fcntl.LOCK_SH)
                result['timings']['execution_lock_wait_seconds'] = perf_counter()-wait_start
                for name, identity in result.get('build', {}).get('binary_identities', {}).items():
                    if file_identity(args.out/'build'/name) != identity:
                        raise RuntimeError('Host output changed between build and execution; retry: '+name)
            if auto_route:
                active_backends = tuple(backend for backend in BACKEND_TARGETS
                                        if set(selected_targets).intersection(BACKEND_TARGETS[backend]))
                result['capabilities'] = {backend: capability_report(backend, args.platform, args.filter)
                                          for backend in active_backends}
            result['selected_targets'] = selected_targets
            result['selected_backends'] = active_backends
            discovered = {}
            if args.action in ('run','list','debug'):
                for backend in active_backends:
                    pattern = gtest_filter(backend, args.filter, args.platform)
                    for target in selected_targets:
                        if target not in BACKEND_TARGETS[backend]:
                            continue
                        binary = args.out/'build'/target
                        if not binary.is_file():
                            result['commands'].append({'name':backend+'-'+target,'status':'BLOCKED','exit_code':2,
                                'reason':'binary_missing_and_incremental_build_disabled'})
                            continue
                        extra_env = {'SIM_HOST_BOOT_SLOT_COUNT':'2','SIM028_INIT_MODEL':args.init_model} if target in REAL else {}
                        env = host_environment(binary, session/(target+'-discovery-%p.profraw'), extra_env)
                        started = perf_counter()
                        cases, listing = discover(binary, pattern, env, 60, allow_empty=True)
                        result['timings']['discovery'].append({'target':target,'seconds':perf_counter()-started,
                                                               'count':len(cases)})
                        if cases:
                            discovered[target] = cases
                            (session/(target+'-discovery.txt')).write_text(listing)
                result['discovery'] = discovered
                counts = {}
                for target, cases in discovered.items():
                    for name in cases:
                        key = (name, None if auto_route else ('legacy' if target in LEGACY else 'target'))
                        counts.setdefault(key, []).append(target)
                ambiguous = {name: targets for (name, _), targets in counts.items() if len(targets) > 1}
                if ambiguous or (args.action == 'debug' and sum(map(len, discovered.values())) != 1):
                    result['status'] = 'BLOCKED'
                    result['reason'] = ('Ambiguous test selection: '+str(ambiguous) if ambiguous else
                                        'Debug requires exactly one test')
                    print(result['reason']+'; specify --backend, --target or a narrower --filter', file=sys.stderr)
                    return 2
            for backend in active_backends:
                for target in selected_targets:
                    if target not in BACKEND_TARGETS[backend] or args.action in ('build','editor'):
                        continue
                    binary = args.out/'build'/target
                    report_dir = session/backend/target
                    backend_filter = gtest_filter(backend,args.filter,args.platform)
                    options = ['--binary',binary,'--output',report_dir,'--filter',backend_filter]
                    if target in REAL:
                        options += ['--env','SIM_HOST_BOOT_SLOT_COUNT=2','--env','SIM028_INIT_MODEL='+args.init_model]
                    if args.action != 'all':
                        if target not in discovered:
                            continue
                        if args.action == 'list':
                            for name in discovered[target]:
                                print(f'[{backend}/{target}] {name}', flush=True)
                            continue
                        options += ['--discovery-file', session/(target+'-discovery.txt')]
                    if args.action == 'debug':
                        breakpoint = args.breakpoint
                        if breakpoint is None:
                            breakpoint = 'OHOS::Telephony::SimStateManager::UnlockPin'
                            if backend == 'target':
                                breakpoint = 'OHOS::Telephony::MultiSim::SimModule::SetPrimarySlotId'
                            elif backend_filter == 'RealSimPrimary.C0CoreToRealManagerAndController':
                                breakpoint = 'OHOS::Telephony::CoreServiceSim::SetPrimarySlotId'
                        options += ['--breakpoint', breakpoint]
                        if args.slot is not None:
                            options += ['--slot',str(args.slot)]
                        if args.primary_cleanup_diagnostic:
                            options += ['--primary-cleanup-diagnostic']
                        run(backend+'-'+target+'-debug','debug.py',options)
                        continue
                    if args.action == 'run' and exact_case(backend_filter) and args.backend != 'both' and not args.full_evidence:
                        options += ['--development-evidence']
                    run(backend+'-'+target,'run_gtest.py',options)
                    reports = list(report_dir.glob('gtest-*/result.json'))
                    if len(reports)!=1:
                        continue
                    child = json.loads(reports[0].read_text())
                    if child['status'] == 'UNSUPPORTED' and result['commands'][-1]['exit_code'] == 2:
                        result['commands'][-1]['status'] = 'UNSUPPORTED'
                    for case in child['cases']:
                        case.update(target=target,backend=backend,report=str(reports[0]),scope='host_component',
                            scenario=scenario_for_test(backend,case['id']))
                    result.setdefault('backend_reports',{}).setdefault(backend,{'cases':[],'status':'NOT_RUN'})['cases'].extend(child['cases'])
                    result['cases'].extend(child['cases']);save()
                    slices = {'sim_test_host':'state','sim_primary_slot_real_test':'primary',
                              'sim_g2a_query_test':'query','sim_upstream_query_test':'upstream'}
                    if args.action=='all' and backend=='legacy' and target in slices:
                        run(backend+'-'+target+'-execution','audit_replacement_execution.py',['--slice',slices[target],'--binary',binary,'--profiles',reports[0].parent,'--output',session/(backend+'-'+target+'-execution')])
                    if args.action=='all' and backend=='target' and target=='sim_test_target_host':
                        run(backend+'-'+target+'-execution','audit_replacement_execution.py',['--slice','target-adapter','--binary',binary,'--profiles',reports[0].parent,'--output',session/(backend+'-'+target+'-execution')])
                        run(backend+'-'+target+'-topology-execution','audit_replacement_execution.py',['--slice','target-topology','--binary',binary,'--profiles',reports[0].parent,'--output',session/(backend+'-'+target+'-topology-execution')])
                        run(backend+'-'+target+'-primary-b-execution','audit_replacement_execution.py',['--slice','target-primary-b','--binary',binary,'--profiles',reports[0].parent,'--output',session/(backend+'-'+target+'-primary-b-execution')])
                if args.action in ['all','run']:
                    blocked_rows=[row for row in capability_report(backend,args.platform,args.filter)
                                  if row['status'] in ('BLOCKED', 'UNSUPPORTED')]
                    for row in blocked_rows:
                        result['commands'].append({'name':backend+'-'+row['scenario'],'status':row['status'],'exit_code':2,
                            'reason':row['reason'],'backend':backend,'scenario':row['scenario']});save()
                    if backend == 'target' and not result.get('backend_reports',{}).get(backend,{}).get('cases') and not blocked_rows:
                        result['commands'].append({'name':'target-scenario-support','status':'BLOCKED','exit_code':2,
                            'backend':'target','reason':'No Target adapter/production implementation matches the selected scenario; no Legacy fallback'});save()
        if args.action=='editor':
            run('editor-discovery','check_editor.py',['--out',args.out,'--output',session/'editor','--target',*selected_targets])
        if args.action=='all' and 'legacy' in expanded_backends(args.backend):
            run('capability-negative-controls','check_capabilities.py',['--binary',args.out/'build/sim_capability_probe','--output',session/'capabilities'])
            run('profile-safety','profile_safety.py',['--binary',args.out/'build/sim_test_host','--output',session/'profile-safety'])
        if execution_lock:
            execution_lock.close()
            execution_lock = None
        if args.action in ['all','integrity']:
            run('integrity','check_test_integrity.py')
        if args.action in ['all','architecture']:
            run_architecture_stage(session, run)
        if args.action in ['all','technical']:
            run('technical','technical.py',['--output',session/'technical'])
        if args.action=='all' and 'legacy' in expanded_backends(args.backend):
            run('device-static','check_device.py')
            run('migration-equivalence','verify_migration.py',['--reports',session/'result.json','--output',session/'comparison.json'])
        if args.action=='mutation' or (args.action=='all' and 'legacy' in expanded_backends(args.backend)):
            run('mutation','mutations.py',['--out',args.out,'--output',session/'mutation'])
        if args.action=='device':
            active=expanded_backends(args.backend)
            if 'legacy' in active:
                options=['--output',session/'device/legacy','--filter',args.filter]
                if args.ohos_root:options+=['--ohos-root',args.ohos_root]
                if args.device_binary:options+=['--binary',args.device_binary]
                if args.device_component:options+=['--component']
                if args.resource_file:options+=['--resource-file',args.resource_file]
                if args.native_target:options+=['--native-target',args.native_target]
                if args.library:options+=['--library',args.library]
                run('legacy-device','device.py',options)
                report=session/'device/legacy/result.json'
                if report.is_file():
                    native=json.loads(report.read_text());result['platform_result']=native
                    expected={'PASS':0,'FAIL':1,'BLOCKED':2,'UNSUPPORTED':2}.get(native['status'])
                    result['commands'][-1]['status']=native['status'] if expected==result['commands'][-1]['exit_code'] else 'FAIL'
            if 'target' in active:
                result['commands'].append({'name':'target-device','status':'BLOCKED','exit_code':2,
                    'backend':'target','reason':'Target production SA/IPC/device target is not linked; no Legacy fallback'})
                save()

        for backend, report in result.get('backend_reports',{}).items():
            backend_commands=[row for row in result['commands'] if row.get('name','').startswith(backend+'-') or row.get('backend')==backend]
            report['status']='FAIL' if any(row['status']=='FAIL' for row in backend_commands) else \
                'BLOCKED' if any(row['status']=='BLOCKED' for row in backend_commands) else                 'UNSUPPORTED' if any(row['status']=='UNSUPPORTED' for row in backend_commands) else 'PASS'
            (session/(backend+'.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
        if args.backend=='both' and args.action in ['all','run']:
            legacy=result.get('backend_reports',{}).get('legacy',{'status':'BLOCKED','cases':[]})
            target=result.get('backend_reports',{}).get('target',{'status':'BLOCKED','cases':[]})
            required=[row['id'] for row in selected_scenarios(args.filter)
                      if row.get('differential') == 'SUPPORTED']
            result['differential']=compare_reports(legacy,target,required,args.compare_with_oracle)
            (session/'differential.json').write_text(json.dumps(result['differential'],ensure_ascii=False,indent=2)+'\n')
            overall=result['differential']['overall']
            if overall not in ('PASS',):
                result['commands'].append({'name':'both-differential','status':overall,'exit_code':2 if overall=='BLOCKED' else 1,
                    'reason':'Both requires every requested scenario to execute on both backends and match business Oracle'})
        statuses={r['status'] for r in result['commands']}
        result['status']='FAIL' if 'FAIL' in statuses else 'BLOCKED' if 'BLOCKED' in statuses else 'UNSUPPORTED' if 'UNSUPPORTED' in statuses or (args.action=='run' and not result['cases'])  else 'PASS'
        if args.action in ('run', 'list') and not result.get('discovery') and result['status'] in ('PASS','UNSUPPORTED'):
            result['status'] = 'UNSUPPORTED'
            result['reason'] = 'No GoogleTest matched the filter in the selected Host executables'
            print(result['reason'], file=sys.stderr)
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
    except (OSError, RuntimeError, ValueError, subprocess.TimeoutExpired) as error:
        result['status'] = 'FAIL'
        result['reason'] = str(error)
        print(str(error), file=sys.stderr)
        return 1
    finally:
        if execution_lock:
            execution_lock.close()
        result['protected_profile_unchanged']=original==(hashlib.sha256(protected.read_bytes()).hexdigest() if protected.is_file() else None)
        if not result['protected_profile_unchanged']:result['status']='FAIL'
        result['finished_utc']=datetime.now(timezone.utc).isoformat();save();print('Report: '+str(session/'result.json'),flush=True)
        if not result['protected_profile_unchanged']: return 1

if __name__=='__main__':raise SystemExit(main())
