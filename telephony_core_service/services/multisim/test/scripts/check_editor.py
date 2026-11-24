#!/usr/bin/env python3
"""Check the GoogleTest XML locations consumed by the VS Code Testing panel."""
import argparse
import json
from pathlib import Path
import subprocess
from time import perf_counter
import xml.etree.ElementTree as ET

from build_replacement import LEGACY, REAL, TARGET
from run_gtest import host_environment


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--target', nargs='+', choices=LEGACY + TARGET, default=LEGACY + TARGET)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    build = args.out.resolve() / 'build'
    cases = []
    errors = []
    timing = []
    source_lines = {}
    for target in args.target:
        xml = output / (target + '.xml')
        env = host_environment(build / target, output / 'discovery-%p.profraw',
                               {'SIM_HOST_BOOT_SLOT_COUNT': '2', 'SIM028_INIT_MODEL': 'single-active'}
                               if target in REAL else {})
        started = perf_counter()
        processes = 0
        try:
            # Match TestMate: identify the framework, then enumerate with XML.
            processes += 1
            help_result = subprocess.run([str(build / target), '--help'], env=env,
                                         capture_output=True, text=True, timeout=30)
            if help_result.returncode or help_result.stderr or 'Google Test' not in help_result.stdout:
                raise ValueError('GoogleTest help not recognized')
            if xml.exists():
                xml.unlink()
            processes += 1
            result = subprocess.run([str(build / target), '--gtest_list_tests',
                                     '--gtest_output=xml:' + str(xml)], env=env,
                                    capture_output=True, text=True, timeout=60)
            (output / (target + '.log')).write_text(result.stdout + result.stderr)
            if result.returncode or result.stderr:
                raise ValueError('Enumeration failed or wrote stderr')
            tests = ET.parse(xml).getroot()
            count = 0
            for suite in tests.findall('testsuite'):
                for test in suite.findall('testcase'):
                    file = Path(test.get('file', ''))
                    line = int(test.get('line', '0'))
                    if not file.is_absolute() or not file.is_file():
                        raise ValueError('Unresolvable source: ' + str(file))
                    if file not in source_lines:
                        source_lines[file] = file.read_text().splitlines()
                    lines = source_lines[file]
                    if not 1 <= line <= len(lines):
                        raise ValueError('Invalid source line: ' + str(line))
                    cases.append(dict(target=target, name=suite.get('name') + '.' + test.get('name'),
                                      file=str(file), line=line, source=lines[line - 1]))
                    count += 1
            if not count:
                raise ValueError('No tests discovered')
        except (OSError, ValueError, ET.ParseError, subprocess.TimeoutExpired) as error:
            errors.append(dict(target=target, error=str(error)))
        finally:
            timing.append(dict(target=target, processes=processes, seconds=perf_counter()-started))
    by_source_file = {}
    for case in cases:
        by_source_file.setdefault(case['file'], []).append(
            dict(target=case['target'], name=case['name'], line=case['line']))
    report = dict(status='FAIL' if errors else 'PASS', cases=cases,
                  by_source_file=by_source_file, errors=errors, timings=timing)
    (output / 'locations.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f"Editor discovery: {report['status']}; {len(cases)} source locations; {errors}")
    return bool(errors)


if __name__ == '__main__':
    raise SystemExit(main())
