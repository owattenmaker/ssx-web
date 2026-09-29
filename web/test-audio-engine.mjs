// web/audio-engine.js: MIX.INF parsing and the bus graph/mix ramps against a minimal Web Audio stand-in.
import assert from 'node:assert/strict';
import { parseMixInf, createAudioEngine, AUDIO_BUSES } from './audio-engine.js';

const mixes = parseMixInf(`# comment
[Mix 1]
MUSIC = 100
DJ = 70
TIME = 3000
[Mix 0]
MUSIC = 100
DJ = 100
TIME = 3000
`);
assert.deepEqual(mixes.map((m) => m.index), [0, 1]);
assert.equal(mixes[1].levels.DJ, 0.7);
assert.equal(mixes[1].timeMs, 3000);

// Minimal AudioContext stand-in: records gain automation.
class Param { constructor(v) { this.value = v; this.events = []; } cancelScheduledValues() {} setValueAtTime(v, t) { this.events.push(['set', v, t]); this.value = v; }
  linearRampToValueAtTime(v, t) { this.events.push(['ramp', v, t]); } setTargetAtTime(v) { this.value = v; } }
class Gain { constructor() { this.gain = new Param(1); } connect() {} }
globalThis.AudioContext = class { constructor() { this.currentTime = 10; this.state = 'suspended'; this.destination = {}; } createGain() { return new Gain(); } resume() { this.state = 'running'; return Promise.resolve(); } suspend() { this.state = 'suspended'; return Promise.resolve(); } };
const engine = createAudioEngine({ mixes });
assert.equal(engine.bus('MUSIC'), null, 'no graph before a user gesture');
assert.ok(engine.unlock());
for (const b of AUDIO_BUSES) assert.ok(engine.bus(b));
engine.setMix(1);
assert.deepEqual(engine.bus('DJ').gain.events.at(-1), ['ramp', 0.7, 13]);
const before = engine.bus('DJ').gain.events.length; engine.setMix(1);
assert.equal(engine.bus('DJ').gain.events.length, before, 'SetMix ignores the current mix');
assert.throws(() => engine.setMix(5));
assert.equal(engine.slider('music'), 10, 'profile default 10 of 11 (0x14F458)');
engine.setSlider('music', 20); assert.equal(engine.slider('music'), 11, 'slider steps clamp to 0..11');
console.log('audio engine: MIX.INF parsing, buses and mix ramps OK');
