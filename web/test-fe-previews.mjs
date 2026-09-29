// Main menu > Previews (web/fe-previews.js, 146Bonusmat): the menu from UI/fe-menus.json (tools/export_fe_menus.py),
// the trailers from MOVIES/movies.json (tools/export_movies.py NFSXSELL NFLXSELL ST3XSELL), without a browser.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { FePreviews, previewsModel, previewMovies, previewsDisabled, PREVIEW_KEYS, PREVIEWS_SCREEN, MAIN_PREVIEWS_INDEX } from './fe-previews.js';

const root = new URL('./public/assets/', import.meta.url).pathname;
const readJson = (p) => (fs.existsSync(root + p) ? JSON.parse(fs.readFileSync(root + p)) : null);
const menus = readJson('UI/fe-menus.json');
const exported = readJson('MOVIES/movies.json');
const quiet = console.info; console.info = () => {};

// ---- movie rows (pure) ----
const fake = { movies: [{ key: 'INTRO', src: 'MOVIES/INTRO.mp4', reward: 'data/movies/intro.mpc' },
  { key: 'NFLXSELL', src: 'MOVIES/NFLXSELL.mp4', preview: 1 }, { key: 'ST3XSELL', src: 'MOVIES/ST3XSELL.mp4' }] };
const rows = previewMovies(fake);
assert.deepEqual(rows.map((m) => m?.key ?? null), [null, 'NFLXSELL', 'ST3XSELL'], 'by preview index, else by key');
assert.deepEqual(previewsDisabled(rows), [true, false, false], 'a missing trailer disables its item');
assert.deepEqual(previewsDisabled(rows, new Set([2])), [true, false, true], 'a trailer that failed to play too');
assert.deepEqual(previewMovies(null).map(Boolean), [false, false, false]);

if (!menus?.screens?.['146Bonusmat']) {
  console.log('fe-previews: UI/fe-menus.json not exported (python3 tools/export_fe_menus.py): menu checks skipped');
} else {
  // ---- the menu (146Bonusmat) ----
  const model = previewsModel(menus.screens['146Bonusmat']);
  assert.deepEqual(model.texts, ['Need for Speed™ Underground', 'NFL STREET', 'NBA STREET Vol. 2']);
  assert.deepEqual(model.frames, [30, 38, 48], 'focus states');
  assert.equal(model.intro, 20);
  const sc = menus.screens['146Bonusmat'];
  assert.equal(sc.elements.find((e) => e.label === 'helptext')?.text, 'View trailers of other EA™ games.');
  assert.equal(sc.elements.find((e) => e.name === '091ba3d3')?.text, 'Previews');
  // the orange bar follows the focus (y 158 + 20i)
  model.frames.forEach((f, i) => assert.equal(sc.events.find((ev) => ev.frame === f && ev.element === '000006ec')?.props[1], 158 + 20 * i));
  // focus state i turns item i white and leaves the others dark
  model.frames.forEach((f, i) => model.items.forEach((n, j) => assert.equal(sc.events.find((ev) => ev.frame === f && ev.element === n).props[14] === 255, i === j)));

  // ---- the screen with a stub UI ----
  const ui = { screen: 'main', index: 0, ready: true, fonts: {}, sets: [], syncs: 0, characterSelect: null,
    set(s) { const prev = this.screen; this.screen = s; pv.enter(s, prev); this.index = 0; this.sets.push(s); }, sync() { this.syncs++; } };
  const pv = new FePreviews(ui);
  assert.equal(pv.ready, false);
  assert.ok(pv.init(menus, fake));
  assert.ok(pv.ready && pv.owns(PREVIEWS_SCREEN) && !pv.owns('main'));
  ui.set(PREVIEWS_SCREEN);
  assert.deepEqual(pv.items(), model.texts);
  assert.equal(pv.disabled(0), true); assert.equal(pv.disabled(1), false);
  assert.equal(pv.focus(), 1, 'focus skips the missing trailer');
  assert.deepEqual(pv.layout(0).map((v) => +v.toFixed(2)), [190, +(160 * 448 / 480).toFixed(2), 300, +(20 * 448 / 480).toFixed(2)]);
  assert.equal(pv.layout(2)[1], 200 * 448 / 480);
  const key = (code, extra = {}) => pv.key({ code, preventDefault() {}, ...extra });
  assert.ok(key('ArrowDown')); assert.equal(ui.index, 2);
  assert.ok(key('ArrowDown')); assert.equal(ui.index, 1, 'wraps and skips the disabled item');
  assert.ok(key('ArrowUp')); assert.equal(ui.index, 2);
  assert.equal(key('ArrowLeft'), false);
  // Cross plays the focused trailer (no DOM here: record it); Enter/Escape skip it and the screen stays
  const played = []; pv.play = function (i) { this.movie = { index: i, handle: { stop: () => played.push('stop') } }; played.push(this.movies[i].key); return true; };
  pv.choose(0); assert.deepEqual(played, [], 'disabled item does nothing');
  assert.ok(key('Enter')); assert.deepEqual(played, ['ST3XSELL']); assert.ok(pv.playing);
  assert.ok(key('ArrowUp')); assert.equal(ui.index, 2, 'modal while playing');
  assert.ok(key('Escape')); assert.deepEqual(played, ['ST3XSELL', 'stop']); assert.ok(!pv.playing); assert.equal(ui.screen, PREVIEWS_SCREEN);
  pv.choose(1); assert.equal(played.at(-1), 'NFLXSELL'); ui.set('main'); assert.equal(played.at(-1), 'stop', 'leaving the screen stops the trailer');
  // Triangle: white fade, then the Main Menu on Previews
  ui.set(PREVIEWS_SCREEN); assert.ok(key('Escape')); assert.ok(pv.flash); assert.equal(ui.screen, PREVIEWS_SCREEN);
  pv.flash.to(); assert.equal(ui.screen, 'main'); assert.equal(ui.index, MAIN_PREVIEWS_INDEX);
  assert.equal(pv.key({ code: 'Enter', preventDefault() {} }), false, 'not owned on the main menu');
}

// ---- exported trailers (tools/export_movies.py) ----
if (!exported) console.log('fe-previews: MOVIES/movies.json not exported: trailer checks skipped');
else {
  const rowsOnDisk = previewMovies(exported), missing = PREVIEW_KEYS.filter((_, i) => !rowsOnDisk[i]);
  if (missing.length) console.log(`fe-previews: trailers not exported (${missing.join(' ')}): python3 tools/export_movies.py ${PREVIEW_KEYS.join(' ')}`);
  rowsOnDisk.forEach((m, i) => {
    if (!m) return;
    assert.equal(m.key, PREVIEW_KEYS[i]); assert.equal(m.preview, i); assert.equal(m.reward, null);
    assert.ok(fs.existsSync(root + m.src), `${m.src} exists`);
    assert.ok(m.seconds > 30 && m.channels === 2, `${m.key}: a stereo trailer`);
  });
  if (menus) assert.deepEqual(rowsOnDisk.map((m) => m?.name ?? null).filter(Boolean), previewsModel(menus.screens['146Bonusmat']).texts.filter((_, i) => rowsOnDisk[i]), 'names = the menu texts');
  // the other movies stay in the merged index (reward videos, backcountry arrivals)
  for (const k of ['INTRO', 'MTNALIVE']) if (fs.existsSync(root + `MOVIES/${k}.mp4`)) assert.ok(exported.movies.some((m) => m.key === k), `${k} kept in movies.json`);
}
console.info = quiet;
console.log('fe-previews ok');
