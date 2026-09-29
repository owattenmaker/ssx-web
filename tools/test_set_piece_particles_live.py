#!/usr/bin/env python3
"""Live parity of engine/set_piece_particles.hpp (tests/set_piece_particles_live.cpp) against the full
recompiled original: randomized unit oracles (0x3705E0/0x370788/0x36D500/0x370DC8/0x3710D0) and, per
location, scenarios that launch the real stage programs through the original dispatchers and run the
original world object pass 0x354F98(group 1) with the port mirroring every particle operation (hooks).

Scenarios are derived from web/public/assets/<LOC>/PARTICLES/particles.json (tools/export_set_piece_particles.py):
every slot-2 owner of a particle program, every slot-2 trigger that starts a slot-5 timer owning one
(livecomp.json starts), and one slot-1 owner per section program; the savestate is the first kept PS2
snapshot of the location's run in which every instance involved is resident (lookup gp+0x16C8).
Logs: local/reference/set-piece-particles/live.log.

usage: test_set_piece_particles_live.py [--locations ARA1,BRA2,BHP1] [--cases N] [--ticks N]
"""
import argparse, glob, json, re, struct, subprocess, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_live_build import build_live, vtable_targets  # noqa: E402
import original_oracle_build  # noqa: E402


class _ResponseFileRun:
    """The link line of ~14k oracle objects exceeds ARG_MAX: pass long command lines through a clang
    response file (the shared cached_oracle_build stays unchanged)."""
    def __init__(self, module): self.module = module
    def __getattr__(self, name): return getattr(self.module, name)
    def run(self, args, **kw):
        if isinstance(args, list) and len(args) > 2000:
            rsp = FOLDER / 'link.rsp'; rsp.write_text('\n'.join('"' + a.replace('\\', '\\\\').replace('"', '\\"') + '"' for a in args[2:]))
            args = args[:2] + ['@' + str(rsp)]
        return self.module.run(args, **kw)


original_oracle_build.subprocess = _ResponseFileRun(original_oracle_build.subprocess)
FOLDER = ROOT / 'local/reference/set-piece-particles'
GP = 0x4A30F0
RUNS = {'ARA1': ['setpieces/race.tick*.p2s', 'setpieces/full.tick*.p2s'], 'BRA2': ['setpieces-bra2/full.tick*.p2s'],
        'BHP1': ['setpieces-bhp1/full.tick*.p2s']}
LIVECOMP = {'ARA1': 'web/public/assets/LIVECOMP/livecomp.json', 'BRA2': 'web/public/assets/BRA2/LIVECOMP/livecomp.json',
            'BHP1': 'web/public/assets/BHP1/LIVECOMP/livecomp.json'}


def tick_of(path): return int(re.search(r'tick(\d+)', str(path)).group(1))


class State:
    def __init__(self, path):
        self.path = Path(path)
        with zipfile.ZipFile(path) as z: self.ee = z.read('eeMemory.bin')

    def u(self, a): return struct.unpack_from('<I', self.ee, a & 0x1FFFFFF)[0]

    def instance(self, resource):
        u = self.u; t = u(u(u(u(GP + 0x16C8)) + 8) + (resource & 0xFF) * 4)
        if not t: return None
        e = u(u(t + 0x1C) + (resource >> 8) * 4); a = (e >> 8) << 2
        return a if a and u(a + 0x78) == resource else None

    def group_vtables(self):
        """Entity/modifier/effect vtables of the world object groups (for linking)."""
        u = self.u; out = set(); base = u(GP + 0x2898 + 4)
        for g in range(8):
            head = base + g * 0x44; o = u(head + 8); n = 0
            while o != head + 0x14 and o and n < 5000:
                out.add(u(o + 0xC)); c = u(o + 0x1C) if 0x48E000 <= u(o + 0xC) < 0x492000 else 0
                if 0x100000 < c < 0x2000000:
                    for k in range(4):
                        m = u(c + 4 * k)
                        if 0x100000 < m < 0x2000000: out.add(u(m)); out.add(u(m + 4))
                    e = u(c + 0x10)
                    while 0x100000 < e < 0x2000000: out.add(u(e + 8)); e = u(e)
                o = u(o + 4); n += 1
        return {v for v in out if 0x440000 <= v < 0x4A0000}


def scenarios(code, ticks):
    data = json.loads((ROOT / f'web/public/assets/{code}/PARTICLES/particles.json').read_text())
    livecomp = json.loads((ROOT / LIVECOMP[code]).read_text())
    timer_starts = {}
    for inst in livecomp['instances']:
        for st in inst['starts']:
            if st['trigger'] == 'contact': timer_starts.setdefault(inst['resource'], []).append(st['ownerResource'])
    want = []
    for p in data['programs']:
        if not any(c['builtin'] in (16, 25, 26) for c in p['calls']): continue
        targets = [c['target'] for c in p['calls'] if c.get('target') is not None and c['builtin'] in (16, 25, 26)]
        for o in p['owners']:
            if o['slot'] == 2: want.append((f"p{p['program']}-{o['owner'].split('_', 2)[-1]}", [(2, o['resource'])], [o['resource']] + targets))
            if o['slot'] == 5:
                for trig in timer_starts.get(o['resource'], []): want.append((f"p{p['program']}-{o['owner'].split('_', 2)[-1]}", [(2, trig)], [trig, o['resource']] + targets))
        slot1 = [o for o in p['owners'] if o['slot'] == 1]
        if slot1: want.append((f"p{p['program']}-{slot1[0]['owner'].split('_', 2)[-1]}", [(1, o['resource']) for o in slot1[:4]], [o['resource'] for o in slot1[:4]]))
    states = sorted({s for pat in RUNS[code] for s in glob.glob(str(ROOT / 'local/ps2-capture/runs' / pat))}, key=tick_of)
    loaded = {}
    out = []; seen = set()
    for label, launches, needed in want:
        if label in seen: continue
        seen.add(label)
        for s in states:
            st = loaded.setdefault(s, State(s))
            addrs = {r: st.instance(r) for r in needed}
            if all(addrs.values()):
                out.append((label, Path(s), [(slot, addrs[r]) for slot, r in launches], ticks)); break
        else: print(f'{code} {label}: no savestate with every instance resident', file=sys.stderr)
    return out, loaded


PARTICLE_CTX = {0x345F90: 'birth', 0x3705E0: 'static', 0x370DC8: 'dynamic'}
# PS2 savestate pairs (local/ps2-capture/runs/...) of the setpieces full run and its setpieces-fx/vis replay.
REPLAY_PAIRS = [('setpieces/full.tick418', 'setpieces/full.tick819'), ('setpieces-fx/vis.tick319', 'setpieces-fx/vis.tick449'),
                ('setpieces-fx/vis.tick449', 'setpieces-fx/vis.tick481'), ('setpieces-fx/vis.tick481', 'setpieces-fx/vis.tick518'),
                ('setpieces/full.tick819', 'setpieces/fulldense.tick894'), ('setpieces/fulldense.tick894', 'setpieces/fulldense.tick914'),
                ('setpieces/fulldense.tick914', 'setpieces/fulldense.tick938'), ('setpieces/fulldense.tick938', 'setpieces/full.tick969'),
                ('setpieces/full.tick969', 'setpieces/full.tick1079'), ('setpieces/full.tick1079', 'setpieces/full.tick1219'),
                ('setpieces/full.tick1219', 'setpieces/full.tick1621'), ('setpieces-fx/vis.tick1398', 'setpieces-fx/vis.tick1428'),
                ('setpieces/full.tick1621', 'setpieces/full.tick2019'), ('setpieces-fx/vis.tick1988', 'setpieces-fx/vis.tick2003'),
                ('setpieces/full.tick2419', 'setpieces/full.tick2819'), ('setpieces/full.tick3618', 'setpieces/fulldense.tick3708'),
                ('setpieces/fulldense.tick3708', 'setpieces/fulldense.tick3739'), ('setpieces/fulldense.tick3739', 'setpieces/fulldense.tick3779'),
                ('setpieces/fulldense.tick3779', 'setpieces/full.tick3919'), ('setpieces/full.tick3919', 'setpieces/full.tick4019'),
                ('setpieces-fx/vis.tick4119', 'setpieces-fx/vis.tick4148'), ('setpieces/full.tick10818', 'setpieces/fulldense.tick10869'),
                ('setpieces/fulldense.tick10869', 'setpieces/fulldense.tick10889'), ('setpieces/fulldense.tick10889', 'setpieces/fulldense.tick10918'),
                ('setpieces/fulldense.tick10918', 'setpieces/fulldense.tick11179'), ('setpieces/fulldense.tick11179', 'setpieces/fulldense.tick11199'),
                ('setpieces/fulldense.tick11199', 'setpieces/full.tick11249'), ('setpieces/full.tick11249', 'setpieces/full.tick11418'),
                ('setpieces/full.tick11619', 'setpieces/full.tick11758'), ('setpieces/full.tick11758', 'setpieces/full.tick11918'),
                ('setpieces/full.tick11918', 'setpieces/full.tick12018'), ('setpieces-fx/vis.tick12289', 'setpieces-fx/vis.tick12339'),
                ('setpieces/full.tick12348', 'setpieces/full.tick12420')]


def replays(copies):
    """PS2 savestate pairs replayed with the traced 0x4FF018 draw indices (tools/trace_visual_rng.py runs of
    the same script: draw-for-draw identical, each savestate located by 0x4FF018 word 5). Contacts
    (0x30A060 trigger) and section activations (0x30A298) of the trace are launched after the entity pass."""
    traces = [ROOT / 'local/ps2-capture/runs/visual-rng/full2.trace.json', ROOT / 'local/ps2-capture/runs/visual-rng/full12k.trace.json']
    trace = next((t for t in traces if t.exists()), None)
    if not trace: return []
    with_calls = trace.name.startswith('full2')
    d = json.loads(trace.read_text()); E = d['entries']
    draws = [i for i, e in enumerate(E) if not e['ra'] & 1]
    by_w5 = {E[i]['w5']: n for n, i in enumerate(draws)}
    from set_piece_location import Location
    loc = Location('ARA1'); handled = {}
    for inst, row in loc.handler_rows():
        for slot, w in enumerate(row):
            if w != 0xFFFFFFFF and w & 255 == loc.track and slot in (1, 2): handled.setdefault(loc.resource(inst), set()).add(slot)
    out = []; runs = ROOT / 'local/ps2-capture/runs'
    for a, b in REPLAY_PAIRS:
        pa, pb = runs / f'{a}.p2s', runs / f'{b}.p2s'
        if not (pa.exists() and pb.exists()): continue
        sa, sb = State(pa), State(pb)
        wa, wb = sa.u(0x4FF018 + 20), sb.u(0x4FF018 + 20)
        if wa not in by_w5 or wb not in by_w5: continue
        ia, ib = by_w5[wa], by_w5[wb]
        def rider_pass_started(index, tick):   # has the rider manager of `tick` begun before draw `index`?
            k = draws[index] - 1
            while k >= 0 and E[k]['tick'] == tick:
                if E[k]['ra'] & 1 and E[k]['ra'] & ~1 == 0x128AF0: return True
                k -= 1
            return E[draws[index]]['tick'] != tick
        ta, tb = tick_of(pa), tick_of(pb)
        first = ta + 1 if rider_pass_started(ia, ta) else ta
        last = tb if rider_pass_started(ib, tb) else tb - 1
        queue = {}; launches = {}
        for n in range(ia, ib):
            e = E[draws[n]]
            if PARTICLE_CTX.get(e['ctx_fn']): queue.setdefault(e['tick'], []).append(n)
        if with_calls:
            for e in E[draws[ia]:draws[ib - 1] + 1]:
                fn = e['ra'] & ~1
                if not (e['ra'] & 1) or fn not in (0x30A060, 0x30A298) or not first <= e['tick'] <= last: continue
                inst = e['ctx_a0']; res = sa.u(inst + 0x78) if 0x100000 < inst < 0x2000000 else None
                slot = 2 if fn == 0x30A060 else 1
                if res is not None and slot in handled.get(res, ()): launches.setdefault(e['tick'], []).append((slot, inst))
        na, nb = f"ARA1-{a.split('/')[-1]}", f"ARA1-{b.split('/')[-1]}"
        copies[na] = pa; copies[nb] = pb
        lines = [f'R {a.split("/")[-1]}->{b.split("/")[-1]} {na} {nb} {first} {last} {ia}']
        lines += [f'Q {t} ' + ' '.join(map(str, q)) for t, q in sorted(queue.items())]
        lines += [f'L {t} {slot} {addr:x}' for t, ls in sorted(launches.items()) for slot, addr in ls]
        out += lines
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--locations', default='ARA1,BRA2,BHP1'); ap.add_argument('--cases', type=int, default=3000); ap.add_argument('--ticks', type=int, default=420)
    a = ap.parse_args()
    FOLDER.mkdir(parents=True, exist_ok=True)
    blocks = []
    for code in a.locations.split(','):
        data = json.loads((ROOT / f'web/public/assets/{code}/PARTICLES/particles.json').read_text())
        for p in data['programs']:
            for c in p['calls']:
                if c['builtin'] not in (16, 25, 26): continue
                b = data['blocks'][c['block_index']]; words = b['words'] + [0] * (60 - len(b['words']))
                targets = [b['target']] if b['target'] is not None else [o['resource'] for o in p['owners']]
                for t in targets: blocks.append(f"{t} {b['builtin']} " + ' '.join(f'{w:x}' for w in words))
    (FOLDER / 'blocks.txt').write_text('\n'.join(blocks) + '\n')
    vtables = set(); lines = []; copies = {}
    unit = ROOT / 'local/ps2-capture/runs/setpieces/race.tick718.p2s'; copies['unit'] = unit
    for code in a.locations.split(','):
        sc, loaded = scenarios(code, a.ticks)
        for label, state, launches, ticks in sc:
            name = f'{code}-{tick_of(state)}' + ('-full' if 'full.' in state.name else '')
            copies[name] = state; vtables |= loaded[str(state)].group_vtables()
            rider = json.loads((state.parent / 'full.capture.json').read_text())['rider'][2:]
            lines.append(f"{code}:{label} {name} {ticks} {rider} " + ' '.join(f'{slot} {addr:x}' for slot, addr in launches))
    (FOLDER / 'scenarios.txt').write_text('\n'.join(lines) + '\n')
    (FOLDER / 'replays.txt').write_text('\n'.join(replays(copies)) + '\n')
    for name, path in copies.items():
        if (FOLDER / f'{name}.ee').exists(): continue
        with zipfile.ZipFile(path) as z:
            for suffix, member in [('ee', 'eeMemory.bin'), ('vuc', 'vu0MicroMem.bin'), ('vud', 'vu0Memory.bin')]:
                (FOLDER / f'{name}.{suffix}').write_bytes(z.read(member))
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    builtins = sorted({struct.unpack_from('<I', elf, 0x441F38 + 4 * i - 0xFF000)[0] for i in range(111)} - {0})
    roots = [0x370018, 0x3705E0, 0x370788, 0x36D500, 0x370B60, 0x370DC8, 0x3710D0, 0x370CF8, 0x345B40, 0x345F90, 0x345AD0, 0x345E88,
             0x34FED8, 0x356078, 0x354F98, 0x30A060, 0x30A298, 0x3458C0, 0x345C90, 0x346060, 0x4103B0, 0x3D76F0, 0x1530E0, 0x36D400] + [b for b in builtins if 0x100000 <= b < 0x440000]
    fixed = [(0x48F250, 0xE8), (0x48F5F0, 0xE8), (0x490E80, 0x1D0), (0x490B10, 0x1D0), (0x48EE60, 0x1D0), (0x491268, 0x48), (0x491370, 0x48),
             (0x4912B0, 0xC0), (0x493118, 0x48), (0x493160, 0x48), (0x491680, 0x1D0), (0x4906F0, 0x1D0), (0x483648, 0x40), (0x483688, 0x40)]
    interface = struct.unpack_from('<I', (FOLDER / 'unit.ee').read_bytes(), 0x14701A0 + 0x6C0)[0]
    tables = fixed + [(interface, 0x200)] + [(v, 0x1D0) for v in sorted(vtables) if v not in {f[0] for f in fixed}]
    roots += vtable_targets(tables)
    binary = ROOT / 'build/ssx3_set_piece_particles_live'
    need = build_live(roots, ROOT / 'tests/set_piece_particles_live.cpp', binary, FOLDER / 'live-oracle', cache=ROOT / 'build/set-piece-particles-live-objects')
    run = subprocess.run([str(binary), str(FOLDER), str(FOLDER / 'blocks.txt'), str(FOLDER / 'scenarios.txt'), str(a.cases), str(FOLDER / 'replays.txt')], capture_output=True, text=True, timeout=14400)
    log = f'{len(need)} original entries linked; {len(lines)} scenarios\n' + run.stdout + run.stderr
    (FOLDER / 'live.log').write_text(log); print(log, end='')
    if run.returncode: raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
