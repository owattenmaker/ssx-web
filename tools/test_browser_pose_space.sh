#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p local/browser-validation
clang++ -std=c++20 -O2 -ffp-contract=off tests/browser_pose_space.cpp \
 engine/animation_motion.cpp engine/rider_pose_motion.cpp \
 engine/orientation_motion.cpp engine/ground_motion.cpp \
 engine/ground_animation_control.cpp -o local/browser-validation/pose-space
local/browser-validation/pose-space
