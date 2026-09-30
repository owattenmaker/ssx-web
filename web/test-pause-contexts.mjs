// Pause contexts and screen phases, stepped frame by frame (docs/pause-contexts.md; web/pause-contexts.js, web/screen-phases.js).
// The PS2 numbers are the ARMSX2 captures of docs/ctm-decomp-screens.md E8..E11: the pause stops the game tick on its open frame and
// resumes it the frame after the closing press (caps/pause-open.json), each screen's first accepted press (MCOMM +60 dead / +62 taken,
// Yes / No +23 / +31, Map +23 / +31, Rider Details +32 / +40), and Yes / No No -> the pause's input at +83 (+81 dead, caps/yno81..87).
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { setPv } from './pv-flags.js';
import { createPauseContexts, CTX, BIT } from './pause-contexts.js';
import { createScreenPhases, PHASE } from './screen-phases.js';
import { createBigChallenges, setBigChallengeTable } from './big-challenges.js';

setPv('ps2MenuInput', true);

// A page in the order of web/main.js: the UI pass (the pad menus' rAF loop: phases.step, then the keys) comes before the game frame
// (Start's pause gate 0x230A74, then the tick if the stack lets the simulation run).
function page() {
  const warnings = [], contexts = createPauseContexts({ warn: (m) => warnings.push(m) });
  const phases = createScreenPhases({ contexts: () => contexts });
  const ui = {
    screen: 'game', previousScreen: null, contexts, phases, index: 0,
    set(s) { const prev = this.screen; this.previousScreen = prev; this.screen = s; if (s !== prev) phases.enter(s, prev); }, sync() {},
  };
  let menu = null, ticks = 0, frame = 0; const keys = new Map(), game = new Map(), log = [];
  const open = (screen) => { menu ??= contexts.push(CTX.MENU, { owner: 'pause menu', audio: true, ends: (s) => s === 'game' }); ui.set(screen); };
  const close = () => { contexts.pop(menu); menu = null; ui.set('game'); };
  const press = (at, f) => { keys.set(at, f); };   // a menu key in frame `at`'s UI pass
  const start = (at, f) => { game.set(at, f); };   // the game frame's own Start (0x230A74), before that frame's tick
  function step() {
    frame++;
    phases.step();
    const k = keys.get(frame); if (k) log.push([frame, ui.screen, phases.accepts() ? (k(), 'taken') : 'dead']);
    game.get(frame)?.();
    if (!contexts.simFrame()) ticks++;
    return ticks;
  }
  return { ui, contexts, phases, open, close, press, start, step, log, warnings, get frame() { return frame; }, get ticks() { return ticks; } };
}
const run = (p, n) => { for (let k = 0; k < n; k++) p.step(); };

// ---- E11: the tick stops on the pause's open frame, stays stopped under its sub-screens, resumes the frame after the closing press ----
{
  const p = page(); run(p, 9); const before = p.ticks, openAt = 10;
  p.start(openAt, () => p.open('ctm-mcomm'));             // Start seen by frame 10's game frame
  p.step(); assert.equal(p.ticks, before, 'no tick on the open frame');
  p.press(openAt + 62, () => p.ui.set('ctm-peaks'));      // Map (MCOMM +62 taken)
  run(p, 62); assert.equal(p.ui.screen, 'ctm-peaks'); assert.equal(p.ticks, before, 'the world stays frozen under the Map');
  const back = p.frame + 31; p.press(back, () => p.ui.set('ctm-mcomm'));   // Triangle on the Map at +31
  run(p, 31); assert.equal(p.ui.screen, 'ctm-mcomm');
  const ret = p.frame + 62; p.press(ret, () => p.close());  // Return, after the MCOMM replayed its intro (E10)
  run(p, 62); assert.equal(p.ui.screen, 'game'); assert.equal(p.ticks, before, 'no tick on the frame the closing press is accepted');
  p.step(); assert.equal(p.ticks, before + 1, 'the tick resumes the frame after');
  assert.ok(p.log.every(([, , r]) => r === 'taken'), JSON.stringify(p.log));
  assert.equal(p.contexts.audioPaused, false, 'the audio pause went with the menu');
}

// ---- E8: each measured screen's first accepted press, counted from the press that opened it ---------------------------------------
{
  const first = (screen, previous, offset = 0) => {
    const ph = createScreenPhases(); let f = 0; const out = [];
    for (let k = 0; k < offset; k++) { ph.step(); f++; }
    ph.enter(screen, previous);
    for (let d = offset; d <= 90; d++) { if (d > offset) ph.step(); out[d] = ph.accepts(); }
    return out;
  };
  const m = first('ctm-mcomm', 'game');
  assert.equal(m[60], false, 'MCOMM +60 dead'); assert.equal(m[62], true, 'MCOMM +62 taken');
  assert.equal(first('ctm-pause', 'game')[60], false, 'the race pause opens like the MCOMM');
  for (const s of ['ctm-quit', 'ctm-giveup', 'ctm-restart']) { const y = first(s, 'ctm-pause'); assert.equal(y[23], false, s + ' +23 dead'); assert.equal(y[31], true, s + ' +31 taken'); }
  const map = first('ctm-peaks', 'ctm-mcomm'); assert.equal(map[23], false, 'Map +23 dead'); assert.equal(map[31], true, 'Map +31 taken');
  assert.equal(first('ctm-goals', 'ctm-peaks')[0], true, 'the Map\'s levels are one screen: no lock');
  // Rider Details: ui.set runs at the lodge flash's switch, about Cross + 11 (web/menu-rules.js)
  const det = first('ctm-details', 'ctm-lodge', 11); assert.equal(det[32], false, 'Rider Details +32 dead'); assert.equal(det[38], true, 'Rider Details +38 taken'); assert.equal(det[40], true, '+40 taken');
  const ph = createScreenPhases(); ph.enter('ctm-mcomm', 'game');
  const phases = []; for (let d = 0; d <= 61; d++) { if (d) ph.step(); phases[d] = ph.phase; }
  assert.deepEqual([phases[0], phases[1], phases[59], phases[60], phases[61]], [PHASE.BUILD, PHASE.WAIT, PHASE.WAIT, PHASE.SHOW, PHASE.ACTIVE], 'phases 2 -> 4 -> 3 -> 5');
  setPv('ps2MenuInput', false); const off = createScreenPhases({ rules: () => false }); off.enter('ctm-mcomm', 'game'); assert.equal(off.accepts(), true, 'pv ps2MenuInput off: no lock'); setPv('ps2MenuInput', true);
}

// ---- E9: Yes / No No -> the pause: 20 frames of TransitionOut, the pop, the restart a pass later, then the pause's own intro -------
{
  const p = page(); p.open('ctm-pause'); p.ui.set('ctm-restart');
  run(p, 40); assert.ok(p.phases.accepts());
  const at = p.frame; assert.equal(p.phases.leave('ctm-restart', () => p.ui.set('ctm-pause')), true, 'No plays the outro');
  assert.equal(p.phases.phase, PHASE.EXIT); assert.equal(p.phases.accepts(), false, 'phase 6: no input');
  assert.equal(p.phases.leave('ctm-restart', () => assert.fail('a second choice')), true, 'a press during the outro is swallowed');
  run(p, 20); assert.equal(p.phases.phase, PHASE.STOP); assert.equal(p.ui.screen, 'ctm-restart', 'Stop at +20: still the popup');
  p.step(); assert.equal(p.ui.screen, 'ctm-pause', 'popped on the next pass');
  const acc = []; while (p.frame < at + 84) { p.step(); acc[p.frame - at] = p.phases.accepts(); }
  assert.equal(acc[81], false, '+81 dead'); assert.equal(acc[82], false, '+82 dead'); assert.equal(acc[83], true, 'pause input at +83');
  assert.equal(p.contexts.top, CTX.MENU, 'the popup pushed nothing: the pause\'s context 2 stayed underneath');
  // an exit is cut when something else changes the screen meanwhile (the action does not run)
  p.ui.set('ctm-giveup'); run(p, 40); let ran = false; p.phases.leave('ctm-giveup', () => { ran = true; }); p.ui.set('game'); run(p, 30); assert.equal(ran, false);
  // pv ps2MenuInput off: no outro, the choice acts on the press
  const q = createScreenPhases({ rules: () => false }); q.enter('ctm-restart', 'ctm-pause'); assert.equal(q.leave('ctm-restart', () => {}), false);
}

// ---- the card: context 1 for its lifetime stops the NIS tick (and the simulation), not the world-state tick; the fade keeps its clock ----
{
  const p = page(); p.ui.set('cutscene');
  assert.equal(p.contexts.stops(BIT.NIS), false, 'the gate idle plays');
  p.ui.set('ctm-objectives');
  assert.equal(p.contexts.top, CTX.CARD); assert.equal(p.contexts.stops(BIT.NIS), true, 'the card holds the NIS');
  assert.equal(p.contexts.stops(BIT.SIM), true); assert.equal(p.contexts.stops(BIT.WORLD), false, 'mask 0xFFFFFFDB: the world-state tick runs');
  assert.equal(p.contexts.audioPaused, false, 'the card keeps the audio');
  p.ui.set('game'); assert.equal(p.contexts.top, 0, 'Continue pops it'); assert.equal(p.contexts.stops(BIT.NIS), false);
  // web/cutscenes.js update: the NIS clock reads host.nisStopped(); the fade advances its own clock (seq.fadeT) meanwhile
  const cs = fs.readFileSync(new URL('cutscenes.js', import.meta.url), 'utf8');
  assert.match(cs, /const nisStopped = !!host\.nisStopped\?\.\(\);/);
  assert.match(cs, /else if \(s\.heldSeq === seq\) \{ seq\.fadeT = \(seq\.fadeT \?\? seq\.t\) \+ dt \* TICK_HZ; return lastPose; \}/, 'held: only the fade clock runs');
  assert.match(cs, /const adv = stopTick \? Math\.floor\(seq\.t\) \+ 1 - seq\.t : dt \* TICK_HZ;/, 'the push\'s tick runs to the next whole tick (the idle: t = 1.0)');
  assert.doesNotMatch(cs, /idleFrozen/);
  const main = fs.readFileSync(new URL('main.js', import.meta.url), 'utf8');
  assert.match(main, /nisStopped:\(\)=>contexts\.stops\(CTX_BIT\.NIS\)/, 'main.js gives the cutscenes the NIS bit');
}

// ---- Big Challenge prompt: context 3 without the audio pause; the pause menu's context 2 with it --------------------------------------
{
  setBigChallengeTable([{ id: 7, misc: 0, flags: 0 }]);
  const heap = new ArrayBuffer(1 << 16), I = new Int32Array(heap); let offer = [0, 0, 0]; const prompts = [];
  const base = { HEAPU8: new Uint8Array(heap), HEAPF32: new Float32Array(heap), _mission_hud: () => 0, _mission_offer_peek: () => { I.set(offer, 256); return 1024; },
    _mission_offer_pop: () => { const id = offer[1]; offer = [0, 0, 0]; return id; }, _mission_ui_events: () => 2048, _mission_prompt: (k, id) => prompts.push([k, id]) };
  const core = new Proxy(base, { get: (t, k) => (k in t ? t[k] : typeof k === 'string' && k.startsWith('_') ? () => 0 : undefined) });
  const contexts = createPauseContexts(), audio = []; contexts.onChange(({ audio: a, audioChanged }) => { if (audioChanged) audio.push(a); });
  let resumed = 0; const ui = { screen: 'game', contexts, set(s) { this.screen = s; }, sync() {}, cb: { resume: () => { resumed++; } } };
  const bc = await createBigChallenges({ core, ui, careerUI: () => null });
  offer = [8, 7, 0]; bc.tick();
  assert.equal(ui.screen, 'ctm-bcstart'); assert.equal(contexts.top, CTX.PROMPT, 'the offer pushes context 3');
  assert.equal(contexts.stops(BIT.SIM), true, 'the ride stops'); assert.equal(contexts.audioPaused, false, 'the music and SFX play on (no 289B70)'); assert.deepEqual(audio, []);
  bc.choose(1); assert.equal(ui.screen, 'game'); assert.equal(contexts.top, 0, 'No pops it'); assert.deepEqual(prompts.at(-1), [2, 7]); assert.equal(resumed, 1);
  const menu = contexts.push(CTX.MENU, { owner: 'pause menu', audio: true }); assert.equal(contexts.audioPaused, true, 'the pause menu pauses the audio'); contexts.pop(menu); assert.deepEqual(audio, [true, false]);
}

// ---- an MCOMM Transport: its context pops at the Yes and the world ticks through the ride (PS2 to-c-x 921 -> 1471) --------------------
{
  const p = page(); p.open('ctm-mcomm'); run(p, 70); const t0 = p.ticks;
  p.ui.set('ctm-peaks'); run(p, 40); p.ui.set('ctm-goals'); p.ui.set('ctm-events'); p.ui.set('ctm-confirm');
  p.close();   // main.js transportInWorld: overlay.close() on the held (NIS) path, then ui.set('game') and the ride
  run(p, 3); assert.equal(p.ticks, t0 + 2, 'the world ticks from the frame after the Yes');
  const main = fs.readFileSync(new URL('main.js', import.meta.url), 'utf8');
  assert.match(main, /try\{if\(held\)\{overlay\.close\(\);/, 'transportInWorld pops the MCOMM at the Yes');
  assert.match(main, /simHeld=contexts\.simFrame\(\);/, 'the tick loop reads the stack');
  assert.doesNotMatch(main, /function pause\(|(?<![\w.$])paused=|pv\('pauseContexts'\)/, 'one path: no legacy pause flag or switch');
}

// ---- the stack itself ------------------------------------------------------------------------------------------------------------
{
  const warnings = [], c = createPauseContexts({ warn: (m) => warnings.push(m) });
  const a = c.push(CTX.MENU, { owner: 'menu', audio: true, ends: (s) => s === 'game' }), b = c.push(CTX.HOLD, { owner: 'WS10 cutscene hold' });
  assert.equal(c.top, CTX.HOLD); assert.equal(c.stops(BIT.NIS), false, 'a hold stops the simulation only'); assert.equal(c.stops(BIT.SIM), true);
  assert.equal(c.pop(a), true); assert.equal(c.pop(a), false, 'pop is idempotent'); assert.equal(c.audioPaused, false);
  c.audit('game'); assert.deepEqual(warnings, []);
  const leak = c.push(CTX.MENU, { owner: 'menu', ends: (s) => s === 'game' }); c.audit('ctm-mcomm'); assert.deepEqual(warnings, []); c.audit('game'); c.audit('game');
  assert.equal(warnings.length, 1, 'a holder left open under the ride warns once'); c.pop(leak);
  c.pop(b); assert.equal(c.simFrame(), true, 'popped since the last game frame: this frame still stops'); assert.equal(c.simFrame(), false);
  c.push(CTX.PROMPT); assert.deepEqual(c.clear(), ['?']); assert.equal(c.top, 0);
  assert.throws(() => c.push(12), /Unknown pause context/);
}
// ---- the pad menus drive the UI frame counter: one pass per PS2 pad update, before its keys (web/gamepad-menus.js) --------------------
{
  const { createPadMenus } = await import('./gamepad-menus.js');
  const ph = createScreenPhases(); ph.enter('ctm-mcomm', 'game'); const seen = [];
  const m = createPadMenus({ menu: () => true, running: () => false, send: (t, c) => { if (t === 'keydown') seen.push([c, ph.frame, ph.accepts()]); } });
  const pad = { buttons: Array.from({ length: 17 }, () => ({ value: 0, pressed: false })), axes: [0, 0, 0, 0] };
  let t = 0; const f = (n, ms = 1000 / 60) => { for (let k = 0; k < n; k++) { t += ms; m.step(pad, t, () => ph.step()); } };
  f(57); pad.buttons[13].pressed = true; pad.buttons[13].value = 1;
  f(1, 3000 / 60);   // a dropped display frame: three pad updates in one rAF, each after its own UI pass
  assert.deepEqual(seen, [['ArrowDown', 58, false]], 'the press is seen on the first of the three updates and gated on that frame, not the rAF\'s last (60)');
  f(3); pad.buttons[13].pressed = false; pad.buttons[13].value = 0; f(4); pad.buttons[13].pressed = true; pad.buttons[13].value = 1; f(1);
  assert.deepEqual(seen.at(-1), ['ArrowDown', 68, true], 'a later press after the lock (+61) is taken');
}
setPv('ps2MenuInput', null);
console.log('pause contexts ok');
