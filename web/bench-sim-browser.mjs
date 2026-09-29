// Paired simulation benchmark in a browser (docs/sim-performance.md "Simulation core, round 3"). QA tool, not in npm test.
// web/bench-sim-page.js runs web/bench-sim.mjs's six-rider tick for each core build in one page, in alternating chunks (machine
// load hits every build alike; frame-rate runs of the whole game drift +-15% between page loads on a busy machine).
//   node bench-sim-browser.mjs [--browser chrome|webkit] [--cpu 4] [--ticks 2000] [--repeat 1] [--solo 1] SPEC SPEC...
// SPEC: CORE_DIR[:fast][@AI_RACERS_JS] (a core.js / core.wasm directory, e.g. web/runtime or a CORE_OUT= build; ":fast" turns the
// presentation fast mode on; an ai-racers.js to use instead of web/'s). Prints ms per tick and the median per-chunk ratio to the
// first SPEC. Chrome runs headless with --mute-audio (web/headless-chrome.mjs); WebKit through web/webkit-driver.mjs.
import fs from 'node:fs'; import path from 'node:path';
import { startBrowser, sleep } from './headless-chrome.mjs';
import { startWebKit } from './webkit-driver.mjs';
const W = path.dirname(new URL(import.meta.url).pathname);
const argv = process.argv.slice(2), arg = (k, d) => { const i = argv.indexOf('--' + k); if (i < 0) return d; const v = argv[i + 1]; argv.splice(i, 2); return v; };
const kind = arg('browser', 'chrome'), CPU = +arg('cpu', '1'), TICKS = +arg('ticks', '2000'), REPEAT = +arg('repeat', '1'), SOLO = arg('solo', '0') === '1'; // --solo 1: the human alone
const specs = argv.length ? argv : [path.join(W, 'runtime')];
const coreDirs = [], aiFiles = [];
const pageSpecs = specs.map((s) => { const [core, ai] = s.split('@'), [dir, mode] = core.split(':');
  coreDirs.push(path.resolve(dir)); let a = ''; if (ai) { aiFiles.push(path.resolve(ai)); a = '@' + (aiFiles.length - 1); }
  return `${coreDirs.length - 1}${mode === 'fast' ? ':fast' : ''}${a}`; }).join(',');
const types = { js: 'text/javascript', wasm: 'application/wasm' };
const plugin = { name: 'bench-sim', configureServer(server) { server.middlewares.use((req, res, next) => {
  const u = new URL(req.url, 'http://x'); let file = null;
  if (u.pathname === '/__bench/page.html') { res.setHeader('Content-Type', 'text/html'); res.end('<!doctype html><meta charset="utf-8"><title>sim bench</title><script type="module" src="/bench-sim-page.js"></script>'); return; }
  let m = /^\/__bench\/core\/(\d+)\/(core\.(js|wasm))$/.exec(u.pathname); if (m) file = path.join(coreDirs[+m[1]], m[2]);
  m = /^\/__bench\/ai\/(\d+)\.js$/.exec(u.pathname); if (m) file = aiFiles[+m[1]];
  if (!file) return next();
  res.setHeader('Content-Type', types[file.split('.').pop()]); res.setHeader('Cache-Control', 'no-store'); res.end(fs.readFileSync(file)); }); } };
const { createServer } = await import('vite');
const server = await createServer({ root: W, logLevel: 'error', plugins: [plugin], server: { host: '127.0.0.1', port: 33000 + Math.floor(Math.random() * 1000), strictPort: false, hmr: false } });
await server.listen(); const origin = (server.resolvedUrls?.local?.[0] || `http://127.0.0.1:${server.config.server.port}/`).replace(/\/$/, '');
let B;
if (kind === 'chrome') { const b = await startBrowser({ width: 800, height: 600 }); if (!b) { console.log('no Chrome'); process.exit(0); } if (CPU > 1) await b.send('Emulation.setCPUThrottlingRate', { rate: CPU }); B = { goto: (u) => b.goto(u), eval: (e) => b.evaluate(e), close: () => b.close() }; }
else { const w = await startWebKit({ width: 800, height: 600 }); if (!w) { console.log('no WebKit'); process.exit(0); } B = { goto: (u) => w.goto(u), eval: (e) => w.eval(e), close: () => w.close() }; }
try {
  for (let r = 0; r < REPEAT; r++) {
    await B.goto(`${origin}/__bench/page.html?mute=1&ticks=${TICKS}&solo=${SOLO ? 1 : 0}&specs=${encodeURIComponent(pageSpecs)}`); let st = null;
    for (let i = 0; i < 3600; i++) { await sleep(2000); try { st = JSON.parse(await B.eval('JSON.stringify(window.__bench)')); } catch { continue; } if (st && (st.status === 'done' || st.status === 'error')) break; }
    if (!st || st.status !== 'done') { console.log('failed', st?.error ?? st?.status); break; }
    st.results.forEach((x, k) => console.log(`${kind}${CPU > 1 ? ' ' + CPU + 'x' : ''} run ${r}: ${specs[k]}: ${x.msPerTick.toFixed(3)} ms/tick` + (k ? `, ${x.ratio.toFixed(3)}x of the first` : '')));
  }
} finally { await B.close(); await server.close(); }
process.exit(0);
