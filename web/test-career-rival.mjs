// The peak rival in career events (docs/career-events.md "The peak rival in career events", docs/ai-racers.md "Difficulty by race level"):
//  * career slope style: nobody rides, the rival is POSTED in slot 1 (0x238E20 career path GMM+0x14 = 5, 0x239938 +0x1C = 0x145750) --
//    the derived PS2 career Style Mile heat 1 (Nate 358740) through web/lineup.js / web/career.js;
//  * a career race final: slot 1 is the rival (0x23A108 -> 0x23A3D8), the Peak 2 courses' Nate from lineups.json's career-only parts
//    (pv careerRival) and the riders' 0x10C4F8 difficulty words from the race level (pv careerLevel): the page's assembly of the derived
//    PS2 career finals (characters/career/{CRA3,DRA4}-final-zoe) equals their riders leaf for leaf;
//  * 0x10C4F8 against every exported countdown document (Single Event level 1, career levels).
// The simulation of those finals is gated by test-ps2-captures careerrival/{cra3,dra4}-final.
// CAREER_RIVAL_DATA=dir: read <dir>/<course>/lineups.json (a scratch export) instead of public/assets.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { Career, MODE, aiFreestyleScore } from './career.js';
import { assembleLineup, freestyleRoster, freestyleSlots, lineupFor, nextWord, npcDifficulty, peakRival, seededWords } from './lineup.js';
import { setPv } from './pv-flags.js';

const here = (p) => new URL(p, import.meta.url);
const json = (u) => JSON.parse(fs.readFileSync(u, 'utf8'));
const local = (p) => here('../local/' + p);
let checks = 0;

// ---- 0x10C4F8 + 0x10C758's course factor against every exported countdown document ---------------------------------------
const COURSE = { ARA1: 0, BRA2: 1, CRA3: 2, DRA4: 3 };
let docs = 0;
for (const [code, course] of Object.entries(COURSE)) {
  for (const sub of ['lineups', 'lineups-career']) {
    const dir = local(`assets/native/${code}/${sub}/`);
    if (!fs.existsSync(dir)) continue;
    for (const f of fs.readdirSync(dir).filter((n) => n.endsWith('.json') && n !== 'summary.json')) {
      const d = json(new URL(f, dir)), level = d.lineup_state?.race_level ?? 1;   // Single Event states: level 1
      for (const r of d.riders) {
        const want = npcDifficulty(r.slot, level, course);
        assert.deepEqual([r.npc.crouch_parameter_df8, r.npc.driving_state.parameter_df8, r.npc.driving_state.parameter_dfc], [want.df8, want.df8, want.dfc],
          `${code} ${f} slot ${r.slot}: 0x10C4F8 at level ${level}`);
      }
      docs++;
    }
  }
}
if (docs) { console.log(`0x10C4F8 difficulty: ${docs} exported countdowns exact`); checks++; } else console.log('skip 0x10C4F8 check: no local/assets/native lineups');
// the three levels differ (slot 2, Ruthless Ridge): level 0 / 1 / 2
assert.deepEqual([0, 1, 2].map((l) => npcDifficulty(2, l, 2).df8), [55.80497360229492, 80.38914489746094, 84.99810028076172]);
assert.equal(npcDifficulty(1, 2, 3).dfc, 1, 'slot 1 level 2 on Intimidator: capped at 1.0');

// ---- career race finals: the rival in slot 1 (0x23A3D8), never in the qualifier / semi, never in a Single Event ---------------
for (const [human, rival] of [[4, 7], [7, 4], [3, 7]]) {
  const entries = lineupFor({ seed: 0xB57109A9, human: { base: human, cheat: 0 }, peak: 1 }).entries;
  assert.ok(!entries.includes(rival), `0x23A4F0 never holds the peak rival ${rival}`);
  assert.equal(lineupFor({ entries, human: { base: human, cheat: 0 }, peak: 1, round: 3, career: true, previous: [8, null, 10, 5] }).values[0], rival, `career final slot 1 = 0x145750 (${rival})`);
  assert.ok(!lineupFor({ entries, human: { base: human, cheat: 0 }, peak: 1, round: 3, career: false }).values.includes(rival), 'Single Event final: no rival');
}
checks++;

// ---- the page's assembly of the derived PS2 career finals --------------------------------------------------------------------
const dataDir = process.env.CAREER_RIVAL_DATA ? new URL(process.env.CAREER_RIVAL_DATA.replace(/\/?$/, '/'), `file://${process.cwd()}/`) : here('public/assets/');
for (const code of ['CRA3', 'DRA4']) {
  const docFile = local(`assets/native/${code}/lineups-career/${code}-final-zoe.json`), dataFile = new URL(`${code}/lineups.json`, dataDir);
  if (!fs.existsSync(docFile)) { console.log(`skip ${code} final: no exported career countdown (tools/export_lineups.py export-career --course ${code})`); continue; }
  const data = json(dataFile), doc = json(docFile), ls = doc.lineup_state;
  if (!data.career_skins?.nate) { console.log(`skip ${code} final: ${dataFile.pathname} has no career_skins.nate (the scratch export: CAREER_RIVAL_DATA=../local/career-rival/export)`); continue; }
  assert.equal(peakRival(data.peak, ls.human_base), 7, `${code}: the Peak 2 rival is Nate`);
  const values = ls.characters.slice(1, 6).map((c, k) => (ls.cheats[k + 1] || c));   // 0x23A668 values: a cheat id rides on the human's base
  assert.deepEqual(values, [7, 8, 10, 5, 6], `${code}: the PS2 final's roster (rival, the previous round's top three, entry 9)`);
  const leaves = (r, out = {}, pre = '') => { for (const [k, v] of Object.entries(r)) { if (v && typeof v === 'object' && !Array.isArray(v)) leaves(v, out, pre + k + '.'); else out[pre + k] = v; } return out; };
  const pick = (paths) => Object.fromEntries(doc.riders.map((r) => { const f = leaves(r); return [String(r.slot), Object.fromEntries(paths.filter((p) => p in f).map((p) => [p, f[p]]))]; }));
  const opts = { round: 3, level: ls.race_level, moment: pick(data.paths.moment), state: pick(data.paths.state) };
  setPv('careerRival', false);
  assert.throws(() => assembleLineup(data, ls.human_base, values, opts), /No computer-rider data for nate/, `${code}: pv careerRival off: Nate's career-only parts stay missing`);
  setPv('careerRival', true);
  const got = assembleLineup(data, ls.human_base, values, opts).riders;
  assert.equal(got.length, doc.riders.length);
  got.forEach((r, k) => { const a = leaves(r), b = leaves(doc.riders[k]); const diff = Object.keys({ ...a, ...b }).filter((p) => JSON.stringify(a[p]) !== JSON.stringify(b[p]));
    assert.deepEqual(diff, [], `${code} final slot ${k + 1} (${doc.riders[k].character}): the page's record differs from the PS2's at ${diff.slice(0, 4)}`); });
  // level 1 (pv careerLevel off) differs from the PS2 career's level 2 only in the difficulty words
  const plain = assembleLineup(data, ls.human_base, values, { ...opts, level: null }).riders;
  plain.forEach((r, k) => { const a = leaves(r), b = leaves(doc.riders[k]); assert.deepEqual(Object.keys(a).filter((p) => JSON.stringify(a[p]) !== JSON.stringify(b[p])).sort(),
    k === 0 ? ['npc.driving_state.parameter_dfc'] : ['npc.crouch_parameter_df8', 'npc.driving_state.parameter_df8', 'npc.driving_state.parameter_dfc'], `${code} slot ${k + 1}: level 1 vs the career's level 2`); });
  setPv('careerRival', null);
  console.log(`${code} career final: the page's five riders (Nate in slot 1, round 3, race level ${ls.race_level}) equal the PS2 countdown's`); checks++;
}

// ---- career slope style: Nate posted, nobody rides (the derived PS2 career Style Mile heat 1) --------------------------------
const heatFile = local('career-rival/dss2-career-heat1.json');
if (fs.existsSync(heatFile)) {
  const f = json(heatFile), data = json(here('public/assets/CAREER/career.json')), rules = data.rules;
  assert.deepEqual([f.single_535C11, f.gmm10, f.gmm14, f.npc_count_535C04, f.riders_535BF8], [0, 5, 5, 0, 1], 'career slope style: GMM+0x14 = 5, no computer rider (0x238E20 0x238F7C)');
  const w = seededWords(f.roster_seed); let draws = 0; const counted = () => { draws++; return nextWord(w); };
  for (let k = 0; k < f.roster_draws - 65; k++) counted();   // the career's earlier events on the running generator
  const before = draws, entries = freestyleRoster(w, 4, 7); draws += 50;
  const { opponent, posted } = freestyleSlots(entries, { rival: 7, single: false, slope: true });
  assert.equal(opponent, null, 'career slope style: no opponent rides');
  assert.deepEqual([4, ...posted, ...entries.slice(4)], f.roster, 'roster GMM+0x18.. = [Zoe, Nate (0x145750), shuffled ...] (0x239938)');
  posted.forEach((c, col) => assert.deepEqual([1, 2, 3].map((round) => aiFreestyleScore(rules, f.course, round, col, f.levels[2], counted)), f.posted_rounds.map((r) => r[1 + col]),
    `posted slot ${1 + col} (${c}) heat 1 / heat 2 / final (0x1453D0, freestyle level ${f.levels[2]})`));
  assert.equal(draws, f.roster_draws, 'roster generator draws');
  assert.equal(posted[0], 7, 'Nate posted in slot 1 with the leading column');
  // the same through Career (web/career.js startEvent): no opponent, Nate first
  class Memory { constructor() { this.map = new Map(); } getItem(k) { return this.map.get(k) ?? null; } setItem(k, v) { this.map.set(k, String(v)); } removeItem(k) { this.map.delete(k); } }
  const c = new Career(data, { storage: new Memory(), rosterSeed: () => f.roster_seed }); c.rider('zoe').level.freestyle.level = f.levels[2];
  const g = c.rosterWords(true); for (let k = 0; k < before; k++) nextWord(g);
  const ev = c.startEvent('zoe', MODE.SLOPESTYLE, f.course, true);
  assert.equal(ev.opponent, null); assert.deepEqual(ev.ai.map((a) => a.character), posted); assert.deepEqual(ev.ai.map((a) => a.scores), posted.map((_, col) => f.posted_rounds.map((r) => r[1 + col])));
  console.log(`career Style Mile heat 1: Nate posted ${f.posted_rounds[0][1]} / ${f.posted_rounds[1][1]} / ${f.posted_rounds[2][1]}, nobody rides (PS2 exact)`); checks++;
} else console.log('skip career Style Mile: no local/career-rival/dss2-career-heat1.json');
console.log(`career rival: ${checks} check group(s) passed`);
