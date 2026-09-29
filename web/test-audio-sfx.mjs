// Sound effects, speech events, crowd, world sounds and painters (docs/audio-logic.md sections 4-5).
// node test-audio-sfx.mjs  (needs web/public/assets/AUDIO from tools/export_audio.py, export_speech_events.py,
// export_world_audio.py, export_animation_audio.py; skips the asset checks when they are missing)
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { curve, CURVES, surfaceClass, boardFamily, parsePatchTags, SLOT } from './sfx.js';
import { parseEvents, hdrFromRegistry, makeBankTable, EaRng, createSpeechState, resolveEvent } from './audio-speech-events.js';
import { speakerMask, subjectMask, charId, peakMask, courseBit, placeMask, busOfBank, frontEndCue, EV, CHAR_ID } from './audio-speech.js';
import { frontEndSpeechBank, speakFrontEnd, SPEECH_ABBR } from './rider-speech.js';
import { parseMidx, crowdConfig } from './audio-crowd.js';
import { falloff, emitterVolume, scriptSoundRoute } from './audio-world.js';
import { queryAudioPainters } from './audio-painters.js';
import { createSfxGame, AE, gruntIndex, relationOf } from './sfx-game.js';
import { CHARTUNE_EVENT, UI_SOUND, COURSE_INDEX, eventKind } from './game-audio.js';
import { CHANNEL_SCALE, MASTER_GAIN, SLIDER_DEFAULT } from './audio-engine.js';
import { SPU_GAUSS, spuResample } from './spu-interp.js';

const A = new URL('./public/assets/AUDIO/', import.meta.url);
const has = (p) => fs.existsSync(new URL(p, A));
const read = (p) => new Uint8Array(fs.readFileSync(new URL(p, A)));
const readJson = (p) => JSON.parse(fs.readFileSync(new URL(p, A), 'utf8'));
let checks = 0; const ok = (c, m) => { assert.ok(c, m); checks++; };

// ---- 290B58 curves, surface classes, families ----
ok(curve(CURVES.landing_vol_482D30, 50) === 10 && curve(CURVES.landing_vol_482D30, 3000) === 127, 'landing curve ends');
ok(Math.abs(curve(CURVES.landing_vol_482D30, 1000) - 57) < 1e-9, 'landing curve interpolates (800..1200 -> 34..80)');
ok(curve(CURVES.loopA_vol_482AF8, 0.00255361) === 101, 'loop A volume peak');
ok(curve(CURVES.glide0_pitch_482BA0, 0.02) === 0.5 && curve(CURVES.glide0_pitch_482BA0, -1) === 1, 'non-monotonic x: first interval that contains v, then the ends');
ok(surfaceClass(-1) === 0 && surfaceClass(1) === 2 && surfaceClass(4) === 3 && surfaceClass(10) === 4 && surfaceClass(9) === 5 && surfaceClass(7) === 7 && surfaceClass(11) === 8, '291710 classes');
ok(boardFamily(3, 4) === 2 && boardFamily(3, 0) === 0 && boardFamily(1, 0) === 1 && boardFamily(5, 0) === 2, '2934D0 families');

// ---- speech masks and tables ----
ok(speakerMask(charId('nate')) === 0x80 && speakerMask(charId('viggo')) === 0x40 && speakerMask(charId('elise')) === 1, '29F2B0 CHARDB masks');
ok(speakerMask(-1) === 0 && subjectMask(-1) === 0x1000, 'guest speaker 0 / subject 0x1000');
ok(peakMask(0) === 1 && peakMask(11) === 1 && peakMask(2) === 2 && peakMask(4) === 4, '2A1E20 peaks');
ok(courseBit(0) === 1 && courseBit(4) === 16 && courseBit(5) === 64 && courseBit(11) === (1 << 12) && courseBit(17, 0x10) === (1 << 9), '2A1BD8 course bits');
ok(placeMask(0) === 1 && placeMask(5) === 0x200 && placeMask(10) === 0, '0x483070 place masks');
ok(busOfBank('DJ_Artist_Intro_eng') === 'DJ' && busOfBank('PA_Finish_Line_eng') === 'PA' && busOfBank('Arcade_Prompts') === 'ARCADESPEECH' && busOfBank('Whooh_mac') === 'CHARACTER', '29EEE0 buses');

// ---- game-audio tables ----
ok(CHARTUNE_EVENT[charId('moby')] === 10 && CHARTUNE_EVENT[charId('mac')] === 1 && CHARTUNE_EVENT[charId('viggo')] === 6, 'chartune by winner CHARDB id');
ok(UI_SOUND[0] === 3 && UI_SOUND[11] === 2 && UI_SOUND[13] === 0xd && UI_SOUND[14] === 0x12, '294F78 table');
ok(COURSE_INDEX.BHP1 === 11 && eventKind(COURSE_INDEX.BHP1) === 3 && eventKind(COURSE_INDEX.ARA1) === 0, 'location ids / kinds');
ok(gruntIndex('nate') === 7 && gruntIndex('sam') === 7 && gruntIndex('moby') === 0, '29F378 grunt banks');

// ---- gain staging (288D18 channel scales, master byte, SPU interpolation) ----
ok(CHANNEL_SCALE.WORLD.DJ === 0.55 && CHANNEL_SCALE.WORLD.PA === 0.43 && CHANNEL_SCALE.WORLD.ARCADESFX === 0.75 && CHANNEL_SCALE.FE.CHARACTER === 0.6, '288D18 tables');
ok(CHANNEL_SCALE.WORLD.speakers[charId('zoe')] === 0.8 && CHANNEL_SCALE.WORLD.speakers[charId('nate')] === 1, 'per-speaker scales (CHARDB order)');
ok(Math.round(MASTER_GAIN * 127) === 115 && SLIDER_DEFAULT === 10, 'SND master byte 115, profile sliders 10');
ok(SPU_GAUSS.length === 512 && SPU_GAUSS[0] === -1 && SPU_GAUSS[511] === 0x59b3 && SPU_GAUSS[255] === 0x12c7, 'SPU Gaussian table');
{ let sum = 0; for (let i = 0; i < 256; i++) { const t = SPU_GAUSS[255 - i] + SPU_GAUSS[511 - i] + SPU_GAUSS[256 + i] + SPU_GAUSS[i]; sum = Math.max(sum, Math.abs(t - 0x7f80)); } ok(sum <= 0x100, 'Gaussian taps sum to ~1.0'); }
{ const d = { sampleRate: 24000, length: 480, channels: 1, data: [new Float32Array(480).fill(0.5)], loopStart: -1, loopEnd: -1 };
  const r = spuResample(d); ok(r.sampleRate === 48000 && r.length === 960 && Math.abs(r.data[0][400] - 0.5 * 0x7f80 / 0x8000) < 0.001, 'SPU resample 24k -> 48k, DC gain 0x7F80 / 0x8000'); }

// ---- world sounds ----
ok(falloff(2, 0.25) === 0.75 && falloff(5, 0.5) === 1 && Math.abs(falloff(5, 0.9) - 0.33333) < 1e-9, '2B82B8 falloffs');
ok(emitterVolume({ shape: 'sphere', pos: [0, 0, 0], radius: 100, falloff: 2 }, [50, 0, 0]) === 0.5, 'sphere emitter');
ok(emitterVolume({ shape: 'sphere', pos: [0, 0, 0], radius: 100, falloff: 2 }, [150, 0, 0]) === 0, 'outside the sphere');
ok(JSON.stringify(scriptSoundRoute(102)) === JSON.stringify({ slot: SLOT.WORLD8, sound: 2 }) && scriptSoundRoute(201).slot === SLOT.TRANSPORT && scriptSoundRoute(0) === null, '2974A0 routing');

// ---- dispatchers (fake voice layer) ----
{
  const played = [];
  const voice = (o) => ({ ...o, setVolume() {}, setBend() {}, setWheel() {}, stop() { this.stopped = true; }, playing: () => true });
  const sfx = { play: (o) => { const v = voice(o); played.push(v); return v; }, bankOf: () => ({ bnk: true }), spatial: () => ({ g: 1, az: 0, d: 5 }), stopAll() {} };
  const speechLog = []; const speech = new Proxy({ free: true }, { get: (t, k) => (k in t ? t[k] : (...a) => speechLog.push([k, ...a])) });
  const crowdLog = []; const crowd = { arm: () => crowdLog.push('arm'), cancel: () => crowdLog.push('cancel'), trick: (L) => crowdLog.push('trick' + L), fall: (L) => crowdLog.push('fall' + L), update() {}, debug: () => ({}) };
  const music = { resetIntensity() {}, duckToLoops: () => true, event() {}, streamLevel() {}, loopsLevel() {}, loopsStart: () => true, loopsStop() {}, loopsRequested: () => false };
  const engine = { busGain: () => 1 };
  const game = createSfxGame({ engine, sfx, speech, crowd, world: null, music, random: () => 0 });
  const tel = new Float32Array(24); const events = []; let qPtr = 0;
  const heap = new Float32Array(4096);
  const core = { HEAPF32: heap, _audio_events() { heap.fill(0, 0, 321); heap[0] = events.length; events.forEach((e, i) => heap.set(e, 1 + 5 * i)); return 0; }, _audio_events_clear() { events.length = 0; }, _audio_telemetry() { heap.set(tel, 1024); return 1024 * 4; } };
  const step = (evs = []) => { events.push(...evs); game.tick({ riders: [{ core, character: 'mac', slot: 0, human: true }], raceInfo: null, pending: 0 }); };
  tel[4] = 0; tel[5] = 0; step();
  played.length = 0;
  tel[5] = 4; step([[AE.TAKEOFF, 0.3, 1, 4.5, 0]]); // class 3 (surface 4): LAND 3*8+2, volume clamp(0.3*127, 64, 127)
  const t = played.find((p) => p.slot === SLOT.LAND);
  ok(t && t.sound === 26 && t.volume === 64 && t.bus === 'BOARD', `take-off sound ${JSON.stringify(t && { s: t.sound, v: t.volume })}`);
  ok(crowdLog.includes('arm') && speechLog.some((x) => x[0] === 'bigAir'), 'take-off: crowd anticipation + Big_Air speech (T >= 4)');
  played.length = 0; crowdLog.length = 0; tel[5] = 0;
  step([[AE.LANDING, 0.5, 1000, 0, 0]]);
  const l = played.find((p) => p.slot === SLOT.LAND);
  ok(l && l.sound === 1 && l.volume === 57 && crowdLog[0] === 'trick1', 'landing: LAND class*8+1, curve 0x482D30, Cheer20');
  played.length = 0;
  step([[AE.BOOST, 1, 1, 0, 0]]);
  ok(played.some((p) => p.sound === 0x7a) && played.some((p) => p.sound === 0x4a), 'boost start: loop 0x7A + one-shot 0x4A');
  step([[AE.BOOST, 3, 0, 0, 0]]);
  ok(played.find((p) => p.sound === 0x7a).stopped, 'boost release stops the loop at refcount 0');
  played.length = 0; crowdLog.length = 0;
  step([[AE.CRASH, -0.25, 0, 0, 0]]);
  ok(played.some((p) => p.slot === SLOT.MAIN && p.sound === 0x30) && played.some((p) => p.slot === SLOT.GRUNT && p.sound >= 3 && p.sound <= 5) && crowdLog[0] === 'fall1', 'crash: 0x30+rnd, grunt 3..5, Ahh20');
  played.length = 0;
  step([[AE.CRASH_LOOP, 0x19, 0, 0, 0]]);
  const loop = played.find((p) => p.sound === 0x33);
  ok(loop, 'slide loop 0x33 for clip class 0x19');
  step([[AE.CRASH_AIR, 0, 0, 0, 0]]);
  ok(loop.stopped, 'leaving the slide phase stops the loops');
}

// ---- 2A1820 friend / foe with the ARMSX2 Snow Jam records (slots 0 Zoe human, 1 Psymon, 2 Allegra) ----
{
  // scores[a][b] = level of b's record about a; kinds[a][b] = kind. Anchor: Psymon -> Zoe 00 00 00, Allegra -> Zoe 01 00 00,
  // Zoe -> every other 01 00 00. Tick 5009: Psymon -> Zoe 00 02 0c (12 hits).
  const anchor = { scores: [[0, 0, 0], [0, 0, 0], [0, 0, 0]], kinds: [[0, 0, 1], [1, 0, 1], [1, 1, 0]] };
  ok(relationOf(anchor, 2, 0) === 1 && relationOf(anchor, 0, 2) === 1, 'Allegra / Zoe: friend (kind 1) -> 0x2080 / 0x207D at ticks 846, 1482');
  ok(relationOf(anchor, 1, 0) === 1 && relationOf(anchor, 0, 1) === 0, 'Psymon / Zoe at the anchor: friend');
  const later = { scores: [[0, 2, 0], [0, 0, 0], [0, 0, 0]], kinds: anchor.kinds };
  ok(relationOf(later, 1, 0) === 2 && relationOf(later, 0, 1) === 2, 'Psymon -> Zoe level 2: foe either way -> 0x2081 / 0x207F at tick 3138');
  // Dispatcher: an overtake after the 600-tick gate posts the friend pair; before it nothing.
  const posted = [];
  const speech = { free: true, pass: (A, B, rel, fast) => posted.push([A.id, B.id, rel, fast]) };
  const sfx = { play: () => null, bankOf: () => null, spatial: () => ({ g: 1, az: 0, d: 5 }), stopAll() {} };
  const game = createSfxGame({ engine: { busGain: () => 1 }, sfx, speech, crowd: { update() {}, debug: () => ({}) }, world: null, music: { resetIntensity() {} } });
  const heap = new Float32Array(4096); const cores = [0, 1, 2].map((slot) => ({ slot, t: new Float32Array(24), ev: [] }));
  for (const c of cores) Object.assign(c, { HEAPF32: heap, _audio_events() { heap.fill(0, 0, 321); heap[0] = c.ev.length; c.ev.forEach((e, i) => heap.set(e, 1 + 5 * i)); return 0; }, _audio_events_clear() { c.ev.length = 0; }, _audio_telemetry() { heap.set(c.t, 1024); return 4096; } });
  cores[2].t.set([1500, 0, 0], 19); cores[0].t.set([500, 0, 0], 19); // Allegra 10 m/s faster along Zoe's heading
  game.G.relation = (a, b) => relationOf(anchor, a, b);
  const list = [['zoe', true], ['psymon', false], ['allegra', false]].map(([character, human], slot) => ({ core: cores[slot], character, slot, human }));
  const tickWith = (phase, evs = []) => { cores[0].ev.push(...evs); game.tick({ riders: list, raceInfo: new Float32Array([0, 0, 0, 0, 0, phase, 0, 0]) }); };
  game.start(); tickWith(4); tickWith(5); // GO
  tickWith(5, [[AE.OVERTAKE, 0, 0 * 8 + 1, 1, 2]]); // Zoe loses first place to Allegra before the gate
  ok(posted.length === 0, 'pass lines are gated for 600 ticks after GO');
  for (let i = 0; i < 598; i++) tickWith(5);
  tickWith(5, [[AE.OVERTAKE, 0, 0 * 8 + 1, 1, 2]]);
  ok(posted.length === 1 && posted[0][0] === charId('allegra') && posted[0][1] === charId('zoe') && posted[0][2] === 1 && posted[0][3] === 1, `friend pass after the gate ${JSON.stringify(posted)}`);
}

// ---- assets ----
if (has('banks/zBxsfx.bnk')) {
  const bnk = read('banks/zBxsfx.bnk'), j = readJson('banks/zBxsfx.json');
  const tags = parsePatchTags(bnk, j.entries[0x20].headerOffset)[0];
  const env = tags.get(0x19), base = env.addr + env.v, dv = new DataView(bnk.buffer, bnk.byteOffset);
  ok(dv.getInt32(base, true) === 125 && dv.getInt32(base + 4, true) === 127 && tags.get(0x09).v === 4, 'whoosh envelope 125 ticks -> 127 (self-relative table)');
  ok(tags.get(0x0f).v === 20 && tags.get(0x11).v === 300 && tags.get(0x1e).v === 100, 'patch tags');
  ok(parsePatchTags(bnk, j.entries[0x34].headerOffset).length === 4, 'multi-patch entry = 4 layers');
}
// ---- front-end rider speech (web/rider-speech.js -> game-audio speak -> 2A16B0 / 2A1778) ----
ok(frontEndSpeechBank('select', { id: 'sam' }) === null && frontEndSpeechBank('customize', { id: 'sam' }) === null, 'Sam has no original voice');
ok(frontEndSpeechBank('select', { id: 'zoe' }, { career: true }) === null, 'no Post_Selection in Conquer the Mountain (0x535C11 == 0)');
ok(frontEndSpeechBank('customize', { id: 'zoe' }, { career: true }) === 'Customize_zoe', 'Customize in every mode');
ok(frontEndSpeechBank('select', { kind: 'cheat', id: 'unknown', base: 'mac' }) === 'Post_Selection_mac', 'a cheat skin speaks with its base rider');
{
  const calls = []; const ui = { careerMode: false, cb: { speech: (b, o) => { calls.push([b, o.bus]); return Promise.resolve(true); } } };
  speakFrontEnd(ui, 'select', { id: 'nate' }); speakFrontEnd({ ...ui, careerMode: true }, 'select', { id: 'nate' }); speakFrontEnd(ui, 'customize', { id: 'sam' });
  ok(JSON.stringify(calls) === '[["Post_Selection_nat","CHARACTER"]]', 'speakFrontEnd: one call, the CHARACTER bus');
}
for (const [rider, abbr] of Object.entries(SPEECH_ABBR)) {
  const sel = frontEndCue(frontEndSpeechBank('select', { id: rider })), cz = frontEndCue(frontEndSpeechBank('customize', { id: rider }));
  ok(sel.kind === 'select' && sel.event === EV.POST_SELECTION && sel.id === CHAR_ID[rider] && abbr.length === 3, `${rider}: Post_Selection -> 0x20BC, speaker ${CHAR_ID[rider]}`);
  ok(cz.kind === 'customize' && cz.event === EV.CUSTOMIZE && cz.id === CHAR_ID[rider], `${rider}: Customize -> 0x20BD`);
}
ok(frontEndCue('Post_Selection_eng') === null && frontEndCue('Big_Air_zoe') === null && frontEndCue(null) === null, 'frontEndCue rejects other banks');
if (has('speech/Events.evt') && has('speech/registry.json')) {
  const events = parseEvents(read('speech/Events.evt'));
  const banks = makeBankTable(readJson('speech/registry.json').banks.map(hdrFromRegistry));
  const rng = new EaRng(0x1234), st = createSpeechState();
  const r = resolveEvent(events, banks, 0x20c5, [1, 1, 1], rng, st);
  ok(r.items.map((i) => i.bank).join() === 'PA_Silence_500,PA_Finish_Line_eng', 'PA finish line resolves');
  const big = resolveEvent(events, banks, 0x2084, [4, 3], rng, st);
  ok(big.items[1]?.bank === 'Big_Air_Amateur_mob', 'Moby big air (mask 4)');
  ok(resolveEvent(events, banks, 0x2084, [4, 3], rng, st).items.length === 0, 'Big_Air maxRepeat 1');
  const nate = resolveEvent(events, banks, 0x20b8, [speakerMask(charId('nate'))], rng, st);
  ok(nate.items.at(-1)?.bank === 'Whooh_vig', 'PS2 swap: Nate (mask 0x80) speaks the _vig bank');
  ok(events.events.get(0x20e5).P === 1000 && events.events.get(0x20c5).P === 801, 'priorities');
  // 0x20BC / 0x20BD for every voiced rider: a silence lead-in on ARCADESPEECH, then the rider's own bank on CHARACTER,
  // except the PS2's Nate/Viggo swap in Events.evt (Nate, mask 0x80, speaks _vig; Viggo, mask 0x40, speaks _nat).
  const heard = { ...SPEECH_ABBR, nate: 'vig', viggo: 'nat' };
  for (const rider of Object.keys(SPEECH_ABBR)) for (const [ev, prefix] of [[EV.POST_SELECTION, 'Post_Selection'], [EV.CUSTOMIZE, 'Customize']]) {
    const lines = new Set(), rng = new EaRng(0x3d7); // one generator seeded once (2ADF60), as in the game
    for (let k = 0; k < 12; k++) {
      const res = resolveEvent(events, banks, ev, [speakerMask(CHAR_ID[rider])], rng, createSpeechState());
      const last = res.items.at(-1);
      ok(last?.bank === `${prefix}_${heard[rider]}` && busOfBank(last.bank) === 'CHARACTER' && res.items.slice(0, -1).every((i) => busOfBank(i.bank) === 'ARCADESPEECH'), `${rider} ${prefix}: ${last?.bank}`);
      lines.add(last.line);
    }
    ok(lines.size > 1, `${rider} ${prefix}: random line choice`);
  }
}
if (has('banks/Cheer30.eam')) {
  const ev = parseMidx(read('banks/Cheer30.eam'));
  const notes = ev.filter((e) => e.type === 'noteOn').map((e) => e.note);
  ok(notes.join() === '64,62' && ev.find((e) => e.type === 'program').program === 18, 'Cheer30: program 18, notes 64 + 62');
  const cat = readJson('catalog.json');
  const cfg = crowdConfig(cat.crowdInf);
  ok(cfg[2].trick[0] === 'Cheer30.eam' && cfg[1].fall[0] === 'Ahh20.eam' && cfg[0].maxBend === 32, 'CROWD.INF [GLOBAL]');
}
if (has('world/BRA2.json') && has('world/ABC1.json')) {
  const bra = readJson('world/BRA2.json'), abc = readJson('world/ABC1.json'), ara = readJson('world/ARA1.json');
  ok(queryAudioPainters(bra, { x: -180000, y: -60000 }, 'BRA2').mix === 2, 'Metro-City mix 2 region');
  ok(queryAudioPainters(bra, { x: -225046, y: 125128 }, 'BRA2').mix === 0, 'Metro-City start: mix 0');
  ok(queryAudioPainters(abc, { x: 20000, y: 50000 }, 'ABC1').mix === 1, 'Happiness mix 1 region');
  ok(ara.locations.ARA1.banks.slot8 === 'ARA1_slot8.bnk' && ara.locations.ARA1.ambience && ara.watrig['41'].bank === 5, 'ARA1 world audio');
  ok(Object.keys(ara.locations).join() === 'A_ARA1,ARA1,ARA1_B', 'resident locations');
}
if (has('anim-events.json')) {
  const a = readJson('anim-events.json');
  ok(a.clips['72'][0] === 0x8052 && a.clips['225'].includes(0x8050), 'animation sound events');
}
console.log(`audio sfx: ${checks} checks passed`);
