// The browser verifier's verdict on replay files (docs/native.md "N2"): the reference the native verifier must reproduce. Each file
// goes to a private records server (web/server/mp-server.mjs, a temporary records folder); the game's own ?verify page (a private
// Vite server, headless Chrome from web/headless-chrome.mjs startBrowser: muted, mock keychain) re-simulates it with
// window.ssxVerify(id), as web/server/records-verifier.mjs drives it, here with ?simtrace=1 so every replayed tick's hash is kept.
// Prints one JSON line per file: {file, ok, reason, value, finishTick, simMs}; --traces DIR writes DIR/<n>.trace.json per file.
//   node native/tools/browser-verify.mjs [--traces DIR] FILE.ssxr...   (a raw SSXR file or the deflate-raw upload form)
// CORE_DIR=<a CORE_OUT build>: that core is served over web/runtime (web/headless-chrome.mjs).
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';
import { startBrowser, startServer } from '../../web/headless-chrome.mjs';
import { decodeReplayFile, encodeReplayFile } from '../../web/server/replay-file.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const web = path.join(root, 'web');
const args = process.argv.slice(2);
const tracesAt = args.indexOf('--traces');
const traces = tracesAt >= 0 ? args[tracesAt + 1] : null;
const files = args.filter((a, i) => a !== '--traces' && i !== tracesAt + 1);
if (!files.length) {
  console.error('usage: node native/tools/browser-verify.mjs [--traces DIR] FILE.ssxr...');
  process.exit(2);
}
if (traces) fs.mkdirSync(traces, { recursive: true });

function readReplay(file) {
  let bytes = fs.readFileSync(file);
  if (!(bytes[0] === 0x53 && bytes[1] === 0x53 && bytes[2] === 0x58 && bytes[3] === 0x52)) bytes = zlib.inflateRawSync(bytes);
  return decodeReplayFile(bytes);
}

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'ssx-native-oracle-'));
const token = `oracle-${Math.random().toString(16).slice(2)}-token`;
fs.writeFileSync(path.join(tmp, 'token'), token + '\n');
const port = 24000 + Math.floor(Math.random() * 2000);
const API = `http://127.0.0.1:${port}`;
const mp = spawn(process.execPath, ['server/mp-server.mjs', '--port', String(port), '--host', '127.0.0.1'], {
  cwd: web,
  env: {
    ...process.env,
    MP_RECORDS_DIR: path.join(tmp, 'records'),
    MP_RECORDS_ASSETS: path.join(web, 'public', 'assets'),
    MP_RECORDS_VERIFIER_TOKEN_FILE: path.join(tmp, 'token')
  },
  stdio: ['ignore', 'pipe', 'pipe']
});
let server = null;
let browser = null;
let code = 0;
try {
  await new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error('records server did not start')), 8000);
    mp.stdout.on('data', (d) => {
      if (String(d).includes('multiplayer server on')) {
        clearTimeout(timer);
        resolve();
      }
    });
  });
  process.env.MP_PORT = String(port);
  server = await startServer();
  browser = await startBrowser({ width: 640, height: 480 });
  if (!browser) {
    console.error('no Chrome (web/headless-chrome.mjs findChrome)');
    process.exit(77);
  }
  for (let k = 0; k < files.length; k++) {
    const { meta, pad } = readReplay(files[k]);
    // each file on a fresh page, under its own name (records.mjs cleanName: letters)
    const name = `NAT${String.fromCharCode(65 + Math.floor(k / 26))}${String.fromCharCode(65 + (k % 26))}`;
    const body = zlib.deflateRawSync(encodeReplayFile({ meta: { ...meta, name }, pad }));
    const sent = await (await fetch(`${API}/mp/records/submit`, { method: 'POST', body })).json();
    if (!sent.ok) {
      console.log(JSON.stringify({ file: files[k], ok: null, reason: `submit refused: ${JSON.stringify(sent)}` }));
      continue;
    }
    await browser.goto(`${server.origin}/?verify=1&qa=1&mute=1&cutscenes=0&quality=low&simtrace=1`);
    await browser.waitFor('!!window.ssxVerify && !!window.ssxQA && !!window.ssxQA.ui()?.ready', 300000);
    const r = await browser.evaluate(`(async()=>{const base=window.__simTrace.ticks.length;const res=await window.ssxVerify(${JSON.stringify(sent.id)});
      return {res,trace:window.__simTrace.ticks.slice(base)}})()`);
    const res = r.res ?? {};
    if (traces) fs.writeFileSync(path.join(traces, `${k}.trace.json`), JSON.stringify(r.trace));
    console.log(JSON.stringify({ file: files[k], ok: res.ok ?? null, reason: res.reason ?? 'no result', value: res.value ?? null,
      finishTick: res.finishTick ?? null, simMs: res.ms ?? null, core: res.core ?? null, traceTicks: r.trace.length }));
  }
} catch (e) {
  console.error(String(e?.stack || e));
  code = 1;
} finally {
  if (browser) await browser.close();
  if (server) await server.close();
  mp.kill();
  fs.rmSync(tmp, { recursive: true, force: true });
}
process.exit(code);
