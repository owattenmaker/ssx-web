#!/bin/sh
# engine/ps2_fpu.hpp against PS2 hardware rows and ARMSX2's console model (docs/ps2-float.md "Validation").
# Needs the local copies under local/ps2-float: armsx2src (ARMSX2 2ecae98a2c sources, GPL-3.0) and ps2autotests.
# usage: tools/ps2-float/test_ps2_fpu.sh [random_per_generator] [exhaustive_scale]
set -eu
cd "$(dirname "$0")/../.."
L=local/ps2-float
mkdir -p "$L/bin" "$L/gen"
python3 tools/ps2-float/extract_console_rows.py "$L/armsx2src" "$L/ps2autotests" "$L/gen/console_rows.hpp"
clang++ -std=c++20 -O2 -Iengine -I"$L/gen" tests/ps2_fpu_console.cpp -o "$L/bin/ps2_fpu_console"
"$L/bin/ps2_fpu_console" | tail -1
clang++ -std=c++20 -O2 -ffp-contract=off -c -I"$L/shim" "$L/shim/FPU.cpp" -o "$L/bin/armsx2_fpu.o"
clang++ -std=c++20 -O2 -c -I"$L/shim" "$L/shim/arm64/FPU-divunit-arm64.cpp" -o "$L/bin/armsx2_divunit.o"
clang++ -std=c++20 -O2 -ffp-contract=off -Iengine -I"$L/shim" tests/ps2_fpu_fuzz.cpp "$L/bin/armsx2_fpu.o" "$L/bin/armsx2_divunit.o" -o "$L/bin/ps2_fpu_fuzz"
"$L/bin/ps2_fpu_fuzz" "${1:-200000}" "${2:-4}" | tail -1
