#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p local/browser-validation/include/emscripten
printf '#pragma once\n#define EMSCRIPTEN_KEEPALIVE\n' > local/browser-validation/include/emscripten/emscripten.h
/usr/bin/clang++ -std=c++20 -O2 -frounding-math -ffp-contract=off -fobjc-arc \
 -Ilocal/browser-validation/include -Iengine \
 -Ilocal/vendor/ModernGekko/vendor/dolphin/Externals/tinygltf/tinygltf \
 tests/pickup_contact_probe.mm engine/world_collision_asset.mm web/world_bridge.cpp \
 -framework Foundation -o local/browser-validation/pickup-contact-probe
local/browser-validation/pickup-contact-probe local/assets/native/ARA1/world_collision.json local/browser-pickups/ara1-catalog.json
