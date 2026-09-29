#!/bin/sh
# The fast toward-zero paths of engine/software_float.hpp (and the rider-context helpers built on them) against their
# reference definitions, natively and as WebAssembly, and on arm64 against the FPU itself in toward-zero +
# flush-to-zero mode (docs/sim-performance.md "Fast exact primitives").
# Usage: tools/test_exact_float.sh [native pairs per generator] [native sweep stride] [wasm pairs] [wasm sweep stride]
# (stride 1 = every binary32 against the fixed operands; the defaults take about a minute).
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
clang++ -std=c++20 -O2 -ffp-contract=off tests/exact_float_fast.cpp -o build/exact-float-fast
build/exact-float-fast "${1:-2000000}" "${2:-16}"
local/vendor/emsdk/upstream/emscripten/em++ tests/exact_float_fast.cpp -std=c++20 -O3 -ffp-contract=off -matomics -mbulk-memory -ftls-model=local-exec -o build/exact-float-fast.js
node build/exact-float-fast.js "${3:-500000}" "${4:-64}"
