"""Current physical roots; active code has no historical layout dependency."""
from pathlib import Path
TEST = Path(__file__).resolve().parents[1]
CORE = TEST.parents[2]
REPO = CORE.parent
