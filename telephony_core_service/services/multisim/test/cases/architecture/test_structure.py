#!/usr/bin/env python3
"""Real include/GN mutations in a disposable copy exercise the boundary guard."""

import sys as _sys
from pathlib import Path as _Path
_TEST_ROOT = next(p for p in _Path(__file__).resolve().parents if (p / "scripts/_paths.py").is_file())
_sys.path.insert(0, str(_TEST_ROOT / "scripts"))
from _paths import TEST as _TEST, CORE as _CORE, REPO as _REPO, locate, guard_asset
_sys.path.insert(0, str(_TEST / "cases/architecture"))
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
from check_structure import SOURCE, check, token_sequence
from check_directory_migration import check_directory_migration


class BoundaryGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="multisim-boundary-negative-")
        cls.source = Path(cls.temporary.name) / "multisim"
        shutil.copytree(SOURCE, cls.source, ignore=shutil.ignore_patterns("test", "tools"))

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def mutate(self, name, transform, expected):
        path = self.source / name
        before = path.read_text()
        try:
            path.write_text(transform(before))
            with self.assertRaisesRegex(ValueError, expected):
                check(self.source)
        finally:
            path.write_text(before)

    def test_forbidden_includes(self):
        cases = [
            ("infrastructure/compatibility/api/real_sim_count_query.h", "account/topology_service.h"),
            ("infrastructure/compatibility/api/real_sim_count_query.h", "card/runtime/card_runtime.h"),
            ("infrastructure/sim_identity.h", "account/topology_service.h"),
            ("account/account_registry.h", "infrastructure/request/request_context.h"),
            ("account/topology_service.h", "role_assignment/assignment_service.h"),
            ("card/runtime/card_runtime.h", "account/number/set_show_number_use_case.h"),
            ("card/runtime/card_runtime.h", "infrastructure/compatibility/api/legacy_boundary_ports.h"),
            ("infrastructure/compatibility/api/legacy_sim_facade.h", "account/topology_service.h"),
            ("account/topology_requests.h", "account/account_registry.h"),
        ]
        for source, included in cases:
            with self.subTest(source=source, included=included):
                self.mutate(source, lambda s: s + '\n#include "' + included + '"\n', "Forbidden include")

    def test_missing_include(self):
        self.mutate("infrastructure/sim_identity.h", lambda s: s + '\n#include "missing_header.h"\n',
                    "Missing or out-of-scope")

    def test_macro_and_continued_includes(self):
        self.mutate("infrastructure/sim_identity.h", lambda s: s +
                    '\n#define PRIVATE_HEADER "account/topology_service.h"\n#include PRIVATE_HEADER\n',
                    "Unsupported non-literal include")
        self.mutate("infrastructure/sim_identity.h", lambda s: s +
                    '\n#include \\\n "account/topology_service.h"\n', "Forbidden include")

    def test_include_cycle(self):
        self.mutate("infrastructure/sim_identity.h", lambda s: s + '\n#include "infrastructure/request/request_ref.h"\n',
                    "include cycle")

    def test_target_cycle(self):
        def add_dependency(text):
            return text.replace('source_set("assignment_contracts") {',
                                'source_set("assignment_contracts") {\n  deps = [ ":topology_contracts" ]', 1)
        self.mutate("BUILD.gn", add_dependency, "GN target cycle")

    def test_target_reverse_dependency(self):
        self.mutate("BUILD.gn", lambda s: s.replace('source_set("sim_values") {',
                    'source_set("sim_values") {\n  deps = [ ":runtime_core" ]', 1), "Forbidden GN dependency")

    def test_target_dynamic_list(self):
        self.mutate("BUILD.gn", lambda s: s.replace('source_set("sim_values") {',
                    'source_set("sim_values") {\n  deps += [ ":runtime_core" ]', 1), "Unsupported dynamic GN list")

    def test_target_not_reachable(self):
        self.mutate("BUILD.gn", lambda s: s.replace('    ":assembly_core",\n', ''),
                    "GN skeleton group must cover")

    def test_private_header_export(self):
        self.mutate("BUILD.gn", lambda s: s.replace('  public = [ "infrastructure/request/request_context.h" ]',
                    '  public = []'), "Non-public GN header")

    def test_duplicate_owner(self):
        self.mutate("infrastructure/sim_identity.h", lambda s: s + "\nstruct RequestId { int duplicate; };\n",
                    "Expected one definition of RequestId")

    def check_mapped_mutation(self, name, transform):
        path = self.source / name
        before = path.read_text()
        try:
            path.write_text(transform(before))
            with self.assertRaisesRegex(ValueError, "changed bytes beyond mapped paths"):
                check_directory_migration(self.source, token_sequence, repo=SOURCE.parents[2])
        finally:
            path.write_text(before)

    def test_move_preserves_cpp_members(self):
        self.check_mapped_mutation("infrastructure/sim_identity.h",
                                  lambda s: s.replace("struct SlotId {", "struct SlotId { int extra;", 1))

    def test_move_preserves_gn_visibility(self):
        self.check_mapped_mutation("BUILD.gn", lambda s: s.replace('":*"', '"//*"', 1))

    def test_retired_top_level_directory(self):
        for name in ("common", "support", "integration", "startup", "compatibility", "card_selection"):
            path = self.source / name
            path.mkdir()
            try:
                with self.subTest(directory=name):
                    with self.assertRaisesRegex(ValueError, "Unsupported top-level directory"):
                        check(self.source)
            finally:
                path.rmdir()

    def test_unlisted_source(self):
        path = self.source / "card/records/forgotten.h"
        try:
            path.write_text("struct Forgotten {};\n")
            with self.assertRaisesRegex(ValueError, "exactly one GN target"):
                check(self.source)
        finally:
            path.unlink()

    def test_comment_does_not_add_dependency(self):
        path = self.source / "infrastructure/sim_identity.h"
        before = path.read_text()
        try:
            path.write_text(before + '\n/*\n#include "account/topology_service.h"\n*/\n')
            self.assertGreater(check(self.source)["files"], 0)
        finally:
            path.write_text(before)


if __name__ == "__main__":
    unittest.main()
