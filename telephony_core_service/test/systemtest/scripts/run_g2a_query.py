#!/usr/bin/env python3
"""Build and audit the G2a real read-only attachment, independently of capacity and OnInit suites."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import time
import xml.etree.ElementTree as ET

from build_clock import await_build_timestamps

SYSTEM = Path(__file__).resolve().parents[1]
CORE = SYSTEM.parents[1]
SCENARIOS = [
    ("single", "1", None),
    ("dual", "2", None),
    ("zero", "0", None),
    ("negative_atoi", "-2", None),
    ("malformed_atoi", "invalid", None),
    ("vendor_override", "2", "5"),
    ("vendor_result_propagation", "2", "-31"),
]
SOURCES = [
    "services/core/src/core_service_sim.cpp",
    "services/core/src/legacy_sim_query_binding.cpp",
    "services/multisim/application/query/real_sim_count_query.cpp",
    "services/sim/src/sim_manager.cpp",
    "services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp",
]
REQUIRED_EXECUTIONS = [
    ("CoreServiceSim", "GetRealSimCount", "services/core/src/core_service_sim.cpp"),
    ("CoreServiceSim", "AttachReadOnlyQueries", "services/core/src/core_service_sim.cpp"),
    ("LegacySimQueryBinding", "ReadLegacyRealSimCount", "services/core/src/legacy_sim_query_binding.cpp"),
    ("RealSimCountQuery", "Read", "services/multisim/application/query/real_sim_count_query.cpp"),
    ("SimManager", "GetRealSimCount", "services/sim/src/sim_manager.cpp"),
    ("TelephonyExtWrapper", "GetRealSimCountExt", "services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp"),
]


def digest_tree(root):
    digest = hashlib.sha256()
    for path in sorted(root.rglob("*")):
        if path.is_file():
            digest.update(str(path.relative_to(root)).encode() + b"\0" + path.read_bytes() + b"\0")
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gtest-root", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--libxml2-include", default=os.environ.get("SIM_LIBXML2_INCLUDE", "/usr/include/libxml2"),
                        help="libxml2 headers directory; defaults to SIM_LIBXML2_INCLUDE or the standard Linux location")
    args = parser.parse_args()
    output = Path(args.output).resolve()
    if output == CORE or CORE in output.parents:
        raise SystemExit("G2a build/run output must be outside repository sources")
    output.mkdir(parents=True, exist_ok=True)
    result = {
        "schema_version": 1,
        "case_id": "SIM-G2A-READ-001",
        "status": "BLOCKED",
        "executed": False,
        "scope": "real_CoreServiceSim_to_original_SimManager_readonly_attachment",
        "manager_on_init_executed": False,
        "core_init_executed": False,
        "card_presence": "not_observed_or_required_for_this_query",
        "device_runtime": "not_connected",
        "original_capacity_suite": "not_run_or_counted_here",
        "full_sim_initialization": "not_claimed",
        "production_sources": [],
        "commands": [],
        "scenarios": [],
        "counts": {"PASS": 0, "FAIL": 0, "BLOCKED": 1},
        "environment": {"os": platform.platform(), "cpu": platform.machine()},
        "limits": ["No CoreService::Init, Manager::OnInit, IPC/SA/permission or device validation",
                   "LTO intentionally closes only the selected production query",
                   "RIL is a strict platform Mock; vendor override is an explicit ABI boundary"],
    }

    # Clear only this runner's known execution artifacts before any fallible preparation.
    # A failed or interrupted retry must not leave an earlier PASS XML/JSON as current evidence.
    for name, _, _ in SCENARIOS:
        for extension in [".xml", ".profraw"]:
            (output / ("g2a-" + name + extension)).unlink(missing_ok=True)
    for artifact in ["g2a-query.profdata", "coverage-export.log"]:
        (output / artifact).unlink(missing_ok=True)
    (output / "g2a-query-result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")

    def command(argv, name, timeout=120, environment=None, runtime=False):
        start = time.monotonic()
        process = subprocess.Popen(argv, cwd=output, env=environment, text=True,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
        if runtime:
            result["executed"] = True
        try:
            stdout, stderr = process.communicate(timeout=timeout)
            code = process.returncode
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            stdout, stderr = process.communicate()
            code = 124
        text = stdout + stderr
        (output / (name + ".log")).write_text(text)
        result["commands"].append({"argv": argv, "exit_code": code,
                                   "elapsed_seconds": time.monotonic() - start, "log": name + ".log"})
        if code:
            raise RuntimeError(name + " failed; see " + str(output / (name + ".log")))
        return stdout

    try:
        result["head_sha"] = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=CORE, text=True).strip()
        result["dirty"] = bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=CORE, text=True))
        lock = json.loads((SYSTEM / "configs/dependency_lock.json").read_text())
        gtest = Path(args.gtest_root).resolve()
        if digest_tree(gtest) != lock["googletest"]["tree_sha256"]:
            raise RuntimeError("GoogleTest tree differs from pinned dependency")
        for header in lock["platform_headers"]:
            if hashlib.sha256((SYSTEM / header["path"]).read_bytes()).hexdigest() != header["sha256"]:
                raise RuntimeError("Pinned platform header changed: " + header["path"])
        result["googletest_commit"] = lock["googletest"]["commit"]
        result["production_sources"] = [{"path": path, "sha256": hashlib.sha256((CORE / path).read_bytes()).hexdigest()}
                                        for path in SOURCES]
        result["toolchain"] = {}
        for tool in ["gn", "ninja", "clang++", "ld.gold", "llvm-profdata-18", "llvm-cov-18"]:
            result["toolchain"][tool] = command([tool, "--version"], "version-" + tool).splitlines()[0]
        command(["python3", str(SYSTEM / "scripts/generate_ril_mock.py"), "--check"], "ril-interface-check")
        result["build_clock"] = await_build_timestamps(SYSTEM)
        gn_args = "production_root=" + json.dumps(str(CORE)) + " gtest_root=" + json.dumps(str(gtest))
        gn_args += " libxml2_include_dir=" + json.dumps(str(Path(args.libxml2_include).resolve()))
        result["blocking_stage"] = "gn_generation"
        command(["gn", "gen", str(output), "--root=" + str(SYSTEM), "--args=" + gn_args], "gn-gen")
        result["blocking_stage"] = "query_product_link"
        command(["ninja", "-C", str(output), "-j", "3", "sim_g2a_query_test"], "ninja-build", timeout=300)
        command(["clang++", "-std=c++17", "-fPIC", "-shared",
                 str(SYSTEM / "stubs/g2a_vendor_boundary.cpp"),
                 "-o", str(output / "libtelephony_ext_service.z.so")], "vendor-abi-boundary-build")
        result["blocking_stage"] = "runtime"
        profiles = []
        for name, parameter, vendor in SCENARIOS:
            xml = output / ("g2a-" + name + ".xml")
            profile = output / ("g2a-" + name + ".profraw")
            xml.unlink(missing_ok=True)
            profile.unlink(missing_ok=True)
            environment = os.environ.copy()
            environment.pop("SIM_G2A_VENDOR_RESULT", None)
            environment.pop("SIM_HOST_NEGATIVE_CONTROL", None)
            environment["SIM_G2A_SLOT_PARAMETER"] = parameter
            environment["LD_LIBRARY_PATH"] = str(output)
            environment["LLVM_PROFILE_FILE"] = str(profile)
            if vendor is not None:
                environment["SIM_G2A_VENDOR_RESULT"] = vendor
            command([str(output / "sim_g2a_query_test"),
                     "--gtest_filter=G2aReadOnlyQuery.OriginalManagerAttachment",
                     "--gtest_output=xml:" + str(xml)], "run-" + name, timeout=15, environment=environment, runtime=True)
            tree = ET.parse(xml).getroot()
            cases = tree.findall(".//testcase")
            if len(cases) != 1 or tree.findall(".//failure") or tree.findall(".//skipped"):
                raise RuntimeError("Unexpected GoogleTest scenario result: " + name)
            properties = {x.attrib["name"]: x.attrib["value"] for x in tree.findall(".//property")}
            result["scenarios"].append({"name": name, "status": "PASS", "xml": xml.name,
                                        "slot_parameter": parameter, "vendor_result": vendor,
                                        "properties": properties})
            profiles.append(str(profile))
        result["blocking_stage"] = "production_execution_audit"
        profdata = output / "g2a-query.profdata"
        command(["llvm-profdata-18", "merge", "-sparse", *profiles, "-o", str(profdata)], "coverage-merge")
        coverage_text = command(["llvm-cov-18", "export", str(output / "sim_g2a_query_test"),
                                 "-instr-profile=" + str(profdata)], "coverage-export")
        coverage = json.loads(coverage_text)
        functions = [function for section in coverage["data"] for function in section["functions"]]
        audits = []
        for owner, symbol, source in REQUIRED_EXECUTIONS:
            expected_source = (CORE / source).resolve()
            matches = [function for function in functions
                       if owner in function["name"] and symbol in function["name"] and
                       any(Path(filename).resolve() == expected_source for filename in function["filenames"])]
            if not any(function["count"] > 0 for function in matches):
                raise RuntimeError("No actual production execution evidence: " + owner + "::" + symbol)
            audits.extend({"name": item["name"], "executions": item["count"], "filenames": item["filenames"]}
                          for item in matches)
        boot = [item for item in functions if ("SimManager" in item["name"] and "OnInit" in item["name"])]
        if any(item["count"] > 0 for item in boot):
            raise RuntimeError("Selected query unexpectedly executed SimManager::OnInit")
        link_map = output / "sim_g2a_query_test.map"
        if not link_map.exists() or not link_map.stat().st_size:
            raise RuntimeError("Missing query product link map")
        result["production_execution_audit"] = {"functions": audits, "link_map": link_map.name,
                                                 "purpose": "actual production participation, not an exact count Oracle",
                                                 "lto_scope": "selected query only; uncalled platform paths eliminated"}
        result["status"] = "PASS"
        result["counts"] = {"PASS": len(result["scenarios"]), "FAIL": 0, "BLOCKED": 0}
        result["blocking_stage"] = None
        result["binary_sha256"] = hashlib.sha256((output / "sim_g2a_query_test").read_bytes()).hexdigest()
    except Exception as error:
        result["diagnostic"] = str(error)
        if result["executed"]:
            result["status"] = "FAIL"
            result["counts"] = {"PASS": len(result["scenarios"]), "FAIL": 1, "BLOCKED": 0}
    finally:
        (output / "g2a-query-result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({"status": result["status"], "executed": result["executed"], "counts": result["counts"],
                      "result": str(output / "g2a-query-result.json")}, ensure_ascii=False))
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
