// pv coreNetWorld (docs/core-migration.md "M7"): two identical Snow Jam runs (the same pad, so they contest pickups) exchange WORLD packets over a 6-tick link,
// once with web/net/mp-game.js's JS replication (its beginTick / endTick block, copied here verbatim in logic: sentTriggers, the
// pickup arbiter, encodeWorld / decodeFrame) and once with the core's (net_world_begin / apply / packet). Per tick: both cores' boost
// counters, revocations and the sent bytes must be identical. CORE_JS=path/to/core.js overrides the runtime; skipped on a core without net_world_apply.
import assert from 'node:assert/strict';
import { createTestRider } from './net/test-core.mjs';
import { createPickupArbiter } from './net/pickup-arbiter.js';
import { encodeWorld, decodeFrame } from './net/rider-packet.js';

const LATENCY = 6, TICKS = +(process.env.TICKS || 5200);
{
  const probe = await createTestRider({});
  if (!probe.core._net_world_apply) {
    console.log('net world: the core has no net_world_apply (an older build); skipped');
    process.exit(0);
  }
}
async function run(coreWorld) {
  const clients = [];
  for (const slot of [0, 1]) {
    const r = await createTestRider({});
    r.startEvent();
    const core = r.core;
    core._world_events();
    const c = { slot, r, core, inbox: [], sent: [], trace: [], sentTriggers: new Set(), pickups: createPickupArbiter({ slot }), revoked: 0, applied: 0 };
    if (coreWorld) core._net_world_begin(slot);
    else {
      const p = core._world_triggers(), n = new Uint32Array(core.HEAPU8.buffer, p, 1)[0];
      for (const k of new Uint32Array(core.HEAPU8.buffer, p + 4, n)) c.sentTriggers.add(k);
    }
    clients.push(c);
  }
  const pad = new Float32Array(24);
  pad[22] = 1;
  for (let t = 0; t < TICKS; t++) {
    for (const c of clients) {
      const core = c.core;
      const due = c.inbox.filter((m) => m.at <= t);
      c.inbox = c.inbox.filter((m) => m.at > t);
      for (const m of due) {
        if (coreWorld) {
          const p = core._malloc(m.bytes.length);
          core.HEAPU8.set(m.bytes, p);
          core._net_world_apply(p, m.bytes.length, m.from);
          core._free(p);
          continue;
        }
        const frame = new Uint8Array(m.bytes.length + 1);
        frame[0] = m.from;
        frame.set(m.bytes, 1);
        const w = decodeFrame(frame);
        for (const k of w.triggers) if (!c.sentTriggers.has(k)) { c.sentTriggers.add(k); core._world_trigger_apply?.(k); }
        for (let at = 0; at < w.events.length; ) {
          const len = 2 + w.events[at + 1];
          if (at + len > w.events.length) break;
          if (w.events[at] === 2 && c.pickups.remote(w.events[at + 2], w.tick, w.slot) && core._pickup_revoke) { core._pickup_revoke(w.events[at + 2]); c.revoked++; }
          const q = core._malloc(len * 4);
          new Uint32Array(core.HEAPU8.buffer, q, len).set(w.events.subarray(at, at + len));
          try { core._world_event_apply(q); c.applied++; } finally { core._free(q); }
          at += len;
        }
      }
      c.r.tick(pad);
      let bytes = null;
      if (coreWorld) {
        const p = core._net_world_packet(t) >> 2, n = new Uint32Array(core.HEAPU8.buffer)[p];
        if (n) bytes = new Uint8Array(core.HEAPU8.buffer, (p + 1) * 4, n).slice();
      } else {
        const triggers = [];
        const p = core._world_triggers(), n = new Uint32Array(core.HEAPU8.buffer, p, 1)[0];
        for (const k of new Uint32Array(core.HEAPU8.buffer, p + 4, n)) if (!c.sentTriggers.has(k)) { c.sentTriggers.add(k); triggers.push(k); }
        const ep = core._world_events(), en = new Uint32Array(core.HEAPU8.buffer, ep, 1)[0];
        const ev = new Uint32Array(core.HEAPU8.buffer, ep + 4, en).slice();
        for (let at = 0; at < en; at += 2 + ev[at + 1]) if (ev[at] === 2) c.pickups.took(ev[at + 2], t);
        c.pickups.prune(t);
        if (triggers.length || en) bytes = encodeWorld({ tick: t, triggers, events: ev });
      }
      if (bytes) {
        c.sent.push([t, Array.from(bytes).join(',')]);
        clients[1 - c.slot].inbox.push({ at: t + LATENCY, bytes, from: c.slot });
      }
      const b = new Float32Array(core.HEAPF32.buffer, core._boost_info(), 8);
      c.trace.push(`${b[2]},${b[4]},${new Float32Array(core.HEAPF32.buffer, core._rider_world_state(), 3).join(',')}`);
    }
  }
  return clients.map((c) => ({ sent: c.sent, trace: c.trace, revoked: coreWorld ? new Uint32Array(c.core.HEAPU8.buffer)[(c.core._net_world_stats() >> 2) + 2] : c.revoked,
    applied: coreWorld ? new Uint32Array(c.core.HEAPU8.buffer)[(c.core._net_world_stats() >> 2) + 4] : c.applied }));
}
const js = await run(false), core = await run(true);
for (const s of [0, 1]) {
  assert.deepEqual(core[s].sent, js[s].sent, `slot ${s}: the same WORLD packets`);
  const first = core[s].trace.findIndex((x, i) => x !== js[s].trace[i]);
  assert.equal(first, -1, `slot ${s}: boost counters and position identical every tick (first difference ${first})`);
  assert.equal(core[s].revoked, js[s].revoked);
  assert.equal(core[s].applied, js[s].applied);
}
console.log(`net world: ${TICKS} ticks, packets ${js.map((c) => c.sent.length)}, revoked ${js.map((c) => c.revoked)}, applied ${js.map((c) => c.applied)}: JS and core identical`);
