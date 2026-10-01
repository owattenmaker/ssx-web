// pv riderPrefetch (docs/first-load.md "The event's riders and the course parse"): the human rider's files download from the event
// pick, the lineup is planned (web/ai-race.js plan) before the human rider loads so its riders download alongside it, the intro's
// cutscene data under the warm-up. Headless Chrome + a private Vite server, the reference presentation seed:
//  - two Snow Jam events and a Big Air one in a row, twice: the same lineups (roster seeds, riders) and the same
//    first 300 race ticks (the human, every computer rider's reference motion, the game RNG);
//  - no rider file is downloaded twice (each prefetched body is taken by its loader).
//   node test-rider-prefetch.mjs     (skips without Chrome / WebGPU or without the game data)
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { startBrowser, startServer, sleep } from './headless-chrome.mjs';

if (!fs.existsSync(new URL('./public/assets/courses.json', import.meta.url))) { console.log('rider prefetch: skipped (no game data)'); process.exit(0); }
const browser = await startBrowser({ width: 960, height: 720, init: 'performance.setResourceTimingBufferSize(8000)' });
if (!browser) { console.log('rider prefetch: skipped (no Chrome)'); process.exit(0); }
const server = await startServer();
const E = (x) => browser.evaluate(x);
async function session() {
  await browser.goto(`${server.origin}/?qa=1&mute=1&cutscenes=0&quality=low&presentationSeed=0x182200&rider=zoe`);
  await browser.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
  const out = [];
  for (const code of ['ARA1', 'ARA1', 'BRA2']) {
    const from = await E('performance.now()');
    await E(`(()=>{const ui=ssxQA.ui();ui.startSingleEvent(ui.courses.find(c=>c.code===${JSON.stringify(code)}&&!c.rivalMode));return 1})()`);
    await browser.waitFor(`document.getElementById('stage')?.dataset.screen==='ctm-objectives'||!!document.body.dataset.loadError`, 300000);
    assert.equal(await E('document.body.dataset.loadError || ""'), '');
    const lineup = await E('JSON.stringify({values:window.ssxAiRace?.lineup?.values,seed:window.ssxAiRace?.lineup?.seed,names:window.ssxAiRace?.names?.()})');
    const twice = JSON.parse(await E(`JSON.stringify((()=>{const n=new Map();for(const e of performance.getEntriesByType('resource'))if(e.startTime>=${from}&&/\\/assets\\/(RIDER_|WARDROBE\\/)/.test(e.name)){const p=new URL(e.name).pathname;n.set(p,(n.get(p)||0)+1);}return [...n].filter(([,c])=>c>1).map(([p])=>p)})())`));
    await E(`(()=>{window.__raf0=window.__raf0||window.requestAnimationFrame;const raf=window.__raf0.bind(window),at=performance.now();window.requestAnimationFrame=cb=>raf(()=>cb(at));ssxQA.ui().careerUI.play();return 1})()`);
    await sleep(300);
    const ticks = JSON.parse(await E(`(()=>{const c=window.ssxEffects.core,a=window.ssxAiRace,out=[];for(let i=0;i<300;i++){const pad=new Array(24).fill(0);if(i%120<60)pad[20]=0.9;if(i%90<15)pad[10]=1;ssxQA.advance(1,pad);out.push([...new Float32Array(c.HEAPF32.buffer,c._reference_motion(),6),...a.racers.npcs.flatMap(n=>[...new Float32Array(n.core.HEAPF32.buffer,n.core._reference_motion(),6)]),...new Uint32Array(c.HEAPU8.buffer,c._animation_rng_words(),6)].join(','));}return JSON.stringify(out)})()`));
    out.push({ code, lineup, ticks, twice });
    await E(`(()=>{window.requestAnimationFrame=window.__raf0;const ui=ssxQA.ui();ui.cb.quit();ui.set('main');return 1})()`); await sleep(500);
  }
  return out;
}
try {
  if (!(await browser.hasWebGPU(server.origin))) { console.log('rider prefetch: skipped (no WebGPU in headless Chrome)'); process.exit(0); }
  // two sessions: the prefetch and the shared parse leave the lineups and races as they are, run to run
  const off = await session(), on = await session();
  off.forEach((o, i) => {
    const n = on[i], first = o.ticks.findIndex((t, k) => t !== n.ticks[k]);
    assert.equal(n.lineup, o.lineup, `${o.code} #${i + 1}: the lineup`);
    assert.equal(first, -1, `${o.code} #${i + 1}: the race differs at tick ${first}`);
    assert.deepEqual(n.twice, [], `${o.code} #${i + 1}: rider files downloaded twice with the prefetch`);
    console.log(`${o.code} #${i + 1}: lineup ${o.lineup}; 300 ticks identical`);
  });
} finally { await browser.close(); await server.close(); }
console.log('rider prefetch OK: the same lineups and races, every prefetched rider file taken once');
process.exit(0);
