#!/usr/bin/env python3
"""Seal full-replacement inputs and raw results without importing the old framework.

The manifest is an inventory, not a coverage or equivalence certificate. Run this
before retiring any old implementation. Existing evidence is never overwritten.
"""
import argparse
import hashlib
import json
import platform
from pathlib import Path
import subprocess
import tarfile
import xml.etree.ElementTree as ET

TEST = Path(__file__).resolve().parents[1]
REPO = TEST.parents[3]


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def git(*args):
    return subprocess.check_output(['git', *args], cwd=REPO).decode().strip()


def disposition(path):
    """Planned disposition, deliberately separate from completed migration."""
    name = path.as_posix()
    if name.startswith('docs/archive/') or '/baseline/' in name or '/golden/' in name:
        return 'historical_evidence', 'Immutable original evidence; no new runtime dependency'
    if name.startswith('cases/architecture/'):
        return 'rewrite', 'Retain boundary, mutation and provenance checks in unified scheduling'
    if name.startswith('cases/') and path.suffix == '.cpp':
        return 'migrate_case', 'Preserve assertions and production execution paths; compare raw results'
    if name.startswith('cases/compatibility/scenarios/') or '/oracle/' in name:
        return 'rewrite', 'Inline business expectations into cases; archive original expectations'
    if name.startswith('framework/host/stubs/'):
        return 'reuse_framework', 'External platform boundaries only; retain strict mocks and cleanup'
    if name.startswith('framework/compatibility/fakes/'):
        return 'reuse_framework', 'Device facts and transport fault injection; never synthesize SUT results'
    if name.startswith('framework/compatibility/contracts/') or '/adapters/' in name:
        return 'rewrite', 'Replace adapter hierarchy with one fixture and platform implementation'
    if name.startswith('scripts/') or path.suffix in ('.gn', '.gni') or path.name == '.gn':
        return 'rewrite', 'One build/run/discovery/report flow; retire duplicate runners after equivalence'
    if path.suffix in ('.md', '.json'):
        return 'historical_evidence', 'Review active configuration separately; preserve original bytes'
    return 'reuse_framework', 'Review implementation before relocation; no deletion authorized by this row'


def xml_cases(path):
    root = ET.parse(path).getroot()
    rows = []
    for suite in root.iter('testsuite'):
        for case in suite.findall('testcase'):
            failures = [n.text or '' for n in case.findall('failure')]
            skipped = case.find('skipped')
            status = ('FAIL' if failures else 'NOT_RUN' if case.get('status') == 'notrun'
                      else 'UNSUPPORTED' if skipped is not None else 'PASS')
            rows.append({'id': case.get('classname', suite.get('name', '')) + '.' + case.get('name', ''),
                         'status': status, 'failures': failures,
                         'properties': {p.get('name'): p.get('value') for p in case.findall('properties/property')}})
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--results', required=True, type=Path, help='Completed external run directory')
    parser.add_argument('--output', required=True, type=Path, help='New evidence directory (may be in docs/archive)')
    parser.add_argument('--results-only', action='store_true', help='Seal a follow-up run; original inventory remains in the baseline')
    args = parser.parse_args()
    results, output = args.results.resolve(), args.output.resolve()
    if not results.is_dir() or results == REPO or REPO in results.parents:
        parser.error('--results must be an external run directory')
    if output.exists():
        parser.error('--output must not exist; evidence is append-only')
    report = results / 'result.json'
    if not report.is_file():
        parser.error('--results must contain result.json from the completed runner')
    summary = json.loads(report.read_text())
    commands = summary.get('commands') or [
        {key: row[key] for key in ('id', 'status', 'exit_code', 'command', 'reason')}
        for row in summary.get('cases', [])]
    if not commands:
        parser.error('No completed commands in result.json')
    head = git('rev-parse', 'HEAD')
    prefix = TEST.relative_to(REPO).as_posix() + '/'
    paths = subprocess.check_output(['git', 'ls-files', '-z', '--', str(TEST), str(REPO / '.vscode')], cwd=REPO).split(b'\0')
    assets = []
    if args.results_only:
        paths = []
    for encoded in paths:
        if not encoded:
            continue
        relative = encoded.decode()
        path = REPO / relative
        local = Path(relative.removeprefix(prefix))
        action, reason = disposition(local)
        assets.append({'path': relative, 'sha256': hashlib.sha256(subprocess.check_output(['git', 'show', head + ':' + relative], cwd=REPO)).hexdigest(),
                       'worktree_sha256_at_capture': digest(path), 'bytes': path.stat().st_size,
                       'planned_disposition': action, 'reason': reason, 'migration_status': 'NOT_MIGRATED'})
    raw = []
    cases = []
    for path in sorted(results.rglob('*')):
        if not path.is_file() or path.is_symlink():
            continue
        # Compiler objects, executables and profiles are reproducible build products.
        if path.suffix not in ('.xml', '.log', '.json', '.txt'):
            continue
        if 'build' in path.relative_to(results).parts or 'gn' in path.relative_to(results).parts:
            continue
        name = path.relative_to(results).as_posix()
        raw.append({'path': name, 'sha256': digest(path), 'bytes': path.stat().st_size})
        if path.suffix == '.xml':
            cases.extend({'report': name, **row} for row in xml_cases(path))
    # Raw child reports retain authoritative exit codes. Do not collapse blocked
    # preparation into passed business cases or deduplicate scenario executions.
    output.mkdir(parents=True)
    with tarfile.open(output / 'raw-results.tar.gz', 'w:gz') as archive:
        for row in raw:
            archive.add(results / row['path'], arcname=row['path'], recursive=False)
    manifest = {'schema_version': 1, 'head': head, 'platform': platform.platform(),
                'machine': platform.machine(), 'git_status': git('status', '--short'),
                'scope': 'Original inputs and results; planned dispositions are not completed migrations',
                'commands': commands, 'assets': assets, 'raw_files': raw, 'cases': cases,
                'archive_sha256': digest(output / 'raw-results.tar.gz')}
    (output / 'baseline.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    print(f'{len(assets)} assets; {len(cases)} XML observations; {len(raw)} raw reports sealed at {output}')


if __name__ == '__main__':
    main()
