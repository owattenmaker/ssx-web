// LiveComp animation (web/livecomp-animation.js) against PS2 memory
// (tools/export_livecomp.py livecomp-snapshots.json: race, full-course and GO captures):
// 1. every clean (not dirty) snapshot: node matrices recomputed from the object state bit for bit;
// 2. consecutive snapshots of one capture: the object words replayed tick by tick;
// 3. start-gate doors: fire('go') in tick 181 then the entity pass -> the GO capture states;
// 4. ravens: contact at tick 3404 -> ravenanima_1000/1001/1002 (chained by slot-5 crossings)
//    reproduce the full-course snapshots 3479/3618 bit for bit.
// usage: node test-livecomp-animation.mjs [LOCATION] (default ARA1; others public/assets/<LOC>/LIVECOMP,
// where snapshot records carry the LiveComp object address: streamed sections rebuild their LiveComps).
import { readFileSync, existsSync } from 'node:fs';
import { LiveCompAnimation, liveCompConstruct, liveCompTick, liveCompMatrices } from './livecomp-animation.js';
import { fromBits, bitsOf } from './ee-scalar-float.js';

const LOC = process.argv[2] || 'ARA1';
const dir = new URL(LOC === 'ARA1' ? './public/assets/LIVECOMP/' : `./public/assets/${LOC}/LIVECOMP/`, import.meta.url);
const data = JSON.parse(readFileSync(new URL('livecomp.json', dir), 'utf8'));
let failures = 0; const fail = (m) => { if (failures++ < 12) console.error(m); };
const anim = new LiveCompAnimation(data, { random: () => 0 });
const byRes = anim.byResource;
const f = fromBits;
const stateFrom = (inst, h) => {
  const s = liveCompConstruct(inst, [0xffffffff, 1, 0, bitsOf(-1), bitsOf(-1), bitsOf(30), 0, bitsOf(-1), 0, 0, 0], () => 0);
  Object.assign(s, { mode: (h[0] << 16) >> 16, enabled: h[1] | 0, done: h[2] | 0, delay: h[3] | 0, rate: f(h[4]), low: f(h[5]), high: f(h[6]),
    time: f(h[7]), sampleTime: f(h[8]), previous: f(h[9]), unclamped: f(h[10]), dirty: true });
  return s;
};
const head = (s) => [s.mode & 0xffff, s.enabled >>> 0, s.done >>> 0, s.delay >>> 0, bitsOf(s.rate), bitsOf(s.low), bitsOf(s.high), bitsOf(s.time), bitsOf(s.sampleTime), bitsOf(s.previous), bitsOf(s.unclamped)];
const sameHead = (s, h) => head(s).every((x, i) => (i === 0 ? x === (h[0] & 0xffff) : x === h[i]));
const flatBits = (ms) => ms.flatMap((m) => m.flat().map(bitsOf));
if (existsSync(new URL('livecomp-snapshots.json', dir))) {
  const recs = JSON.parse(readFileSync(new URL('livecomp-snapshots.json', dir), 'utf8')).records;
  let clean = 0;
  for (const r of recs) {
    const inst = byRes.get(r.resource); if (r.dirty || r.name.includes('spintwin')) continue;   // spintwin: base matrix from its Spline/Position modifier (0x356078), other port
    const s = stateFrom(inst, r.head); const m = flatBits(liveCompMatrices(s));
    if (m.some((x, i) => x !== r.matrices[i])) fail(`${r.snapshot} ${r.name}: node matrices differ`); clean++;
  }
  const lineage = (name) => name.replace(/\.tick\d+\.p2s$/, '') + (name.startsWith('go') ? '' : '');
  const groups = new Map();
  for (const r of recs) { const k = lineage(r.snapshot) + '|' + r.resource + (r.object === undefined ? '' : '|' + r.object); if (!groups.has(k)) groups.set(k, []); groups.get(k).push(r); }
  let pairs = 0, ticks = 0, rebuilt = 0;
  // Section streaming (SECTIONS/sections.json reference run of the same capture) destroys and rebuilds LiveComps; a
  // rebuilt object can reuse the old address. A pair that does not replay is counted as rebuilt when its resource
  // left the list in between (R&B pickups).
  const sectionsFile = new URL(LOC === 'ARA1' ? './public/assets/ARA1/SECTIONS/sections.json' : `./public/assets/${LOC}/SECTIONS/sections.json`, import.meta.url);
  const leaves = existsSync(sectionsFile) ? (JSON.parse(readFileSync(sectionsFile, 'utf8')).reference_run?.scans ?? []).flatMap((sc) => sc.leave.filter((e) => e[1] === 'destroy').map((e) => [sc.tick, e[0]])) : [];
  const leftBetween = (res, from, to) => leaves.some(([t, r]) => r === res && t >= from && t < to);
  for (const list of groups.values()) {
    list.sort((a, b) => a.frame - b.frame);
    for (let i = 0; i + 1 < list.length; i++) {
      const a = list[i], b = list[i + 1]; if (b.frame === a.frame) continue;
      const s = stateFrom(byRes.get(a.resource), a.head);
      for (let t = a.frame; t < b.frame; t++) liveCompTick(s);
      if (!sameHead(s, b.head) && a.snapshot.startsWith('full.') && leftBetween(a.resource, a.frame, b.frame)) { rebuilt++; continue; }
      if (!sameHead(s, b.head)) fail(`${a.snapshot}->${b.snapshot} ${a.name}: replay ${JSON.stringify(head(s))} != ${JSON.stringify(b.head.slice(0, 11))}`);
      pairs++; ticks += b.frame - a.frame;
    }
  }
  console.log(`${clean} clean PS2 LiveComp snapshots: node matrices bit-exact; ${pairs} consecutive pairs replayed (${ticks} ticks)` + (rebuilt ? `, ${rebuilt} spanning a section rebuild` : ''));
  // 3. doors at GO
  const go = recs.filter((r) => r.snapshot.startsWith('go.') && r.name.includes('startgatedoor'));
  if (go.length) {
    const sim = new LiveCompAnimation(data, { random: () => 0 }); let checked = 0;
    for (let tick = 181; tick <= 255; tick++) {
      if (tick === 181) sim.fire('go');
      sim.tick();
      for (const r of go.filter((x) => x.frame === tick)) { const s = sim.state(r.resource); if (!s || !sameHead(s, r.head)) fail(`door ${r.name} tick ${tick}`); checked++; }
    }
    console.log(`start-gate doors: fire('go') in tick 181 reproduces ${checked} GO-capture door states`);
  }
  // 4. ravens: contact 3404
  const trig = data.instances.flatMap((x) => x.starts).find((s) => s.owner === 'mdl_ARA1_raventriggera_1000');
  const ravens = recs.filter((r) => r.snapshot.startsWith('full.') && r.name.includes('ravenanima'));
  if (trig && ravens.length) {
    const sim = new LiveCompAnimation(data, { random: () => 0 }); let checked = 0;
    for (let tick = 3404; tick <= 3618; tick++) {
      sim.tick();
      if (tick === 3404) sim.fire('contact', trig.ownerResource);   // rider phase, after the entity pass
      for (const r of ravens.filter((x) => x.frame === tick)) {
        const s = sim.state(r.resource); if (!s || !sameHead(s, r.head)) fail(`raven ${r.name} tick ${tick}`);
        else if (!r.dirty && flatBits(liveCompMatrices(s)).some((x, i) => x !== r.matrices[i])) fail(`raven ${r.name} tick ${tick} matrices`);
        checked++;
      }
    }
    console.log(`ravens: contact in tick 3404 + slot-5 chain (1001 at 5/30 s, 1002 at 7/30 s) reproduce ${checked} full-course raven states`);
  }
} else console.log('livecomp-snapshots.json absent: skipping PS2 comparison');
// deltas are identity at rest (bind pose == time with zero channels is not required; check finiteness)
const a2 = new LiveCompAnimation(data, { random: () => 0 });
a2.fire('go'); a2.tick();
const door = data.instances.find((x) => x.name.includes('startgatedoor')); // Crow's Nest has no start gate
const d = door ? a2.nodeDeltas(door.resource) : [[0]];
if (!d || d.flat().some((x) => !Number.isFinite(x))) fail('door native delta');
if (failures) { console.error(`${failures} LiveComp failures`); process.exit(1); }
console.log(`LiveCompAnimation: ${data.instances.length} animated instances OK`);
