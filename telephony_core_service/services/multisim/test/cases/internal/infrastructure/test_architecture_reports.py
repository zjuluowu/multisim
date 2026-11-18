"""Default editor/CLI reports retain architecture evidence without building in source."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

TEST = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(TEST / 'scripts'))
spec = importlib.util.spec_from_file_location('sim_cli', TEST / 'scripts/test.py')
cli = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cli)


class ArchitectureReports(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name) / 'repo'
        self.session = self.repo / '.host-out/results/session'
        self.session.mkdir(parents=True)
        self.execution = None

    def run_stage(self, code, produce=True):
        def run(name, script, options):
            self.assertEqual((name, script), ('architecture', 'run_architecture.py'))
            self.assertEqual(options[:2], ['--with-gn', '--migration'])
            self.execution = Path(options[-1])
            if produce:
                self.execution.mkdir(parents=True)
                (self.execution / 'evidence.txt').write_text('actual compiler output')
            return code
        with patch.object(cli, 'REPO', self.repo):
            return cli.run_architecture_stage(self.session, run)

    def test_default_report_retains_evidence_after_external_build(self):
        self.assertEqual(self.run_stage(0), 0)
        self.assertFalse(self.execution.is_relative_to(self.repo))
        self.assertFalse(self.execution.exists())
        self.assertEqual((self.session / 'architecture/evidence.txt').read_text(), 'actual compiler output')
        self.assertTrue((self.session / 'architecture-location.json').is_file())

    def test_failed_build_preserves_failure_and_partial_artifacts(self):
        self.assertEqual(self.run_stage(1), 1)
        self.assertTrue((self.session / 'architecture/evidence.txt').is_file())

    def test_early_block_without_artifacts_remains_blocked(self):
        self.assertEqual(self.run_stage(2, produce=False), 2)
        self.assertFalse((self.session / 'architecture').exists())

    def test_explicit_external_report_builds_directly_there(self):
        self.session = Path(self.temp.name) / 'external-report'
        self.session.mkdir()
        self.assertEqual(self.run_stage(0), 0)
        self.assertEqual(self.execution, self.session / 'architecture')
        self.assertTrue(self.execution.exists())
        self.assertFalse((self.session / 'architecture-location.json').exists())


if __name__ == '__main__':
    unittest.main()
