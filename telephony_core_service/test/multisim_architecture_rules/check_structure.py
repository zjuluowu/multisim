#!/usr/bin/env python3
"""Check the closed multisim skeleton's textual includes and GN source boundaries.

This is deliberately a source/target check, not a C++ semantic or production
write-ownership proof. All conditional includes are checked conservatively.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "services/multisim"
SUFFIXES = {".h", ".hpp", ".cpp", ".cc", ".cxx"}
CONTRACTS = {"topology_contracts", "assignment_contracts", "runtime_contracts"}
TYPES = {"topology_types", "assignment_types"}
VALUES = {"sim_values", "request_protocol"}

# Explicit semantic boundaries: physical groups do not define layers.
SOURCE_GROUPS = {'readonly_query': ('integration/api/real_sim_count_query.cpp', 'integration/api/real_sim_count_query.h'),
 'assembly_contracts': ('integration/lifecycle/host_ports.h',
                        'integration/lifecycle/manager_publication.h',
                        'integration/lifecycle/sim_module_factory.h'),
 'assembly_core': ('integration/lifecycle/sim_module.h', 'integration/lifecycle/sim_module_factory.cpp'),
 'assignment_adapters': ('card_selection/assignment_output_adapter.h',
                         'card_selection/assignment_store_adapter.h'),
 'assignment_cases': ('card_selection/assignment_service.cpp', 'card_selection/assignment_service.h'),
 'assignment_contracts': ('card_selection/assignment_requests.h',
                          'card_selection/assignment_store.h',
                          'card_selection/legacy_entry.h',
                          'card_selection/primary/primary_topology_port.h',
                          'card_selection/profile_preference_port.h',
                          'card_selection/primary/radio_execution_port.h'),
 'assignment_domain': ('card_selection/assignment_aggregate.cpp',
                       'card_selection/assignment_aggregate.h',
                       'card_selection/role_selection.h'),
 'assignment_protocols': ('card_selection/account_role_protocol.h',
                          'card_selection/primary/primary_switch_plan.cpp',
                          'card_selection/primary/primary_switch_plan.h'),
 'assignment_types': ('card_selection/assignment_types.h', 'card_selection/primary/primary_execution.h'),
 'compatibility_contracts': ('integration/api/legacy_boundary_ports.h',),
 'compatibility_core': ('integration/api/legacy_sim_facade.cpp',
                        'integration/api/legacy_sim_facade.h',
                        'integration/events/completion_cell.h',
                        'integration/events/legacy_completion.cpp',
                        'integration/events/legacy_completion.h',
                        'integration/events/legacy_output_adapter.cpp',
                        'integration/events/legacy_output_adapter.h',
                        'card/esim/legacy_profile_result.h'),
 'repository_exports': ('integration/lifecycle/exports/sim_host_ports.h',
                        'integration/lifecycle/exports/sim_module_factory.h'),
 'request_mechanism': ('support/request/runtime_operation_store.h',),
 'request_protocol': ('support/request/request_context.h',),
 'runtime_cases': ('card/esim/local_card_requests.cpp',),
 'runtime_contracts': ('card/esim/local_card_requests.h',
                       'card/esim/profile_requests.h',
                       'card/records/ef_requests.h',
                       'card/security/pin_requests.h'),
 'runtime_core': ('integration/lifecycle/card_runtime.cpp',
                  'integration/lifecycle/card_runtime.h',
                  'integration/lifecycle/result_sinks.h'),
 'runtime_mechanisms': ('card/records/load_session.cpp',
                        'card/records/load_session.h',
                        'device/resource/resource_barrier.cpp',
                        'device/resource/resource_barrier.h',
                        'card/state/session_registry.cpp',
                        'card/state/session_registry.h'),
 'sim_values': ('support/request_ref.h',
                'support/sim_identity.h',
                'support/slot_catalog.cpp',
                'support/slot_catalog.h'),
 'topology_adapters': ('account/account_store_adapter.cpp',
                       'account/account_store_adapter.h',
                       'account/activation/activation_hook_adapter.h'),
 'topology_cases': ('account/number/set_show_number_use_case.cpp',
                    'account/number/set_show_number_use_case.h',
                    'account/number/show_number_query.cpp',
                    'account/number/show_number_query.h',
                    'account/activation/activate_sim_use_case.cpp',
                    'account/activation/activate_sim_use_case.h',
                    'account/topology_service.cpp',
                    'account/topology_service.h'),
 'topology_contracts': ('account/account_role_planning_port.h',
                        'account/account_store.h',
                        'account/activation/activation_assignment_port.h',
                        'account/activation/activation_runtime_port.h',
                        'account/discovery/card_observation_port.h',
                        'account/legacy_entry.h',
                        'account/number/number_query_host_port.h',
                        'account/number/show_number_completion_port.h',
                        'account/number/show_number_runtime_port.h',
                        'account/topology_requests.h'),
 'topology_domain': ('account/account_registry.cpp',
                     'account/account_registry.h',
                     'account/binding_index.cpp',
                     'account/binding_index.h'),
 'topology_protocols': ('account/number/show_number_plan.cpp',
                        'account/number/show_number_plan.h',
                        'account/activation/activation_progress.cpp',
                        'account/activation/activation_progress.h',
                        'account/discovery/discovery_observation_ledger.cpp',
                        'account/discovery/discovery_observation_ledger.h'),
 'topology_types': ('account/account_types.h',)}


def category(name):
    for target, files in SOURCE_GROUPS.items():
        if name in files:
            return target
    raise ValueError("Source outside the implemented target layout: " + name)


def allowed_edge(user, provider):
    if user == provider:
        return True
    if user in {"sim_values", "readonly_query"}:
        return False
    if user == "request_protocol":
        return provider == "sim_values"
    if user == "request_mechanism":
        return provider in VALUES
    if user in TYPES:
        return provider in TYPES | {"sim_values"}
    if user in CONTRACTS:
        return provider in CONTRACTS | TYPES | VALUES
    if user in {"compatibility_contracts", "assembly_contracts"}:
        return provider in CONTRACTS | TYPES | VALUES | {"compatibility_contracts"}
    if user.endswith("_domain"):
        return provider in {"sim_values", user.replace("_domain", "_types")}
    if user.startswith(("topology_", "assignment_")):
        owner = user.split("_", 1)[0]
        common = CONTRACTS | TYPES | VALUES
        if user.endswith("_adapters"):
            return provider in common
        return provider in common | {"request_mechanism", owner + "_domain", owner + "_protocols"}
    if user in {"runtime_core", "runtime_cases", "runtime_mechanisms"}:
        return provider in CONTRACTS | TYPES | VALUES | {
            "request_mechanism", "runtime_mechanisms", "runtime_cases"}
    if user == "compatibility_core":
        return provider in CONTRACTS | TYPES | VALUES | {"compatibility_contracts"}
    if user == "repository_exports":
        return provider == "assembly_contracts"
    return user == "assembly_core"


def without_comments(text):
    # Keep strings intact; comment-looking bytes inside a literal are not comments.
    return re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/',
                  lambda m: "" if m[0].startswith(("//", "/*")) else m[0], text)


def reject_cycles(graph, kind):
    active, done, chain = set(), set(), []

    def visit(node):
        if node in active:
            raise ValueError(kind + " cycle: " + " -> ".join(chain[chain.index(node):] + [node]))
        if node in done:
            return
        active.add(node)
        chain.append(node)
        for dependency in sorted(graph.get(node, ())):
            visit(dependency)
        chain.pop()
        active.remove(node)
        done.add(node)

    for node in sorted(graph):
        visit(node)


def gn_targets(source):
    # BUILD.gn intentionally uses literal lists, so this small checker stays auditable.
    build = without_comments((source / "BUILD.gn").read_text())
    build = re.sub(r"(?m)#.*$", "", build)
    if re.search(r"\b(?:sources|public|deps|public_deps)\s*[+-]=", build):
        raise ValueError("Unsupported dynamic GN list mutation; keep literal boundary lists")
    result = {}
    blocks = re.findall(r'source_set\("(\w+)"\)\s*\{([^{}]*)\}', build)
    if len(blocks) != len(re.findall(r'\bsource_set\s*\(', build)):
        raise ValueError("Unsupported non-literal GN source set")
    for name, body in blocks:
        lists = {}
        for field in ("sources", "public", "deps", "public_deps"):
            assignments = re.findall(r"\b" + field + r"\s*=", body)
            values = re.findall(r"\b" + field + r"\s*=\s*\[([^]]*)\]", body)
            if len(assignments) > 1 or len(values) != len(assignments):
                raise ValueError("Unsupported non-literal/duplicate GN list: " + name + ": " + field)
            content = values[0] if values else ""
            if re.sub(r'"[^"$]*"|[\s,]', "", content):
                raise ValueError("Unsupported GN list expression: " + name + ": " + field)
            lists[field] = re.findall(r'"([^"]+)"', content)
        if name in result or not re.search(r"\bcheck_includes\s*=\s*true\b", body):
            raise ValueError("Duplicate target or missing check_includes: " + name)
        if not re.search(r"\btestonly\s*=\s*true\b", body):
            raise ValueError("Skeleton targets must remain testonly: " + name)
        result[name] = lists
    if not result:
        raise ValueError("No literal skeleton source sets found")
    groups = re.findall(r'group\("skeleton"\)\s*\{([^{}]*)\}', build)
    dependencies = re.findall(r'\bdeps\s*=\s*\[([^]]*)\]', groups[0]) if len(groups) == 1 else []
    if len(dependencies) != 1 or set(re.findall(r'"([^"]+)"', dependencies[0])) != {":" + t for t in result}:
        raise ValueError("GN skeleton group must cover every source set exactly")
    return result


def check(source=SOURCE):
    files = {p.relative_to(source).as_posix(): p for p in source.rglob("*") if p.suffix in SUFFIXES}
    targets = gn_targets(source)
    membership = Counter()
    target_graph = {}
    for target, data in targets.items():
        for name in data["sources"]:
            if name not in files or category(name) != target:
                raise ValueError(f"GN source misclassified or missing: {target}: {name}")
            membership[name] += 1
        if set(data["public"]) - set(data["sources"]):
            raise ValueError("GN public header outside its source set: " + target)
        target_graph[target] = set()
        for dep in data["deps"] + data["public_deps"]:
            if not dep.startswith(":") or dep[1:] not in targets or not allowed_edge(target, dep[1:]):
                raise ValueError(f"Forbidden GN dependency: {target} -> {dep}")
            target_graph[target].add(dep[1:])
    if set(files) != set(membership) or any(count != 1 for count in membership.values()):
        raise ValueError("Each C++ source/header must belong to exactly one GN target")
    reject_cycles(target_graph, "GN target")

    def dependency_visible(user, provider):
        pending = list(target_graph[user])
        visited = set()
        while pending:
            target = pending.pop()
            if target == provider:
                return True
            if target not in visited:
                visited.add(target)
                pending.extend(dep[1:] for dep in targets[target]["public_deps"])
        return False

    graph = {}
    all_code = []
    for name, path in sorted(files.items()):
        # C++ removes line splices before recognizing comments and directives.
        code = without_comments(re.sub(r"\\\r?\n", "", path.read_text()))
        all_code.append(code)
        graph[name] = set()
        for directive in re.findall(r'^\s*#\s*include\b([^\n]*)', code, re.M):
            literal = re.fullmatch(r'\s*(["<])([^">]+)[">]\s*', directive)
            if not literal:
                raise ValueError(f"Unsupported non-literal include: {name}: {directive.strip()}")
            delimiter, include = literal.groups()
            if delimiter == "<":
                # The closed skeleton has only standard-library angle includes.
                if include not in {"algorithm", "array", "cassert", "cstddef", "cstdint", "functional",
                                   "limits", "map", "memory", "optional", "set", "string", "tuple",
                                   "utility", "variant", "vector"}:
                    raise ValueError(f"Unverified platform/angle include: {name}: {include}")
                continue
            candidates = [(path.parent / include).resolve(), (source / include).resolve()]
            resolved = next((p for p in candidates if p.is_file()), None)
            if resolved is None or not resolved.is_relative_to(source.resolve()):
                raise ValueError(f"Missing or out-of-scope quoted include: {name}: {include}")
            provider = resolved.relative_to(source.resolve()).as_posix()
            if provider not in files or not allowed_edge(category(name), category(provider)):
                raise ValueError(f"Forbidden include: {name} -> {provider}")
            user_target, provider_target = category(name), category(provider)
            if user_target != provider_target:
                if provider not in targets[provider_target]["public"]:
                    raise ValueError(f"Non-public GN header: {name} -> {provider}")
                if not dependency_visible(user_target, provider_target):
                    raise ValueError(f"GN dependency missing for include: {name} -> {provider}")
            graph[name].add(provider)
    reject_cycles(graph, "include")
    code = "\n".join(all_code)
    for symbol in ("RequestId", "StepRef", "DefaultRole", "AccountFieldRevisions", "SubscriptionId", "SlotId",
                   "RuntimeOperationStore", "AccountRegistry", "AssignmentAggregate", "TopologyService",
                   "AssignmentService", "CardRuntime", "SessionRegistry", "ResourceBarrier", "LoadSession",
                   "LocalCardRequests"):
        definitions = re.findall(r'\b(?:enum\s+class|class|struct)\s+' + symbol + r'\b[^;{]*\{', code)
        if len(definitions) != 1:
            raise ValueError(f"Expected one definition of {symbol}, found {len(definitions)}")
    return {"files": len(files), "includes": sum(map(len, graph.values())), "targets": len(targets)}


def token_sequence(text):
    code = re.sub(r"(?m)^\s*#.*$", "", without_comments(text))
    return re.findall(r'"(?:\\.|[^"\\])*"|\w+|[^\s\w]', code)


def type_definitions(text):
    tokens = token_sequence(text)
    definitions = Counter()
    for index, token in enumerate(tokens[:-1]):
        if token not in ("class", "struct") or not re.fullmatch(r"\w+", tokens[index + 1]):
            continue
        cursor = index + 2
        while cursor < len(tokens) and tokens[cursor] not in ("{", ";"):
            cursor += 1
        if cursor == len(tokens) or tokens[cursor] != "{":
            continue  # Forward declaration; it has no layout to preserve.
        depth, end = 1, cursor + 1
        while depth and end < len(tokens):
            depth += (tokens[end] == "{") - (tokens[end] == "}")
            end += 1
        start = index - 1 if index and tokens[index - 1] == "enum" else index
        definitions[tuple(tokens[start:end])] += 1
    return definitions


def check_migration():
    manifest = json.loads((Path(__file__).parent / "source_migration.json").read_text())
    repo = ROOT.parent
    baseline, prefix = manifest["baseline"], manifest["source_root"] + "/"
    original = subprocess.check_output(["git", "ls-tree", "-rz", "--name-only", baseline, "--", prefix], cwd=repo)
    paths = {p.decode()[len(prefix):] for p in original.split(b"\0") if p}
    if paths != {entry["old"] for entry in manifest["files"]}:
        raise ValueError("Migration map does not cover exactly the baseline files")
    for entry in manifest["files"]:
        before = subprocess.check_output(["git", "show", baseline + ":" + prefix + entry["old"]], cwd=repo)
        if hashlib.sha256(before).hexdigest() != entry["sha256"]:
            raise ValueError("Baseline digest mismatch: " + entry["old"])
        text = before.decode()
        # The only intentional existing call-expression changes expose opaque API entries.
        if entry["old"] == "compatibility/legacy_sim_facade.cpp":
            text = text.replace("assignment_.StartPrimarySwitch(issued)",
                                "StartLegacyPrimarySwitch(assignment_, issued)")
            text = text.replace("activation_.StartOrdinaryActivation(issued)",
                                "StartLegacyOrdinaryActivation(activation_, issued)")
        after = "\n".join((SOURCE / name).read_text() for name in entry["destinations"])
        missing = Counter(token_sequence(text)) - Counter(token_sequence(after))
        if missing:
            raise ValueError(f"Original code tokens missing from {entry['old']}: {dict(missing)}")
        if type_definitions(text) - type_definitions(after):
            raise ValueError("Original type definition/member order changed: " + entry["old"])
        if entry["old"].endswith(".cpp"):
            after = re.sub(r'RequestId StartLegacy(?:OrdinaryActivation|PrimarySwitch)\([^{}]*\)\s*'
                           r'\{ return entry\.Start(?:OrdinaryActivation|PrimarySwitch)\(command\); \}', "", after)
            if token_sequence(text) != token_sequence(after):
                raise ValueError("Original implementation token order changed: " + entry["old"])
    print(f"migration: PASS ({len(paths)} original files, type definitions and implementation token order)", flush=True)
    revision = manifest["layout_revision"]
    tracked = subprocess.check_output(["git", "ls-tree", "-rz", "--name-only", revision["baseline"], "--", prefix], cwd=repo)
    inputs = {p.decode()[len(prefix):] for p in tracked.split(b"\0") if p}
    entries = revision["files"]
    if inputs != {entry["old"] for entry in entries} or len(inputs) != len(entries):
        raise ValueError("Layout revision must map every input file exactly once")
    actual = {p.relative_to(SOURCE).as_posix() for p in SOURCE.rglob("*") if p.is_file()}
    additions = manifest.get("additions", [])
    if actual != {entry["target"] for entry in entries} | {entry["target"] for entry in additions}:
        raise ValueError("Layout revision destinations differ from actual files")
    for entry in entries:
        before = subprocess.check_output(["git", "show", revision["baseline"] + ":" + prefix + entry["old"]], cwd=repo)
        if hashlib.sha256(before).hexdigest() != entry["sha256"]:
            raise ValueError("Layout input digest mismatch: " + entry["old"])
        if Path(entry["old"]).suffix in SUFFIXES:
            after = (SOURCE / entry["target"]).read_text()
            # Includes are checked separately; preserve all remaining tokens, including header bodies.
            if token_sequence(before.decode()) != token_sequence(after):
                raise ValueError("Layout changed code tokens: " + entry["old"])
    for entry in additions:
        before = subprocess.check_output(["git", "show", entry["source_commit"] + ":" + prefix + entry["source_path"]], cwd=repo)
        if hashlib.sha256(before).hexdigest() != entry["source_sha256"]:
            raise ValueError("G2a historical input digest differs")
        if token_sequence(before.decode()) != token_sequence((SOURCE / entry["target"]).read_text()):
            raise ValueError("G2a query semantics differ from historical source")
    # A directory move must preserve sources/public/deps and the complete GN policy.
    before_build = subprocess.check_output([
        "git", "show", revision["baseline"] + ":" + prefix + "BUILD.gn"], cwd=repo).decode()
    for entry in sorted(entries, key=lambda e: -len(e["old"])):
        before_build = before_build.replace(entry["old"], entry["target"])
    current_build = (SOURCE / "BUILD.gn").read_text()
    if additions:
        query_block = 'source_set("readonly_query") {\n  testonly = true\n  visibility = [ ":*", "//test/multisim_architecture_rules:*" ]\n  check_includes = true\n  sources = [\n    "integration/api/real_sim_count_query.cpp",\n    "integration/api/real_sim_count_query.h",\n  ]\n  public = [ "integration/api/real_sim_count_query.h" ]\n}\n\n'
        if current_build.count(query_block) != 1:
            raise ValueError("G2a query boundary changed")
        current_build = current_build.replace(query_block, "").replace('    ":readonly_query",\n', "")
    if before_build != current_build:
        raise ValueError("Layout changed GN policy beyond mapped paths")
    print(f"layout revision: PASS ({len(entries)} files; C++ tokens unchanged outside directives)", flush=True)



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--migration", action="store_true", help="Also verify the recorded baseline mapping")
    args = parser.parse_args()
    try:
        result = check()
        print("structure: PASS " + json.dumps(result, sort_keys=True) + " (textual includes/targets only)")
        if args.migration:
            check_migration()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, "structure: FAIL: " + str(error) + "\n")


if __name__ == "__main__":
    main()
