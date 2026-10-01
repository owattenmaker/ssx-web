// Conquer the Mountain flow against the PS2 (docs/ctm-parity.md). web/ctm-flow-ps2.json is the condensed PS2 trace
// (tools/ctm_flow_trace.py from the ARMSX2 captures in local/ps2-capture/ctm-parity: world states, NIS lists queued, scripts
// played, the course of every ride, the career block, and the texts / focused items read off the frames). This drives the
// browser's career screens (web/career-ui.js, web/character-select.js) through the same steps with a stub UI that records
// what the page would do (world loads, cutscene requests, rides, screens, prompts) and checks each step against that trace.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { Career, MODE, MEDAL } from './career.js';
import { CareerScreens } from './career-ui.js';
import { CharacterSelect } from './character-select.js';
import { arrivalSteps, podiumSteps, heatSteps, transportSteps } from './cutscenes.js';
import { CtmPda, ICON } from './ctm-pda.js';
import { createPauseContexts } from './pause-contexts.js';
import { createScreenPhases } from './screen-phases.js';
import { freeRideWorldOf } from './free-ride.js';
import { switchesWorld } from './ctm-transport.js';

globalThis.location ??= { href: 'http://localhost/' };   // career-ui.js reads ?qa
const ps2 = JSON.parse(fs.readFileSync(new URL('./ctm-flow-ps2.json', import.meta.url)));
const data = JSON.parse(fs.readFileSync(new URL('./public/assets/CAREER/career.json', import.meta.url)));
const shop = (() => { try { return JSON.parse(fs.readFileSync(new URL('./public/assets/CAREER/shop.json', import.meta.url))); } catch { return null; } })();
class Memory { constructor() { this.map = new Map(); } getItem(k) { return this.map.get(k) ?? null; } setItem(k, v) { this.map.set(k, String(v)); } removeItem(k) { this.map.delete(k); } }
const observed = (step) => { const o = ps2.observed.find((x) => x.step === step); assert.ok(o, `PS2 observation ${step}`); return o; };
const run = (name) => { const r = ps2.runs[name]; assert.ok(r, `PS2 run ${name}`); return r; };
const tick = () => new Promise((r) => setTimeout(r, 0));
// the NIS list ids a cutscene request queues (the PS2 trace records list ids: movie 29..31, groups 0..25)
const listIds = (steps) => steps.map((s) => (s.fmv != null ? Number(s.fmv) : s.group));
const bit = (hex, n) => ((parseInt(hex, 16) >>> 0) & (1 << n)) !== 0;
const careerAt = (r, pred) => r.career.find(([, c]) => pred(c))?.[1];

// ---- the stub page ------------------------------------------------------------------------------------------------
function page(storage = new Memory()) {
  const log = [];
  const contexts = createPauseContexts(), phases = createScreenPhases({ contexts: () => contexts });   // as web/ui.js
  const ui = {
    contexts, phases,
    screen: 'main', index: 0, ready: true, careerMode: true, log, riders: [{ id: 'zoe', name: 'Zoe' }], rider: { id: 'zoe', name: 'Zoe', kind: 'rider' },
    courses: data.courses.map((c) => ({ code: c.code, ready: true })), loading: {}, cutscene: { fadeFrom() {}, drawCover() {}, preFade: async () => {}, clearOverlay() {} },
    set(s) { this.previousScreen = this.screen; this.screen = s; this.index = 0; if (s !== this.previousScreen) phases.enter(s, this.previousScreen); log.push(['screen', s]); }, sync() {},
    items() { return cs.owns(this.screen) ? cs.items(this.screen) : []; },
    rememberRider() { log.push(['remember']); },
    cb: {
      freeRide(course, opt) { log.push(['load', course, !!opt?.reload]); return false; },   // the page switches world, then resume()
      start() { log.push(['ride', cs.freeRide?.course]); }, resume() { log.push(['resume']); }, quit() { log.push(['quit']); },
      cutscene(o) { log.push(['cutscene', o.kind, o.location ?? null, !!o.firstVisit, o.place ?? null]); return Promise.resolve({ played: true }); },
      freeRideRespawn(st) { log.push(['respawn', st]); }, attributes() {}, timeLimit() {}, standings: null, lineup: () => [],
    },
  };
  const cs = new CareerScreens(ui); ui.careerUI = cs;
  // a choice on a screen with an exit (ps2MenuInput) acts at its Stop: step the UI frames through it (web/screen-phases.js)
  { const choose = cs.choose.bind(cs); cs.choose = (i) => { const r = choose(i); for (let k = 0; phases.leaving && k < 200; k++) phases.step(); return r; }; }
  cs.data = data; cs.career = new Career(data, { storage, shop });
  cs.loc = new (cs.loc.constructor)(data.strings);
  return { ui, cs, log, storage };
}
const last = (log, kind) => [...log].reverse().find((e) => e[0] === kind);
const since = (log, at, kind) => log.slice(at).filter((e) => e[0] === kind);

// ---- 1. Select Character -> the career start (PS2 new-career: Cross -> Basic Controls load, no Setup Character) --------
{
  const { ui, cs, log } = page();
  ui.screen = 'character';
  CharacterSelect.prototype.selectDone.call({ ui, human: () => ui.rider });
  assert.ok(!log.some((e) => e[0] === 'screen' && e[1] === 'setup'), 'CTM Select Character has no Setup Character (0x1A0B00)');
  assert.deepEqual(last(log, 'load'), ['load', 14, true], 'a new career loads Happiness (145C38: +0 set -> course 14), a world load');
  assert.equal(observed('ctm-select').screen, 'load');
  // Single Event keeps Setup Character (0x1A0AD0)
  const other = page(); other.ui.careerMode = false; CharacterSelect.prototype.selectDone.call({ ui: other.ui, human: () => other.ui.rider });
  assert.equal(other.ui.screen, 'setup');
}

// ---- 2. The new-career opening (PS2 run new-career) ----------------------------------------------------------------
const nc = run('new-career');
{
  const { ui, cs, log } = page();
  cs.enter(); const at = log.length;
  assert.ok(cs.resume(), 'the page resumes the pending world load');
  const cut = last(log, 'cutscene');
  assert.deepEqual(cut.slice(0, 4), ['cutscene', 'arrival', 'ABC1', true], 'WS10 first visit of Happiness');
  const queued = nc.queued.map(([, q]) => q[1]).reduce((a, b) => (b.length > a.length ? b : a), []);
  assert.deepEqual(listIds(arrivalSteps('ABC1', true)), queued, `NIS list 1 = ${queued} (movie 29, abc1_heli_arr_midway, heli_arrb_<char>_midwayabc1)`);
  assert.deepEqual(nc.scripts.map(([, s]) => s), [153, 163], 'PS2 played #153 then #163 after the movie');
  assert.equal(since(log, at, 'ride').length, 0, 'the ride waits for the arrival list (WS10 -> WS1..4 after it)');
  await tick(); await tick();
  assert.deepEqual(last(log, 'ride'), ['ride', nc.rides[0][1]], 'free ride at Happiness');
  const me = cs.me, ws4 = careerAt(nc, (c) => c.visited !== '0x0');
  assert.equal(me.firstRun !== false, ws4.new === 1, 'the new-career flag stays set after the drop');
  assert.equal(((me.visited >>> 0) & (1 << 14)) !== 0, bit(ws4.visited, 14), 'Happiness marked visited at WS4');
  assert.equal(ui.screen, 'game');
}

// ---- 3. Quit and come back: the flag still set -> Happiness again, no cutscene; the first crossing clears it (reenter) ----
const re = run('reenter');
{
  const { ui, cs, log, storage } = page();
  cs.enter(); cs.resume(); await tick(); await tick();
  cs.quitToTitle(); assert.equal(ui.screen, 'title', 'the career quits to the title screen (PS2 quit-save-tri)');
  const again = page(storage); again.cs.enter(); const at = again.log.length; again.cs.resume(); await tick();
  assert.deepEqual(again.log[at - 1], ['load', re.rides[0][1], true], 'the same rider starts at Happiness again (new-career flag still set)');
  assert.equal(since(again.log, at, 'cutscene').length, re.scripts.length, 'a visited Happiness plays nothing (PS2 reenter: no script)');
  assert.deepEqual(last(again.log, 'ride'), ['ride', 14]);
  const crossing = re.ws.find(([, w]) => w === 11), after = re.career.find(([s]) => s >= crossing[0])[1];
  again.cs.courseChanged(17);
  assert.equal(again.cs.me.firstRun === false, after.new === 0, 'the first location crossing (WS11 0x236928) clears the flag');
  again.cs.crossingArrived(17);   // pv crossingArrival: world state 10 at the ABC1_A Load trigger (PS2 ctm-parity/mountain: the Load's record)
  assert.equal(again.cs.me.lastStation, 17, 'a station reached is the last lodge (WS10 0x2356FC)');
  // 4. an existing career starts at the last lodge, riding, with no cutscene and no prompt (PS2 start-lodge)
  const sl = run('start-lodge'), third = page(again.storage);
  third.cs.enter(); const at3 = third.log.length; third.cs.resume(); await tick();
  assert.deepEqual(third.log[at3 - 1], ['load', sl.rides[0][1], true], 'the last lodge (+0x27C = 17)');
  assert.equal(since(third.log, at3, 'cutscene').length, 0);
  assert.equal(third.ui.screen, 'game', 'riding from the station top; the lodge prompt only at the door');
  assert.deepEqual(sl.scripts.map(([, s]) => s), [148], 'PS2: lodge_arr3 only once the rider reaches the door');
}

// ---- 5. MCOMM Quit (PS2 quit-ctm, quit-save, quit-save-tri) ----------------------------------------------------------
{
  const { ui, cs, log } = page();
  cs.enter(); cs.resume(); await tick(); await tick();
  ui.set('ctm-mcomm'); cs.choose(6);
  const q = observed('quit'), qs = observed('quit-save');
  assert.equal(ui.screen, 'ctm-quit'); assert.deepEqual(cs.promptLines('ctm-quit'), [q.prompt]);
  assert.deepEqual(cs.items('ctm-quit'), q.items); assert.equal(cs.items('ctm-quit')[ui.index], q.focus, 'Quit Game opens on No');
  cs.choose(0);
  assert.equal(ui.screen, 'ctm-quitsave'); assert.deepEqual(cs.promptLines('ctm-quitsave'), [qs.prompt]);
  assert.equal(cs.items('ctm-quitsave')[ui.index], qs.focus, 'the save question opens on Yes');
  cs.choose(0);
  assert.equal(ui.screen, observed('quit-end').screen, 'then the title screen, not the main menu');
  assert.equal(cs.freeRide, null, 'the career session ends (the next CTM start is a world load)');
  // No on Quit Game goes back to the MCOMM on Quit
  const b = page(); b.cs.enter(); b.cs.resume(); await tick(); await tick(); b.ui.set('ctm-mcomm'); b.cs.choose(6); b.cs.choose(1);
  assert.equal(b.ui.screen, 'ctm-mcomm'); assert.equal(b.ui.index, 6);
}

// ---- 6. Snow Jam: qualifier -> semi -> final -> rewards -> results (PS2 race-q, to-final, race-f, race-f95, after-final) ----
function field(me, others) {   // the human first, computer riders with times (null = still riding / DNF)
  return [{ human: true, finishTicks: me, remaining: 0, place: 0, origin: 1 }, ...others.map((c, i) => ({ character: c, finishTicks: null, remaining: 50000, place: i + 1, origin: 600000, dnf: true }))];
}
async function snowJamEvent({ earned = 0, rounds = [1, 2, 3], storage } = {}) {
  const p = page(storage); const { cs, ui } = p;
  cs.enter(); cs.resume(); await tick(); await tick();
  const r = cs.me; r.earned = earned; r.cash = earned;
  cs.freeRide = { course: 0 };   // the rider rode into the Snow Jam gate (ctm-event.js)
  cs.begin(MODE.RACE, 0, true); cs.career.active.ev.round = rounds[0];
  return p;
}
{
  const p = await snowJamEvent(); const { ui, cs, log } = p;
  const q = observed('qualifier-results');
  cs.finish({ ticks: 13500, raceTicks: 13500, standings: field(13500, [8, 2, 0, 5, 4]) });
  await tick();
  assert.equal(ui.screen, 'ctm-results', 'a qualifier pays nothing: straight to the results (PS2 race-q WS5 -> 7, no overlay)');
  assert.deepEqual(cs.items('ctm-results'), q.items, 'results items (item 0 = Next heat, 0x1E6310)');
  assert.equal(cs.t(cs.result.outcome.message), q.message);
  assert.equal(cs.me.firstRun, false, 'the first results clear the new-career flag (WS7 enter 0x236E10)');
  // Next heat: the gondola ride-up, the semi card (PS2 to-final with the final forced: 150, 146, the start hut 96, 73)
  const tf = run('to-final');
  assert.deepEqual(tf.scripts.map(([, s]) => s), [150, 146, 96, 73]);
  assert.deepEqual(listIds(heatSteps(true)), [19, 20, 4, 5], 'the final heat: gond_inair, gond_inair_<char>, start hut, gate idle');
  assert.deepEqual(tf.queued[0][1], [[4, 5], [19, 20]], 'PS2 WS13 queues list 0 [4, 5] and list 1 [19, 20]');
  cs.choose(0); await tick();
  assert.deepEqual(last(log, 'cutscene').slice(0, 2), ['cutscene', 'heat']);
  // the semi won: item 0 becomes 'Final Round' (0x1E6420 kT_OVRCMNFinalRound when GMM+0x70 == 3)
  cs.finish({ ticks: 13400, raceTicks: 13400, standings: field(13400, [8, 2, 0, 5, 4]) }); await tick();
  assert.equal(cs.items('ctm-results')[0], observed('final-card').sub, "semi won -> 'Final Round'");
}

// ---- 7. The final: podium, the reward list, the results (PS2 race-f / after-final), the earnings goal (race-f95) -------
const shape = (lines) => lines.map((l) => [l.text.replace(/ \(.*\)$/, ''), l.indent]);
{
  const { ui, cs, log } = await snowJamEvent({ rounds: [3] });
  cs.finish({ ticks: 14300, raceTicks: 14300, standings: field(14300, [3, 8, 2, 1, 6]) });
  await tick(); await tick();
  assert.deepEqual(podiumSteps(0).map((s) => s.group), run('race-f').queued[0][1][0], 'the winner podium (list 9, win_ps_<char>)');
  assert.deepEqual(last(log, 'cutscene').slice(0, 2), ['cutscene', 'podium']);
  const rf = observed('final-rewards');
  assert.equal(ui.screen, 'ctm-award', 'a paying result opens the reward list first (20A8F8: overlay 0x10)');
  const lines = cs.rewardLines(), want = shop ? rf.lines : rf.lines.filter(([t]) => t !== 'Accessory');
  assert.deepEqual(shape(lines), want, 'reward list rows (Cash / Gold medal earned / its gear)');
  assert.equal(cs.items('ctm-award').length, lines.length, 'every row is a list row (Up/Down scroll it)');
  cs.choose(0);
  const fr = observed('final-results');
  assert.equal(ui.screen, 'ctm-results'); assert.deepEqual(cs.items('ctm-results'), fr.items);
  const msg = cs.t(['kT_OVRCongratulationsPlatinum', 'kT_OVRCongratulationsGold', 'kT_OVRCongratulationsSilver', 'kT_OVRCongratulationsBronze'][cs.result.outcome.medal]);
  assert.equal(msg, fr.message.replace('Gold', ['Platinum', 'Gold', 'Silver', 'Bronze'][cs.result.outcome.medal]));
  const cash = run('race-f').career; assert.equal(cash.at(-1)[1].cash - cash[0][1].cash, 10000, 'PS2: $10,000 for the gold');
  assert.equal(cs.result.outcome.cash, 10000);
}
{
  const { ui, cs } = await snowJamEvent({ rounds: [3], earned: 95000 });
  cs.finish({ ticks: 14300, raceTicks: 14300, standings: field(14300, [3, 8, 2, 1, 6]) });
  await tick(); await tick();
  const rg = observed('final-rewards-goal'), lines = shape(cs.rewardLines());
  assert.deepEqual(lines.slice(0, rg.lines.length), rg.lines, 'goal award rows: title, the pass, poster, 4 trading cards, then the gold');
  const f95 = run('race-f95').career, before = f95[0][1].locks, after = f95.at(-1)[1].locks;
  assert.ok(bit(before, 12) && !bit(after, 12), 'PS2: the earnings goal clears the Peak 2 lock bit 12');
  assert.equal(cs.me.peaks[1], true, 'Peak 2 pass');
  // 8. Transport -> Select Peak -> Peak 2 -> 'Go to this peak now?' (Yes) -> the DBC2 first arrival (PS2 f95-after, peak2-arr)
  cs.choose(0); cs.choose(0); await tick();
  assert.equal(ui.screen, 'ctm-peaks');
  const gp = observed('go-peak');
  cs.choose(1);
  assert.equal(ui.screen, 'ctm-gopeak'); assert.deepEqual(cs.promptLines('ctm-gopeak'), [gp.prompt]); assert.equal(cs.items('ctm-gopeak')[ui.index], gp.focus);
  cs.choose(0); const at = ui.log.length;
  assert.deepEqual(ui.log.at(-1).slice(0, 2), ['load', 15], 'a world load at Ruthless');
  cs.resume(); await tick(); await tick();
  const fa = run('f95-after'), arrival = fa.queued.map(([, q]) => q[1]).find((q) => q[0] === 30);
  assert.deepEqual(last(ui.log, 'cutscene').slice(0, 4), ['cutscene', 'arrival', 'DBC2', true]);
  assert.deepEqual(listIds(arrivalSteps('DBC2', true)), arrival, `the first arrival list ${arrival}: movie 30, dbc2_heli_arr, heli_arrb_<char>`);
  assert.deepEqual(run('peak2-arr').scripts.map(([, s]) => s), [124, 137]);
  assert.deepEqual(last(ui.log, 'ride'), ['ride', run('peak2-arr').rides[0][1]], 'free ride at Ruthless');
  assert.ok(((cs.me.visited >>> 0) & (1 << 15)) !== 0 && bit(run('peak2-arr').career.at(-1)[1].visited, 15), 'visited +0xACC bit 15');
  assert.deepEqual(fa.scripts.slice(0, 2).map(([, s]) => s), [149, 122], 'PS2 rides the heli (heli_inair, heli_inair_<char>) during the load');
  assert.deepEqual(listIds(transportSteps({ heli: true })), [13, 14]);
  // a later transport into a visited backcountry: the heli drop only (WS10 0x235220 with the transport flag)
  assert.deepEqual(listIds(arrivalSteps('DBC2', false)), [16, 17]);
}

// ---- 9. The lodge: Return to Game and Quit (PS2 lodge-return, lr-tri, lodge-quit) ----------------------------------
{
  const { ui, cs, log } = page();
  cs.enter(); cs.resume(); await tick(); await tick();
  cs.freeRide = { course: 17, station: 17 };
  ui.set('ctm-enterlodge'); cs.choose(0);
  assert.equal(ui.screen, 'ctm-lodge');
  cs.choose(0); const lr = observed('lodge-return');
  assert.equal(ui.screen, 'ctm-saveprompt'); assert.deepEqual(cs.promptLines('ctm-saveprompt'), [lr.prompt]); assert.equal(cs.items('ctm-saveprompt')[ui.index], lr.focus);
  const at = log.length; cs.choose(0);
  assert.deepEqual(log.slice(at).filter((e) => e[0] !== 'screen'), [['resume'], ['respawn', 17]], 'the lodge exit rides from the station top (no transport)');
  assert.equal(ui.screen, 'game', 'no lodge prompt until the rider rides into the door again (PS2 lr-tri: #148 at s1100)');
  assert.deepEqual(run('lr-tri').rides[0][1], 17);
  // Quit: 'Quit to Title screen?' (Yes), then the save question (Yes), then the title
  ui.set('ctm-lodge'); cs.choose(7);
  const lq = observed('lodge-quit'), lqs = observed('lodge-quit-save');
  assert.deepEqual(cs.promptLines('ctm-quit'), [lq.prompt]); assert.equal(cs.items('ctm-quit')[ui.index], lq.focus);
  cs.choose(0); assert.deepEqual(cs.promptLines('ctm-quitsave'), [lqs.prompt]); assert.equal(cs.items('ctm-quitsave')[ui.index], lqs.focus);
  cs.choose(0); assert.equal(ui.screen, 'title');
}

// ---- 9b. The lodge exit as a world load (pv lodgeWorldLoad, main.js ui.cb.freeRideWorldLoad) ---------------------
// With the page's world-load callback the Return to Game is the world start at the station (0x1A11C0 -> 146D98): the
// station goes to the callback and the old in-world path (resume + freeRideRespawn) is not taken. Section 9 above is
// the fallback (a page without the callback).
{
  const { ui, cs, log } = page();
  ui.cb.freeRideWorldLoad = (station) => { log.push(['worldLoad', station]); return true; };
  cs.enter(); cs.resume(); await tick(); await tick();
  for (const station of [17, 19]) {
    cs.freeRide = { course: station, station };
    ui.set('ctm-lodge'); cs.choose(0); assert.equal(ui.screen, 'ctm-saveprompt');
    const at = log.length; cs.choose(0);
    assert.deepEqual(log.slice(at).filter((e) => e[0] !== 'screen'), [['worldLoad', station]], `the lodge exit at ${station} is a world load at that station (no resume, no respawn)`);
    assert.equal(ui.screen, 'game'); assert.equal(cs.me.lastStation, station); assert.equal(cs.freeRide.course, station);
  }
}

// ---- 10. Saves from before the visited mask keep their state -------------------------------------------------------
{
  const { cs } = page(); const r = cs.me; r.firstRun = false; r.arrived = { 15: true };
  assert.equal(cs.visitedMask(r), (1 << 14) | (1 << 15), 'old first-run flag -> Happiness visited, arrived -> Ruthless');
}
// ---- 11. The MCOMM from OV.LUI (tools/export_ctm_screens.py): every row icon of table 0x441C30 has its "%s group" ------
{
  let screens = null; try { screens = JSON.parse(fs.readFileSync(new URL('./public/assets/UI/ctm-screens.json', import.meta.url))); } catch {}
  if (screens) {
    const groups = CtmPda.prototype.iconGroups(screens.screens['31paus_freeride']);
    for (const icon of Object.values(ICON)) assert.ok(groups[icon], `${icon} group`);
    assert.equal(groups.conticon.first, 390, 'Return: focus state from frame 390'); assert.equal(groups.mapicon.first, 490, 'Transport (mapicon): 490');
    const yn = screens.screens['87yndialog'].elements.map((e) => e.label);
    for (const l of ['title_quit', 'title_quitchallenge', 'yes', 'no']) assert.ok(yn.includes(l), `87yndialog ${l}`);
    assert.ok(screens.screens.PDATemplate.elements.some((e) => e.kind === 'text' && /%d/.test(e.text || '')), 'the temperature text %d°%C');
    // the Big Challenge overlays 0x1D / 0x1E (PS2 pc75 s100: Big Challenge / Point Challenge / the description / Accept challenge? / Yes)
    const offer = observed('bc-offer'), bcs = screens.screens['63bc_start'], bcf = screens.screens['90bc_fail'];
    assert.ok(bcs && bcf, '63bc_start / 90bc_fail exported');
    for (const l of ['title', 'chalname', 'ObjText', 'yes', 'no']) assert.ok(bcs.elements.some((e) => e.label === l), `63bc_start ${l}`);
    assert.ok(bcs.elements.some((e) => e.text === offer.question), 'Accept challenge?');
    for (const l of ['yes', 'no', 'info']) assert.ok(bcf.elements.some((e) => e.label === l), `90bc_fail ${l}`);
    assert.deepEqual(bcs.labels.map((x) => x.frame).filter((f) => f >= 60 && f <= 70), [60, 65, 70], 'rest 60, Yes 65, No 70');
  } else console.log('test-ctm-flow: ctm-screens.json not exported (python3 tools/export_ctm_screens.py), MCOMM checks skipped');
}
// ---- 12. Collectibles and the Freeride goal (PS2 explore-info: 41 Happiness collectibles, 22 Peak 1 challenges) --------
{
  const { setBigChallengeTable, bigChallengeInfo, challengeStatus, statusWords } = await import('./big-challenges.js');
  const { exploreNext, mountainPercent } = await import('./career.js');
  const { COLLECTIBLE_TOTALS } = await import('./free-ride-hud.js');
  const { format } = await import('./locale.js');
  const table = JSON.parse(fs.readFileSync(new URL('./public/assets/BIGCHAL/big-challenges.json', import.meta.url))).challenges;
  setBigChallengeTable(table);
  const mem = ps2.memory['explore-info-start'];
  const { cs } = page(); const c = cs.career, id = 'zoe';
  // the poked profile: Happiness row (count byte, 64-bit mask) and the 22 completed status words
  const [n14, lo, hi] = mem.collect['14'];
  for (let i = 0; i < 64; i++) if ((((i < 32 ? lo : hi) >>> (i & 31)) & 1)) c.markCollected(id, 14, i, 0);
  assert.equal(c.collectCount(id, 14), n14, 'Happiness 41 collected');
  const words = statusWords(c, id); mem.status.forEach((w, i) => { words[i] = w >>> 0; });
  const peakGoals = observed('explore-peak-goals'), all = observed('explore-all-mountain'), goal = observed('explore-freeride-goal'), list = observed('explore-freeride-list');
  // Peak 1 Goals: Freeride n / 2 = the two medals (157BF0 goal 2), ticked when complete
  const medals = (c.collectMedal(id, 1) !== MEDAL.NONE) + (c.challengeMedal(id, 1) !== MEDAL.NONE);
  assert.deepEqual(['Freeride', `${medals} / 2`, c.goalComplete(id, 1, 'freeride')], peakGoals.rows[2]);
  assert.deepEqual(observed('explore-goal-list').ticked, ['race', 'freestyle', 'freeride', 'earnings'].map((g) => c.goalComplete(id, 1, g)));
  // All Mountain: 6% from 22 / 88 challenges and 41 / 425 collectibles (0x204D40)
  const bc = [1, 2, 3].reduce((n, p) => n + c.challengesDone(id, p), 0), bcAll = [1, 2, 3].reduce((n, p) => n + c.peakChallengeTotal(p), 0);
  const got = [1, 2, 3].reduce((n, p) => n + c.peakCollected(id, p), 0), gotAll = [1, 2, 3].reduce((n, p) => n + c.peakCollectTotal(p), 0);
  assert.equal(`${mountainPercent({ golds: 0, rivals: 0, challenges: bc, challengeTotal: bcAll, collected: got, collectTotal: gotAll, highlights: 0 })}%`, all.percent);
  assert.deepEqual([`${bc} / ${bcAll}`, `${got} / ${gotAll}`], [all.rows[2][1], all.rows[3][1]]);
  // Peak 1 Freeride Goals: "Challenges Complete: 22/40" / "Gold medal at 32", "Collectibles: 41/155" / "Silver Medal at 70"
  const t = (k) => cs.t(k);
  const rows = [[c.challengesDone(id, 1), c.peakChallengeTotal(1), c.rules.challenge_medals[0], 'kT_CMNChalCompNum'], [c.peakCollected(id, 1), c.peakCollectTotal(1), c.rules.collectible_medals[0], 'kT_CMNColllectNum']]
    .map(([n, total, row, key]) => { const next = exploreNext(n, row); return [format(t(key), n, total), next ? format(t(next.key), next.at) : null, n >= row[3]]; });
  assert.deepEqual(rows, goal.rows);
  assert.equal(format(t('kT_CMNPeakFRGoals'), 1), goal.title); assert.equal(format(t('kT_CMNNumComplete'), medals, 2), goal.complete);
  // the freeride list INFO: Collectibles n / N (0x43FA70) and Big Challenges n / N or N/A for every location
  const byName = { 'Happiness': 14, 'Green Station': 17, 'R&B': 5 };
  for (const e of list.entries) {
    const course = byName[e.name];
    assert.equal(`${c.collectCount(id, course)} / ${COLLECTIBLE_TOTALS[course]}`, e.collectibles, `${e.name} collectibles`);
    assert.equal(bigChallengeInfo(c, id, course), e.challenges, `${e.name} Big Challenges`);
    assert.equal(data.courses[course].name, e.run);
  }
}
// ---- 13. The Freeride goal completed in the world (PS2 sd-goal: Speed Demon is the 12th challenge, 40 collectibles) -------
{
  const { challengeStatus, statusWords } = await import('./big-challenges.js');
  const before = ps2.memory['sd-goal-start'], after = ps2.memory['sd-goal-end'];
  const { cs } = page(); const c = cs.career, id = 'zoe';
  cs.messages.data = JSON.parse(fs.readFileSync(new URL('./public/assets/CAREER/messages.json', import.meta.url))); cs.messages.hook(c);
  const [, lo, hi] = before.collect['14'];
  for (let i = 0; i < 64; i++) if ((((i < 32 ? lo : hi) >>> (i & 31)) & 1)) c.markCollected(id, 14, i, 0);
  const words = statusWords(c, id); before.status.forEach((w, i) => { words[i] = w >>> 0; });
  assert.ok(!c.goalComplete(id, 1, 'freeride') && !c.me?.peaks?.[1]);
  assert.equal(c.challengeMedal(id, 1), MEDAL.NONE, '11 challenges: no Big Challenge medal yet');
  challengeStatus(c, id, 20, after.status[20]);   // 307308: Speed Demon 0x12 -> 0x1A
  assert.equal(c.challengeMedal(id, 1), MEDAL.BRONZE, 'the 12th challenge: bronze (0x45B018)');
  assert.ok(c.goalComplete(id, 1, 'freeride'), 'the Freeride goal completes (157BF0 goal 2)');
  const r = c.rider(id);
  assert.equal(r.peaks[1], !bit(after.locks, 12), 'the Peak 2 pass (+0x278 bit 12 cleared)');
  assert.ok(r.awards.includes(11) && parseInt(after.awards.slice(2, 4), 16) === 0x08, 'award 11 (+0xF28 bit 11)');
  assert.deepEqual(r.messages.entries.map((e) => e[0]), after.inbox.map((e) => e[0]), 'message 249 posted (the mail icon)');
  assert.equal(observed('bc-success-goal').reward_list, false);
  // the collectible side: the 40th collectible with the Big Challenge medal already there completes the goal the same way
  const other = page(); const c2 = other.cs.career;
  const w2 = statusWords(c2, id); after.status.forEach((w, i) => { w2[i] = w >>> 0; });
  for (let i = 0; i < 39; i++) c2.markCollected(id, 14, i, 0);
  assert.ok(!c2.goalComplete(id, 1, 'freeride'));
  c2.markCollected(id, 14, 39, 500);   // 30B9A0 -> 10F338 -> 1599A0
  assert.ok(c2.goalComplete(id, 1, 'freeride') && c2.rider(id).peaks[1] && c2.rider(id).awards.includes(11), 'collectible bronze completes the goal');
}
// ---- 14. A page reload during a career (pv careerReload) = the PS2 re-entry (runs reenter / start-lodge): the new-career flag set ->
// Happiness again (no cut: visited), cleared -> the last lodge; the career runs (before: a plain run, ui.careerMode false) --------------
{
  const prevLoc = globalThis.location, prevSS = globalThis.sessionStorage;
  globalThis.sessionStorage = new Memory();
  for (const [flagSet, lodge, want] of [[true, 17, 14], [false, 18, 18]]) {
    const storage = new Memory();
    globalThis.location = { href: 'http://localhost/?course=PEAK1&peakCourse=17&rider=zoe&autostart=1' };
    { const { cs } = page(storage); const r = cs.career.rider('zoe'); r.firstRun = flagSet ? undefined : false; r.lastStation = lodge; r.visited = (1 << 14) | (1 << 17) | (1 << 18); cs.career.persist();
      cs.enterWorld(flagSet ? 14 : lodge); }   // the career's world: the tab is marked
    const { ui, cs, log } = page(storage); ui.careerMode = false; ui.course = { code: 'PEAK1', freeRide: {} };
    assert.ok(cs.resume(), 'a reload of the marked career URL re-enters the career');
    assert.equal(ui.careerMode, true);
    assert.deepEqual(last(log, 'load'), ['load', want, true], flagSet ? 'the new-career flag set: Happiness (145C38 -> 14), as PS2 reenter' : 'the last lodge (146D98), as PS2 start-lodge');
    // another URL (the tab left the career for a menu / another course): no re-entry
    globalThis.location = { href: 'http://localhost/?course=ARA1&autostart=1&rider=zoe' };
    const other = page(storage); other.ui.careerMode = false; delete other.cs.career.save.pending;   // (the stub page never ran the world load)
    assert.equal(other.cs.resume(), false, 'a reload of another URL is not a career re-entry');
  }
  globalThis.location = prevLoc; globalThis.sessionStorage = prevSS;
  console.log('reload: a marked career URL re-enters at Happiness (flag set) or the last lodge, with the career');
}
// ---- 15. Transport lists and help (pv transportLists): the row tables 0x4781D0 / 0x4786E0, 0x207430's help ------------------------
{
  const { ui, cs } = page(); const id = 'zoe'; const r = cs.career.rider(id); r.peaks = [true, true, true];
  const names = (peak, goal) => { cs.peak = peak; cs.goal = goal; return cs.list().map((e) => [e.course, e.mode]); };
  const elf = new URL('../local/disc/SLUS_207.72', import.meta.url);
  const want = { race: [], freestyle: [] };
  if (fs.existsSync(elf)) {   // the tables themselves: {course, ?, mode} rows of 0x6C
    const b = fs.readFileSync(elf), row = (a, k) => [b.readInt32LE(a - 0xFF000 + k * 0x6C), b.readInt32LE(a - 0xFF000 + k * 0x6C + 8)];
    for (const [goal, base, per] of [['race', 0x4781D0, [4, 4, 4]], ['freestyle', 0x4786E0, [5, 5, 5]]]) { let k = 0; for (const n of per) { want[goal].push(Array.from({ length: n }, () => row(base, k++)).filter(([c]) => c !== 23)); } }
  } else { want.race = [[[0, 0], [1, 0], [14, 4], [14, 6]], [[2, 0], [3, 0], [15, 4], [15, 7]], [[4, 0], [16, 4], [16, 8]]]; want.freestyle = [[[5, 1], [8, 3], [11, 2], [14, 5], [14, 9]], [[6, 1], [9, 3], [12, 2], [15, 5], [15, 10]], [[7, 1], [10, 3], [13, 2], [16, 5], [16, 11]]]; }
  for (const goal of ['race', 'freestyle']) for (let p = 1; p <= 3; p++) assert.deepEqual(names(p, goal).map(([c, m]) => [m >= 6 ? [14, 15, 16][(m - 6) % 3] : c, m]), want[goal][p - 1], `peak ${p} ${goal}: the PS2 row table order`);
  cs.peak = 2; cs.goal = 'freestyle'; assert.deepEqual(cs.items('ctm-events').slice(0, 4), ['Style Mile', 'Launch Time', 'Schizophrenia', 'Ruthless Jam']);
  cs.peak = 3; assert.equal(cs.items('ctm-events')[3], 'Throne Jam', 'the row\'s inline name (PS2 allpeak out-apj-card fs-list)'); cs.peak = 2;
  // help: a locked peak Jam -> "Complete Ruthless Jam to unlock."; an open rival row -> kT_HELPChalAvail
  const jam = cs.list().findIndex((e) => e.mode === 10);
  assert.equal(cs.helpText('ctm-events', jam), cs.t('kT_HELPLockCompEvent').replace('%s', 'Ruthless Jam'));
  for (const e of cs.career.goalEvents(id, 2, 'freestyle')) if (e.mode < 4) r.medals[`${e.mode}:${e.course}`] = MEDAL.GOLD;
  const rivalRow = cs.list().findIndex((e) => e.mode === 5);
  assert.equal(cs.helpText('ctm-events', rivalRow), 'A battle against your rival.');
  console.log('transport lists: the Race / Freestyle rows in the PS2 table order; the rival / peak help lines');
}
// ---- 16. Stations and heats (pv stationFlow): the lodge prompt, the booth, the post-event map, the heli drop, WS13 by event type -----
{
  // the lodge prompt: Triangle = No (1F72E0); pv doorNoPlace: No places the rider at the station's session point 0 (PS2 ctm-parity/door:
  // tick 636, (-65782, 33404) at Green, forward x 833 cm/s)
  { const { ui, cs, log } = page(); cs.freeRide = { course: 17, station: 17 }; ui.set('ctm-enterlodge'); cs.back();
    assert.equal(ui.screen, 'game'); assert.deepEqual(last(log, 'respawn'), ['respawn', 17], 'No: the session point 0 placement'); }
  // a station row asks the Transport question (PS2 peak3/nav out-fr-to-black-station prompt)
  { const { cs } = page(); cs.selected = { station: true, course: 21 }; assert.deepEqual(cs.promptLines('ctm-confirm'), [cs.t('kT_MAPTransArea', 'Transport to this area now?')]); }
  // the booth: the map on the ridden peak; Back = world state 15 at the station (session point 1)
  { const { ui, cs, log } = page(); ui.cb.freeRideSession = (p) => log.push(['session', p]); cs.freeRide = { course: 19 }; cs.openBooth(19);
    assert.equal(ui.screen, 'ctm-peaks'); assert.equal(ui.index, 3 - 2, 'Select Peak on Peak 2 (the ridden peak)');
    cs.back(); assert.equal(ui.screen, 'game'); assert.deepEqual(last(log, 'session'), ['session', 1]); }
  // the post-event map (mode 4): Back is ignored (0x2022A4)
  { const { ui, cs } = page(); cs.afterEvent = true; ui.set('ctm-peaks'); cs.back(); assert.equal(ui.screen, 'ctm-peaks'); }
  // a transport across a world switch into a visited backcountry: the heli drop [16, 17] (0x235220 with the transport flag); a world
  // load there (the career start) plays nothing
  { const { cs, log } = page(); const r = cs.career.rider('zoe'); r.firstRun = false; r.visited = (1 << 14) | (1 << 15) | (1 << 17);
    cs.freeRide = { course: 17 }; cs.goWorld(15); assert.ok(cs.career.save.pending.transport); cs.resume(); await tick();
    assert.deepEqual(last(log, 'cutscene').slice(0, 4), ['cutscene', 'arrival', 'DBC2', false], 'the heli drop at Ruthless');
    const w = page(); const r2 = w.cs.career.rider('zoe'); r2.firstRun = false; r2.visited = (1 << 15); w.cs.goWorld(15, { reload: true }); w.cs.resume(); await tick();
    assert.equal(last(w.log, 'cutscene'), undefined, 'a world load at a visited backcountry: nothing'); }
  // WS13 (the results' Restart / next heat) by event type: races ride the gondola, freestyle queues its gate lists alone, rival nothing
  { const { ui, cs, log } = page(); cs.career.startEvent('zoe', MODE.HALFPIPE, 11, true); cs.result = { outcome: { round: 1 }, mode: MODE.HALFPIPE }; cs.restartToCard(true); await tick();
    assert.deepEqual(last(log, 'cutscene').slice(0, 2), ['cutscene', 'restart'], 'freestyle: no gondola (27A860 for event type 0 only)'); }
  { const { ui, cs, log } = page(); cs.career.startEvent('zoe', MODE.RIVAL_TIME, 14, true); cs.result = { outcome: { round: 1 }, mode: MODE.RIVAL_TIME }; cs.restartToCard(true); await tick();
    assert.equal(last(log, 'cutscene'), undefined, 'a rival event queues nothing (0x235B38)'); assert.equal(ui.screen, 'ctm-objectives'); }
  // pv ws13Rival: a peak run (event type 5 / 6 like the rival challenges) takes the same branch: no cutscene, the card
  { const { ui, cs, log } = page(); cs.career.startEvent('zoe', 6, null, true); cs.result = { outcome: { round: 1 }, mode: 6 }; cs.restartToCard(true); await tick();
    assert.equal(last(log, 'cutscene'), undefined, 'a peak run queues nothing (0x235B38)'); assert.equal(ui.screen, 'ctm-objectives'); }
  { const { ui, cs, log } = page(); cs.career.startEvent('zoe', MODE.RACE, 0, true); cs.result = { outcome: { round: 1 }, mode: MODE.RACE }; cs.restartToCard(true); await tick();
    assert.deepEqual(last(log, 'cutscene').slice(0, 2), ['cutscene', 'heat'], 'a race rides the gondola'); }
  // pv crossWorld: riding out of Peak 2 into Green Base Station: the crossing clears the new-career flag, a world load (no ride) at 17
  { const { cs, log } = page(); const r = cs.career.rider('zoe'); r.firstRun = undefined; r.lastStation = 20;
    cs.freeRide = { course: 3 }; cs.crossWorld(17); assert.equal(r.firstRun, false); assert.deepEqual(last(log, 'load'), ['load', 17, true]);
    cs.resume(); await tick(); assert.equal(r.lastStation, 17, 'the last lodge at the station reached (WS10)'); }
  console.log('stations: lodge Triangle = No at the door, the booth on the ridden peak and its Back, the post-event map, the heli drop');
}
// ---- 17. pv mountainRide: the free ride in the whole-mountain world (the PS2's one world) -------------------------------------
{
  {
    assert.equal(freeRideWorldOf(3), 'MOUNTAIN', 'desktop tier: the whole mountain');
    const { ui, cs, log } = page(); ui.course = { code: 'MOUNTAIN', freeRide: { kind: 4 } };
    assert.equal(switchesWorld(ui, 17), false, 'every course is in the loaded world: a transport inside it, not a world switch');
    // riding out of Intimidator into Green Base Station (22DF50: WS11 -> WS10 at A): the career follows the rider's location
    const r = cs.career.rider('zoe'); r.firstRun = false; r.lastStation = 20; r.visited = (1 << 14) | (1 << 15) | (1 << 3);
    cs.freeRide = { course: 3 }; cs.peak = 2; cs.courseChanged(17);
    assert.equal(cs.peak, 1, 'the MCOMM Transport opens on the peak being ridden');
    cs.crossingArrived(17);   // the DRA4_A Load (pv crossingArrival: WS10 there)
    assert.equal(r.lastStation, 17, 'the last lodge (146E10 at 0x2356FC)'); assert.ok(r.visited & (1 << 17), 'A visited (+0xACC)');
    assert.equal(cs.freeRide.course, 17);
    // Gravitude into Yellow Mid Station: Peak 2
    cs.freeRide = { course: 4 }; cs.peak = 3; cs.courseChanged(19); assert.equal(cs.peak, 2); cs.crossingArrived(19); assert.equal(r.lastStation, 19);
  }
  // pv crossingArrival (PS2 ctm-parity/mountain/fr-dra4a): a riding crossing's Unload is world state 11 (the new-career flag, the course);
  // the last lodge and the visited bit wait for the connector's Load trigger (WS10 at record 19215, WS4 at 19216)
  {
    { const { cs } = page(); const r = cs.career.rider('zoe'); r.firstRun = undefined; r.lastStation = 20; r.visited = (1 << 3) | (1 << 14);
      cs.freeRide = { course: 3 }; cs.courseChanged(17);
      assert.equal(r.firstRun, false, 'WS11 enter clears the new-career flag at the Unload'); assert.equal(cs.freeRide.course, 17);
      assert.equal(r.lastStation, 20, 'no last lodge before the Load'); assert.ok(!(r.visited & (1 << 17)), 'not visited before the Load');
      cs.crossingArrived(18); assert.equal(r.lastStation, 20, 'another row\'s Load: nothing');
      cs.crossingArrived(17); assert.equal(r.lastStation, 17, 'WS10 at the Load: the last lodge'); assert.ok(r.visited & (1 << 17), 'WS4: visited');
      // a Load met without its Unload (the rider placed past it): the Load's event comes before the course change: WS10 at the change
      { const { cs: c2 } = page(); const r3 = c2.career.rider('zoe'); r3.firstRun = false; r3.lastStation = 20; c2.freeRide = { course: 3 };
        c2.crossingArrived(17); c2.courseChanged(17); assert.equal(r3.lastStation, 17, 'the Load before the course change'); }
      // a transport's arrival is world state 10 at once
      const w = page(); w.ui.cb.freeRide = () => 'transport'; const r2 = w.cs.career.rider('zoe'); r2.firstRun = false; r2.lastStation = 17;
      w.cs.freeRide = { course: 0 }; w.cs.goWorld(18); w.cs.courseChanged(18); assert.equal(r2.lastStation, 18, 'a transport arrives at once');
    } }
  console.log('mountainRide: the whole mountain on the desktop tier, crossings keep the career on the rider\'s location');
}
// ---- ps2MenuInput through the screen phases (web/screen-phases.js): the race pause's Restart -> No, counted in UI frames (PS2
// caps/yno81..87: the pause's input at +83, +81 dead), and the MCOMM Quit -> Yes -> the save question the same way ----------------------
{
  {
    const { ui, cs } = page(); ui.set('game'); ui.set('ctm-pause'); cs.go('ctm-restart', 1);
    for (let k = 0; k < 40; k++) ui.phases.step();
    const f0 = ui.phases.frame; cs.choose(1);
    assert.equal(ui.screen, 'ctm-pause', 'No: back on the pause'); assert.equal(ui.phases.frame - f0, 21, 'No acts on the pass after its Stop (outro 20)');
    assert.equal(ui.index, 1, 'on Restart');
    let n = ui.phases.frame - f0; while (!ui.phases.accepts()) { ui.phases.step(); n++; }
    assert.equal(n, 83, 'the pause takes input at +83');
    const q = page(); q.cs.freeRide = { course: 17 }; q.ui.set('ctm-mcomm'); q.cs.quitFrom = 'ctm-mcomm'; q.ui.set('ctm-quit');
    for (let k = 0; k < 31; k++) q.ui.phases.step();
    const g0 = q.ui.phases.frame; q.cs.choose(0);
    assert.equal(q.ui.screen, 'ctm-quitsave', 'Yes -> the save question'); assert.equal(q.ui.phases.frame - g0, 21);
  }
  { // the card takes Continue from its phase 5 only (menu-rules.js ctm-objectives activate 30), from any input path
    {
      const { ui, cs, log } = page(); cs.career.startEvent('zoe', MODE.RIVAL_TIME, 14, true); cs.result = { outcome: { round: 1 }, mode: MODE.RIVAL_TIME }; cs.restartToCard(true); await tick();
      assert.equal(ui.screen, 'ctm-objectives'); assert.equal(ui.contexts.top, 1, 'the card holds context 1');
      for (let k = 0; k < 30; k++) ui.phases.step(); cs.choose(0); assert.equal(ui.screen, 'ctm-objectives', 'Continue at +30: dead');
      ui.phases.step(); cs.choose(0); assert.equal(ui.screen, 'game', 'Continue at +31: the ride'); assert.equal(ui.contexts.top, 0, 'and its context goes');
      assert.deepEqual(last(log, 'ride'), ['ride', cs.freeRide?.course]);
    }
  }
  console.log('ps2MenuInput: Yes / No outros and the pause intro stepped in UI frames');
}
console.log('test-ctm-flow: CTM flow matches the PS2 trace (%d runs, %d observed screens)', Object.keys(ps2.runs).length, ps2.observed.length);
