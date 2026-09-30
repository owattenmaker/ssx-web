// web/gc-watchdog.js: the JavaScriptCore test, the kick, and the stall rule on a fake clock / registry (docs/mobile.md "Hangs").
import assert from 'node:assert/strict';
import { isJavaScriptCore, kickMemory, createGcWatchdog } from './gc-watchdog.js';
const ua = (userAgent) => ({ userAgent });
assert.equal(isJavaScriptCore(ua('Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/19.0 Safari/605.1.15')), true);
assert.equal(isJavaScriptCore(ua('Mozilla/5.0 (iPhone; CPU iPhone OS 19_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) CriOS/140.0 Mobile/15E148 Safari/604.1')), true);
assert.equal(isJavaScriptCore(ua('Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36')), false);
assert.equal(isJavaScriptCore(ua('Mozilla/5.0 (Linux; Android 14) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0 Mobile Safari/537.36')), false);
assert.equal(isJavaScriptCore(ua('Mozilla/5.0 (X11; Linux x86_64; rv:140.0) Gecko/20100101 Firefox/140.0')), false);
assert.ok(kickMemory() instanceof WebAssembly.Memory);

// fake env: collect(full) runs the registry callbacks of the young (eden) or all (full) registered markers; the engine's full
// collection comes with the `threshold`-th kick memory (the fast-memory slot count), none when threshold is 0
function run(script) {
  let t = 0, tick = null; const regs = [];
  class Registry { constructor(cb) { this.cb = cb; this.items = []; regs.push(this); } register(_o, v) { this.items.push(v); } }
  const collect = (full) => { for (const r of regs) { const keep = []; for (const v of r.items) (v < 0 || full ? r.cb(v) : keep.push(v)); r.items = keep; } };
  let made = 0, pendingFull = false;
  const w = createGcWatchdog({ now: () => t, log: () => {}, isSafe: () => script.safe?.(t) ?? false,
    makeMemory: () => { made++; if (script.threshold && made % script.threshold === 0) pendingFull = true; return {}; },
    env: { FinalizationRegistry: Registry, setInterval: (f) => { tick = f; }, requestAnimationFrame: null } });
  for (let s = 0; s < script.secs; s++) { t = s * 1000; tick(); if (pendingFull) { pendingFull = false; collect(true); } script.step(s, collect); }
  return { made, w };
}
const healthy = (s, c) => { c(false); if (s % 5 === 0) c(true); };
// healthy: edens every second, a full every 5 s -> never
assert.equal(run({ secs: 600, threshold: 4, step: healthy }).made, 0);
// idle pause: nothing collects at all -> never (no allocation: neither edens nor fulls)
assert.equal(run({ secs: 600, threshold: 4, step: () => {} }).made, 0);
// stall (edens only after 10 s): the kick starts 40 s after the last full + the 5 s safe wait, one memory a second until the
// 4th brings a full collection, then the episode ends; the fulls come back (healthy again) -> no second episode
{ const r = run({ secs: 200, threshold: 4, step: (s, c) => { c(false); if ((s < 10 || s > 80) && s % 5 === 0) c(true); } });
  assert.equal(r.w.stalls.length, 1); const e = r.w.stalls[0]; assert.equal(r.made, 4); assert.equal(e.memories, 4);
  assert.ok(e.at >= 45000 && e.at <= 51000, 'kick at ' + e.at); assert.ok(e.recoveredMs >= 3000 && e.recoveredMs <= 5000, 'recovered ' + e.recoveredMs); }
// a safe moment (pause / load / cutscene) lets it go without the extra wait
{ const r = run({ secs: 60, threshold: 1, safe: () => true, step: (s, c) => { c(false); if (s === 3) c(true); } }); assert.equal(r.w.stalls.length, 1); assert.ok(r.w.stalls[0].safe && r.w.stalls[0].at <= 46000); }
// an engine that never collects: at most KICK_MAX (6) memories, then it gives up until the cooldown ends
{ const r = run({ secs: 70, threshold: 0, step: (s, c) => { c(false); if (s === 1) c(true); } }); assert.equal(r.made, 6); assert.equal(r.w.stalls[0].gaveUp, true); }
console.log('gc-watchdog: ok');
