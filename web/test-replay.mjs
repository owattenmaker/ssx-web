// The race replay (web/replay.js, web/replay-ui.js, web/replay_camera.inc; docs/replay.md).
//   node: the pad recording (RLE, out-of-band calls, highlight skip points) without a core.
//   Chrome (?pv=replay): a Single Event run with a scripted pad and a Give Up, its results, then the replay behind them: every
//   replayed tick's ?simtrace hash equals the live run's (riders, poses, world states, both RNG streams, the score object),
//   a second loop too; the post-race state (the career save, the relationships, the results) is what the run left; the
//   Replay item opens the full replay (paused on its first frame, Play / camera cycle / timeline / Replay Menu -> Exit
//   replay back to the results and their replay); the Web-cam follows the course's camera triggers. Then a backcountry rival
//   run (rolling start) and a super pipe run, replayed exactly. BHP1-PS2: a neutral The Junction run to its finish, the auto
//   replay's Web-cam cuts and trigger cameras on the PS2's ticks (menus/replay/bhp1-neutral). MOUNTAIN: a CTM free ride (a
//   streamed world, no replay on the PS2) asks for no camera triggers ("Replay cameras unavailable" came from it, 2026-10-01).
//   node test-replay.mjs [--only ARA1,ABC1,...]
//   CORE_DIR=dir (a CORE_OUT=dir sh web/build-core.sh core) / ASSETS_DIR=dir (staged assets: <CODE>/camera-triggers.json,
//   UI/replay-screens.json; files there win over web/public/assets): check a scratch build before it goes live.
// The browser part is skipped without Chrome / WebGPU, or with a core that predates the replay view (no replay_camera_step).
import assert from 'node:assert/strict';
import { createRecording, createReplay, REPLAY_CAMERAS } from './replay.js';
import fs from 'node:fs';
import { startBrowser, startServer, sleep } from './headless-chrome.mjs';

// ---- node: recording ----
{
  const rec = createRecording(), pads = [];
  for (let t = 0; t < 500; t++) { const p = new Float32Array(24); if (t % 100 < 40) p[10] = 1; if (t > 250) p[20] = Math.fround(0.25 + (t % 7) / 10); pads.push(p); rec.push(p); if (t === 300) rec.note('giveUp'); }
  assert.equal(rec.ticks, 500);
  const out = new Float32Array(24), cursor = { run: 0 };
  for (let t = 0; t < 500; t++) assert.deepEqual(Array.from(rec.pad(t, out, cursor)), Array.from(pads[t]), `pad of tick ${t}`);
  assert.deepEqual(Array.from(rec.pad(42, out)), Array.from(pads[42]), 'random access');
  assert.deepEqual(rec.eventsAt(301).map((e) => e.kind), ['giveUp'], 'a call between ticks 300 and 301 comes before tick 301');
  assert(rec.runs < 300 && rec.bytes < 4000, `change stream: ${rec.runs} records, ${rec.bytes} bytes`);
  // a keyboard / pad channel is a pad byte (web/pad-input.js): one byte a change; an analog stick moving every tick
  const kb = createRecording(), B = Math.fround(255 * new Float32Array(new Uint32Array([0x3b808081]).buffer)[0]);
  const stick = (b) => { const n = Math.max(Math.trunc((79 - b) * 255 / 79), 0), p = Math.max(Math.trunc((b - 176) * 255 / 79), 0), f = (x) => { const y = new Float32Array([x * new Float32Array(new Uint32Array([0x3b808081]).buffer)[0]]); if (Math.abs(y[0]) > Math.abs(x * new Float32Array(new Uint32Array([0x3b808081]).buffer)[0])) new Uint32Array(y.buffer)[0] -= 1; return y[0]; }; return [f(n), f(p)]; };
  const kbPads = [];
  for (let t = 0; t < 3000; t++) { const p = new Float32Array(24); if (t % 90 < 45) p[10] = B; const [n, q] = stick((t * 7) % 256); p[20] = n; p[21] = q; kbPads.push(p); kb.push(p); }
  const cur = {}; for (let t = 0; t < 3000; t++) assert.deepEqual(Array.from(kb.pad(t, out, cur)), Array.from(kbPads[t]), `analog pad of tick ${t}`);
  assert(kb.bytes < 3000 * 12, `an analog stick moving every tick: ${kb.bytes} bytes for 3000 ticks`);
  assert.equal(REPLAY_CAMERAS.length, 9); assert.equal(REPLAY_CAMERAS[0].name, '-  Web-cam');
  // skip points: the start, the kept highlight buckets (take-off bucket of a landing worth 1000 / a 5 s jump / a crash), the end
  let restarts = 0;
  const host = { allowed: () => true, snapshot: () => ({}), restart: () => (restarts++, true), simulate: () => ({}), present: () => {}, event: () => {}, ended: () => {} };
  const r = createReplay(host); r.liveStart();
  for (let t = 0; t < 2000; t++) {
    r.record(new Float32Array(24));
    const air = (t >= 130 && t < 200) || (t >= 700 && t < 1100) || (t >= 1500 && t < 1510);
    r.observe(!air, t >= 200 ? 1500 : 0, false, false);
  }
  r.finish();
  assert.deepEqual(r.highlights, [120, 660], `highlights ${r.highlights}`);   // 130 (+1500 points), 700 (6.7 s); 1500 (short, no points) dropped
  assert(r.start('full')); assert.equal(r.tick, 1); assert(r.paused);
  assert(r.skip(1)); while (r.seeking) r.frame(1 / 60); assert.equal(r.tick, 120);
  assert(r.skip(1)); while (r.seeking) r.frame(1 / 60); assert.equal(r.tick, 660);
  assert(r.skip(1)); while (r.seeking) r.frame(1 / 60); assert.equal(r.tick, 2000);
  assert(!r.skip(1), 'nothing after the end');
  const before = restarts; assert(r.skip(-1)); while (r.seeking) r.frame(1 / 60); assert.equal(r.tick, 660); assert.equal(restarts, before + 1, 'back = from the start again');
  console.log('Replay recording: pad change stream + calls, highlight skip points OK');
}

// ---- browser ----
const only = process.argv.includes('--only') ? new Set(process.argv[process.argv.indexOf('--only') + 1].split(',')) : null;
const CASES = [
  { key: 'ARA1', course: 'ARA1', ticks: 1500, full: true },
  { key: 'ABC1', course: 'ABC1', mode: 4, ticks: 1200 },
  { key: 'BHP1', course: 'BHP1', ticks: 1200 },
].filter((c) => !only || only.has(c.key));
const browser = await startBrowser({ width: 640, height: 480 });
if (!browser) { console.log('Replay browser check SKIPPED: no Chrome found (set CHROME=/path)'); process.exit(0); }
// The page from web/ (web/headless-chrome.mjs startServer), with CORE_DIR / ASSETS_DIR served over web/runtime and web/public/assets.
async function startOverlayServer() {
  const coreDir = process.env.CORE_DIR, assetsDir = process.env.ASSETS_DIR;
  if (!coreDir && !assetsDir) return startServer();
  const { createServer } = await import('vite');
  const plugin = { name: 'replay-scratch', configureServer(server) { server.middlewares.use((req, res, next) => {
    const url = (req.url || '').split('?')[0];
    const m = coreDir && /^\/runtime\/core\.(js|wasm)$/.exec(url), a = assetsDir && /^\/assets\/(.+)$/.exec(url);
    const file = m ? `${coreDir}/core.${m[1]}` : a && fs.existsSync(`${assetsDir}/${a[1]}`) ? `${assetsDir}/${a[1]}` : null;
    if (!file) return next();
    res.setHeader('Content-Type', file.endsWith('.js') ? 'application/javascript' : file.endsWith('.wasm') ? 'application/wasm' : 'application/json');
    res.setHeader('Cache-Control', 'no-store'); res.end(fs.readFileSync(file)); }); } };
  const server = await createServer({ root: new URL('.', import.meta.url).pathname, logLevel: 'error', plugins: [plugin], server: { host: '127.0.0.1', port: 29000 + Math.floor(Math.random() * 2000), strictPort: false, hmr: false } });
  await server.listen();
  return { origin: server.resolvedUrls.local[0].replace(/\/$/, ''), close: () => server.close() };
}
const assetExists = (rel) => fs.existsSync(new URL(`./public/assets/${rel}`, import.meta.url)) || (!!process.env.ASSETS_DIR && fs.existsSync(`${process.env.ASSETS_DIR}/${rel}`));
const server = await startOverlayServer();
const failures = [], rows = [];
try {
  if (!(await browser.hasWebGPU(server.origin))) { console.log('Replay browser check SKIPPED: headless Chrome has no WebGPU adapter here'); process.exit(0); }
  for (const c of CASES) {
    const t0 = Date.now();
    await browser.goto(`${server.origin}/?qa=1&course=${c.course}&rider=zoe&cutscenes=0&quality=low&simtrace=1&mute=1&pv=replay`);
    await browser.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
    const loadError = await browser.evaluate('document.body.dataset.loadError || ""'); if (loadError) { failures.push(`${c.key}: load failed: ${loadError}`); continue; }
    const entry = await browser.evaluate(`(()=>{const ui=ssxQA.ui(),e=ui.courses.find(x=>x.code===${JSON.stringify(c.course)}&&(${c.mode ?? 0}?x.rivalMode===${c.mode ?? 0}:!x.rivalMode));if(!e)return null;ui.startSingleEvent(e);return e.name})()`);
    if (!entry) { failures.push(`${c.key}: no Single Event entry`); continue; }
    await browser.waitFor(`document.getElementById('stage')?.dataset.screen === 'ctm-objectives' || !!document.body.dataset.loadError`, 300000);
    await browser.waitFor('!!window.ssxEffects?.core', 60000);
    if (!(await browser.evaluate('!!window.ssxEffects.core._replay_camera_step'))) { console.log('Replay browser check SKIPPED: the core predates the replay view (rebuild web/runtime with web/build-core.sh)'); break; }
    // The frame clock stops (frames keep drawing with dt 0): only the test ticks. A scripted pad (tuck / steer / grab), then Give Up.
    await browser.evaluate(`(()=>{const raf=window.requestAnimationFrame.bind(window),at=performance.now();window.requestAnimationFrame=cb=>raf(()=>cb(at));
      ssxQA.ui().careerUI.play();const B=Math.fround(255*new Float32Array(new Uint32Array([0x3b808081]).buffer)[0]);
      window.__pad=(t)=>{const p=new Array(24).fill(0);if(t%400<200)p[10]=B;if(t%300>150)p[21]=0.6;else if(t%300>60)p[20]=0.5;if(t%700>600)p[11]=B;return p;};
      window.__run=(from,n)=>{for(let t=from;t<from+n;t++)ssxQA.advance(1,window.__pad(t));return 1};return 1})()`);
    await sleep(300);
    const base = await browser.evaluate('window.__simTrace.ticks.length');
    for (let i = 0; i < c.ticks; i += 200) await browser.evaluate(`window.__run(${i},${Math.min(200, c.ticks - i)})`);
    await browser.evaluate('(()=>{ssxQA.ui().careerUI.giveUp();return 1})()');
    let screen = ''; for (let k = 0; k < 40 && !/results|award|records/.test(screen); k++) { await browser.evaluate('(()=>{for(let i=0;i<20;i++)ssxQA.advance(1,new Array(24).fill(0));return 1})()'); screen = await browser.evaluate('ssxQA.ui().screen'); }
    if (!/results|award|records/.test(screen)) { failures.push(`${c.key}: no results screen (${screen})`); continue; }
    const post = await browser.evaluate(`JSON.stringify({save:localStorage.getItem('ssx3.career.v2'),rel:[localStorage.getItem('ssx3.relationships.v1'),sessionStorage.getItem('ssx3.relationships.session.v1')],result:ssxQA.ui().careerUI?.result??null,screen:ssxQA.ui().screen})`);
    const R = await browser.evaluate('(()=>{const r=ssxQA.replay();return {finish:r.finishTick,runs:r.recording.runs,bytes:r.recording.bytes,events:r.recording.events.length,avail:r.available()}})()');
    await sleep(400);   // a frame: the replay starts behind the results
    const started = await browser.evaluate('(()=>{const r=ssxQA.replay();return {active:r.active,mode:r.mode,tick:r.tick}})()');
    if (!started.active || started.mode !== 'auto') { failures.push(`${c.key}: the replay did not start behind the results ${JSON.stringify(started)}`); continue; }
    const F = R.finish, repStart = await browser.evaluate('window.__simTrace.ticks.length');
    for (let i = 0; i <= F; i += 400) await browser.evaluate(`(()=>{const r=ssxQA.replay();for(let k=0;k<400&&r.tick<=${F};k++)r.frame(1/60);return r.tick})()`);
    const cam = await browser.evaluate('(()=>{const c=window.ssxEffects.core;return c._replay_camera_info?Array.from(new Float32Array(c.HEAPF32.buffer,c._replay_camera_info(),10)):null})()');
    // a second loop (the rewind after the finish tick), its first 600 ticks
    const loop2 = await browser.evaluate(`(()=>{const r=ssxQA.replay();let n=0;while(r.loops<1&&n<50){r.frame(1/60);n++;}const s=window.__simTrace.ticks.length-r.tick;for(let k=0;k<600;k++)r.frame(1/60);return {start:s,loops:r.loops}})()`);
    const live = await browser.evaluate(`window.__simTrace.ticks.slice(${base},${base + F + 1})`);
    const rep = await browser.evaluate(`window.__simTrace.ticks.slice(${repStart},${repStart + F + 1})`);
    const rep2 = await browser.evaluate(`window.__simTrace.ticks.slice(${loop2.start},${loop2.start + 600})`);
    const bad = (a, b, n) => { for (let i = 0; i < n; i++) if (a[i] !== b[i]) return i; return -1; };
    const m1 = rep.length === F + 1 ? bad(live, rep, F + 1) : -2, m2 = bad(live, rep2, Math.min(600, F + 1));
    if (m1 !== -1) failures.push(`${c.key}: the replay leaves the live run at tick ${m1} of ${F + 1}`);
    if (m2 !== -1) failures.push(`${c.key}: the second loop leaves the live run at tick ${m2}`);
    const post2 = await browser.evaluate(`JSON.stringify({save:localStorage.getItem('ssx3.career.v2'),rel:[localStorage.getItem('ssx3.relationships.v1'),sessionStorage.getItem('ssx3.relationships.session.v1')],result:ssxQA.ui().careerUI?.result??null,screen:ssxQA.ui().screen})`);
    if (post2 !== post) failures.push(`${c.key}: the replay changed the post-race state`);
    let full = '';
    if (c.full && assetExists('UI/replay-screens.json')) {   // the Replay item (its overlay needs the 64replay export)
      const f = await browser.evaluate(`(()=>{const ui=ssxQA.ui(),r=ssxQA.replay();const items=ui.items();const k=items.indexOf('Replay');if(k<0||ui.careerUI.disabled(ui.screen,k))return {err:'Replay item greyed: '+items.join('/')};ui.index=k;ui.choose(k);
        const a={screen:ui.screen,mode:r.mode,paused:r.paused,tick:r.tick,cam:r.cameraName};r.playPause();for(let i=0;i<300;i++)r.frame(1/60);const b={tick:r.tick,paused:r.paused};
        const U=ui.replayUi;U.key({code:'Escape'});U.key({code:'Escape'});const c1=r.cameraName;U.key({code:'ShiftLeft'});const tl=U.timeline;U.key({code:'Enter'});const menu=U.menu;U.key({code:'ArrowDown'});U.key({code:'Space'});
        return {a,b,c1,tl,menu,after:{screen:ui.screen,mode:r.mode,tick:r.tick}};})()`);
      full = JSON.stringify(f);
      if (f.err) failures.push(`${c.key}: ${f.err}`);
      else {
        if (f.a.screen !== 'replay' || f.a.mode !== 'full' || !f.a.paused || f.a.tick !== 1 || f.a.cam !== '-  Web-cam') failures.push(`${c.key}: the full replay did not open paused on its first frame ${JSON.stringify(f.a)}`);
        if (f.b.tick !== 301 || f.b.paused) failures.push(`${c.key}: Play did not run the replay ${JSON.stringify(f.b)}`);
        if (f.c1 !== '-  Near-cam' || f.tl !== false || f.menu !== 0) failures.push(`${c.key}: camera cycle / timeline / menu ${full}`);
        if (!/results|award|records/.test(f.after.screen) || f.after.mode !== 'auto' || f.after.tick !== 0) failures.push(`${c.key}: Exit replay did not go back to the results and their replay ${JSON.stringify(f.after)}`);
      }
    }
    rows.push(`${c.key} (${entry}): run ${F + 1} ticks, pad ${R.runs} runs / ${R.bytes} bytes, ${R.events} call(s); replay exact ${m1 === -1}, loop 2 exact ${m2 === -1}; camera [type ${cam?.[0]}, triggers ${cam?.[4]}, fired ${cam?.[8]}]; ${((Date.now() - t0) / 1000).toFixed(0)} s`);
    if (c.full && assetExists(`${c.course}/camera-triggers.json`) && !(cam?.[4] > 0 && cam?.[8] > 0)) failures.push(`${c.key}: the Web-cam fired no camera trigger (${cam})`);
  }
  // The Web-cam against the PS2 (menus/replay/bhp1-neutral: The Junction, a neutral pad to the finish, then the auto replay).
  // PS2 director log: the view's algorithm and its Bounded eye (= the trigger's bound point) per replay frame; page tick = frame + 1.
  // [tick, type, trigger]: DEFAULT_3 0x3D, Bounded 0x5B on that trigger's enter camera (its point and fov). Two switches fall in a poll
  // gap of the capture (frames 397..429: trigger 1, 3597..3629: trigger 7); their ticks here are the page's, inside the gap.
  if (!only || only.has('BHP1-PS2')) {
    const PS2_CUTS = [[1, 0x3d, -1], [244, 0x5b, 0], [403, 0x5b, 1], [844, 0x3d, 1], [870, 0x5b, 2], [949, 0x5b, 3], [1250, 0x5b, 2],
      [1470, 0x3d, 2], [1536, 0x5b, 3], [1845, 0x5b, 5], [2127, 0x5b, 4], [2136, 0x3d, 4], [2362, 0x5b, 4], [2437, 0x5b, 5],
      [2718, 0x5b, 4], [2958, 0x5b, 6], [2962, 0x3d, 6], [3237, 0x5b, 6], [3541, 0x5b, 8], [3616, 0x5b, 7], [3837, 0x3d, 7],
      [3901, 0x5b, 8], [4185, 0x5b, 9]];
    const GAPS = [[397, 431], [3597, 3631]];
    await browser.goto(`${server.origin}/?qa=1&course=BHP1&rider=zoe&cutscenes=0&quality=low&mute=1&pv=replay`);
    await browser.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
    await browser.evaluate(`(()=>{const ui=ssxQA.ui(),e=ui.courses.find(x=>x.code==='BHP1'&&!x.rivalMode);ui.startSingleEvent(e);return 1})()`);
    await browser.waitFor(`document.getElementById('stage')?.dataset.screen === 'ctm-objectives'`, 300000);
    await browser.waitFor('!!window.ssxEffects?.core', 60000);
    await browser.evaluate(`(()=>{const raf=window.requestAnimationFrame.bind(window),at=performance.now();window.requestAnimationFrame=cb=>raf(()=>cb(at));
      ssxQA.ui().careerUI.play();return 1})()`);
    await sleep(300);
    let screen = '';
    for (let n = 0; n < 6000 && !/results|award|records/.test(screen); n += 100) {
      await browser.evaluate('(()=>{for(let i=0;i<100;i++)ssxQA.advance(1,new Array(24).fill(0));return 1})()');
      screen = await browser.evaluate('ssxQA.ui().screen');
    }
    const finish = await browser.evaluate('ssxQA.replay().finishTick');
    await sleep(400);
    const cuts = await browser.evaluate(`(()=>{const r=ssxQA.replay(),c=window.ssxEffects.core,s=[];let last='';
      for(let k=0;k<${finish}+60&&r.loops<1;k++){r.frame(1/60);const v=new Float32Array(c.HEAPF32.buffer,c._replay_camera_info(),10);
        const key=v[0]+'/'+v[9];if(key!==last){s.push([r.tick,v[0],v[9]]);last=key;}}return s})()`);
    const inGap = (t) => GAPS.some(([a, b]) => t >= a && t < b);
    const same = cuts.length === PS2_CUTS.length && cuts.every((x, k) => x[1] === PS2_CUTS[k][1] && x[2] === PS2_CUTS[k][2] && (x[0] === PS2_CUTS[k][0] || inGap(x[0])));
    if (finish !== 4402) failures.push(`BHP1-PS2: the neutral run finishes at tick ${finish}, the PS2's at 4402`);
    if (!same) failures.push(`BHP1-PS2: the Web-cam's cuts differ from the PS2's: ${JSON.stringify(cuts)}`);
    rows.push(`BHP1-PS2 (neutral pad): finish ${finish}; the Web-cam's ${cuts.length} cuts / trigger cameras on the PS2's ticks: ${same}`);
  }
  // A Conquer the Mountain free ride (a streamed world: no replay there on the PS2, no camera-triggers.json) asks for no triggers
  if (!only || only.has('MOUNTAIN')) {
    await browser.goto(`${server.origin}/manifest.webmanifest`);
    await browser.evaluate('(()=>{localStorage.clear();sessionStorage.clear();return 1})()');
    await browser.goto(`${server.origin}/?qa=1&mute=1&cutscenes=0&quality=low&presentationSeed=0x182200&rider=zoe&pv=replay`);
    await browser.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
    await browser.evaluate(`(()=>{const ui=ssxQA.ui();ui.careerMode=true;ui.onlineMode=false;ui.careerUI.enter();return 1})()`);
    await browser.waitFor(`(document.getElementById('stage').dataset.screen==='game'&&window.demoState?.running)||!!document.body.dataset.loadError`, 300000);
    await sleep(1500);
    const free = await browser.evaluate(`JSON.stringify({course:ssxQA.ui().course?.code,
      asked:performance.getEntriesByType('resource').filter(e=>/camera-triggers/.test(e.name)).map(e=>e.name.replace(location.origin,''))})`);
    const warned = browser.logs.filter((l) => /Replay cameras unavailable/.test(l));
    if (JSON.parse(free).asked.length || warned.length) failures.push(`MOUNTAIN: the free ride asked for camera triggers ${free} ${warned.join(' | ')}`);
    rows.push(`MOUNTAIN free ride: ${free}, no replay camera warning ${!warned.length}`);
  }
} finally { await browser.close(); await server.close(); }
for (const r of rows) console.log(r);
if (failures.length) { console.error('Replay check FAILED:\n' + failures.join('\n')); process.exit(1); }
console.log(`Replay check OK: ${rows.length} check(s): replays tick-exact behind the results, post-race state untouched, the Replay item works, the Web-cam on the PS2's cuts`);
process.exit(0);
