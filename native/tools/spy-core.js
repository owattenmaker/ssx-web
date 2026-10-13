// The native gate (docs/core-migration.md "Native core"): a course core whose every export call is logged for native/core's replayer: the arguments (non-finite numbers
// as strings), the bytes of each argument that is a live malloc block, the shared / visual RNG words the harness writes between
// calls, and before each tick's pad call a probe hash of the human's state. REAL_CORE=<core.js> SPY_LOG=<out.jsonl>.
import fs from 'node:fs';
import { pathToFileURL } from 'node:url';
const create = (await import(pathToFileURL(process.env.REAL_CORE).href)).default;
const out = fs.openSync(process.env.SPY_LOG, 'w');
let buffered = [];
const flush = () => { if (buffered.length) { fs.writeSync(out, buffered.join('\n') + '\n'); buffered = []; } };
const emit = (o) => { buffered.push(JSON.stringify(o)); if (buffered.length > 2000) flush(); };
const num = (v) => (typeof v === 'number' ? (Number.isFinite(v) ? v : String(v)) : v === undefined ? 'undefined' : typeof v === 'boolean' ? (v ? 1 : 0) : v);
const fnv = (u32) => { let h = 0x811c9dc5; for (let i = 0; i < u32.length; i++) h = Math.imul(h ^ u32[i], 16777619) >>> 0; return h; };
export default async function (opts) {
  const m = await create(opts);
  const blocks = new Map();
  const raw = {};
  for (const [k, f] of Object.entries(m)) if (k[0] === '_' && k[1] !== '_' && typeof f === 'function') raw[k] = f;
  let rngPtr = 0, visPtr = 0, lcgPtr = 0, rngLast = null, visLast = null, lcgLast = null;
  const words = (p, n) => Array.from(new Uint32Array(m.HEAPU8.buffer, p, n));
  const watch = () => {
    if (rngPtr) { const w = words(rngPtr, 6); if (rngLast && w.some((x, k) => x !== rngLast[k])) emit({ w: 'rng', v: w }); }
    if (visPtr) { const w = words(visPtr, 6); if (visLast && w.some((x, k) => x !== visLast[k])) emit({ w: 'visual', v: w }); }
    if (lcgPtr) { const w = words(lcgPtr, 1); if (lcgLast && w[0] !== lcgLast[0]) emit({ w: 'lcg', v: w }); }
  };
  const settle = () => { if (rngPtr) rngLast = words(rngPtr, 6); if (visPtr) visLast = words(visPtr, 6); if (lcgPtr) lcgLast = words(lcgPtr, 1); };
  const probe = () => {
    const p = [raw._rider_state(), raw._rider_world_state(), raw._pose_physical(), raw._animation_rng_words(), raw._visual_rng_words()];
    const h = [fnv(new Uint32Array(m.HEAPU8.buffer, p[0], 16)), fnv(new Uint32Array(m.HEAPU8.buffer, p[1], 20)), fnv(new Uint32Array(m.HEAPU8.buffer, p[2], 12)),
      fnv(new Uint32Array(m.HEAPU8.buffer, p[3], 6)), fnv(new Uint32Array(m.HEAPU8.buffer, p[4], 6))];
    emit({ p: h });
  };
  for (const [k, f] of Object.entries(raw)) {
    m[k] = (...a) => {
      if (k === '_malloc') { const p = f(...a); blocks.set(p, a[0]); emit({ n: k, a: a.map(num), r: p }); return p; }
      if (k === '_free') { blocks.delete(a[0]); emit({ n: k, a: a.map(num) }); return f(...a); }
      watch();
      if (k === '_tick_input' || k === '_pad_tick') probe();
      const b = {};
      a.forEach((v, i) => { if (typeof v === 'number' && blocks.has(v)) b[i] = Buffer.from(m.HEAPU8.subarray(v, v + blocks.get(v))).toString('base64'); });
      const r = f(...a);
      if (k === '_animation_rng_words') rngPtr = r;
      if (k === '_visual_rng_words') visPtr = r;
      if (k === '_visual_lcg_word') lcgPtr = r;
      emit({ n: k, a: a.map(num), ...(Object.keys(b).length ? { b } : {}), r: typeof r === 'number' ? num(r) : r === undefined ? null : String(r) });
      settle();
      return r;
    };
  }
  process.on('exit', () => { watch(); probe(); flush(); fs.closeSync(out); });
  return m;
}
