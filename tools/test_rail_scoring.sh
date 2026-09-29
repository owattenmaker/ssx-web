#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p local/browser-validation
python3 tools/test_rail_score_native.py
local/vendor/emsdk/upstream/emscripten/em++ tests/rail_score_wasm.cpp -std=c++20 -O3 -ffp-contract=off -sMODULARIZE -sEXPORT_ES6 -sEXPORTED_FUNCTIONS='["_malloc","_free","_rail_score_rows"]' -sEXPORTED_RUNTIME_METHODS='["HEAPU8","HEAPU32"]' -o local/browser-validation/rail-score.mjs
node tools/test_rail_score_wasm.mjs
