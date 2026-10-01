// Online multiplayer client (web/server/mp-server.mjs protocol). Plain WebSocket; works in the browser and in node
// (global WebSocket). Keeps the lobby list / current lobby, a server-clock estimate for the synchronized start and
// the race clock, reconnects by itself after a network drop (the server keeps the seat for 30 s), and relays binary
// rider packets (web/net/rider-packet.js).
export const PROTOCOL = 2;
export function defaultServerUrl(loc = globalThis.location) {
  if (!loc) return 'ws://127.0.0.1:8787/mp';
  return `${loc.protocol === 'https:' ? 'wss' : 'ws'}://${loc.host}/mp`;
}

// Reconnect token: the same browser tab resumes its lobby seat after a page reload (server grace 30 s).
export function sessionToken(storage = globalThis.sessionStorage) {
  try { let t = storage?.getItem('ssx3.mp.token'); if (!t) { t = Math.random().toString(36).slice(2) + Date.now().toString(36); storage?.setItem('ssx3.mp.token', t); } return t; }
  catch { return Math.random().toString(36).slice(2); }
}

const CLOCK_WINDOW_MS = 60000, PING_MS = 2000, RECONNECT_MS = [500, 1000, 2000, 4000, 8000, 8000, 8000];

export function createMpClient({
  url = defaultServerUrl(),
  name = 'Rider',
  rider = 'zoe',
  pkg = null,
  base = null,
  outfit = null,
  pair = null,
  token = sessionToken(),
  WebSocketImpl = globalThis.WebSocket,
  version = PROTOCOL
} = {}) {
  const listeners = new Map();
  const emit = (type, value) => {
    for (const f of listeners.get(type) ?? [])
      try {
        f(value);
      } catch (e) {
        console.warn(`mp: ${type} listener failed`, e);
      }
  };
  const state = {
    status: 'idle',
    id: null,
    lobbies: [],
    lobby: null,
    race: null,
    slot: -1,
    goAt: 0,
    results: null,
    error: null,
    errorCode: null,
    offset: 0,
    rtt: Infinity,
    chat: [],
    reconnects: 0,
    sentBytes: 0,
    sentFrames: 0
  };
  let ws = null,
    pingTimer = null,
    samples = [],
    wanted = false,
    retry = 0,
    retryTimer = null;
  const send = (m) => {
    if (ws?.readyState === 1) ws.send(JSON.stringify(m));
  };
  const hello = () => send({ t: 'hello', name, rider, pkg, base, outfit, pair, version, token });
  function onMessage(m) {
    switch (m.t) {
      case 'welcome':
        state.id = m.id;
        state.status = 'online';
        state.resumed = !!m.resumed;
        state.resumeRace = m.race ?? null;
        retry = 0;
        state.error = null;
        state.errorCode = null;
        if (!samples.length) state.offset = m.serverTime - Date.now();
        emit('status', state.status);
        emit('welcome', m);
        break;
      case 'pong': {
        // clock offset from the lowest-latency samples of the last minute (NTP style; clocks drift)
        const t = Date.now(),
          rtt = t - m.c;
        samples.push({ at: t, rtt, offset: m.s - (m.c + rtt / 2) });
        samples = samples.filter((s) => t - s.at < CLOCK_WINDOW_MS).slice(-40);
        const best = samples.reduce((a, s) => (s.rtt < a.rtt ? s : a));
        state.rtt = best.rtt;
        state.offset = best.offset;
        emit('clock', state.offset);
        break;
      }
      case 'lobbies':
        state.lobbies = m.lobbies;
        emit('lobbies', m.lobbies);
        break;
      case 'lobby':
        state.lobby = m.lobby;
        emit('lobby', m.lobby);
        break;
      case 'left':
        state.lobby = null;
        state.race = null;
        emit('lobby', null);
        break;
      case 'error':
        state.error = m.message;
        state.errorCode = m.code ?? null;
        if (m.code === 'version') {
          wanted = false;
          state.status = 'version';
          emit('status', state.status);
        }
        emit('error', m.message);
        break;
      case 'start':
        state.race = m.race;
        state.slot = m.you;
        state.goAt = 0;
        state.results = null;
        emit('start', m.race);
        break;
      case 'go':
        state.goAt = m.at;
        emit('go', m.at);
        break;
      case 'finished':
        emit('finished', m);
        break;
      case 'presence':
        emit('presence', m);
        break;
      case 'results':
        state.results = m.results;
        state.race = null;
        emit('results', m.results);
        break;
      case 'chat':
        state.chat.push({ from: m.from, text: m.text, at: Date.now() });
        state.chat = state.chat.slice(-20);
        emit('chat', m);
        break;
    }
  }
  function open() {
    return new Promise((resolve, reject) => {
      state.status = state.status === 'online' ? 'reconnecting' : state.reconnects ? 'reconnecting' : 'connecting';
      emit('status', state.status);
      let settled = false;
      try {
        ws = new WebSocketImpl(url);
      } catch (e) {
        reject(e);
        return;
      }
      ws.binaryType = 'arraybuffer';
      ws.onopen = () => {
        hello();
        samples = samples.filter((s) => Date.now() - s.at < CLOCK_WINDOW_MS);
        const ping = () => send({ t: 'ping', c: Date.now() });
        ping();
        clearInterval(pingTimer);
        pingTimer = setInterval(ping, PING_MS);
        settled = true;
        resolve();
      };
      ws.onmessage = (e) => {
        if (typeof e.data === 'string') {
          let m;
          try {
            m = JSON.parse(e.data);
          } catch {
            return;
          }
          onMessage(m);
        } else emit('state', new Uint8Array(e.data));
      };
      ws.onclose = () => {
        clearInterval(pingTimer);
        ws = null;
        if (state.status === 'version') return;
        state.status = 'offline';
        emit('status', state.status);
        if (wanted) scheduleReconnect();
      };
      ws.onerror = (e) => {
        state.error = 'Cannot reach the multiplayer server';
        emit('error', state.error);
        if (!settled) reject(e);
      };
    });
  }
  // A dropped socket comes back by itself (same token: the server resumes the seat, also mid-race).
  function scheduleReconnect() {
    clearTimeout(retryTimer);
    const delay = RECONNECT_MS[Math.min(retry, RECONNECT_MS.length - 1)];
    retry++;
    state.status = 'reconnecting';
    emit('status', state.status);
    retryTimer = setTimeout(() => {
      if (!wanted) return;
      state.reconnects++;
      open().catch(() => {});
    }, delay);
  }
  const client = {
    state,
    on(type, f) {
      if (!listeners.has(type)) listeners.set(type, new Set());
      listeners.get(type).add(f);
      return () => listeners.get(type).delete(f);
    },
    connect() {
      wanted = true;
      if (ws && ws.readyState <= 1) return Promise.resolve();
      return open();
    },
    disconnect() {
      wanted = false;
      clearTimeout(retryTimer);
      clearInterval(pingTimer);
      ws?.close();
      ws = null;
      state.status = 'offline';
    },
    // Test hook: drop the socket as a network failure would (the client reconnects by itself).
    dropForTest() {
      ws?.close();
    },
    // Server time (ms) now, from the clock-sync estimate.
    serverNow() {
      return Date.now() + state.offset;
    },
    // Local Date.now()-style ms at which server time `at` happens.
    localTimeOf(at) {
      return at - state.offset;
    },
    setProfile(profile) {
      if (profile.name) name = profile.name;
      if (profile.rider) rider = profile.rider;
      if (profile.pkg) pkg = profile.pkg;
      if (profile.pair) pair = profile.pair;
      if ('base' in profile) base = profile.base ?? null;
      if ('outfit' in profile) outfit = profile.outfit ?? null;
      if (state.lobby) send({ t: 'update', name, rider, pkg, base, outfit, pair });
    },
    get profile() {
      return { name, rider, pkg, base, outfit, pair };
    },
    list() {
      send({ t: 'list' });
    },
    create(options = {}) {
      send({ t: 'create', ...options });
    },
    join(id) {
      send({ t: 'join', lobby: id });
    },
    leave() {
      send({ t: 'leave' });
    },
    ready(on) {
      send({ t: 'update', ready: !!on });
    },
    course(code) {
      send({ t: 'course', course: code });
    },
    start() {
      send({ t: 'start' });
    },
    loaded() {
      send({ t: 'loaded' });
    },
    finish(ticks, dnf = false, reason = '') {
      send({ t: 'finish', ticks, dnf, reason });
    },
    chat(text) {
      send({ t: 'chat', text });
    },
    // Binary rider packet (web/net/rider-packet.js); relayed to the other racers with the sender's slot prefixed.
    // A congested socket drops state frames (the next one supersedes them), never attacks.
    sendState(bytes, { essential = false } = {}) {
      if (ws?.readyState !== 1 || (!essential && ws.bufferedAmount > 64 * 1024)) return false;
      ws.send(bytes);
      state.sentBytes += bytes.length;
      state.sentFrames++;
      return true;
    },
    get isHost() {
      return !!state.lobby && state.lobby.hostId === state.id;
    },
    get connected() {
      return ws?.readyState === 1 && state.status === 'online';
    }
  };
  return client;
}

// Invite link for a lobby on this page's origin.
export function inviteLink(lobbyId, loc = globalThis.location) {
  const u = new URL(loc.href); u.search = ''; u.hash = ''; u.searchParams.set('lobby', lobbyId); return u.toString();
}
