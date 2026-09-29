// Free-roam vs event frame cost on the same ground (docs/sim-performance.md "Free-roam steady state"). QA tool, not in npm test.
// Headless Chrome (--mute-audio, web/headless-chrome.mjs) or the system WebKit (web/webkit-driver.mjs), ?mute=1, a private Vite
// server over web/ (or --origin). web/perf-probe.js records every drawn frame; an autopilot rides the AIP race paths.
//   node bench-free-roam.mjs [--browser chrome|webkit] [--scen hub,zone,ev,evnoai] [--secs 45] [--phone 1] [--cpu 4]
//                            [--pv staticWorld] [--profile 1] [--out DIR] [--origin URL]
//   node bench-free-roam.mjs --summary DIR [DIR...] [--by vis] [--detail]
// Scenarios: happy = the CTM start at Happiness (peakCourse 14) riding down; hub = Peak 1 free ride from Green Base Station down A_ARA1 into ARA1 (the hub row, then A_ARA1 alone after its
// Unload trigger, then the ARA1 row; split by the drawn location set with --by vis); zone = free ride on ARA1 (Snow Jam's row);
// ev / evnoai = the Snow Jam race with / without computer riders. Free ride runs with the streaming switches on
// (streamWarm, streamGate, streamAhead, ctmWorldAudio) so load work stays out of the steady state; --pv adds switches.
import fs from 'node:fs'; import path from 'node:path';
import { startBrowser, startServer, sleep } from './headless-chrome.mjs';
import { startWebKit } from './webkit-driver.mjs';
const WEB = path.dirname(new URL(import.meta.url).pathname);
const argv = process.argv.slice(2);
const args = Object.fromEntries(argv.reduce((a, v, i, all) => (v.startsWith('--') ? [...a, [v.slice(2), all[i + 1] && !all[i + 1].startsWith('--') ? all[i + 1] : '1']] : a), []));

// ---- summary ----------------------------------------------------------------------------------------------------------------
const q = (a, p) => { if (!a.length) return 0; const s = [...a].sort((x, y) => x - y); return s[Math.min(s.length - 1, Math.floor(p * s.length))]; };
const mean = (a) => (a.length ? a.reduce((x, y) => x + y, 0) / a.length : 0);
export function summarise(frames) {
  const fr = frames.filter((f) => f.screen === 'game' && !f.cs), iv = fr.map((f) => f.iv).filter((x) => x > 0), m = (k) => mean(fr.map(k));
  const gpu = fr.filter((f) => f.gpu != null).map((f) => f.gpu);
  return { frames: fr.length, p50: q(iv, 0.5), p95: q(iv, 0.95), p99: q(iv, 0.99), max: Math.max(0, ...iv), over20: iv.filter((x) => x > 20).length, over34: iv.filter((x) => x > 34).length, over50: iv.filter((x) => x > 50).length,
    cb: m((f) => f.cb), cb95: q(fr.map((f) => f.cb), 0.95), sim: m((f) => f.sim), ticks: m((f) => f.ticks), render: m((f) => f.js.render || 0), freeRide: m((f) => f.js['fr.update'] || 0),
    setPieces: m((f) => f.js['sp.update'] || f.js['esp.update'] || 0), draws: m((f) => f.draws), ktris: m((f) => f.tris) / 1000, pipelineSwitches: m((f) => f.pipeSwitch), passes: m((f) => f.passes),
    bindGroups: m((f) => f.binds), writeKB: m((f) => f.wbytes) / 1024, gpu: gpu.length ? mean(gpu) : null, gpu95: gpu.length ? q(gpu, 0.95) : null,
    builds: fr.reduce((n, f) => n + (f.builds || 0), 0), over100: iv.filter((x) => x > 100).length };
}
function printSummary(dirs) {
  const BY = args.by || null, rows = [];
  for (const d of dirs) for (const file of fs.readdirSync(d).filter((f) => f.endsWith('.json'))) {
    const data = JSON.parse(fs.readFileSync(path.join(d, file), 'utf8')); const groups = new Map();
    for (const f of data.frames) { const k = BY ? String(f[BY] ?? '-') : ''; if (!groups.has(k)) groups.set(k, []); groups.get(k).push(f); }
    for (const [k, g] of groups) { const s = summarise(g); if (s.frames >= 20) rows.push([path.basename(d) + '/' + file.replace('.json', '') + (k ? ` [${k}]` : ''), s]); }
  }
  const f1 = (x) => (x == null ? '    -' : x.toFixed(1).padStart(5));
  console.log('scenario'.padEnd(56), 'frames   p50   p95   p99   max >20 >34 >50 >100 |    cb  cb95   sim ticks render frUpd spUpd | draws ktri  sw pass |   gpu gpu95 | builds');
  for (const [n, s] of rows) console.log(n.padEnd(56), String(s.frames).padStart(6), f1(s.p50), f1(s.p95), f1(s.p99), f1(s.max), String(s.over20).padStart(3), String(s.over34).padStart(3), String(s.over50).padStart(3), String(s.over100).padStart(4), '|', f1(s.cb), f1(s.cb95), f1(s.sim), s.ticks.toFixed(2).padStart(5), f1(s.render), f1(s.freeRide), f1(s.setPieces), '|', s.draws.toFixed(0).padStart(5), s.ktris.toFixed(0).padStart(4), s.pipelineSwitches.toFixed(0).padStart(3), s.passes.toFixed(0).padStart(4), '|', f1(s.gpu), f1(s.gpu95), '|', String(s.builds).padStart(5));
}
if (args.summary) { printSummary([args.summary, ...argv.filter((a) => !a.startsWith('--') && fs.existsSync(a) && a !== args.summary)]); process.exit(0); }

// ---- run --------------------------------------------------------------------------------------------------------------------
const kind = args.browser || 'chrome', SECS = +(args.secs ?? 45), PHONE = args.phone === '1', CPU = +(args.cpu ?? 1);
const out = path.resolve(args.out || `bench-free-roam-${Date.now()}`); fs.mkdirSync(out, { recursive: true });
const log = (...a) => { const s = a.join(' '); console.log(s); fs.appendFileSync(path.join(out, 'driver.log'), s + '\n'); };
// Autopilot paths: every race path of the locations, as source-cm polylines (origin + segment direction x length).
function paths(names) {
  const outp = [];
  for (const [loc, keep] of names) {
    const d = JSON.parse(fs.readFileSync(path.join(WEB, 'public/assets/PEAK1', loc, 'paths.json'), 'utf8'));
    for (const p of d.variants['0'].race_paths) { if (keep && !keep.includes(p.index)) continue; const pts = [p.origin]; for (const s of p.segments) { const a = pts.at(-1); pts.push([a[0] + s[0] * s[3], a[1] + s[1] * s[3], a[2] + s[2] * s[3]]); } outp.push({ name: `${loc}:${p.index}`, pts }); }
  }
  return outp;
}
// --stream default: the streaming switches at their defaults (the riding builds as players get them)
const STREAM = [args.stream === 'default' ? '' : 'streamWarm,streamGate,streamAhead,ctmWorldAudio', args.pv || ''].filter(Boolean).join(','), PVQ = STREAM ? `&pv=${STREAM}` : ''; // an empty ?pv= turns every switch off
const ARA1_RUN = [['ARA1', [0, 3, 4, 5, 6, 7]]];
const SCEN = {
  hub: { url: `/?course=PEAK1&autostart=1${PVQ}`, paths: [['A', [0, 4]], ['A_ARA1', null], ['ARA1', [0, 3]]], free: true, secs: 60 },
  zone: { url: `/?course=PEAK1&peakCourse=0&autostart=1${PVQ}`, paths: ARA1_RUN, free: true },
  ev: { url: `/?course=ARA1&autostart=1${args.pv ? '&pv=' + args.pv : ''}`, paths: ARA1_RUN },
  // happy: the Conquer the Mountain start at Happiness (peakCourse 14), riding down (autopilot on every Peak 1 race path near the rider)
  happy: { url: `/?course=PEAK1&peakCourse=14&autostart=1${PVQ}`, paths: ['A', 'ABA1', 'ABC1', 'ABC1_A', 'ARA1', 'ARA1_B', 'ASS1', 'A_ABA1', 'A_ARA1', 'A_ASS1', 'B', 'BHP1', 'BRA2', 'B_BHP1', 'B_BRA2', 'DRA4_A'].map((l) => [l, null]), free: true, secs: 60 },
  evnoai: { url: `/?course=ARA1&autostart=1&ai=0${args.pv ? '&pv=' + args.pv : ''}`, paths: ARA1_RUN },
};
const PROBE = fs.readFileSync(path.join(WEB, 'perf-probe.js'), 'utf8');
const server = args.origin ? null : await startServer(), origin = args.origin || server.origin;
let B;
if (kind === 'chrome') {
  const b = await startBrowser({ init: PROBE, width: PHONE ? 390 : 1280, height: PHONE ? 844 : 960 });
  if (!b) { console.log('no Chrome'); process.exit(0); }
  if (PHONE) await b.send('Emulation.setDeviceMetricsOverride', { width: 390, height: 844, deviceScaleFactor: 3, mobile: true });
  if (CPU > 1) await b.send('Emulation.setCPUThrottlingRate', { rate: CPU });
  B = { goto: (u) => b.goto(u), eval: (e) => b.evaluate(e), close: () => b.close(), send: b.send };
} else {
  const w = await startWebKit({ width: PHONE ? 390 : 1280, height: PHONE ? 844 : 960 });
  if (!w) { console.log('no WebKit'); process.exit(0); }
  B = { goto: async (u) => { await w.goto(u); await w.eval(`(()=>{${PROBE};return 1})()`); }, eval: (e) => w.eval(e), close: () => w.close() };
}
const waitFor = async (expr, ms) => { const t = Date.now(); while (Date.now() - t < ms) { try { if (await B.eval(expr)) return; } catch {} await sleep(500); } throw Error('timeout ' + expr); };
try {
  for (const name of (args.scen || 'hub,zone,evnoai').split(',')) {
    const sc = SCEN[name]; const url = origin + sc.url + '&perf=1&qa=1&mute=1' + (PHONE ? '&quality=low' : '');
    log(name, url); await B.goto(url);
    await waitFor(`!!window.__cp && window.__perfUI?.()?.screen==='game' && !!window.__perfCore?.()?.core && !window.__cutscenes?.active`, 300000);
    if (sc.free) await waitFor(`window.__freeRide && !window.__freeRide.stalled()`, 120000);
    await B.eval(`(()=>{const P=window.__cp;P.paths=${JSON.stringify(paths(sc.paths))};P.usePad=true;P.auto=true;P.timestamps=true;P.byLoc=true;return 1})()`);
    await sleep(3000);
    const PROF = args.profile === '1' && kind === 'chrome';
    if (PROF) { await B.send('Profiler.enable'); await B.send('Profiler.setSamplingInterval', { interval: 250 }); await B.send('Profiler.start'); }
    await B.eval(`(()=>{window.__cp.take();window.__cp.on=true;window.__cp.label=${JSON.stringify(name)};return 1})()`);
    const t0 = Date.now(), all = { frames: [], gc: [], longtasks: [] };
    while (Date.now() - t0 < (sc.secs ?? SECS) * 1000) { await sleep(5000); const r = await B.eval(`window.__cp.take()`); all.frames.push(...r.frames); all.gc.push(...r.gc); all.longtasks.push(...r.longtasks); (all.buildLog ??= []).push(...(r.buildLog || [])); }
    if (PROF) { const { profile } = await B.send('Profiler.stop'); fs.writeFileSync(path.join(out, name + '.cpuprofile'), JSON.stringify(profile)); }
    await B.eval(`(()=>{window.__cp.on=false;window.__cp.auto=false;return 1})()`);
    fs.writeFileSync(path.join(out, name + '.json'), JSON.stringify(all));
    const s = summarise(all.frames); log(name, JSON.stringify(Object.fromEntries(Object.entries(s).map(([k, v]) => [k, v == null ? v : +v.toFixed(2)]))));
  }
} finally { await B.close(); await server?.close(); }
printSummary([out]);
process.exit(0);
