// The time-attack screens (pv onlineTimeAttack; docs/big-multiplayer.md "UI"), made from the PS2's own online screens
// (FE.LUI / OV.LUI, tools/export_online_screens.py -> UI/online-screens.json, UI/online-overlays.json), played by web/lui-player.js:
//   ta-lobby     231lobby_menu (set up at 0x1BCB58): the sessions in the channel rows, the focused one's players in the room panel,
//                Create Room
//   ta-create    232CreateRoomPopup (0x1BE7D8) over the lobby: Event (the Channel Name row), Room Name (the game keyboard), Time limit
//                (the Password row), Done
//   ta-room      235chat_room (0x1B1F88): the session's players, the focused one's rank / best / rider / state, the message history
//                (the event, the phase and its time left); Select rides the event, Help opens the standings, Previous leaves
//   ta-standings 249Stats (0x1C2D18): the window's standings, ten rows a page with the slider, the event and its course
//   ta-results   the window's results: the PS2's Single Event results panel (OV_darkblue + 43final_standings, web/results-lui.js) with
//                the gold / silver / bronze hexes on the top three (the user's rules, 2026-10-13); after the podium cutscene, or in its
//                place on the courses without podium steps (the backcountry: the PS2 never shows a podium there; its Single Event
//                results with medals are the closest presentation it has)
//   ta-vote      the same results panel with three thumbnail boxes across the bottom: two events and RANDOM (the user's rules). A box
//                is the event's Select Event map (MAPGFX_map_peak<A|B|C> with the event's route red, web/fe-event-select.js: the game's
//                own event art; there is no course picture on the disc) and its name; RANDOM is the whole mountain (MAPGFX_map_mtn)
//                under the GL '?'; each box shows its live votes. Left / Right pick, Select votes
//   ta-circuit   109MultiplayerStandings (0x24AE90) on the OV_darkblue panel: the session's rounds (event, mode, winner) and its leader
//                (from the standings: Help)
// The screens keep their PS2 layout, fonts, colours and focus frames; their texts are the session's (port-only labels where the PS2 had
// online-service rows: channels, passwords, EA Messenger, which are hidden). The behaviour behind each PS2 screen (its channels, room
// passwords, buddies) is not ported: these are new screens in the PS2's clothes.
// Also: the phase-driven moves (the podium cutscene with each rider's place and name over its step, the results, the vote, the room when
// time is up) and the in-race HUD line.
import { LuiScreen } from './lui-player.js';
import { raceTime } from './race-time.mjs';
import { eventEntry } from './net/ta-game.js';
import { MEDAL } from './career.js';
import { PATHS, PATH_RED } from './fe-event-select.js';
import { drawPlaceLabel } from './race-place-hud.js';
import { toScene } from './cutscenes.js';
import { HUD_FONTS } from './trick-hud.js';
import { outfitPreviewEntry } from './wardrobe.js';

const SY = 448 / 480;
const FPS = 60;
const ROOT = '/assets/UI/';
export const TA_SCREENS = ['ta-lobby', 'ta-create', 'ta-room', 'ta-standings', 'ta-results', 'ta-vote', 'ta-circuit'];
// the vote's third option (web/server/sessions.mjs RANDOM)
const RANDOM = 'random';
// the vote boxes: three across the bottom, under the results panel
const BOX = { y: 362, w: 176, h: 82, gap: 22, thumbH: 56 };
// GL_1-1 (the loading screen's page): the orange '?' cell
const QUESTION = [164, 2, 34, 50];
// a rider's name label sits this far above its podium step (source cm)
const LABEL_HEIGHT = 235;
const MODE_NAMES = ['Race', 'Slope Style', 'Super Pipe', 'Big Air', 'Rival Time', 'Rival Points'];
const MODE_TAB = [1, 2, 2, 2, 1, 2];

// Element names (the exports' hashes) and focus frames, from the screens' timelines (labelled frames).
const LOBBY = {
  intro: 25,
  rowFocus: [35, 40, 45, 50],
  createFocus: 55,
  playerFocus: 65,
  title: '09e16025',
  rowTitles: ['08d4ec5c', '0dd37f9c', '0162f99c', '0a5d147c'],
  rowValues: ['00206bdc', '031ff81c', '02abb51c', '0e6980bc'],
  create: '015bd59d',
  inRoom: '0a05963d',
  players: Array.from({ length: 10 }, (_, i) => `079d637${i}`),
  // the channel locks and arrows, Help, EA Messenger and their key caps, the message counts
  hidden: ['01b76bc2', '01af7754', '09fc9605', '0a0df935', '0aee82e2', '0af69e74', '06b52385', '06c44cb5', '06cd32d3', '03a143e3', '07a85645',
    '04c4c960', '06a068e5']
};
const CREATE = {
  intro: 25,
  focus: [35, 45, 55, 65],
  title: '0c55e7c5',
  labels: ['0b583635', '0377a3b5', '0dbf40e4'],
  values: ['049299d5', '0f17ca35', '001f6964'],
  done: '0cf8c105',
  hidden: ['06cd32d3', '07a85645']
};
const ROOM = {
  intro: 25,
  name: '013bb775',
  players: Array.from({ length: 6 }, (_, i) => `06912b5${i}`),
  bars: Array.from({ length: 6 }, (_, i) => `0b8304c${i + 1}`),
  detail: { name: '0635b795', rank: '07667fe7', best: '07696314', rider: '0761795f', state: '0d43c2d3' },
  chat: '0fc3bd04',
  select: '049354e4',
  help: '0fa3e350',
  third: '053f5323',
  // EA Messenger, Type Message and their key caps, the message counts (Previous / Esc leaves the session, as every screen's Previous)
  hidden: ['0ef065a2', '04c4c960', '000004f1', '053f5323', '06a068e5', '081a8545']
};
const STATS = {
  intro: 25,
  title: '005b0b25',
  tabs: ['0c88c86c', '0879b54c', '0273ccac'],
  event: '0c59a924',
  course: '09937d24',
  value: '053ccb33',
  ranks: Array.from({ length: 10 }, (_, i) => `099f46b${i}`),
  values: Array.from({ length: 10 }, (_, i) => `0ae6cb5${i}`),
  users: Array.from({ length: 10 }, (_, i) => `0b9a0d7${i}`),
  slider: '07a2fac2',
  // Help, EA Messenger and their key caps, the message counts
  hidden: ['090d5ba3', '04c4c960', '06cd32d3', '04a08a25', '05a8b885']
};
const CIRCUIT = {
  intro: 30,
  focus: [45, 50, 55],
  current: '01cd9763',
  final: '0b0b7847',
  events: ['030ae1b5', '03eae1b5', '03cae1b5', '03aae1b5', '038ae1b5', '026ae1b5'],
  modes: ['03119b65', '031f9b65', '031d9b65', '031b9b65', '03199b65', '03079b65'],
  winners: ['00007331', '00007332', '00007333', '00007334', '00007335', '00007336'],
  ranks: ['00000331', '00000332', '00000333', '00000334', '00000335', '00000336', '00000337'],
  leading: '09e54c44',
  won: '0eefdc04',
  name: '09d838d5',
  items: ['0263fe50', '0263fe51', '0263fe52']
};

const minutes = (ms) => `${Math.floor(ms / 60000)}:${String(Math.floor((ms % 60000) / 1000)).padStart(2, '0')}`;

export class TaScreens {
  constructor(ui, ta) {
    this.ui = ui;
    this.ta = ta;
    this.lui = {};
    this.data = null;
    this.enterAt = 0;
    this.focusAt = 0;
    this.screen = null;
    this.form = { event: -1, name: '', limitMin: 6 };
    this.podiumShown = 0;
    this.voteShown = 0;
    this.watch = setInterval(() => this.follow(), 250);
  }
  get ready() {
    return !!this.data;
  }
  now() {
    return (performance.now() * FPS) / 1000;
  }
  async load() {
    try {
      const [fe, ov] = await Promise.all(['online-screens.json', 'online-overlays.json'].map(async (f) => (await fetch(ROOT + f)).json()));
      const images = { ...(this.ui.characterSelect?.images || {}) };
      const pages = [...new Set([...(fe.pages || []), ...(ov.pages || [])])].filter((p) => !images[p]);
      await Promise.all(pages.map(async (p) => {
        try {
          const im = new Image();
          im.src = ROOT + p + '.png';
          await im.decode();
          images[p] = im;
        } catch {}
      }));
      for (const [name, screen] of Object.entries({ ...fe.screens, ...ov.screens })) this.lui[name] = new LuiScreen(screen, images, this.ui);
      // the podium steps of each course (CUTSCENES/locators.json, anchors 20 / 21 / 22: the podium scripts stand bindings 1 / 2 / 3 there)
      try {
        this.locators = (await (await fetch('/assets/CUTSCENES/locators.json')).json()).locations;
      } catch {
        this.locators = null;
      }
      try {
        const gl = new Image();
        gl.src = '/assets/LOADING/GL_1-1.png';
        await gl.decode();
        this.gl = gl;
      } catch {
        this.gl = null;
      }
      this.thumbs = new Map();
      this.data = { fe, ov };
      return true;
    } catch (e) {
      console.warn('Time attack screens unavailable (python3 tools/export_online_screens.py)', e);
      return false;
    }
  }
  owns(screen) {
    return this.ready && TA_SCREENS.includes(screen);
  }
  get st() {
    return this.ta.state;
  }
  // ---- what the screens show ----
  eventName(key) {
    const entry = eventEntry(this.ui.courses, key);
    return entry?.name ?? key ?? '';
  }
  modeName(key) {
    return MODE_NAMES[+String(key).split(':')[0]] ?? '';
  }
  value(key, v) {
    if (v == null) return '';
    return +String(key).split(':')[0] === 0 || +String(key).split(':')[0] === 4 ? raceTime(v, false) : String(v);
  }
  phaseLine() {
    const s = this.st.session;
    if (!s) return '';
    const left = minutes(this.ta.timeLeft());
    const name = this.eventName(s.window?.event ?? s.event);
    if (s.phase === 'load') return `${name}: loading`;
    if (s.phase === 'window') return `${name}: ${left} left`;
    if (s.phase === 'overtime') return `${name}: time is up, last runs finishing`;
    if (s.phase === 'podium') return `${name}: results`;
    if (s.phase === 'vote') return `Vote for the next event: ${Math.ceil(this.ta.timeLeft() / 1000)} s`;
    return `Next: ${this.eventName(s.voteResult?.event)}`;
  }
  sessions() {
    return this.st.sessions ?? [];
  }
  rotation() {
    const keys = [];
    for (const c of this.ui.courses ?? []) {
      if (c.freeRide || c.ready === false) continue;
      const mode = c.rivalMode ?? { race: 0, slopestyle: 1, superpipe: 2, bigair: 3 }[c.event];
      if (mode != null) keys.push(`${mode}:${c.code}`);
    }
    return keys;
  }
  // ---- ui.js delegation (through web/mp-ui.js) ----
  items(screen) {
    const s = this.st;
    if (screen === 'ta-lobby') return [...this.sessions().map((x) => x.name), 'Create Room'];
    if (screen === 'ta-create') return ['Event', 'Room Name', 'Time limit', 'Done'];
    if (screen === 'ta-room') return [...s.roster.values()].map((m) => m.name).concat(s.roster.size ? [] : ['']);
    if (screen === 'ta-standings') return (s.results?.window === s.session?.window?.id ? s.results.standings : s.standings ?? []).map((r) => r.name).concat(['']);
    if (screen === 'ta-vote') return (s.session?.candidates ?? []).map((k) => (k === RANDOM ? 'Random' : this.eventName(k)));
    if (screen === 'ta-results') return ['Continue'];
    if (screen === 'ta-circuit') return ['Continue', 'Replay', 'Quit'];
    return [];
  }
  disabled(screen, i) {
    if (screen === 'ta-lobby') {
      const x = this.sessions()[i];
      return !!x && x.players >= x.maxPlayers;
    }
    if (screen === 'ta-circuit') return i === 1;
    return false;
  }
  layout(screen, i) {
    if (screen === 'ta-lobby') {
      const k = i - this.lobbyTop();
      return i >= this.sessions().length ? [93, 318 * SY, 222, 22 * SY] : [90, (125 + 45 * Math.max(0, Math.min(3, k))) * SY, 225, 40 * SY];
    }
    if (screen === 'ta-create') return i < 3 ? [67, (130 + 33 * i) * SY, 290, 26 * SY] : [175, 245 * SY, 310, 30 * SY];
    if (screen === 'ta-vote') return [this.boxX(i), BOX.y, BOX.w, BOX.h];
    if (screen === 'ta-results') return [450, 299, 126, 17];
    if (screen === 'ta-circuit') return [470, (345 + 18 * i) * SY, 110, 17 * SY];
    if (screen === 'ta-room') return [82, (112 + 21 * Math.min(5, i)) * SY, 236, 20 * SY];
    return [90, (200 + 20 * (i % 10)) * SY, 480, 18 * SY];
  }
  lobbyTop() {
    return Math.max(0, Math.min(this.sessions().length - 4, this.ui.index - 3));
  }
  open() {
    this.ta.list();
    this.ui.set(this.st.session ? 'ta-room' : 'ta-lobby');
    this.ui.index = 0;
    this.ui.sync();
  }
  choose(i) {
    const ui = this.ui;
    const screen = ui.screen;
    if (screen === 'ta-lobby') {
      const x = this.sessions()[i];
      if (x) {
        if (!this.disabled(screen, i)) this.ta.join(x.id);
      } else this.go('ta-create', 0);
      return;
    }
    if (screen === 'ta-create') {
      if (i === 1) this.nameKeyboard();
      else if (i === 3) {
        const keys = this.rotation();
        const event = this.form.event >= 0 ? keys[this.form.event] : undefined;
        this.ta.create({ name: this.form.name || undefined, limitMs: this.form.limitMin * 60000, event });
      } else this.step(i, 1);
      return;
    }
    if (screen === 'ta-room') {
      this.ride();
      return;
    }
    if (screen === 'ta-vote') {
      this.ta.vote(i);
      return;
    }
    if (screen === 'ta-results') {
      this.go('ta-room', 0);
      return;
    }
    if (screen === 'ta-circuit') {
      if (i === 0) this.go(this.st.session?.phase === 'vote' ? 'ta-vote' : 'ta-room', 0);
      else if (i === 2) this.leave();
      return;
    }
  }
  back() {
    const screen = this.ui.screen;
    if (screen === 'ta-lobby') this.go('mp-lobbies', 0);
    else if (screen === 'ta-create') this.go('ta-lobby', this.sessions().length);
    else if (screen === 'ta-room') this.leave();
    else if (screen === 'ta-standings') this.go(this.returnTo ?? 'ta-room', 0);
    else if (screen === 'ta-vote' || screen === 'ta-results') this.go('ta-room', 0);
    else if (screen === 'ta-circuit') this.go('ta-standings', 0);
  }
  key(e) {
    if (!this.owns(this.ui.screen)) return false;
    const fe = this.ui.feScreens;
    if (fe?.keyboard?.kind === 'record' && this.ui.screen === 'ta-create') {
      fe.keyboardKey(e);
      this.ui.sync();
      return true;
    }
    if (this.ui.screen === 'ta-create' && (e.code === 'ArrowLeft' || e.code === 'ArrowRight')) {
      e.preventDefault();
      this.step(this.ui.index, e.code === 'ArrowLeft' ? -1 : 1);
      return true;
    }
    if (this.ui.screen === 'ta-room' && (e.code === 'ShiftLeft' || e.code === 'ShiftRight') && !e.repeat) {
      e.preventDefault();
      this.returnTo = 'ta-room';
      this.go('ta-standings', 0);
      return true;
    }
    // the standings' Help: the session's rounds (109MultiplayerStandings)
    if (this.ui.screen === 'ta-standings' && (e.code === 'ShiftLeft' || e.code === 'ShiftRight') && !e.repeat) {
      e.preventDefault();
      this.go('ta-circuit', 0);
      return true;
    }
    // the vote boxes run left to right
    if (this.ui.screen === 'ta-vote' && (e.code === 'ArrowLeft' || e.code === 'ArrowRight')) {
      e.preventDefault();
      const n = this.items('ta-vote').length;
      this.ui.index = n ? (this.ui.index + (e.code === 'ArrowLeft' ? n - 1 : 1)) % n : 0;
      this.ui.sync();
      return true;
    }
    return false;
  }
  step(i, d) {
    if (i === 0) {
      const n = this.rotation().length;
      this.form.event = ((this.form.event + 1 + d + n + 1) % (n + 1)) - 1;
    } else if (i === 2) this.form.limitMin = Math.max(1, Math.min(30, this.form.limitMin + d));
    this.ui.sync();
  }
  nameKeyboard() {
    const fe = this.ui.feScreens;
    if (!fe?.openKeyboard) return;
    fe.openKeyboard('record', { text: this.form.name || fe.playerName || '', done: (name) => {
      this.form.name = String(name || '').trim();
      this.ui.sync();
    } });
  }
  go(screen, index = 0) {
    this.ui.set(screen);
    this.ui.index = index;
    this.ui.sync();
  }
  leave() {
    this.ta.leave();
    this.go('ta-lobby', 0);
  }
  // Select in the room: back to the event's objectives card (the window open), loading it first when the page is elsewhere
  ride() {
    const s = this.st.session;
    if (!s) return;
    if (s.phase !== 'window' && s.phase !== 'load') {
      this.ui.mpUI?.flash?.('Time is up for this event', 3000);
      return;
    }
    const ev = this.ui.careerUI?.career?.active?.ev;
    const entry = eventEntry(this.ui.courses, s.window?.event ?? s.event);
    if (ev && entry && this.ui.course?.code === entry.code) this.go('ctm-objectives', 0);
    else this.ta.enter();
  }
  // ---- the session's phases move the page (every 250 ms) ----
  follow() {
    const s = this.st.session;
    const ui = this.ui;
    if (!s || !this.ready) return;
    const screen = ui.screen;
    const inEvent = ['game', 'pause', 'ctm-objectives', 'ctm-results', 'ctm-award', 'ctm-records', 'results'].includes(screen) || TA_SCREENS.includes(screen);
    // the podium takes a rider still on course too: after the overtime its run can no longer count
    if (s.phase === 'podium' && this.st.results?.window === s.window?.id && this.podiumShown !== s.window.id && inEvent) {
      this.podiumShown = s.window.id;
      if (this.st.attemptOpen) this.ta.endAttempt('time');
      this.podium();
      return;
    }
    if (s.phase === 'vote' && this.voteShown !== s.window.id && screen !== 'cutscene' && (inEvent || screen === 'ta-results')) {
      this.voteShown = s.window.id;
      this.go('ta-vote', 0);
      return;
    }
    // time is up while the page waits on the card: the room
    if (s.phase === 'overtime' && screen === 'ctm-objectives') this.go('ta-room', 0);
    // the event left (Pause > Quit, the results' Continue to Select Event): the session goes on in the room
    if (['main', 'title', 'fe-peak', 'fe-mode', 'fe-event'].includes(screen)) this.go('ta-room', 0);
  }
  // The CTM podium cutscene (web/cutscenes.js, WS12 0x27AC60) with the top three in their own characters and outfits where the course
  // has its steps (the winner's win_ps scene: bindings 1 / 2 / 3 on anchors 20 / 21 / 22), each labelled with its place and name;
  // then (or at once, without steps) the results with the medals.
  podium() {
    const r = this.st.results;
    const after = () => {
      this.labels = null;
      if (this.st.session?.phase === 'podium') this.go('ta-results', 0);
    };
    const roles = (r?.podium ?? []).map((x) => this.role(x)).filter(Boolean);
    const code = String(r?.event ?? '').split(':')[1];
    if (!r?.podiumCourse || !roles.length || !this.ui.cb.cutscene) {
      after();
      return;
    }
    this.labels = { code, rows: r.podium.slice(0, 3) };
    try {
      Promise.resolve(this.ui.cb.cutscene({ kind: 'podium', place: 0, roles })).then(after, after);
    } catch {
      after();
    }
  }
  // a podium rider as the cutscene cast takes it: the roster entry of its character (a cheat skin over its base). The front end's
  // preview models exist for the default outfits and for this page's own worn outfit (wardrobe.js outfitPreviewEntry), so another
  // player's outfit stands in its character's default one.
  role(x) {
    const known = (this.ui.riders || []).find((e) => e.id === x.rider);
    if (!known) return x.rider ?? null;
    let entry = { ...known, ...(x.base ? { kind: 'cheat', base: x.base } : {}) };
    if (x.id === this.st.you?.id || x.id === this.ta.myId?.()) entry = outfitPreviewEntry(entry);
    return { entry, character: known.character ?? null, scale: null };
  }
  // Over the podium cutscene: each rider's place (the race HUD's own number and suffix) and name above its step.
  cutsceneOverlay(c) {
    const l = this.labels;
    const cam = this.ta.view?.camera?.();
    const origin = this.ta.view?.origin;
    const steps = this.locators?.[l?.code]?.locators?.['0']?.podium_steps;
    if (!l || !cam || !origin || !steps || this.ui.screen !== 'cutscene') return;
    cam.updateMatrixWorld?.();
    l.rows.forEach((row, place) => {
      const p = steps[String(20 + place)];
      if (!p) return;
      const [x, y, z] = toScene([p[0], p[1], p[2] + LABEL_HEIGHT], origin);
      const v = { x, y, z };
      const e = cam.matrixWorldInverse.elements;
      const vx = e[0] * v.x + e[4] * v.y + e[8] * v.z + e[12];
      const vy = e[1] * v.x + e[5] * v.y + e[9] * v.z + e[13];
      const vz = e[2] * v.x + e[6] * v.y + e[10] * v.z + e[14];
      const q = cam.projectionMatrix.elements;
      const w = q[3] * vx + q[7] * vy + q[11] * vz + q[15];
      if (!(w > 0)) return;
      const sx = ((q[0] * vx + q[4] * vy + q[8] * vz + q[12]) / w + 1) * 320;
      const sy = (1 - (q[1] * vx + q[5] * vy + q[9] * vz + q[13]) / w) * 224;
      const scale = 0.6;
      const width = (place === 0 ? 66 : 80) * scale;
      drawPlaceLabel(this.ui, c, place, sx - width / 2, sy - 42 * scale - 4, scale);
      // the name in the front-end font, with the dark shadow of the world captions (cutscenes.js loadingCaption)
      this.ui.text(c, row.name, sx + 1.5, sy + 1.5, 15, '#1a1a10', 'FEFONT', 'center');
      this.ui.text(c, row.name, sx, sy, 15, '#f2f5ec', 'FEFONT', 'center');
    });
  }
  // ---- drawing ----
  events(lui, intro, focus) {
    const out = [];
    for (const ev of lui.screen.events) {
      if (ev.frame <= intro) out.push({ ev, start: ev.frame });
      else if (ev.frame === focus) out.push({ ev, start: focus });
    }
    return out;
  }
  drawScreen(c, name, intro, focus, override) {
    const lui = this.lui[name];
    c.save();
    c.scale(1, SY);
    lui.draw(c, this.events(lui, intro, focus), Math.max(intro, focus ?? 0) + 1000, override);
    c.restore();
  }
  draw(c, b) {
    const screen = this.ui.screen;
    if (screen !== this.screen) {
      this.screen = screen;
      this.enterAt = this.now();
    }
    if (screen !== 'ta-circuit') {
      b.fillStyle = '#75a9cb';
      b.fillRect(0, 0, 640, 448);
    }
    if (screen === 'ta-lobby' || screen === 'ta-create') this.drawLobby(c, screen === 'ta-create');
    if (screen === 'ta-create') this.drawCreate(c);
    if (screen === 'ta-room') this.drawRoom(c);
    if (screen === 'ta-standings') this.drawStandings(c);
    if (screen === 'ta-results') this.drawResults(c, false);
    if (screen === 'ta-vote') this.drawResults(c, true);
    if (screen === 'ta-circuit') this.drawCircuit(c);
  }
  drawLobby(c, behind) {
    const list = this.sessions();
    const i = behind ? -1 : this.ui.index;
    const top = this.lobbyTop();
    const focusRow = i >= 0 && i < list.length ? i - top : -1;
    const focused = i >= 0 ? list[i] : null;
    const names = new Map(LOBBY.rowTitles.map((n, k) => [n, k]));
    const values = new Map(LOBBY.rowValues.map((n, k) => [n, k]));
    const players = new Map(LOBBY.players.map((n, k) => [n, k]));
    const hidden = new Set(LOBBY.hidden);
    const focus = behind ? null : focusRow >= 0 ? LOBBY.rowFocus[focusRow] : LOBBY.createFocus;
    this.drawScreen(c, '231lobby_menu', LOBBY.intro, focus, (e) => {
      if (hidden.has(e.name)) return { hidden: true };
      if (e.name === LOBBY.title) return { text: 'Time Attack' };
      if (names.has(e.name)) {
        const x = list[top + names.get(e.name)];
        return x ? { text: x.name } : { text: names.get(e.name) === 0 && !list.length ? 'No sessions yet' : '' };
      }
      if (values.has(e.name)) {
        const x = list[top + values.get(e.name)];
        return { text: x ? this.eventName(x.event) : '' };
      }
      if (e.name === LOBBY.inRoom) return { text: focused ? `${focused.players} in Room` : '' };
      if (players.has(e.name)) return { text: focused?.names?.[players.get(e.name)] ?? '' };
      return null;
    });
  }
  drawCreate(c) {
    const keys = this.rotation();
    const i = this.ui.index;
    const labels = ['Event', 'Room Name', 'Time limit'];
    const values = [this.form.event >= 0 ? this.eventName(keys[this.form.event]) : 'Random', this.form.name || this.ui.feScreens?.playerName || '',
      `${this.form.limitMin}:00`];
    const hidden = new Set(CREATE.hidden);
    this.drawScreen(c, '232CreateRoomPopup', CREATE.intro, CREATE.focus[Math.max(0, Math.min(3, i))], (e) => {
      if (hidden.has(e.name)) return { hidden: true };
      if (e.name === CREATE.title) return { text: 'Create Session' };
      const l = CREATE.labels.indexOf(e.name);
      if (l >= 0) return { text: labels[l] };
      const v = CREATE.values.indexOf(e.name);
      if (v >= 0) return { text: values[v] };
      return null;
    });
    const fe = this.ui.feScreens;
    if (fe?.keyboard?.kind === 'record') {
      c.save();
      c.scale(1, SY);
      fe.drawKeyboard(c, fe.now());
      c.restore();
    }
  }
  drawRoom(c) {
    const s = this.st;
    const roster = [...s.roster.values()];
    const i = Math.max(0, Math.min(roster.length - 1, this.ui.index));
    const top = Math.max(0, Math.min(roster.length - 6, i - 5));
    const focused = roster[i];
    const row = focused ? (s.standings ?? []).find((r) => r.id === focused.id) ?? (s.you?.id === focused.id ? s.you : null) : null;
    const key = s.session?.window?.event;
    const hidden = new Set([...ROOM.hidden, ...ROOM.bars]);
    this.drawScreen(c, '235chat_room', ROOM.intro, null, (e) => {
      if (hidden.has(e.name)) return { hidden: true };
      if (e.name === ROOM.name) return { text: s.session?.name ?? '' };
      const k = ROOM.players.indexOf(e.name);
      if (k >= 0) {
        const m = roster[top + k];
        const on = top + k === i;
        return { text: m ? `${m.name}${m.online === false ? ' (away)' : ''}` : '', props: on ? { 14: 255, 15: 255, 16: 255 } : undefined };
      }
      if (e.name === ROOM.detail.name) return { text: focused?.name ?? '' };
      if (e.name === ROOM.detail.rank) return { text: row ? `Rank: ${row.rank + 1}` : 'Rank: -' };
      if (e.name === ROOM.detail.best) return { text: row ? `Best: ${this.value(key, row.value)}` : 'Best: -' };
      if (e.name === ROOM.detail.rider) return { text: `Rider: ${focused?.rider ? focused.rider[0].toUpperCase() + focused.rider.slice(1) : '-'}` };
      if (e.name === ROOM.detail.state) return { text: focused?.loaded ? 'Ready' : 'Loading' };
      if (e.name === ROOM.chat) return { text: this.phaseLine() };
      if (e.name === ROOM.select) return { text: 'Ride' };
      if (e.name === ROOM.help) return { text: 'Standings' };
      if (e.name === ROOM.third) return { text: 'Leave' };
      return null;
    });
  }
  drawStandings(c) {
    const s = this.st;
    const final = s.results?.window === s.session?.window?.id ? s.results : null;
    const rows = final ? final.standings : s.standings ?? [];
    const key = final?.event ?? s.session?.window?.event;
    const page = Math.floor(Math.max(0, this.ui.index) / 10);
    const pages = Math.max(1, Math.ceil(rows.length / 10));
    const tab = MODE_TAB[+String(key).split(':')[0]] ?? 0;
    const hidden = new Set(STATS.hidden);
    const youId = s.you?.id;
    this.drawScreen(c, '249Stats', STATS.intro, null, (e) => {
      if (hidden.has(e.name)) return { hidden: true };
      if (e.name === STATS.title) return { text: final ? 'Final Standings' : 'Standings' };
      const t = STATS.tabs.indexOf(e.name);
      if (t >= 0) return t === tab ? null : { hidden: true };
      if (e.name === STATS.event) return { text: this.modeName(key) };
      if (e.name === STATS.course) return { text: this.eventName(key) };
      if (e.name === STATS.value) return { text: +String(key).split(':')[0] === 0 || +String(key).split(':')[0] === 4 ? 'Time' : 'Points' };
      const k = Math.max(STATS.ranks.indexOf(e.name), STATS.values.indexOf(e.name), STATS.users.indexOf(e.name));
      if (k >= 0) {
        const r = rows[page * 10 + k];
        const mine = r && r.id === youId;
        const props = mine ? { 14: 255, 15: 255, 16: 255 } : undefined;
        if (STATS.ranks[k] === e.name) return { text: r ? String(page * 10 + k + 1) : '', props };
        if (STATS.values[k] === e.name) return { text: r ? this.value(key, r.value) : '', props };
        return { text: r?.name ?? '', props };
      }
      if (e.name === STATS.slider && pages > 1) return { props: { 1: 18 + (page / (pages - 1)) * 180 } };
      return null;
    });
  }
  // The window's results in the PS2's Single Event results panel: the top six with the gold / silver / bronze hexes on the podium's
  // three; with the vote's boxes below it while the vote runs.
  drawResults(c, vote) {
    const st = this.st;
    const r = st.results;
    const rl = this.ui.careerUI?.resultsLui;
    const key = r?.event ?? st.session?.window?.event;
    const rows = (r?.standings ?? []).slice(0, 6);
    const kinds = [MEDAL.GOLD, MEDAL.SILVER, MEDAL.BRONZE];
    const medals = (r?.podium ?? []).slice(0, 3).map((p, k) => ({ row: rows.findIndex((x) => x.id === p.id), kind: kinds[k] }));
    const timed = +String(key).split(':')[0] === 0 || +String(key).split(':')[0] === 4;
    if (rl?.ready) {
      rl.results(c, {
        title: this.eventName(key),
        sub: 'Session Results',
        heads: ['Rank', 'Riders', timed ? 'Time' : 'Points'],
        rows: rows.map((x, i) => ({ rank: i + 1, name: x.name, value: this.value(key, x.value), human: x.id === st.you?.id })),
        medals,
        message: vote ? this.phaseLine() : st.session?.phase === 'podium' ? 'The vote for the next event follows.' : this.phaseLine(),
        items: vote ? [] : ['Continue'],
        index: 0,
        noItems: vote
      });
    }
    if (vote) this.drawBoxes(c);
  }
  boxX(i) {
    const total = 3 * BOX.w + 2 * BOX.gap;
    return 320 - total / 2 + i * (BOX.w + BOX.gap);
  }
  // The vote's boxes: the event's map art, its name on the menu rows' strip (the focused one orange with white text, as every online
  // screen's focus bar), and its votes in the corner.
  drawBoxes(c) {
    const s = this.st.session;
    const cands = s?.candidates ?? [];
    const votes = this.st.votes ?? [];
    cands.forEach((k, i) => {
      const x = this.boxX(i);
      const y = BOX.y;
      const on = this.ui.index === i;
      c.fillStyle = '#05121c';
      c.fillRect(x - 3, y - 3, BOX.w + 6, BOX.h + 6);
      const thumb = this.thumb(k);
      if (thumb) c.drawImage(thumb, x, y, BOX.w, BOX.thumbH);
      else {
        c.fillStyle = '#2f5f7f';
        c.fillRect(x, y, BOX.w, BOX.thumbH);
      }
      c.fillStyle = on ? '#9c3c06' : '#c9d6df';
      c.fillRect(x, y + BOX.thumbH, BOX.w, BOX.h - BOX.thumbH);
      this.ui.text(c, k === RANDOM ? 'Random' : this.eventName(k), x + BOX.w / 2, y + BOX.thumbH + 4, 15, on ? '#f4f6f2' : '#0f2533', 'FEFONT', 'center');
      // the votes: white on the dark frame's corner tab
      const n = votes[i] ?? 0;
      c.fillStyle = '#05121c';
      c.fillRect(x + BOX.w - 34, y - 3, 37, 20);
      this.ui.text(c, String(n), x + BOX.w - 15, y - 1, 14, this.st.myVote === i ? '#e8bd72' : '#f4f6f2', 'FEFONT', 'center');
    });
  }
  // An event's thumbnail: its peak map with only its route, red (the Select Event map, web/fe-event-select.js), cropped around the
  // route; RANDOM: the mountain with the GL '?'. Drawn once per event.
  thumb(key) {
    if (this.thumbs.has(key)) return this.thumbs.get(key);
    const es = this.ui.eventSelect;
    const out = document.createElement('canvas');
    out.width = BOX.w * 2;
    out.height = BOX.thumbH * 2;
    const o = out.getContext('2d');
    if (key === RANDOM) {
      const mtn = es?.images?.MAPGFX_map_mtn;
      if (!mtn) return null;
      o.drawImage(mtn, 0, 64, 256, 256 * (BOX.thumbH / BOX.w), 0, 0, out.width, out.height);
      if (this.gl) {
        const [qx, qy, qw, qh] = QUESTION;
        const h = out.height * 0.8;
        o.drawImage(this.gl, qx, qy, qw, qh, out.width / 2 - (h * qw) / qh / 2, out.height * 0.1, (h * qw) / qh, h);
      }
      this.thumbs.set(key, out);
      return out;
    }
    const code = String(key).split(':')[1];
    const label = PATHS[code];
    const peak = { A: 1, B: 1, C: 2, D: 2, E: 3 }[code?.[0]];
    if (!es?.map || !label || !peak) return null;
    const page = 'MAPGFX_map_peak' + 'ABC'[peak - 1];
    const draw = (withMap) => {
      const cv = document.createElement('canvas');
      cv.width = 640;
      cv.height = 480;
      es.map.draw(cv.getContext('2d'), [], 0, (e) => {
        if (e.kind === 'group' || e.kind === 'menu') return null;
        if (e.label === 'MapPic') return withMap ? { sprite: { page, sx: 0, sy: 0, sw: 256, sh: 256 } } : { hidden: true };
        if (e.label === label) return { props: es.recolour(e, PATH_RED) };
        return { hidden: true };
      });
      return cv;
    };
    // the route's box: where the path alone draws
    const d = draw(false).getContext('2d').getImageData(0, 0, 640, 480).data;
    let x0 = 640;
    let y0 = 480;
    let x1 = 0;
    let y1 = 0;
    for (let y = 0; y < 480; y++) {
      for (let x = 0; x < 640; x++) {
        if (d[(y * 640 + x) * 4 + 3] === 0) continue;
        x0 = Math.min(x0, x);
        x1 = Math.max(x1, x);
        y0 = Math.min(y0, y);
        y1 = Math.max(y1, y);
      }
    }
    if (x1 < x0) return null;
    const full = draw(true);
    // the map picture's own box: the crop stays inside it
    const m = full.getContext('2d').getImageData(0, 0, 640, 480).data;
    let mx0 = 640;
    let my0 = 480;
    let mx1 = 0;
    let my1 = 0;
    for (let y = 0; y < 480; y += 2) {
      for (let x = 0; x < 640; x += 2) {
        if (m[(y * 640 + x) * 4 + 3] === 0) continue;
        mx0 = Math.min(mx0, x);
        mx1 = Math.max(mx1, x);
        my0 = Math.min(my0, y);
        my1 = Math.max(my1, y);
      }
    }
    const aspect = BOX.thumbH / BOX.w;
    const w = Math.min(mx1 - mx0, Math.max(150, (x1 - x0) * 1.5, ((y1 - y0) * 1.4) / aspect));
    const h = Math.min(my1 - my0, w * aspect);
    const cx = Math.max(mx0 + w / 2, Math.min(mx1 - w / 2, (x0 + x1) / 2));
    const cy = Math.max(my0 + h / 2, Math.min(my1 - h / 2, (y0 + y1) / 2));
    o.drawImage(full, cx - w / 2, cy - h / 2, w, h, 0, 0, out.width, out.height);
    this.thumbs.set(key, out);
    return out;
  }
  drawCircuit(c) {
    const r = this.st.results;
    const history = (r?.history ?? []).slice(-6);
    const wins = new Map();
    for (const h of r?.history ?? []) if (h.winner) wins.set(h.winner, (wins.get(h.winner) ?? 0) + 1);
    const leader = [...wins.entries()].sort((a, b) => b[1] - a[1])[0]?.[0] ?? null;
    const winner = r?.podium?.[0]?.name ?? null;
    this.ui.careerUI?.resultsLui?.panel?.(c, '', '', { openOnly: true });
    const i = Math.max(0, Math.min(2, this.ui.index));
    this.drawScreen(c, '109MultiplayerStandings', CIRCUIT.intro, CIRCUIT.focus[i], (e) => {
      if (e.name === CIRCUIT.final) return { hidden: true };
      const k = Math.max(CIRCUIT.events.indexOf(e.name), CIRCUIT.modes.indexOf(e.name), CIRCUIT.winners.indexOf(e.name));
      if (k >= 0) {
        const h = history[k];
        if (CIRCUIT.events[k] === e.name) return { text: h ? this.eventName(h.event) : '' };
        if (CIRCUIT.modes[k] === e.name) return { text: h ? this.modeName(h.event) : '' };
        return { text: h ? (h.winner ? `${h.winner} ${this.value(h.event, h.value)}` : '-') : '' };
      }
      const rank = CIRCUIT.ranks.indexOf(e.name);
      if (rank >= 0) return rank < history.length ? null : { hidden: true };
      if (e.name === CIRCUIT.leading) return leader && leader !== winner ? { text: `${leader} is leading this session.` } : { hidden: true };
      if (e.name === CIRCUIT.won) return winner ? { text: `Congratulations ${winner}!  You have won ${this.eventName(r.event)}.` } : { hidden: true };
      if (e.name === CIRCUIT.name) return { text: this.st.session?.name ?? '' };
      if (e.name === CIRCUIT.items[1]) return { alpha: 128 };
      return null;
    });
  }
  // The in-race HUD of a session (port-only arrangement of the race HUD's own elements):
  //   * the session's standings in the 3-row standings of descriptor 0x17 (web/trick-hud.js standings, 0x1ED104..0x1ED4AC: the rank
  //     digit with its 'ST' / 'ND' / 'RD' at half scale, the value column right-aligned at x + 66.997 x scale, rows 21 apart, the
  //     HUD font's shadow; this player's row as the human row: the name cut to three characters over the red box), the values the
  //     session's times or scores; in the place of a freestyle event's posted standings (web/career-ui.js hud skips those in a session);
  //     this player in the third row when not in the top three;
  //   * the time left centred under the race clock (descriptor 0x1A) in the HUD font, red-blinking in its last 10 s as the freestyle
  //     clock (0x4C8688), TIME'S UP after the deadline.
  hud(c) {
    const s = this.st.session;
    if (!s || this.ui.screen !== 'game' || !this.st.attemptOpen) return;
    const left = this.ta.timeLeft();
    const red = s.phase !== 'window' || (left < 10000 && Math.floor(left / 500) % 2 === 0);
    const text = s.phase === 'window' ? minutes(left) : "TIME'S UP";
    this.ui.text(c, text, 320, 44, 15, red ? '#ff0000' : '#eef5ee', 'HUDFONT', 'center');
    const hud = this.ui.trickHud;
    const renderer = this.ui.trickHudRenderer;
    if (!hud || !renderer) return;
    const key = s.window?.event;
    const top = (this.st.standings ?? []).slice(0, 3);
    const you = this.st.you;
    const rows = top.map((r) => ({ rank: r.rank, text: this.value(key, r.value), human: r.id === you?.id, name: r.name }));
    if (you && !rows.some((r) => r.human)) rows.splice(Math.min(2, rows.length), 1, { rank: you.rank, text: this.value(key, you.value), human: true, name: you.name });
    if (!rows.length) return;
    const list = [];
    this.standingsList(hud, list, rows);
    renderer.render(c, list);
  }
  // trick-hud.js standings() with text values and the session's own ranks
  standingsList(hud, out, rows) {
    const f = Math.fround;
    const d = hud.desc(0x17);
    const [sx, sy] = d.scale;
    const argb = d.argb.slice();
    const shadow = hud.font.HUDFONT.shadow.slice();
    const pitch = f(f(HUD_FONTS.HUDFONT.height * 1) * sy);
    const col = f(d.x + f(sx * f(66.99718475341797)));
    const half = [f(sx * 0.5), f(sy * 0.5)];
    const put = (text, x, y, scale) => out.push({ kind: 'text', font: 'HUDFONT', text, x, y, scale, argb: argb.slice(), shadow: shadow.slice(),
      shadowArgb: [argb[0], 0, 0, 0], order: d.order });
    const widths = rows.map((r) => hud.measure('HUDFONT', r.text, sx, sy).w);
    const widest = Math.max(0, ...widths);
    rows.forEach((r, i) => {
      const y = f(d.y + f(pitch * i));
      put(r.text, f(col + f(widest - widths[i])), y, [sx, sy]);
      if (!r.human) {
        const digit = String(r.rank + 1);
        put(digit, d.x, y, [sx, sy]);
        const suffix = r.rank === 0 ? 'ST' : r.rank === 1 ? 'ND' : r.rank === 2 ? 'RD' : 'TH';
        put(suffix, f(f(d.x + 1) + hud.measure('HUDFONT', digit, sx, sy).w), f(y + f(1.5995806455612183)), half.slice());
      } else {
        put(String(r.name ?? '').slice(0, 3).toUpperCase(), d.x, y, [sx, sy]);
        out.push({ kind: 'rect', x: d.x, y, w: f(f(col - d.x) + widest), h: pitch, scale: [1, 1], argb: [1, 1, 0, 0], order: d.order - 1 });
      }
    });
  }
  dispose() {
    clearInterval(this.watch);
  }
}
