#!/usr/bin/env python3
"""Exercise the browser animation graph's channel fade contract."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
binary=root/'local/browser-validation/browser-channel-fade'
binary.parent.mkdir(parents=True,exist_ok=True)
subprocess.run(['xcrun','clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off',
 str(root/'tests/browser_channel_fade.cpp'),str(root/'engine/animation_sequence.cpp'),'-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True)
