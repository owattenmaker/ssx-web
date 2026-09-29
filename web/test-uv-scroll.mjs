// UVScroll (web/uv-scroll.js): replays every live UVScroll of consecutive PS2 race
// snapshots (tools/export_uv_scroll.py uv-scroll-snapshots.json) tick by tick and
// requires the later snapshot's state bit for bit; checks the exported creation
// states against the builtin-21 programs (fence -0.025 u per tick, etc).
// usage: node test-uv-scroll.mjs [LOCATION] (default ARA1; others public/assets/<LOC>/UVSCROLL).
import { readFileSync, existsSync } from 'node:fs';
import { UvScroll, uvScrollTick } from './uv-scroll.js';
import { fromBits, bitsOf } from './ee-scalar-float.js';

const LOC = process.argv[2] || 'ARA1';
const dir = new URL(LOC === 'ARA1' ? './public/assets/UVSCROLL/' : `./public/assets/${LOC}/UVSCROLL/`, import.meta.url);
const data = JSON.parse(readFileSync(new URL('uv-scroll.json', dir), 'utf8'));
let failures = 0;
const fail = (m) => { if (failures++ < 10) console.error(m); };
const fence = data.instances.filter((x) => x.name.startsWith(`mdl_${LOC}_fencebuv_`) && !x.name.includes('fencebuv_eb'));
if ((LOC === 'ARA1' ? fence.length < 200 : false) || fence.some((x) => bitsOf(x.initial.stepU) !== bitsOf(-0.025) || x.initial.stepV !== 0 || x.initial.mode !== 5)) fail('fencebuv UVScroll programs');
if (existsSync(new URL('uv-scroll-snapshots.json', dir))) {
  const snap = JSON.parse(readFileSync(new URL('uv-scroll-snapshots.json', dir), 'utf8'));
  const fields = snap.fields, ints = new Set(['mode', 'active']);
  const state = (w) => Object.fromEntries(fields.map((f, i) => [f, ints.has(f) ? w[i] | 0 : fromBits(w[i])]));
  let ticks = 0, rebuilt = 0;
  const byResource = new Map(data.instances.map((x) => [x.resource, x]));
  for (const p of snap.pairs) {
    const s = state(p.before);
    for (let t = 0; t < p.ticks; t++) uvScrollTick(s, data.fps);
    ticks += p.ticks;
    const got = fields.map((f) => (ints.has(f) ? s[f] >>> 0 : bitsOf(s[f])));
    if (got.some((x, i) => x !== p.after[i]) && LOC !== 'ARA1') {
      // A streamed section (BRA2) can unload and rebuild the UVScroll at the same address between the
      // snapshots: then the later state is the exported creation state stepped k < ticks ticks.
      const init = byResource.get(p.resource)?.initial, fresh = init && { ...init };
      let k = 0; const same = () => fields.every((f, i) => (ints.has(f) ? fresh[f] >>> 0 : bitsOf(fresh[f])) === p.after[i]);
      if (fresh) for (; k < p.ticks && !same(); k++) uvScrollTick(fresh, data.fps);
      if (fresh && k < p.ticks) { rebuilt++; continue; }
    }
    if (got.some((x, i) => x !== p.after[i])) fail(`${p.name}: ${p.ticks} ticks ${JSON.stringify(state(p.after))} != ${JSON.stringify(s)}`);
  }
  console.log(`${snap.pairs.length - rebuilt} PS2 UVScroll snapshot pairs replayed bit-exactly (${ticks} ticks)` + (rebuilt ? `, ${rebuilt} rebuilt by section streaming and matched from the exported creation state` : ''));
} else console.log('uv-scroll-snapshots.json absent: skipping PS2 comparison');
const anim = new UvScroll(data);
for (let t = 0; t < 40; t++) anim.tick();
const [u] = fence.length ? anim.offsetOf(fence[0].resource) : [0];
if (fence.length && Math.abs(Math.abs(u) - 0) > 1e-5 && Math.abs(Math.abs(u) - 1) > 1e-5) fail(`fence period is 40 ticks, u=${u}`);
if (failures) { console.error(`${failures} UV scroll failures`); process.exit(1); }
console.log(`UvScroll: ${data.instances.length} instances (${fence.length} course fences at -0.025 u/tick)`);
