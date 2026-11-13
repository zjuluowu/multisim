#!/usr/bin/env python3
"""Shared primary contract entry; reuses the existing GN/GTest Legacy launcher."""

import sys as _sys
from pathlib import Path as _Path
_TEST_ROOT = next(p for p in _Path(__file__).resolve().parents if (p / "scripts/_paths.py").is_file())
_sys.path.insert(0, str(_TEST_ROOT / "scripts"))
from _paths import TEST as _TEST, CORE as _CORE, REPO as _REPO, locate, guard_asset
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

GUARD = (_TEST / 'cases/compatibility')
SYSTEM = _TEST / 'framework/host'
sys.path.insert(0, str((_TEST / 'cases/primary_slot/oracle')))
from primary_oracle import evaluate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--host-out', type=Path, help='Configured Host output; forwarded to the original Legacy runner')
    parser.add_argument('--adapter', choices=['legacy', 'target', 'both'], default='legacy')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    catalog_file = (_TEST / 'cases/primary_slot/scenarios/primary_slot.json')
    catalog = json.loads(catalog_file.read_text())
    inputs = [catalog_file, Path(__file__), (_TEST / 'cases/primary_slot/oracle/primary_oracle.py'),
              (_TEST / 'framework/compatibility/contracts/primary_slot.h'), (_TEST / 'framework/compatibility/adapters/legacy/legacy_primary_adapter.h'),
              (_TEST / 'scripts/run_legacy_primary_contract.py'), (_TEST / 'cases/primary_slot/real_test.cpp'),
              (_TEST / 'cases/primary_slot/real_scenario.h'), (_TEST / 'framework/host/BUILD.gn')]
    input_hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    lock = json.loads(((_TEST / 'cases/primary_slot/baseline/contract-lock.json')).read_text())
    if input_hashes[str(catalog_file)] != lock['catalog_sha256']:
        raise RuntimeError('Shared scenario lock mismatch')
    frozen = json.loads(((_TEST / 'cases/primary_slot/baseline/frozen-baseline.json')).read_text())
    golden = (_TEST / 'cases/primary_slot/legacy_contract/golden.json')
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    golden_key = 'telephony_core_service/services/multisim/test/systemtest/primary_slot/legacy_contract/golden.json'
    if digest(golden) != frozen['files'][golden_key]:
        raise RuntimeError('Frozen Golden drift; refuse to derive new expectations')
    report = {'schema': 1, 'catalog_sha256': digest(catalog_file), 'legacy': 'NOT_RUN',
              'target': 'BLOCKED', 'comparison': 'NOT_RUN',
              'target_reason': 'StartPrimarySwitch and SwitchPrimaryRadio are declarations without complete production definitions; no Target adapter registered',
              'native_product': 'UNKNOWN', 'cases': [], 'input_sha256': input_hashes}
    code = 2
    if args.adapter in ('legacy', 'both'):
        command = [sys.executable, '-B', str((_TEST / 'scripts/run_legacy_primary_contract.py')),
                   '--output', str(output / 'legacy'), '--repeat', '1', '--jobs', '2',
                   '--init-model', 'dual-active-primary0']
        if args.host_out is not None:
            command += ['--host-out', str(args.host_out.resolve())]
        with (output / 'legacy-command.log').open('w') as stream:
            code = subprocess.run(command, cwd=_REPO, stdout=stream, stderr=subprocess.STDOUT).returncode
        report['command'] = command
        report['raw_legacy_exit'] = code
        result_path = output / 'legacy/validation.json'
        if not result_path.is_file():
            report['legacy'] = 'FAIL'
        else:
            result = json.loads(result_path.read_text())
            report['binary_sha256'] = result['binary_sha256']
            report['legacy_whole_gtest'] = result['whole_gtest_status']
            report['legacy_original_business'] = result['business_status']
            measured = {c['test']: c for r in result['runs'] for c in r['cases']}
            for scenario in catalog['scenarios']:
                case = measured.get(scenario['legacy_test'], {})
                props = case.get('properties', {})
                if scenario['branch'] == 'initialization failure boundary':
                    observation = {'api_symbol': props.get('primary_direct_no_ril_symbol'),
                                   'max_sim_count': int(props.get('primary_max_sim_count', -1))}
                else:
                    try:
                        observation = json.loads(props.get('primary_observation', '{}'))
                    except json.JSONDecodeError:
                        observation = {}
                checked = evaluate(scenario, observation)
                if case.get('business') != 'PASS':
                    checked['status'] = 'FAIL'
                    checked['differences'].append('original business/StrictMock gate did not pass')
                report['cases'].append({'id': scenario['id'], 'original': case.get('business', 'NOT_RUN'),
                    'lifecycle': case.get('lifecycle', 'NOT_RUN'), 'raw_exit': case.get('raw_exit'),
                    'observation': observation, 'contract': checked})
            report['legacy'] = 'PASS' if len(report['cases']) == 7 and all(c['contract']['status'] == 'PASS' for c in report['cases']) else 'FAIL'
            if report['legacy'] != 'PASS':
                code = 1
    if any(hashlib.sha256(Path(p).read_bytes()).hexdigest() != h for p, h in input_hashes.items()):
        report['input_drift'] = 'FAIL'
        report['legacy'] = 'FAIL'
        code = 1
    (output / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'cases'}, ensure_ascii=False, indent=2))
    # Preserve the real Legacy nonzero result; never launder cleanup FAIL to exit0.
    return code


if __name__ == '__main__':
    sys.exit(main())
