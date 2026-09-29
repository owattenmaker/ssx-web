// pv lazyCourse (docs/first-load.md "Title before the course"): the title and the menus start with the front end only and the
// page's course loads behind them from Press START. Headless Chrome + a private Vite server:
//  - "Press START" before the course; nothing of the course downloads at the title; the boot meter never goes backwards;
//  - Press START starts the course behind the menus; the Select Character preview shows lit (a preview core of its own) and
//    stays lit through the course's core once it is in;
//  - Snow Jam picked at once after Press START: the event load screen opens at once, its percentage never goes backwards
//    and stays <= 90 until the course is in; the event loads;
//  - another course picked while Snow Jam loads: Snow Jam is abandoned, the chosen course loads;
//  - ?qa=1: ssxQA once the course is live, and ssxQA.start() races.
//   node test-lazy-course.mjs     (skips without Chrome / WebGPU or without the game data)
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { startBrowser, startServer, sleep } from './headless-chrome.mjs';

if (!fs.existsSync(new URL('./public/assets/courses.json', import.meta.url))) { console.log('lazy course: skipped (no game data)'); process.exit(0); }
const INIT = `window.__pct=[];const __pp=()=>{try{const s=window.__perfUI?.()?.loading?.session;if(s&&s.shownPercent!=null&&__pct.at(-1)?.[1]!==s.shownPercent)__pct.push([performance.now(),s.shownPercent]);}catch{}requestAnimationFrame(__pp);};requestAnimationFrame(__pp);`;
const browser = await startBrowser({ init: INIT, width: 960, height: 720 });
if (!browser) { console.log('lazy course: skipped (no Chrome)'); process.exit(0); }
const server = await startServer();
const failures = [];
const check = async (name, fn) => { const n = browser.logs.length; try { await fn(); const errs = browser.logs.slice(n).filter((l) => /^(error|exception)/.test(l)); assert.deepEqual(errs, [], 'console errors'); console.log(`ok   ${name}`); } catch (e) { failures.push(`${name}: ${e.message}`); console.log(`FAIL ${name}: ${e.message}`); } };
const E = (x) => browser.evaluate(x);
const enter = async () => { for (const type of ['keyDown', 'keyUp']) await browser.send('Input.dispatchKeyEvent', { type, code: 'Enter', key: 'Enter', windowsVirtualKeyCode: 13 }); };
const mark = (n) => E(`Math.round(performance.getEntriesByName('${n}')[0]?.startTime ?? -1)`);
const screen = () => E(`document.getElementById('stage').dataset.screen`);
const open = async (q = '') => { await browser.goto(`${server.origin}/?perf=1&diag=0&mute=1&pv=lazyCourse${q}`); await browser.waitFor(`!!performance.getEntriesByName('boot:ready')[0] || !!document.body.dataset.loadError`, 240000); assert.equal(await E('document.body.dataset.loadError || ""'), ''); };
const eventUp = `['cutscene','ctm-objectives','game'].includes(document.getElementById('stage').dataset.screen) || !!document.body.dataset.loadError`;
try {
  if (!(await browser.hasWebGPU(server.origin))) { console.log('lazy course: skipped (no WebGPU in headless Chrome)'); process.exit(0); }
  await check('the title before the course; Press START loads it behind the menus; the Select Character preview', async () => {
    await open();
    assert.ok((await mark('course:live')) < 0, 'no course at "Press START"');
    const meter = await E(`(()=>{const h=ssxBoot.history;return h.every((x,i)=>!i||x[1]>=h[i-1][1]-1e-9)})()`); assert.ok(meter, 'the boot meter never goes backwards');
    const n0 = await E(`performance.getEntriesByType('resource').filter(e=>/\\/assets\\/(ARA1|RIDER_|TEXTURES|ANIMATIONS)/.test(e.name)).length`);
    await sleep(1500);
    assert.equal(await E(`performance.getEntriesByType('resource').filter(e=>/\\/assets\\/(ARA1|RIDER_|TEXTURES|ANIMATIONS)/.test(e.name)).length`), n0, 'no course download at the title');
    await enter(); await sleep(1200); assert.equal(await screen(), 'main');
    await enter(); await sleep(400); assert.equal(await screen(), 'character');
    await browser.waitFor(`(()=>{const p=__perfUI().characterSelect.preview3d;return p.ready&&p.model.group.visible&&p.model.lit.value})()`, 120000);
    await browser.waitFor(`performance.getEntriesByName('course:live').length>0`, 240000); await sleep(500);
    assert.ok(await E(`(()=>{const p=__perfUI().characterSelect.preview3d;return p.model.group.visible&&p.model.lit.value})()`), 'lit through the course core');
    const loads = JSON.parse(await E('JSON.stringify(window.__courseLoads)'));
    assert.ok(loads[0].lazy && loads.some((l) => l.background && l.code === 'ARA1'), JSON.stringify(loads));
  });
  await check('Snow Jam at once after Press START: the event load screen covers the course, honestly', async () => {
    await open(); await enter(); await sleep(150);
    await E(`(()=>{const ui=__perfUI();ui.startSingleEvent(ui.courses.find(c=>c.code==='ARA1'));return 1})()`);
    await sleep(200); assert.equal(await screen(), 'loading');
    await browser.waitFor(eventUp, 240000); assert.equal(await E('document.body.dataset.loadError || ""'), '');
    const pct = JSON.parse(await E('JSON.stringify(__pct)')), live = await mark('course:live');
    assert.ok(pct.every((x, i) => !i || x[1] >= pct[i - 1][1]), 'the percentage never goes backwards');
    assert.ok(pct.filter((x) => x[0] < live).every((x) => x[1] <= 90), 'the percentage stays <= 90 until the course is in: ' + JSON.stringify(pct.filter((x) => x[0] < live).slice(-3)));
  });
  await check('another course while Snow Jam loads: it is abandoned, the chosen course loads', async () => {
    await open(); await enter(); await sleep(100);
    await E(`(()=>{const ui=__perfUI();ui.startSingleEvent(ui.courses.find(c=>c.code==='BRA2'));return 1})()`);
    await sleep(200); assert.equal(await screen(), 'loading');
    await browser.waitFor(eventUp, 240000); assert.equal(await E('document.body.dataset.loadError || ""'), '');
    const loads = JSON.parse(await E('JSON.stringify(window.__courseLoads)'));
    assert.ok(loads.some((l) => l.code === 'BRA2') && !loads.some((l) => l.background && l.code === 'ARA1'), JSON.stringify(loads.map((l) => [l.code, l.background, l.after])));
    assert.equal(await E(`__perfUI().course.code`), 'BRA2');
  });
  await check('?qa=1: ssxQA once the course is live, ssxQA.start() races', async () => {
    await browser.goto(`${server.origin}/?qa=1&pv=lazyCourse&quality=low&mute=1`);
    await browser.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 240000);
    assert.ok((await mark('course:live')) > 0);
    await E('(()=>{ssxQA.start();return 1})()'); await sleep(1500);
    const s = await E('JSON.stringify(ssxQA.advance(30))'); assert.ok(JSON.parse(s).every(Number.isFinite));
    assert.equal(await screen(), 'game');
  });
} finally { await browser.close(); await server.close(); }
if (failures.length) { console.error(`lazy course FAILED:\n${failures.join('\n')}`); process.exit(1); }
console.log('lazy course OK: the title before the course, the course behind the menus, the event load screen over the rest');
process.exit(0);
