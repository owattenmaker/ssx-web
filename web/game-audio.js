// Game audio director: the original's music flow, song choice, radio modes, speech (DJ Atomika, PA announcer,
// riders, arcade), sound effects, crowd, world sounds and the mixer, driven by front-end/race hooks from main.js and
// ui.js. Behaviour and addresses: docs/audio-logic.md; formats: docs/audio-formats.md; song graphs: web/pathfinder.js.
//
// Modules: web/audio-engine.js (mixer), web/pathfinder.js (music), web/audio-speech.js (+ audio-speech-events.js),
// web/sfx.js (voices), web/sfx-game.js (gameplay dispatchers), web/audio-crowd.js, web/audio-world.js,
// web/audio-painters.js (MusicTrigger / Mix / Ambience painters).
import { createAudioEngine, installAudioUnlock, SLIDER_DEFAULT } from './audio-engine.js';
import { decodeBankPatch, toAudioBuffer } from './audio-decode.js';
import { loadSong, createPathfinderPlayer, prefetchSongStart, streamSongStart, raceIntensity } from './pathfinder.js';
import { createSfx, SLOT } from './sfx.js';
import { createSpeech, charId, peakMask, courseBit, CHAR_ID, EV } from './audio-speech.js';
import { createCrowd, crowdConfig } from './audio-crowd.js';
import { createWorldAudio } from './audio-world.js';
import { queryAudioPainters } from './audio-painters.js';
import { createSfxGame, gruntIndex, relationOf } from './sfx-game.js';
import { pv } from './pv-flags.js';
import { avalancheState, takeLoopEvents } from './avalanche-state.js';

const ROOT = '/assets/AUDIO/';
// 28F140: charsel (front-end theme) event per front-end screen (table 0x482A70). Screens not listed keep the current one.
const FE_EVENT = Object.freeze({ title: 0, main: 1, character: 1, setup: 1, details: 1, event: 1, options: 4, 'fe-music': 4, 'fe-playlist': 4, 'fe-sound': 4,
  'ctm-mcomm': 1, 'ctm-lodge': 1, 'ctm-equip': 2, 'ctm-gear': 2, 'ctm-attributes': 3, 'ctm-rewards': 6, 'ctm-trophies': 7, 'ctm-stats': 8 });
// 28D8A0 CurrentCategory: course index -> 0 Race, 1 SlopeStyle, 2 BigAir, 3 HalfPipe, 4 BackCountry.
// Location code -> course table index (tools/locations.py course_index).
export const COURSE_INDEX = Object.freeze({ ARA1: 0, BRA2: 1, CRA3: 2, DRA4: 3, ERA5: 4, ASS1: 5, DSS2: 6, ESS3: 7, ABA1: 8, CBA2: 9, EBA3: 10,
  BHP1: 11, CHP2: 12, EHP3: 13, ABC1: 14, DBC2: 15, EBC3: 16 });
export function courseCategory(course) { return course <= 4 ? 0 : course <= 7 ? 1 : course <= 10 ? 2 : course <= 13 ? 3 : 4; }
// 0x535C10 event kind by course (0 Race, 1 Slope Style, 2 Big Air, 3 Half Pipe, 4 Free Ride).
export const eventKind = (course) => (course <= 4 ? 0 : course <= 7 ? 1 : course <= 10 ? 2 : course <= 13 ? 3 : 4);
// 294F78 UI event -> bank 0 sound (table 0x482D60); 294F48 plays sound 0.
export const UI_SOUND = Object.freeze({ 0: 3, 9: 3, 1: 4, 10: 4, 2: 2, 11: 2, 3: 1, 12: 1, 4: 0xd, 13: 0xd, 7: 0x10, 8: 0x11, 14: 0x12, 15: 7 });
// 28CDF8: chartune event by the winner's CHARDB character (table 0x482A10).
export const CHARTUNE_EVENT = Object.freeze([10, 2, 8, 1, 5, 9, 3, 7, 4, 6]);
const SETTINGS_KEY = 'ssx3.audio';
// pv audioDeclick (docs/audio-logic.md 9.13): music and speech that stop or start mid-waveform ramp over 5 ms (no click); a song's
// first bar is scheduled 40 ms ahead (its decode no longer makes it start late, skipping its first milliseconds).
const DECLICK_MS = 5, SONG_LEAD_S = 0.04;
// pv musicLookahead: the music scheduler simulates and schedules 2.5 s ahead instead of 1 s, so a main-thread stall of up to ~3 s
// (a phone's lazy course load under the menus: 1.6 s) cannot make a bar late. Inputs still act at their arrival time (the player
// re-simulates from the last real-time snapshot), so events, intensity and the song timing are unchanged.
const MUSIC_LOOKAHEAD_S = 2.5;

// 28D488 PickNextSong over the FE songs (the first `count` ADDTOFE songs, playlist order): random start slot, then
// 1..11 steps of NextSong(useCategory) (2B42B8), then one more step (with, then without the category) if it
// repeated the previous song. rand15 = 15-bit random.
export function pickNextSong({ songs, inList, category, previous, rand15 }) {
  const count = songs.length; if (!count) return -1;
  const hasCategory = songs.some((s, i) => inList(i) && s.categories?.includes(category));
  const next = (index, useCategory) => {
    for (let n = 0; n < count; n++) { index = (index + 1) % count; if (inList(index) && (!useCategory || !hasCategory || songs[index].categories?.includes(category))) return index; }
    return index;
  };
  let index = Math.floor(rand15() * count / 0x7fff) % count;
  const steps = Math.floor(rand15() * 10 / 0x7fff) + 1;
  for (let i = 0; i < steps; i++) index = next(index, true);
  if (index === previous) { index = next(index, true); if (index === previous) index = next(index, false); }
  return index;
}

// Speech line choice kept for tools/tests (the game path uses the Events.evt interpreter).
export function pickLine(lines, keys, history, random = Math.random) {
  const matches = lines.filter((l) => !keys || keys.every((k, i) => k === undefined || l.fields[i] === k));
  if (!matches.length) return null;
  const avoid = new Set(history.slice(-Math.min(Math.floor(matches.length / 2), 10)));
  const fresh = matches.filter((l) => !avoid.has(l.index));
  const pool = fresh.length ? fresh : matches;
  return pool[Math.floor(random() * pool.length)];
}

function loadSettings() {
  const d = { music: SLIDER_DEFAULT, effects: SLIDER_DEFAULT, speech: SLIDER_DEFAULT, radioMode: 0, dj: true, arcadeAudio: true, playlist: new Array(35).fill(true) };
  try { const s = JSON.parse(globalThis.localStorage?.getItem(SETTINGS_KEY) ?? 'null'); if (s && typeof s === 'object') Object.assign(d, s); } catch {}
  if (!Array.isArray(d.playlist) || d.playlist.length !== 35) d.playlist = new Array(35).fill(true);
  return d;
}

export function createGameAudio({ fetchJson = (p) => fetch(p).then((r) => { if (!r.ok) throw new Error(`${p}: ${r.status}`); return r.json(); }),
  fetchBytes = (p) => fetch(p).then((r) => { if (!r.ok) throw new Error(`${p}: ${r.status}`); return r.arrayBuffer(); }).then((b) => new Uint8Array(b)),
  now = () => performance.now(),
  // pv musicStream (main.js): songs stream their .mus bar by bar (pathfinder.js createMusStream); busy() = the game's downloads run
  musicStream = null } = {}) {
  let catalog = null, engine = null, ready = null, sfx = null, speech = null, crowd = null, world = null, sfxGame = null;
  const cache = new Map(); // path -> promise
  const once = (key, make) => { if (!cache.has(key)) cache.set(key, make().catch((e) => { cache.delete(key); throw e; })); return cache.get(key); };
  const json = (p) => once('j:' + p, () => fetchJson(ROOT + p));
  const bytes = (p) => once('b:' + p, () => fetchBytes(ROOT + p));
  // One byte range of a game file (pv musicStream): {start, bytes}; a 200 (no range support) is the whole file from 0. With the
  // Range header it bypasses web/downloads.js: music is not load work, and is not counted by the load screens.
  const rangeBytes = (p, a, b, priority = 'high') => fetch(ROOT + p, { headers: { range: `bytes=${a}-${b - 1}` }, priority }).then(async (r) => {
    if (r.status !== 206 && !r.ok) throw new Error(`${p}: ${r.status}`);
    const start = r.status === 206 ? Number(/bytes (\d+)-/.exec(r.headers.get('content-range') || '')?.[1] ?? a) : 0;
    return { start, bytes: new Uint8Array(await r.arrayBuffer()) };
  });
  const rand15 = () => Math.floor(Math.random() * 0x8000);
  const settings = loadSettings();
  const nowPlaying = new Set();
  // Director event timeline (QA, web/test-audio-timeline.mjs): the calls the PS2 recordings log (docs/audio-logic.md 9.11),
  // [ms, kind, ...args]: pick (28D488), play (PlaySong), event (SendEvent), fade, stop, pause, resume, dj (28E548 kind),
  // request (28E100 kind), code (28E8C0), speech (2B1458 DJ / PA event), line (the banks of a started line), nowplaying
  // (28F478 HUD 5), fe (28F140 charsel event), loading, worldload, leave.
  const timeline = [];
  const tl = (kind, ...args) => { timeline.push([Math.round(now()), kind, ...args]); if (timeline.length > 800) timeline.shift(); };
  const state = { music: null, musicId: null, feEvent: null, previousSong: -1, songIndex: -1, goSeen: false, chartuneIndex: -1,
    paused: false, loading: null, course: 0, courseCode: null, singleEvent: true, inWorld: false, preview: false, ambienceMode: false,
    character: null, place: 0, winner: null, rounds: { current: 1, total: 1 }, replay: false,
    trigger: { a: -99, b: -99, zone: 0, zoneExit: 0, rearm: 1, latch: -1 }, pendingDj: {}, region: null, track: -1, humanCore: null, ticks: 0, boothFlag: 0,
    free: null, residentKey: '', startPhase: null };

  async function init() {
    catalog = await json('catalog.json');
    const mixes = catalog.mixes.map((m, index) => ({ index, timeMs: m.TIME ?? 0,
      levels: Object.fromEntries(['MUSIC', 'DJ', 'PA', 'CHARACTER', 'BOARD', 'COLLISION', 'AMBIENT', 'ARCADESFX', 'ARCADESPEECH'].map((b) => [b, (m[b] ?? 100) < 0 ? undefined : (m[b] ?? 100) / 100])) }));
    engine = createAudioEngine({ mixes, sliders: { music: settings.music, effects: settings.effects, speech: settings.speech }, djEnabled: settings.dj, radioMode: settings.radioMode, arcadeAudio: settings.arcadeAudio,
      interruptGate: pv('audioInterrupt'), declickMs: pv('audioDeclick') ? DECLICK_MS : 0 }); // docs/audio-logic.md 9.13
    installAudioUnlock(engine);
    sfx = createSfx({ engine, fetchJson: (p) => json(p), fetchBytes: (p) => bytes(p), startAfterDecode: pv('sfxStartAfterDecode') }); // docs/audio-logic.md 9.13
    speech = createSpeech({ engine, json, bytes, declickMs: pv('audioDeclick') ? DECLICK_MS : 0,
      onPost: (id, speaker) => { if (speaker === 0xa || speaker === 0xb) tl('speech', id.toString(16)); },
      onLines: (banks, speaker) => { if (speaker === 0xa || speaker === 0xb) tl('line', banks.join('+')); } });
    speech.init().catch((e) => console.warn('Speech events unavailable', e));
    speech.onIdle = onIdle;
    crowd = createCrowd({ sfx, bytes, config: crowdConfig(catalog.crowdInf ?? []) });
    world = createWorldAudio({ sfx, crowd, json, onSpecial: () => { // 2B6550 -> 2A34D0(audio, 0, 1, lst): listener rider's place
      if (!(state.course < 5 || eventKind(state.course) === 0)) return;
      if (speech.free) speech.riderPosition({ id: charId(state.character) }, state.place, true);
    } });
    sfxGame = createSfxGame({ engine, sfx, speech, crowd, world, music: musicAdapter, animationEvents: clipEvent });
    sfxGame.G.instancePosition = instancePosition;
    sfxGame.G.zoneActive = () => state.trigger.zone !== 0; // 28D898
    sfxGame.G.relation = (a, b) => relFrom(state.relTick, a, b);          // overtakes: records at the tick-start ranking
    sfxGame.G.pairRelation = (a, b) => state.relPair.get(`${a}:${b}`) ?? relFrom(state.relTick, a, b);
    // Requests made before the first user gesture only record what should play; start it once audio unlocks.
    engine.onUnlock(() => {
      if (state.musicId && !state.music) playSong(state.musicId, state.feEvent ?? 0, { ambience: state.ambienceMode }).catch((e) => console.warn('Music start failed', e));
      if (!state.inWorld) sfx.loadBank(SLOT.MAIN, 'SSX3Menu');
    });
    animationClipEvents().catch(() => {});
  }
  const whenReady = () => (ready ??= init());
  const feSongs = () => catalog.songs.filter((s) => s.ADDTOFE).slice(0, 35);
  const ctx = () => (engine?.unlocked ? engine.context : null);
  const musicMode = () => settings.radioMode !== 2; // 28D960
  // The human rider's radio mode and custom playlist from its profile record at a world load (api.riderMusic: web/audio-menu.js
  // worldMusic, pv riderMusic); the global settings file keeps the last ones applied.
  function riderMusic() {
    let m = null; try { m = api.riderMusic?.(); } catch {} if (!m) return;
    settings.playlist = m.playlist.slice(0, 35).map(Boolean);
    if (m.radioMode !== settings.radioMode) { settings.radioMode = m.radioMode; engine?.setRadioMode(settings.radioMode); }
    tl('rider-music', settings.radioMode, settings.playlist.flatMap((b, i) => (b ? [i] : [])).join(','));
  }
  // Playlist membership (modes 0: Radio BIG = PLAYLIST.INF [SSX Mix]; 1/3: the custom list, audio+0x6098).
  function inPlaylist(i) {
    if (settings.radioMode === 1 || settings.radioMode === 3) return settings.playlist.some(Boolean) ? !!settings.playlist[i] : true;
    const mix = catalog.playlist?.[0]?.songs; if (!mix) return true;
    const id = feSongs()[i]?.id; return mix.some((n) => n.toLowerCase() === String(id).toLowerCase());
  }

  // ---- relationships (2A1820): {scores: 155B50(a, b), kinds: 155AB0(a, b)} 6x6 from the computer riders' document
  // (web/lineup.js keeps them live through the in-race 0x155BF0 events).
  const relFrom = relationOf;
  const relSnapshot = (ai) => {
    const racers = ai?.racers; if (!racers?.relation) return null; // live records (web/ai-racers.js relation(a, b))
    const scores = [], kinds = [];
    for (let a = 0; a < 6; a++) { scores.push([]); kinds.push([]); for (let b = 0; b < 6; b++) { const r = racers.relation(a, b); scores[a].push(r.score); kinds[a].push(r.kind); } }
    return kinds.some((row, a) => row.some((k, b) => a !== b && k == null)) ? null : { scores, kinds };
  };
  state.relPair = new Map();
  function hookPairs(ai) {
    const racers = ai?.racers; if (!racers || racers.__audioPairHook) return;
    racers.__audioPairHook = true;
    // 10E228..10E468: the hit speech 2A0A30(A = other, B = target) runs before 155BF0 changes the records.
    racers.onPairAudio = (target, other) => { const v = relFrom(relSnapshot(ai), other, target); if (v != null) state.relPair.set(`${other}:${target}`, v); };
  }

  // ---- animation clip events (basic.afl ids with the 0x8000 flag, tools/export_animation_audio.py) ----
  let clipTable = null;
  async function animationClipEvents() { clipTable = await json('anim-events.json').catch(() => null); }
  function clipEvent(clip, bit) { const e = clipTable?.clips?.[String(clip >> 8)]; return e ? e[bit] ?? null : null; }
  function instancePosition(resource) {
    const c = state.humanCore; if (!c?._audio_instance_position) return null;
    const p = new Float32Array(c.HEAPF32.buffer, c._audio_instance_position(resource >>> 0), 3);
    return p[0] === 0 && p[1] === 0 && p[2] === 0 ? null : [p[0], p[1], p[2]];
  }

  // ---- music ------------------------------------------------------------------------------------------------
  async function playSong(id, event, { ambience = false, intensity = null } = {}) {
    tl('play', id, event ?? 0);
    const context = ctx(); stopMusic(); state.currentId = id;
    state.ambienceMode = ambience;
    if (!context) { state.musicId = id; state.feEvent = event; return null; } // not unlocked yet: remember what should be playing
    const token = {}; state.musicToken = token;
    const file = songFile(id); // e.g. 'Rock Star' -> Rock_Star
    const song = await loadSong(file, { fetchJson: (p) => json(p), fetchBytes: (p) => bytes(p), ...(musicStream ? { fetchRange: rangeBytes, busy: musicStream.busy } : {}),
      workerDecode: pv('musicWorkerDecode') }); // docs/audio-logic.md 9.13
    if (state.musicToken !== token) return null;
    // Intensity (branch selector): 127 for front-end/podium/hub songs; races ramp it from 0 (28F000, raceIntensity).
    const level = intensity ?? (state.raceMusic ? 0 : 127);
    // MicroTalk songs ("Screw Up"): the opening bar is worker-decoded before the start (pathfinder.js prefetchSongStart).
    const random = await (song.mus?.stream ? streamSongStart : prefetchSongStart)(song, event ?? 0, { intensity: level }).catch(() => null);   // streamed: its opening bars first
    if (state.musicToken !== token) return null;
    const player = createPathfinderPlayer({ context, destination: engine.bus(ambience ? 'AMBIENT' : 'MUSIC'), song, intensity: level, ambience, ...(random ? { random } : {}),
      ...(pv('bigChallengeAudio') ? { clockRandom: true, loopDestination: ambience ? null : engine.master } : {}), // docs/audio-logic.md 3.9
      ...(pv('audioDeclick') ? { declickMs: DECLICK_MS } : {}), ...(pv('musicLookahead') ? { lookahead: MUSIC_LOOKAHEAD_S, eventFirst: true } : {}) }); // docs/audio-logic.md 9.13
    state.intensityFrames = 0;
    state.music = player; state.musicId = id; state.song = song;
    player.start(event, pv('audioDeclick') ? context.currentTime + SONG_LEAD_S : undefined); // (a lead: the first bar starts on time, not mid-waveform)
    if (state.paused || state.countdownHold) player.pause(); // (countdownHold: 29C420(1)'s song waits for GO)
    const s = catalog.songs.find((x) => x.id === id);
    if (s?.ADDTOFE && state.inWorld && engine.busGain('MUSIC') > 0) { tl('nowplaying', s.TITLE); for (const f of nowPlaying) try { f({ title: s.TITLE, artist: s.ARTIST, album: s.ALBUM }); } catch {} } // 28F478
    return player;
  }
  const songFile = (id) => catalog.music?.find((m) => m.id === id)?.json?.replace(/^music\//, '').replace(/\.json$/, '') ?? id;
  // pv musicPrefetchNext (docs/audio-logic.md 9.13): a song picked for a later request (ChangeSong's 2 s, a fly-over's 3 s) starts
  // streaming its opening bars now, so the request finds them in (6 Mbit/s: 4.4-4.8 s of silence between the songs).
  function prefetchPicked(event = 0) {
    if (!pv('musicPrefetchNext') || !musicStream || !ctx()) return;
    const s = feSongs()[state.songIndex]; if (!s) return;
    loadSong(songFile(s.id), { fetchJson: (p) => json(p), fetchBytes: (p) => bytes(p), fetchRange: rangeBytes, busy: musicStream.busy, workerDecode: pv('musicWorkerDecode') })
      .then((song) => streamSongStart(song, event, { intensity: state.raceMusic ? 0 : 127 })).catch(() => {});
  }
  function stopMusic() { state.musicToken = null; state.music?.stop(); state.music = null; }
  function fadeOutMusic(seconds) { const m = state.music; state.music = null; state.musicToken = null; tl('fade', seconds); m?.fadeOut(seconds); }
  function sendEvent(e) { tl('event', e); state.music?.event(e); }
  // 28C8C8 hooks (web/sfx-game.js big air).
  const musicAdapter = {
    duckToLoops: () => (catalog?.songs.find((s) => s.id === state.musicId)?.DUCKTOLOOPS ?? 1) === 1 && musicMode() && !!state.music,
    streamLevel: (v) => state.music?.setLevel?.(v),
    loopsLevel: (v) => state.music?.loops?.setLevel(v),
    loopsStart: () => { if (!state.music?.loops) return false; state.music.loops.start(); return true; },
    loopsStop: () => state.music?.loops?.stop(),
    loopsRequested: () => !!state.music?.loops?.requested,
    event: (e) => { if (musicMode()) { tl('event', e); state.music?.send?.(e); } }, // forced send (2B3B88), not the SendEvent filter
    resetIntensity: () => { state.intensityFrames = 0; },
    // 29C420(digit) (pv ctmRestartAudio), with audio+0x627C set: "3" (0x29C6B4..0x29C704) fades a hub song (ids 1-3) or chartune
    // (0xC9) over 2 s; "1" (0x29C710..0x29C784), when no song is going (audio+0x530 clear: here, none or it has ended) or a hub song /
    // chartune plays, picks the next playlist song (28D488), plays it with event 0 (28CF98) and pauses it (2B3A70) for GO's resume.
    // PS2 music logs: results-restart-audio (round 1, the finished heat's song: pick / play 0 / pause at "1", resume + event 0 at
    // GO); pause-restart-audio2 and heat2-audio (a song going: nothing at the countdown).
    countdown: (d) => {
      if (!pv('ctmRestartAudio') || !musicMode() || !state.resumeGo || state.free || !ctx()) return; // (audio locked: nothing plays yet)
      const special = /^Peak[123]$/.test(state.musicId ?? '') || state.musicId === 'chartune';
      if (d === 3 && special && state.music) { fadeOutMusic(2); return; }
      if (d === 1 && (special || !state.music || state.music.finished) && !(state.musicToken && !state.music)) { // (not while a song loads)
        log('countdown 1: next song, paused'); state.countdownHold = true; pickSong(); playMusic(0);
      }
    },
  };
  // ---- audio director (SSXAudioSystem; docs/audio-logic.md 3.3, 3.7, 3.8, 4.3 and "Free ride / Peak 1") ----------
  // Game state: 0x535C08 course (state.course), 0x535C10 kind and 0x535C12 mode (free-ride worlds pass them to
  // runStart; race events derive the kind from the course).
  const gameKind = () => state.free?.kind ?? eventKind(state.course);
  const challenge = () => { const m = state.free?.mode ?? -1; return m >= 6 && m <= 11; }; // 2A42C8: time / points challenges
  const isHub = (c) => c >= 17 && c <= 21;          // 2A3FD8
  const isBackcountry = (c) => c >= 14 && c <= 16;  // 2A4030
  const djOn = () => settings.dj && (settings.radioMode === 0 || settings.radioMode === 1); // audio+0x62B8 (287558)
  const currentSed = () => catalog?.songs.find((x) => x.id === (state.currentId ?? state.musicId))?.SEDVALUE ?? -1; // 2B4908
  const NO_DEST = 23;
  const dir = {
    dest: NO_DEST,        // +0x6284 transport destination (28F558)
    lastDest: 0,          // +0x5814
    cinematic: 0,         // +0x6254 run 28E8C0(19) when the location cinematic ends (28E888)
    arrived: 0,           // +0x6258
    bcIntro: 0,           // +0x578C
    hubFirst: 1,          // +0x5790 (0 after a new career's intro: the first hub chatter is Char_Stories)
    hubForced: -1,        // +0x5798
    hubUsed: 0,           // +0x573C
    tmVariant: 1,         // +0x5780 Text_Message argument
    firstVisit: new Array(23).fill(0), // +0x579C[23] = !visited(location) at world load (145D38)
    visited: 0,           // session copy of the profile's visited mask (P+0xACC) when the UI does not supply one
  };
  const trace = [];
  const log = (t) => { trace.push(`${Math.round(now())} ${t}`); if (trace.length > 64) trace.shift(); };
  // Profile visited mask (P+0xACC): the career can supply it through context().visited / markVisited.
  const visited = (loc) => { const m = context().visited; return (((typeof m === 'number' ? m : dir.visited) >>> loc) & 1) === 1; };
  function markVisited(loc) { if (loc < 0 || loc > 22) return; dir.visited |= 1 << loc; try { api.context?.()?.markVisited?.(loc); } catch {} }

  // 2ADCA0 / 2ADDE0: the audio timer queue (ms). Keys name the callbacks: 'request' 28E088, 'dj' 28E548, 'retry' 28E068.
  // A callback returning false stays queued (retried on the next pump).
  const timers = [];
  function post(key, ms, fn) { timers.push({ key, at: now() + ms, fn }); }
  function cancel(key) { for (let i = timers.length; i-- > 0;) if (timers[i].key === key) timers.splice(i, 1); }
  function runTimers() {
    const t = now();
    for (let i = 0; i < timers.length;) {
      if (timers[i].at > t) { i++; continue; }
      const [e] = timers.splice(i, 1);
      if (e.fn() === false) { e.at = t + 1; timers.push(e); }
    }
  }
  // 28E088: a music request (kind 0 hub song, 1 PlayMusic(0), 2 transport arrival, 3 PlayMusic(36), 4 PlayMusic(0) in
  // any radio mode) is taken only when no song plays, the song ended, or `force`; 28E100 executes it.
  const request = (kind, force) => () => {
    if (!force && (state.music || state.musicToken) && !state.music?.finished) return false;
    executeRequest(kind); return true;
  };
  function executeRequest(kind) { // 28E100
    log(`request ${kind}`); tl('request', kind);
    if (kind === 0) { if (!musicMode()) return; const loc = dir.dest !== NO_DEST ? dir.dest : state.course; dir.dest = NO_DEST; playHubSong(peakMask(loc)); return; }
    if (kind === 2) {
      if (!musicMode() || dir.dest === NO_DEST) return;
      const d = dir.dest; dir.dest = NO_DEST;
      if (isBackcountry(d)) { pickSong(); playMusic(36, { forced: true }); } else playHubSong(peakMask(d));
      return;
    }
    if (kind === 4) { playMusic(0); return; }
    if (musicMode()) playMusic(kind === 3 ? 36 : 0);
  }
  // Hub songs Peak1 / Peak2 / Peak3 (ids 1-3, 28CF98 / 28E100): event 12, intensity 127 (2B3C28; 28F000 keeps it).
  const HUB_SONG = (peak) => (peak === 1 ? 'Peak1' : peak === 2 ? 'Peak2' : 'Peak3');
  function playHubSong(peak) { log(`hub song ${HUB_SONG(peak)}`); return playSong(HUB_SONG(peak), 12, { intensity: 127 }); }
  // 28D8A0 CurrentCategory: time challenges -> Race, points challenges -> SlopeStyle, else by course.
  const currentCategory = () => { const m = state.free?.mode ?? -1; return m >= 6 && m <= 8 ? 0 : m >= 9 && m <= 11 ? 1 : courseCategory(state.course); };
  // 28D488 PickNextSong: the playlist index PlayMusic will play.
  function pickSong(eventHint = 0) {
    const songs = feSongs();
    let index = pickNextSong({ songs, inList: inPlaylist, category: currentCategory(), previous: state.previousSong, rand15 });
    const forced = (() => { try { return new URL(globalThis.location?.href ?? '').searchParams.get('audioSong'); } catch { return null; } })(); // QA: level comparisons
    if (forced) { const k = songs.findIndex((x) => x.id.toLowerCase() === forced.toLowerCase()); if (k >= 0) index = k; }
    state.songIndex = index; tl('pick', songs[index]?.id ?? null);
    prefetchPicked(eventHint);
  }
  // 28CF98 PlayMusic(event, forced, songId, t0): radio mode 2 -> Peak<n>Amb (songId, else the course's peak) on AMBIENT;
  // a station (course >= 17) outside the time / points challenges, not forced -> the peak's hub song; otherwise the
  // current playlist index with `event` (36 when t0 at a station); the played index is the next pick's "previous".
  async function playMusic(event, { forced = false, songId = -1, t0 = false } = {}) {
    if (state.replay) return; // 288AE0 (+0x6294 is never set by the port)
    state.trigger.zone = 0; state.trigger.zoneExit = 0; // +0x623C / +0x6240
    if (!musicMode()) { const peak = songId > 0 ? songId : peakMask(state.course); log(`ambience Peak${peak === 1 ? 1 : peak === 2 ? 2 : 3}Amb`); await playSong(`Peak${peak === 1 ? 1 : peak === 2 ? 2 : 3}Amb`, 12, { ambience: true, intensity: 127 }); return; }
    if (state.course >= 17 && !forced && !t0 && !challenge()) { await playHubSong(peakMask(state.course)); return; }
    const songs = feSongs(); if (state.songIndex < 0) pickSong();
    state.previousSong = state.songIndex; // +0x62A4
    log(`song ${songs[state.songIndex].id} event ${t0 && state.course >= 17 ? 36 : event}`);
    await playSong(songs[state.songIndex].id, t0 && state.course >= 17 ? 36 : event);
  }
  // 28BF78 SetRadioMode in a world with a song active: 2 -> the peak ambience unless it plays; 0 -> a playlist song when
  // leaving the ambience; 1 / 3 (custom list, non-empty) -> a playlist song when the current one (not a hub song) is
  // not in the list (2B4AF0) or is charsel. PlayMusic event 0.
  function radioModeChanged() {
    const m = settings.radioMode, id = state.currentId ?? state.musicId ?? '', amb = /^Peak\dAmb$/.test(id);
    log(`radio mode ${m}`); tl('radio', m);
    if (m === 2) { if (!amb) { playMusic(0); if (pv('bigChallengeAudio')) { state.challengeMusic = false; state.challengeType = 0; } } return; } // 0x28C2B0: +0x5FD0 / +0x5FD4 = 0
    if (m === 0) { if (amb) { pickSong(); playMusic(0); } return; }
    if (!settings.playlist.some(Boolean) || /^Peak\d$/.test(id)) return;
    const i = feSongs().findIndex((x) => x.id === id);
    if (i < 0 || !inPlaylist(i) || id === 'charsel') { pickSong(); playMusic(0); }
  }
  // PickNextSong + PlayMusic (world load 2867E8, restarts, radio changes).
  async function playWorldMusic(event) { if (musicMode()) pickSong(); await playMusic(event); }
  // pv mountainAudio: the whole-mountain world (MOUNTAIN: the All Peak Race / Peak 2 Race, the CTM free ride on desktop) streams like
  // a peak world, so its world audio follows the streaming rows too (286CA8: a location's slot 8 / 9 banks when its data arrives).
  // Before: every location of world/MOUNTAIN.json counted as resident, the 44 location banks were decoded at the load (+130 MB of
  // PCM) and slots 8 / 9 held the last one's (B's) at the start.
  const isPeakWorld = (code) => /^PEAK\d$/.test(code ?? '') || (code === 'MOUNTAIN' && pv('mountainAudio'));
  // 2A4168 / 2A41D8 / 2A4030 on the current course (and kind): Big Air, Half Pipe, backcountry.
  const courseFlags = (c) => ({ bigAir: (c >= 8 && c <= 10) || gameKind() === 2, halfPipe: (c >= 11 && c <= 13) || gameKind() === 3, backcountry: isBackcountry(c) });
  // Entering the lodge (PS2 music/runs/lodge): 286A80 leaves the world (Resume if paused, Stop without a fade, timers and
  // speech cancelled, mix 0), then the front end's charsel (285FB0) with the lodge's event 1 (28F140). The browser keeps
  // the world loaded under the lodge; its sounds stay paused.
  function enterLodge() {
    state.lodge = true; tl('leave'); if (state.music || state.musicToken || state.musicId) tl('stop');
    state.paused = false; timers.length = 0; stopMusic(); state.musicId = null; speech?.stop(); state.pendingDj = {};
    world?.ambienceStop(0); sfxGame?.pauseStops(); engine?.setMix(0); state.raceMusic = false;
  }
  // Conquer-the-Mountain world load (2867E8 + 2A4A78) for the free-ride worlds. 234F40 (world state 10 enter) runs first:
  // a backcountry's first visit is its intro (+0x578C = 1, +0x6254 = 1, +0x5790 = 0) and Happiness starts pktrans for the
  // plane (28CD48: event 1, paused until state 10 resumes it). PS2 new career (scratchpad music/runs/newcareer): pktrans at
  // sample 573, world load at 594 with no PickNextSong / PlayMusic (pktrans is the song) and no 2A4A78 DJ timer (+0x578C).
  function introStart() {
    const c = state.course;
    if (state.free.kind === 4 && isBackcountry(c) && !visited(c)) {
      dir.bcIntro = 1; dir.hubFirst = 0; dir.cinematic = 1;
      if (c === 14) { log('pktrans (plane intro)'); playSong('pktrans', 1, { intensity: 127 }); }
      // Peaks 2 / 3: the PS2 travel to an unvisited backcountry starts pktrans with the destination peak's event (28E8C0(20):
      // 2 for Peak 2, else 3) and the heli arrival cut resumes it; the browser reaches them by a world load, so here.
      else if (c === 15 || c === 16) { log('pktrans (peak arrival)'); playSong('pktrans', peakMask(c) === 2 ? 2 : 3, { intensity: 127 }); }
      markVisited(c); // 145D38 then reads the location as visited (PS2 579C[14] = 0 at ctmstart.f02830)
      return true;
    }
    return false;
  }
  function worldLoadFree(intro = introStart()) {
    tl('worldload', state.course);
    for (let k = 0; k < 23; k++) dir.firstVisit[k] = visited(k) ? 0 : 1; // 145D38 per location
    if (!intro) markVisited(state.course);
    state.trigger.latch = -1; // 28DF08
    log(`world load ${state.courseCode} course ${state.course} kind ${state.free.kind} mode ${state.free.mode}`);
    if ((state.currentId ?? state.musicId) !== 'pktrans') playWorldMusic(0);
    if (dir.bcIntro) return; // 2A4A78
    if (isHub(state.course)) { latch(13); post('dj', 2, () => djTimer(2)); }
    else { const sed = currentSed(); post('dj', 2, () => djTimer(4, sed)); }
  }
  // Ride start: the location's arrival cinematic ends (28E888 -> 28E8C0(19)). An intro cinematic the page plays (the
  // Happiness plane, the heli drops; main.js ui.cb.cutscene kind 'arrival' -> arrivalCinematic) holds it until its end:
  // PS2 new career, cinematic end at sample 3248 (916 ticks of the plane), DJ kind 0 at +150 samples, Peak1 at +180.
  function startFree() {
    if (dir.cinematic && dir.hold) { dir.heldEnd = true; log('cinematic end held (arrival cinematic)'); return; }
    cinematicEnd();
  }

  // 28E548: the DJ timer callback (kind = timer data +0x1C, sed = +0x20).
  function djTimer(kind, sed = -1) {
    const p = state.pendingDj;
    log(`dj timer ${kind}${sed !== -1 ? ` sed ${sed}` : ''}`); tl('dj', kind);
    if (kind === 0) { // spoke arrival: Radio BIG intro, First_Spoke, on Peak 1 the text message (variant 2)
      speech.stop(); speech.radioBigIntro(1, 0); speech.flush();
      if (!challenge()) { p.firstSpoke = 1; if (peakMask(state.course) === 1) { p.textMessage = 1; dir.tmVariant = 2; } }
    } else if (kind === 1 || kind === 2) { // hub: Radio BIG intro, hub chatter
      speech.stop(); speech.radioBigIntro(kind === 2 ? 0 : 1, 1); speech.flush();
      if (!challenge()) p.hubChatter = 1;
    } else if (kind >= 3 && kind <= 5) { // song change (3), free-ride start at a location (4 first time, 5)
      if (sed === -1) return;
      if (kind !== 5) { speech.stop(); speech.radioBigIntro(kind === 4 ? 0 : 1, 0); speech.flush(); }
      const loc = state.course;
      // 146008(profile, 0, 1): profile option bit P+0x278 bit 12, set in every PS2 profile read (menus/fr savestates).
      if (!challenge() && peakMask(loc) === 1 && dir.firstVisit[loc] === 1) {
        if (loc < 5 || (loc >= 5 && loc <= 7)) p.freeRideIntro = 1; // 2A40E0 race / 2A4158 slope location
        dir.firstVisit[loc] = 0;
      }
      p.artist = 1; p.sed = sed;
    } else if (kind === 7) { // arriving in the backcountry
      speech.stop(); speech.radioBigIntro(1, 0); speech.flush();
      p.artist = 1; p.bcIntro = 1; p.sed = currentSed();
    }
  }
  // 28DEF0: MusicTrigger latch (+0x629C); true when it changed.
  function latch(id) { const T = state.trigger; if (T.latch === id) return false; T.latch = id; return true; }

  // World sounds at 60 Hz while a cutscene runs without game ticks: emitters and crowd loops at the listener (the NIS
  // camera), the location ambience of the course (29D290 focus: before the first contact, the event's own location).
  function nisWorldTick() {
    if (!world?.loaded) return;
    if (state.track < 0 && !state.free) {
      const doc = worldDoc(), loc = doc && Object.values(doc.locations).find((l) => l.locationId === state.course);
      if (loc) { state.track = loc.track; world.focusTrack(loc.track); }
    }
    world.update(state.listenerCm);
    crowd?.update(() => 3);
  }
  // 28E8C0(19, 1) via 28E888: the location's arrival cinematic ended (+0x6254 set by a transport or a backcountry intro).
  function cinematicEnd() {
    if (!dir.cinematic) return;
    dir.cinematic = 0; log('cinematic end (28E8C0 19)'); tl('code', 19);
    const c = state.course;
    if (isBackcountry(c) && !dir.bcIntro) { // arrival in the backcountry: DJ kind 4 (first) / 5, event 0
      const k = gameKind();
      if (musicMode() && k !== 5 && k !== 6) { const sed = currentSed(); post('dj', 0, () => djTimer(dir.arrived ? 5 : 4, sed)); sendEvent(0); }
      dir.arrived = 0; return;
    }
    if (dir.bcIntro) { // a new career's intro (Happiness): the hub theme after 3 s, the spoke DJ at 2.5 s
      if (musicMode()) {
        if (!djOn()) { tl('fade', 2); state.music?.fadeOut?.(2); }
        if (peakMask(c) === 1) { if (latch(11)) { post('request', 3000, request(0, true)); post('dj', 2500, () => djTimer(0)); } }
        else { state.trigger.latch = -1; pickSong(); post('request', 2000, request(1, true)); post('dj', 1500, () => djTimer(7)); }
      }
      dir.arrived = 0; return;
    }
    if (isHub(c) && !dir.arrived) { speech.radioBigIntro(0, 1); speech.flush(); state.pendingDj.hubChatter = 1; } // 2A3F90
    dir.arrived = 0;
  }
  // 28E8C0(20, 1) from 27A860 (transport / lodge / return from an event, with 28F558's destination in +0x6284).
  // Not called at a location crossing (22DF50): see freeRideCourse.
  // pv boothDj: the transport booth (stage builtin 68 action 3, 0x302410) calls 2A49E8: audio+0x5818 = 1 and a 30 000 ms timer (2ADCA0, the
  // pmf at 0x4A36F0 -> 2A4A68) that clears it. 2A4A38 = the flag and 2A10C0(audio, 10) (the request being spoken is the DJ's): while
  // that holds, the travel (28E8C0 20) neither stops the speech before pktrans (0x28EDF0: 2B11B0 skipped) nor queues the radio big
  // intro (0x28EDB4: 2A26F0 / 2B1758 / +0x6258 / +0x5754 skipped), so a DJ line started at the booth plays on over the transport.
  const boothHold = () => pv('boothDj') && !!state.boothFlag && !!speech?.speaking?.(0xa);
  function travel() {
    log(`travel (28E8C0 20) dest ${dir.dest}`); tl('code', 20);
    dir.arrived = 0; state.pendingDj.textMessage = 0; dir.lastDest = dir.dest; dir.cinematic = 1; state.trigger.latch = -1; dir.bcIntro = 0;
    for (const k of ['bcChallenge', 'radioBigOutro', 'finishLine', 'hubChatter', 'hubChatter2', 'hubChatter3', 'eventIntro', 'radioBigIntro', 'firstSpoke', 'freeRideIntro', 'artist', 'textMessage', 'bcIntro']) state.pendingDj[k] = 0; // 2A4550
    cancel('request'); cancel('dj'); cancel('retry');
    world?.ambienceStop(5.03); sfxGame?.pauseStops(); // 29D678 (gp-0x45C8 = 5.03 s), 296E20 / 297438
    if (!musicMode()) { // 28D5A0: the destination peak's ambience, event 30
      if (dir.dest !== NO_DEST) { const peak = peakMask(dir.dest), id = `Peak${peak === 1 ? 1 : peak === 2 ? 2 : 3}Amb`; if (state.musicId !== id) playMusic(0, { songId: peak }); }
      sendEvent(30); return;
    }
    let pick = true;
    if (!state.free || state.free.kind !== 4) { // 2A4078 race / 2A40E8 slope event: a new song unless round 1
      if ((state.course < 5 || gameKind() === 0 || (state.course >= 5 && state.course <= 7) || gameKind() === 1 || gameKind() === 6) && round() >= 2) { pickSong(); pick = false; post('request', 10, request(3, true)); }
    }
    const d = dir.dest;
    if (d !== NO_DEST && isBackcountry(d) && dir.firstVisit[d] === 1) { // pktrans: first arrival on a new peak
      if (!boothHold()) speech.stop(); log('pktrans');
      playSong('pktrans', peakMask(d) === 2 ? 2 : 3, { intensity: 127 }).then((m) => m?.pause());
      return;
    }
    if (pick && d !== NO_DEST) { pickSong(); post('request', 10, request(2, true)); }
    if (dir.lastDest !== NO_DEST && dir.lastDest !== state.course && !boothHold()) { speech.radioBigIntro(0, 1); speech.flush(); dir.arrived = 1; state.pendingDj.hubChatter3 = 1; } // 2A4718, 2A4A38
  }

  // ---- loading loop (28F768 / 28FA98: LoadingScreen.bnk looped on MUSIC, 1 s fade out) --------------------------
  async function loadingLoop(on) {
    const context = ctx(); if (on !== !!state.loadingWanted) tl('loading', on ? 1 : 0); state.loadingWanted = on;
    if (!on) { const l = state.loading; state.loading = null; if (l && context) { const t = context.currentTime; l.gain.gain.setValueAtTime(l.gain.gain.value, t); l.gain.gain.linearRampToValueAtTime(0, t + 1); l.source.stop(t + 1.05); } return; }
    if (!context || state.loading) return;
    const [bank, bnk] = await Promise.all([json('banks/LoadingScreen.json'), bytes('banks/LoadingScreen.bnk')]);
    const patch = bank.entries[0].patches[0];
    if (!state.loadingWanted || state.loading) return; // stopped (or started) while fetching
    const buffer = toAudioBuffer(context, decodeBankPatch(bnk, patch));
    const source = context.createBufferSource(); source.buffer = buffer; source.loop = true;
    const gain = context.createGain(); source.connect(gain); gain.connect(engine.bus('MUSIC')); source.start();
    state.loading = { source, gain };
  }

  // ---- banks (285FB0 FE, 2862A8 WORLD from BANKS.INF, 29F3F8 grunts) ----
  function loadWorldBanks(character) {
    const w = Object.fromEntries(Object.entries(catalog.banksInf.find((b) => b.name === 'WORLD') ?? {}).filter(([k]) => k !== 'name'));
    const name = (f) => f.replace(/\.bnk$/i, '');
    const exact = { zbxsfx: 'zBxsfx' }; // exported file names (case-sensitive servers)
    const bank = (slot, f) => f && sfx.loadBank(slot, exact[name(f).toLowerCase()] ?? name(f));
    bank(SLOT.MAIN, w.MAIN); bank(SLOT.BOARD, w.BOARD); bank(SLOT.SPUBOARD, w.SPUBOARD); bank(SLOT.MOUNTAIN, w.MOUNTAIN);
    bank(SLOT.CROWD, w.CROWD); bank(SLOT.TRANSPORT, w.TRANSPORT); bank(SLOT.LAND, w.LAND);
    sfx.loadBank(SLOT.GRUNT, `GRNT_${['MOB', 'KAO', 'ARI', 'MAC', 'ZOE', 'GRF', 'ELI', 'NAT', 'PSY', 'VIG'][gruntIndex(character)]}`);
    sfx.loadBank(SLOT.GRUNT_AI, 'GRNT_AI');
  }

  // ---- speech OnIdle (2A43B8): one pending DJ flag per idle edge, in this order ----
  function onIdle() {
    const p = state.pendingDj, E = speech.EV, peak = peakMask(state.course);
    const idle = (name, f) => { log(`idle ${name}`); f(); };
    if (p.radioBigIntro) { p.radioBigIntro = 0; return idle('Radio_Big_Intro', () => speech.radioBigIntro(0, 0)); }                       // 0x5768
    if (p.bcIntro) { p.bcIntro = 0; return idle('BC_Intro', () => speech.dj(E.BC_INTRO, [peak])); }                                       // 0x5784 2A2AD0
    if (p.firstSpoke) { p.firstSpoke = 0; return idle('First_Spoke', () => speech.dj(E.FIRST_SPOKE, [peak])); }                           // 0x576C 2A2638
    if (p.textMessage) { p.textMessage = 0; return idle('Text_Message', () => { speech.dj(E.TEXT_MESSAGE, [dir.tmVariant]); dir.tmVariant = 1; }); } // 0x577C 2A2B88
    if (p.freeRideIntro) { p.freeRideIntro = 0; return idle('Free_Ride_Intro', () => speech.dj(E.FREE_RIDE_INTRO, [courseBit(state.course)])); } // 0x5770 2A2C30
    if (p.bcChallenge) { const r = p.bcChallenge; p.bcChallenge = 0; return idle('BC_Challenge', () => speech.bcChallenge({ id: charId(r) }, { id: charId(state.character) })); } // 0x5788 (2A1138: rival slot 1 -> human slot 0)
    if (p.artist) { p.artist = 0; return idle('Artist_Intro', () => { if (musicMode()) speech.artistIntro(p.sed); }); }                   // 0x5774 2A2860
    if (p.hubChatter) { p.hubChatter = 0; return idle('Hub', () => hubChatter(0)); }                                                   // 0x574C
    if (p.hubChatter2) { p.hubChatter2 = 0; return idle('Hub', () => hubChatter(0)); }                                                 // 0x5750
    if (p.hubChatter3) { p.hubChatter3 = 0; return idle('Hub', () => hubChatter(1)); }                                                 // 0x5754
    if (p.eventIntro) { p.eventIntro = 0; return idle('Event_Intro', () => speech.eventIntro(state.course, p.region ?? -1)); }          // 0x5758 2A2568
    if (p.radioBigOutro) { p.radioBigOutro = 0; idle('Radio_Big_Outro', () => speech.radioBigOutro()); }                                // 0x5740 (no return: 0x5744 follows)
    if (p.finishLine) { p.finishLine = 0; idle('Finish_Line', () => speech.finishLineRider({ id: charId(state.character) }, state.place, true)); } // 0x5744
  }

  // 2A2E50(pool) hub chatter: after a new career's intro (+0x5790 == 0, not just arrived) the first call is Char_Stories,
  // Mtn_History is marked used and Peak_Boss forced next; a forced category plays once; otherwise a uniformly random
  // unused category of the pool (8, or 5 for pool 1) with the used mask (+0x573C) reset when exhausted.
  const HUB = ['HUB_CHAR_STORIES', 'HUB_TERRAIN_INFO', 'HUB_WEATHER', 'PEAK_BOSS', 'TEXT_MESSAGE', 'HUB_GOING_ONS', 'HUB_LOCAL_STORIES', 'HUB_MTN_HISTORY'];
  function hubChatter(pool = 0) {
    let k = -1, random = false;
    if (!dir.hubFirst && !dir.arrived) { dir.hubFirst = 1; dir.hubForced = 3; dir.hubUsed |= 0x80; dir.bcIntro = 0; k = 0; }
    else if (dir.hubForced !== -1 && !dir.arrived) { k = dir.hubForced; dir.hubForced = -1; }
    else {
      const n = pool ? 5 : 8;
      let free = 0; for (let i = 0; i < n; i++) free += (dir.hubUsed >> i) & 1 ? 0 : 1;
      if (!free) { dir.hubUsed = 0; free = n; }
      let r = Math.floor((rand15() * free) / 0x7fff) + 1; k = 0; random = true;
      for (; k < 31; k++) { if (!((dir.hubUsed >> k) & 1) && --r === 0) break; }
    }
    dir.hubUsed |= 1 << k;
    if (k >= 8) return;
    const peak = peakMask(state.course), E = speech.EV;
    log(`hub chatter ${HUB[k]}`); if (random) tl('hub', HUB[k]); // timeline: a random category
    if (k === 0) speech.dj(E.HUB_CHAR_STORIES, [subjectMaskOf(state.character), peak]);
    else if (k === 3) speech.dj(E.PEAK_BOSS, [SUBJECT_MASK[peakBoss()]]); // 2A2A10: 2A1DA0(145750) the peak rival's subject mask
    else if (k === 4) { speech.dj(E.TEXT_MESSAGE, [dir.tmVariant]); dir.tmVariant = 1; }
    else speech.dj(E[HUB[k]], [peak]);
    speech.flush(); // 2B1758
  }
  const SUBJECT_MASK = [0x4, 0x10, 0x200, 0x2, 0x20, 0x100, 0x1, 0x80, 0x8, 0x40]; // 2A1DA0 by CHARDB id
  const subjectMaskOf = (name) => { const id = charId(name); return id >= 0 ? SUBJECT_MASK[id] : 0x1000; };
  // 145750: the peak rival (web/career.js rival): Mac / Nate / Psymon, Griff / Zoe / Elise when the player rides that one.
  const peakBoss = () => { const p = peakMask(state.course), me = charId(state.character); const [boss, alt] = p === 1 ? [3, 5] : p === 2 ? [7, 4] : [8, 6]; return me === boss ? alt : boss; };

  // ---- 28D988: MusicTrigger painter (radio modes 0/1/3) ----
  function musicTrigger(a, b) {
    const T = state.trigger;
    const changed = a !== T.a || b !== T.b;
    const eventMode = challenge(); // 2A42C8: mode 6..11 (time / points challenges)
    const leave = () => { if (T.zone) sendEvent(T.zone + 1); T.zone = 0; T.zoneExit = 0; }; // 28D7D8
    let store = true;
    if (a === -1) { if (T.a === 1 || T.a === 2) leave(); T.rearm = 1; }
    else if (a === 1) { if (changed && !(pv('bigChallengeAudio') && (state.challengeMusic || state.challengePrompt))) { if (T.zone && T.zone !== b) sendEvent(T.zone + 1); sendEvent(b); T.zone = b; T.zoneExit = 0; } } // 28D630 (+0x5FD4 / +0x5FD8 gate)
    else if (a === 2) { if (changed) { if (T.zone === b) T.zoneExit = b; else leave(); } }                                          // 28D740
    else if (a === 0) {
      if (b === 0) { if (!state.music && !state.musicToken) playMusic(0); else sendEvent(0); }
      else if (b === 11) { // hub approach: event 11, the hub song 2 s later (forced with the DJ on), the spoke / hub DJ at
        // 1.5 s; in a time / points challenge a song change instead
        if (T.rearm && latch(11)) {
          if (eventMode) { if (changed) changeSong(false); }
          else {
            log('trigger 11'); sendEvent(11);
            post('request', 2000, request(0, djOn()));
            if (changed) post('dj', 1500, () => djTimer(dir.hubFirst ? 1 : 0));
          }
          T.rearm = 0;
        }
      } else if (b === 13) { if (latch(13)) { if (changed && !eventMode) state.pendingDj.hubChatter2 = 1; sendEvent(13); } }
      else if (b >= 14 && b <= 17) { if (latch(b) && changed && !eventMode) { state.pendingDj.eventIntro = 1; state.pendingDj.region = b; state.pendingDj.radioBigOutro = 1; } }
      else if (b === 18) { if (gameKind() === 4 && !eventMode && T.rearm && latch(18)) { if (changed) changeSong(false); T.rearm = 0; } }
      else if (b === 42) { if (changed) state.music?.send?.(42); } // 2B3B88
      else sendEvent(b);
    } else store = false;
    if (store) { T.a = a; T.b = b; }
  }
  // 28DF18 ChangeSong (free ride): with a DJ line busy, retry every 100 ms (28E068); else fade out (DJ off), event 18,
  // PickNextSong, the new song 2 s later (request kind 1, forced), the DJ intro for its SEDVALUE at 1.5 s (kind 3).
  function changeSong(retry) {
    if (!speech.free && engine.busGain('DJ') > 0) { if (!retry) post('retry', 100, () => changeSong(true)); return false; } // 2A10C0(10)
    log('change song');
    if (!djOn()) { tl('fade', 2); state.music?.fadeOut?.(2); }
    sendEvent(18);
    pickSong();
    post('request', 2000, request(1, true));
    const sed = feSongs()[state.songIndex]?.SEDVALUE ?? -1;
    post('dj', 1500, () => djTimer(3, sed));
    return true;
  }
  // 2899F8: Ambience painter (radio mode 2): 31 / 32 start Peak1Amb / Peak2Amb (PlayMusic with that song), 33 nothing,
  // anything else is a section event.
  function ambiencePainter(amb) {
    if (amb < 0 || amb === 33) return;
    if (amb === 31) { if (state.musicId !== 'Peak1Amb') playMusic(0, { songId: 1 }); return; }
    if (amb === 32) { if (state.musicId !== 'Peak2Amb') playMusic(0, { songId: 2 }); return; }
    sendEvent(amb);
  }

  // Career context from the UI (main.js sets gameAudio.context): {career, round (1..3; single events run round 3)}.
  const context = () => { try { return api.context?.() ?? {}; } catch { return {}; } };
  const round = () => context().round ?? 3;
  const api = {
    context: null,
    get raceMusic() { return !!state.raceMusic; },
    get engine() { return engine; },
    get sfx() { return sfx; },
    get speechEngine() { return speech; },
    get songTitle() { const s = catalog?.songs.find((x) => x.id === state.musicId); return s?.ADDTOFE ? { title: s.TITLE, artist: s.ARTIST, album: s.ALBUM } : null; },
    whenReady,
    unlock() { return engine?.unlock(); },
    // ---- settings (Audio menus, web/audio-menu.js) ----
    getSettings() { return { ...settings, playlist: settings.playlist.slice() }; },
    setSettings(partial = {}) {
      const before = { ...settings };
      Object.assign(settings, partial);
      if (partial.playlist) settings.playlist = partial.playlist.slice(0, 35).map(Boolean);
      try { globalThis.localStorage?.setItem(SETTINGS_KEY, JSON.stringify(settings)); } catch {}
      if (!engine) return;
      for (const k of ['music', 'effects', 'speech']) if (partial[k] !== undefined) engine.setSlider(k, settings[k]);
      if (partial.dj !== undefined) engine.setDj(settings.dj);
      if (partial.arcadeAudio !== undefined) engine.setArcadeAudio(settings.arcadeAudio);
      if (partial.radioMode !== undefined && partial.radioMode !== before.radioMode) { // 28BF78
        engine.setRadioMode(settings.radioMode);
        if (state.inWorld && catalog && (state.music || state.musicToken || state.musicId)) radioModeChanged();
      }
    },
    songs() { return catalog ? feSongs().map((s, index) => ({ index, id: s.id, title: s.TITLE, artist: s.ARTIST, album: s.ALBUM })) : []; },
    // Request Line highlight (196B90): FE -> the song's PREVIEW event on charsel; in game -> the song now.
    previewSong(index) {
      const s = feSongs()[index]; if (!s) return;
      state.preview = true;
      if (!state.inWorld) { if (state.musicId === 'charsel' && s.PREVIEW >= 0) sendEvent(s.PREVIEW); return; }
      state.songIndex = index; playSong(s.id, 0);
    },
    endPreview({ career = false } = {}) { if (state.preview && !state.inWorld && career) sendEvent(10); state.preview = false; }, // 0x1976E0: event 10 after a preview (career only)
    onNowPlaying(f) { nowPlaying.add(f); return () => nowPlaying.delete(f); },
    // UI sounds: 294F78 (event -> bank 0 sound) and 294F48 / raw index.
    ui(ev) { const snd = UI_SOUND[ev]; if (snd !== undefined) api.uiSound(snd); },
    uiSound(snd) { sfx?.play({ slot: SLOT.MAIN, sound: snd, bus: 'UI', volume: 127, tag: 'ui' }); },
    // Front-end screen entered (28F140): charsel plays in the front end, with the screen's section event.
    async screen(name) {
      await whenReady();
      if (name === 'results' || name.startsWith('ctm-results')) { state.replay = true; speech.replay = true; if (sfxGame) sfxGame.G.replay = true; api.podium(); return; } // 288AE0: the replay runs behind the results
      if (name === 'ctm-objectives') { const c = context(); if (c.career && round() < 2 && eventKind(state.course) !== 4) speech.sponsorIntro(); return; } // 1FB588 -> 2A31C0
      if (['game', 'loading', 'pause', 'audio', 'audio-playlist'].includes(name)) return;
      const event = FE_EVENT[name]; if (event === undefined) return;
      // In a world the lodge is the only front end: the PS2 leaves the world for it (music/runs/lodge: 286A80 Stop, 285FB0
      // charsel, 28F140(1)). The free-ride MCOMM (Messages, Options, Audio, Transport) and the pause menus are overlays on
      // the paused world (289B70 pauses the song; music/runs/mcomm: no 28F140, no charsel until 289BB8 resumes the song).
      if (state.inWorld && !state.lodge) { if (name !== 'ctm-lodge') return; enterLodge(); }
      if (state.musicId !== 'charsel' || !state.music) { state.raceMusic = false; state.feEvent = event; tl('fe', event); await playSong('charsel', event); return; }
      if (state.feEvent === event) return; // ignored if the state is unchanged
      state.feEvent = event; tl('fe', event); sendEvent(event);
    },
    // Lodge Return to Game / Transport (the lodge exit, 286200 FadeOut(1 s)) is a world load at the station: see runStart.
    // Event load (FE -> game 286200: FadeOut(1 s)) with the loading-screen loop; world load 2867E8 picks a song
    // and plays it with event 36 (single event) / 0 (Conquer the Mountain) once the world is up.
    async loadingStart({ courseCode = null, character = null } = {}) {
      await whenReady();
      if (state.carry) { if (courseCode) api.prepare({ courseCode, character }); return; } // in-world event load: no FE fade, no loading loop
      fadeOutMusic(1.0); state.feEvent = null;
      if (courseCode) api.prepare({ courseCode, character });
      await loadingLoop(true);
    },
    // Conquer the Mountain event started from free ride (web/ctm-event.js): the PS2 runs it in the streamed world, so
    // there is no world teardown (286A80) or world load (2867E8) and the song the fly-over's code 21-25 picked plays on
    // into the approach, the card and the countdown. The browser switches course in the page under a cover: with the
    // carry set, leaveWorld keeps the music and its request timers, the load screen hooks start no loop, and the next
    // worldLoaded neither restarts the music nor plays the single-event venue intro. Cleared by that worldLoaded.
    carryWorld(on = true) { state.carry = !!on; log(`carry ${state.carry}`); },
    // Loading-screen prep: world banks, the course's world audio and every patch decoded in 4 ms slices.
    prepare({ courseCode = null, character = null } = {}) {
      if (!sfx) return;
      engine.setScaleMode('WORLD'); // 2862A8 -> 288D18(1)
      if (character) state.character = character;
      loadWorldBanks(state.character);
      if (courseCode) state.courseCode = courseCode;
      // A streamed peak world starts with no location in memory: gameTick feeds the streaming rows (setResident).
      if (isPeakWorld(courseCode)) world.setResident(state.residentKey ? state.residentKey.split(',').map(Number) : []);
      Promise.resolve(courseCode ? world.load(courseCode) : null).then(() => {
        // pv sfxWarmFirst (docs/audio-logic.md 9.13): what the intro starts first (the location ambience, the world banks, the crowd loops)
        // decodes first; the crowd loop decoded at its first play under the intro (97 ms at 4x CPU) when the warm-up had not reached it
        if (engine.unlocked) return sfx.warm(pv('sfxWarmFirst') ? [SLOT.WORLD9, SLOT.CROWD, SLOT.WORLD8, SLOT.MAIN, SLOT.BOARD, SLOT.SPUBOARD, SLOT.LAND, SLOT.GRUNT, SLOT.GRUNT_AI, SLOT.MOUNTAIN, SLOT.TRANSPORT]
          : [SLOT.MAIN, SLOT.BOARD, SLOT.SPUBOARD, SLOT.LAND, SLOT.CROWD, SLOT.GRUNT, SLOT.GRUNT_AI, SLOT.WORLD9, SLOT.WORLD8, SLOT.MOUNTAIN, SLOT.TRANSPORT]);
      }).catch((e) => console.warn('Audio prep failed', e));
    },
    async worldLoaded({ courseIndex = 0, singleEvent = true, courseCode = null, character = null, rounds = 1, freeRide = null } = {}) {
      await whenReady();
      // A streamed peak world (PEAK1) needs its game kind and current course: the load only prepares, runStart
      // (freeRide) starts the world; a later load notification for the running world is ignored.
      if (isPeakWorld(courseCode) && !freeRide) { if (!state.free) api.prepare({ courseCode, character }); return; }
      if (freeRide) singleEvent = false; // free ride and the peak runs are Conquer the Mountain (0x535C11 = 0)
      state.raceMusic = true; state.course = courseIndex; state.singleEvent = singleEvent; state.goSeen = false; state.resumeGo = 1; // 0x2867AC: audio+0x627C = 1 at every world load
      state.free = freeRide ? { kind: freeRide.kind ?? 4, mode: freeRide.mode ?? 12 } : null; state.rolled = false;
      state.inWorld = true; state.replay = false; speech.replay = false; sfxGame.G.replay = false; state.chartuneIndex = -1; state.rounds = { current: 1, total: rounds };
      state.courseCode = courseCode; if (character) state.character = character;
      state.trigger = { a: -99, b: -99, zone: 0, zoneExit: 0, rearm: 1, latch: -1 }; state.pendingDj = {}; state.track = -1; state.region = null; state.relTick = null; state.relPair.clear();
      if (!state.carry) riderMusic(); // 2867E8: the human's radio mode (158700 -> 28BF78) and custom playlist (158750 -> 28C2D0)
      api.prepare({ courseCode, character });
      sfxGame.start({ courseIndex, ...courseFlags(courseIndex), raceKind: gameKind() });
      const intro = !!state.free && introStart(); // 234F40's pktrans comes before the loading screen ends (PS2 samples 573 / 594)
      await loadingLoop(false);
      if (state.free) { worldLoadFree(intro); return; }
      if (state.carry && (state.music || state.musicToken || state.musicId || timers.some((t) => t.key === 'request'))) return; // in-world event start: the fly-over's song plays on (until the run starts)
      tl('worldload', courseIndex); // 2867E8 (free ride: worldLoadFree)
      await playWorldMusic(singleEvent ? 36 : 0);
      if (singleEvent) speech.venueIntro(courseIndex); // 286E20 -> 2A39E0
    },
    // Run start (startRun): the world song if the load did not start one yet; a restart goes back to the idle
    // section (28F2C0 -> event 36) or, after the podium tune, to a new playlist song (kind 3).
    // Free ride / peak runs (`freeRide: {kind, mode}`, course = the current Peak 1 course 0x535C08): the world load
    // (2867E8 + 2A4A78) and the ride start (234F40 state 10 enter, 28E888 when its cinematic ends).
    async runStart({ courseIndex = 0, singleEvent = true, courseCode = null, character = null, freeRide = null } = {}) {
      await whenReady(); state.carry = false;
      if (state.restarting && !freeRide) { // a Conquer the Mountain restart (restartRun): the round's handler again, no music call (2302A8)
        state.restarting = false; if (character) state.character = character;
        state.replay = false; speech.replay = false; sfxGame.G.replay = false; state.relTick = null; state.relPair.clear(); state.rolled = false;
        sfxGame.start({ courseIndex, ...courseFlags(courseIndex), raceKind: gameKind() }); return;
      }
      if (character) state.character = character;
      if (freeRide && (state.lodge || !(state.free && state.inWorld && state.raceMusic && state.courseCode === courseCode))) {
        // Out of the lodge (PS2 music/runs/lodge): 286200 FadeOut(1 s) of charsel, then the world load at the station (hub
        // song, event 12; 2A4A78 DJ kind 2: Radio BIG intro, hub chatter).
        if (state.lodge) { state.lodge = false; fadeOutMusic(1.0); state.feEvent = null; sfx?.pauseAll(false, (v) => v.tag !== 'ui'); }
        await api.worldLoaded({ courseIndex, singleEvent: false, courseCode, character, freeRide });
        state.freeStart = false; startFree(); return;
      }
      if (freeRide) { state.course = courseIndex; if (state.freeStart) { state.freeStart = false; startFree(); } } // the world loaded at the load screen's end (freeWorldLoaded)
      else if (!state.raceMusic || (!state.music && !state.musicToken)) return api.worldLoaded({ courseIndex, singleEvent, courseCode, character });
      state.replay = false; speech.replay = false; sfxGame.G.replay = false; state.relTick = null; state.relPair.clear(); state.rolled = false;
      sfxGame.start({ courseIndex, ...courseFlags(courseIndex), raceKind: gameKind() });
      if (state.musicId === 'chartune') { state.songIndex = state.chartuneIndex; await playMusic(36); } // 28F200 / 28F2C0
      else if (state.goSeen) sendEvent(36);
    },
    // pv ctmWorldAudio (main.js, the load screen closing on a streamed world): the free-ride world load 2867E8 + 234F40 runs when the
    // world is up, before world state 10's arrival list, as on the PS2 (music/runs/newcareer: pktrans s573, loading off and the world
    // load s594, WS10 resumes pktrans s629, the ABC1 movie s650..2325 and the plane NIS under it, 28E8C0(19) at the cinematic's end
    // s3248). The ride start (runStart) then only runs 28E888 (startFree: the cinematic end, held while an arrival list plays).
    async freeWorldLoaded({ courseIndex = 0, courseCode = null, character = null, freeRide = null } = {}) {
      await whenReady(); state.carry = false;
      if (character) state.character = character;
      if (state.lodge) { state.lodge = false; fadeOutMusic(1.0); state.feEvent = null; sfx?.pauseAll(false, (v) => v.tag !== 'ui'); }
      await api.worldLoaded({ courseIndex, singleEvent: false, courseCode, character, freeRide });
      state.freeStart = true;
    },
    // A replay starts or loops (web/replay.js): the game sound state of a run start, without the music; 288AE0 stays set.
    replayRewind() {
      if (!sfxGame) return;
      sfxGame.start({ courseIndex: state.course, ...courseFlags(state.course), raceKind: gameKind() });
      state.replay = true; speech.replay = true; sfxGame.G.replay = true; state.relTick = null; state.relPair.clear();
    },
    // GO (29C7B0): resume; event 37 on the first run's GO (0 afterwards); single event / MP: DJ Radio BIG intro
    // (2A3170 -> 2A26F0(0, 0), flushed) and the artist intro pending (0x5774); career medal runs: PA 2A3400.
    // Medal run (GMM+0x98, ARMSX2): set by the race (23A108) / freestyle (238E20) handler inits for round 3 only, 0 in
    // the rival challenge handlers (23B268 / 23C0D0); the line needs Conquer the Mountain (0x535C11 == 0).
    go({ medalRun = !!context().career && round() === 3 && !(context().mode >= 4) } = {}) {
      if (state.free) return; // no countdown in free ride / the peak runs (rolling start, gameTick)
      state.countdownHold = false;
      state.music?.resume();
      // 29CB08: event 0 while audio+0x627C is set (every world load sets it; PS2 music/runs/single: event 0 at the first GO), else
      // event 37 (the run after a Big Air / Half Pipe heat, 286F94 cleared it) and 0x627C = 1.
      if (musicMode()) sendEvent(state.resumeGo ? 0 : 37); state.resumeGo = 1; state.goSeen = true;
      const s = catalog?.songs.find((x) => x.id === state.musicId);
      if (state.singleEvent) { speech.radioBigIntro(0, 0); speech.flush(); }
      else if (medalRun) { speech.medalRunIntro(state.course); speech.flush(); }
      // 0x5774 artist intro pending: single event / MP (2A3170), career round 1, or round 2 of a race / slope style.
      const r = round(), k = eventKind(state.course);
      if (state.singleEvent || r === 1 || (r === 2 && (k === 0 || k === 1))) { state.pendingDj.artist = 1; state.pendingDj.sed = s?.SEDVALUE ?? -1; }
    },
    // Rolling start of the time / points challenges (234AD0 -> 233AA0 for event kinds 4..6; audio 2872A8, docs/backcountry.md):
    // no countdown and no GO sound. The GO tick (0x582C) is the start tick; the song gets event 0 (or a new playlist song
    // when none is playing). Rival Time / Rival Points outside multiplayer: the rival's BC_Challenge (0x5788) is queued,
    // otherwise DJ Radio BIG intro (2A26F0) + flush; then the artist intro is pending with 0x5778 = 1.
    rivalStart({ rival = 'mac', rivalMode = true, mp = false } = {}) {
      if (state.music || state.musicToken) { state.music?.resume(); if (musicMode()) sendEvent(0); } else playWorldMusic(0); // +0x530: a song started (or loading)
      state.goSeen = true;
      if (rivalMode && !mp) state.pendingDj.bcChallenge = rival;
      else { speech.radioBigIntro(0, 0); speech.flush(); }
      state.pendingDj.artist = 1; state.pendingDj.sed = currentSed(); // 0x5778 = 2B4908: the playing index's SEDVALUE
    },
    // Big Challenges (web/big-challenges.js; audio +0x5FD4 = challenge music on): 29D6E0 sends 33 / 34 / 38 by the challenge type
    // (table 0x43EE10 row +0x22, jump table 0x482DE0: 1/2/3; 0 and 4 none), 29D8E0 (complete) and 29DBB0 (stop; UI event 14 on a
    // fail) send 39 while it is on.
    // pv bigChallengeAudio: the three functions as the PS2 runs them (docs/audio-logic.md 3.9). A start attaches the song's loop
    // bank to the overlay track (2B4620) before the event, so the event's stinger (the loop bank's long ending, Wobble's own
    // third one at bank entry 66) plays; a hub song is replaced by a playlist song instead; completion plays 0x6D + Arcade_Prompts 1.
    challengeStart(type) {
      if (!pv('bigChallengeAudio')) { const e = { 1: 33, 2: 34, 3: 38 }[type]; state.challengeMusic = e !== undefined && !!state.music; if (state.challengeMusic && musicMode()) sendEvent(e); return; }
      state.challengePrompt = false;                                              // +0x5FD8 = 0
      sfxGame?.challengeStops();                                                  // 296E20 / 297438, 29B3C0
      const loops = state.music?.loops;
      if (loops?.requested) { if (musicMode()) musicAdapter.event(8); loops.stop(); } // 2B46E8: forced 8, 2B4708
      if (!musicMode()) return;                                                   // radio mode 2
      if (/^Peak[123]$/.test(state.currentId ?? state.musicId ?? '')) {          // 2B49E0 song id 1..3: a playlist song now
        log('challenge start: hub song -> playlist song'); tl('challenge', 'hub');
        cancel('request'); cancel('dj'); cancel('retry'); latch(18); pickSong(); playMusic(0); return;
      }
      state.challengeType = type | 0;                                             // +0x5FD0 (row +0x22 of 0x43EE10)
      const e = { 1: 33, 2: 34, 3: 38 }[type] ?? -1;                             // jump table 0x482DE0 (0 / 4: -1, no event)
      if (state.music?.core?.voice?.(1) && musicAdapter.loopsStart()) { sendEvent(e); state.challengeMusic = true; } // 2B4620, +0x20 SendEvent, +0x5FD4 = 1
      state.music?.loops?.stop();                                                 // 2B4708
    },
    // 29D8E0 (the challenge completed): event 39 while the flag is set, then always sound 0x6D and Arcade_Prompts 1 (2A3C00).
    challengeEnd() {
      if (!pv('bigChallengeAudio')) { if (!state.challengeMusic) return; state.challengeMusic = false; if (musicMode()) sendEvent(39); return; }
      if (state.challengeMusic) sendEvent(39);
      state.challengeMusic = false; state.challengeType = 0;
      sfxGame?.challengeComplete(); speech?.arcade(EV.ARCADE_PROMPTS, 1);
    },
    // 29DBB0(audio, 0 fail / 1 quit or decline): only while the flag is set: UI event 14 on a fail, the stops, event 39.
    challengeStop(fail = false) {
      if (!pv('bigChallengeAudio')) { if (!state.challengeMusic) return; if (fail) api.ui(14); state.challengeMusic = false; if (musicMode()) sendEvent(39); return; }
      if (!state.challengeMusic) return;
      if (fail) api.ui(14);
      sfxGame?.challengeStops(); sendEvent(39);
      state.challengeMusic = false; state.challengeType = 0;
    },
    // 29D6D0 (the prompt's Yes, 1F7548 / 1F7738): +0x5FD8 = 1 until the start.
    challengeAccepted() {
      if (pv('bigChallengeAudio')) state.challengePrompt = true;
      if (pv('audioDeclick')) state.music?.warmEvents?.([33, 34, 38]); // the stingers decoded before the start (docs/audio-logic.md 9.13)
    },
    // Pause menu (289B70 / 289BB8): music freezes (pitch 0), SFX voices pause (29CE28); resume cancels queued speech.
    pause(on) {
      if (!on && !state.paused) { sfx?.pauseAll(false, (v) => v.tag !== 'ui'); return; } // not paused (a world load / transport resumed it)
      state.paused = on; tl(on ? 'pause' : 'resume');
      if (on) { state.music?.pause(); sfxGame?.pauseStops(); } else { state.music?.resume(); speech?.cancelQueued(); }
      sfx?.pauseAll(on, (v) => v.tag !== 'ui');
    },
    // Finish (286EA0): ending section (10), or 36 when runs remain (Big Air / Half Pipe heats); PA finish lines.
    finish({ runsLeft = false, place = state.place, timedOut = false, winner = null, ai = null, challengeKind = 0 } = {}) {
      if (state.free && (state.free.kind === 5 || state.free.kind === 6)) challengeKind = state.free.kind; // Peak 1 Race / Jam
      if (ai && !winner) { const w = ai.standings?.().find((r) => r.rank === 0); if (w) winner = w.human ? state.character : w.character; }
      if (musicMode()) sendEvent(runsLeft ? 36 : 10);
      if (runsLeft) { state.goSeen = false; state.resumeGo = 0; } // 286F94
      state.place = place; if (winner) state.winner = winner;
      if (timedOut) return; // rider+0x480
      const me = { id: charId(state.character) };
      if (state.course < 5 || eventKind(state.course) === 0 || challengeKind === 5) speech.paFinishLine(me, place); // 2A4078 -> 2A3708 (kind 5: time challenges incl. Rival Time)
      else if (challengeKind === 6) speech.riderPosition(me, place);                                   // 2A34D0 (GMM+0 = 1 in Rival Points)
      else if (round() < 3) speech.riderPosition(me, place);                                            // 2A34D0
      else speech.paFinishLine(me, place);
    },
    // 28CDF8 podium: final round and the human placed 1st-3rd -> chartune with the winner's character event.
    podium({ place = state.place, winner = state.winner ?? state.character } = {}) {
      if (!musicMode() || place > 2 || round() !== 3) return; // G+0xC0 state 3 (final round), place < 3
      if (state.musicId === 'chartune' && state.music && !state.music.finished) return; // already started by the podium cutscene (web/cutscenes.js)
      const id = CHAR_ID[winner]; if (id === undefined) return;
      state.chartuneIndex = state.songIndex;
      playSong('chartune', CHARTUNE_EVENT[id], { intensity: 127 });
    },
    // Leaving the world (286A80): stop (no fade), mix 0, speech off, rider loops and world sounds off.
    // pv ctmRestartAudio: a Conquer the Mountain Restart is not a world teardown (docs/audio-logic.md 9.13). The pause's Restart
    // (0x20D1D8 -> 2302A8: world state 2 directly, 2870A0 -> 2871B0) stops the riders' loops, the Uber voice and the ducks; the song
    // was resumed at the Yes (285D78 / 289BB8) and plays on under the card, no event (PS2 music log pause-restart-audio). The
    // results' Restart runs world state 13 too (28E8C0(20, 1): heat(), from the 'heat' cutscene). The page quits the run around it
    // (career-ui.js restartToCard -> ui.cb.quit -> leaveWorld): that leaveWorld keeps the song and the banks, and the next runStart
    // makes no world load and sends no event 36 (GO's event 0 restarts the song as on the PS2).
    restartRun({ fromResults = false } = {}) {
      if (!pv('ctmRestartAudio') || !sfxGame || !state.inWorld || state.free) return false;
      log(`restart (${fromResults ? 'results, WS13' : 'pause, 2302A8'})`); tl('restart', fromResults ? 1 : 0);
      if (state.paused) api.pause(false);                   // 285D78 game resume -> 289BB8
      sfxGame.stop(); engine?.release(0.5);                 // 2871B0: 296E20 / 297438, 29B3C0, 2948A0, 287F00
      state.restarting = true; state.goSeen = false; state.countdownHold = false;
      return true;
    },
    leaveWorld() {
      if (state.restarting) { state.humanCore = null; return; } // (restartRun: the quit around a restart keeps the world's audio)
      state.humanCore = null; // the world's core instance: main.js drops it at a course change (unloadCourse)
      // Song files (~5-20 MB each): kept only while in use. A course change stays in the page (main.js navigateCourse), so the
      // byte cache would otherwise keep every song ever played (a playing song holds its own bytes; a replay re-fetches).
      for (const k of [...cache.keys()]) if (k.startsWith('b:music/')) cache.delete(k);
      if (state.carry === true) { // in-world event start (carryWorld): only the world's own sounds go
        state.carry = 'switched'; state.inWorld = false; state.free = null; state.residentKey = ''; for (let i = timers.length; i-- > 0;) if (timers[i].key !== 'request') timers.splice(i, 1);
        sfxGame?.stop(); world?.unload(); crowd?.reset(); sfx?.stopAll({ fade: 0.25 });
        for (const s of [SLOT.BOARD, SLOT.SPUBOARD, SLOT.MOUNTAIN, SLOT.CROWD, SLOT.TRANSPORT, SLOT.LAND, SLOT.WORLD8, SLOT.WORLD9, SLOT.DYNAMIC]) sfx?.unloadBank(s);
        return;
      }
      tl('leave'); if (state.music || state.musicToken || state.musicId) tl('stop');
      state.lodge = false; dir.hold = false; dir.heldEnd = false; state.paused = false; state.freeStart = false;
      state.raceMusic = false; state.inWorld = false; state.free = null; state.residentKey = ''; timers.length = 0; stopMusic(); speech?.stop(); engine?.setMix(0); engine?.setScaleMode('FE'); // 285FB0 -> 288D18(0)
      sfxGame?.stop(); world?.unload(); crowd?.reset(); sfx?.stopAll({ fade: 0.25 });
      for (const s of [SLOT.BOARD, SLOT.SPUBOARD, SLOT.MOUNTAIN, SLOT.CROWD, SLOT.TRANSPORT, SLOT.LAND, SLOT.GRUNT, SLOT.GRUNT_AI, SLOT.WORLD8, SLOT.WORLD9, SLOT.DYNAMIC]) sfx?.unloadBank(s);
      sfx?.loadBank(SLOT.MAIN, 'SSX3Menu');
    },
    // tWPIGD_Mix painter region (289C98): mix index at the primary rider's position.
    setMix(index) { engine?.setMix(index < 0 ? 0 : index); },
    // Per rendered frame: listener = the camera (285930); scene coordinates = world metres - origin.
    listener(camera, origin) {
      if (!sfx) return;
      runTimers();
      // 285BF8 runs every frame whatever the game state: speech dispatch (60 Hz) and the voice callbacks / 3D update.
      const now = performance.now(); state.frameAt ??= now;
      let n = Math.min(Math.floor((now - state.frameAt) / (1000 / 60)), 8);
      if (n > 0) { state.frameAt += n * (1000 / 60); if (now - state.frameAt > 1000) state.frameAt = now; }
      const idle = state.nis && state.inWorld && now - (state.gameTickAt ?? -1e9) > 100; // a cutscene over a world whose game ticks are not running
      while (n-- > 0) { speech.update(); sfx.tick(1 / 60); if (idle) nisWorldTick(); }
      if (!camera) return;
      camera.updateMatrixWorld?.(); sfx.setListener(camera, origin);
      const o = origin ? [origin.x, origin.y, origin.z] : [0, 0, 0];
      state.listenerCm = [(camera.position.x + o[0]) * 100, -(camera.position.z + o[2]) * 100, (camera.position.y + o[1]) * 100];
    },
    // Per 60 Hz game tick (main.js simTick, after the riders and the course events).
    gameTick({ core, ai = null, raceInfo = null, finished = false, pending = 0, character = null, running = true, place = null, winner = null } = {}) {
      if (!sfxGame || !core) return;
      state.gameTickAt = performance.now();
      state.humanCore = core; if (character) state.character = character;
      if (place != null) state.place = place; if (winner) state.winner = winner;
      const list = [{ core, character: state.character, slot: 0, human: true, finished, place: state.place }];
      if (ai) for (const n of ai.racers.npcs) list.push({ core: n.core, character: n.character, slot: n.slot, human: false, finished: !!n.finished });
      if (ai) { hookPairs(ai); state.relTick ??= relSnapshot(ai); }
      const hud = core._score_hud_slots ? new Float32Array(core.HEAPF32.buffer, core._score_hud_slots(), 44 * 6).slice() : null;
      sfxGame.tick({ riders: list, raceInfo, running, paused: state.paused, pending, hudSlots: hud, finished, timedOut: !!core._race_timed_out?.() });
      state.relPair.clear(); state.relTick = ai ? relSnapshot(ai) : null; // the next tick's ranking sees this tick's end state
      runTimers();
      if (state.free) {
        // Streamed world: the locations whose data is in memory (streaming rows 1, 2, 5, 7; web/peak_world.inc rows).
        if (core._peak_world_rows && isPeakWorld(state.courseCode)) {
          const p = core._peak_world_rows() >> 2, H = new Int32Array(core.HEAPU8.buffer), tracks = [];
          for (let i = 0; i < H[p]; i++) { const o = p + 1 + 4 * i; if (H[o + 3] === 0 && [1, 2, 5, 7].includes(H[o + 2])) tracks.push(H[o + 1]); }
          const key = tracks.join(','); if (key !== state.residentKey) { state.residentKey = key; world.setResident(tracks); log(`resident ${key}`); }
        }
        // 2872A8 (objectives Continue, kinds 5 / 6): the peak runs start rolling, without a countdown.
        const k = state.free.kind;
        if ((k === 5 || k === 6) && !state.rolled && raceInfo?.[5] === 5) { state.rolled = true; log('rolling start'); api.rivalStart({ rivalMode: state.free.mode === 4 || state.free.mode === 5 }); }
      }
      // Painters (2898A8): region = track of the human's last contacted patch (keep it while unknown).
      const contact = core._terrain_contact_info ? new Float32Array(core.HEAPF32.buffer, core._terrain_contact_info(), 1)[0] : -1;
      if (contact >= 0) { state.track = contact & 0xff; world.focusTrack(state.track); }
      const wd = world.loaded ? world : null;
      if (wd && state.track >= 0) {
        const r = sfxGame.riders().find((x) => x.human);
        const doc = worldDoc();
        const region = doc && Object.entries(doc.locations).find(([, l]) => l.track === state.track)?.[0];
        if (region && r) {
          const q = queryAudioPainters(doc, { x: Math.fround(r.t[12]), y: Math.fround(r.t[13]) }, region);
          engine.setMix(q.mix < 0 ? 0 : q.mix);                                  // 289C98
          if (settings.radioMode !== 2) musicTrigger(q.musicTrigger.a, q.musicTrigger.b); // 28D988
          else ambiencePainter(q.ambience);                                         // 2899F8
        }
      }
      if ((++state.ticks & 63) === 1) { let m = 0; try { m = +(globalThis.localStorage?.getItem('ssx3.soundMode') || 0); } catch {} sfx.setMono(m === 2); } // Sound Mode (web/audio-menu.js)
      world.update(state.listenerCm);
      if (pv('avalanche')) { // 0x29DEF0 rumble (docs/avalanche.md "Audio"): the human core's avalanches, listener = the human rider +0x110
        const av = avalancheState(core), me = sfxGame.riders().find((x) => x.human);
        if (av) world.avalanche(av.loop, av.tumblers, me ? [me.t[12], me.t[13], me.t[14]] : null, takeLoopEvents(core));
      }
      if (!state.paused && state.music && state.raceMusic && musicMode()) { // 28F000: hub songs stay at 127, others ramp
        if (/^Peak\d$/.test(state.musicId)) state.music.setIntensity?.(127);
        else { state.intensityFrames++; state.music.setIntensity?.(raceIntensity(state.intensityFrames)); }
      }
    },
    // Location crossing in the streamed world (22DF50: 0x535C08 changes on the streamer pass after an Unload / Load
    // trigger; web/free-ride.js 'course'). The PS2 director gets no call here: across the Snow Jam -> Blue Base Station
    // crossing (menus/fr/sj-14..22) audio+0x6254 / +0x5814 stay 0 while the hub song (request kind 0) and the spoke DJ
    // come from ARA1_B's MusicTrigger 11 zone. The new course feeds every course-dependent check (peak, category,
    // backcountry, hub, the first-visit Free_Ride_Intro) and the ambience / painters follow the rider's contact track.
    freeRideCourse(course, { kind } = {}) {
      if (!state.free || !(course >= 0) || course === state.course) return;
      state.course = course; markVisited(course); log(`course ${course}`);
      Object.assign(sfxGame.G, { courseIndex: course, bigAirEvent: courseFlags(course).bigAir, halfPipeEvent: courseFlags(course).halfPipe, backcountry: courseFlags(course).backcountry });
    },
    // Transport inside the world (28F558 then 27A860 -> 28E8C0(20, 1); the browser reloads the page instead, so this
    // is for an in-world transport): `arrived()` is 28E888 when the arrival cinematic ends.
    // pv boothDj: the transport booth's trigger (2A49E8): the flag for 30 s of the audio timer queue.
    booth() { if (!pv('boothDj')) return; state.boothFlag = 1; log('booth flag (2A49E8)'); cancel('booth'); post('booth', 30000, () => { state.boothFlag = 0; }); },
    travel(dest) {
      if (!state.free) return;
      // 28F678 map close resumes the game (289BB8) before 27A860's travel: the song plays on under the transport (PS2
      // music/runs/transport: resume, map close, then 28E8C0(20)).
      if (state.lodge) { if (state.paused) api.pause(false); state.lodge = false; fadeOutMusic(1.0); state.feEvent = null; state.course = dest; dir.dest = NO_DEST; worldLoadFree(); return; } // from the lodge: a world load at dest
      if (musicMode() && (state.music || state.musicToken || state.musicId)) { tl('stop'); stopMusic(); } // 28F520 at the Transport confirm (0x202068): Stop in music modes
      if (state.paused) api.pause(false);
      dir.dest = dest;
      if (dest === state.course) { dir.arrived = 0; dir.bcIntro = 0; state.trigger.latch = -1; pickSong(); post('request', 10, request(2, true)); return; } // 28EF90
      travel();
    },
    arrived() { cinematicEnd(); },
    // pv heatSong: world state 13 (a Conquer the Mountain event's Next heat, and the results' Restart: WS13 0x235A18 calls 27A860 at
    // 0x235C44, which calls 28E8C0(20, 1)). Its event branch (0x28EC90..0x28ED18): unless free ride (2A4040), in a race (2A4078) or
    // slope-style (2A40E8) event with the round (*(G+0xC0)+0) >= 2: PickNextSong (28D488) and a forced request kind 3 (PlayMusic 36)
    // after 10 ms (2ADCA0). So heats 2 and 3 each start a new song; the port kept heat 1's song for all three.
    heat() { if (!pv('heatSong') || !sfx || state.free) return; log('world state 13 (28E8C0 20)'); dir.dest = NO_DEST; travel(); },
    // An arrival cinematic the page plays (main.js ui.cb.cutscene kind 'arrival': the Happiness plane, the heli drops):
    // the ride start's 28E888 waits for its end (startFree).
    arrivalCinematic(on) { if (on) { dir.hold = true; return; } dir.hold = false; if (dir.heldEnd) { dir.heldEnd = false; cinematicEnd(); } },
    // A cutscene is playing (web/cutscenes.js). The PS2 world keeps updating under an NIS (PS2 audio log, ctm/audio
    // NIS-AUDIO.md: at the Single Event intro's first tick the location ambience (bank 9 #0, 29D370), the crowd loops
    // (bank 5, 2A5D08) and the stage loops start; the CTM fly-over restarts the crowd loops at t22), so while the browser's
    // game ticks are not running (event intros before the run starts) the world sounds tick here (nisWorldTick).
    cutscene(on) { state.nis = !!on; },
    // NIS kind-7 music codes (0x280640 -> 28E8C0(code, 0)): with a2 = 0 the 19 / 20 branches do nothing; 21-25 (the
    // venue fly-overs) in music modes = PickNextSong 28D488, FadeOut(1 s) 2B3D48, request kind 3 (PlayMusic 36, forced)
    // after 3000 ms (28EEE8..28EF48); any other code is sent to the current song as a Pathfinder event (28EF58).
    cutsceneMusic(code) {
      log(`cutscene music ${code}`); tl('code', code);
      if (code === 19 || code === 20) return;
      if (code >= 21 && code <= 25) { if (!musicMode()) return; pickSong(36); fadeOutMusic(1); post('request', 3000, request(3, true)); return; } // (kind 3 plays event 36)
      sendEvent(code);
    },
    // Per frame (60 Hz frames): race intensity ramp 28F000 (kept for callers that do not use gameTick).
    update() {},
    resetIntensity() { state.intensityFrames = 0; },
    // 0x390EC8 lightning strike at `distanceCm` (web/weather.inc; the original polls 0x390EF8 for the thunder delay): 291438.
    thunder(distanceCm) { world?.thunder(distanceCm); },
    // QA: what is playing.
    debug() {
      const c = engine?.context;
      return { unlocked: !!engine?.unlocked, context: c?.state, time: c ? +c.currentTime.toFixed(2) : 0, music: state.musicId, playing: !!state.music, finished: !!state.music?.finished,
        position: state.music?.position ?? null, intensity: state.music?.intensity ?? null, loading: !!state.loading, mix: engine?.mixIndex, scale: engine?.scaleMode, characterGain: engine?.busOutput('CHARACTER')?.gain?.value ?? null, speakerGains: engine ? Array.from({ length: 10 }, (_, k) => +(engine.speakerGain(k) ?? 0).toFixed(3)) : null, radioMode: settings.radioMode,
        challenge: { music: !!state.challengeMusic, type: state.challengeType ?? 0, prompt: !!state.challengePrompt }, // +0x5FD4 / +0x5FD0 / +0x5FD8
        speech: speech?.debug(), sfx: sfxGame?.debug(), world: world?.debug(), trigger: { ...state.trigger }, track: state.track, voices: sfx?.voiceCount ?? 0, pools: sfx?.pools, byTag: sfx?.byTag(),
        director: { free: state.free, course: state.course, dest: dir.dest, hubFirst: dir.hubFirst, bcIntro: dir.bcIntro, tmVariant: dir.tmVariant, firstVisit: dir.firstVisit.join(''),
          pending: Object.fromEntries(Object.entries(state.pendingDj).filter(([k, v]) => v && k !== 'sed' && k !== 'region')), timers: timers.map((t) => `${t.key}@${Math.round(t.at - now())}`), trace: trace.slice(-32) } };
    },
    // Front-end rider speech (web/rider-speech.js): Post_Selection / Customize through their Events.evt events.
    async speak(bank) { await whenReady(); await speech.init(); return speech.speakBank(bank); },
    sendEvent,
    // QA / tests: the director event timeline (see `tl`).
    timeline() { return timeline.slice(); },
    timelineReset() { timeline.length = 0; },
    // Tests / QA: the director's internal entry points (timer pump, painters, DJ timer, speech OnIdle).
    _music: () => state.music, // tests: the playing Pathfinder player
    _countdown: (d) => musicAdapter.countdown(d), // tests: 29C420's music part (web/sfx-game.js calls it on each countdown digit)
    _director: { pump: () => runTimers(), musicTrigger: (a, b) => musicTrigger(a, b), ambience: (v) => ambiencePainter(v), djTimer: (k, sed) => djTimer(k, sed), idle: () => onIdle() },
  };
  let worldDocCache = null;
  function worldDoc() {
    if (!state.courseCode) return null;
    if (worldDocCache?.code !== state.courseCode) { worldDocCache = { code: state.courseCode, doc: null }; json(`world/${state.courseCode}.json`).then((d) => { if (worldDocCache?.code === state.courseCode) worldDocCache.doc = d; }).catch(() => {}); }
    return worldDocCache.doc;
  }
  return api;
}
