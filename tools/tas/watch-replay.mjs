// Watch a TAS replay file with the game's own Watch Replay in the real page (docs/tas.md "Proof"), in headless Chrome
// (web/headless-chrome.mjs startBrowser: --mute-audio) or the system WebKit (web/webkit-driver.mjs: muted page), ?mute=1 always.
//
//   node tools/tas/watch-replay.mjs REPLAY.ssxr [--browser chrome|webkit] [--viewer zoe] [--freeze | --realtime] [--live trace.json]
//        [--dump dump.json] [--out result.json]
//
// The replay is served by a records server of this script's own (web/server/mp-server.mjs on 127.0.0.1 with a temporary
// MP_RECORDS_DIR, removed afterwards): the page's Watch Replay downloads it from there, as it does from a board. Nothing reaches
// the public server. Then: ui.cb.watchOnlineReplay (web/online-replay.js: the uploader's rider, lineup, warm-up, attribute bytes,
// replay.load, the full replay), every tick stepped (replay.step), the finish record (web/game-tick.js rec.finish) and the
// ?simtrace hash of every tick, compared with --live (a tools/tas/page-run.mjs trace.json of the live run).
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import zlib from 'node:zlib';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { startBrowser, startServer, sleep } from '../../web/headless-chrome.mjs';
import { decodeReplayFile } from '../../web/server/replay-file.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
const web = path.resolve(here, '../../web');
const args = process.argv.slice(2);
const opt = (k, d) => {
  const i = args.indexOf('--' + k);
  return i < 0 ? d : args[i + 1];
};
const file = args.find((a, i) => !a.startsWith('--') && !(i > 0 && args[i - 1].startsWith('--')));
const kind = opt('browser', 'chrome');
const bytes = fs.readFileSync(file);
const { meta } = decodeReplayFile(bytes);

// the records server
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'ssx-tas-watch-'));
const port = 25000 + Math.floor(Math.random() * 2000);
const mp = spawn(process.execPath, ['server/mp-server.mjs', '--port', String(port), '--host', '127.0.0.1'], { cwd: web,
  env: { ...process.env, MP_RECORDS_DIR: path.join(tmp, 'records') }, stdio: ['ignore', 'pipe', 'pipe'] });
const result = { file, browser: kind, claim: meta.claim, finishTick: meta.finishTick };
let browser = null;
let server = null;
try {
  await new Promise((res, rej) => {
    const t = setTimeout(() => rej(new Error('records server did not start')), 10000);
    mp.stdout.on('data', (d) => {
      if (String(d).includes('multiplayer server on')) {
        clearTimeout(t);
        res();
      }
    });
  });
  const API = `http://127.0.0.1:${port}`;
  const sub = await (await fetch(`${API}/mp/records/submit`, { method: 'POST', body: zlib.deflateRawSync(bytes) })).json();
  if (!sub.ok) throw new Error('local records server refused the replay: ' + JSON.stringify(sub));
  result.id = sub.id;
  process.env.MP_PORT = String(port);
  server = await startServer();
  const url = `${server.origin}/?qa=1&course=${meta.course}&rider=${opt("viewer", "zoe")}&cutscenes=0&quality=low&simtrace=1&mute=1`;
  let E;
  if (kind === 'webkit') {
    const { startWebKit } = await import('../../web/webkit-driver.mjs');
    browser = await startWebKit({ width: 640, height: 480 });
    if (!browser) throw new Error('no WebKit driver');
    await browser.goto(url);
    E = (js) => browser.eval(js);
  } else {
    browser = await startBrowser({ width: 640, height: 480 });
    if (!browser) throw new Error('no Chrome');
    await browser.goto(url);
    E = (js) => browser.evaluate(js);
  }
  const waitFor = async (expr, ms) => {
    const t = Date.now();
    while (Date.now() - t < ms) {
      try {
        if (await E(expr)) return;
      } catch {}
      await sleep(250);
    }
    throw new Error('timeout: ' + expr);
  };
  await waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
  // --freeze: the frame clock stopped (frames draw with dt 0), so only replay.step ticks (as tools/tas/page-run.mjs does)
  if (args.includes('--freeze')) await E(`(()=>{const raf=window.requestAnimationFrame.bind(window),at=performance.now();window.requestAnimationFrame=cb=>raf(()=>cb(at));return 1})()`);
  // Watch Replay from the board's row (career-ui's ctm-board Cross: ui.cb.watchOnlineReplay)
  await E(`(()=>{ssxQA.ui().cb.watchOnlineReplay({entry:{id:${JSON.stringify(sub.id)}},key:${JSON.stringify(meta.event)},back:()=>{}});return 1})()`);
  try {
    await waitFor(`ssxQA.ui().screen==='replay'&&ssxQA.replay().active`, 300000);
  } catch (e) {
    console.log('state', JSON.stringify(await E(`({screen:ssxQA.ui().screen,loading:!!ssxQA.ui().loading?.active,course:ssxQA.ui().course?.code,err:document.body.dataset.loadError||null})`)));
    throw e;
  }
  // the full replay starts paused on its first tick (replay.start('full') stepped tick 0): step the rest, keeping the finish
  // the human as loaded (main.js loadedRider / the merged settings): what a replay elsewhere must load again
  const riderInfo = JSON.parse(await E(`JSON.stringify((()=>{const r=ssxQA.loadedRider()||{};const s=JSON.stringify(ssxQA.humanSettings());let h=0x811c9dc5;for(let i=0;i<s.length;i++)h=Math.imul(h^s.charCodeAt(i),16777619)>>>0;
  const pick={};for(const k of ['id','package','root','kind','base','stamp','uber','outfit','career','replayFixed'])pick[k]=r[k]??null;
  const c=window.ssxEffects.core;return {rider:pick,uberRows:(r.uber_choice||[]).length,outfitSettings:!!r.outfit_settings,settingsHash:h,settingsLength:s.length,stats:Array.from(new Float32Array(c.HEAPF32.buffer,c._rider_attribute_stats(),12))}})())`));
  console.log('rider', JSON.stringify(riderInfo));
  const base = await E('window.__simTrace.ticks.length - 1');
  // per tick (as tools/tas/page-run.mjs dump.json): the human's rider state, the shared game RNG, each computer rider's rider state
  await E(`(()=>{window.__tasDump=[];window.__tasRow=()=>{const c=window.ssxEffects.core,ai=ssxQA.aiRace?.();
    const row=[...new Float32Array(c.HEAPF32.buffer,c._rider_state(),16),...new Uint32Array(c.HEAPU8.buffer,c._animation_rng_words(),6)];
    for(const n of ai?.racers?.npcs??[])row.push(...new Float32Array(n.core.HEAPF32.buffer,n.core._rider_state(),16));
    row.push(...new Uint32Array(c.HEAPU8.buffer,c._visual_rng_words(),6),...new Float32Array(c.HEAPF32.buffer,c._pose_physical(),12),...new Float32Array(c.HEAPF32.buffer,c._animation_info(),19),...new Float32Array(c.HEAPF32.buffer,c._boost_info(),8));
    {const u=new Uint32Array(c.HEAPU8.buffer,c._score_object_dump(),0x1d0/4);let h=0x811c9dc5;for(let i=0;i<u.length;i++)h=Math.imul(h^u[i],16777619)>>>0;row.push(h);}
    window.__tasDump.push(row)};window.__tasRow();return 1})()`);
  let fin = null;
  // --realtime: Play, and the page's own frame clock runs the replay at 1x (as a viewer watches it); the finish is read from the
  // core's race result once the replay has passed the finish tick
  if (args.includes('--realtime')) {
    await E('(()=>{ssxQA.replay().playPause();return 1})()');
    const t0 = Date.now();
    while (Date.now() - t0 < 600000) {
      const at = await E('ssxQA.replay().tick');
      if (at > meta.finishTick || !(await E('ssxQA.replay().active'))) break;
      await sleep(2000);
    }
    fin = await E(`(()=>{const c=window.ssxEffects.core,r=new Float32Array(c.HEAPF32.buffer,c._race_result_info(),6);
      return r[0]?{ticks:r[1],tick:null,realtime:true}:{none:true}})()`);
  }
  for (let guard = 0; guard < 200 && !fin; guard++) {
    fin = await E(`(()=>{const r=ssxQA.replay();for(let k=0;k<500&&r.tick<=${meta.finishTick};k++){const t=r.tick,x=r.step();window.__tasRow();
      if(x&&x.finish)return {...x.finish,tick:t};}
      return r.tick>${meta.finishTick}?{none:true}:null})()`);
  }
  if (opt('dump')) fs.writeFileSync(opt('dump'), await E('JSON.stringify(window.__tasDump)'));
  result.finish = fin;
  const trace = await E(`window.__simTrace.ticks.slice(${base},${base}+${meta.finishTick + 1})`);
  result.traceTicks = trace.length;
  if (opt('live')) {
    const live = JSON.parse(fs.readFileSync(opt('live'), 'utf8'));
    const n = Math.min(live.length, trace.length);
    let first = -1;
    for (let i = 0; i < n && first < 0; i++) if (live[i] !== trace[i]) first = i;
    result.liveCompared = n;
    result.firstLiveDifference = first;
  }
  result.ticks = fin && !fin.none ? Math.round(fin.ticks) : null;
  result.seconds = result.ticks != null ? +(result.ticks / 60).toFixed(3) : null;
  result.underPlatinumGate = result.ticks != null && result.ticks <= 9000;
  console.log(JSON.stringify(result));
  if (opt('out')) fs.writeFileSync(opt('out'), JSON.stringify(result, null, 1));
} finally {
  try {
    if (browser) await browser.close();
  } catch {}
  if (server) await server.close();
  mp.kill();
  fs.rmSync(tmp, { recursive: true, force: true });
}
