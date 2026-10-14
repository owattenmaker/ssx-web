// The page's front-end screen at a given state (docs/native.md "N3", FE parity; ssx3_ui_shot draws native's): the game's page in
// headless Chrome (web/headless-chrome.mjs startBrowser: muted, mock keychain; ?mute=1), the screen opened over the hidden world with
// its state fixed, one ui.draw recorded (--canvas-log: the browser-shot.mjs recorder's shape), #stage captured.
//   node native/tools/ui-shot.mjs OUT.png --screen loading [--frame F] [--percent P] [--black B] [--hint-counter N]
//        [--device keyboard|gamepad] [--keyboard Classic|Simple] [--canvas-log OUT.json]
//   node native/tools/ui-shot.mjs OUT.png --screen single [--ticks T] [--rider R] [--keys K1,K2,...] [--frame F]
//        a Single Event race's results through careerUI.finish (Snow Jam, the QA standings ssx3_ui_shot uses, fresh records, the PS2's
//        local table: ?pv=-onlineRecords), then the menu keys as key presses once the screen takes input
import fs from 'node:fs';
import { startBrowser, startServer, sleep } from '../../web/headless-chrome.mjs';

const args = process.argv.slice(2);
const opt = (k, d) => {
  const i = args.indexOf('--' + k);
  return i < 0 ? d : args[i + 1];
};
const out = args.find((a, i) => !a.startsWith('--') && !(i > 0 && args[i - 1].startsWith('--')));
if (!out) throw new Error('usage: ui-shot.mjs OUT.png --screen loading [--frame F] [--percent P] ...');
const screen = opt('screen', 'loading');
const frame = +opt('frame', 120);
const percent = +opt('percent', 42);
const black = +opt('black', 0);
const hintCounter = +opt('hint-counter', 0);
const device = opt('device', 'keyboard');
const keyboard = opt('keyboard', 'Classic');
const canvasLog = opt('canvas-log');
const humanTicks = +opt('ticks', 12000);
const rider = opt('rider', 'zoe');
const keyList = (opt('keys', '') || '').split(',').filter(Boolean);

const browser = await startBrowser({ width: 640, height: 480 });
if (!browser) {
  console.error('no Chrome');
  process.exit(77);
}
const server = await startServer();
const E = (js) => browser.evaluate(js);
try {
  await browser.goto(`${server.origin}/manifest.webmanifest`);
  await E(`(()=>{localStorage.clear();sessionStorage.clear();localStorage.setItem('ssx3.widescreen','0');
    localStorage.setItem('ssx3.keyboard',${JSON.stringify(keyboard)});sessionStorage.setItem('ssx3.loadingHint',${JSON.stringify(String(hintCounter))});return 1})()`);
  await browser.goto(`${server.origin}/?qa=1&cutscenes=0&quality=low&mute=1${screen === 'single' ? '&pv=-onlineRecords' : ''}`);
  await browser.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
  await browser.waitFor('!!ssxQA.ui().loading?.available && !!ssxQA.ui().fonts?.FEFONT', 120000);
  // the screen's draw at its fixed state (ui.draw for the load screen; the career UI's results panels drawn alone, the page's own loop
  // stopped from clearing them)
  let drawJs = 'ui.draw(ui.lastState || {})';
  if (screen === 'loading') {
    // the session opened, its state fixed (update) and its device; the page's own loop redraws it
    await E(`(()=>{const ui=ssxQA.ui(),l=ui.loading;l.cancel();l.open();l.forceDevice=${JSON.stringify(device)};
      l.update=()=>({frame:${frame},percent:${percent},black:${black}});return 1})()`);
  } else if (['results', 'records', 'racecard'].includes(screen)) {
    await browser.waitFor('!!ssxQA.ui().careerUI?.resultsLui?.ready', 120000);
    const names = ['Zoe', 'Mac', 'Elise', 'Psymon', 'Kaori', 'Moby'];
    const sample = {
      results: `rl.results(ctx, { title: 'Snow Jam', sub: 'Single Event Results', heads: ['Rank', 'Riders', 'Time'],
        rows: ${JSON.stringify(names)}.map((name, i) => ({ rank: i + 1, name, value: ['03:54:12', '03:55:40', '03:57:02', '03:59:33', '04:01:18', '04:05:47'][i], human: i === 0 })),
        medal: { row: 0, kind: 1 }, message: "Congratulations! You've received a Gold medal.", items: ['Continue', 'Restart', 'Replay', 'Records', 'Quit'],
        index: ${percent % 5}, disabled: (k) => k === 2 })`,
      records: `rl.records(ctx, { title: 'Snow Jam', timed: true, rows: ['PLAYER 1', 'EA', 'EA', 'EA', 'EA'].map((name, i) => ({ name, rider: ${JSON.stringify(names)}[i],
        value: ['03:54:12', '04:10:00', '04:20:00', '04:30:00', '04:40:00'][i], player: i === 0 })), message: 'Congratulations! You set a new record time.',
        items: ['Continue', 'Save Records'], index: ${percent % 2} })`,
      racecard: `rl.raceCard(ctx, { title: 'Snow Jam', sub: 'Single Event', objective: 'Finish in first place to earn a Gold medal.',
        riders: ${JSON.stringify(names)}.map((name, i) => ({ name, human: i === 0 })), recordLabel: 'Record time:', record: '03:54:12', continueLabel: 'Continue' })`
    }[screen];
    await E(`(async()=>{const { forceInputDevice } = await import('/input-glyphs.js'); forceInputDevice(${JSON.stringify(device)});
      const ui=ssxQA.ui(); ui.draw=()=>{}; window.__drawFixed=()=>{const ctx=document.querySelector('#ui').getContext('2d'),rl=ui.careerUI.resultsLui;
        rl.clock=()=>${frame}; ctx.clearRect(0,0,640,448); document.querySelector('#ui-bg').getContext('2d').clearRect(0,0,640,448); ${sample};};
      window.__drawFixed(); return 1})()`);
    drawJs = 'window.__drawFixed()';
  } else if (screen === 'single') {
    await browser.waitFor('!!ssxQA.ui().careerUI?.resultsLui?.ready && !!ssxQA.ui().careerUI?.career', 120000);
    const names = ['Mac', 'Psymon', 'Elise', 'Kaori', 'Moby'];
    const finishes = [12500, 12800, 13100, 13300, 13600];
    const standings = [{ human: true, finishTicks: humanTicks, dnf: false }, ...names.map((name, i) => ({ human: false, name, finishTicks: finishes[i], remaining: 0, place: i + 1, origin: 0, dnf: false }))];
    await E(`(async()=>{const { forceInputDevice } = await import('/input-glyphs.js'); forceInputDevice(${JSON.stringify(device)});
      const ui=ssxQA.ui(),cu=ui.careerUI; ui.riderIndex=Math.max(0,ui.riders.findIndex((r)=>r.id===${JSON.stringify(rider)}));
      cu.career.startEvent(cu.riderId,0,0,false); cu.active={mode:0,course:0,career:false,rideIn:false,inWorld:false};
      cu.finish({ticks:${humanTicks},raceTicks:${humanTicks},dnf:false,standings:${JSON.stringify(standings)}});
      await new Promise((r)=>setTimeout(r,50)); return 1})()`);
    // each key once the screen's intro lock (31 UI frames) has passed
    for (const code of keyList) {
      await sleep(1000);
      await E(`(()=>{dispatchEvent(new KeyboardEvent('keydown',{code:${JSON.stringify(code)},bubbles:true,cancelable:true}));
        dispatchEvent(new KeyboardEvent('keyup',{code:${JSON.stringify(code)},bubbles:true,cancelable:true}));return 1})()`);
    }
    await sleep(1000);
    await E(`(()=>{const ui=ssxQA.ui(); ui.draw=()=>{}; window.__drawFixed=()=>{const ctx=document.querySelector('#ui').getContext('2d'),cu=ui.careerUI;
        cu.resultsLui.clock=()=>${frame}; ctx.clearRect(0,0,640,448); const bg=document.querySelector('#ui-bg').getContext('2d'); bg.clearRect(0,0,640,448);
        cu.draw(ctx,bg);};
      window.__drawFixed(); return 1})()`);
    drawJs = 'window.__drawFixed()';
    const st = await E(`(()=>{const ui=ssxQA.ui(),o=ui.careerUI.result.outcome;return {place:o.place,medal:o.medal,record:o.record,screen:ui.screen,index:ui.index}})()`);
    console.log(JSON.stringify(st));
  } else throw new Error('unknown screen ' + screen);
  await sleep(500);
  if (canvasLog) {
    await E(`(()=>{
      const ui = ssxQA.ui(), ctx = document.querySelector('#ui').getContext('2d');
      const fnv = (d) => { let h = 2166136261; for (let i = 0; i < d.length; i++) { h ^= d[i]; h = Math.imul(h, 16777619) >>> 0; } return h >>> 0; };
      const nameOf = (im) => {
        if (im instanceof HTMLImageElement) return decodeURIComponent(im.src.split('/').pop().replace(/\\.png$/, ''));
        if (im instanceof HTMLCanvasElement) return 'canvas:' + fnv(im.getContext('2d').getImageData(0, 0, im.width, im.height).data).toString(16).padStart(8, '0');
        return String(im);
      };
      const rec = (window.__canvasRec = { list: [], on: true });
      const push = (op, a, img) => { if (!rec.on) return; const m = ctx.getTransform(); const e = { op, args: a, m: [m.a, m.b, m.c, m.d, m.e, m.f], alpha: ctx.globalAlpha }; if (img !== undefined) e.img = img; rec.list.push(e); };
      const wrap = (name, fn) => { const orig = ctx[name].bind(ctx); ctx[name] = (...a) => { fn(...a); return orig(...a); }; };
      wrap('drawImage', (im, ...a) => {
        const w = im.naturalWidth || im.width, h = im.naturalHeight || im.height;
        const n = a.length === 2 ? [0, 0, w, h, a[0], a[1], w, h] : a.length === 4 ? [0, 0, w, h, ...a] : a;
        push('drawImage', n, nameOf(im));
      });
      wrap('fillRect', (...a) => push('fillRect', [...a, ctx.fillStyle]));
      wrap('roundRect', (x, y, w, h, r) => push('roundRect', [x, y, w, h, r]));
      wrap('fill', () => { if (typeof ctx.fillStyle !== 'string') push('fill', ['gradient']); });
      wrap('stroke', () => push('stroke', [ctx.strokeStyle, ctx.lineWidth]));
      wrap('moveTo', () => push('arrow', []));
      ${drawJs};
      rec.on = false;
      return JSON.stringify(rec.list);
    })()`).then((log) => fs.writeFileSync(canvasLog, log));
  }
  const png = await browser.screenshot('#stage');
  fs.writeFileSync(out, Buffer.from(png, 'base64'));
  console.log(JSON.stringify({ screen, frame, percent, out }));
} finally {
  await browser.close();
  await server.close();
}
process.exit(0);
