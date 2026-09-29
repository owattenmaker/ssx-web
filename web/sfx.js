// Sound-effect player: the original bank slots, the game voice manager (2906B8 / 2AC868) and the EA SND patch
// voices (3BB588 / 3B55F0), played through the web/audio-engine.js buses. docs/audio-logic.md section 5.
// Which sound plays when is decided by web/sfx-game.js (the ported 29xxxx dispatchers).
//
// Request (2906B8): {slot, sound, bus, volume 0..127 (mBaseVol), bend (0x1000 = 1.0), position (source cm, Z up;
// null = 2D), posStatic, tag, callback(voice, dt)}. Every layer of the bank entry plays (3BB588 starts all patches
// whose velocity/key ranges contain 127/60, which is every patch in retail). Per layer (3BA7B0/3BA938):
//   gain  = patchVol x gameVol x env / 127^3 x volLFO / 127      (patchVol = tag 0x0E +- rnd(tag 0x0F))
//   pitch = 2^(cents/1200) x bend/4096, cents = tag 0x10 + (60 - root tag 0x07) x 100 +- rnd(tag 0x11) + pitch LFO
//   pan   = equal-power between speakers at +-90 deg: patch pan (tag 0x0C +- rnd(tag 0x0D)) + azimuth
// gameVol (2AC868, once per frame) = trunc(base x ((30 - max(d - 0.5, 0)) / 30)^2) for positional voices
// (d = metres from the listener: camera view space + (0, 25, -100) cm), else base; changes <= 4 are not sent.
// The bus gain (slider x mix x duck) is applied by the engine's bus nodes (the mVolControl pointer 287968).
import { decodeBankPatch, toAudioBuffer } from './audio-decode.js';
import { createGuardedWorker, workerUrl } from './worker-guard.js';   // build handshake + main-thread fallback (docs/workers.md)
import { decodeAudioJob } from './audio-decode-job.js';
const AUDIO_DECODE_WORKER = workerUrl((Worker) => new Worker(new URL('./audio-decode-worker.js', import.meta.url), { type: 'module' }));
import { spuResample } from './spu-interp.js';
import { audioStats, noteDecode } from './audio-stats.js';   // field counters (web/diagnostics.js)
import { pv } from './pv-flags.js';

// Bank slots (loaders 285FB0 FE, 2862A8 WORLD from BANKS.INF, 29F3F8 grunts, 28F700 loading screen).
export const SLOT = Object.freeze({ MAIN: 0, BOARD: 1, MOUNTAIN: 2, DYNAMIC: 3, TRANSPORT: 4, CROWD: 5, AUX: 6, TRICKY: 7,
  WORLD8: 8, WORLD9: 9, LAND: 0xA, DYNAMIC_C: 0xC, LOADING: 0xD, GRUNT: 0xE, GRUNT_AI: 0xF, SPUBOARD: 0x10 });

// 290B58: 5-point piecewise-linear curve [x[0..4], y[0..4]] (first interval containing v, else the ends; 0 when v
// falls into a gap of a non-monotonic x list).
export function curve(c, v) {
  const x = c[0], y = c[1];
  for (let i = 1; i < 5; i++) if (x[i - 1] <= v && v < x[i]) return y[i - 1] + (v - x[i - 1]) * (y[i] - y[i - 1]) / (x[i] - x[i - 1]);
  if (v < x[0]) return y[0];
  if (x[4] <= v) return y[4];
  return 0;
}
const C = (x, y) => Object.freeze([Object.freeze(x), Object.freeze(y)]);
// Curve tables read from SLUS_207.72 (addresses in the names).
export const CURVES = Object.freeze({
  loopA_vol_482AF8: C([0.00024549, 0.00255361, 0.02695501, 0.1500098, 0.1999571], [0, 101, 61, 0, 0]),
  loopA_pitch_482B20: C([0.05, 0.1, 0.15, 0.2, 0.25], [1.000076, 1.2, 1.5, 2, 2]),
  glide0_vol_482B78: C([0.0185, 0.09460434, 0.3431811, 0.7000265, 1], [0, 16, 28, 86, 104]),
  glide0_pitch_482BA0: C([0.05, 0, 0.5, 0.8054432, 0.95], [1, 0.5, 0.5, 0.5, 0.5]),
  glide1_vol_482BC8: C([0.0185, 0.1, 0.5, 0.7000253, 0.9510077], [2, 15, 32, 97, 127]),
  glide1_pitch_482BF0: C([0.1555642, 1, 0.5, 0.8, 0.95], [0.9999683, 0.9358777, 0.75, 0.6496397, 0.5]),
  glide2_vol_482C18: C([0.0185, 0.1, 0.5, 0.6, 0.95], [2, 15, 30, 50, 80]),
  glide2_pitch_482C40: C([0.05, 0.2, 0.5, 0.8, 0.95], [1, 0.9, 0.75, 0.65, 0.5]),
  carve0_vol_482C68: C([0.01004235, 0.2500007, 0.4999053, 0.7587754, 1], [0, 11, 36, 71, 107]),
  carve0_pitch_482C90: C([0, 0, 0.4999508, 0.8, 1], [1, 0.5, 0.5, 0.5, 0.5]),
  carve1_vol_482CB8: C([0.01, 0.2500082, 0.5000058, 0.9, 1], [0, 49, 59, 70, 127]),
  carve1_pitch_482CE0: C([0.05, 0.2, 0.5, 0.8, 0.95], [0.6011547, 0.5, 0.5672877, 0.5, 0.5]),
  carve2_pitch_482D08: C([0.05, 0.1544433, 0.5, 0.8, 0.95], [1, 0.5, 0.5, 1.000354, 0.5]),
  landing_vol_482D30: C([100, 300, 800, 1200, 2000], [10, 28, 34, 80, 127]),
  crash_vol_482DA0: C([100, 800, 1200, 1400, 1600], [127, 127, 127, 127, 127]),
  // 290C10(i, x): table 0x445898 + i*0x28.
  contact_445898: C([200, 400, 550, 800, 800], [33, 70, 100, 127, 127]),
  bump_4458C0: C([100, 1000, 2000, 4000, 4000], [20, 70, 100, 127, 127]),
  t2_4458E8: C([500, 1000, 1500, 2000, 2000], [0, 10, 20, 40, 40]),
  grunt_445910: C([500, 1000, 1500, 2000, 2500], [60, 75, 90, 100, 127]),
});

// 291710: surface id (rider+0x438, -1..19) -> board sound class (jump table 0x482AA0).
const SURFACE_CLASS = [0, 0, 2, 1, 1, 3, 0, 0, 7, 7, 5, 4, 8, 0, 1, 0, 0, 0, 0, 7, 0];
export function surfaceClass(surface) { return SURFACE_CLASS[(surface | 0) + 1] ?? 0; }
// 2934D0: class -> board loop family (0 snow, 1 packed/ice, 2 hard: rails, wood, metal).
export function boardFamily(cls, motion) { return cls === 0 ? 0 : cls <= 2 ? 1 : cls === 3 ? (motion === 4 ? 2 : 0) : 2; }
// 3C72A0: ((rand & 0x7FFF) - 0x4000) * x >> 14, uniform in [-x, x).
export const rndRange = (x, random = Math.random) => (((Math.floor(random() * 0x8000)) - 0x4000) * x) >> 14;
// 290B58-style trunc toward zero (cvt.w.s).
const trunc = Math.trunc;

// ---- patch parameters with their file addresses (table tags are self-relative offsets, 3C72F8 signed BE) ----
export function parsePatchTags(bnk, headerOffset) {
  let p = headerOffset + 4; const patches = []; let cur = new Map();
  for (;;) {
    const t = bnk[p++];
    if (t === 0xfc || t === 0xfd) continue;
    if (t === 0xff) break;
    if (t === 0xfe) { patches.push(cur); cur = new Map(); continue; }
    let len = bnk[p++];
    if (len === 0xff) { len = ((bnk[p] << 24) | (bnk[p + 1] << 16) | (bnk[p + 2] << 8) | bnk[p + 3]) >>> 0; p += 4; }
    if (t < 0x80 && len <= 4) {
      let v = 0; for (let i = 0; i < len; i++) v = v * 256 + bnk[p + i];
      if (len && bnk[p] & 0x80) v -= 2 ** (8 * len);
      cur.set(t, { v, addr: p });
    }
    p += len;
  }
  patches.push(cur);
  return patches;
}
const i32 = (b, a) => (b[a] | (b[a + 1] << 8) | (b[a + 2] << 16) | (b[a + 3] << 24));
// Envelope 0x19: {s32 ticks, s32 level} x count (tag 0x09), 100 Hz ticks, start level tag 0x1C.
function patchEnvelope(bnk, tags) {
  const t = tags.get(0x19); if (!t) return null;
  const count = Math.max(tags.get(0x09)?.v ?? 1, 1), base = t.addr + t.v, segments = [];
  for (let j = 0; j < count; j++) segments.push({ ticks: i32(bnk, base + 8 * j), level: i32(bnk, base + 8 * j + 4) });
  return { start: tags.get(0x1c)?.v ?? 127, segments };
}
// LFO tables (int8) 0x1D (volume, length 0x1E) / 0x20 (pitch, length 0x21, depth 0x22, random start 0x23).
function patchTable(bnk, tags, tag, lengthTag) {
  const t = tags.get(tag); if (!t) return null;
  let n = (tags.get(lengthTag)?.v ?? 0) & 0xff; if (!n) n = 256;
  const base = t.addr + t.v, values = new Int8Array(n);
  for (let i = 0; i < n; i++) values[i] = (bnk[base + i] << 24) >> 24;
  return values;
}

// Listener (285930): camera view space + (0, 25, -100) cm; az = atan2(x, z) (0 ahead, +90 deg right), metres.

// startAfterDecode (pv sfxStartAfterDecode, docs/audio-logic.md 9.13): a voice's layers are decoded before its start time is read, so
// a first play that decodes starts at once with its whole envelope, not already late (its attack skipped by the decode time).
export function createSfx({ engine, fetchJson, fetchBytes, random = Math.random, mono = false, startAfterDecode = false, evictMs = 30000 }) {
  const banks = new Map();      // slot -> {name, promise, json, bnk, tags: Map(entry -> patches)}
  const buffers = new Map();    // `${name}/${entry}/${layer}` -> AudioBuffer
  const lfoBuffers = new Map(); // table key -> AudioBuffer (100 Hz steps at 3000 Hz)
  const voices = new Set();
  let playLog = null; // QA: [time, slot, sound, bus, volume, distance gain, layers] of recent requests
  let listener = null;          // {inv: camera matrixWorldInverse elements (browser metres)}
  const ctx = () => (engine.unlocked && engine.live !== false ? engine.context : null); // (live: pv audioInterrupt, a stopped context takes no voices)

  function loadBank(slot, name) {
    const current = banks.get(slot);
    if (current && current.name === name) return current.promise;
    keep(name); if (current) evictLater(current.name); // pv bankEvict
    const bank = { name, json: null, bnk: null, tags: new Map() };
    // The bank becomes playable (bank.bnk set) once its MicroTalk patches are decoded off-thread, like the original's
    // asynchronous bank load (28BD10 / 29B818 report "loaded" only when the data is in memory).
    bank.promise = fetchJson(`banks/${name}.json`)
      .then((json) => fetchBytes(`banks/${json.file}`).then(async (raw) => {
        const bnk = raw instanceof Uint8Array ? raw : new Uint8Array(raw);
        await predecode(name, json, bnk);
        bank.json = json; bank.bnk = bnk; return bank;
      }))
      .catch((e) => { console.warn(`Bank ${name} unavailable`, e); return bank; });
    banks.set(slot, bank);
    return bank.promise;
  }
  function unloadBank(slot) { const b = banks.get(slot); banks.delete(slot); if (b) evictLater(b.name); }
  // MicroTalk patches of a bank -> decoded (key `${name}/${sound}/${layer}`), in a worker when available.
  const decoded = new Map();
  // pv bankEvict: a bank no slot holds any more drops its decoded patches and AudioBuffers after EVICT_MS (both caches only grew:
  // a streamed world swaps slots 8 / 9 per location (286CA8), so a long ride kept the PCM of every location it passed). The grace
  // period keeps a rider hovering at a row boundary from re-decoding on every swap: a bank loaded again within it keeps its cache
  // (predecode skips decoded keys). A bank still decoding is evicted once its decode has ended; playing voices hold their buffers.
  const EVICT_MS = evictMs, evictTimers = new Map(), decoding = new Map(); // name -> timer / decodes in flight
  const held = (name) => { for (const b of banks.values()) if (b.name === name) return true; return false; };
  function keep(name) { clearTimeout(evictTimers.get(name)); evictTimers.delete(name); }
  function evictLater(name) {
    if (!pv('bankEvict') || !name) return;
    clearTimeout(evictTimers.get(name));
    evictTimers.set(name, setTimeout(() => {
      evictTimers.delete(name);
      if (held(name)) return;
      if (decoding.get(name)) { evictLater(name); return; }
      const pre = `${name}/`;
      for (const k of [...decoded.keys()]) if (k.startsWith(pre)) decoded.delete(k);
      for (const k of [...buffers.keys()]) if (k.startsWith(pre)) buffers.delete(k);
      evicted.push(name); if (evicted.length > 64) evicted.shift();
    }, EVICT_MS));
  }
  const evicted = []; // QA: the last banks evicted
  let worker = null;
  function decodeWorker() {
    if (worker !== null || typeof Worker === 'undefined') return worker;
    worker = createGuardedWorker({ name: 'audio-decode', url: AUDIO_DECODE_WORKER, local: () => decodeAudioJob, cloneInput: false });
    return worker;
  }
  async function predecode(name, json, bnk) {
    const list = [];
    json.entries?.forEach((e, sound) => e?.patches?.forEach((patch, layer) => { if (patch.codec === 4) list.push({ key: `${name}/${sound}/${layer}`, patch }); }));
    const todo = list.filter((x) => !decoded.has(x.key) && !buffers.has(x.key)); if (!todo.length) return;
    const w = decodeWorker();
    const run = (items) => w ? w.request({ bnk, patches: items.map((x) => x.patch) })
      : Promise.resolve(items.map((x) => decodeBankPatch(bnk, x.patch)));
    decoding.set(name, (decoding.get(name) ?? 0) + 1);
    try {
      for (let i = 0; i < todo.length; i += 8) { // small batches: other banks' requests interleave
        const items = todo.slice(i, i + 8);
        const out = await run(items).catch(() => items.map((x) => decodeBankPatch(bnk, x.patch)));
        items.forEach((x, k) => decoded.set(x.key, out[k]));
      }
    } finally { const n = decoding.get(name) - 1; if (n > 0) decoding.set(name, n); else decoding.delete(name); }
  }
  // Decode every patch of the given loaded banks into the buffer cache in time slices (the loading screen keeps
  // animating): the first play of a sound then costs nothing (MicroTalk ambience loops take ~20 ms to decode).
  let warming = false;
  async function warm(slots, { budgetMs = 4 } = {}) {
    const slice = () => new Promise((r) => setTimeout(r, 0));
    for (const slot of slots) {
      const bank = banks.get(slot); if (!bank) continue;
      await bank.promise; if (!bank.bnk || !engine.context) continue;
      let t0 = performance.now();
      for (let sound = 0; sound < (bank.json?.entries?.length ?? 0); sound++) {
        if (banks.get(slot) !== bank) break;
        const layers = layersOf(bank, sound); if (!layers) continue;
        warming = true; for (const layer of layers) { try { bufferFor(bank, sound, layer); } catch {} } warming = false;
        if (performance.now() - t0 > budgetMs) { await slice(); t0 = performance.now(); }
      }
    }
  }
  const bankOf = (slot) => banks.get(slot);

  function layersOf(bank, sound) {
    const entry = bank.json?.entries?.[sound]; if (!entry?.patches?.length) return null;
    let tags = bank.tags.get(sound);
    if (!tags) { tags = parsePatchTags(bank.bnk, entry.headerOffset); bank.tags.set(sound, tags); }
    return entry.patches.map((patch, i) => ({ patch, tags: tags[i] ?? new Map(), index: i }));
  }
  function bufferFor(bank, sound, layer) {
    const key = `${bank.name}/${sound}/${layer.index}`;
    let b = buffers.get(key);
    if (!b) {
      const t0 = performance.now();
      const d = decoded.get(key) ?? decodeBankPatch(bank.bnk, layer.patch);
      b = toAudioBuffer(engine.context, layer.patch.codec === 5 ? spuResample(d) : d); // hardware voices: SPU Gaussian interpolation
      buffers.set(key, b); decoded.delete(key);
      if (!warming) noteDecode(performance.now() - t0, `sfx:${bank.name}/${sound}`); // (the loading screen's warm-up decodes in slices on purpose)
    }
    return b;
  }
  // A looped 100 Hz step table as an audio-rate signal (values mapped by `map`), for AudioParam modulation.
  function lfoBuffer(key, table, map) {
    let b = lfoBuffers.get(key);
    if (!b) {
      const rate = 8000, step = rate / 100; b = engine.context.createBuffer(1, table.length * step, rate); // 100 Hz steps (3B55F0 tick)
      const d = b.getChannelData(0); for (let i = 0; i < table.length; i++) { const v = map(table[i]); d.fill(Number.isFinite(v) ? v : 0, i * step, (i + 1) * step); } // never NaN into Safari's mix
      lfoBuffers.set(key, b);
    }
    return b;
  }

  // Distance gain and azimuth for a source position (cm, Z up) from the listener (2ABC18 / 285930).
  function spatial(position, vanish = 30) {
    if (!position || !listener) return { g: 1, az: 0 };
    const m = listener.inv, o = listener.origin, x = position[0] / 100 - o[0], y = position[2] / 100 - o[1], z = -position[1] / 100 - o[2]; // scene metres
    const vx = m[0] * x + m[4] * y + m[8] * z + m[12], vy = m[1] * x + m[5] * y + m[9] * z + m[13], vz = m[2] * x + m[6] * y + m[10] * z + m[14];
    // PS2 view space: +x right, +z ahead (three.js looks down -z), listener 1 m ahead and 25 cm off vertically.
    const px = vx, py = -vy + 0.25, pz = -vz - 1;
    const d = Math.sqrt(px * px + py * py + pz * pz), k = Math.max(d - 0.5, 0);
    const g = k >= vanish ? 0 : ((vanish - k) / vanish) ** 2; // 2ABC18 (mVanish, default 30 m)
    return { g, az: Math.atan2(px, pz), d };
  }
  const panValue = (patchPanRad, az) => {
    if (mono) return 0;
    let a = patchPanRad + az; a = Math.atan2(Math.sin(a), Math.cos(a));
    if (a > Math.PI / 2) a = Math.PI - a; else if (a < -Math.PI / 2) a = -Math.PI - a;
    return a / (Math.PI / 2);
  };

  // 2906B8 + 3BB588: start a voice (all layers). Returns a handle or null (bank not loaded / audio locked).
  // note/velocity: MIDI-driven voices (crowd .eam, 3B8F78): cents += (note - root) x 100, patch volume x velocity / 127.
  // wheel: bend-wheel byte (64 = centre; tag 0x0A range, 3BC6B8). positional voices get the per-frame doppler bend
  // 2AC868 writes after the callback (ratio 1.0: it overwrites a callback bend that differs by more than 0xFF).
  // Hardware voice pools (3BA0B0, stereo config): SPU 48 (PS-ADPCM), IOP-mixed 8 (MicroTalk, EA-XA, PCM8); one voice
  // per channel of every layer. A free voice is taken first; otherwise the lowest-priority voice (tag 0x06) with
  // priority <= min(new, 100) is stolen, the oldest on ties (priority > 100 is never stolen); a stolen voice stops at
  // once. If a layer cannot get its voices, the layers already started are stopped and the request fails.
  const POOL = { spu: 48, iop: 8 }, used = { spu: [], iop: [] };
  let serial = 0;
  const poolOf = (patch) => (patch.codec === 5 ? 'spu' : 'iop');
  function allocate(pool, n, priority) {
    const list = used[pool], cap = POOL[pool];
    const stealable = () => list.filter((h) => !h.freed && h.priority <= Math.min(priority, 100)).sort((a, b) => a.priority - b.priority || a.serial - b.serial);
    if (list.length + n > cap) {
      const victims = stealable();
      while (list.length + n > cap && victims.length) {
        const v = victims.shift(); const owner = v.layer; audioStats.sfxStolen++;
        for (const h of owner.hw) release(h);
        try { owner.source.stop(); } catch {}
        owner.done = true;
        if (owner.voice.layers.every((l) => l.done)) finish(owner.voice);
      }
      if (list.length + n > cap) { audioStats.sfxDropped++; return null; }
    }
    const got = []; for (let i = 0; i < n; i++) { const h = { pool, priority, serial: ++serial, freed: false }; list.push(h); got.push(h); }
    return got;
  }
  function release(h) { if (h.freed) return; h.freed = true; const list = used[h.pool], i = list.indexOf(h); if (i >= 0) list.splice(i, 1); }

  // 2ADCA0 delayed request: the voice is requested (and its hardware voices allocated) when the timer fires.
  function play(opts) {
    if (!(opts.delayMs > 0)) return startVoice(opts);
    let real = null, cancelled = false, base = opts.volume ?? 127;
    const handle = { deferred: true, setVolume: (v) => { base = v; real?.setVolume(v); }, setBend: (b) => real?.setBend(b), setWheel: (b) => real?.setWheel(b),
      stop: (sec) => { cancelled = true; real?.stop(sec); }, playing: () => !cancelled && (!real || real.playing()) };
    setTimeout(() => { if (!cancelled) { real = startVoice({ ...opts, delayMs: 0, volume: base }); if (!real) cancelled = true; } }, opts.delayMs);
    return handle;
  }
  function startVoice({ speaker = -1, slot, sound, bus = 'UI', volume = 127, bend = 0x1000, position = null, posStatic = false, callback = null, tag = null, owner = null,
    note = 60, velocity = 127, wheel = 64, vanish = 30 }) {
    const context = ctx(); const bank = bankOf(slot);
    if (!context && engine.unlocked && engine.live === false) audioStats.sfxGated++; // (pv audioInterrupt: the context is stopped)
    if (!context || !bank?.bnk) return null;
    const layers = layersOf(bank, sound); if (!layers) return null;
    const pre = startAfterDecode ? new Map(layers.map((l) => { try { return [l, bufferFor(bank, sound, l)]; } catch (e) { console.warn(`Sound ${bank.name}/${sound} failed`, e); return [l, null]; } })) : null;
    const when = context.currentTime;
    const out = context.createGain(); // gameVol / 127 (per-frame, 2AC868)
    const voice = { slot, sound, bus, tag, owner, position: position && posStatic ? [...position] : position, posRef: posStatic ? null : position,
      base: volume, bend, callback, ended: false, fading: false, sent: -1, out, layers: [], started: when, priority: 0, wheel, paused: false, vanish };
    out.connect(engine.bus(bus, speaker)); // CHARACTER: per-speaker gain (287968 bus 4)
    for (const layer of layers) {
      const { patch, tags } = layer; const T = (t, d) => tags.get(t)?.v ?? d;
      let buffer; if (pre) { buffer = pre.get(layer); if (!buffer) continue; } else try { buffer = bufferFor(bank, sound, layer); } catch (e) { console.warn(`Sound ${bank.name}/${sound} failed`, e); continue; }
      const hw = allocate(poolOf(patch), patch.channels, T(0x06, 0));
      if (!hw) { // 3BB588: a layer without voices stops the layers already started; the request fails
        for (const l of voice.layers) { for (const h of l.hw) release(h); try { l.source.stop(); } catch {} l.done = true; }
        voice.layers.length = 0; break;
      }
      const source = context.createBufferSource(); source.buffer = buffer;
      if (patch.loopStart >= 0 && patch.loopEnd > patch.loopStart) { source.loop = true; source.loopStart = patch.loopStart / patch.sampleRate; source.loopEnd = (patch.loopEnd + 1) / patch.sampleRate; }
      const pv = Math.trunc(Math.min(Math.max(T(0x0e, 127) + rndRange(T(0x0f, 0), random), 0), 127) * velocity / 127);
      const cents = T(0x10, 0) + (note - T(0x07, 60)) * 100 + rndRange(T(0x11, 0), random);
      const wheelRange = T(0x0a, 0) * 100;
      const panIndex = Math.min(Math.max(T(0x0c, 64) + rndRange(T(0x0d, 0), random), 0), 127);
      const patchPan = Math.sin((panIndex - 64) / 64 * Math.PI / 2) * Math.PI / 2;
      source.detune.value = cents + ((wheel - 64) * wheelRange >> 6); source.playbackRate.value = Math.max(bend, 0) / 0x1000;
      const g = context.createGain(); g.gain.value = pv / 127; // patch volume
      const env = patchEnvelope(bank.bnk, tags);
      const eg = context.createGain(); let stopAt = null;
      if (env) {
        const p = eg.gain; let t = when, level = env.start / 127; p.setValueAtTime(level, when);
        for (const s of env.segments) { if (s.ticks <= 0 || s.ticks >= 0x7fffffff) { if (s.ticks <= 0) { level = s.level / 127; p.setValueAtTime(level, t); continue; } break; } t += s.ticks / 100; level = s.level / 127; p.linearRampToValueAtTime(level, t); }
        if (env.segments.every((s) => s.ticks < 0x7fffffff)) stopAt = t; // 3B55F0: hard stop after the last segment
      }
      let node = source; node.connect(g); node = g; g.connect(eg); node = eg;
      const volTable = patchTable(bank.bnk, tags, 0x1d, 0x1e);
      if (volTable) { // volume LFO: gain x tbl/127, 10 ms per step from index 0
        const lg = context.createGain(); lg.gain.value = 0;
        const ls = context.createBufferSource(); ls.buffer = lfoBuffer(`${bank.name}/${sound}/${layer.index}/v`, volTable, (v) => v / 127); ls.loop = true;
        ls.connect(lg.gain); node.connect(lg); node = lg; ls.start(when); layer.lfo = [ls];
      }
      const pitchTable = patchTable(bank.bnk, tags, 0x20, 0x21);
      if (pitchTable) { // pitch LFO: (tbl - 64) x depth >> 6 cents, random start when tag 0x23 != 0
        const depth = T(0x22, 0), ps = context.createBufferSource();
        ps.buffer = lfoBuffer(`${bank.name}/${sound}/${layer.index}/p/${depth}`, pitchTable, (v) => ((v - 64) * depth) >> 6); ps.loop = true;
        const offset = T(0x23, 0) ? Math.floor(random() * pitchTable.length) / 100 : 0;
        ps.connect(source.detune); ps.start(when, offset); (layer.lfo ??= []).push(ps);
      }
      const pan = context.createStereoPanner ? context.createStereoPanner() : null;
      if (pan) { node.connect(pan); pan.connect(out); } else node.connect(out);
      voice.priority = Math.max(voice.priority, T(0x06, 0));
      const entry = { source, pan, patchPan, lfo: layer.lfo ?? [], cents, wheelRange, hw, voice, done: false };
      for (const h of hw) h.layer = entry;
      voice.layers.push(entry);
      source.start(when); if (stopAt != null) try { source.stop(stopAt); } catch {}
      source.onended = () => { entry.done = true; for (const h of hw) release(h); if (voice.layers.every((l) => l.done)) finish(voice); };
    }
    if (!voice.layers.length) { out.disconnect(); return null; }
    if (playLog) { playLog.push([+context.currentTime.toFixed(2), slot, sound, bus, volume, position ? +spatial(position, vanish).g.toFixed(2) : 1, voice.layers.length]); if (playLog.length > 400) playLog.shift(); }
    voice.setVolume = (v) => { voice.base = v; };
    voice.setBend = (b) => { // 2ABE78: clamped 0..0x3FFF, changes <= 0xFF ignored
      b = Math.min(Math.max(b | 0, 0), 0x3fff); if (Math.abs(b - voice.bend) <= 0xff) return; voice.bend = b;
      for (const l of voice.layers) l.source.playbackRate.setValueAtTime(b / 0x1000, context.currentTime);
    };
    voice.stop = (seconds = 0.25) => fadeOut(voice, seconds <= 0 ? 0.25 : seconds);
    // 3BC6B8: bend-wheel byte on every layer (cents += (b - 64) x range >> 6; no effect without tag 0x0A).
    voice.setWheel = (b) => {
      b = Math.min(Math.max(b | 0, 0), 127); if (b === voice.wheel) return; voice.wheel = b;
      for (const l of voice.layers) if (l.wheelRange) l.source.detune.setValueAtTime(l.cents + ((b - 64) * l.wheelRange >> 6), context.currentTime);
    };
    voice.playing = () => !voice.ended && !voice.fading;
    update(voice, true);
    voices.add(voice);
    return voice;
  }
  function finish(voice) { if (voice.ended) return; voice.ended = true; voices.delete(voice); for (const l of voice.layers) for (const h of l.hw ?? []) release(h); for (const l of voice.layers) for (const s of l.lfo) try { s.stop(); } catch {} setTimeout(() => { try { voice.out.disconnect(); } catch {} }, 50); }
  // 2AD5F0 / 2ABB38: linear ramp of the voice volume to 0 over `seconds` (default stop 250 ms), then stop.
  function fadeOut(voice, seconds) {
    if (voice.ended || voice.fading) return; voice.fading = true;
    const t = engine.context.currentTime, p = voice.out.gain; p.cancelScheduledValues(t); p.setValueAtTime(p.value, t); p.linearRampToValueAtTime(0, t + seconds);
    for (const l of voice.layers) try { l.source.stop(t + seconds + 0.01); } catch {}
    setTimeout(() => finish(voice), (seconds + 0.05) * 1000);
  }
  // 2AC868: per-frame gain/pan of one voice.
  function update(voice, immediate = false) {
    const context = engine.context; if (!context || voice.fading) return;
    const pos = voice.posRef ?? voice.position;
    const s = pos ? spatial(pos, voice.vanish) : { g: 1, az: 0 };
    const vol = Math.min(Math.max(trunc(Math.min(Math.max(voice.base, 0), 127) * s.g), 0), 127);
    if (immediate || Math.abs(vol - voice.sent) > 4) { // 2ABF60: changes of 4 or less are not sent
      voice.sent = vol;
      if (immediate) voice.out.gain.setValueAtTime(vol / 127, voice.started); else voice.out.gain.setTargetAtTime(vol / 127, context.currentTime, 0.004);
    }
    for (const l of voice.layers) if (l.pan) { const v = panValue(l.patchPan, pos ? s.az : 0); if (immediate) l.pan.pan.value = v; else l.pan.pan.setTargetAtTime(v, context.currentTime, 0.004); }
  }
  // Per frame (285BF8 -> 2AB958): callbacks first, then the 3D/volume update.
  function tick(dt) {
    for (const v of [...voices]) {
      if (v.ended || v.paused) continue;
      if (v.callback && dt > 0) v.callback(v, dt);
      if (v.posRef ?? v.position) v.setBend(0x1000); // 2AC868: doppler ratio 1.0 after the callback (positional voices)
      update(v);
    }
  }
  // 29CE28 -> 2AD2A8 -> 2ACAC8: every active, non-fading game voice gets bend 0 (pitch 0) and volume 0 while the
  // pause menu is up; resume restores the bend and the next update resends the volume.
  function pauseAll(on, filter = () => true) {
    const t = engine.context?.currentTime ?? 0;
    for (const v of voices) if (filter(v) && !v.fading && v.paused !== on) {
      v.paused = on;
      for (const l of v.layers) l.source.playbackRate.setValueAtTime(on ? 0 : v.bend / 0x1000, t);
      const g = v.out.gain; g.cancelScheduledValues(t); g.setValueAtTime(g.value, t);
      if (on) g.setTargetAtTime(0, t, 0.002); else { v.sent = -1; update(v, false); }
    }
  }
  function stopAll({ slot = null, tag = null, owner = null, fade = 0.25 } = {}) {
    for (const v of [...voices]) if ((slot == null || v.slot === slot) && (tag == null || v.tag === tag) && (owner == null || v.owner === owner)) fadeOut(v, fade);
  }
  return {
    SLOT, loadBank, unloadBank, bankOf, play, tick, stopAll, pauseAll, warm,
    // QA (pv bankEvict): what the caches hold, by bank
    cacheStats() { const by = {}; const add = (k, n) => { const b = k.slice(0, k.indexOf('/')); by[b] = (by[b] ?? 0) + n; };
      for (const [k, d] of decoded) add(k, (d?.data ?? []).reduce((a, c) => a + (c?.byteLength ?? 0), 0));
      for (const [k, b] of buffers) add(k, (b?.length ?? 0) * (b?.numberOfChannels ?? 1) * 4);
      return { decoded: decoded.size, buffers: buffers.size, mb: Math.round(Object.values(by).reduce((a, c) => a + c, 0) / 1048576), by, evicted: evicted.slice() }; },
    // Camera for the listener (three.js camera with an up-to-date matrixWorldInverse), or null (2D only).
    // camera: three.js camera (scene space = world metres - origin), origin: THREE.Vector3 or [x, y, z] metres.
    setListener(camera, origin = null) {
      if (!camera) { listener = null; return; }
      const o = origin ? (Array.isArray(origin) ? origin : [origin.x, origin.y, origin.z]) : [0, 0, 0];
      listener = { inv: camera.matrixWorldInverse.elements.slice(), origin: o };
    },
    setMono(on) { mono = !!on; }, // audio+0x62B0 output mode (Sound Mode: mono -> azimuth 0)
    get voiceCount() { return voices.size; },
    logPlays(on = true) { playLog = on ? [] : null; }, get plays() { return playLog?.slice() ?? []; },
    get pools() { return { spu: used.spu.length, iop: used.iop.length }; },
    byTag() { const m = {}; for (const v of voices) { const k = `${v.tag ?? v.bus}:${v.slot}/${v.sound}`; m[k] = (m[k] ?? 0) + 1; } return m; },
    voices() { return [...voices]; },
    spatial,
  };
}
