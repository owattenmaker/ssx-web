// Remote rider effects (web/fx_puppet.inc, web/net/remote-fx.js): a racer's core records its FX inputs every FX
// pass; they travel in the state packets (web/net/rider-packet.js, 3 records per packet: one full, two deltas); a
// puppet core replays them. With the sender's exact pose the board track, wake, boost, board sparks / fist sparkle and
// every snow particle the puppet builds equal the sender's bit for bit (the records carry the sender's particle and
// visual random words: its camera draws from the particle generator between FX passes).
import assert from 'node:assert/strict';
import { createTestRider, scriptedPad, file } from './net/test-core.mjs';
import { drainFx, encodeState, decodeFrame } from './net/rider-packet.js';
import { createFxPuppet } from './net/remote-fx.js';

const rider = await createTestRider({ lighting: true });
rider.startEvent(); rider.core._fx_recording(1);
const puppet = await createFxPuppet({ riderText: file('RIDER_ZOE/rider.json').toString(), settingsText: file('ANIMATIONS/initial.json').toString(),
  packetsJson: file('ANIMATIONS/animation-packets.json').toString(), packetsBin: new Uint8Array(file('ANIMATIONS/animation-packets.bin')),
  environment: { meta: JSON.parse(file('ARA1/environment.json')), bytes: new Uint8Array(file('ARA1/environment.bin')) } });
const c = rider.core, p = puppet.core, f = (core, ptr, n) => new Float32Array(core.HEAPF32.buffer, ptr, n);
const same = (a, b) => a.length === b.length && a.every((x, i) => Object.is(x, b[i]));
const poses = new Map();
let packets = 0, fxBytes = 0, compared = 0, snowCountDiff = 0, snowDataDiff = 0, trails = 0, wakes = 0;
const mismatch = {};
let pending = [], placements = 0, rescues = 0;
for (let t = 0; t < 2400; t++) {
  const { state } = rider.tick(scriptedPad(t, 11));
  const posePtr = c._world_pose_bones(), bones = f(c, posePtr, 1)[0];
  poses.set(t, f(c, posePtr + 4, bones * 7).slice());
  // The sender tags a tick's record with the reset its core made during that tick (before the FX pass).
  const nowPlacements = f(c, c._reset_info(), 3)[2], nowRescues = state[13], reset = nowRescues !== rescues ? 2 : nowPlacements !== placements ? 1 : 0;
  placements = nowPlacements; rescues = nowRescues;
  pending.push(...drainFx(c).map((x, i, all) => ({ ...x, t, reset: i === all.length - 1 ? reset : 0 })));
  if (t % 3 !== 2) continue;
  // One packet: the tick's pose plus the FX records since the last packet (their ticks from `back`).
  const fx = pending.map((x) => ({ back: t - x.t, values: x.values, reset: x.reset })); pending = [];
  const bytes = encodeState({ tick: t, pose: poses.get(t), scale: [1, 1, 1], fx, placements, rescues });
  const d = decodeFrame(new Uint8Array([1, ...bytes])); packets++; fxBytes += bytes.length - (56 + bones * 28);
  assert.ok(d?.fx?.length === fx.length, 'FX records round trip');
  for (const x of d.fx) {
    assert.ok(same(x.values, fx[d.fx.indexOf(x)].values), 'FX record values exact');
    const tick = d.tick - x.back;
    puppet.step(x.values, poses.get(tick), x.reset);
    // Compare after each replayed pass with the sender's outputs of that tick (only the newest tick's are still live).
    if (tick !== t) continue;
    compared++;
    const ti = f(c, c._trail_info(), 6), pi = f(p, p._trail_info(), 6);
    const checks = {
      trail: same(f(c, c._trail_ribbon(), ti[0] * 9), f(p, p._trail_ribbon(), pi[0] * 9)) && same(f(c, c._trail_roof(), ti[1] * 9), f(p, p._trail_roof(), pi[1] * 9)),
      wake: same(f(c, c._wake_vertices(), f(c, c._wake_info(), 13)[11] * 9), f(p, p._wake_vertices(), f(p, p._wake_info(), 13)[11] * 9)),
      boost: [0, 1, 2].every((k) => same(f(c, c._boost_fx_vertices(k), f(c, c._boost_fx_info(), 12)[6 + k] * 9), f(p, p._boost_fx_vertices(k), f(p, p._boost_fx_info(), 12)[6 + k] * 9))),
      impact: same(f(c, c._impact_fx_info(), 5).slice(1), f(p, p._impact_fx_info(), 5).slice(1)),
    };
    for (const [k, ok] of Object.entries(checks)) if (!ok) mismatch[k] ??= tick;
    if (ti[0]) trails++; if (f(c, c._wake_info(), 13)[11]) wakes++;
    const sc = f(c, c._snow_info(), 10), sp = f(p, p._snow_info(), 10); if (!same(sc, sp)) snowCountDiff++;
    else if (![...Array(10).keys()].every((k) => same(f(c, c._snow_particles(k), sc[k] * 8), f(p, p._snow_particles(k), sp[k] * 8)))) snowDataDiff++;
  }
}
console.log(`FX: ${packets} packets, ${Math.round(fxBytes / packets)} B of FX records per packet, ${compared} ticks compared (${trails} with a track, ${wakes} with a wake), snow count differences ${snowCountDiff}, particle data differences ${snowDataDiff}`, mismatch);
assert.deepEqual(mismatch, {}, 'track, wake, boost and sparks equal the sender');
assert.ok(trails > 500 && wakes > 50, 'the run has tracks and wakes');
assert.ok(fxBytes / packets < 600, 'FX records stay small');
assert.equal(snowCountDiff + snowDataDiff, 0, 'snow particles equal the sender');
console.log('remote rider effects: FX records, puppet replay (track, wake, boost, sparks, snow exact) OK');
