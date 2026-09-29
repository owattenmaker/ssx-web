#!/usr/bin/env python3
"""MagnetModifier conformance (tests/magnet_modifier_reference.cpp): randomized
instruction-level oracles of the recompiled originals 0x3572E0 constructor, 0x3573F8
update (real 0x2D1C70 clock and 0x2D1B58 rider position), 0x357528/0x361A60 evaluate and
matrix getter, 0x357660 contact gate (real rider interface), 0x3568B0 entity bounds on the
live pickup entities and builtin90 0x305478 end to end (keyed-argument decode, 0x3559F8,
0x3554B0 attach), against engine/magnet_modifier.hpp, on The Junction pipe-run tick-2518
savestate image (14 live pickup magnets). Every case compares all 0xB0 modifier bytes.
usage: test_magnet_modifier_native.py [CASES]   Log: local/reference/magnet-modifier/reference.log."""
import subprocess, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_live_build import build_live, vtable_targets  # noqa: E402
FOLDER = ROOT / 'local/reference/magnet-modifier'
STATE = ROOT / 'local/ps2-capture/runs/pipe-run.tick2518.p2s'
RIDER_INTERFACE = 0x4583A8   # rider+0x6C0 vtable (0x140BC0 human flag, 0x1408F0 position, 0x140B80 index)


def main():
    FOLDER.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(STATE) as z:
        for suffix, member in [('ee', 'eeMemory.bin'), ('vuc', 'vu0MicroMem.bin'), ('vud', 'vu0Memory.bin')]:
            (FOLDER / f'state.{suffix}').write_bytes(z.read(member))
    roots = [0x3572E0, 0x3573F8, 0x357528, 0x361A60, 0x357660, 0x3568B0, 0x305478]
    roots += vtable_targets([(0x48F420, 0xE8), (0x490E80, 0x1A0), (0x491220, 0x30), (RIDER_INTERFACE, 0x100)])
    binary = ROOT / 'build/ssx3_magnet_modifier_reference'
    need = build_live(roots, ROOT / 'tests/magnet_modifier_reference.cpp', binary, FOLDER / 'reference-oracle',
                      cache=ROOT / 'build/magnet-modifier-reference-objects')
    cases = sys.argv[1] if len(sys.argv) > 1 else '20000'
    run = subprocess.run([str(binary), str(FOLDER), cases], capture_output=True, text=True, timeout=3600)
    log = f'{len(need)} original entries linked\n' + run.stdout + run.stderr
    (FOLDER / 'reference.log').write_text(log)
    print(log, end='')
    if run.returncode:
        raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
