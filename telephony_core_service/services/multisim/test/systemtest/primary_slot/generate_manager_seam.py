#!/usr/bin/env python3
"""Generate fail-closed declarations at the explicitly partial ISimManager seam."""
import argparse
import hashlib
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
CORE = HERE.parents[4]


def generate():
    source = CORE / 'interfaces/innerkits/include/i_sim_manager.h'
    text = re.sub(r'/\*.*?\*/|//[^\n]*', '', source.read_text(), flags=re.S)
    methods = re.findall(r'virtual\s+([^;{}]+?)\s+(\w+)\s*\((.*?)\)\s*=\s*0\s*;', text, re.S)
    if len(methods) != text.count('virtual'):
        raise RuntimeError('Unparsed ISimManager declaration; inspect the seam generator')
    lines = ['// Generated from real ISimManager; unsupported calls throw, never fabricate business results.',
             '// Interface SHA256: ' + hashlib.sha256(source.read_bytes()).hexdigest(),
             '#pragma once', '#include "i_sim_manager.h"', '#include <stdexcept>',
             'namespace PrimarySlotHost {', 'using namespace OHOS::Telephony;', 'using OHOS::sptr;',
             'class UnscriptedSimManager : public OHOS::Telephony::ISimManager {', 'public:']
    for returns, name, args in methods:
        args = re.sub(r'\s*=\s*false\b', '', re.sub(r'\s+', ' ', args).strip())
        lines += ['    ' + re.sub(r'\s+', ' ', returns).strip() + ' ' + name + '(' + args + ') override',
                  '    { throw std::logic_error("ISimManager seam: unexpected ' + name + '"); }']
    return '\n'.join(lines + ['};', '}']) + '\n'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    options = parser.parse_args()
    destination = HERE / 'unscripted_sim_manager.h'
    generated = generate()
    if options.check:
        if destination.read_text() != generated:
            raise SystemExit('ISimManager seam drift; regenerate after interface review')
    else:
        destination.write_text(generated)
