// Online rider packets (web/net/rider-packet.js) and remote riders (web/net/remote-riders.js):
//   * a recorded run (scripted pad, tricks, crashes) of a human core: every tick the rider's packet (pose, pair view,
//     lighting, 3 FX records) is encoded, decoded and the skin palette rebuilt on the "receiver"
//     (web/net/pose-codec.js) -- it must equal the sender's core palette bit for bit; the packet stays <= 2 KB;
//   * interpolation, dead-reckoned position correction, ghost pair view, teleports, attack events.
import assert from 'node:assert/strict';
import { createTestRider, scriptedPad, json } from './net/test-core.mjs';
import { skinLayout, buildPalette } from './net/pose-codec.js';
import { captureState, encodeState, decodeFrame, encodeAttack, drainFx, stateSize, ATTACK, STATE } from './net/rider-packet.js';
import { createRemoteRiders } from './net/remote-riders.js';

const frameOf = (slot, bytes) => { const f = new Uint8Array(bytes.length + 1); f[0] = slot; f.set(bytes, 1); return f; };
const environment = json('ARA1/environment.json');
for (const pkg of ['RIDER_ZOE', 'RIDER_PSYMON']) {
  const rider = await createTestRider({ rider: pkg, lighting: true });
  const layout = skinLayout(json(pkg + '/rider.json'));
  rider.startEvent(); rider.core._fx_recording(1);
  let recent = [], compared = 0, maxBytes = 0, total = 0, crashTicks = 0, lit = 0;
  for (let t = 0; t < 1800; t++) {
    rider.tick(scriptedPad(t, pkg.length));
    const c = rider.core;
    recent.push(...drainFx(c).map((r) => ({ t, values: r.values }))); recent = recent.slice(-3); // a 20 Hz packet carries 3 FX passes
    const s = captureState(c, { tick: t, lightingConfig: environment.irradiance, fx: recent.map((r) => ({ back: t - r.t, values: r.values })) });
    if (!s) continue;
    const bytes = encodeState(s), d = decodeFrame(frameOf(2, bytes));
    assert.ok(d && d.kind === STATE && d.slot === 2 && d.tick === t, `tick ${t}: packet round trip`);
    maxBytes = Math.max(maxBytes, bytes.length); total += bytes.length;
    const ref = new Uint32Array(c.HEAPU8.buffer, c._rider_skin_palette(), c._rider_skin_palette_count() * 16);
    const mine = new Uint32Array(buildPalette(layout, d.pose, d.scale).buffer);
    for (let i = 0; i < ref.length; i++) if (ref[i] !== mine[i]) assert.fail(`${pkg} tick ${t}: palette word ${i} ${ref[i].toString(16)} != ${mine[i].toString(16)}`);
    assert.deepEqual([...d.pair], [...new Uint32Array(c.HEAPU8.buffer, c._pair_view(), 140)].map((w, i) => (i >= 7 + 4 * Math.min(d.pair[1], 20) && i < 87) || i >= 137 ? 0 : w), `tick ${t}: pair view`);
    if (d.lighting) lit++;
    if (new Float32Array(c.HEAPF32.buffer, c._crash_info(), 1)[0]) crashTicks++;
    compared++;
  }
  console.log(`${pkg}: ${compared} ticks, palette bit-exact from the pose packet (${layout.groups.length} groups, ${layout.bones} bones), packet max ${maxBytes} B, mean ${Math.round(total / compared)} B, ${crashTicks} crash ticks, lighting in ${lit}`);
  assert.ok(compared > 1500 && crashTicks > 50, 'the run covers riding and crashes');
  assert.ok(maxBytes <= 2048, `packet ${maxBytes} B > 2 KB`);
  assert.ok(total / compared <= 2000, 'mean packet within 2 KB with the FX records');
  assert.equal(lit, compared, 'lighting inputs travel with every packet');
}
assert.equal(stateSize(27, 16), 56 + 27 * 28 + (7 + 64 + 50) * 4 + 200);

// ---- receiver behaviour ----
const rig = json('RIDER_ZOE/rider.json'), layout = skinLayout(rig), bones = layout.bones;
const pose = (x) => { const p = new Float32Array(bones * 7); for (let b = 0; b < bones; b++) { p[b * 7] = x; p[b * 7 + 6] = 1; } return p; };
const pair = (x) => { const w = new Uint32Array(140), f = new Float32Array(w.buffer); w[0] = 1; w[1] = 2; f[3] = x; f[7] = x + 10; f[11] = x + 20; f[99] = x; f[132] = x; f[87] = 600; return w; };
const packet = (tick, x, serial = 0) => frameOf(3, encodeState({ tick, pose: pose(x), scale: [1, 1, 1], pair: pair(x), remaining: 1000 - x, serial }));
const remote = createRemoteRiders({ delayTicks: 6 });
remote.add(3, rig, { name: 'Bob' });
remote.receive(packet(0, 0), 0); remote.receive(packet(3, 30), 50); remote.receive(packet(6, 60), 100);
// Render tick 10 -> pose tick 4 (between 3 and 6, 1/3), position corrected to the tick-10 prediction: 60 + 600 * 4/60 = 100.
let [o] = remote.frame([3], 10, 1000);
const expectedPose = buildPalette(layout, pose(40), [1, 1, 1]);
// The first frame snaps the correction; x translation lanes = interpolated pose (40) + offset (100 - 40).
assert.ok(Math.abs(o.core.palette[12] - (expectedPose[12] + 60)) < 1e-3, `corrected x ${o.core.palette[12]} vs ${expectedPose[12] + 60}`);
assert.ok(Math.abs(o.core.palette[0] - expectedPose[0]) < 1e-6);
const ghost = new Float32Array(remote.ghost(3, 10).buffer);
assert.ok(Math.abs(ghost[99] - 100) < 1e-3 && Math.abs(ghost[7] - 110) < 1e-3 && Math.abs(ghost[11] - 120) < 1e-3 && ghost[15] === 0, 'ghost spheres moved to the predicted position only');
assert.ok(remote.live(3, 10) && !remote.live(3, 200), 'stale after 45 ticks without packets');
const rank = remote.rankInput(3, 10); assert.ok(Math.abs(rank[3] - (1000 - 60 - 40)) < 1e-3, `remaining extrapolated ${rank[3]}`);
// A teleport (placement serial) is not interpolated across.
remote.receive(packet(9, 5000, 1), 150);
[o] = remote.frame([3], 13.5, 1100); // pose tick 7.5 -> between 6 (serial 0) and 9 (serial 1): holds tick 6
assert.ok(Math.abs(o.core.palette[12] - (buildPalette(layout, pose(60), [1, 1, 1])[12] + remote.racers.get(3).offset[0])) < 1e-3);
assert.ok(Math.abs(remote.racers.get(3).offset[0] - (5000 + 600 * 4.5 / 60 - 60)) < 1e-2, 'teleport snaps the correction');
// Attack events and malformed frames.
const a = decodeFrame(frameOf(1, encodeAttack({ victim: 0, attacker: 1, tick: 77, direction: [1, 0, 0], amount: 1234.5 })));
assert.deepEqual([a.kind, a.slot, a.victim, a.attacker, a.tick, a.direction, a.amount], [ATTACK, 1, 0, 1, 77, [1, 0, 0], 1234.5]);
assert.equal(decodeFrame(new Uint8Array([1, 2, 3])), null);
assert.equal(decodeFrame(frameOf(1, new Uint8Array(60).fill(2))), null);
console.log('remote riders: bit-exact palettes from pose packets, interpolation, dead reckoning, ghosts, teleports, attacks OK');
