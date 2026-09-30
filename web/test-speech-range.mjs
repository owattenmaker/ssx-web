// pv speechRange (docs/audio-logic.md 9.15): a speech line loads as its own byte range of its bank's .dat into a small LRU, fetched
// when the line is resolved or predicted, instead of the whole .dat kept for the session. Against the whole-file path, node, a Web
// Audio stand-in and the exported speech banks:
//  - the same lines start on the same ticks with the same PCM (instant ranges, and ranges that take 12 ms: inside the prediction's lead);
//  - only the played lines' bytes load, and the LRU stays under its budget;
//  - a server that answers a range with the whole file (200): that bank is kept whole, as before;
//  - a finished or stopped line lets go of its nodes.
//   node test-speech-range.mjs     (skips without the game data)
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { createSpeech, EV, CHAR_ID } from './audio-speech.js';

const root = new URL('public/assets/AUDIO/', import.meta.url);
if (!fs.existsSync(new URL('speech/Events.evt', root))) { console.log('speech range: skipped (no game data)'); process.exit(0); }
const read = (p) => fs.readFileSync(new URL(p, root));
const json = async (p) => JSON.parse(read(p));
const bytes = async (p) => new Uint8Array(read(p));
const lcg = (seed) => { let s = seed >>> 0; return () => { s = (Math.imul(s, 1664525) + 1013904223) >>> 0; return s / 4294967296; }; };

function fakeEngine() {
  const ctx = { currentTime: 0, sources: [], nodes: [],
    createBuffer(ch, len, rate) { const data = Array.from({ length: ch }, () => new Float32Array(len)); return { numberOfChannels: ch, length: len, sampleRate: rate, duration: len / rate, data, copyToChannel(x, c) { data[c].set(x); } }; },
    createGain() { const g = { gain: { value: 1, setValueAtTime() {}, linearRampToValueAtTime() {}, cancelScheduledValues() {} }, connect() {}, disconnect() { g.cut = true; } }; ctx.nodes.push(g); return g; },
    createBufferSource() { const s = { connect() {}, disconnect() { s.cut = true; }, start(t) { s.at = t; }, stop() { s.stopped = true; } }; ctx.sources.push(s); ctx.nodes.push(s); return s; } };
  return { context: ctx, live: true, bus: () => ({}), busGain: () => 1, duck() {}, release() {} };
}
const sum = (b) => { let x = 0; for (const d of b.data) for (let i = 0; i < d.length; i += 97) x += d[i]; return +x.toFixed(6); };
// A scripted stretch of speech: DJ, PA and rider events, some posted while a line plays (the voice busy: predicted ahead).
const SCRIPT = [[5, (s) => s.radioBigIntro(0, 1)], [20, (s) => s.eventIntro(0)], [200, (s) => s.riderPosition({ id: CHAR_ID.zoe }, 0)],
  [230, (s) => s.whooh({ id: CHAR_ID.mac, human: true, upright: true })], [520, (s) => s.dj(EV.HUB_WEATHER, [1])], [540, (s) => s.dj(EV.HUB_MTN_HISTORY, [1])],
  [900, (s) => s.bigAir({ id: CHAR_ID.zoe, human: true, upright: true }, 1)], [1100, (s) => s.riderPosition({ id: CHAR_ID.mac }, 2)], [1400, (s) => s.dj(EV.HUB_CHAR_STORIES, [1])]];
async function run({ range = null, latencyMs = 0, frames = 1800, lineCacheBytes = 2 << 20 } = {}) {
  const engine = fakeEngine(), ctx = engine.context, log = [], got = [];
  const r = range && (async (p, a, b) => { got.push([p, a, b]); if (latencyMs) await new Promise((q) => setTimeout(q, latencyMs)); return range(p, a, b); });
  const loaded = new Set(); const whole = async (p) => { loaded.add(p); return bytes(p); };
  const s = createSpeech({ engine, json, bytes: whole, range: r, lineCacheBytes, random: lcg(7) });
  await s.init();
  let n = 0;
  for (let tick = 0; tick < frames; tick++) {
    ctx.currentTime = tick / 60;
    for (const [at, f] of SCRIPT) if (at === tick) f(s);
    s.update();
    while (n < ctx.sources.length) { const src = ctx.sources[n++]; log.push([tick, src.buffer.length, sum(src.buffer), +src.at.toFixed(4)]); }
    await new Promise((q) => (latencyMs ? setTimeout(q, 1000 / 60) : setImmediate(q)));
  }
  s.stop();
  return { log, got, loaded: [...loaded].filter((p) => p.endsWith('.dat')), stats: s.debug().lineCache, ctx };
}
const files = (p, a, b) => ({ start: a, bytes: new Uint8Array(read(p).subarray(a, b)) });

const base = await run();
assert.ok(base.log.length >= 6, `the script plays lines (${base.log.length})`);
const inst = await run({ range: files });
assert.deepEqual(inst.log, base.log, 'instant ranges: the same lines on the same ticks, the same PCM');
assert.deepEqual(inst.loaded, [], 'no whole .dat loaded');
const wholeMB = base.loaded.reduce((a, p) => a + read(p).length, 0) / 1048576, rangeKB = inst.got.reduce((a, [, x, y]) => a + y - x, 0) / 1024;
assert.ok(inst.stats.predicted > 0, 'lines predicted at a post');
const slow = await run({ range: files, latencyMs: 12 });
assert.deepEqual(slow.log.map((x) => x.slice(0, 3)), base.log.map((x) => x.slice(0, 3)), '12 ms ranges: still the same ticks (fetched at the post / prediction)');
// a small budget: least recently used lines go; bytes stay under it (plus the one line being added)
const small = await run({ range: files, lineCacheBytes: 256 * 1024 });
assert.deepEqual(small.log, base.log, 'a small cache: the same lines');
assert.ok(small.stats.kb <= 256 + 900 && small.stats.evicted > 0, JSON.stringify(small.stats));
// a server without ranges (200, the whole file): that bank is kept whole and serves its other lines
const w = await run({ range: (p) => ({ start: 0, bytes: new Uint8Array(read(p)) }) });
assert.deepEqual(w.log, base.log, '200: the same lines');
assert.ok(w.stats.wholeBanks.length > 0 && w.stats.kb === 0, JSON.stringify(w.stats));
// nodes: every started line let go of its source once stopped / ended
for (const src of inst.ctx.sources) src.onended?.();
assert.ok(inst.ctx.nodes.every((x) => x.cut), 'finished lines let go of their nodes');
console.log(`speech range OK: ${base.log.length} lines on the same ticks; whole .dat ${wholeMB.toFixed(1)} MB -> ${rangeKB.toFixed(0)} KB of ranges (${inst.got.length} requests), LRU ${inst.stats.kb} KB`);
