#!/usr/bin/env bash
set -euo pipefail
if [ "$#" -eq 0 ]; then set -- all; fi
exec python3 -B "$(dirname "${BASH_SOURCE[0]}")/host.py" "$@"
