#!/usr/bin/env python3
import json
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

SCRIPTS = Path(__file__).resolve().parents[3] / 'scripts'
sys.path.insert(0, str(SCRIPTS))
from backend_selector import capability_report, gtest_filter, load_config, resolve
from compare_backends import compare, compare_reports, C0, C0_ORACLE, load_oracle


class BackendSelectorTest(unittest.TestCase):
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
        self.assertNotIn('services/sim', target)
        self.assertNotIn('sim_production_host', target)


if __name__ == '__main__':
    unittest.main()
