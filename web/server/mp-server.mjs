#!/usr/bin/env node
// SSX 3 online server: lobbies, race control and the rider-state relay, plus (optionally) the built game itself.
// Dependency-free: node:http and a minimal RFC 6455 WebSocket implementation.
//
//   node web/server/mp-server.mjs [--port 8787] [--host 0.0.0.0] [--static web/dist-online,web/public]
//   env: MP_PORT, MP_HOST, MP_STATIC (comma separated roots), MP_ORIGINS (comma separated allowed page origins)
//
// Development: `npm run online` runs this next to the Vite dev server, which proxies /mp here. Hosting: `npm run
// online:serve` builds the game and serves web/dist-online (code) + web/public (the extracted game data) and /mp from this
// one process on one port (docs/multiplayer.md "Hosting"). No COOP/COEP headers are needed (no SharedArrayBuffer).
//
// Protocol (JSON text frames, client -> server):
//   {t:'hello', name, rider, pkg, base, outfit, pair, version, token} -> {t:'welcome', id, serverTime, version, resumed, race?}
//                                                 (a different protocol version gets {t:'error', code:'version'} and
//                                                 the socket closes; the same token within RESUME_GRACE_MS of a drop
//                                                 resumes the player: lobby, host, race seat)
//   {t:'ping', c}                                -> {t:'pong', c, s}  (clock sync: s = server ms)
//   {t:'list'}                                   -> {t:'lobbies', lobbies}  (also pushed on every change)
//   {t:'create', name, course, maxPlayers}       -> {t:'lobby', lobby}
//   {t:'join', lobby}                            -> {t:'lobby', lobby} (joining a racing lobby waits for the next race)
//   {t:'leave'}                                  -> {t:'left'}  (mid-race: that racer is DNF)
//   {t:'update', ready?, rider?, pkg?, base?, outfit?, pair?, name?} -> lobby broadcast
//   {t:'course', course}                         (host) -> lobby broadcast
//   {t:'start'}                                  (host) -> {t:'start', race:{id, course, seed, players}, you: slot}
//   {t:'loaded'}                                 -> when every racer loaded (or LOAD_TIMEOUT_MS): {t:'go', at};
//                                                 racers not loaded by then are dropped from the race (DNF, 'late')
//   {t:'finish', ticks, dnf}                     -> {t:'finished', id, slot, ticks, dnf, reason} to the lobby;
//                                                 {t:'results', results} once every racer finished or is DNF, or
//                                                 FINISH_GRACE_MS after the first finisher, or RACE_LIMIT_MS after GO
//   {t:'chat', text}                             -> {t:'chat', from, text}
// Server -> client extras: {t:'presence', id, slot, online} (a racer's socket dropped / came back),
//   {t:'host', id} is folded into the lobby broadcast (host migration when the host leaves, also mid-race).
// Binary frames (client -> server): rider packets (web/net/rider-packet.js: kind 2 state, 3 attack, 4 world) of a racer
// in a running race, relayed to the lobby's other racers prefixed with the sender's slot byte.
import http from 'node:http';
import crypto from 'node:crypto';
import os from 'node:os';
import fs from 'node:fs';
import path from 'node:path';
import { createRunCheck, AHEAD_TICKS } from './plausibility.mjs';
import { createGate } from './gate.mjs';

export const PROTOCOL = 2;
const args = Object.fromEntries(process.argv.slice(2).reduce((a, v, i, all) => (v.startsWith('--') ? [...a, [v.slice(2), all[i + 1]]] : a), []));
const PORT = +(args.port ?? process.env.MP_PORT ?? 8787), HOST = args.host ?? process.env.MP_HOST ?? '0.0.0.0';
const STATIC = String(args.static ?? process.env.MP_STATIC ?? '').split(',').filter(Boolean).map((p) => path.resolve(p));
// Gzip copies of the game data (server/precompress.mjs): <PRECOMPRESSED>/<path under a static root>.gz, served to
// clients that accept gzip when at least as new as the file.
const PRECOMPRESSED = args.precompressed ?? process.env.MP_PRECOMPRESSED ? path.resolve(args.precompressed ?? process.env.MP_PRECOMPRESSED) : null;
const ORIGINS = String(process.env.MP_ORIGINS ?? '').split(',').filter(Boolean);
if (process.env.MP_REQUIRE_LOOPBACK === '1' && !['127.0.0.1', '::1', 'localhost'].includes(HOST)) { console.error(`mp-server: MP_REQUIRE_LOOPBACK=1 but --host ${HOST}`); process.exit(1); }
// Hosting (docs/hosting.md): behind a local reverse tunnel (cloudflared) MP_TRUST_PROXY=1 takes the client address from
// CF-Connecting-IP; MP_GATE_PASSWORD_FILE (or MP_GATE_PASSWORD) turns on the shared-password gate (web/server/gate.mjs).
const TRUST_PROXY = process.env.MP_TRUST_PROXY === '1';
// The edge worker (deploy/edge-worker.js) checks the gate cookie at Cloudflare and caches game files there; its requests
// carry X-SSX-Edge = the shared secret (MP_EDGE_SECRET_FILE) and get shared-cacheable headers. Everyone else: private.
const EDGE_SECRET = process.env.MP_EDGE_SECRET_FILE ? fs.readFileSync(process.env.MP_EDGE_SECRET_FILE, 'utf8').trim() : '';
const fromEdge = (req) => {
  if (!EDGE_SECRET) return false;
  const a = Buffer.from(String(req.headers['x-ssx-edge'] ?? '')),
    b = Buffer.from(EDGE_SECRET);
  return a.length === b.length && crypto.timingSafeEqual(a, b);
};
const clientAddress = (req) => (TRUST_PROXY && typeof req.headers['cf-connecting-ip'] === 'string' ? req.headers['cf-connecting-ip'] : req.socket.remoteAddress) || '?';
const GATE = createGate({ password: process.env.MP_GATE_PASSWORD || null, passwordFile: process.env.MP_GATE_PASSWORD_FILE || null, secretFile: process.env.MP_GATE_SECRET_FILE || null,
  secure: process.env.MP_GATE_INSECURE !== '1', clientAddress });
// Online course records (web/server/records.mjs, docs/online-records.md): MP_RECORDS_DIR = their state directory (the host's
// ~/ssx-host/state/records); off without it. The disc tables (CAREER/career.json) come from MP_RECORDS_ASSETS or the static roots.
const RECORDS_ASSETS = (() => {
  try {
    return process.env.MP_RECORDS_ASSETS
      ? path.resolve(process.env.MP_RECORDS_ASSETS)
      : ([
          ...STATIC.map((r) => path.join(r, 'assets')),
          path.join(path.dirname(new URL(import.meta.url).pathname), '..', 'public', 'assets')
        ].find((p) => fs.existsSync(path.join(p, 'CAREER', 'career.json'))) ?? null);
  } catch {
    return null;
  }
})();
// Loaded only when configured, and never fatal: a missing module, directory or table leaves the records off (503) and the rest running.
let RECORDS = null;
if (process.env.MP_RECORDS_DIR) {
  try { const { createRecords } = await import('./records.mjs'); RECORDS = createRecords({ dir: path.resolve(process.env.MP_RECORDS_DIR), assets: RECORDS_ASSETS, clientAddress }); }
  catch (e) { console.warn('records: off (failed to start)', e?.message); RECORDS = null; }
}
const MAX_PLAYERS = 6, RESUME_GRACE_MS = +(process.env.MP_RESUME_MS ?? 30000), SILENT_DROP_MS = 10000, LOAD_TIMEOUT_MS = 45000, GO_DELAY_MS = 2500;
const FINISH_GRACE_MS = +(process.env.MP_FINISH_GRACE_MS ?? 90000), RACE_LIMIT_MS = +(process.env.MP_RACE_LIMIT_MS ?? 15 * 60000);
// Default 'flag' (Owen, 2026-09-28: log only, never reject, for this friends-only build: honest finishes were rejected
// twice -- backcountry finishes against a countdown the rolling start does not have, and clients ahead of the server clock
// after a hitch; plausibility.mjs countdownTicks / AHEAD_RECOVER_TICKS, net/race-pace.js). MP_PLAUSIBILITY=reject opts in.
const PLAUSIBILITY = ['reject', 'flag', 'off'].includes(process.env.MP_PLAUSIBILITY) ? process.env.MP_PLAUSIBILITY : 'flag', VERDICT_WAIT_MS = 2000;
const MAX_CLIENTS = +(process.env.MP_MAX_CLIENTS ?? 300), MAX_LOBBIES = 100, MAX_TEXT = 4096, MAX_BINARY = 16384;
// Rate limits (token buckets per connection): control messages, binary frames/bytes, chat, lobby creation.
const LIMITS = { text: [20, 40], binary: [90, 180], bytes: [120000, 240000], chat: [1, 5], create: [0.5, 2] };

// ---- minimal WebSocket ------------------------------------------------------------------------------------------
function accept(req, socket) {
  const key = req.headers['sec-websocket-key'];
  if (!key || req.headers.upgrade?.toLowerCase() !== 'websocket') { socket.destroy(); return null; }
  const hash = crypto.createHash('sha1').update(key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64');
  socket.write(`HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ${hash}\r\n\r\n`);
  socket.setNoDelay(true);
  const ws = { socket, open: true, onText: null, onBinary: null, onClose: null };
  let buffer = Buffer.alloc(0), fragments = [], fragmentOp = 0, fragmentBytes = 0;
  const frame = (op, payload) => { const n = payload.length;
    const head =
      n < 126
        ? Buffer.from([0x80 | op, n])
        : n < 65536
          ? Buffer.from([0x80 | op, 126, n >> 8, n & 255])
          : (() => {
              const h = Buffer.alloc(10);
              h[0] = 0x80 | op;
              h[1] = 127;
              h.writeBigUInt64BE(BigInt(n), 2);
              return h;
            })();
    return Buffer.concat([head, payload]); };
  ws.send = (data) => {
    if (!ws.open) return;
    const payload = typeof data === 'string' ? Buffer.from(data) : Buffer.from(data.buffer ?? data, data.byteOffset ?? 0, data.byteLength ?? data.length);
    // Back-pressure: a client that cannot keep up loses rider frames, never control messages.
    if (typeof data !== 'string' && socket.writableLength > 512 * 1024) { ws.dropped = (ws.dropped ?? 0) + 1; return; }
    socket.write(frame(typeof data === 'string' ? 1 : 2, payload));
  };
  ws.close = (code = 1000) => { if (!ws.open) return; ws.open = false; try { socket.write(frame(8, Buffer.from([code >> 8, code & 255]))); } catch {} socket.end(); ws.onClose?.(); };
  socket.on('data', (chunk) => {
    buffer = Buffer.concat([buffer, chunk]);
    for (;;) {
      if (buffer.length < 2) return;
      const fin = buffer[0] & 0x80, op = buffer[0] & 15, masked = buffer[1] & 0x80; let len = buffer[1] & 127, p = 2;
      if (len === 126) { if (buffer.length < 4) return; len = buffer.readUInt16BE(2); p = 4; }
      else if (len === 127) { if (buffer.length < 10) return; len = Number(buffer.readBigUInt64BE(2)); p = 10; }
      if (len > 1 << 20 || !masked) { ws.close(1009); return; } // clients must mask; nothing legitimate is that large
      const need = p + 4 + len; if (buffer.length < need) return;
      const mask = buffer.subarray(p, p + 4), payload = Buffer.from(buffer.subarray(p + 4, need));
      for (let i = 0; i < payload.length; i++) payload[i] ^= mask[i & 3];
      buffer = buffer.subarray(need);
      if (op === 8) { ws.close(); return; }
      if (op === 9) { socket.write(frame(10, payload)); continue; }
      if (op === 10) continue;
      if (op !== 0) { fragmentOp = op; fragments = []; fragmentBytes = 0; }
      fragments.push(payload); fragmentBytes += payload.length;
      if (fragmentBytes > 1 << 20) { ws.close(1009); return; }
      if (!fin) continue;
      const message = Buffer.concat(fragments); fragments = []; fragmentBytes = 0;
      if (fragmentOp === 1) ws.onText?.(message.toString('utf8')); else if (fragmentOp === 2) ws.onBinary?.(message);
    }
  });
  const gone = () => { if (ws.open) { ws.open = false; ws.onClose?.(); } };
  socket.on('close', gone); socket.on('error', gone);
  return ws;
}
function bucket([rate, burst]) { return { rate, burst, tokens: burst, at: Date.now() }; }
function take(b, n = 1) { const t = Date.now(); b.tokens = Math.min(b.burst, b.tokens + (t - b.at) / 1000 * b.rate); b.at = t; if (b.tokens < n) return false; b.tokens -= n; return true; }

// ---- lobbies ----------------------------------------------------------------------------------------------------
const clients = new Map(); // id -> player
const lobbies = new Map(); // id -> {id, name, host, course, maxPlayers, members:[clientId], race}
const now = () => Date.now();
const id6 = () => crypto.randomBytes(4).toString('base64url').replace(/[-_]/g, 'x').slice(0, 6).toUpperCase();
const send = (c, m) => c?.ws.send(JSON.stringify(m));
const clean = (v, n) => String(v ?? '').replace(/[\u0000-\u001f\u007f]/g, '').trim().slice(0, n);
// A cheat skin's base rider (a riders.json id) and a worn outfit key (web/wardrobe.js outfitKey).
const cleanBase = (v) => (/^[a-z0-9_]{1,24}$/.test(String(v ?? '')) ? String(v) : null);
const cleanOutfit = (v) => (/^(w1:\d{1,6}(,\d{1,6}){0,63}|sam:RIDER_[A-Z0-9_]{1,24})$/.test(String(v ?? '')) ? String(v) : null);
const cleanPair = (p) =>
  p && [p.weight_attribute, p.collision_stat, p.attack_stat].every((x) => Number.isFinite(+x))
    ? { weight_attribute: +p.weight_attribute, collision_stat: +p.collision_stat, attack_stat: +p.attack_stat }
    : null;
const summary = (l) => ({ id: l.id, name: l.name, course: l.course, players: l.members.length, maxPlayers: l.maxPlayers, racing: !!l.race, host: clients.get(l.host)?.name ?? '' });
const member = (c, l) => ({
  id: c.id,
  name: c.name,
  rider: c.rider,
  pkg: c.pkg,
  base: c.base ?? null,
  outfit: c.outfit ?? null,
  pair: c.pair,
  ready: c.ready,
  slot: c.slot,
  online: c.ws.open,
  waiting: !!l?.race && !l.race.slots.has(c.id)
});
const detail = (l) => ({ ...summary(l), hostId: l.host, members: l.members.map((m) => member(clients.get(m), l)),
  race: l.race ? { id: l.race.id, goAt: l.race.goAt, finished: [...l.race.finish.entries()].map(([id, f]) => ({ id, ...f })) } : null });
const broadcastLobbies = () => { const list = [...lobbies.values()].map(summary); for (const c of clients.values()) if (!c.lobby) send(c, { t: 'lobbies', lobbies: list }); };
const broadcastLobby = (l) => { const d = detail(l); for (const m of l.members) send(clients.get(m), { t: 'lobby', lobby: d }); broadcastLobbies(); };
const toLobby = (l, m) => { for (const id of l.members) send(clients.get(id), m); };

function leave(c, reason = 'left') {
  const l = lobbies.get(c.lobby); c.lobby = null; c.ready = false;
  if (!l) return;
  if (l.race?.slots.has(c.id)) recordFinish(l, c, { ticks: 0, dnf: true, reason });
  c.slot = -1;
  l.members = l.members.filter((m) => m !== c.id);
  if (!l.members.length) { lobbies.delete(l.id); broadcastLobbies(); return; }
  if (l.host === c.id) l.host = l.members.find((m) => clients.get(m)?.ws.open) ?? l.members[0]; // host migration
  if (l.race) { l.race.loaded.delete(c.id); maybeGo(l); maybeResults(l); }
  broadcastLobby(l);
}
function join(c, l) {
  if (c.lobby === l.id) { send(c, { t: 'lobby', lobby: detail(l) }); return; }
  if (c.lobby) leave(c);
  if (l.members.length >= l.maxPlayers) return send(c, { t: 'error', message: 'Lobby is full' });
  l.members.push(c.id); c.lobby = l.id; c.ready = false; c.slot = -1;
  broadcastLobby(l);
  if (l.race) send(c, { t: 'error', code: 'racing', message: 'A race is on: you race in the next one' });
}
// A claimed finish time is checked against the racer's streamed run (web/server/plausibility.mjs) once the packet
// showing the finish is in (it can trail the claim by a packet), or VERDICT_WAIT_MS after the claim.
const wallTicks = (r) => (r.goAt ? (now() - r.goAt) * 0.06 : 0);
function claimFinish(l, c, ticks) {
  const r = l.race; if (!r || r.finish.has(c.id) || !r.slots.has(c.id) || r.claims.has(c.id)) return;
  if (PLAUSIBILITY === 'off') { recordFinish(l, c, { ticks, dnf: false }); return; }
  r.claims.set(c.id, { ticks: Math.max(0, ticks | 0), at: now() });
}
function settleClaims(l, force = false) {
  const r = l.race; if (!r) return;
  for (const [id, claim] of [...r.claims]) {
    const check = r.checks.get(id) ?? createRunCheck({ course: l.course });
    if (!check.run.finishPacket && !force && now() - claim.at < VERDICT_WAIT_MS) continue;
    r.claims.delete(id);
    const verdict = check.verdict(claim.ticks); r.verdicts.set(id, verdict);
    if (!verdict.ok)
      console.warn(
        `mp: implausible finish by ${r.names[id]} (${claim.ticks} ticks, ${l.course}):`,
        JSON.stringify(verdict.findings),
        JSON.stringify(verdict.stats)
      );
    else if (verdict.stats.lead > AHEAD_TICKS)
      console.log(
        `mp: finish by ${r.names[id]} (${claim.ticks} ticks, ${l.course}) ran up to ${verdict.stats.lead} ticks ahead of the server clock (accepted)`
      );
    const rejected = !verdict.ok && PLAUSIBILITY === 'reject';
    recordFinish(l, clients.get(id) ?? { id }, rejected ? { dnf: true, reason: 'invalid' } : { ticks: claim.ticks, dnf: false }, verdict);
  }
}
function recordFinish(l, c, { ticks, dnf, reason }, verdict = null) {
  const r = l.race; if (!r || r.finish.has(c.id) || !r.slots.has(c.id)) return;
  const f = { ticks: dnf ? 0 : Math.max(0, ticks | 0), dnf: !!dnf, reason: dnf ? String(reason ?? 'dnf').slice(0, 16) : '', at: now(),
    ...(verdict ? { verified: verdict.ok, findings: verdict.ok ? [] : verdict.findings.map((x) => x.code) } : {}) };
  r.claims.delete(c.id);
  r.finish.set(c.id, f); if (!dnf && !r.firstFinishAt) r.firstFinishAt = now();
  toLobby(l, { t: 'finished', id: c.id, slot: r.slots.get(c.id), ...f });
}
function maybeGo(l) {
  const r = l.race; if (!r || r.goAt) return;
  const waiting = [...r.slots.keys()].filter((m) => !r.loaded.has(m));
  if (waiting.length && now() - r.startedAt < LOAD_TIMEOUT_MS) return;
  r.goAt = now() + GO_DELAY_MS;
  for (const m of waiting) { const c = clients.get(m); if (c) recordFinish(l, c, { dnf: true, reason: 'late' }); }
  toLobby(l, { t: 'go', at: r.goAt });
}
function maybeResults(l) {
  const r = l.race; if (!r || r.done) return;
  const timeUp = (r.firstFinishAt && now() - r.firstFinishAt > FINISH_GRACE_MS) || (r.goAt && now() - r.goAt > RACE_LIMIT_MS) || (!r.goAt && now() - r.startedAt > LOAD_TIMEOUT_MS + RACE_LIMIT_MS);
  settleClaims(l, timeUp);
  const racers = [...r.slots.keys()], pending = racers.filter((m) => !r.finish.has(m));
  if (pending.length && !timeUp) return;
  for (const m of pending) { const c = clients.get(m); if (c) recordFinish(l, c, { dnf: true, reason: 'time' }); else r.finish.set(m, { ticks: 0, dnf: true, reason: 'left' }); }
  r.done = true;
  const results = racers.map((m) => ({ id: m, slot: r.slots.get(m), name: r.names[m], rider: r.riders[m], ...r.finish.get(m) }))
    .sort((a, b) => (a.dnf - b.dnf) || (a.ticks - b.ticks) || (a.slot - b.slot));
  toLobby(l, { t: 'results', raceId: r.id, results });
  l.race = null; for (const m of l.members) { const c = clients.get(m); if (c) { c.ready = false; c.slot = -1; } }
  broadcastLobby(l);
}
setInterval(() => { try { tick(); } catch (e) { console.warn('mp: heartbeat failed', e); } }, 1000).unref();
function tick() {
  // Heartbeat: clients ping every 2 s; a socket silent for SILENT_DROP_MS is dead (e.g. a proxy kept it open).
  for (const c of clients.values()) if (c.ws.open && now() - (c.ws.lastSeen ?? 0) > SILENT_DROP_MS) c.ws.close(1001);
  for (const l of lobbies.values()) { maybeGo(l); maybeResults(l); }
}

function onMessage(c, m) {
  let l = lobbies.get(c.lobby);
  if (m.t !== 'hello' && m.t !== 'ping' && !c.hello) return;
  switch (m.t) {
    case 'hello': {
      if (m.version !== PROTOCOL) {
        send(c, {
          t: 'error',
          code: 'version',
          message: `This game (online protocol ${m.version ?? '?'}) and the server (protocol ${PROTOCOL}) differ: reload the page.`,
          server: PROTOCOL
        });
        setTimeout(() => c.ws.close(4000), 50);
        return;
      }
      const token = clean(m.token, 64);
      const previous = token && [...clients.values()].find((o) => o !== c && o.token === token);
      if (previous) { // resume: adopt the new socket into the old player record (an old still-open socket is replaced)
        clearTimeout(previous.dropTimer); previous.dropTimer = null;
        const old = previous.ws; previous.ws = c.ws; c.ws.player = previous; clients.delete(c.id); if (old.open && old !== c.ws) { old.player = null; old.close(4001); }
        previous.limits = c.limits; c = previous;
      }
      c.hello = true; c.token = token;
      c.name = clean(m.name ?? c.name, 16) || 'Rider'; c.rider = clean(m.rider ?? c.rider, 24) || 'zoe';
      c.pkg = /^RIDER_[A-Z0-9_]{1,24}$/.test(String(m.pkg)) ? String(m.pkg) : c.pkg ?? `RIDER_${c.rider.toUpperCase()}`; c.pair = cleanPair(m.pair) ?? c.pair ?? null;
      if ('base' in m) c.base = cleanBase(m.base); if ('outfit' in m) c.outfit = cleanOutfit(m.outfit);
      const l2 = lobbies.get(c.lobby),
        race =
          l2?.race && l2.race.slots.has(c.id)
            ? { id: l2.race.id, course: l2.course, slot: l2.race.slots.get(c.id), goAt: l2.race.goAt, finished: l2.race.finish.has(c.id) }
            : null;
      send(c, { t: 'welcome', id: c.id, serverTime: now(), version: PROTOCOL, resumed: !!previous, race });
      if (l2) { if (previous && race) toLobby(l2, { t: 'presence', id: c.id, slot: race.slot, online: true }); broadcastLobby(l2); }
      else send(c, { t: 'lobbies', lobbies: [...lobbies.values()].map(summary) });
      break;
    }
    case 'ping': send(c, { t: 'pong', c: m.c, s: now() }); break;
    case 'list': send(c, { t: 'lobbies', lobbies: [...lobbies.values()].map(summary) }); break;
    case 'create': {
      if (!take(c.limits.create)) return send(c, { t: 'error', message: 'Slow down' });
      if (lobbies.size >= MAX_LOBBIES) return send(c, { t: 'error', message: 'Too many lobbies on this server' });
      const lobby = { id: id6(), name: clean(m.name, 24) || `${c.name}'s lobby`, host: c.id, course: clean(m.course, 8) || 'ARA1',
        maxPlayers: Math.min(Math.max(m.maxPlayers | 0 || MAX_PLAYERS, 2), MAX_PLAYERS), members: [], race: null };
      lobbies.set(lobby.id, lobby); join(c, lobby); break;
    }
    case 'join': { const target = lobbies.get(clean(m.lobby, 8).toUpperCase()); if (!target) send(c, { t: 'error', code: 'missing', message: 'Lobby not found' }); else join(c, target); break; }
    case 'leave': leave(c); send(c, { t: 'left' }); send(c, { t: 'lobbies', lobbies: [...lobbies.values()].map(summary) }); break;
    case 'update': {
      if ('ready' in m) c.ready = !!m.ready;
      if (m.rider) c.rider = clean(m.rider, 24); if (m.name) c.name = clean(m.name, 16) || c.name;
      if (/^RIDER_[A-Z0-9_]{1,24}$/.test(String(m.pkg))) c.pkg = String(m.pkg); if (m.pair) c.pair = cleanPair(m.pair) ?? c.pair;
      if ('base' in m) c.base = cleanBase(m.base); if ('outfit' in m) c.outfit = cleanOutfit(m.outfit);
      if (l) broadcastLobby(l); break;
    }
    case 'course': if (l && l.host === c.id && !l.race) { l.course = clean(m.course, 8) || l.course; broadcastLobby(l); } break;
    case 'start': {
      if (!l || l.host !== c.id || l.race) break;
      const racers = l.members.filter((id) => clients.get(id)?.ws.open);
      racers.forEach((id, slot) => { clients.get(id).slot = slot; });
      l.race = {
        id: id6(),
        startedAt: now(),
        loaded: new Set(),
        finish: new Map(),
        claims: new Map(),
        checks: new Map(),
        verdicts: new Map(),
        goAt: 0,
        done: false,
        firstFinishAt: 0,
        slots: new Map(racers.map((id, slot) => [id, slot])),
        names: Object.fromEntries(racers.map((id) => [id, clients.get(id).name])),
        riders: Object.fromEntries(racers.map((id) => [id, clients.get(id).rider]))
      };
      const race = { id: l.race.id, course: l.course, seed: crypto.randomInt(0x7fffffff), players: racers.map((id) => member(clients.get(id), l)) };
      for (const id of racers) send(clients.get(id), { t: 'start', race, you: clients.get(id).slot });
      broadcastLobby(l); break;
    }
    case 'loaded': if (l?.race?.slots.has(c.id) && !l.race.goAt) { l.race.loaded.add(c.id); maybeGo(l); } break;
    case 'finish': if (l?.race) { if (m.dnf) recordFinish(l, c, { ticks: 0, dnf: true, reason: m.reason || 'dnf' }); else claimFinish(l, c, m.ticks); maybeResults(l); } break;
    case 'chat': {
      if (!l || !take(c.limits.chat)) break;
      const text = clean(m.text, 120); if (text) toLobby(l, { t: 'chat', from: c.name, id: c.id, text });
      break;
    }
  }
}
const binaryStats = { in: 0, relayed: 0, bytes: 0, dropped: 0, reason: '' };
function onBinary(c, data) {
  binaryStats.in++;
  const l = lobbies.get(c?.lobby), slot = l?.race?.slots.get(c.id);
  const ok = slot != null && l.race.goAt && !l.race.finish.get(c.id)?.dnf && data.length >= 2 && data.length <= MAX_BINARY && (data[0] === 2 || data[0] === 3 || data[0] === 4);
  if (!ok || !take(c.limits.binary) || !take(c.limits.bytes, data.length)) { binaryStats.dropped++; binaryStats.reason = ok ? 'rate' : `slot ${slot} len ${data.length} kind ${data[0]}`; return; }
  binaryStats.relayed++; binaryStats.bytes += data.length;
  const race = l.race, out = Buffer.allocUnsafe(data.length + 1); out[0] = slot; data.copy(out, 1);
  for (const id of race.slots.keys()) if (id !== c.id) clients.get(id)?.ws.send(out);
  // Finish plausibility (web/server/plausibility.mjs): the run as streamed; a pending claim settles on its finish packet
  // (maybeResults may end the race here).
  if (data[0] === 2) { let check = race.checks.get(c.id); if (!check) race.checks.set(c.id, (check = createRunCheck({ course: l.course })));
  // its stage teleports (web/server/teleport-beams.mjs)
check.feed(data, wallTicks(race)); if (race.claims.has(c.id) && check.run.finishPacket) maybeResults(l); }
}

// ---- static files (the built game) -------------------------------------------------------------------------------
const TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json',
  '.wasm': 'application/wasm',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.svg': 'image/svg+xml',
  '.ico': 'image/x-icon',
  '.bin': 'application/octet-stream',
  '.f32': 'application/octet-stream',
  '.ttf': 'font/ttf',
  '.woff2': 'font/woff2',
  '.txt': 'text/plain; charset=utf-8',
  '.webmanifest': 'application/manifest+json'
};
const realRoots = new Map(),
  realRoot = (root) => {
    if (!realRoots.has(root)) {
      try {
        realRoots.set(root, fs.realpathSync(root));
      } catch {
        realRoots.set(root, root);
      }
    }
    return realRoots.get(root);
  };
function serveStatic(req, res) {
  const edge = fromEdge(req);
  if (!STATIC.length || !['GET', 'HEAD'].includes(req.method)) return false;
  let pathname; try { pathname = decodeURIComponent(new URL(req.url, 'http://x').pathname); } catch { return false; }
  if (pathname.includes('\0') || pathname.includes('\\') || pathname.split('/').some((seg) => seg.startsWith('.'))) return false; // no dotfiles / traversal segments
  if (pathname.endsWith('/')) pathname += 'index.html';
  for (const root of STATIC) {
    const file = path.join(root, pathname);
    if (!file.startsWith(root + path.sep)) continue; // no escaping the roots
    let real; try { real = fs.realpathSync(file); } catch { continue; }
    if (!real.startsWith(realRoot(root) + path.sep)) continue; // nor through a symlink
    let stat; try { stat = fs.statSync(real); } catch { continue; }
    if (!stat.isFile()) continue;
    let gz = null; // precompressed copy (no ranges on it: the whole encoded body); brotli first where precompress made one (*-xbox.tex)
    const accepts = String(req.headers['accept-encoding'] ?? ''), copy = (ext, encoding) => {
      const candidate = path.join(PRECOMPRESSED, path.relative(realRoot(root), real)) + ext;
      if (!candidate.startsWith(PRECOMPRESSED + path.sep)) return null;
      try { const g = fs.statSync(candidate); if (g.isFile() && g.mtimeMs >= stat.mtimeMs) return { file: candidate, size: g.size, encoding }; } catch {}
      return null;
    };
    // (a Range request gets its bytes of the file itself: web/audio-speech.js reads single speech lines out of a .dat, docs/audio-logic.md 9.15)
    const ranged = /^bytes=/.test(String(req.headers.range ?? ''));
    if (PRECOMPRESSED && !ranged && /\bbr\b/.test(accepts)) gz = copy('.br', 'br');
    if (PRECOMPRESSED && !ranged && !gz && /\bgzip\b/.test(accepts)) gz = copy('.gz', 'gzip');
    // Vite's content-hashed bundles (8-character hash), top level of /assets only
    const etag = `"${stat.size.toString(36)}-${Math.floor(stat.mtimeMs).toString(36)}${gz ? (gz.encoding === 'br' ? '-br' : '-gz') : ''}"`,
      hashed = /^\/assets\/[^/]+-[A-Za-z0-9_-]{8}\.(js|css|wasm)$/.test(pathname);
    const headers = { 'content-type': TYPES[path.extname(file).toLowerCase()] ?? 'application/octet-stream', etag, 'accept-ranges': 'bytes',
      // Behind the gate every response is `private`: the browser caches it, a shared cache (the Cloudflare edge) must not,
      // or it would serve the cached file to visitors without a session (edge caches key on the URL only).
      'cache-control': pathname.endsWith('.html') ? (GATE ? 'private, no-cache' : 'no-cache') : pathname === '/build.json' ?
      // the deploy check (web/build-check.js)
'no-store'   : `${GATE && !edge ? 'private' : 'public'}, ${hashed ? 'max-age=31536000, immutable' : 'max-age=3600'}`, 'x-content-type-options': 'nosniff' };
    // The edge keeps a copy 5 minutes, then revalidates it with the ETag (304, no body when unchanged): unchanged files
    // never leave the host twice. Hashed bundles never change.
    // stale-while-revalidate: an edge copy older than 5 minutes still answers at once while the edge refreshes it from
    // here; without it every such file waited on a round trip through the tunnel, one after another (a repeat visit
    // made ~46 such trips before the title, docs/first-load.md). An asset deploy bumps the edge worker's CACHE_GEN so no
    // stale copy of a changed file is served (docs/hosting.md).
    if (edge && !pathname.endsWith('.html')) { res.removeHeader('cdn-cache-control'); headers['cdn-cache-control'] = hashed ? 'max-age=31536000' : 'max-age=300, stale-while-revalidate=604800'; }
    headers['x-decoded-length'] = stat.size; // web/downloads.js: the real size for its progress bar, whatever the transfer encoding
    if (PRECOMPRESSED) headers.vary = 'Accept-Encoding';
    if (req.headers['if-none-match'] === etag) { res.writeHead(304, headers); res.end(); return true; }
    if (gz) {
      headers['content-encoding'] = gz.encoding; headers['content-length'] = gz.size; delete headers['accept-ranges'];
      res.writeHead(200, headers); if (req.method === 'HEAD') { res.end(); return true; }
      fs.createReadStream(gz.file).on('error', () => res.destroy()).pipe(res); return true;
    }
    let start = 0, end = stat.size - 1, status = 200;
    const range = /^bytes=(\d*)-(\d*)$/.exec(req.headers.range ?? '');
    if (range && (range[1] || range[2])) {
      start = range[1] ? +range[1] : Math.max(0, stat.size - +range[2]); end = range[1] && range[2] ? Math.min(+range[2], stat.size - 1) : stat.size - 1;
      if (start > end || start >= stat.size) { res.writeHead(416, { 'content-range': `bytes */${stat.size}` }); res.end(); return true; }
      status = 206; headers['content-range'] = `bytes ${start}-${end}/${stat.size}`;
    }
    headers['content-length'] = Math.max(0, end - start + 1);
    res.writeHead(status, headers);
    if (req.method === 'HEAD' || stat.size === 0) { res.end(); return true; } // an empty file: no read stream (end would be -1)
    fs.createReadStream(real, { start, end }).on('error', () => res.destroy()).pipe(res);
    return true;
  }
  return false;
}

// ---- field diagnostics (web/diagnostics.js) ------------------------------------------------------------------------
// Real devices POST their error / GPU / timeline events; one JSON line per batch in MP_DIAG_LOG (the host keeps it in
// ~/ssx-host/logs, writable in the sandbox). Bounded: 64 KB per batch, 20 batches a minute per address, 20 MB per file.
const DIAG_LOG = process.env.MP_DIAG_LOG || null, DIAG_MAX_FILE = 20 * 1048576, diagBuckets = new Map();
function diagPost(req, res) {
  const done = (code) => { res.writeHead(code, { 'content-type': 'text/plain', 'cache-control': 'no-store' }); res.end(); };
  if (!DIAG_LOG) { req.resume(); done(204); return; }
  const address = clientAddress(req); let b = diagBuckets.get(address); if (!b) diagBuckets.set(address, (b = bucket([20 / 60, 20])));
  if (!take(b)) { req.resume(); done(429); return; }
  let body = '', over = false;
  req.on('data', (d) => { body += d; if (body.length > 65536) { over = true; req.destroy(); } });
  req.on('end', () => {
    if (over) return;
    let parsed; try { parsed = JSON.parse(body); } catch { done(400); return; }
    try {
      if ((fs.statSync(DIAG_LOG, { throwIfNoEntry: false })?.size ?? 0) < DIAG_MAX_FILE)
        fs.appendFileSync(DIAG_LOG, JSON.stringify({ at: new Date().toISOString(), address, ...parsed }) + '\n');
    } catch (e) {
      console.warn('diag: write failed', e?.message);
    }
    done(204);
  });
}
setInterval(() => { const t = Date.now(); for (const [k, b] of diagBuckets) if (t - b.at > 600000) diagBuckets.delete(k); }, 600000).unref();

// ---- server -----------------------------------------------------------------------------------------------------
const server = http.createServer((req, res) => {
  res.setHeader('x-content-type-options', 'nosniff'); res.setHeader('referrer-policy', 'same-origin'); res.setHeader('x-frame-options', 'SAMEORIGIN');
  if (GATE) res.setHeader('cdn-cache-control', 'no-store'); // Cloudflare honours CDN-Cache-Control: never keep a gated response at the edge
  if (GATE && GATE.handle(req, res)) return;
  // The site is public without a gate: keep it out of search engines.
  res.setHeader('x-robots-tag', 'noindex, nofollow');
  if (req.url === '/robots.txt') {
    res.writeHead(200, { 'content-type': 'text/plain', 'cache-control': 'public, max-age=3600' });
    res.end('User-agent: *\nDisallow: /\n');
    return;
  }
  if (req.url === '/mp/diag' && req.method === 'POST') { diagPost(req, res); return; } // web/diagnostics.js (gated like everything else)
  // online records (gated)
  if (req.url.startsWith('/mp/records')) {
    if (RECORDS?.enabled) {
      if (RECORDS.handle(req, res)) return;
    } else {
      req.resume();
      res.writeHead(503, { 'content-type': 'application/json', 'cache-control': 'no-store' });
      res.end('{"error":"off"}');
      return;
    }
  }
  if (req.url === '/mp/status') {
    res.writeHead(200, { 'content-type': 'application/json', ...(GATE ? {} : { 'access-control-allow-origin': '*' }), 'cache-control': 'no-store' });
    res.end(
      JSON.stringify({
        ok: true,
        protocol: PROTOCOL,
        binary: binaryStats,
        clients: clients.size,
        lobbies: [...lobbies.values()].map((l) => ({
          ...summary(l),
          race: l.race && {
            loaded: [...l.race.loaded],
            goAt: l.race.goAt,
            racers: [...l.race.slots.entries()],
            finish: [...l.race.finish.entries()],
            checks: [...l.race.checks.entries()].map(([id, k]) => [
              id,
              { packets: k.run.packets, path: Math.round(k.run.path), route: Math.round(k.run.route), findings: k.run.findings.length }
            ]),
            verdicts: [...l.race.verdicts.entries()],
            open: l.members.map((m) => clients.get(m)?.ws.open)
          }
        }))
      })
    );
    return;
  }
  let served; try { served = serveStatic(req, res); } catch (e) { // one bad request never takes the server (and its races) down
    console.warn('static: request failed', req.url, e?.message); if (!res.headersSent) { res.writeHead(500, { 'content-type': 'text/plain' }); res.end('error'); } else res.destroy(); return; }
  if (served) return;
  res.writeHead(404, { 'content-type': 'text/plain' }); res.end('not found');
});
server.on('upgrade', (req, socket) => {
  if (!req.url.startsWith('/mp')) { socket.destroy(); return; }
  if (GATE && !GATE.valid(req)) { socket.end('HTTP/1.1 401 Unauthorized\r\nConnection: close\r\n\r\n'); return; }
  if (ORIGINS.length && req.headers.origin && !ORIGINS.includes(req.headers.origin)) { socket.destroy(); return; }
  if (clients.size >= MAX_CLIENTS) { socket.destroy(); return; }
  const ws = accept(req, socket); if (!ws) return;
  const fresh = { id: crypto.randomUUID().slice(0, 8), ws, token: '', name: 'Rider', rider: 'zoe', pkg: 'RIDER_ZOE', pair: null, lobby: null, ready: false, slot: -1, dropTimer: null, hello: false,
    limits: { text: bucket(LIMITS.text), binary: bucket(LIMITS.binary), bytes: bucket(LIMITS.bytes), chat: bucket(LIMITS.chat), create: bucket(LIMITS.create) } };
  clients.set(fresh.id, fresh); ws.player = fresh; ws.strikes = 0;
  ws.onText = (text) => {
    ws.lastSeen = now(); const c = ws.player; if (!c) return;
    if (text.length > MAX_TEXT || !take(c.limits.text)) { if (++ws.strikes > 200) ws.close(1008); return; }
    let m; try { m = JSON.parse(text); } catch { return; }
    if (!m || typeof m !== 'object') return;
    try { onMessage(c, m); } catch (e) { console.warn('mp: message failed', e); }
  };
  ws.onBinary = (data) => { ws.lastSeen = now(); if (!ws.player) return; try { onBinary(ws.player, data); } catch (e) { console.warn('mp: binary frame failed', e); } };
  ws.lastSeen = now();
  // A dropped player keeps its lobby / race seat for RESUME_GRACE_MS (page reload / network blip), then leaves.
  ws.onClose = () => {
    const c = ws.player; if (!c || c.ws !== ws) return;
    if (!c.token || !c.hello) { leave(c); clients.delete(c.id); return; }
    const l = lobbies.get(c.lobby);
    if (l) { if (l.race?.slots.has(c.id)) toLobby(l, { t: 'presence', id: c.id, slot: l.race.slots.get(c.id), online: false }); broadcastLobby(l); }
    c.dropTimer = setTimeout(() => { if (c.ws.open) return; leave(c, 'disconnected'); clients.delete(c.id); }, RESUME_GRACE_MS);
  };
});
server.listen(PORT, HOST, () => {
  const lan = Object.values(os.networkInterfaces()).flat().filter((a) => a && a.family === 'IPv4' && !a.internal).map((a) => a.address);
  console.log(`SSX 3 multiplayer server on ws://${HOST}:${PORT}/mp (protocol ${PROTOCOL})${STATIC.length ? `, serving ${STATIC.join(' + ')}` : ''}${GATE ? ', password gate on' : ''}`);
  const page = STATIC.length ? PORT : (process.env.PORT || 5173);
  if (HOST === '0.0.0.0' || HOST === '::') for (const a of lan) console.log(`  friends on your network open: http://${a}:${page}/`);
});
export { server };
