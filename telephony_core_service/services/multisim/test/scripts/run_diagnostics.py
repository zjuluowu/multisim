#!/usr/bin/env python3
"""Trace the original failing scenario with read-only LLDB; propagate test failure."""

import sys as _sys
from pathlib import Path as _Path
_TEST_ROOT = next(p for p in _Path(__file__).resolve().parents if (p / "scripts/_paths.py").is_file())
_sys.path.insert(0, str(_TEST_ROOT / "scripts"))
from _paths import TEST as _TEST, CORE as _CORE, REPO as _REPO, locate, guard_asset
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET
from summarize_graph import summarize

HERE = (_TEST / 'cases/primary_slot/lifecycle')
REPO = _REPO
CASE = 'RealSimPrimary.C1ThreePhasesWithStaleCheckResponseRejected'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=REPO / '.host-out/build/sim_primary_slot_real_test')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    commands = out / 'trace.lldb'
    commands.write_text('settings set target.disable-aslr false\ncommand script import "' +
                        str((_TEST / 'scripts/trace_graph.py')) + '"\nrun\n')
    argv = ['lldb', '--batch', '--source', str(commands), '--', str(binary),
            '--gtest_filter=' + CASE, '--gtest_output=xml:' + str(out / 'test.xml')]
    env = dict(os.environ, SIM_HOST_BOOT_SLOT_COUNT='2', SIM_LIFE_GRAPH=str(out / 'graph.json'),
               LLVM_PROFILE_FILE=str(out / 'trace-%p.profraw'))
    with (out / 'trace.log').open('w') as stream:
        result = subprocess.run(argv, env=env, cwd=out, stdout=stream, stderr=subprocess.STDOUT, timeout=120)
    exits = re.findall(r'Process \d+ exited with status = (\d+)', (out / 'trace.log').read_text())
    inferior_exit = int(exits[-1]) if exits else None
    report = {'command': argv, 'lldb_exit': result.returncode, 'inferior_exit': inferior_exit,
              'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
              'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(),
              'status': 'BLOCKED'}
    if (out / 'graph.json').exists() and (out / 'test.xml').exists():
        graph = json.loads((out / 'graph.json').read_text())
        cases = ET.parse(out / 'test.xml').getroot().findall('.//testcase')
        failures = sum(bool(c.findall('failure')) for c in cases)
        snapshots = graph['snapshots']
        complete = (not graph['errors'] and len(snapshots) >= 3 and
                    any(any(c['type'].endswith('ITelRilManager') and c['use_count'] == 10
                            for c in s.get('retained_control_blocks', [])) for s in snapshots))
        if complete:
            proof = summarize(graph)
            (out / 'ownership-proof.json').write_text(json.dumps(proof, indent=2) + '\n')
            complete = proof['status'] == 'PASS'
        report.update(executed=len(cases), failed=failures,
                      diagnostics='PASS' if complete else 'FAIL',
                      status=('FAIL' if failures or inferior_exit else 'PASS') if complete and result.returncode == 0 and inferior_exit is not None else 'BLOCKED')
    (out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return {'PASS': 0, 'FAIL': 1, 'BLOCKED': 2}[report['status']]


if __name__ == '__main__':
    raise SystemExit(main())
