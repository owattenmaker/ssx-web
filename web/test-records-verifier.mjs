// The online records' verifier end to end (web/server/records-verifier.mjs, the page's ?verify path in web/online-replay.js,
// docs/online-records.md "The verifier"): a local records server with the verifier token, a genuine Single Event Snow Jam run
// uploaded through the page (Save Records), two forgeries uploaded beside it (the genuine replay with a faster claim; a forged
// replay, a neutral pad stream under the genuine claim), all three listed at once (D6); then the verifier (headless Chrome, the
// game's own page) re-simulates them: the genuine run is verified, both forgeries are pulled. Prints the CPU and memory each
// verification cost. All five runs are verified on one page (--reload 5), each exact. Skipped without Chrome / WebGPU / the game data.
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';
import { findChrome, startBrowser, startServer, sleep } from './headless-chrome.mjs';
import { decodeReplayFile, encodeReplayFile } from './server/replay-file.mjs';

const web = path.dirname(fileURLToPath(import.meta.url)), assets = path.join(web, 'public', 'assets');
if (!fs.existsSync(path.join(assets, 'CAREER', 'career.json'))) { console.log('records verifier check SKIPPED: no game data'); process.exit(0); }
const browser = await startBrowser({ width: 640, height: 480 });
if (!browser) { console.log('records verifier check SKIPPED: no Chrome'); process.exit(0); }
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'ssx-verifier-'));
const career = JSON.parse(fs.readFileSync(path.join(assets, 'CAREER', 'career.json'), 'utf8'));
career.rules.records[12] = career.rules.records[12].map((r, k) => ({ ...r, value: 900 + k }));   // Snow Jam poked slow (the PS2 captures' poke)
career.rules.records[20] = career.rules.records[20].map((r, k) => ({ ...r, value: 104 - k }));   // Crow's Nest (big air) poked low
fs.mkdirSync(path.join(tmp, 'assets', 'CAREER'), { recursive: true }); fs.writeFileSync(path.join(tmp, 'assets', 'CAREER', 'career.json'), JSON.stringify(career));
const token = 'test-' + Math.random().toString(16).slice(2) + '-verifier-token'; fs.writeFileSync(path.join(tmp, 'token'), token + '\n');
const port = 24000 + Math.floor(Math.random() * 2000), API = `http://127.0.0.1:${port}`;
const mp = spawn(process.execPath, ['server/mp-server.mjs', '--port', String(port), '--host', '127.0.0.1'], { cwd: web,
  env: { ...process.env, MP_RECORDS_DIR: path.join(tmp, 'records'), MP_RECORDS_ASSETS: path.join(tmp, 'assets'), MP_RECORDS_VERIFIER_TOKEN_FILE: path.join(tmp, 'token') },
  stdio: ['ignore', 'pipe', 'pipe'] });
await new Promise((res, rej) => { const t = setTimeout(() => rej(new Error('records server')), 8000); mp.stdout.on('data', (d) => { if (String(d).includes('multiplayer server on')) { clearTimeout(t); res(); } }); });
process.env.MP_PORT = String(port);
const server = await startServer();
const E = (js) => browser.evaluate(js), failures = [];
const board = async (event = '0:ARA1') => (await (await fetch(`${API}/mp/records/board?event=${event}`)).json()).rows.filter((r) => !r.default);
let browserOpen = true;
try {
  if (!(await browser.hasWebGPU(server.origin))) { console.log('records verifier check SKIPPED: no WebGPU'); process.exit(0); }
  // ---- a genuine run, uploaded the player's way ----
  await browser.goto(`${server.origin}/?qa=1&course=ARA1&rider=zoe&cutscenes=0&quality=low&mute=1`);
  await browser.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
  await E(`(async()=>{const ui=ssxQA.ui();ui.feScreens.playerName='OWEN';await ui.careerUI.online.records.load(true);return 1})()`);
  await E(`(()=>{const ui=ssxQA.ui(),e=ui.courses.find(x=>x.code==='ARA1'&&!x.rivalMode);ui.startSingleEvent(e);return 1})()`);
  await browser.waitFor(`document.getElementById('stage')?.dataset.screen === 'ctm-objectives'`, 300000);
  await E(`(()=>{const raf=window.requestAnimationFrame.bind(window),at=performance.now();window.requestAnimationFrame=cb=>raf(()=>cb(at));ssxQA.ui().careerUI.play();
    window.__pad=(t)=>{const p=new Array(24).fill(0);p[22]=1;const s=Math.sin(t/90);if(s>0.6)p[21]=0.3;else if(s<-0.6)p[20]=0.3;return p;};
    window.__run=(from,n)=>{for(let t=from;t<from+n;t++){ssxQA.advance(1,window.__pad(t));if(/results|records|award/.test(ssxQA.ui().screen))return t+1;}return -1};return 1})()`);
  await sleep(300);
  let done = -1; for (let i = 0; i < 22000 && done < 0; i += 500) done = await E(`window.__run(${i},500)`);   // reused for big air below
  for (let k = 0; k < 60 && (await E('ssxQA.ui().screen')) !== 'ctm-records'; k++) await E('(()=>{for(let i=0;i<20;i++)ssxQA.advance(1,new Array(24).fill(0));return 1})()');
  assert.equal(await E('ssxQA.ui().screen'), 'ctm-records', 'the top-5 screen');
  await E(`(()=>{const ui=ssxQA.ui();ui.index=1;ui.choose(1);ui.careerUI.key({code:'Enter',key:'Enter',preventDefault(){}});return 1})()`);
  await browser.waitFor(`ssxQA.ui().careerUI.online.run?.state==='sent'`, 60000);
  // ---- a score event: a Single Event big air run (Crow's Nest) with jumps, spins and grabs; its claim is the score latched at the finish ----
  await E(`(()=>{const ui=ssxQA.ui();ui.careerUI.online.records.load(true);const e=ui.courses.find(x=>x.code==='ABA1'&&!x.rivalMode);ui.startSingleEvent(e);return 1})()`);
  await browser.waitFor(`document.getElementById('stage')?.dataset.screen === 'ctm-objectives'`, 300000);
  await E(`(()=>{ssxQA.ui().careerUI.play();
    window.__pad=(t)=>{const p=new Array(24).fill(0),ph=t%600;p[22]=ph<100||ph>500?1:0;if(ph>100&&ph<160)p[10]=1;if(ph>170&&ph<230){p[5]=1;p[12]=ph>190?1:0;}
      if(ph>400&&ph<440)p[10]=1;if(ph>445&&ph<500){p[6]=1;p[13]=1;}return p;};return 1})()`);
  await sleep(300);
  done = -1; for (let i = 0; i < 12000 && done < 0; i += 500) done = await E(`window.__run(${i},500)`);
  for (let k = 0; k < 60 && (await E('ssxQA.ui().screen')) !== 'ctm-records'; k++) await E('(()=>{for(let i=0;i<20;i++)ssxQA.advance(1,new Array(24).fill(0));return 1})()');
  const air = await E(`(()=>{const ui=ssxQA.ui();return {screen:ui.screen,run:ui.careerUI.online.run&&{key:ui.careerUI.online.run.key,value:ui.careerUI.online.run.value,rank:ui.careerUI.online.run.rank}}})()`);
  assert.ok(air.screen === 'ctm-records' && air.run?.key === '3:ABA1' && air.run.value > 104, `the big air top-5 screen ${JSON.stringify(air)}`);
  await E(`(()=>{const ui=ssxQA.ui();ui.index=1;ui.choose(1);ui.careerUI.key({code:'Enter',key:'Enter',preventDefault(){}});return 1})()`);
  await browser.waitFor(`ssxQA.ui().careerUI.online.run?.state==='sent'`, 60000);
  await browser.close(); browserOpen = false;
  const genuine = (await board()).find((r) => r.name === 'OWEN');
  assert.ok(genuine?.replay, 'the genuine run is listed');
  // ---- two forgeries beside it ----
  const file = decodeReplayFile(zlib.inflateRawSync(Buffer.from(await (await fetch(`${API}/mp/records/replay?id=${genuine.id}`)).arrayBuffer())));
  if (process.env.KEEP_FILES) { fs.mkdirSync(process.env.KEEP_FILES, { recursive: true }); fs.writeFileSync(path.join(process.env.KEEP_FILES, 'genuine.bin'), Buffer.from(await (await fetch(`${API}/mp/records/replay?id=${genuine.id}`)).arrayBuffer())); }
  const send = async (meta, pad) => (await fetch(`${API}/mp/records/submit`, { method: 'POST', body: zlib.deflateRawSync(encodeReplayFile({ meta, pad })) })).json();
  const fake1 = await send({ ...file.meta, name: 'FAKE1', claim: { ticks: file.meta.claim.ticks - 150 } }, file.pad);   // a faster claim, the same replay
  const fake2 = await send({ ...file.meta, name: 'FAKE2' }, new Uint8Array(0));                                          // a forged replay: no input at all
  const airRun = (await board('3:ABA1')).find((r) => r.name === 'OWEN'); assert.ok(airRun?.replay, 'the big air run is listed');
  const airFile = decodeReplayFile(zlib.inflateRawSync(Buffer.from(await (await fetch(`${API}/mp/records/replay?id=${airRun.id}`)).arrayBuffer())));
  assert.equal(airFile.meta.claim.score, air.run.value, 'the score claim is the latched finish score');
  const fake3 = await send({ ...airFile.meta, name: 'FAKE3', claim: { score: airFile.meta.claim.score + 500 } }, airFile.pad);   // a higher score, the same replay
  assert.ok(fake1.ok && fake2.ok && fake3.ok, `forgeries accepted by the tier-0 checks ${JSON.stringify([fake1, fake2, fake3])}`);
  const before = (await board()).map((r) => r.name);
  assert.ok(['OWEN', 'FAKE1', 'FAKE2'].every((n) => before.includes(n)), `listed at once (D6): ${before}`);
  // ---- the verifier ----
  const v = spawn(process.execPath, ['server/records-verifier.mjs', '--once', '--page', server.origin, '--api', API, '--token-file', path.join(tmp, 'token'),
    '--profile', path.join(tmp, 'profile'), '--max-load', '1000', '--timeout', '300', '--reload', '5', '--chrome', findChrome()], { cwd: web, stdio: ['ignore', 'pipe', 'pipe'] });
  let out = ''; v.stdout.on('data', (d) => { out += d; process.stdout.write(d); }); v.stderr.on('data', (d) => { out += d; });
  const code = await new Promise((r) => v.on('exit', r));
  assert.equal(code, 0, `verifier exit ${code}: ${out.slice(-800)}`);
  const results = JSON.parse(out.split('VERIFIER_RESULTS ')[1] || '[]'), by = (id) => results.find((r) => r.id === id);
  assert.equal(by(genuine.id)?.ok, true, `the genuine run reproduced: ${JSON.stringify(by(genuine.id))}`);
  assert.equal(by(fake1.id)?.ok, false, `the faster claim: ${JSON.stringify(by(fake1.id))}`);
  assert.equal(by(fake2.id)?.ok, false, `the forged replay: ${JSON.stringify(by(fake2.id))}`);
  assert.equal(by(airRun.id)?.ok, true, `the big air run reproduced: ${JSON.stringify(by(airRun.id))}`);
  assert.equal(by(fake3.id)?.ok, false, `the higher score: ${JSON.stringify(by(fake3.id))}`);
  assert.deepEqual((await board('3:ABA1')).map((r) => r.name), ['OWEN'], 'big air after the verifier');
  assert.match(out, /WebGPU available/, 'the start check'); assert.match(out, /cycle \d+: \d+ queued/, 'the cycle heartbeat');
  const after = await board();
  assert.deepEqual(after.map((r) => r.name), ['OWEN'], `after the verifier: ${after.map((r) => r.name)}`);
  assert.equal(after[0].verified, true);
  for (const r of results) console.log(`verify ${r.id}: ${r.ok ? 'reproduced' : 'not reproduced'} (${r.reason}) -> ${r.action}; ${JSON.stringify(r.stats)}`);
} catch (e) { failures.push(String(e?.stack || e)); }
finally { if (browserOpen) await browser.close(); await server.close(); mp.kill(); fs.rmSync(tmp, { recursive: true, force: true }); }
if (failures.length) { console.error('records verifier check FAILED:\n' + failures.join('\n')); process.exit(1); }
console.log('records verifier check OK');
process.exit(0);
