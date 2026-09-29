// Headless Firefox (WebDriver BiDi) renderer for the browser port: loads the dev server with
// ?qa, drives window.ssxQA deterministically and saves a PNG of the game canvas.
// Usage: node tools/web_render.mjs OUT.png [--ticks N] [--url URL] [--hud] [--backend webgl]
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const args = process.argv.slice(2);
const out = args.find((a) => !a.startsWith('--'));
const opt = (name, fallback) => (args.includes(name) ? args[args.indexOf(name) + 1] : fallback);
const ticks = Number(opt('--ticks', '0'));
const backend = opt('--backend', '');
const base = opt('--url', 'http://127.0.0.1:5173/');
// --params 'course=BRA2' appends query parameters (tools/locations.py courses; --no-glide drops qaGlide for the event start).
const url = `${base}?qa=1${args.includes('--no-glide') ? '' : '&qaGlide=1'}${backend ? '&backend=' + backend : ''}${opt('--params', '') ? '&' + opt('--params', '') : ''}`;
const port = 9300 + Math.floor(Math.random() * 400);
fs.mkdirSync(new URL('../local/firefox-profiles/', import.meta.url), { recursive: true });
const profile = fs.mkdtempSync(new URL('../local/firefox-profiles/', import.meta.url).pathname + 'p-');
fs.writeFileSync(path.join(profile, 'user.js'), [
  'user_pref("dom.webgpu.enabled", true);', 'user_pref("gfx.webgpu.ignore-blocklist", true);',
  'user_pref("remote.enabled", true);', 'user_pref("browser.shell.checkDefaultBrowser", false);',
  'user_pref("toolkit.telemetry.reportingpolicy.firstRun", false);', 'user_pref("datareporting.policy.dataSubmissionEnabled", false);',
].join('\n'));
const ff = spawn('/Applications/Firefox.app/Contents/MacOS/firefox', ['--headless', '--no-remote', '--profile', profile, `--remote-debugging-port=${port}`, '--window-size=1280,960', 'about:blank'], { stdio: ['ignore', 'ignore', 'pipe'] });
let stderr = ''; ff.stderr.on('data', (d) => { stderr += d; });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let ws;
for (let i = 0; i < 100 && !ws; i++) {
  await sleep(200);
  try { ws = await new Promise((resolve, reject) => { const s = new WebSocket(`ws://127.0.0.1:${port}/session`); s.onopen = () => resolve(s); s.onerror = reject; }); } catch { ws = null; }
}
if (!ws) { ff.kill(); throw new Error('Firefox BiDi did not start: ' + stderr.slice(-500)); }
let id = 0; const pending = new Map();
ws.onmessage = (m) => { const msg = JSON.parse(m.data); if (msg.id && pending.has(msg.id)) { const { resolve, reject } = pending.get(msg.id); pending.delete(msg.id); msg.type === 'error' ? reject(new Error(msg.error + ': ' + msg.message)) : resolve(msg.result); } };
const send = (method, params = {}) => new Promise((resolve, reject) => { const n = ++id; pending.set(n, { resolve, reject }); ws.send(JSON.stringify({ id: n, method, params })); });
try {
  await send('session.new', { capabilities: {} });
  const tree = await send('browsingContext.getTree', {});
  const context = tree.contexts[0].context;
  await send('browsingContext.setViewport', { context, viewport: { width: 1280, height: 960 } }).catch(() => {});
  await send('browsingContext.navigate', { context, url, wait: 'complete' });
  const evaluate = async (expression) => { const r = await send('script.evaluate', { expression, target: { context }, awaitPromise: true }); if (r.type === 'exception') throw new Error(JSON.stringify(r.exceptionDetails).slice(0, 800)); return r.result?.value; };
  for (let i = 0; i < 300; i++) { if (await evaluate('!!window.ssxQA || !!document.body.dataset.loadError')) break; await sleep(200); }
  const err = await evaluate('document.body.dataset.loadError || ""');
  if (err) throw new Error('Page load failed: ' + err);
  const backendName = await evaluate('(async()=>{window.ssxQA.start();return window.demoState?.backend||""})()');
  if (ticks) await evaluate(`JSON.stringify(window.ssxQA.advance(${ticks}))`);
  await evaluate(`window.ssxQA.hud(${args.includes('--hud')})`);
  await sleep(1500);
  const shot = await send('browsingContext.captureScreenshot', { context, clip: { type: 'element', element: await send('script.evaluate', { expression: 'document.querySelector("#stage")', target: { context }, awaitPromise: false }).then((r) => r.result) } });
  fs.writeFileSync(out, Buffer.from(shot.data, 'base64'));
  console.log(JSON.stringify({ out, ticks, backend: backendName || (await evaluate('window.demoState?.backend||""')) }));
} finally {
  try { ws.close(); } catch {}
  ff.kill();
  fs.rmSync(profile, { recursive: true, force: true });
}
