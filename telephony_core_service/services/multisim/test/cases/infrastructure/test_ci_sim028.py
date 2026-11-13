#!/usr/bin/env python3
"""Admission-policy tests: regressions must never become known-failure passes."""

import sys as _sys
from pathlib import Path as _Path
_TEST_ROOT = next(p for p in _Path(__file__).resolve().parents if (p / "scripts/_paths.py").is_file())
_sys.path.insert(0, str(_TEST_ROOT / "scripts"))
from _paths import TEST as _TEST, CORE as _CORE, REPO as _REPO, locate, guard_asset
import copy
from pathlib import Path
import tempfile
import unittest
import ci_sim028 as ci


class AdmissionPolicy(unittest.TestCase):
    def setUp(self):
        self.expected = [n for names in ci.POLICY['targets'].values() for n in names]
        self.known = ci.POLICY['known_failures']
        self.rows = [{'name': n, 'status': 'FAIL' if n in self.known else 'PASS',
                      'exit_code': 1 if n in self.known else 0, 'valid_xml': True, 'properties': copy.deepcopy(ci.POLICY.get('required_properties', {}).get(n, {})),
                      'messages': copy.deepcopy(self.known.get(n, {}).get('failure_messages', []))}
                     for n in self.expected]

    def evaluate(self):
        return ci.evaluate(self.rows, self.expected, self.known)

    def test_exact_known_failures_are_still_failures(self):
        r = self.evaluate()
        self.assertEqual(r['status'], 'PASS')
        self.assertEqual(r['raw_test_status'], 'FAIL')
        self.assertEqual((r['passed'], r['failed']), (91, 5))
        self.assertTrue(all(c['status'] == 'FAIL' for c in r['known_failures']))

    def test_new_case_failure_rejects_admission(self):
        name = 'AdmissionNegativeControl.NewRegression'
        self.expected.append(name)
        self.rows.append({'name': name, 'status': 'FAIL', 'exit_code': 1,
                          'valid_xml': True, 'messages': ['new failure']})
        self.assertIn('new_regression:' + name, self.evaluate()['reasons'])

    def test_new_failure_on_existing_pass_case_rejects(self):
        c = next(r for r in self.rows if r['name'] not in self.known)
        c.update(status='FAIL', exit_code=1, messages=['unexpected'])
        self.assertEqual(self.evaluate()['status'], 'FAIL')

    def test_added_assertion_on_known_case_rejects(self):
        c = next(r for r in self.rows if r['name'] in self.known)
        c['messages'].append('a different bug in the same test')
        self.assertEqual(self.evaluate()['status'], 'FAIL')

    def test_known_failure_fix_requires_policy_review(self):
        c = next(r for r in self.rows if r['name'] in self.known)
        c.update(status='PASS', exit_code=0, messages=[])
        self.assertEqual(self.evaluate()['status'], 'FAIL')

    def test_crash_cannot_masquerade_as_known_failure(self):
        c = next(r for r in self.rows if r['name'] in self.known)
        c['exit_code'] = -11
        self.assertEqual(self.evaluate()['status'], 'FAIL')

    def test_missing_duplicate_and_skip_reject(self):
        original = copy.deepcopy(self.rows)
        self.rows.pop()
        self.assertEqual(self.evaluate()['status'], 'FAIL')
        self.rows = original + [copy.deepcopy(original[0])]
        self.assertEqual(self.evaluate()['status'], 'FAIL')
        self.rows = original
        self.rows[0]['skipped'] = True
        self.assertEqual(self.evaluate()['status'], 'FAIL')

    def test_missing_oracle_metadata_rejects(self):
        r = next(r for r in self.rows if r['properties'])
        r['properties'].clear()
        self.assertEqual(self.evaluate()['status'], 'FAIL')

    def test_zero_or_malformed_xml_rejects(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / 'x.xml'
            p.write_text('<testsuites/>')
            self.assertFalse(ci.read_case(p, 'A.B', 0)['valid_xml'])
            p.write_text('not xml')
            self.assertFalse(ci.read_case(p, 'A.B', 1)['valid_xml'])

    def test_exit_code_does_not_override_failure_xml(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / 'x.xml'
            p.write_text('<testsuites><testsuite><testcase classname="A" name="B"><failure message="bad"/></testcase></testsuite></testsuites>')
            self.assertEqual(ci.read_case(p, 'A.B', 0)['status'], 'FAIL')

    def test_running_prefix_is_a_real_suite_not_a_banner(self):
        listing = 'Running main() from gmock_main.cc\nRunningRegression.\n  NewFailure\nTyped/Values. # TypeParam = int\n  Valid/0 # parameter\n'
        self.assertEqual(ci.parse_discovery(listing),
                         ['RunningRegression.NewFailure', 'Typed/Values.Valid/0'])

    def test_source_root_spaces_normalize(self):
        self.assertEqual(ci.normalize('/tmp/source with spaces/telephony_core_service/t.cc:8\nbad'),
                         'telephony_core_service/t.cc:8\nbad')

    def test_path_normalization_preserves_assertions(self):
        self.assertEqual(ci.normalize('/tmp/a/telephony_core_service/test.cc:7\nExpected 1, actual 0'),
                         'telephony_core_service/test.cc:7\nExpected 1, actual 0')


if __name__ == '__main__':
    unittest.main()
