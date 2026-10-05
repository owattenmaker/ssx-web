// Game-data downloads (docs/hosting.md): every GET of /assets/* in the page goes through here (installed before any
// other module runs, web/main.js first import).
// - Same-URL requests share one download: the course package is read by main.js, ai-race.js, the rider loaders, ...
//   and a second request that starts before the first finishes (or while the HTTP cache declines a big entry) would
//   fetch it again. The finished body is kept for SHARE_MS so a follow-up request gets it from memory.
// - Bytes are counted as they arrive. The game's own screens show the progress in their own style (no extra widget):
//   the event loading screen's percentage (web/loading-screen.js), the title card's "Loading..." line and a
//   FEFONT "Loading..." in the menus (web/ui.js), all through downloadProgress().
// Responses stay ordinary Response objects (status, headers, body); each caller gets its own copy of the bytes.
// SHARE_MS: measured on a Snow Jam event load, every repeat request of an /assets URL starts within 1.8 s of the first
// finishing; 20 s kept the whole load (~107 MB of bodies) alive ~20 s into the race (Safari counts it against the
// page's memory-pressure thresholds).
import { pv } from './pv-flags.js';
const SHARE_MS = 5000, IDLE_RESET_MS = 700;
const nativeFetch = globalThis.fetch.bind(globalThis);
const shared = new Map(); // url -> Promise<{status, statusText, headers, bytes}>
// total: every body byte received in the page, never reset (a retry's bytes count again): the load screens' meter (pv loadMeter)
const state = { active: 0, files: 0, received: 0, expected: 0, since: 0, idleAt: 0, total: 0 };
export const downloadState = state;
// Per-file bytes as they stream (url, received, decoded size or 0): the title's load meter (web/boot-screen.js via main.js).
const byteListeners = new Set();
export function onDownloadBytes(fn) { byteListeners.add(fn); return () => byteListeners.delete(fn); }

function assetUrl(input, init) {
  if (init && (init.method && init.method !== 'GET' || init.body || init.signal || init.headers)) return null;
  const raw = typeof input === 'string' ? input : input instanceof URL ? input.href : null; // Request objects: untouched
  if (!raw) return null;
  let url; try { url = new URL(raw, location.href); } catch { return null; }
  return url.origin === location.origin && url.pathname.startsWith('/assets/') ? url.href : null;
}

// Transient failures retry with backoff (field reports: "TypeError: Failed to fetch" on Windows Chrome, "Error in input
// stream" on Linux Firefox: a body read that broke half way): network errors, a broken or stalled body (no byte for
// 20 s), and HTTP 408 / 429 / 5xx. 404 and other answers go to the caller as they are. After the last try the
// error (network: true) reaches the loader, whose failure shows on the game's own screens (the title card's
// "Load failed", web/main.js). Nothing retries while the page is being left (pagehide aborts the downloads).
export const downloadRetry = { delaysMs: [500, 1500, 4000, 8000], stallMs: 20000 }; // test-downloads.mjs shortens them
let leaving = false;
globalThis.addEventListener?.('pagehide', () => { leaving = true; });
globalThis.addEventListener?.('pageshow', () => { leaving = false; });
const transientStatus = (status) => status === 408 || status === 429 || status >= 500;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function downloadOnce(url) {
  const abort = new AbortController(); let stalled = false, timer = 0;
  const watchdog = () => { clearTimeout(timer); timer = setTimeout(() => { stalled = true; abort.abort(); }, downloadRetry.stallMs); };
  watchdog();
  let length = 0, got = 0, counted = false;
  try {
    const response = await nativeFetch(url, { signal: abort.signal });
    // Expected size: the server's X-Decoded-Length (web/server/mp-server.mjs; survives compression on the way), else the
    // Content-Length of an unencoded body; a compressed body without either grows the total as it arrives.
    const decoded = +response.headers.get('x-decoded-length') || 0, encoded = !!response.headers.get('content-encoding');
    length = decoded || (encoded ? 0 : +response.headers.get('content-length') || 0);
    if (!response.body || !response.ok) {
      watchdog();
      return {
        status: response.status,
        statusText: response.statusText,
        headers: response.headers,
        bytes: new Uint8Array(await response.arrayBuffer())
      };
    }
    state.active++; state.files++; state.expected += length; if (!state.since) state.since = performance.now(); counted = true;
    // A known size: the body goes straight into one buffer (no chunk list plus a second full copy to join it: on a phone the
    // Snow Jam package's ~68 MB peaked twice over while it arrived, docs/presentation.md); else the chunks are joined at the end.
    let chunks = [], whole = length && (pv('flyover') || pv('loadCopies')) ? new Uint8Array(length) : null;   // (with pv flyover / loadCopies until verified)
    const reader = response.body.getReader();
    for (;;) {
      watchdog(); const { done, value } = await reader.read(); if (done) break;
      if (whole && got + value.length <= whole.length) whole.set(value, got);
      else { if (whole) { chunks.push(whole.subarray(0, got)); whole = null; } chunks.push(value); } // the size was wrong: chunks from here
      got += value.length;
      state.received += value.length;
      state.total += value.length;
      if (!length) state.expected = Math.max(state.expected, state.received);
      for (const fn of byteListeners)
        try {
          fn(url, got, length);
        } catch {}
    }
    let bytes;
    if (whole) bytes = got === whole.length ? whole : whole.slice(0, got);
    else { bytes = new Uint8Array(got); let at = 0; for (const c of chunks) { bytes.set(c, at); at += c.length; } }
    chunks = null;
    return { status: response.status, statusText: response.statusText, headers: response.headers, bytes };
  } catch (e) {
    if (counted) { state.received -= got; state.expected = Math.max(0, state.expected - (length || got)); } // the retry counts again
    if (stalled) throw Object.assign(Error(`no data for ${downloadRetry.stallMs / 1000} s`), { stalled: true });
    throw e;
  } finally {
    clearTimeout(timer);
    if (counted) { state.active--; if (!state.active) state.idleAt = performance.now(); }
  }
}

async function download(url) {
  for (let attempt = 0; ; attempt++) {
    let failure;
    try {
      const result = await downloadOnce(url);
      if (!transientStatus(result.status) || attempt >= downloadRetry.delaysMs.length) return result;
      failure = `HTTP ${result.status}`;
    } catch (e) {
      if (leaving) throw e;
      failure = e?.message || String(e);
      if (attempt >= downloadRetry.delaysMs.length) {
        const path = new URL(url).pathname;
        throw Object.assign(Error(`Download failed: ${path} (${failure}) after ${attempt + 1} tries`), { network: true, cause: e });
      }
    }
    if (leaving) throw Error('page closed');
    console.warn(`Download retry ${attempt + 1}/${downloadRetry.delaysMs.length}: ${new URL(url).pathname} (${failure})`);
    await sleep(downloadRetry.delaysMs[attempt]);
  }
}

globalThis.fetch = function fetch(input, init) {
  const url = assetUrl(input, init);
  if (!url) return nativeFetch(input, init);
  let entry = shared.get(url);
  if (!entry) {
    entry = download(url); shared.set(url, entry);
    entry.then(() => setTimeout(() => shared.get(url) === entry && shared.delete(url), SHARE_MS), () => shared.delete(url));
  } else if (entry.prefetched) { // a prefetch's first taker: from here the usual SHARE_MS
    entry.prefetched = false; entry.then(() => setTimeout(() => shared.get(url) === entry && shared.delete(url), SHARE_MS), () => {});
  }
  return entry.then((r) => { // the Response constructor copies the bytes (each caller may transfer its buffer to a worker)
    const headers = new Headers(r.headers); headers.delete('content-encoding'); headers.delete('content-length');
    if (pv('loadCopies')) return sharedResponse(r, headers);
    return new Response(r.bytes, { status: r.status, statusText: r.statusText, headers });
  });
};

// pv loadCopies (docs/mobile.md "Load spikes"): a caller's Response whose body is read from the shared bytes. new Response(bytes) copied
// them, and arrayBuffer() / json() read that copy into another: with the shared copy and the download's chunk list plus its join, a load
// held 3-4 copies of every file while it read it (WebKit Malloc). Here arrayBuffer() / bytes() / blob() make the caller's one copy (it may
// transfer or edit it), text() / json() decode the shared bytes directly; `body` and clone() make a copy only when asked for. The values
// are the Response's: UTF-8 with the BOM dropped (TextDecoder), JSON.parse of that text. The Response itself has no body (null): a first
// version gave it a JS ReadableStream body and WebKit kept more memory with it (2 runs, docs/mobile.md "Load spikes").
const nativeBodyUsed = Object.getOwnPropertyDescriptor(Response.prototype, 'bodyUsed')?.get;
// The Response lets go of the shared bytes once its body is read: a caller whose scope keeps the Response (an async function's
// locals: the texture archive job keeps its whole archive's Response) must not keep a whole file with it.
function sharedResponse(r, headers) {
  let bytes = r.bytes, used = false, stream = null;
  const init = { status: r.status, statusText: r.statusText, headers };
  const res = new Response(null, init);
  const consumed = () => used || (stream ? !!nativeBodyUsed?.call(stream) : false);
  const take = (read, native) => {
    if (stream) return stream[native]();   // `body` was asked for: the stream's copy is the body from here
    if (used) return Promise.reject(new TypeError('Body has already been consumed.'));
    used = true; const b = bytes; bytes = null;
    try { return Promise.resolve(read(b)); } catch (e) { return Promise.reject(e); }
  };
  const text = (b) => new TextDecoder().decode(b);
  Object.defineProperties(res, {
    bodyUsed: { get: () => consumed(), configurable: true },
    body: {
      get: () => {
        if (!stream && used) return null;
        if (!stream) {
          stream = new Response(bytes.slice(), init);
          bytes = null;
        }
        return stream.body;
      },
      configurable: true
    },
    arrayBuffer: { value: () => take((b) => b.slice().buffer, 'arrayBuffer'), configurable: true, writable: true },
    bytes: { value: () => take((b) => b.slice(), 'bytes'), configurable: true, writable: true },
    text: { value: () => take(text, 'text'), configurable: true, writable: true },
    json: { value: () => take((b) => JSON.parse(text(b)), 'json'), configurable: true, writable: true },
    blob: {
      value: () => take((b) => new Blob([b], { type: headers.get('content-type') ?? '' }), 'blob'),
      configurable: true,
      writable: true
    },
    clone: {
      value: () => {
        if (stream) return stream.clone();
        if (used) throw new TypeError('Body has already been consumed.');
        return sharedResponse({ ...r, bytes }, headers);
      },
      configurable: true,
      writable: true
    }
  });
  return res;
}

// Prefetch (web/ctm-event.js, pv flyover): the download starts (shared like any request) and its one copy of the bytes waits for
// the first request of that URL (then SHARE_MS as usual), at most PREFETCH_KEEP_MS. A plain fetch() + arrayBuffer() would hold
// three copies per file while it reads (the shared bytes, the Response copy, the ArrayBuffer): the ~68 MB Snow Jam package
// peaked at +300-400 MB of renderer memory under the fly-over (Chrome 390x844, 4x; docs/presentation.md). Resolves ok (2xx).
export const PREFETCH_KEEP_MS = 30000;
// keepMs: how long an untaken copy is kept (riderPrefetch: the event's riders, taken when the course behind the load screen is in).
export function prefetchDownload(input, { keepMs = PREFETCH_KEEP_MS } = {}) {
  const url = assetUrl(input); if (!url) return Promise.resolve(false);
  let entry = shared.get(url);
  if (!entry) {
    entry = download(url); entry.prefetched = true; shared.set(url, entry);
    entry.then(() => setTimeout(() => shared.get(url) === entry && shared.delete(url), keepMs)?.unref?.(), () => shared.delete(url)); // (node tests: no wait)
  }
  return entry.then((r) => r.status >= 200 && r.status < 300, () => false);
}

// The bytes of a download in flight or kept (a prefetch) without taking it: null when there is none, or it failed. Shared: read only.
// (riderPrefetch: a rider package's world.json names the texture archives to prefetch.)
export function peekDownload(input) {
  const url = assetUrl(input), entry = url ? shared.get(url) : null;
  return entry ? entry.then((r) => (r.status >= 200 && r.status < 300 ? r.bytes : null), () => null) : Promise.resolve(null);
}

// ---- progress for the game's screens ------------------------------------------------------------------------------
// A batch runs from the first download after an idle moment to the last one finishing; after IDLE_RESET_MS idle the
// counters start over. fraction: received / expected (expected grows as new files start, so it can step back only when
// a new batch begins).
export function downloadProgress(now = performance.now()) {
  if (!state.active && state.since && now - state.idleAt > IDLE_RESET_MS) { state.received = state.expected = state.files = 0; state.since = 0; }
  const total = Math.max(state.expected, state.received);
  return {
    active: state.active > 0,
    received: state.received,
    // every body byte so far in the page (pv loadMeter)
    total: state.total,
    expected: total,
    files: state.files,
    fraction: total ? Math.min(1, state.received / total) : 1,
    busyMs: state.active && state.since ? now - state.since : 0
  };
}
