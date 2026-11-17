#!/usr/bin/env python3
"""Authenticate historical evidence, immutable contracts and the single active tree.

Case implementation is intentionally editable. Historical framework layout is
not frozen as a runtime requirement; its exact original bytes stay in the archive.
"""
import hashlib,io,json,re,subprocess,tarfile
from pathlib import Path
from _paths import TEST,REPO

def check():
    migration=json.loads((TEST/'docs/Full_Replacement_Migration.json').read_text())
    archive=TEST/migration['archive']
    if hashlib.sha256(archive.read_bytes()).hexdigest()!=migration['archive_sha256']:raise RuntimeError('Original source archive changed')
    prefix=TEST.relative_to(REPO).as_posix()+'/'
    names=subprocess.check_output(['git','ls-tree','-rz','--name-only',migration['baseline'],'--',prefix],cwd=REPO).decode().rstrip('\0').split('\0')
    active=[n[len(prefix):] for n in names if '/docs/archive/' not in n]
    with tarfile.open(archive) as stream: archived={m.name:stream.extractfile(m).read() for m in stream if m.isfile()}
    if set(archived)!=set(active):raise RuntimeError('Original active archive inventory incomplete')
    raw=subprocess.check_output(['git','cat-file','--batch'],input=('\n'.join(migration['baseline']+':'+prefix+n for n in active)+'\n').encode(),cwd=REPO);stream=io.BytesIO(raw)
    for name in active:
        header=stream.readline().split()
        if len(header)!=3 or header[1]!=b'blob':raise RuntimeError('Missing original Git evidence: '+name)
        data=stream.read(int(header[2]));assert stream.read(1)==b'\n'
        if archived[name]!=data:raise RuntimeError('Original source evidence drift: '+name)
    edges=migration['files']
    def resolve(name):
        for row in edges:
            if name==row['old']:name=row.get('new')
            if name is None:return None
        return name
    for row in edges:
        if row['action']=='retired' and (TEST/row['old']).is_file():raise RuntimeError('Retired active file restored: '+row['old'])
        if row.get('new') and resolve(row['new']) and not (TEST/resolve(row['new'])).is_file():raise RuntimeError('Migration destination missing: '+row['new'])
    # Golden/contracts/reference data retain original bytes. Ordinary TEST_F edits
    # do not require hash-list or runner changes.
    protected=0
    for old,data in archived.items():
        new=resolve(old)
        if new and (new.startswith('cases/data/') or new.startswith('framework/host/g2a_reference/') or new=='framework/host/configs/dependency_lock.json'):
            if (TEST/new).read_bytes()!=data:raise RuntimeError('Immutable expectation/reference changed: '+new)
            protected+=1
    # Previously published historical evidence must remain at the original paths.
    changed=subprocess.check_output(['git','diff',migration['baseline'],'--name-only','-z','--',prefix+'docs/archive'],cwd=REPO).decode().rstrip('\0').split('\0')
    original=set(names)
    if any(n in original for n in changed):raise RuntimeError('Published historical evidence changed')
    for path in (TEST/'framework').rglob('*'):
        if path.suffix in ('.cpp','.h'):
            for line in path.read_text().splitlines():
                if line.lstrip().startswith('#include') and '/cases/' in line:raise RuntimeError('Framework depends on cases')
    from ci_gate import load_known
    known=load_known()
    expected={}
    from ci_gate import signature
    import xml.etree.ElementTree as ET
    with tarfile.open(TEST/'docs/archive/full_replacement_20261116/replacement_host/raw-results.tar.gz') as archive:
        for member in archive:
            if not member.name.endswith('.xml'):continue
            for case in ET.fromstring(archive.extractfile(member).read()).iter('testcase'):
                if case.find('failure') is not None:
                    expected[case.get('classname')+'.'+case.get('name')]=[signature(f.get('message','')+(f.text or '')) for f in case.findall('failure')]
    if set(known)!=set(expected) or any(known[n]['classifier_failures']!=expected[n] for n in expected):
        raise RuntimeError('Active policy provenance differs from original raw XML')
    from source_boundaries import verify_source_tables
    verify_source_tables()
    from check_device import check as device_check
    device_check()
    subprocess.run(['python3','-B',str(TEST/'scripts/update_case_sources.py'),'--check'],check=True)
    print(f'integrity: PASS ({len(active)} exact archived originals; {len(edges)} dispositions; {protected} immutable live contracts/references)')
    return {'status':'PASS','archived_originals':len(active),'dispositions':len(edges),'protected_contracts':protected}
if __name__=='__main__':check()
