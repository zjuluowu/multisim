#!/usr/bin/env python3
"""Compare only the implemented capacity slice; Legacy values require review."""
import argparse
import json
from pathlib import Path
parser = argparse.ArgumentParser()
parser.add_argument("--legacy", required=True)
parser.add_argument("--split", required=True)
parser.add_argument("--output", required=True)
args = parser.parse_args()
a = json.loads(Path(args.legacy).read_text())
b = json.loads(Path(args.split).read_text())
result = {"scope": "SIM-COMP-001_capacity_only", "legacy_sha": a.get("head_sha"), "split_sha": b.get("head_sha"), "review_items": [], "status": "COMPARABLE"}
for field in ["case_id", "level", "scenario_digest", "stub_config_version", "permission_profile", "external_contract_manifest_digest", "toolchain", "environment"]:
    if a.get(field) != b.get(field):
        result["review_items"].append({"context_changed": field})
        result["status"] = "INCOMPARABLE"
if a.get("status") != "PASS" or b.get("status") != "PASS":
    result["status"] = "BLOCKED"
if result["status"] == "COMPARABLE":
    x = a.get("observations", {}).get("legacy_capacity")
    y = b.get("observations", {}).get("legacy_capacity")
    if x != y:
        result["review_items"].append({"oracle_id": "O-L06", "classification": "LEGACY_BEHAVIOR", "legacy_capacity": x, "split_capacity": y, "action": "MANUAL_REVIEW_NOT_AUTOMATIC_FAILURE"})
    result["confirmed_contract_result"] = "Both normal GTest runs passed O-C11; no exact-value contract assumed"
output = Path(args.output).resolve()
system = Path(__file__).resolve().parents[1]
if output == system or system in output.parents:
    raise SystemExit("Diff output must be outside systemtest sources")
output.write_text(json.dumps(result, indent=2) + "\n")
print(result["status"])
raise SystemExit(0 if result["status"] == "COMPARABLE" else 2)
