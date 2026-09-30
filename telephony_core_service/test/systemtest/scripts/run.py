#!/usr/bin/env python3
"""Build/execute one controlled component slice; preparation failures are BLOCKED."""
from build_clock import await_build_timestamps
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import signal
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET

SYSTEM = Path(__file__).resolve().parents[1]
CORE = SYSTEM.parents[1]
CASE = "SIM-COMP-001"

def tree_digest(root):
    digest = hashlib.sha256()
    for item in sorted(root.rglob("*")):
        if item.is_file():
            digest.update(str(item.relative_to(root)).encode() + b"\0" + item.read_bytes() + b"\0")
    return digest.hexdigest()

parser = argparse.ArgumentParser()
parser.add_argument("--gtest-root", required=True)
parser.add_argument("--output", required=True)
args = parser.parse_args()
output = Path(args.output).resolve()
if output == SYSTEM or SYSTEM in output.parents:
    raise SystemExit("Build and run products must be outside systemtest sources")
output.mkdir(parents=True, exist_ok=True)
# Remove only known generated XML: a failed preparation must not expose stale PASS.
for artifact in ["normal.xml", "negative.xml", "adapter.xml"]:
    (output / artifact).unlink(missing_ok=True)
result = {"schema_version": 1, "case_id": CASE, "level": "B", "repository": "zjuluowu/multisim", "status": "BLOCKED", "executed": False, "oracle_ids": ["O-C11", "O-H01", "O-H02", "O-L06"], "toolchain": {}, "environment": {"os": platform.platform(), "target_os": "linux", "cpu": platform.machine()}, "topology": {"configured_slots": 1, "cards": "NOT_ESTABLISHED", "ril": "NOT_CONNECTED"}, "stub_config_version": "capacity-gmock-parameter-v2", "permission_profile": "NOT_COVERED", "blocking_stage": "dependency_preparation", "blocked_original_cases": ["SIM-B-001", "SIM-B-002", "SIM-B-003", "SIM-B-004", "SIM-B-005"], "commands": [], "counts": {"PASS": 0, "FAIL": 0, "SKIP": 0, "BLOCKED": 1}, "safety": {"synthetic_only": True, "identity_plaintext_emitted": False}, "service_entry": "BLOCKED_NOT_EXECUTED", "cleanup": {"completed": False}, "restore": "No platform or persistent card state modified"}

def command(argv, name, timeout=120, environment=None):
    started = time.monotonic()
    try:
        process = subprocess.Popen(argv, cwd=output, env=environment, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
        stdout, stderr = process.communicate(timeout=timeout)
        code, text = process.returncode, stdout + stderr
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.communicate()
        code, text = 124, "Timeout: output omitted to keep diagnostics safe\n"
    (output / (name + ".log")).write_text(text)
    result["commands"].append({"argv": argv, "exit_code": code, "elapsed_seconds": time.monotonic() - started, "log": name + ".log"})
    return code, text

namespace = None
try:
    result["head_sha"] = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=CORE, text=True).strip()
    result["dirty"] = bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=CORE, text=True))
    lock = json.loads((SYSTEM / "configs/dependency_lock.json").read_text())
    for header in lock["platform_headers"]:
        actual = hashlib.sha256((SYSTEM / header["path"]).read_bytes()).hexdigest()
        if actual != header["sha256"]:
            raise RuntimeError("Platform header differs from pinned source: " + header["path"])
    result["external_contract_manifest_digest"] = hashlib.sha256(
        (SYSTEM / "configs/upstream_contracts.json").read_bytes()).hexdigest()
    gtest = Path(args.gtest_root).resolve()
    if tree_digest(gtest) != lock["googletest"]["tree_sha256"]:
        raise RuntimeError("GoogleTest source tree differs from the pinned archive")
    result["toolchain"]["googletest"] = lock["googletest"]["commit"]
    result["toolchain"]["googlemock"] = lock["googletest"]["commit"]
    for tool in ["gn", "ninja", "clang++", "ld.gold", "llvm-profdata-18", "llvm-cov-18"]:
        code, text = command([tool, "--version"], "version-" + tool)
        if code: raise RuntimeError("Required tool unavailable: " + tool)
        result["toolchain"][tool] = text.splitlines()[0]
    source_paths = ["services/sim/src/sim_manager.cpp", "services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp", "interfaces/innerkits/include/i_sim_manager.h", "services/sim/include/sim_manager.h", "interfaces/innerkits/include/telephony_types.h"]
    result["production_sources"] = [{"path": name, "sha256": hashlib.sha256((CORE/name).read_bytes()).hexdigest()} for name in source_paths]
    result["source_manifest_digest"] = hashlib.sha256((SYSTEM / "configs/source_manifest.json").read_bytes()).hexdigest()
    result["harness_digest"] = tree_digest(SYSTEM)
    result["scenario_digest"] = hashlib.sha256(json.dumps({"case": CASE, "topology": result["topology"], "stub": result["stub_config_version"], "permission": result["permission_profile"]}, sort_keys=True).encode()).hexdigest()
    gn_args = "production_root=" + json.dumps(str(CORE)) + " gtest_root=" + json.dumps(str(gtest))
    code, _ = command(["python3", str(SYSTEM / "scripts/generate_ril_mock.py"), "--check"], "ril-interface-check")
    if code: raise RuntimeError("RIL mock differs from the current production interface")
    result["build_clock"] = await_build_timestamps(SYSTEM)
    result["blocking_stage"] = "gn_generation"
    code, _ = command(["gn", "gen", str(output), "--root=" + str(SYSTEM), "--args=" + gn_args], "gn-gen")
    if code: raise RuntimeError("GN generation failed")
    result["blocking_stage"] = "product_link"
    code, _ = command(["ninja", "-C", str(output), "sim_component_test", "platform_adapter_test"], "ninja-build")
    result["build_exit_code"] = code
    result["production_target"] = "sim_production_host"
    if code: raise RuntimeError("Host product/test link failed; no GoogleTest executed")
    result["blocking_stage"] = "runtime"
    result["binary_sha256"] = hashlib.sha256((output / "sim_component_test").read_bytes()).hexdigest()
    namespace = Path(tempfile.mkdtemp(prefix="capacity-", dir=output))
    env = os.environ.copy()
    env.pop("SIM_HOST_NEGATIVE_CONTROL", None)
    env["LLVM_PROFILE_FILE"] = str(output / "normal.profraw")
    normal_xml = output / "normal.xml"
    argv = [str(output / "sim_component_test"), "--gtest_filter=SimComponent.CapacityReadOnly", "--gtest_output=xml:" + str(normal_xml)]
    code, _ = command(argv, "normal", timeout=10, environment=env)
    result["executed"] = True
    result["run_exit_code"] = code
    root = ET.parse(normal_xml).getroot()
    cases = root.findall(".//testcase")
    properties = {node.attrib["name"]: node.attrib["value"] for node in root.findall(".//property")}
    result["gtest_counts"] = {"tests": len(cases), "failures": len(root.findall(".//failure")), "skipped": len(root.findall(".//skipped"))}
    result["observations"] = properties
    result["convergence"] = {"condition_id": "public_capacity_stable", "budget_seconds": 1, "converged": properties.get("converged") == "true", "timed_out": code == 124}
    result["status"] = "PASS" if code == 0 and len(cases) == 1 and not root.findall(".//failure") else "FAIL"
    result["counts"] = {"PASS": int(result["status"] == "PASS"), "FAIL": int(result["status"] == "FAIL"), "SKIP": 0, "BLOCKED": 0}
    if result["status"] != "PASS": raise RuntimeError("Normal scenario did not pass")
    env["SIM_HOST_NEGATIVE_CONTROL"] = "1"
    env["LLVM_PROFILE_FILE"] = str(output / "negative.profraw")
    code, _ = command([str(output / "sim_component_test"), "--gtest_filter=SimComponent.CapacityReadOnly", "--gtest_output=xml:" + str(output / "negative.xml")], "negative", timeout=10, environment=env)
    negative_root = ET.parse(output / "negative.xml").getroot()
    expected = code == 1 and any("O-C11" in (node.text or "") for node in negative_root.findall(".//failure"))
    result["negative_control"] = {"status": "FAIL", "exit_code": code, "expected_failure_verified": expected, "gtest_tests": len(negative_root.findall(".//testcase")), "gtest_failures": len(negative_root.findall(".//failure")), "kind": "observer_evidence_corruption_not_production_fault"}
    if not expected: raise RuntimeError("Negative control did not detect confirmed Oracle violation")
    code, _ = command(["llvm-profdata-18", "merge", "-sparse", str(output / "normal.profraw"), "-o", str(output / "normal.profdata")], "coverage-merge")
    if code: raise RuntimeError("Production execution audit failed")
    code, text = command(["llvm-cov-18", "export", str(output / "sim_component_test"), "-instr-profile=" + str(output / "normal.profdata")], "coverage-export")
    if code: raise RuntimeError("Cannot export production execution evidence")
    data = json.loads(text)
    functions = [item for section in data["data"] for item in section["functions"] if "SimManager" in item["name"] and "GetMaxSimCount" in item["name"]]
    result["production_execution_audit"] = {"coverage_functions": [{"name": f["name"], "executions": f["count"], "filenames": f["filenames"]} for f in functions], "link_map": "sim_component_test.map", "purpose": "participation_audit_not_business_call_count_Oracle"}
    if not any(item["count"] > 0 for item in functions): raise RuntimeError("No evidence that production capacity method executed")
    adapter_env = os.environ.copy()
    adapter_env.pop("SIM_HOST_NEGATIVE_CONTROL", None)
    code, _ = command([str(output / "platform_adapter_test"), "--gtest_output=xml:" + str(output / "adapter.xml")], "event-adapter", timeout=10, environment=adapter_env)
    adapter_root = ET.parse(output / "adapter.xml").getroot()
    adapter_cases = adapter_root.findall(".//testcase")
    result["infrastructure_checks"] = {
        "level": "INFRA_NOT_SIM_BUSINESS", "exit_code": code,
        "tests": len(adapter_cases), "failures": len(adapter_root.findall(".//failure")),
        "skipped": len(adapter_root.findall(".//skipped")), "xml": "adapter.xml",
        "cases": [{"name": item.attrib["name"], "properties": {node.attrib["name"]: node.attrib["value"] for node in item.findall(".//property")}} for item in adapter_cases],
    }
    if code or len(adapter_cases) != 9 or adapter_root.findall(".//failure") or adapter_root.findall(".//skipped"):
        raise RuntimeError("Platform adapter self-check failed; block SIM scenario expansion")
    result["blocking_stage"] = None
    result["reason"] = "Controlled component capacity slice ran; all service/SIM-state/account scenarios remain blocked"
except Exception as error:
    result["reason"] = str(error)
    if result["executed"]:
        result["status"] = "FAIL"
        result["counts"] = {"PASS": 0, "FAIL": 1, "SKIP": 0, "BLOCKED": 0}
finally:
    try:
        if namespace is not None:
            shutil.rmtree(namespace)
            if namespace.exists(): raise RuntimeError("Temporary namespace remains")
        result["cleanup"] = {"completed": True, "temporary_namespace_removed": namespace is not None, "processes_reaped": True}
    except Exception as error:
        result["status"] = "FAIL"
        result["reason"] = "Cleanup failed: " + str(error)
        result["cleanup"] = {"completed": False, "block_following_cases": True}
    result["blocked_scenarios"] = [
        {"case_id": case, "level": "B", "status": "BLOCKED", "executed": False,
         "reason": "Service entry capability/provider unresolved" if case == "SIM-B-001" else "SIM production state/account initialization closure and controlled runtime incomplete"}
        for case in result["blocked_original_cases"]
    ]
    result["blocked_scenarios"].append({"case_id": "SIM-CHAR-001", "level": "C", "status": "BLOCKED", "executed": False, "reason": "Public preinitialization characterization cannot link real state/account methods"})
    result["counts_scope"] = "SIM-COMP-001 normal execution only; blocked designs and infrastructure checks are reported separately"
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps({"status": result["status"], "executed": result["executed"], "reason": result.get("reason"), "result": str(output / "result.json")}))
raise SystemExit(0 if result["status"] == "PASS" else 1)
