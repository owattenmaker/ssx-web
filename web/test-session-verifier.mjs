// The verifier's session lane end to end (web/server/records-verifier.mjs, web/server/sessions.mjs, the page's ?verify path for
// session runs in web/online-replay.js; docs/big-multiplayer.md TA-2): a local mp-server with sessions and records on, two session
// players (node WebSocket clients) in a Snow Jam time-attack window, a genuine solo run ridden in the page (no computer riders, the
// maxed attribute bytes) uploaded as player 1's best, a faster claim on the same replay as player 1's next best, a forged replay (no
// input) under the genuine claim as player 2's, and the genuine run also on the Snow Jam records board. One verifier pass (--once)
// verifies the genuine session run, pulls both forgeries and still verifies the records board's run (the records lane keeps its
// turn). Prints each verification's cost. Skipped without Chrome / WebGPU / the game data.
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';
import { findChrome, startBrowser, startServer, sleep } from './headless-chrome.mjs';
import { encodeReplayFile } from './server/replay-file.mjs';

const web = path.dirname(fileURLToPath(import.meta.url));
const assets = path.join(web, 'public', 'assets');
if (!fs.existsSync(path.join(assets, 'CAREER', 'career.json'))) {
  console.log('session verifier check SKIPPED: no game data');
  process.exit(0);
}
const browser = await startBrowser({ width: 640, height: 480 });
if (!browser) {
  console.log('session verifier check SKIPPED: no Chrome');
  process.exit(0);
}
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'ssx-session-verifier-'));
const token = 'test-' + Math.random().toString(16).slice(2) + '-verifier-token';
fs.writeFileSync(path.join(tmp, 'token'), token + '\n');
const port = 26000 + Math.floor(Math.random() * 2000);
const API = `http://127.0.0.1:${port}`;
// the run is ridden as fast as the page goes, so the wall-time check is off for this test (wallSlackMs)
const options = { wallSlackMs: 1e9, goDelayMs: 100, minLimitMs: 1000 };
const mp = spawn(process.execPath, ['server/mp-server.mjs', '--port', String(port), '--host', '127.0.0.1'], { cwd: web,
  env: { ...process.env, MP_SESSIONS: '1', MP_SESSION_OPTIONS: JSON.stringify(options), MP_RECORDS_DIR: path.join(tmp, 'records'),
    MP_RECORDS_ASSETS: assets, MP_RECORDS_VERIFIER_TOKEN_FILE: path.join(tmp, 'token') }, stdio: ['ignore', 'pipe', 'pipe'] });
let mpOut = '';
mp.stdout.on('data', (d) => { mpOut += d; });
mp.stderr.on('data', (d) => { mpOut += d; });
for (let i = 0; i < 160 && !/multiplayer server on/.test(mpOut); i++) await sleep(50);
assert.match(mpOut, /multiplayer server on/, mpOut);
process.env.MP_PORT = String(port);
const server = await startServer();
const E = (js) => browser.evaluate(js);
const failures = [];

function player(name) {
  return new Promise((resolve) => {
    const ws = new WebSocket(`ws://127.0.0.1:${port}/mp`);
    const box = { ws, messages: [], waiters: [], token: `tok-${name}-${Math.random()}` };
    box.wait = (t, pred = () => true) => new Promise((res) => {
      const hit = box.messages.find((m) => m.t === t && pred(m));
      if (hit) return res(hit);
      box.waiters.push({ t, pred, res });
    });
    box.send = (m) => ws.send(JSON.stringify(m));
    ws.onmessage = (e) => {
      const m = JSON.parse(e.data);
      box.messages.push(m);
      for (const w of [...box.waiters]) if (w.t === m.t && w.pred(m)) {
        box.waiters.splice(box.waiters.indexOf(w), 1);
        w.res(m);
      }
    };
    ws.onopen = () => {
      box.send({ t: 'hello', name, rider: 'zoe', version: 2, token: box.token });
      // the client's clock-sync pings (mp-client.js: every 2 s); a socket silent for 10 s is closed by the server
      box.pinger = setInterval(() => box.send({ t: 'ping', c: Date.now() }), 2000);
      ws.addEventListener('close', () => clearInterval(box.pinger));
      resolve(box);
    };
  });
}
const upload = async (p, sessionId, attempt, meta, pad) => {
  const body = zlib.deflateRawSync(encodeReplayFile({ meta, pad }));
  const r = await fetch(`${API}/mp/session/run?session=${sessionId}&window=1&attempt=${attempt}`, { method: 'POST', headers: { 'x-ssx-session': p.token }, body });
  return r.json();
};

let browserOpen = true;
let p1 = null;
let p2 = null;
try {
  if (!(await browser.hasWebGPU(server.origin))) {
    console.log('session verifier check SKIPPED: no WebGPU');
    process.exit(0);
  }
  // ---- the session: two players in a Snow Jam window ----
  p1 = await player('ANNA');
  p2 = await player('BEN');
  await p1.wait('welcome');
  await p2.wait('welcome');
  p1.send({ t: 'ta-create', name: 'Verify', limitMs: 30 * 60000, event: '0:ARA1' });
  const sid = (await p1.wait('ta-joined')).session.id;
  p2.send({ t: 'ta-join', session: sid });
  await p2.wait('ta-joined');
  p1.send({ t: 'ta-loaded', window: 1 });
  p2.send({ t: 'ta-loaded', window: 1 });
  const open = await p1.wait('ta-state', (m) => m.session.phase === 'window');
  await sleep(Math.max(0, open.session.window.startAt - Date.now()) + 50);
  p1.send({ t: 'ta-attempt', window: 1, n: 1 });
  p1.send({ t: 'ta-attempt', window: 1, n: 2 });
  p2.send({ t: 'ta-attempt', window: 1, n: 1 });

  // ---- a genuine solo run in the page: no computer riders, attributes 55 x 7 ----
  await browser.goto(`${server.origin}/?qa=1&course=ARA1&rider=zoe&cutscenes=0&quality=low&mute=1`);
  await browser.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
  await E(`(()=>{const ui=ssxQA.ui();if(!ui.cb.soloLineupNext())throw new Error('no ai race');const e=ui.courses.find(x=>x.code==='ARA1'&&!x.rivalMode);ui.startSingleEvent(e);return 1})()`);
  await browser.waitFor(`document.getElementById('stage')?.dataset.screen === 'ctm-objectives'`, 300000);
  await E(`(()=>{const raf=window.requestAnimationFrame.bind(window),at=performance.now();window.requestAnimationFrame=cb=>raf(()=>cb(at));
    const ui=ssxQA.ui();ui.cb.attributeOverride([55,55,55,55,55,55,55]);ui.careerUI.play();
    window.__pad=(t)=>{const p=new Array(24).fill(0);p[22]=1;const s=Math.sin(t/90);if(s>0.6)p[21]=0.3;else if(s<-0.6)p[20]=0.3;return p;};
    window.__run=(from,n)=>{for(let t=from;t<from+n;t++){ssxQA.advance(1,window.__pad(t));if(/results|records|award/.test(ssxQA.ui().screen))return t+1;}return -1};return 1})()`);
  await sleep(300);
  let done = -1;
  for (let i = 0; i < 22000 && done < 0; i += 500) done = await E(`window.__run(${i},500)`);
  assert.ok(done > 0, 'the solo run finishes');
  const run = await E(`(async()=>{const ui=ssxQA.ui(),info=ui.cb.onlineRun();const ticks=Math.round(info.finish.ticks);
    const f=await ui.cb.onlineReplayFile({event:'0:ARA1',mode:0,course:'ARA1',name:'OWEN',claim:{ticks}});
    return {meta:f.meta,pad:Array.from(f.pad),ticks,ai:ssxQA.ui().cb.onlineRun().available}})()`);
  await browser.close();
  browserOpen = false;
  assert.equal(run.meta.lineup?.enabled, false, `a solo lineup: ${JSON.stringify(run.meta.lineup)}`);
  assert.deepEqual(run.meta.lineup.values, []);
  assert.deepEqual(run.meta.rider.attributes, [55, 55, 55, 55, 55, 55, 55]);
  const pad = new Uint8Array(run.pad);
  if (process.env.KEEP_RUN) fs.writeFileSync(process.env.KEEP_RUN, JSON.stringify(run));
  console.log(`solo Snow Jam run: ${run.ticks} race ticks, ${run.meta.ticks} recorded, pad ${pad.length} B`);

  // ---- uploads: the genuine run, a faster claim on it, a forged replay ----
  const genuine = await upload(p1, sid, 1, run.meta, pad);
  assert.equal(genuine.kept, true, JSON.stringify(genuine));
  const faster = await upload(p1, sid, 2, { ...run.meta, claim: { ticks: run.ticks - 150 } }, pad);
  assert.equal(faster.kept, true, JSON.stringify(faster));
  const forged = await upload(p2, sid, 1, run.meta, new Uint8Array(0));
  assert.equal(forged.kept, true, JSON.stringify(forged));
  // the same run on the records board (the records lane must still get its turn)
  const rec = await (await fetch(`${API}/mp/records/submit`, { method: 'POST', body: zlib.deflateRawSync(encodeReplayFile({ meta: run.meta, pad })) })).json();
  assert.ok(rec.ok, JSON.stringify(rec));

  // ---- one verifier pass ----
  const v = spawn(process.execPath, ['server/records-verifier.mjs', '--once', '--page', server.origin, '--api', API, '--token-file', path.join(tmp, 'token'),
    '--profile', path.join(tmp, 'profile'), '--max-load', '1000', '--session-max-load', '1000', '--timeout', '300', '--reload', '5', '--chrome', findChrome()],
    { cwd: web, stdio: ['ignore', 'pipe', 'pipe'] });
  let out = '';
  v.stdout.on('data', (d) => {
    out += d;
    process.stdout.write(d);
  });
  v.stderr.on('data', (d) => { out += d; });
  const code = await new Promise((r) => v.on('exit', r));
  assert.equal(code, 0, `verifier exit ${code}: ${out.slice(-800)}`);
  const results = JSON.parse(out.split('VERIFIER_RESULTS ')[1] || '[]');
  const by = (id) => results.find((r) => r.id === id);
  assert.equal(by(genuine.run)?.ok, true, `the genuine session run: ${JSON.stringify(by(genuine.run))}`);
  assert.equal(by(genuine.run)?.action, 'verified');
  assert.equal(by(faster.run)?.ok, false, `the faster claim: ${JSON.stringify(by(faster.run))}`);
  assert.equal(by(faster.run)?.action, 'pulled');
  assert.equal(by(forged.run)?.ok, false, `the forged replay: ${JSON.stringify(by(forged.run))}`);
  assert.equal(by(rec.id)?.ok, true, `the records board's run: ${JSON.stringify(by(rec.id))}`);
  assert.equal(by(rec.id)?.lane, 'records');
  // the standings: ANNA's genuine run verified, BEN's forged run gone
  const st = await p1.wait('ta-standings', (m) => m.rows.length === 1 && m.rows[0].status === 'verified');
  assert.equal(st.rows[0].name, 'ANNA');
  assert.equal(st.rows[0].value, run.ticks);
  for (const r of results) console.log(`verify ${r.lane} ${r.id}: ${r.ok ? 'reproduced' : 'not reproduced'} (${r.reason}) -> ${r.action}; ${JSON.stringify(r.stats)}`);
} catch (e) {
  failures.push(String(e?.stack || e));
} finally {
  if (browserOpen) await browser.close();
  p1?.ws.close();
  p2?.ws.close();
  await server.close();
  mp.kill();
  fs.rmSync(tmp, { recursive: true, force: true });
}
if (failures.length) {
  console.error('session verifier check FAILED:\n' + failures.join('\n'));
  process.exit(1);
}
console.log('session verifier check OK');
process.exit(0);
