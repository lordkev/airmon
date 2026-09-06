#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -z "${IDF_PATH:-}" ]]; then
  export IDF_TOOLS_PATH="$ROOT/.tools/idf"
  source "$ROOT/.tools/esp-idf/export.sh" >/dev/null
fi
python "$ROOT/scripts/embed_web.py"
cd "$ROOT/firmware"
idf.py "$@" build
