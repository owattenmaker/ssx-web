#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p local/browser-validation/include/emscripten
printf '#pragma once\n#define EMSCRIPTEN_KEEPALIVE\n' > local/browser-validation/include/emscripten/emscripten.h
/usr/bin/clang++ -std=c++20 -O2 -frounding-math -ffp-contract=off -fobjc-arc \
 -Ilocal/browser-validation/include -Iengine \
 -Ilocal/vendor/ModernGekko/vendor/dolphin/Externals/tinygltf/tinygltf \
 tests/browser_world_loader_reference.mm engine/world_collision_asset.mm web/world_bridge.cpp \
 -framework Foundation -o local/browser-validation/world-loader-reference
local/browser-validation/world-loader-reference local/assets/native/ARA1/world_collision.json local/assets/native/ARA1/terrain.json
