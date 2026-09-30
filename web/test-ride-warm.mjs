// pv rideWarm (docs/ctm-flow.md "Ride start"): a Conquer the Mountain world load loads, sets up and compiles the career rider under
// the load screen, so the ride's first frames do not. Headless Chrome + a private Vite server: a new career from the menus (cutscenes
// off, the frame clock frozen so only ssxQA.advance ticks), the switch off and on:
//  - on: the rider's warm-up ran under the load screen (ride:rider / ride:compile measures), and nothing of the rider's package is
//    fetched after the ride starts;
//  - the ride is the same tick for tick (the human's reference motion, rider state and game RNG, 300 ticks of a pad).
//   node test-ride-warm.mjs     (skips without Chrome / WebGPU or without the game data)
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { startBrowser, startServer, sleep } from './headless-chrome.mjs';
import { PV_DEFAULTS } from './pv-flags.js';

if (!fs.existsSync(new URL('./public/assets/courses.json', import.meta.url))) { console.log('ride warm: skipped (no game data)'); process.exit(0); }
const browser = await startBrowser({ width: 960, height: 720, init: 'performance.setResourceTimingBufferSize(8000)' });
if (!browser) { console.log('ride warm: skipped (no Chrome)'); process.exit(0); }
const server = await startServer();
const E = (x) => browser.evaluate(x);
async function ride(flags) {
  await browser.goto(`${server.origin}/manifest.webmanifest`); await E('(()=>{localStorage.clear();sessionStorage.clear();return 1})()');   // a new career
  await browser.goto(`${server.origin}/?qa=1&mute=1&cutscenes=0&quality=low&presentationSeed=0x182200&rider=zoe&pv=${flags}`);
  await browser.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
  await E(`(()=>{const raf=window.requestAnimationFrame.bind(window),at=performance.now();window.requestAnimationFrame=cb=>raf(()=>cb(at));const ui=ssxQA.ui();ui.careerMode=true;ui.onlineMode=false;ui.careerUI.enter();return 1})()`);
  await browser.waitFor(`(document.getElementById('stage').dataset.screen==='game'&&window.demoState?.running)||!!document.body.dataset.loadError`, 300000);
  const start = await E('performance.now()'); await sleep(1000);
  assert.equal(await E('document.body.dataset.loadError || ""'), '');
  const info = JSON.parse(await E(`JSON.stringify({course:ssxQA.ui().course?.code,warm:performance.getEntriesByType('measure').filter(m=>m.name.startsWith('ride:')).map(m=>m.name),late:performance.getEntriesByType('resource').filter(e=>e.startTime>=${start}-50&&/\\/assets\\/(RIDER_ZOE|WARDROBE\\/ZOE)\\//.test(e.name)&&!/\\/fe\\//.test(e.name)).map(e=>new URL(e.name).pathname)})`));
  const ticks = JSON.parse(await E(`(()=>{const c=window.ssxEffects.core,out=[];for(let i=0;i<300;i++){const pad=new Array(24).fill(0);if(i%150<90)pad[20+(i%300<150?0:1)]=0.8;if(i%80<14)pad[10]=1;if(i%211<30)pad[11]=1;ssxQA.advance(1,pad);out.push([...new Float32Array(c.HEAPF32.buffer,c._reference_motion(),6),...new Float32Array(c.HEAPF32.buffer,c._rider_state(),16),...new Uint32Array(c.HEAPU8.buffer,c._animation_rng_words(),6)].join(','));}return JSON.stringify(out)})()`));
  return { info, ticks };
}
try {
  if (!(await browser.hasWebGPU(server.origin))) { console.log('ride warm: skipped (no WebGPU in headless Chrome)'); process.exit(0); }
  const off = await ride('-rideWarm'), on = await ride('rideWarm');
  // the career free ride's world at quality=low: MOUNTAIN once pv peakRelease is on (web/free-ride.js mountainFreeRide), else the peak world
  const world = PV_DEFAULTS.peakRelease && PV_DEFAULTS.mountainRide ? 'MOUNTAIN' : 'PEAK1';
  assert.equal(off.info.course, world); assert.equal(on.info.course, world);
  assert.deepEqual(on.info.warm.sort(), ['ride:compile', 'ride:rider'], 'the rider warmed under the load screen');
  assert.deepEqual(on.info.late, [], 'nothing of the rider package fetched after the ride started');
  const first = off.ticks.findIndex((x, i) => x !== on.ticks[i]);
  assert.equal(first, -1, `the ride differs at tick ${first}`);
  console.log(`ride warm: off fetched ${off.info.late.length} rider files after the ride started, on none; 300 ticks identical`);
} finally { await browser.close(); await server.close(); }
console.log('ride warm OK: the career rider set up and compiled under the load screen, the ride unchanged');
process.exit(0);
