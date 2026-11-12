"""Technical positive/negative checks only; not Target business execution."""
import copy
import json
from pathlib import Path
import unittest
from primary_oracle import evaluate, compare, PRIMARY, DATA, ICCID, TARGET_ICCID, MAIN, DATA_EVENT

CATALOG = json.loads((Path(__file__).resolve().parents[1] / 'scenarios/primary_slot.json').read_text())
C0 = next(s for s in CATALOG['scenarios'] if '.C0' in s['legacy_test'])


def example():
    def event(kind, **fields):
        return dict(kind=kind, fields={k: str(v) for k, v in fields.items()})
    return {'schema': 1, 'public': {'primary': '1', 'data': '1', 'primary_result': '0', 'switching': 'false'},
            'parameters': {PRIMARY: '1', DATA: '1', ICCID: TARGET_ICCID}, 'events': [
                event('api_input', slot=1, at_us=0),
                event('parameter_write', key=PRIMARY, value=1, at_us=10),
                event('publication', action=MAIN, slot=1, sim=1, done='false', user='false', at_us=11),
                event('publication', action=DATA_EVENT, slot=-1, sim=1, done='false', user='false', at_us=12),
                event('api_return', symbol='TELEPHONY_ERR_SUCCESS', result=0, at_us=13)]}


class PrimaryOraclePolicy(unittest.TestCase):
    def test_complete_observation(self):
        self.assertEqual(evaluate(C0, example())['status'], 'PASS')

    def test_missing_measurements_fail(self):
        for obj in ({}, {'schema': 1}, dict(example(), events=[])):
            self.assertEqual(evaluate(C0, obj)['status'], 'FAIL')

    def test_wrong_return(self):
        o = example(); o['events'][-1]['fields']['symbol'] = 'TELEPHONY_ERR_LOCAL_PTR_NULL'
        self.assertEqual(evaluate(C0, o)['status'], 'FAIL')

    def test_wrong_target(self):
        o = example(); o['events'][0]['fields']['slot'] = '0'
        self.assertEqual(evaluate(C0, o)['status'], 'FAIL')

    def test_wrong_final_role(self):
        o = example(); o['public']['primary'] = '0'
        self.assertEqual(evaluate(C0, o)['status'], 'FAIL')

    def test_publication_order(self):
        o = example(); o['events'][2:4] = reversed(o['events'][2:4])
        self.assertEqual(evaluate(C0, o)['status'], 'FAIL')

    def test_unexpected_device_request(self):
        o = example(); o['events'].insert(1, {'kind': 'ril_primary', 'fields': {'slot': '1'}})
        self.assertEqual(evaluate(C0, o)['status'], 'FAIL')

    def test_publication_identity(self):
        o = example(); o['events'][2]['fields']['sim'] = '2'
        self.assertEqual(evaluate(C0, o)['status'], 'FAIL')

    def test_persistence_hash(self):
        o = example(); o['parameters'][ICCID] = 'wrong'
        self.assertEqual(evaluate(C0, o)['status'], 'FAIL')

    def test_return_before_publication(self):
        o = example(); o['events'].insert(2, o['events'].pop())
        self.assertEqual(evaluate(C0, o)['status'], 'FAIL')

    def test_noncontract_fields_do_not_cause_diff(self):
        a, b = example(), example(); b['events'][2]['fields']['user'] = 'true'
        b['events'][0]['fields']['thread'] = 'another implementation'
        self.assertEqual(compare(evaluate(C0, a), evaluate(C0, b))['status'], 'PASS')

    def test_legacy_fail_target_pass_requires_adjudication(self):
        self.assertEqual(compare({'status': 'FAIL'}, evaluate(C0, example()))['status'], 'BLOCKED')

    def test_new_regression_fails(self):
        self.assertEqual(compare(evaluate(C0, example()), {'status': 'FAIL'})['status'], 'FAIL')

    def test_different_scenarios_blocked(self):
        a = evaluate(C0, example()); b = copy.deepcopy(a); b['normalized']['scenario_id'] = 'different'
        self.assertEqual(compare(a, b)['status'], 'BLOCKED')

    def test_actual_differences_are_not_ignored(self):
        a = evaluate(C0, example()); b = copy.deepcopy(a); b['normalized']['parameters'][DATA] = '0'
        self.assertEqual(compare(a, b)['status'], 'FAIL')


if __name__ == '__main__':
    unittest.main()
