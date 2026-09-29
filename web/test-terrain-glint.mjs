// Terrain glint (pv terrainGlint; web/world-material.js glintBytes / glintUv, docs/visual-parity.md 41.6):
//  1. the UV against the PS2: E at terrain +0x360 (38B370) in five savestates (local/reference/terrain-glint/e-matrix.json), the
//     patch normal of 374518 = the opposite of the package normal, UV = (n, 1) x E;
//  2. the exported packages (when the assets are in): patch resources of terrain-render.json, 1 grey / 2 blue, both 32 x 32 images.
import fs from 'node:fs';
import { glintUv } from './world-material.js';

let failures = 0, checks = 0;
const fail = (m) => { if (failures++ < 12) console.error(m); };
const path = new URL('../local/reference/terrain-glint/e-matrix.json', import.meta.url);
if (fs.existsSync(path)) {
  let worst = 0;
  for (const c of JSON.parse(fs.readFileSync(path, 'utf8')).cases) {
    const E = c.E, cam = [c.camera[0] / 100, c.camera[2] / 100, -c.camera[1] / 100]; // source cm -> native m
    for (let k = 0; k < 200; k++) {
      const t = k * 0.7, p = k * 1.3, nn = [Math.sin(t) * Math.cos(p), Math.cos(t), Math.sin(t) * Math.sin(p)]; // native unit normal
      const n = [-nn[0], nn[2], -nn[1]]; // 374518's normal, source axes
      const want = [0, 1].map((j) => n[0] * E[j] + n[1] * E[4 + j] + n[2] * E[8 + j] + E[12 + j]); // (n, 1) x E, qwords = rows
      const got = glintUv(nn, cam); checks++;
      worst = Math.max(worst, Math.abs(got[0] - want[0]), Math.abs(got[1] - want[1]));
    }
  }
  if (worst > 2e-4) fail(`glint UV vs the PS2 E: worst ${worst}`);
  console.log(`glint UV against the PS2 E (5 states): worst ${worst.toExponential(2)}`);
} else console.log('E-matrix fixture missing: skipped');
const assets = new URL('./public/assets/', import.meta.url);
for (const loc of ['ARA1', 'EBC3', 'ERA5', 'ESS3']) {
  const f = new URL(`${loc}/terrain-glint.json`, assets);
  if (!fs.existsSync(f)) { console.log(`${loc}/terrain-glint.json not in web/public/assets yet: skipped`); continue; }
  const g = JSON.parse(fs.readFileSync(f, 'utf8')), render = JSON.parse(fs.readFileSync(new URL(`${loc}/terrain-render.json`, assets), 'utf8'));
  const known = new Set(render.patches.map((p) => p.resource)); checks++;
  if (!g.patches.every(([r, k]) => known.has(r) && (k === 1 || k === 2))) fail(`${loc}: glint patches outside terrain-render.json`);
  for (const [kind, im] of Object.entries(g.textures)) { checks++; if (im.width !== 32 || im.height !== 32 || Buffer.from(im.rgba, 'base64').length !== 4096) fail(`${loc}: ${kind} image`); }
}
if (failures) { console.error(`test-terrain-glint: ${failures} failures`); process.exit(1); }
console.log(`test-terrain-glint: ok (${checks} checks)`);
