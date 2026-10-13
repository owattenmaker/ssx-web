#!/bin/sh
set -eu
# The rules module (docs/core-migration.md M5; web/rules-module.js): engine/game_rules.hpp (web/rules_bridge.cpp, the same rules the
# course core compiles) and the presentation generator (web/rules_module.cpp), a small wasm loaded at boot. RULES_OUT: build elsewhere
# (a scratch module) without touching web/runtime. Both files are swapped in together, as web/build-core.sh does.
cd "$(dirname "$0")/.."
out=${RULES_OUT:-web/runtime}
mkdir -p "$out"
tmp=$(mktemp -d "$out/.rules.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
EMXX=${EMXX:-local/vendor/emsdk/upstream/emscripten/em++}
"$EMXX" web/rules_bridge.cpp web/rules_module.cpp -Iengine -std=c++20 -O3 -ffp-contract=off -fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=1 \
  -sMODULARIZE -sEXPORT_ES6 -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=1048576 -sSTACK_SIZE=65536 \
  -sEXPORTED_FUNCTIONS='["_malloc","_free","_rules_eval","_presentation_boot","_presentation_booted","_presentation_draw","_presentation_note","_presentation_roster_seed","_presentation_state"]' \
  -sEXPORTED_RUNTIME_METHODS='["HEAPF32","HEAPU8"]' -o "$tmp/rules.js"
mv -f "$tmp/rules.wasm" "$out/rules.wasm" && mv -f "$tmp/rules.js" "$out/rules.js"
