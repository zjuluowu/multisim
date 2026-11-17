#!/usr/bin/env python3
"""Check the supported business/protocol boundary, not directory counts alone."""
import json,re
from _paths import TEST

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
    for path in ordinary:
        text=path.read_text()
        if re.search(r'\b(?:EXPECT_CALL|ON_CALL|StrictMock|NiceMock|PrimaryEnvironment|SimManager|CoreServiceSim|mutex|condition_variable)\b',text):sites.append(path.name)
        for name in re.findall(r'#include "([^"]+)"',text):
            if name not in {'sim_test.h','sim_protocol_test.h'}:raise RuntimeError('Ordinary case includes implementation '+str(path)+': '+name)
    if sites:raise RuntimeError('Low-level operations in ordinary cases '+str(sites))
    return {'status':'PASS','public_headers':2,'public_host_includes':0,'ordinary_low_level_sites':0,'root_case_files':len(ordinary),'ordinary_files':sum('sim_protocol_test.h' not in p.read_text() for p in ordinary),'protocol_files':sum('sim_protocol_test.h' in p.read_text() for p in ordinary),'public_include_graph':[str(p.relative_to(TEST)) for p in sorted(visited)]}
if __name__=='__main__':print(json.dumps(check(),indent=2))
