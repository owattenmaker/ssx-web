// Online course records (docs/online-records.md): one board per event (the PS2's record slots 6..25: the 14 standard events and
// the 6 backcountry rival events; the peak runs keep the local table), each run with its replay. Mounted by mp-server.mjs behind
// the password gate; dependency-free.
//
//   GET  /mp/records                      -> {version, events: {"<mode>:<COURSE>": {timed, top: [row x 5]}}}
//   GET  /mp/records/board?event=K&offset=O&limit=N -> {event, timed, total, rows: [row]} (rank order, at most MAX_LIMIT a call)
//   GET  /mp/records/replay?id=ID         -> the stored replay (deflate-raw of web/server/replay-file.mjs), immutable
//   POST /mp/records/submit               -> body: deflate-raw of a replay file; {ok, id, rank, kept, top}
//   row = {id, rank, name, character, rider, value, default?, replay, verified, core, at}
//
// Rules (PS2 0x154AB8 first, then the coordinator's decisions D2-D7, 2026-09-30):
//   - order: time events by ticks ascending, score events by score descending, then the earlier submission (seq); the PS2 ranks
//     whole seconds with a tie going to the newer run (0x154D58), which the online boards do not use (D3);
//   - the PS2 defaults (0x43FB28, CAREER/career.json rules.records) seed every board as replay-less `default` rows with seq 0 and
//     never count for the one-entry-per-name rule (D4);
//   - one entry per name (case-insensitive) per event, the best kept (D2); a worse run under a listed name is not stored;
//   - listed at once; `verified` stays null until a verifier (second pass) sets it; a failed entry is pulled (D6);
//   - names: the game keyboard's Player Name characters (fe-screens.js KEYS_OFF.name: letters, digits, !@#$%^&*(), space),
//     1..8 characters (1CD088(kb, 8)), trimmed; a small blocklist;
//   - tier-0 checks (no simulation): the replay decodes, its meta matches the claim, the pad stream ends by the finish tick, the
//     time is not below the course floor (route length at plausibility.mjs MAX_AVERAGE), sizes are bounded.
// Privacy: nothing about the client is stored (no address, user agent, cookie); the rate limits key on the address in memory
// only; `at` is the UTC day.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import zlib from 'node:zlib';
import { MAX_AVERAGE } from './plausibility.mjs';
import { decodeReplayFile, padStreamInfo } from './replay-file.mjs';

export const MAX_ENTRIES = 100;          // real runs kept per event (defaults on top of these)
export const MAX_LIMIT = 100;            // rows per board request
export const MAX_UPLOAD = 512 * 1024;    // compressed bytes
export const MAX_INFLATED = 4 * 1048576; // decompressed bytes
export const MAX_BYTES_PER_TICK = 64;    // pad stream bytes per recorded tick (an analog run is ~7)
export const NAME_RE = /^[A-Za-z0-9 !@#$%^&*()]{1,8}$/;
export const LIMITS = { minute: [6 / 60, 6], hour: [30 / 3600, 30] };   // per client address: rate per s, burst
const BLOCK = ['fuck', 'shit', 'cunt', 'nigg', 'fag', 'rape', 'nazi', 'hitler', 'kkk', 'slut', 'whore', 'dick', 'cock', 'pussy', 'penis', 'bitch'];
const TIMED_MODES = new Set([0, 4]);     // race, Rival Time (career.js isTimed for modes < 6)

export function cleanName(name) {
  if (typeof name !== 'string') return null;
  const n = name.replace(/\s+/g, ' ').trim();
  if (!NAME_RE.test(n)) return null;
  const flat = n.toLowerCase().replace(/[^a-z]/g, '').replace(/0/g, 'o');
  if (BLOCK.some((w) => flat.includes(w))) return null;
  return n;
}
const day = (t) => new Date(t).toISOString().slice(0, 10);

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

function bucket([rate, burst], at = Date.now()) { return { tokens: burst, rate, burst, at }; }
function take(b, t = Date.now()) {
  b.tokens = Math.min(b.burst, b.tokens + (Math.max(0, t - b.at) / 1000) * b.rate);
  b.at = Math.max(b.at, t);
  if (b.tokens < 1) return false;
  b.tokens -= 1;
  return true;
}

// dir: the state directory (MP_RECORDS_DIR); assets: the game data root (CAREER/career.json, <code>/npc-riders.json).
export function createRecords({ dir, assets, now = () => Date.now(), clientAddress = (req) => req.socket?.remoteAddress ?? '', log = console } = {}) {
  let ok = false, board = null;
  const readJson = (p) => { try { return JSON.parse(fs.readFileSync(p, 'utf8')); } catch { return null; } };
  const career = assets ? readJson(path.join(assets, 'CAREER', 'career.json')) : null;
  const events = eventTable(career);
  const floors = new Map();   // event -> minimum ticks (route length / MAX_AVERAGE), race courses with npc-riders.json
  for (const [key, ev] of events) {
    if (!ev.timed || !assets) continue;
    const doc = readJson(path.join(assets, ev.code, 'npc-riders.json')), route = doc?.riders?.[0]?.progress_origin;
    if (route > 0) floors.set(key, Math.floor(route / MAX_AVERAGE * 60));
  }
  const boardFile = dir && path.join(dir, 'board.json'), replayDir = dir && path.join(dir, 'replays');
  if (dir && events.size) {
    try {
      fs.mkdirSync(replayDir, { recursive: true });
      board = readJson(boardFile);
      if (!board || board.version !== 1 || typeof board.events !== 'object') board = { version: 1, seq: 0, events: {} };
      ok = true;
    } catch (e) { log.warn?.('records: off (state directory unusable)', e?.message); }
  } else if (dir) log.warn?.('records: off (no CAREER/career.json under the assets root)');

  // Rows of an event: the stored runs plus the defaults, in rank order.
  const defaults = (ev) => ev.defaults.slice(0, 5).map((d, i) => ({ id: `d${ev.slot}-${i}`, name: d.name, character: d.character, rider: null,
    value: ev.timed ? d.value * 60 : d.value, default: true, replay: false, verified: null, core: null, at: null, seq: 0 }));
  const better = (ev, a, b) => (ev.timed ? a.value - b.value : b.value - a.value) || a.seq - b.seq;
  const rows = (key) => { const ev = events.get(key); return [...defaults(ev), ...(board.events[key] ?? []).filter((r) => r.verified !== false)].sort((a, b) => better(ev, a, b)); };
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
    core: r.core ?? null,
    at: r.at ?? null
  });
  const save = () => { const tmp = boardFile + '.tmp'; fs.writeFileSync(tmp, JSON.stringify(board)); fs.renameSync(tmp, boardFile); };
  const limits = new Map();
  setInterval(() => { const t = now(); for (const [k, v] of limits) if (t - v.minute.at > 3600e3) limits.delete(k); }, 600e3).unref?.();

  const api = {
    get enabled() { return ok; }, events,
    summary() {
      const out = {};
      for (const [key, ev] of events) out[key] = { timed: ev.timed, top: rows(key).slice(0, 5).map(pub).map((r, i) => ({ ...r, rank: i })) };
      return { version: 1, events: out };
    },
    board(key, offset = 0, limit = MAX_LIMIT) {
      const ev = events.get(key); if (!ev) return null;
      const all = rows(key), o = Math.max(0, offset | 0), n = Math.max(1, Math.min(MAX_LIMIT, limit | 0 || MAX_LIMIT));
      return { event: key, timed: ev.timed, total: all.length, rows: all.slice(o, o + n).map((r, i) => pub(r, o + i)) };
    },
    replayPath(id) { if (!/^[0-9a-f]{16}$/.test(id ?? '')) return null; const p = path.join(replayDir, id + '.bin'); return fs.existsSync(p) ? p : null; },
    // compressed: the uploaded body (deflate-raw of a replay file). Returns {status, body}.
    submit(compressed, address = '') {
      if (!ok) return { status: 503, body: { error: 'off' } };
      let lim = limits.get(address); if (!lim) limits.set(address, (lim = { minute: bucket(LIMITS.minute, now()), hour: bucket(LIMITS.hour, now()) }));
      if (!take(lim.minute, now()) || !take(lim.hour, now())) return { status: 429, body: { error: 'rate' } };
      if (!compressed?.length || compressed.length > MAX_UPLOAD) return { status: 413, body: { error: 'size' } };
      let file; try { file = decodeReplayFile(zlib.inflateRawSync(compressed, { maxOutputLength: MAX_INFLATED })); } catch { return { status: 400, body: { error: 'replay' } }; }
      const m = file.meta, key = m.event, ev = events.get(key);
      if (!ev || m.mode !== ev.mode || m.course !== ev.code) return { status: 400, body: { error: 'event' } };
      const name = cleanName(m.name); if (!name) return { status: 400, body: { error: 'name' } };
      const value = ev.timed ? m.claim?.ticks : m.claim?.score;
      if (!Number.isInteger(value) || value <= 0 || value > (ev.timed ? 60 * 60 * 30 : 1e8)) return { status: 400, body: { error: 'claim' } };
      if (m.giveUp || m.dnf) return { status: 400, body: { error: 'claim' } };   // forced finishes are not submitted (docs/online-records.md)
      const pad = padStreamInfo(file.pad);
      if (!pad || !Number.isInteger(m.finishTick) || m.finishTick < 0 || m.ticks !== m.finishTick + 1 || pad.lastTick > m.finishTick
        || file.pad.length > MAX_BYTES_PER_TICK * m.ticks + 64) return { status: 400, body: { error: 'replay' } };
      if (ev.timed && (value > m.ticks || value < m.ticks - 400)) return { status: 400, body: { error: 'claim' } };   // race clock vs recorded ticks (countdown 179 / 180, 0 rolling)
      if (ev.timed && floors.has(key) && value < floors.get(key)) return { status: 400, body: { error: 'claim' } };
      if (!Number.isInteger(m.character) || m.character < 0 || m.character > 9) return { status: 400, body: { error: 'rider' } };
      if (typeof m.core !== 'string' || !/^[0-9a-f]{8,64}$/.test(m.core)) return { status: 400, body: { error: 'core' } };
      // one entry per name, the best kept
      const list = (board.events[key] ??= []), lower = name.toLowerCase(), mine = list.find((r) => r.name.toLowerCase() === lower);
      const entry = { id: crypto.randomBytes(8).toString('hex'), name, character: m.character, rider: typeof m.rider?.id === 'string' ? m.rider.id.slice(0, 24) : null,
        value, replay: true, verified: null, core: m.core, build: typeof m.build === 'string' ? m.build.slice(0, 40) : null, at: day(now()), seq: ++board.seq };
      if (mine && better(ev, mine, entry) <= 0) { const all = rows(key); return { status: 200, body: { ok: true, kept: false, id: mine.id, rank: all.indexOf(mine), top: all.slice(0, 5).map(pub) } }; }
      fs.writeFileSync(path.join(replayDir, entry.id + '.bin'), compressed);
      if (mine) { list.splice(list.indexOf(mine), 1); api.dropReplay(mine.id); }
      list.push(entry); list.sort((a, b) => better(ev, a, b));
      for (const r of list.splice(MAX_ENTRIES)) api.dropReplay(r.id);
      save();
      const all = rows(key), rank = all.indexOf(entry);
      return { status: 200, body: { ok: true, kept: rank >= 0, id: entry.id, rank, top: all.slice(0, 5).map(pub) } };
    },
    // The verifier (second pass): true keeps the entry and marks it; false pulls it (D6).
    verify(id, result) {
      if (!ok) return false;
      for (const list of Object.values(board.events)) { const r = list.find((x) => x.id === id); if (r) { r.verified = !!result; save(); return true; } }
      return false;
    },
    dropReplay(id) { try { fs.unlinkSync(path.join(replayDir, id + '.bin')); } catch {} },
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
        res.writeHead(200, { 'content-type': 'application/octet-stream', 'content-length': bytes.length, 'cache-control': 'private, max-age=31536000, immutable' }); res.end(bytes); return true;
      }
      if (req.method === 'POST' && p === '/mp/records/submit') {
        const chunks = []; let size = 0, over = false;
        req.on('data', (d) => { if (over) return; size += d.length; if (size > MAX_UPLOAD) { over = true; chunks.length = 0; json(413, { error: 'size' }); return; } chunks.push(d); });
        req.on('end', () => { if (over) return; const r = api.submit(Buffer.concat(chunks), clientAddress(req)); json(r.status, r.body); });
        req.on('error', () => {});
        return true;
      }
      req.resume(); json(404, { error: 'not found' }); return true;
    },
  };
  return api;
}
