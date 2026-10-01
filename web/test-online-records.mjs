// Online records in the page (pv onlineRecords, docs/online-records.md), headless Chrome against a local records server:
// a Single Event Snow Jam run (tuck to the finish) makes the online top 5 (the server's board seeded slow, like the PS2 captures'
// poked records), "Top 5 Record Times" opens with Continue / Save Records / Online Records and the run in the player's colour,
// Save Records opens the game's keyboard (8 characters, Player Name's rules), Done uploads the run with its replay, the server
// lists it first; Online Records shows the full board; Watch Replay plays the downloaded run in the full replay, every replayed
// tick's ?simtrace equal to the live run's, and Exit replay goes back to the board; the main menu's Leaderboards opens a board
// through the Select Event maps. Also the client rules (rankAmong) without a browser.
//   node test-online-records.mjs [--ticks N]   (N: the live run's tick budget, default 22000)
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { rankAmong } from './online-records.js';
import { startBrowser, startServer, sleep } from './headless-chrome.mjs';

const web = path.dirname(fileURLToPath(import.meta.url));
const CAM_INFO = '(()=>{const c=window.ssxEffects?.core;return c?._replay_camera_info?Array.from(new Float32Array(c.HEAPF32.buffer,c._replay_camera_info(),10)):null})()';
// ---- client rules ----
{
  const rows = [{ name: 'A', value: 100 }, { name: 'B', value: 200, default: true }, { name: 'C', value: 300 }, { name: 'D', value: 400 }, { name: 'E', value: 500 }];
  assert.equal(rankAmong(rows, { timed: true, value: 50 }), 0);
  assert.equal(rankAmong(rows, { timed: true, value: 200 }), 2, 'a tie goes below the earlier entry');
  assert.equal(rankAmong(rows, { timed: true, value: 600 }), -1);
  assert.equal(rankAmong(rows, { timed: true, value: 350, name: 'c' }), -1, 'your own better entry keeps the run out');
  assert.equal(rankAmong(rows, { timed: true, value: 250, name: 'c' }), 2, 'your own worse entry is replaced');
  assert.equal(rankAmong(rows, { timed: true, value: 450, name: 'E' }), 4, 'replaces your own 5th');
  assert.equal(rankAmong(rows, { timed: true, value: 150, name: 'B' }), 1, 'a default name is not yours');
  assert.equal(rankAmong([{ name: 'X', value: 1000 }], { timed: false, value: 1000 }), -1, 'score tie');
  assert.equal(rankAmong([{ name: 'X', value: 1000 }], { timed: false, value: 1001 }), 0);
  console.log('online records: client rank rules OK');
}

const TICKS = process.argv.includes('--ticks') ? +process.argv[process.argv.indexOf('--ticks') + 1] : 22000;
const assets = path.join(web, 'public', 'assets');
if (!fs.existsSync(path.join(assets, 'CAREER', 'career.json'))) { console.log('online records page check SKIPPED: no game data'); process.exit(0); }
const browser = await startBrowser({ width: 640, height: 480 });
if (!browser) { console.log('online records page check SKIPPED: no Chrome'); process.exit(0); }
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'ssx-online-records-'));
// the server's tables: the disc's, with Snow Jam's race records poked to 15:00..15:04 (the PS2 captures' poke)
const career = JSON.parse(fs.readFileSync(path.join(assets, 'CAREER', 'career.json'), 'utf8'));
career.rules.records[12] = career.rules.records[12].map((r, k) => ({ ...r, value: 900 + k }));
fs.mkdirSync(path.join(tmp, 'assets', 'CAREER'), { recursive: true }); fs.writeFileSync(path.join(tmp, 'assets', 'CAREER', 'career.json'), JSON.stringify(career));
const port = 21000 + Math.floor(Math.random() * 2000);
const mp = spawn(process.execPath, ['server/mp-server.mjs', '--port', String(port), '--host', '127.0.0.1'], { cwd: web, env: { ...process.env, MP_RECORDS_DIR: path.join(tmp, 'records'), MP_RECORDS_ASSETS: path.join(tmp, 'assets') }, stdio: ['ignore', 'pipe', 'pipe'] });
await new Promise((res, rej) => { const t = setTimeout(() => rej(new Error('records server')), 8000); mp.stdout.on('data', (d) => { if (String(d).includes('multiplayer server on')) { clearTimeout(t); res(); } }); });
process.env.MP_PORT = String(port);   // the dev server's /mp proxy (web/vite.config.js)
const server = await startServer();
const failures = [], log = (s) => console.log(s);
const check = (ok, what) => { if (!ok) failures.push(what); return ok; };
const E = (js) => browser.evaluate(js);
const SHOTS = process.env.SHOTS || null;   // a directory: the screens as PNGs (records, keyboard, board, replay, leaderboards)
const shot = async (name) => { if (!SHOTS) return; await sleep(400); fs.mkdirSync(SHOTS, { recursive: true }); fs.writeFileSync(path.join(SHOTS, name + '.png'), Buffer.from(await browser.screenshot('#stage'), 'base64')); };
try {
  if (!(await browser.hasWebGPU(server.origin))) { console.log('online records page check SKIPPED: no WebGPU'); process.exit(0); }
  await browser.goto(`${server.origin}/?qa=1&course=ARA1&rider=zoe&cutscenes=0&quality=low&simtrace=1&mute=1&pv=replay,onlineRecords`);
  await browser.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
  check(!(await E('document.body.dataset.loadError || ""')), 'page load');
  await E(`(async()=>{const ui=ssxQA.ui();ui.feScreens.playerName='TESTER';await ui.careerUI.online.records.load(true);return ui.careerUI.online.records.status})()`);
  check((await E('ssxQA.ui().careerUI.online.records.status')) === 'online', 'the boards load');
  // ---- a Single Event Snow Jam run to the finish ----
  await E(`(()=>{const ui=ssxQA.ui(),e=ui.courses.find(x=>x.code==='ARA1'&&!x.rivalMode);ui.startSingleEvent(e);return 1})()`);
  await browser.waitFor(`document.getElementById('stage')?.dataset.screen === 'ctm-objectives' || !!document.body.dataset.loadError`, 300000);
  await E(`(()=>{const raf=window.requestAnimationFrame.bind(window),at=performance.now();window.requestAnimationFrame=cb=>raf(()=>cb(at));ssxQA.ui().careerUI.play();
    window.__pad=(t)=>{const p=new Array(24).fill(0);p[22]=1;const s=Math.sin(t/90);if(s>0.6)p[21]=0.3;else if(s<-0.6)p[20]=0.3;return p;};
    window.__run=(from,n)=>{for(let t=from;t<from+n;t++){ssxQA.advance(1,window.__pad(t));if(/results|records|award/.test(ssxQA.ui().screen))return t+1;}return -1};return 1})()`);
  await sleep(300);
  const base = await E('window.__simTrace.ticks.length');
  // Watch Replay from the board on screen, to the finish (N ticks); the watched ticks are the trace's last N (the replay pauses at its end);
  // each compared with the live run's. Exit replay back to the board. -> the first differing tick, -1 when exact
  const watchExact = async (N, { course = null, cam = null } = {}) => {
    await E(`(()=>{const ui=ssxQA.ui();ui.careerUI.online.board.cursor=0;ui.choose(0);return 1})()`);
    await browser.waitFor(`ssxQA.ui().screen==='replay' && ssxQA.replay().mode==='full'${course ? ` && ssxQA.ui().course?.code==='${course}'` : ''}`, 300000);
    await E(`(()=>{const r=ssxQA.replay();r.playPause();let n=0;while(r.tick<${N}&&n<${N}+50){r.frame(1/60);n++;}return r.tick})()`);
    if (cam) cam(await E(CAM_INFO));   // the replay view's camera state while the replay is up
    const live = await E(`window.__simTrace.ticks.slice(${base},${base + N})`), rep = await E(`(()=>{const t=window.__simTrace.ticks;return t.slice(t.length-${N})})()`);
    let miss = rep.length === N ? -1 : -2; for (let i = 0; i < N && miss === -1; i++) if (live[i] !== rep[i]) miss = i;
    await E(`(()=>{const ui=ssxQA.ui();ui.replayUi.key({code:'Enter'});ui.replayUi.key({code:'ArrowDown'});ui.replayUi.key({code:'Space'});return 1})()`);
    await browser.waitFor(`ssxQA.ui().screen==='ctm-board'`, 20000);
    return miss;
  };
  let done = -1, t0 = Date.now();
  for (let i = 0; i < TICKS && done < 0; i += 500) done = await E(`window.__run(${i},500)`);
  for (let k = 0; k < 60 && !/records|results|award/.test(await E('ssxQA.ui().screen')); k++) await E('(()=>{for(let i=0;i<20;i++)ssxQA.advance(1,new Array(24).fill(0));return 1})()');
  const fin = await E(`(()=>{const ui=ssxQA.ui(),r=ssxQA.replay(),o=ui.careerUI.online;return {screen:ui.screen,items:ui.items(),top:ui.careerUI.topTime,run:o.run&&{rank:o.run.rank,value:o.run.value,key:o.run.key},finish:r.finishTick,bytes:r.recording?.bytes}})()`);
  log(`run: ${JSON.stringify(fin)} (${((Date.now() - t0) / 1000).toFixed(0)} s)`);
  if (check(fin.screen === 'ctm-records' && fin.top && fin.run?.rank === 0, `Top 5 Record Times after a top run: ${JSON.stringify(fin)}`)) {
    await shot('1-records');
    check(JSON.stringify(fin.items) === JSON.stringify(['Continue', 'Save Records', 'Online Records']), `items ${fin.items}`);
    // ---- Save Records: the keyboard, a name typed, Done ----
    const kb = await E(`(()=>{const ui=ssxQA.ui();ui.index=1;ui.choose(1);const k=ui.feScreens.keyboard;return k&&{kind:k.kind,text:k.text,max:k.max}})()`);
    check(kb?.kind === 'record' && kb.text === 'TESTER' && kb.max === 8, `the keyboard opens with the Player Name ${JSON.stringify(kb)}`);
    await shot('2-keyboard');
    const typed = await E(`(()=>{const ui=ssxQA.ui(),fe=ui.feScreens;fe.keyPress('Clear');for(const ch of 'OWEN')ui.careerUI.key({code:'Key'+ch,key:ch,preventDefault(){}});const t=fe.keyboard.text;ui.careerUI.key({code:'Enter',key:'Enter',preventDefault(){}});return t})()`);
    check(typed === 'OWEN', `typed ${typed}`);
    await browser.waitFor(`ssxQA.ui().careerUI.online.run?.state==='sent' || ssxQA.ui().careerUI.online.note==='The record could not be saved.' || ssxQA.ui().careerUI.online.note==='Online records unavailable.'`, 60000);
    const sent = await E(`(()=>{const o=ssxQA.ui().careerUI.online;return {state:o.run.state,note:o.note,id:o.run.id,top:o.records.top(o.run.key).slice(0,2).map(r=>r.name)}})()`);
    log(`submit: ${JSON.stringify(sent)}`);
    check(sent.state === 'sent' && sent.top[0] === 'OWEN', `the upload ${JSON.stringify(sent)}`);
    check((await E('ssxQA.ui().index')) === 0, 'the focus moves to Continue after the save');
    await shot('3-saved');
    const board = await (await fetch(`http://127.0.0.1:${port}/mp/records/board?event=0:ARA1`)).json();
    check(board.rows[0]?.name === 'OWEN' && board.rows[0].replay && board.rows[0].value === fin.run.value, `server board ${JSON.stringify(board.rows[0])}`);
    // ---- Online Records -> the board -> Watch Replay ----
    await E(`(()=>{const ui=ssxQA.ui();ui.index=2;ui.choose(2);return ui.screen})()`);
    await browser.waitFor(`ssxQA.ui().careerUI.online.board?.status==='ready'`, 30000);
    const b = await E(`(()=>{const ui=ssxQA.ui(),b=ui.careerUI.online.board;return {screen:ui.screen,items:ui.items(),rows:b.rows.length,first:b.rows[0]?.name,greyed:ui.careerUI.disabled('ctm-board',0)}})()`);
    check(b.screen === 'ctm-board' && b.first === 'OWEN' && b.rows === 6 && !b.greyed, `the board ${JSON.stringify(b)}`);
    await shot('4-board');
    await E(`(()=>{const ui=ssxQA.ui();ui.careerUI.key({code:'ArrowDown',preventDefault(){}});const g=ui.careerUI.disabled('ctm-board',0);ui.careerUI.key({code:'ArrowUp',preventDefault(){}});return g})()`).then((g) => check(g === true, 'a default row has no replay'));
    const repStart = await E('window.__simTrace.ticks.length');
    await E(`(()=>{ssxQA.ui().choose(0);return 1})()`);
    await browser.waitFor(`ssxQA.ui().screen==='replay' && ssxQA.replay().mode==='full'`, 300000);
    const N = fin.finish + 1;
    const w = await E(`(()=>{const r=ssxQA.replay();const a={tick:r.tick,paused:r.paused,finish:r.finishTick};r.playPause();let n=0;while(r.tick<${N}&&n<${N}+50){r.frame(1/60);n++;}return {...a,tick2:r.tick}})()`);
    log(`watch: ${JSON.stringify(w)}`);
    // the replay view (web/replay_camera.inc) follows Snow Jam's camera triggers: [type, mode, ..., triggers 4, ..., fired 8, last id 9]
    const cam = await E(CAM_INFO);
    check(cam?.[4] === 48 && cam?.[8] > 0, `Watch Replay's Web-cam: ARA1's 48 camera triggers loaded and firing ${JSON.stringify(cam)}`);
    await shot('5-replay');
    check(w.finish === fin.finish, `the downloaded run's finish tick ${w.finish} (live ${fin.finish})`);
    const live = await E(`window.__simTrace.ticks.slice(${base},${base + N})`), rep = await E(`window.__simTrace.ticks.slice(${repStart},${repStart + N})`);
    let miss = -1; for (let i = 0; i < Math.min(live.length, rep.length); i++) if (live[i] !== rep[i]) { miss = i; break; }
    check(miss === -1 && rep.length === N, `Watch Replay leaves the live run at tick ${miss} of ${N} (repStart ${repStart})`);
    log(`watch trace: ${N} ticks compared, first difference ${miss}`);
    const back = await E(`(()=>{const ui=ssxQA.ui();ui.replayUi.key({code:'Enter'});ui.replayUi.key({code:'ArrowDown'});ui.replayUi.key({code:'Space'});return {screen:ui.screen}})()`);
    await sleep(500);
    check((await E('ssxQA.ui().screen')) === 'ctm-board', `Exit replay back to the board (${JSON.stringify(back)})`);
    check(!(await E('ssxQA.replay().available()')), 'after a watched run, the page run\'s own Replay is unavailable');
    // Watch Replay twice more on the same course and page, each to the finish (three in a row)
    for (const k of [2, 3]) { const m = await watchExact(N); check(m === -1, `Watch Replay #${k} on the same course leaves the live run at tick ${m} of ${N}`); log(`watch #${k}: ${N} ticks, first difference ${m}`); }
    globalThis.__N = N;
  }
  // ---- the main menu's Leaderboards: the sixth row -> Select Peak / Mode / Event -> the event's board ----
  await E(`(()=>{const ui=ssxQA.ui();ui.careerUI.online.board=null;ui.set('main');ui.index=5;ui.sync();return 1})()`);
  await sleep(600);
  const menu = await E(`(()=>{const ui=ssxQA.ui();return {items:ui.items(),dis:ui.mainDisabled(),lui:ui.mainMenu?.model?.items?.length}})()`);
  check(menu.items[5] === 'Leaderboards' && menu.dis.length === 6 && menu.dis[5] === false && menu.lui === 6, `main menu ${JSON.stringify(menu)}`);
  await shot('6-main-menu');
  await E(`(()=>{const ui=ssxQA.ui();ui.choose(5);return ui.screen})()`);
  // On a loaded machine a choice can come before the screen takes input (its intro / phase): wait for the screen, then repeat the
  // choice until the screen changes (a choice that only reads state, e.g. finding a row, runs once).
  const state = () => E(`JSON.stringify((()=>{const ui=ssxQA.ui(),es=ui.eventSelect;return {screen:ui.screen,ready:ui.ready,flash:!!es.flash,boards:!!es.boards,index:ui.index,peak:es.peak}})())`);
  const pick = async (screen, expr, { advances = true } = {}) => {
    try { await browser.waitFor(`ssxQA.ui().screen===${JSON.stringify(screen)} && !ssxQA.ui().eventSelect.flash`, 30000); }
    catch (e) { throw new Error(`${e.message}; state ${await state()}`); }
    await sleep(300);
    if (!advances) return E(expr);
    for (let k = 0; k < 10; k++) {
      const r = await E(expr);
      try { await browser.waitFor(`ssxQA.ui().screen!==${JSON.stringify(screen)} || !!ssxQA.ui().eventSelect.flash`, 2000); return r; } catch {}
    }
    throw new Error(`the choice on ${screen} did not take; state ${await state()}`);
  };
  await pick('fe-peak', `(()=>{const ui=ssxQA.ui();ui.eventSelect.choose(2);return 1})()`);   // Peak 1 (rows: Peak 3, 2, 1)
  await pick('fe-mode', `(()=>{const ui=ssxQA.ui();ui.eventSelect.choose(0);return 1})()`);   // Race
  const row = await pick('fe-event', `(()=>{const es=ssxQA.ui().eventSelect,l=es.events(),k=l.findIndex(e=>e.code==='ARA1'&&e.mode===0);ssxQA.ui().index=k;return {k,playable:l.map(e=>e.playable)}})()`, { advances: false });
  await shot('7-select-event');
  await E(`(()=>{const ui=ssxQA.ui();ui.eventSelect.choose(${row.k});return 1})()`);
  await browser.waitFor(`ssxQA.ui().screen==='ctm-board' && ssxQA.ui().careerUI.online.board?.status==='ready'`, 20000);
  const fb = await E(`(()=>{const b=ssxQA.ui().careerUI.online.board;return {fe:b.fe,first:b.rows[0]?.name,n:b.rows.length}})()`);
  check(fb.fe && fb.first === 'OWEN' && fb.n === 6, `Leaderboards board ${JSON.stringify(fb)}`);
  await shot('8-leaderboards-board');
  await E(`(()=>{ssxQA.ui().careerUI.back();return 1})()`);
  await browser.waitFor(`ssxQA.ui().screen==='fe-event'`, 10000);
  log(`leaderboards: ${JSON.stringify({ menu: menu.items[5], row, board: fb })}`);
  // ---- Watch Replay from another course: the front end on Metro-City (Back to a history entry: a course switch to its menu), the
  //      Snow Jam board, Watch Replay switches the course back under the load screen and replays the run exactly ----
  await E(`(()=>{const u=new URL(location.href);u.searchParams.set('course','BRA2');u.searchParams.delete('autostart');history.pushState({ssx3:1},'',u);dispatchEvent(new PopStateEvent('popstate',{state:{ssx3:1,screen:'main'}}));return 1})()`);
  await browser.waitFor(`ssxQA.ui().screen==='main' && ssxQA.ui().course?.code==='BRA2' && ssxQA.ui().ready`, 300000);
  await E(`(()=>{const ui=ssxQA.ui();ui.index=5;ui.choose(5);return 1})()`);
  await pick('fe-peak', `(()=>{ssxQA.ui().eventSelect.choose(2);return 1})()`); await pick('fe-mode', `(()=>{ssxQA.ui().eventSelect.choose(0);return 1})()`);
  await pick('fe-event', `(()=>{ssxQA.ui().eventSelect.choose(${row.k});return 1})()`);
  await browser.waitFor(`ssxQA.ui().screen==='ctm-board' && ssxQA.ui().careerUI.online.board?.status==='ready'`, 20000);
  const M = globalThis.__N ?? 1200;
  const miss2 = await watchExact(M, { course: 'ARA1', cam: (cam2) => check(cam2?.[4] === 48 && cam2?.[8] > 0, `Watch Replay after a course switch: ARA1's camera triggers (not BRA2's) ${JSON.stringify(cam2)}`) });
  check(miss2 === -1, `Watch Replay after a course switch leaves the live run at tick ${miss2} of ${M}`);
  const back2 = await E(`(()=>{const ui=ssxQA.ui();return {screen:ui.screen,fe:ui.careerUI.online.board?.fe}})()`);
  check(back2.screen === 'ctm-board' && back2.fe, `Exit replay back to the Leaderboards board ${JSON.stringify(back2)}`);
  log(`cross-course watch: ${M} ticks, first difference ${miss2}, back ${JSON.stringify(back2)}`);
  // ---- a viewer riding another rider (Psymon) watches Zoe's run: Zoe loads for the replay, Psymon is back after it ----
  await E(`(async()=>{const ui=ssxQA.ui();await ui.cb.rider(ui.riders.find(r=>r.id==='psymon'));return 1})()`);
  const miss3 = await watchExact(M);
  check(miss3 === -1, `Watch Replay with another rider selected leaves the live run at tick ${miss3} of ${M}`);
  const who = await E(`(()=>{const ui=ssxQA.ui();return {screen:ui.screen,rider:ui.careerUI.online.ui.cb.onlineRun?.()?.character}})()`);
  check(who.screen === 'ctm-board' && who.rider === 8, `back to the board with Psymon (character 8) ${JSON.stringify(who)}`);
  log(`other rider watch: ${M} ticks, first difference ${miss3}, after ${JSON.stringify(who)}`);
  // ---- the server gone: the last good boards ('cached'); none ever loaded: 'offline' (the PS2 table on the records screen) ----
  mp.kill(); await sleep(400);
  const off = await E(`(async()=>{const o=ssxQA.ui().careerUI.online;await o.records.load(true);const a=o.records.status,top=o.records.top('0:ARA1')?.[0]?.name;localStorage.removeItem('ssx3.onlineRecords.v1');const {OnlineRecords}=await import('/online-records.js');const r=new OnlineRecords();await r.load(true);return {a,top,b:r.status}})()`);
  check(off.a === 'cached' && off.top === 'OWEN' && off.b === 'offline', `offline fallback ${JSON.stringify(off)}`);
  log(`offline: ${JSON.stringify(off)}`);
} catch (e) { failures.push(String(e?.stack || e)); }
finally {
  try { console.log(browser.logs.filter((l) => /error|exception|warn/i.test(l)).slice(-20).join('\n')); } catch {}
  await browser.close(); await server.close(); mp.kill(); fs.rmSync(tmp, { recursive: true, force: true });
}
if (failures.length) { console.error('online records page check FAILED:\n' + failures.join('\n')); process.exit(1); }
console.log('online records page check OK');
process.exit(0);
