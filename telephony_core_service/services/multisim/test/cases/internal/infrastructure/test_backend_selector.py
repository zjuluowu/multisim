#!/usr/bin/env python3
import json
import re
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

SCRIPTS = Path(__file__).resolve().parents[3] / 'scripts'
sys.path.insert(0, str(SCRIPTS))
from backend_selector import capability_report, gtest_filter, load_config, resolve
import build_replacement as build
from compare_backends import compare, compare_reports, C0, C0_ORACLE, load_oracle
from test import backend_filter
from _paths import CORE, TEST


class BackendSelectorTest(unittest.TestCase):
    def test_approved_compatibility_has_independent_regression_contracts(self):
        matrix = json.loads((TEST / 'config/approved_compatibility_contracts.json').read_text())
        contracts = matrix['contracts']
        milestones = {'CD-01': 'M6', 'CD-02': 'M7A', 'CD-03': 'M7B'}
        self.assertEqual({key: row['milestone'] for key, row in contracts.items()}, milestones)
        record = CORE / matrix['decision_record']
        decisions = record.read_text()
        state = (CORE / 'docs/SIM_Refactor_Execution_State.md').read_text()
        scenarios = json.loads((TEST / 'config/backend_capabilities.json').read_text())['scenarios']
        common = {row['id']: row for row in scenarios if row['differential'] == 'SUPPORTED'}
        common_target_cases = {row['target']['test'] for row in common.values()}
        target = (TEST / 'cases/target/primary/c0_topology_integration_test.cpp').read_text()
        legacy = (TEST / 'cases/internal/primary_initialization_test.cpp').read_text()

        def assert_registered(source, test_name):
            suite, case = test_name.split('.')
            self.assertRegex(source, rf'\bTEST(?:_F)?\(\s*{re.escape(suite)},\s*{re.escape(case)}\s*\)')

        for key, row in contracts.items():
            with self.subTest(decision=key):
                anchor = key.lower()
                self.assertIn(f'<a id="{anchor}"></a>', decisions)
                self.assertIn(f'{record.name}#{anchor}', state)
                self.assertEqual(row['status'], 'APPROVED_TARGET_BEHAVIOR')
                self.assertIn(row['common_scenario'], common)
                self.assertTrue(row['target_host'])
                for case in row['target_host']:
                    assert_registered(target, case)
                    self.assertNotIn(case, common_target_cases)
                if 'legacy_host' in row:
                    self.assertEqual(row['legacy_host']['init_model'], 'single-active')
                    assert_registered(legacy, row['legacy_host']['case'])
                else:
                    self.assertTrue((CORE / row['legacy_source_only']).is_file())

    def test_unfiltered_both_run_uses_reviewed_differential_cases(self):
        args = SimpleNamespace(action='run', backend='both', filter='*', platform='host')
        legacy = backend_filter(args, 'legacy').split(':')
        target = backend_filter(args, 'target').split(':')
        self.assertIn('RealSimPrimary.C0CoreToRealManagerAndController', legacy)
        self.assertIn('TargetPrimary.C0AssignmentServiceAlreadyPrimary', target)
        self.assertNotIn('TargetArchitecture.C0PrimaryPlanUsesNoDeviceSwitch', target)
        self.assertEqual(len(legacy), len(target))
        args.action = 'all'
        self.assertEqual(backend_filter(args, 'legacy'), '*')

    def test_host_route_uses_current_registrations_and_gn_ownership(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cases = root / 'cases'
            cases.mkdir()
            legacy, target = cases/'legacy.cpp', cases/'target.cpp'
            legacy.write_text('TEST_F(Fixture, First) {}\nTEST_P(Param, Values) {}')
            target.write_text('TEST_F(Fixture, Second) {}')
            targets = [build.LEGACY[0], build.TARGET[0]]
            database = root/'compile_commands.json'
            database.write_text(json.dumps([
                {'directory': str(root), 'file': str(source),
                 'command': f'clang++ -c {source} -o obj/{owner}.{source.stem}.o'}
                for source, owner in zip((legacy, target), targets)
            ]))
            with patch.object(build, 'TEST', root):
                def route(pattern):
                    return build.route_cases(database, [{'targets': targets, 'filter': pattern}])
                self.assertEqual(route('Fixture.Second'), [targets[1]])
                self.assertEqual(route('Prefix/Param.Values/0'), [targets[0]])
                self.assertEqual(route('Fixture.First:Fixture.Second'), targets)
                self.assertEqual(route('Unknown.Case'), targets)
                target.write_text('TEST_F(Fixture, First) {}')
                self.assertEqual(route('Fixture.First'), targets)

    def test_dependency_fast_path_revalidates_changes_and_full_mode(self):
        with tempfile.TemporaryDirectory() as temporary:
            file = Path(temporary) / 'pinned.h'
            file.write_text('correct')
            current, timing = {}, {}
            with patch.object(build, 'digest_file', wraps=build.digest_file) as digest:
                def verify(previous, full=False):
                    build.verify_group('test', [file], lambda: build.digest_file(file),
                                       previous, current, timing, full)
                verify({})
                verify(dict(current))
                self.assertTrue(timing['test']['reused'])
                self.assertEqual(digest.call_count, 1)
                verify(dict(current), full=True)
                self.assertFalse(timing['test']['reused'])
                file.write_text('changed')
                verify(dict(current))
                self.assertFalse(timing['test']['reused'])
                self.assertEqual(digest.call_count, 3)

    def test_defaults_are_legacy_and_host(self):
        config = load_config()
        self.assertEqual(config['backend'], 'legacy')
        self.assertEqual(config['platform'], 'host')

    def test_cli_wins_over_config(self):
        args = SimpleNamespace(backend='target', platform='host', build_if_missing=False,
            compare_with_oracle=False, output_dir=Path('isolated'))
        config = resolve(args)
        self.assertEqual(config['backend'], 'target')
        self.assertFalse(config['build_if_missing'])
        self.assertEqual(config['output_dir'], 'isolated')

    def test_oracle_is_derived_from_frozen_primary_contract(self):
        oracle = load_oracle(C0)
        self.assertEqual(oracle['parameters']['primary_iccid'], C0_ORACLE['parameters']['primary_iccid'])
        self.assertEqual(oracle['notifications'], ['primary', 'data'])
        self.assertEqual(oracle['device_switch_commands'], 0)
        success = load_oracle('SIM028-LEGACY-B-SUCCESS-DUAL-PRIMARY0')
        error = load_oracle('SIM028-LEGACY-B-ERROR-DUAL-PRIMARY0')
        timeout = load_oracle('SIM028-LEGACY-B-TIMEOUT-DUAL-PRIMARY0')
        self.assertEqual(success['device_switch_commands'], 1)
        self.assertEqual(success['retry_count'], 0)
        self.assertEqual(error['api_result'], 83886087)
        self.assertEqual(timeout['api_result'], 83886087)

    def test_independent_data_requires_actual_subscription_and_ordered_trace(self):
        from compare_backends import DATA_CONTRACT

        scenario = DATA_CONTRACT['id']
        def report(subscription, announced, swap=False):
            observation = {
                'public': {'primary': '0', 'data': '1', 'subscription': str(subscription)},
                'parameters': {'persist.telephony.MainSlotId': '0', 'persist.telephony.MainCellularDataSlotId': '1'},
                'events': [
                    {'kind': 'parameter_write', 'fields': {
                        'key': 'persist.telephony.MainCellularDataSlotId', 'value': '1'}},
                    {'kind': 'publication', 'fields': {
                        'action': 'usual.event.SIM.DEFAULT_DATA_SUBSCRIPTION_CHANGED', 'sim': str(announced)}},
                    {'kind': 'diagnostic', 'fields': {'name': 'SET_DEFAULT_CELLULAR_DATA'}},
                    {'kind': 'diagnostic', 'fields': {'name': 'ACCOUNT_NOTIFY'}},
                    {'kind': 'api_return', 'fields': {'result': '0'}},
                ],
            }
            if swap:
                observation['events'][2], observation['events'][3] = observation['events'][3], observation['events'][2]
            return {'status': 'PASS', 'cases': [{'scenario': scenario, 'status': 'PASS',
                     'details': {'properties': {'default_data_observation': json.dumps(observation)}}}]}

        legacy = report(1, 1)
        self.assertEqual(compare_reports(legacy, report(41, 41), [scenario])['overall'], 'PASS')
        self.assertEqual(compare_reports(legacy, report(41, 40), [scenario])['oracle'], 'FAIL')
        self.assertEqual(compare_reports(legacy, report(41, 41, swap=True), [scenario])['oracle'], 'FAIL')
        missing = {'status': 'PASS', 'cases': [{'scenario': scenario, 'status': 'PASS'}]}
        self.assertEqual(compare_reports(legacy, missing, [scenario], False)['overall'], 'BLOCKED')

    def test_same_scenario_maps_different_gtest_names(self):
        rows = capability_report('target', 'host', 'C0CoreToRealManagerAndController')
        self.assertEqual(rows[0]['status'], 'SUPPORTED')
        self.assertEqual(rows[0]['scenario'], C0)
        self.assertEqual(gtest_filter('target', rows[0]['scenario']),
            'TargetPrimary.C0AssignmentServiceAlreadyPrimary')

    def test_rejects_invalid_backend(self):
        with tempfile.TemporaryDirectory() as temporary:
            config = Path(temporary) / 'config.json'
            config.write_text(json.dumps({'backend': 'automatic'}))
            with self.assertRaises(ValueError):
                load_config(config)

    def test_comparator_compares_normalized_fields_only(self):
        legacy = [{'scenario': 'S', 'status': 'PASS', 'primary_role': 'B', 'address': 1}]
        target = [{'scenario': 'S', 'status': 'PASS', 'primary_role': 'B', 'address': 2}]
        scenarios, differences = compare(legacy, target)
        self.assertEqual(scenarios, ['S'])
        self.assertEqual(differences, [])

    @staticmethod
    def _report(backend_fields=None):
        fields = dict(C0_ORACLE)
        fields.update(backend_fields or {})
        return {'status': 'PASS', 'cases': [{'scenario': C0, 'status': 'PASS',
            'details': {'properties': {'primary_observation': json.dumps(fields)}}}]}

    def test_both_rejects_zero_matches(self):
        result = compare_reports({'status': 'PASS', 'cases': []},
            {'status': 'PASS', 'cases': []}, [C0])
        self.assertEqual(result['overall'], 'BLOCKED')
        self.assertEqual(result['scenarios'], [C0])

    def test_both_rejects_single_backend_blocked(self):
        result = compare_reports(self._report(), {'status': 'BLOCKED', 'cases': []}, [C0])
        self.assertEqual(result['overall'], 'BLOCKED')
        self.assertEqual(result['legacy_target'], 'BLOCKED')

    def test_both_reports_result_difference(self):
        result = compare_reports(self._report(), self._report({'primary_role': 0}), [C0])
        self.assertEqual(result['overall'], 'FAIL')
        self.assertEqual(result['legacy_target'], 'FAIL')

    def test_both_accepts_same_executed_scenario(self):
        result = compare_reports(self._report(), self._report(), [C0])
        self.assertEqual(result['scenarios'], [C0])
        self.assertEqual(result['oracle'], 'PASS')
        self.assertEqual(result['legacy_target'], 'PASS')
        self.assertEqual(result['overall'], 'PASS')

    def test_both_rejects_missing_or_fabricated_business_fields(self):
        incomplete = self._report()
        observation = json.loads(incomplete['cases'][0]['details']['properties']['primary_observation'])
        observation.pop('primary_role')
        incomplete['cases'][0]['details']['properties']['primary_observation'] = json.dumps(observation)
        result = compare_reports(self._report(), incomplete, [C0])
        self.assertEqual(result['overall'], 'FAIL')
        self.assertEqual(result['oracle'], 'FAIL')
        fabricated_top_level = {'status': 'PASS', 'cases': [{
            'scenario': C0, 'status': 'PASS', **C0_ORACLE}]}
        result = compare_reports(self._report(), fabricated_top_level, [C0])
        self.assertEqual(result['overall'], 'FAIL')
        fabricated = self._report()
        fabricated['cases'][0]['status'] = 'BLOCKED'
        result = compare_reports(fabricated, self._report(), [C0])
        self.assertEqual(result['overall'], 'FAIL')

    def test_architecture_only_cannot_make_both_pass(self):
        result = compare_reports(
            {'status': 'PASS', 'cases': [{'scenario': 'ARCH', 'status': 'PASS'}]},
            {'status': 'PASS', 'cases': [{'scenario': 'ARCH', 'status': 'PASS'}]}, [C0])
        self.assertEqual(result['overall'], 'BLOCKED')

    def test_target_link_target_has_no_legacy_production(self):
        build = (SCRIPTS.parent / 'framework/internal/host/BUILD.gn').read_text()
        start = build.index('static_library("sim_target_production_host")')
        end = build.index('# Primary-slot entry contract.', start)
        target = build[start:end]
        self.assertIn('services/multisim/role_assignment/primary/primary_switch_plan.cpp', target)
        self.assertIn('services/multisim/role_assignment/assignment_store_adapter.cpp', target)
        self.assertNotIn('services/sim', target)
        self.assertNotIn('sim_production_host', target)


if __name__ == '__main__':
    unittest.main()
