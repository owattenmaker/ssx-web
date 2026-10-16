// pv archiveRanges (docs/asset-formats.md "Texture archives: range reads"): a rider gear archive (WARDROBE/<ID>/gear.tex,
// gear-xbox.tex) read as HTTP byte ranges, its header and index first and then only the entries a package asks for, instead of the
// whole file. An outfit draws 1-2 gear entries (16-96 KB) out of a 15-38 MB Xbox HD archive (2.6-4.1 MB brotli on the wire).
//
// A range answer counts only when it is exactly the bytes asked for: status 206, a Content-Range with the asked start and end (or the
// end of the file), no content coding, and a body of that length. Anything else (a 200 with the whole file, a range over a
// compressed copy: the Cloudflare edge served ranges of its cached brotli body, 2026-10-15) throws a RangeUnsupported error and
// the caller reads the whole archive as before (web/texture-archive.js).
//
// The request URL carries ?identity=1: deploy/edge-worker.js then fetches the file under its own edge cache key with
// Accept-Encoding: identity, and web/server/mp-server.mjs serves the file itself (never a precompressed copy) for it. The browser
// already sends Accept-Encoding: identity with a Range header (Fetch: "Range" requests). The Vite dev server ignores the query.
// HTTP cache, measured over three visits on one profile: Chrome keeps every range (sparse entries, If-Range: each one a 304 on
// the next visit); WebKit keeps at most one 206 per URL, and not every time (the first range was a 304 on some visits; one URL
// per range, ?at=<start>, kept none), so a repeat visit in Safari reads its ranges again (~130-200 KB per outfit archive).
const MAGIC = 'SSXTEX01';
// the first read: the 12-byte head and the index (52-99 KB in the Xbox HD gear archives); a longer index is read in a second range
export const HEAD_BYTES = 131072;
// a transient failure of one range is retried this many times before the whole-archive fallback (web/downloads.js retries that)
const RANGE_TRIES = 2;

// broken: a range answer of this page's server failed the checks once; later archives go straight to the whole-file read
export const rangeState = { broken: false };

export class RangeUnsupported extends Error {
  constructor(message) {
    super(message);
    this.name = 'RangeUnsupported';
    this.rangeUnsupported = true;
  }
}
const abortError = () => new DOMException('The archive was aborted.', 'AbortError');

export function rangeUrl(url) {
  return url + (url.includes('?') ? '&' : '?') + 'identity=1';
}

// bytes [start, end) of `url`: Uint8Array of exactly those bytes (shorter only at the end of the file), and the file size
export async function readRange(url, start, end, { signal = null, fetcher = globalThis.fetch } = {}) {
  if (signal?.aborted) throw abortError();
  const r = await fetcher(rangeUrl(url), { headers: { range: `bytes=${start}-${end - 1}` }, signal });
  if (r.status === 404) throw Error(`${url}: 404`);
  if (r.status !== 206) {
    r.body?.cancel?.().catch?.(() => {});
    throw new RangeUnsupported(`${url}: HTTP ${r.status} to a range`);
  }
  const coding = (r.headers.get('content-encoding') || 'identity').toLowerCase();
  const m = /^bytes (\d+)-(\d+)\/(\d+|\*)$/.exec(r.headers.get('content-range') || '');
  const total = m && m[3] !== '*' ? +m[3] : null;
  const last = m ? +m[2] : -1;
  const decoded = +r.headers.get('x-decoded-length') || 0;
  const bad =
    coding !== 'identity' ||
    !m ||
    +m[1] !== start ||
    (last !== end - 1 && !(total != null && last === total - 1 && last < end - 1)) ||
    (decoded && total != null && decoded !== total);
  if (bad) {
    r.body?.cancel?.().catch?.(() => {});
    throw new RangeUnsupported(`${url}: range answer ${r.headers.get('content-range')} (${coding})`);
  }
  const bytes = new Uint8Array(await r.arrayBuffer());
  if (bytes.length !== last - start + 1) throw new RangeUnsupported(`${url}: ${bytes.length} bytes for ${start}-${last}`);
  return { bytes, total };
}

// readRange with retries of a network failure; RangeUnsupported, 404 and aborts are not retried
async function readRangeRetry(url, start, end, opts) {
  for (let attempt = 1; ; attempt++) {
    try {
      return await readRange(url, start, end, opts);
    } catch (e) {
      if (e?.rangeUnsupported || e?.name === 'AbortError' || /: 404$/.test(e?.message ?? '') || attempt >= RANGE_TRIES) throw e;
    }
  }
}

// The ranged archive: {url, index, start, entries, ranged: true, size, held, entryBlob(id)}. held: the bytes it keeps (head and
// entries read), reported through onBytes(held) for the rider archive budget (web/texture-archive.js trimRiderArchives).
export async function rangedArchive(url, { signal = null, fetcher = globalThis.fetch, onBytes = null } = {}) {
  const opts = { signal, fetcher };
  const first = await readRangeRetry(url, 0, HEAD_BYTES, opts);
  let head = first.bytes;
  if (head.length < 12 || String.fromCharCode(...head.subarray(0, 8)) !== MAGIC) throw Error(`${url}: not a texture archive`);
  const n = new DataView(head.buffer, head.byteOffset, 12).getUint32(8, true);
  if (12 + n > head.length) {
    const rest = await readRangeRetry(url, head.length, 12 + n, opts);
    const all = new Uint8Array(12 + n);
    all.set(head.subarray(0, Math.min(head.length, 12 + n)));
    all.set(rest.bytes, head.length);
    head = all;
  }
  const index = JSON.parse(new TextDecoder().decode(head.subarray(12, 12 + n)));
  const start = 12 + n + ((16 - ((12 + n) % 16)) % 16);
  const entries = new Map(index.entries.map((e) => [e.id, e]));
  const parts = new Map();
  const archive = {
    url,
    index,
    start,
    entries,
    ranged: true,
    size: first.total,
    held: 12 + n,
    // one entry's bytes as a Blob (the type the whole-archive slice has); read once, kept with the archive
    entryBlob(id) {
      const e = entries.get(id);
      if (!e) return Promise.reject(Error(`${url}: no texture ${id}`));
      if (!parts.has(id)) {
        const job = readRangeRetry(url, start + e.offset, start + e.offset + e.size, { fetcher }).then(({ bytes }) => {
          archive.held += bytes.length;
          onBytes?.(archive.held);
          return new Blob([bytes], { type: e.codec ? 'application/octet-stream' : 'image/png' });
        });
        job.catch(() => {
          if (parts.get(id) === job) parts.delete(id);
        });
        parts.set(id, job);
      }
      return parts.get(id);
    }
  };
  onBytes?.(archive.held);
  return archive;
}
