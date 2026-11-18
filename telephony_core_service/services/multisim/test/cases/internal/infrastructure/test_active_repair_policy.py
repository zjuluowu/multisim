"""Published repairs cannot silently revert to historical failure acceptance."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

TEST = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(TEST / 'scripts'))
import ci_gate


class ActiveRepairPolicy(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.path = self.root / 'cases/data/resolved_failures.json'
        self.path.parent.mkdir(parents=True)
        self.names = sorted(ci_gate.load_known())

    def load(self, names):
        self.path.write_text(json.dumps({'resolved': names}))
        with patch.object(ci_gate, 'TEST', self.root), patch.object(ci_gate, 'load_known', return_value=self.names):
            return ci_gate.load_resolved()

    def test_all_reviewed_repairs_remain_required(self):
        self.assertEqual(self.load(self.names), set(self.names))

    def test_missing_file_cannot_restore_failure_acceptance(self):
        with patch.object(ci_gate, 'TEST', self.root):
            with self.assertRaisesRegex(RuntimeError, 'policy missing'):
                ci_gate.load_resolved()

    def test_removed_repair_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'all five'):
            self.load(self.names[:-1])

    def test_duplicate_repair_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'exactly once'):
            self.load(self.names + [self.names[0]])

    def test_unknown_repair_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'all five'):
            self.load(self.names[:-1] + ['Unreviewed.Defect'])


if __name__ == '__main__':
    unittest.main()
