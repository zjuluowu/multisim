#!/usr/bin/env python3
"""Compile and run pure architecture rules without an OHOS or device runtime."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="clang++")
    args = parser.parse_args()
    tests = Path(__file__).resolve().parent
    source = tests.parents[1] / "services" / "multisim"
    suites = {
        "topology": ["model/slot_catalog.cpp", "topology/domain/binding_index.cpp",
                     "topology/domain/account_registry.cpp",
                     "topology/application/discovery_observation_ledger.cpp",
                     "topology/application/activation_progress.cpp"],
        "resource": ["runtime/resource/resource_barrier.cpp"],
        "assignment": ["assignment/domain/assignment_aggregate.cpp"],
    }
    with tempfile.TemporaryDirectory(prefix="multisim-architecture-rules-") as output:
        for name, sources in suites.items():
            binary = Path(output) / name
            command = [args.compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                       "-UNDEBUG", "-I", str(source), str(tests / (name + "_rules.cpp"))]
            command.extend(str(source / item) for item in sources)
            subprocess.run(command + ["-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
            print(name + ": PASS (pure rules only)", flush=True)


if __name__ == "__main__":
    main()
