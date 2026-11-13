#!/usr/bin/env python3
"""Compile current SIM external declaration contracts and reject representative ABI drift.

This is an L1 declaration guard. It does not validate vtable slots, vendor symbols,
product toolchain/layout, shared-library identity, or any runtime behavior.
"""

import sys as _sys
from pathlib import Path as _Path
_TEST_ROOT = next(p for p in _Path(__file__).resolve().parents if (p / "scripts/_paths.py").is_file())
_sys.path.insert(0, str(_TEST_ROOT / "scripts"))
from _paths import TEST as _TEST, CORE as _CORE, REPO as _REPO, locate, guard_asset
_sys.path.insert(0, str(_TEST / "cases/architecture"))
import argparse
from pathlib import Path
import subprocess
import tempfile


def check(compiler="clang++"):
    tests = (_TEST / 'cases/architecture')
    component = _CORE
    includes = [
        _TEST / "framework/host/stubs/include",
        component / "interfaces/innerkits/include",
        component / "interfaces/innerkits/ims/include",
        component / "interfaces/innerkits/satellite",
        component / "services/telephony_ext_wrapper/include",
        component / "services/network_search/include",
        component / "services/tel_ril/include",
        component / "utils/common/include",
        component / "utils/log/include",
    ]
    command = [compiler, "-std=c++17", "-fsyntax-only", "-DLOG_DOMAIN=0xD001F04"]
    for path in includes:
        command.extend(["-I", str(path)])
    unit = str(tests / "open_closed_contract_rules.cpp")
    subprocess.run(command + [unit], check=True)
    # Each negative control changes a real copied public header. The compiler must
    # reject a static assertion, not merely fail for a missing platform dependency.
    mutations = [
        ("radio_event.h", component / "interfaces/innerkits/include/radio_event.h",
         "    RADIO_SIM_ACCOUNT_LOADED,", "    RADIO_NEW_EVENT,\n    RADIO_SIM_ACCOUNT_LOADED,"),
        ("sim_state_type.h", component / "interfaces/innerkits/include/sim_state_type.h",
         "    PIN_ENABLE,\n    PIN_DISABLE,", "    PIN_DISABLE,\n    PIN_ENABLE,"),
        ("i_icc_file.h", component / "interfaces/innerkits/include/i_icc_file.h",
         "        GID1_FILE_LOAD,\n        GID2_FILE_LOAD,", "        GID2_FILE_LOAD,\n        GID1_FILE_LOAD,"),
        ("telephony_ext_wrapper.h", component / "services/telephony_ext_wrapper/include/telephony_ext_wrapper.h",
         "typedef void (*CacheAssetPinForUpgrade)(", "typedef bool (*CacheAssetPinForUpgrade)("),
        ("i_icc_file.h", component / "interfaces/innerkits/include/i_icc_file.h",
         "const std::string opName)", "const std::string &opName)"),
    ]
    with tempfile.TemporaryDirectory(prefix="sim-contract-negative-") as temporary:
        for index, (name, source, before, after) in enumerate(mutations):
            text = source.read_text()
            if text.count(before) != 1:
                raise RuntimeError(f"Negative-control anchor is no longer unique: {name}/{index}")
            directory = Path(temporary) / str(index)
            directory.mkdir()
            (directory / name).write_text(text.replace(before, after, 1))
            result = subprocess.run(command[:1] + ["-I", str(directory)] + command[1:] + [unit],
                                    capture_output=True, text=True)
            if result.returncode == 0 or "static assertion failed" not in result.stderr:
                raise RuntimeError(f"Declaration drift was not detected by assertions: {name}/{index}\n"
                                   + result.stdout + result.stderr)
    print("open/closed declarations: PASS (enum/type assertions; 5 compiler negative controls; not product ABI)",
          flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="clang++")
    check(parser.parse_args().compiler)
