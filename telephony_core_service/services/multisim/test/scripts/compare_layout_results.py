#!/usr/bin/env python3
"""Compare original runner reports and raw XML after a path-only test migration."""
import argparse
import json
from pathlib import Path
import xml.etree.ElementTree as ET
from _paths import TEST, REPO


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--after', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--recorded-root', type=Path, help='Original runtime root when reading a relocated evidence archive')
    parser.add_argument('--evidence-root', type=Path, help='Extracted archive root; defaults to the parent of --before')
    args = parser.parse_args()
    if args.output.resolve() == REPO or REPO in args.output.resolve().parents:
        parser.error('Comparison output must be outside the repository')
    mapping = json.loads((TEST / 'docs/Test_Architecture_Migration.json').read_text())['files']

    def artifact(value):
        path = Path(value)
        if args.recorded_root is not None and path.is_relative_to(args.recorded_root):
            return (args.evidence_root or args.before.resolve().parent) / path.relative_to(args.recorded_root)
        return path

    def normalize(text):
        for r in sorted(mapping, key=lambda r: -len(r['new'])):
            text = text.replace('services/multisim/test/' + r['new'], 'services/multisim/test/' + r['old'])
        return text

    def cases(path):
        rows = {}
        for case in ET.parse(path).getroot().findall('.//testcase'):
            name = case.get('classname') + '.' + case.get('name')
            if name in rows:
                raise RuntimeError('Duplicate case: ' + name)
            rows[name] = {'status': case.get('status'), 'result': case.get('result'),
                          'skipped': case.find('skipped') is not None,
                          'failures': [normalize(f.get('message', '')) for f in case.findall('failure')]}
        if not rows:
            raise RuntimeError('Empty XML: ' + str(path))
        return rows

    before_host = json.loads((args.before / 'host/latest.json').read_text())
    after_host = json.loads((args.after / 'host/latest.json').read_text())
    before_groups = {r['name']: r for r in before_host['groups']}
    after_groups = {r['name']: r for r in after_host['groups']}
    if before_groups.keys() != after_groups.keys():
        raise RuntimeError('Host group collection changed')
    report = {'host': [], 'g2a': [], 'mutation': {}, 'pin': {}}
    for name in before_groups:
        a, b = before_groups[name], after_groups[name]
        for field in ('tests', 'passed', 'failed', 'skipped', 'exit_code', 'status'):
            if a[field] != b[field]:
                raise RuntimeError('Host result changed: ' + name + ': ' + field)
        x = cases(artifact(before_host['artifacts']) / (name + '.xml'))
        y = cases(artifact(after_host['artifacts']) / (name + '.xml'))
        if x != y:
            raise RuntimeError('Host case/assertion result changed: ' + name)
        report['host'].append({'group': name, 'tests': a['tests'], 'passed': a['passed'], 'failed': a['failed'], 'comparison': 'IDENTICAL'})
    for old in sorted((args.before / 'g2a').glob('g2a-*.xml')):
        new = args.after / 'g2a' / old.name
        if cases(old) != cases(new):
            raise RuntimeError('G2a scenario changed: ' + old.name)
        report['g2a'].append({'xml': old.name, 'comparison': 'IDENTICAL'})
    if len(report['g2a']) != 7:
        raise RuntimeError('Seven G2a scenarios required')
    for suite in ('mutation', 'pin'):
        a = json.loads((args.before / suite / 'result.json').read_text())
        b = json.loads((args.after / suite / 'result.json').read_text())
        if a['counts'] != b['counts'] or a['status'] != b['status']:
            raise RuntimeError(suite + ' original counts/status changed')
        ma, mb = a['mutation'], b['mutation']
        if ma['status'] != mb['status'] or ma['status'] != 'DETECTED':
            raise RuntimeError(suite + ' mutation capability changed')
        # Check both the injected failure and the original-code restoration via raw XML.
        old_artifacts, new_artifacts = artifact(a['artifacts']), artifact(b['artifacts'])
        for xml in ('mutation.xml', 'restored.xml') if suite == 'mutation' else ('mutation.xml', 'restored.xml'):
            if cases(old_artifacts / xml) != cases(new_artifacts / xml):
                raise RuntimeError(suite + ': ' + xml + ' changed')
        report[suite] = {'counts': b['counts'], 'raw_status': b['status'], 'mutation': 'DETECTED', 'restoration': 'IDENTICAL'}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('comparison: PASS (case identities, statuses, assertion failures, raw exits and restored mutation results)')


if __name__ == '__main__':
    main()
