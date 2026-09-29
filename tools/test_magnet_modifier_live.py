#!/usr/bin/env python3
"""Live MagnetModifier parity (tests/magnet_modifier_live.cpp; development oracle).

On The Junction (BHP1) pipe-run savestates (three memory layouts: ticks 97, 188, 1399, 2518)
every live pickup magnet is stepped by the full original entity update 0x356198 + 0x3568B0,
with the human rider's +0x110 driven by the PS2 pipe-finish capture trajectory (raw, shifted
past each pickup, shifted + jittered), and the real contact chain 0x355770 (gate 0x357660,
slot-2 program 9 through 0x30A060: points, builtin69 halo removal, builtin1 Debounce) while
the rider is inside the magnet box, against engine/magnet_modifier.hpp (every 0xB0 byte, the
entity +0x20 timer and the award tick per tick; the 0x355F10 PositionModifier vs
originalMagnetFreeze). Also prints the port timeline of the real pointa_1001 case.
usage: test_magnet_modifier_live.py [TICKS]   Log: local/reference/magnet-modifier/live.log"""
import struct, subprocess, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_live_build import build_live, vtable_targets  # noqa: E402
FOLDER = ROOT / 'local/reference/magnet-modifier'
RUNS = ROOT / 'local/ps2-capture/runs'
STATES = ['97', '188', '1399', '2518']
RIDER_INTERFACE = 0x4583A8


def trajectory(out):
    """(capture tick, rider+0x110 quad) of every pipe-finish record (layout rider_100_b40 at +32)."""
    data = (RUNS / 'pipe-finish.bin').read_bytes()
    rows = bytearray()
    for at in range(0, len(data) - 16383, 16384):
        tick = struct.unpack_from('<I', data, at + 4)[0]
        rows += struct.pack('<I', tick) + data[at + 32 + 0x10: at + 32 + 0x20]
    out.write_bytes(bytes(rows))
    return len(rows) // 20


def main():
    FOLDER.mkdir(parents=True, exist_ok=True)
    for tick in STATES:
        with zipfile.ZipFile(RUNS / f'pipe-run.tick{tick}.p2s') as z:
            for suffix, member in [('ee', 'eeMemory.bin'), ('vuc', 'vu0MicroMem.bin'), ('vud', 'vu0Memory.bin')]:
                (FOLDER / f'pipe{tick}.{suffix}').write_bytes(z.read(member))
    records = trajectory(FOLDER / 'pipe-finish-trajectory.bin')
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    builtins = sorted({struct.unpack_from('<I', elf, 0x441F38 + 4 * i - 0xFF000)[0] for i in range(111)} - {0})
    roots = [0x356198, 0x3568B0, 0x355770, 0x30A060, 0x30A298] + [b for b in builtins if 0x100000 <= b < 0x440000]
    roots += vtable_targets([(0x48F420, 0xE8), (0x48F5F0, 0xE8), (0x490E80, 0x1D0), (0x4906F0, 0x1D0), (0x491680, 0x1D0),
                             (0x491B00, 0x1D0), (0x491220, 0x30), (RIDER_INTERFACE, 0x200), (0x483648, 0x40), (0x483688, 0x40)])
    binary = ROOT / 'build/ssx3_magnet_modifier_live'
    need = build_live(roots, ROOT / 'tests/magnet_modifier_live.cpp', binary, FOLDER / 'live-oracle',
                      cache=ROOT / 'build/magnet-modifier-live-objects')
    ticks = sys.argv[1] if len(sys.argv) > 1 else '400'
    run = subprocess.run([str(binary), str(FOLDER), str(FOLDER / 'pipe-finish-trajectory.bin'), ticks] + [f'pipe{t}' for t in STATES],
                         capture_output=True, text=True, timeout=7200)
    log = f'{len(need)} original entries linked; trajectory {records} pipe-finish records\n' + run.stdout + run.stderr
    (FOLDER / 'live.log').write_text(log)
    print(log, end='')
    if run.returncode:
        raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
