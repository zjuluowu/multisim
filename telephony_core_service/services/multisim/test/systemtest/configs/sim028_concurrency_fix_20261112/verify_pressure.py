#!/usr/bin/env python3
"""Repeat existing real SIM cases after a successful, source-verified TSan CI run."""
import argparse
import concurrent.futures
import importlib.util
import json
from pathlib import Path
import sys

SYSTEM = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('sim028_ci', SYSTEM / 'tools/ci_sim028.py')
ci = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ci)
CASES = [
    'RealSimPrimary.BDirectRilFailureKeepsPreviousPrimary',
    'ConcurrencyRegression.CrossQueueStateSnapshotsAndSetter',
    'RealSimPrimary.C1CheckFailureRestoresParametersAndExhaustsFiveRetries',
    'ConcurrencyRegression.C1RetryHandoffInterleaving',
    'ConcurrencyRegression.C1RetriesDrainWhileStopping',
    'ShutdownContract.ANormalPrimaryThenStop',
]
PHASES = ['ShutdownExtra.C1StopAtCheck', 'ShutdownExtra.C1StopAtUpdate', 'ShutdownExtra.C1StopAtComplete']
NEW = ['ConcurrencyRegression.QueueHandoffDoesNotDelayImmediateEvent',
       'ConcurrencyRegression.CrossQueueStateSnapshotsAndSetter',
       'ConcurrencyRegression.C1RetryHandoffInterleaving',
       'ConcurrencyRegression.C1RetriesDrainWhileStopping',
       'ConcurrencyRegression.C1CommunicationTimeoutThenFiveRetries']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ci-out', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--rounds', type=int, default=50)
    parser.add_argument('--phase-rounds', type=int, default=10)
    parser.add_argument('--parallel', type=int, default=4)
    args = parser.parse_args()
    out, prepared = args.out.resolve(), args.ci_out.resolve()
    if ci.ROOT == out or ci.ROOT in out.parents or out in ci.ROOT.parents:
        parser.error('--out must be outside the source workspace')
    if min(args.rounds, args.phase_rounds, args.parallel) < 1:
        parser.error('repeat counts and parallelism must be positive')
    if out.exists() and any(out.iterdir()):
        parser.error('Refuse to overwrite pressure evidence')
    out.mkdir(parents=True, exist_ok=True)
    result = {'status': 'FAIL', 'cases': [], 'arguments': vars(args).copy(),
              'runner_sha256': ci.sha(Path(__file__).read_bytes()),
              'ci_helper_sha256': ci.sha((SYSTEM / 'tools/ci_sim028.py').read_bytes())}
    result['arguments'] = {k: str(v) if isinstance(v, Path) else v for k, v in result['arguments'].items()}
    try:
        manifest = json.loads((Path(__file__).parent / 'increment-manifest-v4.json').read_text())
        for row in manifest['files']:
            if ci.sha((prepared / 'repo' / row['path']).read_bytes()) != row['candidate_sha256']:
                raise RuntimeError('Compiled candidate source mismatch: ' + row['path'])
        full = json.loads((prepared / 'thread/latest.json').read_text())
        root_result = json.loads((prepared / 'result.json').read_text())
        root_thread = next((p for p in root_result['profiles'] if p['profile'] == 'thread'), None)
        if root_result['admission'] != 'PASS' or not root_thread or root_thread['run'] != full['run']:
            raise RuntimeError('Wait for the complete matching CI invocation, not a stale latest.json')
        marker = prepared / 'isolation.json'
        if int(Path(full['run']).name.removeprefix('run-')) < marker.stat().st_mtime_ns:
            raise RuntimeError('Full TSan run predates the current candidate snapshot')
        result.update(candidate_isolation_sha256=ci.sha(marker.read_bytes()),
                      full_ci_result_sha256=ci.sha((prepared / 'result.json').read_bytes()))
        required = set(ci.POLICY['targets'][ci.REAL] + NEW)
        if full['admission']['status'] != 'PASS' or full['sanitizer'] != 'thread':
            raise RuntimeError('A successful complete TSan profile is required before pressure')
        if {r['name'] for r in full['cases']} != required or len(full['cases']) != len(required):
            raise RuntimeError('Full TSan inventory mismatch')
        binary = prepared / 'thread/build' / ci.REAL
        digest = ci.sha(binary.read_bytes())
        if digest != full['binary_sha256'][ci.REAL]:
            raise RuntimeError('Binary changed after the successful full TSan run')
        result.update(full_ci_reference=full['run'], full_tests=len(required), binary_sha256=digest)
        work = [(name, i) for name in CASES for i in range(args.rounds)]
        work += [(name, i) for name in PHASES for i in range(args.phase_rounds)]

        def run(item):
            name, index = item
            directory = out / (name.replace('.', '_') + '-' + str(index))
            directory.mkdir()
            xml = directory / 'case.xml'
            step = ci.command([binary, '--gtest_filter=' + name, '--gtest_output=xml:' + str(xml)],
                              directory / 'case.log', directory, ci.test_env(directory, True, 'thread'), 120)
            row = ci.read_case(xml, name, step['exit_code'])
            row.update(step)
            row['admission'] = ci.evaluate([row], [name], {})
            ci.save(directory / 'result.json', row)
            return row

        with concurrent.futures.ThreadPoolExecutor(max_workers=args.parallel) as pool:
            for row in pool.map(run, work):
                result['cases'].append(row)
                ci.save(out / 'result.json', result)
        if ci.sha(binary.read_bytes()) != digest:
            raise RuntimeError('Binary changed while pressure was running')
        result['status'] = 'PASS' if all(r['admission']['status'] == 'PASS' for r in result['cases']) else 'FAIL'
    except (OSError, ValueError, RuntimeError) as error:
        result['error'] = str(error)
    ci.save(out / 'result.json', result)
    print(json.dumps({'status': result['status'], 'runs': len(result['cases']),
                      'failed': sum(r['admission']['status'] != 'PASS' for r in result['cases'])}))
    return int(result['status'] != 'PASS')


if __name__ == '__main__':
    sys.exit(main())
