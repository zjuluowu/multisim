#!/usr/bin/env python3
"""Normalize and compare independently executed Legacy/Target reports."""
import argparse
import json
from pathlib import Path

DEFAULT_FIELDS = ('status', 'scenario', 'api_result', 'primary_role', 'default_data_role',
                  'parameters', 'notifications', 'device_switch_commands', 'retry_count')
C0 = 'SIM028-LEGACY-C0-DUAL-PRIMARY0'
TEST = Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0, str(TEST / 'cases/internal'))
from primary_expectations import differential_oracle

C0_ORACLE = differential_oracle(C0)

def load_oracle(scenario):
    return differential_oracle(scenario)


def _legacy_observation(case):
    value = case.get('details', {}).get('properties', {}).get('primary_observation')
    return json.loads(value) if value else None


def _target_observation(case):
    value = case.get('details', {}).get('properties', {}).get('primary_observation')
    return json.loads(value) if value else None


def normalized(case):
    if not case:
        return {}
    observation = _legacy_observation(case) or _target_observation(case)
    if case.get('scenario') == C0 and observation is None:
        return {'status': case.get('status'), 'scenario': C0, 'measurement': 'MISSING'}
    if observation and 'public' in observation:
        public = observation['public']
        parameters = observation.get('parameters', {})
        events = observation.get('events', [])
        notifications = []
        device_calls = 0
        retries = 0
        for event in events:
            if event.get('kind') == 'publication':
                action = event.get('fields', {}).get('action', '')
                if 'DEFAULT_MAIN_SUBSCRIPTION_CHANGED' in action:
                    notifications.append('primary')
                elif 'DEFAULT_DATA_SUBSCRIPTION_CHANGED' in action:
                    notifications.append('data')
            if event.get('kind') in ('ril_primary', 'ril_protocol'):
                device_calls += 1
            if event.get('kind') == 'retry':
                retries += 1
        return {
            'status': case.get('status'), 'scenario': case.get('scenario'),
            'api_result': int(public.get('primary_result', -1)),
            'primary_role': int(public.get('primary', -1)),
            'default_data_role': int(public.get('data', -1)),
            'parameters': {
                'user_preference': parameters.get('persist.telephony.userPreferPrimarySlot') == 'true',
                'primary_slot': int(parameters.get('persist.telephony.MainSlotId', -1)),
                'primary_iccid': parameters.get('persist.telephony.MainCard.Iccid'),
                'default_data_slot': int(parameters.get('persist.telephony.MainCellularDataSlotId', -1)),
            },
            'notifications': notifications, 'device_switch_commands': device_calls,
            'retry_count': retries,
        }
    if observation:
        result = dict(observation)
        result['status'] = case.get('status')
        result['scenario'] = case.get('scenario')
        return {key: result[key] for key in DEFAULT_FIELDS if key in result}
    return {key: case[key] for key in DEFAULT_FIELDS if key in case}


def compare(left, right, required_scenarios=None):
    def index(cases):
        rows = {}
        duplicates = set()
        for case in cases:
            scenario = case.get('scenario')
            if not scenario:
                continue
            if scenario in rows:
                duplicates.add(scenario)
            else:
                rows[scenario] = normalized(case)
        return rows, duplicates
    left_index, left_duplicates = index(left)
    right_index, right_duplicates = index(right)
    if required_scenarios is None:
        required_scenarios = sorted(set(left_index) & set(right_index))
    scenarios = sorted(set(required_scenarios))
    differences = []
    for scenario in scenarios:
        if scenario in left_duplicates or scenario in right_duplicates:
            differences.append({'scenario': scenario, 'duplicate': 'legacy' if scenario in left_duplicates else 'target'})
            continue
        a, b = left_index.get(scenario), right_index.get(scenario)
        if not a or not b:
            differences.append({'scenario': scenario, 'missing': 'legacy' if not a else 'target'})
            continue
        if a != b:
            fields = sorted(set(a) | set(b))
            differences.append({'scenario': scenario, 'fields': [
                {'field': field, 'legacy': a.get(field), 'target': b.get(field)}
                for field in fields if a.get(field) != b.get(field)]})
    return scenarios, differences


def compare_reports(left, right, required_scenarios, compare_oracle=True):
    scenarios, differences = compare(left.get('cases', []), right.get('cases', []), required_scenarios)
    statuses = (left.get('status'), right.get('status'))
    missing = any(item.get('missing') for item in differences)
    executed = bool(scenarios) and not missing
    oracle_differences = []
    if compare_oracle and executed and C0 in scenarios:
        indexes = []
        for report in (left, right):
            by_scenario = {case.get('scenario'): normalized(case) for case in report.get('cases', [])}
            indexes.append(by_scenario.get(C0, {}))
        for backend, value in zip(('legacy', 'target'), indexes):
            mismatches = {key: {'expected': expected, 'actual': value.get(key)}
                          for key, expected in C0_ORACLE.items() if value.get(key) != expected}
            if mismatches:
                oracle_differences.append({'backend': backend, 'scenario': C0, 'fields': mismatches})
    if missing or not executed or any(status in ('BLOCKED', 'NOT_RUN', 'UNSUPPORTED') for status in statuses):
        overall = 'BLOCKED'
    elif 'FAIL' in statuses or differences or oracle_differences:
        overall = 'FAIL'
    else:
        overall = 'PASS'
    return {
        'legacy': statuses[0], 'target': statuses[1],
        'oracle': 'NOT_RUN' if not compare_oracle else ('BLOCKED' if not executed else
                  ('FAIL' if oracle_differences else 'PASS')),
        'oracle_differences': oracle_differences,
        'legacy_target': 'BLOCKED' if not executed else ('FAIL' if differences else 'PASS'),
        'overall': overall, 'scenarios': scenarios, 'differences': differences,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--legacy', type=Path, required=True)
    parser.add_argument('--target', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--scenario', action='append', required=True)
    parser.add_argument('--no-oracle', action='store_true')
    args = parser.parse_args()
    left, right = json.loads(args.legacy.read_text()), json.loads(args.target.read_text())
    result = compare_reports(left, right, args.scenario, not args.no_oracle)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n')
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0 if result['overall'] == 'PASS' else 2 if result['overall'] == 'BLOCKED' else 1


if __name__ == '__main__':
    raise SystemExit(main())
