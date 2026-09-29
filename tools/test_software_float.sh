#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build local/browser-validation
clang++ -std=c++20 -O2 -frounding-math -ffp-contract=off tests/software_float_reference.cpp -o build/software-float-reference
build/software-float-reference local/browser-validation/float-corpus.bin
local/vendor/emsdk/upstream/emscripten/em++ tests/software_float_wasm.cpp -std=c++20 -O3 -ffp-contract=off -sMODULARIZE -sEXPORT_ES6 -sEXPORTED_FUNCTIONS='["_malloc","_free","_evaluate","_rounding_scope_test"]' -sEXPORTED_RUNTIME_METHODS='["HEAPU8","HEAPU32"]' -o local/browser-validation/software-float.mjs
node tools/test_software_float.mjs
