#!/usr/bin/env python3
"""Real include/GN mutations in a disposable copy exercise the boundary guard."""
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
from check_structure import SOURCE, check


class BoundaryGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="multisim-boundary-negative-")
        cls.source = Path(cls.temporary.name) / "multisim"
        shutil.copytree(SOURCE, cls.source)

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
            ("foundation/sim_identity.h", "../topology/topology_service.h"),
            ("topology/accounts/account_registry.h", "../../foundation/orchestration/request_context.h"),
            ("topology/topology_service.h", "assignment/assignment_service.h"),
            ("runtime/card_runtime.h", "topology/account_profile/set_show_number_use_case.h"),
            ("runtime/card_runtime.h", "compatibility/api/legacy_boundary_ports.h"),
            ("compatibility/legacy_sim_facade.h", "topology/topology_service.h"),
            ("topology/api/topology_requests.h", "topology/accounts/account_registry.h"),
        ]
        for source, included in cases:
            with self.subTest(source=source, included=included):
                self.mutate(source, lambda s: s + '\n#include "' + included + '"\n', "Forbidden include")

    def test_missing_include(self):
        self.mutate("foundation/sim_identity.h", lambda s: s + '\n#include "missing_header.h"\n',
                    "Missing or out-of-scope")

    def test_macro_and_continued_includes(self):
        self.mutate("foundation/sim_identity.h", lambda s: s +
                    '\n#define PRIVATE_HEADER "topology/topology_service.h"\n#include PRIVATE_HEADER\n',
                    "Unsupported non-literal include")
        self.mutate("foundation/sim_identity.h", lambda s: s +
                    '\n#include \\\n "topology/topology_service.h"\n', "Forbidden include")

    def test_include_cycle(self):
        self.mutate("foundation/sim_identity.h", lambda s: s + '\n#include "foundation/request_ref.h"\n',
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
        self.mutate("BUILD.gn", lambda s: s.replace('  public = [ "foundation/orchestration/request_context.h" ]',
                    '  public = []'), "Non-public GN header")

    def test_duplicate_owner(self):
        self.mutate("foundation/sim_identity.h", lambda s: s + "\nstruct RequestId { int duplicate; };\n",
                    "Expected one definition of RequestId")

    def test_unlisted_source(self):
        path = self.source / "runtime/records/forgotten.h"
        try:
            path.write_text("struct Forgotten {};\n")
            with self.assertRaisesRegex(ValueError, "exactly one GN target"):
                check(self.source)
        finally:
            path.unlink()

    def test_comment_does_not_add_dependency(self):
        path = self.source / "foundation/sim_identity.h"
        before = path.read_text()
        try:
            path.write_text(before + '\n/*\n#include "topology/topology_service.h"\n*/\n')
            self.assertGreater(check(self.source)["files"], 0)
        finally:
            path.write_text(before)


if __name__ == "__main__":
    unittest.main()
