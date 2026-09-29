// Set-piece particle draw (web/set-piece-particle-sprites.js) against the original VU1
// program run on live PS2 emitters (tools/test_set_piece_particle_draw_native.py ->
// local/browser-validation/set-piece-particle-draw-oracle.json).
import assert from 'node:assert/strict';
import {readFileSync, existsSync} from 'node:fs';
import {burstSprites, trailSprites, lfsrNext, vuDiv, SET_PIECE_PARTICLE_DRAW} from './set-piece-particle-sprites.js';

// Unit checks that need no oracle.
assert.equal(lfsrNext(0x3f800001), 0x3f800002);
assert.equal(vuDiv(1, 3), Math.fround(1 / 3) > 1 / 3 ? 0.33333331346511841 : Math.fround(1 / 3));
assert.equal(SET_PIECE_PARTICLE_DRAW.blend[0].gsAlpha, 0x48);
assert.equal(SET_PIECE_PARTICLE_DRAW.blend[1].gsAlpha, 0x44);

const path = new URL('../local/browser-validation/set-piece-particle-draw-oracle.json', import.meta.url);
if (!existsSync(path)) {
  console.log('set-piece particle draw: oracle missing (run tools/test_set_piece_particle_draw_native.py); unit checks only');
  process.exit(0);
}
const oracle = JSON.parse(readFileSync(path, 'utf8'));
const signed = (x) => x | 0;
const expected = (words) => ({rgba: words.slice(4, 8).map(signed), v0: words.slice(8, 12).map(signed), v1: words.slice(16, 20).map(signed)});
const stats = {cases: 0, synthetic: 0, sprites: 0, approximateTail: 0, byKind: {}};
for (const c of oracle.cases) {
  const vu = {bits: c.vuRows};
  const label = `${c.state.split('/').pop()} ${c.kind} 0x${c.modifier.toString(16)} tex${c.texture}`;
  if (c.kind === 'particle') {
    const got = burstSprites(c.kernel, {vu}), want = c.oracle[0].sprites;
    assert.equal(got.length, want.length, `${label}: sprite count ${got.length} vs ${want.length}`);
    got.forEach((s, i) => assert.deepEqual(s.gs && {rgba: s.gs.rgba, v0: s.gs.v0, v1: s.gs.v1}, expected(want[i]), `${label}: sprite ${i} (slot ${s.slot} copy ${s.copy})`));
    stats.sprites += got.length;
  } else {
    const got = trailSprites({kernel: c.kernel, capacity: c.capacity, cursor: c.cursor, particleCount: c.particleCount, positions: c.positions, velocities: c.velocities}, {vu});
    c.oracle.forEach((batch, b) => {
      const mine = got.filter((s) => s.batch === b), exact = mine.filter((s) => !s.approximate), want = batch.sprites;
      // Approximate particles (the extra group's i > 0, reading stale VU rows) come last.
      assert.ok(want.length >= exact.length, `${label} batch ${b}: oracle ${want.length} < exact ${exact.length}`);
      exact.forEach((s, i) => assert.deepEqual({rgba: s.gs.rgba, v0: s.gs.v0, v1: s.gs.v1}, expected(want[i]), `${label} batch ${b}: sprite ${i} (group ${s.group} particle ${s.particle})`));
      stats.approximateTail += want.length - exact.length;
      stats.sprites += exact.length;
    });
  }
  stats.cases++; if (c.synthetic) stats.synthetic++;
  const key = `${c.kind} tex${c.texture} blend${c.blend}`;
  stats.byKind[key] = (stats.byKind[key] || 0) + 1;
}
// The world-space output (what a renderer draws) projects onto the original pixels: centre
// and half size (world extent, capped at 64 px) within 1/4 px (both GS vertices are ftoi4-truncated).
const bits = new DataView(new ArrayBuffer(4));
const float = (u) => (bits.setUint32(0, u >>> 0), bits.getFloat32(0));
let worstCentre = 0, worstHalf = 0, checked = 0;
for (const c of oracle.cases) {
  const r = c.vuRows.map((row) => row.map(float));
  const sprites = c.kind === 'particle' ? burstSprites(c.kernel, {vu: {bits: c.vuRows}})
    : trailSprites({kernel: c.kernel, capacity: c.capacity, cursor: c.cursor, particleCount: c.particleCount, positions: c.positions, velocities: c.velocities}, {vu: {bits: c.vuRows}});
  for (const s of sprites) {
    if (s.approximate) continue;
    assert.ok(s.positionCm.every(Number.isFinite) && s.halfSizeCm >= 0 && s.colour.every((x) => x >= 0 && x <= 255));
    const [x, y, z] = s.positionCm, clip = [0, 1, 2, 3].map((i) => x * r[0][i] + y * r[1][i] + z * r[2][i] + r[3][i]);
    const centre = [0, 1].map((i) => r[5][i] + (clip[i] / clip[3]) * r[4][i]);
    const half = [0, 1].map((i) => Math.min(64, Math.abs(s.halfSizeCm * r[6][i] * r[4][i] / clip[3])));
    const gsCentre = [0, 1].map((i) => (s.gs.v0[i] + s.gs.v1[i]) / 32), gsHalf = [0, 1].map((i) => Math.abs(s.gs.v1[i] - s.gs.v0[i]) / 32);
    worstCentre = Math.max(worstCentre, ...centre.map((v, i) => Math.abs(v - gsCentre[i])));
    worstHalf = Math.max(worstHalf, ...half.map((v, i) => Math.abs(v - gsHalf[i])));
    checked++;
  }
}
assert.ok(worstCentre < 0.25 && worstHalf < 0.25, `world output vs GS: centre ${worstCentre} px, half ${worstHalf} px`);
console.log(`set-piece particle world output: ${checked} sprites project within ${worstCentre.toFixed(4)} px (centre) / ${worstHalf.toFixed(4)} px (half size) of the GS sprites`);
console.log(`set-piece particle draw: ${stats.cases - stats.synthetic} live emitters + ${stats.synthetic} NumBlur-3 variants, ${stats.sprites} GS sprites bit-exact vs the original VU program (${stats.approximateTail} stale-row tail sprites excluded)`, JSON.stringify(stats.byKind));
