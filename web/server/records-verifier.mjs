#!/usr/bin/env node
// The online records' verifier (decision D5, docs/online-records.md "The verifier"): on the host, outside the server's sandbox, as its
// own LaunchAgent. It asks mp-server (loopback, the token) which runs to check, opens the game's own page in headless Chrome in
// ?verify mode, lets the page re-simulate each stored run on the current build (web/online-replay.js verifyOnlineRun: the replay's
// pad stream from its start state, the finish record of the tick it finishes on) and reports the result (records.mjs verifyEntry):
// reproduced -> verified (a flagged run is listed); not reproduced on its own core -> pulled; on another core -> stale (D7).
//   node server/records-verifier.mjs [--page URL] [--api URL] [--token-file F] [--profile DIR] [--chrome PATH] [--once]
//        [--interval S] [--max-load L] [--per-cycle N] [--timeout S] [--reload N]
//   --page / --api: the page and the records API (default both http://127.0.0.1:8787); --token-file: the shared secret (the server's
//   MP_RECORDS_VERIFIER_TOKEN_FILE); --profile: Chrome's own profile (default ./verifier-profile); --once: work the queue, then exit;
//   --interval: seconds between queue checks (60); --max-load: skip while the 1-minute load average per core is above this (0.6);
//   --per-cycle: runs per check (10); --timeout: seconds per run (900); --reload: a fresh page every N runs (5).
// Gentle on the host: Chrome runs under `nice -n 19`, one run at a time, only while no online race is on (/mp/status) and the load is
// low, and it is closed whenever the queue is empty. Every run logs its wall time, the page's simulation time, Chrome's CPU time and
// its peak resident memory (the process tree's RSS). Dependency-free: imports only node:.
import { spawn, spawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const argv = process.argv.slice(2), opt = (k, d) => { const i = argv.indexOf('--' + k); return i < 0 ? d : argv[i + 1]; }, flag = (k) => argv.includes('--' + k);
const PAGE = opt('page', 'http://127.0.0.1:8787').replace(/\/$/, ''), API = opt('api', PAGE).replace(/\/$/, '');
const TOKEN = (() => { const f = opt('token-file', process.env.MP_RECORDS_VERIFIER_TOKEN_FILE); try { return f ? fs.readFileSync(f, 'utf8').trim() : ''; } catch { return ''; } })();
const PROFILE = path.resolve(opt('profile', 'verifier-profile')), ONCE = flag('once');
const INTERVAL = +opt('interval', 60) * 1000, MAX_LOAD = +opt('max-load', 0.6), PER_CYCLE = +opt('per-cycle', 10), TIMEOUT = +opt('timeout', 900) * 1000;
const RELOAD = +opt('reload', 5);
const CHROME = opt('chrome', process.env.CHROME) || ['/Applications/Google Chrome.app/Contents/MacOS/Google Chrome', '/Applications/Chromium.app/Contents/MacOS/Chromium',
  '/usr/bin/google-chrome', '/usr/bin/chromium'].find((p) => fs.existsSync(p));
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const log = (...a) => console.log(new Date().toISOString(), ...a);
if (!TOKEN) { console.error('records-verifier: no token (--token-file)'); process.exit(2); }
if (!CHROME) { console.error('records-verifier: no Chrome (--chrome)'); process.exit(2); }

const api = async (p, init = {}) => {
  const r = await fetch(API + p, { ...init, headers: { 'x-ssx-verifier': TOKEN, 'content-type': 'application/json', ...(init.headers || {}) } });
  if (!r.ok) throw new Error(`${p}: ${r.status}`);
  return r.json();
};
// the host is busy: the load average per core, or an online race running (its racers' frames come first)
async function busy() {
  if (os.loadavg()[0] / os.cpus().length > MAX_LOAD) return 'load';
  try { const s = await (await fetch(PAGE + '/mp/status')).json(); if ((s.lobbies || []).some((l) => l.racing || l.race)) return 'online race'; } catch {}
  return null;
}

// ---- Chrome over the DevTools protocol ----
let chrome = null;
async function startChrome() {
  fs.mkdirSync(PROFILE, { recursive: true });
  const port = 29500 + Math.floor(Math.random() * 400);
  const proc = spawn('/usr/bin/nice', ['-n', '19', CHROME, `--remote-debugging-port=${port}`, `--user-data-dir=${PROFILE}`, '--headless=new', '--mute-audio',
    '--no-first-run', '--no-default-browser-check', '--window-size=640,480', '--enable-unsafe-webgpu', '--disable-background-timer-throttling',
    '--disable-renderer-backgrounding', 'about:blank'], { stdio: 'ignore' });
  let target;
  for (let i = 0; i < 200 && !target; i++) { await sleep(150); try { target = (await (await fetch(`http://127.0.0.1:${port}/json`)).json()).find((t) => t.type === 'page'); } catch {} }
  if (!target) { proc.kill('SIGKILL'); throw new Error('Chrome did not start'); }
  const ws = new WebSocket(target.webSocketDebuggerUrl); await new Promise((r, j) => { ws.onopen = r; ws.onerror = j; });
  let id = 0; const pending = new Map();
  ws.onmessage = (m) => {
    const msg = JSON.parse(m.data), p = pending.get(msg.id); if (!p) return;
    pending.delete(msg.id); if (msg.error) p.reject(new Error(JSON.stringify(msg.error))); else p.resolve(msg.result);
  };
  const send = (method, params = {}) => new Promise((resolve, reject) => { const k = ++id; pending.set(k, { resolve, reject }); ws.send(JSON.stringify({ id: k, method, params })); });
  const evaluate = async (expression, ms = 60000) => {
    const r = await Promise.race([send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true }), sleep(ms).then(() => ({ timeout: true }))]);
    if (r.timeout) throw new Error('timeout');
    if (r.exceptionDetails) throw new Error(JSON.stringify(r.exceptionDetails).slice(0, 300));
    return r.result.value;
  };
  chrome = { proc, send, evaluate, ws, runs: 0, pid: proc.pid };
  return chrome;
}
function stopChrome() { if (!chrome) return; try { chrome.ws.close(); } catch {} chrome.proc.kill('SIGKILL'); chrome = null; }
async function openPage() {
  await chrome.send('Page.navigate', { url: `${PAGE}/?verify=1&qa=1&mute=1&cutscenes=0&quality=low` });
  const t = Date.now();
  while (Date.now() - t < 300000) {
    await sleep(1000);
    try { if (await chrome.evaluate('!!window.ssxVerify && !!window.ssxQA && !!window.ssxQA.ui()?.ready', 5000)) { chrome.runs = 0; return; } } catch {}
    try { const e = await chrome.evaluate('document.body?.dataset?.loadError || ""', 5000); if (e) throw new Error('page load: ' + e); } catch (e) { if (/page load/.test(e.message)) throw e; }
  }
  throw new Error('page did not become ready');
}
// the Chrome process tree: CPU seconds and resident MB (ps)
function treeStats() {
  if (!chrome) return { cpu: 0, rss: 0 };
  const r = spawnSync('ps', ['-A', '-o', 'pid=,ppid=,rss=,time='], { encoding: 'utf8' }); if (r.status !== 0) return { cpu: 0, rss: 0 };
  const rows = r.stdout.trim().split('\n').map((l) => l.trim().split(/\s+/)).map(([pid, ppid, rss, time]) => ({ pid: +pid, ppid: +ppid, rss: +rss, time }));
  const tree = new Set([chrome.pid]); let grew = true;
  while (grew) { grew = false; for (const p of rows) if (!tree.has(p.pid) && tree.has(p.ppid)) { tree.add(p.pid); grew = true; } }
  const secs = (t) => t.split(/[-:]/).reverse().reduce((s, v, i) => s + (+v) * [1, 60, 3600, 86400][i], 0);
  const mine = rows.filter((p) => tree.has(p.pid));
  return { cpu: mine.reduce((s, p) => s + secs(p.time), 0), rss: mine.reduce((s, p) => s + p.rss, 0) / 1024 };
}

async function verifyOne(item, core) {
  if (chrome.runs >= RELOAD) await openPage();
  const before = treeStats(), t0 = Date.now(); let peak = before.rss;
  const sampler = setInterval(() => { peak = Math.max(peak, treeStats().rss); }, 2000);
  let res;
  try { res = await chrome.evaluate(`window.ssxVerify(${JSON.stringify(item.id)})`, TIMEOUT); }
  // a run that keeps the page past the timeout is not reproduced (a forged replay must not stay listed by stalling the verifier);
  // any other page failure is 'could not run' (tried again, at most three times per core)
  catch (e) { res = e.message === 'timeout' ? { ok: false, reason: `no result in ${TIMEOUT / 1000} s` } : { ok: null, reason: `page: ${e.message}` }; }
  finally { clearInterval(sampler); }
  chrome.runs++;
  const after = treeStats(); peak = Math.max(peak, after.rss);
  const stats = { wallS: +((Date.now() - t0) / 1000).toFixed(1), simMs: res?.ms ?? null, chromeCpuS: +(after.cpu - before.cpu).toFixed(1), peakMB: Math.round(peak) };
  const body = { id: item.id, ok: res?.ok ?? null, reason: res?.reason ?? 'no result', core: res?.core ?? core, value: res?.value ?? null };
  const out = await api('/mp/records/verifier/result', { method: 'POST', body: JSON.stringify(body) }).catch((e) => ({ error: e.message }));
  log(`run ${item.id} ${item.event} claim ${item.claim}${item.flagged ? ' (flagged)' : ''}: ${body.ok === null ? 'not run' : body.ok ? 'reproduced' : 'NOT reproduced'} (${body.reason})`,
    `-> ${out.action ?? out.error}`, JSON.stringify(stats));
  if (res?.ok === null || /no result in/.test(res?.reason ?? '')) await openPage().catch(() => {});   // start fresh for the next
  return { ...body, stats, action: out.action };
}

async function cycle() {
  const why = await busy(); if (why) { log(`skipped: ${why}`); return { done: 0, skipped: why }; }
  // the current build's core: from the page itself (the queue is per core: D7 (3) re-verifies runs verified on another one)
  let started = false, done = 0, results = [];
  try {
    const peek = await api('/mp/records/verifier/queue?limit=1'); if (!peek.items?.length && ONCE && !chrome) return { done: 0, results };
    if (!chrome) { await startChrome(); started = true; await openPage(); }
    const core = await chrome.evaluate('window.ssxVerifyCore ? window.ssxVerifyCore() : null', 60000);
    const { items = [] } = await api(`/mp/records/verifier/queue?core=${encodeURIComponent(core ?? '')}&limit=${PER_CYCLE}`);
    if (!items.length) { log('queue empty'); stopChrome(); return { done: 0, results }; }
    for (const item of items) {
      if (await busy()) break;
      results.push(await verifyOne(item, core)); done++;
    }
  } catch (e) { log('cycle failed:', e.message); stopChrome(); }
  if (started && ONCE) stopChrome();
  return { done, results };
}

process.on('SIGTERM', () => { stopChrome(); process.exit(0); });
process.on('SIGINT', () => { stopChrome(); process.exit(0); });
if (ONCE) {
  let total = [];
  for (let k = 0; k < 20; k++) { const r = await cycle(); total = total.concat(r.results || []); if (!r.done) break; }
  stopChrome();
  console.log('VERIFIER_RESULTS ' + JSON.stringify(total));
  process.exit(0);
}
for (;;) { const r = await cycle(); await sleep(r.done ? 1000 : INTERVAL); }
