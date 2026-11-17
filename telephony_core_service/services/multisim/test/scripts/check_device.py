#!/usr/bin/env python3
"""Static native wiring checks; these cannot establish product compilation."""
import json,re,subprocess
from pathlib import Path
from _paths import TEST,CORE

def check():
    native=(TEST/'BUILD.gn').read_text();host=(TEST/'framework/internal/host/BUILD.gn').read_text()
    subprocess.run(['gn','format','--dry-run',str(TEST/'BUILD.gn')],check=True,stdout=subprocess.DEVNULL)
    for text in [native,host]:
        if 'sources.gni' not in text or 'sim_case_sources' not in text:raise RuntimeError('Shared cases missing')
    if 'profile_startup.cpp' in native:raise RuntimeError('Host preinit ABI must not be linked into native product targets')
    if '.preinit_array' in (TEST/'framework/internal/common/main.cpp').read_text():raise RuntimeError('Platform-specific startup leaked into common main')
    if 'sim_protocol_case_sources' not in native or 'component_sources.gni' not in native:raise RuntimeError('Shared component inputs missing')
    for p in (TEST/'cases').rglob('*.cpp'):
        if re.search(r'^\s*#\s*(if|ifdef|ifndef).*\b(HOST|DEVICE|OHOS|SIM_HAS_LLVM_PROFILE)\b',p.read_text(),re.M):raise RuntimeError('Platform branch in business case: '+str(p))
    api=(CORE/'interfaces/innerkits/BUILD.gn').read_text()
    if not re.search(r'\("tel_core_service_api"\)',api):raise RuntimeError('Actual product API target missing')
    for name in ['UnlockPin','UnlockPuk','GetMaxSimCount','GetSimState','HasSimCard','GetCardType','GetSimIccId']:
        if name not in (CORE/'interfaces/innerkits/include/core_service_client.h').read_text():raise RuntimeError('Missing product API: '+name)
    device=(TEST/'framework/internal/device/sim_test.cpp').read_text()
    for required in ['CoreServiceClient','AddStateObserver','RemoveStateObserver','RESTORE','GetSelfTokenID','CheckSystemAbility']:
        if required not in device:raise RuntimeError('Native responsibility missing: '+required)
    return {'status':'PASS','native_build':'NOT_RUN','device_execution':'NOT_RUN','scope':'GN syntax, shared sources, local API existence and implementation responsibilities only'}
if __name__=='__main__':print(json.dumps(check(),indent=2))
