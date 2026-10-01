// The lodge and career features in play (docs/career-events.md "Lodge shops, awards and attributes"), against the PS2 runs in
// local/ps2-capture/lodge/runs (ARMSX2, derived from menus/ctm/state-lodge-peak1 with the menu hook cleaned):
//  1. careerRider (web/career-rider.js, main.js): the rider entry carries the profile's uber rows and outfit, and is resolved again
//     when the mode, the gear record or the uber selection changes (l2: Madonna bought and selected -> runtime byte 0x5316D1 = 2,
//     which 0x14FEA8 reads in the air; the capture gate monster-stoneage performs it). l8 / l10: the equipped board is ridden
//     out of the station.
//  2. lodgeDetails (web/lodge-ui.js, web/fe-screens.js): the lodge's Rider Details as on the PS2 (every item but Cheat
//     Characters live, Down skips it), Ubertrick Setup = 66ut_btnmap with the buy popup (l2: $10,000 -> lock 0x3FC -> 0x3F8).
//  3. playerName: 'PLAYER 1', 8 characters, a letter at the limit replaces the last one (l4: 'a' x 3 -> 'PLAYER a'), and the
//     records take the name (name-records: OWEN on Top 5 Record Times).
//  4. riderMusic (web/audio-menu.js, web/game-audio.js): radio mode and custom playlist per rider (l6 / l7: owned 0x3,
//     playlist 0x3, R+0xF80 = 1; l10: the world load applies them, audio+0x608C = 1, audio+0x6098 = 0x3).
//  5. lodgeCheats (web/lodge-ui.js, web/character-select.js, main.js careerId): l11 buys Brodi in Rewards > Cheat Characters,
//     l12 opens Rider Details > Cheat Characters (now live: Zoe and Brodi, no arrows) and picks Brodi (setup slot 0x535B20
//     +0x12 = 10, nothing in the profile), l13 rides out of the station as Brodi.
//  7. lodgeRewards (web/fe-options.js): the lodge's Rewards is the rewards room that sells at this lodge's peak (l11: '$' on
//     Brodi / JP / Marisol, 'This item is for sale.', the popup 'Cheat Character:' Brodi, Cost $20,000, You have $ 200,000).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { Career, PLAYER_NAME, MODE, recordSlot } from './career.js';
import { UBER_ROWS, uberEntries, uberRow, uberChoiceRows, initialUber } from './lodge.js';
import { uberSelection, uberRows, riderStamp, withProfile } from './career-rider.js';
import { outfitStamp } from './wardrobe.js';
import { loadCharacter, humanSettings, composeCheat } from './character-roster.js';
import { keyboardPress, PS2_PLAYER_NAME, PLAYER_NAME_MAX, defaultPlayerName } from './fe-screens.js';
import { LodgeScreens } from './lodge-ui.js';
import { riderMusic } from './audio-menu.js';
import { CharacterSelect } from './character-select.js';
import { CareerScreens } from './career-ui.js';
import { FeExtraScreens } from './fe-options.js';

const read = (p) => JSON.parse(fs.readFileSync(new URL(p, import.meta.url), 'utf8'));
const data = read('./public/assets/CAREER/career.json'), shop = read('./public/assets/CAREER/shop.json');
const select = read('./public/assets/UI/character-select.json'), initial = read('./public/assets/ANIMATIONS/initial.json');
const points = select.uber_points.table;
class Memory { constructor() { this.map = new Map(); } getItem(k) { return this.map.get(k) ?? null; } setItem(k, v) { this.map.set(k, String(v)); } removeItem(k) { this.map.delete(k); } }
globalThis.fetch = async (url) => {   // character-roster.js reads RIDER_<ID>/settings.json
  const f = new URL('./public' + String(url).split('?')[0], import.meta.url);
  if (!fs.existsSync(f)) return { ok: false, status: 404, headers: { get: () => 'text/html' }, json: async () => null };
  return { ok: true, status: 200, headers: { get: () => 'application/json' }, json: async () => JSON.parse(fs.readFileSync(f, 'utf8')) };
};
const zoe = { id: 'zoe', name: 'Zoe', package: 'RIDER_ZOE', kind: 'rider', character: 4 };
const newUi = (career) => ({ careerMode: true, onlineMode: false, careerUI: { career }, characterSelect: { data: select } });

// ---- 1. careerRider ---------------------------------------------------------------------------------------------------
{
  const c = new Career(data, { storage: new Memory(), shop }), ui = newUi(c);
  assert.equal(uberSelection(ui, zoe), null, 'no record yet: the package defaults');
  assert.deepEqual(uberRows(ui, zoe), []);
  const r = c.rider('zoe'); r.cash = 200000;   // PS2 l1 poke: cash 200,000
  c.uber('zoe');
  assert.deepEqual(uberRows(ui, zoe), [], "the rider's own selection adds no rows (initial.json holds Zoe's twelve)");
  const fresh = riderStamp(ui, zoe);
  // PS2 l2: Nose Grab (category 4) Madonna, entry 2: $10,000, lock 0x3FC -> 0x3F8, then selected (runtime byte 0x5316D1 = 2)
  assert.equal(c.uber('zoe')[4].lock, 1020); assert.equal(c.uber('zoe')[4].selected, 10, 'Pommel Me');
  assert.ok(c.buyUber('zoe', 4, 2)); assert.equal(r.cash, 190000); assert.equal(c.uber('zoe')[4].lock, 1016);
  assert.equal(c.uber('zoe')[4].selected, 10, 'buying does not select (0x184F40)');
  assert.ok(c.selectUber('zoe', 4, 2));
  const stamp = riderStamp(ui, zoe);
  assert.notEqual(stamp, fresh, 'a new selection makes the resolved rider stale');
  const rows = uberRows(ui, zoe);
  assert.equal(rows.length, 1); assert.deepEqual(rows[0], [1, 4, uberRow(uberEntries(shop, 4)[2], points)]);
  assert.equal(rows[0][2].semantic, 138, 'Madonna = trick id 138 (UBER_NOSE...)');
  const entry = withProfile(ui, zoe, zoe);
  assert.equal(entry.stamp, stamp); assert.equal(entry.entry, zoe); assert.equal(entry.uber, JSON.stringify([[4, 138, 188, 67]]));
  // the rows reach the grab profile (web/character-roster.js -> humanSettings): set 1 slot 4 only
  const character = await loadCharacter(entry), settings = humanSettings(initial, character);
  const uber = settings.original_grab_control.profile.uber;
  assert.equal(uber[1][4].semantic, 138); assert.equal(uber[1][4].score_id, 67);
  assert.deepEqual(uber[0][4], initial.original_grab_control.profile.uber[0][4], 'the hidden base uber (tier < 5) is unchanged');
  for (let k = 0; k < 15; k++) if (k !== 4) assert.deepEqual(uber[1][k], initial.original_grab_control.profile.uber[1][k]);
  // what else makes the entry stale: the mode, the gear record
  const inCareer = riderStamp(ui, zoe); ui.careerMode = false;
  assert.notEqual(riderStamp(ui, zoe), inCareer, 'career -> free play');
  ui.careerMode = true; assert.equal(riderStamp(ui, zoe), inCareer);
  const before = outfitStamp(ui, zoe); r.gearFlags = { ...(r.gearFlags || {}), 215: 0x16 };
  assert.notEqual(outfitStamp(ui, zoe), before, 'a committed gear row changes the outfit stamp');
  assert.notEqual(riderStamp(ui, zoe), inCareer);
  r.cash = 5; assert.equal(riderStamp(ui, zoe), riderStamp(ui, zoe), 'cash is not part of it');
  // a cheat skin performs its base rider's ubers; the skin's own overrides still win (0x150198 returns them first)
  const snow = { id: 'snowballs', kind: 'cheat', base: 'zoe', package: 'RIDER_SNOWBALLS' };
  assert.deepEqual(uberRows(ui, snow), rows);
  assert.equal(outfitStamp(ui, snow), 'fixed');
  // Sam and the others without score table: nothing (no guessed points)
  assert.deepEqual(uberRows({ ...ui, characterSelect: null }, zoe), []);
  console.log('careerRider: uber rows from the profile record (Madonna 138 in set 1 slot 4), stale on mode / gear / selection');
}

// ---- 2. lodgeDetails --------------------------------------------------------------------------------------------------
{
  const c = new Career(data, { storage: new Memory(), shop });
  const calls = [];
  const fe = { kbLui: {}, owns: (s) => s === 'fe-uber' || s === 'fe-profile', openFromLodge: (s, back) => { calls.push(s); fe.back = back; }, openKeyboard: (k) => calls.push('kb:' + k), keyboard: null };
  const ui = { screen: 'ctm-details', index: 0, feScreens: fe, set(s) { this.screen = s; }, sync() {}, riders: [] };
  const cs = { ui, career: c, riderId: 'zoe', t: (k, f) => f, lodgePeak: 1 };
  const lodge = new LodgeScreens(cs);
  assert.deepEqual([0, 1, 2, 3, 4, 5, 6].map((i) => lodge.disabled('ctm-details', i)), [false, false, true, false, false, false, false], 'PS2 lodge/20-rider-details: only Cheat Characters greyed');
  lodge.choose(3); lodge.choose(5); lodge.choose(6);
  assert.deepEqual(calls, ['fe-uber', 'kb:name', 'fe-profile']);
  fe.back(); assert.equal(ui.screen, 'ctm-details'); assert.equal(ui.index, 6, 'back from Rider Profile to its row');
  // Down skips Cheat Characters and wraps (PS2 l1: Trophies -> Ubertrick Setup)
  const key = (code) => lodge.key({ code, preventDefault() {} });
  ui.index = 1; key('ArrowDown'); assert.equal(ui.index, 3); ui.index = 6; key('ArrowDown'); assert.equal(ui.index, 0);
  console.log('lodgeDetails: Rider Details items, Ubertrick Setup / Player Name / Rider Profile routes, the menu skip');
}

// ---- 3. playerName ----------------------------------------------------------------------------------------------------
{
  assert.equal(defaultPlayerName(), 'PLAYER 1'); assert.equal(PS2_PLAYER_NAME, 'PLAYER 1'); assert.equal(PLAYER_NAME_MAX, 8);
  assert.equal(PLAYER_NAME, 'PLAYER 1', 'career default = 0x147170 with an empty slot name');
  let k = { kind: 'name', text: 'PLAYER 1', caret: 7, caps: false, shift: false, max: 8, overwriteFull: true };
  for (let i = 0; i < 3; i++) k = keyboardPress(k, 'a');
  assert.equal(k.text, 'PLAYER a', 'PS2 l4: three a on a full name -> PLAYER a'); assert.equal(k.caret, 7);
  k = keyboardPress({ ...k, text: 'OW', caret: 2 }, 'e'); assert.equal(k.text, 'OWe'); assert.equal(k.caret, 3);
  // the records' player entry (0x154DDC -> 0x147170): PS2 name-records shows OWEN on Top 5 Record Times
  const c = new Career(data, { storage: new Memory() }); c.playerName = 'OWEN';
  const slot = recordSlot(data.rules, MODE.RACE, 0); c.save.records[slot].forEach((e, i) => { e.value = 900 + i; delete e.ticks; });
  c.startEvent('zoe', MODE.RACE, 0, true); assert.equal(c.heatRecord(235 * 60), 0);
  assert.equal(c.records(slot, true)[0].name, 'OWEN'); assert.ok(c.records(slot, true)[0].player);
  const d = new Career(data, { storage: new Memory() }); d.startEvent('zoe', MODE.RACE, 0, true);
  d.save.records[slot].forEach((e, i) => { e.value = 900 + i; delete e.ticks; }); d.heatRecord(235 * 60);
  assert.equal(d.records(slot, true)[0].name, 'PLAYER 1', 'no name entered');
  console.log('playerName: PLAYER 1, 8 characters, overwrite at the limit, the records name');
}

// ---- 4. riderMusic ----------------------------------------------------------------------------------------------------
{
  assert.deepEqual(riderMusic({ owned: [0, 1], playlist: [0, 1], radioMode: 1 }).radioMode, 1);
  assert.deepEqual(riderMusic({ owned: [0, 1], playlist: [0, 1], radioMode: 1 }).playlist.flatMap((b, i) => (b ? [i] : [])), [0, 1]);
  assert.equal(riderMusic({ owned: [], playlist: [] }, { radioMode: 3 }).radioMode, 0, 'a custom mode needs a playlist (0x196B08)');
  assert.equal(riderMusic({ owned: [4], playlist: [4] }, { radioMode: 2 }).radioMode, 2, 'a record from before keeps the global mode');
  assert.equal(riderMusic({ owned: [4], playlist: [4], radioMode: 0 }, { radioMode: 2 }).radioMode, 0);
  const mod = await import('./game-audio.js').catch(() => null);
  if (mod) {
    const root = new URL('./public', import.meta.url).pathname; let T = 1000;
    const ga = mod.createGameAudio({ now: () => T, fetchJson: async (p) => JSON.parse(fs.readFileSync(root + p, 'utf8')), fetchBytes: async (p) => new Uint8Array(fs.readFileSync(root + p)) });
    ga.riderMusic = () => riderMusic({ owned: [0, 1], playlist: [0, 1], radioMode: 1 });   // PS2 l6 / l7: Deep End, All Night; Custom Playlist [DJ]
    for (let n = 0; n < 12; n++) {
      ga.timelineReset?.();
      await ga.worldLoaded({ courseIndex: n % 5, singleEvent: true, courseCode: 'ARA1', character: 'zoe' });
      assert.equal(ga.getSettings().radioMode, 1);
      const picks = ga.timeline().filter((e) => e[1] === 'pick').map((e) => e[2]);
      assert.ok(picks.length && picks.every((id) => ['Deep', 'Night'].includes(id)), `the world load picks from the rider's playlist: ${picks}`);
      assert.ok(ga.timeline().some((e) => e[1] === 'rider-music'));
      ga.leaveWorld();
    }
    ga.riderMusic = () => null; ga.timelineReset?.();
    await ga.worldLoaded({ courseIndex: 0, singleEvent: true, courseCode: 'ARA1', character: 'zoe' });
    assert.ok(!ga.timeline().some((e) => e[1] === 'rider-music'), 'without a record the settings stay'); assert.equal(ga.getSettings().radioMode, 1);
    ga.leaveWorld();
  }
  console.log('riderMusic: per-rider radio mode and playlist, applied at the world load (2867E8)');
}
// ---- 5. lodgeCheats ---------------------------------------------------------------------------------------------------
{
  const c = new Career(data, { storage: new Memory(), shop, save: undefined });
  const riders = [{ id: 'zoe', name: 'Zoe', kind: 'rider', character: 4 }, { id: 'brodi', name: 'Brodi', kind: 'cheat', character: 10, base: 'zoe' }, { id: 'jp', name: 'JP', kind: 'cheat', character: 12 }];
  const log = [];
  const ui = { screen: 'ctm-details', index: 0, riders, riderIndex: 0, careerMode: true, careerUI: { career: c }, feScreens: { kbLui: {}, owns: () => true, keyboard: null },
    get rider() { return this.riders[this.riderIndex]; }, set(s) { this.screen = s; }, sync() {}, cb: { rider: async (r) => { log.push(r.id + ':' + (r.base || '') + ':' + !!r.career); } } };
  const cs = new CharacterSelect(ui); cs.data = { screens: {} }; ui.characterSelect = cs;
  Object.defineProperty(cs, 'ready', { get: () => true });
  const lodge = new LodgeScreens({ ui, career: c, riderId: 'zoe', t: (k, f) => f, lodgePeak: 1 });
  assert.equal(lodge.disabled('ctm-details', 2), true, 'no cheat character owned: greyed (PS2 lodge/runs/l1)');
  c.rider('zoe').cash = 200000; assert.ok(c.buyReward('zoe', 'cheat_character', 0, 1), 'Brodi, $20,000 in the Peak 1 lodge (l11)');
  assert.equal(c.rider('zoe').cash, 180000);
  assert.equal(lodge.disabled('ctm-details', 2), false, 'owned: live (l12)');
  lodge.choose(2); assert.ok(cs.cheatOpen); assert.ok(cs.overlay('ctm-details'));
  assert.deepEqual(cs.cheatList().map((r) => r.id), ['zoe', 'brodi'], 'the base rider and the owned skins');
  lodge.key({ code: 'ArrowRight', preventDefault() {} }); lodge.key({ code: 'Enter', preventDefault() {} });
  await new Promise((r) => setTimeout(r, 0));
  assert.deepEqual(log, ['brodi:zoe:true'], 'the pick is the career rider: the skin on the base rider, career scale');
  assert.equal(cs.cheat?.id, 'brodi'); assert.equal(cs.cheatOpen, false);
  console.log('lodgeCheats: Cheat Characters live once owned, the 131cheat_char list over the lodge, the pick = the career rider');
}
// ---- 6. the Transport freeride lists: the table 0x478D38 (peak * 0x360 + row * 0x6C: +0 course, +0xC station) in the order the
// PS2 lists them (menus/ctm/53-freeride-list, peak2-arr, peak3/nav/out-fr-to-*/list: The Throne, Black Station, Gravitude,
// Much-2-Much, Perpendiculous, Kick Doubt); Peak 3's rows 6 and 7 are course 23 (none), not listed ---------------------------
{
  const cs = Object.create(CareerScreens.prototype);
  Object.assign(cs, { data, career: new Career(data, { storage: new Memory() }), goal: 'freeride', ui: { cb: { freeRide: () => {} }, rider: { id: 'zoe' } } });
  const names = (peak) => { cs.peak = peak; return cs.list().map((x) => x.name); };
  assert.deepEqual(names(1), ['Happiness', 'Green Station', 'R&B', 'Snow Jam', "Crow's Nest", 'Blue Station', 'Metro-City', 'The Junction']);
  assert.deepEqual(names(2).length, 8);
  assert.deepEqual(cs.list().map((x) => x.course), [19, 2, 9, 12, 15, 20, 6, 3]);
  assert.deepEqual(names(3), ['The Throne', 'Black Station', 'Gravitude', 'Much-2-Much', 'Perpendiculous', 'Kick Doubt']);
  console.log('freeride lists: the three peaks in the order of 0x478D38 and the PS2 frames');
}
// ---- 7. lodgeRewards ------------------------------------------------------------------------------------------------------
{
  const c = new Career(data, { storage: new Memory(), shop }); c.rider('zoe').cash = 200000;
  const fe = { lodgeBack: () => {}, baseId: 'zoe', prompt: null, now: () => 0, data: { strings: {} } };
  const ui = { careerUI: { career: c, lodgePeak: 1, t: (k, f) => f }, sync() {} };
  const ex = Object.create(FeExtraScreens.prototype); Object.assign(ex, { fe, ui, category: 4, item: 0 });
  assert.equal(ex.lodge(), true);
  assert.deepEqual([0, 1, 2, 3, 4, 7].map((i) => ex.status(i).state), ['buy', 'elsewhere', 'buy', 'elsewhere', 'buy', 'locked'], 'Peak 1 lodge: Brodi / JP / Marisol for sale (PS2 l11 $ slots)');
  const slot = ex.galleryOverride({ name: '06f92620' }); assert.equal(slot.sprite, undefined, "the slot's own '$'"); assert.equal(slot.alpha, 255);
  assert.ok(ex.galleryOverride({ name: '06f92621' }).sprite, "Eddie (Peak 2): '?'");
  ex.choose('fe-gallery', 0);
  assert.equal(fe.prompt.buy.name, 'Brodi'); assert.deepEqual(fe.prompt.buy.rows.map((r) => r[1]), ['$20,000', '$ 200,000']); assert.equal(fe.prompt.index, 0, 'Yes focused');
  fe.prompt.yes(); assert.equal(c.rider('zoe').cash, 180000); assert.deepEqual(c.owned('zoe', 'cheat_character'), [0]);
  assert.equal(ex.status(0).state, 'owned');
  fe.lodgeBack = null; assert.equal(ex.lodge(), false); assert.equal(ex.status(2).state, 'elsewhere', 'the front end sells nothing (peak 0: every priced item is elsewhere)');
  console.log('lodgeRewards: the rewards room sells at the lodge peak, the $ slots, the buy popup');
}
// ---- 8. rivalCard: a rival challenge's objectives card over the ready state (PS2 {happiness-mac,ruthless,the-throne}-ready:
// WS2 at race tick 0, the world and the riders behind the card); main.js readyView is asked for the rival modes only -------------
{
  const calls = [];
  const cs = Object.create(CareerScreens.prototype);
  Object.assign(cs, { data, career: new Career(data, { storage: new Memory() }), ui: { rider: { id: 'zoe' }, set(s) { calls.push(s); }, loadEvent(done) { done(); }, cb: { readyView: () => calls.push('ready') } } });
  cs.begin(4, 16, false); assert.deepEqual(calls, ['ctm-objectives', 'ready'], 'The Throne (Rival Time)');
  calls.length = 0; cs.begin(5, 15, false); assert.deepEqual(calls, ['ctm-objectives', 'ready'], 'Ruthless Jam (Rival Points)');
  calls.length = 0; cs.begin(0, 0, false); assert.deepEqual(calls, ['ctm-objectives'], 'Snow Jam: the start-gate idle NIS lies under its card');
  console.log('rivalCard: the ready view under the rival challenge cards');
}
console.log('career rider: ok');
process.exit(0);
