#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build local/browser-validation
clang++ -std=c++20 -O2 -frounding-math -ffp-contract=off tests/air_motion_rounding.cpp engine/air_motion.cpp -o build/air-motion-rounding
build/air-motion-rounding local/browser-validation/air-input.bin local/browser-validation/air-expected.bin
local/vendor/emsdk/upstream/emscripten/em++ tests/air_motion_rounding.cpp engine/air_motion.cpp -std=c++20 -O3 -ffp-contract=off -sMODULARIZE -sEXPORT_ES6 -sEXPORTED_FUNCTIONS='["_malloc","_free","_air_evaluate"]' -sEXPORTED_RUNTIME_METHODS='["HEAPU8","HEAPU32"]' -o local/browser-validation/air-rounding.mjs
node tools/test_air_motion_rounding.mjs
