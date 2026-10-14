// Two real pages, a normal online lobby and two starts through the gates. A prepared offline rider manager must
// not intercept the network pair host (2026-10-14 gate freeze). --webkit runs the second player in Safari's engine.
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import { startBrowser, startServer, sleep } from './headless-chrome.mjs';
import { startWebKit } from './webkit-driver.mjs';

const useWebKit = process.argv.includes('--webkit');
const course = process.env.MP_COURSE || 'ARA1';
const port = 23000 + Math.floor(Math.random() * 1000);
const browsers = [];
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
    await raw.goto(`${server.origin}/?qa=1&perf=1&course=${course}&rider=${rider}&cutscenes=0&quality=low&mute=1`);
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
  for (let round = 0; round < 2; round++) {
    await b.eval('ssxQA.ui().mpUI.client.ready(true)');
    await a.waitFor('ssxQA.ui().mpUI.client.state.lobby.members[1].ready', 10000);
    await a.eval('ssxQA.ui().mpUI.client.start()');
    for (const [i, page] of browsers.entries()) {
      const until = Date.now() + 60000;
      while (await page.eval('ssxQA.ui().screen') !== 'game') {
        assert.ok(Date.now() < until, `player ${i} loads the online race`);
        await sleep(5000);
        console.log(`online page: player ${i} loading`, await page.eval(status));
      }
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
