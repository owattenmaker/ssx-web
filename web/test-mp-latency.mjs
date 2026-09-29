// Rider contacts under network latency (docs/multiplayer.md "Latency"): a zero-latency reference -- both riders in one
// pair world, the original 0x107888 applying both halves, contact streams keyed like online -- against two online
// clients (web/net/pair-net.js) over 50 / 100 / 200 ms one-way latency, without and with the ghost response
// prediction (lag compensation). Scenario: two riders racing side by side, three forced side contacts (B put 30 cm
// beside A, as if it swerved in). Measured: riders' distance 5/15/30 ticks after each contact (the visible push),
// how far each client's ghost of the other is from the other's true position, and the mean path divergence from
// the reference. The same seed twice gives the same run (deterministic contact streams).
import assert from 'node:assert/strict';
import { createTestRider, json } from './net/test-core.mjs';
import { createRemoteRiders } from './net/remote-riders.js';
import { createPairNet, contactStream } from './net/pair-net.js';
import { rngNext } from './ai-racers.js';
import { captureState, encodeState, encodeAttack, decodeFrame, ATTACK } from './net/rider-packet.js';
const rig = json('RIDER_ZOE/rider.json'), SEED = 12345, TICKS = 700;
const STRONG = !!process.env.STRONG;
const pad = (slot, t) => { const p = new Float32Array(24); if (t > 200) { if (slot === 1 && t % 120 < 70) p[20] = STRONG ? 1 : 0.6; if (slot === 0 && t % 150 < 40) p[21] = STRONG ? 1 : 0.4; if (STRONG && slot === 0 && t % 150 >= 75 && t % 150 < 110) p[20] = 0.3; } return p; };
const setup = async () => { const riders = []; for (const s of [0, 1]) { const r = await createTestRider({}); r.startEvent(); riders.push(r); } riders[1].core._pair_translate(20, 90, 0); return riders; };
const EVENTS = [300, 420, 540];
// Forced side contact: B is put 30 cm beside A (across A's velocity), as if it had swerved in.
function converge(cores, tick) {
  if (!EVENTS.includes(tick)) return;
  const a = new Float32Array(cores[0].HEAPF32.buffer, cores[0]._rider_world_state(), 16), b = pos(cores[1]);
  const sp = Math.hypot(a[3], a[4]) || 1, side = [-a[4] / sp * 30, a[3] / sp * 30];
  cores[1]._pair_translate(a[0] + side[0] - b[0], a[1] + side[1] - b[1], 0);
}
const pos = (c) => Array.from(new Float32Array(c.HEAPF32.buffer, c._rider_world_state(), 3));
const planar = (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1]);
function metrics(trace) {
  // After each forced contact: ticks the riders stay interpenetrating (< 45 cm apart) and their distance 5/15/30 ticks on.
  const ev = EVENTS.map((e) => ({ overlap: trace.slice(e, e + 40).filter((d) => d < 45).length, d5: +trace[e + 5].toFixed(0), d15: +trace[e + 15].toFixed(0), d30: +trace[e + 30].toFixed(0) }));
  return { overlapTicks: ev.map((x) => x.overlap).join('/'), d5: ev.map((x) => x.d5).join('/'), d15: ev.map((x) => x.d15).join('/'), d30: ev.map((x) => x.d30).join('/') };
}
// ---- reference: one world, both halves ----
async function reference() {
  const riders = await setup(), cores = riders.map((r) => r.core), human = cores[0];
  const init = [2, 0, 1]; for (let a = 0; a < 6; a++) { for (let b = 0; b < 6; b++) init.push(a < 2 && b < 2 && a !== b ? 1 : 0, 1, 1e10, 0, 0, 0, 0, 0, 0); init.push(a); } for (let i = 0; i < 36; i++) init.push(0);
  let p = human._malloc(init.length * 4); human.HEAPF32.set(init, p >> 2); human._race_world_reset(p); human._free(p);
  const pairs = []; for (let s = 0; s < 6; s++) pairs.push(65, 0.0909090936, 0.0909090936); pairs.push(0); p = human._malloc(pairs.length * 4); human.HEAPF32.set(pairs, p >> 2); human._race_world_pair_setup(p); human._free(p);
  let tick = 0, viewed = -1, streams = new Map(); const counts = { separations: 0, translation: [0, 0] };
  const stream = (t) => { const k = `${tick}:${t}`; let w = streams.get(k); if (!w) streams.set(k, (w = contactStream(SEED, tick, 0, 1, t))); return w; };
  const rng = (c) => new Uint32Array(c.HEAPU8.buffer, c._animation_rng_words(), 6);
  human.pairHost = {
    view(s, out) { viewed = s; new Uint32Array(human.HEAPU8.buffer, out, 140).set(new Uint32Array(cores[s].HEAPU8.buffer, cores[s]._pair_view(), 140)); },
    translate(s, x, y, z) { cores[s]._pair_translate(x, y, z); counts.translation[s] += Math.hypot(x, y); },
    velocity(s, x, y, z, reseed) { cores[s]._pair_set_velocity(x, y, z, reseed); },
    react(s, kind, animation, attack, e) { const c = cores[s], q = c._malloc(40); c.HEAPF32.set(new Float32Array(human.HEAPF32.buffer, e, 10), q >> 2); const w = rng(c), saved = w.slice(), st = stream(s); w.set(st); c._pair_react(kind, animation, attack, q); st.set(rng(c)); rng(c).set(saved); c._free(q); },
    random() { return rngNext(stream(viewed)); },
  };
  for (const [i, c] of cores.entries()) { c._rider_host(2); c.riderHost = { pairs: () => { streams = new Map(); const o = new Float32Array(human.HEAPF32.buffer, human._race_world_pairs(tick, i), 5); counts.separations += o[1]; } }; }
  const trace = [], tr = [];
  for (tick = 0; tick < TICKS; tick++) {
    // Both riders step; B's 121750 happens after A's in the one world (slot order), approximated by stepping in order.
    const buf = human._malloc(36 * 4), b = new Float32Array(human.HEAPF32.buffer, buf, 36); b.fill(0); cores.forEach((c, s) => { const q = pos(c); b.set([q[0], q[1], q[2], 1, 0, 1], s * 6); }); human._race_world_frame(tick, buf); human._free(buf);
    converge(cores, tick); riders[0].tick(pad(0, tick)); riders[1].tick(pad(1, tick));
    trace.push(planar(pos(cores[0]), pos(cores[1]))); tr.push([pos(cores[0]), pos(cores[1])]);
  }
  return { trace, tr, counts: { separations: counts.separations, translationA: +counts.translation[0].toFixed(0), translationB: +counts.translation[1].toFixed(0) } };
}
// ---- online: two clients, latency L each way ----
async function online(L, predictResponse) {
  const riders = await setup(), clients = [];
  for (const s of [0, 1]) { const remote = createRemoteRiders(); remote.add(1 - s, rig, {}); const c = { slot: s, rider: riders[s], remote, outbox: [], translation: 0 };
    c.net = createPairNet({ core: riders[s].core, remote, slot: s, count: 2, seed: SEED, predictResponse, sendAttack: (e) => c.outbox.push({ at: tick + L, bytes: encodeAttack(e) }) });
    const t0 = riders[s].core._pair_translate; riders[s].core.pairHost.translate = ((orig) => (sl, x, y, z) => { if (sl === s) c.translation += Math.hypot(x, y); orig(sl, x, y, z); })(riders[s].core.pairHost.translate);
    clients.push(c); }
  let tick = 0; const trace = [], tr = [], ghostErr = [], ghostTick = [], o_contacts = [], lastSep = [0, 0];
  for (tick = 0; tick < TICKS; tick++) {
    for (const c of clients) { if (c.net.counts.separations !== lastSep[c.slot]) { lastSep[c.slot] = c.net.counts.separations; o_contacts.push(tick); } }
    converge(riders.map((r) => r.core), tick);
    for (const c of clients) {
      c.rider.tick(pad(c.slot, tick), { begin: () => c.net.beginTick(), end: () => c.net.endTick() });
      if ((c.net.tick) % 3 === 1) { const st = captureState(c.rider.core, { tick: c.net.tick - 1 }); if (st) c.outbox.push({ at: tick + L, bytes: encodeState(st) }); }
    }
    for (const c of clients) { const o = clients[1 - c.slot]; for (const m of c.outbox.filter((m) => m.at <= tick)) { const f = new Uint8Array(m.bytes.length + 1); f[0] = c.slot; f.set(m.bytes, 1); const d = decodeFrame(f); if (d?.kind === ATTACK) o.net.receiveAttack(d); else o.remote.receive(f); } c.outbox = c.outbox.filter((m) => m.at > tick); }
    trace.push(planar(pos(riders[0].core), pos(riders[1].core))); tr.push([pos(riders[0].core), pos(riders[1].core)]);
    // What each client shows of the other now vs where the other really is (the ghost / drawn position).
    for (const c of clients) { const g = c.remote.ghost(1 - c.slot, c.net.tick - 1); if (!g || EVENTS.some((e) => tick >= e && tick < e + 20)) continue; const f = new Float32Array(g.buffer); const truth = pos(riders[1 - c.slot].core); ghostErr.push(Math.hypot(f[99] - truth[0], f[100] - truth[1], f[101] - truth[2])); ghostTick.push(tick); }
  }
  const sepTicks = []; // ticks near contacts on either client
  const near = (t) => o_contacts.some((x) => Math.abs(x - t) < 30);
  const contactErr = ghostErr.filter((_, i) => near(ghostTick[i]));
  const stat = (a) => a.length ? { mean: +(a.reduce((x, y) => x + y, 0) / a.length).toFixed(1), max: +Math.max(...a).toFixed(1) } : null;
  return { trace, tr, ghost: stat(ghostErr), ghostNearContacts: stat(contactErr), counts: { separationsA: clients[0].net.counts.separations, separationsB: clients[1].net.counts.separations, translationA: +clients[0].translation.toFixed(0), translationB: +clients[1].translation.toFixed(0), corrections: clients.map((c) => c.remote.stats.corrections ?? 0) } };
}
const ref = await reference();
console.log('reference', JSON.stringify({ ...metrics(ref.trace), ...ref.counts }));
const rows = {};
for (const L of [3, 6, 12]) for (const [name, pr] of [['plain', false], ['predicted', true]]) {
  const o = await online(L, pr);
  const err = o.tr.map((p, i) => (planar(p[0], ref.tr[i][0]) + planar(p[1], ref.tr[i][1])) / 2);
  const meanErr = err.reduce((a, b) => a + b, 0) / err.length;
  rows[`${L}:${name}`] = { meanErr, o };
  console.log(`${Math.round(L * 50 / 3)} ms ${name.padEnd(10)}`, JSON.stringify({ ...metrics(o.trace), ghost: o.ghost, ...o.counts, meanPathErrorCm: +meanErr.toFixed(1) }));
}
// Determinism: the same run again gives the same paths (contact streams keyed by seed/tick/pair, not machine RNG).
const again = await online(6, true);
assert.deepEqual(again.tr, rows['6:predicted'].o.tr, 'the same seed and inputs give the same run');
assert.deepEqual([...contactStream(7, 100, 0, 1, 1)], [...contactStream(7, 100, 1, 0, 1)], 'a pair has one stream whichever client computes it');
for (const L of [6, 12]) assert.ok(rows[`${L}:predicted`].meanErr < rows[`${L}:plain`].meanErr, `lag compensation brings ${L * 50 / 3} ms closer to the reference`);
assert.ok(rows['12:predicted'].meanErr < 60, '200 ms stays within 60 cm of the zero-latency paths');
console.log('online contacts under latency: reference, 50/100/200 ms with and without lag compensation, deterministic streams OK');
