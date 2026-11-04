#!/usr/bin/env python3
"""Build real SIM contract host, execute suites and preserve auditable results outside the repository."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

GUARD = Path(__file__).resolve().parent
CORE = GUARD.parents[1]
REPO = CORE.parent
SYSTEM = CORE / "test/systemtest"
sys.path.insert(0, str(SYSTEM / "scripts"))
from build_clock import await_build_timestamps
from run_g2a_query import digest_tree

PRODUCTION = [
    "services/sim/src/sim_state_manager.cpp", "services/sim/src/sim_state_handle.cpp",
    "services/sim/src/icc_state.cpp", "services/tel_ril/src/observer_handler.cpp",
    "frameworks/native/src/core_manager_inner.cpp",
    "services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp",
    "utils/common/src/tel_event_handler.cpp", "utils/common/src/tel_event_queue.cpp",
]
REQUIRED = [
    ("SimStateManager", "Init", PRODUCTION[0]),
    ("SimStateManager", "GetSimState", PRODUCTION[0]),
    ("SimStateManager", "HasSimCard", PRODUCTION[0]),
    ("SimStateManager", "UnlockPin", PRODUCTION[0]),
    ("SimStateHandle", "ProcessEvent", PRODUCTION[1]),
    ("SimStateHandle", "GetSimCardData", PRODUCTION[1]),
    ("SimStateHandle", "ProcessIccCardState", PRODUCTION[1]),
    ("SimStateHandle", "SimStateEscape", PRODUCTION[1]),
]
FILTERS = {"compat": "*-*DEFECT_*", "defect": "*DEFECT_*"}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check_semantics():
    baseline = json.loads((GUARD / "semantic_baseline.json").read_text())
    for name, expected in baseline["sha256"].items():
        if sha(GUARD / name) != expected:
            raise RuntimeError("Compatibility asset drift requires explicit contract review: " + name)
    # Independently compilable common code may only include these project headers.
    allowed = set(baseline["sha256"])
    for name in allowed:
        for line in (GUARD / name).read_text().splitlines():
            if line.startswith('#include "') and line.split('"')[1] not in allowed:
                raise RuntimeError("Implementation dependency escaped adapter: " + name + ": " + line)
    return baseline


def xml_cases(path):
    tree = ET.parse(path).getroot()
    cases = []
    for case in tree.findall(".//testcase"):
        if case.find("skipped") is not None or case.get("status") == "notrun":
            raise RuntimeError("Skipped/not-run cases cannot pass the guard")
        cases.append({"test": case.get("classname") + "." + case.get("name"),
                      "status": "FAIL" if case.find("failure") is not None else "PASS",
                      "failures": [f.get("message", "") for f in case.findall("failure")],
                      "observations": {p.get("name"): p.get("value") for p in case.findall("./properties/property")}})
    if not cases:
        raise RuntimeError("No actual GoogleTest cases executed")
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sut", choices=["legacy", "multisim"], default="legacy")
    parser.add_argument("--suite", choices=["compat", "defect", "target"], default="compat")
    parser.add_argument("--product", choices=["host_dual_psim", "nezha_4c3s"], default="host_dual_psim")
    parser.add_argument("--gtest-root")
    parser.add_argument("--output", required=True)
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--mutation-check", action="store_true")
    args = parser.parse_args()
    output = Path(args.output).resolve()
    if output == REPO or REPO in output.parents:
        parser.error("Output must be outside the Git repository")
    if not 1 <= args.repeat <= 100:
        parser.error("repeat must be in 1..100")
    output.mkdir(parents=True, exist_ok=True)
    # Each invocation uses its own artifacts, so interruption/retry never reuses an old PASS.
    import tempfile
    session = Path(tempfile.mkdtemp(prefix="execution-", dir=output))
    result_path = output / "result.json"
    result = {"schema_version": 1, "status": "BLOCKED", "executed": False,
              "sut": args.sut, "suite": args.suite, "product": args.product,
              "counts": {"PASS": 0, "FAIL": 0, "BLOCKED": 1}, "runs": [], "commands": [],
              "artifacts": str(session), "scope": "real_SIM_card_state_component_with_RIL_boundary",
              "full_manager_on_init": "NOT_RUN", "IPC_SA_device": "NOT_RUN",
              "account_default_roles_persistence_esim": "NOT_RUN"}

    def save():
        result_path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")

    def command(argv, label, timeout=180, env=None, allow_failure=False):
        start = time.monotonic()
        with (session / (label + ".log")).open("w") as log:
            process = subprocess.Popen(argv, cwd=session, stdout=log, stderr=subprocess.STDOUT,
                                       env=env, start_new_session=True)
            try:
                code = process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                code = 124
        result["commands"].append({"argv": argv, "exit_code": code,
                                   "seconds": time.monotonic() - start, "log": label + ".log"})
        save()
        if code and not allow_failure:
            raise RuntimeError(label + " failed (" + str(code) + "); see " + str(session / (label + ".log")))
        return code

    def build(directory, mutation=None):
        options = "production_root=" + json.dumps(str(CORE)) + " gtest_root=" + json.dumps(str(gtest))
        if mutation:
            options += " sim_guard_state_source=" + json.dumps(str(mutation))
        label = "mutant" if mutation else "normal"
        command(["gn", "gen", str(directory), "--root=" + str(SYSTEM), "--args=" + options], label + "-gn")
        command(["ninja", "-C", str(directory), "-j", "3", "sim_guard_legacy"], label + "-build", timeout=360)

    def execute(binary, label, test_filter):
        xml = session / (label + ".xml")
        profile = session / (label + ".profraw")
        env = os.environ.copy()
        env["LLVM_PROFILE_FILE"] = str(profile)
        # Vendor hooks must not be loaded from an ambient product library.
        env["LD_LIBRARY_PATH"] = str(session)
        result["executed"] = True
        code = command([str(binary), "--gtest_filter=" + test_filter, "--gtest_shuffle", "--gtest_random_seed=41",
                        "--gtest_output=xml:" + str(xml)], label, timeout=45, env=env, allow_failure=True)
        if code not in (0, 1):
            raise RuntimeError(label + " crashed/timed out with exit " + str(code))
        cases = xml_cases(xml)
        if (code == 0) != all(c["status"] == "PASS" for c in cases):
            raise RuntimeError("GoogleTest exit/XML disagree")
        return {"label": label, "exit_code": code, "xml": xml.name, "profile": str(profile), "cases": cases}

    save()
    try:
        result["source_commit"] = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=CORE, text=True).strip()
        result["dirty"] = bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=CORE, text=True))
        result["semantic_baseline"] = check_semantics()
        if args.sut == "multisim":
            raise RuntimeError("Real multisim card-state business has not migrated; adapter binding is BLOCKED, not PASS")
        if args.suite == "target" or args.product != "host_dual_psim":
            raise RuntimeError("Target/nezha business and product policy not yet confirmed or bound to real SUT")
        if not args.gtest_root:
            raise RuntimeError("--gtest-root must identify the pinned verified dependency")
        if args.mutation_check and args.suite != "compat":
            raise RuntimeError("Mutation check requires the compatibility suite")
        gtest = Path(args.gtest_root).resolve()
        lock = json.loads((SYSTEM / "configs/dependency_lock.json").read_text())
        if digest_tree(gtest) != lock["googletest"]["tree_sha256"]:
            raise RuntimeError("GoogleTest tree differs from the pinned source")
        for header in lock["platform_headers"]:
            if sha(SYSTEM / header["path"]) != header["sha256"]:
                raise RuntimeError("Pinned platform header differs: " + header["path"])
        result["dependency_commit"] = lock["googletest"]["commit"]
        result["production_sources"] = [{"path": p, "sha256": sha(CORE / p)} for p in PRODUCTION]
        result["validation_inputs"] = [{"path": str(p.relative_to(CORE)), "sha256": sha(p)}
                                       for p in sorted(GUARD.rglob("*")) if p.is_file() and p.suffix in (".h", ".cpp", ".py")]
        host_inputs = [SYSTEM / ".gn", SYSTEM / "BUILD.gn", SYSTEM / "gn/BUILD.gn",
                       SYSTEM / "gn/BUILDCONFIG.gn", SYSTEM / "configs/dependency_lock.json",
                       SYSTEM / "scripts/build_clock.py", SYSTEM / "scripts/generate_ril_mock.py",
                       SYSTEM / "scripts/run_g2a_query.py", GUARD / "semantic_baseline.json",
                       SYSTEM / "tests/event_transport_test.cpp", GUARD / "traceability/inventory.json"]
        host_inputs += [p for p in (SYSTEM / "stubs").rglob("*") if p.is_file() and p.suffix in (".h", ".cpp")]
        result["validation_inputs"] += [{"path": str(p.relative_to(CORE)), "sha256": sha(p)} for p in sorted(host_inputs)]
        headers = [p for parent in ["interfaces", "services", "utils", "common", "frameworks/native"]
                   for p in (CORE / parent).rglob("*.h")]
        result["production_header_digest"] = hashlib.sha256(json.dumps(
            [(str(p.relative_to(CORE)), sha(p)) for p in sorted(headers)]).encode()).hexdigest()
        for tool in ["gn", "ninja", "clang++", "ld.gold", "llvm-profdata-18", "llvm-cov-18"]:
            command([tool, "--version"], "version-" + tool)
        command([sys.executable, str(SYSTEM / "scripts/generate_ril_mock.py"), "--check"], "ril-interface")
        result["build_clock"] = await_build_timestamps(SYSTEM)
        normal = session / "build"
        build(normal)
        binary = normal / "sim_guard_legacy"
        for repeat in range(args.repeat):
            run = execute(binary, "run-" + str(repeat), FILTERS[args.suite])
            expected = 7 if args.suite == "compat" else 1
            if len(run["cases"]) != expected:
                raise RuntimeError("Unexpected case count: " + str(len(run["cases"])))
            result["runs"].append(run)
            save()
        if args.suite == "compat":
            profiles = [r["profile"] for r in result["runs"]]
            profdata = session / "normal.profdata"
            command(["llvm-profdata-18", "merge", "-sparse", *profiles, "-o", str(profdata)], "merge")
            command(["llvm-cov-18", "export", str(binary), "-instr-profile=" + str(profdata)], "coverage")
            coverage = json.loads((session / "coverage.log").read_text())
            functions = [f for d in coverage["data"] for f in d["functions"]]
            audits = []
            for owner, symbol, source in REQUIRED:
                token = str(len(owner)) + owner + str(len(symbol)) + symbol
                matches = [f for f in functions if token in f["name"] and f["count"] > 0
                           and any(Path(n).resolve() == (CORE / source).resolve() for n in f["filenames"])]
                if not matches:
                    raise RuntimeError("Real production execution not proven: " + owner + "::" + symbol)
                audits.append({"symbol": owner + "::" + symbol, "source": source,
                               "executions": sum(f["count"] for f in matches)})
            result["production_execution"] = audits
        if args.mutation_check:
            source = CORE / PRODUCTION[1]
            text = source.read_text()
            needle = "case ICC_CONTENT_READY:\n            externalState_ = SimState::SIM_STATE_READY;"
            if text.count(needle) != 1:
                raise RuntimeError("Controlled mutation anchor no longer unique")
            mutant = session / "sim_state_handle_mutant.cpp"
            mutant.write_text(text.replace(needle, needle.replace("SIM_STATE_READY;", "SIM_STATE_LOCKED;")))
            mutated_build = session / "mutant-build"
            build(mutated_build, mutant)
            failed = execute(mutated_build / "sim_guard_legacy", "mutation", "*LIFE_001")
            if len(failed["cases"]) != 1 or failed["cases"][0]["status"] != "FAIL":
                raise RuntimeError("Oracle did not detect the real production READY mapping fault")
            restored = execute(binary, "restored", FILTERS["compat"])
            if any(c["status"] != "PASS" for c in restored["cases"]) or sha(source) != result["production_sources"][1]["sha256"]:
                raise RuntimeError("Restored production regression or source changed")
            result["mutation"] = {"status": "DETECTED", "change": "production READY mapping returns LOCKED",
                                  "source_copy_sha256": sha(mutant), "failure": failed, "restored": restored,
                                  "tracked_production_untouched": True}
        cases = [c for r in result["runs"] for c in r["cases"]]
        for item in result["production_sources"] + result["validation_inputs"]:
            if sha(CORE / item["path"]) != item["sha256"]:
                raise RuntimeError("Input changed during build/execution: " + item["path"])
        header_digest = hashlib.sha256(json.dumps(
            [(str(p.relative_to(CORE)), sha(p)) for p in sorted(headers)]).encode()).hexdigest()
        if header_digest != result["production_header_digest"]:
            raise RuntimeError("Production headers changed during build/execution")
        result["counts"] = {"PASS": sum(c["status"] == "PASS" for c in cases),
                            "FAIL": sum(c["status"] == "FAIL" for c in cases), "BLOCKED": 0}
        result["status"] = "FAIL" if result["counts"]["FAIL"] else "PASS"
        result["binary_sha256"] = sha(binary)
    except Exception as error:
        result["diagnostic"] = str(error)
        if result["executed"]:
            result["status"] = "FAIL"
            result["counts"]["FAIL"] += 1
            result["counts"]["BLOCKED"] = 0
    finally:
        save()
    print(json.dumps({k: result[k] for k in ("status", "executed", "counts")}, ensure_ascii=False))
    print(str(result_path))
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
