#!/usr/bin/env python3
"""Live parity of the Snow Jam falling billboard (tests/falling_billboard_live.cpp; web/falling_billboard.inc on
engine/livecomp_animation.hpp + engine/rail_modifier.hpp) on the PS2 run that fires it
(local/ps2-capture/runs/setpieces-bb/fall.*: pad script local/ps2-capture/scripts/setpieces-bb-fall.json, the
human contacts mdl_ARA1_bcvolume_1001 at tick 1913): construction at fall.tick1913, 150-tick lockstep of the
full original 0x356198 / 0x361098 / 0x35C5A0 / 0x35C698, and every later kept savestate of the run.
Needs web/generated/falling_billboard_seed.hpp (tools/export_falling_billboard.py).
Log: local/reference/falling-billboard/live.log."""
import glob, re, struct, subprocess, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_live_build import build_live, vtable_targets  # noqa: E402
FOLDER = ROOT / 'local/reference/falling-billboard'
RUN = ROOT / 'local/ps2-capture/runs/setpieces-bb'
GP = 0x4A30F0
BOARD, INSTANCE = 0x66B08, 0x1016B20


def tick_of(ee):
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    return u(u(u(u(GP - 0x848) + 0x84) + 0x0C) + 8)


def main():
    FOLDER.mkdir(parents=True, exist_ok=True)
    start = RUN / 'fall.tick1913.p2s'
    with zipfile.ZipFile(start) as z:
        ee = z.read('eeMemory.bin')
        for suffix, member in [('ee', 'eeMemory.bin'), ('vuc', 'vu0MicroMem.bin'), ('vud', 'vu0Memory.bin')]:
            (FOLDER / f'start.{suffix}').write_bytes(z.read(member))
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    assert u(INSTANCE + 0x78) == BOARD
    entity = u(INSTANCE + 0xC); t0 = tick_of(ee)
    lines = [f'entity {entity:x}']
    arr = memoryview(ee).cast('I')
    for i in range(0x100000 // 4, len(arr)):
        if arr[i] == 0x4911D0 and u(i * 4 - 8 + 0x40) == INSTANCE:
            lines.append(f'rail {i * 4 - 8:x} {u(i * 4 - 8 + 0x30):x}')
    lines.append('ticks 150')
    for p in sorted(glob.glob(str(RUN / 'fall.tick*.p2s')), key=lambda p: int(re.search(r'tick(\d+)', p).group(1))):
        with zipfile.ZipFile(p) as z: mem = z.read('eeMemory.bin')
        t = tick_of(mem)
        if 0 < t - t0 <= 150:
            target = FOLDER / f'tick{t}.ee'; target.write_bytes(mem); lines.append(f'target {t - t0} {target}')
    (FOLDER / 'config.txt').write_text('\n'.join(lines) + '\n')
    roots = [0x356198, 0x3568B0, 0x361098, 0x35C5A0, 0x35C698, 0x35C4E0, 0x3451C0]
    roots += vtable_targets([(0x490B10, 0x1D0), (0x4911D0, 0x50), (0x490AD0, 0x40)])
    binary = ROOT / 'build/ssx3_falling_billboard_live'
    need = build_live(roots, ROOT / 'tests/falling_billboard_live.cpp', binary, FOLDER / 'live-oracle', cache=ROOT / 'build/falling-billboard-live-objects')
    run = subprocess.run([str(binary), str(FOLDER)], capture_output=True, text=True, timeout=3600)
    log = f'{len(need)} original entries linked; start {start.name} (tick {t0})\n' + run.stdout + run.stderr
    (FOLDER / 'live.log').write_text(log); print(log, end='')
    if run.returncode: raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
