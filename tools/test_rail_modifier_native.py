#!/usr/bin/env python3
"""AnimTeeter / RailModifier conformance (tests/rail_modifier_reference.cpp): randomized
instruction-level oracles of the recompiled originals on a synthetic EE image (the ELF loaded
at its addresses) against engine/rail_modifier.hpp: 3610E0 node matrices (34DC90, 34E348,
351800, 351538, 351A80, 31BE50, 34DD18), 342358 teeter update, 342538 apply force, 3421A0
AnimTeeter ctor (34D9B0, 355280, 34E448, 351508), 35B708 RailModifier ctor (35C0E8, 35C040),
35C4E0 tick, 35C5A0 transform (34FED8) and the 35C698 type-2 rail query.
Log: local/reference/rail-modifier/reference.log."""
import subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_live_build import build_live, vtable_targets  # noqa: E402
FOLDER = ROOT / 'local/reference/rail-modifier'


def synthetic_image():
    FOLDER.mkdir(parents=True, exist_ok=True)
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    ee = bytearray(32 * 1024 * 1024)
    ee[0xFF000:0xFF000 + len(elf)] = elf
    (FOLDER / 'synthetic.ee').write_bytes(bytes(ee))
    (FOLDER / 'synthetic.vuc').write_bytes(bytes(4096))
    (FOLDER / 'synthetic.vud').write_bytes(bytes(4096))


def main():
    synthetic_image()
    roots = [0x3610E0, 0x342358, 0x342538, 0x3421A0, 0x35B708, 0x35C4E0, 0x35C5A0, 0x35C698, 0x35C0E8, 0x35C040]
    roots += vtable_targets([(0x4908F8, 0x1B0), (0x490AD0, 0x40), (0x4911D0, 0x50)])
    binary = ROOT / 'build/ssx3_rail_modifier_reference'
    need = build_live(roots, ROOT / 'tests/rail_modifier_reference.cpp', binary, FOLDER / 'reference-oracle',
                      cache=ROOT / 'build/rail-modifier-reference-objects')
    run = subprocess.run([str(binary), str(FOLDER)], capture_output=True, text=True, timeout=3600)
    log = f'{len(need)} original entries linked\n' + run.stdout + run.stderr
    (FOLDER / 'reference.log').write_text(log)
    print(log, end='')
    if run.returncode:
        raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
