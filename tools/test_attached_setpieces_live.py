#!/usr/bin/env python3
"""Live parity of engine/parent_modifier.hpp (tests/attached_setpieces_live.cpp): the full recompiled
original entity updates (0x356198 + 0x3568B0, LiveComp 0x341D48/0x361098, Spline 0x359698/0x361B90,
ParentModifier 0x3619B8/0x357108/0x361940) in lockstep with the port, and every later kept savestate of
the same run:
  ara1-raven  race.tick4319 (ravensplineanima_1000 in flight) for 1300 ticks, targets 4719/5118/5518
  bhp1        the-junction-ready (race tick 0): blimpa_1000 + blimpad_1000/1001 + blimplights_1000 and
              the searchlightglowa_* children of the searchlightbasea_* LiveComps, 4420 ticks, targets
              setpieces-bhp1/full.tick*.
Needs web/generated/attached_seed_{ARA1,BHP1}.hpp (tools/export_attached_setpieces.py --location ...).
Log: local/reference/attached-setpieces/live.log."""
import glob, re, struct, subprocess, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_live_build import build_live, vtable_targets  # noqa: E402
from export_attached_setpieces import entities  # noqa: E402
FOLDER = ROOT / 'local/reference/attached-setpieces'
RUNS = ROOT / 'local/ps2-capture/runs'
GP = 0x4A30F0


def tick_of(ee):
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    return u(u(u(u(GP - 0x848) + 0x84) + 0x0C) + 8)


def stage(name, start, lines_of, later, ticks):
    work = FOLDER / name; work.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(start) as z:
        ee = z.read('eeMemory.bin')
        for suffix, member in [('ee', 'eeMemory.bin'), ('vuc', 'vu0MicroMem.bin'), ('vud', 'vu0Memory.bin')]:
            (work / f'start.{suffix}').write_bytes(z.read(member))
    t0 = tick_of(ee); lines = lines_of(ee) + [f'ticks {ticks}']
    for p in later:
        with zipfile.ZipFile(p) as z: mem = z.read('eeMemory.bin')
        t = tick_of(mem)
        if not 0 < t - t0 <= ticks: continue
        target = work / f'tick{t}.ee'; target.write_bytes(mem); lines.append(f'target {t - t0} {target}')
    (work / 'config.txt').write_text('\n'.join(lines) + '\n')


def raven_lines(ee):
    e = entities(ee)[0x91908]
    return [f'splinelc 91908 {e[0]:x} {e[2]:x}']


def bhp1_lines(ee):
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    ents = entities(ee); lines = []
    for res, (e, vt, mod, inst) in ents.items():
        if mod and u(mod) == 0x48F250 and vt == 0x490B10: lines.append(f'splinelc {res:x} {e:x} {mod:x}')
    parents = set()
    for res, (e, vt, mod, inst) in ents.items():
        if mod and u(mod) == 0x48F508:
            parent = u(u(mod + 0xA0) + 0x78); parents.add(parent)
            lines.append(f'parent {res:x} {e:x} {mod:x} {parent:x} {u(mod + 0x90)}')
    splined = {int(l.split()[1], 16) for l in lines if l.startswith('splinelc')}
    for parent in sorted(parents - splined):
        lines.insert(0, f'livecomp {parent:x} {ents[parent][0]:x}')
    return lines


def main():
    FOLDER.mkdir(parents=True, exist_ok=True)
    later = lambda pattern: sorted(glob.glob(str(RUNS / pattern)), key=lambda p: int(re.search(r'tick(\d+)', p).group(1)))
    stage('ara1-raven', RUNS / 'setpieces/race.tick4319.p2s', raven_lines, later('setpieces/race.tick*.p2s'), 1300)
    stage('bhp1', ROOT / 'local/reference/pcsx2/the-junction-ready.p2s', bhp1_lines, later('setpieces-bhp1/full.tick*.p2s'), 4420)
    roots = [0x356198, 0x3568B0, 0x361098, 0x361940, 0x357108, 0x3451C0]
    roots += vtable_targets([(0x490B10, 0x1D0), (0x490E80, 0x1D0), (0x48F250, 0xE8), (0x48F508, 0xE8), (0x48F5F0, 0xE8)])
    binary = ROOT / 'build/ssx3_attached_setpieces_live'
    need = build_live(roots, ROOT / 'tests/attached_setpieces_live.cpp', binary, FOLDER / 'live-oracle', cache=ROOT / 'build/attached-setpieces-live-objects')
    log = f'{len(need)} original entries linked\n'; code = 0
    for scenario in ('ara1-raven', 'bhp1'):
        run = subprocess.run([str(binary), str(FOLDER), scenario], capture_output=True, text=True, timeout=7200)
        log += run.stdout + run.stderr; code |= run.returncode
    (FOLDER / 'live.log').write_text(log); print(log, end='')
    if code: raise SystemExit(code)


if __name__ == '__main__':
    main()
