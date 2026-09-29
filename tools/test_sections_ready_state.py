#!/usr/bin/env python3
"""Step the browser set-piece players (web/livecomp-animation.js, web/uv-scroll.js, web/flag-animation.js)
from web/public/assets/<LOC>/SECTIONS/ready-state.json (tools/export_section_ready_state.py) and compare
with later PS2 savestates of the same timeline (countdown anchor, game tick 18, then the full-course
set-piece runs local/ps2-capture/runs/setpieces*/full.tick*.p2s, which start from that anchor).

Convention checked: savestate game+8 == T holds the state after the entity passes of ticks 0..T-1,
i.e. the browser players after T renderer ticks (set_piece_info()[1] == T).  Pieces the section
system rebuilt or destroyed before T (sections.json reference_run: enter slot1 / leave destroy) are
excluded.  Flags: wind keyframes draw from the shared visual RNG 0x4FF018, so they are checked up to
the anchor only, with the anchor's wind delta supplying the keyframe word; the grid-parity rule is
derived from the ARA1 flag-snapshots.json mesh buffers.
Usage: python3 tools/test_sections_ready_state.py [LOC ...]
"""
import glob, json, re, subprocess, sys, tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import export_sections as X  # noqa: E402
import export_section_ready_state as R  # noqa: E402

NODE = r'''
import { readFileSync } from 'node:fs';
const web = process.argv[2], fixturePath = process.argv[3];
const { liveCompConstruct, liveCompTick } = await import(web + '/livecomp-animation.js');
const { uvScrollTick } = await import(web + '/uv-scroll.js');
const { flagWindTick, flagClothTick, flagVertices, add, sub, mul, div } = await import(web + '/flag-animation.js');
const fx = JSON.parse(readFileSync(fixturePath, 'utf8'));
const F = new Float32Array(1), U = new Uint32Array(F.buffer); const bitsOf = (x) => { F[0] = x; return U[0]; };
const eq = (a, b) => Array.isArray(a) ? a.every((x, i) => eq(x, b[i])) : (typeof a === 'number' && !Number.isInteger(a) || typeof b === 'number' && !Number.isInteger(b) ? bitsOf(a) === bitsOf(b) : a === b);
const out = {};
for (const [code, f] of Object.entries(fx)) {
  const ready = f.ready, lc = f.livecomp, r = { livecomp: [0, 0, 0], uvscroll: [0, 0, 0], livecomp_fields: {}, uv_fields: {}, bad: [] };
  const byRes = new Map(lc.instances.map((x) => [x.resource, x]));
  // LiveComp: construct from the section start words, then overwrite with the ready state.
  const lives = new Map();
  for (const x of ready.livecomp) {
    const inst = byRes.get(x.resource); if (!inst) { r.livecomp[2]++; continue; }
    const words = (inst.starts.find((s) => s.trigger === 'section') ?? inst.starts[0]).words;
    const inst2 = { ...inst, matrixRows: [[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]], nodes: inst.nodes.map((n) => ({ ...n, bindRows: [0,1,2,3].map((k) => n.bind.slice(4*k, 4*k+4)) })) };
    const s = liveCompConstruct(inst2, words, () => 0); Object.assign(s, x.state); s.dirty = true; s.advanced = false; lives.set(x.resource, s);
  }
  const uvs = new Map(ready.uvscroll.map((x) => [x.resource, { ...x.state }]));
  let t = 0;
  for (const snap of f.snapshots) {
    for (; t < snap.tick; t++) { for (const s of lives.values()) liveCompTick(s); for (const s of uvs.values()) uvScrollTick(s, 60); }
    for (const [res, want] of Object.entries(snap.livecomp)) {
      const s = lives.get(+res); if (!s || snap.skip.includes(+res)) continue;
      const bad = Object.keys(want).filter((k) => !['previous', 'unclamped'].includes(k) && !eq(s[k], want[k]));
      r.livecomp[bad.length ? 1 : 0]++; if (bad.length) r.bad.push([code, snap.tick, +res, 'livecomp', bad]);
    }
    for (const [res, want] of Object.entries(snap.uvscroll)) {
      const s = uvs.get(+res); if (!s || snap.skip.includes(+res)) continue;
      const bad = Object.keys(want).filter((k) => !eq(s[k], want[k]));
      r.uvscroll[bad.length ? 1 : 0]++; if (bad.length) r.bad.push([code, snap.tick, +res, 'uv', bad]);
    }
  }
  // Flags to the anchor: find the 0x3177F0 word that reproduces the anchor's keyframe delta (brute force the 23 mantissa bits).
  const fl = f.flags, a = f.anchorFlags, mode = f.windMode;
  const wind = { ...ready.flags.wind }; const slots = ready.flags.slots.map((s) => ({ ...s, phase: s.phase.slice(), uvOffset: s.uvOffset.slice(), parameters: fl.cloths[s.cloth].parameters }));
  const words = []; let keyframes = 0;
  for (let tick = 0; tick < 18; tick++) {
    flagWindTick(wind, mode, 60, () => {
      keyframes++;
      // solve directly: delta = low + (amp - low) * r with r = mantissa/2^23; search the mantissa.
      const amp = { 2: 0.30000001192092896, 3: 0.44999998807907104 }[mode] ?? 0.15000000596046448;
      for (let m = 0; m < 0x800000; m++) { F[0] = 0; U[0] = (m | 0x3f800000) >>> 0; const r = sub(F[0], 1); let d = add(-amp, mul(sub(amp, -amp), r));
        if (bitsOf(d) === bitsOf(a.wind.delta)) { words.push(m); return m; } }
      words.push(-1); return 0;
    });
    for (const s of slots) { const cloth = { parameters: s.parameters, phase: s.phase, uvOffset: s.uvOffset, parity: s.parity, width: s.width, height: s.height, widthSpan: s.width - 1, heightSpan: s.height - 1, base: new Float32Array(s.width * s.height * 3), vertices: new Float32Array(s.width * s.height * 3) };
      flagClothTick(cloth, wind.wind, tick, new Float32Array(fl.sine_table)); }
  }
  const windOk = ['wind', 'base', 'delta', 'timer'].every((k) => bitsOf(wind[k]) === bitsOf(a.wind[k]));
  let phaseOk = 0; for (const s of slots) { const w = a.slots.find((x) => x.slot === s.slot); if (w && eq(s.phase, w.phase) && eq(s.uvOffset, w.uvOffset)) phaseOk++; }
  r.flags = { slots: slots.length, phasesExactAtAnchor: phaseOk, windExactAtAnchor: windOk, keyframesToAnchor: keyframes, keyframeWords: words };
  // Parity: which pass recomputed each captured slot's mesh (ARA1 flag-snapshots.json)?
  if (f.flagSnapshots) {
    const sine = new Float32Array(fl.sine_table); const rel = {};
    for (const c of f.flagSnapshots.cases) {
      const def = fl.cloths[c.cloth], w = def.width, h = def.height;
      const cloth = { parameters: def.parameters, width: w, height: h, widthSpan: w - 1, heightSpan: h - 1, base: new Float32Array(c.grid.flat()), phase: c.phase.slice() };
      const q = (v) => Array.from(v, (x) => Math.trunc(mul(x, c.mesh_scale))); const bufs = c.buffers.map((b) => b.flat());
      const cur = bufs.some((b) => { const v = q(flagVertices(cloth, c.manager.wind, sine)); return b.every((x, i) => x === v[i]); });
      const lastPass = cur ? c.tick - 1 : c.tick - 2;   // snapshot T = start of tick T: the last pass was T-1
      const key = `pass%2=${lastPass % 2},slotParity=${c.parity}`; rel[key] = (rel[key] ?? 0) + 1;
    }
    r.flagParity = rel;
  }
  out[code] = r;
}
console.log(JSON.stringify(out));
'''


def fixtures(code):
    ready = json.loads((ROOT / f'web/public/assets/{code}/SECTIONS/ready-state.json').read_text())
    sections = json.loads((ROOT / f'web/public/assets/{code}/SECTIONS/sections.json').read_text())
    touched = []
    for s in sections['reference_run']['scans']:
        for e in s['enter'] + s['leave']:
            if e[1] in ('slot1', 'destroy'): touched.append((s['tick'], e[0]))
    paths = [X.location_state(code, 'anchor')] + sorted(glob.glob(X.SNAPSHOTS[code]), key=lambda p: int(re.search(r'tick(\d+)', p).group(1)))
    snaps = []; anchor_flags = None
    for p in paths:
        m = X.Memory(p); T = m.tick()
        if T > sections['reference_run']['last_tick']: continue
        st = R.export(m, code)
        if anchor_flags is None: anchor_flags = st['flags']
        snaps.append(dict(tick=T, livecomp={x['resource']: x['state'] for x in st['livecomp']}, uvscroll={x['resource']: x['state'] for x in st['uvscroll']},
                          skip=sorted({r for t, r in touched if t < T})))
    snaps.sort(key=lambda s: s['tick'])
    flags = json.loads((R.asset(code, 'FLAGS') / 'flags.json').read_text())
    fsnap = R.asset(code, 'FLAGS') / 'flag-snapshots.json'
    return dict(ready=ready, livecomp=json.loads((R.asset(code, 'LIVECOMP') / 'livecomp.json').read_text()), flags=flags, windMode=flags['wind']['mode'],
                anchorFlags=anchor_flags, snapshots=snaps, flagSnapshots=json.loads(fsnap.read_text()) if code == 'ARA1' and fsnap.exists() else None)


def main():
    codes = sys.argv[1:] or ['ARA1', 'BRA2', 'BHP1']
    with tempfile.TemporaryDirectory() as d:
        fx = Path(d) / 'fixtures.json'; fx.write_text(json.dumps({c: fixtures(c) for c in codes}))
        script = Path(d) / 'run.mjs'; script.write_text(NODE)
        run = subprocess.run(['node', str(script), 'file://' + str(ROOT / 'web'), str(fx)], capture_output=True, text=True)
    if run.returncode: print(run.stderr); sys.exit(1)
    res = json.loads(run.stdout); failed = 0
    for code, r in res.items():
        print(f"{code}: LiveComp {r['livecomp'][0]} exact / {r['livecomp'][1]} off ({r['livecomp'][2]} not in livecomp.json); "
              f"UVScroll {r['uvscroll'][0]} exact / {r['uvscroll'][1]} off; flags {r['flags']}; parity {r.get('flagParity')}")
        for b in r['bad'][:8]: print('   ', b)
        failed += r['livecomp'][1] + r['uvscroll'][1] + (r['flags']['slots'] - r['flags']['phasesExactAtAnchor']) + (not r['flags']['windExactAtAnchor'])
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
