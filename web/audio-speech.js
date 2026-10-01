// The one speech voice (SpeechInstance 0) shared by DJ Atomika, the PA announcer, rider speech and arcade speech,
// driven by the original EA speech-event script (speech/Events.evt, web/audio-speech-events.js) exactly like the
// PS2 (docs/audio-logic.md section 4):
//   category function (29FCC8..2A3EB8): bus gate, then 2B1458 admits the request into the game's 10-slot table
//   (ttl 180 frames) and 3D7418 posts the event with its bitmask arguments into the 16-slot pending table;
//   every frame 2B0C78 -> 2AFC88: voice busy -> busy = 1; free (idle or < 500 ms left, 2B0290) after busy ->
//   OnIdle hook (2A43B8, one pending DJ flag) then 3D7EC8 resolves the best pending event (highest priority,
//   newest) into (bank, line) items and flushes older ones; the next frame 3D8040 streams every line back to back
//   (only while the game request is still in its table).
// Lines route by bank name (29EEE0): DJ_ -> DJ, PA_ -> PA, Arcade -> ARCADESPEECH, else CHARACTER; a DJ line ducks
// the mode-0 group to 65 % over 1 s (29F000) and the duck is released 0.5 s after the DJ stream ends (285BF8).
import { parseEvents, hdrFromRegistry, makeBankTable, EaRng, createSpeechState, SpeechScheduler, GameRequestTable } from './audio-speech-events.js';
import { decodeSpeechLine, toAudioBuffer } from './audio-decode.js';
import { audioStats, noteDecode } from './audio-stats.js';   // field counters (web/diagnostics.js)

// Character ids (123128, CHARDB order) and the mask table shared by 29F2B0 (speaker) and 2A1DA0 (subject).
export const CHAR_ID = Object.freeze({ moby: 0, kaori: 1, allegra: 2, mac: 3, zoe: 4, griff: 5, elise: 6, nate: 7, psymon: 8, viggo: 9 });
const CHAR_MASK = [0x4, 0x10, 0x200, 0x2, 0x20, 0x100, 0x1, 0x80, 0x8, 0x40];
export const charId = (name) => CHAR_ID[name] ?? -1; // -1: guest / non-standard (123168 != 0)
export const speakerMask = (id) => (id >= 0 && id < 10 ? CHAR_MASK[id] : 0);      // 29F2B0
export const subjectMask = (id) => (id >= 0 && id < 10 ? CHAR_MASK[id] : 0x1000); // 2A1DA0
// 2A1E20 peak by location (table 0x483010): 1 / 2 / 4.
export function peakMask(loc) { return [0, 1, 5, 8, 11, 14, 17, 18].includes(loc) ? 1 : [2, 3, 6, 9, 12, 15, 19, 20].includes(loc) ? 2 : 4; }
// 2A1BD8(loc, -1): course 0..4 -> bit loc, 5..16 -> bit loc + 1 (bit 5 unused); with a hub gate region the
// (hub location, region) pair picks the event's course (tables 0x482F90 + code).
const HUB_GATES = { 17: { 0xe: 0, 0xf: 5, 0x10: 8 }, 18: { 0xe: 1, 0x11: 11 }, 19: { 0xe: 2, 0x10: 9, 0x11: 12 }, 20: { 0xe: 3, 0xf: 6 }, 21: { 0xe: 4, 0xf: 7, 0x10: 10, 0x11: 13 } };
export function courseBit(loc, region = -1) {
  if (loc >= 17) { const c = HUB_GATES[loc]?.[region]; if (c === undefined) return 0; loc = c; }
  return loc < 0 ? 0 : loc <= 4 ? (1 << loc) : loc <= 16 ? (1 << (loc + 1)) : 0;
}
// 2A1E68 non-MP place mask (table 0x483070) from the result index; MP race places use 0..4 -> 1..0x10, else 0x200.
const PLACE_MASK = [1, 2, 4, 8, 0x10, 0x200, 0x40, 0x80, 0x100, 0x200];
export const placeMask = (i) => PLACE_MASK[i] ?? 0;
// 2A34D0 listener-rider place (table 0x4830F0 on rider+0xEC).
const RIDER_PLACE_MASK = [1, 2, 4, 8, 0x10, 0x200];
const LANG = 1; // 29F198: English (the only language on the USA disc)

// Event ids (Events.evt).
export const EV = Object.freeze({
  POST_OBJECT_COLLISION: 0x2077, HIT_BY_FRIEND: 0x2078, PASSED_BY_FRIEND: 0x207d, AGGRESSION_RESPONSE: 0x207e, PASSED_BY_FOE: 0x207f,
  HEY_PASS_FRIEND: 0x2080, PASS_FOE: 0x2081, HIT_FOE: 0x208a, HIT_FRIEND: 0x208e, HIT_BY_FOE: 0x208f,
  BIG_AIR: [0x2084, 0x2083, 0x2091], TRICK_EASY: [0x2082, 0x209a, 0x209c], TRICK_DIFFICULT: [0x2090, 0x209b, 0x209d], WIPEOUT: [0x2088, 0x20a0, 0x20a1],
  ARCADE_POWER_UPS: 0x20a7, ARCADE_PROMPTS: 0x20a8, ARCADE_BONUS: 0x20a9, WHOOH: 0x20b8, FINISH_LINE_RIDER: 0x20b9, FIRST_SPOKE: 0x20ba,
  PA_VENUE_INTRO: 0x20bb, POST_SELECTION: 0x20bc, CUSTOMIZE: 0x20bd, PA_SPONSOR_INTRO: 0x20c0, PA_RIDER_INTRO: 0x20c1, PA_RIDER_RACE_INTRO: 0x20c2,
  PA_MEDAL_RUN_INTRO: 0x20c3, PA_RIDER_POSITION: 0x20c4, PA_FINISH_LINE: 0x20c5, PA_MEDALS: 0x20c7, HUB_WEATHER: 0x20c8, HUB_MTN_HISTORY: 0x20c9,
  HUB_CHAR_STORIES: 0x20ca, EVENT_INTRO: 0x20cc, ARTIST_INTRO: 0x20cf, BC_CHALLENGE: 0x20d3, HEY: 0x20d4, RADIO_BIG_INTRO: 0x20e5,
  RADIO_BIG_OUTRO: 0x20e6, HUB_GOING_ONS: 0x20e7, HUB_LOCAL_STORIES: 0x20e8, HUB_TERRAIN_INFO: 0x20e9, HUB_CHAR_PROGRESS: 0x2102,
  PEAK_BOSS: 0x210b, BC_INTRO: 0x212b, TEXT_MESSAGE: 0x212c, FREE_RIDE_INTRO: 0x212d, AGGRESSION: 0x212e, HIGH_TRICK_SCORE: 0x212f,
  ARCADE_UBER: 0x2133, ARCADE_ICONS: 0x2145,
});
const SPEAKER = Object.freeze({ DJ: 0xa, PA: 0xb, ARCADE: 0xc });
// 29EEE0: bank name -> bus.
// Front-end cue from a web/rider-speech.js bank name: Post_Selection_<abbr> -> 2A16B0 (0x20BC), Customize_<abbr> ->
// 2A1778 (0x20BD), for the CHARDB id of <abbr>. The request is the event, not the bank: Events.evt picks the bank from
// the speaker mask (so Nate, mask 0x80, speaks the _vig bank and Viggo the _nat bank, as on the PS2).
const FE_ABBR = { ari: 'allegra', eli: 'elise', grf: 'griff', kao: 'kaori', mac: 'mac', mob: 'moby', nat: 'nate', psy: 'psymon', vig: 'viggo', zoe: 'zoe' };
export function frontEndCue(bank) {
  const m = /^(Post_Selection|Customize)_(\w+)$/.exec(bank ?? ''); if (!m) return null;
  const id = charId(FE_ABBR[m[2]]); if (id < 0) return null;
  return { kind: m[1] === 'Customize' ? 'customize' : 'select', id, event: m[1] === 'Customize' ? EV.CUSTOMIZE : EV.POST_SELECTION };
}
export function busOfBank(name) { return name.startsWith('DJ_') ? 'DJ' : name.startsWith('PA_') ? 'PA' : name.startsWith('Arcade') ? 'ARCADESPEECH' : 'CHARACTER'; }

// declickMs (audioDeclick, docs/audio-logic.md 9.13): a line stopped while it plays ramps out over this many ms (no click).
// radioBigStops (pv djQueueRules): Radio BIG intro stops the current line before it posts (2A26F0: 2B11B0 at 0x2A272C, after the 29F0D8 gate).
// range(path, start, end) -> Promise<{start, bytes}> (pv speechRange, docs/audio-logic.md 9.15): a line loads as its own byte range of
// the bank's .dat into an LRU of lineCacheBytes (a 200's whole file serves its bank as before), fetched when a line is resolved or
// predicted (predict: at a post, the scheduler's next dispatch on a copy of its state). Without it the whole .dat loads and stays.
export function createSpeech({
  engine,
  json,
  bytes,
  range = null,
  lineCacheBytes = 2 << 20,
  random = Math.random,
  now = () => engine.context?.currentTime ?? 0,
  onPost = null,
  onLines = null,
  declickMs = 0,
  radioBigStops = () => false
}) {
  let events = null,
    banks = null,
    ready = null;
  const rng = new EaRng(Math.floor(random() * 0x7fff)); // 3DB790 seeded from the game rand (2ADF60)
  const st = createSpeechState();
  let sched = null;
  const requests = new GameRequestTable();
  const lines = new Map(); // bank name -> promise {json, dat}
  let tick = 0,
    busy = 1,
    pending = null,
    voice = null,
    djReleaseAt = -1;
  const posts = []; // QA: the last posted events [tick, id, speaker, ...args]
  let onIdle = null; // 2A43B8 hook (game-audio): posts at most one pending DJ request
  let replay = false; // 288AE0

  function init() {
    return (ready ??= Promise.all([bytes('speech/Events.evt'), json('speech/registry.json')]).then(([evt, reg]) => {
      events = parseEvents(evt instanceof Uint8Array ? evt : new Uint8Array(evt));
      banks = makeBankTable(reg.banks.map(hdrFromRegistry));
      sched = new SpeechScheduler(events, banks, rng, st);
    }));
  }
  const bankData = (name) => {
    if (!lines.has(name))
      lines.set(
        name,
        Promise.all([json(`speech/${name}.json`), bytes(`speech/${name}.dat`)]).then(([j, d]) => ({ json: j, dat: d }))
      );
    return lines.get(name);
  };
  // One line's bytes: {line, dat} for decodeSpeechLine, or null (no such line).
  const cached = new Map(); // `${bank}#${line}` -> {promise, size} (LRU order; size once in)
  const wholeDat = new Map(); // bank -> its .dat, when the server answered a range with the whole file (200)
  let cachedBytes = 0;
  const lineStats = { fetched: 0, hits: 0, evicted: 0, predicted: 0 };
  function lineData(bank, index) {
    if (!range) return bankData(bank).then(({ json: j, dat }) => (j.lines[index] ? { line: j.lines[index], dat } : null));
    const key = `${bank}#${index}`,
      hit = cached.get(key);
    if (hit) {
      cached.delete(key);
      cached.set(key, hit);
      lineStats.hits++;
      return hit.promise;
    }
    const entry = { size: 0 };
    entry.promise = json(`speech/${bank}.json`).then((j) => {
      const line = j.lines[index];
      if (!line) return null;
      if (wholeDat.has(bank)) return { line, dat: wholeDat.get(bank) };
      lineStats.fetched++;
      return range(`speech/${bank}.dat`, line.offset, line.offset + line.size).then((r) => {
        if (r.bytes.length !== line.size) {
          wholeDat.set(bank, r.bytes);
          return { line, dat: r.bytes };
        } // (a 200: kept for its bank, as before)
        entry.size = line.size;
        cachedBytes += line.size;
        trimLines(key);
        return { line: { ...line, offset: 0 }, dat: r.bytes };
      });
    });
    entry.promise.catch(() => {
      if (cached.get(key) === entry) {
        cached.delete(key);
        cachedBytes -= entry.size;
      }
    });
    cached.set(key, entry);
    return entry.promise;
  }
  function trimLines(keep) {
    for (const [k, e] of cached) {
      if (cachedBytes <= lineCacheBytes) break;
      if (k === keep || !e.size) continue;
      cached.delete(k);
      cachedBytes -= e.size;
      lineStats.evicted++;
    }
  }
  const prefetch = (list) => {
    if (range) for (const r of list ?? []) for (const it of r.items ?? []) lineData(it.bank, it.line).catch(() => {});
  };
  // The lines the next dispatch would resolve, on a copy of the scheduler (slots, EA random, history): fetched while the voice is still
  // busy or the dispatch is a frame away. A later post can change the pick; the real dispatch fetches its own lines too.
  function predict() {
    if (!range || !sched) return;
    try {
      const copy = Object.assign(Object.create(Object.getPrototypeOf(sched)), sched, {
        rng: Object.assign(Object.create(Object.getPrototypeOf(sched.rng)), sched.rng, { s: sched.rng.s.slice() }),
        state: structuredClone(sched.state),
        slots: sched.slots.map((x) => ({ ...x })),
        count: sched.count.slice(),
        lastPosted: sched.lastPosted.slice(),
        lastPostedPlain: sched.lastPostedPlain.slice()
      });
      const out = copy.dispatch(0, tick);
      if (out?.length) {
        lineStats.predicted++;
        prefetch(out);
      }
    } catch {}
  }
  const audible = (bus) => engine.busGain?.(bus) > 0;
  // 2B0290 / 2B0E28: the voice is free when idle or the playing line has < 500 ms left.
  const free = () => !voice || (!voice.loading && voice.end - now() < 0.5);
  const busyVoice = () => !!voice && (voice.loading || voice.end > now());

  // 2B1458 + 3D7418. Returns true when posted.
  function post(eventId, args, { speaker = 0, refreshDup = false } = {}) {
    if (!sched || replay) return false;
    if (!requests.admit(0, eventId, refreshDup, { speaker })) return false;
    const ok = sched.post(eventId, args, tick, 0);
    if (ok) {
      posts.push([tick, eventId.toString(16), speaker, ...args]);
      if (posts.length > 40) posts.shift();
      onPost?.(eventId, speaker, args);
      predict();
    }
    return ok;
  }
  // 2B1758: resolve the best pending event now (sent on the next free frame).
  function flush() {
    if (!sched) return;
    const out = sched.dispatch(0, tick);
    if (out?.some((r) => r.items.length)) {
      pending = out;
      busy = 0;
      prefetch(out);
    }
  }

  // 3D9BD8 -> 2AF8A8 -> 2B04D8: stream the resolved lines back to back on the one voice.
  async function send(list) {
    const items = [];
    for (const r of list) {
      const req = requests.lookup(0, r.eventId);
      if (req) for (const it of r.items) items.push({ ...it, speaker: req.speaker ?? -1 });
    }
    if (!items.length) return;
    onLines?.(
      items.map((it) => it.bank),
      items[0].speaker
    ); // 29EEE0 per line (bank name -> bus)
    const context = engine.context;
    if (!context || engine.live === false) return; // (live: audioInterrupt)
    const current = {
      loading: true,
      end: Infinity,
      sources: [],
      buses: new Set(),
      lines: items.map((it) => `${it.bank}#${it.line}`),
      speaker: items[0].speaker
    };
    voice = current;
    let data;
    try {
      data = await Promise.all(items.map((it) => lineData(it.bank, it.line)));
    } catch (e) {
      console.warn('Speech bank unavailable', e);
      if (voice === current) voice = null;
      return;
    }
    if (voice !== current) return;
    let t = context.currentTime + 0.02;
    for (let i = 0; i < items.length; i++) {
      const it = items[i];
      if (!data[i]) continue;
      const { line, dat } = data[i];
      const bus = busOfBank(it.bank);
      const t0 = performance.now();
      const buffer = toAudioBuffer(context, decodeSpeechLine(dat, line));
      noteDecode(performance.now() - t0, 'speech');
      const src = context.createBufferSource();
      src.buffer = buffer;
      // CHARACTER: per-speaker gain
      if (declickMs > 0) {
        const g = context.createGain();
        src.connect(g);
        g.connect(engine.bus(bus, it.speaker));
        src.__fade = g;
      } else src.connect(engine.bus(bus, it.speaker));
      if (t < context.currentTime) audioStats.speechLate++;
      src.start(t);
      src.__at = t;
      src.onended = () => letGo(src); // (a finished / stopped line lets go of its nodes)
      if (bus === 'DJ' && !current.buses.has('DJ')) {
        const at = t;
        setTimeout(
          () => {
            if (voice === current) engine.duck(0.65, 0.9999);
          },
          Math.max(0, (at - context.currentTime) * 1000)
        );
      } // 29F000
      current.buses.add(bus);
      current.sources.push(src);
      t += buffer.duration;
    }
    current.loading = false;
    current.end = t;
  }
  function letGo(src) {
    try {
      src.disconnect();
    } catch {}
    try {
      src.__fade?.disconnect();
    } catch {}
  }
  function stop() {
    const v = voice;
    voice = null;
    pending = null;
    const c = engine.context,
      t = c?.currentTime ?? 0,
      dk = declickMs / 1000;
    if (v)
      for (const s of v.sources)
        try {
          if (dk > 0 && s.__fade && s.__at < t) {
            const g = s.__fade.gain;
            g.cancelScheduledValues(t);
            g.setValueAtTime(1, t);
            g.linearRampToValueAtTime(0, t + dk);
            s.stop(t + dk);
          } // playing: ramp out
          else {
            s.stop();
            letGo(s);
          } // (stopped at once, or never started: its ended event may never come)
        } catch {}
    if (v?.buses.has('DJ')) engine.release(0.5);
  }

  const api = {
    EV,
    init,
    post,
    flush,
    stop,
    get ready() {
      return !!sched;
    },
    set onIdle(f) {
      onIdle = f;
    },
    set replay(v) {
      replay = !!v;
    },
    get free() {
      return free();
    },
    get busy() {
      return busyVoice();
    },
    audible,
    // Per 60 Hz game frame (2B0C78 -> 2AFC88, 2B1720).
    update() {
      if (!sched) return;
      tick++;
      requests.tick();
      if (voice && !voice.loading && voice.end <= now()) {
        // 285BF8: DJ duck released 0.5 s after the stream stops
        if (voice.buses.has('DJ')) engine.release(0.5);
        voice = null;
      }
      if (!free()) {
        busy = 1;
        return;
      }
      if (busy === 1) {
        onIdle?.();
        const out = sched.dispatch(0, tick);
        if (out?.some((r) => r.items.length)) {
          pending = out;
          busy = 0;
          prefetch(out);
        }
      } else if (pending) {
        const p = pending;
        pending = null;
        busy = 1;
        send(p);
      }
    },
    // Pause resume (289BB8): queued speech is cancelled.
    cancelQueued() {
      pending = null;
      if (!sched) return;
      for (let i = 0; i < 16; i++) if (sched.slots[i].active) sched.kill(i);
      requests.e.forEach((x) => {
        x.active = false;
      });
    },
    // ---- category functions (args: scratchpad-verified recipes, docs/audio-logic.md 4.3-4.5) ----
    // Rider: r = {id (CHARDB, -1 guest), human, listener (== L), upright, inRange}. Common gate: CHARACTER audible.
    riderGate(r, { needFree = true, needUpright = true, needListener = true } = {}) {
      if (!audible('CHARACTER') || replay || !r) return false;
      if (needListener && !r.human && !r.listener) return false;
      if (needUpright && !r.upright) return false;
      if (needFree && !free()) return false;
      return r.inRange !== false;
    },
    bigAir(r, peak) {
      if (api.riderGate(r)) post(EV.BIG_AIR[peak === 1 ? 0 : peak === 2 ? 1 : 2], [speakerMask(r.id), 3], { speaker: r.id });
    }, // 29FCC8
    trick(r, peak, difficult) {
      if (api.riderGate(r))
        post((difficult ? EV.TRICK_DIFFICULT : EV.TRICK_EASY)[peak === 1 ? 0 : peak === 2 ? 1 : 2], [speakerMask(r.id)], { speaker: r.id });
    }, // 29FF80
    // 2A02D8
    wipeout(r, peak, hub = false) {
      if (api.riderGate(r, { needUpright: false }))
        post(EV.WIPEOUT[peak === 1 ? 0 : peak === 2 ? 1 : 2], [speakerMask(r.id), hub ? 1 : 3], { speaker: r.id });
    },
    whooh(r) {
      if (api.riderGate(r, { needUpright: false })) post(EV.WHOOH, [speakerMask(r.id)], { speaker: r.id });
    }, // 2A1560
    objectHit(r) {
      if (api.riderGate(r)) post(EV.POST_OBJECT_COLLISION, [speakerMask(r.id)], { speaker: r.id });
    }, // 2A0E70 (flag && human)
    // 2A0560: A overtakes B; rel 2 foe / 0,1 friend; fast = 2 at >= 1341.12 cm/s.
    pass(A, B, rel, fast) {
      if (rel === 2) {
        post(EV.PASS_FOE, [speakerMask(A.id), fast], { speaker: A.id });
        post(EV.PASSED_BY_FOE, [speakerMask(B.id), fast], { speaker: B.id });
      } else {
        post(EV.HEY_PASS_FRIEND, [speakerMask(A.id), fast, subjectMask(B.id) | 0x1000], { speaker: A.id });
        post(EV.PASSED_BY_FRIEND, [speakerMask(B.id), fast], { speaker: B.id });
      }
    },
    // 2A0A30: A hits B.
    hit(A, B, rel) {
      const a = [speakerMask(A.id), 3],
        b = [speakerMask(B.id), 3];
      if (rel === 2) {
        post(EV.HIT_BY_FOE, b, { speaker: B.id });
        post(EV.HIT_FOE, a, { speaker: A.id });
      } else if (rel === 1) {
        post(EV.AGGRESSION_RESPONSE, b, { speaker: B.id });
        post(EV.HIT_FOE, a, { speaker: A.id });
      } else {
        post(EV.HIT_BY_FRIEND, b, { speaker: B.id });
        post(EV.HIT_FRIEND, a, { speaker: A.id });
      }
    },
    finishLineRider(r, place, rival = false, mp = false) {
      // 2A1400
      if (!audible('CHARACTER') || replay) return;
      const p = placeMask(place);
      post(EV.FINISH_LINE_RIDER, [speakerMask(r.id), p === 1 ? 1 : rival || mp ? 2 : p === 2 || p === 4 ? 4 : 2], { speaker: r.id });
    },
    hey(r, to) {
      if (audible('CHARACTER') && !replay) post(EV.HEY, [speakerMask(r.id), subjectMask(to.id)], { speaker: r.id });
    }, // 2A1280
    bcChallenge(r, to) {
      if (audible('CHARACTER') && !replay) post(EV.BC_CHALLENGE, [speakerMask(r.id), subjectMask(to.id), 4], { speaker: r.id });
    }, // 2A1138
    postSelection(id) {
      if (audible('CHARACTER') && free()) post(EV.POST_SELECTION, [speakerMask(id)], { speaker: id, refreshDup: true });
    }, // 2A16B0
    customize(id) {
      if (audible('CHARACTER') && free()) post(EV.CUSTOMIZE, [speakerMask(id)], { speaker: id, refreshDup: true });
    }, // 2A1778
    // Arcade speech (ARCADESPEECH, speaker 0xC).
    arcade(eventId, mask) {
      if (audible('ARCADESPEECH') && !replay && mask) post(eventId, [mask], { speaker: SPEAKER.ARCADE });
    },
    // DJ (29F0D8 gate: DJ audible, i.e. DJ on and radio mode != 2).
    dj(eventId, args) {
      if (audible('DJ') && !replay) post(eventId, [LANG, ...args], { speaker: SPEAKER.DJ });
    },
    // 2A26F0
    radioBigIntro(a1, a2) {
      if (radioBigStops() && audible('DJ')) stop();
      api.dj(EV.RADIO_BIG_INTRO, [a1 ? 1 : 2, a2 ? 2 : 1]);
    },
    // 2A2860
    artistIntro(sed) {
      const s = sed ?? -1;
      api.dj(EV.ARTIST_INTRO, [
        3,
        s < 0 || s === 999 ? 0 : s < 100 ? (2 ** s) >>> 0 : 0,
        s < 0 || s === 999 || s < 100 ? 0 : (2 ** (s - 100)) >>> 0
      ]);
    },
    eventIntro(loc, region = -1) {
      api.dj(EV.EVENT_INTRO, [courseBit(loc, region), 3]);
    }, // 2A24B0 / 2A2568
    radioBigOutro() {
      api.dj(EV.RADIO_BIG_OUTRO, []);
    }, // 2A27D8
    // PA (29F128 gate).
    pa(eventId, args) {
      if (audible('PA') && !replay) post(eventId, [LANG, ...args], { speaker: SPEAKER.PA });
    },
    venueIntro(loc) {
      api.pa(EV.PA_VENUE_INTRO, [courseBit(loc)]);
    }, // 2A39E0
    sponsorIntro() {
      api.pa(EV.PA_SPONSOR_INTRO, [1]);
    }, // 2A31C0
    medalRunIntro(loc) {
      api.pa(EV.PA_MEDAL_RUN_INTRO, [courseBit(loc)]);
    }, // 2A3400
    // NIS kind-7 PA cues (2A19D8 codes 1 / 2 / 7; the approaches and the podium scenes): the subject is the first human
    // (28B1D8 +0x28 -> 2A1D48 = 2A1DA0 subject mask); PA_Medals adds the human's place 0/1/2 -> 1/2/4 (else 0), or 8
    // when the BE library record 0xD +0x14 is clear outside multiplayer (not the career podium).
    riderIntro(r) {
      api.pa(EV.PA_RIDER_INTRO, [subjectMask(r?.id ?? -1)]);
    }, // 2A32B0
    riderRaceIntro(r) {
      api.pa(EV.PA_RIDER_RACE_INTRO, [subjectMask(r?.id ?? -1)]);
    }, // 2A3358
    medals(r, place, career = true) {
      if (!replay) api.pa(EV.PA_MEDALS, [subjectMask(r?.id ?? -1), !career ? 8 : place === 0 ? 1 : place === 1 ? 2 : place === 2 ? 4 : 0]);
    }, // 2A3860
    paFinishLine(r, place) {
      if (r.id >= -1) api.pa(EV.PA_FINISH_LINE, [subjectMask(r.id), placeMask(place)]);
    }, // 2A3708
    riderPosition(r, place, fromListener = false) {
      api.pa(EV.PA_RIDER_POSITION, [subjectMask(r.id), fromListener ? (RIDER_PLACE_MASK[place] ?? 0) : placeMask(place)]);
    }, // 2A34D0
    // Front-end rider speech: web/rider-speech.js asks for Post_Selection_<abbr> / Customize_<abbr> (frontEndCue).
    speakBank(bank) {
      const cue = frontEndCue(bank);
      if (!cue) return false;
      if (cue.kind === 'customize') api.customize(cue.id);
      else api.postSelection(cue.id);
      return true;
    },
    // 2A10C0(audio, speaker): the request being spoken now (2B1220 -> 2AB150 -> the request's +0x80) is that speaker's (10 = the DJ)
    speaking(speaker) {
      return !!voice && voice.speaker === speaker;
    },
    debug() {
      return {
        ready: !!sched,
        tick,
        busy,
        voice: voice
          ? {
              loading: voice.loading,
              left: +(voice.end - now()).toFixed(2),
              buses: [...voice.buses],
              lines: voice.lines,
              speaker: voice.speaker
            }
          : null,
        pending: sched ? sched.slots.filter((s) => s.active).map((s) => s.ev.id.toString(16)) : [],
        last: st.last.get(0)?.id?.toString(16) ?? null,
        posts: posts.slice(),
        lineCache: {
          ...lineStats,
          lines: cached.size,
          kb: Math.round(cachedBytes / 1024),
          wholeBanks: [...wholeDat.keys()],
          banks: lines.size
        }
      };
    }
  };
  return api;
}
