// Online course records (docs/online-records.md): one board per event (the PS2's record slots 6..25: the 14 standard events and
// the 6 backcountry rival events; the peak runs keep the local table), each run with its replay. Mounted by mp-server.mjs;
// dependency-free; imports only node: and web/server/ (the deploy ships this folder on its own).
//
//   GET  /mp/records                      -> {version, events: {"<mode>:<COURSE>": {timed, top: [row x 5]}}}
//   GET  /mp/records/board?event=K&offset=O&limit=N -> {event, timed, total, rows: [row]} (rank order, at most MAX_LIMIT a call)
//   GET  /mp/records/replay?id=ID         -> the stored replay (deflate-raw of web/server/replay-file.mjs), immutable
//   POST /mp/records/submit               -> body: deflate-raw of a replay file; {ok, id, rank, kept, review?, top}
//   row = {id, rank, name, character, rider, value, default?, replay, verified, stale?, core, at}
//   the verifier (web/server/records-verifier.mjs, loopback + token, never through the tunnel):
//   GET  /mp/records/verifier/queue?core=C -> {items: [{id, event, claim, core, flagged}]}
//   POST /mp/records/verifier/result       -> {id, ok: true | false | null, reason, core, value}
//
// Rules (PS2 0x154AB8 first, then the coordinator's decisions D2-D7, 2026-09-30, and the anti-cheat floor, 2026-10-01):
//   - order: time events by ticks ascending, score events by score descending, then the earlier submission (seq) (D3);
//   - the PS2 defaults (0x43FB28, CAREER/career.json rules.records) seed every board as replay-less `default` rows with seq 0 and
//     never count for the one-entry-per-name rule (D4);
//   - one entry per name (case-insensitive) per event, the best kept (D2); a worse run under a listed name is not stored;
//   - listed at once (D6) unless under its event's floor: a timed run under 0.9 x the real-world record (record-floors.json, from
//     speedrun.com; else the route length at plausibility.mjs MAX_AVERAGE) is stored `flagged` and not listed until the verifier
//     reproduces it or an admin approves it (records-admin.mjs);
//   - the verifier re-simulates each run on the current build: a run recorded on that core that does not reproduce its claim is
//     pulled; one recorded on an older core gets one try per core and is marked `stale` (the "earlier version" note, D7) instead;
//   - names: the game keyboard's Player Name characters (fe-screens.js KEYS_OFF.name), 1..8 characters, trimmed; a small blocklist;
//   - tier-0 checks (no simulation): the replay decodes, its meta matches the claim, the pad stream ends by the finish tick, sizes.
// Privacy: nothing about the client is stored (no address, user agent, cookie); the rate limits key on the address in memory
// only; `at` is the UTC day.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import zlib from 'node:zlib';
import { MAX_AVERAGE } from './plausibility.mjs';
import { decodeReplayFile, padStreamInfo } from './replay-file.mjs';

export const MAX_ENTRIES = 100;          // listed runs kept per event (defaults on top of these)
export const MAX_FLAGGED = 200;          // flagged runs kept in all (the oldest drop with their replays)
export const MAX_LIMIT = 100;            // rows per board request
export const MAX_UPLOAD = 512 * 1024;    // compressed bytes
export const MAX_INFLATED = 4 * 1048576; // decompressed bytes
export const MAX_BYTES_PER_TICK = 64;    // pad stream bytes per recorded tick (an analog run is ~7)
export const NAME_RE = /^[A-Za-z0-9 !@#$%^&*()]{1,8}$/;
export const LIMITS = { minute: [6 / 60, 6], hour: [30 / 3600, 30] };   // per client address: rate per s, burst
const BLOCK = ['fuck', 'shit', 'cunt', 'nigg', 'fag', 'rape', 'nazi', 'hitler', 'kkk', 'slut', 'whore', 'dick', 'cock', 'pussy',
  'penis', 'bitch'];
const TIMED_MODES = new Set([0, 4]);     // race, Rival Time (career.js isTimed for modes < 6)
const FLOORS_FILE = new URL('./record-floors.json', import.meta.url);

export function cleanName(name) {
  if (typeof name !== 'string') return null;
  const n = name.replace(/\s+/g, ' ').trim();
  if (!NAME_RE.test(n)) return null;
  const flat = n.toLowerCase().replace(/[^a-z]/g, '').replace(/0/g, 'o');
  if (BLOCK.some((w) => flat.includes(w))) return null;
  return n;
}
const day = (t) => new Date(t).toISOString().slice(0, 10);
const readJson = (p) => { try { return JSON.parse(fs.readFileSync(p, 'utf8')); } catch { return null; } };
export const timedEvent = (key) => TIMED_MODES.has(+String(key).split(':')[0]);
// rank order of two runs of an event (D3)
export const better = (key, a, b) => (timedEvent(key) ? a.value - b.value : b.value - a.value) || a.seq - b.seq;

// The events with a board, from the disc's career tables: course index -> code, record_slots rows [mode, slot, mode, slot].
export function eventTable(career) {
  const events = new Map();
  const rules = career?.rules, courses = career?.courses;
  if (!rules?.record_slots || !Array.isArray(courses)) return events;
  rules.record_slots.forEach((row, course) => {
    const code = courses[course]?.code; if (!code || course >= 17) return;
    for (const [mode, slot] of [[row[0], row[1]], [row[2], row[3]]]) {
      if (slot >= 26 || slot < 6 || mode > 5) continue;   // no slot / a peak run's (local only)
      events.set(`${mode}:${code}`, { mode, course, code, slot, timed: TIMED_MODES.has(mode), defaults: rules.records?.[slot] ?? [] });
    }
  });
  return events;
}
// The anti-cheat floors (race ticks) of the timed events: record-floors.json's (0.9 x the speedrun.com record), else the route floor.
export function floorTable(events, assets, recordFloors = readJson(FLOORS_FILE)) {
  const floors = new Map();
  for (const [key, ev] of events) {
    if (!ev.timed) continue;
    const wr = recordFloors?.events?.[key];
    if (Number.isInteger(wr?.floorTicks)) { floors.set(key, { ticks: wr.floorTicks, source: 'speedrun.com' }); continue; }
    const route = assets ? readJson(path.join(assets, ev.code, 'npc-riders.json'))?.riders?.[0]?.progress_origin : null;
    if (route > 0) floors.set(key, { ticks: Math.floor((route / MAX_AVERAGE) * 60), source: 'route' });
  }
  return floors;
}

// ---- the board file: shared by the server, records-admin.mjs and the verifier's results ------------------------------------
//   {version: 1, seq, events: {key: [listed entry]}, flagged: [entry + {event, flagged: {reason, floor, source}}]}
export function loadBoard(file) {
  const b = readJson(file);
  if (!b || b.version !== 1 || typeof b.events !== 'object') return { version: 1, seq: 0, events: {}, flagged: [] };
  b.flagged ??= [];
  return b;
}
export function saveBoard(file, board) { const tmp = file + '.tmp'; fs.writeFileSync(tmp, JSON.stringify(board)); fs.renameSync(tmp, file); }
const dropReplay = (replayDir, id) => { if (!replayDir) return; try { fs.unlinkSync(path.join(replayDir, id + '.bin')); } catch {} };
export function findEntry(board, id) {
  for (const [key, list] of Object.entries(board.events)) { const i = list.findIndex((x) => x.id === id); if (i >= 0) return { key, list, i, entry: list[i], flagged: false }; }
  const i = board.flagged.findIndex((x) => x.id === id);
  return i >= 0 ? { key: board.flagged[i].event, list: board.flagged, i, entry: board.flagged[i], flagged: true } : null;
}
// List an entry on its event's board: one entry per name, the best kept (D2), at most MAX_ENTRIES. {kept, replaced}
export function listEntry(board, key, entry, replayDir) {
  const list = (board.events[key] ??= []), lower = entry.name.toLowerCase();
  const mine = list.find((r) => r.name.toLowerCase() === lower && r.verified !== false);
  if (mine && better(key, mine, entry) <= 0) { dropReplay(replayDir, entry.id); return { kept: false, replaced: null, mine }; }
  if (mine) { list.splice(list.indexOf(mine), 1); dropReplay(replayDir, mine.id); }
  list.push(entry); list.sort((a, b) => better(key, a, b));
  for (const r of list.splice(MAX_ENTRIES)) dropReplay(replayDir, r.id);
  return { kept: list.includes(entry), replaced: mine ?? null };
}
export function flagEntry(board, key, entry, flag, replayDir) {
  board.flagged.push({ ...entry, event: key, flagged: flag });
  for (const r of board.flagged.splice(0, Math.max(0, board.flagged.length - MAX_FLAGGED))) dropReplay(replayDir, r.id);
}
// Admin: a flagged run onto its board (records-admin.mjs approve). {ok, kept}
export function approveEntry(board, id, replayDir) {
  const f = findEntry(board, id); if (!f?.flagged) return { ok: false };
  f.list.splice(f.i, 1);
  const { event, flagged, ...entry } = f.entry;
  const r = listEntry(board, event, { ...entry, approved: flagged?.reason ?? true }, replayDir);
  return { ok: true, kept: r.kept };
}
export function deleteEntry(board, id, replayDir) {
  const f = findEntry(board, id); if (!f) return false;
  f.list.splice(f.i, 1); dropReplay(replayDir, id); return true;
}
// The verifier's result on core `core`: ok true / false (re-simulated, the claim reproduced or not) / null (could not run).
export function verifyEntry(board, id, { ok, reason = '', core = null, value = null } = {}, replayDir = null) {
  const f = findEntry(board, id); if (!f) return { found: false };
  const e = f.entry, own = !!core && e.core === core;
  e.verifyNote = String(reason).slice(0, 200); e.verifiedValue = Number.isFinite(value) ? value : null;
  if (ok === null) { e.attempts = (e.attempts ?? 0) + 1; if (e.attempts >= 3 && core) (e.tried ??= []).push(core); return { found: true, action: 'retry' }; }
  if (ok) {
    e.verified = true; e.verifiedCore = core; e.stale = false; e.attempts = 0;
    if (f.flagged) { f.list.splice(f.i, 1); const { event, flagged, ...entry } = e; return { found: true, action: 'listed', ...listEntry(board, event, entry, replayDir) }; }
    return { found: true, action: 'verified' };
  }
  if (core && !own) { (e.tried ??= []).push(core); e.stale = true; return { found: true, action: 'stale' }; }   // D7: another core's run
  e.verified = false;   // its own core does not reproduce it: pulled (rows skip verified === false); a flagged one stays flagged
  if (f.flagged) e.flagged = { ...e.flagged, reason: `${e.flagged.reason}, verify-failed` };
  return { found: true, action: f.flagged ? 'kept-flagged' : 'pulled' };
}
// What the verifier should run on core `core`: unverified runs (flagged first), then runs verified on another core (D7 (3)).
export function verifyQueue(board, core, limit = 20) {
  const out = [], seen = (e) => (e.tried ?? []).includes(core);
  const item = (e, key, flagged) => ({ id: e.id, event: key, claim: e.value, core: e.core, flagged });
  for (const e of board.flagged) if (e.verified == null && !seen(e)) out.push(item(e, e.event, true));
  for (const [key, list] of Object.entries(board.events)) for (const e of list) if (e.replay && e.verified == null && !seen(e)) out.push(item(e, key, false));
  for (const [key, list] of Object.entries(board.events)) {
    for (const e of list) if (e.verified === true && core && e.verifiedCore !== core && !seen(e)) out.push(item(e, key, false));
  }
  return out.slice(0, limit);
}

function bucket([rate, burst], at = Date.now()) { return { tokens: burst, rate, burst, at }; }
function take(b, t = Date.now()) {
  b.tokens = Math.min(b.burst, b.tokens + (Math.max(0, t - b.at) / 1000) * b.rate);
  b.at = Math.max(b.at, t);
  if (b.tokens < 1) return false;
  b.tokens -= 1;
  return true;
}

// dir: the state directory (MP_RECORDS_DIR); assets: the game data root (CAREER/career.json, <code>/npc-riders.json);
// floors: a record-floors.json document in place of the shipped one (tests); verifierToken: the verifier's shared secret (or null).
export function createRecords({ dir, assets, now = () => Date.now(), clientAddress = (req) => req.socket?.remoteAddress ?? '', log = console,
  floors: recordFloors = undefined, verifierToken = null } = {}) {
  let ok = false, board = null, stamp = null;
  const career = assets ? readJson(path.join(assets, 'CAREER', 'career.json')) : null;
  const events = eventTable(career);
  const floors = floorTable(events, assets, recordFloors === undefined ? readJson(FLOORS_FILE) : recordFloors);
  const boardFile = dir && path.join(dir, 'board.json'), replayDir = dir && path.join(dir, 'replays');
  const fileStamp = () => { try { const s = fs.statSync(boardFile); return `${s.mtimeMs}:${s.size}`; } catch { return null; } };
  if (dir && events.size) {
    try { fs.mkdirSync(replayDir, { recursive: true }); board = loadBoard(boardFile); stamp = fileStamp(); ok = true; }
    catch (e) { log.warn?.('records: off (state directory unusable)', e?.message); }
  } else if (dir) log.warn?.('records: off (no CAREER/career.json under the assets root)');
  // records-admin.mjs edits board.json on the host: a changed file is read again before the next request
  const sync = () => { const s = fileStamp(); if (s && s !== stamp) { board = loadBoard(boardFile); stamp = s; } };
  const save = () => { saveBoard(boardFile, board); stamp = fileStamp(); };

  // Rows of an event: the listed runs plus the defaults, in rank order.
  const defaults = (ev) => ev.defaults.slice(0, 5).map((d, i) => ({ id: `d${ev.slot}-${i}`, name: d.name, character: d.character, rider: null,
    value: ev.timed ? d.value * 60 : d.value, default: true, replay: false, verified: null, core: null, at: null, seq: 0 }));
  const rows = (key) => [...defaults(events.get(key)), ...(board.events[key] ?? []).filter((r) => r.verified !== false)].sort((a, b) => better(key, a, b));
  const pub = (r, rank) => ({
    id: r.id,
    rank,
    name: r.name,
    character: r.character,
    rider: r.rider,
    value: r.value,
    ...(r.default ? { default: true } : {}),
    replay: !!r.replay,
    verified: r.verified ?? null,
    ...(r.stale ? { stale: true } : {}),
    core: r.core ?? null,
    at: r.at ?? null
  });
  const limits = new Map();
  setInterval(() => { const t = now(); for (const [k, v] of limits) if (t - v.minute.at > 3600e3) limits.delete(k); }, 600e3).unref?.();

  const api = {
    get enabled() { return ok; }, events, floors,
    summary() {
      sync();
      const out = {};
      for (const [key, ev] of events) out[key] = { timed: ev.timed, top: rows(key).slice(0, 5).map(pub).map((r, i) => ({ ...r, rank: i })) };
      return { version: 1, events: out };
    },
    board(key, offset = 0, limit = MAX_LIMIT) {
      sync();
      const ev = events.get(key); if (!ev) return null;
      const all = rows(key), o = Math.max(0, offset | 0), n = Math.max(1, Math.min(MAX_LIMIT, limit | 0 || MAX_LIMIT));
      return { event: key, timed: ev.timed, total: all.length, rows: all.slice(o, o + n).map((r, i) => pub(r, o + i)) };
    },
    replayPath(id) { if (!/^[0-9a-f]{16}$/.test(id ?? '')) return null; const p = path.join(replayDir, id + '.bin'); return fs.existsSync(p) ? p : null; },
    // compressed: the uploaded body (deflate-raw of a replay file). Returns {status, body}.
    submit(compressed, address = '') {
      if (!ok) return { status: 503, body: { error: 'off' } };
      sync();
      let lim = limits.get(address); if (!lim) limits.set(address, (lim = { minute: bucket(LIMITS.minute, now()), hour: bucket(LIMITS.hour, now()) }));
      if (!take(lim.minute, now()) || !take(lim.hour, now())) return { status: 429, body: { error: 'rate' } };
      if (!compressed?.length || compressed.length > MAX_UPLOAD) return { status: 413, body: { error: 'size' } };
      let file;
      try { file = decodeReplayFile(zlib.inflateRawSync(compressed, { maxOutputLength: MAX_INFLATED })); } catch { return { status: 400, body: { error: 'replay' } }; }
      const m = file.meta, key = m.event, ev = events.get(key);
      if (!ev || m.mode !== ev.mode || m.course !== ev.code) return { status: 400, body: { error: 'event' } };
      const name = cleanName(m.name); if (!name) return { status: 400, body: { error: 'name' } };
      const value = ev.timed ? m.claim?.ticks : m.claim?.score;
      if (!Number.isInteger(value) || value <= 0 || value > (ev.timed ? 60 * 60 * 30 : 1e8)) return { status: 400, body: { error: 'claim' } };
      if (m.giveUp || m.dnf) return { status: 400, body: { error: 'claim' } };   // forced finishes are not submitted (docs/online-records.md)
      const pad = padStreamInfo(file.pad);
      if (!pad || !Number.isInteger(m.finishTick) || m.finishTick < 0 || m.ticks !== m.finishTick + 1 || pad.lastTick > m.finishTick
        || file.pad.length > MAX_BYTES_PER_TICK * m.ticks + 64) return { status: 400, body: { error: 'replay' } };
      // race clock vs recorded ticks (countdown 179 / 180, 0 rolling)
      if (ev.timed && (value > m.ticks || value < m.ticks - 400)) return { status: 400, body: { error: 'claim' } };
      if (!Number.isInteger(m.character) || m.character < 0 || m.character > 9) return { status: 400, body: { error: 'rider' } };
      if (typeof m.core !== 'string' || !/^[0-9a-f]{8,64}$/.test(m.core)) return { status: 400, body: { error: 'core' } };
      const lower = name.toLowerCase(), mine = (board.events[key] ?? []).find((r) => r.name.toLowerCase() === lower && r.verified !== false);
      const entry = { id: crypto.randomBytes(8).toString('hex'), name, character: m.character, rider: typeof m.rider?.id === 'string' ? m.rider.id.slice(0, 24) : null,
        value, replay: true, verified: null, core: m.core, build: typeof m.build === 'string' ? m.build.slice(0, 40) : null, at: day(now()), seq: ++board.seq };
      const top = () => rows(key).slice(0, 5).map(pub);
      if (mine && better(key, mine, entry) <= 0) return { status: 200, body: { ok: true, kept: false, id: mine.id, rank: rows(key).indexOf(mine), top: top() } };
      fs.writeFileSync(path.join(replayDir, entry.id + '.bin'), compressed);
      const floor = floors.get(key);
      if (floor && value < floor.ticks) {   // under the floor: kept for the verifier / an admin, not listed
        flagEntry(board, key, entry, { reason: 'under-floor', floor: floor.ticks, source: floor.source }, replayDir); save();
        return { status: 200, body: { ok: true, kept: false, review: true, id: entry.id, rank: -1, top: top() } };
      }
      listEntry(board, key, entry, replayDir); save();
      const all = rows(key), rank = all.indexOf(entry);
      return { status: 200, body: { ok: true, kept: rank >= 0, id: entry.id, rank, top: all.slice(0, 5).map(pub) } };
    },
    // The verifier: its result for one entry (verifyEntry), or what to run next.
    verify(id, result) {
      if (!ok) return false;
      sync();
      const r = verifyEntry(board, id, typeof result === 'object' && result ? result : { ok: !!result }, replayDir);
      if (r.found) save();
      return r.found ? r : false;
    },
    queue(core, limit) { if (!ok) return []; sync(); return verifyQueue(board, core, limit); },
    get stored() { sync(); return board; },   // the board document (tests, the admin's view)
    // node:http glue: true when the request was a records request (answered).
    handle(req, res) {
      const url = new URL(req.url, 'http://x'), p = url.pathname;
      if (p !== '/mp/records' && !p.startsWith('/mp/records/')) return false;
      const json = (code, body) => { res.writeHead(code, { 'content-type': 'application/json', 'cache-control': 'no-store' }); res.end(JSON.stringify(body)); };
      if (!ok) { req.resume(); json(503, { error: 'off' }); return true; }
      if (req.method === 'GET' && p === '/mp/records') { json(200, api.summary()); return true; }
      if (req.method === 'GET' && p === '/mp/records/board') {
        const b = api.board(
          url.searchParams.get('event'),
          +url.searchParams.get('offset') || 0,
          +url.searchParams.get('limit') || MAX_LIMIT
        );
        json(b ? 200 : 404, b ?? { error: 'event' });
        return true;
      }
      if (req.method === 'GET' && p === '/mp/records/replay') {
        const f = api.replayPath(url.searchParams.get('id')); if (!f) { json(404, { error: 'replay' }); return true; }
        const bytes = fs.readFileSync(f);
        res.writeHead(200, { 'content-type': 'application/octet-stream', 'content-length': bytes.length, 'cache-control': 'private, max-age=31536000, immutable' });
        res.end(bytes); return true;
      }
      if (req.method === 'POST' && p === '/mp/records/submit') {
        const chunks = []; let size = 0, over = false;
        req.on('data', (d) => { if (over) return; size += d.length; if (size > MAX_UPLOAD) { over = true; chunks.length = 0; json(413, { error: 'size' }); return; } chunks.push(d); });
        req.on('end', () => { if (over) return; const r = api.submit(Buffer.concat(chunks), clientAddress(req)); json(r.status, r.body); });
        req.on('error', () => {});
        return true;
      }
      if (p.startsWith('/mp/records/verifier/')) {
        // only the host's verifier: loopback, not through the tunnel (cloudflared adds CF-Connecting-IP), with the token; else 404
        const local = ['127.0.0.1', '::1', '::ffff:127.0.0.1'].includes(req.socket?.remoteAddress) && !req.headers['cf-connecting-ip'];
        const given = Buffer.from(String(req.headers['x-ssx-verifier'] ?? '')), want = Buffer.from(verifierToken ? String(verifierToken) : '');
        const tokenOk = want.length >= 16 && given.length === want.length && crypto.timingSafeEqual(given, want);
        if (!local || !tokenOk) { req.resume(); json(404, { error: 'not found' }); return true; }
        if (req.method === 'GET' && p === '/mp/records/verifier/queue') {
          json(200, { items: api.queue(url.searchParams.get('core') || null, Math.min(50, +url.searchParams.get('limit') || 20)) }); return true;
        }
        if (req.method === 'POST' && p === '/mp/records/verifier/result') {
          let body = '';
          req.on('data', (d) => { body += d; if (body.length > 8192) req.destroy(); });
          req.on('end', () => {
            let m; try { m = JSON.parse(body); } catch { json(400, { error: 'json' }); return; }
            const r = api.verify(String(m.id ?? ''), { ok: m.ok === null ? null : !!m.ok, reason: m.reason, core: typeof m.core === 'string' ? m.core : null, value: m.value });
            json(r ? 200 : 404, r || { error: 'id' });
          });
          return true;
        }
      }
      req.resume(); json(404, { error: 'not found' }); return true;
    },
  };
  return api;
}
