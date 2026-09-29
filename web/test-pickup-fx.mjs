// Pickup feedback that is not simulation (docs/pickup-recovery.md "Pickup effects and HUD, 2026-09-25"):
// 1. rider FX web/boost_gameplay.inc: the air streamers (RFX+0xAD0, 0x2EF6D0 / draw 0x2EF950) under every jump, turned into
//    the purple prbn beams by a trick boost (rider+0x2EC), and the power-up aura (RFX+0x9C0, 0x2EADD0 / draw 0x2EB198) that
//    wraps the board only while the trick boost holds in the air; neither draws from the random streams.
// 2. the free-ride counter / cash feedback of a collect (web/free-ride-hud.js collectFeedback: HUD slots 0x31 and 0x18).
// The HUD popups themselves ('Collect +$ n' 0x31, cash 0x2E/0x2F) are gated against the original draws in test-trick-hud.mjs;
// the award burst at the frozen magnet matrix against PS2 savestates in test-stage-world.mjs (bhp1-pickup-burst).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
import { collectFeedback } from './free-ride-hud.js';
import { TrickHud } from './trick-hud.js';

const core = await createCore();
process.on('uncaughtException', (e) => { console.error(e instanceof Error ? e : core.getExceptionMessage(e)); process.exit(1); });
const root = new URL('public/assets/', import.meta.url), read = (p) => fs.readFileSync(new URL(p, root)), json = (p) => JSON.parse(read(p));
const put = (bytes) => { const p = core._malloc(bytes.length); core.HEAPU8.set(bytes, p); return p; };
const str = (path) => put(Buffer.concat([read(path), Buffer.from([0])]));
const meta = str('ANIMATIONS/animation-packets.json'), rig = str('RIDER_SAM/rider.json'), cfg = str('ANIMATIONS/initial.json'), packets = read('ANIMATIONS/animation-packets.bin'), packetsPtr = put(packets);
core._init_animation(meta, rig, cfg, packetsPtr, packets.length); core._init_race(cfg); core._animation_use_physics(1);
const mesh = read('ARA1/collision.bin'), mp = put(mesh); core._init_world(mp, mesh.length / 4);
const terrain = str('ARA1/terrain.json'), world = str('ARA1/world_collision.json'), hash = put(Buffer.from(json('ARA1/terrain.json').source_sha256 + '\0'));
core._init_terrain(terrain); core._init_world_collision(world, hash); core._init_body_terrain(terrain);
const start = json('ARA1/start.json');
const info = () => Array.from(new Float32Array(core.HEAPF32.buffer, core._rider_fx_info(), 8));
const vertices = (strip, n) => new Float32Array(core.HEAPF32.buffer, core._rider_fx_vertices(strip), n * 9).slice();

// A jump off the start slope (tuck, jump held 90..150), optionally with a trick boost awarded just before take-off.
function run(trick) {
  core._reset_animation(); core._reset_race(); core._reset_rider(...start.position, start.heading);
  const rows = [];
  for (let tick = 0; tick < 240; tick++) {
    if (tick === 140 && trick) core._award_boost_pickup(2, 5);
    const jump = +(tick >= 90 && tick < 150);
    core._race_begin(); const s = new Float32Array(core.HEAPF32.buffer, core._step_rider(0, jump, 0, 0), 16).slice();
    core._animation_tick(s[7], 0, 0, s[9], s[8], jump, 0, 0, 0, 0, s[15], 0); core._race_end();
    const fx = info(); rows.push({ tick, grounded: s[8], fx, v4: fx[6] ? vertices(4, fx[6]) : null, v5: fx[7] ? vertices(5, fx[7]) : null, v1: fx[3] ? vertices(1, fx[3]) : null });
  }
  return rows;
}
const plain = run(false), trick = run(true);
const air = (rows) => rows.filter((r) => !r.grounded);
assert.ok(air(plain).length > 20, 'the fixture jump leaves the ground');
// Plain jump: strm streamers (texture 63) that grow while airborne, no aura.
assert.ok(air(plain).some((r) => r.fx[6] > 0 && r.fx[7] > 0), 'streamers draw in the air');
assert.ok(plain.every((r) => r.fx[1] === 63 && r.fx[2] + r.fx[3] + r.fx[4] + r.fx[5] === 0), 'no trick boost: strm, no aura');
const grow = air(plain).map((r) => r.fx[6]); assert.ok(Math.max(...grow) > grow[0], 'the streamer ring grows (even frames) while airborne');
// Landing: the ring shrinks by two rows a tick until it stops drawing.
const landed = plain.findIndex((r, k) => k > 0 && r.grounded && !plain[k - 1].grounded);
assert.ok(landed > 0 && plain.slice(landed).some((r) => r.fx[6] === 0), 'streamers fade out after landing');
// Streamer alpha: at most 0.3 (x128 = 38) and decreasing along the ring; RGB 255 (x 2 MODULATE).
{ const r = air(plain).find((x) => x.fx[6] >= 30), v = r.v4;
  let maxA = 0; for (let i = 0; i < r.fx[6]; i++) { maxA = Math.max(maxA, v[i * 9 + 8]); assert.equal(v[i * 9 + 5], 255 / 128); }
  assert.ok(maxA <= 38 / 128 + 1e-6 && maxA > 0.25, `strm alpha ${maxA}`); }
// Trick boost: prbn streamers (61, alpha up to 1, RGB 0.5 x 255) and the four aura faces while airborne.
const trickAir = air(trick).filter((r) => r.tick > 141);
assert.ok(trickAir.length && trickAir.every((r) => r.fx[1] === 61), 'trick boost: prbn streamers');
assert.ok(trickAir.some((r) => r.fx[2] > 0 && r.fx[3] > 0 && r.fx[4] > 0 && r.fx[5] > 0), 'trick boost: the power-up aura draws in the air');
{ const r = trickAir.find((x) => x.fx[6] >= 30), v = r.v4; let maxA = 0; for (let i = 0; i < r.fx[6]; i++) { maxA = Math.max(maxA, v[i * 9 + 8]); assert.equal(v[i * 9 + 5], 127 / 128); } assert.ok(maxA > 0.9 && maxA <= 1, `prbn alpha ${maxA}`); }
{ const r = trickAir.find((x) => x.fx[3] > 0), v = r.v1; // top face: the centre cap (colour 255, alpha 128) then rows (127, 64)
  const alphas = new Set(); for (let i = 0; i < r.fx[3]; i++) alphas.add(Math.round(v[i * 9 + 8] * 128)); assert.ok(alphas.has(128) && alphas.has(64), `aura alphas ${[...alphas]}`); }
assert.ok(trick.filter((r) => r.grounded && r.tick > 141).every((r) => r.fx[2] === 0), 'the aura needs the air (count reset on the ground)');

// pv streamers (web/boost_gameplay.inc set_rider_fx_render_scale): 2EF6D0 / 2EF950 take the nose / tail offsets (90 x row 0) and the
// half width (7.5 x row 2) from the board bone's render matrices (x the rider geometry scale 0.85; PS2 setpieces-bra2 tick 418: a ring
// pair 153 cm apart): with the switch the pair spacing and the strip width are 0.85 x the unit-row ones.
if (!core._set_rider_fx_render_scale) console.log('skip streamer render scale: the core predates set_rider_fx_render_scale');
else {
  const pairs = (rows) => { const r = air(rows).find((x) => x.fx[6] >= 30 && x.fx[7] >= 30), a = r.v4, b = r.v5;
    const d = (i, j, u, v) => Math.hypot(u[i * 9] - v[j * 9], u[i * 9 + 1] - v[j * 9 + 1], u[i * 9 + 2] - v[j * 9 + 2]);
    return { width: d(0, 1, a, a), apart: Math.hypot((a[0] + a[9]) / 2 - (b[0] + b[9]) / 2, (a[1] + a[10]) / 2 - (b[1] + b[10]) / 2, (a[2] + a[11]) / 2 - (b[2] + b[11]) / 2), tick: r.tick }; };
  const unit = pairs(plain); core._set_rider_fx_render_scale(1); const scaled = pairs(run(false)); core._set_rider_fx_render_scale(0);
  const k = json('ANIMATIONS/initial.json').original_animation.scale[0];
  assert.ok(Math.abs(scaled.width / unit.width - k) < 1e-3 && Math.abs(unit.width - 15) < 1e-2, `strip width ${unit.width.toFixed(3)} -> ${scaled.width.toFixed(3)} (x ${k})`);
  assert.ok(Math.abs(scaled.apart - 180 * k) < 0.5 && Math.abs(unit.apart - 180) < 0.5, `nose / tail ${unit.apart.toFixed(2)} -> ${scaled.apart.toFixed(2)} cm apart at the newest ring entry (180 x ${k})`);
}

// Free-ride collect feedback (0x1EF630 -> 21F9B0, 0x1EF850 with 0x1ECBC8's pulse).
const hud = new TrickHud(json('UI/trick-hud.json'), { FEFONT: json('UI/FEFONT-glyphs.json'), HUDFONT: json('UI/HUDFONT-glyphs.json') });
const slots = (entries) => { const s = new Array(44).fill(null); for (const [k, v] of Object.entries(entries)) s[k] = v; return s; };
let fb = collectFeedback({ trickHud: hud, lastState: { trickSlots: slots({}) } });
assert.deepEqual(fb, { counterScale: 1, counterGreen: false, cashScale: 1, cashGreen: false }, 'no popup: plain counter and cash');
fb = collectFeedback({ trickHud: hud, lastState: { trickSlots: slots({ 35: { type: 0x31, maximum: 1.5, value: 0.3 }, 24: { type: 0x18, maximum: 0.7, value: 0.1 } }) } });
assert.ok(fb.counterGreen && fb.cashGreen && fb.counterScale !== 1 && fb.cashScale !== 1, `collect start: green, pulsing ${JSON.stringify(fb)}`);
fb = collectFeedback({ trickHud: hud, lastState: { trickSlots: slots({ 35: { type: 0x31, maximum: 1.5, value: 0.645 } }) } });
assert.ok(fb.counterGreen && fb.counterScale !== 1, 'ratio 0.43: still green and pulsing');
fb = collectFeedback({ trickHud: hud, lastState: { trickSlots: slots({ 35: { type: 0x31, maximum: 1.5, value: 0.9 } }) } });
assert.ok(!fb.counterGreen && fb.counterScale === 1, 'ratio > 0.44: back to the layout colour');
console.log(`pickup FX: streamers ${Math.max(...plain.map((r) => r.fx[6]))} vertices (strm), trick boost prbn + aura ${Math.max(...trick.map((r) => r.fx[3]))} vertices; collect counter/cash feedback OK`);
