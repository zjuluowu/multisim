#!/usr/bin/env python3
"""Reject production reattachment and incomplete test reference builds."""
import sys
from pathlib import Path
import unittest
TEST = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(TEST / "scripts"))
from source_boundaries import CORE, verify_source_tables, is_method
class SourceBoundary(unittest.TestCase):
    def setUp(self):
        self.names = ["BUILD.gn", "test/BUILD.gn", "test/core_service_test.gni",
                      "test/unittest/raw_parcel_core_service_gtest/BUILD.gn",
                      "services/multisim/test/framework/internal/host/BUILD.gn", "services/multisim/BUILD.gn"]
        self.files = {n: (CORE / n).read_text() for n in self.names}
    def test_current(self):
        self.assertTrue(verify_source_tables(self.files.__getitem__)["production_attachment_absent"])
    def test_production_leaks(self):
        for name in self.names[:4]:
            for source in ["services/multisim/infrastructure/compatibility/api/real_sim_count_query.cpp",
                           "services/core/src/legacy_sim_query_binding.cpp"]:
                with self.subTest(table=name, source=source):
                    original = self.files[name]
                    self.files[name] += '\nsources += ["' + source + '"]\n'
                    with self.assertRaises(RuntimeError): verify_source_tables(self.files.__getitem__)
                    self.files[name] = original
    def test_reference_sources_required_once(self):
        name = self.names[4]
        original = self.files[name]
        for source in ["$test_root/framework/internal/host/g2a_reference/core_service_sim.cpp",
                       "$test_root/framework/internal/host/g2a_reference/legacy_sim_query_binding.cpp",
                       "$production_root/services/multisim/infrastructure/compatibility/api/real_sim_count_query.cpp"]:
            for replacement in ["", '"' + source + '", "' + source + '"']:
                with self.subTest(source=source, replacement=replacement):
                    self.files[name] = original.replace('"' + source + '"', replacement)
                    with self.assertRaises(RuntimeError): verify_source_tables(self.files.__getitem__)
        self.files[name] = original
    def test_leaf_testonly(self):
        name = self.names[5]
        self.files[name] = self.files[name].replace('source_set("readonly_query") {\n  testonly = true', 'source_set("readonly_query") {\n  testonly = false')
        with self.assertRaises(RuntimeError): verify_source_tables(self.files.__getitem__)
    def test_method_matching(self):
        self.assertTrue(is_method({"name": "_ZN4OHOS9Telephony17RealSimCountQuery4ReadEv"}, "RealSimCountQuery", "Read"))
        self.assertFalse(is_method({"name": "_ZN4OHOS9Telephony21LegacySimQueryBinding22ReadLegacyRealSimCountEv"}, "LegacySimQueryBinding", "Read"))
if __name__ == "__main__": unittest.main()
