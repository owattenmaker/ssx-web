#!/usr/bin/env python3
"""LiveComp animation conformance (tests/livecomp_animation_reference.cpp): randomized
instruction-level oracles of the recompiled originals on a synthetic EE image against
engine/livecomp_animation.hpp: 341AA0 constructor (34D9B0, 355280, 34E448, 351508, 34E348,
317830/317890), 341D48 tick (341E48/341EC0/341F38, 34EBA0) and 361098 node matrices
(34DC90 -> 351800/351538/351A80/31BE50, 34DD18).
Log: local/reference/livecomp-animation/reference.log."""
import subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_live_build import build_live  # noqa: E402
FOLDER = ROOT / 'local/reference/livecomp-animation'


def main():
    FOLDER.mkdir(parents=True, exist_ok=True)
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    ee = bytearray(32 * 1024 * 1024); ee[0xFF000:0xFF000 + len(elf)] = elf
    (FOLDER / 'synthetic.ee').write_bytes(bytes(ee)); (FOLDER / 'synthetic.vuc').write_bytes(bytes(4096)); (FOLDER / 'synthetic.vud').write_bytes(bytes(4096))
    # jalr targets reached: entity vtable 0x490B10 +0x198 34DC90, +0x1A0 34E348, +0xC0 356078,
    # +0x108 34DD18, +0x110 341FC8; channel vtable 0x490AD0 +0x10 351800.
    roots = [0x341AA0, 0x341D48, 0x361098, 0x341FC8, 0x34DC90, 0x34DD18, 0x34E348, 0x356078, 0x351800]
    binary = ROOT / 'build/ssx3_livecomp_animation_reference'
    need = build_live(roots, ROOT / 'tests/livecomp_animation_reference.cpp', binary, FOLDER / 'reference-oracle',
                      cache=ROOT / 'build/livecomp-animation-reference-objects')
    run = subprocess.run([str(binary), str(FOLDER)], capture_output=True, text=True, timeout=3600)
    log = f'{len(need)} original entries linked\n' + run.stdout + run.stderr
    (FOLDER / 'reference.log').write_text(log); print(log, end='')
    if run.returncode: raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
