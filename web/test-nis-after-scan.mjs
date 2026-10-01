// The NIS teleport after the section scan (pv nisAfterScan; docs/ctm-parity.md "The NIS teleport after the section scan").
// PS2 frame order (docs/visual-rng-order.md 2): 121818 of update U fires builtin 68 (the lodge door / booth), the rider manager ends
// with the section pass 0x101B60 (0x129124), WS_tick enters world state 14, and the cut's rider actor (123640) places the rider at
// update U + 1's NIS tick 0x230BE4, before that update's rider manager. The page's station cut fires inside game-tick.js simulate
// (free-ride.js drainEvents -> main.js 'stationCut' -> nisHoldAt); with nisAfterScan the placement waits for host.nisStart, which
// simulate calls before every pass of the next tick. This checks that contract on a stub core: the firing tick's section pass runs
// before the hold, and the hold before the next tick's passes.
import assert from 'node:assert/strict';
import { createGameTick } from './game-tick.js';

const calls = [];
const heap = new ArrayBuffer(1 << 16);
const core = new Proxy({ HEAPF32: new Float32Array(heap), HEAPU8: new Uint8Array(heap), HEAP32: new Int32Array(heap) }, {
  get(t, k) { if (k in t) return t[k]; if (typeof k === 'string' && k.startsWith('_')) return (...a) => { calls.push(k); return 0; }; return undefined; },
});
let pending = null, tickNo = 0;
const host = {
  core, padPtr: 0, clock: 0, state: new Float32Array(16), lastRescues: 0, animationReady: false, aiActive: false,
  freeRide: { stalled: () => false, tick() { if (tickNo === 1) pending = { pos: [1, 2, 3], yaw: 0 }; } }, // builtin 68 in tick 1's 121818
  nisStart() { if (!pending) return; const a = pending; pending = null; core._nis_hold(1, a.pos[0], a.pos[1], a.pos[2], 1, 0); },
};
const tick = createGameTick(host);
const input = new Float32Array(24);
for (tickNo = 1; tickNo <= 2; tickNo++) { calls.push(`tick ${tickNo}`); tick.simulate(input); }
const at = (name, from = 0) => calls.indexOf(name, from);
const t2 = at('tick 2'), scan1 = at('_section_pass'), hold = at('_nis_hold');
assert.ok(scan1 >= 0 && scan1 < t2, 'tick 1 runs its section pass');
assert.ok(hold > scan1, 'the hold comes after the firing tick\'s section pass');
assert.ok(hold > t2 && hold < at('_race_begin', t2) && hold < at('_step_rider', t2), 'the hold comes before every pass of the next tick');
assert.equal(calls.filter((c) => c === '_nis_hold').length, 1);
console.log('test-nis-after-scan: the station cut\'s hold after the firing tick\'s section pass, before the next tick\'s passes');
