#!/usr/bin/env python3
"""Check the supported business/protocol boundary, not directory counts alone."""
import json,re
from _paths import TEST

LOW_LEVEL = re.compile(r'\b(?:EXPECT_CALL|ON_CALL|StrictMock|NiceMock|PrimaryEnvironment|SimManager|CoreServiceSim|mutex|condition_variable)\b')
SHARED_INCLUDES = {'cstddef', 'cstdint', 'string', 'vector', 'gtest/gtest.h', 'telephony_errors.h'}


def check_shared(root):
    headers = {f'shared/{path.name}' for path in root.glob('*.h')}
    for name in headers:
        path = root.parent / name
        source = path.read_text()
        if LOW_LEVEL.search(source):
            raise RuntimeError(f'Low-level operation in shared scenario: {path}')
        for directive in re.findall(r'^[ \t]*#[ \t]*include\b([^\n]*)', source, re.M):
            literal = re.fullmatch(r'\s*(?:<([^>]+)>|"([^"]+)")\s*', directive)
            if not literal or (literal[1] or literal[2]) not in SHARED_INCLUDES:
                raise RuntimeError(f'Shared scenario depends on an implementation: {path}: {directive.strip()}')
    return headers


def check():
    root=TEST/'framework';public=sorted(root.glob('*.h'))
    if {p.name for p in public}!={'sim_test.h','sim_protocol_test.h'}:raise RuntimeError('Only the business and explicit protocol headers are public')
    visited=set()
    def inspect(path):
        if path in visited:return
        visited.add(path);text=path.read_text()
        if re.search(r'\b(?:SimHost|Sim028Host|PrimaryEnvironment|MockParameterService|MockTelRilManager|FakeRil|RilFaultControls)\b',text):raise RuntimeError('Private type in public include graph: '+str(path))
        for name in re.findall(r'#include "([^"]+)"',text):
            if '/host/' in name or '/device/' in name:raise RuntimeError('Private platform header exposed: '+name)
            dependency=root/name
            if not dependency.is_file():raise RuntimeError('Missing public dependency '+name)
            inspect(dependency)
    for path in public:inspect(path)
    ordinary=list((TEST/'cases').glob('*.cpp'));sites=[]
    shared = check_shared(TEST/'cases/shared')
    for path in ordinary:
        text=path.read_text()
        if LOW_LEVEL.search(text):sites.append(path.name)
        for name in re.findall(r'#include "([^"]+)"',text):
            if name not in {'sim_test.h','sim_protocol_test.h'} | shared:raise RuntimeError('Ordinary case includes implementation '+str(path)+': '+name)
    if sites:raise RuntimeError('Low-level operations in ordinary cases '+str(sites))
    return {'status':'PASS','public_headers':2,'public_host_includes':0,'ordinary_low_level_sites':0,'root_case_files':len(ordinary),'ordinary_files':sum('sim_protocol_test.h' not in p.read_text() for p in ordinary),'protocol_files':sum('sim_protocol_test.h' in p.read_text() for p in ordinary),'public_include_graph':[str(p.relative_to(TEST)) for p in sorted(visited)]}
if __name__=='__main__':print(json.dumps(check(),indent=2))
