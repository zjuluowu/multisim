import copy
from pathlib import Path
import sys
import unittest

TEST = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(TEST / 'scripts'))
from verify_migration import is_reviewed_repair


class RepairComparison(unittest.TestCase):
    def setUp(self):
        self.mapping = dict(old_id='Original.Case', origin='host_baseline')
        self.old = dict(status='FAIL')
        self.new = dict(status='PASS', exit_code=0, valid_xml=True, unsafe_runtime=False)
        self.policy = dict(old_id='Original.Case', origin='host_baseline', before_status='FAIL',
                           after_status='PASS', allowed_observation_changes=['observations'])

    def accepted(self, *, policy=None, failures=None, differences=None):
        return is_reviewed_repair(self.mapping, self.old, self.new, failures or [],
                                  differences or {}, self.policy if policy is None else policy)

    def test_explicit_repair_preserves_allowed_raw_difference(self):
        self.assertTrue(self.accepted(differences={'observations': ['wrong', 'correct']}))

    def test_unreviewed_change_is_rejected(self):
        self.assertFalse(self.accepted(policy={}))

    def test_other_observation_change_is_rejected(self):
        self.assertFalse(self.accepted(differences={'external_calls': ['old', 'changed']}))

    def test_other_historical_identity_is_rejected(self):
        self.policy['old_id'] = 'Different.Case'
        self.assertFalse(self.accepted())

    def test_fail_skip_and_runtime_fault_are_rejected(self):
        for status in ['FAIL', 'UNSUPPORTED', 'BLOCKED']:
            self.new['status'] = status
            self.assertFalse(self.accepted())
        self.new['status'] = 'PASS'
        self.new['unsafe_runtime'] = True
        self.assertFalse(self.accepted())

    def test_assertions_exit_xml_and_original_pass_cannot_be_hidden(self):
        self.assertFalse(self.accepted(failures=['unexpected assertion']))
        for key, value in [('exit_code', 1), ('valid_xml', False)]:
            original = copy.copy(self.new)
            self.new[key] = value
            self.assertFalse(self.accepted())
            self.new = original
        self.old['status'] = 'PASS'
        self.assertFalse(self.accepted())


if __name__ == '__main__':
    unittest.main()
