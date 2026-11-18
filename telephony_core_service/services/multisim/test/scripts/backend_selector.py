#!/usr/bin/env python3
"""Resolve SIM test backend configuration without importing either SUT."""
import argparse
import json
from pathlib import Path

TEST = Path(__file__).resolve().parents[1]
DEFAULT_CONFIG = TEST / 'config/test_backend.json'
CAPABILITIES = TEST / 'config/backend_capabilities.json'
BACKENDS = ('legacy', 'target', 'both')
PLATFORMS = ('host', 'device')


def load_config(path=DEFAULT_CONFIG):
    value = json.loads(Path(path).read_text())
    merged = {
        'backend': 'legacy', 'platform': 'host', 'build_if_missing': True,
        'compare_with_oracle': True, 'output_dir': '.test-out',
    }
    merged.update(value)
    if merged['backend'] not in BACKENDS:
        raise ValueError('backend must be legacy, target, or both')
    if merged['platform'] not in PLATFORMS:
        raise ValueError('platform must be host or device')
    if not isinstance(merged['build_if_missing'], bool) or not isinstance(merged['compare_with_oracle'], bool):
        raise ValueError('build_if_missing and compare_with_oracle must be booleans')
    if not isinstance(merged['output_dir'], str) or not merged['output_dir']:
        raise ValueError('output_dir must be a non-empty string')
    return merged


def resolve(args, config_path=DEFAULT_CONFIG):
    config = load_config(config_path)
    for key in ('backend', 'platform'):
        value = getattr(args, key, None)
        if value is not None:
            config[key] = value
    if getattr(args, 'build_if_missing', None) is not None:
        config['build_if_missing'] = args.build_if_missing
    if getattr(args, 'compare_with_oracle', None) is not None:
        config['compare_with_oracle'] = args.compare_with_oracle
    if getattr(args, 'output_dir', None) is not None:
        config['output_dir'] = str(args.output_dir)
    return config


def expanded_backends(value):
    return ('legacy', 'target') if value == 'both' else (value,)


def capabilities(path=CAPABILITIES):
    return json.loads(Path(path).read_text())['scenarios']


def selected_scenarios(test_filter='*'):
    rows = capabilities()
    if not test_filter or test_filter == '*':
        return rows
    return [row for row in rows if test_filter in row['id'] or
            any(test_filter in (row[backend].get('test') or '') for backend in ('legacy', 'target'))]


def capability_report(backend, platform, test_filter='*'):
    report = []
    for row in selected_scenarios(test_filter):
        value = row[backend].get(platform)
        reason = row[backend].get('reason')
        report.append({'scenario': row['id'], 'test': row[backend].get('test'),
                       'status': value, 'reason': reason})
    return report


def gtest_filter(backend, test_filter='*', platform='host'):
    if not test_filter or test_filter == '*':
        return '*'
    rows = selected_scenarios(test_filter)
    if not rows:
        return test_filter
    names = [row[backend].get('test') for row in rows
             if row[backend].get(platform) != 'NOT_APPLICABLE' and row[backend].get('test')]
    return ':'.join(names) if names else test_filter


def scenario_for_test(backend, test_name):
    for row in capabilities():
        if row[backend].get('test') == test_name:
            return row['id']
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, default=DEFAULT_CONFIG)
    parser.add_argument('--backend', choices=BACKENDS)
    parser.add_argument('--platform', choices=PLATFORMS)
    parser.add_argument('--filter', default='*')
    args = parser.parse_args()
    config = load_config(args.config)
    if args.backend:
        config['backend'] = args.backend
    if args.platform:
        config['platform'] = args.platform
    print(json.dumps({'config': config, 'capabilities': {
        backend: capability_report(backend, config['platform'], args.filter)
        for backend in expanded_backends(config['backend'])}}, indent=2))


if __name__ == '__main__':
    main()
