#!/usr/bin/env python3
"""Set-piece entity state at race tick 0 (the location's ready savestate = start of game tick 0, before
the first entity pass and before the first 0x101B60 scan), in the shapes the browser players consume.

Output: web/public/assets/<LOC>/SECTIONS/ready-state.json (git-ignored).
  livecomp[]  resource, name, entity class, and `state`: the fields of web/livecomp-animation.js
              liveCompConstruct() that are not derived from the model (mode, enabled, done, delay, low,
              high, time, rate, sampleTime, previous, unclamped); LiveComp object = entity - 0x30:
              +0x00 mode (s16), +0x04 enabled, +0x08 done, +0x0C delay, +0x10 rate, +0x14 low,
              +0x18 high, +0x1C time, +0x20 sample time, +0x24 previous, +0x28 unclamped.
              Use: s = liveCompConstruct(inst, words, () => 0); Object.assign(s, state);
              s.dirty = true; s.advanced = false; live.active.set(resource, s).
  uvscroll[]  resource, name, owner entity class, and `state`: exactly the keys of web/uv-scroll.js
              (mode, timer, onTime, offTime, angle, spin, axis, u, v, stepU, stepV, active); UVScroll
              object = entity container (+0x1C) +4: +0 mode, +4 timer, +8 on, +0xC off, +0x10 angle,
              +0x14 spin, +0x20 axis[4], +0x30 u, +0x34 v, +0x38/+0x3C step, +0x40 active.
  flags       manager = *(*(*(gp-0x848)+0x84)+0x70): wind {wind, base, delta, timer} (+0x10..+0x1C);
              slots[] (15 x 0x188 at +0x20): slot, cloth (flags.json index), group (flags.json group of
              the grid, matched by the rest-grid corners +0x74), parity (+0x90), phase[4] (+0x78),
              uvOffset[2] (+0x88), instances (+0x94.., registration order, resources); visual RNG words
              0x4FF018[6] at ready.
  emitters[]  type-13 entities (0x48EE60) present at ready: resource, name, stage slot-1 program.
  snowfall_wind  the snowfall object's wind-layer timers (0x2E4F50 +0x90, call order): each expiry draws the LCG 4 times.
  riders[]    per roster slot: the snow FX object's RiderBreath state (rider_fx); visual_lcg_4a3afc = gp+0xA0C. With the
              visual words they seed the countdown breath / trail / snow draws at start_event (web/stage_world.inc).
  other       remaining entity classes on instances (DeadNode, type-16, MultiSpline hosts, teeters, ...).
Numbers are the exact binary32 values (JSON doubles round-trip them); `bits` mirrors give the words.
Usage: python3 tools/export_section_ready_state.py --location ARA1|BRA2|BHP1 [--state PATH]
"""
import argparse, json, struct, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import export_sections as X  # noqa: E402
from locations import state as location_state  # noqa: E402

GP = 0x4A30F0
LIVECOMP, OBJECT, FLAG, EMITTER = 0x490B10, 0x490E80, 0x48FC10, 0x48EE60


def asset(code, sub):
    return ROOT / 'web/public/assets' / (sub if code == 'ARA1' else f'{code}/{sub}')


def bits(x): return X.fbits(x)


def livecomp_state(m, e):
    o = e - 0x30
    return dict(mode=m.h(o), enabled=m.i(o + 4), done=m.i(o + 8), delay=m.i(o + 0xC), rate=m.f(o + 0x10), low=m.f(o + 0x14),
                high=m.f(o + 0x18), time=m.f(o + 0x1C), sampleTime=m.f(o + 0x20), previous=m.f(o + 0x24), unclamped=m.f(o + 0x28))


def uv_state(m, s):
    return dict(mode=m.i(s), timer=m.f(s + 4), onTime=m.f(s + 8), offTime=m.f(s + 0xC), angle=m.f(s + 0x10), spin=m.f(s + 0x14),
                axis=m.v(s + 0x20, 4), u=m.f(s + 0x30), v=m.f(s + 0x34), stepU=m.f(s + 0x38), stepV=m.f(s + 0x3C), active=m.i(s + 0x40))


def float_bits(d):
    return {k: ([bits(x) for x in v] if isinstance(v, list) else bits(v)) for k, v in d.items() if isinstance(v, float) or (isinstance(v, list) and v and isinstance(v[0], float))}


def export(m, code):
    rows = X.instance_rows(m); names = {}
    try: names = {(i['rid'] << 8) | i['track']: i['name'] for i in json.loads((ROOT / f'local/assets/native/{code}/world_collision.json').read_text())['instances']}
    except FileNotFoundError: pass
    live, uv, emitters, other = [], [], [], {}
    for inst, r in sorted(rows.items(), key=lambda kv: kv[1]['resource']):
        e = m.u(inst + 0xC)
        if not e: continue
        res = r['resource']; vt = m.u(e + 0xC); name = names.get(res)
        if m.u(e + 0x18) != inst:   # clone instances sharing their host's entity (MultiSpline cars)
            other.setdefault('shares_entity_of_another_instance', []).append(res); continue
        container = m.u(e + 0x1C)
        uvs = m.u(container + 4) if container and 0x100000 < m.u(container + 4) < 0x2000000 else 0
        if vt == LIVECOMP:
            st = livecomp_state(m, e); live.append(dict(resource=res, name=name, state=st, bits=float_bits(st)))
        if uvs and vt in (LIVECOMP, OBJECT):
            st = uv_state(m, uvs); uv.append(dict(resource=res, name=name, owner=X.VT.get(vt), state=st, bits=float_bits(st)))
        if vt == EMITTER:
            emitters.append(dict(resource=res, name=name, program=r['slot1'] >> 8 if r['stage'] and r['slot1'] != -1 else None))
        if vt not in (LIVECOMP, EMITTER, FLAG) and not (vt == OBJECT and uvs):
            other.setdefault(r['entity_class'] or hex(vt), []).append(res)
    # flag manager
    fm = m.u(m.u(m.u(GP - 0x848) + 0x84) + 0x70)
    wind = dict(zip(('wind', 'base', 'delta', 'timer'), m.v(fm + 0x10, 4)))
    flag_data = json.loads((asset(code, 'FLAGS') / 'flags.json').read_text())
    slots = []
    for k in range(15):
        c = fm + 0x20 + 0x188 * k; n = m.u(c + 0x5C)
        if not n: continue
        speed = [bits(x) for x in m.v(c + 4, 4)]; model = m.u(c + 0x58)
        cloth = next(x for x in flag_data['cloths'] if x['model'] == model and [bits(v) for v in x['parameters']['speed']] == speed)
        w, h = m.u(c + 0x64), m.u(c + 0x68); grid = m.u(c + 0x74)
        corners = [m.v(grid + 12 * i) for i in (0, w - 1, (h - 1) * w, h * w - 1)]
        group = next(gi for gi, g in enumerate(cloth['groups']) if all([bits(x) for x in corners[i]] == [bits(x) for x in g['corners']['position'][i]] for i in range(4)))
        slots.append(dict(slot=k, cloth=cloth['index'], group=group, parity=m.u(c + 0x90), phase=m.v(c + 0x78, 4), uvOffset=m.v(c + 0x88, 2),
                          instances=[m.u(m.u(c + 0x94 + 4 * j) + 0x78) for j in range(n)], width=w, height=h))
    return dict(livecomp=live, uvscroll=uv, emitters=emitters, riders=rider_fx(m), visual_lcg_4a3afc=m.u(0x4A3AFC), snowfall_wind=snowfall_wind(m),
                flags=dict(wind=wind, wind_bits=float_bits(wind), slots=slots, parity_word_gp_f20=m.u(GP + 0xF20)),
                visual_rng_4ff018=[m.u(0x4FF018 + 4 * k) for k in range(6)], gameplay_rng_4ff030=[m.u(0x4FF030 + 4 * k) for k in range(6)],
                other={k: sorted(v) for k, v in other.items()})


def snowfall_wind(m):
    """The snowfall object (vtable 0x487FD8 at +0xC; 0x2E5DA0) wind layers in call order (slots +0x10..+0x3C, non-null):
    0x2E4F50 timer +0x90 (seconds; below 0 after -= 1/60 it draws the visual LCG 4 times and re-arms)."""
    hits = []; at = m.ee.find(struct.pack('<I', 0x487FD8))
    while at >= 0:
        o = at - 0xC; slots = [m.u(o + 0x10 + 4 * k) for k in range(12)]
        if at % 4 == 0 and any(slots) and all(s == 0 or 0x100000 < s < 0x2000000 for s in slots): hits.append((o, [s for s in slots if s]))
        at = m.ee.find(struct.pack('<I', 0x487FD8), at + 1)
    if len(hits) != 1: raise ValueError(f'snowfall object: {len(hits)} candidates')
    o, layers = hits[0]
    return dict(object=o, layers=layers, timers=[m.f(s + 0x90) for s in layers], bits=[m.u(s + 0x90) for s in layers])


def rider_fx(m):
    """Per roster slot (human first as in the roster), the snow FX object's RiderBreath 2E1120 state: FX = rider - 0x2D0
    (human) / rider - 0x410 (computer), FX+0 = the rider; +0x14 accumulator, +0x18 phase, +0x1C effort, +0x20 clock,
    +0x24 duration. Seeds web/animation_bridge.cpp breathState (the countdown breath draws the visual LCG and 0x4FF018)."""
    game = m.u(m.u(m.u(GP - 0x848) + 0x84) + 0x0C); out = []
    for n in range(m.u(game + 0x78)):
        r = m.u(game + 0x28 + 4 * n); human = m.u(r + 0x6C0) == 0x4583A8
        fx = r - (0x2D0 if human else 0x410)
        if m.u(fx) != r: raise ValueError(f'rider {r:#x}: snow FX object not at {fx:#x}')
        out.append(dict(slot=n, human=human, fx=fx, breath=dict(accumulator=m.f(fx + 0x14), phase=m.u(fx + 0x18), effort=m.f(fx + 0x1C),
                                                              clock=m.f(fx + 0x20), duration=m.f(fx + 0x24))))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--location', default='ARA1'); ap.add_argument('--state'); ap.add_argument('--output')
    a = ap.parse_args(); code = a.location
    path = Path(a.state) if a.state else location_state(code, 'ready')
    m = X.Memory(path); a_ = m.activation()
    if m.tick() != 0 or m.i(a_ + 0xD0) != -1:
        raise ValueError(f'{path}: not the start of race tick 0 (game+8 {m.tick()}, activation D0 {m.i(a_ + 0xD0)})')
    out = dict(version=1, location=code, source=str(path.relative_to(ROOT)), game_tick=0,
               convention=('state at the start of game tick 0 (before its entity pass and its 0x101B60 scan) = the browser set-piece '
                           'state before set-pieces-renderer processes ticks === 0 (set_piece_info()[1] == 0); after the renderer '
                           'processed ticks 0..T-1 the players must equal a PS2 savestate with game+8 == T'),
               **export(m, code))
    target = Path(a.output) if a.output else ROOT / 'web/public/assets' / code / 'SECTIONS/ready-state.json'
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(out, separators=(',', ':')) + '\n')
    print(json.dumps(dict(output=str(target.relative_to(ROOT)), livecomp=len(out['livecomp']), uvscroll=len(out['uvscroll']),
                          flag_slots=len(out['flags']['slots']), flag_instances=sum(len(s['instances']) for s in out['flags']['slots']),
                          emitters=len(out['emitters']), other={k: len(v) for k, v in out['other'].items()})))


if __name__ == '__main__':
    main()
