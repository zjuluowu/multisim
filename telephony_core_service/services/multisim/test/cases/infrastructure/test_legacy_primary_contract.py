#!/usr/bin/env python3
"""Negative controls for separating business evidence from original failing GTest."""

import sys as _sys
from pathlib import Path as _Path
_TEST_ROOT = next(p for p in _Path(__file__).resolve().parents if (p / "scripts/_paths.py").is_file())
_sys.path.insert(0, str(_TEST_ROOT / "scripts"))
from _paths import TEST as _TEST, CORE as _CORE, REPO as _REPO, locate, guard_asset
import unittest
import xml.etree.ElementTree as ET
from run_legacy_primary_contract import classify


def fixture():
    case = ET.Element('testcase', status='run', result='completed')
    props = ET.SubElement(case, 'properties')
    for name, value in {'business_assertions_before_cleanup': 'PASS',
                        'strict_ril_expectations_verified': 'PASS', 'object_lifetime': 'FAIL'}.items():
        ET.SubElement(props, 'property', name=name, value=value)
    ET.SubElement(case, 'failure', message='../../primary_slot/real_scenario.h:321\n'
                  'Value of: SimHost::AwaitSchedulerStopped(3s)\n'
                  '  Actual: false\nExpected: true\nREAL_GRAPH_LIFETIME_NOT_DRAINED\n')
    return case


class EvidenceClassification(unittest.TestCase):
    def test_cleanup_does_not_turn_gtest_green(self):
        result = classify(fixture(), 1, '')
        self.assertEqual(result['business'], 'PASS')
        self.assertEqual(result['gtest'], 'FAIL')
        self.assertEqual(result['post_destruction_callback_safety'], 'NOT_VERIFIED')

    def test_business_failure_is_not_accepted(self):
        case = fixture()
        ET.SubElement(case, 'failure', message='real_test.cpp:99 PRIMARY_COMMIT_TARGET')
        self.assertEqual(classify(case, 1, '')['business'], 'FAIL')

    def test_marker_substring_is_not_accepted(self):
        case = fixture()
        case.find('failure').set('message', 'real_scenario.h:321\nwrong business value REAL_GRAPH_LIFETIME_NOT_DRAINED')
        self.assertEqual(classify(case, 1, '')['business'], 'FAIL')

    def test_mock_failure_is_not_accepted(self):
        case = fixture()
        ET.SubElement(case, 'failure', message='Unexpected mock function call')
        self.assertEqual(classify(case, 1, '')['business'], 'FAIL')

    def test_missing_business_marker_is_not_accepted(self):
        case = fixture()
        case.find('properties').remove(case.find('properties/property'))
        self.assertEqual(classify(case, 1, '')['business'], 'FAIL')

    def test_strict_verification_must_pass(self):
        case = fixture()
        case.findall('properties/property')[1].set('value', 'FAIL')
        self.assertEqual(classify(case, 1, '')['business'], 'FAIL')

    def test_crash_timeout_and_sanitizer_fail_closed(self):
        for code, log in [(-11, ''), (124, ''), (87, ''), (1, 'AddressSanitizer: heap-use-after-free'),
                          (1, 'ThreadSanitizer: data race')]:
            with self.subTest(code=code, log=log):
                self.assertEqual(classify(fixture(), code, log)['business'], 'FAIL')

    def test_skip_is_not_accepted(self):
        case = fixture()
        ET.SubElement(case, 'skipped')
        self.assertEqual(classify(case, 1, '')['business'], 'FAIL')

    def test_incomplete_xml_is_not_accepted(self):
        case = fixture()
        case.set('result', 'suppressed')
        self.assertEqual(classify(case, 1, '')['business'], 'FAIL')

    def test_zero_exit_with_failure_is_not_accepted(self):
        self.assertEqual(classify(fixture(), 0, '')['business'], 'FAIL')

    def test_real_full_pass_remains_pass(self):
        case = ET.Element('testcase', status='run', result='completed')
        self.assertEqual(classify(case, 0, '')['gtest'], 'PASS')

    def test_setup_failure_is_not_business_failure_or_pass(self):
        case = ET.Element('testcase', status='run', result='completed')
        ET.SubElement(case, 'failure', message='real_test.cpp:45 IsSimActive=false')
        result = classify(case, 1, '')
        self.assertEqual(result['business'], 'NOT_RUN')
        self.assertEqual(result['gtest'], 'FAIL')
        self.assertEqual(result['classification'], 'precondition_failure')

    def test_diagnostic_barrier_bypass_never_becomes_business_pass(self):
        case = fixture()
        ET.SubElement(case.find('properties'), 'property', name='initialization_ready_idle', value='NOT_RUN')
        result = classify(case, 1, '')
        self.assertEqual(result['business'], 'NOT_RUN')
        self.assertEqual(result['gtest'], 'FAIL')


if __name__ == '__main__':
    unittest.main()
