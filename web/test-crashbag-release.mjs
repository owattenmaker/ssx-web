// A crashbag the rider hit, then its location leaves the world (docs/course-switch.md "Crashbags across a location's release"):
// the PS2 destroys the crashbag's Object entity, with its RollerModifier, at the unload start (row -> 7: 230360 -> 3551A8(entities,
// group 1 / 8, track) -> 361038 -> 3553C0 -> 34FBF0). The core used to keep the roller running, and pv peakRelease then freed the
// collision nodes its collider points into: "Original roller collider has no sphere tree", bad_alloc and out-of-bounds traps out of
// every game tick, and the in-world Transport stalled ("Transport failed") or the page went down.
// Headless Chrome (--mute-audio) + a private Vite server, QA mode: MOUNTAIN at R&B (ASS1), the neutral pad rides into the
// crashbag 376843 (a roller), then a Transport to Crow's Nest (ABA1) evicts R&B. Checked:
//  - no roller of the evicted track survives its release, and none is left once the Transport arrives;
//  - no core exception / trap / "Transport failed", the arrival lands and the game ticks on for 5 s.
//   node test-crashbag-release.mjs            (the live core)
//   CORE_DIR=path/to/core node test-crashbag-release.mjs   (a scratch core: core.js + core.wasm served as /runtime/)
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { startBrowser, sleep } from './headless-chrome.mjs';

if (!fs.existsSync(new URL('./public/assets/MOUNTAIN', import.meta.url))) { console.log('crashbag release: skipped (no game data)'); process.exit(0); }
const web = new URL('.', import.meta.url).pathname, coreDir = process.env.CORE_DIR ? path.resolve(process.env.CORE_DIR) : null;
const { createServer } = await import('vite');
const coreSwap = { name: 'core-dir', configureServer(server) { if (coreDir) server.middlewares.use((req, res, next) => {
  const m = /^\/runtime\/(core\.(?:js|wasm))(\?.*)?$/.exec(req.url || ''); if (!m) return next();
  const body = fs.readFileSync(path.join(coreDir, m[1])); res.setHeader('Content-Type', m[1].endsWith('.js') ? 'text/javascript' : 'application/wasm'); res.setHeader('Cache-Control', 'no-store'); res.end(body); }); } };
const server = await createServer({ root: web, logLevel: 'error', plugins: [coreSwap], server: { host: '127.0.0.1', port: 29000 + Math.floor(Math.random() * 2000), strictPort: false, hmr: false } });
await server.listen();
const origin = (server.resolvedUrls?.local?.[0] || `http://127.0.0.1:${server.config.server.port}/`).replace(/\/$/, '');
const browser = await startBrowser({ width: 960, height: 720 });
if (!browser) { console.log('crashbag release: skipped (no Chrome)'); await server.close(); process.exit(0); }
const E = (x) => browser.evaluate(x);
try {
  if (!(await browser.hasWebGPU(origin))) console.log('crashbag release: skipped (no WebGPU)');
  else await run();
} finally { await browser.close(); await server.close(); }
async function run() {
  await browser.goto(`${origin}/?qa=1&mute=1&perf=1&rider=zoe&presentationSeed=0x182200&course=MOUNTAIN&peakCourse=5&autostart=1`);
  await browser.waitFor(`(()=>{const ui=window.ssxQA?.ui?.();return !!(ui&&ui.screen==='game'&&!ui.loading?.active&&window.__freeRide?.core?._game_tick)})()`, 300000);
  // core exceptions (decoded) and traps, from here on
  await E(`(()=>{const L=window.__crashLog=[];const dec=x=>{try{if(x instanceof WebAssembly.Exception)return 'C++ '+JSON.stringify(window.__freeRide.core.getExceptionMessage(x));}catch{}return String(x?.message||x)};
    addEventListener('error',e=>{if(L.length<50)L.push('error: '+dec(e.error??e.message))});addEventListener('unhandledrejection',e=>{if(L.length<50)L.push('rejection: '+dec(e.reason))});
    const ce=console.error.bind(console);console.error=(...a)=>{if(L.length<50)L.push('console: '+a.map(dec).join(' ').slice(0,300));ce(...a)};return 1})()`);
  const ROLLERS = `JSON.stringify((()=>{const c=window.__freeRide.core,F=c.HEAPF32,p=c._roller_info()>>2,n=F[p];return {tick:c._game_tick(),course:window.__freeRide.course(),rollers:Array.from({length:n},(_,k)=>F[p+4+13*k])}})())`;
  let s; const t0 = Date.now();
  do { await sleep(500); s = JSON.parse(await E(ROLLERS)); } while (!s.rollers.length && Date.now() - t0 < 90000);
  assert.ok(s.rollers.length, `the neutral pad hit a crashbag at R&B (tick ${s.tick})`);
  const track = s.rollers[0] & 255, hitTick = s.tick;
  assert.equal(await E('String(ssxQA.ui().cb.freeRide(8))'), 'transport', 'an in-world Transport to ABA1');
  // the release of R&B (its row out of the world, its collision freed) happens during the ride; no roller of it may run past it
  const t1 = Date.now(); let arrived = null, stale = null;
  while (Date.now() - t1 < 90000) {
    await sleep(250);
    try { s = JSON.parse(await E(ROLLERS)); } catch (e) { assert.fail(`the core trapped during the Transport: ${String(e.message).slice(0, 300)} | ${await E('JSON.stringify(window.__crashLog.slice(0, 3))').catch(() => '')}`); }
    const freed = await E(`(()=>{const fr=window.__freeRide;return !fr.render.has('ASS1')})()`);
    if (freed && s.rollers.some((r) => (r & 255) === track)) stale ??= s;
    const ui = JSON.parse(await E(`JSON.stringify({screen:ssxQA.ui().screen,load:!!ssxQA.ui().loading?.active})`));
    if (s.course === 8 && ui.screen === 'game' && !ui.load) { arrived = s; break; }
  }
  assert.ok(arrived, `the Transport arrived at ABA1 (last ${JSON.stringify(s)})`);
  const tick0 = arrived.tick; await sleep(5000); const end = JSON.parse(await E(ROLLERS));
  const log = JSON.parse(await E('JSON.stringify(window.__crashLog)'));
  assert.deepEqual(log.filter((l) => !/Replay cameras/.test(l)), [], 'no core exception, trap or failed Transport');
  assert.equal(stale, null, `no roller of the released track ${track} runs after R&B left the world (${JSON.stringify(stale)})`);
  assert.ok(!end.rollers.some((r) => (r & 255) === track), `no roller of track ${track} after the arrival (${JSON.stringify(end)})`);
  assert.ok(end.tick > tick0 + 60, `the game ticks on after the arrival (${tick0} -> ${end.tick})`);
  console.log(`crashbag release: roller hit at tick ${hitTick} (track ${track}), gone with R&B's unload; Transport to ABA1 arrived at tick ${tick0}, ${end.tick - tick0} ticks after it, no core errors${coreDir ? ' (core ' + coreDir + ')' : ''}`);
}
