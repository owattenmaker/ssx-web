// Gameplay sound dispatch: the SSXAudioSystem dispatchers (29xxxx) fed by the core's audio event queue
// (web/audio_events.inc) and per-tick telemetry, for the human and the computer riders. docs/audio-logic.md 3.6,
// 5.2-5.5; the voice layer is web/sfx.js, speech web/audio-speech.js, crowd web/audio-crowd.js, world sounds
// web/audio-world.js.
import { SLOT, CURVES, curve, surfaceClass, boardFamily, rndRange } from './sfx.js';
import { CHAR_ID, charId, peakMask, EV } from './audio-speech.js';

export const AE = Object.freeze({ TAKEOFF: 1, LANDING: 2, CRASH: 3, CRASH_LOOP: 4, CRASH_AIR: 5, CRASH_GRUNT: 6, CRASH_EXIT: 7, CRASH_METER: 8,
  BOOST: 9, FILL_RESET: 10, GRAB: 11, UBER_DENIED: 12, HANDPLANT: 13, COMBO: 14, NAMED_TRICK: 15, TRICK_SPEECH: 16, WHOOH: 17, SOFT: 18,
  OBSTACLE: 19, CONTACT: 20, RESET: 21, PICKUP: 22, ANIM_EVENT: 23, PAIR: 24, TIME_UP: 25, FINISH: 26, OVERTAKE: 27, SCRIPT_SOUND: 28, ARCADE_BONUS: 29 });
// 29F3F8 (table 0x482F60, CHARDB order) / 29F378 (guests 0xE/0x10/0x15 -> 6, others -> 7).
const GRNT = ['MOB', 'KAO', 'ARI', 'MAC', 'ZOE', 'GRF', 'ELI', 'NAT', 'PSY', 'VIG'];
export const gruntIndex = (name) => { const id = charId(name); return id >= 0 ? id : 7; };
const f32 = (core, ptr, n) => new Float32Array(core.HEAPF32.buffer, ptr, n);
const TELEMETRY = 24;
const rand15 = (random) => Math.floor(random() * 0x8000);
// 2A1820 with the records {scores: 155B50(a, b) levels, kinds: 155AB0(a, b)}: 2 foe, 1 / 0 friend; null without data.
export function relationOf(m, a, b) {
  if (!m?.scores || !m?.kinds) return null;
  if ((m.scores[a]?.[b] ?? 0) >= 2 || (m.scores[b]?.[a] ?? 0) >= 2) return 2;
  const k = m.kinds[a]?.[b]; return k === 0 ? 0 : k === 1 ? 1 : 2;
}

export function createSfxGame({ engine, sfx, speech, crowd, world, music, random = Math.random, animationEvents = null }) {
  const riders = new Map(); // core -> rider state
  const G = { courseIndex: 0, bigAirEvent: false, halfPipeEvent: false, backcountry: false, zoneActive: () => false, running: false, paused: false,
    countdown: 0, goPhase: null, timedOut: false, finished: false, points: { last: 0, voice: null }, hud: { prev: null, uberVoice: null },
    fill: { prev: 0, bend: 0x1000, tricky: 0 }, air: { flag: 0, T: 0, start: 0, end: 0, voice: null, gain: 0, loopsOn: false }, listener: null, clipEvents: animationEvents };
  const human = () => [...riders.values()].find((r) => r.human);

  function rider(core, { character, slot, isHuman }) {
    let r = riders.get(core);
    if (!r) {
      r = { core, slot, character, id: charId(character), human: isHuman, t: new Float32Array(TELEMETRY), prevT: new Float32Array(TELEMETRY),
        loops: { a: null, b: null, c: null }, loopClass: -1, loopMotion: -1, smoothB: 0, smoothC: 0, crash: { a: null, b: null }, boost: 0, boostVoice: null,
        landVoice: null, takeoffVoice: null, retry: false, grunt: 0, meterPrev: 1, finished: false, place: 0 };
      riders.set(core, r);
    }
    return r;
  }
  const pos = (r) => [r.t[12], r.t[13], r.t[14]];
  const inRange = (r) => { const s = sfx.spatial(pos(r), 100); return s.d === undefined || s.d < 100; }; // 288B40: 100 m gate
  const speechRider = (r) => ({ id: r.id, human: r.human, listener: r.human, upright: r.t[4] !== 2 && !r.finished, inRange: inRange(r) });
  const peak = () => peakMask(G.courseIndex);
  const play = (o) => sfx.play(o);
  const humanGate = (r) => r.human;

  // ---- board loops (292A50 start, callbacks 292CB8 / 292DA0 / 292EF8 / 293018) ----
  function riderClass(r) { return surfaceClass(r.t[4] === 4 ? r.t[6] : r.t[5]); } // 291710 (rail: audio+0x598C)
  function loopBank(c) { return c <= 3 ? SLOT.SPUBOARD : SLOT.BOARD; }
  function startLoops(r) {
    const c = r.loopClass = riderClass(r);
    const p = pos(r);
    if (r.human) r.loops.a = play({ slot: SLOT.SPUBOARD, sound: 0, bus: 'BOARD', volume: 0, position: p, tag: 'board', owner: r });
    r.loops.b = play({ slot: loopBank(c), sound: c + (r.human ? 4 : 6), bus: 'BOARD', volume: 0, position: p, tag: 'board', owner: r });
    if (r.human) r.loops.c = play({ slot: loopBank(c), sound: c + 3, bus: 'BOARD', volume: 0, position: p, tag: 'board', owner: r });
  }
  function stopLoops(r) { for (const k of ['a', 'b', 'c']) { r.loops[k]?.stop(0.25); r.loops[k] = null; } }
  const muted = (r) => r.t[4] === 2 || r.finished || G.paused; // motion 2, rider+0x470 >= 0, pause/NIS gate
  function updateLoops(r) {
    if (!r.loops.b && !r.loops.a) { if (sfx.bankOf(SLOT.BOARD)?.bnk && sfx.bankOf(SLOT.SPUBOARD)?.bnk) startLoops(r); return; }
    const motion = r.t[4], spd = r.t[0], turn = r.t[1], brake = r.t[2], comp = r.t[3];
    let c = riderClass(r); const fam = boardFamily(c, motion);
    if ((motion === 1 || motion === 5) && fam === 2) c = 9; // air: family 2 stores class 9 (restart on contact)
    if (c !== r.loopClass && c !== 9 && r.loopClass !== -1) { // class change: 2ABB38 then 291C88 / 292508 again
      r.loops.b?.stop(0.25); r.loops.c?.stop(0.25);
      r.loopClass = c;
      r.loops.b = play({ slot: loopBank(c), sound: c + (r.human ? 4 : 6), bus: 'BOARD', volume: 0, position: pos(r), tag: 'board', owner: r });
      if (r.human) r.loops.c = play({ slot: loopBank(c), sound: c + 3, bus: 'BOARD', volume: 0, position: pos(r), tag: 'board', owner: r });
    } else if (c === 9) r.loopClass = 9;
    const m = muted(r);
    if (r.loops.a) { // 292CB8
      const vol = !m && motion === 0 ? Math.trunc(curve(CURVES.loopA_vol_482AF8, spd)) : 0;
      r.loops.a.setVolume(vol); r.loops.a.setBend(Math.trunc(curve(CURVES.loopA_pitch_482B20, spd) * 4096));
    }
    if (r.loops.b) { // 293290 / 293350 -> 293538 / 293740 / 293948
      let x = comp * (1 - (brake + turn)); if (spd < 0.1) x *= spd * 10;
      if (Math.abs(r.smoothB - x) > 0.1) x = r.smoothB - 0.25 * (r.smoothB - x);
      r.smoothB = x;
      const [V, P] = fam === 0 ? [CURVES.glide0_vol_482B78, CURVES.glide0_pitch_482BA0] : fam === 1 ? [CURVES.glide1_vol_482BC8, CURVES.glide1_pitch_482BF0] : [CURVES.glide2_vol_482C18, CURVES.glide2_pitch_482C40];
      r.loops.b.setVolume(m ? 0 : Math.trunc(curve(V, x))); r.loops.b.setBend(Math.trunc(curve(P, x) * 4096));
    }
    if (r.loops.c) { // 293410 -> 293BB0 / 293DC0 / 293FC8
      let y = brake + turn; const d = y - r.smoothC;
      if (Math.abs(d) > 0.1) y = r.smoothC + (d < 0 ? 0.5 : 0.125) * d;
      r.smoothC = y;
      let vol, bend;
      if (fam === 0) { vol = curve(CURVES.carve0_vol_482C68, y); bend = curve(CURVES.carve0_pitch_482C90, comp); if (spd < 0.1) vol *= spd * 10; }
      else if (fam === 1) { const yy = spd < 0.1 ? y * spd * 10 : y; vol = curve(CURVES.carve1_vol_482CB8, yy); bend = curve(CURVES.carve1_pitch_482CE0, comp); }
      else { vol = 127 * (spd < 0.1 ? spd * 10 : 1); bend = curve(CURVES.carve2_pitch_482D08, comp); }
      r.loops.c.setVolume(m ? 0 : Math.trunc(vol)); r.loops.c.setBend(Math.trunc(bend * 4096));
    }
  }

  // ---- grunts (29F660) ----
  function grunt(r, kind) {
    if (!(engine.busGain('CHARACTER') > 0) || r.finished) return;
    const speed = Math.hypot(r.t[19], r.t[20], r.t[21]);
    const vol = Math.min(Math.max(Math.trunc(curve(CURVES.grunt_445910, speed)), 0), 127); // 290C40
    let slot, sound;
    if (r.human) { slot = SLOT.GRUNT; sound = kind === 1 ? Math.floor(rand15(random) * 3 / 0x7fff) : kind === 0 ? Math.floor(rand15(random) * 3 / 0x7fff) + 3 : ((r.grunt = (r.grunt + 1) % 6) + 5) % 6; } // audio+0x5734 cycles 0..5
    else { slot = SLOT.GRUNT_AI; sound = gruntIndex(r.character) * 2 + (kind === 1 ? 1 : 0); }
    if (r.gruntVoice?.playing()) return; // repeat tag 3 (295028)
    r.gruntVoice = play({ slot, sound, bus: 'CHARACTER', speaker: r.id, volume: vol, position: pos(r), tag: 'grunt', owner: r });
  }
  // ---- crash (10EB30 -> 296310 / 29F660 / 2961F0) ----
  function crashSound(r, surface) { // 296310
    if (!inRange(r)) return;
    const s = surface ?? r.t[5];
    if (s === 2 || s === 3) play({ slot: SLOT.LAND, sound: 9, bus: 'UI', volume: 127, position: pos(r), tag: 'crash', owner: r });
    else play({ slot: SLOT.MAIN, sound: 0x30 + Math.floor(rand15(random) * 3 / 0x8000), bus: 'UI', volume: 127, position: pos(r), tag: 'crash', owner: r });
  }
  function crashLoop(r, which) { // 296868 (0x33) / 296E80 (0x34)
    const k = which === 0x33 ? 'a' : 'b';
    if (r.crash[k]?.playing()) return;
    r.crash[k] = play({ slot: SLOT.MAIN, sound: which, bus: 'UI', volume: 127, position: pos(r), tag: 'crash-loop', owner: r });
  }
  function stopCrashLoops(r) { r.crash.a?.stop(0.75); r.crash.b?.stop(0.75); r.crash.a = r.crash.b = null; } // 296E20 / 297438
  // ---- boost (298D90 / 299368 / 2992D8) ----
  function boost(r, kind, pressed) {
    if (!r.human) return;
    if (kind === 1) {
      if (r.boost === 0) r.boostVoice = play({ slot: SLOT.MAIN, sound: 0x7a, bus: 'ARCADESFX', volume: 127, tag: 'boost' });
      if (pressed) play({ slot: SLOT.MAIN, sound: 0x4a, bus: 'ARCADESFX', volume: 127 });
      r.boost++;
    } else if (kind === 2) play({ slot: SLOT.MAIN, sound: 0x6c, bus: 'ARCADESFX', volume: 127 });
    else if (kind === 3) { if (r.boost > 0 && --r.boost === 0) { r.boostVoice?.stop(0.25); r.boostVoice = null; } }
  }
  // G.replay (288AE0, the replay behind the results / the Replay item): the arcade feedback (points, boost fill, combo, Uber,
  // Tricky, crash meter, grab) is blocked; board, carve, take-off, landing, crash and grunt sounds play (docs/replay.md).
  const arcade = (sound, extra = {}) => (G.replay ? null : play({ slot: SLOT.MAIN, sound, bus: 'ARCADESFX', volume: 127, ...extra }));

  // ---- 294170 take-off / 294678 / 2948D0 landing ----
  function takeoff(r, speed, predValid, T) {
    if (r.human) { // listener rider
      r.takeoffVoice?.stop(0);
      const c = riderClass(r);
      r.takeoffVoice = play({ slot: SLOT.LAND, sound: c * 8 + 2, bus: 'BOARD', volume: Math.min(Math.max(Math.trunc(speed * 127), 64), 127), position: pos(r), tag: 'land', owner: r });
    }
    if (predValid) takeoffFollowup(r, T); else r.retry = true; // 2947B0 retries while the predictor is not in flight
  }
  function takeoffFollowup(r, T) { // 294678
    r.retry = false;
    if (r.human && T > 2) crowd.arm();
    if (T >= 4 && !G.backcountry) speech.bigAir(speechRider(r), peak());
  }
  function landing(r, value, impact, kind) {
    if (r.human) { // crowd (focus rider)
      if (value <= 0) crowd.cancel(); else crowd.trick(value < 0.25 ? 0 : value < 0.65 ? 1 : 2);
    }
    if (!inRange(r)) return;
    r.landVoice?.stop(0);
    const c = riderClass(r);
    r.landVoice = play({ slot: SLOT.LAND, sound: c * 8 + (r.human ? 1 : 5), bus: 'BOARD', volume: Math.min(Math.max(Math.trunc(curve(CURVES.landing_vol_482D30, Math.abs(impact))), 0), 127), position: pos(r), tag: 'land', owner: r });
  }

  // ---- event dispatch ----
  function dispatch(r, type, a, b, c, d) {
    switch (type) {
      case AE.TAKEOFF: takeoff(r, a, b > 0, c); break;
      case AE.LANDING: r.retry = false; landing(r, a, b, c); break;
      case AE.CRASH:
        crashSound(r, b); grunt(r, 0);
        if (r.human) { const f = Math.abs(a); if (f <= 0) crowd.cancel(); else crowd.fall(f < 0.25 ? 0 : f < 0.65 ? 1 : 2); music.resetIntensity(); } // 2961F0, 28F108
        break;
      case AE.CRASH_LOOP:
        if (b === 0) crashLoop(r, a === 0x19 || a === 0x1d ? 0x33 : 0x34);
        else { crashLoop(r, c ? 0x33 : 0x34); crashSound(r); } // 12D4E8: both air-phase paths (class 0x19/0x1D -> 296868, else 296E80) end in 296310
        break;
      case AE.CRASH_AIR: stopCrashLoops(r); break;
      case AE.CRASH_GRUNT: grunt(r, a); break;
      case AE.CRASH_EXIT: speech.wipeout(speechRider(r), peak()); break;
      case AE.CRASH_METER:
        if (!r.human) break;
        if (a) arcade(0x60); // 29E970
        else { arcade(0x5f, { bend: 0x1000 + Math.trunc(Math.min(b, r.meterPrev) * 5) * 0x50 }); } // 29E590
        r.meterPrev = b; break;
      case AE.BOOST: if (!G.replay) boost(r, a, b > 0); break;   // 288AE0 blocks the boost sounds
      case AE.FILL_RESET: if (r.human) G.fill.bend = 0x1000; break; // 29AB08
      case AE.GRAB: if (r.human) arcade(0x77); break;           // 29A530
      case AE.UBER_DENIED: if (r.human) arcade(0x6c); break;    // 299B70
      case AE.HANDPLANT: if (r.human) arcade(0x61); break;      // 29DC48
      case AE.COMBO: // 29B430
        if (!r.human) break;
        arcade(0x67);
        if (b < 4 && a + b >= 4 && a + b <= 8) speech.arcade(EV.ARCADE_UBER, 1);
        break;
      case AE.NAMED_TRICK: if (r.human) speech.arcade(EV.ARCADE_UBER, 8); break; // 29B7E0
      case AE.TRICK_SPEECH: speech.trick(speechRider(r), peak(), a > 0); break;  // 29FF80
      case AE.WHOOH: speech.whooh(speechRider(r)); break;                       // 2A1560
      case AE.SOFT: // 2A0E70
        grunt(r, 2);
        if (a && r.human) speech.objectHit(speechRider(r));
        break;
      case AE.OBSTACLE: // 2989A8 (tag 6)
        if (!r.human || r.obstacleVoice?.playing()) break;
        r.obstacleVoice = play({ slot: SLOT.MAIN, sound: 0x36, bus: 'UI', volume: 127, position: pos(r), tag: 'obstacle', owner: r });
        break;
      case AE.CONTACT: if (world) world.contact(String(a), b, c, pos(r), inRange(r)); break;
      case AE.RESET: // 29A220
        if (r.human && !G.replay) play({ slot: SLOT.MAIN, sound: 0x7b, bus: 'UI', volume: 127 });   // 288AE0 blocks the reset sound
        stopCrashLoops(r); if (r.boost) { r.boost = 1; boost(r, 3); }
        if (r.human) music.resetIntensity();
        break;
      case AE.PICKUP: { // 29CED8
        if (!r.human) break;
        arcade([0x74, 0x71, 0x75, 0x76, 0x70, 0x70][a] ?? 0x70);
        if (a === 0) speech.arcade(EV.ARCADE_POWER_UPS, 1); else if (a === 1) speech.arcade(EV.ARCADE_POWER_UPS, 2);
        if (a === 2) speech.arcade(EV.ARCADE_ICONS, b === 2 ? 1 : b === 3 ? 2 : b === 5 ? 4 : b === 10 ? 8 : 0);
        break;
      }
      case AE.ANIM_EVENT: { // 103AA0 -> 104CC8 -> 289B18 (human)
        if (!r.human) break;
        const id = G.clipEvents?.(a, b); if (id == null || !(id & 0x8000)) break;
        const sound = { 0x50: 0x51, 0x51: 0x50, 0x52: 0x52 }[id & 0x7fff]; if (sound === undefined) break;
        play({ slot: SLOT.MAIN, sound, bus: 'UI', volume: 127, position: pos(r), tag: 'anim', owner: r });
        break;
      }
      case AE.PAIR: pair(r, a, c, d); break;
      case AE.ARCADE_BONUS: if (r.human) speech.arcade(EV.ARCADE_BONUS, 1); break; // 1194C0 -> 2A3CE8(audio, rider, 1)
      case AE.OVERTAKE: overtake(a, b >> 3, b & 7, c, d); break;
      case AE.SCRIPT_SOUND: if (world && r.human) { const p = G.instancePosition?.(c); if (p) world.script(a, b, c, p, G.courseIndex); } break;
      default: break;
    }
  }
  const coreOfSlot = (slot) => [...riders.values()].find((x) => x.slot === slot);
  // 107E70 wrappers on the target: 0 soft 10E228 / 1 crash 10E2E8 -> 298488 bump + grunt; 2 soft attack 10E3A8 /
  // 3 crash attack 10E468 -> 298138 attack; 2A0A30(A = other, B = target); speech only from 10E3A8.
  function pair(_, kind, targetSlot, otherSlot) {
    const B = coreOfSlot(targetSlot), A = coreOfSlot(otherSlot); if (!A || !B) return;
    if (!(A.human || B.human)) return;
    if (kind < 2) { // 298488 (tag 7)
      if (inRange(B) && !B.bumpVoice?.playing()) {
        const rel = Math.hypot(B.t[19] + A.t[19], B.t[20] + A.t[20], B.t[21] + A.t[21]); // vadd of the two velocities (as coded)
        B.bumpVoice = play({ slot: SLOT.MAIN, sound: 0x36, bus: 'UI', volume: Math.min(Math.max(Math.trunc(curve(CURVES.bump_4458C0, Math.trunc(rel))), 0), 127), position: pos(B), tag: 'bump', owner: B });
      }
    } else if (!B.attackVoice?.playing()) B.attackVoice = play({ slot: SLOT.MAIN, sound: 0x5a, bus: 'UI', volume: 127, position: pos(B), tag: 'attack', owner: B }); // 298138 (tag 5)
    // 2A0A30: grunt for B always (after the gates), speech only from 10E3A8 (kind 2).
    if (engine.busGain('CHARACTER') > 0 && B.t[4] !== 2 && A.t[4] !== 2) {
      grunt(B, 0);
      if (kind === 2 && speech.free && (inRange(A) || inRange(B))) speech.hit(speechRider(A), speechRider(B), relation(A, B, 'pair'));
    }
  }
  // 2A1820 rel(A, B): 2 when two riders or both human; else from the relationship records (155B50 levels, 155AB0
  // kinds) as they are when the original evaluates it: pair hits use the records before this contact's 155BF0
  // update (G.pairRelation, recorded by the pair hook), overtakes the records at the tick-start ranking 10F998.
  const relation = (A, B, source = 'tick') => {
    if (riders.size === 2 || (A.human && B.human)) return 2;
    const v = source === 'pair' ? G.pairRelation?.(A.slot, B.slot) : G.relation?.(A.slot, B.slot);
    return v ?? 2;
  };
  // 299E28: rank change of `slot` (old -> new place), behind / ahead riders.
  function overtake(slot, oldPlace, newPlace, behind, ahead) {
    const r = coreOfSlot(slot); if (!r || !r.human || riders.size < 2 || !G.running || G.paused || G.raceKind !== 0) return;
    if (newPlace === 0 && oldPlace !== 0) arcade(0x6d); // tag 1
    if (!(G.sinceGo >= 600)) return; // 2A0560: game tick - audio+0x582C (the GO tick, written by 29C7B0) >= 600 (ARMSX2)
    const gained = newPlace < oldPlace, other = coreOfSlot(gained ? behind : ahead); if (!other) return;
    const A = gained ? r : other, Bq = gained ? other : r; // A overtakes B
    if (A.t[4] === 2 || Bq.t[4] === 2 || !speech.free) return;
    const heading = Math.atan2(Bq.t[20], Bq.t[19]); // relative speed along B's heading
    const d = (A.t[19] - Bq.t[19]) * Math.cos(heading) + (A.t[20] - Bq.t[20]) * Math.sin(heading);
    if (!(d > 670.56 && d < 1788.16) || !inRange(A) || inRange(A) !== inRange(Bq)) return;
    speech.pass(speechRider(A), speechRider(Bq), relation(A, Bq), d >= 1341.12 ? 2 : 1);
  }

  // ---- HUD / score sounds (117FE0, 29AB40) ----
  function hudSounds(r, slots, pending, superTime, tier) {
    // 29A7D8: pending trick points tick (0x4D) when pts % 100 falls below the last value.
    // 117FE0 calls it only while the pending points are positive, so the last remainder survives a bail/commit.
    if (Math.trunc(pending) > 0) {
      const m = Math.trunc(pending) % 100;
      if (m < G.points.last) { G.points.voice?.stop(0); G.points.voice = arcade(0x4d); }
      G.points.last = m;
    }
    if (!slots) return;
    const type = (k) => slots[6 * k], prev = G.hud.prev;
    if (prev) {
      if (prev[10] === 0x34 && type(10) !== 0x34) G.hud.uberVoice = arcade(0x66);       // 29B0E0
      if (prev[10] !== 0x34 && type(10) === 0x34) { G.hud.uberVoice?.stop(0.25); G.hud.uberVoice = null; } // 29B3C0
      if (prev[9] === 0x34 && type(9) !== 0x34 && superTime > 0) { // 299638
        arcade(0x6b, { delayMs: 400 });
        speech.arcade(EV.ARCADE_PROMPTS, tier === 10 ? 0x10 : tier < 5 ? 4 : 8);
      }
      if (prev[9] !== 0x34 && type(9) === 0x34 && superTime === 0) arcade(0x69);        // 2997B8
    }
    G.hud.prev = Array.from({ length: 44 }, (_, k) => type(k));
  }
  // 29AB40 runs in the per-frame audio update 285BF8, before that frame's game tick: it reads the HUD bank and +0x2F0 as the
  // previous tick left them (PS2 uber-super-expire audio log: every Tricky countdown tick one tick after a same-tick read).
  function frameSounds(r, slots, superTime) {
    if (!slots) return;
    // 29AB40 (a): boost-meter fill steps -> 0x65 after 150 ms with a rising pitch.
    const max = slots[6 * 6 + 1], value = slots[6 * 6 + 2];
    const n = Math.floor(Math.fround(value / max) * 10); // slot 6 is a value slot (maximum -1, value = -meter): the ratio is the meter
    if (G.fill.prev < n && n > 0) { arcade(0x65, { delayMs: 150, bend: G.fill.bend }); G.fill.bend = (G.fill.bend + 0x100) & 0xffff; }
    G.fill.prev = n;
    // 29AB40 (b): Tricky countdown ticks (0x68) on each half second below 6.5 s (even) / 2.5 s (odd).
    const t = Math.floor(superTime * 2);
    if (t > 0 && t === G.fill.tricky - 1 && t < 13 && (!(t & 1) || t < 5) && !r.finished && G.running && !G.paused) arcade(0x68);
    G.fill.tricky = t;
  }

  // ---- 28C8C8: big air (music duck to loops) + air whoosh (2910E0 / 2913D8) ----
  function bigAir(r) {
    const A = G.air, motion = r.t[4], status = r.t[7];
    let T = 0;
    if (motion === 1) {
      if (status === 1 || status === 3) { if (A.flag === 1) T = A.T; else { T = r.t[8]; A.start = 0; A.T = T; } }
      else if (A.flag === 1) T = A.T;
    }
    const musicVol = engine.busGain('MUSIC');
    const prev = A.flag; A.flag = 0;
    const s3 = T > 3.9995 && music.duckToLoops() && !G.bigAirEvent && !G.halfPipeEvent && !G.zoneActive();
    if (G.replay) return;
    if (T > 2) {
      A.flag = 1;
      let e = r.t[10]; if (e < A.start) e = A.start; A.start = e;
      if (!prev) A.end = e + Math.max((r.t[9] - r.t[10]) * 0.5, 0.5);
      const floor = Math.min(Math.max((1 - T * 0.8425197 * 0.5) * 127, 20), 127);
      let stream;
      if (e < A.end) {
        const p = e / A.end, v = (Math.trunc(Math.min(Math.max(127 - p * 107, 20), 127)) << 24) >> 24;
        if (s3) music.loopsLevel(((127 - (v - 20)) * musicVol / 127 * 100) | 0);
        stream = (Math.trunc(Math.min(Math.max(127 - (127 - floor) * p, floor), 127)) << 24) >> 24;
      } else { stream = Math.trunc(floor); if (s3) music.loopsLevel((musicVol * 100) | 0); }
      A.gain = (127 - stream) / 127;
      if (s3) music.streamLevel(stream);
      if (!prev) { // 2910E0: whoosh (bank 0 snd 0x20, 2D, gain audio+0x5830)
        A.voice = play({ slot: SLOT.MAIN, sound: 0x20, bus: 'UI', volume: Math.trunc(127 * A.gain), tag: 'air' });
        if (s3 && music.loopsStart()) { A.loopsOn = true; music.event(7); }
      }
      A.voice?.setVolume(Math.trunc(127 * A.gain));
    } else if (prev === 1) {
      A.voice?.stop(0.25); A.voice = null; A.gain = 0; // 2913D8
      if (A.loopsOn || music.loopsRequested()) { music.event(8); music.loopsStop(); }
      A.loopsOn = false;
      music.streamLevel(127); music.loopsLevel((musicVol * 100) | 0);
    }
  }

  // Drain one core's queue.
  function drain(r) {
    const c = r.core; if (!c._audio_events) return;
    const q = f32(c, c._audio_events(), 1 + 64 * 5); const n = q[0] | 0; const ev = q.slice(1, 1 + n * 5); c._audio_events_clear();
    r.prevT.set(r.t); r.prevValid = !!r.hasT; r.hasT = true; r.t.set(f32(c, c._audio_telemetry(), TELEMETRY));
    for (let i = 0; i < n; i++) dispatch(r, ev[5 * i], ev[5 * i + 1], ev[5 * i + 2], ev[5 * i + 3], ev[5 * i + 4]);
  }

  const api = {
    G,
    // World start (286E20 -> 2929D8 -> 292A50) / teardown (286A80 -> 292B48).
    start({ courseIndex = 0, bigAir = false, halfPipe = false, backcountry = false, raceKind = 0 } = {}) {
      api.stop();
      Object.assign(G, { courseIndex, bigAirEvent: bigAir, halfPipeEvent: halfPipe, backcountry, raceKind, countdown: 0, goPhase: null, timedOut: false, finished: false, sinceGo: -1 });
      G.points = { last: 0, voice: null }; G.hud = { prev: null, uberVoice: null }; G.fill = { prev: 0, bend: 0x1000, tricky: 0 }; G.uberTier = undefined; G.frameSlots = null;
      G.air = { flag: 0, T: 0, start: 0, end: 0, voice: null, gain: 0, loopsOn: false };
    },
    stop() {
      for (const r of riders.values()) { stopLoops(r); stopCrashLoops(r); r.boostVoice?.stop(0.25); }
      riders.clear(); G.air.voice?.stop(0.25);
      sfx.stopAll({ tag: 'boost' });
    },
    // One 60 Hz game tick after the cores stepped. riders: [{core, character, slot, human, finished, place}].
    tick({ riders: list, raceInfo = null, running = true, paused = false, pending = 0, hudSlots = null, finished = false, timedOut = false }) {
      G.running = running; G.paused = paused;
      if (G.sinceGo >= 0) G.sinceGo++; // ticks since GO, counted before this tick's events are dispatched
      for (const x of list) { const r = rider(x.core, { character: x.character, slot: x.slot, isHuman: x.human }); r.finished = !!x.finished; r.place = x.place ?? 0; drain(r); }
      const h = human(); if (!h) return;
      for (const r of riders.values()) {
        if (r.retry && (r.t[7] === 1 || r.t[7] === 3)) takeoffFollowup(r, r.t[8]); // 2947B0
        if (r.retry && r.t[4] !== 1) r.retry = false;
        updateLoops(r);
      }
      // Countdown (29C420: ch5 sound 0x4E per second) and GO (29C7B0: 0x5E on ARCADESFX + UI sound 0x62).
      const phase = raceInfo ? raceInfo[5] : null;
      // 0x234C..: the countdown state calls 29C420 on entering (3) and whenever its remaining ticks are a multiple of 60 -- also at
      // 0 on the GO tick -- one tick after the core's raceInfo[6] (PS2 monster-yellowcard audio log: beeps at 60, 120 and 180 = GO).
      const digit = phase === 4 ? Math.ceil(raceInfo[6] / 60) : 0;
      const beep = (d) => { if (G.replay) return; play({ slot: SLOT.MAIN, sound: 0x4e, bus: 'UI', volume: 127 }); if (d > 0) music.countdown?.(d); };   // no countdown in a replay (288AE0)
      if (phase === 4 && G.goPhase !== 4) beep(3);
      else if (phase === 4 && raceInfo[6] > 0 && (raceInfo[6] + 1) % 60 === 0) beep((raceInfo[6] + 1) / 60);
      else if (phase === 5 && G.goPhase === 4) beep(0);
      G.countdown = digit;
      if (G.goPhase === 3 && phase === 5 && G.backcountry) G.sinceGo = 0; // rolling start (2872A8 writes the GO tick 0x582C, no GO sound)
      if (G.goPhase === 4 && phase === 5) { G.sinceGo = 0; if (!G.replay) play({ slot: SLOT.MAIN, sound: 0x5e, bus: 'ARCADESFX', volume: 127 }); play({ slot: SLOT.MAIN, sound: 0x62, bus: 'UI', volume: 127 }); } // a replay's GO: 0x62 only (PS2 replay audio log)
      G.goPhase = phase;
      // 125228: TIME'S UP -> arcade prompt 2.
      if (timedOut && !G.timedOut) speech.arcade(EV.ARCADE_PROMPTS, 2);
      G.timedOut = timedOut;
      if (h.t[7] !== undefined) {
        if (h.prevValid) frameSounds(h, G.frameSlots, h.prevT[22] ?? 0);
        hudSounds(h, hudSlots, pending, h.t[22] ?? 0, h.t[23] ?? 0);
        // 29B738 (per frame, previous tick's +0x2F4): the focus rider reaching Uber tier 10 (Super Uber) while racing ->
        // Arcade_Uber variant 2; audio+0x581C keeps the last tier seen.
        if (h.prevValid) { const tier = h.prevT[23] ?? 0; if (G.uberTier !== undefined && tier === 10 && G.uberTier !== 10 && running && !paused) speech.arcade(EV.ARCADE_UBER, 2); G.uberTier = tier; }
      }
      G.frameSlots = hudSlots;
      // 28C8C8 runs in the per-frame audio update 285BF8, before that frame's game tick: it sees the previous tick's
      // predictor (PS2 pipe-uber-chain audio log: every air whoosh starts one tick after the browser's same-tick read).
      bigAir({ t: h.prevT });
      // 290FD0: anticipation level from the remaining air time and the pending trick value.
      crowd.update(() => {
        if (h.t[4] !== 1) return 3;
        const t = (h.t[7] === 1 || h.t[7] === 3) ? h.t[8] - h.t[10] : 4; if (t > 3) return 3;
        const p = pending / 10000; return p <= 0 ? 3 : p < 0.25 ? 0 : p < 0.5 ? 1 : 2;
      });
      if (finished && !G.finished) { // 286EA0 (focus rider)
        crowd.trick(2); stopCrashLoops(h); G.hud.uberVoice?.stop(0.25);
        stopLoops(h); // the rider loops fall silent after the finish (rider+0x470)
      }
      G.finished = finished;
    },
    // 29ED18 (pause menu entry): the focus rider's crash loops, the pending-Uber sound and the boost loop stop.
    pauseStops() {
      const h = human(); if (!h) return;
      stopCrashLoops(h); G.hud.uberVoice?.stop(0.25); G.hud.uberVoice = null;
      if (h.boost > 0) { h.boostVoice?.stop(0); h.boostVoice = null; h.boost = 0; }
    },
    // Big Challenge start / stop (29D6E0 / 29DBB0; pv bigChallengeAudio): the first rider's crash slide loops (296E20 /
    // 297438) and the pending-Uber sound (29B3C0). Unlike 29ED18 the boost loop keeps running.
    challengeStops() { const h = human(); if (h) stopCrashLoops(h); G.hud.uberVoice?.stop(0.25); G.hud.uberVoice = null; },
    // 29D8E0 (challenge complete): bank 0 sound 0x6D on ARCADESFX, volume 127, not positional.
    challengeComplete() { return arcade(0x6d); },
    // 29C420 / 29C7B0 callbacks are driven from tick(); these expose the dispatchers for tests.
    dispatch, rider, riders: () => [...riders.values()],
    debug() { return { riders: [...riders.values()].map((r) => ({ slot: r.slot, human: r.human, motion: r.t[4], class: r.loopClass, loops: Object.values(r.loops).filter(Boolean).length, boost: r.boost })),
      air: { flag: G.air.flag, T: +G.air.T.toFixed(2), gain: +G.air.gain.toFixed(2), loops: G.air.loopsOn }, crowd: crowd.debug(), voices: sfx.voiceCount }; },
  };
  return api;
}
