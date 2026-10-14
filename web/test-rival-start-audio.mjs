// pv rivalStartAudio (docs/backcountry.md "HUD, speech, music, FX"): an offline backcountry rival event's rolling start runs 2872A8
// (main.js -> web/game-audio.js rivalStart) once, as single player (0x535C11 != 2): the rival's BC_Challenge 0x20D3 (2A1138: the
// rival speaks to the human; speaker mask, subject mask, 4), then the artist intro 0x20CF. ARMSX2 (happiness-ready, Zoe vs Mac):
// 2B1458 event 0x20D3 with speaker 3 (Mac) at the rolling start, the stream Bc_Challenge_mac (local/speech-speaker/ps2/bc-start).
// Off: the start never reached the old phase 3 -> 5 edge (the race starts at phase 5), so nothing ran, and mpGame (always
// created) made every offline start the multiplayer path.
// Real game path: Single Event > Happiness Rival Time (ui.startSingleEvent), the card's Continue (careerUI.play), real frames.
//   node test-rival-start-audio.mjs      (skips without Chrome / WebGPU)
import assert from 'node:assert/strict';
import { startBrowser, startServer, sleep } from './headless-chrome.mjs';

const instrument = `(()=>{
  const a=window.__perfAudio;
  window.__rs=[];
  const f=a.rivalStart;
  a.rivalStart=function(o){window.__rs.push(JSON.stringify(o));return f.apply(this,arguments);};
  return 1;
})()`;
async function run(browser, server, pvList) {
  const q = pvList ? `&pv=${pvList}` : '';
  await browser.goto(`${server.origin}/?qa=1&perf=1&course=ABC1&rider=zoe&cutscenes=0&quality=low&mute=1${q}`);
  await browser.waitFor('!!window.ssxQA', 300000);
  await browser.waitFor('!!window.__perfAudio?.speechEngine', 60000);
  await sleep(3000);
  await browser.evaluate(instrument);
  const entry = await browser.evaluate(`(()=>{const ui=ssxQA.ui(),e=ui.courses.find(x=>x.code==='ABC1'&&x.rivalMode===4);if(!e)return null;ui.startSingleEvent(e);return e.name})()`);
  assert.ok(entry, 'Happiness Rival Time is in the Single Event list');
  await browser.waitFor(`document.getElementById('stage')?.dataset.screen === 'ctm-objectives'`, 300000);
  await browser.evaluate('(()=>{ssxQA.ui().careerUI.play();return 1})()');
  await browser.waitFor(`ssxQA.ui().screen === 'game'`, 60000);
  // the rolling start and the next speech dispatches (2A43B8 fires one pending request per idle edge)
  await sleep(8000);
  return {
    starts: await browser.evaluate('window.__rs'),
    posts: await browser.evaluate('window.__perfAudio.debug().speech.posts')
  };
}

const browser = await startBrowser({ width: 640, height: 480 });
if (!browser) {
  console.log('Rival start audio SKIPPED: no Chrome found (set CHROME=/path)');
  process.exit(0);
}
const server = await startServer();
try {
  if (!(await browser.hasWebGPU(server.origin))) console.log('Rival start audio SKIPPED: headless Chrome has no WebGPU adapter here');
  else {
    const on = await run(browser, server, 'rivalStartAudio');
    assert.deepEqual(on.starts, [JSON.stringify({ rival: 'mac', mp: false })], 'one rolling start, single player, Mac the rival');
    const bc = on.posts.filter((p) => p[1] === '20d3');
    // [tick, event, speaker, speaker mask (Mac 0x2), subject mask (Zoe 0x20), 4]
    assert.equal(bc.length, 1, 'BC_Challenge posted once');
    assert.deepEqual(bc[0].slice(2), [3, 0x2, 0x20, 4], 'Mac challenges Zoe');
    assert.ok(on.posts.some((p) => p[1] === '20cf'), 'the artist intro follows');
    assert.ok(!on.posts.some((p) => p[1] === '20e5'), 'no Radio BIG intro outside multiplayer');
    const off = await run(browser, server, '-rivalStartAudio');
    assert.deepEqual(off.starts, [], 'off: the old phase 3 -> 5 edge never comes');
    console.log('Rival start audio OK: rolling start -> BC_Challenge_mac to Zoe, then the artist intro (off: none)');
  }
} finally {
  await browser.close();
  await server.close();
}
