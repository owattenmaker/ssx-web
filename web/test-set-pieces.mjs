// Snow Jam chairlifts (MultiSplineModifier, web/set_piece_gameplay.inc) against the PS2 capture
// local/ps2-capture/runs/setpieces/race.bin (watch windows 0x592E80/0x593580 modifiers and 0xBAB330
// clones): after world load the core holds the race-tick-0 state; each race_begin is one original
// tick (0x35A560 + LiveComp 0x3568B0), so capture record T equals the state after T + 1 race_begins.
// Also checks the draw deltas (moving_instances keys resource + car * 2^20) and the collision clones.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const capturePath = '../local/ps2-capture/runs/setpieces/race.bin';
if (!fs.existsSync(capturePath) || !fs.existsSync('../local/event-activation/set-pieces.json')) { console.log('Set pieces: capture/export not present, skipped'); process.exit(0); }
const c = await createCore();
const str = (text) => { const data = new TextEncoder().encode(text + '\0'), p = c._malloc(data.length); c.HEAPU8.set(data, p); return p; };
const world = fs.readFileSync('public/assets/ARA1/world_collision.json', 'utf8'), hash = JSON.parse(fs.readFileSync('public/assets/ARA1/terrain.json', 'utf8')).source_sha256;
c._init_world_collision(str(world), str(hash));
c._init_rails(str(fs.readFileSync('public/assets/ARA1/rails.json', 'utf8')), str(hash));
const manifest = JSON.parse(fs.readFileSync(capturePath.replace(/\.bin$/, '.capture.json'), 'utf8'));
const RECORD = manifest.record, watches = manifest.layout.watches, base = manifest.layout.watch_offset;
const windowOf = (address) => { let offset = 0; for (const w of watches) { const a = Number(w.address); if (address >= a && address + 4 <= a + w.length) return offset + address - a; offset += w.length; } throw Error('address outside the watches'); };
const data = fs.readFileSync(capturePath), view = new DataView(data.buffer, data.byteOffset, data.byteLength);
const records = new Map(); for (let k = 0; k + RECORD <= data.length; k += RECORD) records.set(view.getUint32(k + 4, true), k);
const lifts = [[0x592E80, [0xBAB6A0, 0xBAB5F0, 0xBAB540]], [0x593580, [0xBAB490, 0xBAB3E0, 0xBAB330]]];
const bits = () => { const p = c._set_piece_bits(), n = new Uint32Array(c.HEAPU8.buffer, p, 1)[0]; return new Uint32Array(c.HEAPU8.buffer, p, 1 + n * 49).slice(); };
assert.equal(bits()[0], 2, 'two chairlifts attached at world load');
const ticks = Number(process.env.SET_PIECE_TICKS || 2400);
let compared = 0;
for (let tick = 0; tick < ticks; tick++) {
  c._race_begin();
  const at = records.get(tick); if (at === undefined) continue;
  const web = bits(), word = (address) => view.getUint32(at + base + windowOf(address), true);
  lifts.forEach(([modifier, clones], l) => {
    const o = 1 + l * 49;
    assert.equal(web[o], word(modifier + 0x10), `tick ${tick} lift ${l} distance`);
    clones.forEach((clone, k) => { for (let j = 0; j < 16; j++) assert.equal(web[o + 1 + k * 16 + j], word(clone + 0x10 + 4 * j), `tick ${tick} lift ${l} car ${k} matrix word ${j}`); });
  });
  compared++;
}
assert.ok(compared > 1000);
const info = new Float32Array(c.HEAPF32.buffer, c._set_piece_info(), 2 + 2 * 12).slice();
assert.equal(info[1], ticks);
// Draw deltas: six cars, keys resource + car * 2^20, rigid 3x3 blocks.
const p = c._moving_instances(), count = c.HEAPF32[p >> 2], keys = [];
for (let i = 0; i < count; i++) { const v = new Float32Array(c.HEAPF32.buffer, p + 4 + i * 68, 17); keys.push(v[0]);
  for (let col = 0; col < 3; col++) assert.ok(Math.abs(Math.hypot(v[1 + col * 4], v[2 + col * 4], v[3 + col * 4]) - 1) < 1e-4, 'delta rotation is orthonormal'); }
assert.deepEqual(keys.sort((a, b) => a - b), [407560, 711688, 407560 + 1048576, 711688 + 1048576, 407560 + 2097152, 711688 + 2097152].sort((a, b) => a - b));
// Collision clones are runtime instances, not package instances.
const packageCount = JSON.parse(world).instances.length;
assert.equal(new Int32Array(c.HEAPU8.buffer, c._world_collision_info(), 8)[0], packageCount);
// Log teeters: four bound, at rest the node-1 draw delta is the identity (to float rounding).
const tq = c._set_piece_teeters() >> 2, teeters = c.HEAPF32[tq];
assert.equal(teeters, 4, 'four log teeters bound on ARA1');
for (let k = 0; k < teeters; k++) { const e = c.HEAPF32.subarray(tq + 5 + 17 * k, tq + 21 + 17 * k); [0, 5, 10, 15].forEach((i) => assert.ok(Math.abs(e[i] - 1) < 1e-4)); [12, 13, 14].forEach((i) => assert.ok(Math.abs(e[i]) < 1e-3)); }
assert.equal(c.HEAPF32[tq + 2], 0, 'no rider force on a scripted-free run');
console.log('Set pieces: chairlift distance and six car matrices bit-exact vs PS2 on', compared, 'capture ticks; draw deltas', keys.length);
