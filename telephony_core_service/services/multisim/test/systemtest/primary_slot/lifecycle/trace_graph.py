"""Read-only LLDB diagnostics: named DWARF fields, no target expressions or writes.

Private members are inspected only by the debugger; no C++ access-control bypass,
raw offset interpretation, additional target shared ownership, or forced cleanup.
"""
import json
import os
import re
import lldb

REPORT = {'schema_version': 1, 'method': 'read_only_named_DWARF_fields_and_breakpoints',
          'events': [], 'snapshots': [], 'errors': []}
WATCHES = {}
OUT = os.environ.get('SIM_LIFE_GRAPH', '/tmp/sim028-lifetime-graph.json')

def save():
    with open(OUT, 'w') as stream:
        json.dump(REPORT, stream, indent=2)
        stream.write('\n')

def member(value, name, depth=0):
    raw = value.GetNonSyntheticValue()
    found = raw.GetChildMemberWithName(name)
    if found.IsValid():
        return found
    if depth < 4:
        for index in range(raw.GetType().GetNumberOfDirectBaseClasses()):
            base = raw.GetChildAtIndex(index)
            result = member(base, name, depth + 1)
            if result.IsValid():
                return result
    return lldb.SBValue()

def pointer(value):
    value.SetPreferSyntheticValue(True)
    if value.GetType().IsPointerType():
        return value
    p = member(value, '_M_ptr')
    if p.IsValid():
        return p
    # LLDB's libstdc++ unique_ptr synthetic child exposes the typed pointer.
    for child in value:
        if child.GetType().IsPointerType():
            return child
    return lldb.SBValue()

def count(value):
    control = member(member(value, '_M_refcount'), '_M_pi')
    if not control.IsValid() or not control.GetValueAsUnsigned():
        return None
    field = member(control.Dereference(), '_M_use_count')
    return field.GetValueAsSigned() if field.IsValid() else None

def graph(root):
    nodes, edges, visited, aliases = {}, [], set(), {}
    def obj(value, address, typename, depth):
        key = hex(address)
        if (address, typename) in visited or depth > 30 or len(nodes) >= 300:
            return
        visited.add((address, typename))
        nodes[key] = {'type': typename}
        fields(value, address, '', depth)
    def fields(value, owner, path, depth):
        if depth > 35 or not value.IsValid():
            return
        typ = value.GetType().GetCanonicalType().GetName() or ''
        if value.GetType().IsPointerType() or value.GetType().IsReferenceType():
            return
        value.SetPreferSyntheticValue(True)
        smart = re.match(r'^(?:const )?std::(?:__)?(?:shared_ptr|weak_ptr|unique_ptr)<', typ) is not None
        if smart:
            p = pointer(value)
            address = p.GetValueAsUnsigned() if p.IsValid() else 0
            if not address:
                return
            ownership = 'weak' if 'weak_ptr<' in typ else 'unique' if 'unique_ptr<' in typ else 'shared'
            dynamic = p.GetDynamicValue(lldb.eDynamicDontRunTarget)
            if dynamic.IsValid(): p = dynamic
            address = p.GetValueAsUnsigned()
            target_type = p.GetType().GetPointeeType().GetCanonicalType().GetName() or ''
            # Save a typed control-block address in debugger-owned data. No
            # target shared_ptr copy/increment. Read again only for the proven
            # retained SCC and its owned objects, never freed/unrelated objects.
            retained = ['SimStateManager', 'SimStateHandle', 'SimFileManager', 'IccFile',
                        'IccFileController', 'MultiSimMonitor', 'MultiSimController',
                        'RadioProtocolController', 'ITelRilManager', 'IccDiallingNumbersHandler',
                        'IccDiallingNumbersManager', 'IccDiallingNumbersCache', 'UsimDiallingNumbersService']
            if ownership == 'shared' and target_type.rsplit('::', 1)[-1] in retained:
                control = member(member(value, '_M_refcount'), '_M_pi')
                if control.IsValid():
                    WATCHES[hex(address)] = (target_type, control.GetValueAsUnsigned(), control.GetType().GetPointeeType())

            edges.append({'from': hex(owner), 'to': hex(address), 'member': path,
                          'ownership': ownership, 'shared_count_at_snapshot': count(value) if ownership != 'unique' else None,
                          'target_type': target_type})
            if ownership != 'weak' and ('OHOS::' in target_type) and 'Mock' not in target_type:
                obj(p.Dereference(), address, target_type, depth + 1)
            return
        if value.GetType().IsPointerType() or value.GetType().IsReferenceType():
            return
        container = any(token in typ for token in ['std::vector<', 'std::map<', 'std::list<', 'std::__cxx11::list<', 'std::pair<'])
        if container:
            for index in range(min(value.GetNumChildren(), 300)):
                child = value.GetChildAtIndex(index)
                label = child.GetName() or str(index)
                if 'std::map<' in typ:
                    key = member(child, 'first')
                    if key.IsValid(): label += '{event=' + str(key.GetValueAsSigned()) + '}'
                fields(child, owner, path + '.' + label, depth + 1)
            return
        if 'OHOS::' not in typ or 'Mock' in typ:
            return
        raw = value.GetNonSyntheticValue()
        for child in raw:
            name = child.GetName() or ''
            if name.startswith('OHOS::'):
                address = child.GetLoadAddress()
                if address != lldb.LLDB_INVALID_ADDRESS:
                    aliases[hex(address)] = hex(owner)
            fields(child, owner, path + ('.' if path else '') + name, depth + 1)
    obj(root.Dereference(), root.GetValueAsUnsigned(), root.GetType().GetPointeeType().GetCanonicalType().GetName(), 0)
    return {'nodes': nodes, 'edges': edges, 'base_subobject_aliases': aliases}

def event(frame, bp_loc, internal_dict):
    try:
        name = frame.GetFunctionName() or ''
        this = frame.FindVariable('this')
        row = {'function': name, 'this': hex(this.GetValueAsUnsigned()),
               'thread_id': frame.GetThread().GetThreadID()}
        if 'ObserverHandler::RegObserver' in name or 'ObserverHandler::Remove(' in name:
            handler = frame.FindVariable('handler')
            if handler.GetType().IsReferenceType(): handler = handler.Dereference()
            ptr = pointer(handler)
            row.update({'event_id': frame.FindVariable('what').GetValueAsSigned(),
                        'handler': hex(ptr.GetValueAsUnsigned()), 'handler_type': ptr.GetDynamicValue(lldb.eDynamicDontRunTarget).GetTypeName()})
        if 'SimManager::~SimManager(' in name and this.GetValueAsUnsigned():
            REPORT['snapshots'].append(dict(stage='manager_destructor_entry', manager=row['this'], **graph(this)))
        REPORT['events'].append(row)
        save()
    except Exception as error:
        REPORT['errors'].append(repr(error)); save()
    return False

def checkpoint(frame, bp_loc, internal_dict):
    try:
        for index in range(min(frame.GetThread().GetNumFrames(), 6)):
            candidate = frame.GetThread().GetFrameAtIndex(index)
            if (candidate.GetFunctionName() or '').startswith('Sim028Host::LifecycleCheckpoint('):
                frame = candidate
                break
        manager = frame.FindVariable('manager')
        stage = frame.FindVariable('stage').GetSummary()
        snapshot = {'frame': frame.GetFunctionName(), 'arguments': [(v.GetName(), v.GetValue(), v.GetSummary()) for v in frame.GetVariables(True, False, False, True)], 'stage': stage, 'manager': hex(manager.GetValueAsUnsigned()),
                    'ril': hex(frame.FindVariable('ril').GetValueAsUnsigned())}
        if manager.GetValueAsUnsigned(): snapshot.update(graph(manager))
        if stage == '"after_cleanup"':
            destroyed = {e['this'] for e in REPORT['events'] if '::~' in e['function']}
            snapshot['retained_control_blocks'] = []
            for address, (typ, control_address, control_type) in WATCHES.items():
                if address in destroyed: continue
                target = frame.GetThread().GetProcess().GetTarget()
                control = target.CreateValueFromAddress('retained_control', lldb.SBAddress(control_address, target), control_type)
                field = member(control, '_M_use_count')
                snapshot['retained_control_blocks'].append({'address': address, 'type': typ,
                    'control_block': hex(control_address),
                    'use_count': field.GetValueAsSigned() if field.IsValid() and field.GetError().Success() else None})
        snapshot['threads'] = [{'id': t.GetThreadID(), 'frames': [t.GetFrameAtIndex(i).GetFunctionName() for i in range(min(t.GetNumFrames(), 16))]} for t in frame.GetThread().GetProcess()]
        REPORT['snapshots'].append(snapshot)
        save()
    except Exception as error:
        REPORT['errors'].append(repr(error)); save()
    return False

def __lldb_init_module(debugger, internal_dict):
    target = debugger.GetSelectedTarget()
    REPORT['resolved_breakpoints'] = []
    names = ['SimManager', 'MultiSimMonitor', 'MultiSimController', 'SimStateManager',
             'SimStateHandle', 'SimFileManager', 'IccFile', 'SimFile', 'IccFileController',
             'SimSmsManager', 'SimSmsController', 'SimAccountManager', 'StkManager',
             'StkController', 'RadioProtocolController', 'OperatorConfigCache', 'SimStateTracker']
    for name in names:
        bp = target.BreakpointCreateByRegex(r'OHOS::Telephony::' + name + r'::(~?' + name + r')\(')
        bp.SetScriptCallbackFunction(__name__ + '.event')
        REPORT['resolved_breakpoints'].append({'pattern': str(bp), 'locations': bp.GetNumLocations()})
    for name in ['RegObserver', 'Remove', 'RemoveAll']:
        bp = target.BreakpointCreateByRegex(r'OHOS::Telephony::ObserverHandler::' + name + r'\(')
        bp.SetScriptCallbackFunction(__name__ + '.event')
        REPORT['resolved_breakpoints'].append({'pattern': str(bp), 'locations': bp.GetNumLocations()})
    bp = target.BreakpointCreateByName('Sim028Host::LifecycleCheckpoint')
    bp.SetScriptCallbackFunction(__name__ + '.checkpoint')
    save()
