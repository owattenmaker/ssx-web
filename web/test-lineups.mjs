// Computer-rider lineups (web/lineup.js, tools/export_lineups.py, docs/characters.md "Computer-rider lineups"):
//  1. the original roster build 0x23A4F0 + copy 0x23A668, fed each savestate's roster seed, gives the lineup found in
//     every derived countdown (the 30 characters as the human, Brodi on Psymon, and the coverage states of Snow Jam and
//     Metro-City), and the reference sessions' seed is presentation draw 130 of their boot seed;
//  2. the records assembled from lineups.json with a state's own moment words give that state's npc-riders document
//     exactly (local/assets/native/<course>/lineups/*.json, when present), the pair records' distance/bearing coming from
//     the core's 0x10F560 refresh at the first tick; Zoe with the anchor lineup gives npc-riders.json itself;
//  3. every assembled lineup initialises and races 600 ticks (cores set up again in place, as the browser does; one
//     lineup is also checked against freshly created cores, tick by tick).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
import { createNodeRace } from './ai-race-node.mjs';
import { execFile } from 'node:child_process';
import os from 'node:os';
import { ageRelationships, anchorRandomWords, applyRelationshipEvent, assembleLineup, buildRoster, humanCharacter, humanGridState, lineupFor, loadDraws, nextWord,
  peakRival, relationshipScores, seededWords, REFERENCE_ROSTER_SEED, SEED_DRAW } from './lineup.js';

const root = new URL('public/assets/', import.meta.url);
const readJson = (p) => JSON.parse(fs.readFileSync(new URL(p, root)));
const roster = readJson('riders.json');
// Peak 2 races (CRA3 Ruthless Ridge, DRA4 Intimidator: characters/lineups-<course>, scripts/make_peak2_lineup_states.py) take the same checks.
const courses = ['ARA1', 'BRA2', 'CRA3', 'DRA4'].filter((c) => fs.existsSync(new URL(`${c}/lineups.json`, root)));
assert(courses.includes('ARA1'), 'web/public/assets/ARA1/lineups.json (tools/export_lineups.py build)');
const data = Object.fromEntries(courses.map((c) => [c, readJson(`${c}/lineups.json`)]));

// ---- 1. the algorithm ----------------------------------------------------------------------------------------------
assert.deepEqual(buildRoster(REFERENCE_ROSTER_SEED, 4, 0), [8, 2, 0, 5, 13, 7, 9, 6, 1, 14], 'Snow Jam anchor roster');
assert.equal(peakRival(0, 3), 5); assert.equal(peakRival(1, 7), 4); assert.equal(peakRival(2, 2), 8);
// Route roles by round (0x10C758 -> 0x10C450; web/lineup.js npcRoundRole): the derived career countdowns' owner +0xE00 / +0xE04
{ const career = new URL('../local/assets/native/ARA1/lineups-career/', import.meta.url);
  for (const [file, round] of [['ARA1-qual-zoe.json', 1], ['ARA1-semi-zoe.json', 2], ['ARA1-final-zoe.json', 3]]) {
    if (!fs.existsSync(new URL(file, career))) continue;
    const doc = JSON.parse(fs.readFileSync(new URL(file, career))), built = assembleLineup(data.ARA1, 4, [8, 2, 0, 5, 6], { round });
    for (const r of doc.riders) { const b = built.riders.find((x) => x.slot === r.slot);
      assert.equal(b.npc.score_state.role_e00, r.npc.score_state.role_e00, `${file} slot ${r.slot} role +0xE00`);
      assert.equal(b.npc.score_state.allow_flag0_e04, r.npc.score_state.allow_flag0_e04, `${file} slot ${r.slot} +0xE04`); }
    console.log(`lineups: round ${round} route roles = ${file}'s`); } }
let states = 0;
for (const course of courses) {
  for (const o of data[course].observed) {
    const got = lineupFor({ seed: o.seed, human: { base: o.human_base, cheat: 0 }, peak: data[course].peak });
    assert.deepEqual(got.values, o.values, `${course} ${o.state}: lineup`);
    if (o.presentation_seed != null) {   // a reference session: the roster seed is presentation draw 130 since the boot seeding
      const w = seededWords(o.presentation_seed); let seed = 0; for (let n = 0; n < SEED_DRAW; n++) seed = nextWord(w);
      assert.equal(seed, o.seed, `${o.state}: presentation draw ${SEED_DRAW}`); assert.equal(o.seed_draw, SEED_DRAW);
    }
    states++;
  }
}
// The 30 characters as the human: riders.json identity (cheat skins ride on Zoe in their states) and the lineup.
const characterStates = data.ARA1.observed.filter((o) => roster.some((r) => r.id === o.state));
assert.equal(characterStates.length, 30, 'one derived countdown per original character');
for (const o of characterStates) {
  const entry = roster.find((r) => r.id === o.state), human = humanCharacter(entry.kind === 'cheat' ? { ...entry, base: 'zoe' } : entry, roster);
  assert.equal(human.base, o.human_base, `${o.state}: human base character`);
  assert.deepEqual(lineupFor({ seed: REFERENCE_ROSTER_SEED, human, peak: 0 }).values, o.values, `${o.state}: reference lineup`);
}
assert.deepEqual(lineupFor({ seed: REFERENCE_ROSTER_SEED, human: humanCharacter(roster.find((r) => r.id === 'psymon'), roster) }).riders.map((r) => [r.base, r.cheat]),
  [[7, 0], [2, 0], [0, 0], [4, 0], [8, 13]], 'Psymon: Nate, Allegra, Moby, Zoe, Luther (on Psymon)');
assert.equal(humanCharacter(roster.find((r) => r.id === 'sam'), roster).base, 3, 'Sam races in Mac\'s slot');
// Career rounds (0x23A254 / 0x23A360): semi = the qualifier's top three but the human + entries 5..7; final = rival + the semi's.
const entries = buildRoster(REFERENCE_ROSTER_SEED, 4, 0);
assert.deepEqual(lineupFor({ seed: REFERENCE_ROSTER_SEED, human: { base: 4 }, career: true, round: 2, previous: [2, null, 13, 0] }).values, [2, 13, ...entries.slice(5, 8)]);
assert.deepEqual(lineupFor({ seed: REFERENCE_ROSTER_SEED, human: { base: 4 }, career: true, round: 3, previous: [null, 7, 9] }).values, [3, 7, 9, entries[8], entries[9]]);
console.log(`lineups: 0x23A4F0 gives the observed lineup of all ${states} countdown states (${courses.join(', ')})`);
// The game RNG at the anchor: seeded 0 at the load, the load's draws (course, human), 10 start-command draws.
const cheatIds = new Set(roster.filter((r) => r.kind === 'cheat').map((r) => r.id));
for (const course of courses) for (const o of data[course].observed) {
  const human = { base: o.human_base, cheat: cheatIds.has(o.human) ? 1 : 0 };
  const extra = o.state === '__anchor' ? data[course].load_draws.reference_anchor_extra ?? 0 : 0;   // Peak 2's reference anchor path: one draw more (export_lineups.py build)
  assert.equal(o.shared_draws, loadDraws(data[course], human) + 10 + extra, `${course} ${o.state}: game RNG draws at the anchor`);
}
const zoeAnchorDraws = { ARA1: 19, BRA2: 22, CRA3: 21, DRA4: 22 };   // the capture baselines' game RNG draws at the anchor
const anchorWords = (course) => { const w = seededWords(0); for (let n = 0; n < zoeAnchorDraws[course]; n++) nextWord(w); return w; };
for (const course of courses) {
  const w = anchorRandomWords(data[course], { base: 4, cheat: 0 }); for (let n = 0; n < (data[course].load_draws.reference_anchor_extra ?? 0); n++) nextWord(w);
  assert.deepEqual(w, anchorWords(course), `${course}: Zoe's anchor RNG (capture baselines)`);
}
// The no-lineup courses' anchor rule (web/event-anchor-rng.js) with a human other than Zoe: Mac at The Junction (PS2 run
// local/ps2-capture/runs/nonzoe/junction-mac from characters/mac-junction/countdown.p2s: record 0 holds 17 draws from seed 0,
// Zoe's anchors 16), i.e. base 17 with the lineups character rule (Zoe -1, Mac 0).
{ const { eventAnchorWords } = await import('./event-anchor-rng.js');
  assert.equal(eventAnchorWords('BHP1', { base: 4, cheat: 0 }).draws, 16, 'The Junction anchor, Zoe');
  assert.equal(eventAnchorWords('BHP1', { base: 3, cheat: 0 }).draws, 17, 'The Junction anchor, Mac (PS2 nonzoe/junction-mac record 0)');
  const cap = new URL('../local/ps2-capture/runs/nonzoe/junction-mac.bin', import.meta.url);
  if (fs.existsSync(cap)) { const b = fs.readFileSync(cap), got = Array.from({ length: 6 }, (_, k) => b.readUInt32LE(8896 + 4 * k));
    assert.deepEqual(got, eventAnchorWords('BHP1', { base: 3, cheat: 0 }).words, 'The Junction anchor words, Mac (capture record 0)'); } }
// Two events / a restart in one session (characters/two-event): the menus draw nothing, the load draws 128 then the seed;
// every load (and a restart) ages the participants' relationship records (0x155E58); a restart keeps the roster.
const sessionFile = new URL('ARA1/lineup-sessions.json', root);
if (fs.existsSync(sessionFile)) {
  const S = JSON.parse(fs.readFileSync(sessionFile));
  const drawAt = (n) => { const w = seededWords(S.boot_seed); let v = 0; for (let k = 0; k < n; k++) v = nextWord(w); return v; };
  const participants = (st) => ({ characters: st.characters.map((c, k) => (st.cheats[k] ? st.characters[0] : c)), banks: st.banks });
  for (const [name, sess] of Object.entries(S.sessions)) {
    const race1 = sess.race1, next = sess['ready2'] || sess['restart-b'];
    assert.equal(race1.roster_seed, drawAt(SEED_DRAW), `${name}: first roster seed`);
    if (sess.select2) {
      assert.equal(sess.select2.presentation_draws, race1.presentation_draws, `${name}: pause, quit, title and menus draw nothing`);
      assert.equal(sess.ready2.roster_seed, drawAt(sess.select2.presentation_draws + 129), `${name}: second roster seed = 129th draw after the menus`);
      assert.deepEqual(ageRelationships(race1.relationships, ...Object.values(participants(sess.ready2))), sess.ready2.relationships, `${name}: second load ages the records`);
      assert.deepEqual(lineupFor({ seed: sess.ready2.roster_seed, human: { base: 4 } }).values, sess.ready2.characters.slice(1).map((c, k) => sess.ready2.cheats[k + 1] || c), `${name}: second lineup`);
    } else {
      assert.equal(next.roster_seed, race1.roster_seed, `${name}: a restart keeps the roster`);
      assert.deepEqual(ageRelationships(race1.relationships, ...Object.values(participants(next))), next.relationships, `${name}: a restart ages the records again`);
    }
  }
  console.log(`lineups: ${Object.keys(S.sessions).length} two-event/restart sessions: presentation draws, second lineups and relationship ageing exact`);
}

// ---- 2. the records -----------------------------------------------------------------------------------------------
const probe = await createCore();
const worldRecords = (doc, humanPosition) => {   // race_world_reset + the tick-0 refresh (0x10F560) on the grid positions
  const init = [6, doc.world.tail, doc.world.rank_mode];
  for (let a = 0; a < 6; a++) { for (let b = 0; b < 6; b++) { const r = doc.world.records[a][b]; init.push(r.enabled, r.human, 1e10, 0, r.t10, r.t14, r.t18, r.t1c, r.t20); } init.push(doc.world.ranks[a]); }
  for (let a = 0; a < 6; a++) for (let b = 0; b < 6; b++) init.push(doc.relationships.scores[a][b]);
  const p = probe._malloc(init.length * 4); probe.HEAPF32.set(init, p >> 2); probe._race_world_reset(p); probe._free(p);
  const riders = [humanPosition, ...doc.riders.map((r) => r.race.position)].flatMap((pos) => [...pos, 1e5, 0, 1]);
  const q = probe._malloc(36 * 4); probe.HEAPF32.set(riders, q >> 2); probe._race_world_frame(0, q); probe._free(q);
  const s = new Float32Array(probe.HEAPF32.buffer, probe._race_world_state(), 396);
  return doc.world.records.map((row, a) => row.map((r, b) => [s[(a * 6 + b) * 10 + 2], s[(a * 6 + b) * 10 + 3], s[(a * 6 + b) * 10 + 7]]));
};
const strip = (doc) => { const d = JSON.parse(JSON.stringify(doc)); delete d.lineup_state; delete d.provenance; for (const row of d.world.records) for (const r of row) { delete r.distance; delete r.bearing; } return d; };
const leaves = (x, pre = '', out = {}) => { if (x && typeof x === 'object' && !Array.isArray(x)) { for (const [k, v] of Object.entries(x)) leaves(v, pre ? `${pre}.${k}` : k, out); } else out[pre] = x; return out; };
let checked = 0;
for (const course of courses) {
  const d = data[course];
  const anchor = assembleLineup(d, d.anchor.human_base, d.anchor.values);
  assert.equal(JSON.stringify(anchor), JSON.stringify(readJson(`${course}/npc-riders.json`)), `${course}: Zoe with the anchor lineup = npc-riders.json`);
  const folder = new URL(`../../../local/assets/native/${course}/lineups/`, root);
  if (!fs.existsSync(folder)) { console.log(`lineups: ${course} per-state documents not present (local/assets/native/${course}/lineups), assembly check skipped`); continue; }
  for (const o of d.observed) {
    const file = new URL(`${o.state}.json`, folder); if (!fs.existsSync(file)) continue;
    const doc = JSON.parse(fs.readFileSync(file));
    const own = (key) => Object.fromEntries(doc.riders.map((r) => { const l = leaves(r); return [String(r.slot), Object.fromEntries(d.paths[key].map((p) => [p, l[p]]))]; }));
    const got = assembleLineup(d, o.human_base, o.values, { moment: own('moment'), state: own('state'), extra: { anchor_tick: doc.anchor_tick, human_identity: doc.human_identity } });
    assert.deepEqual(strip(got), strip(doc), `${course} ${o.state}: assembled document`);
    const refreshed = worldRecords(got, o.human_position);
    doc.world.records.forEach((row, a) => row.forEach((r, b) => { if (a !== b) assert.deepEqual(refreshed[a][b], [Math.fround(r.distance), Math.fround(r.bearing), r.t1c], `${course} ${o.state}: pair record ${a}->${b}`); }));
    checked++;
  }
}
console.log(`lineups: ${checked} observed documents assembled exactly from slot/grid/skin/base parts (pair records by the 0x10F560 refresh)`);

// ---- 3. in-race relationship events (0x155BF0) against the PS2 --------------------------------------------------------
if (fs.existsSync(sessionFile)) {
  const S = JSON.parse(fs.readFileSync(sessionFile));
  for (const name of ['zoe-quit-300', 'zoe-quit-900']) {
    const sess = S.sessions[name]; if (!sess) continue;
    const race = await createNodeRace({}), d = data.ARA1, rel = race.racers.document.relationships;
    let tables = ageRelationships(d.relationships_fresh, rel.characters, rel.target_banks);
    race.racers.setAnchorRng(anchorRandomWords(d, { base: 4, cheat: 0 }));
    let events = 0;
    race.racers.onReact = (target, other, kind, attack) => { events++; applyRelationshipEvent(tables, rel.characters, rel.target_banks, target, other, (kind === 2 ? 1 : 0) + (attack ? 2 : 0));
      race.racers.setRelationships(relationshipScores(tables, rel.characters, rel.target_banks, rel.rival_character)); };
    race.start();
    const pad = new Float32Array(24);
    for (let t = 0; t < sess.race1.tick; t++) race.tick(pad);
    assert.deepEqual(tables, sess.race1.relationships, `${name}: relationship records after ${sess.race1.tick} ticks (0x155BF0)`);
    console.log(`lineups: ${name}: ${events} rider-pair relationship events, records equal the PS2 at tick ${sess.race1.tick}`);
  }
}

// ---- 4. held-out lineups raced against PS2 captures (tools/ps2_capture.py --ai-state from the derived countdowns) -----
const captures = [
  ['psymon-idle', 'ARA1', 'psymon', null, true], ['moby-idle', 'ARA1', 'moby', null, true], ['stretch-idle', 'ARA1', 'stretch', null, true],
  ['cov-psymon-78e51061', 'ARA1', 'psymon-78e51061', null, true], ['cov-kaori-684c4645', 'ARA1', 'kaori-684c4645', null, true], ['zoe-pairs-idle', 'ARA1', 'zoe', null, false],
  ['bra2-viggo-1818e811', 'BRA2', 'viggo-1818e811', null, true], ['bra2-allegra-4b8545e9', 'BRA2', 'allegra-4b8545e9', null, true], ['bra2-brodi-on-psymon', 'BRA2', 'brodi-on-psymon', 'psymon', true]];
const runs = new URL('../../../local/ps2-capture/runs/lineups/', root);
const present = captures.filter(([cap]) => fs.existsSync(new URL(`${cap}.bin`, runs)));
const compare = ([cap, course, state, base, isolate]) => new Promise((resolve, reject) => {
  const o = data[course].observed.find((x) => x.state === state), humanId = roster.find((r) => r.id === o.human) ? o.human : state.split('-')[0];
  const doc = assembleLineup(data[course], o.human_base, o.values, { extra: { anchor_tick: o.tick } }), tmp = os.tmpdir();
  const docFile = `${tmp}/lineup-${cap}.json`, report = `${tmp}/lineup-${cap}.report.json`; fs.writeFileSync(docFile, JSON.stringify(doc));
  const args = ['compare-ai-capture.mjs', new URL(`${cap}.bin`, runs).pathname, '--world-draws', '--human', humanId, '--document', docFile, '--report', report, ...(isolate ? ['--isolate'] : []), ...(base ? ['--base', base] : [])];
  execFile(process.execPath, args, { cwd: new URL('.', import.meta.url).pathname, maxBuffer: 1 << 28 }, (err) => { if (err) return reject(err); const r = JSON.parse(fs.readFileSync(report)).summary; fs.rmSync(docFile); fs.rmSync(report); resolve({ cap, course, r }); });
});
const results = [];
for (let k = 0; k < present.length; k += 3) results.push(...await Promise.all(present.slice(k, k + 3).map(compare)));
for (const { cap, course, r } of results) {
  assert(r.ticks >= 899 && !r.firstRngMismatch && !r.firstRankMismatch && !r.firstPairRecordMismatch && r.ai.every((a) => !a.firstInexact), `${cap}: computer riders/RNG/ranks/pairs exact vs the PS2 ${JSON.stringify(r).slice(0, 400)}`);
  // Metro-City: a human idle on the grid leaves it ~750 ticks in (velocity only; the course's own gap, also with Zoe)
  assert(!r.firstHumanInexact || (course === 'BRA2' && r.firstHumanInexact.tick >= 740), `${cap}: human ${JSON.stringify(r.firstHumanInexact)}`);
}
if (present.length) console.log(`lineups: ${present.length} held-out lineups (non-reference seeds, other humans, cheat skins as computer riders, Metro-City) race 900 ticks exactly like the PS2 captures`);

// ---- 5. racing ----------------------------------------------------------------------------------------------------
const pad = new Float32Array(24);
const pos = (c) => Array.from(new Float32Array(c.HEAPF32.buffer, c._rider_world_state(), 16)).slice(0, 3);
async function raceCourse(course, lineups) {
  const race = await createNodeRace({ course });
  const seen = new Set(); let raced = 0;
  for (const { label, humanBase, values } of lineups) {
    const doc = assembleLineup(data[course], humanBase, values);
    const key = JSON.stringify(doc); if (seen.has(key)) continue; seen.add(key);
    for (const r of doc.riders) race.resources.riderText[r.package] ??= fs.readFileSync(new URL(`${r.package}/rider.json`, root), 'utf8');
    race.racers.setDocument(doc);
    assert.deepEqual(race.racers.npcs.map((n) => n.character), doc.riders.map((r) => r.character), `${label}: riders set up`);
    race.start();
    const start = race.racers.npcs.map((n) => pos(n.core));
    for (let t = 0; t < 600; t++) race.tick(pad);
    race.racers.npcs.forEach((n, k) => {
      const p = pos(n.core), moved = Math.hypot(p[0] - start[k][0], p[1] - start[k][1], p[2] - start[k][2]);
      assert(p.every(Number.isFinite) && moved > 1000, `${course} ${label}: ${n.character} moved ${moved.toFixed(0)} cm in 600 ticks`);
    });
    raced++;
  }
  return { race, raced };
}
// Snow Jam: every character's own lineup (reference seed), Sam, a cheat on Psymon, and the coverage lineups.
const humans = [...roster.map((r) => ({ label: r.id, human: humanCharacter(r.kind === 'cheat' ? { ...r, base: 'zoe' } : r, roster) })),
  { label: 'brodi+psymon', human: humanCharacter({ ...roster.find((r) => r.id === 'brodi'), base: 'psymon' }, roster) }];
const araLineups = [...humans.map(({ label, human }) => ({ label, humanBase: human.base, values: lineupFor({ seed: REFERENCE_ROSTER_SEED, human }).values })),
  ...data.ARA1.observed.map((o) => ({ label: o.state, humanBase: o.human_base, values: o.values }))];
const t0 = performance.now();
const { race: ara, raced } = await raceCourse('ARA1', araLineups);
// Cores set up again in place race exactly like freshly created ones (Psymon's lineup, 600 ticks, every rider every tick).
const psymon = araLineups.find((l) => l.label === 'psymon'), doc = assembleLineup(data.ARA1, psymon.humanBase, psymon.values);
const words = readJson('ANIMATIONS/initial.json').original_animation.random_state;
ara.racers.setDocument(doc); ara.start(); ara.racers.setSharedRng(words);
const fresh = await createNodeRace({ document: doc });
fresh.start(); fresh.racers.setSharedRng(words);
for (let t = 0; t < 600; t++) {
  ara.tick(pad); fresh.tick(pad);
  ara.racers.npcs.forEach((n, k) => assert.deepEqual(pos(n.core), pos(fresh.racers.npcs[k].core), `set-up-again core ${n.character} differs from a fresh core at tick ${t}`));
}
// A restart (start() again on the same cores, as the pause menu's Restart) races exactly like the first start.
{
  const words = anchorRandomWords(data.ARA1, { base: 8, cheat: 0 }); ara.racers.setAnchorRng(words);
  const run = () => { ara.start(); const out = []; for (let t = 0; t < 400; t++) { ara.tick(pad); out.push(ara.racers.npcs.map((n) => pos(n.core)).flat()); } return out; };
  const first = run(), again = run();
  first.forEach((row, t) => assert.deepEqual(again[t], row, `restart differs at tick ${t}`));
  ara.racers.setAnchorRng(null);
}
let bra = 0, peak2 = 0;
if (data.BRA2) bra = (await raceCourse('BRA2', [{ label: 'anchor', humanBase: 4, values: data.BRA2.anchor.values }, { label: 'psymon', humanBase: 8, values: lineupFor({ seed: REFERENCE_ROSTER_SEED, human: { base: 8 } }).values },
  ...data.BRA2.observed.map((o) => ({ label: o.state, humanBase: o.human_base, values: o.values }))])).raced;
// Peak 2: the anchor, the peak-rival case (Nate as the human: Zoe is the rival) and two cheat humans (every observed lineup: scratchpad verify, docs).
for (const course of ['CRA3', 'DRA4']) if (data[course]) peak2 += (await raceCourse(course, [{ label: 'anchor', humanBase: 4, values: data[course].anchor.values },
  ...data[course].observed.filter((o) => /^(nate-|brodi-on-psymon|jp-human)/.test(o.state)).map((o) => ({ label: o.state, humanBase: o.human_base, values: o.values }))])).raced;
console.log(`lineups: ${raced} distinct Snow Jam lineups, ${bra} Metro-City lineups and ${peak2} Peak 2 lineups raced 600 ticks; set-up-again cores = fresh cores (${((performance.now() - t0) / 1000).toFixed(0)} s)`);
// Style Mile (DSS2, Peak 2 slope style: one computer opponent, 0x239938 with rival Nate / Zoe for Nate; R&B's checks are in
// test-slopestyle-bigair.mjs): the opponent of every derived countdown, Zoe's anchor = npc-riders.json, the documents exact.
if (fs.existsSync(new URL('DSS2/lineups.json', root))) {
  const { freestyleRoster } = await import('./lineup.js');
  const d = readJson('DSS2/lineups.json'), folder = new URL('../../../local/assets/native/DSS2/lineups/', root);
  assert.equal(JSON.stringify(assembleLineup(d, d.anchor.human_base, d.anchor.values)), JSON.stringify(readJson('DSS2/npc-riders.json')), 'DSS2: Zoe with the anchor opponent = npc-riders.json');
  let exact = 0;
  for (const o of d.observed) {
    assert.deepEqual(o.values, [freestyleRoster(seededWords(o.seed), o.human_base, peakRival(d.peak, o.human_base)).at(-1)], `DSS2 ${o.state}: the opponent is the last shuffled character`);
    assert.equal(o.shared_draws, loadDraws(d, { base: o.human_base, cheat: cheatIds.has(o.human) ? 1 : 0 }) + d.load_draws.start + (o.state === '__anchor' ? d.load_draws.reference_anchor_extra ?? 0 : 0), `DSS2 ${o.state}: game RNG draws at the anchor`);
    const file = new URL(`${o.state}.json`, folder); if (!fs.existsSync(file)) continue;
    const doc = JSON.parse(fs.readFileSync(file));
    const own = (key) => Object.fromEntries(doc.riders.map((r) => { const l = leaves(r); return [String(r.slot), Object.fromEntries(d.paths[key].map((p) => [p, l[p]]))]; }));
    assert.deepEqual(strip(assembleLineup(d, o.human_base, o.values, { moment: own('moment'), state: own('state'), extra: { anchor_tick: doc.anchor_tick, human_identity: doc.human_identity } })), strip(doc), `DSS2 ${o.state}: assembled document`);
    exact++;
  }
  assert.deepEqual([...new Set(d.observed.map((o) => o.values[0]))].sort((a, b) => a - b), [0, 1, 2, 3, 4, 5, 6, 8, 9], 'DSS2: every possible opponent observed (Nate is the rival or the human)');
  console.log(`lineups: Style Mile: ${d.observed.length} opponents by 0x239938, ${exact} documents assembled exactly, anchor = npc-riders.json`);
}
process.exit(0);
