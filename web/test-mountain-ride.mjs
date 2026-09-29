// The Conquer the Mountain free ride across a peak boundary in the whole-mountain world (pv mountainRide; docs/ctm-parity.md
// "The whole mountain"), against a PS2 capture of the same ride (local/ps2-capture/ctm-parity/mountain/runs/fr-dra4a: a free-ride
// state on Intimidator (Zoe, the last lodge poked to D so the arrival shows), ridden down by tools/ps2_autopilot.py through DRA4_A
// into Green Base Station (resumed once from its kept state at record 18000: fr-dra4a-full); scratch ctm/mtn/fr_analyze.py ->
// fr-dra4a-full.analysis.json):
//  1. the core's streaming (web/peak_world.inc) in free ride (event kind 4, mode 12), driven only by the PS2's trigger records
//     (each connector's Unload = the record where 0x535C08 changes, each Load = the record where the new row turns active), holds
//     the same locations as the PS2 on every record and the same current course;
//  2. the arrival (22DF50 -> world state 11 -> 10 at A): the PS2 writes the last lodge (+0x27C = 17, 146E10 at 0x2356FC) and the
//     visited bit (+0xACC bit 17), which web/career-ui.js courseChanged does at the course change;
//  3. the rider's position is continuous across the crossing (no step larger than its speed allows: one world, no switch).
// CORE=path/to/core.js overrides the runtime.
import fs from 'node:fs';
import assert from 'node:assert/strict';

const analysisUrl = new URL('../local/ps2-capture/ctm-parity/mountain/runs/fr-dra4a-full.analysis.json', import.meta.url);
const root = new URL('public/assets/', import.meta.url);
if (!fs.existsSync(analysisUrl) || !fs.existsSync(new URL('MOUNTAIN/peak.json', root))) { console.log('test-mountain-ride: capture analysis or MOUNTAIN world not present; skipped'); process.exit(0); }
const createCore = (await import(process.env.CORE || './runtime/core.js')).default;
const manifest = JSON.parse(fs.readFileSync(new URL('MOUNTAIN/peak.json', root)));
const a = JSON.parse(fs.readFileSync(analysisUrl));
const put = (core, text) => { const b = Buffer.from(text + '\0'); const p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };

// ---- 1. streaming ----
const core = await createCore();
{ const p = put(core, JSON.stringify({ streaming: manifest.streaming, residency: manifest.residency })); try { core._peak_world_manifest(p); } finally { core._free(p); } }
core._peak_world_event_kind(4); core._peak_world_game_mode(12);
const start = a.events.find((e) => e.kind === 'start'); core._peak_world_start(start.course);
const byCodeRow = new Map(manifest.streaming.map((s) => [s.code, s.id]));
const ps2 = new Map(Object.entries(start.rows)), changes = new Map();
for (const e of a.events) if (e.kind === 'rows') changes.set(e.record, e.changes);
const unloads = new Map(), loads = new Map();
for (const e of a.events) if (e.kind === 'course') unloads.set(e.record, e.value);
for (const e of a.events) if (e.kind === 'rows' && e.changes.some(([, f, t]) => f === 1 && t === 2)) { const c = [...unloads.entries()].filter(([r]) => r <= e.record).pop(); if (c && !e.changes.some(([n]) => /SKY$/.test(n))) loads.set(e.record, c[1]); }
assert.ok([...unloads.values()].includes(17), 'the capture crosses into Green Base Station (course 17)');
const rowState = (code) => { const p = core._peak_world_rows() >> 2, H = new Int32Array(core.HEAPU8.buffer), n = H[p], id = byCodeRow.get(code); for (let k = 0; k < n; k++) if (H[p + 1 + 4 * k] === id) return H[p + 3 + 4 * k]; return 0; };
const held = (x) => x !== 0 && x !== 7, reading = (x) => x === 3 || x === 4 || x === 6 || x === 8 || x === 1;
const codes = manifest.locations.map((l) => l.code);
let checked = 0, first = null, courseMiss = 0, readSkew = 0, heldAtCross = null;
for (let i = 1; i < a.records; i++) {
  if (unloads.has(i)) core._peak_world_request(unloads.get(i), 0);
  if (loads.has(i)) core._peak_world_request(loads.get(i), 1);
  core._peak_world_tick();
  for (const [n, , t] of changes.get(i) || []) ps2.set(n, t);
  for (const code of codes) {
    const w = rowState(code), p = ps2.get(code) ?? 0; checked++;
    if (w === p) continue;
    if (held(w) === held(p) && (reading(w) || reading(p))) { readSkew++; continue; }   // a read may end a few ticks apart (texture sub-chunks in flight)
    if (!first) first = { record: i, code, web: w, ps2: p };
  }
  const pc = [...unloads.entries()].filter(([r]) => r <= i).pop()?.[1] ?? start.course; if (core._peak_world_course() !== pc) courseMiss++;
  if (unloads.get(i) === 17) heldAtCross = codes.filter((c) => held(rowState(c))).sort().join(' ');
}
console.log(`PS2 free ride Intimidator -> Green: ${a.records} records, ${unloads.size} Unload / ${loads.size} Load triggers; ${checked} row states compared, first difference ${first ? JSON.stringify(first) : 'none'}, ${readSkew} read-end skews`);
console.log(`  held after the DRA4_A Unload (row 17): ${heldAtCross}`);
assert.equal(courseMiss, 0, 'the current course 0x535C08 follows the PS2');
assert.equal(first, null, 'every location row holds what the PS2 holds');

// ---- 2. the arrival: the career words the PS2 writes at the crossing ----
const cross = a.events.find((e) => e.kind === 'course' && e.value === 17);
const [flagAt, lodgeAt, visitedAt] = a.career_watch.map((h) => parseInt(h, 16) - 0x4AAAC8);   // Zoe's block: +0, +0x27C, +0xACC
assert.deepEqual([flagAt, lodgeAt, visitedAt], [0, 0x27C, 0xACC]);
const careerAt = (record) => { let v = start.career; for (const e of a.events) { if (e.record > record) break; if (e.kind === 'career') v = e.value; } return v; };
const before = careerAt(cross.record - 1), after = careerAt(a.records - 1);
assert.equal(before[1], 20, 'the derived state starts with the last lodge at D');
assert.equal(after[1], 17, 'the last lodge is Green Base Station after the crossing (146E10)');
assert.ok(after[2] & (1 << 17), 'A is visited (+0xACC bit 17)'); assert.ok(!(before[2] & (1 << 17)), 'A was not visited before');
const lodgeEvent = a.events.find((e) => e.kind === 'career' && e.value[1] === 17), visitEvent = a.events.find((e) => e.kind === 'career' && (e.value[2] & (1 << 17)));
const loadRecord = [...loads.entries()].find(([, c]) => c === 17)?.[0];
const ws = (from, to) => a.events.find((e) => e.kind === 'ws' && e.was === from && e.value === to)?.record;
// pv crossingArrival: world state 11 from the Unload (the course change), 10 at the Load (the row active: the last lodge), 4 the next
// record (the visited bit); web/career-ui.js courseChanged / crossingArrived
assert.equal(ws(4, 11), cross.record, 'world state 11 at the Unload');
assert.equal(ws(11, 10), loadRecord, 'world state 10 at the Load trigger (the row turns active)');
assert.equal(lodgeEvent.record, loadRecord, 'the last lodge at the Load (WS10 enter, 0x2356FC)');
assert.equal(ws(10, 4), loadRecord + 1); assert.equal(visitEvent.record, loadRecord + 1, 'the visited bit at WS4, the next record');
console.log(`  arrival: course 17 (WS 4 -> 11) at record ${cross.record}; the Load at ${loadRecord}: WS 11 -> 10 and the last lodge 20 -> 17; WS 10 -> 4 and the visited bit at ${visitEvent.record}`);

// ---- 3. position continuity ----
const step = a.steps.find((s) => s.course === 17);
assert.ok(step, 'steps around the crossing');
assert.ok(step.max_step_cm <= step.max_speed_step_cm * 1.5 + 50, `no jump at the crossing: ${JSON.stringify(step)}`);
console.log(`  position: largest step ${step.max_step_cm} cm per record at the crossing (speed allows ${step.max_speed_step_cm})`);
console.log('test-mountain-ride: ok');
