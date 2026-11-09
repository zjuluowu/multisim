#!/usr/bin/env python3
"""Run the independent SIM-OC-013 real Legacy hook contract; write only external artifacts."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

from run_sim_guard import CORE, REPO, SYSTEM, PRODUCTION, check_semantics, digest_tree, sha, xml_cases
from build_clock import await_build_timestamps


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gtest-root", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--mutation-check", action="store_true")
    args = parser.parse_args()
    output = Path(args.output).resolve()
    if output == REPO or REPO in output.parents:
        parser.error("Output must be outside the Git repository")
    output.mkdir(parents=True, exist_ok=True)
    session = Path(tempfile.mkdtemp(prefix="pin-memory-", dir=output))
    result = {
        "schema_version": 1, "contract": "SIM-OC-013", "status": "BLOCKED", "executed": False,
        "counts": {"PASS": 0, "FAIL": 0, "BLOCKED": 1}, "commands": [], "artifacts": str(session),
        "scope": "Real Legacy UnlockPin/UnlockPuk and removal hook at synthetic external callback boundary",
        "limits": ["No AlterPin/SetLockState/FDN binding in the current Driver", "No late-result policy change",
                   "No vendor asset, ABI, IPC, full Manager OnInit or device verification",
                   "Absent response identity is characterization, not approved asset cleanup policy"],
        "old_compatibility_suite": "NOT_RUN; original 17 scenarios and 5 defect cases unchanged",
    }

    def save():
        (output / "result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")

    def command(argv, label, timeout=180, env=None, allowed=(0,)):
        started = time.monotonic()
        log = session / (label + ".log")
        with log.open("w") as stream:
            process = subprocess.Popen(argv, cwd=session, stdout=stream, stderr=subprocess.STDOUT,
                                       env=env, start_new_session=True)
            try:
                code = process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                code = 124
        result["commands"].append({"argv": argv, "exit_code": code,
                                   "seconds": time.monotonic() - started, "log": log.name})
        save()
        if code not in allowed:
            raise RuntimeError(label + " failed; see external log (payload never added to diagnostic)")
        return code

    def audit_sources(directory, label, expected_manager):
        command(["gn", "desc", str(directory), "//:sim_guard_legacy_production", "sources",
                 "--root=" + str(SYSTEM), "--format=json"], label)
        table = json.loads((session / (label + ".log")).read_text())
        actual = table["//:sim_guard_legacy_production"]["sources"]
        expected = {str((CORE / p).resolve()) for p in PRODUCTION[:6]}
        expected.remove(str((CORE / PRODUCTION[0]).resolve()))
        expected.add(str(expected_manager.resolve()))
        if len(actual) != len(expected) or {str(Path(p).resolve()) for p in actual} != expected:
            raise RuntimeError("PIN host production source table differs from its declared normal/mutant inputs")
        result.setdefault("source_tables", {})[label] = actual

    save()
    try:
        result["source_commit"] = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=CORE, text=True).strip()
        result["dirty"] = bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=CORE, text=True))
        result["old_semantic_baseline"] = check_semantics()
        gtest = Path(args.gtest_root).resolve()
        lock = json.loads((SYSTEM / "configs/dependency_lock.json").read_text())
        if digest_tree(gtest) != lock["googletest"]["tree_sha256"]:
            raise RuntimeError("GoogleTest tree differs from pinned source")
        for header in lock["platform_headers"]:
            if sha(SYSTEM / header["path"]) != header["sha256"]:
                raise RuntimeError("Pinned platform header differs: " + header["path"])
        result["dependency_commit"] = lock["googletest"]["commit"]
        inputs = [CORE / p for p in PRODUCTION]
        inputs += [p for folder in [CORE / "services/multisim/test/sim_guard", SYSTEM / "stubs"]
                   for p in folder.rglob("*") if p.is_file() and p.suffix in (".h", ".cpp", ".py")]
        inputs += [SYSTEM / p for p in [".gn", "BUILD.gn", "gn/BUILD.gn", "gn/BUILDCONFIG.gn",
                                        "scripts/build_clock.py", "scripts/generate_ril_mock.py",
                                        "scripts/run_g2a_query.py", "configs/dependency_lock.json"]]
        result["inputs"] = [{"path": str(p.relative_to(CORE)), "sha256": sha(p)} for p in sorted(set(inputs))]
        result["build_clock"] = await_build_timestamps(SYSTEM)
        command([sys.executable, "-B", str(SYSTEM / "scripts/generate_ril_mock.py"), "--check"], "ril-interface")
        build = session / "build"
        options = ("production_root=" + json.dumps(str(CORE)) + " gtest_root=" + json.dumps(str(gtest)) +
                   ' sim_guard_state_source="" sim_guard_manager_source=""')
        command(["gn", "gen", str(build), "--root=" + str(SYSTEM), "--args=" + options], "gn")
        audit_sources(build, "normal-source-table", CORE / PRODUCTION[0])
        command(["ninja", "-C", str(build), "-j", "3", "sim_guard_pin_memory"], "build", timeout=360)
        binary = build / "sim_guard_pin_memory"
        xml = session / "pin-memory.xml"
        profile = session / "pin-memory.profraw"
        env = os.environ.copy()
        env["LLVM_PROFILE_FILE"] = str(profile)
        env["LD_LIBRARY_PATH"] = str(session)
        result["executed"] = True
        code = command([str(binary), "--gtest_shuffle", "--gtest_random_seed=41",
                        "--gtest_output=xml:" + str(xml)], "run", timeout=45, env=env, allowed=(0, 1))
        cases = xml_cases(xml)
        expected = {"PinMemoryContract." + name for name in [
            "PinSuccessUsesOriginalSlotIdentityAndCredential", "PukSuccessUsesNewPinAndPinAlterOperation",
            "FailedPinDoesNotPublishMemoryHook", "RemovalUsesAbsentResponseIdentityWithoutCredential"]}
        if len(cases) != len(expected) or {c["test"] for c in cases} != expected:
            raise RuntimeError("Missing, duplicate or unexpected PIN contract cases")
        if (code == 0) != all(c["status"] == "PASS" for c in cases):
            raise RuntimeError("GoogleTest exit/XML disagree")
        result["cases"] = cases
        result["counts"] = {"PASS": sum(c["status"] == "PASS" for c in cases),
                            "FAIL": sum(c["status"] == "FAIL" for c in cases), "BLOCKED": 0}
        profdata = session / "pin-memory.profdata"
        command(["llvm-profdata-18", "merge", "-sparse", str(profile), "-o", str(profdata)], "merge")
        command(["llvm-cov-18", "export", str(binary), "-instr-profile=" + str(profdata)], "coverage")
        coverage = json.loads((session / "coverage.log").read_text())
        functions = [f for data in coverage["data"] for f in data["functions"]]
        result["production_execution"] = []
        for owner, method, relative in [
            ("SimStateManager", "UnlockPin", PRODUCTION[0]),
            ("SimStateManager", "UnlockPuk", PRODUCTION[0]),
            ("SimStateHandle", "HandleSimAbsent", PRODUCTION[1]),
        ]:
            token = str(len(owner)) + owner + str(len(method)) + method
            matches = [f for f in functions if token in f["name"] and f["count"] > 0
                       and any(Path(n).resolve() == (CORE / relative).resolve() for n in f["filenames"])]
            if not matches:
                raise RuntimeError("Real production execution not proven: " + owner + "::" + method)
            result["production_execution"].append({"symbol": owner + "::" + method,
                "source": relative, "executions": sum(f["count"] for f in matches)})
        if args.mutation_check:
            manager = CORE / PRODUCTION[0]
            source = manager.read_text()
            start = source.index("int32_t SimStateManager::UnlockPuk(")
            end = source.index("int32_t SimStateManager::AlterPin(", start)
            body = source[start:end]
            if body.count("PinOperationType::PIN_ALTER") != 1:
                raise RuntimeError("PUK memory hook mutation anchor no longer unique")
            mutant = session / "sim_state_manager_mutant.cpp"
            if mutant.resolve() == REPO or REPO in mutant.resolve().parents:
                raise RuntimeError("Mutation source must be outside the Git repository")
            mutant.write_text(source[:start] + body.replace("PinOperationType::PIN_ALTER",
                                                         "PinOperationType::PIN_ENABLE") + source[end:])
            mutant_build = session / "mutant-build"
            mutated_options = options.replace('sim_guard_manager_source=""',
                                              "sim_guard_manager_source=" + json.dumps(str(mutant)))
            command(["gn", "gen", str(mutant_build), "--root=" + str(SYSTEM), "--args=" + mutated_options],
                    "mutant-gn")
            audit_sources(mutant_build, "mutant-source-table", mutant)
            command(["ninja", "-C", str(mutant_build), "-j", "3", "sim_guard_pin_memory"],
                    "mutant-build", timeout=360)
            mutant_xml = session / "mutation.xml"
            env["LLVM_PROFILE_FILE"] = str(session / "mutation.profraw")
            mutation_code = command([str(mutant_build / "sim_guard_pin_memory"),
                "--gtest_filter=PinMemoryContract.PukSuccessUsesNewPinAndPinAlterOperation",
                "--gtest_output=xml:" + str(mutant_xml)], "mutation", timeout=45, env=env, allowed=(0, 1))
            mutant_cases = xml_cases(mutant_xml)
            if mutation_code != 1 or len(mutant_cases) != 1 or mutant_cases[0]["status"] != "FAIL":
                raise RuntimeError("Spy failed to detect the real PUK memory hook operation mutation")
            restored_xml = session / "restored.xml"
            env["LLVM_PROFILE_FILE"] = str(session / "restored.profraw")
            command([str(binary), "--gtest_output=xml:" + str(restored_xml)],
                    "restored", timeout=45, env=env)
            restored = xml_cases(restored_xml)
            if len(restored) != len(expected) or {c["test"] for c in restored} != expected or any(
                    c["status"] != "PASS" for c in restored):
                raise RuntimeError("Original PIN memory contract did not recover after isolated mutation")
            result["mutation"] = {"status": "DETECTED",
                "change": "Real UnlockPuk PIN_ALTER hook changed to PIN_ENABLE in an external source copy",
                "source_copy_sha256": sha(mutant), "cases": mutant_cases,
                "restored_counts": {"PASS": len(restored), "FAIL": 0},
                "tracked_production_untouched": sha(manager) == next(
                    item["sha256"] for item in result["inputs"] if item["path"] == PRODUCTION[0])}
        for item in result["inputs"]:
            if sha(CORE / item["path"]) != item["sha256"]:
                raise RuntimeError("Input changed during run: " + item["path"])
        result["binary_sha256"] = sha(binary)
        result["xml_sha256"] = sha(xml)
        result["status"] = "PASS" if not result["counts"]["FAIL"] else "FAIL"
    except Exception as error:
        result["diagnostic"] = str(error)
        if result["executed"]:
            result["status"] = "FAIL"
            result["counts"]["FAIL"] += 1
            result["counts"]["BLOCKED"] = 0
    finally:
        save()
    print(json.dumps({key: result[key] for key in ["status", "executed", "counts"]}))
    print(output / "result.json")
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
