"""Public dependency and per-operation native capability boundary controls."""
import importlib.util,shutil,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
T=next(p for p in Path(__file__).resolve().parents if (p/'scripts/_paths.py').is_file());sys.path.insert(0,str(T/'scripts'))
import check_public_api as guard
class PublicBoundary(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name);(self.root/'cases').mkdir()
  for name in ['sim_test.h','sim_protocol_test.h','internal/common/sim_types.h','internal/common/resource_catalog.h','internal/common/observations.h','internal/common/state_assertions.h']:
   dst=self.root/'framework'/name;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(T/'framework'/name,dst)
  (self.root/'cases/plain.cpp').write_text('#include "sim_test.h"\n')
 def tearDown(self):self.temp.cleanup()
 def check(self):
  with patch.object(guard,'TEST',self.root):return guard.check()
 def test_supported_graph(self):self.assertEqual(self.check()['public_host_includes'],0)
 def test_direct_host_include_rejected(self):
  with (self.root/'framework/sim_test.h').open('a') as f:f.write('\n#include "internal/host/runtime.h"\n')
  with self.assertRaises(RuntimeError):self.check()
 def test_transitive_private_type_rejected(self):
  with (self.root/'framework/internal/common/sim_types.h').open('a') as f:f.write('\nstruct SimHost {};\n')
  with self.assertRaises(RuntimeError):self.check()
 def test_ordinary_mock_usage_rejected(self):
  (self.root/'cases/plain.cpp').write_text('EXPECT_CALL(mock,Operation());\n')
  with self.assertRaises(RuntimeError):self.check()
 def test_native_read_only_setup_requires_no_mutation_resource(self):
  text=(T/'framework/internal/device/sim_test.cpp').read_text();setup=text.split('void SimTest::SetUp()',1)[1].split('void SimTest::TearDown()',1)[0]
  for forbidden in ['SIM_DEVICE_CONTROL_SOCKET','SET_TELEPHONY_STATE','RESERVE','< 2']:
   self.assertNotIn(forbidden,setup)
  self.assertIn('GET_TELEPHONY_STATE',setup)
 def test_native_identity_permission_is_not_required_by_read_only_queries(self):
  text=(T/'framework/internal/device/sim_test.cpp').read_text()
  self.assertIn('Snapshot Read(int slot, bool identity = false)',text)
  block=text.split('Snapshot Read(',1)[1].split('struct SimTest::Environment',1)[0]
  self.assertIn('if (identity)',block)
if __name__=='__main__':unittest.main()
