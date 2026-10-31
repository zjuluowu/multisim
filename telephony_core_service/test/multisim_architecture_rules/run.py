#!/usr/bin/env python3
"""Check skeleton boundaries and run pure rules; optionally compile every unit with GN."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_structure import check, check_migration


def syntax_check(compiler, source, output):
    units = sorted(p for p in source.rglob("*") if p.suffix in (".h", ".cpp"))
    flags = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(source)]
    for unit in units:
        subprocess.run(flags + ["-x", "c++", "-fsyntax-only", str(unit)], check=True)
    combined = output / "all_headers.cpp"
    combined.write_text("".join('#include "' + p.relative_to(source).as_posix() + '"\n'
                                for p in units if p.suffix == ".h"))
    subprocess.run(flags + ["-fsyntax-only", str(combined)], check=True)
    print(f"syntax: PASS ({len(units)} independent units and one combined header TU)", flush=True)


def gn_build(compiler, source, output):
    component = source.parents[1]
    dotfile = Path(__file__).parent / "gn/.gn"
    build = output / "gn"
    subprocess.run(["gn", "gen", str(build), "--root=" + str(component),
                    "--dotfile=" + str(dotfile), "--check",
                    "--args=multisim_cxx=" + json.dumps(shutil.which(compiler) or compiler)], check=True)
    subprocess.run(["ninja", "-C", str(build), "-j", "4"], check=True)
    print("GN: PASS (all skeleton objects; no production link or startup)", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="clang++")
    parser.add_argument("--check-syntax", action="store_true", help="Check each header/source independently")
    parser.add_argument("--with-gn", action="store_true", help="Also check headers and build all GN objects")
    parser.add_argument("--migration", action="store_true", help="Verify the recorded 73-file baseline map")
    parser.add_argument("--output", type=Path, help="Keep results in this directory outside the repository")
    args = parser.parse_args()
    tests = Path(__file__).resolve().parent
    source = tests.parents[1] / "services" / "multisim"
    print("structure: PASS " + json.dumps(check(source), sort_keys=True), flush=True)
    if args.migration:
        check_migration()
    subprocess.run([sys.executable, "-B", str(tests / "test_structure.py")], check=True)
    suites = {
        "topology": ["foundation/slot_catalog.cpp", "topology/accounts/binding_index.cpp",
                     "topology/accounts/account_registry.cpp",
                     "topology/discovery/discovery_observation_ledger.cpp",
                     "topology/activation/activation_progress.cpp"],
        "resource": ["runtime/resource/resource_barrier.cpp"],
        "assignment": ["assignment/domain/assignment_aggregate.cpp",
                       "assignment/primary/primary_switch_plan.cpp",
                       "runtime/euicc/local_card_requests.cpp"],
        "runtime_protocol": ["runtime/session/session_registry.cpp"],
    }
    with tempfile.TemporaryDirectory(prefix="multisim-architecture-rules-") as temporary:
        output = (args.output or Path(temporary)).resolve()
        if output.is_relative_to(source.parents[2]):
            parser.error("--output must be outside the repository")
        output.mkdir(parents=True, exist_ok=True)
        if args.check_syntax or args.with_gn:
            syntax_check(args.compiler, source, output)
        for name, sources in suites.items():
            binary = Path(output) / name
            command = [args.compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                       "-UNDEBUG", "-I", str(source), str(tests / (name + "_rules.cpp"))]
            command.extend(str(source / item) for item in sources)
            subprocess.run(command + ["-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
            print(name + ": PASS (pure rules only)", flush=True)
        if args.with_gn:
            gn_build(args.compiler, source, output)


if __name__ == "__main__":
    main()
