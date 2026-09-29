// Monster tricks (web/monster-tricks.js) against the core's detection table (initial.json original_trick_identity.named_tricks,
// exported from 0x43D608) and the core trick names (116950 with the monster name table 0x43D320).
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { MONSTER_TRICKS, MONSTER_LIST, unlockedMonsters, monsterMedal, runStats, recordRun } from './monster-tricks.js';
const named = JSON.parse(fs.readFileSync(new URL('public/assets/BHP1/initial.json', import.meta.url), 'utf8')).original_trick_identity.named_tricks;
assert.equal(named.length, 24);
named.forEach((row, k) => { const m = MONSTER_TRICKS[k]; assert.equal(m.id, row.id); assert.equal(m.bonus, row.points); assert.deepEqual(m.fields, row.identity_fields, m.name); });
assert.deepEqual([...MONSTER_LIST].sort((a, b) => a - b), MONSTER_TRICKS.map((m) => m.id));
assert.deepEqual(unlockedMonsters([1, 0, 0, 0, 0, 0, 0, 0]), [1]);
assert.deepEqual(unlockedMonsters([3, 3, 3, 3, 3, 3, 3, 3]).length, 24);
assert.deepEqual(unlockedMonsters([0, 2, 0, 0, 0, 0, 0, 1]), [5, 6, 17]);
assert.equal(monsterMedal(0, 12000), 2); assert.equal(monsterMedal(6, 1499), 0); assert.equal(monsterMedal(7, 100), 3);
// 0x155420 personal bests: counts as ints, floats (rail +0x154, handplant +0x158, air +0x14C) as (int)(f + 0.5).
{ const w = new Uint32Array(0x1D0 / 4); const fl = (o, v) => { w[o / 4] = new Uint32Array(new Float32Array([v]).buffer)[0]; };
  w[0x128 / 4] = 3; w[0x114 / 4] = 5; w[0x118 / 4] = 1; w[0x11C / 4] = 2; w[0x184 / 4] = 11; fl(0x158, 4.6); fl(0x154, 12000.4); fl(0x14C, 8.49);
  assert.deepEqual(runStats(w), [3, 5, 1, 2, 11, 5, 12000, 8]);
  const st = {}; assert.deepEqual(recordRun(st, runStats(w)), [3, 7, 1, 0, 2]); assert.deepEqual(st.medals, [2, 2, 2, 1, 0, 0, 0, 1]);
  assert.deepEqual(recordRun(st, [0, 0, 0, 0, 0, 0, 0, 0], { score: 500000, seconds: 90, scoreEvent: true }), [6]); assert.equal(st.medals[6], 2); assert.equal(st.bests[4], 11);
  assert.deepEqual(unlockedMonsters(st.medals), [1, 3, 5, 6, 2, 8, 12, 13, 15, 17]); }
// Career Highlights rows (web/career-highlights.js, 0x1F5DA0) from the exported 35car_stat screen.
{ const f = new URL('public/assets/UI/career-highlights.json', import.meta.url);
  if (fs.existsSync(f)) { const { highlightRow } = await import('./career-highlights.js'); const data = JSON.parse(fs.readFileSync(f, 'utf8'));
    for (const n of ['hl1', 'hlsec1', 'hlsec1a', 'checkmark1', 'checkbox1', 'HL_arrowup', 'HL_arrowdown']) assert.ok(data.names[n], n);
    assert.deepEqual(highlightRow(data, [1, 0, 0, 0, 0, 0, 0, 0], 0), { id: 1, on: true, label: 'Stay on a rail - 25m', title: 'Da Housecat  Monster Trick', text: 'FS 540 G-Money' });
    assert.equal(highlightRow(data, [1, 0, 0, 0, 0, 0, 0, 0], 1).text, 'Complete highlight to unlock description');
    assert.equal(highlightRow(data, [0, 0, 0, 0, 0, 0, 0, 3], 23).label, 'Do a 100x combo in an event.'); } }
console.log('monster tricks: 24 table rows match the core detection table; FE list order and medal unlock rule checked');
