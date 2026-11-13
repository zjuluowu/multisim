"""LLDB-only controlled counterexample; no inferior memory writes or production edits.

Run C0 with SIM028_INIT_OLD_WAIT=1 and SIM028_INIT_ORDER=present-first.
Pause a *real* post-account cache rebuild between clear and assignment, let the
test's public getter run, then release the writer at normal fixture destruction.
The expected result is the original IsSimActive assertion failure, not a PASS.
"""

import sys as _sys
from pathlib import Path as _Path
_TEST_ROOT = next(p for p in _Path(__file__).resolve().parents if (p / "scripts/_paths.py").is_file())
_sys.path.insert(0, str(_TEST_ROOT / "scripts"))
from _paths import TEST as _TEST, CORE as _CORE, REPO as _REPO, locate, guard_asset
import json
from pathlib import Path
import time
import lldb

events = []
inspections = 0
loaded = False
held = None
main_held = None
output = '/tmp/sim028-initialization-debug.json'


def owning_frame(frame, name):
    for candidate in frame.GetThread():
        if name in (candidate.GetFunctionName() or ''):
            return candidate
    return frame


def vector_size(value):
    # Typed DWARF reads only: no inferior function calls while threads are gated.
    impl = value.GetNonSyntheticValue().GetChildMemberWithName('_M_impl')
    begin = impl.GetChildMemberWithName('_M_start')
    end = impl.GetChildMemberWithName('_M_finish')
    if not begin.IsValid() or not end.IsValid():
        return None
    stride = begin.GetType().GetPointeeType().GetByteSize()
    return (end.GetValueAsUnsigned() - begin.GetValueAsUnsigned()) // stride if stride else None


def cache_size(frame):
    obj = frame.FindVariable('this').Dereference()
    return vector_size(obj.GetChildMemberWithName('localCacheInfo_'))


def record(kind, frame, **values):
    queue = 'UNKNOWN'
    for caller in frame.GetThread():
        if 'ffrt::Worker' in (caller.GetFunctionName() or ''):
            state = caller.FindVariable('state').GetNonSyntheticValue()
            ptr = state.GetChildMemberWithName('_M_ptr')
            if ptr.IsValid():
                queue = ptr.Dereference().GetChildMemberWithName('name').GetSummary() or 'UNKNOWN'
            break
    row = dict(kind=kind, time_ns=time.monotonic_ns(), thread=frame.GetThread().GetThreadID(),
               function=frame.GetFunctionName(), queue=queue,
               source=str(frame.GetLineEntry().GetFileSpec()), line=frame.GetLineEntry().GetLine(),
               stack=[f.GetFunctionName() for f in frame.GetThread()][:12], **values)
    events.append(row)
    Path(output).write_text(json.dumps(events, indent=2) + '\n')
    print('CONTROLLED_INIT ' + json.dumps(row))


def account(frame, bp_loc, internal_dict):
    global loaded
    loaded = True
    record('account_event', owning_frame(frame, 'AccountObserver::ProcessEvent'))
    return False


def cache(frame, bp_loc, internal_dict):
    global held, inspections
    if not loaded or held is not None:
        return False
    frame = owning_frame(frame, 'MultiSimController::GetListFromDataBase')
    size = cache_size(frame)
    row_count = vector_size(frame.FindVariable('newCache'))
    if inspections < 4:
        record('cache_inspection', frame, cache_size=size, queried_rows=row_count)
        inspections += 1
    if size != 0 or not row_count:
        if main_held is not None and main_held.IsSuspended():
            main_held.Resume()
        return False
    # Pause only the real writer. The test thread continues its original getter.
    thread = frame.GetThread()
    if not thread.Suspend():
        record('suspend_failed', frame)
        return True
    held = thread
    record('cache_clear_writer_paused', frame, controller=frame.FindVariable('this').GetValue(),
           cache_size=size, queried_rows=row_count,
           caller=thread.GetFrameAtIndex(1).GetFunctionName())
    if main_held is not None and main_held.IsSuspended():
        main_held.Resume()
    return False


def trace(frame, bp_loc, internal_dict):
    global main_held
    frame = owning_frame(frame, 'Sim028Host::InitTrace')
    stage = frame.FindVariable('stage')
    error = lldb.SBError()
    text = frame.GetThread().GetProcess().ReadCStringFromMemory(stage.GetValueAsUnsigned(), 128, error)
    if error.Success() and text == 'account_wait_returned' and held is None:
        main_held = frame.GetThread()
        if main_held.Suspend():
            record('reader_waits_for_controlled_cache_refresh', frame)
    return False


def query(frame, bp_loc, internal_dict):
    frame = owning_frame(frame, 'MultiSimController::IsSimActive')
    if held is not None and held.IsSuspended() and frame.GetThread().GetThreadID() != held.GetThreadID():
        record('public_active_query_while_writer_paused', frame,
               controller=frame.FindVariable('this').GetValue(), cache_size=cache_size(frame),
               slot=frame.FindVariable('slotId').GetValueAsSigned())
    return False


def release(frame, bp_loc, internal_dict):
    if held is not None and held.IsSuspended():
        record('fixture_cleanup_releases_writer', frame)
        held.Resume()
    return False


def ready(frame, bp_loc, internal_dict):
    if held is not None and held.IsSuspended():
        record('ready_barrier_releases_controlled_writer', frame)
        held.Resume()
    return False


def state_init(frame, bp_loc, internal_dict):
    frame = owning_frame(frame, 'SimStateManager::Init')
    record('state_manager_init', frame, object=frame.FindVariable('this').GetValue(),
           slot=frame.FindVariable('slotId').GetValueAsSigned())
    return False


def state_event(frame, bp_loc, internal_dict):
    frame = owning_frame(frame, 'SimStateHandle::ProcessEvent')
    obj = frame.FindVariable('this').Dereference()
    record('state_handler_event', frame, object=frame.FindVariable('this').GetValue(),
           slot=obj.GetChildMemberWithName('slotId_').GetValueAsSigned(),
           event=frame.FindVariable('eventId').GetValueAsUnsigned(),
           state=obj.GetChildMemberWithName('externalState_').GetValueAsSigned())
    return False


def state_transition(frame, bp_loc, internal_dict):
    frame = owning_frame(frame, 'SimStateHandle::SimStateEscape')
    obj = frame.FindVariable('this').Dereference()
    record('state_transition_before', frame, object=frame.FindVariable('this').GetValue(),
           slot=frame.FindVariable('slotId').GetValueAsSigned(),
           ril_state=frame.FindVariable('simState').GetValueAsSigned(),
           state=obj.GetChildMemberWithName('externalState_').GetValueAsSigned())
    return False


def __lldb_init_module(debugger, internal_dict):
    target = debugger.GetSelectedTarget()
    callbacks = [('Sim028Host::AccountObserver::ProcessEvent', 'account'),
                 ('Sim028Host::InitTrace', 'trace'),
                 ('OHOS::Telephony::MultiSimController::IsSimActive', 'query'),
                 ('SimHost::AwaitSchedulerReadyIdle', 'ready'),
                 ('OHOS::Telephony::SimStateManager::Init', 'state_init'),
                 ('OHOS::Telephony::SimStateHandle::SimStateEscape', 'state_transition'),
                 ('Sim028Host::RealScenario::~RealScenario', 'release')]
    for name, callback in callbacks:
        bp = target.BreakpointCreateByName(name)
        bp.SetScriptCallbackFunction(__name__ + '.' + callback)
    bp = target.BreakpointCreateByLocation('multi_sim_controller.cpp', 753)
    bp.SetScriptCallbackFunction(__name__ + '.cache')
    bp = target.BreakpointCreateByLocation('sim_state_handle.cpp', 839)
    bp.SetScriptCallbackFunction(__name__ + '.state_event')
