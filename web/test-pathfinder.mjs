// Pathfinder runtime tests (node): graph walking, event quantisation, loop-overlay beat math, Web Audio scheduling
// against a fake AudioContext, and an optional offline WAV render (--render DIR).
//   node test-pathfinder.mjs [--render /path/to/dir]
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import assert from 'node:assert/strict';
import { prepareSong, createGraphWalker, createPathfinderCore, createPathfinderPlayer, loadSong, prefetchSongStart,
  raceIntensity, rampLevel, renderSongOffline } from './pathfinder.js';
import { encodeWav, decodeMusicSample } from './audio-decode.js';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const MUSIC = path.join(HERE, 'public/assets/AUDIO/music');
if (!fs.existsSync(path.join(MUSIC, 'Go.json'))) {
  console.log('test-pathfinder: skipped (run tools/export_audio.py to create web/public/assets/AUDIO)');
  process.exit(0);
}
const readJson = (id) => JSON.parse(fs.readFileSync(path.join(MUSIC, `${id}.json`)));
const songs = new Map();
function song(id, { audio = false } = {}) {
  const key = id + (audio ? '+audio' : '');
  if (!songs.has(key)) {
    const json = readJson(id);
    const file = (kind) => { const t = json.tracks.find((x) => x.kind === kind); return t ? new Uint8Array(fs.readFileSync(path.join(MUSIC, t.file))) : null; };
    songs.set(key, prepareSong(json, audio ? { mus: file('stream'), loops: file('bank') } : {}));
  }
  return songs.get(key);
}
function lcg(seed = 1) { let s = seed >>> 0; return () => { s = (Math.imul(s, 1664525) + 1013904223) >>> 0; return s / 4294967296; }; }
const tests = [];
const test = (name, fn) => tests.push([name, fn]);
const near = (a, b, tol = 0.01) => assert.ok(Math.abs(a - b) <= tol, `${a} != ${b}`);

// ---------------------------------------------------------------------------------------------------------------
test('every song graph: audio nodes point at samples of their own track, walks stay valid', () => {
  const ids = fs.readdirSync(MUSIC).filter((f) => f.endsWith('.json')).map((f) => f.slice(0, -5));
  assert.ok(ids.length >= 45);
  for (const id of ids) {
    const s = song(id), g = s.graph;
    for (const n of g.nodes) {
      if (n.sample > 0) {
        assert.ok(n.sample <= g.samples.length, `${id} node ${n.index} sample ${n.sample}`);
        assert.equal(g.samples[n.sample - 1].track, n.track, `${id} node ${n.index} track`);
      }
      for (const b of n.branches) assert.ok(b.node === -1 || (b.node >= 0 && b.node < g.nodes.length + 40), `${id} branch`);
    }
    for (const start of [0, 36, 12, 1]) {
      if (!g.events[start]?.actions.length) continue;
      for (const intensity of [0, 64, 127]) {
        const w = createGraphWalker(s, lcg(start * 7 + intensity), { intensity });
        w.start(start, 0);
        let prevEnd = null;
        for (let i = 0; i < 120; i++) {
          const seg = w.step(0); if (!seg) break;
          assert.ok(seg.sample >= 1 && seg.sample <= g.samples.length, `${id} e${start} sample`);
          assert.equal(g.samples[seg.sample - 1].kind, 'stream');
          if (prevEnd != null) near(seg.start, prevEnd, 1e-6); // gapless
          prevEnd = seg.end;
        }
      }
    }
  }
});

test('Go from event 0: intro part (node 3 -> 5..20, section 1) then the 3-way body; commits 500 ms before each end', () => {
  const s = song('Go');
  const w = createGraphWalker(s, lcg(3));
  w.start(0, 0);
  const bar = (56888 / 32000) * 1000;
  const segs = [];
  for (let i = 0; i < 17; i++) segs.push(w.step(0));
  assert.deepEqual(segs.slice(0, 16).map((x) => x.node), Array.from({ length: 16 }, (_, i) => 5 + i));
  assert.equal(segs[0].sample, 87); assert.equal(segs[0].start, 0);
  segs.slice(0, 16).forEach((x, i) => { assert.equal(x.section, 1); near(x.start, i * bar, 1e-6); if (i) near(x.commit, i * bar - 500, 1e-6); });
  // node 20 -> 4 (end, 1 branch) -> 21 (3-way by intensity 0) -> 23
  assert.equal(segs[16].node, 23);
  assert.deepEqual(w.core.state.regs.slice(1, 6), [5, 5, 5, 5, 5]); // op 0x0A -33 after the jump saves node 5
});

test('branch value = intensity (voice+1): inclusive ranges, first match wins', () => {
  const s = song('Go');
  const pick = (v) => { const w = createGraphWalker(s, lcg(1), { intensity: v }); w.start(0, 0); for (let i = 0; i < 16; i++) w.step(0); return w.step(0).node; };
  assert.equal(pick(0), 23); assert.equal(pick(50), 23); assert.equal(pick(51), 39); assert.equal(pick(100), 39);
  assert.equal(pick(101), 55); assert.equal(pick(127), 55);
});

test('PS2 trace (ARMSX2, Snow Jam, Avalanche): intensity ramp after GO picks 34,35,36,5,6,7,24,25,26,43,44,45,46', () => {
  // Observed on hardware emulation: voice+0x36 commits and the intensity byte at each commit (28F000 ramp).
  const s = song('Avalanche');
  const w = createGraphWalker(s, lcg(1), { intensity: 127 });
  w.start(0, 0);
  const first = w.step(0);
  assert.equal(first.node, 34);
  const intensities = [127, 127, 0, 17, 35, 52, 70, 87, 104, 121, 127, 127];
  const got = [first.node];
  for (const v of intensities) { w.setIntensity(v); got.push(w.step(0).node); }
  assert.deepEqual(got, [34, 35, 36, 5, 6, 7, 24, 25, 26, 43, 44, 45, 46]);
});

test('SendEvent quantisation: a jump lands after the audio already committed (commit point = end - 500 ms)', () => {
  const s = song('Go'), bar = (56888 / 32000) * 1000;
  // Event 36 before node 5's successor is committed (t < bar - 500): idle part 439 -> 441 follows node 5.
  let c = createPathfinderCore(s, { random: lcg(1) });
  c.sendEvent(0, 0); c.sendEvent(36, 1000); c.advanceTo(4000);
  let main = c.outputs.filter((o) => o.type === 'segment' && o.voice === 0);
  assert.deepEqual(main.slice(0, 3).map((o) => o.node), [5, 441, 442]);
  near(main[1].start, bar, 1e-6); assert.equal(main[1].commit, 1000); assert.equal(main[1].section, 2);
  assert.equal(c.state.regs[3], 5);
  // After the commit point the next bar (6) is already queued: the jump plays after it.
  c = createPathfinderCore(s, { random: lcg(1) });
  c.sendEvent(0, 0); c.sendEvent(36, 1500); c.advanceTo(6000);
  main = c.outputs.filter((o) => o.type === 'segment' && o.voice === 0);
  assert.deepEqual(main.slice(0, 3).map((o) => o.node), [5, 6, 441]);
  near(main[2].start, 2 * bar, 1e-6); assert.equal(c.state.regs[3], 6);
  // Idle part (end node 440, flags 0xF) loops until event 37 returns to register 3 (-19).
  c.advanceTo(2 * bar + 16 * bar * 2 + 100);
  main = c.outputs.filter((o) => o.type === 'segment' && o.voice === 0);
  assert.deepEqual(main.slice(2, 2 + 33).map((o) => o.node), [...Array.from({ length: 16 }, (_, i) => 441 + i), ...Array.from({ length: 16 }, (_, i) => 441 + i), 441]);
  const t37 = c.time; c.sendEvent(37, t37); c.advanceTo(t37 + 5000);
  main = c.outputs.filter((o) => o.type === 'segment' && o.voice === 0 && o.commit >= t37);
  assert.equal(main[0].node, 6); assert.equal(main[0].commit, t37);
  assert.equal(main[1].node, 7);
});

test('loop counter end node (flags 0x6040): part plays 7 times, then branch value 0 exits', () => {
  const s = song('Go'), bar = (56888 / 32000) * 1000;
  const c = createPathfinderCore(s, { random: lcg(2) });
  c.sendEvent(1, 0); // op10 reg1, jump 383 (385..400 + end 384)
  c.advanceTo(bar * 16 * 8);
  const nodes = c.outputs.filter((o) => o.type === 'segment' && o.voice === 0).map((o) => o.node);
  assert.equal(nodes.filter((n) => n === 385).length, 7);
  const exit = nodes.indexOf(403);
  assert.equal(exit, 16 * 7); // 7 passes of 16 bars, then part 401 (403..418)
  assert.equal(c.voice(0).loopNode, 402); // next counter: 402 (0xF, endless) now owns voice+0x2C
});

test('end of song: event 10 -> 379 -> 381, 382 -> end node 380 without branches; finished after the last bar', () => {
  const s = song('Go'), bar = (56888 / 32000) * 1000;
  const c = createPathfinderCore(s, { random: lcg(2) });
  c.sendEvent(0, 0); c.sendEvent(10, 100); c.advanceTo(20000);
  const main = c.outputs.filter((o) => o.type === 'segment' && o.voice === 0).map((o) => o.node);
  assert.deepEqual(main, [5, 381, 382]);
  assert.equal(c.finished(3 * bar - 1), false);
  assert.equal(c.finished(3 * bar + 1), true);
  assert.ok(c.outputs.some((o) => o.type === 'stop' && o.voice === 0 && Math.abs(o.time - (3 * bar - 500)) < 1e-6));
});

test('events outside the table and empty events do nothing', () => {
  const s = song('Go');
  const c = createPathfinderCore(s, { random: lcg(2) });
  c.sendEvent(0, 0);
  assert.equal(c.sendEvent(200, 10), false);
  assert.equal(c.sendEvent(12, 20), true); // exists, no actions
  c.advanceTo(3000);
  assert.deepEqual(c.outputs.filter((o) => o.type === 'segment').map((o) => o.node), [5, 6]);
  assert.equal(c.state.events.length, 0);
});

test('loops (track 1): event 7 waits for the master beat; slice k = bank entry value+k-1 on master beat k', () => {
  const s = song('Go'), bar = (56888 / 32000) * 1000, beat = bar / 4;
  // random 0.1 -> 12 -> branch 0-43 -> node 513 (sample 474 = bank 32..35); 0.5 -> 64 -> 521 (bank 16..19)
  for (const [rnd, node, bank] of [[0.1, 513, 32], [0.5, 521, 16], [0.9, 529, 0]]) {
    const c = createPathfinderCore(s, { random: () => rnd });
    c.sendEvent(0, 0);
    const t7 = 3 * bar + 2.5 * beat; // mid beat 3 of node 8's bar: next master beat = beat index 3 -> slice 4
    c.sendEvent(7, t7);
    const pending = c.state.events.length;
    assert.equal(pending, 1); // op 0x04 on track 1 is still waiting
    c.advanceTo(t7 + 3 * bar);
    const loops = c.outputs.filter((o) => o.type === 'segment' && o.voice === 1);
    near(loops[0].start, 3 * bar + 3 * beat, 1e-6);
    assert.equal(loops[0].node, node); assert.equal(loops[0].slice, 4); assert.equal(loops[0].bankIndex, bank + 3);
    // then one slice per master beat; the next node (successor) restarts at slice 1 on the downbeat
    for (let i = 1; i < loops.length; i++) near(loops[i].start - loops[i - 1].start, beat, 1e-6);
    assert.equal(loops[1].node, node + 1); assert.equal(loops[1].slice, 1); near(loops[1].start, 4 * bar, 1e-6);
    assert.deepEqual(loops.slice(1, 5).map((o) => o.slice), [1, 2, 3, 4]);
    // event 8: flush + stop at once
    const t8 = c.time; c.sendEvent(8, t8);
    assert.ok(c.outputs.some((o) => o.type === 'cut' && o.voice === 1 && o.time === t8));
    assert.equal(c.voice(1).active, false);
    c.advanceTo(t8 + 5000);
    assert.equal(c.outputs.filter((o) => o.type === 'segment' && o.voice === 1 && o.start > t8).length, 0);
  }
});

test('challenge start (event 33): stream cut, loop ending 537 at once, stream restarts 1200 ms later at part 491', () => {
  const s = song('Go');
  const c = createPathfinderCore(s, { random: () => 0.2 }); // 25 -> 0-69 -> 539 (bank 49, 2658 ms)
  c.sendEvent(0, 0);
  c.sendEvent(33, 5000);
  c.advanceTo(12000);
  assert.ok(c.outputs.some((o) => o.type === 'cut' && o.voice === 0 && o.time === 5000));
  const loop = c.outputs.find((o) => o.type === 'segment' && o.voice === 1);
  assert.equal(loop.node, 539); assert.equal(loop.bankIndex, 49); assert.equal(loop.start, 5000);
  const main = c.outputs.filter((o) => o.type === 'segment' && o.voice === 0 && o.commit >= 5000);
  assert.equal(main[0].node, 493); assert.equal(main[0].start, 6200); assert.equal(main[0].section, 3);
  assert.equal(c.state.regs[4], 8); // saved before the cut (node 8 was the committed bar at 5000 ms)
});

test('ramp curves (3D5A98) and race intensity (28F000)', () => {
  near(rampLevel(127, 0, 0.25, 1), 95.25); near(rampLevel(0, 127, 0.25, 1), 31.75);
  near(rampLevel(127, 0, 0.5, 2), 127 - 127 * 0.25); near(rampLevel(0, 127, 0.5, 2), 127 * 0.75);
  near(rampLevel(127, 0, 0.5, 3), 127 / 0.5 / 25); assert.equal(rampLevel(127, 0, 0.01, 3), 127);
  assert.equal(rampLevel(127, 0, 1, 1), 0);
  assert.equal(raceIntensity(0), 0); assert.equal(raceIntensity(400), 63); assert.equal(raceIntensity(800), 127);
  assert.equal(raceIntensity(1000, 1), 127); assert.equal(raceIntensity(600, 2), 63);
});

// ---------------------------------------------------------------------------------------------------------------
// Web Audio player against a fake AudioContext.
function fakeContext() {
  const param = () => ({ value: 1, setValueAtTime(v) { this.value = v; }, linearRampToValueAtTime() {}, cancelScheduledValues() {}, setValueCurveAtTime() {} });
  const ctx = {
    currentTime: 0, destination: {}, sources: [],
    createGain() { return { gain: param(), connect() {}, disconnect() {} }; },
    createBiquadFilter() { return { type: '', frequency: param(), Q: param(), connect() {}, disconnect() {} }; },
    createBuffer(ch, len, rate) { return { numberOfChannels: ch, length: len, sampleRate: rate, duration: len / rate, copyToChannel() {} }; },
    createBufferSource() {
      const s = { buffer: null, connect(n) { s.dest = n; }, disconnect() { s.disconnected = true; },
        start(when, off = 0) { s.when = when; s.offset = off; }, stop(t = ctx.currentTime) { s.stopAt = t; } };
      ctx.sources.push(s); return s;
    },
  };
  return ctx;
}

test('player: sources scheduled gaplessly ~0.5-1 s ahead; late event replays the provisional future exactly', () => {
  const s = song('Go', { audio: true }), bar = (56888 / 32000);
  const ctx = fakeContext();
  const p = createPathfinderPlayer({ context: ctx, destination: ctx.destination, song: s, random: lcg(5), autoPump: false });
  p.start(0, 0);
  assert.equal(ctx.sources.length, 1); near(ctx.sources[0].when, 0, 1e-9);
  ctx.currentTime = 1.3; p.pump(); // node 6 committed at bar-0.5 s = 1.278 s
  assert.equal(ctx.sources.length, 2); near(ctx.sources[1].when, bar, 1e-9);
  assert.equal(ctx.sources[1].buffer.sampleRate, 32000); near(ctx.sources[1].buffer.duration, bar, 1e-9);
  // Event 36 at 1.5 s (after the commit of 6): 441 follows 6
  ctx.currentTime = 1.5; p.event(36);
  ctx.currentTime = 3.2; p.pump();
  const starts = ctx.sources.filter((x) => !x.stopAt).map((x) => +x.when.toFixed(4));
  assert.deepEqual(starts, [0, +bar.toFixed(4), +(2 * bar).toFixed(4)]);
  assert.equal(p.position.node, 6);
  assert.equal(p.event(36), false); // same event again: ignored (2B3BC0)
  // Provisional look-ahead: pump at 3.3 s simulates to 4.3 s (441's successor committed at 4.83 s: not yet).
  ctx.currentTime = 3.3; p.pump();
  assert.equal(ctx.sources.filter((x) => !x.stopAt).length, 3);
  ctx.currentTime = 4.0; p.pump();
  assert.equal(ctx.sources.filter((x) => !x.stopAt).length, 4); // 442 at 3*bar, scheduled ~1.3 s ahead
  near(ctx.sources[3].when, 3 * bar, 1e-9);
});

test('player: pause freezes the position, resume restarts sources with the right offset', () => {
  const s = song('Go', { audio: true }), bar = (56888 / 32000);
  const ctx = fakeContext();
  const p = createPathfinderPlayer({ context: ctx, destination: ctx.destination, song: s, random: lcg(5), autoPump: false });
  p.start(0, 10);
  ctx.currentTime = 10.4; p.pump();
  p.pause();
  assert.ok(ctx.sources.every((x) => x.stopAt != null));
  ctx.currentTime = 20; p.pump(); near(p.position.timeMs, 400, 1e-6);
  const before = ctx.sources.length; p.event(36); assert.equal(ctx.sources.length, before); // no sound while paused
  p.resume();
  const live = ctx.sources.filter((x) => x.stopAt == null);
  // node 5 resumes at its offset; the event sent at song 400 ms committed 441 right after it (look-ahead 1 s)
  assert.equal(live.length, 2); near(live[0].when, 20, 1e-9); near(live[0].offset, 0.4, 1e-9);
  near(live[1].when, 20 - 0.4 + bar, 1e-9); assert.equal(live[1].offset, 0);
  ctx.currentTime = 20 + 2 * bar - 0.4 - 0.45; p.pump(); // the event sent while paused (at song 400 ms) -> 441
  const next = ctx.sources.filter((x) => x.stopAt == null);
  assert.equal(next.length, 3); near(next[2].when, 20 - 0.4 + 2 * bar, 1e-9);
  const q = p.core.state.voices[0].queue; assert.deepEqual([1, 2].map((k) => q.find((x) => Math.abs(x.start - k * bar * 1000) < 1).node), [441, 442]);
});

test('player: loops need loops.start() (2B4620); event 7 slices land on master beats; finished after event 10', () => {
  const s = song('Go', { audio: true }), bar = (56888 / 32000), beat = bar / 4;
  const ctx = fakeContext();
  const p = createPathfinderPlayer({ context: ctx, destination: ctx.destination, song: s, random: () => 0.1, autoPump: false });
  p.start(0, 0);
  ctx.currentTime = 3 * bar + 2.5 * beat; p.loops.start(); p.event(7);
  ctx.currentTime += 0.5; p.pump();
  const loops = ctx.sources.filter((x) => x.buffer.sampleRate === 22050);
  assert.ok(loops.length >= 2);
  near(loops[0].when, 3 * bar + 3 * beat, 1e-9); near(loops[1].when, 4 * bar, 1e-9);
  // music mode: each slice latches the loop level when it starts (3D41A8 volume; 2AF6C0 gated by audio+0x6280)
  ctx.currentTime = 3 * bar + 3 * beat + 0.05; p.loops.setLevel(40);
  near(loops[0].dest.gain.value, 1, 1e-9); near(loops[1].dest.gain.value, 0.4, 1e-9);
  ctx.currentTime = 4 * bar + 0.1; p.event(8);
  ctx.currentTime += 0.2; p.pump();
  assert.ok(ctx.sources.filter((x) => x.buffer.sampleRate === 22050 && x.when > 4 * bar + 0.1).length === 0 ||
    ctx.sources.filter((x) => x.buffer.sampleRate === 22050 && x.when > 4 * bar + 0.1).every((x) => x.stopAt != null));
  p.event(10);
  assert.equal(p.finished, false);
  for (let t = ctx.currentTime; t < 20; t += 0.25) { ctx.currentTime = t; p.pump(); }
  assert.equal(p.finished, true);
});

test('loadSong uses catalog-relative paths', async () => {
  const seen = [];
  const s = await loadSong('Go', {
    fetchJson: async (p) => { seen.push(p); return readJson('Go'); },
    fetchBytes: async (p) => { seen.push(p); return new Uint8Array(4); },
  });
  assert.deepEqual(seen, ['music/Go.json', 'music/go.mus', 'music/goloops0.mus']);
  assert.equal(s.graph.nodes.length, 541);
});

test('Screw Up (MicroTalk bars): worker decode ahead of time, the opening bar prefetched with replayed random draws', async () => {
  const json = readJson('Screw_Up');
  const mus = new Uint8Array(fs.readFileSync(path.join(MUSIC, json.tracks.find((t) => t.kind === 'stream').file)));
  const jobs = [];
  // Stand-in for musicDecodeWorker(): same function, settles when the test says so.
  const decodeAsync = (bytes, sample) => new Promise((resolve) => jobs.push(() => resolve({ ...decodeMusicSample(bytes.slice(), null, sample), viaWorker: true })));
  const s = prepareSong(json, { mus, decodeAsync });
  const seg = { kind: 'stream', sample: 1, start: 0 };
  assert.equal(s.decodeSegment(seg, { aheadMs: 500 }), null, 'a bar needed later goes to the worker');
  assert.equal(jobs.length, 1);
  assert.equal(s.decodeSegment(seg, { aheadMs: 300 }), null, 'in flight: no second job');
  assert.equal(jobs.length, 1);
  jobs.shift()(); await new Promise((r) => setTimeout(r, 0));
  const got = s.decodeSegment(seg, { aheadMs: 200 }), ref = decodeMusicSample(mus, null, json.samples[0]);
  assert.ok(got.viaWorker); assert.equal(got.length, ref.length);
  for (let c = 0; c < 2; c++) assert.deepEqual(got.data[c], ref.data[c]);
  assert.ok(!s.decodeSegment(seg, { aheadMs: 0 }).viaWorker, 'due now without a ready job: decoded here');
  // Start: the dry run's first bar is decoded before the player starts, and the player commits the same bar.
  const start = prefetchSongStart(s, 0, { intensity: 127 });
  await new Promise((r) => setTimeout(r, 0));
  assert.ok(jobs.length >= 1, 'opening bar requested');
  while (jobs.length) jobs.shift()();
  const random = await start;
  assert.equal(typeof random, 'function');
  const core = createPathfinderCore(s, { random, intensity: 127 });
  core.sendEvent(0, 0); core.advanceTo(1000);
  const first = core.state.voices[0].queue.find((x) => x.start === 0);
  assert.ok(first && first.kind === 'stream');
  assert.ok(s.decodeSegment(first, { aheadMs: 0 })?.viaWorker, 'opening bar ready from the worker');
  assert.equal(jobs.length, 0, 'no main-thread fallback job');
  assert.equal(await prefetchSongStart(song('Go'), 0), null, 'EA-XA songs need no prefetch');
});

// ---------------------------------------------------------------------------------------------------------------
let passed = 0;
for (const [name, fn] of tests) { await fn(); passed++; console.log('ok -', name); }

const renderAt = process.argv.indexOf('--render');
if (renderAt > 0) {
  const dir = process.argv[renderAt + 1];
  fs.mkdirSync(dir, { recursive: true });
  const go = song('Go', { audio: true });
  const renders = [
    ['go-event0-event10-at40s.wav', { script: [{ t: 0, event: 0 }, { t: 40000, event: 10 }], durationMs: 60000,
      intensityRamp: { tier: 0, fromMs: 0 } }],
    // A 8 s big air (28C8C8): event 7 at take-off, stream 127 -> 20 while the loops rise to 100 % by mid-flight,
    // event 8 at landing (stream back to 127 at once); then event 36 (idle part) and 37 (back to the saved bar).
    ['go-event0-bigair-loops.wav', { script: [{ t: 0, event: 0 }, { t: 14000, event: 7 },
      ...Array.from({ length: 41 }, (_, i) => ({ t: 14000 + i * 100, level: 127 - (107 * i) / 40, loopsPct: (100 * i) / 40 })),
      { t: 22000, event: 8 }, { t: 22000, level: 127 }, { t: 30000, event: 36 }, { t: 44000, event: 37 }],
      durationMs: 60000, intensityRamp: { tier: 0, fromMs: 0 }, loopsPct: 0, random: lcg(9) }],
  ];
  for (const [name, opts] of renders) {
    const r = renderSongOffline(go, opts);
    fs.writeFileSync(path.join(dir, name), encodeWav(r));
    const nodes = r.segments.map((x) => `${x.voice ? 'L' : ''}${x.node}@${(x.start / 1000).toFixed(2)}`);
    console.log('rendered', path.join(dir, name), nodes.join(' '));
  }
}
console.log(`test-pathfinder: ${passed} passed`);
