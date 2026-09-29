// Fog puffs (web/fog-puffs.js, docs/visual-parity.md 41):
//  1. the fades at their edges (float32 like 0x2DC190);
//  2. the sprites of the last PS2 frame in kept savestates (local/reference/fog-puffs/frames.json from
//     tools/export_fog_puffs.py --check-state ... --fixture): the camera, the frustum / guard-band clip matrices and the sprite list
//     (depth, alpha, r, g, b in draw order) against buildFogSprites with the same camera (plus 0x2DC190's mode-2 screen-box drop, which
//     the GPU does here by clipping);
//  3. the streamed-package map against the exported packages (when the assets are in).
import fs from 'node:fs';
import * as T from 'three/webgpu';
import { fogPuffLayout, buildFogSprites, clipCulled, instanceFade, puffFade, FOG_PUFF_PEAK, streamedFogRoot, byte128 } from './fog-puffs.js';

let failures = 0, checks = 0;
const fail = (m) => { if (failures++ < 12) console.error(m); };
const expect = (what, got, want) => { checks++; if (got !== want) fail(`${what}: ${got}, want ${want}`); };

// 1. fades
expect('instance near', instanceFade(17999.9), 1);
expect('instance 18000', instanceFade(18000), Math.fround(Math.fround(7000) * Math.fround(0.0001428571413271129)));
expect('instance far', instanceFade(25000), 0);
expect('instance 24999', instanceFade(24999.5) > 0, true);
expect('puff 500', puffFade(1, 500.9), 0);
expect('puff 501', puffFade(1, 501), Math.fround(Math.fround(1) * Math.fround(0.0005000831442885101)));
expect('puff 2499', puffFade(1, 2499.9) < 1, true);
expect('puff 2500', puffFade(1, 2500), 1);
expect('alpha byte', byte128(0.47108), 60);

// 2. PS2 frames
const path = new URL('../local/reference/fog-puffs/frames.json', import.meta.url);
if (fs.existsSync(path)) {
  const cases = JSON.parse(fs.readFileSync(path, 'utf8')).cases; let sprites = 0, identical = 0, rounding = 0;
  for (const c of cases) {
    const V = c.view, A = c.frustum, B = c.guard, layout = fogPuffLayout(c.puffs, [0, 0, 0]);
    // scene (x, y, z) = source (X, Z, -Y) / 100: the view depth row of V as a scene matrix (buildFogSprites reads e[2], e[6], e[10], e[14])
    const mv = new T.Matrix4(); const e = mv.elements; e[2] = -V[2]; e[6] = -V[10]; e[10] = V[6]; e[14] = -V[14] / 100;
    const mul = (M, p) => [0, 1, 2, 3].map((j) => M[j] * p[0] + M[4 + j] * p[1] + M[8 + j] * p[2] + M[12 + j] * p[3]);
    // the box corners back in source cm (scene (x, y, z) = source (X, Z, -Y) / 100)
    const src = new Float64Array(layout.corners.length);
    for (let i = 0; i < layout.corners.length; i += 3) { src[i] = layout.corners[i] * 100; src[i + 1] = -layout.corners[i + 2] * 100; src[i + 2] = layout.corners[i + 1] * 100; }
    const anyFlag = (M, at) => { for (let k = 0; k < 8; k++) { const q = mul(M, [src[at + 3 * k], src[at + 3 * k + 1], src[at + 3 * k + 2], 1]), w = Math.abs(q[3]); if ([0, 1, 2].some((j) => Math.abs(q[j]) > w)) return true; } return false; };
    const result = Array.from({length: layout.instances}, (_, j) => clipCulled(A, src, 24 * j) ? 1 : !anyFlag(A, 24 * j) ? 0 : anyFlag(B, 24 * j) ? 4 : 3); // 0xDB8
    const out = {}, n = buildFogSprites(layout, mv, (j) => result[j] !== 1, out);
    // 0x2DC190 mode 2 (list 2 = result 4): both corners outside one side of the 512 x 448 screen box
    const px = Math.hypot(A[0], A[4], A[8]) / Math.hypot(V[0], V[4], V[8]), py = Math.hypot(A[1], A[5], A[9]) / Math.hypot(V[1], V[5], V[9]);
    const side = (x, y) => (x < 28672 ? 2 : x > 36864 ? 1 : 0) | (y < 29184 ? 8 : y > 36352 ? 4 : 0);
    const list = out.order.slice(0, n).filter((k) => {
      if (result[layout.owner[k]] !== 4) return true;
      const P = layout.pos, src = [P[3 * k] * 100, -P[3 * k + 2] * 100, P[3 * k + 1] * 100, 1], v = mul(V, src), w = v[2], s = layout.size[k] * 100;
      const X = 32768 + 4096 * px * v[0] / w, Y = 32768 - 3584 * py * v[1] / w, sx = 4096 * px * s / w, sy = -3584 * py * s / w;
      return !(side(X - sx, Y - sy) & side(X + sx, Y + sy));
    }).map((k) => [out.depth[k], out.alpha[k], layout.rgb[3 * k], layout.rgb[3 * k + 1], layout.rgb[3 * k + 2]]);
    checks++;
    if (list.length !== c.sprites.length) { fail(`${c.state}: ${list.length} sprites, PS2 ${c.sprites.length}`); continue; }
    let off = 0;
    c.sprites.forEach((p, i) => {
      const q = list[i]; sprites++;
      if (p.every((v, j) => v === q[j])) { identical++; return; }
      // EE single-precision rounding: one sprite's integer depth 1 off (and so its place among equal depths)
      const alt = list.find((r) => Math.abs(r[0] - p[0]) <= 1 && r.slice(1).every((v, j) => v === p[1 + j]));
      if (alt) { rounding++; off++; } else fail(`${c.state} sprite ${i}: PS2 ${p}, page ${q}`);
    });
    if (off > Math.max(3, c.sprites.length / 10)) fail(`${c.state}: ${off} sprites off by rounding`);
  }
  console.log(`PS2 frames: ${cases.length} savestates, ${sprites} sprites: ${identical} identical in draw order, ${rounding} with the depth 1 off (EE rounding)`);
} else console.log('PS2 fog puff frames missing (tools/export_fog_puffs.py --check-state ... --fixture local/reference/fog-puffs/frames.json): skipped');

// 3. streamed packages
const assets = new URL('./public/assets/', import.meta.url);
if (fs.existsSync(new URL('ARA1/fog-puffs.json', assets))) {
  for (const peak of [1, 2, 3]) for (const code of fs.readdirSync(new URL(`PEAK${peak}/`, assets))) {
    const has = fs.existsSync(new URL(`PEAK${peak}/${code}/fog-puffs.json`, assets));
    expect(`PEAK${peak}/${code} in FOG_PUFF_PEAK`, has ? FOG_PUFF_PEAK[code] : undefined, has ? peak : undefined);
  }
  expect('streamed root', streamedFogRoot('E'), '/assets/PEAK3/E/');
} else console.log('fog-puffs.json not in web/public/assets yet: package map skipped');

if (failures) { console.error(`test-fog-puffs: ${failures} failures`); process.exit(1); }
console.log(`test-fog-puffs: ok (${checks} checks)`);
