// Browser save persistence (web/save-store.js, web/career-save.js, web/fe-event-select.js selection): atomic writes,
// interrupted-write recovery, blocked / full storage, the career v1 -> v2 migration, a whole career "reload" round
// trip, the save file export / import, and the Single Event selector's lists. docs/characters.md "Saving progress".
import assert from 'node:assert/strict';
import { readFileSync, existsSync } from 'node:fs';
import { writeText, writeJSON, readJSON, exportSave, importSave, saveSelection, loadSelection, memoryStorage, SAVE_FORMAT, SAVE_VERSION, TMP, storage } from './save-store.js';
import { loadSave, writeSave, migrateCareer, clearSave, CAREER_SAVE_KEY, CAREER_SAVE_V1 } from './career-save.js';
import { Career, MEDAL, eventKey } from './career.js';

class Store { constructor() { this.map = new Map(); this.writes = []; } getItem(k) { return this.map.has(k) ? this.map.get(k) : null; } setItem(k, v) { this.writes.push(k); this.map.set(k, String(v)); } removeItem(k) { this.map.delete(k); } }

// ---- atomic writes and recovery ----
{
  const s = new Store();
  assert.equal(writeJSON('ssx3.x', { a: 1 }, s), true);
  assert.deepEqual(s.writes, ['ssx3.x' + TMP, 'ssx3.x'], 'temp copy first, then the key');
  assert.equal(s.getItem('ssx3.x' + TMP), null, 'temp copy removed after the swap');
  assert.deepEqual(readJSON('ssx3.x', (v) => v, s), { a: 1 });
  // a write interrupted after the temp copy: the complete temp copy is the newer value and finishes the swap
  s.setItem('ssx3.x' + TMP, JSON.stringify({ a: 2 }));
  assert.deepEqual(readJSON('ssx3.x', (v) => v, s), { a: 2 });
  assert.equal(s.getItem('ssx3.x'), JSON.stringify({ a: 2 })); assert.equal(s.getItem('ssx3.x' + TMP), null);
  // a torn temp copy is dropped, the key keeps the last good value
  s.setItem('ssx3.x' + TMP, '{"a":3');
  assert.deepEqual(readJSON('ssx3.x', (v) => v, s), { a: 2 }); assert.equal(s.getItem('ssx3.x' + TMP), null);
  // a validator rejecting the temp copy falls back to the key
  s.setItem('ssx3.x' + TMP, JSON.stringify({ b: 1 }));
  assert.deepEqual(readJSON('ssx3.x', (v) => (v.a ? v : null), s), { a: 2 });
}
{ // full storage: no room for the temp copy -> the key alone is written (setItem is atomic per key)
  const s = new Store(); s.setItem = function (k, v) { if (k.endsWith(TMP)) throw Error('QuotaExceededError'); this.map.set(k, String(v)); };
  assert.equal(writeText('ssx3.y', 'hello', s), true); assert.equal(s.getItem('ssx3.y'), 'hello');
  const full = new Store(); full.setItem = () => { throw Error('QuotaExceededError'); };
  assert.equal(writeText('ssx3.y', 'x', full), false, 'a full store reports false, never throws');
}
{ // blocked storage (sandboxed frame / disabled site data): nothing throws, reads are empty
  const blocked = { getItem() { throw Error('SecurityError'); }, setItem() { throw Error('SecurityError'); }, removeItem() { throw Error('SecurityError'); } };
  assert.equal(readJSON('ssx3.x', (v) => v, blocked), null); assert.equal(writeJSON('ssx3.x', 1, blocked), false);
  assert.equal(loadSave(blocked), null); assert.equal(writeSave({ version: 2, riders: {} }, blocked), false);
  const old = Object.getOwnPropertyDescriptor(globalThis, 'localStorage');
  Object.defineProperty(globalThis, 'localStorage', { get() { throw Error('SecurityError'); }, configurable: true });
  try { const m = storage(); m.setItem('k', 'v'); assert.equal(m.getItem('k'), 'v', 'in-memory stand-in for the session'); }
  finally { if (old) Object.defineProperty(globalThis, 'localStorage', old); else delete globalThis.localStorage; }
}

// ---- career save: v1 -> v2 migration and a whole-career reload ----
const data = JSON.parse(readFileSync(new URL('./public/assets/CAREER/career.json', import.meta.url)));
{
  const s = new Store();
  const v1 = { version: 1, seed: 7, records: null, riders: { zoe: { character: 4, cash: 4321, earned: 9000, medals: { [eventKey(0, 0)]: MEDAL.GOLD }, best: {}, events: {},
    attributes: [10, 5, 5, 5, 5, 5, 5], level: { race: { level: 1, counter: 0 }, freestyle: { level: 1, counter: 0 } }, peaks: [true, true, false], lodgePeak: 1, gearFlags: 'w1:1,2,3',
    collected: { 0: [5, 0] } }, bad: 'not a rider' } };
  s.setItem(CAREER_SAVE_V1, JSON.stringify(v1));
  const loaded = loadSave(s);
  assert.equal(loaded.version, 2); assert.equal(loaded.migratedFrom, 1); assert.equal(loaded.riders.zoe.cash, 4321); assert.equal(loaded.riders.bad, undefined, 'non-rider entries dropped');
  assert.ok(s.getItem(CAREER_SAVE_KEY), 'the migrated save is written as v2'); assert.ok(s.getItem(CAREER_SAVE_V1), 'the v1 key stays as a backup');
  const c = new Career(data, { storage: s, rosterSeed: () => 1 });
  assert.equal(c.rider('zoe').cash, 4321); assert.equal(c.medal('zoe', 0, 0), MEDAL.GOLD); assert.equal(c.rider('zoe').gearFlags, 'w1:1,2,3'); assert.equal(c.collectCount('zoe', 0), 2);
  assert.deepEqual(c.rider('zoe').peaks, [true, true, false]);
  assert.equal(migrateCareer({ version: 3 }), null); assert.equal(migrateCareer('x'), null); assert.equal(migrateCareer(null), null);
}
{ // progress made in one visit comes back in the next (a new Career over the same storage = a page reload)
  const s = new Store(), rosterSeed = () => 1;
  const a = new Career(data, { storage: s, rosterSeed });
  a.rider('mac').cash = 100000;
  assert.equal(a.buyAttribute('mac', 2), true);                                    // Buy Attributes
  a.rider('mac').medals[eventKey(0, 1)] = MEDAL.SILVER;                           // a medal (results screen writes it)
  a.markCollected('mac', 0, 3, 250);                                               // a collectible with its cash
  a.rider('mac').gearFlags = 'w1:5,6';                                             // Equip Gear
  a.rider('mac').messages = { inbox: [{ id: 'kT_MSGABTNATE2Moby1', read: false }] }; // relationship messages
  a.rider('mac').bigChallenges = [1, 0, 3];                                        // Big Challenges status words
  a.save.pending = { rider: 'mac', mode: 0, course: 1, career: false };
  assert.equal(a.persist(), true);
  const b = new Career(data, { storage: s, rosterSeed });
  const r = b.rider('mac');
  assert.equal(r.cash, a.rider('mac').cash); assert.deepEqual(r.attributes, a.rider('mac').attributes); assert.equal(b.medal('mac', 0, 1), MEDAL.SILVER);
  assert.equal(b.collectCount('mac', 0), 1); assert.equal(r.gearFlags, 'w1:5,6'); assert.equal(r.messages.inbox[0].id, 'kT_MSGABTNATE2Moby1');
  assert.deepEqual(r.bigChallenges, [1, 0, 3]); assert.equal(b.save.pending.course, 1); assert.ok(Number.isFinite(b.save.savedAt));
  assert.equal(clearSave(s), true); assert.equal(loadSave(s), null, 'New game: nothing left to load');
}

// ---- selection (last rider / peak / mode / event) ----
{
  const s = memoryStorage(new Map());
  assert.equal(loadSelection(s), null);
  saveSelection({ rider: 'zoe' }, s); saveSelection({ peak: 1, mode: 'freestyle', event: 'ASS1' }, s);
  const v = loadSelection(s); assert.equal(v.rider, 'zoe'); assert.equal(v.mode, 'freestyle'); assert.equal(v.event, 'ASS1');
}

// ---- save file export / import ----
{
  const a = new Store();
  const c = new Career(data, { storage: a, rosterSeed: () => 1 }); c.rider('zoe').cash = 777; c.persist();
  a.setItem('ssx3.outfit.free.v1', JSON.stringify({ zoe: 'w1:9' })); a.setItem('ssx3.playerName', 'OWEN'); a.setItem('ssx3.feOptions', JSON.stringify({ hud: 1 }));
  a.setItem('ssx3.mp.token', 'secret-device-token'); saveSelection({ rider: 'zoe' }, a);
  const file = exportSave(a, Date.UTC(2026, 8, 24));
  assert.equal(file.format, SAVE_FORMAT); assert.equal(file.version, SAVE_VERSION); assert.equal(file.exportedAt, '2026-09-24T00:00:00.000Z');
  assert.equal(file.keys['ssx3.mp.token'], undefined, 'device identity is not part of the save');
  const b = new Store(), r = importSave(JSON.parse(JSON.stringify(file)), b, { migrateCareer });
  assert.equal(r.ok, true, r.error);
  for (const k of Object.keys(file.keys)) assert.equal(b.getItem(k), a.getItem(k), k);
  assert.equal(new Career(data, { storage: b, rosterSeed: () => 1 }).rider('zoe').cash, 777);
  // an export from before the migration (v1 career key) lands as v2
  const old = { format: SAVE_FORMAT, version: 1, keys: { 'ssx3.career.v1': JSON.stringify({ version: 1, riders: { kaori: { cash: 5 } } }) } };
  const d = new Store(); assert.equal(importSave(old, d, { migrateCareer }).ok, true);
  assert.equal(JSON.parse(d.getItem(CAREER_SAVE_KEY)).riders.kaori.cash, 5); assert.equal(d.getItem('ssx3.career.v1'), null);
  // rejected files leave the storage alone
  const e = new Store();
  assert.equal(importSave({ format: 'other' }, e).ok, false);
  assert.equal(importSave({ format: SAVE_FORMAT, version: 99, keys: {} }, e).ok, false);
  assert.equal(importSave({ format: SAVE_FORMAT, version: 2, keys: { 'ssx3.feOptions': '{broken' } }, e).ok, false);
  assert.equal(importSave({ format: SAVE_FORMAT, version: 2, keys: { 'ssx3.career.v2': JSON.stringify({ version: 7 }) } }, e, { migrateCareer }).ok, false);
  assert.equal(importSave({ format: SAVE_FORMAT, version: 2, keys: { 'evil.key': 'x' } }, e).ok, false, 'unknown keys only: nothing to import');
  assert.equal(e.map.size, 0);
  const full = new Store(); full.setItem = () => { throw Error('QuotaExceededError'); };
  assert.equal(importSave(file, full, { migrateCareer }).ok, false);
}

// ---- Single Event selector (web/fe-event-select.js) ----
{
  globalThis.performance ??= { now: () => 0 };
  const { eventList, redPaths, peakUnlocked, INDICATOR_STATE, PATHS, stepIndex } = await import('./fe-event-select.js');
  const courses = JSON.parse(readFileSync(new URL('./public/assets/courses.json', import.meta.url))).courses;
  const playable = courses.flatMap((c) => (c.event === 'backcountry' ? [{ ...c, rivalMode: 4 }, { ...c, name: c.name + ' Jam', rivalMode: 5 }] : [c]));
  const race = eventList({ rules: data.rules, courses: data.courses, playable, peak: 1, mode: 'race' });
  assert.deepEqual(race.map((e) => e.name), ['Snow Jam', 'Metro-City', 'Happiness'], 'PS2 menus/single/06-race-events.png');
  const fs = eventList({ rules: data.rules, courses: data.courses, playable, peak: 1, mode: 'freestyle' });
  assert.deepEqual(fs.map((e) => e.name), ['R&B', "Crow's Nest", 'The Junction', 'Happiness Jam'], 'PS2 menus/single/07-fs-events-1.png');
  assert.ok([...race, ...fs].every((e) => e.playable && e.entry), 'every Peak 1 event starts a ported course');
  assert.equal(fs[3].entry.rivalMode, 5); assert.equal(race[2].entry.rivalMode, 4);
  // Peak 2 (docs/peak2.md): every event playable; Freestyle in the Map LUI row order (PS2 nav/p2/out-lists), not the goal list's
  const p2 = eventList({ rules: data.rules, courses: data.courses, playable, peak: 2, mode: 'race' });
  assert.deepEqual(p2.map((e) => e.name), ['Ruthless Ridge', 'Intimidator', 'Ruthless'], 'PS2 nav/p2/out-lists/p2-race-events.png');
  assert.ok(p2.every((e) => e.playable && e.entry), 'every Peak 2 race event starts a ported course');
  { const { mapRowOrder } = await import('./fe-event-select.js'); const menusFile = new URL('./public/assets/UI/fe-menus.json', import.meta.url);
    if (existsSync(menusFile)) { const map = JSON.parse(readFileSync(menusFile)).screens.Map;
      const p2fs = eventList({ rules: data.rules, courses: data.courses, playable, peak: 2, mode: 'freestyle', order: mapRowOrder(map, 2, 'freestyle') });
      assert.deepEqual(p2fs.map((e) => e.name), ['Style Mile', 'Launch Time', 'Schizophrenia', 'Ruthless Jam'], 'PS2 nav/p2/out-lists/p2-freestyle-events.png');
      assert.ok(p2fs.every((e) => e.playable && e.entry), 'every Peak 2 freestyle event starts a ported course');
      assert.deepEqual(eventList({ rules: data.rules, courses: data.courses, playable, peak: 1, mode: 'freestyle', order: mapRowOrder(map, 1, 'freestyle') }).map((e) => e.name), fs.map((e) => e.name)); } }
  assert.deepEqual([...redPaths(race, { screen: 'fe-mode', mode: 'race', index: 0 })].sort(), ['ABC1', 'ARA1', 'BRA2']);
  assert.deepEqual([...redPaths(fs, { screen: 'fe-mode', mode: 'freestyle', index: 0 })].sort(), ['ABA1', 'ASS1', 'BHP1']);
  assert.deepEqual([...redPaths(fs, { screen: 'fe-event', mode: 'freestyle', index: 3 })], ['ABC1']);
  assert.equal(peakUnlocked(1), true); assert.equal(peakUnlocked(2), true); assert.equal(peakUnlocked(3), true);   // Peak 3 ported (docs/peak3.md)
  assert.equal(stepIndex(0, -1, 3), 2); assert.equal(stepIndex(2, 1, 3), 0);
  // the start indicator of every mapped course sits on that route's first or last vertex (Map states 200..490)
  const menus = new URL('./public/assets/UI/fe-menus.json', import.meta.url);
  if (existsSync(menus)) {
    const map = JSON.parse(readFileSync(menus)).screens.Map, by = new Map(map.elements.map((e) => [e.name, e]));
    const pos = (n) => { const e = by.get(n), p = e.props || {}; const up = e.parent || e.menu; const q = up && by.has(up) ? pos(up) : [0, 0]; return [q[0] + (p[0] || 0), q[1] + (p[1] || 0)]; };
    const label = new Map(map.elements.filter((e) => e.label).map((e) => [e.label, e]));
    for (const [code, frame] of Object.entries(INDICATOR_STATE)) {
      if (!PATHS[code]) continue;
      const path = label.get(PATHS[code]), [px, py] = pos(path.name), nv = path.shape[0], P = path.props;
      const ends = [[px + P[21], py + P[22]], [px + P[21 + 9 * (nv - 1)], py + P[22 + 9 * (nv - 1)]]];
      const ev = map.events.find((v) => v.frame === frame && v.props?.[13] === 255 && by.get(v.element)?.label === 'indicator');
      const [ix, iy] = [pos(label.get('Indicators').name)[0] + ev.props[0], pos(label.get('Indicators').name)[1] + ev.props[1]];
      const d = Math.min(...ends.map(([x, y]) => Math.hypot(x - ix, y - iy)));
      assert.ok(d < 36, `${code}: indicator (${ix},${iy}) is ${d.toFixed(1)} px from its route`);
    }
  } else console.log('  (UI/fe-menus.json not exported: python3 tools/export_fe_menus.py; indicator check skipped)');
}
console.log('save store: atomic writes, recovery, blocked/full storage, career v1->v2 migration and reload, selection, export/import, Single Event lists OK');
