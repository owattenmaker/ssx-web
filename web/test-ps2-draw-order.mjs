// The shared PS2 draw order (web/ps2-draw-order.js, docs/visual-parity.md 41.9):
//  1. drawKey is 0x364240 (hand-built keys, the second texture's low bits, the mode table 0x492010);
//  2. every effect's fields against the last frame's sorted render list in kept PS2 states (local/reference/draw-order/buckets.json:
//     key, priority, t0, rank and textures of the priority 7 / 8 buckets with FX-table textures): the effect specs give the RAM keys,
//     in both texture-handle tables (event boot, Conquer the Mountain), and drawOrder sorts them in the RAM order;
//  3. drawOrder keeps the priorities in their bands and the world's sorted classes before every effect.
import fs from 'node:fs';
import { drawKey, drawOrder, setDrawOrderWorld, fxHandle, FX_HANDLES, FX_SPEC, EFFECT, WORLD_SORTED_ORDER, EFFECT_ORDER_BASE } from './ps2-draw-order.js';

let failures = 0, checks = 0;
const fail = (m) => { if (failures++ < 16) console.error(m); };
const expect = (what, got, want) => { checks++; if (got !== want) fail(`${what}: ${got}, want ${want}`); };

// 1. the key
setDrawOrderWorld(false);
expect('fog puffs (ABC1 2000)', drawKey(EFFECT.fogPuffs).toString(16), '9c00905f');
expect('snowwind brth + spec', drawKey(EFFECT.setPieceParticle(25)).toString(16), '9fffc0f7');
expect('snow spry', drawKey(EFFECT.snow(5)).toString(16), '9fffd067');
expect('no texture', drawKey({ priority: 7, mode: 4 }), ~(24 << 26 | 1 << 13) >>> 0);
expect('second texture alone', drawKey({ priority: 7, mode: 0, handle2: 0x60d }), ~(24 << 26 | 0xd << 9) >>> 0);
expect('mode table', [0, 1, 2, 3, 4, 5, 6, 7, 8, 9].map((m) => (~drawKey({ priority: 7, mode: m }) >>> 13) & 7).join(), '0,4,5,3,1,6,2,2,1,0');
expect('spec is FX 46', fxHandle(FX_SPEC), 1549);
setDrawOrderWorld(true); expect('CTM spec', fxHandle(FX_SPEC), 1555); expect('CTM wake', fxHandle(56), 1549); setDrawOrderWorld(false);

// 2. PS2 RAM
const spec = (b) => {
  const { priority: p, rank, fx, fx2, t0 } = b, snowTex = [5, 6, 14, 15, 16, 17, 18, 19, 20, 21, 25];
  if (p === 7 && fx === 4 && t0 === 1023) return EFFECT.fogPuffs;
  if (p === 7 && rank === 1 && fx2 === FX_SPEC) return EFFECT.setPieceParticle(fx);
  if (p === 7 && rank === 1 && fx2 < 0) return snowTex.includes(fx) ? EFFECT.snow(fx) : fx === 22 ? EFFECT.sparks : { priority: 7, mode: 4, fx };
  if (p === 7 && rank === 3) return fx === 56 ? EFFECT.wake : fx >= 57 && fx <= 61 ? EFFECT.boost(fx) : fx === 62 ? EFFECT.aura : fx === 23 ? EFFECT.icon : fx === 43 ? EFFECT.beam : null;
  if (p === 7 && rank === 6 && fx === 8) return EFFECT.snowfall;
  if (p === 7 && rank === 0 && (fx === 53 || fx === 54)) return { priority: 7, mode: 0, fx }; // light glows 2E2868 (post pass)
  if (p === 8 && rank === 3) return EFFECT.streamer(fx);
  if (p === 8 && rank === 0) return fx >= 37 && fx <= 42 ? EFFECT.halo(fx) : fx === 66 || fx === 67 ? EFFECT.splash(fx) : { priority: 8, t0: 1023, mode: 0, fx };
  return null;
};
const path = new URL('../local/reference/draw-order/buckets.json', import.meta.url);
if (fs.existsSync(path)) {
  const doc = JSON.parse(fs.readFileSync(path, 'utf8')); let rows = 0, mapped = 0; const worlds = new Set();
  for (const s of doc.states) {
    setDrawOrderWorld(s.world === 'ctm'); worlds.add(s.world);
    const keyed = [];
    for (const b of s.buckets) {
      rows++; const e = spec(b); if (!e) { fail(`${s.state}: unmapped ${b.tag} rank ${b.rank}`); continue; }
      mapped++; expect(`${s.state} ${b.tag} key`, drawKey(e).toString(16), b.key.toString(16)); keyed.push([drawOrder(e), b.key]);
    }
    const byOrder = keyed.slice().sort((a, b) => a[0] - b[0]).map((x) => x[1]).join(), ram = keyed.map((x) => x[1]).join();
    expect(`${s.state} order`, byOrder, ram);
  }
  setDrawOrderWorld(false);
  expect('both handle tables covered', [...worlds].sort().join(), 'ctm,event');
  console.log(`  PS2 RAM: ${doc.states.length} states, ${mapped}/${rows} buckets`);
} else console.log('  (no local/reference/draw-order/buckets.json: PS2 RAM checks skipped)');

// 3. bands
expect('priority 6 band', drawOrder({ priority: 6, t0: 1023, mode: 5 }) >= EFFECT_ORDER_BASE, true);
expect('priority 7 in [680, 700)', [EFFECT.fogPuffs, EFFECT.snow(21), EFFECT.fist].every((e) => drawOrder(e) >= 680 && drawOrder(e) < 700), true);
expect('priority 8 in [700, 720)', [EFFECT.streamer(63), EFFECT.halo(37), EFFECT.splash(67)].every((e) => drawOrder(e) >= 700 && drawOrder(e) < 720), true);
expect('world sorted classes first', WORLD_SORTED_ORDER < EFFECT_ORDER_BASE, true);
expect('submission breaks a tie only', drawOrder(EFFECT.chunks(14), 11) < drawOrder(EFFECT.snow(14), 18) && drawOrder(EFFECT.snow(14), 18) < drawOrder(EFFECT.snow(13 + 2)), true);
expect('tables have 80 entries', FX_HANDLES.event.length + FX_HANDLES.ctm.length, 160);

if (failures) { console.error(`test-ps2-draw-order: ${failures} of ${checks} checks failed`); process.exit(1); }
console.log(`test-ps2-draw-order: ok (${checks} checks)`);
