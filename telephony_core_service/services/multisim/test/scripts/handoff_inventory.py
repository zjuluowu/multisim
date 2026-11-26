#!/usr/bin/env python3
"""Measure the tracked M9 source tree and the observed Host build/test closure."""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re
import subprocess
import sys

from _paths import CORE, REPO, TEST

SOURCE_SUFFIXES = {'.h', '.hpp', '.cpp', '.cc', '.c'}
DOMAINS = {'account', 'card', 'device', 'infrastructure', 'role_assignment'}
# Evidence inventory only. The runner does not use this table to discover tests.
SHARED_BODIES = {
    'SIM028-LEGACY-C0-DUAL-PRIMARY0': 'cellular_selection_scenario.h',
    'SIM027-LEGACY-DATA-INDEPENDENT-DUAL-PRIMARY0': 'cellular_selection_scenario.h',
    'SIM025-LEGACY-DEFAULT-VOICE-DUAL': 'default_call_message_scenario.h',
    'SIM026-LEGACY-DEFAULT-SMS-DUAL': 'default_call_message_scenario.h',
    'SIM023-LEGACY-ACCOUNT-METADATA-DUAL': 'account_metadata_scenario.h',
    'SIM024-LEGACY-IMS-PREFERENCE-BOUND': 'ims_preference_scenario.h',
    'SIM023-LEGACY-SHOW-NUMBER-AFTER-CARD-FAILURE': 'show_number_scenario.h',
    'SIM021-ACCOUNT-RELOAD-FRESH-VIEWS': 'account_reload_scenario.h',
}
SHARED_ASSERTION_CASES = (
    {
        'id': 'SIM021-ACCOUNT-RELOAD-FRESH-VIEWS',
        'legacy': {'test': 'RealSimPrimary.FreshAccountReloadKeepsViewsIndependent'},
        'target': {'test': 'AF2Reload.FreshAccountReloadKeepsViewsIndependent'},
        'validation': 'same_source_assertions',
    },
)
FRAMEWORK_SUITES = {'SchedulerBoundary', 'RunnerLifetime', 'EventAdapter', 'ExternalData',
                    'ExternalEvents', 'RilAdapter', 'ResourceModel', 'SimCharacterization'}
TARGET_ARCHITECTURE_SUITES = {'TargetArchitecture', 'TargetTopologyIntegration', 'TargetPrimary',
                              'TargetPrimaryB', 'TargetRadioReceipt', 'TargetAdapterIntegration',
                              'AF2Reload', 'AF2Module', 'AF2Discovery'}
TARGET_INTERNAL_CASES = {
    'AF2AccountMetadata.FieldAndRowIsolationWhileOriginalWriteIsInFlight',
    'AF2AccountMetadata.ReplacementDoesNotReceiveTheOriginalSlotWrite',
    'AF2AccountMetadata.UnknownRequiresExactOriginalEvidenceAfterStop',
    'AF2Data.PrimaryAndDataRejectCompetingSharedWriteInBothOrders',
    'AF2Data.ReplacementDuringOriginalWritePreventsProjectionToNewBinding',
    'AF2Data.StopWaitsForParameterAndAccountNotification',
    'AF2Data.UnknownRetainsSharedWriteUntilOriginalEvidenceAfterStop',
    'AF2ImsPreference.ConfirmedWriteAfterReplacementKeepsRawReturnWithoutAdvancingVersions',
    'AF2ImsPreference.FieldIsolationAndStopDuringOriginalWrite',
    'AF2ImsPreference.ReplacementDuringQueryKeepsOriginalIccidAndOpenCall',
    'AF2ImsPreference.UnknownKeepsTheExactOriginalWriteUntilEvidenceAfterStop',
    'AF2Primary.InterruptedIccidWriteRetainsPrefixAndOnlyAcceptsOriginalEvidenceAfterStop',
    'AF2Primary.ReplacementDuringPrimarySlotWriteRevokesRemainingKeys',
    'AF2Primary.StopInFlightAndDuringOriginalNotificationRetainsCallUntilReturn',
    'AF2ShowNumber.BackwriteRejectsStaleNumberRevisionOnSameBinding',
    'AF2ShowNumber.TimeoutQuarantinesOriginalResourcesUntilCorrelatedProofAfterStop',
    'AF2ShowNumber.UnresolvedStoreRetainsNumberFieldAndUsesOriginalEvidence',
    'AF2VoiceSms.CurrentRoleVersionRejectsPreMutationReload',
    'AF2VoiceSms.InFlightVoiceOrdersOnlyIntersectingWriters',
    'AF2VoiceSms.KnownPartialAndStaleAppliedNeverCommitCurrentRoles',
    'AF2VoiceSms.StaleUnknownVoiceEffectRetainsOnlyItsOriginalWriteSet',
}
# Count a physical line when it contains C/C++ tokens other than comments.
LEXEME = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[^\s]', re.S)


def source_size(text):
    code_lines = set()
    offset = 0
    line = 1
    for value in LEXEME.finditer(text):
        line += text.count('\n', offset, value.start())
        token = value.group()
        if not token.startswith(('//', '/*')):
            code_lines.update(range(line, line + token.count('\n') + 1))
        line += token.count('\n')
        offset = value.end()
    return {'files': 1, 'physical': len(text.splitlines()), 'code': len(code_lines)}


def summarize(rows):
    directories = defaultdict(Counter)
    for row in rows:
        path = Path(row['path'])
        for parent in path.parents:
            directories[parent.as_posix()].update(row['size'])
    return {path: dict(count) for path, count in sorted(directories.items())}


def observed_cases(report):
    data = json.loads(report.read_text())
    if data.get('status') != 'PASS':
        raise ValueError(f'Accepted full Host report required: {report}')
    executed = [row for row in data['cases'] if row['status'] == 'PASS']
    by_id = {row['id']: row for row in executed}
    if len(by_id) != len(executed):
        raise ValueError(f'Duplicate executed test identity: {report}')
    return by_id


def test_locations(files):
    locations = defaultdict(list)
    registration = re.compile(r'\b(?:TEST|TEST_F|TEST_P)\s*\(\s*(\w+)\s*,\s*(\w+)', re.M)
    for file in files:
        source = file.read_text()
        for match in registration.finditer(source):
            locations['.'.join(match.groups())].append({
                'file': file.relative_to(TEST).as_posix(), 'line': source.count('\n', 0, match.start()) + 1,
            })
    return locations


def editor_locations(report):
    listing = json.loads(report.read_text())
    if listing.get('status') != 'PASS':
        raise ValueError(f'Successful GoogleTest discovery report required: {report}')
    result = {}
    for case in listing['cases']:
        key = case['target'], case['name']
        if key in result:
            raise ValueError(f'Duplicate GoogleTest discovery location: {key}')
        result[key] = {'file': Path(case['file']).resolve().relative_to(TEST).as_posix(), 'line': case['line']}
    return result


def semantics(legacy, target, matrix, locations, editor):
    eligible = [dict(row, validation='frozen_oracle') for row in matrix if row['differential'] == 'SUPPORTED']
    paired = [row for row in (*eligible, *SHARED_ASSERTION_CASES) if
              row['legacy']['test'] in legacy and row['target']['test'] in target]
    mapped = {backend: {row[backend]['test'] for row in paired} for backend in ('legacy', 'target')}
    decisions = json.loads((TEST / 'config/approved_compatibility_contracts.json').read_text())['contracts']
    target_decisions = {case: decision for decision, contract in decisions.items()
                        for case in contract['target_host']}
    legacy_decisions = {contract['legacy_host']['case']: decision for decision, contract in decisions.items()
                        if 'legacy_host' in contract}

    def registration(backend, id_, data):
        suite, test = id_.split('.', 1)
        suite, test = suite.split('/')[-1], test.split('/')[0]
        key = data.get('target'), id_
        if editor is not None:
            if key not in editor:
                raise ValueError(f'Executed test absent from GoogleTest discovery: {key}')
            candidates = [editor[key]]
        else:
            candidates = locations[suite + '.' + test]
            if not candidates:
                candidates = locations['SimTest.' + test] if suite == 'RealSimPrimary' else []
        if id_ in mapped[backend]:
            category = 'C'
        elif suite in FRAMEWORK_SUITES:
            category = 'F'
        elif backend == 'target' and (suite in TARGET_ARCHITECTURE_SUITES or id_ in TARGET_INTERNAL_CASES):
            category = 'T'
        else:
            category = 'L' if backend == 'legacy' else 'UNCLASSIFIED'
        decision = (legacy_decisions if backend == 'legacy' else target_decisions).get(id_)
        return {'id': id_, 'category': category, 'target': data.get('target'), 'approved_decision': decision,
                'source_candidates': candidates}

    observations = {backend: [registration(backend, case, rows[case]) for case in sorted(rows)]
                    for backend, rows in (('legacy', legacy), ('target', target))}
    for row in paired:
        body = SHARED_BODIES.get(row['id'])
        if not body:
            continue
        if not (TEST / 'cases/shared' / body).is_file():
            raise ValueError(f'Shared test body is missing: {body}')
        for backend, suite in (('legacy', legacy), ('target', target)):
            case = row[backend]['test']
            sources = registration(backend, case, suite[case])['source_candidates']
            if not any(body in (TEST / source['file']).read_text() for source in sources):
                raise ValueError(f'{backend} registration has no source reference to {body}: {case}')
    return {
        'registrations': observations,
        'registration_counts': {key: dict(Counter(row['category'] for row in rows))
                                for key, rows in observations.items()},
        'known_common_semantics': [{'id': row['id'], 'legacy': row['legacy']['test'],
                                    'target': row['target']['test'],
                                    'validation': row['validation'],
                                    'shared_body': SHARED_BODIES.get(row['id'])} for row in paired],
        'device_gate_rows': [{'id': row['id'], 'category': 'D', 'legacy': row['legacy']['device'],
                              'target': row['target']['device']} for row in matrix],
        'unresolved_semantic_deduplication': True,
    }


def build_inventory(database):
    compiled = set()
    for row in json.loads(database.read_text()):
        command = row.get('command', ' '.join(row.get('arguments', [])))
        if 'libsim_target_production_host.' not in command:
            continue
        file = (Path(row['directory']) / row['file']).resolve()
        compiled.add(file.relative_to(CORE / 'services/multisim').as_posix())
    manifest = (CORE / 'sim_target_sources.gni').read_text()
    expected = set(re.findall(r'\$sim_target_root/([^"\n]+\.cpp)', manifest))
    if not expected or compiled != expected:
        raise ValueError(f'Target Host compile closure differs: missing={sorted(expected - compiled)}, '
                         f'unexpected={sorted(compiled - expected)}')
    return compiled


def execution_inventory(reports, module):
    methods = defaultdict(list)
    for path in reports:
        report = json.loads(path.read_text())
        if report.get('status') != 'PASS':
            raise ValueError(f'Successful real execution audit required: {path}')
        for item in report['methods']:
            for filename in item['files']:
                evidence = Path(filename)
                if evidence.is_relative_to(module):
                    methods[evidence.relative_to(module).as_posix()].append({
                        'method': item['owner'] + '::' + item['method'], 'count': item['count'],
                    })
    return methods


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-db', type=Path, required=True)
    parser.add_argument('--legacy', type=Path)
    parser.add_argument('--target', type=Path)
    parser.add_argument('--editor', type=Path, help='Actual GoogleTest discovery locations from check_editor.py')
    parser.add_argument('--execution', type=Path, nargs='*', default=[], help='Real Target execution audit reports')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    module = CORE / 'services/multisim'
    tracked = subprocess.check_output(['git', 'ls-files', '-z', '--', str(module)], cwd=REPO).decode().split('\0')
    host_sources = build_inventory(args.compile_db)
    sys.path.insert(0, str(TEST / 'cases/architecture'))
    from check_structure import gn_targets

    declarations = {name: owner for owner, target in gn_targets(module).items() for name in target['sources']}
    execution = execution_inventory(args.execution, module)
    production, tests = [], []
    for name in filter(None, tracked):
        file = REPO / name
        relative = file.relative_to(module)
        if file.suffix not in SOURCE_SUFFIXES:
            continue
        domain = relative.parts[0]
        row = {'path': relative.as_posix(), 'size': source_size(file.read_text())}
        if domain in DOMAINS:
            row['build'] = ('host-object' if row['path'] in host_sources else
                            'logical-gn' if row['path'] in declarations else 'undeclared')
            row['logical_owner'] = declarations.get(row['path'])
            if file.suffix == '.cpp':
                row['audited_method_hits'] = execution.get(row['path'], [])
            production.append(row)
        elif domain == 'test' and relative.parts[1] in {'cases', 'framework'}:
            tests.append(row | {'path': Path(*relative.parts[1:]).as_posix()})
    if not host_sources <= {row['path'] for row in production}:
        raise ValueError('Target compile database references sources outside the tracked production inventory')
    result = {'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(),
              'production': production, 'production_by_directory': summarize(production),
              'tests_by_directory': summarize(tests)}
    if args.legacy and args.target:
        legacy, target = observed_cases(args.legacy), observed_cases(args.target)
        matrix = json.loads((TEST / 'config/backend_capabilities.json').read_text())['scenarios']
        common = {row['id'] for row in matrix if row['differential'] == 'SUPPORTED' and
                  row['legacy']['test'] in legacy and row['target']['test'] in target}
        result['backend_registrations'] = {'legacy': sorted(legacy), 'target': sorted(target),
                                          'mapped_common_semantics': sorted(common)}
        cases = [TEST / row['path'] for row in tests
                 if row['path'].startswith('cases/') and row['path'].endswith('.cpp')]
        result['semantic_inventory'] = semantics(legacy, target, matrix, test_locations(cases),
                                                 editor_locations(args.editor) if args.editor else None)
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n')


if __name__ == '__main__':
    main()
