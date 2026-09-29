// GPU device loss recovery (web/gpu-recovery.js) and the field-report crash marker (web/diagnostics.js), in headless
// Chrome over a private dev server (web/headless-chrome.mjs). Skipped without Chrome / WebGPU.
//  1. Metro City at a frozen clock: tick 400, frame A; the device is destroyed (reason "destroyed", as iOS reports it);
//     the renderer gets a new device and frame B must be pixel-identical to A, with no console error on the way.
//  2. A loss while the page is hidden waits for the page to be visible, then recovers.
//  3. No adapter any more: 3 attempts, then the page reloads (gpu-recovery-failed with reload).
//  4. A reload from the game (the game pauses when the page hides, after pagehide) is a clean exit: the next load
//     reports no 'previous-session-died' (the pause used to overwrite the clean mark: 5 false deaths on 'ctm-pause').
import assert from 'node:assert/strict';
import { startBrowser, startServer, sleep } from './headless-chrome.mjs';
const INIT = `(() => {
  if (window !== window.top) return;
  window.__doc = Math.random().toString(36).slice(2); window.__diag = [];
  const collect = (text) => { try { window.__diag.push(...JSON.parse(text).events); } catch {} };
  navigator.sendBeacon = (u, d) => { if (String(u).includes('/mp/diag')) { d.text().then(collect); return true; } return false; };
  const f = window.fetch; window.fetch = (u, o) => { if (String(u).includes('/mp/diag')) { collect(o.body); return Promise.resolve(new Response(null, { status: 204 })); } return f(u, o); };
  let vis = 'visible';
  Object.defineProperty(Document.prototype, 'visibilityState', { get: () => vis, configurable: true });
  Object.defineProperty(Document.prototype, 'hidden', { get: () => vis === 'hidden', configurable: true });
  window.__setVisibility = (v) => { vis = v; document.dispatchEvent(new Event('visibilitychange')); };
})();`;
const browser = await startBrowser({ init: INIT });
if (!browser) { console.log('GPU recovery check SKIPPED: no Chrome found (set CHROME=/path)'); process.exit(0); }
const server = await startServer();
const errorsSince = (n) => browser.logs.slice(n).filter((l) => /^(error|exception)/.test(l));
try {
  if (!(await browser.hasWebGPU(server.origin))) { console.log('GPU recovery check SKIPPED: no WebGPU adapter in headless Chrome'); process.exit(0); }
  await browser.goto(`${server.origin}/?qa=1&diag=1&course=BRA2&quality=low`);
  await browser.waitFor('!!window.ssxQA && !!window.__gpuRecovery', 300000);
  await browser.evaluate(`(()=>{const raf=window.requestAnimationFrame.bind(window);window.__frozenMs=performance.now();window.requestAnimationFrame=cb=>raf(()=>cb(window.__frozenMs));ssxQA.start();ssxQA.hud(false);return 1})()`);
  await sleep(3000); await browser.evaluate('ssxQA.advance(400);1'); await sleep(2500);
  // 1) destroyed device -> the same frame again
  const frameA = await browser.screenshot(), logMark = browser.logs.length;
  await browser.evaluate('window.__gpuRecovery.simulateLoss();1');
  await browser.waitFor('window.__gpuRecovery.state.recoveries === 1', 15000);
  await sleep(2500);
  const frameB = await browser.screenshot();
  assert.ok(frameA.length > 20000, 'frame A has content');
  assert.equal(frameB, frameA, 'the frame after the recovery equals the frame before the loss');
  assert.deepEqual(errorsSince(logMark), [], 'no console error from the loss or the recovery');
  const kinds = await browser.evaluate('window.__diag.map((e) => e.kind + (e.reason ? ":" + e.reason : ""))');
  assert.ok(kinds.includes('gpu-device-lost:destroyed') && kinds.includes('gpu-recovered'), 'diagnostics report the loss and the recovery: ' + kinds.filter((k) => /gpu/.test(k)));
  console.log('Device loss (destroyed) recovered: same frame, no errors');
  // 2) hidden: no recovery until visible
  await browser.evaluate('window.__setVisibility("hidden");window.__gpuRecovery.simulateLoss();1'); await sleep(2000);
  assert.deepEqual(await browser.evaluate('({lost: window.__gpuRecovery.state.recoveries, waiting: window.__gpuRecovery.state.recovering})'), { lost: 1, waiting: true }, 'a hidden page waits');
  await browser.evaluate('window.__setVisibility("visible");1');
  await browser.waitFor('window.__gpuRecovery.state.recoveries === 2', 15000);
  console.log('Device loss while hidden: recovered once visible');
  // 3) no adapter -> reload
  const doc1 = await browser.evaluate('window.__doc');
  await browser.evaluate('navigator.gpu.requestAdapter = async () => null; window.__gpuRecovery.simulateLoss(); 1');
  await browser.waitFor(`window.__doc !== '${doc1}' && !!window.ssxQA`, 300000);
  console.log('Device loss without an adapter: the page reloaded');
  // 4) clean reload from the game: no false 'previous-session-died'
  await browser.evaluate('ssxQA.start();1'); await sleep(1500);
  assert.equal(await browser.evaluate('document.getElementById("stage").dataset.screen'), 'game');
  const doc2 = await browser.evaluate('window.__doc');
  await browser.send('Page.reload'); await sleep(1500);
  await browser.waitFor(`window.__doc !== '${doc2}' && window.__diag.some((e) => e.kind === "hello")`, 60000);
  const died = await browser.evaluate('window.__diag.filter((e) => e.kind === "previous-session-died")');
  assert.deepEqual(died, [], 'a reload from the game is a clean exit');
  console.log('Reload from the game: no previous-session-died');
} finally { await browser.close(); await server.close(); }
console.log('GPU recovery OK');
process.exit(0);
