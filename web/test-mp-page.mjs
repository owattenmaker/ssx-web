// Two real pages, a normal online lobby and two starts through the gates. A prepared offline rider manager must
// not intercept the network pair host (2026-10-14 gate freeze). --webkit runs the second player in Safari's engine.
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import { startBrowser, startServer, sleep } from './headless-chrome.mjs';
import { startWebKit } from './webkit-driver.mjs';

const useWebKit = process.argv.includes('--webkit');
const course = process.env.MP_COURSE || 'ARA1';
// MP_PICK=<code>: before the first race the host picks that course on the lobby's Course row. With PAGE_PV=mpCourseSelect it is chosen in
// the Single Event selector and must not load until the start; without the switch the row steps through the courses, each step loading
// on both pages in the lobby (the old flow, recorded).
const pick = process.env.MP_PICK || '';
// MP_PRERACE=1 (with PAGE_PV=mpPreRace): cutscenes on; each race goes load -> intro -> card -> Continue -> GO. Player 0 presses Continue,
// player 1 lets the card's 20 s auto-continue run in the first race and presses it in the second.
const preRace = process.env.MP_PRERACE === '1';
const port = 23000 + Math.floor(Math.random() * 1000);
const browsers = [];
const shot = async (i, name) => {
  if (!process.env.MP_SHOTS) return;
  fs.mkdirSync(process.env.MP_SHOTS, { recursive: true });
  const file = `${process.env.MP_SHOTS}/${useWebKit ? 'mixed' : 'chrome'}-${name}-${i}.png`;
  if (browsers[i].raw.shot) await browsers[i].raw.shot(file, '#stage');
  else fs.writeFileSync(file, Buffer.from(await browsers[i].raw.screenshot(), 'base64'));
};
const flowOf = `(()=>{const u=ssxQA.ui();return {screen:u.screen,course:u.course?.code,lobby:u.mpUI.client.state.lobby?.course,
  index:u.index,switching:!!u.loading?.session}})()`;
let server;
let mp;
const installErrors = `(()=>{
  window.__mpErrors=[];
  addEventListener('error',e=>{
    let message=e.message;
    try {message=ssxEffects.core.getExceptionMessage(e.error).join(': ');} catch {}
    if(window.__mpErrors.length<8)window.__mpErrors.push(message);
  });
  addEventListener('unhandledrejection',e=>{
    if(window.__mpErrors.length<8)window.__mpErrors.push(String(e.reason));
  });
  window.__screenLog=[];
  const u=ssxQA.ui(),set=u.set.bind(u);
  u.set=(screen,...rest)=>{
    const r=set(screen,...rest);
    window.__screenLog.push({screen,at:Date.now(),script:window.__cutscenes?.state?.script??null});
    return r;
  };
  return 1;
})()`;
const status = `(()=>{
  const mp=window.__perfMp,c=window.__perfCore().core;
  return {screen:ssxQA.ui().screen,errors:window.__mpErrors,packets:mp.stats.sentStates,
    visibility:document.visibilityState,
    frames:mp.stats.frames,loading:ssxQA.ui().loading.session && {
      pending:ssxQA.ui().loading.session.pending,doneAt:ssxQA.ui().loading.session.doneAt,
      stage:ssxQA.ui().loading.session.meter?.current(),percent:ssxQA.ui().loading.session.shownPercent},
    pairs:mp.pairs,remote:[...mp.remote.racers.values()].map(r=>r.latest?.tick??-1),
    position:Array.from(new Float32Array(c.HEAPF32.buffer,c._rider_state(),3))};
})()`;
// The pre-race of one race on both pages: every screen on the way (shots of each), Continue, the shared intro instant and script.
async function preRaceFlow(round) {
  const seen = [new Set(), new Set()];
  const cardSince = [0, 0];
  const scripts = [null, null];
  const actorsSeen = [[], []];
  const until = Date.now() + 150000;
  const probe = `(()=>{const u=ssxQA.ui(),cs=window.__cutscenes;
    const actors=(cs?.seq?.actors??[]).map(a=>({binding:a.object?.ext?.binding??null,rider:a.rider?.id??null,slot:a.rider?.slot??null,
      model:!!a.model}));
    return {screen:u.screen,card:u.mpUI.session.card,actors,
      cut:cs?.state&&{kind:cs.state.kind,step:cs.state.step,script:cs.state.script,t:cs.state.t},
      goIn:window.__perfMp.debug().goIn}})()`;
  while (true) {
    assert.ok(Date.now() < until, 'both pages reach the race through the pre-race');
    const now = await Promise.all(browsers.map((page) => page.eval(probe)));
    for (const [i, f] of now.entries()) {
      const key = f.screen === 'mp-card' && f.card?.ready ? 'mp-card-waiting' : f.screen;
      if (!seen[i].has(key)) {
        seen[i].add(key);
        console.log(`online page: player ${i} ${key}`, JSON.stringify(f));
        if (key !== 'cutscene') await shot(i, `${round}-${key}`);
      }
      if (f.cut?.kind === 'intro' && f.cut.step === 0 && scripts[i] == null) scripts[i] = f.cut.script;
      // the start hut at three moments, with its rider actors (bindings 16..21 = race riders 0..5)
      for (const at of [60, 240, 420]) {
        const key2 = `intro-${at}`;
        if (f.screen === 'cutscene' && f.cut?.step === 0 && f.cut.t > at && !seen[i].has(key2)) {
          seen[i].add(key2);
          console.log(`online page: player ${i} ${key2} actors`, JSON.stringify(f.actors));
          await shot(i, `${round}-${key2}`);
        }
      }
      if (f.screen === 'cutscene' && f.cut?.step === 0) {
        const riders = f.actors.filter((a) => a.rider && a.model).map((a) => `${a.binding}:${a.rider}`);
        actorsSeen[i] = [...new Set([...actorsSeen[i], ...riders])];
      }
      // the whole card (its open animation is over after 33 ticks), before Continue
      if (f.screen === 'mp-card' && !f.card?.ready && cardSince[i] && Date.now() - cardSince[i] > 1500 && !seen[i].has('card-full')) {
        seen[i].add('card-full');
        await shot(i, `${round}-mp-card-full`);
      }
      if (f.screen === 'mp-card' && !f.card?.ready) {
        cardSince[i] ||= Date.now();
        const presses = i === 0 || round === 1;
        if (presses && Date.now() - cardSince[i] > 2500) await browsers[i].eval('(()=>{ssxQA.ui().mpUI.choose(0);return 1})()');
      }
    }
    if (now.every((f) => f.screen === 'game')) break;
    await sleep(200);
  }
  const logs = await Promise.all(browsers.map((page) => page.eval('window.__screenLog')));
  for (const [i, log] of logs.entries()) console.log(`online page: player ${i} screens`, JSON.stringify(log));
  const at = (log, screen) => log.find((e) => e.screen === screen)?.at ?? null;
  for (const [i, log] of logs.entries()) {
    assert.ok(at(log, 'cutscene') != null, `player ${i} plays the intro`);
    assert.ok(at(log, 'mp-card') != null, `player ${i} gets the card`);
    assert.ok(at(log, 'game') > at(log, 'mp-card'), `player ${i} starts the race after the card`);
  }
  console.log('online page: intro scripts', JSON.stringify(scripts), 'rider actors', JSON.stringify(actorsSeen));
  for (const [i, list] of actorsSeen.entries())
    if (scripts[i] != null) assert.ok(list.length >= 2, `player ${i} draws both racers in the start hut`);
  assert.deepEqual(actorsSeen[0], actorsSeen[1], 'both pages bind the same racers to the same actors');
  assert.equal(scripts[0], scripts[1], 'both pages play the same intro script');
  const intro = logs.map((log) => at(log, 'cutscene'));
  console.log(`online page: intro start spread ${Math.abs(intro[0] - intro[1])} ms, game start spread ${Math.abs(at(logs[0], 'game') - at(logs[1], 'game'))} ms`);
  assert.ok(Math.abs(intro[0] - intro[1]) < 1000, 'the intro starts on the shared instant on both pages');
  assert.ok(Math.abs(at(logs[0], 'game') - at(logs[1], 'game')) < 1000, 'the race starts on the shared GO on both pages');
  if (round === 0) {
    const card1 = at(logs[1], 'mp-card'), game1 = at(logs[1], 'game');
    assert.ok(game1 - card1 >= 19000, "player 1's card waits for its 20 s auto-continue");
  }
}
try {
  mp = spawn(process.execPath, ['server/mp-server.mjs', '--port', String(port), '--host', '127.0.0.1'], {
    cwd: new URL('.', import.meta.url), stdio: ['ignore', 'pipe', 'inherit']
  });
  await new Promise((resolve) => mp.stdout.once('data', resolve));
  process.env.MP_PORT = String(port);
  server = await startServer();
  for (const [i, rider] of ['zoe', 'mac'].entries()) {
    const raw = i === 1 && useWebKit ? await startWebKit({ width: 640, height: 480 }) : await startBrowser({ width: 640, height: 480 });
    assert.ok(raw, 'required browser is available');
    const b = { raw, eval: raw.evaluate ?? raw.eval, waitFor: raw.waitFor, close: raw.close };
    browsers.push(b);
    // PAGE_PV: headless-chrome.mjs adds it to Chrome's URL; the WebKit driver does not, so it is added here
    const wkPv = i === 1 && useWebKit && process.env.PAGE_PV ? `&pv=${process.env.PAGE_PV}` : '';
    await raw.goto(`${server.origin}/?qa=1&perf=1&course=${course}&rider=${rider}&${preRace ? '' : 'cutscenes=0&'}quality=low&mute=1${wkPv}`);
    await b.waitFor('!!window.ssxQA', 180000);
    await b.eval(installErrors);
    if (i === 1 && useWebKit && process.env.MP_WK_TIMER === '1') {
      // Explicit automation fallback for a locked display: WebKit suspends rAF and throttles page timers. The
      // driver pumps queued callbacks at real wall time; simulation, loading work and network code stay unchanged.
      await b.eval(`(()=>{
        const animation=window.__perfRenderer._animation;
        animation.stop();
        const callbacks=new Map();
        let id=0;
        window.requestAnimationFrame=fn=>{callbacks.set(++id,fn);return id;};
        window.cancelAnimationFrame=id=>callbacks.delete(id);
        window.__mpFrame=()=>{
          const batch=[...callbacks.values()];
          callbacks.clear();
          for(const fn of batch)fn(performance.now());
          return 1;
        };
        animation.start();
        return 1;
      })()`);
      let pumping = false;
      b.pump = setInterval(async () => {
        if (pumping) return;
        pumping = true;
        try { await b.eval('window.__mpFrame()'); }
        catch (error) { b.pumpError = error; }
        finally { pumping = false; }
      }, 16);
      console.log('online page: WebKit uses driver-pumped rendering', await b.eval('document.visibilityState'));
    }
    await b.eval('(async()=>{await ssxQA.ui().mpUI.session.connect();return 1})()');
    console.log(`online page: player ${i} connected`);
  }
  const [a, b] = browsers;
  await a.eval('ssxQA.ui().mpUI.session.createLobby()');
  await a.waitFor('!!ssxQA.ui().mpUI.client.state.lobby', 10000);
  const id = await a.eval('ssxQA.ui().mpUI.client.state.lobby.id');
  await b.eval(`ssxQA.ui().mpUI.session.joinLobby(${JSON.stringify(id)})`);
  await b.waitFor('ssxQA.ui().mpUI.client.state.lobby?.members.length===2', 10000);
  // both pages on the lobby screen as after main menu -> Online (ui.onlineMode: a load in the lobby returns to it)
  for (const page of browsers) await page.eval(`(()=>{const u=ssxQA.ui();u.onlineMode=true;u.set('mp-lobby');return 1})()`);
  if (pick) {
    const before = await Promise.all(browsers.map((page) => page.eval(flowOf)));
    console.log('online page: lobby before the course choice', JSON.stringify(before));
    await a.eval(`(()=>{const u=ssxQA.ui();u.set('mp-lobby');u.mpUI.choose(2);return 1})()`);
    await sleep(300);
    const opened = await a.eval(flowOf);
    console.log('online page: after the Course row', JSON.stringify(opened));
    await shot(0, 'course-row');
    if (opened.screen === 'fe-peak') {
      // the Single Event selector: Select Peak -> Select Mode -> Select Event, then Cross on the course
      const target = await a.eval(`(()=>{const s=ssxQA.ui().eventSelect;
        for(const peak of [1,2,3])for(const mode of ['race','freestyle']){const k=s.events(peak,mode).findIndex(e=>e.code===${JSON.stringify(pick)}&&e.playable);
          if(k>=0)return {peak,mode,k};}
        return null})()`);
      assert.ok(target, `${pick} is a playable lobby course`);
      const step = async (screen, index, name) => {
        await a.waitFor(`ssxQA.ui().screen===${JSON.stringify(screen)}`, 10000);
        await sleep(400);
        await shot(0, name);
        await a.eval(`(()=>{ssxQA.ui().eventSelect.choose(${index});return 1})()`);
      };
      await step('fe-peak', 3 - target.peak, 'select-peak');
      await step('fe-mode', target.mode === 'race' ? 0 : 1, 'select-mode');
      await step('fe-event', target.k, 'select-event');
      await a.waitFor(`ssxQA.ui().screen==='mp-lobby'`, 10000);
      await b.waitFor(`ssxQA.ui().mpUI.client.state.lobby?.course===${JSON.stringify(pick)}`, 10000);
      // nothing loads in the lobby: both pages keep their course and stay on the lobby screen for a few seconds
      for (let k = 0; k < 6; k++) {
        const now = await Promise.all(browsers.map((page) => page.eval(flowOf)));
        for (const [i, f] of now.entries()) {
          assert.equal(f.course, before[i].course, `player ${i} does not load the picked course in the lobby`);
          assert.equal(f.screen, 'mp-lobby', `player ${i} stays in the lobby`);
          assert.equal(f.lobby, pick, `player ${i} sees the picked course`);
        }
        await sleep(500);
      }
      for (const i of [0, 1]) await shot(i, 'lobby-picked');
    } else {
      // the old Course row: every step loads the next course on both pages, then both return to the lobby
      const trace = [];
      const until = Date.now() + 30000;
      while (Date.now() < until) {
        const now = await Promise.all(browsers.map((page) => page.eval(flowOf)));
        trace.push(now.map((f) => `${f.screen}/${f.course}/${f.lobby}`).join(' | '));
        if (trace.length === 2) for (const i of [0, 1]) await shot(i, 'course-loading');
        if (now.every((f) => f.screen === 'mp-lobby' && f.course === f.lobby) && trace.length > 2) break;
        await sleep(500);
      }
      console.log('online page: old Course row flow\n  ' + [...new Set(trace)].join('\n  '));
      for (const i of [0, 1]) await shot(i, 'lobby-after-load');
    }
  }
  for (let round = 0; round < 2; round++) {
    await b.eval('ssxQA.ui().mpUI.client.ready(true)');
    await a.waitFor('ssxQA.ui().mpUI.client.state.lobby.members[1].ready', 10000);
    for (const page of browsers) await page.eval('window.__screenLog.length=0');
    await a.eval('ssxQA.ui().mpUI.client.start()');
    if (preRace) await preRaceFlow(round);
    for (const [i, page] of browsers.entries()) {
      const until = Date.now() + 90000;
      while (await page.eval('ssxQA.ui().screen') !== 'game') {
        assert.ok(Date.now() < until, `player ${i} loads the online race`);
        await sleep(5000);
        console.log(`online page: player ${i} loading`, await page.eval(status));
      }
    }
    if (pick && round === 0) {
      const flows = await Promise.all(browsers.map((page) => page.eval(flowOf)));
      for (const [i, f] of flows.entries()) assert.equal(f.course, f.lobby, `player ${i} races on the lobby's course`);
    }
    const starts = await Promise.all(browsers.map(page => page.eval(status)));
    const deadline = Date.now() + 30000;
    let states;
    while (Date.now() < deadline) {
      await sleep(1000);
      states = await Promise.all(browsers.map(page => page.eval(status)));
      for (const page of browsers) assert.ifError(page.pumpError);
      for (const s of states) assert.deepEqual(s.errors, [], 'online page must not throw at gate opening');
      if (states.every(s => s.remote[0] >= 720)) break;
    }
    for (const [i, s] of states.entries()) {
      assert.ok(s.remote[0] >= 720, `player ${i} receives at least 12 seconds of peer simulation`);
      assert.ok(s.pairs.checks > 0, `player ${i} runs network pair checks`);
      assert.ok(Math.hypot(...s.position.map((v, k) => v - starts[i].position[k])) > 10, `player ${i} rides past the gates`);
      if (process.env.MP_SHOTS) {
        fs.mkdirSync(process.env.MP_SHOTS, { recursive: true });
        const file = `${process.env.MP_SHOTS}/${useWebKit ? 'mixed' : 'chrome'}-${round}-${i}.png`;
        if (browsers[i].raw.shot) await browsers[i].raw.shot(file, '#stage');
        else fs.writeFileSync(file, Buffer.from(await browsers[i].raw.screenshot(), 'base64'));
      }
    }
    console.log(`online page round ${round + 1} (${useWebKit ? 'Chrome + WebKit' : 'Chrome + Chrome'}):`, JSON.stringify(states));
    for (const page of browsers) await page.eval('ssxQA.ui().mpUI.session.quitRace()');
    await a.waitFor('!ssxQA.ui().mpUI.client.state.lobby?.race', 10000);
  }
} catch (error) {
  for (const [i, b] of browsers.entries()) console.error('online page failure', i, await b.eval(status).catch(String));
  throw error;
} finally {
  for (const b of browsers) {
    clearInterval(b.pump);
    await b.close();
  }
  await server?.close();
  mp?.kill();
}
console.log('online page: two players, synchronized starts, gate collisions, streamed movement and a second race OK');
