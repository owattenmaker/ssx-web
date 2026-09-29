// First load (docs/first-load.md): the title's load meter and first paint.
//  - web/boot-progress.js: weights and fractions (files by bytes on the wire, compute steps by time), creep capped
//    below done, never backwards, reaches 1 only when finished, phase labels follow the work under way;
//  - web/title-data.js: the title screen cut from the exported FE data (06title, the snow loop, the Rider ranking meter),
//    the meter moved to its spot on the title;
//  - web/boot-plugin.js: the inline boot script (bundled, stubs instead of the download hook and key caps) and data
//    (manifest sizes = the files on disk, gzip estimates like server/precompress.mjs), injected only into index.html;
//  - web/boot-files.json: phases, steps and files (present on disk when the game data is);
//  - web/downloads.js onDownloadBytes: per-file bytes as they stream.
import assert from 'node:assert/strict';
import fs from 'node:fs'; import path from 'node:path';
import { createBootProgress, bootKey, BYTES_PER_MS, CREEP_MAX, REQUEST_BYTES as RB } from './boot-progress.js';
import { titleScreenData, titleLuiScreen, METER, METER_AT, TITLE_PAGES } from './title-data.js';
import { bootPhase, bootList, BOOT_PHASES, BOOT_STEPS } from './boot-files.mjs';

const web = path.dirname(new URL(import.meta.url).pathname), UI = path.join(web, 'public/assets/UI');
const readJson = (f) => { try { return JSON.parse(fs.readFileSync(f, 'utf8')); } catch { return null; } };

// ---- progress model ----
assert.equal(bootKey('https://h/assets/UI/FE_1-20.png?g=2'), '/assets/UI/FE_1-20.png');
{
  let now = 0; const clock = () => now;
  const m = { phases: [['code', 'Loading game...'], ['fe', 'Loading menus...'], ['course', 'Loading %s...']], steps: [['js', 100, 'code'], ['world', 1000, 'course']],
    files: [['/assets/main-abcdefgh.js', 600000, 2000000, 'code'], ['/assets/UI/FE_1-20.png', 150000, 150000, 'fe'], ['/assets/ARA1/vertices.bin', 9000000, 20000000, 'course']] };
  const p = createBootProgress(m, { now: clock, start: 0 });
  const total = 3 * RB + 600000 + 150000 + 9000000 + (100 + 1000) * BYTES_PER_MS;   // bytes on the wire + a round trip each, steps by time
  assert.equal(p.target().fraction, 0);
  p.begin('/assets/main-abcdefgh.js'); now = 50;
  const creeping = p.target().fraction;
  assert.ok(creeping > 0 && creeping <= CREEP_MAX * (RB + 600000) / total + 1e-9, 'a file in flight creeps, never past 90% of itself');
  assert.equal(p.target().phase, 'code');
  p.done('https://x/assets/main-abcdefgh.js', 600000); now = 100;
  assert.ok(Math.abs(p.target().fraction - (RB + 600000) / total) < 1e-9);
  p.step('js', 1);
  p.bytes('http://x/assets/ARA1/vertices.bin', 10000000, 20000000);
  const f = p.target().fraction;
  assert.ok(Math.abs(f - (RB + 600000 + 100 * BYTES_PER_MS + 0.5 * (RB + 9000000)) / total) < 1e-9, 'streamed bytes count by their share of the file');
  assert.equal(p.target().phase, 'course', 'the label follows the work under way (the FE picture was not asked for yet)');
  p.step('world', 0.5);
  assert.ok(p.target().fraction > f);
  // the shown value eases towards the target and never goes back, even when a step reports less
  let last = 0; for (let i = 0; i < 200; i++) { now += 16; const v = p.value(now, 16); assert.ok(v.fraction >= last - 1e-12); last = v.fraction; }
  assert.ok(last < 1, 'not done before finish()');
  p.step('world', 0.2); assert.ok(p.value(now + 16, 16).fraction >= last);
  assert.equal(p.value(now, 16).label, 'Loading %s...');
  p.begin('/assets/UI/FE_1-20.png'); now += 1000; assert.equal(p.value(now, 16).phase, 'course', 'the label never goes back to an earlier phase');
  p.finish(); assert.equal(p.target().fraction, 1); assert.equal(p.value(now + 16, 16).fraction, 1);
}
{ // a step with no reports creeps over its typical time, capped at 90%
  let now = 0; const p = createBootProgress({ steps: [['gpu', 200, 'gpu']], phases: [['gpu', 'Starting graphics...']] }, { now: () => now, start: 0 });
  p.step('gpu', 0, 0, { begin: true }); now = 100; assert.ok(Math.abs(p.target().fraction - CREEP_MAX * 0.5) < 1e-9);
  now = 10000; assert.ok(Math.abs(p.target().fraction - CREEP_MAX) < 1e-9);
  p.step('gpu', 1); assert.equal(p.target().fraction, 1);
}
{ // a ?course= deep link maps the boot course's files to the course being loaded
  const p = createBootProgress({ files: [['/assets/ARA1/vertices.bin', 100, 300, 'course'], ['/assets/UI/x.png', 100, 100, 'fe']], course: ['ARA1', 'BRA2'] }, { now: () => 0 });
  assert.ok(p.files.has('/assets/BRA2/vertices.bin') && !p.files.has('/assets/ARA1/vertices.bin') && p.files.has('/assets/UI/x.png'));
}

// ---- boot-files.json ----
assert.equal(bootPhase('/assets/main-DAU347UH.js'), 'code'); assert.equal(bootPhase('/assets/core-CFq4p2dY.wasm'), 'core');
assert.equal(bootPhase('/assets/UI/FE_1-20.png'), 'fe'); assert.equal(bootPhase('/assets/RIDER_MOBY/vertices.bin'), 'riders');
assert.equal(bootPhase('/assets/ARA1/terrain-light-atlas.png'), 'course');
assert.deepEqual(Object.keys(bootList(['/assets/main-DAU347UH.js', '/assets/ARA1/terrain-light-atlas.png', '/assets/UI/FE_1-0.png']).files).sort(), ['course', 'fe']);
const list = readJson(path.join(web, 'boot-files.json'));
assert.ok(list && list.course === 'ARA1');
assert.deepEqual(list.phases, BOOT_PHASES); assert.deepEqual(list.steps, BOOT_STEPS);
for (const [, , phase] of list.steps) assert.ok(list.phases.some(([id]) => id === phase), phase);
for (const [phase, files] of Object.entries(list.files)) { assert.ok(list.phases.some(([id]) => id === phase)); for (const f of files) assert.equal(bootPhase(f), phase, f); }
const haveData = fs.existsSync(path.join(UI, 'fe-menus.json'));
if (haveData) for (const f of Object.values(list.files).flat()) assert.ok(fs.existsSync(path.join(web, 'public', f)), `boot file missing: ${f} (node boot-files.mjs record)`);

// ---- the title screen data ----
{
  const feMenus = { screens: { '06title': { elements: [{ index: 0, name: '00798b7c', kind: 'text', text: 'Press START button', props: { 0: 146, 1: 10 } }], events: [], animations: {} } } };
  const el = (name, extra) => ({ index: 200 + name.length, name, kind: 'shape', props: { 0: 354, 1: 97 }, ...extra });
  const sel = { elements: [el(METER.bar, { kind: 'bar', bar: { background: 'bg', fill: 'fill' } }), el('bg', { props: { 0: 2, 1: 2 } }), el('fill', { props: { 0: 2, 1: 2 } }),
    el(METER.frame), el(METER.cells, { kind: 'group', children: ['c1'], props: { 0: 354, 1: 99 } }), el('c1', { parent: METER.cells, props: { 0: 17, 1: 0 } }), el(METER.value, { kind: 'text', parent: 'grp', props: { 0: 390, 1: 154 } })],
  events: [{ frame: 16, anim: 'a1', element: METER.frame, mode: 1 }, { frame: 3, anim: 'other', element: 'zzz' }], animations: { a1: { frames: 9, tracks: {} }, other: {} } };
  const data = titleScreenData(feMenus, { screens: { '08sel_char': sel, bg_snow_loop: { elements: [{ index: 0, name: 's', kind: 'sprite' }], events: [], animations: {} } } });
  assert.equal(data.meter.elements.length, 7); assert.deepEqual(data.meter.events.map((e) => e.anim), ['a1']); assert.deepEqual(Object.keys(data.meter.animations), ['a1']);
  const screen = titleLuiScreen(data), by = new Map(screen.elements.map((e) => [e.name, e]));
  assert.deepEqual([by.get(METER.frame).props[0], by.get(METER.frame).props[1]], [METER_AT.x, METER_AT.y], 'the meter frame at its spot');
  assert.deepEqual([by.get(METER.bar).props[0], by.get(METER.bar).props[1]], [METER_AT.x, METER_AT.y], 'the bar moves with it');
  assert.deepEqual([by.get('bg').props[0], by.get('fill').props[0]], [2, 2], 'the bar places its background and fill');
  assert.deepEqual([by.get(METER.cells).props[0], by.get('c1').props[0]], [METER_AT.x, 17], 'cells: the group moves, the cells stay relative');
  assert.ok(!by.get(METER.value).parent && by.get(METER.value).props[13] === 255);
  assert.equal(by.get('s').index, 1000); assert.ok(screen.elements.filter((e) => e.meter).every((e) => e.index >= 2000));
}
if (haveData) {
  const data = titleScreenData(readJson(path.join(UI, 'fe-menus.json')), readJson(path.join(UI, 'character-select.json')));
  assert.ok(data?.meter && data.snow, 'exported 06title + bg_snow_loop + the 08sel_char meter (python3 tools/export_fe_menus.py)');
  const texts = data.title.elements.filter((e) => e.kind === 'text').map((e) => e.text);
  assert.ok(texts.includes('Press START button') && texts.some((t) => /Electronic Arts/.test(t)));
  const pages = new Set(data.title.elements.filter((e) => e.sprite).map((e) => e.sprite.page).concat(data.snow.elements.filter((e) => e.sprite).map((e) => e.sprite.page)));
  for (const p of pages) assert.ok(TITLE_PAGES.includes(p), `title page ${p} preloaded`);
}

// ---- the Vite plugin ----
if (haveData) {
  const { default: bootPlugin, bootData, wireSize } = await import('./boot-plugin.js');
  const d = bootData();
  assert.ok(d.title && d.glyphs && d.manifest.files.length > 100 && d.courseNames.ARA1 === 'Snow Jam');
  for (const [p, wire, size] of d.manifest.files.slice(0, 40)) { assert.equal(size, fs.statSync(path.join(web, 'public', p)).size); assert.ok(wire > 0 && wire <= size); }
  const png = d.manifest.files.find(([p]) => p.endsWith('.png')); assert.equal(png[1], png[2], 'PNG: no gzip copy');
  const big = d.manifest.files.find(([p]) => p.endsWith('vertices.bin')); assert.ok(big[1] < big[2] * 0.9, 'binaries: their gzip copy');
  assert.equal(wireSize(path.join(web, 'missing.bin'), 'missing.bin'), null);
  const plugin = bootPlugin(), html = '<html><head></head><body><div id="stage"></div></body></html>';
  assert.equal(await plugin.transformIndexHtml.handler(html, { path: '/fog-gpu-test.html' }), html, 'test pages untouched');
  const out = await plugin.transformIndexHtml.handler(html, { path: '/index.html' });
  const code = out.tags.find((t) => t.tag === 'script' && !t.children.startsWith('window.__SSX_BOOT__')).children;
  assert.ok(code.length < 24000, `boot script ${code.length} bytes`);
  assert.ok(!/globalThis\.fetch\s*=/.test(code) && !/requestAdapter/.test(code), 'no download hook, no GPU in the boot script');
  assert.ok(!/<\/script/i.test(code));
  assert.equal(out.tags.filter((t) => t.attrs?.rel === 'preload').length, TITLE_PAGES.length + 1);
  const data = out.tags.find((t) => t.children?.startsWith('window.__SSX_BOOT__')).children;
  assert.ok(data.length < 64000, `inline data ${data.length} bytes`); assert.ok(!data.includes('</'));
  // a build adds the code bundle and the core
  const bundle = { a: { type: 'chunk', isEntry: true, fileName: 'assets/main-ABCDEFGH.js', code: 'x'.repeat(5000) }, b: { type: 'asset', fileName: 'assets/core-ABCDEFGH.wasm', source: new Uint8Array(3000) }, c: { type: 'asset', fileName: 'assets/main-ABCDEFGH.css', source: 'body{}' } };
  const built = await plugin.transformIndexHtml.handler(html, { path: '/index.html', bundle });
  const boot = JSON.parse(built.tags.find((t) => t.children?.startsWith('window.__SSX_BOOT__')).children.slice('window.__SSX_BOOT__='.length, -1));
  assert.equal(boot.mainScript, '/assets/main-ABCDEFGH.js');
  assert.deepEqual(boot.manifest.files.filter(([, , , ph]) => ph === 'code' || ph === 'core').map(([p, , s, ph]) => [p, s, ph]).sort(), [['/assets/core-ABCDEFGH.wasm', 3000, 'core'], ['/assets/main-ABCDEFGH.css', 6, 'code'], ['/assets/main-ABCDEFGH.js', 5000, 'code']]);
}

// ---- the boot script takes the UI canvases first: same context attributes as web/ui.js ----
{
  const expr = (src, re) => (src.match(re) || [])[1];
  const ui = expr(fs.readFileSync(path.join(web, 'ui.js'), 'utf8'), /export const UI_CANVAS=(.*?\{willReadFrequently:true\})/);
  const boot = expr(fs.readFileSync(path.join(web, 'boot-screen.js'), 'utf8'), /const UI_CANVAS = (.*?\{ willReadFrequently: true \})/);
  assert.ok(ui && boot, 'UI_CANVAS in ui.js and boot-screen.js');
  assert.equal(boot.replace(/\s+/g, ''), ui.replace(/\s+/g, '').replace("globalThis.location?.search??''", 'location.search'));
}

// ---- downloads.js: per-file bytes ----
{
  const chunks = [new Uint8Array(3), new Uint8Array(5)];
  globalThis.location = { href: 'http://game/', origin: 'http://game' };
  globalThis.fetch = async () => new Response(new ReadableStream({ start(c) { for (const k of chunks) c.enqueue(k); c.close(); } }), { headers: { 'x-decoded-length': '8' } });
  const { onDownloadBytes } = await import('./downloads.js');
  const seen = []; const off = onDownloadBytes((url, got, total) => seen.push([new URL(url).pathname, got, total]));
  const r = await fetch('/assets/ARA1/start.json'); assert.equal((await r.arrayBuffer()).byteLength, 8);
  assert.deepEqual(seen, [['/assets/ARA1/start.json', 3, 8], ['/assets/ARA1/start.json', 8, 8]]);
  off(); await fetch('/assets/ARA1/route.json'); assert.equal(seen.length, 2, 'unsubscribed');
}
console.log('boot progress ok');
