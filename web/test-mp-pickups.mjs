// Boost pickups across the network (web/net/pickup-arbiter.js, web/pickup_gameplay.inc pickup_revoke): two clients
// ride the identical run from the same gate (the same pad, so they reach every pickup on the same tick) and exchange
// their shared world events over a 100 ms link. In the original the lower slot takes a pickup touched on the same
// tick; online both take it, then the arbitration gives slot 1's takes back: slot 0 keeps every award, slot 1's boost
// counters drop by exactly the award (the Snow Jam tuck line crosses one of the course's five pickups).
import assert from 'node:assert/strict';
import { createTestRider } from './net/test-core.mjs';
import { createPickupArbiter } from './net/pickup-arbiter.js';

// The rule itself.
const a = createPickupArbiter({ slot: 1 }), b = createPickupArbiter({ slot: 0 });
a.took(7, 100); b.took(7, 100);
assert.equal(a.remote(7, 100, 0), true, 'same tick: the lower slot keeps it');
assert.equal(b.remote(7, 100, 1), false);
a.took(8, 200); assert.equal(a.remote(8, 203, 0), false, 'an earlier own take keeps it');
a.took(9, 300); assert.equal(a.remote(9, 290, 0), true, 'an earlier remote take wins');
a.took(10, 400); assert.equal(a.remote(10, 470, 0), false, 'a take after the debounce is another pickup');

const LATENCY = 6, clients = [];
for (const slot of [0, 1]) { const r = await createTestRider({}); r.startEvent(); clients.push({ slot, r, arbiter: createPickupArbiter({ slot }), outbox: [], revoked: [], taken: 0 }); }
const f32 = (c, p, n) => new Float32Array(c.HEAPF32.buffer, p, n);
const pad = new Float32Array(24); pad[22] = 1; // tuck: the same line for both
for (let t = 0; t < 5200 && clients[1].revoked.length < 1; t++) {
  for (const c of clients) {
    const core = c.r.core;
    // Remote takes that arrived: replayed in the world (shared_world.inc) after the arbitration.
    for (const m of c.outbox.splice(0)) c.inbox = [...(c.inbox ?? []), m];
    const due = (c.inbox ?? []).filter((m) => m.at <= t); c.inbox = (c.inbox ?? []).filter((m) => m.at > t);
    for (const m of due) {
      const counters = () => { const b = f32(core, core._boost_info(), 8); return b[2] + b[4]; }, before = counters(); // window (effect 1) + modifier (effect 2)
      if (c.arbiter.remote(m.resource, m.tick, m.slot)) { assert.equal(core._pickup_revoke(m.resource), 1); c.revoked.push({ resource: m.resource, before, after: counters() }); }
      const q = core._malloc(12); new Uint32Array(core.HEAPU8.buffer, q, 3).set([2, 1, m.resource]); core._world_event_apply(q); core._free(q);
    }
    c.r.tick(pad);
    const p = core._world_events(), n = f32(core, p, 1) && new Uint32Array(core.HEAPU8.buffer, p, 1)[0], ev = new Uint32Array(core.HEAPU8.buffer, p + 4, n).slice();
    for (let at = 0; at < n; at += 2 + ev[at + 1]) if (ev[at] === 2) {
      c.arbiter.took(ev[at + 2], t); c.taken++;
      clients[1 - c.slot].outbox.push({ at: t + LATENCY, tick: t, slot: c.slot, resource: ev[at + 2] });
    }
  }
}
const [c0, c1] = clients;
console.log(`pickups: taken ${c0.taken}/${c1.taken}, conflicts ${c0.arbiter.stats.conflicts}/${c1.arbiter.stats.conflicts}, slot 1 gave back`, JSON.stringify(c1.revoked.map((r) => [r.resource, +r.before.toFixed(3), +r.after.toFixed(3)])));
assert.ok(c1.revoked.length >= 1 && c0.taken >= 1 && c1.taken >= 1, 'the identical runs contested a pickup');
assert.equal(c0.arbiter.stats.revoked, 0, 'slot 0 keeps every contested pickup');
assert.equal(c0.arbiter.stats.kept, c1.arbiter.stats.revoked, 'both clients decided every conflict the same way');
for (const r of c1.revoked) assert.ok(Math.abs(r.before - r.after - Math.min(5, r.before)) < 1e-4, 'the award is taken back exactly (clamped at zero)');
console.log('online boost pickups: one winner per contested pickup, award given back OK');
