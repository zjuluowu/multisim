#!/usr/bin/env python3
"""Incremental GN/Ninja entry for the replacement fixture; no legacy runner imports."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys

TEST = Path(__file__).resolve().parents[1]
REPO = TEST.parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=REPO / '.host-out')
    parser.add_argument('--gtest-root', type=Path, default=os.environ.get('SIM_GTEST_ROOT'))
    parser.add_argument('--jobs', type=int, default=3)
    parser.add_argument('--target', default='sim_test_host', choices=[
        'sim_test_host', 'sim_component_test', 'sim_primary_slot_entry_test',
        'sim_primary_slot_real_test', 'sim_g2a_query_test', 'sim_upstream_query_test'])
    parser.add_argument('--openssl-root', type=Path)
    parser.add_argument('--runtime-root', type=Path)
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine() != 'x86_64' or args.jobs < 1:
        parser.error('Requires Linux x86_64 and a positive --jobs value')
    output = args.out.resolve()
    if output == REPO or output in REPO.parents or output == TEST or TEST in output.parents:
        parser.error('Choose a dedicated build directory outside the test source')
    output.mkdir(parents=True, exist_ok=True)
    config = output / 'replacement.json'
    previous = json.loads(config.read_text()) if config.exists() else {}
    root = args.gtest_root or previous.get('gtest_root')
    if not root:
        parser.error('Set SIM_GTEST_ROOT or --gtest-root to the pinned GoogleTest source')
    root = Path(root).resolve()
    digest = hashlib.sha256()
    for path in sorted(root.rglob('*')):
        if path.is_file():
            digest.update(str(path.relative_to(root)).encode() + b'\0' + path.read_bytes() + b'\0')
    lock = json.loads((TEST / 'framework/host/configs/dependency_lock.json').read_text())
    if digest.hexdigest() != lock['googletest']['tree_sha256']:
        parser.error('GoogleTest source differs from dependency_lock.json')
    for row in lock['platform_headers']:
        if hashlib.sha256((TEST / 'framework/host' / row['path']).read_bytes()).hexdigest() != row['sha256']:
            parser.error('Pinned platform declaration changed: ' + row['path'])
    values = {'gtest_root': str(root), 'production_root': str(REPO / 'telephony_core_service'),
              'host_debug': True,
              'openssl_root': str(args.openssl_root.resolve()) if args.openssl_root else previous.get('openssl_root', ''),
              'sim028_runtime_root': str(args.runtime_root.resolve()) if args.runtime_root else previous.get('sim028_runtime_root', '')}
    if args.target == 'sim_primary_slot_real_test':
        openssl = Path(values['openssl_root'])
        runtime = Path(values['sim028_runtime_root'])
        metadata = openssl / 'sim_dependency.json'
        if not metadata.is_file() or json.loads(metadata.read_text()) != lock['openssl_headers']:
            parser.error('Real Primary requires pinned --openssl-root prepared by prepare_openssl.py')
        for name, expected in lock['openssl_headers']['header_sha256'].items():
            if hashlib.sha256((openssl / name).read_bytes()).hexdigest() != expected:
                parser.error('OpenSSL header drift: ' + name)
        if not (runtime / 'runtime_dependencies.json').is_file():
            parser.error('Real Primary requires --runtime-root prepared by prepare_runtime.py')
        for name, expected in json.loads((runtime / 'runtime_dependencies.json').read_text())['archives'].items():
            if hashlib.sha256((runtime / name).read_bytes()).hexdigest() != expected:
                parser.error('Native archive drift: ' + name)
    subprocess.run([sys.executable, '-B', str(TEST / 'scripts/update_case_sources.py')], check=True)
    gnargs = ' '.join(key + '=' + json.dumps(value) for key, value in values.items())
    subprocess.run(['gn', 'gen', str(output / 'build'), '--root=' + str(TEST / 'framework/host'),
                    '--export-compile-commands', '--args=' + gnargs], check=True)
    config.write_text(json.dumps(values, indent=2) + '\n')
    subprocess.run(['ninja', '-C', str(output / 'build'), '-j', str(args.jobs), args.target], check=True)


if __name__ == '__main__':
    main()
