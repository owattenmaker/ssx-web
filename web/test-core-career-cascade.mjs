// pv coreCareerCascade / coreCareerLodge (docs/core-migration.md M5d / M5e): the career's award cascade, its messages and the lodge's
// purchases in the rules module (engine/career_cascade.hpp) give the JS's results. A scripted career session (race / freestyle / rival rounds, collectibles, cash,
// Big Challenges, explore goals, direct awards, monster medals, relationship notices, lodge purchases and gear, a non-integer value
// in the save) runs in child
// processes: the switch off, on, and on with the kernel failing at one call (the page's fallback). Every round's result (awards,
// opened passes, the reward record, the mail icon), the generator's next words and the save blob must be identical. Skipped when the
// rules module (web/runtime/rules.js; RULES_JS=<scratch>) has no kernel.
import fs from 'node:fs';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const self = fileURLToPath(import.meta.url);
const EVENTS = Number(process.env.EVENTS ?? 500);

if (process.env.CASCADE_ROLE !== 'run') {
  const run = (seed, pv, failAt = '') => {
    const r = spawnSync(process.execPath, [self], {
      env: { ...process.env, CASCADE_ROLE: 'run', SCRIPT_SEED: String(seed), SSX_PV: pv, FAIL_AT: failAt, SSX_QUERY: 'presentationSeed=0x182200' },
      encoding: 'utf8',
      maxBuffer: 1 << 28
    });
    if (r.status !== 0) throw new Error(`core-career-cascade: the session failed (${pv} ${failAt}):\n${r.stderr.slice(-2000)}`);
    return JSON.parse(r.stdout);
  };
  const probe = run(1, 'rulesModule', 'probe');
  if (!probe.kernel) {
    console.log('core-career-cascade: this rules module has no career kernel (skipped)');
    process.exit(0);
  }
  let calls = 0;
  for (const seed of [1, 77]) {
    const off = run(seed, 'rulesModule,-coreCareerCascade,-coreCareerLodge,-coreCareerWorld');
    for (const [name, failAt] of [['on', ''], ['fallback', '25']]) {
      const on = run(seed, 'rulesModule,coreCareerCascade,coreCareerLodge,coreCareerWorld', failAt);
      for (const k of ['results', 'next', 'save']) {
        if (JSON.stringify(on[k]) !== JSON.stringify(off[k])) {
          const a = JSON.stringify(off[k]);
          const b = JSON.stringify(on[k]);
          let i = 0;
          while (i < a.length && a[i] === b[i]) i++;
          throw new Error(`core-career-cascade: seed ${seed} ${name}: ${k} differs at ${i}: ${a.slice(i - 80, i + 80)} / ${b.slice(i - 80, i + 80)}`);
        }
      }
      if (name === 'on' && (!on.calls || on.fails)) throw new Error(`core-career-cascade: seed ${seed}: ${on.calls} kernel calls, ${on.fails} failed`);
      calls += on.calls;
    }
  }
  console.log(`core-career-cascade: 2 sessions x ${EVENTS} events identical off / on / after a kernel failure (${calls} kernel calls)`);
  process.exit(0);
}

// ---- one session (a child process) ---------------------------------------------------------------------------------------------
const { Career, MODE, RIDER_CHARACTER } = await import('./career.js');
const { CareerMessages } = await import('./career-messages.js');
const lineup = await import('./lineup.js');
const { loadRulesModule, rulesModule } = await import('./rules-module.js');
const { setBigChallengeTable, challengeStatus } = await import('./big-challenges.js');
const read = (p) => JSON.parse(fs.readFileSync(new URL(p, import.meta.url)));
const data = read('./public/assets/CAREER/career.json');
const shop = read('./public/assets/CAREER/shop.json');
const messages = read('./public/assets/CAREER/messages.json');
const rows = read('./public/assets/BIGCHAL/big-challenges.json').challenges;
const models = fs
  .readdirSync(new URL('./public/assets/', import.meta.url))
  .filter((d) => fs.existsSync(new URL(`./public/assets/${d}/lineups.json`, import.meta.url)))
  .map((d) => read(`./public/assets/${d}/lineups.json`).presentation_model)
  .filter(Boolean);
setBigChallengeTable(rows);
const m = await loadRulesModule();
if (process.env.FAIL_AT === 'probe') {
  process.stdout.write(JSON.stringify({ kernel: !!m?._career_kernel }));
  process.exit(0);
}
let calls = 0;
let fails = 0;
if (m?._career_kernel) {
  const kernel = m._career_kernel;
  m._career_kernel = (p) => {
    calls++;
    if (calls === Number(process.env.FAIL_AT)) {
      fails++;
      return 0;
    }
    return kernel(p);
  };
}
class Memory {
  constructor() {
    this.map = new Map();
  }
  getItem(k) {
    return this.map.get(k) ?? null;
  }
  setItem(k, v) {
    this.map.set(k, String(v));
  }
  removeItem(k) {
    this.map.delete(k);
  }
}
let script = Number(process.env.SCRIPT_SEED) >>> 0;
const rand = () => {
  script = (Math.imul(script, 1664525) + 1013904223) >>> 0;
  return script;
};
const pick = (list) => list[rand() % list.length];
// (the generator's high bits: its low bits repeat with a short period)
const high = (n) => (rand() >>> 16) % n;
const storage = new Memory();
const career = new Career(data, { storage, shop });
const cu = {
  ui: {},
  career,
  riderId: 'zoe',
  active: null,
  get me() {
    return career.rider(this.riderId);
  }
};
const box = new CareerMessages(cu);
box.data = messages;
box.hook(career);
const riders = Object.keys(RIDER_CHARACTER);
const courses = data.courses.map((c, i) => i).filter((i) => career.standardMode(i) != null);
const results = [];
const note = (out) => [JSON.stringify(out.awards ?? null), out.opened ?? null, career.rewardRecordCount(), JSON.stringify(box.hud)];
for (let e = 0; e < EVENTS; e++) {
  cu.riderId = pick(riders);
  const id = cu.riderId;
  const course = pick(courses);
  let mode = career.standardMode(course);
  if (rand() % 10 === 0) mode = mode === MODE.RACE ? MODE.RIVAL_TIME : MODE.RIVAL_POINTS;
  const isCareer = rand() % 5 !== 0;
  if (rand() % 17 === 0) career.seedRoster();
  career.startEvent(id, mode, course, isCareer);
  cu.active = { career: isCareer };
  const model = pick(models);
  const ticks = 600 + (rand() % 9000);
  lineup.noteRaced(() => ({ model, ticks }));
  for (let k = rand() % 3; k > 0; k--) box.notify(rand() % 10, rand() % 50);
  if (rand() % 7 === 0) results.push(['earn', JSON.stringify(career.earnCash(id, 100 + (rand() % 5000)))]);
  if (rand() % 9 === 0) career.markCollected(id, course, rand() % 64, rand() % 3 ? 0 : 250);
  let out = null;
  if (mode === MODE.RACE) {
    for (let round = 0; round < 3; round++) {
      const standings = Array.from({ length: 6 }, (_, k) => ({
        name: `r${k}`,
        character: k,
        human: k === 0,
        finishTicks: rand() % 4 === 0 ? null : 5000 + (rand() % 3000),
        remaining: rand() % 2000,
        place: k,
        dnf: rand() % 40 === 0
      }));
      out = career.raceResult(standings, { raceTicks: 5000 + (rand() % 3000), origin: 0 });
      results.push([e, round, out.place, out.medal ?? null, out.cash ?? null, ...note(out)]);
      if (!out.advance) break;
    }
  } else if (mode === MODE.RIVAL_TIME || mode === MODE.RIVAL_POINTS) {
    const win = rand() % 2 === 0;
    out = career.rivalResult({ win, values: [1, 2], place: win ? 0 : 1 }, { ticks: 4000 + (rand() % 4000), score: rand() % 200000 });
    results.push([e, 0, out.place, out.medal ?? null, out.cash ?? null, ...note(out)]);
  } else {
    for (let round = 0; round < 3; round++) {
      out = career.freestyleResult(rand() % 300000, null);
      results.push([e, round, out.place, out.medal ?? null, out.cash ?? null, ...note(out)]);
      if (out.medal !== undefined || !out.advance) break;
    }
  }
  if (rand() % 11 === 0) lineup.settleRaced();
  if (rand() % 5 === 0) {
    const r = career.rider(id);
    r.bigChallenges ??= rows.map(() => 0);
    for (let k = rand() % 6; k > 0; k--) r.bigChallenges[rand() % rows.length] |= 8;
    results.push(['explore', JSON.stringify(career.exploreGoal(id, 1 + (rand() % 3), rand() % 2 ? 'challenge' : null))]);
  }
  if (rand() % 23 === 0) results.push(['grant', JSON.stringify(career.grantAward(id, rand() % 32))]);
  // the world's events (M5f): a collect with cash, a Big Challenge's status word (bit 3: completed)
  if (rand() % 4 === 0) results.push(['collect', career.markCollected(id, pick(courses), (rand() >>> 16) % 70 - 3, (rand() >>> 16) % 3 ? 0 : 250)]);
  if (rand() % 4 === 0) {
    const i = (rand() >>> 16) % (rows.length + 2) - 1;
    challengeStatus(career, id, i, ((rand() >>> 16) & 0x1f) | ((rand() >>> 16) % 2 ? 8 : 0));
  }
  if (rand() % 29 === 0) career.monster(id).medals[rand() % 8] = 1 + (rand() % 4);
  if (rand() % 41 === 0) career.rider(id).best['x:' + (rand() % 3)] = (rand() % 1000) / 7;
  // the lodge (M5e): every purchase and selection, by turns
  for (let k = rand() % 4; k > 0; k--) {
    const peak = 1 + (rand() % 3);
    const what = high(8);
    let got;
    if (what === 0) {
      const inv = career.gear(id);
      const e = inv.entries[rand() % inv.entries.length];
      got = career.buyGear(id, e.item, rand() % 3 ? e.tier : peak);
    } else if (what === 1) {
      const inv = career.gear(id);
      got = career.equipGear(id, inv.entries[rand() % inv.entries.length].item, rand() % 2 === 0);
    } else if (what === 2) got = career.buyUber(id, high(10), high(12));
    else if (what === 3) got = career.selectUber(id, high(10), high(12));
    else if (what === 4) got = career.buySong(id, rand() % 40);
    else if (what === 5) {
      const category = pick(Object.keys(data.rewards));
      got = [career.buyReward(id, category, rand() % 30, peak), JSON.stringify(career.lastBonus ?? null)];
    } else if (what === 6) got = career.buyAttributePoint(id, high(8));
    else got = career.buyAttribute(id, high(8));
    results.push(['lodge', e, what, JSON.stringify(got)]);
  }
  if (rand() % 3 === 0) career.rider(id).cash += 20000 + (rand() % 200000);
}
const next = [];
for (let k = 0; k < 8; k++) next.push(lineup.presentationDraw());
next.push(lineup.nextRosterSeed());
const save = JSON.parse(storage.getItem('ssx3.career.v2'));
delete save.savedAt;
process.stdout.write(JSON.stringify({ results, next, save, calls, fails, booted: rulesModule()?._presentation_booted() ?? null }));
