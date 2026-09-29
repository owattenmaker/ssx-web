#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
# Build web/runtime/core.js first when any gameplay C++ source has changed.
python3 tools/build_gameplay_trace.py
node web/compare-gameplay.mjs --prepare
local/browser-validation/native-gameplay-trace web/public/assets local/browser-validation/gameplay-inputs.json local/browser-validation/native-gameplay.bin
node web/compare-gameplay.mjs
