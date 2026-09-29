#!/usr/bin/env python3
"""Phases of section-started looping pieces in PS2 snapshots vs the predicted activation ticks.

For every LiveComp (0x490B10; object = entity - 0x30: +0x10 rate, +0x14 low, +0x18 high, +0x1C time)
and UVScroll (Object 0x490E80 entity, container+4 state: +0x04 timer, +0x30 u, +0x34 v, +0x38/+0x3C step)
on an instance of sections.json in each kept snapshot of the course run (local/ps2-capture/runs/
setpieces*/full.tick*.p2s), the state is predicted from:
  * a slot-1 construction at the last predicted enter tick E < T (reference_run of sections.json,
    engine/section_streaming.hpp semantics): first update in the entity pass of E+1; a snapshot
    with game+8 == T is taken at the start of tick T (before its entity pass): T-E-1 updates;
    LiveComp key-8 start = 0x317830(low, high) with the gameplay-RNG value of that draw, taken from
    the capture's RNG words (record E, +8896) advanced by the draw's index in record E+1's RNG log
    (the 0x101B60 draws come after every rider draw of tick E);
  * otherwise (constructed before the race) the ready savestate (start of tick 0) advanced T updates.
Usage: python3 tools/test_sections_phases.py [LOC ...]
"""
import glob, json, re, struct, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import export_sections as X  # noqa: E402
from export_sections import ee_add, ee_sub, fmul, f32, fbits, bitsf  # noqa: E402

RNG_WORDS = 8896


def rng_next(w):
    """0x317A08 (engine/original_random.hpp)."""
    M = 0xFFFFFFFF; value = (w[5] + w[4]) & M; carry = int(value < w[5] or value < w[4]); w[4] = value
    for i in (3, 2, 1):
        value = (value + w[i] + carry) & M; carry = int(value < w[i]); w[i] = value
    value = (value + w[0] + carry) & M; w[0] = value
    w[5] = (w[5] + 1) & M
    if w[5] == 0:
        i = 4
        while i >= 1:
            w[i] = (w[i] + 1) & M
            if w[i] != 0: break
            i -= 1
        if i == 0: value = (value + 1) & M; w[0] = value
    return value


def unit(lo, hi, r):   # 0x317830
    x = ee_sub(bitsf((r & 0x7FFFFF) | 0x3F800000), 1.0)
    return ee_add(lo, fmul(ee_sub(hi, lo), x))


def livecomp_loop(t, rate, low, high):    # 0x341E48 (rate >= 0 in these pieces)
    t = ee_add(t, rate)
    if high < t: t = ee_add(low, ee_sub(t, high))
    return t


def uv_tick(s):    # 0x35F7D0 for mode 5 without spin (engine/uv_scroll.hpp)
    if not (0 < s['on'] or 0 < s['off']): return
    s['timer'] = ee_add(s['timer'], f32(1 / 60))
    if s['on'] <= s['timer']: s['timer'] = 0.0
    s['u'] = ee_add(s['u'], s['su']); s['v'] = ee_add(s['v'], s['sv'])
    for k in ('u', 'v'):
        if 1.0 < s[k]: s[k] = ee_sub(s[k], 1.0)
        elif s[k] < -1.0: s[k] = ee_add(s[k], 1.0)


def capture_draw_values(code, anchor):
    """(scan tick) -> 0x101B60 draw values in order, from the capture's RNG words and log."""
    path = X.CAPTURES[code]; manifest = json.loads(path.with_suffix('.capture.json').read_text()); size = manifest['record']
    data = path.read_bytes(); out = {}; prev = None
    for at in range(0, len(data) - size + 1, size):
        r = data[at:at + size]; tick = struct.unpack_from('<I', r, 4)[0]
        if prev and tick <= prev[0]: break
        draws = struct.unpack_from('<I', r, X.AI_RNG)[0]
        log = [struct.unpack_from('<4I', r, X.AI_RNG + 32 + 16 * k) for k in range(min(draws, 64))]
        idx = [k for k, e in enumerate(log) if e[3] == X.RNG_MARKER_101B60]
        if idx and prev:
            if draws > 64: raise ValueError('RNG log overflow')
            w = list(prev[1]); values = []
            for k in range(max(idx) + 1):
                v = rng_next(w)
                if k in idx: values.append(v)
            out[tick - 1] = values
        prev = (tick, struct.unpack_from('<6I', r, RNG_WORDS))
    return out


def main():
    codes = sys.argv[1:] or ['ARA1', 'BRA2', 'BHP1']; total_bad = 0
    for code in codes:
        sec = json.loads((ROOT / f'web/public/assets/{code}/SECTIONS/sections.json').read_text())
        programs = {int(k): v for k, v in sec['programs'].items()}
        anchor = X.Memory(X.location_state(code, 'anchor')); ready = X.Memory(X.location_state(code, 'ready'))
        values = capture_draw_values(code, anchor)
        # predicted constructions: resource -> [(tick, draw values)]
        built = {}
        for s in sec['reference_run']['scans']:
            vals = list(values.get(s['tick'], []))
            if len(vals) != len(s['world_draws']): raise ValueError(f'{code} {s["tick"]}: draw count differs')
            for res, action, program in s['enter']:
                if action != 'slot1': continue
                n = programs[program]['rng_draws_if_constructed']
                built.setdefault(res, []).append((s['tick'], vals[:n])); vals = vals[n:]
        rows_ready = {r['resource']: r for r in X.instance_rows(ready).values()}
        def live(m, inst):
            e = m.u(inst + 0xC)
            if not e: return None
            vt = m.u(e + 0xC); c = m.u(e + 0x1C); s = m.u(c + 4) if c and 0x100000 < m.u(c + 4) < 0x2000000 else 0
            lc = dict(rate=m.f(e - 0x20), low=m.f(e - 0x1C), high=m.f(e - 0x18), time=m.f(e - 0x14), mode=m.h(e - 0x30)) if vt == 0x490B10 else None
            uv = dict(timer=m.f(s + 4), on=m.f(s + 8), off=m.f(s + 0xC), u=m.f(s + 0x30), v=m.f(s + 0x34), su=m.f(s + 0x38), sv=m.f(s + 0x3C), mode=m.u(s)) \
                if vt in (0x490E80, 0x490B10) and s and m.u(s) == 5 and m.f(s + 0x10) == 0 and m.f(s + 0x14) == 0 else None
            return e, lc, uv
        ready_live = {res: live(ready, r['inst']) for res, r in rows_ready.items()}
        uv_initial = {}
        try:
            for x in json.loads((X.ROOT / ('web/public/assets/UVSCROLL/uv-scroll.json' if code == 'ARA1' else f'web/public/assets/{code}/UVSCROLL/uv-scroll.json')).read_text())['instances']:
                i = x['initial']; uv_initial[x['resource']] = dict(timer=0.0, on=i['onTime'], off=i['offTime'], u=i['u'], v=i['v'], su=i['stepU'], sv=i['stepV'])
        except FileNotFoundError: pass
        stats = dict(livecomp_scan=[0, 0], livecomp_prerace=[0, 0], uv_scan=[0, 0], uv_prerace=[0, 0]); bad = []
        for path in sorted(glob.glob(X.SNAPSHOTS[code]), key=lambda p: int(re.search(r'tick(\d+)', p).group(1))):
            m = X.Memory(path); T = m.tick()
            if T < 18 or T > sec['reference_run']['last_tick']: continue
            for inst, r in X.instance_rows(m).items():
                res = r['resource']; lv = live(m, inst)
                if not lv: continue
                _, lc, uv = lv
                starts = [b for b in built.get(res, []) if b[0] < T]
                if starts:
                    E, vals = starts[-1]; n = T - E - 1; kind = 'scan'   # snapshot T = start of tick T (before its entity pass)
                else:
                    rl = ready_live.get(res)
                    if not rl: continue
                    E, vals, n, kind = 0, None, T, 'prerace'
                if lc:
                    if kind == 'scan':
                        sources = programs[next(i['slot1'] for i in sec['instances'] if i['resource'] == res)]['rng_draw_sources']
                        if 'builtin3:key8' in sources: t = unit(lc['low'], lc['high'], vals[sources.index('builtin3:key8')])
                        else: t = lc['low']       # no key 7 in these programs: start at the range low
                    else:
                        if not rl[1]: continue
                        t = rl[1]['time']
                    if lc['mode'] != 1: continue
                    for _ in range(n): t = livecomp_loop(t, lc['rate'], lc['low'], lc['high'])
                    ok = fbits(t) == fbits(lc['time']); stats[f'livecomp_{kind}'][0 if ok else 1] += 1
                    if not ok: bad.append((code, T, hex(res), 'livecomp', kind, E, t, lc['time']))
                if uv:
                    if kind == 'scan':
                        if res not in uv_initial: continue
                        s = dict(uv_initial[res])
                    else:
                        if not rl[2]: continue
                        s = {k: rl[2][k] for k in ('timer', 'on', 'off', 'u', 'v', 'su', 'sv')}
                    for _ in range(n): uv_tick(s)
                    ok = all(fbits(s[k]) == fbits(uv[k]) for k in ('timer', 'u', 'v'))
                    stats[f'uv_{kind}'][0 if ok else 1] += 1
                    if not ok: bad.append((code, T, hex(res), 'uv', kind, E, (s['timer'], s['u']), (uv['timer'], uv['u'])))
        print(code, {k: f'{v[0]} exact / {v[1]} off' for k, v in stats.items()})
        for b in bad[:12]: print('  ', b)
        total_bad += len(bad)
    sys.exit(1 if total_bad else 0)


if __name__ == '__main__':
    main()
