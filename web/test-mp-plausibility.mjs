// Server-side finish plausibility (web/server/plausibility.mjs, web/server/mp-server.mjs):
//   * real full runs of the core to the finish (Snow Jam tuck and boost, Snow Jam with tricks/crashes from grid slot 3,
//     BRA2, the Happiness backcountry run: a rolling start, 4 minutes), streamed as the client streams them (20 Hz state
//     packets), arriving with 100 ms latency plus jitter, a 3 s stall (a throttled tab catching up) and 10 % packet
//     loss: no finding;
//   * the race clock's start per event (3-2-1 on races and freestyle, none on backcountry: every courses.json course,
//     the core for one course of each event); the live server's false rejections of 2026-09-28 (backcountry finish
//     packets 1-2 ticks after the claimed clock; a client running 46..92+ ticks ahead after a hitch): accepted;
//   * frame hitches (1-6 s, slow catch-up frames) paced by web/net/race-pace.js on the real frame clock: older clients'
//     overshoot ahead of the server clock is accepted, current clients stay on the server clock;
//   * forgeries of those runs: an earlier claimed time, a sped-up race clock, a course skip, a skip disguised as a
//     reset, no finish in the stream, a straight line to the finish, a race clock held ahead of the server's (also
//     with stale packets replayed in between), a lead beyond the burst cap: each is found;
//   * Metro-City stage teleports (phone booths 0004 -> 0005 / 0006, 0007 -> 0008, the water tower -> 0002 / 0003) made by the
//     core with the stage world: accepted with the placement counter bumped (as the core does) or not; the same jumps
//     elsewhere are found;
//   * the server: by default ('flag') a forged claim is logged and kept unverified; MP_PLAUSIBILITY=reject makes it a DNF 'invalid'.
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { createTestRider, json } from './net/test-core.mjs';
import { captureState, encodeState } from './net/rider-packet.js';
import fs from 'node:fs';
import { createRunCheck, COUNTDOWN_TICKS, AHEAD_TICKS, countdownTicks } from './server/plausibility.mjs';
import { FixedStepClock } from './fixed-step-clock.js';
import { paceSeconds } from './net/race-pace.js';
import { buildTeleportBeams, TELEPORT_BEAMS } from './server/teleport-beams.mjs';
import { loadStageWorld } from './stage-world-compare.mjs';
import { createMpClient } from './net/mp-client.js';

async function realRun(course, mode, slot = 0) {
  const doc = json(course + '/npc-riders.json');
  const r = await createTestRider({ course, gridSeed: slot ? JSON.stringify(doc.riders[slot - 1].ground.state) : '' });
  r.startEvent();
  const pad = (t) => { if (mode === 'tricks') { const p = new Float32Array(24), ph = t % 400; p[22] = ph < 200 ? 1 : 0; if (ph >= 200 && ph < 260) p[10] = 1; if (ph >= 262 && ph < 330) { p[5] = 1; p[12] = 1; } return p; } const p = new Float32Array(24); p[22] = 1; if (mode === 'boost') p[9] = t % 240 < 120 ? 1 : 0; return p; };
  const packets = []; let finish = -1;
  for (let t = 0; t < 60 * 60 * 8 && finish < 0; t++) {
    r.tick(pad(t));
    const p = new Float32Array(r.core.HEAPF32.buffer, r.core._race_progress_info(), 8), done = p[3] >= 0;
    if (done) finish = new Float32Array(r.core.HEAPF32.buffer, r.core._race_result_info(), 6)[1];
    // The client sends every third tick (mp-game.js endTick); the finish is sent in the next packet at the latest.
    if (t % 3 === 0 || done) { const s = captureState(r.core, { tick: t, finished: done }); if (s) packets.push({ tick: t, bytes: encodeState({ ...s, lighting: null, fx: null }) }); }
  }
  assert.ok(finish > 0, `${course} ${mode}: the run reaches the finish`);
  if (process.env.TRACE_RUNS) console.log('run', course, mode, finish);
  return { course, mode, finish, packets };
}
// Stream packets into a run check the way the server sees them: wall ticks = send tick + latency (+ jitter, stalls);
// clock[tick] = the server-clock tick the client ran that tick at (clientClock), else the tick itself.
function judge(run, { claimed = run.finish, packets = run.packets, latency = 6, stallAt = -1, loss = 0.1, seed = 1, clock = null } = {}) {
  const check = createRunCheck({ course: run.course }); let rnd = seed;
  const random = () => ((rnd = (Math.imul(rnd, 1103515245) + 12345) >>> 0) / 2 ** 32);
  let backlog = 0;
  for (const p of packets) {
    if (p.tick === stallAt) backlog = 180; // 3 s stall: packets pile up, then arrive at once (the tab caught up)
    const arrival = (clock ? clock[p.tick] : p.tick) + latency + random() * 4 + backlog; backlog = Math.max(0, backlog - 3);
    if (random() < loss && !(new DataView(p.bytes.buffer).getUint8(1) & 1)) continue;
    check.feed(p.bytes, arrival);
  }
  return check.verdict(claimed);
}
const codes = (v) => v.findings.map((f) => f.code);
// A packet with some header fields changed.
function edit(bytes, { tick, finishTicks, position, placements }) {
  const b = bytes.slice(), v = new DataView(b.buffer);
  if (tick != null) v.setUint32(4, tick, true); if (finishTicks != null) v.setFloat32(12, finishTicks, true);
  if (position) position.forEach((x, k) => v.setFloat32(32 + 4 * k, x, true)); if (placements != null) v.setUint16(28, placements, true);
  return b;
}

const runs = [await realRun('ARA1', 'tuck'), await realRun('ARA1', 'boost'), await realRun('ARA1', 'tricks', 3), await realRun('BRA2', 'tuck'), await realRun('ABC1', 'tuck')];
for (const run of runs) {
  for (const opts of [{}, { latency: 1, loss: 0 }, { latency: 15, stallAt: run.packets[Math.floor(run.packets.length / 2)].tick }]) {
    const v = judge(run, opts);
    assert.ok(v.ok, `${run.course} ${run.mode} ${JSON.stringify(opts)}: a real run is plausible, found ${JSON.stringify(v.findings)}`);
  }
  console.log(`${run.course} ${run.mode}: finish ${run.finish} ticks, ${run.packets.length} packets, path/route ${(judge(run).stats.path / judge(run).stats.route).toFixed(2)}: plausible`);
}
const run = runs[0], P = run.packets, last = (a) => a[a.length - 1];
// 1. An earlier claimed time (the stream shows the real one).
assert.deepEqual(codes(judge(run, { claimed: run.finish - 600 })).includes('time-mismatch'), true);
// 2. The same with a forged stream: finish ticks rewritten 10 s earlier in every finished packet.
const earlier = P.map((p) => ({ ...p, bytes: new DataView(p.bytes.buffer).getUint8(1) & 1 ? edit(p.bytes, { finishTicks: run.finish - 600 }) : p.bytes }));
assert.ok(codes(judge(run, { claimed: run.finish - 600, packets: earlier })).some((c) => c === 'clock-behind' || c === 'finish-early'), 'an earlier finish than the server clock allows');
// 3. A sped-up race clock: every tick label x1.15 (the stream runs ahead of the server clock).
const fast = P.map((p) => ({ tick: p.tick, bytes: edit(p.bytes, { tick: Math.round(p.tick * 1.15) }) }));
assert.ok(codes(judge(run, { packets: fast })).includes('clock-ahead'), 'a race clock running ahead');
// 4. A course skip: 40 s of the stream cut out, the ticks closed up.
const cut = Math.floor(P.length / 3), skip = 800;
const skipped = [...P.slice(0, cut), ...P.slice(cut + skip).map((p) => ({ tick: p.tick - skip * 3, bytes: edit(p.bytes, { tick: p.tick - skip * 3, finishTicks: new DataView(p.bytes.buffer).getUint8(1) & 1 ? run.finish - skip * 3 : undefined }) }))];
const vs = judge(run, { claimed: run.finish - skip * 3, packets: skipped });
assert.ok(codes(vs).includes('speed'), `a course skip: ${JSON.stringify(vs.findings)}`);
// 5. The skip disguised as a reset placement (counters bumped at the jump).
const disguised = skipped.map((p, i) => (i >= cut ? { ...p, bytes: edit(p.bytes, { placements: new DataView(p.bytes.buffer).getUint16(28, true) + 1 }) } : p));
assert.ok(codes(judge(run, { claimed: run.finish - skip * 3, packets: disguised })).includes('reset'), 'a teleport disguised as a reset');
// 6. No finish in the stream.
const unfinished = P.filter((p) => !(new DataView(p.bytes.buffer).getUint8(1) & 1));
assert.ok(codes(judge(run, { packets: unfinished })).includes('no-trace'));
// 7. A straight line from the start to the finish at 80 m/s with a matching clock.
const a = P[0], z = last(P), pa = [0, 1, 2].map((k) => new DataView(a.bytes.buffer).getFloat32(32 + 4 * k, true)), pz = [0, 1, 2].map((k) => new DataView(z.bytes.buffer).getFloat32(32 + 4 * k, true));
const dist = Math.hypot(pz[0] - pa[0], pz[1] - pa[1], pz[2] - pa[2]), ticks = Math.ceil(dist / 8000 * 60), line = [];
for (let t = 0; t <= ticks + COUNTDOWN_TICKS + 3; t += 3) { const f = Math.max(0, t - COUNTDOWN_TICKS) / ticks, fin = t >= ticks + COUNTDOWN_TICKS; line.push({ tick: t, bytes: (() => { const b = edit(a.bytes, { tick: t, position: pa.map((x, k) => x + (pz[k] - x) * Math.min(1, f)), finishTicks: fin ? ticks : -1 }); if (fin) b[1] |= 1; return b; })() }); }
const vl = judge(run, { claimed: ticks, packets: line, loss: 0 });
assert.ok(codes(vl).includes('too-fast') && codes(vl).includes('mean-speed'), `a straight line: ${JSON.stringify(vl.findings)}`);
// 8. A short straight line (half the course) at 40 m/s.
const half = line.map((p) => ({ ...p, bytes: edit(p.bytes, { position: [0, 1, 2].map((k) => pa[k] + (new DataView(p.bytes.buffer).getFloat32(32 + 4 * k, true) - pa[k]) * 0.5) }) }));
assert.ok(codes(judge(run, { claimed: ticks, packets: half, loss: 0 })).includes('path'), 'a run that covers half the course');
console.log('forgeries found: earlier claim, earlier stream, fast clock, course skip, skip as reset, no finish, straight line, half course');

// ---- the race clock's start per event (plausibility.mjs countdownTicks) ----
// Races and freestyle events count down 3-2-1 before the race clock; backcountry events start rolling (the clock reads 1
// on tick 0, GO's): every course of courses.json by its event, and the core for one course of each event.
for (const c of json('courses.json').courses) assert.equal(countdownTicks(c.code), c.event === 'backcountry' ? 0 : COUNTDOWN_TICKS, `${c.code} (${c.event}): its countdown`);
for (const code of ['ARA1', 'BHP1', 'ASS1', 'ABA1', 'ABC1', 'DBC2', 'EBC3']) {
  const r = await createTestRider({ course: code }), pad = new Float32Array(24), C = countdownTicks(code); r.startEvent();
  let phase = -1; for (let t = 0; t < 200; t++) { const { race } = r.tick(pad); if (t === 0) phase = race[5]; }
  const clock = new Float32Array(r.core.HEAPF32.buffer, r.core._race_result_info(), 6)[4];
  assert.equal(phase === 4, C > 0, `${code}: the countdown phase on tick 0 (${phase})`);
  assert.ok(clock - (199 - C) >= 0 && clock - (199 - C) <= 1, `${code}: race clock ${clock} on tick 199, countdown ${C}`);
}
// The live server's rejections (2026-09-28): backcountry finishes whose finish packet came 1-2 ticks after the claimed
// clock ('finish-early' against a countdown the event does not have).
const bc = runs[4], finishedPacket = (p) => new DataView(p.bytes.buffer).getUint8(1) & 1;
assert.ok(bc.finish > 12000, `a long backcountry run (${bc.finish} ticks)`);
for (const extra of [1, 2]) {
  const packets = bc.packets.map((p) => (finishedPacket(p) ? { tick: bc.finish + extra, bytes: edit(p.bytes, { tick: bc.finish + extra }) } : p));
  const v = judge(bc, { packets });
  assert.ok(v.ok, `backcountry finish packet ${extra} tick(s) after the claim: ${JSON.stringify(v.findings)}`);
  assert.ok(codes(judge({ ...bc, course: 'ARA1' }, { packets })).includes('finish-early'), 'the same stream on a course with a countdown');
}
// A rolling start does not widen the claim window: an earlier claim on the backcountry stream is found.
const bcEarlier = bc.packets.map((p) => ({ ...p, bytes: finishedPacket(p) ? edit(p.bytes, { finishTicks: bc.finish - 600 }) : p.bytes }));
assert.ok(codes(judge(bc, { claimed: bc.finish - 600, packets: bcEarlier })).includes('clock-behind'), 'an earlier backcountry finish than the server clock allows');
console.log(`race clock start: 3-2-1 on races / freestyle, rolling on backcountry (${bc.course} ${bc.finish} ticks: plausible, finish packet +1/+2 accepted)`);

// ---- frame hitches: the client's pacing (web/net/race-pace.js) on the real frame clock (web/fixed-step-clock.js) ----
// A client racing `ticks` ticks at 60 Hz frames, a hitch of hitchMs before tick `at`, then frames costing perTickMs per
// tick run (4 ms + ticks); pending: the pace counts the frame clock's debt (current clients) or not (older clients).
// Returns the server-clock tick (ticks since GO) at which each race tick ran.
function clientClock(ticks, { at, hitchMs, perTickMs, pending }) {
  const clock = new FixedStepClock(), ranAt = new Float64Array(ticks + 1); let tick = 0, ms = 0, frame = 1000 / 60, hitched = false;
  while (tick <= ticks) {
    let dt = frame; if (!hitched && tick >= at) { dt = hitchMs; hitched = true; }
    ms += dt;
    const n = clock.advance(paceSeconds({ expected: ms * 0.06, tick, dt: dt / 1000, pending: pending ? clock.pending : 0 }), () => { if (tick <= ticks) ranAt[tick] = ms * 0.06; tick++; });
    frame = Math.max(1000 / 60, 4 + perTickMs * n);
  }
  return ranAt;
}
const leads = [];
for (const r of [runs[0], bc]) {
  const end = r.packets[r.packets.length - 1].tick;
  for (const [at, hitchMs, perTickMs] of [[Math.floor(end / 2), 2000, 2], [Math.floor(end / 2), 3000, 4], [Math.floor(end / 3), 6000, 8], [end - 400, 3000, 4]]) {
    const older = judge(r, { clock: clientClock(end, { at, hitchMs, perTickMs, pending: false }) });
    assert.ok(older.ok, `${r.course}: an older client's ${hitchMs} ms hitch at tick ${at} (${perTickMs} ms/tick): ${JSON.stringify(older.findings)}`);
    assert.ok(older.stats.lead > AHEAD_TICKS, `${r.course}: the older client ran ahead after the hitch (${older.stats.lead})`);
    const current = judge(r, { clock: clientClock(end, { at, hitchMs, perTickMs, pending: true }) });
    assert.ok(current.ok && current.stats.lead <= 0, `${r.course}: a current client stays on the server clock through a ${hitchMs} ms hitch (lead ${current.stats.lead}): ${JSON.stringify(current.findings)}`);
    leads.push(`${r.course} ${hitchMs / 1000} s @${at}: ${older.stats.lead} / ${current.stats.lead}`);
  }
}
console.log(`hitch leads over the server clock (older / current clients): ${leads.join(', ')}`);
// The live server's clock-ahead rejection (2026-09-28): 46..92 ticks ahead within 0.2 s of wall, then back.
{
  const lead = (t) => (t >= 780 && t < 1400 ? Math.min(92, (t - 780) * 0.8) * Math.min(1, (1400 - t) / 300) : 0);
  const v = judge(runs[0], { clock: Float64Array.from({ length: P[P.length - 1].tick + 1 }, (_, t) => t - lead(t)) });
  assert.ok(v.ok && v.stats.lead > AHEAD_TICKS, `a 92-tick transient lead: ${JSON.stringify(v.findings)} lead ${v.stats.lead}`);
}
// Clock forgeries: a race clock held 2 s ahead of the server's; the same with stale packets replayed every 10 s (an
// old tick does not end a lead); one packet beyond the burst cap.
const held = P.map((p) => (p.tick >= 600 ? { tick: p.tick, bytes: edit(p.bytes, { tick: p.tick + 120 }) } : p));
assert.ok(codes(judge(run, { packets: held, loss: 0 })).includes('clock-ahead'), 'a race clock held ahead');
const stale = held.flatMap((p, i) => (i % 200 === 199 ? [p, { tick: p.tick, bytes: P[i - 150].bytes }] : [p]));
assert.ok(codes(judge(run, { packets: stale, loss: 0 })).includes('clock-ahead'), 'a race clock held ahead, stale packets in between');
const burst = P.map((p, i) => (i === 400 ? { tick: p.tick, bytes: edit(p.bytes, { tick: p.tick + 1300 }) } : p));
assert.ok(codes(judge(run, { packets: burst, loss: 0 })).includes('clock-ahead'), 'a lead beyond the burst cap');
console.log('frame hitches: older clients\' overshoot accepted, current clients on the server clock; held / stale / burst leads found');

// ---- stage teleports (web/server/teleport-beams.mjs) ----
assert.deepEqual(buildTeleportBeams(json), TELEPORT_BEAMS.BRA2, 'the teleport table is the BRA2 stage data');
// A BRA2 rider core with its stage world (the booth programs) and the teleport on: the rider placed at a trigger (a booth's
// door, the tower's beam), the gameplay RNG set so that the program picks the wanted destination; 20 Hz packets from the
// placement to 90 ticks after the teleport.
async function beamSegments() {
  const r = await createTestRider({ course: 'BRA2' }), c = r.core, root = new URL('public/assets/', import.meta.url);
  const text = (u) => { const b = Buffer.from(fs.readFileSync(new URL(u, root), 'utf8') + '\0'), p = c._malloc(b.length); c.HEAPU8.set(b, p); return p; };
  c._init_sections(text('BRA2/SECTIONS/sections.json')); assert.ok(loadStageWorld(c, root, 'BRA2'), 'BRA2 stage world');
  c._stage_teleport_enable(1); r.startEvent();
  const zero = new Float32Array(24), f32 = (p, n) => new Float32Array(c.HEAPF32.buffer, p, n);
  let tick = 0; const step = () => { r.tick(zero); tick++; };
  while (tick < 250) step();
  const at = { 0xC9810: [-206726, 78185, -498700, 0, -1], 0x55B10: [-171158, -48684, -590000, 0.309, -0.951], 0x43510: [-197300, 35708, -514500, -1, 0] };
  const out = [];
  for (const [trigger, destination] of [[0xC9810, 0x99010], [0xC9810, 0x68F10], [0x55B10, 0x24210], [0x43510, 0xA2C10], [0x43510, 0x22410]]) {
    const d = TELEPORT_BEAMS.BRA2.find((b) => b.trigger === trigger).destinations.find((x) => x.resource === destination);
    let found = null;
    for (let seed = 1; seed < 400 && !found; seed++) {
      new Uint32Array(c.HEAPU8.buffer, c._animation_rng_words(), 6).set([seed * 2654435761 >>> 0, seed * 40503 >>> 0, seed ^ 0x5a5a5a5a, seed * 69069 >>> 0, seed * 1103515245 >>> 0, seed]);
      const [x, y, z, dx, dy] = at[trigger]; c._place_rider_region(x, y, z, dx, dy, 0, 0);
      const count = f32(c._stage_teleport_info(), 1)[0], packets = []; let jumped = -1;
      { const s = captureState(c, { tick }); if (s) packets.push({ tick, bytes: encodeState({ ...s, lighting: null, fx: null }) }); } // at the placement
      for (let k = 0; k < 150 && (jumped < 0 ? k < 60 : tick < jumped + 90); k++) {
        step(); if (jumped < 0 && f32(c._stage_teleport_info(), 1)[0] !== count) jumped = tick;
        if (tick % 3 === 0) { const s = captureState(c, { tick }); if (s) packets.push({ tick, bytes: encodeState({ ...s, lighting: null, fx: null }) }); }
      }
      if (jumped < 0) continue;
      const P = f32(c._stage_teleport_info() + 4, 3); // the placement point: the human's (2, 0) offset of this destination
      if (Math.hypot(P[0] - (d.r3[0] + 200 * d.r0[0]), P[1] - (d.r3[1] + 200 * d.r0[1])) < 1) found = { trigger, destination, jumped, packets };
    }
    assert.ok(found, `a teleport from ${trigger.toString(16)} to ${destination.toString(16)}`);
    out.push(found);
  }
  return out;
}
const placementsOf = (p) => new DataView(p.bytes.buffer).getUint16(28, true);
const segments = await beamSegments();
for (const seg of segments) {
  const name = `${seg.trigger.toString(16)} -> ${seg.destination.toString(16)}`, P = seg.packets, j = P.findIndex((p) => p.tick >= seg.jumped);
  const feed = (packets, course = 'BRA2') => { const check = createRunCheck({ course }); for (const p of packets) check.feed(p.bytes, p.tick + 6); return check.run; };
  // The core bumps the reset placements by one at the teleport (0x123210 places through 11D660).
  assert.equal(placementsOf(P[j]) - placementsOf(P[j - 1]), 1, `${name}: the teleport bumps the placement counter`);
  const jump = Math.round(Math.hypot(...[0, 1, 2].map((k) => new DataView(P[j].bytes.buffer).getFloat32(32 + 4 * k, true) - new DataView(P[j - 1].bytes.buffer).getFloat32(32 + 4 * k, true))));
  // Accepted as streamed, and with the counter not bumped; the same stream on a course without beams is not.
  const unbumped = P.map((p, i) => (i >= j ? { ...p, bytes: edit(p.bytes, { placements: placementsOf(p) - 1 }) } : p));
  for (const [label, packets] of [['bumped', P], ['not bumped', unbumped]]) {
    const run = feed(packets);
    assert.deepEqual(run.findings, [], `${name} (${label}): a real teleport is plausible, found ${JSON.stringify(run.findings)}`);
    assert.ok(run.teleports.length === 1 && run.teleports[0].destination === seg.destination, `${name} (${label}): matched ${JSON.stringify(run.teleports)}`);
  }
  assert.ok(feed(unbumped, 'ARA1').findings.some((f) => f.code === 'speed'), `${name}: on another course the jump is found`);
  // The same jump elsewhere: the whole approach moved 60 m (the rider never reached the trigger), or the landing moved 60 m
  // (both away from each other, so the jump only gets longer).
  const shift = (packets, from, to, dx) => packets.map((p, i) => (i >= from && i < to ? { ...p, bytes: edit(p.bytes, { position: [0, 1, 2].map((k) => new DataView(p.bytes.buffer).getFloat32(32 + 4 * k, true) + (k === 0 ? dx : 0)) }) } : p));
  const away = Math.sign(new DataView(P[j].bytes.buffer).getFloat32(32, true) - new DataView(P[j - 1].bytes.buffer).getFloat32(32, true)) * 6000; // lengthens the jump
  for (const [label, packets] of [['approach elsewhere', shift(unbumped, 0, j, -away)], ['landing elsewhere', shift(unbumped, j, P.length, away)]]) {
    const codes = feed(packets).findings.map((f) => f.code);
    assert.ok(codes.includes('speed'), `${name} (${label}, not bumped): found ${JSON.stringify(codes)}`);
  }
  if (jump > 20000) { // longer than a reset may jump: bumped and elsewhere is a 'reset' finding
    const codes = feed(shift(P, 0, j, -away)).findings.map((f) => f.code);
    assert.ok(codes.includes('reset'), `${name} (approach elsewhere, bumped): found ${JSON.stringify(codes)}`);
  }
  console.log(`teleport ${name}: ${jump} cm jump accepted (placement +1 and +0), elsewhere found`);
}
// A full Metro-City run judged on its course: nothing changes for a run without teleports.
assert.ok(judge(runs[3]).ok, 'BRA2 run on its course');

// ---- the server ----
const env0 = { ...process.env }; delete env0.MP_PLAUSIBILITY;
for (const mode of ['reject', 'flag']) {
  const port = 19000 + Math.floor(Math.random() * 900);
  const server = spawn(process.execPath, ['server/mp-server.mjs', '--port', String(port), '--host', '127.0.0.1'], { cwd: new URL('.', import.meta.url).pathname, stdio: ['ignore', 'pipe', 'ignore'], env: mode === 'flag' ? env0 /* the default */ : { ...env0, MP_PLAUSIBILITY: mode } }); // 'reject': the default
  await new Promise((r) => server.stdout.once('data', r));
  const wait = (c, type, pred = () => true, ms = 8000) => new Promise((res, rej) => { const t = setTimeout(() => rej(new Error(`timeout ${type}`)), ms); const off = c.on(type, (v) => { if (pred(v)) { clearTimeout(t); off(); res(v); } }); });
  try {
    const url = `ws://127.0.0.1:${port}/mp`, x = createMpClient({ url, name: 'Cheat', token: 'c1' }), y = createMpClient({ url, name: 'Fair', token: 'f1' });
    await x.connect(); await y.connect(); await wait(x, 'status', (s) => s === 'online', 2000).catch(() => {});
    x.create({ course: 'ARA1' }); const lobby = await wait(x, 'lobby', (l) => l); y.join(lobby.id); await wait(x, 'lobby', (l) => l.members.length === 2);
    x.start(); await Promise.all([wait(x, 'start'), wait(y, 'start')]); x.loaded(); y.loaded();
    const go = await wait(x, 'go'); await new Promise((r) => setTimeout(r, go - x.serverNow() + 50));
    // The cheater streams 1 s of its real run, then claims a 1:00.00 finish.
    for (const p of P.slice(0, 20)) x.sendState(p.bytes);
    x.finish(3600); y.finish(0, true, 'quit');
    const results = await wait(x, 'results');
    const cheat = results.find((r) => r.name === 'Cheat');
    if (mode === 'reject') assert.ok(cheat.dnf && cheat.reason === 'invalid' && cheat.findings.includes('no-trace'), `rejected: ${JSON.stringify(cheat)}`);
    else assert.ok(!cheat.dnf && cheat.ticks === 3600 && cheat.verified === false, `flagged: ${JSON.stringify(cheat)}`);
    x.disconnect(); y.disconnect();
  } finally { server.kill(); }
}
console.log('online finish plausibility: real runs pass, forgeries found, server rejects / flags OK');
