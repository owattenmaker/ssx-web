// Conquer the Mountain messages (web/career-messages.js) against the original rules and the PS2 frames
// (docs/characters.md "Relationship messages" / "Career messages"; data: tools/export_messages.py -> public/assets/CAREER/messages.json).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { MessageInbox, MessageRules, CareerMessages, INBOX_SIZE, PEAK_RIVALS, hudTint } from './career-messages.js';
import { ageRelationships, applyRelationshipEvent } from './lineup.js';
import { Career, MEDAL } from './career.js';

const data = JSON.parse(fs.readFileSync('public/assets/CAREER/messages.json'));
const careerData = JSON.parse(fs.readFileSync('public/assets/CAREER/career.json'));
class Memory { constructor() { this.map = new Map(); } getItem(k) { return this.map.get(k) ?? null; } setItem(k, v) { this.map.set(k, String(v)); } removeItem(k) { this.map.delete(k); } }
// A scripted presentation generator: returns the listed words in order and counts the draws.
const script = (...words) => { const f = () => { f.used++; assert.ok(words.length, 'unexpected draw'); return words.shift(); }; f.used = 0; return f; };

// ---- the original tables (0x441630 categories, 0x4C6C08 records, locale) -----------------------------------------
assert.equal(data.categories.length, 61); assert.equal(data.records.length, 256, 'categories 59/60 = items 254/255 (0x1E2428 / 0x1E2518)');
for (let c = 0; c < 10; c++) {
  const row = data.categories[12 + c];
  assert.equal(row.kind, 5, 'categories 12..21: the aggression messages (12 + character)'); assert.equal(row.count, 3);
  for (let i = row.first; i < row.first + row.count; i++) { assert.equal(data.records[i].category, 12 + c); assert.equal(data.records[i].sender, c); assert.equal(data.records[i].flag, 1); }
}
data.categories.forEach((c, k) => { if (k) assert.equal(c.first, data.categories[k - 1].first + data.categories[k - 1].count, 'first = prefix sums'); });
assert.equal(data.categories.filter((c) => c.kind === 2 && c.folder).length, 5, 'five FAQ folders');
assert.equal(data.subjects.aggression.length, 11);
// PS2 frames local/ps2-capture/menus/lineup-messages*.png (Zoe's inbox poked to (0x6D, 3), (0x58, 0)):
const zoe = new MessageInbox(data, { entries: [[0x6D, 3], [0x58, 0]], read: 0, posted: [] });
assert.deepEqual([zoe.sender(0x58), zoe.subject(0x58, 0)], ['Kaori', "Where's the love?"]);
assert.deepEqual([zoe.sender(0x6D), zoe.subject(0x6D, 3)], ['Psymon', 'Hate Mail']);
assert.ok(data.records[0x58].body.startsWith('Guess what?  Nice just went nasty.'), 'the message view body (lineup-messages.f00560)');
assert.equal(zoe.sender(data.categories[10].first + 1), 'SSX 3', 'FAQ sender');
assert.equal(zoe.sender(251), 'Atomika', 'mountain notices: kT_CMNDJAtomica (0x1E2EA0)');
// PS2 frame local/ps2-capture/menus/lineup-message-kinds.f00300.png (a derived state with one message of each kind):
const kinds = new MessageInbox(data, { entries: [[0, 0], [235, 0], [0x73, 3], [0x74, 2], [141, 4], [252, 1]], read: 0, posted: [] });
assert.deepEqual(kinds.box.entries.slice().reverse().map(([i, v]) => [kinds.sender(i), kinds.subject(i, v)]),
  [['Atomika', 'Oh yeah!'], ['Viggo', 'The Big Boss'], ['Mac', 'Way to go!'], ['Mac', 'Practice, practice, practice'], ['Atomika', 'Peak Challenge'], ['Mac', 'Backcountry Challenge']]);
assert.deepEqual(data.categories.filter((c) => c.kind === 2 && c.folder).map((c) => data.records[c.first].subject),
  ['Progression/Rewards', 'The Basics', 'Game Modes', 'Getting Around', 'Miscellaneous'], 'folder rows (lineup-messages.f00200)');
assert.deepEqual([data.texts.message_center, data.texts.total.replace('%d', 2), data.texts.unread.replace('%d', 1), data.texts.select_message, data.texts.collapse],
  ['Message Center', 'Total messages: 2', 'Unread: 1', 'Select a message to view.', 'Collapse']);

// ---- the inbox (0x1E2FE0 add, 0x1E3100 variant, 0x1E31B8 remove) --------------------------------------------------
{
  const box = new MessageInbox(data);
  let d = script(25); assert.deepEqual(box.add(0x58, d), { item: 0x58, variant: 25 % 11 }, 'aggression variant = draw % 11'); assert.equal(d.used, 1);
  d = script(); assert.deepEqual(box.add(data.categories[10].first, d), { item: data.categories[10].first, variant: 0 }, 'flag 0: no draw');
  assert.equal(box.count, 2); assert.equal(box.unread(), 2);
  box.markRead(0); assert.equal(box.unread(), 1); assert.ok(box.isRead(0) && !box.isRead(1));
  box.remove(0); assert.equal(box.count, 1); assert.ok(!box.isRead(0), 'read bits move up with the entries');
  box.markRead(0); box.add(0x6D, script(3)); assert.ok(box.isRead(0) && !box.isRead(1), 'a new message clears its read bit');
  // 25 entries: the next one drops the oldest
  const full = new MessageInbox(data);
  for (let k = 0; k < INBOX_SIZE; k++) { full.add(85 + (k % 30), () => k); if (k % 2) full.markRead(k); }
  assert.equal(full.count, 25);
  full.add(0x6D, () => 7);
  assert.equal(full.count, 25); assert.deepEqual(full.box.entries[0], [86, 1 % 11], 'oldest dropped');
  assert.deepEqual(full.box.entries[24], [0x6D, 7]);
  for (let k = 0; k < 24; k++) assert.equal(full.isRead(k), (k + 1) % 2 === 1, `read bit ${k} shifted`);
  assert.ok(!full.isRead(24));
}

// ---- 0x1E2A08 posting rules --------------------------------------------------------------------------------------
{
  const box = new MessageInbox(data), kaori = 1, human = 4;
  let d = script(); assert.equal(box.notify(human, 30, human, d), null, 'never about the human'); assert.equal(d.used, 0);
  d = script(5); assert.equal(box.notify(kaori, 20, human, d), null, '(5 & 7) + 15 = 20 is not < 20'); assert.equal(d.used, 1);
  d = script(8 + 4, 7, 14); const e = box.notify(kaori, 20, human, d);
  assert.deepEqual(e, { item: 88 + 7 % 3, variant: 14 % 11 }, 'item = first + draw % count, then the variant');
  assert.equal(d.used, 3); assert.ok(box.posted(13)); assert.equal(box.count, 1);
  d = script(); assert.equal(box.notify(kaori, 40, human, d), null, 'posted: no second message'); assert.equal(d.used, 0);
  assert.equal(box.notify(kaori, 5, human, d), null); assert.ok(box.posted(13), 'score 5 keeps the bit');
  assert.equal(box.notify(kaori, 4, human, d), null); assert.ok(!box.posted(13), 'score < 5 clears the bit'); assert.equal(d.used, 0);
  d = script(0, 0, 0); assert.ok(box.notify(kaori, 16, human, d), '0 + 15 < 16 posts again'); assert.equal(box.count, 2);
  d = script(0, 1, 0); assert.ok(box.notify(3, 16, 3 === human ? -1 : human, d)); assert.ok(box.posted(15), 'Mac: category 15');
}

// ---- Conquer the Mountain only, the HUD icon, the save --------------------------------------------------------------
{
  const storage = new Memory(), career = new Career(careerData, { storage });
  const cu = { ui: {}, career, riderId: 'zoe', active: null, get me() { return career.rider('zoe'); } };
  const messages = new CareerMessages(cu); messages.data = data;
  assert.equal(messages.notify(1, 40), null, 'no career event: nothing posted (0x5305F9 != 0)'); assert.equal(messages.hud, null);
  cu.active = { career: true };
  const humanBase = messages.humanBase(), other = humanBase === 1 ? 2 : 1;
  let entry = null; for (let k = 0; k < 64 && !entry; k++) entry = messages.notify(other, 22);   // the session generator; 22 > (r & 7) + 15 for most draws
  assert.ok(entry, 'posted in a career event'); assert.deepEqual(messages.hud, { frames: 0 }, 'HUD event 8');
  const saved = JSON.parse(storage.getItem('ssx3.career.v2'));
  assert.deepEqual(saved.riders.zoe.messages.entries, [[entry.item, entry.variant]], 'the inbox is in the career save');
  const again = new Career(careerData, { storage }); assert.deepEqual(again.rider('zoe').messages, career.rider('zoe').messages, 'round trip');
  // a save written before messages existed
  const old = new Memory(); const legacy = JSON.parse(storage.getItem('ssx3.career.v2')); delete legacy.riders.zoe.messages;
  old.setItem('ssx3.career.v1', JSON.stringify(legacy));
  const oldCareer = new Career(careerData, { storage: old });
  const cu2 = { ui: {}, career: oldCareer, active: null, get me() { return oldCareer.rider('zoe'); } };
  const m2 = new CareerMessages(cu2); m2.data = data;
  assert.equal(oldCareer.rider('zoe').cash, career.rider('zoe').cash); assert.equal(m2.inbox().count, 0, 'old saves: empty inbox');
  // mail icon: white while the 1 s phase <= 0.5, orange after, 5 s (PS2 mail/out samples 11 and 40)
  const spec = data.hud;
  assert.deepEqual([hudTint(spec, 0), hudTint(spec, 30), hudTint(spec, 31), hudTint(spec, 59), hudTint(spec, 60), hudTint(spec, 299), hudTint(spec, 300)],
    [spec.colors[0], spec.colors[0], spec.colors[1], spec.colors[1], spec.colors[0], spec.colors[1], null]);
  assert.deepEqual(spec.colors[0], [1, 1, 1]); assert.ok(spec.colors[1][0] > 0.8 && spec.colors[1][1] < 0.4 && spec.colors[1][2] === 0, 'orange');
  assert.equal(data.sprites.mail_icon.page, 'OV_1-4');
}

// ---- the notices: 0x155E58 ageing (every record, participant order) and 0x155BF0 (the human's record rising) ----------
{
  const row = (score) => Array.from({ length: 10 }, (_, o) => [0, Math.trunc(((score + o) % 30) / 5), (score + o) % 30].map((v) => v.toString(16).padStart(2, '0')).join(''));
  const table = [Array.from({ length: 10 }, (_, c) => row(c * 3))];
  const calls = []; ageRelationships(table, [4, 8], [0, 0], (other, score) => calls.push([other, score]));
  assert.equal(calls.length, 20);
  assert.deepEqual(calls.slice(0, 10), Array.from({ length: 10 }, (_, o) => [o, Math.max(((12 + o) % 30) - 3, 0)]), 'Zoe\'s records first, aged score');
  assert.deepEqual(calls.slice(10), Array.from({ length: 10 }, (_, o) => [o, Math.max(((24 + o) % 30) - 3, 0)]));
  const t2 = [table[0].map((r) => [...r])]; t2[0][4][8] = '000105';   // Zoe's record about Psymon: kind 0, level 1, score 5
  const [before, after, score] = applyRelationshipEvent(t2, [4, 8], [0, 0], 1, 0, 3);   // Zoe (slot 0) crash-attacked Psymon
  assert.deepEqual([before, after, score], [1, 2, 11]); assert.ok(after > before, 'the level rise that posts via 0x1E2A08(Psymon, score)');
}
// ---- the other posters (0x154EE8 chain, 0x159CD0 awards, 0x1E3760) -------------------------------------------------
{
  const zoeCh = 4, macCh = 3;
  // 0x1E32C8 stops early after a removal (step count against the shrinking count)
  const q = new MessageInbox(data); q.add(0x58, () => 0); q.add(0, () => 0); q.add(1, () => 0); q.add(2, () => 0);
  q.removeCategory(1); assert.deepEqual(q.box.entries.map((e) => e[0]), [0x58, 2], 'the last category-1 entry survives, as on the PS2');
  // backcountry results 0x1E2648 / 0x1E2828
  let box = new MessageInbox(data), rules = new MessageRules(data, box, script(3));
  box.add(0, () => 0); rules.backcountryResult(4, 14, 1, zoeCh);
  assert.deepEqual(box.box.entries, [[0x73, 3]], 'loss: reminder removed, 0x73, variant r % 10');
  assert.equal(box.subject(0x73, 3), data.subjects.bc_loss[3]); assert.ok(!box.posted(22));
  rules = new MessageRules(data, box, script(12)); rules.backcountryResult(4, 14, 0, macCh);
  assert.deepEqual(box.box.entries, [[0x76, 2]], 'win as Mac: 0x76, category 22 cleared first'); assert.ok(box.posted(22)); assert.equal(box.subject(0x76, 2), data.subjects.bc_win[2]);
  rules = new MessageRules(data, box, script()); rules.backcountryResult(4, 14, 0, zoeCh); assert.equal(box.count, 1, 'posted: no more results');
  new MessageRules(data, box, script(0)).backcountryResult(5, 16, 1, 8); assert.deepEqual(box.box.entries.at(-1)[0], 0x7F + 8 + 2, 'rival jam peak 3 loss as Psymon');
  // peak challenge flags 0x1E1DD0 and notices 0x1E1EB8 / 0x1E2220 / 0x1E20D0
  box = new MessageInbox(data); rules = new MessageRules(data, box, script());
  rules.peakChallengeFlag(6, true); assert.ok(box.posted(41)); rules.peakChallengeFlag(10, false); assert.ok(box.posted(46));
  rules.pendingPeakChallenges();
  assert.deepEqual(box.box.entries.map((e) => e[0]), [236, 241], 'race peak 1 (silver: first + 1), then jam peak 2 (bronze)');
  assert.ok(box.posted(34) && !box.posted(37) && !box.posted(41) && !box.posted(46));
  rules.peakChallengeFlag(6, false); assert.ok(!box.posted(40), 'peak 1 race notice posted: no flag');
  // rival reminders 0x1E1550
  box = new MessageInbox(data); rules = new MessageRules(data, box, script(5, 7));
  rules.rivalReminders(zoeCh, (p, list) => p === 0, () => false);
  assert.deepEqual(box.box.entries.map((e) => e[0]), [5 % 3, 18 + 7 % 3], 'race and jam reminders of peak 1'); assert.ok(box.posted(1) && box.posted(4));
  rules = new MessageRules(data, box, script()); rules.rivalReminders(zoeCh, () => true, (p) => p === 1);
  assert.equal(box.count, 2, 'peak 2 rival already medalled: only the first unposted category is tried');
  box = new MessageInbox(data); rules = new MessageRules(data, box, script(1)); rules.rivalReminders(macCh, (p, l) => l === 0, () => false);
  assert.deepEqual(box.box.entries[0][0], 3 + 1, 'Mac gets Griff\'s reminders');
  // taunts 0x1E1C10 and the goal walk
  box = new MessageInbox(data); rules = new MessageRules(data, box, script(0, 0, 0, 0));
  rules.goalWalk(0, 'race', [{ mode: 0, medalled: true }, { mode: 0, medalled: false }, { mode: 4, medalled: false }, { mode: 6, medalled: false }], zoeCh);
  const t1 = box.box.entries[0][0]; assert.equal(data.records[t1].category, 28); assert.notEqual(data.records[t1].sender, zoeCh, 'not from the human');
  rules.goalWalk(0, 'race', [{ mode: 0, medalled: true }, { mode: 0, medalled: false }], zoeCh);
  assert.notEqual(box.box.entries[1][0], t1, 'no item twice'); assert.ok(!box.posted(28));
  rules.goalWalk(0, 'race', [{ mode: 0, medalled: true }, { mode: 0, medalled: true }, { mode: 4, medalled: false }], zoeCh);
  assert.equal(box.count, 2); assert.ok(box.posted(28) && box.posted(29), 'rival open: taunts stop (0x1E2370)');
  box = new MessageInbox(data); rules = new MessageRules(data, box, script(0, 4)); rules.rivalTaunt(0, macCh);
  assert.equal(data.records[box.box.entries[0][0]].category, 29, 'the rival\'s rider: category 29');
  assert.equal(box.subject(box.box.entries[0][0], 4), data.subjects.foreshadow[4]);
  // awards 0x159CD0 and earnings 0x1E3760
  box = new MessageInbox(data); rules = new MessageRules(data, box, script(1, 2, 3, 0, 0));
  rules.award(2, () => true, () => false); rules.award(2, () => true, () => false); rules.award(3, () => true, () => false); rules.award(3, () => true, () => false);
  assert.deepEqual(box.box.entries, [[252, 1], [253, 2], [253, 3]], 'award 3 tests bit 58 but sets 57: it repeats');
  assert.equal(box.subject(252, 1), data.subjects.beat_the_peak[1]);
  rules.award(11, (n) => n === 1, (p) => p === 0); assert.deepEqual(box.box.entries.at(-1), [249, 0]);
  rules.earnings(500, 99400, careerData.rules.earnings_goal, () => true); assert.equal(box.count, 4, '99 900 < 100 000');
  rules.earnings(600, 99400, careerData.rules.earnings_goal, () => true); assert.deepEqual(box.box.entries.at(-1)[0], 247);
}

// ---- the career hooks (0x154EE8 order) on web/career.js ---------------------------------------------------------------
{
  const storage = new Memory(), career = new Career(careerData, { storage });
  const cu = { ui: {}, career, riderId: 'zoe', active: null, get me() { return career.rider('zoe'); } };
  const m = new CareerMessages(cu); m.data = data; m.hook(career);
  const play = (mode, course, place) => { career.startEvent('zoe', mode, course, true); career.active.ev.round = 3; return career.completeEvent(place, { ticks: 99999, score: 999999 }); };
  const box = () => m.inbox();
  assert.equal(play(0, 0, 0).medal, MEDAL.GOLD);
  assert.equal(box().count, 1); assert.equal(data.records[box().box.entries[0][0]].category, 28, 'Snow Jam gold, Metro-City missing: a rival taunt');
  play(0, 1, 0);
  assert.equal(box().count, 1, 'rival opens: no taunt, no reminder yet (0x1E1550 runs before the walk)'); assert.ok(box().posted(28));
  play(1, 5, 0);
  const last = box().box.entries.at(-1)[0]; assert.equal(data.records[last].category, 1, 'next career event: the rival race reminder');
  assert.ok(last < 3, 'Zoe gets Mac\'s texts');
  play(4, 14, 1);
  assert.equal(data.records[box().box.entries.at(-1)[0]].category, 22); assert.ok(!box().box.entries.some((e) => data.records[e[0]].category === 1), 'reminder removed');
  const n = box().count; play(0, 0, 5); assert.equal(box().count, n, 'no medal: only the backcountry result could post');
  // single events post nothing
  career.startEvent('zoe', 0, 2, false); career.active.ev.round = 3; career.completeEvent(0, { ticks: 1 }); assert.equal(box().count, n);
  assert.deepEqual(JSON.parse(storage.getItem('ssx3.career.v2')).riders.zoe.messages.entries, box().box.entries, 'saved');
}
// pv ctmSmallFixes: the message view's Delete is 0x1E5800 -> 1E3268(item): the FIRST entry with that item goes (a repeated message:
// the oldest), not the viewed one; the list's Square deletes by index.
{
  const me = { messages: { entries: [[0x58, 0], [0x6D, 3], [0x58, 1]], read: 0, posted: [] } };
  const ui = { screen: 'ctm-message', index: 1, set(s) { this.screen = s; }, sync() {} };
  const m = new CareerMessages({ ui, me, career: { persist() {} } }); m.data = data;
  for (const [on, want] of [[true, [[0x6D, 3], [0x58, 1]]]]) {
    me.messages.entries = [[0x58, 0], [0x6D, 3], [0x58, 1]];
    m.view = { type: 'message', index: 2, item: 0x58, variant: 1 }; ui.screen = 'ctm-message'; m.choose(1);
    assert.deepEqual(me.messages.entries, want, on ? 'the oldest 0x58 removed (1E3268)' : 'switch off: the viewed entry');
  }
}
// pv mailFreeze: the event's posts start the icon where the PS2's froze under WS5 (182 frames: race-f res 3.033), and the Message Center
// freezes it (HUD events 3 / 4) instead of clearing it.
{
  const { setPv } = await import('./pv-flags.js');
  for (const on of [true, false]) {
    setPv('mailFreeze', on);
    const storage = new Memory(), career = new Career(careerData, { storage });
    const ui = { screen: 'game', index: 0, set(x) { this.screen = x; }, sync() {} };
    const cu = { ui, career, riderId: 'zoe', active: null, get me() { return career.rider('zoe'); } };
    const m = new CareerMessages(cu); m.data = data; m.hook(career);
    career.startEvent('zoe', 0, 0, true); career.active.ev.round = 3; career.completeEvent(0, { ticks: 99999, score: 999999 });
    assert.deepEqual(m.hud, { frames: on ? 182 : 0 }, on ? 'the icon at 182 frames' : 'off: a full 5 s');
    m.open(() => {});
    assert.deepEqual(m.hud, on ? { frames: 182 } : null, on ? 'the Message Center freezes the icon' : 'off: cleared');
  }
  setPv('mailFreeze', null);
}
console.log('test-messages: ok');
