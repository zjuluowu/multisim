#!/usr/bin/env python3
"""Check production archive ownership against the generated Host GN graph."""
import json
from pathlib import Path
import re
import subprocess

from _paths import CORE, TEST

LEGACY_ARCHIVE = '//:sim_legacy_production_host'
TARGET_ARCHIVE = '//:sim_target_production_host'
TARGET_TEST = '//:sim_test_target_host'
# Each deliberate omission from the Target Host archive has a current destination.
# These translation units still have logical GN ownership.
TARGET_LOGICAL_ONLY = {
    'account/activation/activate_sim_use_case.cpp': 'Full activation use case awaits the production activation path',
    'account/number/show_number_plan.cpp': 'Logical number planning; the Host path runs SetShowNumberUseCase',
    'card/esim/local_card_requests.cpp': 'Local card/profile scaffold awaits the R adapter',
    'card/runtime/card_runtime.cpp': 'Logical production bridge scaffold; Host uses narrow observation and MSISDN runtimes',
    'infrastructure/compatibility/api/legacy_sim_facade.cpp': 'Logical future compatibility facade; Host uses named H/C entries',
    'infrastructure/compatibility/api/real_sim_count_query.cpp': 'Executed by the historical G2a reference executable',
    'infrastructure/compatibility/events/legacy_output_adapter.cpp': 'Logical production event adapter; Host injects output ports',
}


def cpp_under(paths, root):
    sources = set()
    for filename in paths:
        path = Path(filename)
        if path.suffix == '.cpp' and path.is_relative_to(root):
            sources.add(path.relative_to(root).as_posix())
    return sources


def require_exact(name, actual, expected):
    if actual != expected:
        raise ValueError(f'{name}: missing {sorted(expected - actual)}, unexpected {sorted(actual - expected)}')


def dependency_closure(graph, label):
    visited = set()
    pending = list(graph[label]['deps'])
    while pending:
        dep = pending.pop()
        if dep not in visited:
            visited.add(dep)
            pending.extend(graph[dep]['deps'])
    return visited


def check_graph(graph, legacy_sources, target_sources, logical_sources, target_cases):
    target_root = CORE / 'services/multisim'
    old_root = CORE / 'services/sim/src'
    for label, root, expected in (
        (LEGACY_ARCHIVE, old_root, legacy_sources), (TARGET_ARCHIVE, target_root, target_sources)
    ):
        row = graph[label]
        if row['type'] != 'static_library' or row['testonly']:
            raise ValueError(f'{label} must be a reusable production archive')
        paths = row['sources']
        actual = cpp_under(paths, root)
        require_exact(label, actual, expected)
        if len(actual) != len(paths):
            raise ValueError(f'{label} has duplicate or foreign sources')
        for owner in dependency_closure(graph, label):
            if graph[owner]['testonly'] or any(Path(path).is_relative_to(TEST) for path in graph[owner].get('sources', [])):
                raise ValueError(f'{label} depends on a test target: {owner}')

    require_exact('Target logical-only CPP disposition', logical_sources - target_sources, set(TARGET_LOGICAL_ONLY))
    if not target_sources <= logical_sources:
        raise ValueError('Target Host archive includes CPP without logical GN ownership')
    require_exact('Target case source discovery', cpp_under(graph[TARGET_TEST]['sources'], TEST), target_cases)
    for label, row in graph.items():
        if row['type'] != 'executable':
            continue
        closure = dependency_closure(graph, label)
        if label == TARGET_TEST:
            if TARGET_ARCHIVE not in closure or any(
                cpp_under(graph[dep].get('sources', []), old_root) for dep in closure | {label}
            ):
                raise ValueError('Target test must link Target production and no Legacy SIM implementation')
        elif TARGET_ARCHIVE in closure:
            raise ValueError(f'Legacy or infrastructure executable links Target production: {label}')

    for label in ('//:sim_primary_slot_real_test', '//:sim_bootstrap_characterization'):
        closure = dependency_closure(graph, label)
        if LEGACY_ARCHIVE not in closure or '//:sim_production_host' in closure:
            raise ValueError(f'{label} must link only the full Legacy SIM archive')


def check_host_build(build_dir, host_root):
    graph = json.loads(subprocess.check_output(
        ['gn', 'desc', str(build_dir), '*', '--root=' + str(host_root), '--format=json'], text=True))
    legacy = (CORE / 'sim_legacy_sources.gni').read_text().split('sim_legacy_esim_sources')[0]
    target = (CORE / 'sim_target_sources.gni').read_text()
    logical = (CORE / 'services/multisim/BUILD.gn').read_text()
    check_graph(
        graph,
        set(re.findall(r'\$sim_legacy_root/services/sim/src/([^"\n]+\.cpp)', legacy)),
        set(re.findall(r'\$sim_target_root/([^"\n]+\.cpp)', target)),
        set(re.findall(r'"([^"\n]+\.cpp)"', logical)),
        {path.relative_to(TEST).as_posix() for path in (TEST / 'cases/target').rglob('*.cpp')},
    )


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path, help='Generated Host GN output directory')
    args = parser.parse_args()
    check_host_build(args.build.resolve(), TEST / 'framework/internal/host')
    print('Host production archive ownership: PASS')
