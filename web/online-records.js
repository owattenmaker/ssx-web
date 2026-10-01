// Online course records, the page's side (pv onlineRecords, docs/online-records.md): the server's boards (web/server/records.mjs),
// a last-good copy for offline use, the rank a finished run would take, and the uploads / downloads of run replays
// (web/server/replay-file.mjs). No UI here: web/online-records-ui.js draws the game's screens.
import { encodeReplayFile, decodeReplayFile } from './server/replay-file.mjs';

const CACHE_KEY = 'ssx3.onlineRecords.v1', TIMEOUT_MS = 5000;
export const eventKeyOf = (mode, code) => `${mode}:${code}`;
const store = () => { try { return globalThis.localStorage ?? null; } catch { return null; } };

// The rank a run takes among `rows` (rank order): time events by ticks, score events by score, a tie going to the earlier
// submission (records.mjs, decision D3), so the run must be strictly better than the row it passes. A name already listed
// (case-insensitive, not a default) with an equal or better value keeps the run out (one entry per name, D2). -1 = not in rows.
export function rankAmong(rows, { timed, value, name = null }) {
  const lower = name?.toLowerCase(), mine = lower && rows.find((r) => !r.default && r.name?.toLowerCase() === lower);
  if (mine && (timed ? mine.value <= value : mine.value >= value)) return -1;
  const others = mine ? rows.filter((r) => r !== mine) : rows;
  const at = others.findIndex((r) => (timed ? value < r.value : value > r.value));
  return at < 0 ? (others.length < rows.length ? others.length : -1) : at;
}

async function timed(fetchImpl, url, init = {}) {
  const ctl = typeof AbortController === 'function' ? new AbortController() : null, t = ctl ? setTimeout(() => ctl.abort(), TIMEOUT_MS) : null;
  try { return await fetchImpl(url, { ...init, ...(ctl ? { signal: ctl.signal } : {}) }); } finally { if (t) clearTimeout(t); }
}
async function deflate(bytes) { const s = new Blob([bytes]).stream().pipeThrough(new CompressionStream('deflate-raw')); return new Uint8Array(await new Response(s).arrayBuffer()); }
async function inflate(bytes) { const s = new Blob([bytes]).stream().pipeThrough(new DecompressionStream('deflate-raw')); return new Uint8Array(await new Response(s).arrayBuffer()); }

export class OnlineRecords {
  constructor({ fetchImpl = (...a) => globalThis.fetch(...a), base = '' } = {}) {
    this.fetch = fetchImpl; this.base = base; this.summary = null; this.status = 'idle'; this.loading = null; this.coreHashValue = null;
    try { const c = JSON.parse(store()?.getItem(CACHE_KEY) || 'null'); if (c?.version === 1 && c.events) { this.summary = c; this.status = 'cached'; } } catch {}
  }
  // GET /mp/records (at most every 30 s unless forced): online, or the last good copy ('cached'), or nothing ('offline').
  load(force = false) {
    if (this.loading) return this.loading;
    if (!force && this.status === 'online' && performance.now() - this.loadedAt < 30000) return Promise.resolve(this.summary);
    this.loading = (async () => {
      try {
        const r = await timed(this.fetch, this.base + '/mp/records', { cache: 'no-store' });
        if (!r.ok) throw new Error(String(r.status));
        const s = await r.json(); if (s?.version !== 1 || !s.events) throw new Error('format');
        this.summary = s; this.status = 'online'; this.loadedAt = performance.now();
        try { store()?.setItem(CACHE_KEY, JSON.stringify(s)); } catch {}
      } catch { this.status = this.summary ? 'cached' : 'offline'; }
      finally { this.loading = null; }
      return this.summary;
    })();
    return this.loading;
  }
  get online() { return this.status === 'online'; }
  has(key) { return !!this.summary?.events?.[key]; }
  top(key) { return this.summary?.events?.[key]?.top ?? null; }
  timed(key) { return !!this.summary?.events?.[key]?.timed; }
  // the rank a finished run would take in the event's top 5 (-1: none, or no board)
  rank(key, value, name) { const top = this.top(key); return top ? rankAmong(top, { timed: this.timed(key), value, name }) : -1; }
  async board(key, offset = 0, limit = 100) {
    const r = await timed(this.fetch, `${this.base}/mp/records/board?event=${encodeURIComponent(key)}&offset=${offset}&limit=${limit}`, { cache: 'no-store' });
    if (!r.ok) throw new Error(`board ${r.status}`);
    return r.json();
  }
  // meta + pad (web/server/replay-file.mjs) -> {ok, kept, id, rank, top} or {error}
  async submit(meta, pad) {
    let r;
    try { r = await timed(this.fetch, this.base + '/mp/records/submit', { method: 'POST', headers: { 'content-type': 'application/octet-stream' }, body: await deflate(encodeReplayFile({ meta, pad })) }); }
    catch { return { error: 'network' }; }
    const body = await r.json().catch(() => ({ error: 'network' }));
    if (r.ok && body.top && this.summary?.events?.[meta.event]) {   // the board as the server has it now
      this.summary.events[meta.event].top = body.top.map((row, i) => ({ ...row, rank: i }));
      try { store()?.setItem(CACHE_KEY, JSON.stringify(this.summary)); } catch {}
    }
    return r.ok ? body : { error: body.error || String(r.status) };
  }
  async replay(id) {
    const r = await timed(this.fetch, `${this.base}/mp/records/replay?id=${encodeURIComponent(id)}`);
    if (!r.ok) throw new Error(`replay ${r.status}`);
    return decodeReplayFile(await inflate(new Uint8Array(await r.arrayBuffer())));
  }
  // The core's identity for a replay (sha-256 of core.wasm, 16 hex): the build's own file, a cache hit after the page loaded it.
  async coreHash(url) {
    if (this.coreHashValue) return this.coreHashValue;
    try {
      const bytes = await (await this.fetch(url)).arrayBuffer(), d = new Uint8Array(await crypto.subtle.digest('SHA-256', bytes));
      this.coreHashValue = [...d.slice(0, 8)].map((x) => x.toString(16).padStart(2, '0')).join('');
    } catch { this.coreHashValue = '00000000'; }
    return this.coreHashValue;
  }
}
