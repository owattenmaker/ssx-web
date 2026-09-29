// First-load progress for the title screen (docs/first-load.md): one fraction from the first byte to "Press START
// button". No imports: it is bundled into the inline boot script (web/boot-screen.js) as well as tested in node.
//
// Work items, each with a weight and a fraction done:
// - files: every file the title needs (web/boot-files.json + the code bundle), weighted by its bytes on the wire (the
//   gzip copy when the host serves one) plus REQUEST_BYTES for its round trip. Bytes arrive through web/downloads.js
//   (fetch) as they stream; images and the code bundle only report when they finish (Resource Timing), so while such a
//   file is in flight it creeps towards 90% at the measured bandwidth and never past it.
// - steps: work that downloads nothing (the code bundle starting, the core compile, the GPU, the world build, the
//   computer riders, the last setup), weighted by their typical time at BYTES_PER_MS. A step reports its own fraction
//   when it can (world build, riders) and otherwise creeps towards 90% of its typical time until it is done.
// The display never goes backwards, and nothing sits at 99%: the last step (the title becoming ready) has its own weight.
export const BYTES_PER_MS = 6000;        // compute steps: 1 ms of work weighs as much as 6 KB on the wire (~48 Mbit/s)
export const REQUEST_BYTES = 64 * 1024;  // + each file's round trip (the loaders ask for many small files one after another)
export const CREEP_MAX = 0.9;            // an item in flight without progress reports creeps up to 90% of itself
const MIN_BANDWIDTH = 60;                // bytes/ms floor for the creep estimate (~0.5 Mbit/s)

// Path key: /assets/... pathname without the query (the edge's ?g= / a version query).
export function bootKey(url, base = 'http://x') {
  try { return new URL(url, base).pathname; } catch { return String(url).split('?')[0]; }
}

// manifest: { files: [[path, wire, size, phase]], steps: [[id, ms, phase]], phases: [[id, label]], course?: [from, to] }
//   files may name a course directory (/assets/ARA1/...); course = ['ARA1', 'BRA2'] maps those paths to the course
//   actually loading (a ?course= deep link), whose files have the same names.
export function createBootProgress(manifest = {}, { now = () => Date.now(), start = now() } = {}) {
  const files = new Map(), steps = new Map(), phaseOrder = (manifest.phases || []).map(([id]) => id);
  const labels = new Map(manifest.phases || []);
  const course = manifest.course && manifest.course[0] !== manifest.course[1] ? manifest.course : null;
  const mapPath = (p) => (course && p.startsWith(`/assets/${course[0]}/`) ? `/assets/${course[1]}/` + p.slice(course[0].length + 9) : p);
  for (const [path, wire, size, phase] of manifest.files || []) files.set(mapPath(path), { w: REQUEST_BYTES + (wire || size || 0), size: size || wire || 0, phase, got: 0, done: false, start: null });
  for (const [id, ms, phase] of manifest.steps || []) steps.set(id, { w: Math.max(1, ms * BYTES_PER_MS), ms, phase, f: 0, start: null, done: false, reported: false });
  let shown = 0, doneBytes = 0, firstAt = null, lastPhase = null, phaseAt = 0, finished = false;
  const bandwidth = (t) => { const ms = Math.max(1, t - (firstAt ?? start)); return Math.max(MIN_BANDWIDTH, doneBytes / ms); };

  const api = {
    files, steps,
    // A file (path or URL) started: creeps from now until its bytes or its completion arrive.
    begin(url, t = now()) { const f = files.get(bootKey(url)); if (f && f.start == null) f.start = t; },
    // Streamed bytes of a file (decoded bytes, as the page receives them); total = the decoded size when known.
    bytes(url, got, total = 0, t = now()) {
      const f = files.get(bootKey(url)); if (!f || f.done) return;
      if (f.start == null) f.start = t;
      if (total > 0 && !f.size) f.size = total;
      f.got = Math.max(f.got, got);
    },
    // A file finished (Resource Timing or the end of its stream). wire = bytes that came over the network (0 from cache).
    done(url, wire = 0, t = now()) {
      const f = files.get(bootKey(url)); if (!f || f.done) return false;
      if (f.start == null) f.start = t;
      f.done = true; f.got = f.size; doneBytes += wire > 0 ? wire : 0; if (firstAt == null) firstAt = f.start;
      return true;
    },
    // A step's own fraction (0..1); 1 = done. A step with no reports creeps from its start (begin: true).
    step(id, fraction = 1, t = now(), { begin = false } = {}) {
      const s = steps.get(id); if (!s) return;
      if (s.start == null) s.start = t;
      if (!begin) { s.reported = true; s.f = Math.max(s.f, Math.min(1, fraction)); if (s.f >= 1) s.done = true; }
    },
    finish() { finished = true; for (const s of steps.values()) { s.done = true; s.f = 1; } for (const f of files.values()) f.done = true; },
    // Each item's current fraction.
    itemFraction(item, t, isFile) {
      if (item.done) return 1;
      if (item.start == null) return 0;
      if (isFile) {
        if (item.size > 0 && item.got > 0) return Math.min(0.999, item.got / item.size);
        const expect = item.w / bandwidth(t); return CREEP_MAX * Math.min(1, (t - item.start) / Math.max(1, expect));
      }
      return item.reported ? Math.min(0.999, item.f) : CREEP_MAX * Math.min(1, (t - item.start) / Math.max(1, item.ms));
    },
    // The true fraction of all work (0..1) and the phase that is under way.
    target(t = now()) {
      if (finished) return { fraction: 1, phase: null };
      let total = 0, got = 0; const left = new Map();
      // the phase shown: the first (in load order) with work under way; files not yet asked for do not hold it
      for (const f of files.values()) { const x = api.itemFraction(f, t, true); total += f.w; got += f.w * x; if (!f.done && f.start != null) left.set(f.phase, (left.get(f.phase) || 0) + f.w * (1 - x)); }
      for (const s of steps.values()) { const x = api.itemFraction(s, t, false); total += s.w; got += s.w * x; if (!s.done && s.start != null) left.set(s.phase, (left.get(s.phase) || 0) + s.w * (1 - x)); }
      // ... and it only moves on: work of an earlier phase finishing late (a picture of the menus) does not take it back
      const from = Math.max(0, phaseOrder.indexOf(lastPhase));
      const phase = phaseOrder.find((p, i) => i >= from && (left.get(p) || 0) > 0) ?? lastPhase;
      return { fraction: total ? got / total : 0, phase };
    },
    // What the title shows: eases towards the target (never backwards), with the phase label held for 400 ms at least.
    value(t = now(), dt = 16) {
      const { fraction, phase } = api.target(t);
      const k = 1 - Math.exp(-Math.max(0, dt) / 180);
      shown = finished ? 1 : Math.max(shown, shown + (fraction - shown) * k);
      if (phase !== lastPhase && (lastPhase == null || t - phaseAt >= 400)) { lastPhase = phase; phaseAt = t; }
      return { fraction: shown, target: fraction, phase: lastPhase, label: labels.get(lastPhase) || '' };
    },
  };
  return api;
}
