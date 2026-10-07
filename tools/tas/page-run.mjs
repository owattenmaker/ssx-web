// Play a TAS pad file live in the real page (headless Chrome, --mute-audio, ?mute=1) as a Single Event Metro City race
// (docs/tas.md "Event set-up"), and keep what the page made of it:
//   <out>/run.json     finish (web/game-tick.js rec.finish), place, the career result (medal), the lineup, the attributes
//   <out>/trace.json   the page's ?simtrace hash of every run tick (web/game-tick.js gameTrace)
//   <out>/start.json   the run's start state (replay snapshot, rider, attributes, lineup); <out>/dump.json per-tick states
//   <out>/replay.ssxr  the run's portable replay file (web/server/replay-file.mjs; ui.cb.onlineReplayFile), never uploaded
//
//   node tools/tas/page-run.mjs PAD.tas OUT_DIR [--rider mac] [--course BRA2] [--attributes 55|none] [--ctm [--round 3]]
//
// Nothing is sent to a records server: the page's /mp proxy points at 127.0.0.1:8787, where nothing of ours listens.
import fs from 'node:fs';
import path from 'node:path';
import { startBrowser, startServer, sleep } from '../../web/headless-chrome.mjs';
import { encodeReplayFile } from '../../web/server/replay-file.mjs';
import { parse, toChannels } from './pad-format.mjs';

const args = process.argv.slice(2);
const opt = (k, d) => {
  const i = args.indexOf('--' + k);
  return i < 0 ? d : args[i + 1];
};
const [padPath, outDir] = args.filter((a, i) => !a.startsWith('--') && !(i > 0 && args[i - 1].startsWith('--')));
if (!padPath || !outDir) throw new Error('usage: page-run.mjs PAD.tas OUT_DIR');
const rider = opt('rider', 'mac');
const round = +opt('round', 3);
const ctm = args.includes('--ctm');
const attributes = opt('attributes', '55') === 'none' ? null : +opt('attributes', '55');
const course = opt('course', 'BRA2');
const courseIndex = { ARA1: 0, BRA2: 1 }[course];
const { frames } = parse(fs.readFileSync(padPath, 'utf8'));
fs.mkdirSync(outDir, { recursive: true });

const browser = await startBrowser({ width: 640, height: 480 });
if (!browser) throw new Error('no Chrome');
const server = await startServer();
const E = (js) => browser.evaluate(js);
const t0 = Date.now();
try {
  if (!(await browser.hasWebGPU(server.origin))) throw new Error('headless Chrome has no WebGPU here');
  await browser.goto(`${server.origin}/manifest.webmanifest`);
  await E('(()=>{localStorage.clear();sessionStorage.clear();return 1})()');
  await browser.goto(`${server.origin}/?qa=1&course=${course}&rider=${rider}&cutscenes=0&quality=low&simtrace=1&mute=1`);
  await browser.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
  // The frame clock frozen (frames keep drawing with dt 0): only ssxQA.advance ticks.
  await E(`(()=>{const raf=window.requestAnimationFrame.bind(window),at=performance.now();
    window.requestAnimationFrame=cb=>raf(()=>cb(at));return 1})()`);
  // --attributes N: the profile's seven raw bytes (career rider +0xBDF..+0xBE5; 5 fresh, 55 = level 11, the cap 0x5308D8 + char*15 + 8 + k).
  // careerUI.play() hands them to the physics in every mode (the PS2's stat getters read the profile bank 0x535538 in Single Event too).
  if (attributes != null) {
    await E(`(()=>{const cu=ssxQA.ui().careerUI;cu.career.rider(cu.riderId).attributes=Array(7).fill(${attributes});cu.career.persist();return 1})()`);
  }
  // Single Event (the default, docs/tas.md): the Select Event entry, one final round (0x23A174 forces round 3).
  // --ctm: a fresh career (careerUI.enter() without the free-ride world load), the event begun at --round (a QA-set Final:
  // web/career.js startEvent makes round 1; 0x23A174's round decides the lineup roles and the award at +0x9C).
  const begun = ctm ? await E(`(()=>{
    const ui=ssxQA.ui();
    ui.careerMode=true;
    ui.onlineMode=false;
    const cu=ui.careerUI;
    cu.career.rider(cu.riderId);
    cu.career.seedRoster?.();
    cu.career.persist();
    const start=cu.career.startEvent.bind(cu.career);
    cu.career.startEvent=(...a)=>{const ev=start(...a);ev.round=${round};return ev};
    const ev=cu.begin(0,${courseIndex},true);
    return JSON.stringify({round:ev.round,career:ev.career,roster:ev.roster,rider:cu.riderId,attributes:cu.me.attributes})})()`)
    : await E(`(()=>{const ui=ssxQA.ui(),e=ui.courses.find(x=>x.code==='${course}'&&!x.rivalMode);if(!e)return null;
      ui.startSingleEvent(e);const ev=ui.careerUI.career.active?.ev;return JSON.stringify({entry:e.name,round:ev?.round,career:ev?.career})})()`);
  console.log('event', begun);
  await browser.waitFor(`document.getElementById('stage')?.dataset.screen === 'ctm-objectives' || !!document.body.dataset.loadError`, 300000);
  const loadError = await E('document.body.dataset.loadError || ""');
  if (loadError) throw new Error('load failed: ' + loadError);
  await E('(()=>{ssxQA.ui().careerUI.play();return 1})()');
  await sleep(300);
  // the run's start state (main.js createRaceReplay snapshot, the lineup as onlineReplayFile keeps it): the node harness's input
  const start = await E(`JSON.stringify((()=>{const ui=ssxQA.ui(),r=ssxQA.replay(),ai=ssxQA.aiRace?.(),l=ai?.lineup;
    return {snapshot:{...r.snapshot,inWorld:null},rider:ui.rider?.id,attributes:ui.careerUI?.me?.attributes??null,
      lineup:l?{values:l.values??null,entries:l.entries??null,opponent:l.opponent??null,rival:l.rival??null,mode:l.mode??null,tables:l.tables??null,
        round:ui.careerUI?.career?.active?.ev?.round??3,career:!!ui.careerUI?.career?.active?.ev?.career,enabled:ai.enabled!==false}:null}})())`);
  fs.writeFileSync(path.join(outDir, 'start.json'), start);
  const base = await E('window.__simTrace.ticks.length');
  // per tick: the human's rider state (16 floats), the shared game RNG (6 words), each computer rider's position (3 floats each)
  await E(`(()=>{window.__tasDump=[];window.__tasRow=()=>{const c=window.ssxEffects.core,ai=ssxQA.aiRace?.();
    const row=[...new Float32Array(c.HEAPF32.buffer,c._rider_state(),16),...new Uint32Array(c.HEAPU8.buffer,c._animation_rng_words(),6)];
    for(const n of ai?.racers?.npcs??[])row.push(...new Float32Array(n.core.HEAPF32.buffer,n.core._rider_state(),3));
    window.__tasDump.push(row)};return 1})()`);
  // the run's pad, 400 ticks a call; then neutral until the results
  const CHUNK = 400;
  let finishTick = -1;
  for (let at = 0; at < frames.length && finishTick < 0; at += CHUNK) {
    const chunk = frames.slice(at, at + CHUNK).map((f) => Array.from(toChannels(f)));
    finishTick = await E(`(()=>{const pads=${JSON.stringify(chunk)};const r=ssxQA.replay();
      for(const p of pads){ssxQA.advance(1,p);window.__tasRow();if(r.finishTick>=0)return r.finishTick;}return -1})()`);
  }
  if (finishTick < 0) console.log(`no finish within the pad's ${frames.length} ticks`);
  let screen = '';
  for (let k = 0; k < 80 && !/results|award|records/.test(screen); k++) {
    await E('(()=>{for(let i=0;i<20;i++)ssxQA.advance(1,new Array(24).fill(0));return 1})()');
    screen = await E('ssxQA.ui().screen');
  }
  const info = JSON.parse(await E(`JSON.stringify((()=>{const ui=ssxQA.ui(),r=ssxQA.replay(),cu=ui.careerUI,ai=ssxQA.aiRace?.();
    return {screen:ui.screen,finishTick:r.finishTick,finish:r.finishInfo,result:cu?.result??null,
      standings:ai?.standings?.()??null,lineup:ai?.lineup??null,attributes:cu?.me?.attributes??null}})())`));
  const trace = await E(`window.__simTrace.ticks.slice(${base},${base}+${Math.max(0, finishTick + 1)})`);
  let file = null;
  if (finishTick >= 0) {
    file = await E(`(async()=>{const f=await ssxQA.ui().cb.onlineReplayFile({event:'0:${course}',mode:0,course:'${course}',name:'TAS',
      claim:{ticks:Math.round(ssxQA.replay().finishInfo?.ticks??0)}});return f?{meta:f.meta,pad:Array.from(f.pad)}:null})()`);
  }
  fs.writeFileSync(path.join(outDir, 'run.json'), JSON.stringify({ pad: path.resolve(padPath), ...info, ms: Date.now() - t0 }, null, 1));
  fs.writeFileSync(path.join(outDir, 'trace.json'), JSON.stringify(trace));
  fs.writeFileSync(path.join(outDir, 'dump.json'), await E('JSON.stringify(window.__tasDump)'));
  if (file) {
    fs.writeFileSync(path.join(outDir, 'replay.ssxr'), encodeReplayFile({ meta: file.meta, pad: Uint8Array.from(file.pad) }));
    fs.writeFileSync(path.join(outDir, 'meta.json'), JSON.stringify(file.meta, null, 1));
  }
  const medal = info.result?.medal;
  console.log(`finish tick ${finishTick}, race ticks ${info.finish?.ticks}, screen ${info.screen}, medal ${medal}, ${((Date.now() - t0) / 1000).toFixed(0)} s`);
} finally {
  for (const l of browser.logs.filter((l) => /^(error|exception)/i.test(l)).slice(0, 20)) console.log('LOG', l);
  await browser.close();
  await server.close();
}
