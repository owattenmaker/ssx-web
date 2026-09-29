// Sound / speech dispatch parity against a PS2 capture's call log (tools/ps2_audio_log.py, RUN.audio.json).
// Used as compare-ps2-capture.mjs TICK_HOOK=uber-audio-compare.mjs: after every compared tick the browser's
// gameplay dispatch (web/sfx-game.js, fed by the core audio event queue and the score HUD slots exactly as
// web/game-audio.js gameTick does) runs against a recording voice layer. Compared, per game tick:
//   - every voice start on the given banks (PS2 2906B8 bank slot / sound index; browser sfx.play slot / sound, with a
//     delayed start (2ADCA0: Tricky 0x6B +400 ms, fill tick 0x65 +150 ms) counted at the tick it starts),
//   - the arcade speech category calls with their variant masks (2A3DE0 Arcade_Uber, 2A3C00 Arcade_Prompts,
//     2A3CE8 Arcade_Bonus, 2A3EB8 Arcade_Icons, 2A3B18 Arcade_Power_Ups vs speech.arcade(event, mask)),
//   - the pending-Uber sound stop (29B3C0 with a live voice +0x5FDC != -1 vs the browser stopping its voice).
// AUDIO_BANKS=0,10 selects the banks (default 0 = zbxsfx: arcade / HUD / Uber / Tricky / crash sounds). AUDIO_TRACE=1
// prints both streams.
import fs from 'node:fs';
import { createSfxGame } from './sfx-game.js';
import { EV } from './audio-speech.js';

const ARCADE = { 0x2133: 'arcade_uber', 0x20a8: 'arcade_prompts', 0x20a9: 'arcade_bonus', 0x2145: 'arcade_icons', 0x20a7: 'arcade_power_ups' };
const PS2_ARCADE = new Set(Object.values(ARCADE));

export function create({ core, captureManifest, capturePath }) {
  const audioPath = capturePath.replace(/\.bin$/, '.audio.json');
  if (!fs.existsSync(audioPath)) return { tick() {}, summary: () => ({ audio: null }) };
  const banks = new Set((process.env.AUDIO_BANKS || '0').split(',').map(Number));
  const web = []; let now = 0;
  const key = (e) => e.join(':');
  // 296310 picks the crash sound 0x30 + rand(3) (browser: Math.random): compared as one class.
  const snd = (bank, sound) => (bank === 0 && sound >= 0x30 && sound <= 0x32 ? 'crash30-32' : sound);
  const voice = (o) => ({ ...o, setVolume() {}, setBend() {}, setWheel() {}, playing: () => true,
    stop() { if (o.sound === 0x66 && o.slot === 0 && !this.stopped) web.push([now, 'stop_pending_uber']); this.stopped = true; } });
  const sfx = { play: (o) => { const at = now + Math.round((o.delayMs || 0) * 60 / 1000); if (banks.has(o.slot)) web.push([at, 'snd', o.slot, snd(o.slot, o.sound), ...(o.delayMs ? ['delayed'] : [])]); return voice(o); },
    bankOf: () => ({ bnk: true }), spatial: () => ({ g: 1, az: 0, d: 5 }), stopAll() {}, voiceCount: 0 };
  const speech = new Proxy({ free: true }, { get: (t, k) => (k in t ? t[k] : k === 'arcade' ? (ev, mask) => web.push([now, ARCADE[ev] ?? ev.toString(16), mask]) : () => {}) });
  const crowd = { arm() {}, cancel() {}, trick() {}, fall() {}, update() {}, debug: () => ({}) };
  const music = { resetIntensity() {}, duckToLoops: () => false, event() {}, streamLevel() {}, loopsLevel() {}, loopsStart: () => false, loopsStop() {}, loopsRequested: () => false };
  const clips = JSON.parse(fs.readFileSync(new URL('public/assets/AUDIO/anim-events.json', import.meta.url), 'utf8'));
  const clipEvent = (clip, bit) => { const e = clips?.clips?.[String(clip >> 8)]; return e ? e[bit] ?? null : null; }; // game-audio.js
  // The race phase / countdown (race_end's info, as main.js passes it to game-audio): countdown beeps 0x4E and GO 0x5E / 0x62.
  let raceInfoPtr = 0; const raceEnd = core._race_end; core._race_end = (...a) => (raceInfoPtr = raceEnd(...a));
  const game = createSfxGame({ engine: { busGain: () => 1 }, sfx, speech, crowd, world: null, music, random: () => 0, animationEvents: clipEvent });
  const courseIndex = { ARA1: 0, BRA2: 1, BHP1: 11 }[captureManifest.location] ?? 0;
  game.start({ courseIndex, halfPipe: captureManifest.location === 'BHP1' });
  if (core._audio_events_clear) core._audio_events_clear();
  // Mid-run baseline: the dispatcher state the original carries into record 0 (HUD slot types for the create/remove edges,
  // the displayed meter step of 29AB40 and the Tricky half-seconds) comes from the seeded HUD bank and boost state.
  { const hud = new Float32Array(core.HEAPF32.buffer, core._score_hud_slots(), 44 * 6); game.G.hud.prev = Array.from({ length: 44 }, (_, k) => hud[6 * k]);
    game.G.fill.prev = Math.floor(Math.fround(hud[6 * 6 + 2] / hud[6 * 6 + 1]) * 10); game.G.frameSlots = hud.slice(); }
  // PS2 stream: the call log, in the same shape.
  const log = JSON.parse(fs.readFileSync(audioPath, 'utf8')).entries;
  const ps2 = [];
  for (const e of log) {
    if (e.name === 'play') { if (banks.has(e.x1)) ps2.push([e.tick, 'snd', e.x1, snd(e.x1, e.x2)]); }
    else if (PS2_ARCADE.has(e.name)) ps2.push([e.tick, e.name, e.a2]);
    else if (e.name === 'pending_uber_stop' && e.x0 !== -1) ps2.push([e.tick, 'stop_pending_uber']);
  }
  let first = null, lastTick = null, firstTick = null;
  return {
    tick({ tick }) {
      now = tick - 1; if (firstTick === null) firstTick = tick; lastTick = tick;
      const pending = new Float32Array(core.HEAPF32.buffer, core._animation_info(), 19)[12];
      const hud = core._score_hud_slots ? new Float32Array(core.HEAPF32.buffer, core._score_hud_slots(), 44 * 6).slice() : null;
      if (process.env.AUDIO_EVENTS_TRACE) { const q = new Float32Array(core.HEAPF32.buffer, core._audio_events(), 1 + 64 * 5), n = q[0] | 0; if (n) console.error('events', now, JSON.stringify(Array.from({ length: n }, (_, k) => Array.from(q.slice(1 + 5 * k, 6 + 5 * k))))); }
      game.tick({ riders: [{ core, character: 'zoe', slot: 0, human: true, finished: false }], raceInfo: raceInfoPtr ? new Float32Array(core.HEAPF32.buffer, raceInfoPtr, 8).slice() : null, running: true, pending, hudSlots: hud, finished: false });
    },
    summary() {
      // Both streams inside the compared window (the first compared tick's dispatch has no browser predecessor state).
      const inWin = (e) => e[0] > firstTick && e[0] <= lastTick;
      const a = ps2.filter(inWin), b = web.filter(inWin);
      // Delayed starts (2ADCA0, counted by the sound driver in milliseconds) land within one tick of the 60 Hz estimate.
      const used = new Set();
      for (const e of b) { if (e[e.length - 1] !== 'delayed') continue; e.pop(); const k = key(e.slice(1));
        const hit = a.findIndex((x, j) => !used.has(j) && Math.abs(x[0] - e[0]) <= 1 && key(x.slice(1)) === k); if (hit >= 0) { used.add(hit); e[0] = a[hit][0]; } }
      const byTick = (list) => { const m = new Map(); for (const e of list) { const k = e[0]; if (!m.has(k)) m.set(k, []); m.get(k).push(key(e.slice(1))); } for (const v of m.values()) v.sort(); return m; };
      const A = byTick(a), B = byTick(b), ticks = [...new Set([...A.keys(), ...B.keys()])].sort((x, y) => x - y);
      let exact = 0; const diffs = [];
      for (const t of ticks) { const x = (A.get(t) || []).join(','), y = (B.get(t) || []).join(','); if (x === y) exact++; else diffs.push({ tick: t, ps2: A.get(t) || [], web: B.get(t) || [] }); }
      if (process.env.AUDIO_TRACE) { console.error('ps2', JSON.stringify(a)); console.error('web', JSON.stringify(b)); }
      first = diffs[0] || null;
      return { audio: { banks: [...banks], ps2Events: a.length, webEvents: b.length, eventTicks: ticks.length, eventTicksExact: exact, firstAudioMismatch: first, audioMismatches: diffs.slice(0, 20) } };
    },
  };
}
