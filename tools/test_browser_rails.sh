#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build local/browser-validation/include/emscripten
cat > local/browser-validation/include/emscripten/emscripten.h <<'HEADER'
#pragma once
#define EMSCRIPTEN_KEEPALIVE
HEADER
clang++ -std=c++20 -O2 -frounding-math -ffp-contract=off -Ilocal/browser-validation/include -Ilocal/vendor/ModernGekko/vendor/dolphin/Externals/tinygltf/tinygltf tests/rail_query_browser_reference.cpp tests/rail_query_browser_stubs.cpp web/rail_bridge.cpp -o build/rail-query-browser-reference
build/rail-query-browser-reference web/public/assets/ARA1/rails.json local/browser-validation/rail-queries.bin
cd web
node test-rails.mjs
