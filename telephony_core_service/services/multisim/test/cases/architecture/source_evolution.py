"""Verify active AF-2 inputs and replay the frozen layout comparisons."""
import hashlib
import json
from pathlib import Path


def load_manifest(name):
    return json.loads(Path(__file__).with_name(name + '_source_evolution.json').read_text())


def migration_inputs(source, repo):
    from check_directory_migration import committed_inputs

    m1 = load_manifest('af2_m1')
    m2 = load_manifest('af2_m2')
    m3 = load_manifest('af2_m3')
    prefix = 'telephony_core_service/services/multisim'
    m4 = load_manifest('af2_m4')
    before_m4 = committed_inputs(repo, m4['baseline'], prefix, sorted(m4['files']))
    for name, expected in m4['files'].items():
        if hashlib.sha256((source / name).read_bytes()).hexdigest() != expected:
            raise ValueError('AF2 M4 source input differs: ' + name)

    def pre_m4(name):
        return before_m4[name] if name in before_m4 else (source / name).read_bytes()

    before_m3 = committed_inputs(repo, m3['baseline'], prefix, sorted(m3['files']))
    for name, expected in (m3['files'] | m3['added']).items():
        if hashlib.sha256(pre_m4(name)).hexdigest() != expected:
            raise ValueError('AF2 M3 source input differs: ' + name)

    def pre_m3(name):
        return before_m3[name] if name in before_m3 else pre_m4(name)

    prior = m2['prior_layout_deltas']
    before_m2 = committed_inputs(repo, m2['baseline'], prefix, sorted(set(m2['files']) | set(prior)))
    for name, delta in prior.items():
        if not delta['reason'] or hashlib.sha256(before_m2[name]).hexdigest() != delta['sha256']:
            raise ValueError('Pre-M2 historical layout input differs: ' + name)
        if name not in m2['files'] and pre_m3(name) != before_m2[name]:
            raise ValueError('Active pre-M2 layout input differs: ' + name)
    for name, expected in (m2['files'] | m2['added']).items():
        if hashlib.sha256(pre_m3(name)).hexdigest() != expected:
            raise ValueError('AF2 M2 source input differs: ' + name)
    before_m1 = committed_inputs(repo, m1['baseline'], prefix, sorted(m1['files']))
    for name, expected in m1['files'].items():
        contents = before_m2[name] if name in before_m2 else pre_m3(name)
        if hashlib.sha256(contents).hexdigest() != expected:
            raise ValueError('AF2 M1 source input differs: ' + name)
    return before_m4 | before_m3 | before_m2 | before_m1


def matches_prior_layout_delta(name, predecessor):
    # The historical input has already been checked against its original commit and digest.
    expected = load_manifest('af2_m2')['prior_layout_deltas'].get(name)
    return bool(expected and hashlib.sha256(predecessor).hexdigest() == expected['sha256'])
