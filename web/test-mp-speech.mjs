// Online rider speech in web/sfx-game.js (docs/audio-logic.md 4.5 "Who speaks"):
//  - pv mpRiderSpeech: the other racers are riders without a core (game-audio gameTick online, web/net/mp-game.js speechRiders),
//    the local racer at its own slot. 2A0560 has no 0x535C11 test; its gate 0x140BC8 (rider+0x874 ^ 1, "computer") with +0x87C
//    (a local camera) lets a pass or hit through when one of the two is a local human: both riders' lines post, 2A1820 = 2 for
//    two humans. Two remote racers: neither is local, nothing.
//  - pv rollingGoTick (G.rollingGo): the backcountry rolling start writes the GO tick audio+0x582C (2872A8 at 0x2872D0) on the
//    first tick at phase 5 without a countdown, so the 600-tick pass gate opens.
//   node test-mp-speech.mjs
import assert from 'node:assert/strict';
import { createSfxGame, AE } from './sfx-game.js';
import { charId } from './audio-speech.js';

function setup() {
  const posted = [];
  const speech = {
    free: true,
    pass: (A, B, rel, fast) => posted.push(['pass', A.id, B.id, rel, fast]),
    hit: (A, B, rel) => posted.push(['hit', A.id, B.id, rel])
  };
  const played = [];
  const sfx = {
    play: (o) => {
      played.push(o.tag);
      return null;
    },
    bankOf: () => null,
    spatial: () => ({ g: 1, az: 0, d: 5 }),
    stopAll() {}
  };
  const game = createSfxGame({
    engine: { busGain: () => 1 },
    sfx,
    speech,
    crowd: { update() {}, debug: () => ({}) },
    world: null,
    music: { resetIntensity() {} }
  });
  // the local racer's core: its own events and telemetry (Zoe at slot 1, 5 m/s along +x)
  const heap = new Float32Array(4096);
  const core = { t: new Float32Array(24), ev: [], HEAPF32: heap };
  Object.assign(core, {
    _audio_events() {
      heap.fill(0, 0, 321);
      heap[0] = core.ev.length;
      core.ev.forEach((e, i) => heap.set(e, 1 + 5 * i));
      return 0;
    },
    _audio_events_clear() {
      core.ev.length = 0;
    },
    _audio_telemetry() {
      heap.set(core.t, 1024);
      return 4096;
    }
  });
  core.t.set([500, 0, 0], 19);
  // remote racers: Griff at slot 0 (15 m/s: 10 m/s faster along Zoe's heading), Mac at slot 2
  const griff = { slot: 0, character: 'griff', finished: false, motion: 0, position: [0, 0, 0], velocity: [1500, 0, 0] };
  const mac = { slot: 2, character: 'mac', finished: false, motion: 0, position: [0, 0, 0], velocity: [1500, 0, 0] };
  const list = [{ core, character: 'zoe', slot: 1, human: true, finished: false, place: 0 }];
  for (const o of [griff, mac]) list.push({ key: o, remote: o, character: o.character, slot: o.slot, human: false, finished: false });
  const tickWith = (phase, evs = []) => {
    core.ev.push(...evs);
    game.tick({ riders: list, raceInfo: new Float32Array([0, 0, 0, 0, 0, phase, 0, 0]) });
  };
  return { game, posted, played, tickWith, griff, list };
}

// a countdown race: GO, then the 600-tick gate
{
  const { game, posted, played, tickWith, griff, list } = setup();
  game.start({ courseIndex: 0 });
  tickWith(4);
  tickWith(5);
  for (let i = 0; i < 600; i++) tickWith(5);
  assert.equal(game.riders().length, 3, 'the local racer and two remote racers are riders');
  assert.deepEqual(game.riders().map((r) => [r.slot, r.human, !!r.remote]), [[1, true, false], [0, false, true], [2, false, true]]);
  // 299E28 for Zoe (slot 1): first -> second, the rider ahead now Griff (slot 0): Griff passes Zoe
  tickWith(5, [[AE.OVERTAKE, 1, 0 * 8 + 1, 2, 0]]);
  assert.deepEqual(posted, [['pass', charId('griff'), charId('zoe'), 2, 1]], 'Griff passes Zoe: both lines, foe (two humans)');
  // 299E28 for Mac (slot 2): he passes Griff; neither is a local rider
  posted.length = 0;
  tickWith(5, [[AE.OVERTAKE, 2, 1 * 8 + 0, 0, -1]]);
  assert.deepEqual(posted, [], 'a pass between two remote racers says nothing');
  // 10E3A8 soft attack (kind 2) on Zoe (target slot 1) by Griff (other slot 0): the hit lines, the attack sound, Zoe's grunt
  played.length = 0;
  tickWith(5, [[AE.PAIR, 2, 0, 1, 0]]);
  assert.deepEqual(posted, [['hit', charId('griff'), charId('zoe'), 2]], 'Griff hits Zoe: foe hit lines');
  assert.ok(played.includes('attack') && played.includes('grunt'), `attack sound and grunt ${JSON.stringify(played)}`);
  // a crashed remote racer (motion 2) passes nobody
  posted.length = 0;
  griff.motion = 2;
  tickWith(5, [[AE.OVERTAKE, 1, 0 * 8 + 1, 2, 0]]);
  assert.deepEqual(posted, [], 'no pass lines with a crashed rider');
  // a racer that is no longer live leaves the riders
  game.tick({ riders: list.slice(0, 2), raceInfo: new Float32Array([0, 0, 0, 0, 0, 5, 0, 0]) });
  assert.deepEqual(game.riders().map((r) => r.slot), [1, 0], 'a racer no longer live leaves the riders');
}

// the backcountry rolling start (no countdown): G.rollingGo opens the gate, off it never opens
for (const rollingGo of [true, false]) {
  const { game, posted, tickWith } = setup();
  game.start({ courseIndex: 14, backcountry: true });
  game.G.rollingGo = rollingGo;
  tickWith(5);
  for (let i = 0; i < 600; i++) tickWith(5);
  tickWith(5, [[AE.OVERTAKE, 1, 0 * 8 + 1, 2, 0]]);
  if (rollingGo) assert.equal(posted.length, 1, 'rollingGoTick: the pass gate opens 600 ticks after the rolling start');
  else assert.equal(posted.length, 0, 'off: the rolling start never set the GO tick');
}
console.log('mp speech OK: remote racers pass / hit the local racer with both lines, remote pairs silent, rolling-start GO tick');
