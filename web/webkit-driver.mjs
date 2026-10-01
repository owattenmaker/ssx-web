// Node side of web/webkit-driver.swift: the macOS system WebKit (Safari's engine and WebGPU) for browser tests.
// startWebKit() compiles the driver once (swiftc, cached under the OS temp dir by source hash) and returns
// { goto, eval, waitFor, shot, close }, or null when unavailable (not macOS, no swiftc, SSX_NO_WEBKIT=1).
import { spawn, spawnSync } from 'node:child_process';
import crypto from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import readline from 'node:readline';

const SOURCE = new URL('./webkit-driver.swift', import.meta.url).pathname;
export const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

export function webKitDriverBinary() {
  if (process.platform !== 'darwin' || process.env.SSX_NO_WEBKIT === '1') return null;
  const hash = crypto.createHash('sha256').update(fs.readFileSync(SOURCE)).digest('hex').slice(0, 12);
  // own directory: WebKit keeps per-app data in <tmp>/<executable name>/, which must not collide with the binary
  const dir = path.join(os.tmpdir(), `ssx-webkit-qa-${hash}`), bin = path.join(dir, 'ssx-webkit-qa');
  if (fs.existsSync(bin)) return bin;
  fs.mkdirSync(dir, { recursive: true });
  const r = spawnSync('xcrun', ['swiftc', '-O', '-o', bin + '.tmp', SOURCE], { encoding: 'utf8' });
  if (r.status !== 0) { console.warn('webkit-driver: swiftc failed, WebKit checks skipped\n' + (r.stderr || r.error || '').toString().slice(0, 400)); return null; }
  fs.renameSync(bin + '.tmp', bin);
  return bin;
}

// store: a UUID string, the driver's own persistent data store (repeat-visit runs); trustLocal: accept https://127.0.0.1's certificate.
export async function startWebKit({ width = 1280, height = 960, offscreen = false, store = null, trustLocal = false } = {}) {
  const bin = webKitDriverBinary(); if (!bin) return null;
  const proc = spawn(bin, ['about:blank', String(width), String(height), ...(offscreen ? ['--offscreen'] : []), ...(store ? ['--store', store] : []), ...(trustLocal ? ['--trust-local'] : [])], { stdio: ['pipe', 'pipe', 'inherit'] });
  // WebKit stops requestAnimationFrame while the display sleeps: keep it awake for the driver's lifetime.
  if (process.platform === 'darwin' && proc.pid) { try { spawn('caffeinate', ['-d', '-u', '-w', String(proc.pid)], { stdio: 'ignore', detached: true }).unref(); } catch {} }
  const rl = readline.createInterface({ input: proc.stdout }), waiting = [], events = [];
  let exited = false; proc.on('exit', () => { exited = true; while (waiting.length) waiting.shift()({ ok: false, error: 'webkit-driver exited' }); });
  rl.on('line', (line) => { let m; try { m = JSON.parse(line); } catch { events.push(line); return; } if (m.event) { events.push(m.event); return; } waiting.shift()?.(m); });
  const cmd = (o) => new Promise((resolve) => { if (exited) { resolve({ ok: false, error: 'webkit-driver exited' }); return; } waiting.push(resolve); proc.stdin.write(JSON.stringify(o) + '\n'); });
  const api = {
    events,
    // ?mute=1: WKWebView doesn't set navigator.webdriver, so tell the game to stay silent (web/audio-engine.js testMuted).
    async goto(url) { const u = new URL(url); if (!u.searchParams.has('mute')) u.searchParams.set('mute', '1'); await cmd({ goto: u.href }); await sleep(500); },
    /** Evaluate an expression (awaited) in the page; the value comes back through JSON. */
    async eval(expression) { const r = await cmd({ js: `return (${expression})` }); if (!r.ok) throw new Error(r.error); return r.value == null ? r.value : JSON.parse(r.value); },
    async waitFor(expression, ms = 60000) { const t = Date.now(); while (Date.now() - t < ms) { try { if (await api.eval(expression)) return true; } catch {} await sleep(250); } throw new Error('timeout waiting for ' + expression); },
    async shot(file, selector = 'body') {
      const rect = await api.eval(`(()=>{const r=document.querySelector(${JSON.stringify(selector)}).getBoundingClientRect();return [r.x,r.y,r.width,r.height]})()`);
      const r = await cmd({ shot: file, rect }); if (!r.ok) throw new Error(r.error); return r;
    },
    async close() { try { proc.stdin.write('{"quit":1}\n'); } catch {} await sleep(200); if (!exited) proc.kill(); },
  };
  return api;
}
