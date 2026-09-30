"""Bounded filesystem timestamp convergence before GN on Windows-mounted sources."""
import time

def await_build_timestamps(system, budget_seconds=30):
    inputs = [system / '.gn', system / 'BUILD.gn', system / 'gn/BUILD.gn', system / 'gn/BUILDCONFIG.gn']
    started = time.monotonic()
    initial_skew = max(path.stat().st_mtime for path in inputs) - time.time()
    while True:
        skew = max(path.stat().st_mtime for path in inputs) - time.time()
        if skew <= 0:
            return {'initial_future_skew_seconds': max(0, initial_skew),
                    'wait_seconds': time.monotonic() - started, 'converged': True}
        if time.monotonic() - started >= budget_seconds:
            raise RuntimeError('GN inputs have future timestamps; build clock convergence timed out')
        # Build preparation only, condition driven; never a SIM business Oracle.
        time.sleep(min(skew, 0.05))
