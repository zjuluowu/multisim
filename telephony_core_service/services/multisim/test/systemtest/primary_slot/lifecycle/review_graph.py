#!/usr/bin/env python3
"""Recheck the four confirmed cycle families in a recorded read-only LLDB graph."""
import argparse
import hashlib
import json
from pathlib import Path
from summarize_graph import summarize


def review(graph):
    base = summarize(graph)
    snapshot = next(s for s in graph['snapshots'] if s['stage'] == 'manager_destructor_entry')
    aliases = snapshot['base_subobject_aliases']
    def canonical(address):
        seen = set()
        while address in aliases and aliases[address] != address and address not in seen:
            seen.add(address); address = aliases[address]
        return address
    types = {canonical(a): n['type'] for a, n in snapshot['nodes'].items()
             if n['type'] != 'OHOS::AppExecFwk::EventHandler'}
    edges = [dict(e, **{'from': canonical(e['from']), 'to': canonical(e['to'])})
             for e in snapshot['edges'] if e['ownership'] in ['shared', 'unique']]
    phonebook = []
    for root, typ in types.items():
        if not typ.endswith('::SimStateManager'): continue
        paths = [(root, [])]
        for target in ['SimStateHandle', 'ObserverHandler', 'IccDiallingNumbersManager', 'SimStateManager']:
            paths = [(e['to'], path + [e]) for owner, path in paths for e in edges
                     if e['from'] == owner and types.get(e['to'], '').endswith('::' + target)]
        for end, path in paths:
            if end == root: phonebook.append({'kind': 'phonebook_state', 'root': root, 'edges': path})
    return {'status': 'PASS' if base['status'] == 'PASS' and len(phonebook) == 3 else 'FAIL',
            'scope': 'four confirmed families only, not an exhaustive proof of every feature graph',
            'original_three_families': base['cycles'], 'phonebook_state': phonebook,
            'confirmed_cycle_count': len(base['cycles']) + len(phonebook),
            'retained_controls': base['retained_controls'], 'ril_production_holders': base['ril_production_holders']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--graph', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = review(json.loads(args.graph.read_text()))
    result['graph_sha256'] = hashlib.sha256(args.graph.read_bytes()).hexdigest()
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(result['status'], 'confirmed cycles:', result['confirmed_cycle_count'])
    return int(result['status'] != 'PASS')


if __name__ == '__main__':
    raise SystemExit(main())
