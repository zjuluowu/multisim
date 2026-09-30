#!/usr/bin/env python3
"""Compile/link the optional initialization dependency probe; never execute it."""
from build_clock import await_build_timestamps
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import signal
import subprocess
import time

SYSTEM = Path(__file__).resolve().parents[1]
CORE = SYSTEM.parents[1]

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
    parser.add_argument('--target', choices=['sim_initialization_link_probe', 'sim_bootstrap_characterization'], default='sim_initialization_link_probe')
    options = parser.parse_args()
    output = Path(options.output).resolve()
    if output == SYSTEM or SYSTEM in output.parents:
        parser.error('Output must be outside systemtest')
    output.mkdir(parents=True, exist_ok=True)
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
                code = process.wait(timeout=180)
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
        lock = json.loads((SYSTEM / 'configs/dependency_lock.json').read_text())
        if digest_tree(gtest) != lock['googletest']['tree_sha256']:
            raise RuntimeError('GoogleTest tree differs from pinned source')
        manifest = json.loads((SYSTEM / 'configs/production_sources_candidate.json').read_text())
        names = manifest['unconditional_sim_sources'] if options.target == 'sim_initialization_link_probe' else ['services/sim/src/sim_manager.cpp', 'services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp']
        result['production_sources'] = [
            {'path': name, 'sha256': hashlib.sha256((CORE / name).read_bytes()).hexdigest()}
            for name in names]
        result['toolchain'] = {'googletest_googlemock': lock['googletest']['commit']}
        for tool in ['gn', 'ninja', 'clang++', 'ld.gold']:
            result['toolchain'][tool] = subprocess.check_output([tool, '--version'], text=True).splitlines()[0]
        result['build_clock'] = await_build_timestamps(SYSTEM)
        result['blocking_stage'] = 'gn_generation'
        gn_args = 'production_root=' + json.dumps(str(CORE)) + ' gtest_root=' + json.dumps(str(gtest))
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
        result['status'] = 'LINK_AUDIT_COMPLETE_RUNTIME_BLOCKED'
        result['blocking_stage'] = 'controlled_runtime_not_implemented'
        result['reason'] = 'Link only; RIL/platform assembly and scenario execution are still required'
    except Exception as error:
        result['reason'] = str(error)
    (output / 'initialization_audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({key: result.get(key) for key in ['status', 'executed', 'build_exit_code', 'compiled_objects', 'missing_headers', 'reason']}))
    return 0 if result['status'] == 'LINK_AUDIT_COMPLETE_RUNTIME_BLOCKED' else 1

if __name__ == '__main__':
    raise SystemExit(main())
