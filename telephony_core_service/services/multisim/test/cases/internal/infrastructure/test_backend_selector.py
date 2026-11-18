#!/usr/bin/env python3
import json
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

SCRIPTS = Path(__file__).resolve().parents[3] / 'scripts'
sys.path.insert(0, str(SCRIPTS))
from backend_selector import capability_report, load_config, resolve
from compare_backends import compare


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

    def test_target_c0_is_never_fallback(self):
        rows = capability_report('target', 'host', 'C0CoreToRealManagerAndController')
        self.assertEqual(rows[0]['status'], 'BLOCKED')
        self.assertIn('cannot delegate to Legacy', rows[0]['reason'])

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
