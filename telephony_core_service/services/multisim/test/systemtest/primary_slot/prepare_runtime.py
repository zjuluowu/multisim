#!/usr/bin/env python3
"""Build real C runtime dependencies from explicit local source, without installation."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source-cache', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
roots = {name: args.source_cache.resolve() / name for name in [
    'third_party_bounds_checking_function', 'third_party_cJSON', 'commonlibrary_c_utils']}
record = {'sources': {}, 'commands': [], 'inputs': {}}
expected = {
    'third_party_bounds_checking_function': '61baa7a3eb9347efca58397d1d0504faa96d79f0',
    'third_party_cJSON': '28dba43de56de279d1b5b74565f4277c89c50f35',
    'commonlibrary_c_utils': '3bdc11e00e7bf6e577cee300ad51045ca9958efd',
}
for name, root in roots.items():
    head = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
    if subprocess.check_output(['git', '-C', str(root), 'status', '--porcelain', '--untracked-files=no'], text=True).strip():
        raise SystemExit('Dependency has tracked modifications: ' + name)
    if head != expected[name]:
        raise SystemExit('Dependency revision mismatch: ' + name)
    remote = subprocess.check_output(['git', '-C', str(root), 'remote', 'get-url', 'origin'], text=True).strip()
    record['sources'][name] = {'head': head, 'root': str(root), 'repository': remote}

def run(argv):
    for arg in argv:
        if isinstance(arg, Path) and arg.suffix in ('.c', '.cpp'):
            record['inputs'][str(arg)] = hashlib.sha256(arg.read_bytes()).hexdigest()
    subprocess.run(list(map(str, argv)), check=True)
    record['commands'].append(list(map(str, argv)))

secure = roots['third_party_bounds_checking_function']
objects = []
for name in subprocess.check_output(['git', '-C', str(secure), 'ls-files', 'src/*.c'], text=True).splitlines():
    p = secure / name
    obj = out / (p.stem + '.o')
    run(['clang', '-O2', '-fPIC', '-I' + str(secure / 'include'), '-c', p, '-o', obj])
    objects.append(obj)
run(['ar', 'rcs', out / 'libsim028_securec.a', *objects])
cjson = roots['third_party_cJSON']
run(['clang', '-O2', '-fPIC', '-I' + str(cjson), '-c', cjson / 'cJSON.c', '-o', out / 'cJSON.o'])
run(['ar', 'rcs', out / 'libsim028_cjson.a', out / 'cJSON.o'])
cutils = roots['commonlibrary_c_utils']
run(['clang++', '-std=c++17', '-O2', '-fPIC', '-I' + str(cutils / 'base/include'),
     '-I' + str(secure / 'include'), '-c', cutils / 'base/src/string_ex.cpp', '-o', out / 'string_ex.o'])
run(['clang++', '-std=c++17', '-O2', '-fPIC', '-include', 'cstdint', '-I' + str(cutils / 'base/include'), '-c', cutils / 'base/src/unicode_ex.cpp', '-o', out / 'unicode_ex.o'])
run(['ar', 'rcs', out / 'libsim028_strings.a', out / 'string_ex.o', out / 'unicode_ex.o'])
record['archives'] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in out.glob('*.a')}
(out / 'runtime_dependencies.json').write_text(json.dumps(record, indent=2) + '\n')
print(out / 'runtime_dependencies.json')
