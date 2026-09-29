// pv musicStream (docs/audio-logic.md "Music streaming"): a song's .mus streams bar by bar with range requests
// (web/pathfinder.js createMusStream / loadSong fetchRange / streamSongStart) instead of downloading whole.
//  - a bar not in yet decodes to null (the player asks again), then to the same PCM as the whole file's;
//  - streamSongStart loads the opening bars only, and its random replays the dry run: the player commits those bars;
//  - read-ahead along the graph (depth 2, 1 while the game's downloads run);
//  - a server without ranges (200): the whole file serves every bar;
//  - a simulated minute of a song loads a fraction of its .mus.
//   node test-music-stream.mjs     (skips without the game data)
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { loadSong, createMusStream, streamSongStart, createPathfinderCore } from './pathfinder.js';

const root = new URL('public/assets/AUDIO/', import.meta.url);
if (!fs.existsSync(new URL('music/Go.json', root))) { console.log('music stream: skipped (no game data)'); process.exit(0); }
const fetchJson = async (p) => JSON.parse(fs.readFileSync(new URL(p, root), 'utf8'));
const fetchBytes = async (p) => new Uint8Array(fs.readFileSync(new URL(p, root)));
const files = new Map(), file = (p) => { if (!files.has(p)) files.set(p, fs.readFileSync(new URL(p, root))); return files.get(p); };
function ranges({ whole = false, delay = 0 } = {}) {
  const log = [];
  const fetchRange = async (p, a, b, priority) => { log.push({ p, a, b, priority }); if (delay) await new Promise((r) => setTimeout(r, delay));
    const f = file(p); return whole ? { start: 0, bytes: new Uint8Array(f) } : { start: a, bytes: new Uint8Array(f.subarray(a, b)) }; };
  return { fetchRange, log };
}
const same = (a, b) => a.length === b.length && a.channels === b.channels && a.data.every((c, i) => c.length === b.data[i].length && c.every((v, k) => v === b.data[i][k]));

for (const [id, event] of [['Go', 0], ['charsel', 0], ['pktrans', 1]]) {
  const r = ranges(), whole = await loadSong(id, { fetchJson, fetchBytes });
  const song = await loadSong(`${id}`, { fetchJson, fetchBytes, fetchRange: r.fetchRange });
  assert.ok(song.mus.stream && song.mus.bytesLoaded === 0, `${id}: nothing loaded before a bar is asked for`);
  const size = file(`music/${song.json.tracks.find((t) => t.kind === 'stream').file}`).length;
  // the opening bars, then the player's first commits are all in
  const random = await streamSongStart(song, event, { intensity: 127 });
  assert.equal(typeof random, 'function');
  const opening = song.mus.bytesLoaded;
  assert.ok(opening > 0 && opening < size / 10, `${id}: the opening loads a small part (${opening} of ${size})`);
  const core = createPathfinderCore(song, { random, intensity: 127 }); core.sendEvent(event, 0); core.advanceTo(1500);
  const first = core.state.voices[0].queue.filter((s) => s.start < 1500);
  assert.ok(first.length > 0);
  for (const seg of first) {
    const smp = song.graph.samples[seg.sample - 1];
    assert.ok(song.mus.has(smp.offset, smp.size), `${id}: the committed opening bar ${seg.sample} is in`);
    const a = song.decodeSegment(seg), b = whole.decodeSegment(seg);
    assert.ok(a && same(a, b), `${id}: bar ${seg.sample} decodes as from the whole file`);
  }
  // read-ahead: the bars the last opening bar leads to are in (asked for when it was decoded)
  await new Promise((r) => setTimeout(r, 10));
  const lastNode = first.at(-1).node, succ = new Set(), stack = song.graph.nodes[lastNode].branches.map((b) => b.node), seen = new Set();
  while (stack.length) { const n = stack.pop(); if (n < 0 || n >= song.graph.nodes.length || seen.has(n)) continue; seen.add(n); if (song.graph.nodes[n].sample > 0) succ.add(n); else stack.push(...song.graph.nodes[n].branches.map((b) => b.node)); }
  for (const n of succ) { const smp = song.graph.samples[song.graph.nodes[n].sample - 1]; if (smp.kind === 'stream') assert.ok(song.mus.has(smp.offset, smp.size), `${id}: read-ahead: bar ${song.graph.nodes[n].sample} (after node ${lastNode}) is in`); }
  // a bar never asked for: null, asked for now; in on the next pump
  const far = song.graph.nodes.find((n) => n.sample > 0 && !song.mus.has(song.graph.samples[n.sample - 1].offset, song.graph.samples[n.sample - 1].size) && song.graph.samples[n.sample - 1].kind === 'stream');
  if (far) {
    const seg = { sample: far.sample, node: far.index ?? song.graph.nodes.indexOf(far), kind: 'stream', start: 0, end: 1 };
    assert.equal(song.decodeSegment(seg), null, `${id}: a bar not in yet decodes to null`);
    await new Promise((r) => setTimeout(r, 20));
    const a = song.decodeSegment(seg), b = whole.decodeSegment(seg);
    assert.ok(a && same(a, b), `${id}: then it decodes as from the whole file`);
  }
  // a simulated minute: every committed bar asked for as the player would, a pump every 50 ms
  const core2 = createPathfinderCore(song, { intensity: 127 }); core2.sendEvent(event, 0);
  for (let t = 0; t < 60000; t += 50) { core2.advanceTo(t + 1000); for (const s of core2.state.voices[0].queue) if (s.end > t) song.decodeSegment(s, { aheadMs: s.start - t }); await null; }
  await new Promise((r) => setTimeout(r, 20));
  console.log(`${id}: opening ${(opening / 1024).toFixed(0)} KB, a minute ${(song.mus.bytesLoaded / 1e6).toFixed(1)} MB of ${(size / 1e6).toFixed(1)} MB; ${r.log.length} range requests`);
  assert.ok(song.mus.bytesLoaded < size, `${id}: a minute loads part of the file`);
}
// busy: read-ahead depth 1 (fewer bars asked for than with depth 2)
{
  const count = async (busy) => { const r = ranges(); const s = createMusStream((a, b, p) => r.fetchRange('music/go.mus', a, b, p), { busy: () => busy });
    const song = await loadSong('Go', { fetchJson, fetchBytes, fetchRange: r.fetchRange, busy: () => busy }); void s; await streamSongStart(song, 0, { intensity: 0 }); await new Promise((q) => setTimeout(q, 10)); return r.log.length; };
  const idle = await count(false), busy = await count(true);
  assert.ok(busy <= idle, `busy reads ahead less (${busy} vs ${idle} requests)`);
}
// a server without ranges: the 200's whole body serves every bar
{
  const r = ranges({ whole: true }), song = await loadSong('Buffet', { fetchJson, fetchBytes, fetchRange: r.fetchRange });
  await streamSongStart(song, 0, { intensity: 127 });
  const s = song.graph.samples.filter((x) => x.kind === 'stream');
  assert.ok(s.every((x) => song.mus.has(x.offset, x.size)), 'a 200 serves every bar');
}
console.log('music stream OK: bars load when asked for and decode as from the whole file; the opening first; read-ahead; 200 fallback');
