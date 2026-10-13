// Time-attack sessions (docs/big-multiplayer.md): big online rooms where everyone rides the same event for a time window,
// restarting at will; each player's best complete run counts, ranked by its verified replay; then a podium, a vote for the next
// event, and the next window. Mounted by mp-server.mjs only with MP_SESSIONS=1. Dependency-free; imports only node: and
// web/server/ (the host runs this folder on its own).
//
// Phases (server clock; every client shows them through its clock offset):
//   load -> window -> overtime -> verify -> podium -> vote -> load ...
//   load      the event loads; the window opens once LOAD_QUORUM of the online members reported loaded, or after LOAD_MAX_MS
//   window    attempts (a Single Event run of the event, no computer riders); a finished best is uploaded as its replay file
//   overtime  no new attempts; attempts started before the deadline may finish and upload (at most OVERTIME_MAX_MS)
//   verify    the verifier finishes the runs that could make the podium (at most VERIFY_MAX_MS)
//   podium    the top three verified runs and the final standings (PODIUM_MS)
//   vote      two events from the rotation + RANDOM (VOTE_MS), the result shown for VOTE_RESULT_MS
//
// Protocol (JSON text frames, client -> server; mp-server.mjs routes every 'ta-*' message here):
//   {t:'ta-list'}                                  -> {t:'ta-sessions', sessions} (also pushed on changes until the client joins)
//   {t:'ta-create', name, limitMs, maxPlayers, event?, playerName?} -> the creator joins as host: {t:'ta-joined', ...}
//   {t:'ta-join', session, playerName?}            -> {t:'ta-joined', session, roster, standings, you} (playerName: the name shown)
//   {t:'ta-leave'}                                 -> {t:'ta-left'}
//   {t:'ta-config', limitMs}                       (host) the time limit, from the next window (also this one while it loads)
//   {t:'ta-loaded', window}                        the event is loaded on this page
//   {t:'ta-attempt', window, n}                    attempt n started (the page's startRun)
//   {t:'ta-end', window, n, reason, uploading}     attempt n ended (finish / restart / gave up / quit); uploading: a best follows
//   {t:'ta-vote', choice}                          index into the vote's candidates
// Server -> client: {t:'ta-state', session} on every phase change, {t:'ta-roster', add | drop | update}, {t:'ta-standings', ...}
//   (at most every STANDINGS_MS), {t:'ta-votes', counts}, {t:'ta-results', ...} at the podium, {t:'error', message}.
// HTTP (behind the gate like everything else):
//   POST /mp/session/run?session=S&window=W&attempt=N  header x-ssx-session: the client's token; body: deflate-raw replay file
//        -> {ok, kept, run, status, rank, value} | {error}
//   GET  /mp/session/replay?id=ID                   the stored replay (the verifier page downloads it)
//   the verifier (web/server/records-verifier.mjs; loopback + the records verifier's token, never through the tunnel):
//   GET  /mp/session/verifier/queue?core=C&limit=N  -> {items: [{id, event, claim, core, session, priority}]}
//   POST /mp/session/verifier/result                -> body {id, ok: true | false | null, reason, core, value}
//
// Ranking: the event's rule (records.mjs better): race ticks ascending (the races, Rival Time), score descending (slope style,
// big air, super pipe, Rival Points), then the earlier submission. The podium takes verified runs only.
// Upload checks (no simulation): records.mjs checkRunFile (the claim, the pad stream, the core), the session's event, the maxed
// attribute bytes (55 x 7, the user's decision 2026-10-13), no computer riders outside the backcountry rival events, the attempt
// was announced in this window before its deadline, and the run's own length fits the wall time since the attempt started.
import crypto from 'node:crypto';
import zlib from 'node:zlib';
import { checkRunFile, better } from './records.mjs';
import { decodeReplayFile } from './replay-file.mjs';
import { POSE, LITE, DOT, POSE_HEADER, MAX_BONES, MAX_FREE, liteFromPose, dotFromPose, encodeBundle } from './ghost-packet.mjs';

export const DEFAULTS = {
  limitMs: 6 * 60000,
  minLimitMs: 60000,
  maxLimitMs: 30 * 60000,
  maxPlayers: 50,
  maxSessions: 20,
  loadMaxMs: 45000,
  loadQuorum: 0.9,
  goDelayMs: 2500,
  overtimeMaxMs: 270000,
  uploadGraceMs: 20000,
  verifyMaxMs: 15000,
  podiumMs: 20000,
  voteMs: 15000,
  voteResultMs: 2000,
  standingsMs: 500,
  wallSlackMs: 3000,
  // ghosts (docs/big-multiplayer.md 3.2): the nearest `nearCount` get the pose every bundle (bundleMs), the next `midCount` the lite entry
  // every liteMs and the pose every midPoseMs, everyone else the dot every dotMs; a ghost silent for staleMs is not sent
  bundleMs: 100,
  nearCount: 6,
  midCount: 18,
  liteMs: 250,
  midPoseMs: 2000,
  dotMs: 1000,
  staleMs: 3000
};
export const MAX_ATTRIBUTE = 55;
// the vote's third option: an event of the rotation at random
export const RANDOM = 'random';
export const MAX_UPLOAD = 512 * 1024;
export const MAX_INFLATED = 4 * 1048576;
// Courses whose world has the podium steps (CUTSCENES/locators.json locations[code].locators['0'].podium_steps, 0x4D3770);
// the backcountry courses have none: their sessions show the standings without the podium cutscene.
export const PODIUM_COURSES = new Set(['ABA1', 'ARA1', 'ASS1', 'BHP1', 'BRA2', 'CBA2', 'CHP2', 'CRA3', 'DRA4', 'DSS2', 'EBA3', 'EHP3', 'ERA5', 'ESS3']);
const RIVAL_MODES = new Set([4, 5]);
const UPLOAD_RATE = [6 / 60, 6];

const clean = (v, n) => String(v ?? '').replace(/[\u0000-\u001f\u007f]/g, '').trim().slice(0, n);
const id6 = () => crypto.randomBytes(4).toString('base64url').replace(/[-_]/g, 'x').slice(0, 6).toUpperCase();
const runId = () => crypto.randomBytes(8).toString('hex');

function bucket([rate, burst], at) {
  return { rate, burst, tokens: burst, at };
}
function take(b, t) {
  b.tokens = Math.min(b.burst, b.tokens + (Math.max(0, t - b.at) / 1000) * b.rate);
  b.at = Math.max(b.at, t);
  if (b.tokens < 1) return false;
  b.tokens -= 1;
  return true;
}

// events: records.mjs eventTable (key -> {mode, code, timed, ...}); floors: records.mjs floorTable (key -> {ticks});
// send(clientId, message); clients: Map id -> client (mp-server's); random: () => [0, 1) (tests).
// sendBinary(clientId, bytes): the ghost bundles.
export function createSessions({ events, floors = new Map(), now = () => Date.now(), send, sendBinary = () => {}, clients, verifierToken = null,
  log = console, random = Math.random, options = {} } = {}) {
  const O = { ...DEFAULTS, ...options };
  const sessions = new Map();
  const runs = new Map();
  const subscribers = new Set();
  const rotationAll = [...events.keys()];
  const timed = (key) => !!events.get(key)?.timed;
  let seq = 0;
  // a session's players: the present ones and those who left during this window (their runs still count, Q4)
  const allPlayers = (s) => [...s.players.values(), ...s.departed.values()];
  const playerOf = (s, id) => s.players.get(id) ?? s.departed.get(id) ?? null;

  const member = (p) => ({
    id: p.id,
    index: p.index,
    name: p.name,
    rider: p.rider,
    pkg: p.pkg,
    base: p.base ?? null,
    outfit: p.outfit ?? null,
    online: p.online,
    loaded: p.loadedWindow === p.session.window?.id
  });
  const phaseState = (s) => ({
    id: s.id,
    name: s.name,
    hostId: s.host,
    limitMs: s.limitMs,
    nextLimitMs: s.nextLimitMs,
    maxPlayers: s.maxPlayers,
    players: s.players.size,
    event: s.event,
    phase: s.phase,
    phaseAt: s.phaseAt,
    phaseEndsAt: s.phaseEndsAt,
    window: s.window ? { id: s.window.id, event: s.window.event, startAt: s.window.startAt, endAt: s.window.endAt } : null,
    candidates: s.candidates,
    voteResult: s.voteResult ?? null,
    podiumCourse: PODIUM_COURSES.has(events.get(s.event)?.code)
  });
  const summary = (s) => ({
    id: s.id,
    name: s.name,
    event: s.event,
    phase: s.phase,
    phaseEndsAt: s.phaseEndsAt,
    limitMs: s.limitMs,
    players: s.players.size,
    maxPlayers: s.maxPlayers,
    host: s.players.get(s.host)?.name ?? '',
    // the first ten players' names (the session list's room panel, web/ta-ui.js)
    names: [...s.players.values()].slice(0, 10).map((p) => p.name)
  });
  const toSession = (s, m) => {
    for (const id of s.players.keys()) send(id, m);
  };
  function pushList() {
    const list = [...sessions.values()].map(summary);
    for (const id of subscribers) {
      if (clients.get(id)?.session) subscribers.delete(id);
      else send(id, { t: 'ta-sessions', sessions: list });
    }
  }

  // ---- standings ---------------------------------------------------------------------------------------------------------------
  // A player's rows: its verified best and its pending (unverified) better runs, best first.
  const runOrder = (key) => (a, b) => better(key, a, b);
  function effective(p) {
    const key = p.session.window?.event;
    const list = [p.best, ...p.pending].filter(Boolean);
    if (!list.length) return null;
    list.sort(runOrder(key));
    return list[0];
  }
  function standings(s) {
    const key = s.window?.event;
    const rows = [];
    for (const p of allPlayers(s)) {
      const r = effective(p);
      if (!r) continue;
      rows.push({ id: p.id, name: p.name, rider: p.rider, value: r.value, seq: r.seq, status: r.status, run: r.id, online: p.online });
    }
    rows.sort(runOrder(key));
    return rows.map((r, rank) => ({ ...r, rank }));
  }
  function sendStandings(s, force = false) {
    const t = now();
    if (!force && (!s.standingsDirty || t - s.standingsAt < O.standingsMs)) return;
    s.standingsDirty = false;
    s.standingsAt = t;
    const rows = standings(s);
    const top = rows.slice(0, 10);
    for (const p of s.players.values()) {
      const you = rows.find((r) => r.id === p.id) ?? null;
      send(p.id, { t: 'ta-standings', window: s.window?.id ?? 0, event: s.window?.event, timed: timed(s.window?.event), total: rows.length, rows: top, you });
    }
  }
  // The podium: the first three verified runs in rank order (a player's best verified run; stale / unverifiable runs never count).
  // Undecided: a pending run ranked above the third verified one (the verify phase waits for it). final: the wait is over, so a
  // pending run is skipped and the player's own verified run, or the next player, takes the step.
  function podium(s, final = false) {
    const out = [];
    let undecided = false;
    const order = [];
    for (const p of allPlayers(s)) for (const run of [p.best, ...p.pending]) if (run) order.push({ p, run });
    order.sort((x, y) => better(s.window.event, x.run, y.run));
    const placed = new Set();
    for (const { p, run } of order) {
      if (out.length >= 3) break;
      if (placed.has(p.id)) continue;
      if (run.status === 'pending') {
        undecided = true;
        if (!final) break;
        continue;
      }
      if (run.status !== 'verified') continue;
      placed.add(p.id);
      out.push({ id: p.id, name: p.name, rider: p.rider, pkg: p.pkg, base: p.base ?? null, outfit: p.outfit ?? null, value: run.value, run: run.id,
        status: run.status, rank: out.length });
    }
    return { rows: out, undecided };
  }

  // ---- phases -----------------------------------------------------------------------------------------------------------------
  function setPhase(s, phase, endsAt = 0) {
    s.phase = phase;
    s.phaseAt = now();
    s.phaseEndsAt = endsAt;
    toSession(s, { t: 'ta-state', session: phaseState(s) });
    pushList();
  }
  function startLoad(s, key) {
    s.event = key;
    s.limitMs = s.nextLimitMs;
    s.window = { id: (s.window?.id ?? 0) + 1, event: key, startAt: 0, endAt: 0 };
    s.candidates = null;
    s.voteResult = null;
    s.votes = new Map();
    s.departed = new Map();
    // the replays of the window before the last one go (the last window's stay for its podium and standings)
    for (const r of [...runs.values()]) if (r.session === s.id && r.window < s.window.id - 1) runs.delete(r.id);
    for (const p of s.players.values()) {
      p.best = null;
      p.pending = [];
      p.attempts = [];
    }
    s.standingsDirty = true;
    setPhase(s, 'load', now() + O.loadMaxMs);
  }
  function openWindow(s) {
    const t = now() + O.goDelayMs;
    s.window.startAt = t;
    s.window.endAt = t + s.limitMs;
    setPhase(s, 'window', s.window.endAt);
  }
  const openAttempts = (s) => {
    const t = now();
    let open = 0;
    for (const p of s.players.values()) {
      for (const a of p.attempts) {
        if (a.startedAt > s.window.endAt) continue;
        if (!a.endedAt) open++;
        else if (a.uploading && !a.uploaded && t - a.endedAt < O.uploadGraceMs) open++;
      }
    }
    return open;
  };
  // The vote's options (the user's rules, 2026-10-13): two events of the rotation (not the event just ridden, nor the two before it,
  // while the rotation has enough others), then RANDOM.
  function drawCandidates(s) {
    const recent = new Set([s.event, ...s.recent.slice(-2)]);
    let pool = s.rotation.filter((k) => !recent.has(k));
    if (pool.length < 2) pool = s.rotation.filter((k) => k !== s.event);
    if (pool.length < 2) pool = s.rotation.slice();
    const picks = [];
    while (picks.length < 2 && pool.length) picks.push(pool.splice(Math.floor(random() * pool.length), 1)[0]);
    return [...picks, RANDOM];
  }
  // Most votes wins; a tie (or no vote at all) is drawn among the tied options. RANDOM is any event of the rotation but the two shown,
  // drawn uniformly.
  function finishVote(s) {
    const counts = s.candidates.map(() => 0);
    for (const choice of s.votes.values()) counts[choice]++;
    const top = Math.max(...counts);
    const winners = counts.map((n, i) => (n === top ? i : -1)).filter((i) => i >= 0);
    const pick = winners[Math.floor(random() * winners.length)];
    let event = s.candidates[pick];
    if (event === RANDOM) {
      const shown = new Set(s.candidates);
      const pool = s.rotation.filter((k) => !shown.has(k));
      event = (pool.length ? pool : s.rotation)[Math.floor(random() * (pool.length || s.rotation.length))];
    }
    s.voteResult = { choice: pick, event, random: s.candidates[pick] === RANDOM, counts };
    s.recent.push(s.event);
    s.recent = s.recent.slice(-4);
    setPhase(s, 'vote-result', now() + O.voteResultMs);
  }
  function step(s) {
    const t = now();
    if (s.phase === 'load') {
      const online = [...s.players.values()].filter((p) => p.online);
      const loaded = online.filter((p) => p.loadedWindow === s.window.id).length;
      if ((online.length && loaded >= Math.ceil(online.length * O.loadQuorum)) || t >= s.phaseEndsAt) openWindow(s);
    } else if (s.phase === 'window') {
      if (t >= s.window.endAt) setPhase(s, 'overtime', s.window.endAt + O.overtimeMaxMs);
    } else if (s.phase === 'overtime') {
      if (!openAttempts(s) || t >= s.phaseEndsAt) setPhase(s, 'verify', t + O.verifyMaxMs);
    } else if (s.phase === 'verify') {
      if (!podium(s).undecided || t >= s.phaseEndsAt) {
        const final = standings(s);
        s.results = { t: 'ta-results', window: s.window.id, event: s.window.event, timed: timed(s.window.event), podium: podium(s, true).rows,
          podiumCourse: PODIUM_COURSES.has(events.get(s.window.event)?.code), standings: final };
        s.history.push({ at: t, window: s.window.id, event: s.window.event, timed: s.results.timed, winner: s.results.podium[0] ?? null });
        s.history = s.history.slice(-10);
        // the session's rounds so far (web/ta-ui.js: the circuit standings 109MultiplayerStandings)
        s.results.history = s.history.map((h) => ({ window: h.window, event: h.event, timed: h.timed, winner: h.winner?.name ?? null, value: h.winner?.value ?? null }));
        setPhase(s, 'podium', t + O.podiumMs);
        toSession(s, s.results);
      }
    } else if (s.phase === 'podium') {
      if (t >= s.phaseEndsAt) {
        s.candidates = drawCandidates(s);
        s.votes = new Map();
        setPhase(s, 'vote', t + O.voteMs);
      }
    } else if (s.phase === 'vote') {
      if (s.votesDirty) {
        s.votesDirty = false;
        const counts = s.candidates.map(() => 0);
        for (const choice of s.votes.values()) counts[choice]++;
        toSession(s, { t: 'ta-votes', counts });
      }
      if (t >= s.phaseEndsAt) finishVote(s);
    } else if (s.phase === 'vote-result') {
      if (t >= s.phaseEndsAt) startLoad(s, s.voteResult.event);
    }
    if (s.phase !== 'podium') sendStandings(s);
  }

  // ---- members ----------------------------------------------------------------------------------------------------------------
  function create(c, m) {
    if (sessions.size >= O.maxSessions) return send(c.id, { t: 'error', message: 'Too many sessions on this server' });
    const limit = Number.isFinite(+m.limitMs) && +m.limitMs > 0 ? Math.min(O.maxLimitMs, Math.max(O.minLimitMs, +m.limitMs)) : O.limitMs;
    const maxPlayers = Math.min(Math.max((m.maxPlayers | 0) || O.maxPlayers, 2), O.maxPlayers);
    const s = {
      id: id6(),
      name: clean(m.name, 24) || `${c.name}'s session`,
      host: c.id,
      limitMs: limit,
      nextLimitMs: limit,
      maxPlayers,
      rotation: rotationAll.slice(),
      recent: [],
      players: new Map(),
      event: null,
      phase: 'load',
      phaseAt: 0,
      phaseEndsAt: 0,
      window: null,
      candidates: null,
      votes: new Map(),
      history: [],
      departed: new Map(),
      standingsDirty: false,
      standingsAt: 0,
      emptySince: 0
    };
    sessions.set(s.id, s);
    const first = events.has(m.event) ? m.event : rotationAll[Math.floor(random() * rotationAll.length)];
    join(c, s, { quiet: true, name: m.playerName });
    startLoad(s, first);
    sendJoined(s, s.players.get(c.id));
  }
  function sendJoined(s, p) {
    const rows = standings(s);
    send(p.id, { t: 'ta-joined', you: p.id, session: phaseState(s), roster: [...s.players.values()].map(member), standings: rows,
      results: s.phase === 'podium' ? s.results : null, votes: s.phase === 'vote' ? [...s.votes.entries()] : null });
  }
  function join(c, s, { quiet = false, name = null } = {}) {
    if (c.session === s.id) {
      sendJoined(s, s.players.get(c.id));
      return;
    }
    if (c.session) leave(c);
    if (s.players.size >= s.maxPlayers) return send(c.id, { t: 'error', message: 'Session is full' });
    // back in the same window after leaving: its runs come back with it
    const before = s.departed.get(c.id);
    s.departed.delete(c.id);
    const p = {
      id: c.id,
      session: s,
      // the player's own name (the page's Player Name, web/net/ta-game.js), else the connection's
      name: clean(name, 16) || c.name,
      rider: c.rider,
      pkg: c.pkg,
      base: c.base ?? null,
      outfit: c.outfit ?? null,
      online: true,
      index: (s.nextIndex = (s.nextIndex ?? 0) + 1),
      ghost: null,
      sent: new Map(),
      loadedWindow: 0,
      best: before?.best ?? null,
      pending: before?.pending ?? [],
      attempts: [],
      uploads: before?.uploads ?? bucket(UPLOAD_RATE, now())
    };
    s.players.set(c.id, p);
    s.emptySince = 0;
    c.session = s.id;
    subscribers.delete(c.id);
    toSession(s, { t: 'ta-roster', add: member(p) });
    if (!quiet) sendJoined(s, p);
    pushList();
  }
  function leave(c, reason = 'left') {
    const s = sessions.get(c.session);
    c.session = null;
    if (!s) return;
    const p = s.players.get(c.id);
    if (!p) return;
    // Q4 (docs/big-multiplayer.md): a player who left keeps its runs in the standings of this window, verified ones on the podium
    p.online = false;
    p.left = reason;
    for (const a of p.attempts) if (!a.endedAt) a.endedAt = now();
    s.players.delete(c.id);
    if (p.best || p.pending.length) s.departed.set(c.id, p);
    if (s.host === c.id) s.host = [...s.players.values()].find((x) => x.online)?.id ?? [...s.players.keys()][0] ?? null;
    toSession(s, { t: 'ta-roster', drop: c.id, reason, host: s.host });
    s.standingsDirty = true;
    if (!s.players.size) {
      sessions.delete(s.id);
      for (const r of runs.values()) if (r.session === s.id) runs.delete(r.id);
    }
    pushList();
  }

  // ---- messages ---------------------------------------------------------------------------------------------------------------
  function message(c, m) {
    const s = sessions.get(c.session);
    const p = s?.players.get(c.id);
    switch (m.t) {
      case 'ta-list':
        subscribers.add(c.id);
        send(c.id, { t: 'ta-sessions', sessions: [...sessions.values()].map(summary) });
        return true;
      case 'ta-create':
        create(c, m);
        return true;
      case 'ta-join': {
        const target = sessions.get(clean(m.session, 8).toUpperCase());
        if (!target) send(c.id, { t: 'error', code: 'missing', message: 'Session not found' });
        else join(c, target, { name: m.playerName });
        return true;
      }
      case 'ta-leave':
        leave(c);
        send(c.id, { t: 'ta-left' });
        return true;
      case 'ta-config': {
        if (!s || s.host !== c.id || !Number.isFinite(+m.limitMs)) return true;
        s.nextLimitMs = Math.min(O.maxLimitMs, Math.max(O.minLimitMs, +m.limitMs));
        if (s.phase === 'load') s.limitMs = s.nextLimitMs;
        toSession(s, { t: 'ta-state', session: phaseState(s) });
        return true;
      }
      case 'ta-loaded':
        if (p && s.window && m.window === s.window.id) {
          p.loadedWindow = s.window.id;
          toSession(s, { t: 'ta-roster', update: member(p) });
        }
        return true;
      case 'ta-attempt': {
        if (!p || s.phase !== 'window' || m.window !== s.window.id || !Number.isInteger(m.n)) return true;
        // the window's own clock: no attempt before it opens (1 s of clock-offset slack) or after its deadline
        if (now() < s.window.startAt - 1000 || now() >= s.window.endAt) return true;
        const last = p.attempts[p.attempts.length - 1];
        if (last && m.n <= last.n) return true;
        if (last && !last.endedAt) last.endedAt = now();
        p.attempts.push({ n: m.n, startedAt: now(), endedAt: 0, uploading: false, uploaded: false });
        if (p.attempts.length > 8) p.attempts.shift();
        return true;
      }
      case 'ta-end': {
        const a = p?.attempts.find((x) => x.n === m.n);
        if (!a || a.endedAt || m.window !== s.window?.id) return true;
        a.endedAt = now();
        a.uploading = !!m.uploading && m.reason === 'finish';
        return true;
      }
      case 'ta-vote':
        if (s?.phase === 'vote' && p && Number.isInteger(m.choice) && m.choice >= 0 && m.choice < s.candidates.length) {
          s.votes.set(c.id, m.choice);
          s.votesDirty = true;
        }
        return true;
    }
    return false;
  }

  // ---- runs -------------------------------------------------------------------------------------------------------------------
  // compressed: the request body; token: the client's session token. -> {status, body}
  function submit({ compressed, token, sessionId, windowId, attempt }) {
    const t = now();
    const c = [...clients.values()].find((x) => x.token && x.token === token);
    const s = sessions.get(String(sessionId ?? '').toUpperCase());
    const p = c && s?.players.get(c.id);
    if (!p) return { status: 403, body: { error: 'member' } };
    if (!take(p.uploads, t)) return { status: 429, body: { error: 'rate' } };
    if (!['window', 'overtime'].includes(s.phase) || +windowId !== s.window.id) return { status: 409, body: { error: 'closed' } };
    const a = p.attempts.find((x) => x.n === +attempt);
    if (!a || a.startedAt < s.window.startAt - 1000 || a.startedAt > s.window.endAt) return { status: 409, body: { error: 'attempt' } };
    if (a.uploaded) return { status: 409, body: { error: 'attempt' } };
    if (!compressed?.length || compressed.length > MAX_UPLOAD) return { status: 413, body: { error: 'size' } };
    let file;
    try {
      file = decodeReplayFile(zlib.inflateRawSync(compressed, { maxOutputLength: MAX_INFLATED }));
    } catch {
      return { status: 400, body: { error: 'replay' } };
    }
    const m = file.meta;
    const key = s.window.event;
    const ev = events.get(key);
    if (m.event !== key || m.mode !== ev.mode || m.course !== ev.code) return { status: 400, body: { error: 'event' } };
    const checked = checkRunFile(file, ev);
    if (checked.error) return { status: 400, body: { error: checked.error } };
    const attributes = m.rider?.attributes;
    if (!Array.isArray(attributes) || attributes.length !== 7 || attributes.some((x) => x !== MAX_ATTRIBUTE)) return { status: 400, body: { error: 'attributes' } };
    // no computer riders, except the rival of a backcountry rival event (the event as the PS2 runs it)
    const values = m.lineup?.values;
    if (!RIVAL_MODES.has(ev.mode) && m.lineup && Array.isArray(values) && values.length) return { status: 400, body: { error: 'lineup' } };
    // the recorded run cannot be longer than the wall time since its attempt started
    if (t - a.startedAt < (m.ticks / 60) * 1000 - O.wallSlackMs) return { status: 400, body: { error: 'clock' } };
    a.uploaded = true;
    if (!a.endedAt) a.endedAt = t;
    const run = {
      id: runId(),
      session: s.id,
      window: s.window.id,
      player: p.id,
      event: key,
      value: checked.value,
      core: m.core,
      seq: ++seq,
      status: 'pending',
      tries: 0,
      underFloor: !!(ev.timed && floors.get(key) && checked.value < floors.get(key).ticks),
      bytes: compressed,
      at: t
    };
    const current = effective(p);
    if (current && better(key, current, run) <= 0) return { status: 200, body: { ok: true, kept: false, value: run.value, best: current.value } };
    p.pending.push(run);
    p.pending.sort(runOrder(key));
    for (const dropped of p.pending.splice(3)) runs.delete(dropped.id);
    runs.set(run.id, run);
    s.standingsDirty = true;
    const rank = standings(s).findIndex((r) => r.id === p.id);
    return { status: 200, body: { ok: true, kept: true, run: run.id, status: run.status, rank, value: run.value } };
  }
  // Verifier priority: 0 = the run could make the podium, 1 = the top 10, 2 = any other; then the higher rank, then the earlier submission.
  function queue(core, limit = 20) {
    const items = [];
    for (const s of sessions.values()) {
      if (!s.window || !['window', 'overtime', 'verify'].includes(s.phase)) continue;
      const rows = standings(s);
      for (const p of allPlayers(s)) {
        for (const r of p.pending) {
          if (r.status !== 'pending') continue;
          if (core && (r.triedCores ?? []).includes(core)) continue;
          const ahead = rows.filter((x) => x.id !== p.id && better(s.window.event, x, r) < 0).length;
          const priority = ahead < 3 ? 0 : ahead < 10 ? 1 : 2;
          items.push({ id: r.id, event: r.event, claim: r.value, core: r.core, session: s.id, priority, ahead, seq: r.seq });
        }
      }
    }
    // within a priority, the run that would rank highest first
    items.sort((a, b) => a.priority - b.priority || a.ahead - b.ahead || a.seq - b.seq);
    return items.slice(0, limit).map(({ seq: _, ahead: __, ...x }) => x);
  }
  function verify(id, { ok, reason = '', core = null } = {}) {
    const r = runs.get(id);
    if (!r) return null;
    const s = sessions.get(r.session);
    const p = s && playerOf(s, r.player);
    r.note = String(reason).slice(0, 200);
    if (ok === null) {
      r.tries++;
      if (r.tries >= 3) {
        r.status = 'unverifiable';
        (r.triedCores ??= []).push(core);
      }
    } else if (ok) {
      r.status = 'verified';
      if (p) {
        p.best = r;
        p.pending = p.pending.filter((x) => x !== r && better(r.event, x, r) < 0);
      }
    } else if (core && core !== r.core) {
      // recorded on another core (a deploy during the session): kept as unverified, never on the podium
      r.status = 'stale';
    } else {
      r.status = 'pulled';
      if (p) p.pending = p.pending.filter((x) => x !== r);
      runs.delete(r.id);
    }
    if (s) s.standingsDirty = true;
    if (r.status !== 'pending') log.log?.(`session ${r.session} run ${r.id} ${r.event} ${r.value}: ${r.status} (${r.note})`);
    return { found: true, status: r.status };
  }

  // ---- HTTP -------------------------------------------------------------------------------------------------------------------
  function handle(req, res) {
    const url = new URL(req.url, 'http://x');
    const p = url.pathname;
    if (!p.startsWith('/mp/session/')) return false;
    const json = (code, body) => {
      res.writeHead(code, { 'content-type': 'application/json', 'cache-control': 'no-store' });
      res.end(JSON.stringify(body));
    };
    if (req.method === 'POST' && p === '/mp/session/run') {
      const chunks = [];
      let size = 0;
      let over = false;
      req.on('data', (d) => {
        if (over) return;
        size += d.length;
        if (size > MAX_UPLOAD) {
          over = true;
          chunks.length = 0;
          json(413, { error: 'size' });
          return;
        }
        chunks.push(d);
      });
      req.on('end', () => {
        if (over) return;
        const r = submit({ compressed: Buffer.concat(chunks), token: String(req.headers['x-ssx-session'] ?? ''), sessionId: url.searchParams.get('session'),
          windowId: url.searchParams.get('window'), attempt: url.searchParams.get('attempt') });
        json(r.status, r.body);
      });
      req.on('error', () => {});
      return true;
    }
    if (req.method === 'GET' && p === '/mp/session/replay') {
      const r = runs.get(String(url.searchParams.get('id') ?? ''));
      if (!r) {
        json(404, { error: 'replay' });
        return true;
      }
      res.writeHead(200, { 'content-type': 'application/octet-stream', 'content-length': r.bytes.length, 'cache-control': 'private, max-age=86400, immutable' });
      res.end(r.bytes);
      return true;
    }
    if (p.startsWith('/mp/session/verifier/')) {
      // only the host's verifier: loopback, not through the tunnel (cloudflared adds CF-Connecting-IP), with the token; else 404
      const local = ['127.0.0.1', '::1', '::ffff:127.0.0.1'].includes(req.socket?.remoteAddress) && !req.headers['cf-connecting-ip'];
      const given = Buffer.from(String(req.headers['x-ssx-verifier'] ?? ''));
      const want = Buffer.from(verifierToken ? String(verifierToken) : '');
      const tokenOk = want.length >= 16 && given.length === want.length && crypto.timingSafeEqual(given, want);
      if (!local || !tokenOk) {
        req.resume();
        json(404, { error: 'not found' });
        return true;
      }
      if (req.method === 'GET' && p === '/mp/session/verifier/queue') {
        // active: sessions whose runs can still change the standings (the verifier keeps its page open and polls often meanwhile)
        const active = [...sessions.values()].filter((s) => ['load', 'window', 'overtime', 'verify'].includes(s.phase)).length;
        json(200, { items: queue(url.searchParams.get('core') || null, Math.min(50, +url.searchParams.get('limit') || 20)), active });
        return true;
      }
      if (req.method === 'POST' && p === '/mp/session/verifier/result') {
        let body = '';
        req.on('data', (d) => {
          body += d;
          if (body.length > 8192) req.destroy();
        });
        req.on('end', () => {
          let m;
          try {
            m = JSON.parse(body);
          } catch {
            json(400, { error: 'json' });
            return;
          }
          const r = verify(String(m.id ?? ''), { ok: m.ok === null ? null : !!m.ok, reason: m.reason, core: typeof m.core === 'string' ? m.core : null });
          json(r ? 200 : 404, r ?? { error: 'id' });
        });
        return true;
      }
    }
    req.resume();
    json(404, { error: 'not found' });
    return true;
  }

  // ---- ghosts -------------------------------------------------------------------------------------------------------------------
  // A pose packet (ghost-packet.mjs kind 5) of a session member: kept as its newest; its sanity is the sizes only (display only).
  const ghostStats = { in: 0, inBytes: 0, out: 0, outBytes: 0, bundles: 0, dropped: 0 };
  function binary(c, data) {
    const s = sessions.get(c.session);
    const p = s?.players.get(c.id);
    if (!p || data.length < POSE_HEADER || data[0] !== POSE) {
      ghostStats.dropped++;
      return false;
    }
    const bones = data[32];
    const free = data[33];
    if (!bones || bones > MAX_BONES || free > MAX_FREE || data.length !== POSE_HEADER + 7 * free + 4 * bones) {
      ghostStats.dropped++;
      return false;
    }
    const v = new DataView(data.buffer, data.byteOffset, data.byteLength);
    const position = [v.getFloat32(8, true), v.getFloat32(12, true), v.getFloat32(16, true)];
    if (!position.every(Number.isFinite)) {
      ghostStats.dropped++;
      return false;
    }
    const bytes = Uint8Array.from(data);
    p.ghost = { bytes, seq: (p.ghost?.seq ?? 0) + 1, at: now(), position, riding: !!(data[1] & 1), lite: null, dot: null };
    ghostStats.in++;
    ghostStats.inBytes += data.length;
    return true;
  }
  const dist2 = (a, b) => (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2;
  // Per recipient: the other riding players by distance from the recipient's own last position; every tier's entry when it is due and
  // newer than the last one sent of that kind.
  function ghostBundles(s) {
    const t = now();
    const riders = [...s.players.values()].filter((p) => p.ghost && p.ghost.riding && t - p.ghost.at < O.staleMs);
    if (!riders.length) return;
    for (const r of s.players.values()) {
      if (!r.online) continue;
      const here = r.ghost?.position ?? riders[0].ghost.position;
      const others = riders.filter((p) => p !== r).sort((a, b) => dist2(a.ghost.position, here) - dist2(b.ghost.position, here));
      const entries = [];
      others.forEach((p, rank) => {
        const g = p.ghost;
        const last = r.sent.get(p.id) ?? { pose: 0, poseAt: 0, lite: 0, liteAt: 0, dot: 0, dotAt: 0 };
        const tier = rank < O.nearCount ? 'near' : rank < O.nearCount + O.midCount ? 'mid' : 'far';
        if ((tier === 'near' || (tier === 'mid' && t - last.poseAt >= O.midPoseMs)) && last.pose !== g.seq) {
          entries.push({ player: p.index, kind: POSE, bytes: g.bytes });
          // a pose supersedes the lite and the dot of the same moment
          last.pose = last.lite = last.dot = g.seq;
          last.poseAt = last.liteAt = last.dotAt = t;
        } else if (tier === 'mid' && t - last.liteAt >= O.liteMs && last.lite !== g.seq) {
          g.lite ??= liteFromPose(g.bytes);
          entries.push({ player: p.index, kind: LITE, bytes: g.lite });
          last.lite = g.seq;
          last.liteAt = t;
        } else if (tier === 'far' && t - last.dotAt >= O.dotMs && last.dot !== g.seq) {
          g.dot ??= dotFromPose(g.bytes);
          entries.push({ player: p.index, kind: DOT, bytes: g.dot });
          last.dot = g.seq;
          last.dotAt = t;
        }
        r.sent.set(p.id, last);
      });
      for (let at = 0; at < entries.length; at += 255) {
        const bundle = encodeBundle(t, entries.slice(at, at + 255));
        sendBinary(r.id, bundle);
        ghostStats.bundles++;
        ghostStats.out += Math.min(255, entries.length - at);
        ghostStats.outBytes += bundle.length;
      }
    }
  }

  // ---- mp-server hooks --------------------------------------------------------------------------------------------------------
  const api = {
    enabled: events.size > 0,
    sessions,
    runs,
    message,
    submit,
    queue,
    verify,
    handle,
    standings,
    podium,
    // the socket dropped (the seat is kept for mp-server's resume grace)
    dropped(c) {
      const s = sessions.get(c.session);
      const p = s?.players.get(c.id);
      if (!p) return;
      p.online = false;
      toSession(s, { t: 'ta-roster', update: member(p) });
    },
    resumed(c) {
      const s = sessions.get(c.session);
      const p = s?.players.get(c.id);
      if (!p) return;
      p.online = true;
      toSession(s, { t: 'ta-roster', update: member(p) });
      sendJoined(s, p);
    },
    leave,
    forget(c) {
      subscribers.delete(c.id);
    },
    binary,
    ghostStats,
    // every bundleMs (mp-server's timer): the ghost bundles of every session
    ghostTick() {
      for (const s of sessions.values()) {
        try {
          ghostBundles(s);
        } catch (e) {
          log.warn?.('session ghosts failed', s.id, e);
        }
      }
    },
    tick() {
      for (const s of [...sessions.values()]) {
        try {
          step(s);
        } catch (e) {
          log.warn?.('session step failed', s.id, e);
        }
      }
    },
    status() {
      return [...sessions.values()].map((s) => ({ ...summary(s), window: s.window, queued: queue(null, 50).filter((x) => x.session === s.id).length }));
    }
  };
  return api;
}
