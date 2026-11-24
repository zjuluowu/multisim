"""Check the exact active side of the M1 delta before replaying historical layout checks."""
import hashlib
import json
from pathlib import Path


def migration_inputs(source, repo):
    from check_directory_migration import committed_inputs

    manifest = json.loads(Path(__file__).with_name('af2_m1_source_evolution.json').read_text())
    entries = manifest['files']
    prefix = 'telephony_core_service/services/multisim'
    before = committed_inputs(repo, manifest['baseline'], prefix, sorted(entries))
    for name, expected in entries.items():
        if hashlib.sha256((source / name).read_bytes()).hexdigest() != expected:
            raise ValueError('AF2 M1 source input differs: ' + name)
    return before
