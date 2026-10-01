// The race replay (PS2 cReplay, 0x26C458..0x2721D0; docs/replay.md). The original re-runs the game: at the start gate it
// snapshots the world, then records each human's controller command per tick (RLE, 4096 runs), and a replay restores the
// snapshot and runs the normal 60 Hz simulation with the recorded commands in place of the pad; the computer riders are
// simply simulated again. The browser does the same without the snapshot: the simulation is deterministic, so a replay
// re-initialises the race on the same core through main.js startRun (the restart path) with the few words the reset does not
// cover (web/replay.js start state: the RNG words, the relationships, the career's collectible rows, the time limit ...) and
// feeds the recorded pad. Nothing but the pad samples and that start state is kept: no second copy of the core.
//
//   auto   behind the post-race screens (0x20A8F8 -> 0x2706B8, state 9 -> 1 with R+0x61C loop): from the first countdown tick to
//          the finish tick, 1x, looping; the pad is ignored (0x26FB88 returns while +0x61C is set).
//   full   the results' Replay item (0x20CE0C -> cOVState_REPLAY '64replay', 0x26F8A0(R, 0, 0, 0)): starts paused on the first
//          tick, plays to the finish and pauses there; Cross play / pause, Circle step / slow, R1 / L1 skip (paused only),
//          Triangle camera, Square timeline, D-pad the help panel, Start the Replay Menu (web/replay-ui.js).
import { FixedStepClock } from './fixed-step-clock.js';
import { padStreamInfo } from './server/replay-file.mjs';   // shared with the server (web/server/ imports only web/server/)

// Replay camera cycle 0x445438[R+0x630] with the names 0x20E6B0 writes (ASCII in the ELF, 0x471C40..): Web-cam is the
// trigger-driven auto camera (director mode 0x5D).
export const REPLAY_CAMERAS = Object.freeze([
  { type: 0x5D, name: '-  Web-cam' }, { type: 0x0B, name: '-  Manual-cam' }, { type: 0x3C, name: '-  Near-cam' }, { type: 0x3D, name: '-  Mid-cam' },
  { type: 0x3E, name: '-  Far-cam' }, { type: 0x5E, name: '-  2:00' }, { type: 0x5F, name: '-  10:00' }, { type: 0x60, name: '-  4:00' }, { type: 0x61, name: '-  8:00' }]);

// The recorded pad of one run: the 24-channel pad each tick fed the core (web/game-tick.js simulate) as a byte stream of
// changes, plus the out-of-band calls between ticks (pause Give Up, the Pause Options camera). Every channel web/pad-input.js
// builds is a pad byte (towardZero(byte / 255), 0 / 1 for Select..R3), so a change costs its channel mask and one byte a
// changed channel: a record = varint tick delta, 3-byte mask of the changed channels, 3-byte mask of those kept as float32 (a
// value that is no pad byte, e.g. a QA pad), then the values. A keyboard run is ~10 bytes per key change, an analog stick run
// ~10 bytes a tick (~100 KB for a 3-minute race).
const R255 = new Float32Array(new Uint32Array([0x3b808081]).buffer)[0];
function towardZero(x) { const f = new Float32Array([x]); if (Math.abs(f[0]) > Math.abs(x)) new Uint32Array(f.buffer)[0] -= 1; return f[0]; }
const BYTE_VALUE = Float32Array.from({ length: 256 }, (_, b) => towardZero(b * R255));
const byteOf = (i, v) => { if (i < 4) return v === 0 ? 0 : v === 1 ? 1 : -1; const b = Math.round(v * 255); return b >= 0 && b <= 255 && Object.is(BYTE_VALUE[b], v) ? b : -1; };
export function createRecording() {
  let buf = new Uint8Array(1024), len = 0, ticks = 0, records = 0, lastTick = 0;
  const events = [], last = new Float32Array(24), f32 = new Float32Array(1), u8 = new Uint8Array(f32.buffer);
  const put = (b) => { if (len === buf.length) { const n = new Uint8Array(len * 2); n.set(buf); buf = n; } buf[len++] = b; };
  return {
    get ticks() { return ticks; }, get runs() { return records; }, events,
    get bytes() { return len + events.length * 16; },
    push(input) {
      let mask = 0, floats = 0;
      for (let i = 0; i < 24; i++) { const v = Math.fround(input[i] ?? 0); if (!Object.is(v, last[i])) { mask |= 1 << i; if (byteOf(i, v) < 0) floats |= 1 << i; } }
      if (mask) {
        let dt = ticks - lastTick; lastTick = ticks; records++;
        do { put((dt & 0x7F) | (dt > 0x7F ? 0x80 : 0)); dt >>>= 7; } while (dt);
        put(mask & 255); put((mask >> 8) & 255); put((mask >> 16) & 255); put(floats & 255); put((floats >> 8) & 255); put((floats >> 16) & 255);
        for (let i = 0; i < 24; i++) if (mask & (1 << i)) {
          const v = Math.fround(input[i] ?? 0); last[i] = v;
          if (floats & (1 << i)) { f32[0] = v; for (let k = 0; k < 4; k++) put(u8[k]); } else put(byteOf(i, v));
        }
      }
      ticks++;
    },
    note(kind, value) { events.push({ tick: ticks, kind, value }); },
    // The pad of tick t (0-based), into out (Float32Array(24)); cursor: the caller's sequential reader state (a read before
    // the cursor starts over from the first record).
    pad(t, out, cursor = {}) {
      if (!(cursor.at <= t)) { cursor.pos = 0; cursor.at = 0; cursor.base = 0; cursor.next = -1; cursor.pad = new Float32Array(24); }
      const P = cursor.pad;
      for (;;) {
        if (cursor.next < 0) {   // read the next record's tick
          if (cursor.pos >= len) { cursor.next = Infinity; break; }
          let dt = 0, sh = 0, b; do { b = buf[cursor.pos++]; dt |= (b & 0x7F) << sh; sh += 7; } while (b & 0x80);
          cursor.next = (cursor.base ?? 0) + dt; cursor.base = cursor.next;
        }
        if (cursor.next > t) break;
        const q = cursor.pos, mask = buf[q] | (buf[q + 1] << 8) | (buf[q + 2] << 16), floats = buf[q + 3] | (buf[q + 4] << 8) | (buf[q + 5] << 16); cursor.pos += 6;
        for (let i = 0; i < 24; i++) if (mask & (1 << i)) {
          if (floats & (1 << i)) { for (let k = 0; k < 4; k++) u8[k] = buf[cursor.pos++]; P[i] = f32[0]; }
          else { const b = buf[cursor.pos++]; P[i] = i < 4 ? b : BYTE_VALUE[b]; }
        }
        cursor.next = -1;
      }
      cursor.at = t; out.set(P); return out;
    },
    eventsAt(t) { return events.filter((e) => e.tick === t); },
    // pv onlineRecords (docs/online-records.md): the stream as bytes (web/server/replay-file.mjs), and a recording made from such bytes
    // (a downloaded run: its ticks and out-of-band calls come with it). An imported recording only plays back.
    exportBytes() { return buf.slice(0, len); },
    importBytes(bytes, tickCount, calls = []) {
      const info = padStreamInfo(bytes); if (!info) throw new Error('bad pad stream');
      buf = Uint8Array.from(bytes); len = buf.length; ticks = tickCount; records = info.records; lastTick = Math.max(0, info.lastTick);
      events.length = 0; for (const e of calls) events.push({ tick: e.tick | 0, kind: String(e.kind), value: e.value });
    },
  };
}

// host (main.js):
//   allowed()             the event may replay (race, slope style, big air, pipe, rival; not peak runs, free ride, online)
//   snapshot()            the start state the reset does not restore, taken at the end of a live startRun
//   restart(snapshot)     main.js startRun(snapshot): the race again from its start, no UI / career / audio side effects
//   simulate(input)       web/game-tick.js simulate (one tick); present(rec) its present (replay-aware)
//   event(e)              apply a recorded out-of-band call before its tick
//   ended()               the replay stopped: host clean-up (relationships back, flags)
export function createReplay(host) {
  let rec = null, snapshot = null, finishTick = -1, autoPending = false;   // the live run: recording, its start state, its finish tick
  let mode = null, t = 0, paused = false, slow = 0, cursor = {}, loops = 0, played = 0, seekTo = -1;
  // Highlight buckets (0x2707E0 / 0x270870 / 0x2708F0): the 60-tick bucket of a take-off is kept when its landing gained 1000
  // points or the jump lasted 5 s, or it ended in a crash; a forced reset drops it; 3 per human in single player (R+0xC).
  // R1 / L1 jump between the kept snapshots: the start, these buckets and the end (PS2 Snow Jam final: 6300, 11820, end).
  let highlights = [], air = null, replayed = false;   // replayed: a replay of this run has started (the view's camera carries on from it)
  const pad = new Float32Array(24), clock = new FixedStepClock(), manual = [0, 0, 0];
  let camera = 0;   // REPLAY_CAMERAS index (R+0x630), 0 = Web-cam at every start (0x26F228 / 0x26EEA0)
  const api = {
    // ---- live run ----
    liveStart() { if (mode) api.stop(); rec = createRecording(); snapshot = host.snapshot(); finishTick = -1; autoPending = false; highlights = []; air = null; replayed = false; host.prepare?.(); },
    // After each live tick (web/game-tick.js): the run's highlights. grounded / score (the banked run score) / crash / reset.
    observe(grounded, score, crashing, resetting) {
      if (!rec || finishTick >= 0 || mode || highlights.length >= 3) return;
      const tick = rec.ticks - 1;
      if (resetting) { air = null; return; }
      if (!grounded && !air) air = { bucket: Math.floor(tick / 60) * 60, at: tick, score };
      else if (air && (grounded || crashing)) {
        if (crashing || score - air.score >= 1000 || tick - air.at >= 300) { if (!highlights.includes(air.bucket)) highlights.push(air.bucket); }
        air = null;
      }
    },
    get highlights() { return highlights.slice(); },
    // The run's results are shown (web/game-tick.js): the auto replay starts when the post-race screen is up (state 7 enter).
    resultsShown() { autoPending = true; }, get autoPending() { return autoPending; },
    // One live tick's pad (web/game-tick.js simulate, before the core sees it); nothing once the run finished (the
    // post-finish coast is not recorded: 0x233C50 -> 0x26FA50 writes the end frame at EndRace).
    record(input) { if (rec && finishTick < 0 && !mode) rec.push(input); },
    note(kind, value) { if (rec && finishTick < 0 && !mode) rec.note(kind, value); },
    finish() { if (rec && finishTick < 0 && !mode) finishTick = rec.ticks - 1; },
    get recording() { return rec; }, get finishTick() { return finishTick; }, get snapshot() { return snapshot; },
    get length() { return finishTick + 1; },
    available() { return !!rec && finishTick >= 0 && !!snapshot && host.allowed(); },
    // ---- playback ----
    get mode() { return mode; }, get active() { return !!mode; }, get tick() { return t; }, get paused() { return paused; }, get seeking() { return seekTo >= 0; },
    get loops() { return loops; }, get played() { return played; }, get pending() { return paused || seekTo >= 0 ? 0 : clock.pending; },
    get camera() { return camera; }, get cameraType() { return REPLAY_CAMERAS[camera].type; }, get cameraName() { return REPLAY_CAMERAS[camera].name; },
    // The fraction the timeline marker shows ("percentcomplete" = R+0x484 / R+0x3CC x 315 + 170).
    get progress() { return finishTick > 0 ? Math.min(1, t / (finishTick + 1)) : 0; },
    start(which) {
      if (!api.available()) return false;
      mode = which; camera = 0; loops = 0; slow = 0; autoPending = false; seekTo = -1;
      if (!api.rewind(true)) { mode = null; return false; }
      // The full replay starts paused on its first frame (state 7 -> 3): the original shows the restored start snapshot; the
      // browser draws the run's first tick (the riders and the replay view need one step).
      paused = false; if (which === 'full') { api.step(); paused = true; }
      return true;
    },
    // Back to the first tick (the start snapshot again): startRun(snapshot) re-initialises the race on the same core.
    rewind(first = false) {
      if (!host.restart(snapshot)) return false;
      t = 0; cursor = {}; clock.reset();
      if (first) { host.cameraBegin?.(!replayed); replayed = true; } else host.cameraRewind?.();   // 0x26EE68 / 0x1620D0 (web/replay_camera.inc)
      return true;
    },
    // The Manual camera's sticks (left x / y, right x in [-1, 1]), read by the replay view's step (web/game-tick.js).
    get manual() { return manual; }, setManual(x, y, z) { manual[0] = x; manual[1] = y; manual[2] = z; },
    stop() { if (!mode) return; mode = null; paused = false; seekTo = -1; host.ended(); },
    // One simulated tick of the replay (the recorded pad and the calls recorded before it).
    step() {
      for (const e of rec.eventsAt(t)) host.event(e);
      const r = host.simulate(rec.pad(t, pad, cursor));
      t++; played++;
      if (r) host.present(r);
      return r;
    },
    // A frame of the running replay: the 60 Hz ticks of dt (web/fixed-step-clock.js), looping (auto) or pausing (full) after
    // the finish tick. Slow motion (Circle held, 0x26FB88 ReplayStepSlow): after 10 ticks one step every other tick.
    frame(dt) {
      if (!mode) return 0;
      if (seekTo >= 0) return api.seekFrame();
      if (paused) { clock.reset(); host.cameraPaused?.(); return 0; }   // 0x26FA78: the view still follows a camera change / the Manual sticks
      return clock.advance(Math.min(dt, 0.25), () => {
        if (!mode || paused) return;
        if (t > finishTick) {
          if (mode === 'auto') { loops++; if (!api.rewind()) api.stop(); return; }
          paused = true; return;
        }
        if (slow && (slow++ < 10 || (slow & 1))) return;
        api.step();
        if (mode === 'full' && t > finishTick) paused = true;
      });
    },
    // ---- the full replay's transport (0x26FB88) ----
    playPause() { if (mode !== 'full') return; if (t > finishTick) { if (!api.rewind()) return api.stop(); } paused = !paused; slow = 0; },
    stepOnce() { if (mode !== 'full') return; if (!paused) { paused = true; return; } if (t <= finishTick) api.step(); },
    setSlow(on) { if (mode !== 'full' || seekTo >= 0) return; if (on) { if (!slow) slow = 10; paused = false; } else if (slow) { slow = 0; paused = true; } },
    pause() { if (mode === 'full' && seekTo < 0) { paused = true; slow = 0; } },
    // R1 / L1 (0x26FE50 seek commands 3 / 4, only while paused): to the next / previous kept snapshot, then paused. The
    // original restores that snapshot; the browser runs the simulation there (fast, drawing each frame's last tick), from the
    // start when it goes back. Returns false at the end / start (the refusal sound).
    skip(dir) {
      if (mode !== 'full' || !paused || seekTo >= 0) return false;
      const points = [0, ...highlights.filter((b) => b > 0 && b <= finishTick).sort((a, b) => a - b), finishTick + 1];
      const target = dir > 0 ? points.find((p) => p > t) : [...points].reverse().find((p) => p < t);
      if (target == null) return false;
      if (target < t && !api.rewind()) { api.stop(); return false; }
      if (target === 0) { api.step(); return true; }   // the start: its first tick drawn, as the full replay's first frame
      seekTo = target; slow = 0; return true;
    },
    // One frame of a seek: as many ticks as fit in ~40 ms, the frame's last one drawn; the view cuts when it lands (0x1620D0).
    seekFrame() {
      const until = performance.now() + 40; let n = 0;
      while (seekTo >= 0 && t < seekTo && (n === 0 || performance.now() < until)) {
        for (const e of rec.eventsAt(t)) host.event(e);
        const r = host.simulate(rec.pad(t, pad, cursor)); t++; played++; n++;
        if (t >= seekTo || performance.now() >= until) { if (r) host.present(r); } else host.skipped?.(r);
      }
      if (t >= seekTo) { seekTo = -1; paused = true; host.cameraRewind?.(); }
      return n;
    },
    cycleCamera() { if (mode !== 'full') return; camera = (camera + 1) % REPLAY_CAMERAS.length; host.cameraMode?.(api.cameraType); },
  };
  return api;
}
