#!/usr/bin/env python3
"""SIM-028 CI over the existing GN root. Production patches apply only in isolation."""
import argparse
import concurrent.futures
import copy
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
import zipfile

SYSTEM = Path(__file__).resolve().parents[1]
ROOT = SYSTEM.parents[3].parent
ADMISSION = SYSTEM / 'primary_slot/lifecycle/admission'
POLICY_PATH = SYSTEM / 'configs/sim028_ci.json'
POLICY = json.loads(POLICY_PATH.read_text())
REAL = POLICY['real_target']


def sha(data):
    return hashlib.sha256(data).hexdigest()


def save(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')


def normalize(message):
    return re.sub(r'^[^\n]*?telephony_core_service/', 'telephony_core_service/', message, flags=re.M)


def command(argv, log, cwd, env=None, timeout=900):
    started = time.monotonic()
    with log.open('w') as output:
        process = subprocess.Popen(list(map(str, argv)), cwd=cwd, env=env, stdout=output,
                                   stderr=subprocess.STDOUT, start_new_session=True)
        try:
            code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
            output.write('\nCI_TIMEOUT (failure; no cleanup success inferred)\n')
            code = 124
    return {'command': list(map(str, argv)), 'exit_code': code,
            'seconds': time.monotonic() - started, 'log': str(log)}


def source_files(root):
    names = subprocess.check_output(['git', 'ls-files', '-c', '-o', '--exclude-standard', '-z'],
                                    cwd=root).split(b'\0')
    return sorted({n.decode() for n in names if n and (root / n.decode()).is_file()})


def source_input_hashes(root, names):
    # Compile/runner inputs, excluding evidence and prose. All actual inputs are
    # still overlaid and their raw hashes recorded when creating the snapshot.
    chosen = {}
    for name in names:
        p = Path(name)
        if not name.startswith('telephony_core_service/'):
            continue
        if name in (str(Path(__file__).relative_to(ROOT)), str((SYSTEM / 'tools/test_ci_sim028.py').relative_to(ROOT))):
            continue  # Orchestrator provenance is recorded separately from compiled inputs.
        if name == str((ADMISSION / 'review.json').relative_to(ROOT)):
            continue
        if '/configs/' in name and name not in (
                str(POLICY_PATH.relative_to(ROOT)),
                str((SYSTEM / 'configs/dependency_lock.json').relative_to(ROOT)),
                str((SYSTEM / 'configs/infrastructure_cases.json').relative_to(ROOT))):
            continue
        if p.suffix in ('.cpp', '.h', '.gn', '.gni', '.py', '.sh', '.patch', '.json') or name == str(POLICY_PATH.relative_to(ROOT)):
            chosen[name] = sha((root / name).read_bytes())
    return chosen


def source_signature(root, names):
    return sha(json.dumps(source_input_hashes(root, names), sort_keys=True).encode())


def prepare(root, out, increments=(), advance=False):
    names = source_files(root)
    signature = source_signature(root, names)
    increment_records = [{"path": str(p.resolve()), "sha256": sha(p.read_bytes())} for p in increments]
    marker = out / 'isolation.json'
    if marker.exists():
        old = json.loads(marker.read_text())
        if advance:
            # A candidate revision must prove the previous source untouched,
            # reconstruct the new pinned candidate independently, then preserve
            # unchanged source mtimes so Ninja can rebuild only the real delta.
            for name, expected in old['snapshot_hashes'].items():
                p = out / 'repo' / name
                if not p.is_file() or sha(p.read_bytes()) != expected:
                    raise RuntimeError('Cannot advance a modified isolated source: ' + name)
            stage = out / ('candidate-input-' + str(time.time_ns()))
            stage.mkdir()
            fresh_repo = prepare(root, stage, increments)
            fresh = json.loads((stage / 'isolation.json').read_text())
            if set(old['snapshot_hashes']) - set(fresh['snapshot_hashes']):
                raise RuntimeError('Candidate file removal requires a fresh --out')
            changed = []
            for name, expected in fresh['snapshot_hashes'].items():
                p = out / 'repo' / name
                if not p.is_file() or sha(p.read_bytes()) != expected:
                    p.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(fresh_repo / name, p)
                    if sha(p.read_bytes()) != expected:
                        raise RuntimeError('Candidate advance copy mismatch: ' + name)
                    changed.append(name)
            epoch = str(time.time_ns())
            shutil.copy2(marker, out / ('isolation-' + epoch + '.json'))
            save(out / ('advance-' + epoch + '.json'), {
                'old_marker_sha256': sha(marker.read_bytes()),
                'new_marker_sha256': sha((stage / 'isolation.json').read_bytes()),
                'changed_files': changed, 'fresh_snapshot': str(stage)})
            if (out / 'ci-artifacts.zip').exists():
                shutil.copy2(out / 'ci-artifacts.zip', out / ('ci-artifacts-' + epoch + '.zip'))
            shutil.copy2(stage / 'isolation.json', marker)
            return out / 'repo'
        if old.get('increments', []) != increment_records:
            raise RuntimeError('Candidate increments changed: choose a new --out or explicitly --advance-candidate')
        current_inputs = source_input_hashes(root, names)
        unchanged_inputs = all(old['actual_input_hashes'].get(n) == h for n, h in current_inputs.items())
        old_relevant = set(source_input_hashes(out / 'repo', old['actual_input_hashes']))
        if not unchanged_inputs or set(current_inputs) != old_relevant or old['source_root'] != str(root):
            raise RuntimeError('Source changed: choose a new --out; existing evidence is never overwritten')
        # Detect accidental modifications to the isolated source before reusing a build.
        for name, expected in old['snapshot_hashes'].items():
            if sha((out / 'repo' / name).read_bytes()) != expected:
                raise RuntimeError('Isolated source drift: ' + name)
        return out / 'repo'
    if (out / 'repo').exists():
        raise RuntimeError('Refuse unmarked existing repo directory')
    manifest = json.loads((ADMISSION / 'patch-manifest.json').read_text())['files']
    for row in manifest:
        p = root / row['path']
        if row['added']:
            if p.exists():
                raise RuntimeError('Candidate already applied in source: ' + row['path'])
        elif sha(p.read_bytes().replace(b'\r\n', b'\n')) != row['base_sha256']:
            raise RuntimeError('Production/test baseline drift: ' + row['path'])
    repo = out / 'repo'
    subprocess.run(['git', 'clone', '--no-hardlinks', '--quiet', str(root), str(repo)], check=True)
    raw_hashes = {}
    for name in names:
        target = repo / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(root / name, target)
        raw_hashes[name] = sha((root / name).read_bytes())
        if sha(target.read_bytes()) != raw_hashes[name]:
            raise RuntimeError('Actual-input overlay mismatch: ' + name)
    for row in manifest:
        if not row['added']:
            p = repo / row['path']
            p.write_bytes(p.read_bytes().replace(b'\r\n', b'\n'))
    for name in ('production-minimal.patch', 'test-guard.patch'):
        patch = ADMISSION / name
        subprocess.run(['git', 'apply', '--check', str(patch)], cwd=repo, check=True)
        subprocess.run(['git', 'apply', str(patch)], cwd=repo, check=True)
    for row in manifest:
        if sha((repo / row['path']).read_bytes()) != row['candidate_sha256']:
            raise RuntimeError('Candidate hash mismatch: ' + row['path'])
    increment_paths = set()
    for patch in increments:
        subprocess.run(['git', 'apply', '--check', str(patch.resolve())], cwd=repo, check=True)
        paths = subprocess.check_output(['git', 'apply', '--numstat', str(patch.resolve())], cwd=repo, text=True)
        increment_paths.update(line.split('\t', 2)[2] for line in paths.splitlines())
        subprocess.run(['git', 'apply', str(patch.resolve())], cwd=repo, check=True)
    snapshot_hashes = {name: sha((repo / name).read_bytes()) for name in names}
    snapshot_hashes.update({r['path']: sha((repo / r['path']).read_bytes()) for r in manifest})
    snapshot_hashes.update({n: sha((repo / n).read_bytes()) for n in increment_paths})
    save(marker, {'increments': increment_records, 'source_root': str(root), 'source_signature': signature,
                  'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
                  'actual_input_hashes': raw_hashes, 'snapshot_hashes': snapshot_hashes,
                  'patches': {n: sha((ADMISSION / n).read_bytes()) for n in ('production-minimal.patch', 'test-guard.patch')}})
    return repo


def test_env(directory, real, sanitizer=''):
    env = {k: v for k, v in os.environ.items() if not k.startswith(('SIM_', 'GTEST_'))}
    env.update(LLVM_PROFILE_FILE=str(directory / 'current.profraw'), GTEST_COLOR='no',
               ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:exitcode=86',
               LSAN_OPTIONS='', TSAN_OPTIONS='halt_on_error=1:exitcode=87')
    if real:
        env.update(SIM_HOST_BOOT_SLOT_COUNT='2', SIM_HOST_STRONG_RIL_SUBSCRIPTIONS='1')
    return env


def parse_discovery(text):
    suite, names = '', []
    for line in text.splitlines():
        line = line.split('#')[0].rstrip()
        if line.startswith('  ') and suite:
            names.append(suite + line.strip())
        elif line and not line.startswith(' ') and line.endswith('.'):
            suite = line
    return names


def discovery(binary, directory, real):
    env = test_env(directory, real)
    result = command([binary, '--gtest_list_tests'], directory / (binary.name + '-discovery.log'), directory, env)
    if result['exit_code']:
        raise RuntimeError('Discovery failed: ' + str(binary))
    names = parse_discovery(Path(result['log']).read_text())
    if not names or len(names) != len(set(names)):
        raise RuntimeError('Empty/duplicate discovery: ' + str(binary))
    return names


def read_case(path, requested, process_code):
    row = {'name': requested, 'exit_code': process_code, 'status': 'FAIL', 'messages': [], 'valid_xml': False}
    try:
        cases = ET.parse(path).getroot().findall('.//testcase')
    except (ET.ParseError, OSError):
        return row
    if len(cases) != 1:
        return row
    t = cases[0]
    actual = t.get('classname', '') + '.' + t.get('name', '')
    skipped = bool(t.findall('skipped')) or t.get('status') == 'notrun' or t.get('result') == 'suppressed'
    row.update(valid_xml=actual == requested and not skipped, actual_name=actual, skipped=skipped,
               messages=[normalize(f.get('message', f.text or '')) for f in t.findall('failure')],
               properties={p.get('name'): p.get('value') for p in t.findall('./properties/property')})
    if row['valid_xml'] and not row['messages'] and process_code == 0:
        row['status'] = 'PASS'
    return row


def evaluate(rows, expected, known):
    reasons = []
    names = [r['name'] for r in rows]
    if len(names) != len(set(names)):
        reasons.append('duplicate_cases')
    if set(names) != set(expected):
        reasons.append('collection_mismatch')
    observed_known = []
    for r in rows:
        name = r['name']
        if not r.get('valid_xml') or r.get('skipped'):
            reasons.append('invalid_or_skipped:' + name)
        for key, value in POLICY.get('required_properties', {}).get(name, {}).items():
            if r.get('properties', {}).get(key) != value:
                reasons.append('metadata_mismatch:' + name + ':' + key)
        if name in known:
            observed_known.append(name)
            if (r['status'] != 'FAIL' or r['exit_code'] != 1 or
                    r.get('messages') != known[name]['failure_messages']):
                reasons.append('known_failure_changed:' + name)
        elif r['status'] != 'PASS' or r['exit_code'] != 0:
            reasons.append('new_regression:' + name)
    # This gate NEVER rewrites raw test statuses or XML failures.
    return {'status': 'FAIL' if reasons else 'PASS', 'reasons': reasons,
            'raw_test_status': 'FAIL' if any(r['status'] == 'FAIL' for r in rows) else 'PASS',
            'passed': sum(r['status'] == 'PASS' for r in rows),
            'failed': sum(r['status'] == 'FAIL' for r in rows),
            'known_failures': [copy.deepcopy(r) for r in rows if r['name'] in known]}


def is_lifecycle(name):
    return name.split('.')[0] in POLICY['lifecycle_suites']


def run_profile(profile, repo, out, args):
    directory = out / profile
    directory.mkdir(exist_ok=True)
    run = directory / ('run-' + str(time.time_ns()))
    run.mkdir()
    system = repo / SYSTEM.relative_to(ROOT)
    sanitizer = profile if profile in ('address', 'thread') else ''
    real_only = profile in ('address', 'thread', 'lifecycle')
    targets = [REAL] if real_only else list(POLICY['targets'])
    configure = [sys.executable, '-B', system / 'tools/host.py', 'configure', '--target', REAL,
                 '--with-real-primary', '--out', directory, '--gtest-root', args.gtest_root,
                 '--openssl-root', args.openssl_root, '--runtime-root', args.runtime_root,
                 '--sanitizer', sanitizer]
    result = {'profile': profile, 'sanitizer': sanitizer, 'run': str(run), 'cases': [], 'steps': [],
              'native_ohos': 'BLOCKED', 'admission': {'status': 'FAIL', 'reasons': ['not_executed']}}
    save(run / 'result.json', result)
    try:
        for cmd, log in [(configure, 'configure.log'),
                         (['ninja', '-C', directory / 'build', '-j', args.jobs, *targets], 'build.log')]:
            step = command(cmd, run / log, repo)
            result['steps'].append(step)
            if step['exit_code']:
                raise RuntimeError('configure/build failed; see ' + log)
        binary_hashes = {}
        discovered = {}
        expected = []
        for target in targets:
            binary = directory / 'build' / target
            binary_hashes[target] = sha(binary.read_bytes())
            names = discovery(binary, run, target == REAL)
            required = set(POLICY['targets'][target])
            if not required.issubset(names):
                raise RuntimeError('Required cases disappeared: ' + target)
            names = [n for n in names if not (profile == 'contracts' and is_lifecycle(n))
                     and not (profile == 'lifecycle' and not is_lifecycle(n))]
            discovered[target] = names
            expected.extend(names)
        result.update(binary_sha256=binary_hashes, discovery=discovered, expected=expected)
        for target, names in discovered.items():
            for name in names:
                index = len(result['cases'])
                xml = run / f'case-{index}.xml'
                step = command([directory / 'build' / target, '--gtest_filter=' + name,
                                '--gtest_output=xml:' + str(xml)], run / f'case-{index}.log', run,
                               test_env(run, target == REAL, sanitizer), args.timeout)
                row = read_case(xml, name, step['exit_code'])
                row.update(target=target, command=step['command'], seconds=step['seconds'], xml=xml.name)
                result['cases'].append(row)
                save(run / 'result.json', result)
        result['admission'] = evaluate(result['cases'], expected, POLICY['known_failures'])
        # Readable business/lifecycle separation without changing any raw FAIL.
        for label, selection in [('business_contract', lambda n: not is_lifecycle(n)),
                                 ('lifecycle_stop', is_lifecycle)]:
            rows = [r for r in result['cases'] if selection(r['name'])]
            result[label] = {'tests': len(rows), 'passed': sum(r['status'] == 'PASS' for r in rows),
                             'failed': sum(r['status'] == 'FAIL' for r in rows)}
    except (RuntimeError, subprocess.SubprocessError, OSError) as error:
        result['error'] = str(error)
        result['admission'] = {'status': 'FAIL', 'reasons': [str(error)]}
    save(run / 'result.json', result)
    # Aggregated JUnit preserves original <failure> elements and properties.
    aggregate = ET.Element('testsuites')
    for xml in sorted(run.glob('case-*.xml')):
        try:
            tree = ET.parse(xml).getroot()
            for suite in tree.findall('testsuite'):
                aggregate.append(copy.deepcopy(suite))
        except ET.ParseError:
            pass  # Invalid XML already failed the admission gate above.
    ET.ElementTree(aggregate).write(run / 'junit.xml', encoding='utf-8', xml_declaration=True)
    with zipfile.ZipFile(run / 'logs.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        for file in sorted(run.iterdir()):
            if file.suffix in ('.log', '.xml', '.json'):
                archive.write(file, file.name)
    save(directory / 'latest.json', result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True, help='Dedicated directory outside the source workspace')
    parser.add_argument('--profile', choices=['regular', 'contracts', 'lifecycle', 'address', 'thread', 'all'], default='all')
    parser.add_argument('--gtest-root', type=Path, required=True)
    parser.add_argument('--openssl-root', type=Path, required=True)
    parser.add_argument('--runtime-root', type=Path, required=True)
    parser.add_argument('--candidate-increment', type=Path, action='append', default=[],
                        help='Reviewed incremental patch, applied only after the pinned base candidate (repeatable)')
    parser.add_argument('--advance-candidate', action='store_true',
                        help='Under the output lock, verify old source and independently reconstruct a revised candidate before incremental reuse')
    parser.add_argument('--jobs', default='2')
    parser.add_argument('--timeout', type=int, default=120)
    parser.add_argument('--parallel-profiles', action='store_true')
    args = parser.parse_args()
    out = args.out.resolve()
    if out == ROOT or ROOT in out.parents or out in ROOT.parents:
        parser.error('--out must be outside the source workspace')
    if out.exists() and not (out / 'ci.lock').exists() and any(out.iterdir()):
        parser.error('Refuse nonempty unmarked --out')
    out.mkdir(parents=True, exist_ok=True)
    with (out / 'ci.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        result = {'profiles': [], 'admission': 'FAIL', 'native_production_admission': 'BLOCKED',
                  'policy_sha256': sha(POLICY_PATH.read_bytes()),
                  'runner_sha256': sha(Path(__file__).read_bytes())}
        try:
            repo = prepare(ROOT, out, args.candidate_increment, args.advance_candidate)
            profiles = ['regular', 'address', 'thread'] if args.profile == 'all' else [args.profile]
            if args.parallel_profiles:
                with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
                    result['profiles'] = list(pool.map(lambda p: run_profile(p, repo, out, args), profiles))
            else:
                result['profiles'] = [run_profile(p, repo, out, args) for p in profiles]
            result['admission'] = 'PASS' if all(r['admission']['status'] == 'PASS' for r in result['profiles']) else 'FAIL'
        except (RuntimeError, OSError, subprocess.SubprocessError) as error:
            result['error'] = str(error)
        save(out / 'result.json', result)
        with zipfile.ZipFile(out / 'ci-artifacts.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
            for name in ['result.json', 'isolation.json']:
                if (out / name).exists(): archive.write(out / name, name)
            for index, patch in enumerate(args.candidate_increment):
                archive.write(patch, f'increments/{index}-' + patch.name)
            archive.write(POLICY_PATH, 'sim028_ci.json')
            archive.write(ADMISSION / 'patch-manifest.json', 'patch-manifest.json')
            for profile in result['profiles']:
                logs = Path(profile['run']) / 'logs.zip'
                archive.write(logs, profile['profile'] + '/logs.zip')
        print(json.dumps({'admission': result['admission'], 'profiles': [
            {'profile': r['profile'], **r['admission']} for r in result['profiles']]}, ensure_ascii=False, indent=2))
        return int(result['admission'] != 'PASS')


if __name__ == '__main__':
    sys.exit(main())
