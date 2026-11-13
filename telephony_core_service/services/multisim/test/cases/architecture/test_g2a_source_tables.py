#!/usr/bin/env python3
"""Negative controls for the bounded G2a GN source-table/execution audit."""

import sys as _sys
from pathlib import Path as _Path
_TEST_ROOT = next(p for p in _Path(__file__).resolve().parents if (p / "scripts/_paths.py").is_file())
_sys.path.insert(0, str(_TEST_ROOT / "scripts"))
from _paths import TEST as _TEST, CORE as _CORE, REPO as _REPO, locate, guard_asset
import unittest

from run_g2a_query import CORE, SOURCES, is_method, verify_source_tables


class G2aSourceTables(unittest.TestCase):
    def setUp(self):
        self.files = {name: (CORE / name).read_text() for name in [
            "BUILD.gn", "test/BUILD.gn", "test/core_service_test.gni",
            "test/unittest/raw_parcel_core_service_gtest/BUILD.gn", "services/multisim/test/framework/host/BUILD.gn",
            "services/multisim/BUILD.gn",
        ]}

    def rejected(self, name, before, after):
        self.assertIn(before, self.files[name])
        self.files[name] = self.files[name].replace(before, after, 1)
        with self.assertRaises(RuntimeError):
            verify_source_tables(self.files.__getitem__)

    def test_current_tables(self):
        self.assertTrue(verify_source_tables(self.files.__getitem__)["isolated_testonly_leaf"])

    def test_commented_source_in_each_table(self):
        for name in self.files:
            if name == "services/multisim/BUILD.gn":
                continue
            with self.subTest(table=name):
                original = self.files[name]
                line = next(line for line in original.splitlines() if SOURCES[2] in line)
                self.rejected(name, line, "#" + line)
                self.files[name] = original

    def test_missing_include(self):
        self.rejected("BUILD.gn", '"services/multisim/infrastructure/compatibility/api",', "")

    def test_broad_include(self):
        self.rejected("BUILD.gn", '"services/multisim/infrastructure/compatibility/api"', '"services/multisim"')

    def test_unrelated_production_source(self):
        self.rejected("BUILD.gn", '"' + SOURCES[2] + '",',
                      '"' + SOURCES[2] + '", "services/multisim/account/topology_service.cpp",')

    def test_duplicate_source(self):
        self.rejected("BUILD.gn", '"' + SOURCES[2] + '",', ('"' + SOURCES[2] + '",') * 2)

    def test_source_in_wrong_target(self):
        line = '    "' + SOURCES[2] + '",'
        self.files["BUILD.gn"] = self.files["BUILD.gn"].replace(line, "") + (
            '\nsource_set("wrong") { sources = ["' + SOURCES[2] + '"] }\n')
        with self.assertRaises(RuntimeError):
            verify_source_tables(self.files.__getitem__)

    def test_conditional_query(self):
        line = '    "' + SOURCES[2] + '",'
        self.rejected("BUILD.gn", line, ']\nif (product_feature) { sources += [' + line + '] }\nsources += [')

    def test_source_list_overwritten(self):
        self.rejected("BUILD.gn", '  include_dirs = [', '  sources = []\n  include_dirs = [')

    def test_source_removed_after_addition(self):
        self.rejected("BUILD.gn", '  include_dirs = [',
                      '  sources -= ["' + SOURCES[2] + '"]\n  include_dirs = [')

    def test_launcher_missing_include(self):
        self.rejected("services/multisim/test/framework/host/BUILD.gn", '    "$production_root/services/multisim/infrastructure/compatibility/api",', "")

    def test_leaf_cannot_become_production(self):
        # Select only the query block, leaving all other skeleton targets intact.
        name = "services/multisim/BUILD.gn"
        self.rejected(name, 'source_set("readonly_query") {\n  testonly = true',
                      'source_set("readonly_query") {\n  testonly = false')

    def test_leaf_cannot_import_whole_skeleton(self):
        self.rejected("services/multisim/BUILD.gn", 'source_set("readonly_query") {',
                      'source_set("readonly_query") {\n  public_configs = [ ":skeleton_includes" ]')

    def test_exact_execution_method(self):
        name = {"name": "_ZN4OHOS9Telephony21LegacySimQueryBinding22ReadLegacyRealSimCountEv"}
        self.assertTrue(is_method(name, "LegacySimQueryBinding", "ReadLegacyRealSimCount"))
        self.assertFalse(is_method(name, "LegacySimQueryBinding", "Read"))
        self.assertTrue(is_method({"name": "_ZN4OHOS9Telephony17RealSimCountQuery4ReadEv"}, "RealSimCountQuery", "Read"))
        boot = {"name": "_ZN4OHOS9Telephony10SimManager6OnInitEv"}
        self.assertTrue(is_method(boot, "SimManager", "OnInit"))
        self.assertFalse(is_method(boot, "SimManager", "Init"))


if __name__ == "__main__":
    unittest.main()
