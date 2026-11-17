import sys
"""Result protocol regression tests; no SIM business behavior is implemented here."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

script = next(p for p in Path(__file__).resolve().parents if (p/"scripts/_paths.py").is_file()) / 'scripts/run_gtest.py'
sys.path.insert(0, str(script.parent))
spec = importlib.util.spec_from_file_location('isolated_gtest', script)
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class ResultProtocol(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.xml = Path(self.directory.name) / 'result.xml'

    def classify(self, body='', code=0, name='Selected', status='run'):
        self.xml.write_text('<testsuites><testsuite><testcase classname="Suite" name="' + name +
                            '" status="' + status + '">' + body + '</testcase></testsuite></testsuites>')
        return runner.classify(self.xml, 'Suite.Selected', code)[0]

    def test_exact_case_passes(self):
        self.assertEqual(self.classify(), 'PASS')

    def test_wrong_selected_case_cannot_pass(self):
        self.assertEqual(self.classify(name='Other'), 'FAIL')

    def test_failure_preserves_failure(self):
        self.assertEqual(self.classify('<failure>business assertion</failure>'), 'FAIL')

    def test_exit_failure_overrides_valid_xml(self):
        self.assertEqual(self.classify(code=1), 'FAIL')

    def test_timeout_is_failure(self):
        self.assertEqual(self.classify(code=124), 'FAIL')

    def test_skip_is_unsupported(self):
        self.assertEqual(self.classify('<skipped>Missing test card</skipped>'), 'UNSUPPORTED')

    def test_unexecuted_is_not_pass(self):
        self.assertEqual(self.classify(status='notrun'), 'NOT_RUN')

    def test_crash_after_skip_is_failure(self):
        self.assertEqual(self.classify('<skipped/>', code=-11), 'FAIL')

    def test_missing_and_malformed_xml(self):
        self.assertEqual(runner.classify(self.xml, 'Suite.Selected', 0)[0], 'FAIL')
        self.xml.write_text('truncated')
        self.assertEqual(runner.classify(self.xml, 'Suite.Selected', 0)[0], 'FAIL')

    def test_empty_execution_is_failure(self):
        self.xml.write_text('<testsuites/>')
        self.assertEqual(runner.classify(self.xml, 'Suite.Selected', 0)[0], 'FAIL')


if __name__ == '__main__':
    unittest.main()
