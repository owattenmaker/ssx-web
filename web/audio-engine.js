// Browser audio output: the original 11-channel mixer (docs/audio-logic.md section 2) as one Web Audio graph.
//
//   sources -> bus mix gain (MIX.INF, 2883B0) -> duck gain (287F00 layer) -> user slider gain -> master -> output
//
// Channels (audio+0x62BC+16*ch): 1 MUSIC, 2 DJ, 3 PA, 4 CHARACTER, 5 UI (unnamed: countdown beeps, HUD/menu
// sounds; not in MIX.INF), 6 BOARD, 7 COLLISION, 8 AMBIENT, 9 ARCADESFX, 10 ARCADESPEECH. The user sliders are
// menu steps 0..11, linear (2873D8): Music drives MUSIC, AMBIENT and DJ (DJ only while the DJ is enabled and the
// radio mode is not 2, 287558); Effects drives PA, UI, BOARD, COLLISION and, with arcade audio on (2875D0),
// ARCADESFX/ARCADESPEECH; Speech drives CHARACTER.
//
// Browsers only start audio after a user gesture (Safari is strict): unlock() must run inside a key, pointer or
// gamepad handler. Before that nothing is created and play requests are dropped.
import { onPads, pollPads } from './gamepad.js';
import { audioStats } from './audio-stats.js';   // field counters (web/diagnostics.js)

export const AUDIO_BUSES = Object.freeze(['MUSIC', 'DJ', 'PA', 'CHARACTER', 'UI', 'BOARD', 'COLLISION', 'AMBIENT', 'ARCADESFX', 'ARCADESPEECH']);
const SLIDER = Object.freeze({ MUSIC: 'music', DJ: 'music', AMBIENT: 'music', PA: 'effects', UI: 'effects', BOARD: 'effects',
  COLLISION: 'effects', ARCADESFX: 'effects', ARCADESPEECH: 'effects', CHARACTER: 'speech' });
// 287F00 mode 0 group, mask 0x173 (284BA0): MUSIC, ch5, AMBIENT, BOARD, COLLISION, ARCADESFX.
const DUCK_GROUP = Object.freeze(['MUSIC', 'UI', 'AMBIENT', 'BOARD', 'COLLISION', 'ARCADESFX']);
export const SLIDER_STEPS = 11;
// Profile defaults (0x14F458): options word 0x535610 music / speech / SFX nibbles = 10 (of 11).
export const SLIDER_DEFAULT = 10;
// SND master byte 0x50AA5D = trunc(ch0 x 127): ch0 is set once to 10/11 by the audio ctor (0x285094, gp-0x4660 =
// 0.909091) and no slider changes it, so every SND voice and stream is scaled by 115/127 (ARMSX2: the volume law is
// linear and the SPU2 stage is unity; docs/audio-logic.md 2.1).
export const MASTER_GAIN = Math.trunc((10 / 11) * 127) / 127;
// Tests and agents drive the game in automated browsers on the user's Mac: stay silent there. navigator.webdriver is set
// by Playwright / Puppeteer / safaridriver; ?mute=1 for other harnesses (web/webkit-driver.mjs); ?mute=0 forces sound.
export const testMuted = (() => {
  try {
    const q = new URL(globalThis.location?.href ?? '').searchParams.get('mute');
    if (q === '0') return false;
    if (q === '1') return true;
    return globalThis.navigator?.webdriver === true;
  } catch {
    return false;
  }
})();

// MIX.INF text -> [{index, levels: {BUS: 0..1 | undefined (-1 = keep)}, timeMs}] (parser 287FC8: defaults 100, TIME 0).
export function parseMixInf(text) {
  const mixes = []; let current = null;
  const columns = ['MUSIC', 'DJ', 'PA', 'CHARACTER', 'BOARD', 'COLLISION', 'AMBIENT', 'ARCADESFX', 'ARCADESPEECH'];
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.replace(/#.*/, '').trim(); if (!line) continue;
    const section = /^\[Mix\s+(\d+)\]$/i.exec(line);
    if (section) { current = { index: +section[1], levels: Object.fromEntries(columns.map((c) => [c, 1])), timeMs: 0 }; mixes.push(current); continue; }
    const kv = /^(\w+)\s*=\s*(-?\d+(?:\.\d+)?)$/.exec(line); if (!kv || !current) continue;
    const key = kv[1].toUpperCase(), value = +kv[2];
    if (key === 'TIME') current.timeMs = value; else if (columns.includes(key)) current.levels[key] = value < 0 ? undefined : value / 100;
  }
  return mixes.sort((a, b) => a.index - b.index).slice(0, 25);
}

// 288D18 per-channel base scales (gp+0x414.., applied by 287700: effective = slider x scale x duck x mix), set to the
// FE table by the FE bank load 285FB0 (a1 = 0) and the world table by the WORLD bank load 2862A8 (a1 = 1).
// CHARACTER lines and grunts use a per-speaker gain audio+0x636C + 4 x speaker = ch4 x gp+0x43C + 4 x speaker
// (287968 with bus 4 and a CHARDB id < 10).
export const CHANNEL_SCALE = Object.freeze({
  FE: Object.freeze({ MUSIC: 1, DJ: 1, PA: 1, CHARACTER: 0.6, UI: 1, BOARD: 1, COLLISION: 1, AMBIENT: 1, ARCADESFX: 1, ARCADESPEECH: 1,
    speakers: Object.freeze([1, 1, 1, 1, 1, 1, 1, 1, 1, 1]) }),
  WORLD: Object.freeze({ MUSIC: 1, DJ: 0.55, PA: 0.43, CHARACTER: 0.8, UI: 0.8, BOARD: 0.75, COLLISION: 0.8, AMBIENT: 0.5, ARCADESFX: 0.75, ARCADESPEECH: 0.75,
    // CHARDB order moby, kaori, allegra, mac, zoe, griff, elise, nate, psymon, viggo
    speakers: Object.freeze([0.9, 0.9, 0.9, 0.9, 0.8, 0.85, 0.9, 1, 1, 0.95]) }),
});
const f32 = Math.fround;

// interruptGate (audioInterrupt, docs/audio-logic.md 9.13): once the context has run, a context that stopped (iOS 'interrupted', a
// suspend) takes no new voices (engine.live false: sfx / speech requests are dropped instead of piling up at the frozen clock and
// all starting together on resume), it is resumed again on focus / pageshow / a retry timer as well as on input, and the holds
// (a movie, the hidden page) are counted: showing the page during a movie no longer resumes the game audio under it.
export function createAudioEngine({ mixes = [], sliders = { music: SLIDER_DEFAULT, effects: SLIDER_DEFAULT, speech: SLIDER_DEFAULT },
  djEnabled = true, radioMode = 0, arcadeAudio = true, interruptGate = false, declickMs = 0 } = {}) {
  let ctx = null, master = null, unlocked = false, mixIndex = 0, scaleMode = 'FE', hasRun = false, retry = 0, retryMs = 500;
  const holds = new Set(); // suspend(reason) holds: 'hold' (a movie, web/fe-movie.js), 'hidden' (the page)
  const gate = !!interruptGate;
  // declickMs (audioDeclick, docs/audio-logic.md 9.13): a gain before the master fades everything out before the context suspends
  // (a hidden page, a movie) and in again when it runs (also after an OS interruption): the device no longer stops mid-waveform.
  const DK = Math.max(0, +declickMs || 0) / 1000;
  let fader = null, pendingSuspend = 0;
  const fadeTo = (v) => {
    if (!fader) return; const g = fader.gain, t = ctx.currentTime;
    if (g.cancelAndHoldAtTime) g.cancelAndHoldAtTime(t); else { g.cancelScheduledValues(t); g.setValueAtTime(g.value, t); }
    g.linearRampToValueAtTime(v, t + DK);
  };
  // A refused resume (iOS without a gesture) never leaves the audio off for good: the retry timer keeps trying while the page is
  // visible (500 ms doubling to 5 s, for as long as it takes) and every key / tap / pad press tries at once (unlock).
  const refused = () => { audioStats.ctxResumeFailed++; retryLater(); };
  const tryResume = () => { if (!ctx || !unlocked || ctx.state === 'running' || (gate && holds.size)) return; ctx.resume().catch(refused); };
  const retryLater = () => { // the context stopped on its own: try again (backing off to 5 s) while the page is visible
    if (!gate || retry) return;
    retry = setTimeout(() => {
      retry = 0;
      if (!ctx || ctx.state === 'running' || holds.size) {
        retryMs = 500;
        return;
      }
      if (!globalThis.document?.hidden) tryResume();
      retryMs = Math.min(retryMs * 2, 5000);
      retryLater();
    }, retryMs);
  };
  const speakerNodes = [];
  const mixLevels = {}; // current MIX.INF target per bus (-1 levels keep the previous one)
  const unlockListeners = [];
  const nodes = {}, state = { sliders: { ...sliders }, djEnabled, radioMode, arcadeAudio };
  const sliderLevel = (bus) => {
    // 2873D8 (step x 1/11, float) x 287700 scale (mul.s)
    const v = f32(Math.min(Math.max(f32(state.sliders[SLIDER[bus]] * f32(1 / SLIDER_STEPS)), 0), 1) * f32(CHANNEL_SCALE[scaleMode][bus] ?? 1));
    if (bus === 'DJ' && (!state.djEnabled || state.radioMode === 2 || state.radioMode === 3)) return 0; // 3.3: modes 2 (ambience) and 3 (custom, no DJ)
    if ((bus === 'ARCADESFX' || bus === 'ARCADESPEECH') && !state.arcadeAudio) return 0;
    return v;
  };
  function build() {
    const AC = globalThis.AudioContext || globalThis.webkitAudioContext; if (!AC) return false;
    ctx = new AC({ latencyHint: 'interactive' });
    ctx.addEventListener?.('statechange', () => {
      if (ctx.state === 'running') { hasRun = true; retryMs = 500; if (retry) { clearTimeout(retry); retry = 0; } if (fader && !pendingSuspend) fadeTo(1); return; }
      if (fader && ctx.state !== 'closed') { const t = ctx.currentTime; fader.gain.cancelScheduledValues(t); fader.gain.setValueAtTime(0, t); } // stopped: fade in when it runs again
      if (unlocked && !holds.size && ctx.state !== 'closed') { audioStats.ctxInterrupted++; retryLater(); } // not asked for (iOS 'interrupted')
    });
    master = ctx.createGain(); master.gain.value = testMuted ? 0 : MASTER_GAIN; master.connect(ctx.destination);
    if (DK) { fader = ctx.createGain(); fader.connect(master); }
    const levels = mixes[mixIndex]?.levels ?? {};
    for (const bus of AUDIO_BUSES) {
      const mix = ctx.createGain(), duck = ctx.createGain(), user = ctx.createGain();
      mix.gain.value = levels[bus] ?? 1; user.gain.value = sliderLevel(bus);
      mix.connect(duck); duck.connect(user); user.connect(fader ?? master);
      nodes[bus] = { mix, duck, user };
    }
    for (let k = 0; k < 10; k++) { const g = ctx.createGain(); g.gain.value = CHANNEL_SCALE[scaleMode].speakers[k]; g.connect(nodes.CHARACTER.mix); speakerNodes.push(g); }
    return true;
  }
  const ramp = (param, target, seconds) => {
    const now = ctx.currentTime; param.cancelScheduledValues(now); param.setValueAtTime(param.value, now);
    if (seconds > 0) param.linearRampToValueAtTime(target, now + seconds); else param.setValueAtTime(target, now);
  };
  const refreshSliders = () => {
    if (!ctx) return;
    for (const bus of AUDIO_BUSES) ramp(nodes[bus].user.gain, sliderLevel(bus), 0.02);
    speakerNodes.forEach((g, k) => ramp(g.gain, CHANNEL_SCALE[scaleMode].speakers[k], 0.02));
  };
  return {
    get context() { return ctx; },
    get unlocked() { return unlocked; },
    // Voices may start: false while a context that has run is stopped (interrupted, suspended, a movie).
    get live() { return !gate || !ctx || !hasRun || ctx.state === 'running'; },
    get mixIndex() { return mixIndex; },
    get master() { return master; }, // QA: the final gain stage (level measurements)
    busOutput(name) { return nodes[name]?.user ?? null; }, // QA: a bus after its slider (level measurements)
    // Call from a user-gesture handler. Safe to call repeatedly.
    unlock() {
      if (!ctx && !build()) return false;
      if (gate && holds.has('hidden') && !globalThis.document?.hidden) holds.delete('hidden'); // (input means the page shows: a missed 'visible')
      if (ctx.state !== 'running' && !(gate && holds.size)) { if (!gate) holds.clear(); ctx.resume().catch(refused); }
      if (!unlocked) { unlocked = true; for (const f of unlockListeners.splice(0)) try { f(); } catch (e) { console.warn('Audio unlock listener failed', e); } }
      return true;
    },
    // Run `f` once when audio first unlocks (immediately if it already has).
    onUnlock(f) { if (unlocked) f(); else unlockListeners.push(f); },
    // The input node of a bus (sources connect here). null before unlock().
    bus(name, speaker = -1) {
      if (!ctx) return null; const n = nodes[name]; if (!n) throw new Error(`Unknown audio bus ${name}`);
      return name === 'CHARACTER' && speaker >= 0 && speaker < 10 ? speakerNodes[speaker] : n.mix;
    },
    // 288D18: 'FE' (front end) or 'WORLD' channel scales.
    setScaleMode(mode) { if (!CHANNEL_SCALE[mode] || mode === scaleMode) return; scaleMode = mode; refreshSliders(); },
    get scaleMode() { return scaleMode; },
    speakerGain(k) { return speakerNodes[k]?.gain.value ?? null; }, // QA: the per-speaker CHARACTER scale
    // 287968: a bus's effective volume (0..1): slider x mix target (the game's per-channel base x target). Used by
    // the gates of the speech/SFX dispatchers ("bus volume != 0") and by 28C8C8 (music volume).
    busGain(name) { const lv = mixes[mixIndex]?.levels?.[name]; return sliderLevel(name) * (name === 'UI' ? 1 : (mixLevels[name] ?? lv ?? 1)); },
    // 2883B0 SetMix: ignored when already current; every channel ramps linearly (2887A8/2887F8, 60 Hz steps) to the
    // preset level over the preset's TIME; a -1 level keeps the channel as it is.
    setMix(index) {
      index = Math.max(0, index | 0); if (index === mixIndex) return;
      const mix = mixes[index]; if (!mix) throw new Error(`Unknown mix ${index}`);
      mixIndex = index; for (const bus of AUDIO_BUSES) if (mix.levels[bus] !== undefined) mixLevels[bus] = mix.levels[bus];
      if (!ctx) return;
      for (const bus of AUDIO_BUSES) { const target = mix.levels[bus]; if (target !== undefined) ramp(nodes[bus].mix.gain, target, mix.timeMs / 1000); }
    },
    // 287F00 duck layer. duck(): mode 0 group to `level` over `seconds` (DJ talk-over: 0.65 over 0.9999 s, 29F000);
    // release(): mode 3, every ducked channel back to 1.0 (0.5 s after the DJ stream stops, 285BF8).
    duck(level = 0.65, seconds = 0.9999) { if (ctx) for (const bus of DUCK_GROUP) ramp(nodes[bus].duck.gain, level, seconds); },
    release(seconds = 0.5) { if (ctx) for (const bus of AUDIO_BUSES) ramp(nodes[bus].duck.gain, 1, seconds); },
    // Audio menu: slider steps 0..11 for 'music', 'effects', 'speech'; DJ on/off; radio mode; arcade audio.
    setSlider(group, steps) { state.sliders[group] = Math.min(Math.max(Math.round(steps), 0), SLIDER_STEPS); refreshSliders(); },
    slider(group) { return state.sliders[group]; },
    setDj(on) { state.djEnabled = !!on; refreshSliders(); },
    setRadioMode(mode) { state.radioMode = mode | 0; refreshSliders(); },
    setArcadeAudio(on) { state.arcadeAudio = !!on; refreshSliders(); },
    // Page hidden: stop the audio clock too. Back from the background iOS Safari often refuses to restart the context
    // without a user gesture ("Failed to start the audio device", an unhandled rejection in 5 field sessions): the
    // failure is swallowed here and the next key / tap / pad press retries (installAudioUnlock -> unlock()).
    // reason: 'hold' (a movie) or 'hidden' (the page); with the gate the context runs again only when no hold is left.
    suspend(reason = 'hold') {
      holds.add(reason); if (!ctx || ctx.state !== 'running') return;
      if (!fader) return ctx.suspend().catch(() => {});
      fadeTo(0); clearTimeout(pendingSuspend); // (declick: the fade first; a hidden page's timer may run up to ~1 s later, still faded)
      pendingSuspend = setTimeout(() => { pendingSuspend = 0; if (holds.size && ctx.state === 'running') ctx.suspend().catch(() => {}); }, DK * 1000 + 15);
    },
    resume(reason = 'hold') {
      holds.delete(reason); if (!gate) holds.clear(); else if (holds.size) return;
      if (pendingSuspend) { clearTimeout(pendingSuspend); pendingSuspend = 0; if (ctx?.state === 'running') { fadeTo(1); return; } }
      if (ctx && unlocked && ctx.state !== 'running') return ctx.resume().catch(() => { refused(); return false; });
    },
    retryResume() { tryResume(); }, // focus / pageshow (installAudioUnlock)
    get gated() { return gate; },
    get fader() { return fader; }, // QA: the declick gain before the master (the whole mix)
  };
}

// Unlock `engine` on the first key, pointer or gamepad input, and suspend audio while the page is hidden.
// Returns a function that removes the listeners.
export function installAudioUnlock(engine, target = globalThis) {
  const unlock = () => engine.unlock();
  const events = ['keydown', 'pointerdown', 'touchend', 'mousedown'];
  for (const e of events) target.addEventListener?.(e, unlock, { capture: true });
  let polling = true;
  // Gamepads: any button on any pad of any slot (web/gamepad.js 'use' events, polled here until unlocked).
  // (with the interruption gate the pad listener stays: a pad press also restarts a context that stopped, as keys and taps do)
  const offPads = onPads((type) => { if (type === 'use' && (!engine.unlocked || engine.context?.state !== 'running')) engine.unlock(); });
  const pollLoop = () => { if (!polling) return; if (engine.unlocked) { if (!engine.gated) offPads(); return; } pollPads(); globalThis.requestAnimationFrame?.(pollLoop); };
  globalThis.requestAnimationFrame?.(pollLoop);
  const visibility = () => { if (globalThis.document?.hidden) engine.suspend('hidden'); else engine.resume('hidden'); };
  globalThis.document?.addEventListener?.('visibilitychange', visibility);
  const again = () => { if (engine.gated) engine.retryResume(); };
  for (const e of ['focus', 'pageshow']) globalThis.addEventListener?.(e, again);
  return () => {
    polling = false;
    offPads();
    for (const e of events) target.removeEventListener?.(e, unlock, { capture: true });
    globalThis.document?.removeEventListener?.('visibilitychange', visibility);
    for (const e of ['focus', 'pageshow']) globalThis.removeEventListener?.(e, again);
  };
}
