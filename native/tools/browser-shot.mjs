// The web build's frame at a tick of a live run (docs/native.md "N3", visual parity): the game's own page in headless Chrome
// (web/headless-chrome.mjs startBrowser: muted, mock keychain; ?mute=1), a Single Event played with a TAS pad as
// tools/tas/page-run.mjs plays it, the frame clock frozen, N + 2 ticks stepped (ssxQA.advance), the HUD hidden, #stage
// captured. A frame right after a step draws render alpha 0: the camera of tick N (the previous camera pose) over the state of
// tick N + 1, which ssx3_render_shot --tick N draws the same way. Prints {tick, trace} (the page's ?simtrace hash of tick N).
//   node native/tools/browser-shot.mjs PAD.tas OUT.png --tick N [--course ARA1] [--rider zoe] [--attributes 55|none] [--lineup JSON] [--eval JS]
import fs from 'node:fs';
import { startBrowser, startServer, sleep } from '../../web/headless-chrome.mjs';
import { parse, toChannels } from '../../tools/tas/pad-format.mjs';

const args = process.argv.slice(2);
const opt = (k, d) => {
  const i = args.indexOf('--' + k);
  return i < 0 ? d : args[i + 1];
};
const [padPath, out] = args.filter((a, i) => !a.startsWith('--') && !(i > 0 && args[i - 1].startsWith('--')));
if (!padPath || !out) throw new Error('usage: browser-shot.mjs PAD.tas OUT.png --tick N [--course C] [--rider R] [--attributes N|none] [--lineup JSON]');
const tick = +opt('tick', 600);
const course = opt('course', 'ARA1');
const rider = opt('rider', 'zoe');
const lineup = opt('lineup') ? JSON.parse(opt('lineup')) : null;
const attributes = opt('attributes', 'none') === 'none' ? null : +opt('attributes');
const { frames } = parse(fs.readFileSync(padPath, 'utf8'));

const browser = await startBrowser({ width: 640, height: 480 });
if (!browser) {
  console.error('no Chrome');
  process.exit(77);
}
const server = await startServer();
const E = (js) => browser.evaluate(js);
try {
  await browser.goto(`${server.origin}/manifest.webmanifest`);
  // a fresh profile; widescreen Off (the 4:3 frame)
  await E(`(()=>{localStorage.clear();sessionStorage.clear();localStorage.setItem('ssx3.widescreen','0');return 1})()`);
  await browser.goto(`${server.origin}/?qa=1&course=${course}&rider=${rider}&cutscenes=0&quality=low&simtrace=1&mute=1`);
  await browser.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
  await E(`(()=>{const raf=window.requestAnimationFrame.bind(window),at=performance.now();window.requestAnimationFrame=cb=>raf(()=>cb(at));return 1})()`);
  if (attributes != null) {
    await E(`(()=>{const cu=ssxQA.ui().careerUI;cu.career.rider(cu.riderId).attributes=Array(7).fill(${attributes});cu.career.persist();return 1})()`);
  }
  await E(`(()=>{const ui=ssxQA.ui(),e=ui.courses.find(x=>x.code==='${course}'&&!x.rivalMode);
    if(${JSON.stringify(lineup)})ssxQA.aiRace().fixedNext=${JSON.stringify(lineup)};ui.startSingleEvent(e);return 1})()`);
  await browser.waitFor(`document.getElementById('stage')?.dataset.screen === 'ctm-objectives' || !!document.body.dataset.loadError`, 300000);
  await E('(()=>{ssxQA.ui().careerUI.play();return 1})()');
  await sleep(300);
  const base = await E('window.__simTrace.ticks.length');
  const total = tick + 2;
  for (let at = 0; at < total; at += 400) {
    const chunk = frames.slice(at, Math.min(total, at + 400)).map((f) => Array.from(toChannels(f)));
    await E(`(()=>{for(const p of ${JSON.stringify(chunk)})ssxQA.advance(1,p);return 1})()`);
  }
  await E('(()=>{ssxQA.hud(false);return 1})()');
  // a few frames drawn at that state
  await sleep(1500);
  const png = await browser.screenshot('#stage');
  fs.writeFileSync(out, Buffer.from(png, 'base64'));
  const trace = await E(`window.__simTrace.ticks[${base}+${tick}]`);
  // --eval JS: an expression evaluated in the page at that state (diagnostics, e.g. ssxEffects states)
  const extra = opt('eval') ? await E(opt('eval')) : undefined;
  console.log(JSON.stringify({ tick, trace, out, ...(extra === undefined ? {} : { eval: extra }) }));
} finally {
  await browser.close();
  await server.close();
}
process.exit(0);
