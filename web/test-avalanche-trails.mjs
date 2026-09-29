// Avalanche trails (web/avalanche-trails.js, docs/avalanche.md "Trails"): the sprite evaluation (OriginalSnowParticles::particles,
// particle entry 0xA00) on the PS2's own emitter images and rings (local/reference/avalanche/trails.json, kept states):
//  - particles per birth = kernel N / ring slots, one sprite per live birth younger than its lifetime (count = +0x1E0 here);
//  - newest birth first: the first sprite sits at ring A of cursor + 1 at age 0;
//  - GS colours in range; the retail kernels' ColBase is the ambient colour (tumbler +480, alpha 0), so the alpha stays near 0.
import fs from 'node:fs';
import { trailSprites, TRAIL_FLOATS } from './avalanche-trails.js';

let failures = 0, checks = 0;
const fail = (m) => { if (failures++ < 12) console.error(m); };
const expect = (what, got, want) => { checks++; if (got !== want) fail(`${what}: ${got}, want ${want}`); };
const path = new URL('../local/reference/avalanche/trails.json', import.meta.url);
if (!fs.existsSync(path)) { console.log('test-avalanche-trails: no fixture (local/reference/avalanche/trails.json), skipped'); process.exit(0); }
const doc = JSON.parse(fs.readFileSync(path, 'utf8'));
const obj = (x) => { const image = Uint32Array.from(x.image), ringBits = Uint32Array.from(x.ringB);
  return { image, imageF: new Float32Array(image.buffer), n: x.n, ringA: new Float32Array(Uint32Array.from(x.ringA).buffer), ringB: new Float32Array(ringBits.buffer), ringBits, colours: Uint32Array.from(x.colours) }; };
let trails = 0, alphaSprites = 0;
for (const [state, s] of Object.entries(doc.states)) for (const x of s.trails) {
  const t = obj(x), out = new Float32Array(4096 * TRAIL_FLOATS), n = trailSprites(t, out, 0, 4096); trails++;
  const active = t.image[0x1E0 >> 2], per = Math.trunc(t.image[0x20 >> 2] / t.n), cursor = t.image[0x17C >> 2];
  expect(`${state} ${x.resource} count`, n, active * per);
  const first = (cursor + 1) % t.n;
  expect(`${state} ${x.resource} newest first`, [0, 1, 2].every((c) => Math.abs(out[c] - t.ringA[first * 4 + c]) < 400), true);
  let ok = true; for (let i = 0; i < n; i++) for (let c = 4; c < 8; c++) { const v = out[i * TRAIL_FLOATS + c]; if (!(v >= 0 && v <= 255 && Number.isInteger(v))) ok = false; }
  expect(`${state} ${x.resource} colours`, ok, true);
  for (let i = 0; i < n; i++) if (out[i * TRAIL_FLOATS + 7] > 0) alphaSprites++;
  const disabled = Uint32Array.from(t.image); disabled[0x174 >> 2] = 0;
  expect(`${state} ${x.resource} disabled draws nothing`, trailSprites({ ...t, image: disabled, imageF: new Float32Array(disabled.buffer) }, out, 0, 4096), 0);
}
expect('trails in the fixture', trails, 19);
expect('sprites with alpha > 0 (EBA3 820 only)', alphaSprites, 15);
if (failures) { console.error(`test-avalanche-trails: ${failures} of ${checks} checks failed`); process.exit(1); }
console.log(`test-avalanche-trails: ok (${checks} checks, ${trails} PS2 trails)`);
