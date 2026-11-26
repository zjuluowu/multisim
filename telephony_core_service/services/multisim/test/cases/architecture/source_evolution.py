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
    m8 = load_manifest('af2_m8')
    before_m8 = committed_inputs(repo, m8['baseline'], prefix, sorted(m8['files']))
    for name, expected in m8['files'].items():
        if hashlib.sha256((source / name).read_bytes()).hexdigest() != expected:
            raise ValueError('AF2 M8 source input differs: ' + name)

    def pre_m8(name):
        return before_m8[name] if name in before_m8 else (source / name).read_bytes()

    m7b = load_manifest('af2_m7b')
    before_m7b = committed_inputs(repo, m7b['baseline'], prefix, sorted(m7b['files']))
    for name, expected in (m7b['files'] | m7b['added']).items():
        if hashlib.sha256(pre_m8(name)).hexdigest() != expected:
            raise ValueError('AF2 M7B source input differs: ' + name)

    def pre_m7b(name):
        return before_m7b[name] if name in before_m7b else pre_m8(name)

    m7a = load_manifest('af2_m7a')
    before_m7a = committed_inputs(repo, m7a['baseline'], prefix, sorted(m7a['files']))
    for name, expected in m7a['files'].items():
        if hashlib.sha256(pre_m7b(name)).hexdigest() != expected:
            raise ValueError('AF2 M7A source input differs: ' + name)

    def pre_m7a(name):
        return before_m7a[name] if name in before_m7a else pre_m7b(name)

    m6 = load_manifest('af2_m6')
    before_m6 = committed_inputs(repo, m6['baseline'], prefix, sorted(m6['files']))
    for name, expected in (m6['files'] | m6['added']).items():
        if hashlib.sha256(pre_m7a(name)).hexdigest() != expected:
            raise ValueError('AF2 M6 source input differs: ' + name)

    def pre_m6(name):
        return before_m6[name] if name in before_m6 else pre_m7a(name)

    m5 = load_manifest('af2_m5')
    before_m5 = committed_inputs(repo, m5['baseline'], prefix, sorted(m5['files']))
    for name, expected in m5['files'].items():
        if hashlib.sha256(pre_m6(name)).hexdigest() != expected:
            raise ValueError('AF2 M5 source input differs: ' + name)

    def pre_m5(name):
        return before_m5[name] if name in before_m5 else pre_m6(name)

    m4 = load_manifest('af2_m4')
    before_m4 = committed_inputs(repo, m4['baseline'], prefix, sorted(m4['files']))
    for name, expected in m4['files'].items():
        if hashlib.sha256(pre_m5(name)).hexdigest() != expected:
            raise ValueError('AF2 M4 source input differs: ' + name)

    def pre_m4(name):
        return before_m4[name] if name in before_m4 else pre_m5(name)

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
    return before_m8 | before_m7b | before_m7a | before_m6 | before_m5 | before_m4 | before_m3 | before_m2 | before_m1


def matches_prior_layout_delta(name, predecessor):
    # The historical input has already been checked against its original commit and digest.
    expected = load_manifest('af2_m2')['prior_layout_deltas'].get(name)
    return bool(expected and hashlib.sha256(predecessor).hexdigest() == expected['sha256'])
