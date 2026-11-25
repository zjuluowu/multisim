#!/usr/bin/env python3
"""Require positive production execution evidence for the migrated state slice."""
import argparse
import json
from pathlib import Path
import subprocess

REQUIRED = {
    'SimStateManager': ['Init', 'GetSimState', 'HasSimCard', 'UnlockPin', 'UnlockPuk', 'RefreshSimState'],
    'SimStateHandle': ['ProcessEvent', 'GetSimCardData', 'ProcessIccCardState', 'SimStateEscape', 'GetUnlockResult'],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--slice', choices=['state','primary','query','upstream','target-adapter','target-topology','target-primary-b','legacy-data','target-data','legacy-voice-sms','target-voice-sms','legacy-account-metadata','target-account-metadata'], default='state')
    parser.add_argument('--binary', required=True, type=Path)
    parser.add_argument('--profiles', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    profiles = sorted(args.profiles.glob('*.profraw'))
    if not profiles:
        parser.error('No execution profiles')
    args.output.mkdir(parents=True, exist_ok=True)
    merged = args.output / 'execution.profdata'
    subprocess.run(['llvm-profdata', 'merge', '-sparse', *map(str, profiles), '-o', str(merged)], check=True)
    raw = subprocess.check_output(['llvm-cov', 'export', str(args.binary), '--instr-profile=' + str(merged)])
    (args.output / 'coverage.json').write_bytes(raw)
    functions = [function for data in json.loads(raw)['data'] for function in data['functions']]
    rows = []
    if args.slice == 'state':
        required = REQUIRED
    elif args.slice == 'primary':
        required = {'CoreServiceSim':['SetPrimarySlotId', 'StopAccepting',
                        'WaitForAcceptedCallsDrained', 'AsyncSimGeneralExecute'],
                    'SimManager':['SetPrimarySlotId', 'Stop'],
                    'MultiSimController':['SetPrimarySlotId'],
                    'AccountStoreAdapter':['LoadAccounts', 'MatchAccountByIccid', 'WriteDiscoveredAccount']}
    elif args.slice == 'query':
        required = {'CoreServiceSim':['GetRealSimCount'], 'SimManager':['GetRealSimCount'],
                    'TelephonyExtWrapper':['GetRealSimCountExt'], 'LegacySimQueryBinding':['Read','ReadLegacyRealSimCount'],
                    'RealSimCountQuery':['Read']}
    elif args.slice == 'target-adapter':
        required = {'AssignmentStoreAdapter':['WritePrimaryParameters', 'WriteDefaultData',
                                              'WriteUserPrimaryPreference']}
    elif args.slice == 'target-topology':
        required = {'AccountRegistry':['Read', 'AcceptDiscoveredAccount'], 'BindingIndex':['Find'],
                    'TopologyService':['ReadPrimaryTarget', 'AcceptStoredDiscovery', 'DiscoverAccount',
                                      'ReconcileDiscovery', 'PrepareAccountReload', 'AcceptAccountReload'],
                    'DiscoveryObservationLedger':['BeginStore', 'CompleteStored'],
                    'AssignmentService':['PrepareAccountRoleFragment', 'AcceptDiscoveredRoleProjection',
                                        'StillOwnsInitialRoleReservation', 'FinalizeInitialRoleReservation',
                                        'AcceptReloadedRoleProjectionSet']}
    elif args.slice == 'legacy-data':
        required = {'CoreManagerInner': ['SetDefaultCellularDataSlotId', 'GetDefaultCellularDataSlotId',
                                         'GetDefaultCellularDataSimId'],
                    'SimManager': ['SetDefaultCellularDataSlotId', 'GetDefaultCellularDataSlotId'],
                    'MultiSimController': ['SetDefaultCellularDataSlotId', 'SaveDefaultCellularDataSlotIdInfo',
                                           'SendDefaultCellularDataBroadCast', 'AnnounceDefaultCellularDataSimIdChanged'],
                    'CoreServiceHiSysEvent': ['WriteDefaultDataSlotIdBehaviorEvent'],
                    'MultiSimMonitor': ['NotifySimAccountChanged']}
    elif args.slice == 'target-data':
        required = {'SimModule': ['SetDefaultCellularDataSlotId', 'ReconcileDataParameter'],
                    'LegacySimFacade': ['SetDefaultCellularDataSlotId'],
                    'AssignmentService': ['SetDefaultRole', 'WriteTrackedParameter', 'ReconcileC0Parameter'],
                    'AssignmentAggregate': ['AdmitRoleWriteSet', 'AcceptDataParameterProjection', 'ReleaseRoleWriteSet'],
                    'TopologyService': ['WithCurrentPrimaryTarget'],
                    'AssignmentStoreAdapter': ['WriteC0Parameter', 'ReadC0ParameterEvidence'],
                    'LegacyCompletion': ['RetainRoleWaiter', 'CompleteLegacyInt', 'TakeRoleResult']}
    elif args.slice == 'legacy-voice-sms':
        required = {'CoreServiceSim': ['SetDefaultVoiceSlotId'],
                    'CoreManagerInner': ['SetDefaultSmsSlotId'],
                    'SimManager': ['SetDefaultVoiceSlotId', 'SetDefaultSmsSlotId'],
                    'MultiSimController': ['SetDefaultVoiceSlotId', 'SetDefaultSmsSlotId',
                                           'AnnounceDefaultVoiceSimIdChanged', 'AnnounceDefaultSmsSimIdChanged'],
                    'SimRdbHelper': ['SetDefaultVoiceCard', 'SetDefaultMessageCard']}
    elif args.slice == 'target-voice-sms':
        required = {'SimModule': ['SetDefaultVoiceSlotId', 'SetDefaultSmsSlotId', 'ReconcileRoleProvider'],
                    'LegacySimFacade': ['SetDefaultCallMessageRole'],
                    'AssignmentService': ['SetDefaultRole', 'ReconcileRoleProvider'],
                    'AssignmentAggregate': ['AdmitRoleWriteSet', 'AcceptRoleSelection'],
                    'AccountRoleProtocol': ['NoteLocalRoleMutation'],
                    'AssignmentStoreAdapter': ['WriteDefaultRole', 'ReadRoleProviderEvidence']}
    elif args.slice == 'legacy-account-metadata':
        required = {'CoreManagerInner': ['SetSimLabelIndex'],
                    'SimManager': ['SetShowName', 'GetShowName', 'SetSimLabelIndexByIccId', 'GetSimAccountInfo'],
                    'MultiSimController': ['SetShowName', 'GetShowName', 'SetSimLabelIndexByIccId', 'GetSimAccountInfo'],
                    'SimRdbHelper': ['UpdateDataBySimId', 'UpdateDataByIccId']}
    elif args.slice == 'target-account-metadata':
        required = {'SimModule': ['SetShowName', 'GetShowName', 'SetSimLabelIndexByIccId', 'ReconcileAccountMetadata'],
                    'LegacySimFacade': ['SetShowName', 'GetShowName', 'SetSimLabelIndexByIccId'],
                    'TopologyService': ['SetDisplayName', 'SetSimLabel', 'UpdateMetadata', 'AcceptMetadata',
                                        'ReadAccountAtSlot', 'ReconcileMetadata'],
                    'AccountRegistry': ['AcceptDisplayName', 'AcceptLabel'],
                    'AccountStoreAdapter': ['WriteDisplayName', 'WriteLabel', 'ReadMetadataEvidence'],
                    'RuntimeOperationStore': ['AcceptResult', 'AcceptLateEvidence', 'ClaimCompletion']}
    elif args.slice == 'target-primary-b':
        required = {'AssignmentService':['AcceptRadioStep', 'AcceptPrimarySwitchTimeout',
                    'ScheduleC1Retry', 'AcceptScheduledC1Retry', 'StopAcceptingAndCancelScheduledRetries'],
                    'PrimaryHostLifecycle':['StopAcceptingAndDrain'],
                    'PrimaryHostLifecycle':['StopAcceptingAndDrain'],
                    'ResourceRadioRuntime':['SwitchPrimaryRadio', 'AcceptDeviceResult', 'Recover',
                                            'StopAccepting'],
                    'RuntimeOperationStore':['RecordSent', 'AcceptResult', 'AcceptLateEvidence',
                                             'ClaimCompletion', 'ClaimEarlyCompletion', 'TryMarkTerminal']}
    else:
        required = {'CoreServiceSim':['GetRealSimCount'], 'SimManager':['GetRealSimCount'],
                    'TelephonyExtWrapper':['GetRealSimCountExt']}
    for owner, methods in required.items():
        for method in methods:
            needle = str(len(owner)) + owner + str(len(method)) + method + 'E'
            matched = [f for f in functions if needle in f['name'] and any(
                ('/frameworks/native/src/' in name or '/services/multisim/infrastructure/compatibility/events/' in name or '/services/sim/src/' in name or '/services/core/src/' in name or '/framework/internal/host/g2a_reference/' in name or '/services/multisim/infrastructure/compatibility/api/' in name or '/services/multisim/role_assignment/' in name or '/services/multisim/account/' in name or '/services/multisim/infrastructure/request/' in name or '/services/multisim/device/resource/' in name or '/services/multisim/infrastructure/assembly/' in name or '/services/telephony_ext_wrapper/src/' in name) for name in f['filenames'])]
            count = sum(f['count'] for f in matched)
            rows.append({'owner': owner, 'method': method, 'count': count,
                         'files': sorted({name for f in matched for name in f['filenames']}),
                         'status': 'PASS' if count > 0 else 'FAIL'})
    result = {'scope': 'Positive execution of selected '+args.slice+' production methods; not full SIM coverage',
              'methods': rows, 'status': 'PASS' if all(r['status'] == 'PASS' for r in rows) else 'FAIL'}
    (args.output / 'execution.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    return 0 if result['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
