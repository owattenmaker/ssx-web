#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 tools/test_lighting_brightness_native.py
local/vendor/emsdk/upstream/emscripten/em++ tests/lighting_brightness_wasm.cpp -std=c++20 -O3 -ffp-contract=off -fexceptions -sDISABLE_EXCEPTION_CATCHING=0 -sMODULARIZE -sEXPORT_ES6 -sEXPORTED_FUNCTIONS='["_malloc","_free","_lighting_brightness_batch"]' -sEXPORTED_RUNTIME_METHODS='["HEAPU8","HEAPU32"]' -o local/browser-validation/lighting-brightness.mjs
node tools/test_lighting_brightness_wasm.mjs
