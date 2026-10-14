// pv sfxPs2Rng / crowdSeqPs2 (web/sfx-ps2-rng.js, docs/audio-logic.md 5.6): the audio picks on the presentation stream 0x4FF018.
//   1. The PS2's own draws (ARMSX2 local/sfx-rng/zoe-rng: every 0x3177F0 call from 2ADF60 with the six words before it, and the
//      sound the PS2 then played): the generator and the 296310 / 29F660 formulas give the played variant for all 20.
//   2. web/sfx-game.js with the switch: riderPass() draws the rider pass's picks from the stream in the dispatchers' order and the
//      tick's dispatch plays them; a queue left undrained is not drawn twice; the gates (surface 2 / 3, replay, one grunt at a
//      time) skip the draw; switched off, the stream is untouched.
//   3. web/audio-crowd.js: the pick is list[draw % n]; crowdSeqPs2 posts a read sequence's notes in the reaction's call at
//      10 ms x 94 / 93.75 a tick.
import { nextWord, crashVariant, gruntVariant, listPick, presentationStream } from './sfx-ps2-rng.js';
import { createSfxGame, AE } from './sfx-game.js';
import { createCrowd, CROWD_TICK_MS } from './audio-crowd.js';
import { SLOT } from './sfx.js';
import { setPv } from './pv-flags.js';

let failures = 0;
const ok = (cond, msg) => {
  if (cond) console.log(`ok   ${msg}`);
  else {
    failures++;
    console.log(`FAIL ${msg}`);
  }
};

// ---- 1. ARMSX2 zoe-rng (Snow Jam, five computer riders, 5205 ticks): [tick, dispatcher, words before the draw, played bank, sound] ----
const PS2 = [
  [285, 'grunt0', [0xb7d0ec8e, 0xf99ad391, 0x7095f42b, 0xa619dc97, 0x73ae1dbb, 0x6ff767e9], 14, 3],
  [435, 'crash', [0x631d240b, 0xf87098d3, 0x7b7979af, 0x2d3cd2b2, 0xdc8b2e8b, 0x6ff76c78], 0, 48],
  [461, 'crash', [0x53fa3106, 0xb59c009a, 0x2206eda4, 0xe70b9f5c, 0x5f1f24d, 0x6ff76d3d], 0, 48],
  [736, 'crash', [0xc5083caa, 0xe4226ef9, 0xe1b00164, 0xc359aa48, 0xf2eb28c0, 0x6ff77512], 0, 50],
  [770, 'crash', [0xe2da353a, 0x1e797ae0, 0x1c30c65, 0x2f0bad5a, 0x2b0b81aa, 0x6ff775fe], 0, 50],
  [785, 'crash', [0x9828be98, 0xd90309bf, 0x248e996f, 0xcc2ca6aa, 0xc71bffb5, 0x6ff77674], 0, 50],
  [899, 'crash', [0x30cb9ad9, 0x8e7abf61, 0x4780e2f, 0x78fafeaf, 0x3a59fdb2, 0x6ff779d3], 0, 50],
  [899, 'grunt0', [0xe70ade9e, 0xb63f43c5, 0x27c48464, 0x234c7634, 0xaa517785, 0x6ff779d4], 14, 3],
  [968, 'grunt1', [0x92496ba2, 0xffcdc600, 0x2856a348, 0x30e8be0, 0xfe11fdc1, 0x6ff77d25], 14, 2],
  [1239, 'crash', [0xbf78c46d, 0x6bd459fe, 0xdd2ecfb0, 0xb149a436, 0xa85ae794, 0x6ff78377], 0, 50],
  [1260, 'crash', [0x8e91c39c, 0x31e5605, 0x67739243, 0x2dfaa954, 0xc4b57c21, 0x6ff783e5], 0, 49],
  [1875, 'crash', [0xf2f31d9d, 0x9ba97d36, 0x9c95fce9, 0xe26e831, 0x2f3ec239, 0x6ff79595], 0, 50],
  [2771, 'crash', [0x7fc2bd22, 0xe0502581, 0xc53ed694, 0xae50525a, 0x80923301, 0x6ff7ac5c], 0, 50],
  [2771, 'grunt0', [0xc42beaf1, 0x44692dce, 0x6419084c, 0x9eda31b7, 0xf089df5d, 0x6ff7ac5d], 14, 5],
  [2880, 'grunt0', [0x1ad432c6, 0x4d708f96, 0x974ce67a, 0xe9db2d64, 0x3a5e9a3c, 0x6ff7af07], 14, 3],
  [2882, 'crash', [0xe1e3581, 0x29f03f6c, 0x7d44f7f2, 0xdf63c446, 0x499f54da, 0x6ff7af1e], 0, 49],
  [2914, 'grunt0', [0x4714178d, 0x2871a8c, 0xde3dab54, 0xe9ecf717, 0xc21670b6, 0x6ff7b006], 14, 5],
  [2964, 'grunt0', [0x38bda37e, 0x387d5f9b, 0x1433a57, 0x932d761f, 0x89f08127, 0x6ff7b101], 14, 5],
  [2987, 'grunt0', [0xb690164, 0xaf51ce9d, 0xf676ce8b, 0x223823a1, 0x76882fe4, 0x6ff7b16a], 14, 3],
  [3546, 'crash', [0xe4b154c4, 0x3f0f21a, 0xd8c4097a, 0xaa55b454, 0x9d330770, 0x6ff7bc32], 0, 49]
];
{
  let same = 0;
  for (const [tick, kind, words, bank, sound] of PS2) {
    const w = Uint32Array.from(words);
    const v = nextWord(w);
    const variant = kind === 'crash' ? crashVariant(v) : gruntVariant(kind === 'grunt1' ? 1 : 0, v);
    const want = kind === 'crash' ? bank === SLOT.MAIN : bank === SLOT.GRUNT;
    if (want && variant === sound) same++;
    else console.log(`  tick ${tick} ${kind}: ${variant} vs the PS2's ${bank}:${sound}`);
  }
  ok(same === PS2.length, `the PS2's 20 audio draws give the variants it played (${same}/${PS2.length})`);
  // the next draw's words are the generator stepped once (word 5 counts the draws)
  const w = Uint32Array.from(PS2[6][2]);
  nextWord(w);
  ok(w.every((x, k) => x === PS2[7][2][k]), '0x317A08: the crash draw at 899 steps to the grunt draw\'s words');
  ok(crashVariant(0x7ffe) === 0x32 && crashVariant(0x7fff) === 0x33 && gruntVariant(0, 0) === 3, 'divu by 0x7FFF: r = 0x7FFF gives 3');
  ok(listPick(['a', 'b', 'c'], 0x8dc9ffbb) === ['a', 'b', 'c'][0x8dc9ffbb % 3], '2A4CA0: list[BXrand % n] (unsigned)');
}

// ---- 2. sfx-game with the switch ----
function harness({ on = true, replay = false, distance = 500 } = {}) {
  setPv('sfxPs2Rng', on);
  const played = [];
  const voice = (o) => ({ ...o, setVolume() {}, setBend() {}, setWheel() {}, stop() {}, playing: () => o.tag === 'grunt' && voice.gruntPlaying });
  const sfx = { play: (o) => { const v = voice(o); played.push(v); return v; }, bankOf: () => ({ bnk: true }), spatial: () => ({ g: 1, az: 0, d: distance }), stopAll() {} };
  const speech = new Proxy({ free: true }, { get: (t, k) => (k in t ? t[k] : () => {}) });
  const crowdLog = [];
  const crowd = {
    arm() {}, cancel() {}, update() {}, debug: () => ({}),
    reacts: () => true,
    trick: (L, take) => crowdLog.push(['trick', L, take?.()]),
    fall: (L, take) => crowdLog.push(['fall', L, take?.()])
  };
  const music = { resetIntensity() {}, duckToLoops: () => false, event() {}, streamLevel() {}, loopsLevel() {}, loopsStart: () => false, loopsStop() {}, loopsRequested: () => false };
  const heap = new Float32Array(8192);
  const u32 = new Uint32Array(heap.buffer);
  const wordsAt = 4000;
  const core = {
    HEAPF32: heap,
    HEAPU8: new Uint8Array(heap.buffer),
    ev: [],
    tel: new Float32Array(24),
    _audio_events() { heap.fill(0, 0, 321); heap[0] = this.ev.length; this.ev.forEach((e, i) => heap.set(e, 1 + 5 * i)); return 0; },
    _audio_events_clear() { this.ev.length = 0; },
    _audio_telemetry() { heap.set(this.tel, 1024); return 4096; },
    _visual_rng_words: () => wordsAt * 4
  };
  u32.set(PS2[6][2], wordsAt);
  const presentation = presentationStream(() => core);
  const game = createSfxGame({ engine: { busGain: () => 1 }, sfx, speech, crowd, world: null, music, random: () => 0.999, presentation });
  game.G.replay = replay;
  const list = [{ core, character: 'zoe', slot: 0, human: true }];
  const tick = (events, pass = true) => {
    core.ev.push(...events);
    if (pass) game.riderPass([{ core }]);
    game.tick({ riders: list, raceInfo: null, pending: 0 });
  };
  return { game, core, played, crowdLog, tick, voice, words: () => Array.from(u32.subarray(wordsAt, wordsAt + 6)) };
}
{
  // the first tick makes the rider known (riderPass draws for known riders only)
  const h = harness();
  h.tick([]);
  h.tick([[AE.CRASH, -0.25, 0, 0, 0]]);
  const crash = h.played.find((p) => p.slot === SLOT.MAIN && p.tag === 'crash');
  const grunt = h.played.find((p) => p.slot === SLOT.GRUNT);
  ok(crash?.sound === 50 && grunt?.sound === 3, `crash entry: the stream's 0x32 then grunt 3 (the PS2's 899: ${crash?.sound}, ${grunt?.sound})`);
  ok(h.crowdLog[0]?.[0] === 'fall' && h.crowdLog[0][2] != null, 'crash entry: the fall reaction gets the third draw');
  const w = Uint32Array.from(PS2[6][2]);
  for (let k = 0; k < 3; k++) nextWord(w);
  ok(h.words().every((x, k) => x === w[k]), 'three draws taken from the stream (crash, grunt, crowd)');
}
{
  // an undrained queue is drawn once
  const h = harness();
  h.tick([]);
  h.core.ev.push([AE.CRASH, 0, 0, 0, 0]);
  h.game.riderPass([{ core: h.core }]);
  h.game.riderPass([{ core: h.core }]);
  h.game.tick({ riders: [{ core: h.core, character: 'zoe', slot: 0, human: true }], raceInfo: null, pending: 0 });
  const w = Uint32Array.from(PS2[6][2]);
  nextWord(w);
  nextWord(w);
  ok(h.words().every((x, k) => x === w[k]), 'two rider passes over one queue: the crash and grunt drawn once');
}
{
  // gates: surface 2 / 3 (LAND 9, no draw); a grunt still playing; a replay blocks the grunt (288AE0)
  const h = harness();
  h.tick([]);
  h.tick([[AE.CRASH_GRUNT, 1, 0, 0, 0]]);
  const after = h.words();
  h.voice.gruntPlaying = true;
  h.played.length = 0;
  h.tick([[AE.CRASH, 0, 2, 0, 0]]);
  ok(h.played.some((p) => p.slot === SLOT.LAND && p.sound === 9) && !h.played.some((p) => p.slot === SLOT.GRUNT), 'surface 2: LAND 9, no grunt while one plays');
  ok(h.words().every((x, k) => x === after[k]), 'no draw on surface 2 with a grunt playing');
  const r = harness({ replay: true });
  r.tick([]);
  r.tick([[AE.CRASH_GRUNT, 1, 0, 0, 0]]);
  ok(!r.played.some((p) => p.slot === SLOT.GRUNT) && r.words().every((x, k) => x === PS2[6][2][k]), 'replay: no grunt, no draw (288AE0)');
}
{
  // a draw the rider pass did not see (an event after it) is drawn when it runs
  const h = harness();
  h.tick([]);
  h.tick([[AE.CRASH_GRUNT, 1, 0, 0, 0]], false);
  const g = h.played.find((p) => p.slot === SLOT.GRUNT);
  const w = Uint32Array.from(PS2[6][2]);
  ok(g?.sound === gruntVariant(1, nextWord(w)), 'a late grunt (kind 1) draws at its dispatch');
}
{
  // off: Math.random (here the harness's random), the stream untouched
  const h = harness({ on: false, distance: 5 });
  h.tick([]);
  h.tick([[AE.CRASH, -0.25, 0, 0, 0]]);
  ok(h.words().every((x, k) => x === PS2[6][2][k]), 'off: the stream is not drawn');
  ok(h.played.some((p) => p.slot === SLOT.MAIN && p.sound === 0x32), 'off: the crash pick comes from random');
}
setPv('sfxPs2Rng', null);

// ---- 3. audio-crowd ----
{
  setPv('crowdSeqPs2', true);
  const posted = [];
  const sfx = { play: (o) => { posted.push(o); return { setVolume() {}, stop() {}, playing: () => true }; } };
  const midx = Uint8Array.from([0x4d, 0x49, 0x44, 0x78, 3, 0xc0, 2, 7, 0x90, 60, 100, 0, 0xff, 0x2f]);
  const config = [0, 1, 2].map(() => ({ trick: ['a.eam', 'b.eam'], fall: [], anticipate: [], attack: 1, decay: 4, maxBend: 32 }));
  const crowd = createCrowd({ sfx, bytes: async () => midx, config, random: () => 0 });
  crowd.emitter('e', { patch: 0, position: [0, 0, 0], volume: 1 });
  await new Promise((r) => setTimeout(r, 10));
  posted.length = 0;
  ok(crowd.reacts('trick', 0) && !crowd.reacts('fall', 0), 'reacts: a list for the level and emitters');
  crowd.trick(0, () => 1);
  const note = posted.find((p) => p.note === 60);
  ok(note && Math.abs(note.delayMs - 3 * CROWD_TICK_MS) < 1e-9, `crowdSeqPs2: the read sequence posts in the call, tick 3 at ${note?.delayMs?.toFixed(3)} ms`);
  ok(Math.abs(CROWD_TICK_MS - 10.0267) < 1e-4, 'one sequence tick = 10 ms x 94 / 93.75');
  setPv('crowdSeqPs2', null);
}

if (failures) {
  console.log(`${failures} failed`);
  process.exit(1);
}
console.log('all passed');
