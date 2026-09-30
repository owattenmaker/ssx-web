// World sounds of the resident course locations (docs/audio-logic.md 5.5; data: tools/export_world_audio.py):
//   emitters      2306A8/244880 -> 2B7848 -> 2B7908: per listener, every instance sound emitter in range gets a
//                 WorldTrigger instance (2B5C68) at volume = falloff x 127, bus 5, positional at its centre;
//                 2B5D78 stops untouched ones with a 0.25 s fade. watrig type 1 = {bank slot, sound}; bank 5 feeds
//                 the crowd (web/audio-crowd.js); type 3 = named bank in slot 3 (one at a time, primary listener,
//                 first claim wins, 29B818); type 4 id 77 = PA rider position (2B6550 -> 2A34D0).
//   contacts      105398 -> 296088: the instance's contact id for the node -> watrig type 1, bus 7 (COLLISION),
//                 volume curve 290C10(0, |v|), positional at the rider, 100 m gate, repeat FIFO per (instance, node).
//   ambience      29D290: the focus rider's contact track changes -> location id < 22: bank 9 sound 0 (2D, bus 5,
//                 29D370; restarted when it ends, 29D610); a connector fades it and every bank 8/9 voice over 5.03 s.
//   script sounds stage builtins 30/31/73 (2974A0 / 297950 / 297EB8): id 1-99 Mtn.bnk, 100-149 slot 8, 150-199
//                 slot 9, >= 200 TRANSPORT; bus 5, positional at the instance, distance parameter 300 (2A9988).
//   thunder       291438: bank 8 sound 16 after a lightning flash, delayed by distance / 332 m/s (no weather in the port).
//   avalanche     0x29DEF0 (pv avalanche, docs/avalanche.md "Audio"): the rumble loop, bank 8 sound 2, bus 5, positional at the
//                 tumblers' centroid (vanish 100 m, 2A9988(100, -1)), volume 0x29E438 per update (0x29E4A0), 2 s fade out.
import { SLOT, CURVES, curve } from './sfx.js';
import { avalancheRumble } from './avalanche-state.js';

// 2B82B8 falloff shapes (jump table 0x4835E0).
export function falloff(shape, t) {
  switch (shape) {
    case 0: return 1 - t * t;
    case 1: return 1 - t / (1.5 - 0.5 * t);
    case 2: return 1 - t;
    case 3: { const u = 1 - t; return u / (1.5 - 0.5 * u); }
    case 4: return (1 - t) * (1 - t);
    case 5: return t <= 0.7 ? 1 : (1 - t) * 3.3333;
    default: return 0;
  }
}
const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const len = (a) => Math.sqrt(dot(a, a));
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const norm = (a) => { const l = len(a) || 1; return [a[0] / l, a[1] / l, a[2] / l]; };
// 2B7908: emitter volume (0..1) at listener L, or 0 outside.
export function emitterVolume(e, L) {
  const d = sub(L, e.pos);
  if (e.shape === 'sphere') { const r = len(d); return r < e.radius ? Math.max(falloff(e.falloff, r / e.radius), 0) : 0; }
  if (e.shape === 'ellipsoid') {
    const D = norm(e.axis), S = norm(cross([0, 0, 1], D)), U = cross(D, S);
    let t = 0; const ex = e.extents;
    if (ex[0]) t += (dot(d, D) / ex[0]) ** 2; if (ex[1]) t += (dot(d, S) / ex[1]) ** 2; if (ex[2]) t += (dot(d, U) / ex[2]) ** 2;
    return t < 1 ? Math.max(falloff(e.falloff, t), 0) : 0;
  }
  if (e.shape === 'cone') {
    const r = len(d); if (!(r < e.radius)) return 0;
    const n = r ? [d[0] / r, d[1] / r, d[2] / r] : [0, 0, 0], c = dot(n, e.dir);
    if (!(c > e.cosHalfAngle)) return 0;
    const ang = (Math.PI / 2 - Math.asin(c)) / (Math.PI / 2 - Math.asin(e.cosHalfAngle));
    return Math.max(falloff(e.falloff, r / e.radius) * falloff(e.angleFalloff, ang), 0);
  }
  if (e.shape === 'zone') { const r = len(d); return r < e.radius ? (r <= 0.9 ? 1 : (1 - r) * 10) : 0; } // raw distance (as the PS2)
  return 0;
}
// 2974A0 id routing.
export function scriptSoundRoute(id) {
  if (id >= 1 && id < 100) return { slot: SLOT.MOUNTAIN, sound: id };
  if (id >= 100 && id < 150) return { slot: SLOT.WORLD8, sound: id - 100 };
  if (id >= 150 && id < 200) return { slot: SLOT.WORLD9, sound: id - 150 };
  if (id >= 200) return { slot: SLOT.TRANSPORT, sound: id - 200 };
  return null;
}

export function createWorldAudio({ sfx, crowd, json, onSpecial = null }) {
  let doc = null, token = 0;
  const instances = new Map();       // emitter key -> {voice, adl, entry, named}
  let named = { name: null, owner: null, ready: false, loading: null };
  let track = -1, ambience = null;   // 29D290 last track (audio+0x5FC0) and handle (audio+0x5FC8)
  const contactVoices = new Map();   // `${resource}:${node}` -> voice (295028 repeat FIFO)
  const scriptLoops = new Map();     // instance -> {id, voice} (audio+0x5A00 table)
  const scriptVoices = new Map();    // instance -> one-shot voice (295028 repeat FIFO, tag 0)
  // (a voice that no longer plays blocks nothing: its entry goes, or every instance ever touched kept its ended voice for the session)
  const sweep = (m) => { for (const [k, v] of m) if (!v.playing()) m.delete(k); };
  let rumble = null;                 // 0x29DEF0 voice (audio+0x5FF4) and its position (audio+0x6050)
  const locationByTrack = new Map();
  // Streamed peak worlds (world/PEAK<n>.json, docs/audio-logic.md "Free ride / Peak 1"): only the locations whose
  // streaming row holds their data (states 1, 2, 5, 7) exist; null = every location of the document (race events).
  let resident = null, bankOwner = null, ambienceWanted = false;
  const isResident = (loc) => !resident || resident.has(loc.track);

  let loadedEvent = null, loading = null;
  async function load(event) {
    if (event === loadedEvent) return loading; // same event (loading screen prep, then world load)
    loadedEvent = event;
    return (loading = loadNow(event));
  }
  async function loadNow(event) {
    const my = ++token;
    doc = null; stopAll(0.25); locationByTrack.clear(); track = -1; bankOwner = null; ambienceWanted = false;
    const d = await json(`world/${event}.json`).catch((e) => { console.warn(`World audio ${event} unavailable`, e); return null; });
    if (my !== token || !d) return;
    doc = d;
    for (const [name, loc] of Object.entries(d.locations)) {
      locationByTrack.set(loc.track, { name, ...loc });
      if (isResident(loc)) loadBanks(name, loc);
    }
  }
  // 286CA8: a location's kind-20 banks go to slots 8 / 9 when its data arrives (one course / hub per streaming row).
  function loadBanks(name, loc) {
    if (!loc.banks.slot8 && !loc.banks.slot9) return;
    bankOwner = name;
    if (loc.banks.slot8) sfx.loadBank(SLOT.WORLD8, loc.banks.slot8.replace(/\.bnk$/, ''));
    if (loc.banks.slot9) sfx.loadBank(SLOT.WORLD9, loc.banks.slot9.replace(/\.bnk$/, '')).then(() => { if (ambienceWanted && !ambience && bankOwner === name) api.ambienceStart(); });
  }
  function unload() { loadedEvent = null; loading = null; doc = null; resident = null; bankOwner = null; ++token; stopAll(0); }
  function stopAll(fade = 0.25) {
    for (const [key, i] of instances) { if (i.crowd) crowd.removeEmitter(key); else i.voice?.stop(fade); }
    instances.clear();
    for (const l of scriptLoops.values()) l.voice?.stop(0);
    scriptLoops.clear(); ambience?.stop(fade); ambience = null;
    rumble?.voice?.stop(0); rumble = null;
    named = { name: null, owner: null, ready: false, loading: null };
  }
  // 29B818: named bank into slot 3 (one at a time), true once loaded.
  function namedBank(name) {
    if (named.name !== name) {
      named = { name, owner: null, ready: false, loading: null };
      const n = named;
      n.loading = sfx.loadBank(SLOT.DYNAMIC, name.replace(/\.bnk$/, '')).then(() => { n.ready = true; });
    }
    return named.ready;
  }

  const api = {
    load, stopAll, unload,
    get loaded() { return !!doc; },
    // Streamed world: the tracks whose location data is in memory (null = all). Newly arrived courses / hubs load
    // their banks; emitters, contacts and painters of the other locations are ignored.
    setResident(tracks) {
      const next = tracks ? new Set(tracks) : null, before = resident;
      resident = next;
      if (doc) for (const [name, loc] of Object.entries(doc.locations)) if ((!next || next.has(loc.track)) && before && !before.has(loc.track)) loadBanks(name, loc);
    },
    locationOf(t) { const l = locationByTrack.get(t); return l && isResident(l) ? l : null; },
    // Per frame: L = listener position (source cm), primary = the primary listener's side.
    update(L) {
      if (!doc || !L) return;
      const touched = new Set();
      for (const [name, loc] of Object.entries(doc.locations)) {
        if (!isResident(loc)) continue;
        loc.emitters.forEach((e, index) => {
          const v = emitterVolume(e, L); if (!(v > 0)) return;
          const key = `${e.resource}:${index}`, w = doc.watrig[e.adl]; if (!w) return;
          touched.add(key);
          let inst = instances.get(key);
          if (!inst) { inst = { adl: e.adl, voice: null, crowd: false, special: false }; instances.set(key, inst); }
          if (w.type === 1 && w.bank === SLOT.CROWD) { inst.crowd = true; crowd.emitter(key, { patch: w.snd, position: e.pos, volume: v }); return; } // 2B5838
          if (w.type === 4) { if (!inst.special) { inst.special = true; onSpecial?.(e.adl); } return; } // 2B6550 (id 77)
          const vol = Math.min(Math.max(Math.trunc(v * 127), 0), 127);
          let slot = w.bank, sound = w.snd;
          if (w.type === 3) { // 2B4C38 named-bank gate: first claim wins while it is in range
            if (named.owner && named.owner !== key && instances.has(named.owner)) return;
            const ready = namedBank(w.bank); named.owner = key;
            if (!ready) return;
            slot = SLOT.DYNAMIC; sound = 0;
          }
          if (inst.voice?.playing()) inst.voice.setVolume(vol); // 2B4C38: running voice, volume refreshed
          else inst.voice = sfx.play({ slot, sound, bus: 'UI', volume: vol, position: e.pos, posStatic: true, tag: 'world' });
        });
      }
      for (const [key, inst] of instances) if (!touched.has(key)) { // 2B5D78 -> 2B5758 / 2A72D8: 0.25 s fade
        if (inst.crowd) crowd.removeEmitter(key); else inst.voice?.stop(0.25);
        if (named.owner === key) named.owner = null;
        instances.delete(key);
      }
      if (ambience && !ambience.playing() && ambience.keep) api.ambienceStart(); // 29D610 keep-alive
    },
    // 29D290: the focus rider's contact track (terrain_contact_info()[0] & 0xFF; -1 keeps the previous one).
    focusTrack(t) {
      if (t < 0 || t === track || !doc) return;
      track = t;
      const loc = locationByTrack.get(t);
      const id = loc?.locationId ?? 99;
      if (id < 22) { ambienceWanted = true; api.ambienceStart(); } // (a streamed location's bank 9 may still be loading)
      else { // 29D678: connector
        ambienceWanted = false;
        ambience?.stop(5.03); if (ambience) ambience.keep = false; ambience = null;
        sfx.stopAll({ slot: SLOT.WORLD8, fade: 5.03 }); sfx.stopAll({ slot: SLOT.WORLD9, fade: 5.03 });
      }
    },
    ambienceStop(fade = 5.03) { ambienceWanted = false; ambience?.stop(fade); if (ambience) ambience.keep = false; ambience = null; }, // 29D678
    ambienceStart() { // 29D370: bank 9 sound 0, 2D, volume 127, bus 5
      if (!sfx.bankOf(SLOT.WORLD9)?.bnk) return;
      const v = sfx.play({ slot: SLOT.WORLD9, sound: 0, bus: 'UI', volume: 127, tag: 'ambience' });
      if (v) { v.keep = true; ambience = v; }
    },
    // 296088: instance contact (resource, node index, |v| cm/s) at the rider position (source cm).
    contact(resource, node, speed, position, inRange = true) {
      if (!doc || !inRange) return;
      let list = null;
      for (const loc of Object.values(doc.locations)) if (isResident(loc) && loc.contacts[resource]) { list = loc.contacts[resource]; break; }
      if (!list?.length) return;
      const id = list[Math.min(Math.max(node | 0, 0), list.length - 1)];
      const w = doc.watrig[id]; if (!id || !w || w.type !== 1) return; // named banks never play as contacts (2B5F60)
      const key = `${resource}:${node}`, prev = contactVoices.get(key);
      if (prev?.playing()) return; // 295028
      const vol = Math.min(Math.max(Math.trunc(curve(CURVES.contact_445898, speed)), 0), 127);
      const v = sfx.play({ slot: w.bank, sound: w.snd, bus: 'COLLISION', volume: vol, position: [...position], posStatic: true, tag: 'contact' });
      if (v) { if (contactVoices.size >= 64) sweep(contactVoices); contactVoices.set(key, v); }
    },
    // Stage-script builtins: kind 0 play (2974A0), 1 loop (297950), 2 stop (297EB8).
    script(kind, id, resource, position, location = 0) {
      const r = scriptSoundRoute(id); if (!r) return;
      if (kind === 0) { // 2974A0: id 102 needs a location record word != 0 (144BC0: never in Snow Jam), repeat FIFO per instance
        if (id === 102 && location === 0) return;
        if (scriptVoices.get(resource)?.playing()) return;
      }
      if (kind === 2) { const l = scriptLoops.get(resource); if (l && l.id === id) { l.voice?.stop(0); scriptLoops.delete(resource); } return; }
      if (kind === 1) { const l = scriptLoops.get(resource); if (l) { l.voice?.stop(0); scriptLoops.delete(resource); } }
      const v = sfx.play({ slot: r.slot, sound: r.sound, bus: 'UI', volume: 127, position, posStatic: true, vanish: 300, tag: 'script' });
      if (kind === 1 && v) scriptLoops.set(resource, { id, voice: v });
      else if (v) { if (scriptVoices.size >= 64) sweep(scriptVoices); scriptVoices.set(resource, v); } // 295628
    },
    // 0x29DEF0 / 0x29E4A0: the avalanche rumble. on = the core's loop count > 0; tumblers = [[x, y, z, scale] (source cm)];
    // L = the listener rider's position (rider +0x110, source cm). Once per tick.
    // events: the refcounts after each 0x29DEF0 call since the last tick (a return to 0 stops the voice even if it rises again).
    avalanche(on, tumblers, L, events = []) {
      if (rumble && events.some((v) => v <= 0)) { rumble.voice?.stop(2.0); rumble = null; } // 0x2AD5F0(queue, handle, 2.0, 1)
      if (!on) { if (rumble) { rumble.voice?.stop(2.0); rumble = null; } return; }
      if (!L) return;
      const { volume, centroid } = avalancheRumble(tumblers, L);
      if (!rumble) {
        const position = centroid ? [...centroid] : [...L];
        const voice = sfx.play({ slot: SLOT.WORLD8, sound: 2, bus: 'UI', volume, position, posStatic: false, vanish: 100, tag: 'avalanche' });
        rumble = { voice, position, volume };
      } else {
        if (centroid) { rumble.position[0] = centroid[0]; rumble.position[1] = centroid[1]; rumble.position[2] = centroid[2]; }
        rumble.voice?.setVolume(volume); rumble.volume = volume;
      }
    },
    // 291438: thunder after a lightning flash at `distanceCm` (bank 8 sound 16, 2D, bus 5).
    thunder(distanceCm) {
      const loc = locationByTrack.get(track); if (!loc?.thunder) return;
      sfx.play({ slot: SLOT.WORLD8, sound: 16, bus: 'UI', volume: 127, delayMs: Math.trunc(distanceCm * 0.0018072) * 1000 / 60 });
    },
    debug() { return { loaded: !!doc, event: loadedEvent, resident: resident ? [...resident] : null, banks: bankOwner, track, emitters: instances.size, named: named.name, ambience: !!ambience?.playing(), loops: scriptLoops.size, rumble: rumble ? { playing: !!rumble.voice?.playing?.(), volume: rumble.volume, position: rumble.position.map(Math.round) } : null }; },
  };
  return api;
}
