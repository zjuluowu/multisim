#!/usr/bin/env python3
# Copyright (c) 2026 multisim contributors. Licensed under the Apache License, Version 2.0.
"""Compile and run isolated skeleton protocol checks; never loads production SIM objects."""
import concurrent.futures
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = os.environ.get("CXX", "clang++")
FLAGS = [COMPILER, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT)]
SUITES = {
    "topology_protocol_test": [
        "model/slot_catalog.cpp", "topology/domain/account_registry.cpp",
        "topology/domain/binding_index.cpp", "topology/application/discovery_admission.cpp",
        "topology/application/activation_progress.cpp", "topology/application/show_number_plan.cpp",
    ],
    "assignment_completion_profile_test": [
        "assignment/application/primary_switch_plan.cpp", "application/card_access/local_card_requests.cpp",
    ],
    "runtime_resource_factory_test": ["runtime/resource/resource_barrier.cpp", "runtime/session/session_registry.cpp"],
}


def checked(command):
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(" ".join(map(str, command)) + "\n" + result.stdout + result.stderr)
    return result.stdout


def syntax(path):
    return checked(FLAGS + ["-fsyntax-only", "-x", "c++", str(path)])


def main():
    with tempfile.TemporaryDirectory(prefix="multisim-review-host-") as output:
        for suite, sources in SUITES.items():
            binary = Path(output) / suite
            checked(FLAGS + [str(ROOT / "tests" / (suite + ".cpp"))] +
                    [str(ROOT / source) for source in sources] + ["-o", str(binary)])
            print(checked([str(binary)]).strip())
            print(suite + ": PASS", flush=True)
        units = sorted(ROOT.rglob("*.h")) + sorted(ROOT.rglob("*.cpp"))
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            list(pool.map(syntax, units))
        print(f"Standalone C++17 syntax: {len(units)} units PASS", flush=True)
    print("Scope: pure protocols and private contracts; no OHOS link, provider, device or production migration.")


if __name__ == "__main__":
    main()
