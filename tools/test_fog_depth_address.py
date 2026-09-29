#!/usr/bin/env python3
"""Cross-check the recovered fog strip pass against local GS memory addressing."""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
vendor=ROOT/'local/vendor/PS2Recomp/ps2xRuntime'
binary=ROOT/'build/ssx3_fog_depth_address_reference'
subprocess.run(['xcrun','clang++','-std=c++20','-O2','-Wno-switch','-I'+str(vendor/'include'),str(ROOT/'tests/fog_depth_address_reference.cpp'),str(vendor/'src/lib/gs/ps2_gs_memory.cpp'),'-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True)
