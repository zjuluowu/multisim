"""Test-only reference and production source-table boundaries."""
import re
from _paths import CORE
SOURCES = [
    "services/multisim/test/framework/internal/host/g2a_reference/core_service_sim.cpp",
    "services/multisim/test/framework/internal/host/g2a_reference/legacy_sim_query_binding.cpp",
    "services/multisim/infrastructure/compatibility/api/real_sim_count_query.cpp",
    "services/sim/src/sim_manager.cpp",
    "services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp",
]

def gn_tokens(text):
    # Literal source-table audit only, not a GN evaluator. Keep strings intact and
    # discard comments so a commented-out source cannot count as a compiled input.
    return [token for token in re.findall(r'"(?:\\.|[^"\\])*"|#[^\n]*|\+=|-=|\w+|\S', text)
            if not token.startswith("#")]


def target_body(tokens, target):
    starts = [i + 5 for i in range(len(tokens) - 4)
              if tokens[i:i + 5] == [target[0], "(", '"' + target[1] + '"', ")", "{"]]
    if len(starts) != 1:
        raise RuntimeError("Missing or duplicate G2a target: " + target[1])
    start = starts[0]
    depth = 1
    for end in range(start, len(tokens)):
        depth += (tokens[end] == "{") - (tokens[end] == "}")
        if depth == 0:
            return tokens[start:end]
    raise RuntimeError("Unclosed G2a target: " + target[1])


def literal_lists(tokens, field):
    """Read unconditional literal lists; conditional additions cannot supply G2a."""
    depth = 0
    values = []
    for i, token in enumerate(tokens[:-2]):
        depth += (token == "{") - (token == "}")
        if depth or token != field or tokens[i + 1] not in {"=", "+=", "-="}:
            continue
        if tokens[i + 2] != "[":
            raise RuntimeError("Non-literal G2a list: " + field)
        end = tokens.index("]", i + 3)
        content = tokens[i + 3:end]
        if any(item != "," and not item.startswith('"') for item in content):
            raise RuntimeError("Non-literal G2a list entry: " + field)
        items = [item[1:-1] for item in content if item != ","]
        if tokens[i + 1] == "=":
            values = items
        elif tokens[i + 1] == "+=":
            values.extend(items)
        else:
            values = [value for value in values if value not in items]
    return values


def verify_source_tables(read_text=None):
    read_text = read_text or (lambda name: (CORE / name).read_text())
    production_tables = ["BUILD.gn", "test/BUILD.gn", "test/core_service_test.gni",
                         "test/unittest/raw_parcel_core_service_gtest/BUILD.gn"]
    for name in production_tables:
        tokens = gn_tokens(read_text(name))
        if any("multisim" in token or "legacy_sim_query_binding" in token for token in tokens):
            raise RuntimeError("Refactor attachment leaked into upstream build: " + name)
    name = "services/multisim/test/framework/internal/host/BUILD.gn"
    tokens = gn_tokens(read_text(name))
    body = target_body(tokens, ("source_set", "g2a_readonly_production_host"))
    sources = literal_lists(body, "sources")
    for source in SOURCES[:3]:
        expected = ("$test_root/" + source.split("services/multisim/test/", 1)[1]
                    if "/test/" in source else "$production_root/" + source)
        if sources.count(expected) != 1:
            raise RuntimeError("Missing or duplicate reference input: " + source)
    if len(sources) != 3:
        raise RuntimeError("Unexpected reference source")
    launcher = target_body(tokens, ("source_set", "g2a_query_launcher"))
    if "$test_root/framework/internal/host/g2a_reference" not in literal_lists(launcher, "include_dirs"):
        raise RuntimeError("Missing reference header boundary")
    leaf = target_body(gn_tokens(read_text("services/multisim/BUILD.gn")), ("source_set", "readonly_query"))
    if (leaf[:3] != ["testonly", "=", "true"] or
            literal_lists(leaf, "sources") != ["infrastructure/compatibility/api/real_sim_count_query.cpp", "infrastructure/compatibility/api/real_sim_count_query.h"] or
            literal_lists(leaf, "public") != ["infrastructure/compatibility/api/real_sim_count_query.h"] or
            any(field in leaf for field in ["deps", "public_deps", "public_configs"])):
        raise RuntimeError("G2a leaf must remain isolated and testonly")
    return {"tables": production_tables + [name], "production_attachment_absent": True, "reference_inputs_exact": True,
            "narrow_query_include_boundary": True, "isolated_testonly_leaf": True,
            "product_admission": "NOT_PROVEN_BY_SOURCE_TABLE_CHECK"}


def is_method(function, owner, method):
    # These non-template methods use the Itanium ABI. Lengths and the closing E
    # prevent Read matching ReadLegacyRealSimCount, or Init matching OnInit.
    return str(len(owner)) + owner + str(len(method)) + method + "E" in function["name"]
