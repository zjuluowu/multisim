"""Platform-free oracle for the explicitly scoped primary-slot Host contract.

Input is measured boundary data, never Golden-generated outcomes. Unit-test fixtures
are technical oracle tests; they cannot confer Target business execution status.
"""
PRIMARY = 'persist.telephony.MainSlotId'
DATA = 'persist.telephony.MainCellularDataSlotId'
ICCID = 'persist.telephony.MainCard.Iccid'
PREFERENCE = 'persist.telephony.userPreferPrimarySlot'
TARGET_ICCID = 'AB2FA72FFEABDAB4E56CB9230C9A119AB236ECE269144D4E335A36E2D26558F2'
MAIN = 'usual.event.SIM.DEFAULT_MAIN_SUBSCRIPTION_CHANGED'
DATA_EVENT = 'usual.event.SIM.DEFAULT_DATA_SUBSCRIPTION_CHANGED'
STATUS = 'usual.event.SET_PRIMARY_SLOT_STATUS'


def evaluate(scenario, observation):
    """Return differences, not exceptions, for incomplete or invalid observations."""
    errors = []
    def require(ok, name):
        if not ok:
            errors.append(name)
    try:
        expected = scenario['expect']
        if scenario['branch'] == 'initialization failure boundary':
            require(observation['api_symbol'] == expected['error'], 'unavailable-RIL error')
            require(observation['max_sim_count'] == 2, 'reported physical slots')
            return {'status': 'FAIL' if errors else 'PASS', 'differences': errors,
                    'normalized': dict(observation, scenario_id=scenario['id'])}
        require(observation['schema'] == 1, 'observation schema')
        events = observation['events']
        require(isinstance(events, list) and bool(events), 'nonempty measured events')
        of = lambda kind: [(i, e['fields']) for i, e in enumerate(events) if e['kind'] == kind]
        inputs, returns = of('api_input'), of('api_return')
        require(len(inputs) == len(returns) == 1, 'one input and one return')
        require(inputs[0][0] < returns[0][0], 'input precedes return')
        require(int(inputs[0][1]['slot']) == scenario['input']['slot'], 'input target')
        require(returns[0][1]['symbol'] == expected['error'], 'API error symbol')
        elapsed = (int(returns[0][1]['at_us']) - int(inputs[0][1]['at_us'])) / 1e6
        require(elapsed >= expected['minimum_api_seconds'], 'real API timeout lower bound')
        state, params = observation['public'], observation['parameters']
        require(state['switching'] == 'false', 'terminal switch flag')
        require(int(state['primary_result']) == 0, 'public primary query result')
        require(int(state['primary']) == expected['primary'], 'public primary')
        # The frozen failure tests assert no data commit to slot1, rather than a new data-role rule.
        require(int(state['data']) == expected['data'], 'public data role')
        require(params[PRIMARY] == str(expected['primary']), 'primary parameter')
        require(params[DATA] == str(expected['data']), 'data parameter')
        pubs = [(i, p) for i, p in of('publication') if p['action'] in (MAIN, DATA_EVENT, STATUS)]
        role_pubs = [(i, p) for i, p in pubs if p['action'] != STATUS]
        require([p['action'] for _, p in role_pubs] == ([MAIN, DATA_EVENT] if expected['success_publications'] else []), 'role publication order/count')
        for _, p in role_pubs:
            require(p['sim'] == '1', 'publication subscription identity')
            if p['action'] == MAIN:
                require(p['slot'] == '1', 'publication target slot')
        done = [(i, p) for i, p in pubs if p['action'] == STATUS]
        require([p['done'] for _, p in done] == ['false', 'true'] * expected['attempts'], 'attempt completion sequence')
        require(all(p['user'] == str(expected['status_user_set']).lower() for _, p in done), 'status userSet projection')
        direct = [p for _, p in of('ril_primary')]
        require(len(direct) == expected['ril_primary_count'] and all(p['slot'] == '1' for p in direct), 'dedicated RIL requests')
        protocols = [p for _, p in of('ril_protocol')]
        sessions = []
        for p in protocols:
            if p['session'] not in sessions:
                sessions.append(p['session'])
        require(len(sessions) == expected['protocol_attempts'], 'protocol attempt/session count')
        normalized_protocols = []
        for attempt, session in enumerate(sessions):
            group = [p for p in protocols if p['session'] == session]
            # Cross-slot order inside one phase is not a business contract.
            require([int(p['phase']) for p in group] == [v for phase in expected['protocol_phases'] for v in (phase, phase)], 'phase barrier sequence')
            for phase in expected['protocol_phases']:
                members = [p for p in group if int(p['phase']) == phase]
                require(sorted(int(p['slot']) for p in members) == [0, 1], 'phase participants')
                for p in members:
                    slot = int(p['slot'])
                    modem = slot if phase == 1 or not expected['success_publications'] else 1 - slot
                    require(int(p['modem']) == modem, 'device resource mapping')
                normalized_protocols += [dict(attempt=attempt, phase=phase,
                    slot=int(p['slot']), modem=int(p['modem']), technology=int(p['technology']))
                    for p in sorted(members, key=lambda p: int(p['slot']))]
        require(len(protocols) == 2 * len(expected['protocol_phases']) * expected['protocol_attempts'], 'protocol request cardinality')
        responses = of('ril_response')
        if expected['ril_primary_count']:
            require(len(responses) == (0 if expected['minimum_api_seconds'] else 1), 'dedicated reply count')
            if responses:
                response_index, response = responses[0]
                require(response_index < returns[0][0] and response['slot'] == '1', 'reply before return, correct slot')
                require((int(response['error']) == 0) == expected['success_publications'], 'external reply error')
                if expected['success_publications']:
                    require(all(i > response_index for i, w in of('parameter_write') if w['key'] in (PRIMARY, DATA, ICCID)), 'no early success persistence')
        if expected['protocol_phases'] == [1, 2, 4]:
            stale = [(i, p) for i, p in responses if p.get('session') not in sessions]
            require(len(stale) == 1, 'one measured stale response')
            if stale:
                i, _ = stale[0]
                valid = [j for j, p in responses if j > i and p.get('session') in sessions and p.get('phase') == '1']
                require(bool(valid), 'stale followed by real CHECK response')
                if valid:
                    require(not any(i < j < valid[0] and p['phase'] != '1' for j, p in of('ril_protocol')), 'stale response cannot advance phase')
            require(sorted(int(p['phase']) for _, p in responses if p.get('session') in sessions) == [1, 1, 3, 3, 4, 4], 'CHECK replies, UPDATE NOTIFY and COMPLETE replies')
        writes = [p for _, p in of('parameter_write')]
        if expected['success_publications']:
            require(params[ICCID] == TARGET_ICCID, 'target ICCID hash')
            require(any(p['key'] == PRIMARY and p['value'] == '1' for p in writes), 'actual primary write')
        elif 'C1' in scenario['legacy_test']:
            require(params[ICCID] == 'previous-parameter-value', 'ICCID rollback')
            require(any(p['key'] == PRIMARY and p['value'] == '1' for p in writes), 'speculative write observed')
            require(any(p['key'] == PRIMARY and p['value'] == '0' for p in writes), 'rollback write observed')
        else:
            require(not any(p['key'] in (PRIMARY, DATA) and p['value'] == '1' for p in writes), 'no failure commit')
        if role_pubs:
            require(role_pubs[-1][0] < returns[0][0], 'publication precedes successful return')
        normalized = {
            'scenario_id': scenario['id'], 'api_symbol': returns[0][1]['symbol'], 'target': int(inputs[0][1]['slot']),
            'public': {k: state[k] for k in ('primary', 'data', 'switching')},
            'parameters': {k: params[k] for k in (PRIMARY, DATA)},
            'publications': [{k: p[k] for k in (('action', 'done', 'user') if p['action'] == STATUS else ('action', 'slot', 'sim') if p['action'] == MAIN else ('action', 'sim'))} for _, p in pubs],
            'parameter_writes': [{k: p[k] for k in ('key', 'value')} for p in writes if p['key'] in (PRIMARY, DATA, ICCID, PREFERENCE)],
            'ril_primary': [{'slot': int(p['slot'])} for p in direct],
            'protocol': normalized_protocols,
            'timing': 'at_least_44_seconds' if expected['minimum_api_seconds'] else 'not_compared',
        }
        if expected['success_publications'] or 'C1' in scenario['legacy_test']:
            normalized['parameters'][ICCID] = params[ICCID]
        return {'status': 'FAIL' if errors else 'PASS', 'differences': errors, 'normalized': normalized}
    except (KeyError, IndexError, TypeError, ValueError) as exc:
        return {'status': 'FAIL', 'differences': errors + ['missing/invalid measurement: ' + str(exc)]}


def compare(left, right):
    """Pure comparison; does not assert either implementation was executed."""
    if left.get('status') == 'PASS' and right.get('status') == 'FAIL':
        return {'status': 'FAIL', 'reason': 'Target violates the common contract'}
    if left.get('status') != 'PASS' or right.get('status') != 'PASS':
        return {'status': 'BLOCKED', 'reason': 'both measured contracts must pass before comparison'}
    a, b = left['normalized'], right['normalized']
    if a.get('scenario_id') != b.get('scenario_id'):
        return {'status': 'BLOCKED', 'reason': 'different scenarios cannot be compared'}
    keys = sorted(set(a) | set(b))
    differences = [k for k in keys if a.get(k) != b.get(k)]
    return {'status': 'FAIL' if differences else 'PASS', 'differences': differences,
            'scope': 'offline normalized observation comparison; not execution provenance'}
