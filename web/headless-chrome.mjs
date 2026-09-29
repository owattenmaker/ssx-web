// Headless Chrome + a private Vite server over web/, for the browser tests in npm test (test-shader-budget.mjs,
// test-gpu-recovery.mjs). Chrome for Testing from the Playwright cache, else Google Chrome; CHROME=/path overrides.
// startBrowser() -> null when no Chrome is installed (the caller skips). One browser per test; close() kills it.
import fs from 'node:fs'; import os from 'node:os'; import path from 'node:path'; import { spawn } from 'node:child_process';
const web = path.dirname(new URL(import.meta.url).pathname);
export const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

export function findChrome() {
  if (process.env.CHROME) return process.env.CHROME;
  const found = [];
  for (const dir of [path.join(os.homedir(), 'Library/Caches/ms-playwright'), path.join(os.homedir(), '.cache/ms-playwright')]) {
    let entries = []; try { entries = fs.readdirSync(dir).filter((d) => /^chromium-\d+$/.test(d)).sort((a, b) => +b.split('-')[1] - +a.split('-')[1]); } catch {}
    for (const d of entries) found.push(path.join(dir, d, 'chrome-mac-arm64/Google Chrome for Testing.app/Contents/MacOS/Google Chrome for Testing'), path.join(dir, d, 'chrome-mac/Chromium.app/Contents/MacOS/Chromium'), path.join(dir, d, 'chrome-linux/chrome'));
  }
  found.push('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome');
  return found.find((p) => fs.existsSync(p)) || null;
}

export async function startServer() {
  const { createServer } = await import('vite');
  const server = await createServer({ root: web, logLevel: 'error', server: { host: '127.0.0.1', port: 29000 + Math.floor(Math.random() * 2000), strictPort: false, hmr: false } });
  await server.listen();
  const origin = (server.resolvedUrls?.local?.[0] || `http://127.0.0.1:${server.config.server.port}/`).replace(/\/$/, '');
  return { origin, close: () => server.close() };
}

// init: page script run before every document (Page.addScriptToEvaluateOnNewDocument).
export async function startBrowser({ init = '', width = 1280, height = 960 } = {}) {
  const chrome = findChrome(); if (!chrome) return null;
  const port = 27000 + Math.floor(Math.random() * 2000), profile = fs.mkdtempSync(path.join(os.tmpdir(), 'ssx-headless-'));
  const proc = spawn(chrome, [`--remote-debugging-port=${port}`, `--user-data-dir=${profile}`, '--headless=new', '--mute-audio', '--no-first-run', '--no-default-browser-check',
    `--window-size=${width},${height}`, '--enable-unsafe-webgpu', '--autoplay-policy=no-user-gesture-required', '--disable-background-timer-throttling', '--disable-renderer-backgrounding', 'about:blank'], { stdio: 'ignore' });
  let page;
  for (let i = 0; i < 200 && !page; i++) { await sleep(150); try { page = (await (await fetch(`http://127.0.0.1:${port}/json`)).json()).find((t) => t.type === 'page'); } catch {} }
  if (!page) { proc.kill('SIGKILL'); throw Error('Chrome did not start'); }
  const ws = new WebSocket(page.webSocketDebuggerUrl); await new Promise((r, j) => { ws.onopen = r; ws.onerror = j; });
  let id = 0; const pending = new Map(), logs = [];
  ws.onmessage = (m) => {
    const msg = JSON.parse(m.data), p = pending.get(msg.id);
    if (p) { pending.delete(msg.id); msg.error ? p.reject(Error(JSON.stringify(msg.error))) : p.resolve(msg.result); return; }
    if (msg.method === 'Runtime.consoleAPICalled') logs.push(`${msg.params.type}: ${msg.params.args.map((a) => a.value ?? a.description).join(' ')}`);
    if (msg.method === 'Runtime.exceptionThrown') logs.push('exception: ' + (msg.params.exceptionDetails?.exception?.description ?? msg.params.exceptionDetails?.text));
  };
  const send = (method, params = {}) => new Promise((resolve, reject) => { const n = ++id; pending.set(n, { resolve, reject }); ws.send(JSON.stringify({ id: n, method, params })); });
  const evaluate = async (expression) => { const r = await send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true }); if (r.exceptionDetails) throw Error(JSON.stringify(r.exceptionDetails).slice(0, 600)); return r.result.value; };
  await send('Page.enable'); await send('Runtime.enable');
  await send('Emulation.setDeviceMetricsOverride', { width, height, deviceScaleFactor: 1, mobile: false });
  if (init) await send('Page.addScriptToEvaluateOnNewDocument', { source: init });
  return {
    send, evaluate, logs,
    async goto(url) { await send('Page.navigate', { url }); await sleep(800); },
    async waitFor(expr, ms) { const t = Date.now(); while (Date.now() - t < ms) { try { if (await evaluate(expr)) return; } catch {} await sleep(250); } throw Error(`timeout (${ms / 1000} s): ${expr}`); },
    async screenshot(selector = '#stage') {
      const rect = await evaluate(`(()=>{const r=document.querySelector(${JSON.stringify(selector)}).getBoundingClientRect();return {x:r.x,y:r.y,width:r.width,height:r.height}})()`);
      return (await send('Page.captureScreenshot', { format: 'png', clip: { ...rect, scale: 1 } })).data;
    },
    async hasWebGPU(origin) { await send('Page.navigate', { url: origin + '/manifest.webmanifest' }); await sleep(500); return evaluate('(async () => !!navigator.gpu && !!(await navigator.gpu.requestAdapter()))()'); },
    async close() { try { ws.close(); } catch {} proc.kill('SIGKILL'); await sleep(300); fs.rmSync(profile, { recursive: true, force: true }); },
  };
}
