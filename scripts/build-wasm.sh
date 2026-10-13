#!/usr/bin/env sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
if ! command -v emcc >/dev/null 2>&1; then
  echo "ERROR: emcc was not found. Install and activate the Emscripten SDK first." >&2
  echo "Guide: https://emscripten.org/docs/getting_started/downloads.html" >&2
  exit 127
fi
cd "$ROOT"
emcc wasm/minifs_wasm.c \
  -std=c11 -O2 -Wall -Wextra \
  -o web/minifs-c.js \
  -s MODULARIZE=1 \
  -s EXPORT_NAME=MiniFSCModule \
  -s ENVIRONMENT=web \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s FILESYSTEM=1 \
  -s ASSERTIONS=1 \
  -s EXPORTED_FUNCTIONS='["_minifs_init","_minifs_execute","_minifs_export_tree_json","_minifs_get_cwd","_minifs_free","_malloc","_free"]' \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString"]'
echo "Built web/minifs-c.js and web/minifs-c.wasm"
