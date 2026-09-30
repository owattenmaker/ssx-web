// SSX 3 music runtime: EA "Pathfinder" (.mpf v4) song graphs, ported from the PS2 EE code.
//
// Recovered semantics (addresses are in the EE executable; the full write-up is docs/audio-logic.md section 8):
//   slot / voices      0x517610 + 0x928*slot; one voice per track at slot+0x58[track]. Track 0 = the stream
//                      (0x11000001, MUSDATA .mus), track 1 = the async loop overlay (0x11000002, LOOPDATA bank).
//                      2B21E0 creates them with latency 500 ms / capacity 2 (track 0) and 10 ms / 1 (track 1);
//                      a latency < 50 makes the voice beat-synced to the master (voice+0x2F, 3D25F0).
//   commit rule        3D4F10: a voice commits its next node when the audio still queued for it (3D4950, sum
//                      of the queue entries' remaining ms) is <= its latency and the queue has room (2AEBD0).
//                      The stream therefore always knows its next bar ~500 ms ahead; confirmed on ARMSX2
//                      (commit at 420..490 ms before the end of each 1818 ms Avalanche bar).
//   next node          3D3D20 -> 3D37A0 (branch pick) -> 3D3900 (walk control nodes) -> 3D41A8 (queue).
//                      The branch value is the voice "intensity" byte voice+1 (3D0D70, game 2B3C28): 127 in the
//                      front end / hubs, a 0 -> 127 ramp in races (28F000). Ranges are inclusive, first match
//                      wins, else the nearest range. Random values only come from control nodes with
//                      w4 bits 17..19 == 1 (and sample -2): (now_ms / 23) & 0x7F.
//   loop counters      end nodes (sample -1) with flags>>12 = n: voice+0x2C/+0x20 count n..0 (n+1 passes, then
//                      branch value 0 exits); 0xF (and any n >= 8) loops until an event moves the voice.
//   events             3D16F0 copies the event (index = event number) into a queue; 3D1D28 runs its actions in
//                      order, each may block: 0x02 wait ms since the previous action, 0x04 jump (appended after
//                      the audio already queued; flag byte 15 bit 0 cuts it first; node -17..-31 = register,
//                      byte 14 >= 0 = n-th part of that section), 0x06 volume ramp, 0x0A save register
//                      (-33 = current node). A jump on the loop track waits for the master's next beat.
//   loops              beat-locked 1-beat slices (w12 bit 1): slice k of a node plays bank entry value+k-1 on
//                      master beat k of the bar (3D3658 sync 0x100), one per master beat (3D41A8 +0x48/+0x10C).
//   end of song        a branch to 0xFFFF / a node without branches stops the voice; 3D11B8 reports -1 when
//                      nothing is left playing (player.finished).
//
// Exports: loadSong / prepareSong (assets from tools/export_audio.py), createPathfinderCore (pure, timed state
// machine, no Web Audio), createGraphWalker (stepper for tests), createPathfinderPlayer (Web Audio, look-ahead
// scheduler with deterministic replay), renderSongOffline (node/offline PCM render).
import { decodeMusicSample, decodeBankPatch, parseBank, toAudioBuffer } from './audio-decode.js';
import { createGuardedWorker, workerUrl } from './worker-guard.js';   // build handshake + main-thread fallback (docs/workers.md)
import { decodeAudioJob } from './audio-decode-job.js';
import { audioStats, noteMusicLate, noteDecode } from './audio-stats.js';   // field counters (web/diagnostics.js)
const AUDIO_DECODE_WORKER = workerUrl((Worker) => new Worker(new URL('./audio-decode-worker.js', import.meta.url), { type: 'module' }));

export const PATHFINDER = Object.freeze({
  MAIN_LATENCY_MS: 500, MAIN_CAPACITY: 2,     // 2B21E0 -> 3D25F0(0x11000001, .., 0x1F4, .., t0 = 2)
  LOOP_LATENCY_MS: 10, LOOP_CAPACITY: 1,      // 2B21E0 -> 3D25F0(0x11000002, .., 0xA, .., t0 = 1)
  SYNC_LATENCY_BELOW: 50,                     // 3D25F0: latency < 50 -> voice+0x2F (beat-synced slave)
  MAX_EVENTS: 16,                             // slot+0xDC[16]
  MIXER_RATE: 36000,                          // EE mixer +0x2A; LOWPASS is normalised by rate/2 (3C3178)
});
const EPS = 0.5; // ms tolerance for beat / boundary comparisons

const s8 = (v) => (v << 24) >> 24;
const s16 = (v) => (v << 16) >> 16;
const s4 = (v) => ((v & 0xf) << 28) >> 28;

// ---------------------------------------------------------------------------------------------------------------
// Song data
// ---------------------------------------------------------------------------------------------------------------

function parseAction(raw) {
  const b = Uint8Array.from(raw.match(/../g).map((h) => parseInt(h, 16)));
  const u32 = (p) => (b[p] | (b[p + 1] << 8) | (b[p + 2] << 16) | (b[p + 3] << 24)) >>> 0;
  return {
    mask: b[0] | (b[1] << 8) | (b[2] << 16), // tracks (bit t = track t)
    timeout: u32(4),                         // +4: 0 in all retail data
    op: b[9],
    word: u32(12),                           // +0xC as the interpreter loads it (s4)
    value: s16(b[12] | (b[13] << 8)),
    arg: s8(b[14]),                          // section for 0x04 (-1 = absolute node), register for 0x0A
    argU: b[14],
    flags: b[15],                            // 0x04: bit 0 = flush the voice first
    b12: s8(b[12]), b13: b[13], u16_14: b[14] | (b[15] << 8),
  };
}

// Normalise the exporter's JSON (web/public/assets/AUDIO/music/<Song>.json) into the runtime graph.
export function buildGraph(json) {
  const nodes = json.nodes.map((n) => {
    const w4 = n.w4 >>> 0, w8 = n.w8 >>> 0;
    return {
      index: n.index, sample: n.sample, flags: n.flags,
      track: n.flags & 0x1f, section: (n.flags >> 5) & 0x3f, loopInit: (n.flags >> 12) & 0xf,
      router: w4 & 0xff, measures: (w4 >>> 20) & 0xf, beats: (w4 >>> 24) & 0xf,
      random: (w4 & 0xe0000) === 0x20000, // 3D3B08: w4 & 0xE0000 == 0x20000 -> value = (now/23) & 0x7F
      sync: w8 & 0x300, group: w8 >>> 20, trigger: (w8 >>> 10) & 0x1f, sliced: (n.w12 & 2) !== 0, w12: n.w12,
      branches: n.branches.map((b) => ({ lo: s8(b.lo), hi: s8(b.hi), node: s16(b.node) })),
    };
  });
  const events = json.events.map((e) => ({ id: e.id, actions: e.actions.map((a) => parseAction(a.raw)) }));
  const samples = json.samples.map((s) => ({ ...s, durMs: s.sampleCount && s.sampleRate ? (s.sampleCount * 1000) / s.sampleRate : s.ms }));
  const tracks = json.tracks.map((t) => t.kind);
  const routers = (json.routers || []).map((r) => r.map((p) => ({ from: p.fromNode, to: p.toNode })));
  return { id: json.id, inf: json.inf || {}, nodes, events, samples, tracks, routers };
}

// song = { id, inf, graph, mus, loops, decodeSegment(seg) }. `mus` / `loops` are the original .mus files.
// decodeAsync(bytes, sample) -> Promise<decoded> (optional, e.g. musicDecodeWorker()): MicroTalk stream bars ("Screw Up",
// ~40 ms of EE-exact decoding each) then decode off the main thread when the player asks for them ahead of time.
// asyncAll (pv musicWorkerDecode, docs/audio-logic.md 9.13): every stream bar (EA-XA too, 3-25 ms each on the main thread, a
// 6-channel menu bar the most) is decoded by decodeAsync ahead of its time, not only the MicroTalk ones.
export function prepareSong(json, { mus = null, loops = null, decodeAsync = null, asyncAll = false } = {}) {
  const viaWorker = (sample) => !!decodeAsync && sample?.kind === 'stream' && (asyncAll || sample.codec === 'microtalk');
  // (asyncAll: at most 12 decoded bars wait to be handed out, oldest dropped: bars a re-simulation no longer plays would pile up)
  const prune = () => { let n = 0; for (const v of pending.values()) if (!(v instanceof Promise)) n++; for (const [k, v] of pending) { if (n < 12) break; if (!(v instanceof Promise)) { pending.delete(k); n--; } } };
  const graph = buildGraph(json);
  let bank = null;
  const pending = new Map(); // stream sample index -> Promise (in flight) | decoded (ready, handed out once)
  // a streamed .mus (createMusStream): a stream bar loads when asked for, the bars it leads to after it (read-ahead)
  const streamed = !!mus?.stream;
  const barIn = (sample) => !streamed || sample.kind !== 'stream' || mus.has(sample.offset, sample.size);
  const loadBar = (sample, priority) => (barIn(sample) ? Promise.resolve(true) : mus.load(sample.offset, sample.size, priority));
  const readAhead = (node) => { if (!streamed || node == null) return; for (const n of nextAudioNodes(graph, node, mus.busy() ? 1 : 2)) { const smp = graph.samples[graph.nodes[n].sample - 1]; if (smp?.kind === 'stream') loadBar(smp, 'low'); } };
  const request = (index) => {
    const sample = graph.samples[index - 1];
    const decode = () => decodeAsync(mus.subarray(sample.offset, sample.offset + sample.size), { ...sample, offset: 0 });   // (at once when the bar is in)
    const job = (barIn(sample) ? decode() : loadBar(sample, 'high').then((ok) => (ok ? decode() : Promise.reject(new Error('music bar not loaded')))))
      .then((d) => { if (pending.get(index) !== job) return; if (asyncAll) { pending.delete(index); prune(); } pending.set(index, d); }, () => { if (pending.get(index) === job) pending.delete(index); });
    pending.set(index, job);
    return job;
  };
  const song = {
    id: json.id, title: json.title, inf: graph.inf, graph, json, mus, loops,
    get bank() { if (!bank && loops) bank = parseBank(loops); return bank; },
    // Decoded PCM for a committed segment: whole stream bar, or one loop-bank entry (a 1-beat slice).
    // aheadMs: how long before the segment starts the caller needs it. With decodeAsync, a MicroTalk bar needed later
    // than ASYNC_AHEAD_MS returns null while the worker decodes it (the player asks again on its next pump; stream bars
    // are committed ~500 ms ahead); anything needed sooner decodes here.
    decodeSegment(seg, { aheadMs = 0 } = {}) {
      const sample = graph.samples[seg.sample - 1];
      if (streamed && sample.kind === 'stream') { readAhead(seg.node); if (!barIn(sample)) { if (aheadMs <= 0) (mus.late ??= new Set()).add(seg.sample); loadBar(sample, 'high'); return null; } }   // not in yet: asked again on the next pump (late: due and not in)
      if (viaWorker(sample)) {
        const got = pending.get(seg.sample);
        if (got && !(got instanceof Promise)) { pending.delete(seg.sample); return got; }
        if (!got && aheadMs > ASYNC_AHEAD_MS) { request(seg.sample); return null; }
        if (got && aheadMs > (asyncAll ? SYNC_MARGIN_MS : 0)) return null; // in flight and still in time (asyncAll: decoded here while the next pump could still be late)
        pending.delete(seg.sample);
      }
      if (sample.kind === 'stream') return decodeMusicSample(mus, null, sample);
      const entry = song.bank?.entries[seg.bankIndex];
      if (!entry) return null;
      return decodeBankPatch(loops, entry.patches[0]);
    },
    // Worker-decode these committed segments' MicroTalk stream bars; resolves when they are ready (or failed).
    prefetch(segs) {
      if (streamed) return Promise.all(segs.map((seg) => { const sample = graph.samples[seg.sample - 1]; if (seg.kind !== 'stream' || !sample) return null;
        return loadBar(sample, 'high').then(() => (viaWorker(sample) && barIn(sample) ? (pending.has(seg.sample) ? pending.get(seg.sample) : request(seg.sample)) : null)).catch(() => null); }));
      if (!decodeAsync) return Promise.resolve();
      return Promise.all(segs.map((seg) => {
        const sample = graph.samples[seg.sample - 1];
        if (seg.kind !== 'stream' || !viaWorker(sample)) return null;
        return pending.has(seg.sample) ? pending.get(seg.sample) : request(seg.sample);
      }));
    },
  };
  return song;
}

// Songs with MicroTalk stream bars: decode the bars the song opens with before it starts (the player would otherwise
// decode its first bar on the main thread). Dry-runs the start event on a core whose random draws are recorded; the
// returned random replays them, so the player given it commits exactly the prefetched bars. null for other songs.
export async function prefetchSongStart(song, event, { intensity = 0 } = {}) {
  if (!song.json?.samples?.some((s) => s.kind === 'stream' && s.codec === 'microtalk') || !song.prefetch) return null;
  const draws = [];
  const core = createPathfinderCore(song, { random: () => { const v = Math.random(); draws.push(v); return v; }, intensity });
  core.sendEvent(event, 0);
  core.advanceTo(1000);
  await song.prefetch(core.state.voices[0]?.queue.filter((s) => s.start < 1000) ?? []);
  let i = 0;
  return () => (i < draws.length ? draws[i++] : Math.random());
}

const ASYNC_AHEAD_MS = 150, SYNC_MARGIN_MS = 80, SHIFT_MAX_S = 0.012;

// ---- streamed .mus (pv musicStream, docs/audio-logic.md "Music streaming") ------------------------------------------------------
// The PS2 streams a song's MUSDATA from the disc; the browser downloaded the whole file first (charsel.mus 44 MB, ~60 s at
// 6 Mbit/s, before the menu song could play, sharing the link with the course). A streamed .mus holds only the bars asked for:
// each stream sample is its own byte range (graph.samples offset / size), fetched with fetchRange(start, end, priority) ->
// {start, bytes} (a 206; a server that answers 200 hands over the whole file, which then serves every bar). prepareSong asks for
// a bar when the player commits it (decodeSegment returns null until it is in; the player asks again on its next pump) and
// reads ahead along the graph's branches (depth 2, 1 while busy(): the game's own downloads come first).
// Memory: the bars of a song are kept up to budgetBytes (a safety net: a song's graph reaches a bounded set, Ride 21.7 MB and Clockworks
// 12 MB after 15 simulated minutes; a lower cap made the read-ahead fetch evicted bars again, 3-4x the traffic); beyond it the played,
// then the least recently used bars go, except those used in the last keepMs and a server's whole-file answer (a 200: it serves every
// bar). A bar asked for again after that loads again, as the first time.
export function createMusStream(fetchRange, { busy = () => false, budgetBytes = MUS_BUDGET_BYTES, keepMs = MUS_KEEP_MS, clock = () => performance.now() } = {}) {
  const chunks = [], pending = new Map(), failed = new Map();   // failed: range -> time (a failed bar is asked again after 2 s)
  let bytes = 0, evicted = 0;
  const find = (a, b) => { const c = chunks.find((x) => x.start <= a && b <= x.start + x.bytes.length) || null; if (c) c.used = clock(); return c; };
  function trim() {
    if (bytes <= budgetBytes) return;
    const t = clock(), old = chunks.filter((c) => !c.whole && t - c.used > keepMs).sort((a, b) => (b.played - a.played) || a.used - b.used); // played bars first
    for (const c of old) { if (bytes <= budgetBytes) break; chunks.splice(chunks.indexOf(c), 1); bytes -= c.bytes.length; evicted++; }
  }
  const s = {
    stream: true, busy,
    get bytesLoaded() { return bytes; },
    get evicted() { return evicted; }, // QA: bars let go under the budget
    has: (offset, size) => !!find(offset, offset + size),
    // resolves when [offset, offset + size) is in (or the fetch failed: false)
    load(offset, size, priority = 'high') {
      if (s.has(offset, size)) return Promise.resolve(true);
      const key = `${offset}:${size}`; if (pending.has(key)) return pending.get(key);
      if (performance.now() - (failed.get(key) ?? -1e9) < 2000) return Promise.resolve(false);
      const job = fetchRange(offset, offset + size, priority).then((r) => { if (r?.bytes) { chunks.push({ start: r.start ?? 0, bytes: r.bytes, used: clock(), played: 0, whole: r.bytes.length !== size }); bytes += r.bytes.length; trim(); } return s.has(offset, size); }, () => false)
        .then((ok) => { if (ok) failed.delete(key); else failed.set(key, performance.now()); return ok; }).finally(() => pending.delete(key));
      pending.set(key, job); return job;
    },
    subarray(a, b) { const c = find(a, b); if (!c) throw new Error(`music bytes ${a}..${b} not loaded`); c.played = 1; return c.bytes.subarray(a - c.start, b - c.start); },
  };
  return s;
}
// The audio nodes a node leads to (through control nodes), up to `depth` audio steps: the bars the stream may commit next.
function nextAudioNodes(graph, from, depth) {
  const out = new Set(), seen = new Set([from]); let frontier = [from];
  for (let d = 0; d < depth && frontier.length; d++) {
    const next = [];
    for (const n0 of frontier) {
      const stack = graph.nodes[n0]?.branches.map((b) => b.node) ?? [];
      for (let guard = 0; stack.length && guard < 256; guard++) {
        const n = stack.pop(); if (n < 0 || n >= graph.nodes.length || seen.has(n)) continue; seen.add(n);
        if (graph.nodes[n].sample > 0) { out.add(n); next.push(n); } else stack.push(...graph.nodes[n].branches.map((b) => b.node));
      }
    }
    frontier = next;
  }
  return out;
}
// .mus path -> streamed .mus, for the songs the player needs: the one playing (loadSong role 'play', game-audio.js playSong) and the next
// one (role 'next', prefetchPicked); any other song lets go of its bars (the last 4 songs kept theirs: 38.7 MB of bars 15 min into a Peak
// 2 Race). A player still fading one out holds its own. A song played again streams its bars again. Without a role (tools, tests): the
// two asked for last.
const musStreams = new Map(), MUS_SONGS = 2, MUS_BUDGET_BYTES = 24 << 20, MUS_KEEP_MS = 20000, musRoles = { play: null, next: null };
function musStreamFor(path, fetchRange, opts, role = null) {
  let s = musStreams.get(path);
  if (s) musStreams.delete(path); else s = createMusStream((a, b, priority) => fetchRange(path, a, b, priority), opts);
  musStreams.set(path, s);
  if (role === 'play') { musRoles.play = path; if (musRoles.next === path) musRoles.next = null; }
  else if (role === 'next') musRoles.next = path;
  if (role) { for (const k of [...musStreams.keys()]) if (k !== musRoles.play && k !== musRoles.next) musStreams.delete(k); }
  else while (musStreams.size > MUS_SONGS) musStreams.delete(musStreams.keys().next().value);
  return s;
}
export const musStreamStats = () => [...musStreams].map(([path, s]) => ({ path, mb: +(s.bytesLoaded / 1048576).toFixed(1), evicted: s.evicted })); // QA
// The opening bars of a streamed song, before its player starts (the PS2 fills its stream buffer before it plays): a dry run of
// the start event (as prefetchSongStart) names the bars of its first aheadMs (the start event's jumps included); they load (and
// MicroTalk bars decode) first. The returned random replays
// the dry run's draws, so the player commits those bars. null for a song that is not streamed.
export async function streamSongStart(song, event, { intensity = 0, aheadMs = 4000, startMs = 1500 } = {}) {
  if (!song.mus?.stream || !song.prefetch) return null;
  const draws = [];
  const core = createPathfinderCore(song, { random: () => { const v = Math.random(); draws.push(v); return v; }, intensity });
  core.sendEvent(event, 0);
  core.advanceTo(aheadMs);
  const queue = core.state.voices[0]?.queue ?? [], opening = queue.filter((s) => s.start < startMs);
  // A player whose random nodes follow the audio clock (pv bigChallengeAudio, 0x3D3B08: (clock / 23) & 0x7F) may open with
  // other bars: the dry run again for every clock value; their opening bars too, when they add up to little.
  const alt = new Map();
  for (let v = 0; v < 128; v++) {
    if (v && !(v % 16)) await new Promise((r) => setTimeout(r, 0));   // a few ms at a time
    const c = createPathfinderCore(song, { intensity, clock: (t) => v * 23 + t }); c.sendEvent(event, 0); c.advanceTo(startMs);
    for (const s of c.state.voices[0]?.queue ?? []) if (s.start < startMs && !opening.some((o) => o.sample === s.sample)) alt.set(s.sample, s);
  }
  const altBytes = [...alt.values()].reduce((b, s) => b + (song.graph.samples[s.sample - 1]?.size ?? 0), 0);
  song.prefetch(queue.filter((s) => s.start < aheadMs)).catch(() => {});   // the bars of the first aheadMs start loading...
  await song.prefetch([...opening, ...(altBytes <= 768 * 1024 ? alt.values() : [])]); // ...and it plays once the opening ones are in
  try { performance.mark(`music:start:${song.id}`); } catch {}
  let i = 0;
  return () => (i < draws.length ? draws[i++] : Math.random());
}

// One module worker (web/audio-decode-worker.js) for MicroTalk music bars; null without Worker support (node). A worker
// that cannot start or fails runs the job on the main thread (web/worker-guard.js); a job lost with a failed worker
// rejects (the caller then decodes synchronously).
let musicWorker = null;
// fold: a 6-channel bar comes back as the stereo sum toAudioBuffer would make ([0, 2, 4] / [1, 3, 5], same order: the same floats).
export function musicDecodeWorker({ fold = false } = {}) {
  if (typeof Worker === 'undefined') return null;
  musicWorker ??= createGuardedWorker({ name: 'audio-decode', url: AUDIO_DECODE_WORKER, local: () => decodeAudioJob, cloneInput: false });
  return (bytes, sample) => { const copy = bytes.slice(); return musicWorker.request({ music: copy, sample, ...(fold ? { fold: true } : {}) }, [copy.buffer]).then((decoded) => decoded[0]); };
}

// Catalog song id (e.g. 'Go') -> song. fetchJson/fetchBytes take paths relative to the AUDIO asset root.
// fetchRange(path, start, end, priority) -> {start, bytes} (optional, pv musicStream): the stream track streams bar by bar
// (createMusStream); busy() -> the game's own downloads are running (the read-ahead holds back).
// workerDecode (pv musicWorkerDecode): every stream bar decodes in the worker (prepareSong asyncAll), 6-channel bars folded to stereo there.
// role ('play' | 'next', with fetchRange): which of the songs the streamed .mus cache keeps it as (musStreamFor).
export async function loadSong(id, { fetchJson, fetchBytes, fetchRange = null, busy = undefined, workerDecode = false, role = null }) {
  const json = await fetchJson(`music/${id}.json`);
  const stream = json.tracks.find((t) => t.kind === 'stream');
  const bankTrack = json.tracks.find((t) => t.kind === 'bank');
  const [mus, loops] = await Promise.all([
    stream ? (fetchRange ? musStreamFor(`music/${stream.file}`, fetchRange, { busy }, role) : fetchBytes(`music/${stream.file}`)) : null,
    bankTrack ? fetchBytes(`music/${bankTrack.file}`) : null,
  ]);
  const asU8 = (x) => (x == null ? null : x instanceof Uint8Array || x.stream ? x : new Uint8Array(x));
  const heavy = json.samples.some((s) => s.kind === 'stream' && s.codec === 'microtalk');
  return prepareSong(json, { mus: asU8(mus), loops: asU8(loops), decodeAsync: heavy || workerDecode ? musicDecodeWorker({ fold: !!workerDecode }) : null, asyncAll: !!workerDecode });
}

// Race intensity (28F000, called every frame while racing): trunc(min(counter / N, 1) * 127), counter reset to 0
// by PlayMusic, falls and resets (28F108) and counted up afterwards; N = 800 / 1000 / 1200 frames by the rider
// attribute tier of 289C18 (< 4, < 8, else). Hub, FE and podium songs use 127 (2B3C28(mgr, 0x7F)).
export function raceIntensity(framesSinceReset, tier = 0) {
  const n = [800, 1000, 1200][tier] ?? 800;
  return Math.trunc(Math.min(Math.max(framesSinceReset / n, 0), 1) * 127);
}

// ---------------------------------------------------------------------------------------------------------------
// Core: deterministic, timed Pathfinder state machine (times in ms of song clock)
// ---------------------------------------------------------------------------------------------------------------

// clock (optional): song ms -> the EE's ms-since-boot (2AF370). With it, a random part head's branch value is the PS2's
// (now_ms / 23) & 0x7F (3D3B08) instead of a `random` draw (the Big Challenge stingers, the loop heads, the hub songs).
export function createPathfinderCore(songOrGraph, { random = Math.random, intensity = 0, clock = null } = {}) {
  const g = songOrGraph.graph ?? songOrGraph;
  const { nodes, events, samples } = g;
  const outputs = [];
  let rngLog = [], rngBase = 0; // replay log so that re-simulation from a snapshot repeats the same draws

  const makeVoice = (track) => {
    const main = track === 0;
    const latency = main ? PATHFINDER.MAIN_LATENCY_MS : PATHFINDER.LOOP_LATENCY_MS;
    return {
      track, latency, capacity: main ? PATHFINDER.MAIN_CAPACITY : PATHFINDER.LOOP_CAPACITY,
      sync: latency < PATHFINDER.SYNC_LATENCY_BELOW,
      node: 0, active: false, group: -1, loopNode: -1, loopCount: 0, slice: main ? -1 : 0,
      intensity: intensity & 0x7f, prevIntensity: 0, scale: 127, pct: 100, paused: false,
      queue: [], nextAt: null, startAt: null, pendingJump: false, beats: 0, ramps: [],
    };
  };
  const hasTrack = (t) => samples.some((s) => s.track === t);
  let st = {
    time: 0, seq: 0, rngIdx: 0, regs: new Array(16).fill(0), started: false,
    voices: [makeVoice(0)].concat(hasTrack(1) ? [makeVoice(1)] : []),
    events: [], armed: false,
  };

  const node = (i) => (i >= 0 && i < nodes.length ? nodes[i] : null);
  const emit = (o) => { outputs.push(o); return o; };
  function rand7() {
    const i = st.rngIdx - rngBase;
    let v;
    if (i < rngLog.length) v = rngLog[i]; else { v = random(); rngLog.push(v); }
    st.rngIdx++;
    return Math.floor(v * 128) & 0x7f;
  }

  // 3D3C90: router of the previous node remaps the target.
  function route(prev, n) {
    const pn = node(prev);
    if (!pn || !pn.router) return n;
    for (const p of g.routers[pn.router - 1] || []) if (n === p.from) n = p.to;
    return n;
  }

  // 3D37A0: branch pick.
  function choose(v, n, value) {
    const nd = node(n); if (!nd) return -1;
    const tv = st.voices[nd.track] || v;
    let val = value;
    if (tv.loopNode === n) {
      const c = s4(tv.loopCount); val = c & 0x7f;
      if (s4(nd.loopInit) > 0) { const c2 = c - 1; tv.loopCount = c2 & 0xf; if (s4(c2) === -1) tv.loopNode = -1; }
    }
    for (const b of nd.branches) if (b.lo <= val && val <= b.hi) return b.node;
    if (!nd.branches.length) return -1;
    let best = 127, pick = nd.branches[0];
    for (const b of nd.branches) {
      const d = Math.min(Math.abs(b.lo - val), Math.abs(b.hi - val));
      if (d < best) { best = d; pick = b; }
    }
    return pick.node;
  }

  // 3D3900: walk control nodes (sample <= 0) until an audio node or -1.
  function resolve(v, prev, n, value, t) {
    n = route(prev, n);
    for (let guard = 0; n >= 0 && n < nodes.length && nodes[n].sample <= 0 && guard < 4096; guard++) {
      const nd = nodes[n]; let val = value;
      if (nd.sample === 0) {
        (st.voices[nd.track] || v).group = n;                  // voice+0x30 = part head
        if (nd.trigger) emit({ type: 'trigger', time: t, node: n, index: nd.trigger }); // w8 bits 10-14 (unused)
        if (nd.random) val = clock ? Math.floor(clock(t) / 23) & 0x7f : rand7();
      } else if (nd.sample === -1) {
        const tv = st.voices[nd.track] || v;
        if (nd.loopInit && tv.loopNode !== n) { tv.loopNode = n; tv.loopCount = nd.loopInit; }
      } else if (nd.sample === -2) {
        val = rand7();
      } else if (nd.sample === -3) {
        postEvent((nd.w12 >>> 2) & 0xfff, t);
      }
      n = route(prev, choose(v, n, val));
    }
    return n >= 0 && n < nodes.length ? n : -1;
  }

  // Master for beat sync (3D4B00): the playing voice with the largest latency.
  function master() {
    let m = null;
    for (const v of st.voices) if (v.active && v.node >= 0 && v.group >= 0 && !v.paused && (!m || v.latency > m.latency)) m = v;
    return m;
  }
  const segAt = (v, t) => v.queue.find((s) => s.start - EPS <= t && t < s.end - EPS) || null;
  // Master beat grid at t: the first beat at or after t (strict: after t).
  function masterBeat(t, strict = false) {
    const m = master(); if (!m) return null;
    const seg = segAt(m, t); if (!seg) return null;
    const nd = node(seg.node), per = nd.beats && nd.measures ? nd.beats * nd.measures : 1;
    const beatMs = (seg.end - seg.start) / per, bpm = nd.beats || 1;
    let k = strict ? Math.floor((t - seg.start + EPS) / beatMs) + 1 : Math.ceil((t - seg.start - EPS) / beatMs);
    if (k < 0) k = 0;
    const at = k >= per ? seg.end : seg.start + k * beatMs;
    return { at, index: k, inMeasure: k % bpm, bpm, beatMs, seg, voice: m };
  }

  const lastEnd = (v, t) => { const q = v.queue; return q.length && q[q.length - 1].end > t ? q[q.length - 1].end : t; };
  function pushSeg(v, n, start, dur, extra = {}) {
    const nd = nodes[n], smp = samples[nd.sample - 1];
    const seg = { id: ++st.seq, voice: v.track, node: n, sample: nd.sample, section: nd.section,
      group: v.group, start, end: start + dur, commit: st.time, kind: smp.kind, scale: v.scale, ...extra };
    if (!v.sync) { seg.beat0 = v.beats; v.beats += nd.beats && nd.measures ? nd.beats * nd.measures : 1; }
    v.queue.push(seg);
    emit({ type: 'segment', ...seg });
    return seg;
  }

  function stopVoice(v) { v.node = -1; v.active = false; v.group = -1; v.nextAt = null; v.startAt = null; if (v.sync) v.slice = 0; }
  function cutVoice(v, t) { // 2AF838: flush the queue and stop what is playing now
    v.queue = v.queue.filter((s) => s.start < t - EPS);
    for (const s of v.queue) if (s.end > t) s.end = t;
    v.nextAt = null; v.startAt = null;
    emit({ type: 'cut', voice: v.track, time: t });
  }

  // 3D41A8: queue the audio of v.node. Returns 'queued' | 'defer' | 'stopped'.
  function commit(v, n, t) {
    v.node = n;
    if (n < 0) { stopVoice(v); emit({ type: 'stop', voice: v.track, time: t }); return 'stopped'; }
    const nd = nodes[n];
    if (nd.sample <= 0) { stopVoice(v); return 'stopped'; }
    v.group = nd.group; v.active = true; st.started = true;
    const smp = samples[nd.sample - 1];
    if (!v.sync) { pushSeg(v, n, lastEnd(v, t), smp.durMs); v.nextAt = null; return 'queued'; }

    // Beat-synced slave voice (voice+0x2F).
    let fresh = false;
    const gsync = node(nd.group)?.sync ?? 0;
    if (v.slice <= 0) { // 3D3658: slice of the master's next beat within its measure
      const mb = masterBeat(t);
      let sl;
      if (!mb) sl = gsync === 0 ? 1 : 0;
      else if (gsync === 0x100) sl = mb.inMeasure + 1;
      else if (gsync === 0x200) sl = mb.inMeasure + 1; // (only 0x100 occurs in the retail data)
      else if (gsync === 0x300) sl = 1;
      else sl = 1;
      v.slice = (sl & 0x1f) > 0 ? sl & 0x1f : 1;
      fresh = true;
    }
    const m = master();
    const bankIndex = (smp.bankIndex ?? smp.value) + v.slice - 1;
    if (!m || m === v || !gsync) { // queued directly: whole sample, back to back
      pushSeg(v, n, lastEnd(v, t), smp.durMs, { slice: v.slice, bankIndex });
      v.nextAt = null; return 'queued';
    }
    if (v.startAt != null) { // +0x10C set: wait for it (3D41A8 returns -9999 until then)
      if (v.startAt > t + EPS) return 'defer';
      v.startAt = null;
    } else if (fresh && (gsync === 0x100 || gsync === 0x300)) {
      const mb = masterBeat(t);
      if (mb && mb.at > t + EPS) { v.startAt = mb.at; v.nextAt = null; return 'defer'; } // start on the master's beat
    }
    const per = nd.beats && nd.measures ? nd.beats * nd.measures : 1;
    pushSeg(v, n, t, smp.durMs / per, { slice: v.slice, bankIndex });
    const old = v.slice;
    v.slice = old < per && nd.sliced ? old + 1 : 0;
    const nb = masterBeat(t, true);
    v.nextAt = nb ? nb.at : t + smp.durMs / per; // +0x48: next master beat
    return 'queued';
  }

  // 3D3D20: the voice needs its next node.
  function advance(v, t) {
    const cur = v.node, cn = node(cur);
    const per = cn && cn.beats && cn.measures ? cn.beats * cn.measures : 1;
    if (v.sync && v.slice > 0 && cn && cn.sliced && v.slice <= per) return commit(v, cur, t);
    let next = cur >= 0 ? choose(v, cur, v.intensity) : -1;
    if (next >= 0) next = resolve(v, cur, next, v.intensity, t);
    const nn = node(next);
    if (nn && nn.track !== v.track && st.voices[nn.track]) { // 3D4050: hand the node to its own track's voice
      stopVoice(v);
      const other = st.voices[nn.track];
      emit({ type: 'handover', from: v.track, to: nn.track, node: next, time: t });
      return commit(other, next, t);
    }
    return commit(v, next, t);
  }

  function voiceDue(v) {
    if (v.pendingJump || !v.active || v.node < 0 || v.paused) return Infinity;
    if (v.startAt != null) return Math.max(v.startAt, st.time);
    if (v.nextAt != null) return Math.max(v.nextAt, st.time);
    const live = v.queue.filter((s) => s.end > st.time + EPS);
    if (!live.length) return st.time;
    let d = live[live.length - 1].end - v.latency;
    if (live.length >= v.capacity) d = Math.max(d, live[live.length - v.capacity].end);
    return Math.max(d, st.time);
  }

  // ---- events ---------------------------------------------------------------------------------------------------
  function postEvent(e, t) {
    const ev = events[e & 0xff];
    if (!ev) return false;                                       // 3D16F0: -13, ignored
    if (st.events.length >= PATHFINDER.MAX_EVENTS) return false;
    st.events.push({ e: e & 0xff, idx: 0, post: t, last: t, wake: null,
      actions: ev.actions.map((a) => ({ ...a, deadline: a.timeout || 0, done: false })) });
    emit({ type: 'event', event: e & 0xff, time: t });
    return true;
  }

  // One action on one voice; returns true when complete (3CFC30 opcode table 0x495A30).
  function runOp(inst, a, v, t) {
    switch (a.op) {
      case 0x02: { // wait `word` ms after the previous action completed
        if (t - inst.last >= a.word - 1e-6) return true;
        inst.wake = inst.last + a.word; return false;
      }
      case 0x04: { // jump
        let target = a.value, sec = a.arg;
        if (target >= -31 && target <= -16) target = s16(st.regs[(-target) & 0xf]);
        if (sec <= -16 && target >= -31) sec = s16(st.regs[(-sec) & 0xf]);
        if (target < 0 || target > nodes.length) target = -1;
        else if (sec === -32) sec = sectionOf();
        if (sec >= 0) { // n-th part head (sample 0) of section `sec`
          let k = target, found = -1;
          for (let i = 0; i < nodes.length; i++) {
            if (nodes[i].section !== sec || nodes[i].sample !== 0) continue;
            if (--k === 0) { found = i; break; }
          }
          target = found;
        }
        if (a.flags & 1) cutVoice(v, t);
        const cur = v.node;
        const r = commit(v, resolve(v, cur, target, v.intensity, t), t);
        if (r === 'defer') { inst.wake = v.startAt; v.pendingJump = true; return false; }
        v.pendingJump = false;
        return true;
      }
      case 0x05: v.paused = !!(a.word & 1); emit({ type: 'pause', voice: v.track, on: v.paused, time: t }); return true;
      case 0x06: { // 3D6378 volume ramp: target byte 12, curve byte 13 & 0x7F, time u16 at +0xE
        const from = v.scale;
        v.scale = a.b12;
        const r = { time: t, from, to: a.b12, durMs: a.u16_14, curve: a.b13 & 0x7f };
        v.ramps.push(r); emit({ type: 'ramp', voice: v.track, ...r });
        return true;
      }
      case 0x09: v.scale = a.b12; emit({ type: 'scale', voice: v.track, scale: v.scale, time: t }); return true;
      case 0x0a: { // save register
        if (a.argU < 16) {
          let val = a.value;
          if (val === -33) val = s16(v.node & 0xffff);
          else if (val === -32) val = sectionOf();
          st.regs[a.argU] = val;
        }
        return true;
      }
      case 0x0c: postEvent(a.b12 & 0xff, t); return true;
      case 0x0d: case 0x0e: case 0x0f: // cancel events (3D2218 all / 3D21C0 one)
        if (a.b12 === -1) st.events = st.events.filter((x) => x === inst);
        else st.events = st.events.filter((x) => x === inst || x.e !== (a.b12 & 0xff));
        return true;
      default: // 0x01 sync conditions, 0x03 beat wait, 0x07/0x08/0x0B/0x10 stream parameters: not in retail data
        emit({ type: 'op', op: a.op, voice: v.track, word: a.word, time: t });
        return true;
    }
  }

  function processEvents(t) {
    for (const inst of [...st.events]) {
      if (!st.events.includes(inst)) continue;
      inst.wake = null;
      while (inst.idx < inst.actions.length) {
        const a = inst.actions[inst.idx];
        if (a.deadline) { if (a.deadline < inst.post) a.deadline += t; if (a.deadline < t) a.done = true; }
        if (!a.done) {
          let complete = false, any = false;
          for (let tr = 0; tr < 24; tr++) {
            if (!(a.mask & (1 << tr))) continue;
            const v = st.voices[tr]; if (!v) continue;
            any = true;
            if (runOp(inst, a, v, t)) complete = true;
          }
          if (!any) { inst.wake = null; break; } // no such voice: the action never completes (as on the PS2)
          if (!complete) break;
          a.done = true;
        }
        inst.idx++; inst.last = t;
      }
      if (inst.idx >= inst.actions.length) st.events = st.events.filter((x) => x !== inst);
    }
  }

  function sectionOf() { const m = master(); return m && m.group >= 0 ? nodes[m.group].section : 0; }

  function nextDue() {
    let d = Infinity;
    for (const v of st.voices) d = Math.min(d, voiceDue(v));
    for (const inst of st.events) if (inst.wake != null) d = Math.min(d, Math.max(inst.wake, st.time));
    return d;
  }

  function advanceTo(T) {
    for (let guard = 0; guard < 100000; guard++) {
      const d = nextDue();
      if (!(d <= T)) break;
      st.time = Math.max(st.time, d);
      const t = st.time;
      if (st.events.some((i) => i.wake != null && i.wake <= t + 1e-6)) processEvents(t); // 3D1E80 first
      for (const v of st.voices) {
        if (voiceDue(v) <= t + 1e-6) { v.nextAt = null; advance(v, t); }
      }
    }
    st.time = Math.max(st.time, T);
    for (const v of st.voices) if (v.queue.length > 24) v.queue = v.queue.filter((s) => s.end > st.time - 10000);
  }

  return {
    graph: g, outputs,
    get time() { return st.time; },
    get state() { return st; },
    voice: (t) => st.voices[t] || null,
    // SendEvent(e) at song time t (>= core time). Returns false for events outside the table.
    sendEvent(e, t = st.time) { advanceTo(t); const ok = postEvent(e, t); processEvents(t); return ok; },
    // 3D0D70: branch value for every voice (values >= 128 are ignored).
    setIntensity(value, t = st.time) {
      advanceTo(t);
      if (!(value >= 0 && value < 128)) return;
      for (const v of st.voices) { v.prevIntensity = v.intensity; v.intensity = value | 0; }
    },
    setArmed(on) { st.armed = !!on; },
    advanceTo, nextDue, master, masterBeat,
    // 3D11B8 == -1: nothing committed and nothing left playing on the stream voice.
    finished(t = st.time) { const v = st.voices[0]; return st.started && !v.active && !v.queue.some((s) => s.end > t + EPS); },
    playing(track, t = st.time) { const v = st.voices[track]; return v ? segAt(v, t) : null; },
    snapshot() { return structuredClone(st); },
    restore(s) { st = structuredClone(s); },
    trimRng() { const drop = st.rngIdx - rngBase - 64; if (drop > 256) { rngLog = rngLog.slice(drop); rngBase += drop; } },
  };
}

// ---------------------------------------------------------------------------------------------------------------
// Pure stepper for tests: start(event) then step() returns the next committed segment of a track.
// ---------------------------------------------------------------------------------------------------------------
export function createGraphWalker(song, random = Math.random, { intensity = 0 } = {}) {
  const core = createPathfinderCore(song, { random, intensity });
  let read = 0;
  const take = (track) => {
    while (read < core.outputs.length) {
      const o = core.outputs[read++];
      if (o.type === 'segment' && o.voice === track) return o;
    }
    return null;
  };
  return {
    core,
    get time() { return core.time; },
    start(event = 0, t = 0) { return core.sendEvent(event, t); },
    event(e, t = core.time) { return core.sendEvent(e, t); },
    setIntensity(v, t = core.time) { core.setIntensity(v, t); },
    // Next committed segment of `track` (advancing song time to its commit), or null when the graph ended.
    step(track = 0, limitMs = 3600000) {
      for (;;) {
        const s = take(track); if (s) return s;
        const d = core.nextDue();
        if (!(d <= limitMs)) return null;
        core.advanceTo(d);
      }
    },
  };
}

// ---------------------------------------------------------------------------------------------------------------
// Web Audio player
// ---------------------------------------------------------------------------------------------------------------

// Ramp curve of 3D5A98 (op 0x06): f(t in 0..1) -> level between from and to (0..127 domain).
export function rampLevel(from, to, t, curve) {
  if (t >= 1) return to;
  const d = to - from, rising = d >= 0, tp = rising ? 1 - t : t;
  let f;
  if (curve === 1) f = 1 - tp;
  else if (curve === 2) f = 1 - tp * tp;
  else if (curve === 3) f = 1 / tp / 25;
  else return from;
  if (!(f >= 0 && f <= 1)) return from;
  return Math.min(from, to) + Math.abs(d) * f;
}

// Volume model (2AF6C0 is gated by audio+0x6280, set only for the BIG Mountain ambience, radio mode 2, 28D164):
//   music (default)  the stream level is the game's per-frame write (2B21E0: clamp(pathVol * PathLevel/100)) and
//                    each loop slice latches loops-level% x (voice+0x34)/127 when it starts (3D41A8 -> 2AE7A8);
//                    event volume ramps (op 0x06) and continuous loop-level changes are not applied.
//   ambience: true   op 0x06 ramps and loops.setLevel act continuously on the voices (Peak*Amb fades).
//   loopDestination  (music mode) the overlay track's own output. On the PS2 only the stream voice gets the MUSIC channel
//                    (2B21E0 writes 3B7E70 on 2B2488's stream alone); an overlay entry plays at the byte 3D41A8 latches,
//                    trunc(voice+4 % x 0.01 (double 0x495B98) x voice+0x34), then the SND master. So the caller passes the
//                    master here, not the MUSIC bus, and a slice gain is that byte / 127.
//   clockRandom      random part heads take (now_ms / 23) & 0x7F of the audio clock (3D3B08; 2AF370 = ms since boot),
//                    not `random`.
export function createPathfinderPlayer({ context, destination, song, random = Math.random, intensity = 0,
  lookahead = 1.0, interval = 0.05, autoPump = true, cacheSize = 48, ambience = false, loopDestination = null, clockRandom = false,
  declickMs = 0, eventFirst = false } = {}) {
  let clockMs = 0; // audio-clock ms at song time 0 (moves on with a pause, like the EE clock)
  // declickMs (pv audioDeclick, docs/audio-logic.md 9.13): a source that has to stop or start mid-waveform (pause, resume, an
  // event's cut, Stop, a late bar) ramps over this many ms instead of jumping to / from 0 (a click). The PS2's pause is pitch 0
  // (2B2018: the stream holds its sample, no step) and its resume continues from it; the song timing is unchanged.
  const DK = Math.max(0, +declickMs || 0) / 1000;
  const core = createPathfinderCore(song, { random, intensity, ...(clockRandom ? { clock: (t) => clockMs + t } : {}) });
  const inf = song.inf || {};
  const pathLevelPct = inf.PathLevel ?? 100;
  const ctx = context;
  const out = ctx.createGain(); out.connect(destination);
  const byteLoops = !!loopDestination && !ambience;
  const loopOut = byteLoops ? ctx.createGain() : null; if (loopOut) loopOut.connect(loopDestination);
  const chains = core.state.voices.map((v, i) => {
    const ramp = ctx.createGain(), level = ctx.createGain();
    ramp.connect(level); level.connect(i === 1 && loopOut ? loopOut : out);
    return { ramp, level, rampsApplied: 0 };
  });
  let lowpass = null, lowpassValue = 0xffff;
  const letGoChains = () => { for (const ch of chains) for (const n of [ch.ramp, ch.level]) try { n.disconnect(); } catch {} if (lowpass) try { lowpass.disconnect(); } catch {} }; // (a stopped song's gains)
  let base = 0, started = false, paused = false, pausedAt = 0, stopped = false, timer = null, fading = false;
  let lastEvent = -1, pending = null, snap = null, level = 127, loopsPct = 100, requested = false;
  const scheduled = new Map(); // seg id -> { src, seg, stopAt }
  const waiting = new Map();   // seg id -> seg: committed, not started yet (no audio yet; counted when it ends unplayed)
  const cache = new Map();
  let resuming = false, pumpAt = 0; // field counters: a resume's offset starts are not late; pump intervals while visible
  const resumed = new Set();       // (declick) voices whose first audio after a resume has started (only that one fades in)

  const songNow = () => (paused ? pausedAt : Math.max(0, (ctx.currentTime - base) * 1000));
  const ctxTime = (ms) => base + ms / 1000;

  function buffer(seg, now) {
    const key = seg.kind === 'stream' ? `s${seg.sample}` : `b${seg.bankIndex}`;
    let buf = cache.get(key);
    if (buf) { cache.delete(key); cache.set(key, buf); return buf; }
    const t0 = performance.now();
    const dec = song.decodeSegment(seg, { aheadMs: seg.start - now });
    if (!dec || !dec.length) return null;
    buf = toAudioBuffer(ctx, dec, dec.channels === 6 ? [[0, 2, 4], [1, 3, 5]] : null);
    noteDecode(performance.now() - t0, 'music');
    cache.set(key, buf);
    while (cache.size > cacheSize) cache.delete(cache.keys().next().value);
    return buf;
  }

  function applyLevels() {
    const t = ctx.currentTime;
    const main = Math.min(Math.max(level * pathLevelPct / 100, 0), 127) / 127;
    // (declick: a level write moves there over DK: the big-air duck's per-frame writes and its jump back at the landing, 20 -> 127,
    // no longer step the playing waveform)
    const put = (g, v) => { if (!DK || !started) { g.setValueAtTime(v, t); return; } if (g.cancelAndHoldAtTime) g.cancelAndHoldAtTime(t); else { g.cancelScheduledValues(t); g.setValueAtTime(g.value, t); } g.linearRampToValueAtTime(v, t + DK); };
    put(chains[0].level.gain, main);
    if (!chains[1]) return;
    put(chains[1].level.gain, ambience ? loopsPct / 100 : 1);
    if (ambience) return;
    const now = songNow(); // slices that have not started yet latch the new level when they do
    for (const rec of scheduled.values()) if (rec.gain && rec.seg.start > now) rec.gain.gain.setValueAtTime(sliceGain(rec.seg), t);
  }
  const sliceGain = (seg) => (byteLoops ? Math.max(0, Math.trunc(loopsPct * 0.01 * (seg.scale ?? 127))) / 127 // 3D41A8
    : (loopsPct / 100) * Math.max(0, seg.scale ?? 127) / 127);

  function applyRamps() {
    if (!ambience) return;
    core.state.voices.forEach((v, i) => {
      const ch = chains[i]; if (!ch) return;
      if (!v.ramps.length) { ch.rampsApplied = 0; return; }
      if (v.ramps.length === ch.rampsApplied) return;
      ch.rampsApplied = v.ramps.length;
      const r = v.ramps[v.ramps.length - 1];
      const g = ch.ramp.gain, t0 = Math.max(ctx.currentTime, ctxTime(r.time));
      g.cancelScheduledValues(t0);
      if (!(r.durMs > 0) || r.curve === 0) { g.setValueAtTime(r.to / 127, ctxTime(r.time + (r.durMs || 0))); return; }
      const n = 64, curve = new Float32Array(n);
      for (let k = 0; k < n; k++) curve[k] = Math.max(0, rampLevel(r.from, r.to, k / (n - 1), r.curve) / 127);
      try { g.setValueCurveAtTime(curve, t0, r.durMs / 1000); } catch { g.setValueAtTime(r.to / 127, t0); }
    });
  }

  // shift (startDue): an overlay entry (the stinger) an input made due now that the clock has already passed by at most SHIFT_MAX_S
  // (a render quantum or two: the clock moved on while the input was processed) starts whole at the current time, its attack kept.
  function startSource(seg, now, { shift = false } = {}) {
    const buf = buffer(seg, now); if (!buf) return null;
    const src = ctx.createBufferSource(); src.buffer = buf;
    let gain = null;
    const fade = DK ? ctx.createGain() : null, head = fade ?? src; if (fade) src.connect(fade); // declick gain (pv audioDeclick)
    if (seg.voice === 1 && !ambience) {
      gain = ctx.createGain(); gain.gain.value = sliceGain(seg);
      head.connect(gain); gain.connect(chains[1].ramp);
    } else head.connect(chains[seg.voice].ramp);
    const when = ctxTime(seg.start), t = ctx.currentTime;
    let at = when;
    if (when >= t) { // (a resume right at a bar line: that bar starts from silence mid-song, so it fades in too)
      src.start(when);
      if (fade && resuming && !resumed.has(seg.voice) && when - t < 0.05) { fade.gain.setValueAtTime(0, when); fade.gain.linearRampToValueAtTime(1, when + DK); }
      if (resuming) resumed.add(seg.voice);
    } else if (shift && seg.kind === 'bank' && t - when <= SHIFT_MAX_S) { src.start(t); at = t; } // (an overlay entry: a stream bar keeps its offset, the bar after it continues its waveform)
    else {
      const off = t - when; if (off >= buf.duration) { letGo({ src, fade, gain }); return null; }
      src.start(t, off); at = t; if (!resuming) noteMusicLate(off * 1000); else resumed.add(seg.voice);
      if (fade) { fade.gain.setValueAtTime(0, t); fade.gain.linearRampToValueAtTime(1, t + DK); } // mid-waveform start: fade in
    }
    const rec = { src, gain, fade, at, seg: { ...seg }, stopAt: null };
    src.onended = () => letGo(rec); // (a bar that has played, been cut or stopped lets go of its nodes)
    if (seg.kind === 'stream' && seg.end < seg.start + buf.duration * 1000 - 1) { rec.stopAt = seg.end; src.stop(ctxTime(seg.end)); }
    return rec;
  }
  // Stop a source at ctx time `at`. With the declick, one that sounds by then fades out over DK, ending at `at` when there is time
  // for it, else right after now (a pause, an event's cut at the current time).
  function halt(rec, at) {
    const t = ctx.currentTime; at = Math.max(t, at);
    if (!rec.fade || !(rec.at < at - 1e-4)) { try { rec.src.stop(at); } catch {} return false; }
    const r0 = Math.max(t, at - DK), g = rec.fade.gain;
    if (g.cancelAndHoldAtTime) g.cancelAndHoldAtTime(r0); else { g.cancelScheduledValues(r0); g.setValueAtTime(1, r0); }
    g.linearRampToValueAtTime(0, r0 + DK);
    try { rec.src.stop(r0 + DK); } catch {}
    return true;
  }
  const kill = (rec, atMs) => { const faded = halt(rec, ctxTime(atMs)); if (!faded && atMs <= songNow() + 1) letGo(rec); };
  // A source's whole chain (source, declick gain, slice gain) disconnected: connected nodes stayed alive for the session (and in WebKit
  // kept being processed). Called when it ends, and at once for one stopped now (a source cut off the graph may never fire ended).
  function letGo(rec) { for (const n of [rec.src, rec.fade, rec.gain]) if (n) try { n.disconnect(); } catch {} }

  // Reconcile Web Audio sources with the segments the core has committed (desired state).
  function reconcile(now) {
    const want = new Map();
    for (const v of core.state.voices) for (const s of v.queue) if (s.end > now + EPS) want.set(s.id, s);
    for (const [id, rec] of scheduled) {
      const s = want.get(id);
      if (!s && rec.seg.end <= now + EPS) { scheduled.delete(id); continue; } // finished normally
      if (!s || s.start !== rec.seg.start || s.bankIndex !== rec.seg.bankIndex || s.sample !== rec.seg.sample) {
        kill(rec, Math.max(now, rec.seg.start)); scheduled.delete(id); continue;
      }
      if (s.end < rec.seg.end - EPS && (rec.stopAt == null || s.end < rec.stopAt)) { // cut
        halt(rec, ctxTime(s.end));
        rec.stopAt = s.end; rec.seg.end = s.end;
      }
    }
    for (const [id, s] of waiting) if (!want.has(id)) { waiting.delete(id); if (s.end <= now + EPS && s.start < now) audioStats.musicMissed++; } // ended unplayed
    for (const [id, s] of want) {
      if (scheduled.has(id)) continue;
      if (s.voice === 1 && !core.state.armed) continue;
      const rec = startSource(s, now);
      if (rec) { scheduled.set(id, rec); waiting.delete(id); player.onEvent?.({ type: 'segment', ...s }); } else waiting.set(id, s);
    }
    for (const [id, rec] of scheduled) if (rec.seg.end < now - 2000) scheduled.delete(id);
  }

  // Deterministic look-ahead: decisions up to `now` are final (snapshot), later ones are provisional and are
  // recomputed (same random draws) whenever an input arrives, so late events still act exactly at their time.
  // eventFirst (pv musicLookahead): the audio an input makes due at once (a Big Challenge's stinger) starts right after the input,
  // before the look-ahead re-simulation and the reconcile (a few ms more with 2.5 s ahead: it started 2.7-5.3 ms late, skipped).
  function startDue(now) {
    for (const v of core.state.voices) for (const s of v.queue) {
      if (s.start > now + EPS || s.end <= now + EPS || (s.voice === 1 && !core.state.armed)) continue;
      const old = scheduled.get(s.id); // (ids of provisional segments are reused by a re-simulation: as in reconcile)
      if (old) { if (old.seg.start === s.start && old.seg.sample === s.sample && old.seg.bankIndex === s.bankIndex) continue; kill(old, Math.max(now, old.seg.start)); scheduled.delete(s.id); }
      const rec = startSource(s, now, { shift: true }); if (rec) { scheduled.set(s.id, rec); waiting.delete(s.id); player.onEvent?.({ type: 'segment', ...s }); }
    }
  }
  function simulate(now, input) {
    core.outputs.length = 0;
    if (snap) core.restore(snap);
    core.advanceTo(now);
    if (input) { input(now); if (eventFirst && !paused) startDue(now); }
    snap = core.snapshot();
    core.trimRng();
    core.advanceTo(now + lookahead * 1000);
    if (paused) return; // inputs while paused only change the (frozen) song state; resume() schedules
    reconcile(now);
    applyRamps();
  }

  function pump() {
    if (!started || paused || stopped) return;
    const t = performance.now();
    if (pumpAt && ctx.state === 'running' && !globalThis.document?.hidden && t - pumpAt > audioStats.musicPumpMaxMs) audioStats.musicPumpMaxMs = Math.round(t - pumpAt);
    pumpAt = globalThis.document?.hidden || ctx.state !== 'running' ? 0 : t;
    simulate(songNow());
  }

  const player = {
    core, song, onEvent: null,
    get finished() { return started && core.finished(songNow()); },
    get position() {
      const now = songNow(), seg = core.playing(0, now);
      if (!seg) return { timeMs: now, node: -1, section: -1, beat: 0, beats: 0 };
      const nd = song.graph.nodes[seg.node], per = nd.beats && nd.measures ? nd.beats * nd.measures : 1;
      const beat = ((now - seg.start) / (seg.end - seg.start)) * per;
      return { timeMs: now, node: seg.node, section: seg.section, beat, beats: (seg.beat0 || 0) + beat };
    },
    get intensity() { return core.voice(0)?.intensity ?? 0; },
    start(event = 0, when = ctx.currentTime) {
      if (started || stopped) return;
      started = true; base = when; clockMs = Math.floor(base * 1000); applyLevels(); player.setLowpass(inf.LOWPASS ?? 0xffff);
      snap = null;
      // PlaySong's first event goes straight to 2B20C8 (no duplicate check; mgr+0x41C stays -1).
      simulate(songNow(), (t) => { core.sendEvent(event, t); if (pending != null) { core.sendEvent(pending, t); pending = null; } });
      if (autoPump) timer = setInterval(pump, interval * 1000);
    },
    // 2B3B88 (music vfunc +0x18): the event goes straight to 2B20C8, without the repeat filter and without
    // updating the last event (the big-air loops 7/8 and MusicTrigger 42 use it).
    send(e) {
      if (!started) { pending = e; return true; }
      if (stopped) return false;
      player.onEvent?.({ type: 'event', event: e, timeMs: songNow() });
      simulate(songNow(), (t) => core.sendEvent(e, t));
      return true;
    },
    event(e) { // 2B3BC0 SendEvent: ignored when equal to the previous one
      if (e === lastEvent) return false;
      lastEvent = e;
      if (!started) { pending = e; return true; }
      if (stopped) return false;
      player.onEvent?.({ type: 'event', event: e, timeMs: songNow() });
      simulate(songNow(), (t) => core.sendEvent(e, t));
      return true;
    },
    setIntensity(v) {
      v = Math.max(0, Math.min(127, v | 0));
      if (v === player.intensity) return;
      if (!started) { core.setIntensity(v, 0); return; }
      simulate(songNow(), (t) => core.setIntensity(v, t));
    },
    pause() { // 2B2018: pitch 0 on the stream and overlay pause -> position frozen
      if (!started || paused || stopped) return;
      pausedAt = songNow(); paused = true;
      for (const rec of scheduled.values()) kill(rec, pausedAt);
      scheduled.clear();
    },
    resume() { // 2B2070: pitch 0x1000
      if (!paused || stopped) return;
      base = ctx.currentTime - pausedAt / 1000; paused = false; clockMs = Math.floor(base * 1000);
      for (const ch of chains) ch.rampsApplied = 0;
      pumpAt = 0; resuming = true; resumed.clear(); try { pump(); } finally { resuming = false; }
    },
    fadeOut(seconds) { // 2B3D48 -> 3B7EB0: linear 0..127 ramp in 10 ms steps, then the song is stopped
      if (stopped) return;
      fading = true;
      const t = ctx.currentTime, g = out.gain;
      g.cancelScheduledValues(t); g.setValueAtTime(g.value, t); g.linearRampToValueAtTime(0, t + Math.max(0, seconds));
      setTimeout(() => player.stop(), Math.max(0, seconds) * 1000 + 50);
    },
    stop() {
      if (stopped) return;
      stopped = true;
      if (timer) clearInterval(timer);
      if (DK) { // declick: both outputs ramp to 0 over DK, then everything stops and lets go
        const t = ctx.currentTime;
        for (const g of [out.gain, loopOut?.gain]) { if (!g) continue; if (g.cancelAndHoldAtTime) g.cancelAndHoldAtTime(t); else { g.cancelScheduledValues(t); g.setValueAtTime(g.value, t); } g.linearRampToValueAtTime(0, t + DK); }
        const recs = [...scheduled.values()];
        for (const rec of recs) try { rec.src.stop(t + DK); } catch {}
        scheduled.clear();
        setTimeout(() => { try { out.disconnect(); } catch {} if (loopOut) try { loopOut.disconnect(); } catch {} for (const rec of recs) letGo(rec); letGoChains(); }, DK * 1000 + 60);
        return;
      }
      for (const rec of scheduled.values()) { try { rec.src.stop(); } catch {} letGo(rec); }
      scheduled.clear();
      try { out.disconnect(); } catch {}
      if (loopOut) try { loopOut.disconnect(); } catch {} // (FadeOut 2B2418 fades the stream voice only; Stop ends both)
      letGoChains();
    },
    setLevel(v) { // 2B3C98: path volume (0..127) -> clamp(v * PathLevel / 100) / 127 on the stream
      level = Math.max(0, Math.min(127, +v || 0));
      if (!fading) applyLevels();
    },
    // LOWPASS (3B80A0 -> 3C3178): cutoff normalised by the 36 kHz mixer's Nyquist; >= 18000 removes the filter,
    // which is the case for every retail song (30000..42000, 0xFFFF). Below that a 2nd-order low-pass at `value` Hz
    // stands in for the EE filter module (3CA8A0, response not recovered).
    setLowpass(value) {
      lowpassValue = value;
      const on = value < PATHFINDER.MIXER_RATE / 2;
      chains[0].level.disconnect();
      if (on) {
        lowpass ??= ctx.createBiquadFilter();
        lowpass.type = 'lowpass'; lowpass.frequency.value = Math.max(10, value); lowpass.Q.value = Math.SQRT1_2;
        chains[0].level.connect(lowpass); lowpass.connect(out);
      } else {
        if (lowpass) try { lowpass.disconnect(); } catch {}
        chains[0].level.connect(out);
      }
    },
    get lowpass() { return lowpassValue; },
    loops: {
      // 2B4620 attaches the resident loop bank to the track-1 voice (3D2DF8); 2B4708 clears the request flag.
      // Audible loops are driven by the song graph: the game sends event 7 right after start() (loops on) and
      // event 8 on landing (loops off, flushed); see docs/audio-logic.md section 8.
      start() { requested = true; if (!core.state.armed) { core.setArmed(true); if (snap) snap.armed = true; pump(); } },
      stop() { requested = false; }, // 2B4708 only clears the request flag (song+0x4C); event 8 stops the loops
      get requested() { return requested; },
      setLevel(v) { loopsPct = Math.max(0, Math.min(100, +v || 0)); applyLevels(); }, // 2B3D10 -> 3D5508, 0..100
      get armed() { return core.state.armed; },
    },
    pump,
    // The overlay (track 1) entries these events can jump to, decoded into the buffer cache ahead, one per task: the Big Challenge
    // stingers of 33 / 34 / 38 (game-audio.js challengeAccepted, pv audioDeclick). The event's stinger then starts at the event, not
    // after its decode on the main thread (5 ms of its attack skipped at 4x CPU). The stream bars they jump to are fetched too (a
    // part due at the cut started 100 ms late at 4x CPU). Returns how many overlay entries it warms.
    warmEvents(list) {
      const g = song.graph, want = new Map(), bars = new Map();
      for (const e of list) for (const a of g.events[e]?.actions ?? []) {
        if (a.op !== 0x04 || !(a.mask & 3)) continue;
        let target = a.value; const sec = a.arg;
        if (target < 0 || target > g.nodes.length) continue;
        if (sec >= 0) { let k = target, found = -1; for (let i = 0; i < g.nodes.length; i++) { if (g.nodes[i].section !== sec || g.nodes[i].sample !== 0) continue; if (--k === 0) { found = i; break; } } target = found; }
        if (!(target >= 0)) continue;
        for (const n of g.nodes[target]?.sample > 0 ? [target] : nextAudioNodes(g, target, 1)) {
          const smp = g.samples[g.nodes[n].sample - 1];
          if (smp?.kind === 'bank') { const bankIndex = smp.bankIndex ?? smp.value; want.set(bankIndex, { kind: 'bank', voice: 1, sample: g.nodes[n].sample, bankIndex }); }
          else if (smp?.kind === 'stream') bars.set(g.nodes[n].sample, { kind: 'stream', voice: 0, sample: g.nodes[n].sample, node: n });
        }
      }
      // the stream parts they jump to (a part right after the cut in some songs, the 1200 ms one): downloaded, and worker-decoded with musicWorkerDecode
      if (bars.size) song.prefetch?.([...bars.values()])?.catch?.(() => {});
      const list2 = [...want.values()]; let i = 0;
      const step = () => { if (stopped || i >= list2.length) return; try { buffer(list2[i++], 0); } catch {} setTimeout(step, 0); };
      step();
      return list2.length;
    },
  };
  return player;
}

// ---------------------------------------------------------------------------------------------------------------
// Offline render (node or worker): script = [{ t: ms, event } | { t, intensity } | { t, level } (0..127 path
// volume) | { t, loopsPct } (0..100) | { t, loopsArmed }]. Returns
// { sampleRate, channels: 2, length, data: [L, R], segments }.
// ---------------------------------------------------------------------------------------------------------------
export function renderSongOffline(song, { script = [{ t: 0, event: 0 }], durationMs = 60000, sampleRate = 32000,
  random = Math.random, intensity = 0, intensityRamp = null, loopsPct = 100, level = 127, loopsArmed = true,
  ambience = false } = {}) {
  const core = createPathfinderCore(song, { random, intensity });
  core.setArmed(loopsArmed);
  const steps = [...script].sort((a, b) => a.t - b.t);
  if (intensityRamp) { // { tier, fromMs, fps }: 28F000 race ramp
    const fps = intensityRamp.fps ?? 60, from = intensityRamp.fromMs ?? 0;
    let last = -1;
    for (let f = 0; from + (f * 1000) / fps < durationMs; f++) {
      const v = raceIntensity(f, intensityRamp.tier ?? 0);
      if (v !== last) { steps.push({ t: from + (f * 1000) / fps, intensity: v }); last = v; }
      if (v === 127) break;
    }
    steps.sort((a, b) => a.t - b.t);
  }
  for (const s of steps) {
    if (s.t > durationMs) break;
    if (s.event != null) core.sendEvent(s.event, s.t);
    if (s.intensity != null) core.setIntensity(s.intensity, s.t);
    if (s.loopsArmed != null) core.setArmed(s.loopsArmed);
  }
  core.advanceTo(durationMs);
  const segs = core.outputs.filter((o) => o.type === 'segment');
  // Final ends (cuts shorten queued segments in place): read them back from the voices' queues where present.
  const live = new Map();
  for (const v of core.state.voices) for (const s of v.queue) live.set(s.id, s);
  const cuts = core.outputs.filter((o) => o.type === 'cut');
  const n = Math.ceil((durationMs * sampleRate) / 1000);
  const L = new Float32Array(n), R = new Float32Array(n);
  const pathLevel = song.inf?.PathLevel ?? 100;
  const env = (key, initial, map) => { const e = [[0, map(initial)]]; for (const x of steps) if (x[key] != null) e.push([x.t, map(x[key])]); return e; };
  const envs = [env('level', level, (v) => Math.min(Math.max(v, 0) * pathLevel / 100, 127) / 127),
    env('loopsPct', loopsPct, (v) => Math.min(Math.max(v, 0), 100) / 100)];
  for (const s0 of segs) {
    const s = live.get(s0.id) || s0;
    let end = s.end;
    for (const c of cuts) if (c.voice === s.voice && c.time > s.start - EPS && c.time < end) end = c.time;
    if (s.start >= durationMs || end <= s.start + EPS) continue;
    if (s.voice === 1 && !core.state.armed) continue;
    const dec = song.decodeSegment(s); if (!dec) continue;
    let e = envs[s.voice === 0 ? 0 : 1];
    if (s.voice === 1 && !ambience) { // music mode: the slice latches the level (and voice+0x34) when it starts
      let g0 = e[0][1]; for (const [t, v] of e) if (t <= s.start + EPS) g0 = v;
      e = [[0, g0 * Math.max(0, s.scale ?? 127) / 127]];
    }
    let ei = 0, nextAt = e.length > 1 ? (e[1][0] * sampleRate) / 1000 : Infinity, gain = e[0][1];
    const ratio = dec.sampleRate / sampleRate;
    const first = Math.round((s.start * sampleRate) / 1000);
    const stop = s.kind === 'stream' ? Math.min(n, Math.round((end * sampleRate) / 1000)) : n;
    const l = dec.data[0], r = dec.channels === 6 ? null : dec.data[Math.min(1, dec.channels - 1)];
    for (let i = Math.max(0, first); i < stop; i++) {
      while (i >= nextAt) { ei++; gain = e[ei][1]; nextAt = ei + 1 < e.length ? (e[ei + 1][0] * sampleRate) / 1000 : Infinity; }
      const p = (i - first) * ratio, k = Math.floor(p);
      if (k + 1 >= dec.length) break;
      const fr = p - k;
      let a, b;
      if (dec.channels === 6) {
        a = 0; b = 0;
        for (const c of [0, 2, 4]) a += dec.data[c][k] + (dec.data[c][k + 1] - dec.data[c][k]) * fr;
        for (const c of [1, 3, 5]) b += dec.data[c][k] + (dec.data[c][k + 1] - dec.data[c][k]) * fr;
      } else {
        a = l[k] + (l[k + 1] - l[k]) * fr; b = r[k] + (r[k + 1] - r[k]) * fr;
      }
      L[i] += a * gain; R[i] += b * gain;
    }
  }
  return { sampleRate, channels: 2, length: n, data: [L, R], segments: segs.map((s) => live.get(s.id) || s), outputs: core.outputs };
}
