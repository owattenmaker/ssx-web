#!/bin/sh
# Build the EE oracle (native, no dependencies) to local/ps2-float/bin/oracle.
set -eu
cd "$(dirname "$0")/../../.."
mkdir -p local/ps2-float/bin
clang++ -std=c++20 -O2 -Wall -Iengine -Itools/ps2-float/ee_oracle tools/ps2-float/ee_oracle/oracle.cpp -o local/ps2-float/bin/oracle
echo local/ps2-float/bin/oracle
