#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mode="${1:-original}"
case "$mode" in original|nearest) ;; *) echo "Usage: $0 [original|nearest]" >&2; exit 2;; esac
mkdir -p local/browser-validation/include/emscripten
rounding_source=
if [ "$mode" = nearest ]; then
 cat > local/browser-validation/nearest-rounding.cpp <<'ROUNDING'
// Diagnostic executable only: WebAssembly does not support directed rounding.
// A fresh process starts in nearest mode; intercept every rounding request.
extern "C" int fesetround(int) { return 0; }
ROUNDING
 rounding_source=local/browser-validation/nearest-rounding.cpp
fi
cat > local/browser-validation/include/emscripten/emscripten.h <<'HEADER'
#pragma once
#define EMSCRIPTEN_KEEPALIVE
HEADER
/usr/bin/clang++ -std=c++20 -O2 -frounding-math -ffp-contract=off \
 -Ilocal/browser-validation/include -Iengine \
 -Ilocal/vendor/ModernGekko/vendor/dolphin/Externals/tinygltf/tinygltf \
 $rounding_source web/core.cpp web/race_bridge.cpp web/world_bridge.cpp web/prediction_bridge.cpp web/native-trace.cpp \
 engine/jump_motion.cpp engine/air_entry.cpp engine/air_control.cpp engine/ground_motion.cpp engine/ground_pose_motion.cpp engine/soft_collision_control.cpp engine/air_motion.cpp \
 engine/orientation_motion.cpp engine/race_event.cpp engine/boost_control.cpp engine/landing_motion.cpp engine/air_trajectory.cpp engine/air_alignment.cpp \
 -o "local/browser-validation/native-trace-$mode"
"local/browser-validation/native-trace-$mode" web/public/assets/ARA1/collision.bin web/public/assets/ARA1/terrain.json web/public/assets/ARA1/world_collision.json web/public/assets/ARA1/start.json > "local/browser-validation/native-$mode.csv"
cd web
node compare-native.mjs "$mode"
