#!/usr/bin/env python3
"""Capture local cross-repository evidence without downloading or changing source."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import subprocess

SYSTEM = Path(__file__).resolve().parents[1]
CORE = SYSTEM.parents[3]

# Exact files keep the audit bounded; missing files are evidence, not guessed contracts.
SOURCES = {
    'telephony_core_service': {
        'services/core/src/core_service_sim.cpp': r'SetPrimarySlotId|HasSimCard|IsSimActive|SET_TELEPHONY_STATE',
        'services/core/src/core_service.cpp': r'CoreService::Init|CoreService::OnStop|make_shared<SimManager>|DeInit',
        'services/sim/src/sim_manager.cpp': r'SimManager::SetPrimarySlotId|NotifySimAccountChanged',
        'services/sim/src/multi_sim_controller.cpp': r'SetPrimarySlotId|NeedSwitchSimCard|SwapM0M2|SavePrimary|ResumePrimary|PublishSetPrimary|SendMainCardBroadCast|SetDefaultCellularDataSlotId',
        'services/sim/src/radio_protocol_controller.cpp': r'ProcessCheckRadio|ProcessUpdateRadio|ProcessRadioProtocolNotify|ProcessSetRadioProtocolComplete|ProcessResponseInfoOfEvent|sessionId_|COMMUNICATION_TIMEOUT',
        'services/tel_ril/src/tel_ril_sim.cpp': r'SetPrimarySlot|SetRadioProtocol|SimRadioProtocolUpdated',
        'services/tel_ril/include/observer_handler.h': r'observerHandlerMap_|shared_ptr',
        'services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp': r'swapM0M2|isHandleVSim|dlopen|\.so',
        'services/network_search/src/radio_info.cpp': r'GetPrimarySlotId',
        'services/network_search/src/network_search_manager.cpp': r'GetPrimarySlotId',
        'frameworks/native/src/core_service_proxy.cpp': r'SetPrimarySlotId|SET_PRIMARY_SLOT',
        'frameworks/native/src/core_service_client.cpp': r'SetPrimarySlotId',
        'frameworks/js/network_search/src/napi_radio.cpp': r'NativeSetPrimarySlotId|SetPrimarySlotIdCallback|SetPrimarySlotId\(context|setPrimarySlotId',
        'services/tel_ril/include/tel_ril_base.h': r'ErrorResponse|Response\(|serialId|serial',
    },
    'telephony_ril_adapter': {
        'services/hril/src/hril_sim.cpp': r'SetPrimarySlot|SetRadioProtocol|BuildRadioProtocol|sessionId',
        'services/vendor/src/at_sim.c': r'ReqSetRadioProtocol|HRIL_RADIO_PROTOCOL_PHASE_UPDATE|HRIL_RADIO_PROTOCOL_PHASE_NOTIFY|HNOTI_SIM_RADIO_PROTOCOL_UPDATED',
        'services/vendor/src/vendor_adapter.c': r'\.SetRadioProtocol|\.SetPrimarySlot',
        'interfaces/innerkits/include/hril_vendor_sim_defs.h': r'SetPrimarySlot|SetRadioProtocol',
    },
    'drivers_interface': {
        'ril/v1_5/IRil.idl': r'SetPrimarySlot',
        'ril/v1_1/IRil.idl': r'SetRadioProtocol',
        'ril/v1_1/IRilCallback.idl': r'SetRadioProtocolResponse|SimRadioProtocolUpdated',
    },
    'telephony_cellular_data': {
        'services/src/cellular_data_controller.cpp': r'RADIO_SIM_ACCOUNT_LOADED|SIM_CARD_DEFAULT_DATA_SUBSCRIPTION_CHANGED',
        'services/src/cellular_data_handler.cpp': r'GetPrimarySlotId|RADIO_SIM_ACCOUNT_LOADED',
        'services/src/state_machine/incall_data_state_machine.cpp': r'GetPrimarySlotId|SetDefaultCellularDataSlotId',
    },
    'telephony_state_registry': {
        'services/src/telephony_state_registry_service.cpp': r'UpdateIccAccount|OnIccAccountUpdated|OBSERVER_MASK_ICC_ACCOUNT',
    },
    'telephony_telephony_data': {
        'sim/src/sim_ability.cpp': r'SimAbility::Update|helper_\.Update|SET_TELEPHONY_STATE|/sim/sim_info',
        'sim/src/rdb_sim_helper.cpp': r'CreateSimInfoTableStr|IS_MAIN_CARD|IS_CELLULAR_DATA_CARD|LSI',
        'common/src/rdb_base_helper.cpp': r'RdbBaseHelper::Update|store_->Update',
        'interfaces/innerkits/include/sim_data.h': r'PHY_CARD|LSI|SIM_LABEL_INDEX|IS_MAIN_CARD|IS_CELLULAR_DATA_CARD',
    },
    'distributeddatamgr_data_share': {
        'interfaces/inner_api/consumer/include/datashare_helper.h': r'Creator\(|virtual int Update',
    },
    'notification_common_event_service': {
        'frameworks/native/src/common_event_manager.cpp': r'CommonEventManager::PublishCommonEvent\(|return NewPublishCommonEvent\(',
    },
    'startup_init': {
        'interfaces/innerkits/syspara/parameter.c': r'^int SetParameter|^int GetParameter|SystemSetParameter|SystemReadParam',
    },
}


def git(root, *argv):
    p = subprocess.run(['git', '-C', str(root), *argv], text=True, capture_output=True, timeout=30)
    return p.stdout.strip() if p.returncode == 0 else 'UNAVAILABLE: ' + p.stderr.strip()[:200]


def inspect(root):
    entry = {'path': str(root), 'name': root.name, 'head': git(root, 'rev-parse', 'HEAD'),
             'remote': git(root, 'remote', 'get-url', 'origin'), 'sources': []}
    for name, pattern in SOURCES.get(root.name, {}).items():
        path = root / name
        if not path.is_file():
            entry['sources'].append({'path': name, 'status': 'MISSING'})
            continue
        content = path.read_bytes()
        entry['sources'].append({'path': name, 'sha256': hashlib.sha256(content).hexdigest(),
            'matches': [{'line': i, 'text': line.strip()} for i, line in enumerate(
                content.decode(errors='replace').splitlines(), 1) if re.search(pattern, line)]})
    # Actual bytes, not HEAD, are authoritative for each audited file.
    entry['audited_file_changes'] = git(root, 'diff', '--name-only', '--', *SOURCES.get(root.name, {}).keys()) if entry['sources'] else 'NOT_AUDITED'
    if root.name.startswith('telephony_'):
        argv = ['rg', '-n', 'SetPrimarySlotId|GetPrimarySlotId|SET_PRIMARY_SLOT_STATUS|DEFAULT_MAIN_SUBSCRIPTION_CHANGED|SIM_CARD_DEFAULT_DATA_SUBSCRIPTION_CHANGED',
                '--glob', '*.{cpp,h,c,idl}', '--glob', '!**/test/**', '--glob', '!test/**', '.']
        search = subprocess.run(argv, cwd=root, capture_output=True, text=True, timeout=60)
        lines = search.stdout.splitlines()
        entry['relationship_search'] = {'argv': argv, 'exit_code': search.returncode,
            'matches': lines[:120], 'total_matches': len(lines), 'stderr': search.stderr[:200]}
    return entry


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=Path, action='append', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    roots = {CORE}
    scans = []
    for workspace in args.workspace:
        children = sorted(workspace.iterdir()) if workspace.is_dir() else []
        scans.append({'path': str(workspace), 'exists': workspace.is_dir(),
                      'directories': [p.name for p in children if p.is_dir()]})
        roots.update(p for p in children if p.is_dir() and (p / '.git').exists() and
                     (p.name.startswith('telephony_') or p.name in SOURCES or
                      p.name in ('distributeddatamgr_data_share', 'startup_init',
                                 'notification_common_event_service', 'resourceschedule_ffrt')))
    with ThreadPoolExecutor(max_workers=4) as pool:
        repos = list(pool.map(inspect, sorted(roots)))
    result = {'workspace_scans': scans, 'repositories': repos,
              'product_manifest_compatibility': 'NOT_VERIFIED',
              'missing_implementations': [
                  {'name': n, 'version': 'UNKNOWN: matching product manifest not supplied',
                   'required': why} for n, why in [
                      ('telephony_ext SIM vendor library', 'SwapM0M2SimCards/isHandleVSim/SendSimChgTypeInfo implementation and build feature'),
                      ('simollie', 'No directory/implementation found in the explicitly scanned workspaces; participation not established'),
                      ('product chipset radio/RIL', 'Actual vendor SetPrimarySlot/SetRadioProtocol functions, modem and firmware version')]],
              'source_snippets_are_static_evidence_only': True}
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(f'{len(repos)} repository copies inventoried; output {args.output}')


if __name__ == '__main__':
    main()
