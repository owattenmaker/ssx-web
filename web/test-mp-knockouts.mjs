// pv mpKnockouts (docs/core-migration.md "M7"): an online crash attack counts the attacker's knockout. PS2 (dis.pkl): 107E70's crash
// path after an attack (0x108304) calls 10E468 at 0x10830C, whose 119400 adds 1 to the ATTACKER's score object +0x128 and posts popup
// 0x2C (and 10E098(attacker, 1.0, 2) awards the meter); no multiplayer test (0x535C11) on the way, so the PS2's 2-player mode counts it
// as offline (web/ai-racers.js pair_knockout). Online the victim's client knows the crash: it sends a notice (web/net/pair-net.js
// KNOCKOUT_NOTICE in an attack packet), the attacker's client credits it at its next pair pass. Two clients in one process (as
// web/test-mp-pairs.mjs): B rides 90 cm beside A and charges its punches (150 of every 200 ticks); A's attacked crashes (+0x12C) and B's
// knockouts (+0x128) are counted with the switch on and off. CORE_JS=path/to/core.js overrides the runtime.
import assert from 'node:assert/strict';
import { createTestRider, json } from './net/test-core.mjs';
import { createRemoteRiders } from './net/remote-riders.js';
import { createPairNet, KNOCKOUT_NOTICE } from './net/pair-net.js';
import { setPv } from './pv-flags.js';
import { captureState, encodeState, encodeAttack, decodeFrame, ATTACK } from './net/rider-packet.js';

const LATENCY = 6;
const rig = json('RIDER_ZOE/rider.json');
const word = (core, offset) => new Uint32Array(core.HEAPU8.buffer, core._score_object_dump(), 0x1d0 / 4)[offset / 4];

async function run(on) {
  setPv('mpKnockouts', on);
  const clients = [];
  let tickNow = 0;
  for (const slot of [0, 1]) {
    const rider = await createTestRider({ rider: 'RIDER_ZOE' });
    const remote = createRemoteRiders();
    remote.add(1 - slot, rig, { name: slot ? 'A' : 'B' });
    const client = { slot, rider, remote, outbox: [] };
    const sendAttack = (e) => client.outbox.push({ at: tickNow + LATENCY, bytes: encodeAttack(e) });
    client.net = createPairNet({ core: rider.core, remote, slot, count: 2, seed: 4242, sendAttack });
    clients.push(client);
  }
  const deliver = (to, bytes, from) => {
    const frame = new Uint8Array(bytes.length + 1);
    frame[0] = from;
    frame.set(bytes, 1);
    const d = decodeFrame(frame);
    if (d?.kind !== ATTACK) {
      to.remote.receive(frame);
      return;
    }
    // web/net/mp-game.js: attacks from their attacker, knockout notices from their victim
    if (d.slot === d.attacker) to.net.receiveAttack(d);
    else if (d.slot === d.victim && d.amount === KNOCKOUT_NOTICE) to.net.receiveKnockout(d);
  };
  for (const c of clients) c.rider.startEvent();
  clients[1].rider.core._pair_translate(12, 8, 0);
  for (tickNow = 0; tickNow < 2000; tickNow++) {
    for (const c of clients) {
      const pad = new Float32Array(24);
      if (c.slot === 1 && tickNow > 600 && tickNow % 200 < 150) pad[13 - ((tickNow / 200) & 1)] = 1;
      if (c.slot === 1 && tickNow > 600) {
        const a = clients[0].rider.core;
        const pa = new Float32Array(a.HEAPF32.buffer, a._rider_world_state(), 16);
        const pb = new Float32Array(c.rider.core.HEAPF32.buffer, c.rider.core._rider_world_state(), 3);
        const speed = Math.hypot(pa[3], pa[4]) || 1;
        c.rider.core._pair_translate(pa[0] - (pa[4] / speed) * 90 - pb[0], pa[1] + (pa[3] / speed) * 90 - pb[1], 0);
      }
      c.rider.tick(pad, { begin: () => c.net.beginTick(), end: () => c.net.endTick() });
      if (tickNow % 3 === 0) {
        const s = captureState(c.rider.core, { tick: c.net.tick - 1 });
        if (s) c.outbox.push({ at: tickNow + LATENCY, bytes: encodeState(s) });
      }
    }
    for (const c of clients) {
      const other = clients[1 - c.slot];
      for (const m of c.outbox.filter((m) => m.at <= tickNow)) deliver(other, m.bytes, c.slot);
      c.outbox = c.outbox.filter((m) => m.at > tickNow);
    }
  }
  const [a, b] = clients;
  const out = {
    attackedCrashes: a.net.reactions.own.filter((r) => r.attack && r.kind === 2).length,
    attackedBails: word(a.rider.core, 0x12c),
    knockouts: word(b.rider.core, 0x128),
    credited: b.net.counts.knockouts,
    victimKnockouts: word(a.rider.core, 0x128),
  };
  for (const c of clients) c.net.dispose();
  return out;
}
const off = await run(false);
const on = await run(true);
console.log('switch off', JSON.stringify(off), 'on', JSON.stringify(on));
assert.ok(off.attackedCrashes > 0 && off.attackedBails === off.attackedCrashes, 'B\'s charged punches crash A (attacked bails +0x12C)');
assert.equal(off.knockouts, 0, 'switch off: no knockout is counted online (the old port)');
assert.ok(on.attackedCrashes > 0, 'switch on: attacked crashes');
assert.equal(on.knockouts, on.attackedCrashes, 'switch on: one knockout (+0x128) on the attacker per attacked crash of the victim');
assert.equal(on.credited, on.knockouts);
assert.equal(on.victimKnockouts, 0, 'the victim counts none');
console.log(`online knockouts: ${on.knockouts} for ${on.attackedCrashes} attacked crash(es) with pv mpKnockouts, 0 for ${off.attackedCrashes} without`);
