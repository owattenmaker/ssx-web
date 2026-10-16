// Online race boards (pv mpBoard, docs/multiplayer.md "Multiplayer leaderboard"): one board per event for the results of normal online
// races (web/server/mp-server.mjs lobbies), separate from the verified course records (web/server/records.mjs board.json). Online races
// include contacts and knockouts from other players, so the replay verifier cannot re-check a run alone: these boards are unverified
// (the user's decision, 2026-10-15), like the live time-attack podium. Imports only node: and web/server/ (the host runs this folder
// on its own).
//
//   GET  /mp/race-board?event=K | course=C [&name=N][&limit=L] -> {event, timed, total, rows: [row x L (10)], you: row | null}
//   POST /mp/race-board/save   header x-ssx-session: the client's reconnect token; body JSON {race, name}
//                              -> {ok, kept, id, rank, event, timed, total, rows, you} or {error}
//   row = {id, rank, name, rider, value, race, at}
//
// Rules:
//   - a save is backed by a real online race the server ran: mp-server hands every finished race to raceDone() (its own finish
//     records: the claimed ticks it accepted, the plausibility verdict, the score of a trick event's finish message); a save names the
//     race id and comes from the token of a racer in it, who finished (not DNF, not found implausible), once per race, within
//     RACE_KEEP_MS of the results;
//   - the value is the server's record, never the client's request: race ticks for the timed events (race, Rival Time), the finish
//     message's score for the trick events (slope style, super pipe, big air; unverified);
//   - the event of a course: career.json record_slots' first pair (records.mjs eventTable: 0 race, 1 slope style, 2 super pipe,
//     3 big air, 4 Rival Time for a backcountry course), the same keys as the course records ("<mode>:<COURSE>");
//   - order: time events by ticks ascending, score events by score descending, then the earlier entry (records.mjs better);
//   - one entry per name (case-insensitive) per event, the best kept; at most MAX_ENTRIES an event;
//   - names: records.mjs cleanName (trimmed, the game keyboard's Player Name characters, 1..8, its small blocklist): the PS2 keyboard
//     takes 8 characters (1CD088(kb, 8) at 0x1837A4) and Player Name turns off Tab and the punctuation keys (1CE3C8, 0x1837C4..0x1838A0);
//   - rate limits per connection (the lobby client's id), in memory only: 6 a minute, 30 an hour.
// Privacy: no address, user agent or token is stored; `at` is the UTC day.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { cleanName, better, timedEvent } from './records.mjs';

export const MAX_ENTRIES = 200;
export const MAX_LIMIT = 50;
export const RACE_KEEP_MS = 30 * 60000;
export const MAX_RACES = 500;
export const LIMITS = { minute: [6 / 60, 6], hour: [30 / 3600, 30] };
const MAX_BODY = 2048;

const day = (t) => new Date(t).toISOString().slice(0, 10);
function bucket([rate, burst], at) {
  return { tokens: burst, rate, burst, at };
}
function take(b, t) {
  b.tokens = Math.min(b.burst, b.tokens + (Math.max(0, t - b.at) / 1000) * b.rate);
  b.at = Math.max(b.at, t);
  if (b.tokens < 1) return false;
  b.tokens -= 1;
  return true;
}
// course code -> event key ("<mode>:<COURSE>"): the course's first record-slot pair (records.mjs eventTable rows are in table order)
export function courseEvents(events) {
  const out = new Map();
  for (const [key, ev] of events) {
    const known = out.get(ev.code);
    if (!known || events.get(known).mode > ev.mode) out.set(ev.code, key);
  }
  return out;
}

// events: records.mjs eventTable(career); dir: the state directory (MP_RECORDS_DIR) or null (off)
export function createRaceBoard({ dir, events, now = () => Date.now(), log = console } = {}) {
  let ok = false;
  let board = { version: 1, seq: 0, events: {} };
  const file = dir ? path.join(dir, 'race-board.json') : null;
  const byCourse = courseEvents(events ?? new Map());
  if (file && byCourse.size) {
    try {
      fs.mkdirSync(dir, { recursive: true });
      const read = fs.existsSync(file) ? JSON.parse(fs.readFileSync(file, 'utf8')) : null;
      if (read?.version === 1 && read.events && typeof read.events === 'object') board = read;
      ok = true;
    } catch (e) {
      log.warn?.('race board: off (state unusable)', e?.message);
    }
  }
  const save = () => {
    const tmp = file + '.tmp';
    fs.writeFileSync(tmp, JSON.stringify(board));
    fs.renameSync(tmp, file);
  };
  // finished races: id -> {key, at, racers: Map(clientId -> {ticks, dnf, score, verified, rider}), saved: Set(clientId)}
  const races = new Map();
  const limits = new Map();
  const prune = () => {
    const t = now();
    for (const [id, r] of races) if (t - r.at > RACE_KEEP_MS) races.delete(id);
    while (races.size > MAX_RACES) races.delete(races.keys().next().value);
    for (const [k, v] of limits) if (t - v.hour.at > 3600e3) limits.delete(k);
  };
  const pub = (r, rank) => ({ id: r.id, rank, name: r.name, rider: r.rider, value: r.value, race: r.race, at: r.at });
  const listOf = (key) => board.events[key] ?? [];
  const view = (key, name, limit = 10) => {
    const list = listOf(key);
    const n = Math.max(1, Math.min(MAX_LIMIT, limit | 0 || 10));
    const lower = typeof name === 'string' ? name.trim().toLowerCase() : '';
    const mine = lower ? list.findIndex((r) => r.name.toLowerCase() === lower) : -1;
    return {
      event: key,
      timed: timedEvent(key),
      total: list.length,
      rows: list.slice(0, n).map((r, i) => pub(r, i)),
      you: mine >= 0 ? pub(list[mine], mine) : null
    };
  };

  const api = {
    get enabled() {
      return ok;
    },
    eventOf: (code) => byCourse.get(code) ?? null,
    // mp-server maybeResults: a finished race. racers: [{id (client id), ticks, dnf, score, verified, rider}]
    raceDone({ id, course, racers }) {
      if (!ok || !id) return;
      const key = byCourse.get(course);
      if (!key) return;
      prune();
      const map = new Map();
      for (const r of racers ?? []) map.set(r.id, { ticks: r.ticks | 0, dnf: !!r.dnf, score: r.score, verified: r.verified, rider: r.rider });
      races.set(String(id), { key, at: now(), racers: map, saved: new Set() });
    },
    // a racer's save: clientId from the token (mp-server). -> {status, body}
    save(clientId, { race, name } = {}) {
      if (!ok) return { status: 503, body: { error: 'off' } };
      if (!clientId) return { status: 401, body: { error: 'token' } };
      prune();
      let lim = limits.get(clientId);
      if (!lim) limits.set(clientId, (lim = { minute: bucket(LIMITS.minute, now()), hour: bucket(LIMITS.hour, now()) }));
      if (!take(lim.minute, now()) || !take(lim.hour, now())) return { status: 429, body: { error: 'rate' } };
      const r = races.get(String(race ?? ''));
      if (!r) return { status: 404, body: { error: 'race' } };
      const mine = r.racers.get(clientId);
      if (!mine) return { status: 403, body: { error: 'racer' } };
      if (mine.dnf) return { status: 400, body: { error: 'dnf' } };
      if (mine.verified === false) return { status: 400, body: { error: 'invalid' } };
      if (r.saved.has(clientId)) return { status: 409, body: { error: 'saved' } };
      const timed = timedEvent(r.key);
      const value = timed ? mine.ticks : mine.score;
      // a race needs a time; a trick run that finished with no points is still a finished run (0 is listed, last)
      if (!Number.isInteger(value) || value < (timed ? 1 : 0) || value > (timed ? 60 * 60 * 30 : 1e8)) return { status: 400, body: { error: 'value' } };
      const clean = cleanName(name);
      if (!clean) return { status: 400, body: { error: 'name' } };
      r.saved.add(clientId);
      const entry = {
        id: crypto.randomBytes(8).toString('hex'),
        name: clean,
        rider: /^[a-z0-9_]{1,24}$/.test(String(mine.rider ?? '')) ? mine.rider : null,
        value,
        race: String(race).slice(0, 16),
        at: day(now()),
        seq: ++board.seq
      };
      const list = (board.events[r.key] ??= []);
      const lower = clean.toLowerCase();
      const old = list.find((x) => x.name.toLowerCase() === lower);
      let kept = true;
      if (old && better(r.key, old, entry) <= 0) kept = false;
      else {
        if (old) list.splice(list.indexOf(old), 1);
        list.push(entry);
        list.sort((a, b) => better(r.key, a, b));
        list.splice(MAX_ENTRIES);
        kept = list.includes(entry);
      }
      save();
      const v = view(r.key, clean);
      return { status: 200, body: { ok: true, kept, id: kept ? entry.id : (old?.id ?? null), rank: v.you?.rank ?? -1, name: clean, ...v } };
    },
    board: (key, name, limit) => (ok && events.has(key) ? view(key, name, limit) : null),
    get stored() {
      return board;
    },
    get races() {
      return races;
    },
    // node:http glue. clientOf(token) -> the lobby client's id or null. True when the request was this module's (answered).
    handle(req, res, clientOf) {
      const url = new URL(req.url, 'http://x');
      const p = url.pathname;
      if (p !== '/mp/race-board' && !p.startsWith('/mp/race-board/')) return false;
      const json = (code, body) => {
        res.writeHead(code, { 'content-type': 'application/json', 'cache-control': 'no-store' });
        res.end(JSON.stringify(body));
      };
      if (!ok) {
        req.resume();
        json(503, { error: 'off' });
        return true;
      }
      if (req.method === 'GET' && p === '/mp/race-board') {
        // event=K, or course=CODE (the course's event: the page knows its lobby's course)
        const key = url.searchParams.get('event') ?? api.eventOf(url.searchParams.get('course'));
        const b = api.board(key, url.searchParams.get('name'), +url.searchParams.get('limit') || 10);
        json(b ? 200 : 404, b ?? { error: 'event' });
        return true;
      }
      if (req.method === 'POST' && p === '/mp/race-board/save') {
        const token = String(req.headers['x-ssx-session'] ?? '');
        let body = '';
        let over = false;
        req.on('data', (d) => {
          if (over) return;
          body += d;
          if (body.length > MAX_BODY) {
            over = true;
            json(413, { error: 'size' });
          }
        });
        req.on('end', () => {
          if (over) return;
          let m;
          try {
            m = JSON.parse(body);
          } catch {
            json(400, { error: 'json' });
            return;
          }
          const r = api.save(token ? clientOf(token) : null, { race: m?.race, name: m?.name });
          json(r.status, r.body);
        });
        req.on('error', () => {});
        return true;
      }
      req.resume();
      json(404, { error: 'not found' });
      return true;
    }
  };
  return api;
}
