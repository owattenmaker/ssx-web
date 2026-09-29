// Audio glitch fixes and field counters (docs/audio-logic.md 9.13), node, against Web Audio stand-ins:
//   pv audioDeclick       the music player's pause / resume / cut / Stop ramp over 5 ms instead of stepping to / from 0
//   pv musicWorkerDecode  every stream bar decodes off the main thread; a 6-channel bar folds to the same floats in the worker
//   pv audioInterrupt     a context that stopped takes no new voices, holds (movie, hidden page) are counted, resume is retried
//   pv sfxStartAfterDecode a sound's layers decode before its start time is read (no late start / skipped attack)
//   web/audio-stats.js    late / missed bars, stolen / dropped voices, slow decodes
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import assert from 'node:assert/strict';
import { prepareSong, createPathfinderPlayer } from './pathfinder.js';
import { decodeMusicSample, toAudioBuffer } from './audio-decode.js';
import { decodeAudioJob } from './audio-decode-job.js';
import { audioStats, audioStatsSnapshot } from './audio-stats.js';
import { createAudioEngine } from './audio-engine.js';
import { createSfx, SLOT } from './sfx.js';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const AUDIO = path.join(HERE, 'public/assets/AUDIO'), MUSIC = path.join(AUDIO, 'music');
if (!fs.existsSync(path.join(MUSIC, 'Go.json'))) { console.log('test-audio-glitches: skipped (no exported audio)'); process.exit(0); }
const readJson = (id) => JSON.parse(fs.readFileSync(path.join(MUSIC, `${id}.json`)));
const file = (json, kind) => { const t = json.tracks.find((x) => x.kind === kind); return t ? new Uint8Array(fs.readFileSync(path.join(MUSIC, t.file))) : null; };
const song = (id, opts = {}) => { const json = readJson(id); return prepareSong(json, { mus: file(json, 'stream'), loops: file(json, 'bank'), ...opts }); };
const lcg = (seed = 1) => { let s = seed >>> 0; return () => { s = (Math.imul(s, 1664525) + 1013904223) >>> 0; return s / 4294967296; }; };
const near = (a, b, tol = 1e-9) => assert.ok(Math.abs(a - b) <= tol, `${a} != ${b}`);
const events = (got, want) => { assert.equal(got.length, want.length, JSON.stringify(got)); got.forEach((e, i) => { assert.equal(e[0], want[i][0]); for (let k = 1; k < e.length; k++) near(e[k], want[i][k], 1e-9); }); };
const tests = [];
const test = (name, fn) => tests.push([name, fn]);
const reset = () => { for (const k in audioStats) audioStats[k] = 0; };

// AudioContext stand-in that records every automation event and start / stop.
function fakeContext() {
  const param = (v = 1) => ({ value: v, events: [], setValueAtTime(x, t) { this.events.push(['set', x, t]); this.value = x; }, linearRampToValueAtTime(x, t) { this.events.push(['ramp', x, t]); },
    cancelScheduledValues(t) { this.events.push(['cancel', t]); }, cancelAndHoldAtTime(t) { this.events.push(['hold', t]); }, setValueCurveAtTime() {}, setTargetAtTime(x, t) { this.events.push(['target', x, t]); } });
  const ctx = {
    currentTime: 0, state: 'running', destination: {}, sources: [], gains: [],
    createGain() { const g = { gain: param(), connect(n) { g.dest = n; }, disconnect() { g.disconnected = true; } }; ctx.gains.push(g); return g; },
    createBiquadFilter() { return { type: '', frequency: param(), Q: param(), connect() {}, disconnect() {} }; },
    createStereoPanner() { return { pan: param(0), connect() {}, disconnect() {} }; },
    createBuffer(ch, len, rate) { const data = Array.from({ length: ch }, () => new Float32Array(len)); return { numberOfChannels: ch, length: len, sampleRate: rate, duration: len / rate, data, copyToChannel(x, c) { data[c].set(x); }, getChannelData(c) { return data[c]; } }; },
    createBufferSource() {
      const s = { buffer: null, loop: false, detune: param(0), playbackRate: param(1), connect(n) { s.dest = n; }, disconnect() { s.disconnected = true; },
        start(when, off = 0) { s.when = when; s.offset = off; }, stop(t = ctx.currentTime) { s.stopAt = t; } };
      ctx.sources.push(s); return s;
    },
  };
  return ctx;
}
const bar = 56888 / 32000; // Go's first bars

test('declick: pause fades the sounding bar out over 5 ms, resume fades the restarted bar in, song timing unchanged', () => {
  const ctx = fakeContext();
  const p = createPathfinderPlayer({ context: ctx, destination: ctx.destination, song: song('Go'), random: lcg(5), autoPump: false, declickMs: 5 });
  p.start(0, 10);
  ctx.currentTime = 10.4; p.pump();
  const [first, second] = ctx.sources;
  near(first.when, 10); near(second.when, 10 + bar);
  p.pause();
  near(first.stopAt, 10.405); // sounding: stops 5 ms later, after the ramp
  events(first.dest.gain.events.slice(-2), [['hold', 10.4], ['ramp', 0, 10.405]]);
  near(second.stopAt, 10.4); assert.equal(second.dest.gain.events.length, 0); // not started yet: stopped, no fade needed
  ctx.currentTime = 20; p.resume();
  const live = ctx.sources.filter((x) => x.stopAt == null);
  near(live[0].when, 20); near(live[0].offset, 0.4);            // the same offset as without the declick
  events(live[0].dest.gain.events, [['set', 0, 20], ['ramp', 1, 20.005]]);
  near(live[1].when, 20 - 0.4 + bar); assert.equal(live[1].offset, 0); assert.equal(live[1].dest.gain.events.length, 0);
  assert.equal(audioStats.musicLate, 0, 'a resume is not a late bar');
});

test('declick: a resume right at a bar line fades that bar in (it starts from silence mid-song)', () => {
  const ctx = fakeContext();
  const p = createPathfinderPlayer({ context: ctx, destination: ctx.destination, song: song('Go'), random: lcg(5), autoPump: false, declickMs: 5 });
  p.start(0, 10); ctx.currentTime = 10 + bar - 0.5; p.pump();
  ctx.currentTime = 10 + bar - 0.01; p.pause(); // 10 ms before the bar line (song time frozen there)
  ctx.currentTime = 30; p.resume();
  const live = ctx.sources.filter((x) => x.stopAt == null);
  near(live[0].when, 30); near(live[0].offset, bar - 0.01, 1e-6);            // the last 10 ms of the paused bar, faded in
  near(live[1].when, 30.01); assert.equal(live[1].offset, 0);                 // the next bar at its line
  assert.equal(live[1].dest.gain.events.length, 0);                          // it continues that bar's waveform: no second fade
  // paused exactly on the bar line: the next bar is the first audio and fades in
  const ctx2 = fakeContext(), q = createPathfinderPlayer({ context: ctx2, destination: ctx2.destination, song: song('Go'), random: lcg(5), autoPump: false, declickMs: 5 });
  q.start(0, 10); ctx2.currentTime = 10 + bar - 0.5; q.pump(); ctx2.currentTime = 10 + bar; q.pause(); ctx2.currentTime = 30; q.resume();
  const first = ctx2.sources.filter((x) => x.stopAt == null)[0];
  near(first.when, 30); assert.equal(first.offset, 0); events(first.dest.gain.events, [['set', 0, 30], ['ramp', 1, 30.005]]);
});

test('declick: an event cut fades out ending at the cut when it can, else right after now; Stop ramps the outputs', () => {
  reset();
  const ctx = fakeContext();
  const p = createPathfinderPlayer({ context: ctx, destination: ctx.destination, song: song('Go'), random: lcg(5), autoPump: false, declickMs: 5 });
  p.start(0, 0);
  ctx.currentTime = 0.9; p.pump();
  const bar0 = ctx.sources[0];
  p.send(33); // challenge start: save, cut the stream (flush), stinger, wait 1200 ms, jump
  near(bar0.stopAt, 0.905);
  events(bar0.dest.gain.events.slice(-2), [['hold', 0.9], ['ramp', 0, 0.905]]);
  const outs = ctx.gains.filter((g) => g.dest === ctx.destination);
  ctx.currentTime = 3; p.pump(); p.stop();
  for (const g of outs) events(g.gain.events.slice(-2), [['hold', 3], ['ramp', 0, 3.005]]);
  assert.ok(ctx.sources.every((s) => s.stopAt == null || s.stopAt <= 3.005 + 1e-9));
  assert.ok(outs.every((g) => !g.disconnected), 'the outputs let go after the ramp (timer)');
});

test('without the declick the player is unchanged: hard stops at the pause', () => {
  const ctx = fakeContext();
  const p = createPathfinderPlayer({ context: ctx, destination: ctx.destination, song: song('Go'), random: lcg(5), autoPump: false });
  p.start(0, 10); ctx.currentTime = 10.4; p.pump(); p.pause();
  near(ctx.sources[0].stopAt, 10.4); assert.equal(ctx.gains.filter((g) => g.gain.events.some((e) => e[0] === 'hold')).length, 0);
});

test('counters: a bar started after its time counts as late (with its gap), a bar that never got audio as missed', () => {
  reset();
  const ctx = fakeContext();
  const p = createPathfinderPlayer({ context: ctx, destination: ctx.destination, song: song('Go'), random: lcg(5), autoPump: false });
  p.start(0, 0);
  ctx.currentTime = bar + 0.3; p.pump(); // no pump since 0: bar 1 (due at `bar`) starts 300 ms late
  assert.equal(audioStats.musicLate, 1); assert.equal(audioStats.musicLateMaxMs, 300);
  const s = song('Go'), silent = Object.create(s); silent.decodeSegment = () => null; // no audio for any bar
  const q = createPathfinderPlayer({ context: ctx, destination: ctx.destination, song: silent, random: lcg(5), autoPump: false });
  q.start(0, 100);
  for (let t = 100; t < 100 + 3 * bar; t += 0.05) { ctx.currentTime = t; q.pump(); }
  assert.ok(audioStats.musicMissed >= 2, `missed ${audioStats.musicMissed}`);
  const snap = audioStatsSnapshot(); assert.ok(snap.musicLate === 1 && snap.musicMissed >= 2); assert.ok(Object.values(snap).every((v) => v), 'the snapshot has only the counters that moved');
});

test('worker decode: EA-XA bars go through decodeAsync ahead of time and come back identical; late ones decode here', async () => {
  const json = readJson('Go'), mus = file(json, 'stream');
  const calls = [];
  const decodeAsync = (bytes, sample) => { calls.push(sample); return Promise.resolve(decodeAudioJob({ music: bytes, sample, fold: true }).reply[0]); };
  const s = prepareSong(json, { mus, decodeAsync, asyncAll: true }), seg = { kind: 'stream', sample: 2, node: 0 };
  assert.equal(s.decodeSegment(seg, { aheadMs: 900 }), null, 'asked for ahead of time: null, requested');
  assert.equal(calls.length, 1);
  await new Promise((r) => setTimeout(r, 0));
  const got = s.decodeSegment(seg, { aheadMs: 800 }), ref = decodeMusicSample(mus, null, json.samples[1]);
  assert.equal(got.length, ref.length); assert.deepEqual(got.data[0], ref.data[0]); assert.deepEqual(got.data[1], ref.data[1]);
  const seg3 = { kind: 'stream', sample: 3, node: 0 };
  assert.equal(s.decodeSegment(seg3, { aheadMs: 600 }), null);
  assert.ok(s.decodeSegment(seg3, { aheadMs: 60 }), 'still in flight 60 ms before it is due: decoded here, on time');
  const plain = prepareSong(json, { mus, decodeAsync }); // MicroTalk only (the switch off): an EA-XA bar decodes at once
  assert.ok(plain.decodeSegment(seg, { aheadMs: 900 })); assert.equal(calls.length, 2);
});

test('worker decode: a 6-channel bar folds to stereo in the worker with the same floats as toAudioBuffer', () => {
  const json = readJson('charsel'), sample = json.samples.find((x) => x.kind === 'stream' && x.channels === 6);
  const bytes = new Uint8Array(fs.readFileSync(path.join(MUSIC, json.tracks.find((t) => t.kind === 'stream').file))).subarray(sample.offset, sample.offset + sample.size);
  const dec = decodeMusicSample(bytes, null, { ...sample, offset: 0 });
  const ctx = fakeContext(), ref = toAudioBuffer(ctx, dec, [[0, 2, 4], [1, 3, 5]]);
  const folded = decodeAudioJob({ music: bytes.slice(), sample: { ...sample, offset: 0 }, fold: true }).reply[0];
  assert.equal(folded.channels, 2);
  const buf = toAudioBuffer(ctx, folded, null);
  for (const c of [0, 1]) assert.deepEqual(buf.data[c], ref.data[c]);
});

test('interrupt gate: a stopped context takes no voices, holds are counted, resume is retried', async () => {
  reset();
  let made = null;
  globalThis.AudioContext = class {
    constructor() { made = this; this.currentTime = 1; this.state = 'suspended'; this.destination = {}; this.l = []; }
    addEventListener(t, f) { if (t === 'statechange') this.l.push(f); }
    set(st) { this.state = st; for (const f of this.l) f(); }
    createGain() { return { gain: { value: 1, setValueAtTime() {}, linearRampToValueAtTime() {}, cancelScheduledValues() {} }, connect() {} }; }
    resume() { this.resumes = (this.resumes ?? 0) + 1; if (!this.refuse) this.set('running'); return this.refuse ? Promise.reject(new Error('no')) : Promise.resolve(); }
    suspend() { this.set('suspended'); return Promise.resolve(); }
  };
  const e = createAudioEngine({ interruptGate: true });
  assert.ok(e.unlock()); assert.equal(made.state, 'running'); assert.equal(e.live, true);
  made.set('interrupted');                                  // iOS: a call, Siri, an alarm
  assert.equal(e.live, false, 'no new voices while interrupted'); assert.equal(audioStats.ctxInterrupted, 1);
  await new Promise((r) => setTimeout(r, 650));
  assert.equal(made.state, 'running', 'the retry timer resumed it'); assert.equal(e.live, true);
  e.suspend('hold');                                         // a movie (web/fe-movie.js)
  e.suspend('hidden'); e.resume('hidden');                   // the page hidden and shown during it
  assert.equal(made.state, 'suspended', 'showing the page does not resume the audio under the movie');
  e.unlock(); assert.equal(made.state, 'suspended', 'nor does a key press');
  e.resume('hold'); assert.equal(made.state, 'running');
  assert.equal(audioStats.ctxInterrupted, 1, 'the holds are not interruptions');
  // a resume refused again and again (iOS without a gesture) never leaves it off for good: the timer keeps trying, input tries at once
  made.refuse = true; made.set('interrupted'); const n0 = made.resumes;
  await new Promise((r) => setTimeout(r, 1700)); // 500 + 1000 ms retries
  assert.ok(made.resumes - n0 >= 2, `retried ${made.resumes - n0}`); assert.equal(made.state, 'interrupted'); assert.ok(audioStats.ctxResumeFailed >= 2);
  const n1 = made.resumes; e.unlock(); assert.equal(made.resumes, n1 + 1, 'a key press tries at once');
  made.refuse = false; await new Promise((r) => setTimeout(r, 2100)); // the next retry (2 s) succeeds
  assert.equal(made.state, 'running'); assert.equal(e.live, true);
  const legacy = createAudioEngine({}); legacy.unlock(); const lc = made;
  legacy.suspend('hold'); legacy.suspend('hidden'); legacy.resume('hidden');
  assert.equal(lc.state, 'running', 'switch off: as before, any resume restarts the context'); assert.equal(legacy.live, true);
  delete globalThis.AudioContext;
});

// The audible schedule of a scripted session: every source that sounds (song, voice, bar / bank entry, start, offset, stop), from a
// player pumped every 50 ms. inputs: [ms, fn(player, ctx)] in audio-clock ms.
function audible({ lookahead = 1, declickMs = 0, eventFirst = false, inputs = [], endMs = 40000, id = 'Go' } = {}) {
  const ctx = fakeContext(), segOf = new Map();
  const p = createPathfinderPlayer({ context: ctx, destination: ctx.destination, song: song(id), random: lcg(7), autoPump: false, lookahead, declickMs, eventFirst, clockRandom: true, loopDestination: {} });
  p.onEvent = (e) => { if (e.type === 'segment') segOf.set(ctx.sources.at(-1), e); };
  const todo = [...inputs].sort((a, b) => a[0] - b[0]);
  p.start(0, 1);
  for (let ms = 1000; ms <= endMs; ms += 50) {
    ctx.currentTime = ms / 1000;
    while (todo.length && todo[0][0] <= ms) todo.shift()[1](p, ctx);
    p.pump();
  }
  const out = [];
  for (const src of ctx.sources) {
    const e = segOf.get(src); if (!e) continue;
    const at = Math.max(src.when, 0), stop = src.stopAt ?? Infinity;
    if (stop <= at + 1e-9 || at >= endMs / 1000) continue; // stopped before it started: never sounded (or after the session)
    out.push([e.voice, e.kind === 'stream' ? `s${e.sample}` : `b${e.bankIndex}`, +at.toFixed(6), +src.offset.toFixed(6), Number.isFinite(stop) ? +stop.toFixed(6) : null]);
  }
  return out.sort((a, b) => a[2] - b[2] || a[0] - b[0]);
}
const session = [ // a race: the intensity ramp (28F000), a big air (whoosh duck, loops 7 / 8), a Big Challenge (33: cut + stinger + 1200 ms jump), its stop (39), a pause
  ...Array.from({ length: 60 }, (_, k) => [1500 + k * 200, (p) => p.setIntensity(Math.min(127, k * 3))]),
  [9000, (p) => { p.setLevel(60); }], [9400, (p) => { p.loops.start(); p.send(7); }], [9600, (p) => p.setLevel(20)],
  [13300, (p) => { p.send(8); p.loops.stop(); p.setLevel(127); }],
  [17100, (p) => { p.loops.start(); p.send(33); p.loops.stop(); }],
  [24000, (p) => p.event(39)],
  [27350, (p) => p.pause()], [29800, (p) => p.resume()],
];

test('lookahead 2.5 s: the audible schedule of a race with a big air, a Big Challenge and a pause equals the 1 s one', () => {
  const a = audible({ lookahead: 1, inputs: session }), b = audible({ lookahead: 2.5, inputs: session });
  assert.ok(a.length > 30 && a.some((x) => x[0] === 1), `bars and loop slices (${a.length})`);
  assert.deepEqual(b, a);
  assert.deepEqual(audible({ lookahead: 2.5, eventFirst: true, inputs: session }), a, 'event audio started first: the same schedule');
  // with the declick: the same starts, offsets and bars; only the stops that cut a sounding bar end 5 ms later (the ramp)
  const c = audible({ lookahead: 2.5, declickMs: 5, eventFirst: true, inputs: session });
  assert.deepEqual(c.map((x) => x.slice(0, 4)), a.map((x) => x.slice(0, 4)));
  c.forEach((x, i) => { const d = (x[4] ?? 0) - (a[i][4] ?? 0); assert.ok(Math.abs(d) < 1e-6 || Math.abs(d - 0.005) < 1e-6, `${JSON.stringify(a[i])} -> ${JSON.stringify(x)}`); });
  // the stinger (the loop bank's long ending, track 1) starts exactly at the event, the stream's next part 1200 ms later
  const stinger = a.find((x) => x[0] === 1 && x[2] >= 17.1 - 1e-6);
  near(stinger[2], 17.1, 1e-6); assert.deepEqual(c.find((x) => x[0] === 1 && x[2] >= 17.1 - 1e-6), stinger);
  const next = a.find((x) => x[0] === 0 && x[2] > 17.1);
  near(next[2], 17.1 + 1.2, 1e-6);
  const cut = a.find((x) => x[0] === 0 && x[2] < 17.1 && x[4] != null && Math.abs(x[4] - 17.1) < 1e-6);
  assert.ok(cut, 'the bar playing at the event is cut there'); near(c.find((x) => x[0] === 0 && x[2] === cut[2])[4], 17.105, 1e-6); // its 5 ms ramp starts at the cut
});

test('stingers warmed at the accept (29D6D0): the challenge event plays its stinger from the cache, no decode at the event', async () => {
  const json = readJson('Wobble'), s = prepareSong(json, { mus: file(json, 'stream'), loops: file(json, 'bank') });
  const d0 = s.decodeSegment.bind(s); let bankDecodes = 0; s.decodeSegment = (seg, o) => { if (seg.kind === 'bank') bankDecodes++; return d0(seg, o); };
  const ctx = fakeContext();
  const p = createPathfinderPlayer({ context: ctx, destination: ctx.destination, song: s, random: lcg(3), autoPump: false, clockRandom: true, loopDestination: {} });
  p.start(0, 0); ctx.currentTime = 5; p.pump();
  assert.equal(p.warmEvents([33, 34, 38]), 3, 'Wobble: entries 64, 65, 66');
  await new Promise((r) => setTimeout(r, 30));
  assert.equal(bankDecodes, 3);
  p.loops.start(); p.send(33); p.loops.stop();
  const stinger = ctx.sources.filter((x) => x.buffer.sampleRate === 22050).at(-1);
  near(stinger.when, 5); assert.equal(bankDecodes, 3, 'the stinger came from the cache');
});

test('eventFirst: a stinger the clock passed while the event was processed starts whole (no skipped attack)', () => {
  const run = (eventFirst) => {
    const ctx = fakeContext(), make = ctx.createBufferSource.bind(ctx);
    ctx.createBufferSource = () => { ctx.currentTime += 0.00267; return make(); }; // each source costs a render quantum of clock
    const p = createPathfinderPlayer({ context: ctx, destination: ctx.destination, song: song('Go'), random: lcg(3), autoPump: false, clockRandom: true, loopDestination: {}, lookahead: 2.5, eventFirst });
    p.start(0, 0); ctx.currentTime = 5; p.pump(); ctx.currentTime = 6;
    p.loops.start(); p.send(33); p.loops.stop();
    return ctx.sources.find((x) => x.buffer.sampleRate === 22050);
  };
  const late = run(false), whole = run(true);
  assert.ok(late.offset > 0, 'without: started with its first ms skipped');
  assert.equal(whole.offset, 0); assert.ok(whole.when - 6 <= 0.012, `within ${whole.when - 6} s of the event`);
});

test('worker decode: a failing worker falls back to decoding here, still in time', async () => {
  const json = readJson('Go'), mus = file(json, 'stream');
  const s = prepareSong(json, { mus, decodeAsync: () => Promise.reject(new Error('worker gone')), asyncAll: true }), seg = { kind: 'stream', sample: 4, node: 0 };
  assert.equal(s.decodeSegment(seg, { aheadMs: 900 }), null);
  await new Promise((r) => setTimeout(r, 0));
  assert.equal(s.decodeSegment(seg, { aheadMs: 850 }), null, 'asked again');
  await new Promise((r) => setTimeout(r, 0));
  const got = s.decodeSegment(seg, { aheadMs: 120 }), ref = decodeMusicSample(mus, null, json.samples[3]);
  assert.ok(got); assert.deepEqual(got.data[0], ref.data[0]);
});

test('declick: the whole mix fades out before the context suspends and in when it runs again (also after an OS stop)', async () => {
  const made = [];
  globalThis.AudioContext = class {
    constructor() { made.push(this); this.currentTime = 2; this.state = 'suspended'; this.destination = {}; this.l = []; this.suspends = 0; }
    addEventListener(t, f) { if (t === 'statechange') this.l.push(f); }
    set(st) { this.state = st; for (const f of this.l) f(); }
    createGain() { const ev = []; return { ev, gain: { value: 1, events: ev, setValueAtTime(v, t) { ev.push(['set', v, t]); }, linearRampToValueAtTime(v, t) { ev.push(['ramp', v, t]); }, cancelScheduledValues(t) { ev.push(['cancel', t]); }, cancelAndHoldAtTime(t) { ev.push(['hold', t]); } }, connect(n) { this.dest = n; } }; }
    resume() { this.set('running'); return Promise.resolve(); }
    suspend() { this.suspends++; this.set('suspended'); return Promise.resolve(); }
  };
  const e = createAudioEngine({ declickMs: 5, interruptGate: true }); e.unlock(); const c = made[0], f = e.fader;
  assert.ok(f && e.bus('MUSIC'), 'a fader before the master');
  e.suspend('hidden'); assert.equal(c.suspends, 0, 'not at once');
  events(f.gain.events.slice(-2), [['hold', 2], ['ramp', 0, 2.005]]);
  await new Promise((r) => setTimeout(r, 40)); assert.equal(c.suspends, 1, 'suspended after the fade');
  e.resume('hidden'); events(f.gain.events.slice(-2), [['hold', 2], ['ramp', 1, 2.005]]);
  e.suspend('hold'); e.resume('hold'); await new Promise((r) => setTimeout(r, 40)); assert.equal(c.suspends, 1, 'shown again before the fade ended: no suspend at all');
  c.currentTime = 3; c.set('interrupted'); events(f.gain.events.slice(-1), [['set', 0, 3]]); // an OS stop: silent from there on
  c.set('running'); events(f.gain.events.slice(-2), [['hold', 3], ['ramp', 1, 3.005]]);   // and fades in when it runs again
  delete globalThis.AudioContext;
});

test('sfx: layers decode before the start time is read (switch on); voice counters', async () => {
  reset();
  const bytesOf = (p) => new Uint8Array(fs.readFileSync(path.join(AUDIO, p)));
  const run = async (startAfterDecode) => {
    const ctx = fakeContext(); ctx.currentTime = 5;
    const make = ctx.createBuffer; ctx.createBuffer = (...a) => { ctx.currentTime += 0.02; return make(...a); }; // each decode takes 20 ms
    const engine = { unlocked: true, live: true, context: ctx, bus: () => ({}) };
    const sfx = createSfx({ engine, fetchJson: async (p) => JSON.parse(fs.readFileSync(path.join(AUDIO, p))), fetchBytes: async (p) => bytesOf(p), startAfterDecode });
    await sfx.loadBank(SLOT.MAIN, 'SSX3Menu');
    const v = sfx.play({ slot: SLOT.MAIN, sound: 3, bus: 'UI' }); assert.ok(v);
    const src = ctx.sources.find((s) => s.buffer); return src.when - ctx.currentTime;
  };
  assert.ok(await run(false) < -0.01, 'switch off: the start time was read before the decode (already in the past)');
  near(await run(true), 0, 1e-9);
  // hardware pools (SPU 48 / IOP 8): 60 requests of one sound steal the oldest voices of the same priority
  const ctx = fakeContext(), engine = { unlocked: true, live: true, context: ctx, bus: () => ({}) };
  const sfx = createSfx({ engine, fetchJson: async (p) => JSON.parse(fs.readFileSync(path.join(AUDIO, p))), fetchBytes: async (p) => bytesOf(p) });
  await sfx.loadBank(SLOT.MAIN, 'SSX3Menu');
  for (let i = 0; i < 60; i++) sfx.play({ slot: SLOT.MAIN, sound: 3, bus: 'UI' });
  assert.ok(audioStats.sfxStolen + audioStats.sfxDropped > 0, JSON.stringify(audioStats));
  const gated = createSfx({ engine: { ...engine, live: false }, fetchJson: async (p) => JSON.parse(fs.readFileSync(path.join(AUDIO, p))), fetchBytes: async (p) => bytesOf(p) });
  await gated.loadBank(SLOT.MAIN, 'SSX3Menu');
  const g0 = audioStats.sfxGated;
  assert.equal(gated.play({ slot: SLOT.MAIN, sound: 3, bus: 'UI' }), null, 'a stopped context takes no voice');
  assert.equal(audioStats.sfxGated, g0 + 1, 'counted');
});

test('bankEvict: a bank no slot holds drops its decoded patches after the grace period; loaded again within it, nothing is decoded again', async () => {
  const { setPv } = await import('./pv-flags.js');
  const bytesOf = (p) => new Uint8Array(fs.readFileSync(path.join(AUDIO, p)));
  const make = (evictMs) => createSfx({ engine: { unlocked: false, live: true, context: null, bus: () => ({}) }, fetchJson: async (p) => JSON.parse(fs.readFileSync(path.join(AUDIO, p))), fetchBytes: async (p) => bytesOf(p), evictMs });
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  setPv('bankEvict', true);
  try {
    const sfx = make(1500);
    await sfx.loadBank(SLOT.WORLD9, 'A_slot9');
    const a0 = sfx.cacheStats().by.A_slot9; assert.ok(a0 > 0, 'A decoded');
    // a rider hovering at the A / ARA1 boundary: the slot swaps back and forth within the grace period
    for (let i = 0; i < 4; i++) { await sfx.loadBank(SLOT.WORLD9, 'ARA1_slot9'); await wait(20); await sfx.loadBank(SLOT.WORLD9, 'A_slot9'); await wait(20); }
    let c = sfx.cacheStats(); assert.equal(c.by.A_slot9, a0, 'A kept'); assert.ok(c.by.ARA1_slot9 > 0, 'ARA1 kept within the grace');
    assert.deepEqual(c.evicted, [], 'nothing evicted while hovering');
    await wait(1700); c = sfx.cacheStats();
    assert.equal(c.by.ARA1_slot9, undefined, 'ARA1 (no slot) evicted after the grace'); assert.equal(c.by.A_slot9, a0, 'A (held by slot 9) kept');
    assert.deepEqual(c.evicted, ['ARA1_slot9']);
    sfx.unloadBank(SLOT.WORLD9); await wait(1700);
    assert.equal(sfx.cacheStats().decoded, 0, 'unloaded: evicted'); 
    setPv('bankEvict', false);
    const off = make(50); await off.loadBank(SLOT.WORLD9, 'A_slot9'); await off.loadBank(SLOT.WORLD9, 'ARA1_slot9'); await wait(120);
    assert.ok(off.cacheStats().by.A_slot9 > 0, 'switch off: every bank stays decoded');
  } finally { setPv('bankEvict', null); }
});

test('heatSong: a CTM race event Next heat (WS13 -> 28E8C0(20, 1)) picks a new song from round 2 on (PS2 0x28EC90..0x28ED18)', async () => {
  const { createGameAudio } = await import('./game-audio.js');
  const { setPv } = await import('./pv-flags.js');
  const root = path.join(HERE, 'public');
  let T = 1000; const ctx = { career: true, round: 1, mode: 0 };
  const ga = createGameAudio({ now: () => T, fetchJson: async (p) => JSON.parse(fs.readFileSync(root + p, 'utf8')), fetchBytes: async (p) => new Uint8Array(fs.readFileSync(root + p)) });
  await ga.whenReady(); ga.context = () => ctx;
  await ga.worldLoaded({ courseIndex: 0, singleEvent: false, courseCode: 'ARA1' });
  const since = (n) => ga.timeline().slice(n).map((e) => e.slice(1).join(' '));
  const pump = async (ms) => { for (const end = T + ms; T < end;) { T += 1000 / 60; ga._director.pump(); await new Promise((r) => setImmediate(r)); } };
  setPv('heatSong', true);
  try {
    let n = ga.timeline().length; ga.heat(); await pump(100);
    assert.ok(!since(n).some((e) => /^(pick|request)/.test(e)), `round 1 keeps its song: ${since(n)}`);
    ctx.round = 2; n = ga.timeline().length; ga.heat(); await pump(100);
    // the PS2 at the qualifier's Next heat (ARMSX2 music log local/ps2-capture/music/runs/heat2-audio): 28E8C0(20, 1) from 27A860,
    // PickNextSong (playlist index 14 -> 15), the request callback, PlayMusic(36) two ticks later (PlaySong: Stop, event 36)
    const ev = since(n).filter((e) => /^(code|pick|request|play) /.test(e)).map((e) => e.replace(/^pick .*/, 'pick').replace(/^play .* 36$/, 'play 36'));
    assert.deepEqual(ev, ['code 20', 'pick', 'request 3', 'play 36'], `round 2: ${since(n)}`);
    ctx.round = 3; n = ga.timeline().length; ga.heat(); await pump(100);
    assert.ok(since(n).some((e) => e === 'request 3'), 'the final too');
    setPv('heatSong', false); n = ga.timeline().length; ga.heat(); await pump(100);
    assert.deepEqual(since(n), [], 'switch off: nothing');
  } finally { setPv('heatSong', null); }
});

// The director with a (fake) running context, so songs really play: the CTM restarts and the 29C420 countdown rule against the
// PS2 music logs pause-restart-audio2 / results-restart-audio (local/ps2-capture/music/runs).
async function liveDirector(context) {
  const { createGameAudio } = await import('./game-audio.js');
  const root = path.join(HERE, 'public'); let T = 1000;
  const made = [];
  globalThis.AudioContext = class { constructor() { const c = fakeContext(); c.state = 'running'; c.addEventListener = () => {}; c.resume = () => Promise.resolve(); c.suspend = () => Promise.resolve(); made.push(c); return c; } };
  const ga = createGameAudio({ now: () => T, fetchJson: async (p) => JSON.parse(fs.readFileSync(root + p, 'utf8')), fetchBytes: async (p) => new Uint8Array(fs.readFileSync(root + p)) });
  await ga.whenReady(); ga.context = () => context; ga.engine.unlock();
  const ctx = made[0];
  const pump = async (ms) => { for (const end = T + ms; T < end;) { T += 1000 / 60; ctx.currentTime += 1 / 60; ga._director.pump(); ga._music()?.pump(); await new Promise((r) => setImmediate(r)); } };
  const since = (n) => ga.timeline().slice(n).map((e) => e.slice(1).join(' ')).filter((e) => /^(pick|play|event|stop|fade|request|code|restart|resume|pause) ?/.test(e));
  return { ga, ctx, pump, since, done: () => { delete globalThis.AudioContext; } };
}
test('ctmRestartAudio: the pause Restart keeps the song (resumed at the Yes, event 0 at GO); the results Restart of a finished heat gets a new song at "1"', async () => {
  const { setPv } = await import('./pv-flags.js');
  setPv('ctmRestartAudio', true);
  const ctxInfo = { career: true, round: 1, mode: 0 };
  const L = await liveDirector(ctxInfo);
  try {
    const { ga, pump, since } = L;
    await ga.worldLoaded({ courseIndex: 0, singleEvent: false, courseCode: 'ARA1' }); await pump(3000);
    ga.go(); await pump(4000);
    const song = ga.debug().music; assert.ok(song && ga._music(), 'a song plays');
    // pause Restart (PS2 pause-restart-audio2: game resume + resume at the Yes; nothing at the countdown; GO: resume + event 0)
    ga.pause(true); await pump(500);
    let n = ga.timeline().length;
    ga.restartRun({ fromResults: false }); ga.leaveWorld();       // career-ui.js restartToCard: restartRun, then ui.cb.quit -> leaveWorld
    await ga.runStart({ courseIndex: 0, singleEvent: false, courseCode: 'ARA1' });
    for (const d of [3, 2, 1]) { ga._countdown?.(d); await pump(1000); }
    ga.go(); await pump(500);
    assert.deepEqual(since(n), ['restart 0', 'resume', 'event 0'], 'pause Restart');
    assert.equal(ga.debug().music, song, 'the same song');
    // the heat ends (event 10: the song's ending), the results' Restart in round 1 (PS2 results-restart-audio: WS13 code 20 without
    // a pick; "1": pick, play 0, paused; GO: resume + event 0)
    ga.finish({ place: 0 }); await pump(30000);
    assert.ok(ga._music().finished, 'the ending has played out');
    n = ga.timeline().length;
    ga.restartRun({ fromResults: true }); ga.leaveWorld(); setPv('heatSong', true); ga.heat(); setPv('heatSong', null);
    await ga.runStart({ courseIndex: 0, singleEvent: false, courseCode: 'ARA1' });
    if (process.env.DBG) console.log('dbg', ga._music()?.finished, ga.debug().music, JSON.stringify(ga.debug().director.timers));
    for (const d of [3, 2, 1]) { ga._countdown?.(d); await pump(1000); }
    ga.go(); await pump(500);
    const ev = since(n).map((e) => e.replace(/^pick .*/, 'pick').replace(/^play .* 0$/, 'play 0'));
    assert.deepEqual(ev, ['restart 1', 'code 20', 'pick', 'play 0', 'event 0'], 'results Restart');
    assert.ok(ga._music() && !ga._music().finished);
  } finally { setPv('ctmRestartAudio', null); L.done(); }
});

let failed = 0;
for (const [name, fn] of tests) {
  try { await fn(); console.log(`ok   ${name}`); } catch (e) { failed++; console.log(`FAIL ${name}\n${e.stack}`); }
}
if (failed) { console.log(`${failed} failed`); process.exit(1); }
console.log('audio glitches: declick, worker decode, interrupt gate, sfx start, counters OK');
process.exit(0);
