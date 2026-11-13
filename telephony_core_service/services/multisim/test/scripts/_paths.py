"""Physical locations for test engineering; historic names stay in frozen locks only."""
import json
from pathlib import Path
TEST = Path(__file__).resolve().parents[1]
CORE = TEST.parents[2]
REPO = CORE.parent
_MAP = json.loads((TEST / 'docs/Test_Architecture_Migration.json').read_text())
_FILES = {r['old']: r['new'] for r in _MAP['files']}
_DIRS = {'systemtest': 'framework/host', 'sim_guard': 'cases/compatibility', 'multisim_architecture_rules': 'cases/architecture', 'systemtest/scripts': 'scripts', 'systemtest/tools': 'scripts', 'systemtest/primary_slot': 'cases/primary_slot', 'systemtest/primary_slot/lifecycle': 'cases/primary_slot/lifecycle'}
def locate(historic):
    return TEST / _FILES.get(historic, _DIRS.get(historic, historic))
def guard_asset(name):
    return locate('sim_guard/' + name)
