#!/usr/bin/env python3
"""Reproduce the path-only GN comparison against the frozen Git input."""
import hashlib
import json
from pathlib import Path
import posixpath
import re
import subprocess
import sys

root = next(p for p in Path(__file__).resolve().parents if (p / 'docs/Test_Architecture_Migration.json').is_file())
core, repo = root.parents[2], root.parents[3]
sys.path.insert(0, str(root / 'scripts'))
from run_g2a_query import gn_tokens
manifest = json.loads((root / 'docs/Test_Architecture_Migration.json').read_text())
moves = {r['old']: r['new'] for r in manifest['files']}
dirs = {'systemtest': 'framework/host', 'sim_guard': 'cases/compatibility',
        'systemtest/support': 'cases/host/support', 'systemtest/primary_slot': 'cases/primary_slot'}


def mapped(name):
    if name in moves:
        return moves[name]
    if name in dirs:
        return dirs[name]
    for old, new in moves.items():
        if old.startswith(name + '/'):
            suffix = old[len(name):]
            if new.endswith(suffix):
                return new[:-len(suffix)]
    return None


def relocate_literal(match):
    value = match.group(1)
    if not value:
        return match.group(0)
    prefix = '$production_root/services/multisim/test/'
    if value.startswith(prefix):
        target = mapped(value[len(prefix):])
    elif '$' in value or value.startswith((':', '//')):
        return match.group(0)
    else:
        target = mapped(posixpath.normpath('systemtest/' + value))
    return '"$test_root/' + target + '"' if target else match.group(0)


def blob(name):
    return subprocess.check_output(['git', 'show', manifest['baseline'] + ':' + name], cwd=repo)


before = blob('telephony_core_service/services/multisim/test/systemtest/BUILD.gn')
expected = 'test_root = "$production_root/services/multisim/test"\n' + re.sub(r'"([^"\n]*)"', relocate_literal, before.decode())
expected = expected.replace('"$test_root/cases/compatibility",', '"$test_root/cases/compatibility",\n    "$test_root/framework/compatibility",')
actual = (root / 'framework/host/BUILD.gn').read_text()
if gn_tokens(expected) != gn_tokens(actual):
    raise RuntimeError('Host GN changed beyond exact test paths and the split include root')
production_before = blob('telephony_core_service/services/multisim/BUILD.gn')
production_after = (core / 'services/multisim/BUILD.gn').read_bytes()
if production_before.replace(b'/test/multisim_architecture_rules', b'/test/cases/architecture') != production_after:
    raise RuntimeError('Production GN changed beyond the test visibility label')
print(json.dumps({'status': 'PASS', 'host_gn_policy': 'exact tokens after declared paths and split include root',
                  'production_gn': 'exact bytes after test visibility label',
                  'host_before_sha256': hashlib.sha256(before).hexdigest(),
                  'host_after_sha256': hashlib.sha256(actual.encode()).hexdigest()}, indent=2))
