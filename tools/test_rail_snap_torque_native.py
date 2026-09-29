#!/usr/bin/env python3
"""Rail snap (0x106F78) entity side conformance (tests/rail_snap_torque_reference.cpp): randomized
instruction-level oracles of the recompiled originals on a synthetic EE image against
engine/rail_snap_torque.hpp: A 0x34E698 AnimTeeter contact velocity (0x34E798 node velocity table,
0x351660, 0x3424D0, 0x34E600, 0x3610E0), B the complete 0x106F78 with a real teeter entity behind
the rail (entity vtable+0x154 0x34E698, +0x15C 0x342538, real 0x1231A8; other callees stubbed).
Log: local/reference/rail-snap-torque/reference.log."""
import subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_live_build import build_live, vtable_targets  # noqa: E402
FOLDER = ROOT / 'local/reference/rail-snap-torque'


def main():
    FOLDER.mkdir(parents=True, exist_ok=True)
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    ee = bytearray(32 * 1024 * 1024); ee[0xFF000:0xFF000 + len(elf)] = elf
    (FOLDER / 'synthetic.ee').write_bytes(bytes(ee)); (FOLDER / 'synthetic.vuc').write_bytes(bytes(4096)); (FOLDER / 'synthetic.vud').write_bytes(bytes(4096))
    roots = [0x34E698, 0x34E798, 0x106F78, 0x1231A8, 0x342538, 0x3610E0]
    roots += vtable_targets([(0x4908F8, 0x1B0), (0x490AD0, 0x40)])
    binary = ROOT / 'build/ssx3_rail_snap_torque_reference'
    need = build_live(roots, ROOT / 'tests/rail_snap_torque_reference.cpp', binary, FOLDER / 'reference-oracle',
                      cache=ROOT / 'build/rail-snap-torque-reference-objects')
    run = subprocess.run([str(binary), str(FOLDER)], capture_output=True, text=True, timeout=3600)
    log = f'{len(need)} original entries linked\n' + run.stdout + run.stderr
    (FOLDER / 'reference.log').write_text(log)
    print(log, end='')
    if run.returncode:
        raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
