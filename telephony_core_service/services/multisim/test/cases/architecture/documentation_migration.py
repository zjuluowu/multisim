"""Resolve explicitly retired design copies without excluding module documentation."""
import hashlib
import json
from pathlib import Path


def documentation_moves(source):
    root = Path(__file__).resolve().parent
    historical = json.loads((root / 'source_migration.json').read_text())['documentation_additions']
    evolution = json.loads((root / 'documentation_migration.json').read_text())
    moves = evolution['files']
    if evolution['schema_version'] != 1 or set(moves) != {r['target'] for r in historical}:
        raise ValueError('Documentation migration must cover the registered design copies exactly')
    component = source.parents[1].resolve()
    resolved = {}
    for row in historical:
        old = row['target']
        update = moves[old]
        target = Path(update['target'])
        if (Path(old).name != old or Path(old).suffix != '.md' or target.is_absolute()
                or target.parts[:1] != ('docs',) or '..' in target.parts or target.suffix != '.md'
                or update['sha256'] != row['sha256'] or not update['reason']):
            raise ValueError('Invalid documentation migration: ' + old)
        if (source / old).exists():
            raise ValueError('Retired documentation copy still exists: ' + old)
        current = component / target
        if not current.resolve().is_relative_to(component / 'docs') or not current.is_file():
            raise ValueError('Canonical documentation missing or outside docs: ' + str(target))
        if hashlib.sha256(current.read_bytes()).hexdigest() != update['sha256']:
            raise ValueError('Canonical documentation digest differs: ' + str(target))
        resolved[old] = current
    return resolved


def module_destinations(targets, moves):
    """Only recorded documentation leaves the module; every other file stays covered."""
    targets = set(targets)
    if not set(moves) <= targets:
        raise ValueError('Documentation migration references an unknown historical destination')
    return targets - set(moves)
