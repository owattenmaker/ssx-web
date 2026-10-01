// Crowd audio (docs/audio-logic.md 5.4): the CROWD.INF reactions (EA "MIDx" .eam sequences on Crowd.bnk, bank
// slot 5) and the ambient crowd loops fed by world emitters (watrig ids 41-43 = Crowd.bnk entries 0-2), per listener.
// Addresses: instance ctor 2A4D68, per-frame 2A4E88, reactions 2A64A0 (trick) / 2A6648 (fall), anticipation
// 2A67F0 / 2A6808 / 290FD0, stop 2A6848, loop bend 2A68B0, emitters 2A7678 / 2A7340 / 2A72D8, MIDI 3B9A00.

// ---- .eam (MIDx): [wait][status][d1]([d2]); wait = ticks after the record; 1 tick = 10 ms (3B96B8 at 100 Hz) ----
export function parseMidx(buf) {
  const b = buf instanceof Uint8Array ? buf : new Uint8Array(buf);
  if (b[0] !== 0x4d || b[1] !== 0x49 || b[2] !== 0x44 || b[3] !== 0x78) throw new Error('not a MIDx file');
  const events = []; let p = 4, tick = 0;
  while (p + 3 <= b.length) {
    const wait = b[p], status = b[p + 1], d1 = b[p + 2]; p += 3;
    const hi = status & 0xf0, channel = status & 0x0f, e = { tick, channel };
    if (hi === 0x80 || hi === 0x90 || hi === 0xb0 || hi === 0xe0) {
      const d2 = b[p++];
      if (hi === 0x80) Object.assign(e, { type: 'noteOff', note: d1 });
      else if (hi === 0x90) Object.assign(e, { type: 'noteOn', note: d1, velocity: d2 });
      else if (hi === 0xb0) Object.assign(e, { type: 'control', controller: d1, value: d2 });
      else Object.assign(e, { type: 'pitchBend', value: ((d2 << 7) | d1) >> 7 });
    } else if (hi === 0xc0) Object.assign(e, { type: 'program', program: d1 });
    else if (hi === 0xf0) { if (d1 === 0x2f) { e.type = 'end'; events.push(e); break; } e.type = 'nop'; }
    else e.type = 'unknown';
    events.push(e); tick += wait;
  }
  return events;
}

// CROWD.INF [GLOBAL] (2A6D78 / 2A7040): TRICK1-3, FALL1-3, ANTICIPATE1-3 (.eam lists), ATTACK/DECAY/MAXBEND 1-3.
export function crowdConfig(inf) {
  const g = inf.find((s) => s.name === 'GLOBAL') ?? {};
  const list = (k) => [g[k]].flat().filter(Boolean).map((p) => p.split('\\').pop());
  return [1, 2, 3].map((d) => ({ trick: list(`TRICK${d}`), fall: list(`FALL${d}`), anticipate: list(`ANTICIPATE${d}`),
    attack: g[`ATTACK${d}`] ?? 1, decay: g[`DECAY${d}`] ?? 4, maxBend: g[`MAXBEND${d}`] ?? 32 }));
}

export function createCrowd({ sfx, bytes, config, random = Math.random, bank = 5 }) {
  const sequences = new Map(); // file -> promise events
  const seq = (file) => { if (!sequences.has(file)) sequences.set(file, bytes(`banks/${file}`).then(parseMidx)); return sequences.get(file); };
  const pick = (list) => list[Math.floor(random() * list.length) % list.length]; // 2A4CA0
  // Instance (0x190 bytes): emitters {pos, weight, group}, groups by patch {patch, voice, refs}, reaction.
  const inst = { emitters: new Map(), groups: new Map(), loudness: 0, centroid: [0, 0, 0], reaction: null, anticipation: 0, level: 0, T: 0 };

  // 2A6848: stop the reaction, clamp the swell time to 0.25 s, anticipation off.
  function stopReaction() {
    inst.T = Math.min(inst.T, 0.25); inst.anticipation = 0;
    const r = inst.reaction; inst.reaction = null;
    if (r) { r.cancelled = true; for (const v of r.voices) v.stop(0.25); }
  }
  // 2A8450 + 2A86B8 + 3B9A00: play a sequence on Crowd.bnk at the centroid, volume = loudness, bus 5.
  function playSequence(file) {
    const r = { voices: [], cancelled: false, done: false, started: performance.now() };
    const volume = inst.loudness, position = [...inst.centroid];
    seq(file).then((events) => {
      if (r.cancelled) return;
      const chan = Array.from({ length: 16 }, () => ({ program: 0, volume: 127 }));
      let last = 0;
      for (const e of events) {
        const c = chan[e.channel];
        if (e.type === 'program') c.program = e.program;
        else if (e.type === 'control' && e.controller === 7) c.volume = e.value & 0x7f;
        else if (e.type === 'noteOn') {
          const vol = (c.volume * volume) >> 7; // CC7 x sequence volume >> 7
          const v = sfx.play({ slot: bank, sound: c.program, bus: 'UI', volume: vol, position, posStatic: true, note: e.note, velocity: e.velocity, delayMs: e.tick * 10 });
          if (v) r.voices.push(v);
        }
        last = Math.max(last, e.tick);
      }
      r.lengthMs = last * 10; // the sequence (and its handle) lives until the note-offs (~20 s)
    });
    return r;
  }
  function reactionPlaying() {
    const r = inst.reaction; if (!r) return false;
    if (r.lengthMs == null) return true;
    return performance.now() - r.started < r.lengthMs;
  }
  // 2A64A0 / 2A6648: trick / fall reaction of level L (0..2).
  function react(kind, L) {
    stopReaction();
    if (!inst.emitters.size) return;
    const rec = config[L]; inst.level = L; inst.T = rec.attack + rec.decay;
    const list = kind === 'trick' ? rec.trick : rec.fall; if (!list.length) return;
    inst.reaction = playSequence(pick(list));
  }

  // 2A5D08 / 2A5CD8 / 2A62F0: emitter registration (world sound emitters resolving to bank 5).
  function emitter(key, { patch, position, volume }) {
    let e = inst.emitters.get(key);
    if (!e) {
      e = { patch, position, weight: 0 }; inst.emitters.set(key, e);
      let g = inst.groups.get(patch); if (!g) { g = { patch, voice: null, refs: 0, position: [...position], weight: 0 }; inst.groups.set(patch, g); }
      g.refs++;
    }
    e.position = position; e.weight = Math.min(Math.max(Math.trunc(volume * 127), 0), 127);
  }
  function removeEmitter(key) {
    const e = inst.emitters.get(key); if (!e) return;
    inst.emitters.delete(key);
    const g = inst.groups.get(e.patch);
    if (g && --g.refs <= 0) { g.voice?.stop(0.25); inst.groups.delete(e.patch); }
    if (!inst.emitters.size) { const was = inst.anticipation === 2; stopReaction(); if (was) inst.anticipation = 1; }
  }
  // Loudness (2A4E88): max weight + sum of the others >> 2; centroid = weight-averaged position.
  function blend(list) {
    let max = 0, sum = 0, wsum = 0; const c = [0, 0, 0];
    for (const e of list) { max = Math.max(max, e.weight); sum += e.weight; wsum += e.weight; for (let k = 0; k < 3; k++) c[k] += e.position[k] * e.weight; }
    const loud = max + ((sum - max) >> 2);
    return { loud: Math.min(loud, 127), centroid: wsum ? c.map((x) => x / wsum) : (list[0]?.position ?? [0, 0, 0]) };
  }
  // 2A68B0: per loop-voice callback, the pitch swell after a trick / fall (T shared, decremented per voice).
  function bendCallback(voice, dt) {
    if (inst.T <= 0) { voice.setWheel(64); return; }
    inst.T -= dt;
    const rec = config[inst.level];
    let bend = 0;
    if (inst.T > rec.decay) bend = rec.maxBend * (rec.attack - (inst.T - rec.decay)) / rec.attack;
    else if (inst.T > 0) bend = rec.maxBend * inst.T / rec.decay;
    voice.setWheel(0x40 + Math.trunc(bend));
  }

  return {
    emitter, removeEmitter,
    trick(L) { react('trick', L); },   // 2A73A8
    fall(L) { react('fall', L); },     // 2A73D8
    arm() { if (inst.anticipation === 0) inst.anticipation = 1; },            // 2A7408
    cancel() { if (inst.anticipation === 2) stopReaction(); inst.anticipation = 0; }, // 2A7438
    // Per frame (2A4E88). level(): 290FD0 (0..2, 3 = none).
    update(anticipationLevel) {
      if (!inst.emitters.size) return;
      const all = blend([...inst.emitters.values()]);
      inst.loudness = all.loud; inst.centroid = all.centroid;
      const groups = [...inst.groups.values()];
      for (const g of groups) {
        const b = groups.length === 1 ? all : blend([...inst.emitters.values()].filter((e) => e.patch === g.patch));
        g.position = b.centroid; g.weight = b.loud;
        if (!g.voice || !g.voice.playing()) g.voice = sfx.play({ slot: bank, sound: g.patch, bus: 'UI', volume: b.loud, position: g.position, callback: bendCallback, tag: 'crowd-loop' });
        else g.voice.setVolume(b.loud);
      }
      if (inst.anticipation !== 0) {
        const L = anticipationLevel();
        if (inst.anticipation === 1 && L !== 3) {
          stopReaction();
          const list = config[L].anticipate;
          if (list.length) { inst.reaction = playSequence(pick(list)); inst.anticipation = 2; }
        } else if (inst.anticipation === 2) {
          if (reactionPlaying()) for (const v of inst.reaction?.voices ?? []) v.setVolume(inst.loudness);
          else stopReaction();
        }
      }
    },
    reset() { stopReaction(); for (const g of inst.groups.values()) g.voice?.stop(0.25); inst.groups.clear(); inst.emitters.clear(); inst.T = 0; },
    debug() { return { emitters: inst.emitters.size, groups: inst.groups.size, loudness: inst.loudness, anticipation: inst.anticipation, reaction: !!inst.reaction, T: +inst.T.toFixed(2) }; },
  };
}
