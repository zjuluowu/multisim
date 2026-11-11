"""Derive address-linked ownership proofs from the read-only LLDB snapshot."""
from collections import Counter


def summarize(report):
    snapshot = next(s for s in report['snapshots'] if s['stage'] == 'manager_destructor_entry')
    after = next(s for s in report['snapshots'] if s['stage'] == '"after_cleanup"')
    aliases = snapshot['base_subobject_aliases']
    def canonical(address):
        seen = set()
        while address in aliases and aliases[address] != address and address not in seen:
            seen.add(address)
            address = aliases[address]
        return address
    types = {canonical(a): n['type'].rsplit('::', 1)[-1] for a, n in snapshot['nodes'].items()
             if n['type'] != 'OHOS::AppExecFwk::EventHandler'}
    edges = []
    for edge in snapshot['edges']:
        if edge['ownership'] not in ('shared', 'unique'): continue
        edges.append(dict(edge, original_from=edge['from'], original_to=edge['to'],
                          **{'from': canonical(edge['from']), 'to': canonical(edge['to'])}))
    def cycle(start, sequence):
        def visit(address, remaining, path):
            if not remaining: return path if address == start else None
            for edge in edges:
                if edge['from'] == address and types.get(edge['to']) == remaining[0]:
                    found = visit(edge['to'], remaining[1:], path + [edge])
                    if found: return found
            return None
        return visit(start, sequence, [])
    proofs = []
    for address, typ in types.items():
        if typ == 'SimStateManager':
            for label, sequence in [('monitor_state', ['SimStateHandle', 'ObserverHandler', 'MultiSimMonitor', typ]),
                                    ('records_state', ['SimStateHandle', 'ObserverHandler', 'IccFile', typ])]:
                path = cycle(address, sequence)
                if path: proofs.append({'kind': label, 'root': address, 'edges': path})
        if typ == 'SimFileManager':
            path = cycle(address, ['IccFile', 'ObserverHandler', typ])
            if path: proofs.append({'kind': 'file_records', 'root': address, 'edges': path})
    retained = {c['address']: c for c in after['retained_control_blocks']}
    ril = next(c for c in retained.values() if c['type'].endswith('ITelRilManager'))
    holders = [e for e in edges if e['to'] == ril['address'] and e['from'] in retained and
               e['ownership'] == 'shared']
    counts = Counter(p['kind'] for p in proofs)
    verified = (not report['errors'] and counts == {'monitor_state': 3, 'records_state': 3, 'file_records': 3}
                and len(holders) == 9 and ril['use_count'] == len(holders) + 1
                and all(c['use_count'] is not None and c['use_count'] > 0 for c in retained.values()))
    return {'status': 'PASS' if verified else 'FAIL',
            'scope': 'ownership proof only; the GTest remains FAIL',
            'cycles': proofs, 'retained_controls': list(retained.values()),
            'ril_production_holders': holders, 'ril_fixture_roots': 1,
            'ril_observed_use_count': ril['use_count'],
            'native_threads_after_cleanup': len(after['threads']),
            'queue_worker_stacks': sum(any(f and 'ffrt::Worker(' in f for f in t['frames']) for t in after['threads'])}
