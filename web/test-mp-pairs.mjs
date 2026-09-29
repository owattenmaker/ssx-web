// Rider-vs-rider across the network (web/net/pair-net.js): two clients in one process, each with its own core and
// only its own rider, exchange state packets (web/net/rider-packet.js) and attack events through a simulated
// 100 ms link. Both start on the same grid spot, so the original pair system (0x107888) has to separate them from
// both sides; then client B punches (R1) client A's rider and A's client applies the victim's response.
import assert from 'node:assert/strict';
import { createTestRider, json } from './net/test-core.mjs';
import { createRemoteRiders } from './net/remote-riders.js';
import { createPairNet } from './net/pair-net.js';
import { captureState, encodeState, encodeAttack, decodeFrame, ATTACK } from './net/rider-packet.js';

const LATENCY = 6; // ticks one way
const rig = json('RIDER_ZOE/rider.json');
const clients = [];
for (const slot of [0, 1]) {
  const rider = await createTestRider({ rider: 'RIDER_ZOE' });
  const remote = createRemoteRiders();
  remote.add(1 - slot, rig, { name: slot ? 'A' : 'B' });
  const client = { slot, rider, remote, outbox: [] };
  client.net = createPairNet({ core: rider.core, remote, slot, count: 2, seed: 4242, sendAttack: (e) => client.outbox.push({ at: tickNow + LATENCY, bytes: encodeAttack(e) }) });
  clients.push(client);
}
let tickNow = 0;
const deliver = (to, bytes, from) => { const frame = new Uint8Array(bytes.length + 1); frame[0] = from; frame.set(bytes, 1); const d = decodeFrame(frame); if (d?.kind === ATTACK) to.net.receiveAttack(d); else to.remote.receive(frame); };
for (const c of clients) c.rider.startEvent();
clients[1].rider.core._pair_translate(12, 8, 0); // B overlaps A's body, not exactly on it
const distance = () => { const [a, b] = clients.map((c) => new Float32Array(c.rider.core.HEAPF32.buffer, c.rider.core._rider_world_state(), 3)); return Math.hypot(a[0] - b[0], a[1] - b[1]); };
const start = distance();
let separatedAt = -1, crashA = 0, rankChecks = 0, rankMismatches = 0, rankAll = 0, rankDisagree = 0;
for (tickNow = 0; tickNow < 1100; tickNow++) {
  for (const c of clients) {
    const pad = new Float32Array(24);
    if (c.slot === 1 && tickNow > 600 && tickNow % 60 < 20) pad[13 - (tickNow / 60 & 1)] = 1; // B punches (R1 / L1) every second
    // Attack phase: B rides a fixed 90 cm beside A (moved before its tick), punching left and right.
    if (c.slot === 1 && tickNow > 600) {
      const pa = new Float32Array(clients[0].rider.core.HEAPF32.buffer, clients[0].rider.core._rider_world_state(), 16), pb = new Float32Array(c.rider.core.HEAPF32.buffer, c.rider.core._rider_world_state(), 3);
      const speed = Math.hypot(pa[3], pa[4]) || 1, side = [-pa[4] / speed * 90, pa[3] / speed * 90];
      c.rider.core._pair_translate(pa[0] + side[0] - pb[0], pa[1] + side[1] - pb[1], 0);
    }
    c.rider.tick(pad, { begin: () => c.net.beginTick(), end: () => c.net.endTick() });
    if (tickNow % 3 === 0) { const s = captureState(c.rider.core, { tick: c.net.tick - 1 }); if (s) c.outbox.push({ at: tickNow + LATENCY, bytes: encodeState(s) }); }
  }
  for (const c of clients) { const other = clients[1 - c.slot]; for (const m of c.outbox.filter((m) => m.at <= tickNow)) deliver(other, m.bytes, c.slot); c.outbox = c.outbox.filter((m) => m.at > tickNow); }
  // Ranking: both clients rank from the same instant, so their orders agree (any gap), and when the true remaining
  // distances differ by more than 3 m the leader is 1st.
  if (tickNow % 6 === 1 && tickNow > 220) { rankAll++; if (clients[0].net.rank(0) !== clients[1].net.rank(0)) { rankDisagree++;  } }
  if (tickNow % 6 === 1 && tickNow > 220) {
    const rem = clients.map((c) => new Float32Array(c.rider.core.HEAPF32.buffer, c.rider.core._race_progress_info(), 1)[0]);
    if (Math.abs(rem[0] - rem[1]) > 300) { rankChecks++; const truth = rem[0] < rem[1] ? 0 : 1; for (const c of clients) if (c.net.rank(truth) !== 0) rankMismatches++; }
  }
  if (separatedAt < 0 && clients[0].net.counts.separations && clients[1].net.counts.separations) separatedAt = tickNow;
  if (new Float32Array(clients[0].rider.core.HEAPF32.buffer, clients[0].rider.core._crash_info(), 1)[0]) crashA++;
  if (process.env.TRACE && tickNow % 30 === 0) console.log(tickNow, distance().toFixed(0), clients[0].net.counts.separations, new Float32Array(clients[0].rider.core.HEAPF32.buffer, clients[0].rider.core._rider_state(), 16)[7].toFixed(1));
}
const [a, b] = clients.map((c) => c.net.counts);
console.log(`start distance ${start.toFixed(1)} cm -> ${distance().toFixed(1)} cm; A`, a, 'B', b, `A crashed ${crashA} ticks, ranks`, clients.map((c) => c.net.rank(c.slot)));
assert.ok(start < 60, 'both riders start overlapping');
assert.ok(separatedAt >= 0, 'both clients resolved the contact with the other rider');
assert.ok(a.separations > 0 && b.separations > 0 && a.checks > 0 && b.checks > 0);
assert.ok(a.ghostTranslations > 0 && b.ghostTranslations > 0, 'the ghost half of a contact is left to its own client');
assert.ok(b.sentAttacks > 0, 'B hit A with an attack');
assert.equal(a.receivedAttacks, b.sentAttacks, 'A applied every attack B sent');
assert.ok(a.reactions > 0, 'A reacted to the attacks');
console.log(`ranking checks ${rankChecks}, mismatches ${rankMismatches}; orders compared ${rankAll}, disagreements ${rankDisagree}`);
assert.equal(rankDisagree, 0, 'both clients show the same places');
assert.ok(rankChecks > 5 && rankMismatches === 0, 'both clients rank the leader 1st');
// Attack reactions: the attacker's prediction of the victim's reaction and the victim's own come from the same contact
// stream (race seed, attack tick, pair, victim), so they are the same reaction.
const predicted = clients[1].net.reactions.predicted.filter((r) => r.attack), actual = clients[0].net.reactions.own.filter((r) => r.attack);
const same = predicted.filter((p) => actual.some((a) => a.tick === p.tick && a.kind === p.kind && a.animation === p.animation)).length;
console.log(`attack reactions: B predicted ${JSON.stringify(predicted.map((r) => [r.tick, r.kind, r.animation]))}, A applied ${JSON.stringify(actual.map((r) => [r.tick, r.kind, r.animation]))}`);
assert.ok(actual.length > 0 && same === predicted.length, 'the victim reacts exactly as the attacker predicted');
for (const c of clients) c.net.dispose();
console.log('online rider pairs: contact from both sides, attack events and victim responses, ranking OK');
