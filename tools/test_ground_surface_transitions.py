#!/usr/bin/env python3
"""Verify catalog loading and crossing-frame material order in native riding."""
import subprocess,json,zipfile
from pathlib import Path
from reference_ground_profile import extract_ground

def main():
    root=Path(__file__).resolve().parents[1];folder=root/'local/reference/terrain/surface-ground';folder.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s') as z:fixture=extract_ground(z.read('eeMemory.bin'),0x14701a0)
    path=folder/'catalog-fixture.json';path.write_text(json.dumps(fixture,indent=2)+'\n')
    binary=root/'build/ssx3_ground_surface_transitions';command=['clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off',str(root/'tests/ground_surface_transitions.mm'),str(root/'engine/replay_io.mm')]
    command += [str(p) for p in (root/'build/metal-engine').glob('libssx3_*.a')]
    command += ['-framework','Foundation','-o',str(binary)];subprocess.run(command,check=True)
    subprocess.run([str(binary),str(path)],check=True)

if __name__=='__main__':main()
