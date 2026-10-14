// The web build's frame at a tick of a live run (docs/native.md "N3", visual parity): the game's own page in headless Chrome
// (web/headless-chrome.mjs startBrowser: muted, mock keychain; ?mute=1), a Single Event played with a TAS pad as
// tools/tas/page-run.mjs plays it, the frame clock frozen, N + 2 ticks stepped (ssxQA.advance), the HUD hidden, #stage
// captured. A frame right after a step draws render alpha 0: the camera of tick N (the previous camera pose) over the state of
// tick N + 1, which ssx3_render_shot --tick N draws the same way. Prints {tick, trace} (the page's ?simtrace hash of tick N).
//   node native/tools/browser-shot.mjs PAD.tas OUT.png --tick N [--course ARA1] [--rider zoe] [--attributes 55|none] [--lineup JSON] [--eval JS]
//        [--riders ps2|xbox] [--bc 0|1]   the page's ?riders= (the rider texture set; quality=low's default is ps2) and ?bc=
//        [--hud] [--canvas-log OUT.json]  keep the race HUD; record one ui.draw's #ui canvas calls (ssx3_render_shot --canvas-log)
//        [--audio-log OUT.json]           the director timeline and the sfx requests at tick N (?perf=1's __perfAudio; ssx3 --audio-log),
//                                         with each request's tick (playTicks)
//        [--chunk K] [--paced]            K ticks per page call (400); --paced: one tick per animation frame, as a live run steps
//                                         (the frame's work between the ticks: GO, the listener, the effects' frame history)
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
const riders = opt('riders');
const bc = opt('bc');
const canvasLog = opt('canvas-log');
const keepHud = args.includes('--hud') || !!canvasLog;
const audioLog = opt('audio-log');
const chunkSize = Math.max(1, +opt('chunk', 400));
const paced = args.includes('--paced');
if (riders && !['ps2', 'xbox'].includes(riders)) throw new Error('--riders ps2|xbox');
if (bc && !['0', '1'].includes(bc)) throw new Error('--bc 0|1');
const query = (riders ? `&riders=${riders}` : '') + (bc === '0' ? '&bc=0' : '') + (audioLog ? '&perf=1' : '');
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
  await browser.goto(`${server.origin}/?qa=1&course=${course}&rider=${rider}&cutscenes=0&quality=low&simtrace=1&mute=1${query}`);
  await browser.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
  await E(`(()=>{const raf=window.requestAnimationFrame.bind(window),at=performance.now();window.requestAnimationFrame=cb=>raf(()=>cb(at));return 1})()`);
  // the title's Press START unlocks the audio (a user gesture; the headless profile allows autoplay)
  if (audioLog) await E('(async()=>{const a=window.__perfAudio;await a.whenReady();a.unlock();return 1})()');
  if (attributes != null) {
    await E(`(()=>{const cu=ssxQA.ui().careerUI;cu.career.rider(cu.riderId).attributes=Array(7).fill(${attributes});cu.career.persist();return 1})()`);
  }
  await E(`(()=>{const ui=ssxQA.ui(),e=ui.courses.find(x=>x.code==='${course}'&&!x.rivalMode);
    if(${JSON.stringify(lineup)})ssxQA.aiRace().fixedNext=${JSON.stringify(lineup)};ui.startSingleEvent(e);return 1})()`);
  await browser.waitFor(`document.getElementById('stage')?.dataset.screen === 'ctm-objectives' || !!document.body.dataset.loadError`, 300000);
  if (audioLog) await E('(()=>{window.__perfAudio.sfx.logPlays(true);return 1})()');
  await E('(()=>{ssxQA.ui().careerUI.play();return 1})()');
  await sleep(300);
  const base = await E('window.__simTrace.ticks.length');
  const total = tick + 2;
  // the sfx log's length after each tick (playTicks)
  // and the listener each tick's sounds were placed with (the last listener call before it: main.js's camera position, then its
  // matrixWorldInverse)
  if (audioLog) {
    await E(`(()=>{window.__playCounts=[];window.__listeners=[];const a=window.__perfAudio,l=a.listener.bind(a);
      a.listener=(cam,o)=>{const r=l(cam,o);window.__lastListener=cam?[cam.position.x,cam.position.y,cam.position.z,...cam.matrixWorldInverse.elements]:null;return r};
      return 1})()`);
  }
  const after = audioLog ? 'window.__playCounts.push(window.__perfAudio.sfx.plays.length);window.__listeners.push(window.__lastListener??null);' : '';
  const step = paced ? 'await new Promise((r)=>requestAnimationFrame(r));' : '';
  for (let at = 0; at < total; at += chunkSize) {
    const chunk = frames.slice(at, Math.min(total, at + chunkSize)).map((f) => Array.from(toChannels(f)));
    await E(`(async()=>{for(const p of ${JSON.stringify(chunk)}){ssxQA.advance(1,p);${after}${step}}return 1})()`);
  }
  if (audioLog) {
    const log = await E(`(()=>{const a=window.__perfAudio,c=window.__playCounts,t=[];
      for(let k=0,i=0;k<c.length;k++)for(;i<c[k];i++)t.push(k);
      return {timeline:a.timeline(),plays:a.sfx.plays,playTicks:t,posts:a.speechEngine?.debug?.().posts ?? [],listeners:window.__listeners,time:a.engine?.context?.currentTime ?? null}})()`);
    fs.writeFileSync(audioLog, JSON.stringify(log));
  }
  if (!keepHud) await E('(()=>{ssxQA.hud(false);return 1})()');
  // the #ui context's calls from one ui.draw to the next (the reset fade after it included)
  if (canvasLog) {
    await E(`(()=>{
      const ui = ssxQA.ui(), ctx = document.querySelector('#ui').getContext('2d');
      const fnv = (d) => { let h = 2166136261; for (let i = 0; i < d.length; i++) { h ^= d[i]; h = Math.imul(h, 16777619) >>> 0; } return h >>> 0; };
      const nameOf = (im) => {
        if (im instanceof HTMLImageElement) return decodeURIComponent(im.src.split('/').pop().replace(/\\.png$/, ''));
        if (im instanceof HTMLCanvasElement) return 'canvas:' + fnv(im.getContext('2d').getImageData(0, 0, im.width, im.height).data).toString(16).padStart(8, '0');
        return String(im);
      };
      const rec = (window.__canvasRec = { armed: true, on: false, log: null, list: [] });
      const push = (op, a, img) => { if (!rec.on) return; const m = ctx.getTransform(); const e = { op, args: a, m: [m.a, m.b, m.c, m.d, m.e, m.f], alpha: ctx.globalAlpha }; if (img !== undefined) e.img = img; rec.list.push(e); };
      const wrap = (name, fn) => { const orig = ctx[name].bind(ctx); ctx[name] = (...a) => { fn(...a); return orig(...a); }; };
      wrap('drawImage', (im, ...a) => {
        if (!rec.on) return;
        const w = im.naturalWidth || im.width, h = im.naturalHeight || im.height;
        const n = a.length === 2 ? [0, 0, w, h, a[0], a[1], w, h] : a.length === 4 ? [0, 0, w, h, ...a] : a;
        push('drawImage', n, nameOf(im));
      });
      wrap('fillRect', (...a) => push('fillRect', [...a, ctx.fillStyle]));
      wrap('roundRect', (x, y, w, h, r) => push('roundRect', [x, y, w, h, r]));
      wrap('fill', () => { if (typeof ctx.fillStyle !== 'string') push('fill', ['gradient']); });
      wrap('stroke', () => push('stroke', [ctx.strokeStyle, ctx.lineWidth]));
      wrap('moveTo', () => push('arrow', []));
      const draw = ui.draw.bind(ui);
      ui.draw = (s) => {
        if (rec.on) { rec.on = false; rec.log = rec.list; }
        else if (rec.armed) { rec.armed = false; rec.on = true; }
        return draw(s);
      };
      return 1;
    })()`);
    await browser.waitFor('!!window.__canvasRec?.log', 20000);
    fs.writeFileSync(canvasLog, JSON.stringify(await E('window.__canvasRec.log')));
  }
  // a few frames drawn at that state
  await sleep(1500);
  const png = await browser.screenshot('#stage');
  fs.writeFileSync(out, Buffer.from(png, 'base64'));
  const trace = await E(`window.__simTrace.ticks[${base}+${tick}]`);
  // --eval JS: an expression evaluated in the page at that state (diagnostics, e.g. ssxEffects states)
  const extra = opt('eval') ? await E(opt('eval')) : undefined;
  const decode = await E(`(async()=>{try{const m=await import('/texture-archive.js');return m.textureDecodeConfig()}catch(e){return String(e)}})()`);
  console.log(JSON.stringify({ tick, trace, out, riders: riders ?? 'ps2', decode, ...(extra === undefined ? {} : { eval: extra }) }));
} finally {
  await browser.close();
  await server.close();
}
process.exit(0);
