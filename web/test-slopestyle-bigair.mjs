// R&B slope style (ASS1) and Crow's Nest big air (ABA1): event configuration from the PS2 savestates and the freestyle
// handler rules that differ by kind (docs/slopestyle-bigair.md). Needs web/public/assets/{ASS1,ABA1}/freestyle-event.json
// (tools/export_freestyle_event.py) and CAREER/career.json.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {Career,MODE,aiFreestyleScore,rankEntries} from './career.js';
import {freestyleRoster,freestyleSlots,nextWord,seededWords,REFERENCE_PRESENTATION_SEED,REFERENCE_ROSTER_SEED,SEED_DRAW} from './lineup.js';
import {setPv} from './pv-flags.js';

const read = (p) => JSON.parse(fs.readFileSync(new URL('public/assets/' + p, import.meta.url)));
const data = read('CAREER/career.json'), rules = data.rules;
class Memory { constructor() { this.map = new Map(); } getItem(k) { return this.map.get(k) ?? null; } setItem(k, v) { this.map.set(k, String(v)); } removeItem(k) { this.map.delete(k); } }

// --- event configuration read from the countdown anchors (GMM *0x4A2C6C, 0x535C1x, 0x4D33B8) -------------------------
const rnb = read('ASS1/freestyle-event.json'), crows = read('ABA1/freestyle-event.json');
assert.deepEqual([rnb.game_mode, rnb.handler, rnb.freestyle_kind, rnb.posted_riders, rnb.computer_riders, rnb.time_limit_ticks, rnb.timed, rnb.rank_mode, rnb.event_riders],
  [1, 0, 1, 4, 5, 5400, 1, 2, 2], 'R&B: slope style, freestyle handler, 4 posted riders (0x238E20), 1:30, score ranking, human + opponent');
assert.deepEqual(rnb.bonus.filter((b) => b.value).map((b) => [b.value, Math.round(b.distance * 100) / 100]), [[60, 176456.97], [60, 75402.55]], 'R&B checkpoint list 0x4D33B8');
assert.deepEqual([crows.game_mode, crows.handler, crows.freestyle_kind, crows.posted_riders, crows.time_limit_ticks, crows.rank_mode, crows.event_riders],
  [3, 0, 3, 5, 3600, 2, 1], "Crow's Nest: big air, 5 posted riders, 1:00, solo");
assert.ok(crows.bonus.every((b) => b.value === 0), "Crow's Nest has no checkpoint bonus");
// the table 0x440B38 time limits agree with GMM+0x78
assert.equal(rules.freestyle_ai.find((r) => r.course === 5).time_limit * 60, rnb.time_limit_ticks);
assert.equal(rules.freestyle_ai.find((r) => r.course === 8).time_limit * 60, crows.time_limit_ticks);

// --- 0x1453D0 posted scores of the two savestates (level 1, draws in slot order) ---------------------------------------
const draws = (list) => { let k = 0; return () => (list[k++] + 100 + 200) % 200 + 200 * 7; }; // rand%200-100 = list[k]
const post = (course, list, n) => { const r = draws(list); return Array.from({ length: n }, (_, ai) => aiFreestyleScore(rules, course, 1, ai, 1, r)); };
assert.deepEqual(post(5, [36, 68, -8, 21], 4), [254500, 124080, 49800, 35360], 'R&B slots 2..5 (0x536640 in the ready savestate)');
assert.deepEqual(post(8, [36, 68, -8, 21, 21], 5), [61080, 43420, 19920, 12120, 5040], "Crow's Nest slots 1..5");

// --- roster 0x239938 and posting 0x239AA0 -> 0x1453D0 on the roster generator 0x4C9548 (web/career.js, web/lineup.js) ---------
// Every PS2 savestate of tools/export_freestyle_rosters.py: the three Quick Play anchors, the derived R&B countdowns
// (characters/lineups-ASS1: 10 humans, 9 opponents, 10 cheat humans), Conquer the Mountain heat 1 of R&B and The Junction
// (all three rounds of the handler table) and the CTM R&B level-0/level-2 profiles. The simulation must give the roster
// (GMM+0x18..), who rides, every posted score of every round and the exact number of generator draws.
const peakOf = (course) => data.courses[course].field54 + 1;
const rival = (course, human) => [[3, 5], [7, 4], [8, 6]][peakOf(course) - 1][human === [3, 7, 8][peakOf(course) - 1] ? 1 : 0];
const modeOf = (course) => ({ slopestyle: MODE.SLOPESTYLE, superpipe: MODE.HALFPIPE, bigair: MODE.BIGAIR })[data.courses[course].kind];
const facts = JSON.parse(fs.readFileSync(new URL('public/test-data/CAREER/freestyle-rosters.json', import.meta.url))).states;   // test data
let fsStates = 0, fsScores = 0;
for (const f of facts) {
  const single = !!f.single, mode = modeOf(f.course), slope = mode === MODE.SLOPESTYLE, riv = rival(f.course, f.player);
  const w = seededWords(f.roster_seed); let draws = 0; const counted = () => { draws++; return nextWord(w); };
  const entries = freestyleRoster(w, f.player, riv); draws += 50;   // 0x23C770: 25 swaps, two draws each
  const { opponent, posted } = freestyleSlots(entries, { rival: riv, single, slope });
  assert.deepEqual(f.gmm_characters.slice(0, 10), [f.player, single ? entries[7] : riv, ...entries], `${f.state}: roster GMM+0x18.. (0x239938)`);
  assert.equal(f.computer_riders, opponent != null ? 1 : 0, `${f.state}: computer riders 0x535C04`);
  assert.equal(f.posted_count, slope && single ? 4 : 5, `${f.state}: posted riders GMM+0x14`);
  const first = slope && single ? 2 : 1;
  posted.forEach((c, col) => {
    const want = [0, 1, 2].map((r) => f.rounds[r][first + col]);
    const got = [1, 2, 3].map((round) => (round > 1 && single ? null : aiFreestyleScore(rules, f.course, round, col, f.level, counted)));
    if (single) got[1] = got[2] = got[0];
    assert.deepEqual(got, want, `${f.state}: posted slot ${first + col} (${c}) heat 1 / heat 2 / final (0x1453D0 level ${f.level})`);
    fsScores += single ? 1 : 3;
  });
  assert.equal(draws, f.roster_draws, `${f.state}: roster generator draws since the seed`);
  // the same through Career (web/career.js startEvent, as the browser posts): characters, scores, the opponent
  const c = new Career(data, { storage: new Memory(), rosterSeed: () => f.roster_seed });
  const id = ['moby', 'kaori', 'allegra', 'mac', 'zoe', 'griff', 'elise', 'nate', 'psymon', 'viggo'][f.player];
  c.rider(id).level.freestyle.level = f.level;
  const ev = c.startEvent(id, mode, f.course, !single);
  assert.deepEqual(ev.ai.map((a) => a.character), f.gmm_characters.slice(first, first + posted.length), `${f.state}: Career posted characters`);
  assert.deepEqual(ev.ai.map((a) => a.scores), posted.map((_, col) => [0, 1, 2].map((r) => f.rounds[r][first + col])), `${f.state}: Career posted scores`);
  assert.equal(ev.opponent?.character ?? null, opponent, `${f.state}: Career opponent`);
  if (!single) assert.deepEqual(c.save.roster, (() => { const x = seededWords(f.roster_seed); for (let k = 0; k < f.roster_draws; k++) nextWord(x); return x; })(), `${f.state}: career generator words after the event`);
  fsStates++;
}
assert.ok(fsStates >= 5, 'freestyle-rosters.json (tools/export_freestyle_rosters.py)');

// The reference sessions (boot seed 0x182200): the roster seed is presentation draw 130; the objectives cards (PS2
// nav/out-{rnb,crows,junction}-load/final.png) and the CTM heat-1 cards (menus/rnbctm/zoe-a.f03400, pipegu/nav2.final).
{ const w = seededWords(REFERENCE_PRESENTATION_SEED); let v = 0; for (let n = 0; n < SEED_DRAW; n++) v = nextWord(w); assert.equal(v, REFERENCE_ROSTER_SEED); }
const NAMES = ['Moby', 'Kaori', 'Allegra', 'Mac', 'Zoe', 'Griff', 'Elise', 'Nate', 'Psymon', 'Viggo'];
const card = (mode, course, career) => {
  const c = new Career(data, { storage: new Memory(), rosterSeed: () => REFERENCE_ROSTER_SEED }); c.rider('zoe');
  const ev = c.startEvent('zoe', mode, course, career);
  return { ev, rows: c.postedStandings(ev).slice(0, 3).map((r) => `${NAMES[r.character]} ${r.score}`) };
};
assert.deepEqual(card(MODE.SLOPESTYLE, 5, false).rows, ['Nate 254500', 'Kaori 124080', 'Elise 49800'], 'R&B Single Event card');
assert.equal(NAMES[card(MODE.SLOPESTYLE, 5, false).ev.opponent.character], 'Moby', 'R&B Single Event opponent (the anchor rides Moby)');
assert.deepEqual(card(MODE.SLOPESTYLE, 5, false).ev.ai.map((a) => `${NAMES[a.character]} ${a.scores[0]}`), ['Nate 254500', 'Kaori 124080', 'Elise 49800', 'Griff 35360']);
assert.deepEqual(card(MODE.BIGAIR, 8, false).ev.ai.map((a) => `${NAMES[a.character]} ${a.scores[0]}`), ['Moby 61080', 'Nate 43420', 'Kaori 19920', 'Elise 12120', 'Griff 5040'], "Crow's Nest card");
assert.deepEqual(card(MODE.HALFPIPE, 11, false).rows, ['Moby 162880', 'Nate 98220', 'Kaori 44820'], 'Junction card');
// Peaks 2 and 3: the Single Event countdown anchors of the reference sessions (local/reference/pcsx2/<course>-countdown-anchor.p2s,
// read with tools/export_freestyle_rosters.py facts(): roster seed 0xB57109A9, 0x57A600 posted slots). The page posts the same with
// ?presentationSeed=0x182200; without it the boot seed is the local clock (0x31AE94 reads the RTC), so a page session posts other
// scores than a given capture, as a PS2 booted at another time does (the HUD sweep's CBA2 91620 / 51700 / 29880).
for (const [course, mode, want] of [[9, MODE.BIGAIR, [91620, 51700, 29880, 24240, 14140]], [10, MODE.BIGAIR, [162880, 113740, 79680, 62640, 45480]],
  [6, MODE.SLOPESTYLE, [356300, 206800, 89640, 70720]], [7, MODE.SLOPESTYLE, [661700, 403260, 224100, 141460]],
  [12, MODE.HALFPIPE, [223960, 175780, 94620, 75780, 60660]], [13, MODE.HALFPIPE, [509000, 310200, 219120, 171780, 121320]]])
  assert.deepEqual(card(mode, course, false).ev.ai.map((a) => a.scores[0]), want, `${data.courses[course].name} anchor: posted slots (0x57A600)`);
const ctm = card(MODE.SLOPESTYLE, 5, true);
assert.deepEqual(ctm.rows, ['Mac 254500', 'Nate 121260', 'Kaori 48620'], 'CTM R&B heat-1 card');
assert.equal(ctm.ev.opponent, null, 'CTM slope style: nobody rides (0x238E20 round 1: GMM+0x14 = 5)');
assert.deepEqual(card(MODE.HALFPIPE, 11, true).rows, ['Mac 162880', 'Nate 95980', 'Kaori 43760'], 'CTM Junction heat-1 card');
{ // CTM: the world load seeds the generator once; a second fresh event draws on (R&B then The Junction: 65 + 65 draws)
  const c = new Career(data, { storage: new Memory(), rosterSeed: () => REFERENCE_ROSTER_SEED }); c.rider('zoe'); c.seedRoster();
  c.startEvent('zoe', MODE.SLOPESTYLE, 5, true);
  const w = seededWords(REFERENCE_ROSTER_SEED); for (let k = 0; k < 65; k++) nextWord(w);
  assert.deepEqual(c.save.roster, w);
  const second = c.startEvent('zoe', MODE.HALFPIPE, 11, true), e = freestyleRoster(w, 4, 3);
  assert.deepEqual(second.ai.map((a) => a.character), [3, ...e.slice(0, 4)], 'second career event continues the generator');
  // pv freshEvent: every career entry runs 22D6C8 -> cGameModeMan_initGameMode 0x22D89C (unconditional) -> the old handler's
  // vt+0x1C 0x238C80 (GMM +0x84 = 1 fresh, +0x70 = +0x74 = 0): entering The Junction again posts a fresh heat 1 whose roster
  // draws on from the generator (another 65 draws), not the started event (PS2 ctm-parity/fresh: the Snow Jam final given up,
  // re-entered -> "Qualifier")
  setPv('freshEvent', true);
  const drawn = (k) => { const x = seededWords(REFERENCE_ROSTER_SEED); for (let i = 0; i < k; i++) nextWord(x); return x; };
  assert.deepEqual(c.save.roster, drawn(130), 'two fresh events: 65 + 65 draws');
  const again = c.startEvent('zoe', MODE.HALFPIPE, 11, true), e2 = freestyleRoster(drawn(130), 4, 3);
  assert.notEqual(again, second, 'a career re-entry is a fresh event (0x22D89C -> 0x238C80)');
  assert.equal(again.round, 1, 'fresh: heat 1 (GMM +0x70 = 0)');
  assert.deepEqual(again.ai.map((a) => a.character), [3, ...e2.slice(0, 4)], 'the re-entry draws on from the generator');
  assert.deepEqual(c.save.roster, drawn(195), 'the re-entry drew another 65 words');
  // switch off (the port before freshEvent): the started event is resumed, nothing drawn
  setPv('freshEvent', false);
  const held = c.save.roster.slice(), resumed = c.startEvent('zoe', MODE.HALFPIPE, 11, true);
  assert.equal(resumed, again, 'pv freshEvent off: a started career event is not posted again');
  assert.deepEqual(c.save.roster, held, 'pv freshEvent off: no draws');
  setPv('freshEvent', null);
  const race = c.startEvent('zoe', MODE.RACE, 0, true); assert.equal(race.roster.entries.length, 10, 'a fresh career race builds its roster (0x23A4F0) on the same generator');
}
console.log(`Freestyle rosters: ${fsStates} PS2 savestates (roster, opponent, ${fsScores} posted scores, generator draws) and the objectives cards exact`);

// --- 0x122E50 final estimate ------------------------------------------------------------------------------------------
const f = Math.fround;
const est = (s, rem, org) => { const s1 = s + 1; let d = f(f(org) - f(rem)); if (d < 1) d = 1; let l = f(f(rem) - 1000); if (l < 0) l = 0; return s1 + Math.trunc(f(l * f(f(s1) / d))); };
for (const [s, rem, org] of [[20000, 100000, 246217.5], [0, 246217.5, 246217.5], [5, 500, 246217.5], [123456, 150303.765625, 246217.5]])
  assert.equal(Career.opponentEstimate(s, rem, org), est(s, rem, org));
assert.equal(Career.opponentEstimate(20000, 100000, 246217.5), 20001 + Math.trunc(f(99000 * f(20001 / f(146217.5)))));

// --- results 0x239230 with the live opponent ------------------------------------------------------------------------
{ // Single Event final: the opponent finished before the player -> its finish score
  const c = new Career(data, { storage: new Memory() }); c.rider('zoe');
  const ev = c.startEvent('zoe', MODE.SLOPESTYLE, 5, false);
  const posted = ev.ai.map((a) => a.scores[2]);
  const out = c.freestyleResult(130000, { finished: true, score: 300000, estimate: 0 });
  assert.equal(ev.opponent.scores[2], 300000);
  assert.deepEqual(out.totals, [130000, 300000, ...posted]);
  assert.deepEqual(out.rank, rankEntries([130000, 300000, ...posted], false));
  assert.equal(out.rows[1].opponent, true);
}
{ // final with the opponent still riding -> 0x122E50 estimate
  const c = new Career(data, { storage: new Memory() }); c.rider('zoe');
  const ev = c.startEvent('zoe', MODE.SLOPESTYLE, 5, false);
  c.freestyleResult(1000, { finished: false, score: 40000, estimate: 77777 });
  assert.equal(ev.opponent.scores[2], 77777);
}
{ // career slope style heat 1: no opponent, the five posted riders (rival first); heat 1 ranks heat 1 + heat 2 totals
  const c = new Career(data, { storage: new Memory() }); c.rider('zoe');
  const ev = c.startEvent('zoe', MODE.SLOPESTYLE, 5, true);
  const out = c.freestyleResult(600000, null);
  assert.equal(out.totals.length, 6); assert.deepEqual(out.totals.slice(1), ev.ai.map((a) => a.scores[0] + a.scores[1]));
  assert.equal(out.place, 0); assert.equal(ev.round, 3, 'top 3 in heat 1 goes straight to the final');
}
console.log("Slope style / big air: R&B and Crow's Nest configuration, posting, opponent estimate and results OK");

// --- the R&B computer opponent for every human (web/public/assets/ASS1/lineups.json, tools/export_lineups.py --course ASS1) ---
// 1. every derived countdown's opponent is the last shuffled character (0x239938) and its npc-riders document is assembled
//    exactly from the parts (local/assets/native/ASS1/lineups/*.json when present); Zoe with the anchor = npc-riders.json;
// 2. every possible opponent (the nine base riders other than Mac, who is the rival or the human) is covered, and races;
// 3. held-out lineups raced 900 ticks against PS2 --ai-state captures (local/ps2-capture/runs/lineups-ASS1, when present).
{
  const { assembleLineup, anchorRandomWords, loadDraws } = await import('./lineup.js');
  const { createNodeRace } = await import('./ai-race-node.mjs');
  const d = read('ASS1/lineups.json'), roster = read('riders.json');
  const cheatIds = new Set(roster.filter((r) => r.kind === 'cheat').map((r) => r.id));
  assert.equal(JSON.stringify(assembleLineup(d, d.anchor.human_base, d.anchor.values)), JSON.stringify(read('ASS1/npc-riders.json')), 'R&B: Zoe with the anchor opponent = npc-riders.json');
  const strip = (doc) => { const x = JSON.parse(JSON.stringify(doc)); delete x.lineup_state; delete x.provenance; for (const row of x.world.records) for (const r of row) { delete r.distance; delete r.bearing; } return x; };
  const leaves = (x, pre = '', out = {}) => { if (x && typeof x === 'object' && !Array.isArray(x)) { for (const [k, v] of Object.entries(x)) leaves(v, pre ? `${pre}.${k}` : k, out); } else out[pre] = x; return out; };
  const folder = new URL('../local/assets/native/ASS1/lineups/', import.meta.url);
  let assembled = 0;
  for (const o of d.observed) {
    const riv = rival(5, o.human_base), entries = freestyleRoster(seededWords(o.seed), o.human_base, riv);
    assert.deepEqual(o.values, [entries[7]], `R&B ${o.state}: the opponent is the last shuffled character`);
    assert.equal(o.shared_draws, loadDraws(d, { base: o.human_base, cheat: cheatIds.has(o.human) ? 1 : 0 }) + d.load_draws.start, `R&B ${o.state}: game RNG draws at the anchor`);
    const file = new URL(`${o.state}.json`, folder); if (!fs.existsSync(file)) continue;
    const doc = JSON.parse(fs.readFileSync(file));
    const own = (key) => Object.fromEntries(doc.riders.map((r) => { const l = leaves(r); return [String(r.slot), Object.fromEntries(d.paths[key].map((p) => [p, l[p]]))]; }));
    const got = assembleLineup(d, o.human_base, o.values, { moment: own('moment'), state: own('state'), extra: { anchor_tick: doc.anchor_tick, human_identity: doc.human_identity } });
    assert.deepEqual(strip(got), strip(doc), `R&B ${o.state}: assembled document`);
    assembled++;
  }
  const opponents = [...new Set(d.observed.map((o) => o.values[0]))].sort((a, b) => a - b);
  assert.deepEqual(opponents, [0, 1, 2, 4, 5, 6, 7, 8, 9], 'R&B: every possible Quick Play opponent observed (Mac is the rival or the human)');
  // each opponent races the grid start (600 ticks) for a human it can meet, cores set up again in place
  const race = await createNodeRace({ course: 'ASS1' }), pad = new Float32Array(24);
  const pos = (c) => Array.from(new Float32Array(c.HEAPF32.buffer, c._rider_world_state(), 16)).slice(0, 3);
  for (const v of opponents) {
    const o = d.observed.find((x) => x.values[0] === v), doc = assembleLineup(d, o.human_base, [v]);
    for (const r of doc.riders) race.resources.riderText[r.package] ??= fs.readFileSync(new URL(`public/assets/${r.package}/rider.json`, import.meta.url), 'utf8');
    race.racers.setDocument(doc); race.racers.setAnchorRng(anchorRandomWords(d, { base: o.human_base, cheat: 0 }));
    assert.equal(race.racers.npcs[0].character, doc.riders[0].character);
    race.start(); const start = pos(race.racers.npcs[0].core);
    for (let t = 0; t < 600; t++) race.tick(pad);
    const p = pos(race.racers.npcs[0].core), moved = Math.hypot(p[0] - start[0], p[1] - start[1], p[2] - start[2]);
    assert(p.every(Number.isFinite) && moved > 1000, `R&B opponent ${doc.riders[0].character} moved ${moved.toFixed(0)} cm in 600 ticks`);
  }
  // held-out captures (tools/ps2_capture.py --ai-state --isolate, 900 idle ticks from the derived countdowns)
  const { execFile } = await import('node:child_process'), os = await import('node:os');
  const runs = new URL('../local/ps2-capture/runs/lineups-ASS1/', import.meta.url);
  const caps = fs.existsSync(runs) ? fs.readdirSync(runs).filter((f) => f.endsWith('.bin')).map((f) => f.slice(0, -4)) : [];
  let exact = 0;
  for (const cap of caps) {
    const state = cap.replace(/^ass1-/, ''), o = state === 'zoe-anchor' ? d.observed.find((x) => x.state === '__anchor') : d.observed.find((x) => x.state === state);
    if (!o) continue;
    const humanId = roster.some((r) => r.id === o.human) ? o.human : state.split('-')[0];
    const doc = assembleLineup(d, o.human_base, o.values, { extra: { anchor_tick: o.tick } }), tmp = os.tmpdir();
    const docFile = `${tmp}/ass1-${cap}.json`, report = `${tmp}/ass1-${cap}.report.json`; fs.writeFileSync(docFile, JSON.stringify(doc));
    const r = await new Promise((resolve, reject) => execFile(process.execPath, ['compare-ai-capture.mjs', new URL(`${cap}.bin`, runs).pathname, '--world-draws', '--human', humanId, '--document', docFile, '--report', report, '--isolate'],
      { cwd: new URL('.', import.meta.url).pathname, maxBuffer: 1 << 28 }, (err) => { if (err) return reject(err); const x = JSON.parse(fs.readFileSync(report)).summary; fs.rmSync(docFile); fs.rmSync(report); resolve(x); }));
    assert(r.ticks >= 899 && !r.firstRngMismatch && !r.firstRankMismatch && r.ai.every((a) => !a.firstInexact), `R&B ${cap}: the opponent / RNG / ranks exact vs the PS2 ${JSON.stringify(r).slice(0, 400)}`);
    exact++;
  }
  console.log(`R&B opponents: ${d.observed.length} countdowns (${assembled} documents assembled exactly), ${opponents.length} opponents race, ${exact} held-out captures exact for 900 ticks`);
}
process.exit(0);
