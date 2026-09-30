// Conquer the Mountain events in the streamed world (docs/ctm-events-in-world.md), stage 2 (pv eventWorldData) checks:
//  - every event course's race / reset paths (its initial.json) are exactly its streamed location's paths.json variant 0, the bank the
//    streamed core delivers (web/peak_world.inc deliver_paths), so an in-world event needs no path replacement;
//  - web/ctm-event-plan.js loadEventPlan reads each event package's own data (the race event document, the grid spawn, the riders where
//    the course has them, the GO LiveComp starts);
//  - the core's event_course_seed(code) selects the event start seeds of the 17 event courses and nothing else (CORE_JS=... for a scratch
//    core; skipped on a core without the export).
//   node test-ctm-event-world.mjs
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { loadEventPlan, goStarts } from './ctm-event-plan.js';

const A = new URL('public/assets/', import.meta.url);
if (!fs.existsSync(new URL('courses.json', A))) { console.log('ctm event world: skipped (no game data)'); process.exit(0); }
const read = (p) => JSON.parse(fs.readFileSync(new URL(p, A), 'utf8'));
const courses = read('courses.json').courses;
const EVENTS = ['ARA1', 'BRA2', 'BHP1', 'ABC1', 'ASS1', 'ABA1', 'CRA3', 'DRA4', 'DSS2', 'CBA2', 'CHP2', 'DBC2', 'ERA5', 'ESS3', 'EBA3', 'EHP3', 'EBC3'];
const peakOf = (code) => ['PEAK1', 'PEAK2', 'PEAK3'].find((p) => fs.existsSync(new URL(`${p}/${code}/paths.json`, A)));

// 1. paths
const key = (p) => JSON.stringify(['origin', 'low', 'high', 'segments'].map((k) => p[k]));
for (const code of EVENTS) {
  const entry = courses.find((c) => c.code === code); assert.ok(entry, code);
  const initial = JSON.parse(fs.readFileSync(new URL(entry.initial.replace('/assets/', ''), A), 'utf8'));
  const race = initial.original_race_event.original_race_event.paths, reset = initial.original_reset.paths;
  const bank = read(`${peakOf(code)}/${code}/paths.json`).variants['0'];
  assert.deepEqual(race.map(key), bank.race_paths.slice(0, race.length).map(key), `${code}: race paths = the streamed bank's, in order`);
  const resetKeys = new Set(bank.reset_paths.map(key));
  assert.ok(reset.every((p) => resetKeys.has(key(p))), `${code}: reset paths in the streamed bank`);
}
console.log(`paths: the 17 event courses' race / reset paths are their streamed location's bank`);

// 1b. stage 4 (pv eventInWorldAi): an event package's collision, terrain and rails are exactly its resident locations' (event_locations,
// in that order, same tracks), so a computer rider's context loaded with the event package holds the streamed world's resident set.
const MOUNTAIN = fs.existsSync(new URL('MOUNTAIN/peak.json', A)) ? new Map(read('MOUNTAIN/peak.json').locations.map((l) => [l.code, l])) : null;
if (MOUNTAIN) {
  const inst = (d) => d.instances.map((i) => JSON.stringify([i.track, i.rid, i.name, i.scale, i.matrix]));
  for (const code of EVENTS) {
    const world = read(`${code}/world_collision.json`), s = { inst: [], patches: [], rails: [] };
    for (const l of world.event_locations) {
      const e = MOUNTAIN.get(l.name); assert.equal(e?.track, l.track, `${code}: ${l.name} track`);
      const r = e.root.replace(/^\/assets\//, '');
      s.inst.push(...inst(read(r + 'world_collision.json'))); s.patches.push(...read(r + 'terrain.json').patches.map((p) => JSON.stringify([p.resource_id, p.coefficients])));
      s.rails.push(...read(r + 'rails.json').rails.map((x) => JSON.stringify([x.packed_id, x.runtime_flags, x.segments])));
    }
    assert.deepEqual(inst(world), s.inst, `${code}: collision instances == its resident locations'`);
    assert.deepEqual(read(`${code}/terrain.json`).patches.map((p) => JSON.stringify([p.resource_id, p.coefficients])), s.patches, `${code}: terrain patches`);
    assert.deepEqual(read(`${code}/rails.json`).rails.map((x) => JSON.stringify([x.packed_id, x.runtime_flags, x.segments])), s.rails, `${code}: rails`);
  }
  console.log('resident set: the 17 event packages\' collision, terrain and rails are their resident locations\', in order');
}

// 2. the event plan (files read from disk as the page fetches them)
const fetchJson = async (u) => { const f = new URL(u.replace(/^\/assets\//, ''), A); return fs.existsSync(f) ? JSON.parse(fs.readFileSync(f, 'utf8')) : null; };
const fetchText = async (u) => { const f = new URL(u.replace(/^\/assets\//, ''), A); return fs.existsSync(f) ? fs.readFileSync(f, 'utf8') : null; };
for (const code of EVENTS) {
  const entry = courses.find((c) => c.code === code);
  const plan = await loadEventPlan(entry, { fetchJson, fetchText });
  assert.ok(plan.ready, `${code}: race event document and grid spawn`);
  assert.equal(plan.code, code);
  assert.ok(plan.initialText.includes('original_race_event'), `${code}: the race event document`);
  if (fs.existsSync(new URL(`${code}/npc-riders.json`, A))) assert.ok(plan.npcRiders?.riders?.length > 0, `${code}: computer riders`);
}
const ara1 = await loadEventPlan(courses.find((c) => c.code === 'ARA1'), { fetchJson, fetchText });
assert.equal(ara1.go.instances.length, 12, 'Snow Jam: 12 GO LiveComp starts (the start-gate doors)');
assert.equal(ara1.go.tick, read('LIVECOMP/livecomp.json').go_tick);
assert.deepEqual(goStarts(null), { instances: [], programs: [] });
console.log('event plan: all 17 event packages read (Snow Jam: 12 GO starts at tick ' + ara1.go.tick + ')');

// 3. the core export
const coreUrl = process.env.CORE_JS ? (await import('node:url')).pathToFileURL(process.env.CORE_JS).href : new URL('runtime/core.js', import.meta.url).href;
const createCore = (await import(coreUrl)).default;
const core = await createCore();
if (!core._event_course_seed) { console.log('event_course_seed: skipped (core without the export)'); process.exit(0); }
const str = (s) => { const b = Buffer.from(s + '\0'), p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
const seed = (s) => { const p = str(s); try { return core._event_course_seed(p); } finally { core._free(p); } };
for (const code of EVENTS) assert.equal(seed(code), 1, `${code}: an event course`);
for (const code of ['PEAK1', 'MOUNTAIN', 'A', '']) assert.equal(seed(code), 0, `${code || '""'}: no event start`);
assert.equal(seed('ARA1'), 1); assert.ok(core._camera_event_seed_view(), 'ARA1: its start camera');
seed(''); assert.equal(core._camera_event_seed_view(), 0, '"": no start camera');
console.log('event_course_seed: the 17 event courses select their start seeds, other codes none');
console.log('ctm event world ok');
