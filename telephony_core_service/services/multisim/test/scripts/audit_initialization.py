#!/usr/bin/env python3
"""Compile/link the optional initialization dependency probe; never execute it."""

import sys as _sys
from pathlib import Path as _Path
_TEST_ROOT = next(p for p in _Path(__file__).resolve().parents if (p / "scripts/_paths.py").is_file())
_sys.path.insert(0, str(_TEST_ROOT / "scripts"))
from _paths import TEST as _TEST, CORE as _CORE, REPO as _REPO, locate, guard_asset
from build_clock import await_build_timestamps
import argparse
import hashlib
import fcntl
import json
import os
from pathlib import Path
import platform
import re
import signal
import subprocess
import time

SYSTEM = (_TEST / 'framework/host')
CORE = _CORE

def digest_tree(root):
    digest = hashlib.sha256()
    for path in sorted(root.rglob('*')):
        if path.is_file():
            digest.update(str(path.relative_to(root)).encode() + b'\0' + path.read_bytes() + b'\0')
    return digest.hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gtest-root', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--openssl-root')
    parser.add_argument('--runtime-root')
    parser.add_argument('--libxml2-include', default=os.environ.get('SIM_LIBXML2_INCLUDE', '/usr/include/libxml2'),
                        help='libxml2 headers directory; defaults to SIM_LIBXML2_INCLUDE or the standard Linux location')
    parser.add_argument('--target', choices=['sim_initialization_link_probe', 'sim_bootstrap_characterization', 'sim_primary_slot_real_test'], default='sim_initialization_link_probe')
    options = parser.parse_args()
    output = Path(options.output).resolve()
    if output == SYSTEM or SYSTEM in output.parents:
        parser.error('Output must be outside systemtest')
    output.mkdir(parents=True, exist_ok=True)
    lock_stream = (output / ".sim-build.lock").open("w")
    fcntl.flock(lock_stream, fcntl.LOCK_EX)
    result = {
        'schema_version': 1, 'purpose': 'initialization_dependency_audit_not_test_execution',
        'status': 'BLOCKED', 'executed': False, 'gtest_tests': 0,
        'case_ids': ['SIM-B-002', 'SIM-B-003', 'SIM-B-004', 'SIM-B-005'],
        'level': 'B', 'environment': {'os': platform.platform(), 'target_os': 'linux'},
        'commands': [], 'missing_headers': [], 'compiler_diagnostics': [],
        'production_target': 'sim_initialization_production_host',
        'cleanup': 'No executable started; no persistence or card state modified',
    }
    if options.target == 'sim_bootstrap_characterization':
        result.update({'case_ids': ['SIM-CHAR-001'], 'level': 'C', 'production_target': 'sim_production_host', 'purpose': 'bootstrap_characterization_link_audit_not_execution'})
    def run(argv, name):
        start = time.monotonic()
        with (output / (name + '.log')).open('w') as log:
            process = subprocess.Popen(argv, cwd=output, stdout=log, stderr=log, start_new_session=True)
            try:
                code = process.wait(timeout=300)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                code = 124
        result['commands'].append({'argv': argv, 'exit_code': code,
                                   'elapsed_seconds': time.monotonic() - start, 'log': name + '.log'})
        return code
    try:
        result['head_sha'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=CORE, text=True).strip()
        result['harness_digest'] = digest_tree(SYSTEM)
        gtest = Path(options.gtest_root).resolve()
        lock = json.loads(((_TEST / 'framework/host/configs/dependency_lock.json')).read_text())
        provider = (_TEST / 'cases/primary_slot/provider')
        provider_contract = json.loads((provider / 'contract.json').read_text())
        for entry in provider_contract['source_files']:
            candidate = provider / 'include' / Path(entry['path']).name
            if entry['path'].endswith('.h') and entry['path'].startswith(('interfaces/', 'common/include/')) and (not candidate.exists() or hashlib.sha256(candidate.read_bytes()).hexdigest() != entry['sha256']):
                raise RuntimeError('Pinned Provider header differs: ' + entry['path'])
        result['provider_contract'] = provider_contract
        if digest_tree(gtest) != lock['googletest']['tree_sha256']:
            raise RuntimeError('GoogleTest tree differs from pinned source')
        for header in lock['platform_headers']:
            if hashlib.sha256((SYSTEM / header['path']).read_bytes()).hexdigest() != header['sha256']:
                raise RuntimeError('Platform header differs from pinned source: ' + header['path'])
        result['external_contract_manifest_digest'] = hashlib.sha256(
            ((_TEST / 'framework/host/configs/upstream_contracts.json')).read_bytes()).hexdigest()
        manifest = json.loads(((_TEST / 'framework/host/configs/production_sources_candidate.json')).read_text())
        names = manifest['unconditional_sim_sources'] if options.target != 'sim_bootstrap_characterization' else ['services/sim/src/sim_manager.cpp', 'services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp']
        result['production_sources'] = [
            {'path': name, 'sha256': hashlib.sha256((CORE / name).read_bytes()).hexdigest()}
            for name in names]
        result['toolchain'] = {'googletest_googlemock': lock['googletest']['commit']}
        for tool in ['gn', 'ninja', 'clang++', 'ld.gold']:
            result['toolchain'][tool] = subprocess.check_output([tool, '--version'], text=True).splitlines()[0]
        result['build_clock'] = await_build_timestamps(SYSTEM)
        result['blocking_stage'] = 'gn_generation'
        gn_args = 'production_root=' + json.dumps(str(CORE)) + ' gtest_root=' + json.dumps(str(gtest))
        gn_args += ' libxml2_include_dir=' + json.dumps(str(Path(options.libxml2_include).resolve()))
        if options.openssl_root:
            crypto_root = Path(options.openssl_root).resolve()
            expected = lock['openssl_headers']
            if json.loads((crypto_root / 'sim_dependency.json').read_text()) != expected:
                raise RuntimeError('OpenSSL dependency metadata differs from fixed package')
            for name, digest in expected.get('header_sha256', {}).items():
                if hashlib.sha256((crypto_root / name).read_bytes()).hexdigest() != digest:
                    raise RuntimeError('OpenSSL header differs from prepared fixed package')
            result['openssl_headers'] = expected
            gn_args += ' openssl_root=' + json.dumps(str(crypto_root))
        if options.target == 'sim_primary_slot_real_test':
            if not options.runtime_root:
                raise RuntimeError('Explicit --runtime-root is required for real SIM link audit')
            native = Path(options.runtime_root).resolve()
            native_contract = json.loads((native / 'runtime_dependencies.json').read_text())
            for name, digest in native_contract['archives'].items():
                if hashlib.sha256((native / name).read_bytes()).hexdigest() != digest:
                    raise RuntimeError('Native archive drift: ' + name)
            result['native_dependencies'] = native_contract
            gn_args += ' sim028_runtime_root=' + json.dumps(str(native))
        code = run(['gn', 'gen', str(output), '--root=' + str(SYSTEM), '--args=' + gn_args], 'gn-gen')
        if code:
            raise RuntimeError('Initialization probe GN generation failed')
        result['blocking_stage'] = 'production_compile_or_link'
        code = run(['ninja', '-C', str(output), '-k', '0', '-j', '4', options.target], 'ninja-initialization')
        result['build_exit_code'] = code
        diagnostics = (output / 'ninja-initialization.log').read_text()
        result['missing_headers'] = sorted(set(re.findall(r"fatal error: '([^']+)' file not found", diagnostics)))
        result['compiler_diagnostics'] = sorted(set(re.findall(r'^.*?:\d+:\d+: (?:fatal )?error: .+$', diagnostics, re.M)))
        result['compiled_objects'] = len(list((output / 'obj').rglob('sim_initialization_production_host.*.o')))
        if code:
            raise RuntimeError('Production dependency closure does not compile/link for ' + options.target + '; no scenario executed')
        result['status'] = 'PASS'
        result['blocking_stage'] = None
        result['runtime_status'] = 'NOT_RUN'
        result['reason'] = 'Link audit only; run run_primary_real.py separately for runtime evidence'
    except Exception as error:
        result['reason'] = str(error)
    (output / 'initialization_audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({key: result.get(key) for key in ['status', 'executed', 'build_exit_code', 'compiled_objects', 'missing_headers', 'reason']}))
    return 0 if result['status'] == 'PASS' else 1

if __name__ == '__main__':
    raise SystemExit(main())
