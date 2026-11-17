"""Fail-closed business/lifecycle classification; no build or execution code."""
import re
def classify(case, returncode, log):
    """Only an exact cleanup-only failure may coexist with business PASS."""
    props = {p.get('name'): p.get('value') for p in case.findall('./properties/property')}
    failures = [f.get('message', '') + (f.text or '') for f in case.findall('failure')]
    cleanup_pattern = re.compile(
        r'^.*(?:/|^)(?:real_scenario|primary_environment)\.h:\d+\nValue of: '
        r'(?:SimHost::AwaitSchedulerStopped\(3s\)|retiring\.expired\(\))\n'
        r'  Actual: false\nExpected: true\nREAL_(?:GRAPH|MANAGER)_LIFETIME_NOT_DRAINED\n?$')
    clean_failure = bool(failures) and all(
        cleanup_pattern.fullmatch(f.get('message', '')) for f in case.findall('failure'))
    completed = (case.get('status') == 'run' and case.get('result') == 'completed' and
                 case.find('skipped') is None)
    unsafe = any(s in log for s in ('AddressSanitizer:', 'ThreadSanitizer:',
                                    'runtime error:', 'Segmentation fault', 'Aborted'))
    full_pass = completed and returncode == 0 and not failures and not unsafe
    # Signals, timeout, missing/contradictory XML and other assertion failures fail closed.
    business_pass = full_pass or (completed and returncode == 1 and clean_failure and not unsafe and
        props.get('business_assertions_before_cleanup') == 'PASS' and
        props.get('strict_ril_expectations_verified') == 'PASS' and
        props.get('object_lifetime') == 'FAIL')
    precondition_failure = (completed and returncode == 1 and not unsafe and bool(failures) and
        not clean_failure and 'business_assertions_before_cleanup' not in props and
        'TRACE input request=' not in log)
    bypassed = props.get('initialization_ready_idle') == 'NOT_RUN'
    if bypassed:
        business_pass = False
    return {
        'gtest': 'PASS' if full_pass else 'FAIL',
        'business': 'PASS' if business_pass else 'NOT_RUN' if precondition_failure or bypassed else 'FAIL',
        'lifecycle': props.get('object_lifetime', 'PASS' if full_pass else 'NOT_RUN'),
        'classification': 'diagnostic_barrier_not_run' if bypassed else
                          'cleanup_only' if business_pass and clean_failure else
                          'full_pass' if full_pass else 'precondition_failure' if precondition_failure else
                          'business_or_execution_failure',
        'properties': props, 'failures': failures,
        'post_destruction_callback_safety': 'NOT_VERIFIED' if clean_failure else 'NOT_RUN',
    }
