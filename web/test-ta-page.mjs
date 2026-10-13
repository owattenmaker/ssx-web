// Time attack on the page (pv onlineTimeAttack: web/net/ta-game.js, the career-ui play / finish hooks; docs/big-multiplayer.md
// TA-3) against a local mp-server with sessions on: the page creates a Snow Jam session, a node player joins it; the page loads the
// event (its objectives card: 'ta-loaded'), an attempt before the window opens is refused, then a solo run (no computer riders, the
// maxed attribute bytes) is ridden to the finish and uploaded as the page player's best; the verifier verifies it; a second attempt
// started and given up ends without an upload; after the deadline Continue is refused; the podium shows the page player; the vote
// picks Metro City and the page loads it for the next window. With pv ghostRiders: the node player streams ghost poses, the page draws
// its ghost from the server's bundles, and the page's own poses reach the node player. Skipped without Chrome / WebGPU / the game data.
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { findChrome, startBrowser, startServer, sleep } from './headless-chrome.mjs';
import { encodePose, decodeBundle, freeBones, POSE, FLAG_RIDING } from './server/ghost-packet.mjs';

const web = path.dirname(fileURLToPath(import.meta.url));
const assets = path.join(web, 'public', 'assets');
if (!fs.existsSync(path.join(assets, 'CAREER', 'career.json'))) {
  console.log('time attack page check SKIPPED: no game data');
  process.exit(0);
}
const browser = await startBrowser({ width: 640, height: 480 });
if (!browser) {
  console.log('time attack page check SKIPPED: no Chrome');
  process.exit(0);
}
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'ssx-ta-page-'));
const token = 'test-' + Math.random().toString(16).slice(2) + '-verifier-token';
fs.writeFileSync(path.join(tmp, 'token'), token + '\n');
const port = 24000 + Math.floor(Math.random() * 2000);
const API = `http://127.0.0.1:${port}`;
// the page rides as fast as it goes (wallSlackMs off); short podium and vote
const options = { wallSlackMs: 1e9, goDelayMs: 500, minLimitMs: 1000, podiumMs: 3000, voteMs: 4000, voteResultMs: 500, verifyMaxMs: 60000, overtimeMaxMs: 5000 };
const mp = spawn(process.execPath, ['server/mp-server.mjs', '--port', String(port), '--host', '127.0.0.1'], { cwd: web,
  env: { ...process.env, MP_SESSIONS: '1', MP_SESSION_OPTIONS: JSON.stringify(options), MP_RECORDS_ASSETS: assets, MP_RECORDS_VERIFIER_TOKEN_FILE: path.join(tmp, 'token') },
  stdio: ['ignore', 'pipe', 'pipe'] });
let mpOut = '';
mp.stdout.on('data', (d) => { mpOut += d; });
mp.stderr.on('data', (d) => { mpOut += d; });
for (let i = 0; i < 160 && !/multiplayer server on/.test(mpOut); i++) await sleep(50);
assert.match(mpOut, /multiplayer server on/, mpOut);
process.env.MP_PORT = String(port);
const server = await startServer();
const E = (js) => browser.evaluate(js);
const TA = 'ssxQA.ui().timeAttack';
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
      if (typeof e.data !== 'string') return;
      const m = JSON.parse(e.data);
      box.messages.push(m);
      for (const w of [...box.waiters]) if (w.t === m.t && w.pred(m)) {
        box.waiters.splice(box.waiters.indexOf(w), 1);
        w.res(m);
      }
    };
    ws.onopen = () => {
      box.send({ t: 'hello', name, rider: 'psymon', pkg: 'RIDER_PSYMON', version: 2, token: box.token });
      box.pinger = setInterval(() => box.send({ t: 'ping', c: Date.now() }), 2000);
      ws.addEventListener('close', () => clearInterval(box.pinger));
      resolve(box);
    };
  });
}
const run = async (pad, limit) => {
  let done = -1;
  for (let i = 0; i < limit && done < 0; i += 500) done = await E(`window.__run(${i},500,${pad})`);
  return done;
};

let browserOpen = true;
let bot = null;
try {
  if (!(await browser.hasWebGPU(server.origin))) {
    console.log('time attack page check SKIPPED: no WebGPU');
    process.exit(0);
  }
  await browser.goto(`${server.origin}/?qa=1&course=ARA1&rider=zoe&cutscenes=0&quality=low&mute=1&pv=onlineTimeAttack,ghostRiders`);
  await browser.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
  assert.ok(await E(`!!${TA}`), 'pv onlineTimeAttack installs ui.timeAttack');
  await E(`(async()=>{await ssxQA.ui().mpUI.session.connect();return 1})()`);
  await E(`(()=>{${TA}.create({name:'Page',limitMs:150000,event:'0:ARA1'});return 1})()`);
  await browser.waitFor(`${TA}.active`, 30000);
  const sid = await E(`${TA}.state.session.id`);
  bot = await player('BOT');
  await bot.wait('welcome');
  bot.send({ t: 'ta-join', session: sid });
  await bot.wait('ta-joined');
  bot.send({ t: 'ta-loaded', window: 1 });
  // the page loads the event: its objectives card, then 'ta-loaded'
  await browser.waitFor(`document.getElementById('stage')?.dataset.screen === 'ctm-objectives'`, 300000);
  await browser.waitFor(`${TA}.state.loadedWindow === 1`, 10000);
  // the window opens with the quorum; an attempt before its start is refused (goDelayMs 500)
  await browser.waitFor(`${TA}.state.session.phase === 'window'`, 20000);
  const early = await E(`(()=>{const s=${TA}.state.session;return Date.now()+0<s.window.startAt?${TA}.attemptStart():null})()`);
  if (early !== null) assert.equal(early, false, 'no attempt before the window starts');
  await browser.waitFor(`${TA}.windowOpen`, 10000);
  await E(`(()=>{const raf=window.requestAnimationFrame.bind(window),at=performance.now();window.requestAnimationFrame=cb=>raf(()=>cb(at));
    window.__pads={tuck:(t)=>{const p=new Array(24).fill(0);p[22]=1;const s=Math.sin(t/90);if(s>0.6)p[21]=0.3;else if(s<-0.6)p[20]=0.3;return p;}};
    window.__run=(from,n,pad)=>{for(let t=from;t<from+n;t++){ssxQA.advance(1,window.__pads.tuck(t));if(/results|records|award/.test(ssxQA.ui().screen))return t+1;}return -1};
    ssxQA.ui().careerUI.play();return 1})()`);
  assert.equal(await E(`${TA}.state.attempt`), 1, 'Continue started attempt 1');
  // the node player rides too: its ghost poses (Psymon's rig, standing still) at 20 Hz
  const rig = JSON.parse(fs.readFileSync(path.join(assets, 'RIDER_PSYMON', 'rider.json'), 'utf8'));
  const ghostPose = new Float32Array(rig.bones.length * 7);
  for (let b = 0; b < rig.bones.length; b++) ghostPose[b * 7 + 6] = 1;
  const bin = [];
  bot.ws.binaryType = 'arraybuffer';
  bot.ws.addEventListener('message', (e) => { if (typeof e.data !== 'string') bin.push(new Uint8Array(e.data)); });
  const ghostTimer = setInterval(() => bot.ws.send(encodePose({ flags: FLAG_RIDING, serial: 256, time: Date.now(), pose: ghostPose, scale: [1, 1, 1], free: freeBones(rig.bones) })), 50);
  await sleep(300);
  // the page draws the node player's ghost, and its own poses reach the node player
  await browser.waitFor(`${TA}.ghosts.stats.bundles > 0 && ${TA}.ghosts.stats.drawn > 0 && ${TA}.ghosts.stats.sent > 5`, 30000).catch(() => {});
  const ghostStats = await E(`${TA}.ghosts.stats`);
  if (!(ghostStats.drawn >= 1)) {
    console.log('ghost layer', JSON.stringify(await E(`${TA}.ghosts.inspect()`)));
    console.log(browser.logs.filter((l) => /ta:|ghost/i.test(l)).slice(-5).join('\n'));
  }
  assert.ok(ghostStats.drawn >= 1, `the page draws the ghost ${JSON.stringify(ghostStats)}`);
  // the page's own poses (sent while it rides: the run below ticks it)
  assert.ok((await run(0, 22000)) > 0, 'the attempt finishes');
  assert.ok((await E(`${TA}.ghosts.stats.sent`)) > 0, 'the page sends its ghost');
  const pageIndex = await E(`${TA}.state.roster.get(ssxQA.ui().mpUI.session.client.state.id)?.index`);
  const fromPage = bin.map(decodeBundle).filter(Boolean).flatMap((x) => x.entries).filter((x) => x.player === pageIndex && x.kind === POSE);
  assert.ok(fromPage.length > 0, `the node player received the page's poses (${bin.length} bundles)`);
  assert.equal(fromPage[0].value.bones, 27, "Zoe's 27 bones");
  clearInterval(ghostTimer);
  assert.equal(await E('ssxQA.ui().screen'), 'ctm-results', 'the results, no records screen in a session');
  await browser.waitFor(`${TA}.state.upload && ${TA}.state.upload.status !== 'sending'`, 30000);
  const up = await E(`${TA}.state.upload`);
  assert.equal(up.status, 'kept', JSON.stringify(up));
  // the run's replay as uploaded: no computer riders, the maxed attribute bytes
  const queue = await (await fetch(`${API}/mp/session/verifier/queue`, { headers: { 'x-ssx-verifier': token } })).json();
  assert.equal(queue.items.length, 1);
  assert.equal(queue.items[0].claim, up.value);
  const replay = await E(`(async()=>{const f=await ssxQA.ui().cb.onlineReplayFile({event:'0:ARA1',mode:0,course:'ARA1',name:'X',claim:{ticks:1}});
    return {lineup:f.meta.lineup,attributes:f.meta.rider.attributes}})()`);
  assert.equal(replay.lineup.enabled, false);
  assert.deepEqual(replay.lineup.values, []);
  assert.deepEqual(replay.attributes, [55, 55, 55, 55, 55, 55, 55]);
  // a second attempt (the results' Restart -> the card -> Continue), given up: no upload
  await E(`(()=>{const cu=ssxQA.ui().careerUI;cu.play();return 1})()`);
  assert.equal(await E(`${TA}.state.attempt`), 2);
  for (let k = 0; k < 30; k++) await E('(()=>{ssxQA.advance(10,new Array(24).fill(0));return 1})()');
  await E(`(()=>{ssxQA.ui().cb.giveUp();return 1})()`);
  for (let k = 0; k < 80 && !/results/.test(await E('ssxQA.ui().screen')); k++) await E('(()=>{ssxQA.advance(10,new Array(24).fill(0));return 1})()');
  assert.equal(await E(`${TA}.state.attemptOpen`), false, 'the given-up attempt ended');
  assert.equal(await E(`${TA}.state.upload.attempt`), 1, 'no upload for it');
  // the verifier verifies the page's run
  const v = spawn(process.execPath, ['server/records-verifier.mjs', '--once', '--page', server.origin, '--api', API, '--token-file', path.join(tmp, 'token'),
    '--profile', path.join(tmp, 'profile'), '--max-load', '1000', '--session-max-load', '1000', '--chrome', findChrome()], { cwd: web, stdio: ['ignore', 'pipe', 'pipe'] });
  let out = '';
  v.stdout.on('data', (d) => { out += d; });
  v.stderr.on('data', (d) => { out += d; });
  assert.equal(await new Promise((r) => v.on('exit', r)), 0, out.slice(-600));
  const results = JSON.parse(out.split('VERIFIER_RESULTS ')[1] || '[]');
  assert.equal(results[0]?.ok, true, JSON.stringify(results));
  await browser.waitFor(`${TA}.state.you && ${TA}.state.you.status === 'verified'`, 10000);
  // the deadline: shorten nothing, the host sets no limit change; wait for the window's end, then Continue is refused
  const endAt = await E(`${TA}.state.session.window.endAt`);
  await sleep(Math.max(0, endAt - Date.now()) + 1500);
  assert.equal(await E(`(()=>{const ok=${TA}.attemptStart();return ok})()`), false, 'no attempt after the deadline');
  // the podium: the page's player (verified); the bot never finished
  await browser.waitFor(`!!${TA}.state.results`, 120000);
  const podium = await E(`${TA}.state.results.podium.map(r=>r.name)`);
  assert.equal(podium.length, 1, JSON.stringify(podium));
  // the vote: everyone picks Metro City's candidate if drawn, else the first; the page loads the voted event
  await browser.waitFor(`${TA}.state.session.phase === 'vote'`, 30000);
  const candidates = await E(`${TA}.state.session.candidates`);
  const pick = Math.max(0, candidates.indexOf('0:BRA2'));
  await E(`${TA}.vote(${pick})`);
  bot.send({ t: 'ta-vote', choice: pick });
  await browser.waitFor(`${TA}.state.session.window.id === 2`, 30000);
  assert.equal(await E(`${TA}.state.session.event`), candidates[pick]);
  await browser.waitFor(`${TA}.state.loadedWindow === 2`, 300000);
  const code = candidates[pick].split(':')[1];
  console.log(`next window: ${candidates[pick]} loaded on the page (${await E('ssxQA.ui().course?.code ?? ""')})`);
  assert.equal(await E(`ssxQA.ui().careerUI.career.active.ev && ssxQA.ui().courses.find(c=>c.code==='${code}')?.code`), code);
  // leaving: the solo lineup and the attribute override go
  await E(`(()=>{${TA}.leave();return 1})()`);
  await browser.waitFor(`!${TA}.active`, 10000);
  console.log(`attempt 1: ${up.value} race ticks kept and verified (${JSON.stringify(results[0].stats)}); podium ${podium}`);
} catch (e) {
  failures.push(String(e?.stack || e));
} finally {
  if (browserOpen) await browser.close();
  bot?.ws.close();
  await server.close();
  mp.kill();
  fs.rmSync(tmp, { recursive: true, force: true });
}
if (failures.length) {
  console.error('time attack page check FAILED:\n' + failures.join('\n'));
  process.exit(1);
}
console.log('time attack page check OK');
process.exit(0);
