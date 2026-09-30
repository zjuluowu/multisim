#!/usr/bin/env python3
"""Generate boundary declarations from production signatures, never legacy mocks."""
import argparse
import hashlib
from pathlib import Path
import re

SYSTEM = Path(__file__).resolve().parents[1]
CORE = SYSTEM.parents[1]

def split_params(text):
    parts, start, depth = [], 0, 0
    for index, char in enumerate(text):
        if char in '<([': depth += 1
        elif char in '>)]': depth -= 1
        elif char == ',' and depth == 0:
            parts.append(text[start:index])
            start = index + 1
    if text.strip(): parts.append(text[start:])
    return [part.strip() for part in parts]

def generate():
    source = CORE / 'interfaces/innerkits/include/i_tel_ril_manager.h'
    original = source.read_bytes()
    text = re.sub(r'/\*.*?\*/|//[^\n]*', '', original.decode(), flags=re.S)
    methods = re.findall(r'virtual\s+(\w+)\s+(\w+)\s*\((.*?)\)\s*=\s*0\s*;', text, re.S)
    if len(re.findall(r'virtual', text)) != len(methods) + 1:
        raise ValueError('Unparsed virtual declaration')
    lines = ['// Generated from current production ITelRilManager; no test implementation copied.',
             '// Interface SHA256: ' + hashlib.sha256(original).hexdigest(),
             '#pragma once', '#include <gmock/gmock.h>', '#include "i_tel_ril_manager.h"',
             '#include "telephony_errors.h"', 'namespace OHOS::Telephony {',
             'class MockTelRilManager : public ITelRilManager {', 'public:']
    defaults = []
    for returns, name, args in methods:
        if returns not in ['bool', 'int32_t']:
            raise ValueError('Unsupported RIL return type')
        types = []
        for arg in split_params(re.sub(r'\s+', ' ', args).strip()):
            match = re.fullmatch(r'(.+?)[\s*&]?(\w+)', arg)
            if not match: raise ValueError('Unsupported argument: ' + arg)
            type_name = re.sub(r'\b\w+$', '', arg).strip()
            if not type_name: raise ValueError('Missing parameter type')
            types.append('(' + type_name + ')' if ',' in type_name else type_name)
        lines.append('    MOCK_METHOD(' + returns + ', ' + name + ', (' + ', '.join(types) + '), (override));')
        action = 'false' if returns == 'bool' else 'TELEPHONY_ERR_RIL_CMD_FAIL'
        defaults.append('        ON_CALL(*this, ' + name + '(' + ', '.join(['testing::_'] * len(types)) + ')).WillByDefault(testing::Return(' + action + '));')
    lines += ['    MockTelRilManager()', '    {'] + defaults + ['    }', '};', '}']
    return '\n'.join(lines) + '\n'

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    options = parser.parse_args()
    destination = SYSTEM / 'stubs/mock_tel_ril_manager.h'
    content = generate()
    if options.check:
        if not destination.exists() or destination.read_text() != content:
            raise SystemExit('RIL mock signatures are stale; regenerate explicitly')
    else:
        destination.write_text(content)
