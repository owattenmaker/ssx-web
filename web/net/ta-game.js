// Time-attack sessions on the page (pv onlineTimeAttack; docs/big-multiplayer.md, the server is web/server/sessions.mjs).
// One controller per page, created by web/net/mp-game.js with the lobby client (web/net/mp-client.js) and installed as
// ui.timeAttack. It follows the session's phases from the server and drives the page's own Single Event flow:
//   load      the event's course and its objectives card (ui.startSingleEvent), no computer riders (ui.cb.soloLineupNext; the
//             backcountry rival events keep their rival), the maxed attribute bytes (ui.cb.attributeOverride); 'ta-loaded' once
//             the card is up
//   window    every Continue / Restart (web/career-ui.js play) asks attemptStart(): allowed from the window's start to its deadline,
//             'ta-attempt' n; every finish (career-ui finish) calls finish(): a new best is uploaded as the run's replay file
//             (POST /mp/session/run), the attempt ends with 'ta-end'
//   overtime  no new attempts; the run in progress may finish and upload
//   podium / vote / vote-result   the results and the vote (the screens are web/ta-ui.js, TA-5); then the next load
// The page's own run rules are untouched: a time-attack attempt is a Single Event run of the event. The session's state for the
// screens: session, roster, standings, you, results, votes.
import { encodeReplayFile } from '../server/replay-file.mjs';
import { BUNDLE } from '../server/ghost-packet.mjs';
import { createTaGhosts } from './ta-ghosts.js';
import { pv } from '../pv-flags.js';

export const MAX_ATTRIBUTES = Object.freeze([55, 55, 55, 55, 55, 55, 55]);
const LOAD_POLL_MS = 250;
const TIMED_MODES = new Set([0, 4]);
const RIVAL_MODES = new Set([4, 5]);

async function deflateRaw(bytes) {
  const s = new Blob([bytes]).stream().pipeThrough(new CompressionStream('deflate-raw'));
  return new Uint8Array(await new Response(s).arrayBuffer());
}

// The Single Event entry of a board event key "<mode>:<COURSE>" (web/main.js ui.courses: a backcountry course is listed twice, as
// Rival Time (rivalMode 4) and Rival Points (rivalMode 5)).
export function eventEntry(courses, key) {
  const [modeText, code] = String(key ?? '').split(':');
  const mode = +modeText;
  return (courses ?? []).find((c) => c.code === code && (RIVAL_MODES.has(mode) ? c.rivalMode === mode : !c.rivalMode)) ?? null;
}
// The better of two values of an event (records.mjs better without the submission order).
export const betterValue = (key, a, b) => (TIMED_MODES.has(+String(key).split(':')[0]) ? a < b : a > b);

// scene: {T, scene, load, loader, origin, getCore} from web/net/mp-game.js, for the ghosts (pv ghostRiders); without it no ghosts.
export function createTimeAttack({ ui, client, scene = null, fetchImpl = (...a) => globalThis.fetch(...a), base = '', log = console }) {
  const listeners = new Set();
  const state = {
    sessions: [],
    session: null,
    roster: new Map(),
    standings: null,
    you: null,
    results: null,
    votes: null,
    myVote: -1,
    attempt: 0,
    attemptOpen: false,
    best: null,
    loadedWindow: 0,
    upload: null,
    note: null
  };
  let loadTimer = null;
  // the session's players by their ghost index (web/server/sessions.mjs member.index)
  const byIndex = new Map();
  const reindex = () => {
    byIndex.clear();
    for (const m of state.roster.values()) if (m.index != null) byIndex.set(m.index, m);
  };
  const changed = () => {
    for (const f of listeners) {
      try {
        f(state);
      } catch (e) {
        log.warn?.('ta: listener failed', e);
      }
    }
    ui.sync?.();
  };
  const serverNow = () => (client.serverNow ? client.serverNow() : Date.now());
  const windowOpen = () => {
    const s = state.session;
    if (s?.phase !== 'window' || !s.window) return false;
    const t = serverNow();
    return t >= s.window.startAt && t < s.window.endAt;
  };
  const eventKey = () => state.session?.window?.event ?? state.session?.event ?? null;
  // the name the session shows: the player's own (Options > Player Name, web/fe-screens.js), else the rider's
  const playerName = () => ui.feScreens?.playerName || scene?.rider?.()?.name || 'Rider';
  const mode = () => +String(eventKey() ?? '').split(':')[0];

  // ---- the event on the page ----
  function armRun() {
    // no computer riders in the next event load / heat rebuild (a rival event keeps its rival: ai-race.js prepareRival)
    if (!RIVAL_MODES.has(mode())) ui.cb.soloLineupNext?.(true);
    ui.cb.attributeOverride?.(MAX_ATTRIBUTES.slice());
  }
  function disarm() {
    ui.cb.soloLineupNext?.(false);
    ui.cb.attributeOverride?.(null);
  }
  function enterEvent() {
    const s = state.session;
    const entry = eventEntry(ui.courses, s.window?.event ?? s.event);
    if (!entry) {
      state.note = 'This event is not available on this page';
      changed();
      return;
    }
    // the attempt numbers only grow within a window (the server refuses a repeated one): a new window starts them again
    if (state.attemptWindow !== s.window?.id) {
      state.attemptWindow = s.window?.id;
      state.attempt = 0;
      state.best = null;
      state.results = null;
    }
    state.attemptOpen = false;
    armRun();
    ui.startSingleEvent(entry);
    // loaded = the event's objectives card is up (career-ui single -> the load screen -> 'ctm-objectives')
    clearInterval(loadTimer);
    const windowId = s.window?.id;
    loadTimer = setInterval(() => {
      if (!state.session || state.session.window?.id !== windowId) {
        clearInterval(loadTimer);
        return;
      }
      if (ui.screen === 'ctm-objectives' && state.loadedWindow !== windowId) {
        state.loadedWindow = windowId;
        client.session({ t: 'ta-loaded', window: windowId });
        clearInterval(loadTimer);
        changed();
      }
    }, LOAD_POLL_MS);
  }

  // ---- server messages ----
  client.on('ta-sessions', (m) => {
    state.sessions = m.sessions ?? [];
    changed();
  });
  function applyState(s) {
    const before = state.session;
    state.session = s;
    // a new window loads its event; so does a player who joins while the window is open
    const fresh = !before || before.window?.id !== s.window?.id;
    if ((s.phase === 'load' && (fresh || before.phase !== 'load')) || (s.phase === 'window' && fresh)) enterEvent();
    if (s.phase === 'vote' && before?.phase !== 'vote') {
      state.myVote = -1;
      state.votes = s.candidates.map(() => 0);
    }
    changed();
  }
  client.on('ta-joined', (m) => {
    state.roster = new Map((m.roster ?? []).map((p) => [p.id, p]));
    reindex();
    state.standings = m.standings ?? [];
    state.you = state.standings.find((r) => r.id === m.you) ?? null;
    state.results = m.results ?? null;
    const same = state.session && state.session.id === m.session.id && state.session.window?.id === m.session.window?.id;
    // a resume into the same window keeps the page's event as it is
    if (same) {
      state.session = m.session;
      changed();
    } else {
      state.session = null;
      applyState(m.session);
    }
  });
  client.on('ta-state', (m) => applyState(m.session));
  client.on('ta-roster', (m) => {
    if (m.add) state.roster.set(m.add.id, m.add);
    if (m.update) state.roster.set(m.update.id, m.update);
    if (m.drop) {
      const gone = state.roster.get(m.drop);
      if (gone?.index != null) ghosts?.drop(gone.index);
      state.roster.delete(m.drop);
    }
    reindex();
    if (m.host && state.session) state.session = { ...state.session, hostId: m.host };
    changed();
  });
  client.on('ta-standings', (m) => {
    if (m.window !== state.session?.window?.id) return;
    state.standings = m.rows ?? [];
    state.you = m.you ?? null;
    state.total = m.total ?? state.standings.length;
    changed();
  });
  client.on('ta-votes', (m) => {
    state.votes = m.counts;
    changed();
  });
  client.on('ta-results', (m) => {
    state.results = m;
    changed();
  });
  client.on('ta-left', () => {
    clearInterval(loadTimer);
    ghosts?.clear();
    state.session = null;
    state.roster = new Map();
    state.standings = null;
    state.you = null;
    state.results = null;
    disarm();
    changed();
  });

  // ---- ghosts (pv ghostRiders) ----
  const ghosts = scene && pv('ghostRiders')
    ? createTaGhosts({ ...scene, client, ui, members: () => byIndex, serverNow, riding: () => state.attemptOpen && ui.screen === 'game', attempt: () => state.attempt })
    : null;
  client.on('state', (frame) => {
    if (frame[0] === BUNDLE && state.session) ghosts?.receive(frame);
  });

  // ---- uploads ----
  async function upload(meta, pad, attempt, windowId, sessionId) {
    const body = await deflateRaw(encodeReplayFile({ meta, pad }));
    const url = `${base}/mp/session/run?session=${encodeURIComponent(sessionId)}&window=${windowId}&attempt=${attempt}`;
    const r = await fetchImpl(url, { method: 'POST', headers: { 'x-ssx-session': client.token }, body });
    let res = null;
    try {
      res = await r.json();
    } catch {
      res = { error: String(r.status) };
    }
    return res;
  }

  const api = {
    state,
    ghosts,
    // the page's scene pieces (camera, origin), for web/ta-ui.js's podium labels
    view: scene,
    // this page's player id in the session
    myId: () => client.state?.id ?? null,
    // web/net/mp-game.js aiAllowed(), right before an event's computer-rider lineup is prepared (main.js), on whichever course's race is
    // current: a course switch builds a new one, so the solo lineup is armed there, not only at the event's start
    armLineup() {
      if (state.session) armRun();
    },
    on(f) {
      listeners.add(f);
      return () => listeners.delete(f);
    },
    // joined to a session: the career-ui hooks (play / finish) are this module's
    get active() {
      return !!state.session;
    },
    get windowOpen() {
      return windowOpen();
    },
    // milliseconds left in the window (or the phase), on the server clock
    timeLeft() {
      const s = state.session;
      if (!s) return 0;
      const end = s.phase === 'window' ? s.window?.endAt : s.phaseEndsAt;
      return Math.max(0, (end ?? 0) - serverNow());
    },
    list() {
      client.session({ t: 'ta-list' });
    },
    // the session's event on the page again (web/ta-ui.js: the room's Ride when the page is elsewhere)
    enter() {
      if (state.session) enterEvent();
    },
    create({ name, limitMs, maxPlayers, event } = {}) {
      client.session({ t: 'ta-create', name, limitMs, maxPlayers, event, playerName: playerName() });
    },
    join(id) {
      client.session({ t: 'ta-join', session: id, playerName: playerName() });
    },
    leave() {
      if (state.attemptOpen) api.endAttempt('quit');
      client.session({ t: 'ta-leave' });
    },
    config(limitMs) {
      client.session({ t: 'ta-config', limitMs });
    },
    vote(choice) {
      if (state.session?.phase !== 'vote') return false;
      state.myVote = choice;
      client.session({ t: 'ta-vote', choice });
      changed();
      return true;
    },
    // web/career-ui.js play(): an attempt may start inside the window only. true = start it.
    attemptStart() {
      if (!state.session) return true;
      if (!windowOpen()) {
        const s = state.session;
        state.note = s.phase === 'window' || s.phase === 'load' ? 'The session starts in a moment' : 'Time is up for this event';
        ui.mpUI?.flash?.(state.note, 3000);
        changed();
        return false;
      }
      if (state.attemptOpen) api.endAttempt('restart');
      state.attempt++;
      state.attemptOpen = true;
      state.note = null;
      armRun();
      client.session({ t: 'ta-attempt', window: state.session.window.id, n: state.attempt });
      return true;
    },
    endAttempt(reason, uploading = false) {
      if (!state.attemptOpen || !state.session) return;
      state.attemptOpen = false;
      client.session({ t: 'ta-end', window: state.session.window.id, n: state.attempt, reason, uploading });
    },
    // web/career-ui.js finish(result, ev): the attempt's end; a new best uploads its replay. Returns true (the session took it:
    // no records screen) while in a session.
    finish(result) {
      if (!state.session) return false;
      armRun();
      if (!state.attemptOpen) return true;
      const key = eventKey();
      const info = ui.cb.onlineRun?.();
      const timed = TIMED_MODES.has(mode());
      const fin = info?.finish;
      const value = timed ? (result.dnf || !fin ? null : Math.round(fin.ticks)) : fin ? fin.score | 0 : null;
      if (result.dnf || info?.giveUp || !(value > 0) || !info?.available) {
        api.endAttempt(result.dnf || info?.giveUp ? 'gave up' : 'finish');
        return true;
      }
      const mine = state.you?.value ?? state.best;
      if (mine != null && !betterValue(key, value, mine)) {
        api.endAttempt('finish');
        return true;
      }
      const attempt = state.attempt;
      const windowId = state.session.window.id;
      const sessionId = state.session.id;
      api.endAttempt('finish', true);
      state.upload = { attempt, value, status: 'sending' };
      changed();
      const [modeText, code] = key.split(':');
      Promise.resolve(ui.cb.onlineReplayFile?.({ event: key, mode: +modeText, course: code, name: playerName(), claim: timed ? { ticks: value } : { score: value } }))
        .then((file) => (file ? upload(file.meta, file.pad, attempt, windowId, sessionId) : { error: 'replay' }))
        .then((res) => {
          state.upload = { attempt, value, status: res?.ok ? (res.kept ? 'kept' : 'not kept') : 'failed', error: res?.error ?? null, rank: res?.rank ?? -1 };
          if (res?.ok && res.kept) state.best = value;
          changed();
        })
        .catch((e) => {
          log.warn?.('ta: upload failed', e);
          state.upload = { attempt, value, status: 'failed', error: 'network' };
          changed();
        });
      return true;
    },
    dispose() {
      clearInterval(loadTimer);
      ghosts?.dispose();
      disarm();
      listeners.clear();
    }
  };
  return api;
}
