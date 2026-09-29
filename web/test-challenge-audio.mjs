// Big Challenge audio (docs/audio-logic.md 3.9, pv bigChallengeAudio): 29D6E0 / 29D8E0 / 29DBB0 and the songs' challenge
// stingers, checked against the song data and the PS2 runs (ARMSX2, Snow Jam free ride, Speed Demon = type 3 -> event 38):
//   Wobble  ticks 3076 / 3197 / 3436 / 3526: track 1 committed node 650 (entry 64) / 649 (entry 66) / 650 / 649 one tick after
//           SendEvent(38); track 0 cut, then node 592 (head 590) 75 ticks later; the stinger ends 166 ticks after it started
//   Avalanche tick 3196: node 488 (entry 49), then 432 (head 430) 75 ticks later
//   both: +0x5FD0 = 3, +0x5FD4 = 1, +0x5FD8 = 0, voice1 +4 = 90, +0x34 = 127, the overlay entry's volume byte 114
//   hub song (Peak1, stage 1): 29D6E0 -> 28D488 PickNextSong -> 28CF98 PlayMusic(0): the playlist song, no event 38.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { prepareSong, createPathfinderCore, createPathfinderPlayer } from './pathfinder.js';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const PUBLIC = path.join(HERE, 'public'), MUSIC = path.join(PUBLIC, 'assets/AUDIO/music');
if (!fs.existsSync(path.join(MUSIC, 'Wobble.json'))) { console.log('test-challenge-audio: skipped (no exported audio)'); process.exit(0); }
const readJson = (id) => JSON.parse(fs.readFileSync(path.join(MUSIC, `${id.replace(/ /g, '_')}.json`)));
const song = (id, audio = false) => {
  const json = readJson(id);
  const file = (kind) => { const t = json.tracks.find((x) => x.kind === kind); return t ? new Uint8Array(fs.readFileSync(path.join(MUSIC, t.file))) : null; };
  return prepareSong(json, audio ? { mus: file('stream'), loops: file('bank') } : {});
};
const catalog = JSON.parse(fs.readFileSync(path.join(PUBLIC, 'assets/AUDIO/catalog.json'), 'utf8'));
const licensed = catalog.songs.filter((s) => s.ADDTOFE).map((s) => s.id);

// 1. Song data: every licensed song answers 33 / 34 / 38 with the same actions; the stinger is its loop bank's long ending,
//    picked by a random part head; only Wobble has a third one (entry 66).
{
  const stingers = new Map();
  for (const id of licensed) {
    const s = song(id), g = s.graph;
    for (const e of [33, 34, 38]) {
      const ops = g.events[e].actions.map((a) => a.op);
      assert.deepEqual(ops, [0x0a, 0x04, 0x04, 0x02, 0x04], `${id} event ${e}`);
      assert.equal(g.events[e].actions[3].word, 1200, `${id} event ${e}: 1200 ms before the challenge part`);
    }
    // the branch value of the head for every (now_ms / 23) & 0x7F -> the bank entry it plays
    const byValue = [];
    for (let v = 0; v < 128; v++) {
      const core = createPathfinderCore(s, { intensity: 127, clock: () => v * 23 });
      core.sendEvent(0, 0); core.advanceTo(8000); core.sendEvent(33, 8000); core.advanceTo(8001);
      const seg = core.state.voices[1].queue.find((q) => q.start >= 8000 - 1e-6);
      assert.ok(seg, `${id}: a stinger at the event`); assert.equal(seg.start, 8000);
      byValue.push(seg.bankIndex);
      assert.ok(!core.state.voices[0].queue.some((q) => q.start >= 8000 - 1e-6 && q.start < 9200 - 1e-6), `${id}: track 0 cut until +1200 ms`);
    }
    stingers.set(id, byValue);
  }
  const w = stingers.get('Wobble');
  assert.deepEqual([...new Set(w)].sort(), [64, 65, 66]);
  assert.ok(w.slice(0, 34).every((x) => x === 65) && w.slice(34, 81).every((x) => x === 66) && w.slice(81).every((x) => x === 64), 'Wobble: 0-33 -> 65, 34-80 -> 66, 81-127 -> 64');
  for (const [id, byValue] of stingers) if (id !== 'Wobble') assert.ok(new Set(byValue).size <= 2, `${id}: at most two stingers`);
  // the same 2658 / 2786 ms pair in every bank, Wobble's third one only in wbloops0
  const wobble = song('Wobble', true), bank = wobble.bank;
  const len = (s, k) => s.decodeSegment({ kind: 'bank', bankIndex: k, sample: s.graph.nodes.find((n) => n.sample > 0 && s.graph.samples[n.sample - 1].kind === 'bank').sample }).length;
  assert.equal(bank.entries.length, 67);
  assert.deepEqual([64, 65, 66].map((k) => len(wobble, k)), [58624, 61440, 61003]); // 2658 / 2786 / 2766 ms at 22050 Hz
  console.log('challenge audio: 33 / 34 / 38 in all 35 songs; Wobble 0-33 -> 65, 34-80 -> 66 (its own stinger), 81-127 -> 64');
}

// 2. The PS2 runs, replayed on the core (event 38 at the same song node, the branch value the PS2 clock gave).
{
  const w = song('Wobble');
  for (const [value, node, entry] of [[20, 648, 65], [50, 649, 66], [100, 650, 64]]) {
    const core = createPathfinderCore(w, { intensity: 127, clock: () => value * 23 });
    core.sendEvent(0, 0); core.advanceTo(30000); core.sendEvent(38, 30000); core.advanceTo(33000);
    const v1 = core.state.voices[1].queue.find((q) => q.start >= 30000 - 1e-6);
    assert.deepEqual([v1.node, v1.bankIndex], [node, entry]);
    const next = core.state.voices[0].queue.find((q) => q.start >= 30000 - 1e-6);
    assert.deepEqual([next.node, next.start], [592, 31200], 'Wobble event 38: the challenge part 590 -> 592 after 1200 ms (PS2: +75 ticks)');
    assert.ok(Math.abs(v1.end - v1.start - [2786, 2766, 2658][[648, 649, 650].indexOf(node)]) < 1, 'the stinger plays whole (PS2: 166 ticks)');
  }
  const av = createPathfinderCore(song('Avalanche'), { intensity: 127, clock: () => 10 * 23 });
  av.sendEvent(0, 0); av.advanceTo(20000); av.sendEvent(38, 20000); av.advanceTo(22000);
  assert.deepEqual([av.state.voices[1].queue.at(-1).node, av.state.voices[1].queue.at(-1).bankIndex], [488, 49]);
  assert.equal(av.state.voices[0].queue.find((q) => q.start >= 20000 - 1e-6).node, 432);
  console.log('challenge audio: the PS2 Wobble (649 / 650) and Avalanche (488) starts replay on the core');
}

// 3. Player: the overlay needs the loop bank attached (2B4620) before the event; with loopDestination its slices play the
//    3D41A8 byte on that output (trunc(pct x 0.01 x 127) / 127: 100 -> 1, 90 -> 114 / 127).
function fakeContext() {
  const param = () => ({ value: 1, setValueAtTime(v) { this.value = v; }, linearRampToValueAtTime() {}, cancelScheduledValues() {}, setValueCurveAtTime() {} });
  const node = (extra = {}) => ({ outs: [], connect(n) { this.outs.push(n); return n; }, disconnect() { this.outs = []; }, ...extra });
  const ctx = {
    currentTime: 0, destination: node(), sources: [],
    createGain() { return node({ gain: param() }); },
    createBiquadFilter() { return node({ type: '', frequency: param(), Q: param() }); },
    createBuffer(ch, len, rate) { return { numberOfChannels: ch, length: len, sampleRate: rate, duration: len / rate, copyToChannel() {} }; },
    createBufferSource() {
      const s = node({ buffer: null, detune: param(), playbackRate: param(), loop: false, start(when, off = 0) { s.when = when; s.offset = off; }, stop(t = ctx.currentTime) { s.stopAt = t; } });
      ctx.sources.push(s); return s;
    },
  };
  return ctx;
}
const reaches = (n, target, seen = new Set()) => n === target || (!seen.has(n) && (seen.add(n), (n.outs || []).some((o) => reaches(o, target, seen))));
{
  const w = song('Wobble', true);
  // old path (no attach): the event commits the stinger but nothing plays on the overlay
  let ctx = fakeContext(), music = ctx.createGain();
  let p = createPathfinderPlayer({ context: ctx, destination: music, song: w, intensity: 127, autoPump: false, clockRandom: true });
  p.start(0, 0); ctx.currentTime = 30; p.pump(); p.event(38); ctx.currentTime = 30.5; p.pump();
  assert.equal(ctx.sources.filter((s) => s.buffer.sampleRate === 22050).length, 0, 'without 2B4620 the stinger is silent (the port before pv bigChallengeAudio)');
  p.stop();
  // attached, the overlay on its own output (the master): the clock picks the branch
  for (const [pct, gain] of [[100, 1], [90, 114 / 127]]) {
    ctx = fakeContext(); music = ctx.createGain(); const master = ctx.createGain();
    p = createPathfinderPlayer({ context: ctx, destination: music, song: w, intensity: 127, autoPump: false, clockRandom: true, loopDestination: master });
    p.start(0, 0); ctx.currentTime = 30; p.pump();
    p.loops.setLevel(pct); p.loops.start(); p.event(38); p.loops.stop(); ctx.currentTime = 30.5; p.pump();
    const st = ctx.sources.filter((s) => s.buffer.sampleRate === 22050);
    assert.equal(st.length, 1); assert.equal(st[0].when, 30);
    const value = Math.floor(30000 / 23) & 0x7f; // audio clock 30000 ms at the event (base 0)
    const want = value <= 33 ? 61440 : value <= 80 ? 61003 : 58624;
    assert.equal(st[0].buffer.length, want, `branch value ${value}`);
    const g = st[0].outs[0]; assert.ok(Math.abs(g.gain.value - gain) < 1e-9, `slice byte for ${pct} %`);
    assert.ok(reaches(g, master) && !reaches(g, music), 'the overlay bypasses the MUSIC bus');
    assert.ok(ctx.sources.filter((s) => s.buffer.sampleRate !== 22050).some((s) => reaches(s, music)), 'the stream stays on the MUSIC bus');
    p.stop();
  }
  console.log('challenge audio: player attaches, the stinger plays on the overlay output at the 3D41A8 byte');
}

// 4. Director (web/game-audio.js) with a stand-in AudioContext: the hub-song branch, the attach + event, the flags.
{
  // any node the voice layers ask for: AudioParams on demand (sfx.js pans, detunes, envelopes)
  const fake = fakeContext();
  const anyNode = () => new Proxy({ outs: [], connect(n) { this.outs.push(n); return n; }, disconnect() { this.outs = []; } }, {
    get(o, k) { if (!(k in o) && typeof k === 'string') o[k] = { value: 0, setValueAtTime() {}, linearRampToValueAtTime() {}, setTargetAtTime() {}, cancelScheduledValues() {}, setValueCurveAtTime() {} }; return o[k]; } });
  const plain = fake.createBufferSource.bind(fake);
  fake.createBufferSource = () => { const s = plain(); s.detune = anyNode().detune; s.playbackRate = anyNode().playbackRate; return s; };
  globalThis.AudioContext = function AudioContext() { return new Proxy(Object.assign(fake, { state: 'running', resume: () => Promise.resolve(), suspend: () => Promise.resolve(),
    decodeAudioData: async () => fake.createBuffer(1, 1, 22050) }), { get(o, k) { if (!(k in o) && typeof k === 'string' && k.startsWith('create')) return () => anyNode(); return o[k]; } }); };
  globalThis.location = { href: 'http://localhost/?audioSong=Wobble', search: '?audioSong=Wobble' };
  const { setPv } = await import('./pv-flags.js');
  const { createGameAudio } = await import('./game-audio.js');
  let T = 1000;
  const ga = createGameAudio({ now: () => T, fetchJson: async (p) => JSON.parse(fs.readFileSync(PUBLIC + p, 'utf8')), fetchBytes: async (p) => new Uint8Array(fs.readFileSync(PUBLIC + p)) });
  await ga.whenReady(); ga.unlock();
  const settle = async () => { for (let i = 0; i < 20; i++) await new Promise((r) => setTimeout(r, 5)); };
  const events = () => ga.timeline().filter((e) => e[1] === 'event').map((e) => e[2]);
  const FREE = { kind: 4, mode: 12 };
  // Green Base Station (Peak1), crossing into Snow Jam with the hub song still on, then a challenge start (PS2 stage 1).
  setPv('bigChallengeAudio', true);
  await ga.runStart({ courseIndex: 17, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' }); await settle();
  assert.equal(ga.debug().music, 'Peak1');
  ga.freeRideCourse(0, { kind: 4 });
  ga.timelineReset();
  ga.challengeStart(3); await settle();
  assert.equal(ga.debug().music, 'Wobble', 'hub song: PickNextSong + PlayMusic(0)');
  assert.ok(!events().includes(38), 'no challenge event on the hub song');
  assert.deepEqual(ga.debug().director.timers, [], 'the request / DJ / retry timers are cancelled');
  assert.equal(ga.debug().trigger.latch, 18);
  // Wobble playing: the next start attaches the bank and sends 38; the stinger is on the overlay.
  fake.currentTime += 8; ga.timelineReset();
  ga.challengeStart(3); await settle();
  assert.deepEqual(events(), [38]);
  assert.equal(ga.debug().challenge.music, true);
  const v1 = ga._music().core.state.voices[1];
  assert.ok([64, 65, 66].includes(v1.queue.at(-1).bankIndex) && ga._music().loops.armed);
  // completion: 39, the flag clears
  ga.timelineReset(); ga.challengeEnd(); assert.deepEqual(events(), [39]); assert.equal(ga.debug().challenge.music, false);
  // type 0: no event (-1), the flag set (39 at the stop); a stop without the flag does nothing
  ga.timelineReset(); ga.challengeStart(0); assert.deepEqual(events(), [-1]); assert.equal(ga.debug().challenge.music, true);
  ga.timelineReset(); ga.challengeStop(false); assert.deepEqual(events(), [39]);
  ga.timelineReset(); ga.challengeStop(true); assert.deepEqual(events(), []);
  // radio mode 2: nothing (29D6E0 returns before the hub check)
  // the switch off: the previous behaviour (event without the attach)
  setPv('bigChallengeAudio', false);
  ga.timelineReset(); ga.challengeStart(1); assert.deepEqual(events(), [33]);
  ga.leaveWorld(); setPv('bigChallengeAudio', null);
  console.log('challenge audio: director hub branch, attach + event 38, completion 39, type 0 and the switch');
}
console.log('test-challenge-audio: OK');
