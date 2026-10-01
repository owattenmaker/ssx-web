// Computer-rider lineup of a race, the original way (docs/characters.md "Computer-rider lineups").
//
// Original (SLUS_207.72): the race rules object's event start 0x23A108 (round manager) builds the roster with
// 0x23A4F0 and copies it to the race slots with 0x23A668 (-> 0x2342B8 setup slots, 0x535B20 race copy):
//   0x23A4F0: two distinct "Tricky" cheat riders 10 + r1 % 7 and 10 + r2 % 6 (+1 when >= the first), then the base
//             riders 0..9 except the human's character and the peak rival (0x145750), 10 entries, shuffled by
//             0x23C770 (25 swaps of entries r % 10, r % 10). Heat 1 = entries 0..4 (slots 1..5); a Single Event
//             (0x535C11 != 0) races heat 1. Career rounds: semi = [the qualifier's top three but the human, entries
//             5..7], final = [rival, the semi's top three but the human, entries 8..9] (0x23A254 / 0x23A360).
//   0x23A668: an entry v < 10 races as base rider v; v >= 10 is cheat skin v on the HUMAN's base character.
//   Random draws: 0x237CD8 = 0x317A08 on the roster generator 0x4C9548 (52 draws per build). It is seeded (0x317958)
//   at the event load by the game construction (0x22EFE8) with one draw of the presentation generator 0x4FF018
//   (0x3177F0), which the boot seeds (0x31AE94) from the RTC: sceCdReadClock words w0 ^ (w1 << 3). Every reference
//   session (boot seed 0x182200) takes the roster seed from presentation draw 130 = 0xB57109A9, the Snow Jam /
//   Metro-City anchor lineup for a Zoe human.
// Browser policy (nextRosterSeed): a per-page presentation generator seeded like the boot from the local clock; the
// first event chosen from the menus takes its draw 130 as the roster seed, each later event the draw 129 further on
// (the original's draws between two loads are per-frame presentation effects the browser does not run). ?presentationSeed=0x182200 replays the reference
// sessions; ?lineupSeed=0x... fixes the roster seed. A restart keeps the lineup (0x238C80 resets it only when an
// event is chosen).
import { pv } from './pv-flags.js';

export const SEED_ADD = [0xF22D0E56, 0x96041893, 0x3DF3B646, 0x40DDE76D, 0x97327AE1, 0xD1A9FBE7];
export const REFERENCE_PRESENTATION_SEED = 0x182200;   // the reference ARMSX2 sessions' boot seed
export const REFERENCE_ROSTER_SEED = 0xB57109A9;       // their draw 130
export const SEED_DRAW = 130;                         // presentation draw that seeds the roster (boot -> first race)

// 0x317958: seed a six-word generator.
export function seededWords(seed) { const w = []; let a = seed >>> 0; for (const k of SEED_ADD) { a = (a + k) >>> 0; w.push(a); } return w; }
// 0x317A08: one draw (the same generator as the shared game RNG, web/ai-racers.js rngNext).
export function nextWord(w) {
  let value = (w[5] + w[4]) >>> 0; let carry = (value < w[5] || value < w[4]) ? 1 : 0; w[4] = value;
  for (let k = 3; k >= 1; --k) { const nv = (value + w[k] + carry) >>> 0; carry = nv < w[k] ? 1 : 0; value = nv; w[k] = value; }
  value = (value + w[0] + carry) >>> 0; w[0] = value; w[5] = (w[5] + 1) >>> 0;
  if (w[5] === 0) { let k = 4; while (k >= 1) { w[k] = (w[k] + 1) >>> 0; if (w[k] !== 0) break; --k; } if (k === 0) { value = (value + 1) >>> 0; w[0] = value; } }
  return value;
}
// 0x31AE94: the boot seed from sceCdReadClock (stat, second, minute, hour | pad, day, month, year; BCD bytes).
export function clockSeed(date = new Date()) {
  const bcd = (v) => ((Math.floor(v / 10) % 10) << 4) | (v % 10);
  const w0 = (bcd(date.getSeconds()) << 8) | (bcd(date.getMinutes()) << 16) | (bcd(date.getHours()) << 24);
  const w1 = (bcd(date.getDate()) << 8) | (bcd(date.getMonth() + 1) << 16) | (bcd(date.getFullYear() % 100) << 24);
  return ((w0 ^ (w1 << 3)) >>> 0);
}

// 0x145750: the peak rival (course table 0x43D950 + event * 100 + 0x54 = peak index).
export function peakRival(peak, human) {
  if (peak === 0) return human === 3 ? 5 : 3;
  if (peak === 1) return human === 7 ? 4 : 7;
  if (peak === 2) return human === 8 ? 6 : 8;
  return 3;
}
// 0x23A4F0: the ten roster entries (cheat ids 10..16 or base riders 0..9).
export function buildRoster(seed, human, peak) { return buildRosterFrom(seededWords(seed), human, peak); }
// The same build on a running roster generator (Conquer the Mountain: seeded once at the world load, every fresh event
// draws on; the words are advanced in place).
export function buildRosterFrom(w, human, peak) {
  const first = 10 + (nextWord(w) % 7);
  const r = nextWord(w) % 6;
  const entries = [first, 10 + r < first ? 10 + r : 11 + r];
  const rival = peakRival(peak, human);
  for (let v = 0; v < 10 && entries.length < 10; v++) if (v !== human && v !== rival) entries.push(v);
  while (entries.length < 10) entries.push(0);
  for (let k = 0; k < 25; k++) { const i = nextWord(w) % 10, j = nextWord(w) % 10; const t = entries[i]; entries[i] = entries[j]; entries[j] = t; }
  return entries;
}
// The five computer riders' entries of a round (0x23A108): single event / career qualifier = entries 0..4; career
// semi = the previous round's top three (roster values, human skipped) then entries 5..7; career final = the rival,
// the semi's top three (human skipped) then entries 8..9. `previous`: roster values of the previous round's riders
// in finishing order, human as null.
export function roundEntries(entries, { round = 3, career = false, rival = 3, previous = [] } = {}) {
  if (!career || round === 1) return entries.slice(0, 5);
  const advanced = previous.slice(0, 3).filter((v) => v != null);   // the loop reads three places, skips the human's
  // semi: +0x2C.. = [x, x, entries 5..7], the advanced riders written from +0x2C; final: +0x44.. = [rival, x, x,
  // entries 8, 9], the advanced riders written from +0x48 (x: only written by the advance).
  const out = round === 2 ? [null, null, entries[5], entries[6], entries[7]] : [rival, null, null, entries[8], entries[9]];
  const from = round === 2 ? 0 : 1;
  advanced.forEach((v, i) => { out[from + i] = v; });
  return out.map((v, i) => (v == null ? entries[(round === 2 ? 5 : 8) + i] ?? entries[i] : v));   // a stale word in the original
}
// ---- freestyle events (handler init 0x238E20, docs/slopestyle-bigair.md "Opponent and posted riders") --------------
// 0x239938: the eight characters that are neither the human (GMM+0x18) nor the peak rival (0x145750), in id order,
// shuffled by 0x23C770 (25 swaps of entries r % 8, r % 8) on the roster generator -> GMM+0x20..; +0x1C = the rival, or in
// a Single Event (0x535C11 != 0) the LAST shuffled character. Only base riders 0..9: no cheat skin rides or is posted.
// The posting 0x239AA0 -> 0x1453D0 then draws one word per posted score from the same generator (web/career.js).
// Draw counts: 50 + 4 (Quick Play slope style) / 5 (pipe, big air) / 15 (career: three rounds, 5 riders).
export function freestyleRoster(w, human, rival) {
  const e = [];
  for (let v = 0; v < 10; v++) if (v !== human && v !== rival) e.push(v);
  for (let k = 0; k < 25; k++) { const i = nextWord(w) % e.length, j = nextWord(w) % e.length; const t = e[i]; e[i] = e[j]; e[j] = t; }
  return e;
}
// 0x238E20: who rides and who is posted. Single Event slope style (GMM+0x14 = 4): slot 1 = the last shuffled character
// RIDES (the computer opponent), slots 2..5 post entries 0..3. Career (the round-1 path sets GMM+0x14 = 5 for every kind)
// and pipe / big air: nobody rides, slots 1..5 post [+0x1C, entries 0..3] (career: +0x1C = the rival).
export function freestyleSlots(entries, { rival, single, slope }) {
  const lead = single ? entries[entries.length - 1] : rival;
  return single && slope ? { opponent: lead, posted: entries.slice(0, 4) } : { opponent: null, posted: [lead, ...entries.slice(0, 4)] };
}
// 0x23A668: slot values -> {base, cheat} (a cheat rides on the human's base character).
export function slotRiders(values, humanBase) { return values.map((v) => (v < 10 ? { base: v, cheat: 0 } : { base: humanBase, cheat: v })); }

// ---- browser policy ------------------------------------------------------------------------------------------
const params = () => (typeof location !== 'undefined' ? new URL(location.href).searchParams : new URLSearchParams());
const parseSeed = (text) => { if (text == null || text === '') return null; const v = Number(text.startsWith?.('0x') ? parseInt(text, 16) : text); return Number.isFinite(v) ? v >>> 0 : null; };
// Presentation draws: the boot seeds 0x4FF018 and draws once before the menus; the menus (title, main menu, select, setup,
// peak, mode, event, rules, the pause menu and the quit dialog) draw nothing; each event load makes 128 draws, then the
// roster seed (the 129th); after it the load, overlay, countdown and race effects draw per frame. The per-frame part
// is not reproducible (the consumers -- snow, sprays, crowd, flags, UI, audio of all six riders -- are not all ported):
// noteEventDraws(ticks) adds the measured average (lineups.json presentation_model) for the race ticks played.
let presentation = null, pendingDraws = 0;
const bootPresentation = () => { if (!presentation) { const boot = parseSeed(params().get('presentationSeed')) ?? clockSeed(); presentation = seededWords(boot); nextWord(presentation); } };
export function noteEventDraws(model, ticks) {
  if (model && !parseSeed(params().get('lineupSeed')))
    pendingDraws += Math.max(0, Math.round(model.after_seed + model.per_tick * Math.max(0, ticks - model.anchor_tick)));
}
// The event just raced on this page (web/ai-race.js start): source() = {model, ticks}, noted at the next roster seed
// (a race's own prepare, or web/career.js startEvent for a freestyle event, whichever loads next).
let pendingRace = null;
export function noteRaced(source) { pendingRace = source; }
// A course change in the page (main.js unloadCourse): the raced event's draws are final; note them now so the source (its
// computer riders, their core) is not kept alive until the next roster seed.
export function settleRaced() { if (pendingRace) { const r = pendingRace(); pendingRace = null; if (r) noteEventDraws(r.model, r.ticks); } }
// One draw of the session's presentation generator (0x3177F0), for the other presentation consumers the browser runs
// (career messages 0x1E2A08 / 0x1E2FE0 / 0x1E3100).
export function presentationDraw() { bootPresentation(); return nextWord(presentation); }
// The next event's roster seed.
export function nextRosterSeed() {
  const fixed = parseSeed(params().get('lineupSeed'));
  if (fixed != null) return fixed;
  bootPresentation();
  if (pendingRace) { const r = pendingRace(); pendingRace = null; if (r) noteEventDraws(r.model, r.ticks); }
  for (; pendingDraws > 0; pendingDraws--) nextWord(presentation);   // the previous event's per-frame draws (estimate)
  let seed = 0;
  for (let n = 1; n < SEED_DRAW; n++) seed = nextWord(presentation);   // the load's 128 draws, then the roster seed
  return seed;
}

// ---- game RNG at the countdown anchor (0x4FF030) --------------------------------------------------------------------
// Seeded 0 at the event load; the load draws a number that depends only on the course and the human (lineups.json
// load_draws: base, per base character, +4 for a cheat skin), then each computer rider's first countdown start
// command draws two (0x10AED8 start branch: DF0 amount and sign). Every savestate and capture checks it.
export function loadDraws(data, human) { const d = data.load_draws; return d.base + (d.character[String(human.base)] ?? 0) + (human.cheat ? d.cheat : 0); }
export function anchorRandomWords(data, human) { const w = seededWords(0), n = loadDraws(data, human) + data.load_draws.start; for (let k = 0; k < n; k++) nextWord(w); return w; }
// A backcountry rival challenge (docs/backcountry.md "Rival computer rider"): no countdown, so the game RNG of the ready
// state is the anchor (race tick 0; the rival's provider draws from it on that tick). rivals.json keeps each document's
// ready-state words (doc.anchor_rng: seeded 0, then 4 or 5 draws that depend on the human and the menu path). The
// document's own human starts from them (the PS2 captures' start); any other human from a player's direct menu path:
// the state's count less its menu-path extra (doc.anchor_rng.menu_extra), shifted by the lineups' per-character rule
// (rivals.load_draws: Moby +1, Zoe -1, a cheat skin +4; Ruthless PS2 states: Zoe 3, Nate 4, Moby 5). null: no evidence.
export function rivalAnchorDraws(rivals, doc, human, roster = []) {
  const a = doc?.anchor_rng, rule = rivals?.load_draws;
  if (!a || a.seed !== 0 || !Number.isInteger(a.draws)) return null;
  const own = roster.find((e) => e.id === doc.human), ownBase = own?.character ?? CHARACTER_OF[doc.human];
  if (!rule || !human || ownBase == null || (human.base === ownBase && !human.cheat)) return a.draws;
  const shift = (base, cheat) => (rule.character[String(base)] ?? 0) + (cheat ? rule.cheat : 0);
  return a.draws - (a.menu_extra ?? 0) + shift(human.base, human.cheat) - shift(ownBase, 0);
}
export function rivalAnchorWords(rivals, doc, human, roster = []) {
  const n = rivalAnchorDraws(rivals, doc, human, roster); if (n == null) return null;
  const w = seededWords(0); for (let k = 0; k < n; k++) nextWord(w); return w;
}
const CHARACTER_OF = { moby: 0, kaori: 1, allegra: 2, mac: 3, zoe: 4, griff: 5, elise: 6, nate: 7, psymon: 8, viggo: 9 };

// ---- relationships (0x4A6CA8 + bank*0x9B50 + char*0xF88 + other*3 + 0xBC1: kind, level, score) ------------------------
const rec = (hex) => [0, 2, 4].map((i) => { const b = parseInt(hex.slice(i, i + 2), 16); return b > 127 ? b - 256 : b; });
const hex = (r) => r.map((v) => (v & 255).toString(16).padStart(2, '0')).join('');
function rules(kind, level, score, ageing) {
  if (kind === 1) { if (level >= 4) return [3, 2, 10]; }
  else if (kind === 0) { if (level >= 3) return [0, 2, 10]; }
  else if (kind === 2) { if (level <= 0) return ageing ? [2, 1, score] : [2, 1, 5]; if (level >= 4) return [2, 3, 15]; }
  else if (kind === 3) { if (ageing ? level === 0 : level <= 0) return ageing ? [1, 3, 15] : [3, 1, 5]; if (level >= 5) return [3, 4, 20]; }
  return [kind, level, score];
}
// 0x155E58: every participant's records age at each event load (also a restart): score - 3 (not below 0), level = score / 5.
export function relationshipAge(hexRecord) { const [kind, , score] = rec(hexRecord), s = Math.max(score - 3, 0); return hex(rules(kind, Math.trunc(s / 5), s, true)); }
// 0x155BF0: event 0 soft bump, 1 crash, 2 soft attack, 3 crash attack (+1/+2/+4/+6).
export function relationshipEvent(hexRecord, event) { const [kind, , score] = rec(hexRecord), s = score + ([1, 2, 4, 6][event] ?? 0); return hex(rules(kind, Math.trunc(s / 5), s, false)); }
// notice(other character, aged score) runs for every record in participant order, as 0x155E58 calls 0x1E2A08 (the
// Conquer-the-Mountain relationship messages, web/career-messages.js).
export function ageRelationships(table, characters, banks, notice = null) {
  const out = clone(table);
  characters.forEach((c, k) => {
    out[banks[k]][c] = out[banks[k]][c].map((r, other) => { if (notice) notice(other, Math.max(rec(r)[2] - 3, 0)); return relationshipAge(r); });
  });
  return out;
}
// relationship(own, peer) = the peer's record about own: level (0x155B50; 3 for the peak rival) and kind (0x155AB0;
// 2 for the peak rival, used by the pass/hit speech 0x2A1820).
export function relationshipScores(table, characters, banks, rival) {
  return characters.map((own) => characters.map((peer, p) => (own === rival ? 3 : rec(table[banks[p]][peer][own])[1])));
}
export function relationshipKinds(table, characters, banks, rival) {
  return characters.map((own) => characters.map((peer, p) => (own === rival ? 2 : rec(table[banks[p]][peer][own])[0])));
}
// 0x155BF0 on the table: `other`'s record about `target` (the rider knocked); event 0 soft, 1 crash, 2 soft attack, 3 crash attack.
// Returns [old level, new level, new score] (0x155BF0 notifies 0x1E2A08 when the human's own record rises a level).
export function applyRelationshipEvent(table, characters, banks, target, other, event) {
  const row = table[banks[other]][characters[other]], before = rec(row[characters[target]]);
  row[characters[target]] = relationshipEvent(row[characters[target]], event);
  const after = rec(row[characters[target]]);
  return [before[1], after[1], after[2]];
}

// ---- the human's own grid spot on a course (slot 0; follows the body scale, reverse stance the base rider) --------
export function humanGridState(data, rider, humanBase, { career = false } = {}) {
  if (!data.human_grid) return null;
  const key = career && rider.kind === 'cheat' ? data.human_scale[data.names.base[humanBase]] : data.human_scale[rider.id];
  const grid = data.human_grid[key], base = data.human_base[String(humanBase)];
  if (!grid || !base) return null;
  const state = clone(data.human_template);
  for (const part of [grid, base]) for (const [path, value] of Object.entries(part)) put(state, path, value);
  return state;
}

// Character ids of riders.json entries: base riders 0..9, Sam = Mac's slot 3 (the Sam build), cheats 10..29 on a base.
export function humanCharacter(rider, roster) {
  const entry = roster.find((e) => e.id === rider.id) || rider;
  if (entry.kind === 'cheat') { const base = roster.find((e) => e.id === (rider.base || 'zoe')); return { base: base?.character ?? 4, cheat: entry.character }; }
  return { base: entry.character ?? 3, cheat: 0 };
}
// The lineup for a human: [{base, cheat}] for slots 1..5 plus the facts.
// entries: a roster already built (a Conquer the Mountain event's, web/career.js startEvent: its round-1 build on the
// career's running generator), else the build from `seed`.
export function lineupFor({ seed, entries: built = null, human, peak = 0, round = 3, career = false, previous = [] }) {
  const entries = built ? built.slice() : buildRoster(seed, human.base, peak);
  const values = roundEntries(entries, { round, career, rival: peakRival(peak, human.base), previous });
  return { seed, entries, values, riders: slotRiders(values, human.base), rival: peakRival(peak, human.base) };
}

// ---- the computer riders' records (tools/export_lineups.py -> /assets/<course>/lineups.json) ---------------------
// A record = the anchor record of the slot with its parts replaced: slot (route, AI path), grid (slot x body scale:
// the spot, ground frame, route distances), skin (scale, rig, masks), base (stance, uber table, ids: a cheat rides on
// the human's base), moment and state (the countdown moment and snapshot words: the anchor's; they do not change the
// race, see docs). The document adds the relationships (the tables aged by the load, 0x155E58), the rival,
// the pair inputs by base character and the pair-record rival flags (+0x1C). Distance/bearing of the pair records are
// the anchor's; the manager refresh 0x10F560 recomputes them from the grid at the first tick (game tick 18).
const clone = (v) => (v === undefined ? v : JSON.parse(JSON.stringify(v)));
function put(record, path, value) { const keys = path.split('.'), last = keys.pop(); let o = record; for (const k of keys) o = o[k]; o[last] = clone(value); }
export function f32key(value) { const v = new DataView(new ArrayBuffer(4)); v.setFloat32(0, value); return v.getUint32(0).toString(16).padStart(8, '0'); }
export function skinName(data, value) { return value < 10 ? data.names.base[value] : data.names.cheat[String(value)]; }
// values: the five roster values (0x23A668 input). moment/state: per-slot overrides (verification against a savestate).
// relationships: the tables before this event's load (default: the fresh profile); the load ages them (0x155E58).
// The computer riders' route roles of a race round (cComputer_updateRiderDifficulty 0x10C758 -> 0x10C450(C, slot, GMM+0), the jump table
// 0x456A70; GMM+0 = the round: 1 qualifier, 2 semi, 3 final, and 3 in a Single Event, 0x23A174): owner +0xE00 (route affinity in
// 10D410) = slots 1 / 2: 1, 1, 2; slot 3: 0, 1, 1; slot 4: 0, 0, 1; slot 5: 0; +0xE04 = 1 only for slot 1 in round 3. PS2 countdowns
// ARA1-qual / -semi / -final (local/assets/native/ARA1/lineups-career): 1 1 0 0 0, 1 1 1 0 0, 2 2 1 1 0 (+0xE04 slot 1 of the final).
// lineups.json's slot tables hold round 3's (its anchor is a Single Event). Not covered: the game mode 0x535C12 4 / 5 overrides
// (E00 0 / 2: the backcountry rival events) and the 0x5308D0 bit 2 branch.
export function npcRoundRole(slot, round) {
  if (slot === 1 || slot === 2) return round === 1 || round === 2 ? 1 : 2;
  if (slot === 3) return round === 1 ? 0 : 1;
  if (slot === 4) return round === 1 || round === 2 ? 0 : 1;
  return 0;
}
// The computer riders' difficulty words of a career race (cComputer_updateRiderDifficulty 0x10C758 -> 0x10C4F8(rider, slot, level)): level =
// 147CB8, the human's profile character +0x280 (the race level 0..2, web/career.js level.race); three per-slot jump tables (level 0 0x456AB0,
// level 1 0x456A90, level 2 0x456AD0; gp-0x7DD4..-0x7D24 floats) give +0xDF8 and +0xDFC, then +0xDFC x 0.01 (gp-0x7D20); 0x10C758 scales
// +0xDFC by 1.25 on course 4 and 1.1 (gp-0x7D1C) on courses 2 / 3 and caps it at 1.0. The PS2 FPU rounds toward zero. Checked on all 128
// exported countdowns (Single Event level 1, the career finals CRA3 / DRA4 level 2; local/career-rival/difficulty.py). Not modelled: the
// options word 0x5308D0 bit 2 (DF8 100, DFC 1.0; 0 in every career state). Leaves: npc.crouch_parameter_df8, npc.driving_state.parameter_df8 / dfc.
const SLOT_DIFFICULTY = [
  [[100, 83.89308166503906], [55.80497360229492, 64.6436767578125], [40.250186920166016, 60.00449752807617], [15.542302131652832, 52.00495529174805], [7.954832077026367, 35.020263671875]],
  [[100, 86.91539764404297], [80.38914489746094, 79.59983825683594], [51.00757598876953, 68.90123748779297], [46.601898193359375, 56.843074798583984], [14.000700950622559, 42.441043853759766]],
  [[100, 100], [84.99810028076172, 82.68248748779297], [74.99872589111328, 70.82238006591797], [54.99583435058594, 59.94406509399414], [29.981433868408203, 46.99877166748047]]];
const COURSE_INDEX = { ARA1: 0, BRA2: 1, CRA3: 2, DRA4: 3, ERA5: 4 };
const towardZero = (x) => {
  const y = Math.fround(x);
  if (Math.abs(y) <= Math.abs(x)) return y;
  const v = new DataView(new ArrayBuffer(4));
  v.setFloat32(0, y);
  v.setUint32(0, v.getUint32(0) - 1);
  return v.getFloat32(0);
};
export function npcDifficulty(slot, level, course) {
  const [df8, base] = SLOT_DIFFICULTY[level][slot - 1];
  let dfc = towardZero(Math.fround(base) * Math.fround(0.009999999776482582));
  const k = course === 4 ? 0.25 : course === 2 || course === 3 ? Math.fround(0.10000000149011612) : null;
  if (k != null) dfc = towardZero(dfc + towardZero(dfc * k));
  return { df8: Math.fround(df8), dfc: Math.min(dfc, 1) };
}
// pv careerRival: the Peak 2 courses' career-only rider parts (lineups.json career_skins: Nate, the final's slot-1 rival) are missing
// until the switch is on, as before they were exported. (Peak 1's Mac, ARA1 / BRA2, predates the switch.)
const careerSkinGated = (data, skin) => data.peak === 1 && data.career_skins?.[skin] != null && !pv('careerRival');
// round (optional): the race round whose roles the riders get (npcRoundRole); omitted, the slot tables' (round 3).
// level (optional, a career race under pv careerLevel): the human's race level, the riders' 0x10C4F8 words (npcDifficulty); omitted, the
// slot tables' (level 1).
export function assembleLineup(data, humanBase, values, { moment = data.moment, state = data.state, extra = null, relationships = data.relationships_fresh, round = null, level = null } = {}) {
  const template = data.template;
  const riders = values.map((v, k) => {
    const slot = String(k + 1), base = v < 10 ? v : humanBase, skin = skinName(data, v);
    const record = clone(template.riders[k]);
    // R&B (one slot, no cheat opponent): every rider-dependent leaf is a skin leaf, no slot/grid/base tables
    const grid = data.grid[slot] ? data.grid[slot][data.skin_scale[skin]] : {};
    if (!grid || !data.skin[skin] || careerSkinGated(data, skin) || (Object.keys(data.base).length && !data.base[String(base)]))
      throw new Error(`No computer-rider data for ${skin} (base ${base}) in slot ${slot}`);
    for (const part of [data.slot[slot] ?? {}, grid, data.skin[skin], data.base[String(base)] ?? {}, moment[slot] ?? {}, state[slot] ?? {}])
      for (const [path, value] of Object.entries(part)) put(record, path, value);
    if (round != null && record.npc?.score_state) { record.npc.score_state.role_e00 = npcRoundRole(k + 1, round); record.npc.score_state.allow_flag0_e04 = k === 0 && round === 3; }
    // pv semiFresh: a semi's riders are WS13's new ones, +0x434 = 0x31 until the push-off (11B718; tools/export_lineups.py apply_round_level)
    if (round === 2 && pv('semiFresh') && record.ground?.state && record.identity) { record.ground.state.rider_type = 0x31; record.identity.rider_type434 = 0x31; }
    if (level != null && record.npc?.driving_state && COURSE_INDEX[data.course] != null) {
      const d = npcDifficulty(k + 1, level, COURSE_INDEX[data.course]);
      record.npc.crouch_parameter_df8 = d.df8;
      record.npc.driving_state.parameter_df8 = d.df8;
      record.npc.driving_state.parameter_dfc = d.dfc;
    }
    return record;
  });
  const doc = {};
  for (const key of Object.keys(template)) doc[key] = key === 'riders' ? riders : clone(template[key]);
  const characters = [humanBase, ...values.map((v) => (v < 10 ? v : humanBase))], count = characters.length;
  characters.push(...template.relationships.characters.slice(count));   // R&B: the unused race slots' stale entries (0x5305B0)
  const rel = doc.relationships, banks = rel.target_banks, rival = peakRival(data.peak, humanBase);
  const table = ageRelationships(relationships, characters.slice(0, count), banks);   // 0x155E58: the riders' records only
  rel.scores = relationshipScores(table, characters, banks, rival);
  rel.characters = characters; rel.persistent_human_character = humanBase; rel.rival_character = rival; rel.event_kind = data.peak;
  // Peak 2 (human_pair_inputs): the computer riders' collision / attack stats come from attribute bank 2, the human's (slot 0) from its own
  doc.world.pair_inputs = characters.slice(0, count).map((c, k) => clone(k === 0 && data.human_pair_inputs?.[String(c)] ? data.human_pair_inputs[String(c)] : data.pair_inputs[String(c)]));
  doc.world.records.forEach((row, a) => row.forEach((r, b) => { r.t1c = a < b && r.enabled && rel.scores[a][b] >= 2 ? 1 : 0; }));
  if (extra) Object.assign(doc, clone(extra));
  return doc;
}
