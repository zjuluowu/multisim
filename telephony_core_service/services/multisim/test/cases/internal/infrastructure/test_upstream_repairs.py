"""Local repair fingerprints must not hide unrelated upstream drift."""
import hashlib
import io
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

TEST = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(TEST / 'scripts'))
import check_upstream_sync as sync


class UpstreamRepairProvenance(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / 'docs').mkdir()
        self.source = self.root / 'source.cpp'
        self.source.write_bytes(b'repaired')
        self.patch = self.root / 'repair.patch'
        self.patch.write_bytes(b'explicit repair delta')
        sha = lambda value: hashlib.sha256(value).hexdigest()
        self.repair = dict(upstream_sha256=sha(b'upstream'), repaired_sha256=sha(b'repaired'),
                           patch='repair.patch', patch_sha256=sha(self.patch.read_bytes()))
        self.spec = dict(upstream='pinned', reference_sources={}, local_repairs={'source.cpp': self.repair})
        stream = io.BytesIO()
        with tarfile.open(fileobj=stream, mode='w') as archive:
            item = tarfile.TarInfo('source.cpp')
            item.size = len(b'upstream')
            archive.addfile(item, io.BytesIO(b'upstream'))
        self.archive = stream.getvalue()

    def check(self):
        (self.root / 'docs/Upstream_Sync_Test_Adaptation.json').write_text(json.dumps(self.spec))
        with patch.object(sync, 'TEST', self.root), patch.object(sync, 'CORE', self.root), \
                patch.object(sync.subprocess, 'check_output', return_value=self.archive):
            return sync.check()

    def test_known_repair_is_reported_separately(self):
        result = self.check()
        self.assertEqual(result['files'], [])
        self.assertEqual(result['local_repairs'], ['source.cpp'])

    def test_unlisted_source_drift_is_rejected(self):
        self.spec['local_repairs'] = {}
        with self.assertRaisesRegex(RuntimeError, 'Upstream file differs'):
            self.check()

    def test_repaired_source_drift_is_rejected(self):
        self.source.write_bytes(b'unrelated change')
        with self.assertRaisesRegex(RuntimeError, 'Local repair provenance differs'):
            self.check()

    def test_patch_drift_is_rejected(self):
        self.patch.write_bytes(b'changed patch')
        with self.assertRaisesRegex(RuntimeError, 'Local repair provenance differs'):
            self.check()

    def test_upstream_base_drift_is_rejected(self):
        self.repair['upstream_sha256'] = 'wrong'
        with self.assertRaisesRegex(RuntimeError, 'Local repair provenance differs'):
            self.check()

    def test_unknown_repair_path_is_rejected(self):
        self.source.write_bytes(b'upstream')
        self.spec['local_repairs'] = {'unknown.cpp': self.repair}
        with self.assertRaisesRegex(RuntimeError, 'does not name an upstream source'):
            self.check()


if __name__ == '__main__':
    unittest.main()
