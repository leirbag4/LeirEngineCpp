#!/usr/bin/env bash
# Build + serve the WebEngineDemo (full engine in browser, port 8001).
set -euo pipefail
cd "$(dirname "$0")/.."
PY="python3"
if ! command -v "$PY" >/dev/null 2>&1; then
  for d in "$HOME/emsdk/python/"*/; do
    if [ -x "$d/bin/python3" ]; then PY="$d/bin/python3"; break; fi
  done
fi
exec "$PY" scripts/export_web.py --demo engine "$@"