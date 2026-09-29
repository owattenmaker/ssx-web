// The whole-mountain world MOUNTAIN (docs/peak3.md section 6; tools/export_mountain_world.py, tools/export_peak_stage.py --mountain):
//  0. the manifest: every location of the three peaks (roots in the per-peak packages), all 22 residency rows, the streaming
//     rows, and the request rows decoded from the stage seed: every connector's Unload / Load volume requests a row that holds
//     it, and the peak boundaries ERA5_C -> C (19) and DRA4_A -> A (17);
//  1. the environment slices hold exactly the peaks' patches, texture ids global (track << 12);
//  2. streaming across both peak boundaries with the core's own streaming state machine (web/peak_world.inc): Gravitude's row,
//     ERA5_C's Unload / Load -> Yellow station's row (ERA5 evicted, ERA5_C kept, course 19), and Intimidator's row, DRA4_A's
//     -> Green station's row (course 17); before and after, height and rail probes on the locations answer exactly like the
//     per-course event packages (ERA5, CRA3, DRA4, ARA1) and an evicted location answers nothing;
//  3. environment_add / environment_drop round trip; 4. the All Peak Race's route streamed row by row: core memory.
// CORE=path/to/core.js overrides the runtime (scratch builds).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { locationBatches, environmentBatches } from './peak-world-batches.js';
import { PEAK_RUNS } from './peak-run.js';
import { peakRunWorld } from './free-ride.js';

const createCore = (await import(process.env.CORE || './runtime/core.js')).default;
const root = new URL('public/assets/', import.meta.url);
const read = (p) => fs.readFileSync(new URL(p, root));
const json = (p) => JSON.parse(read(p));
if (!fs.existsSync(new URL('MOUNTAIN/peak.json', root))) { console.log('whole-mountain world not exported (tools/export_mountain_world.py); skipped'); process.exit(0); }
const manifest = json('MOUNTAIN/peak.json');
const pkg = (l) => l.root.replace(/^\/assets\//, '');
const byCode = new Map(manifest.locations.map((l) => [l.code, l]));
const put = (core, text) => { const b = Buffer.from(text + '\0'); const p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
const call = (core, fn, text, ...rest) => { const p = put(core, text); try { return fn(p, ...rest); } catch (e) { throw new Error(core.getExceptionMessage ? core.getExceptionMessage(e).join(': ') : String(e)); } finally { core._free(p); } };

// 0. Manifest and request rows.
assert.equal(manifest.name, 'MOUNTAIN'); assert.equal(manifest.environment_slices, 'ENV/');
const rows = Object.fromEntries(manifest.residency.map((r) => [r.course, r]));
assert.deepEqual(Object.keys(rows).map(Number), Array.from({ length: 22 }, (_, i) => i), 'a residency row for every course 0..21');
for (const peak of [1, 2, 3]) for (const l of json(`PEAK${peak}/peak.json`).locations) assert.ok(byCode.has(l.code), `MOUNTAIN lacks ${l.code}`);
assert.equal(manifest.locations.length, 43);
assert.deepEqual(rows[4].locations, ['ERA5', 'E_ERA5', 'ERA5_C']); assert.deepEqual(rows[19].locations, ['C', 'C_CRA3', 'C_CHP2', 'C_CBA2', 'ERA5_C']);
assert.deepEqual(rows[3].locations, ['DRA4', 'D_DRA4', 'DRA4_A']); assert.deepEqual(rows[17].locations, ['A', 'A_ARA1', 'A_ASS1', 'A_ABA1', 'DRA4_A', 'ABC1_A']);
assert.equal(byCode.get('ERA5_C').root, '/assets/PEAK3/ERA5_C/'); assert.equal(byCode.get('DRA4_A').root, '/assets/PEAK2/DRA4_A/');
for (const l of manifest.locations) for (const f of ['world.json', 'terrain.json', 'world_collision.json', 'rails.json'])
  assert.ok(fs.existsSync(new URL(`${pkg(l)}${f}`, root)), `${l.code}: ${f}`);
const ids = new Set(manifest.streaming.map((s) => s.code));
for (const r of manifest.residency) for (const c of [...r.locations, r.sky, 'TRANSP']) assert.ok(ids.has(c), `no streaming row for ${c}`);
const requests = manifest.requests; assert.ok(Array.isArray(requests), 'request rows (tools/export_peak_stage.py --mountain first)');
const connectors = manifest.locations.filter((l) => l.kind === 2).map((l) => l.code);
for (const code of connectors) for (const action of [0, 2]) {
  const rq = requests.filter((r) => r.location === code && r.action === action);
  assert.equal(rq.length, 1, `${code}: one ${action ? 'Load' : 'Unload'} request`);
  assert.ok(rows[rq[0].course]?.locations.includes(code), `${code} requests row ${rq[0].course}, which does not hold it`);
}
const boundary = (code) => requests.filter((r) => r.location === code && r.action !== 5).map((r) => [r.instance, r.action, r.course]);
assert.deepEqual(boundary('ERA5_C'), [['mdl_ERA5_C_Load_0', 2, 19], ['mdl_ERA5_C_Unload_0', 0, 19]].sort((a, b) => a[0] < b[0] ? -1 : 1));
assert.deepEqual(boundary('DRA4_A'), [['mdl_DRA4_A_Load_0', 2, 17], ['mdl_DRA4_A_Unload_0', 0, 17]].sort((a, b) => a[0] < b[0] ? -1 : 1));
assert.equal(requests.filter((r) => r.action === 3).length, 5); assert.equal(requests.filter((r) => r.action === 4).length, 5); // the five stations' booths / lodge doors
for (const mode of [7, 8, 11]) { assert.equal(peakRunWorld(mode), 'MOUNTAIN'); for (const c of PEAK_RUNS[mode].route) assert.ok(rows[c], `route row ${c}`); }
for (const mode of [6, 9]) assert.equal(peakRunWorld(mode), 'PEAK1'); assert.equal(peakRunWorld(10), 'PEAK2');
console.log(`manifest: ${manifest.locations.length} locations, ${manifest.residency.length} residency rows, ${manifest.streaming.length} streaming rows; ${connectors.length} connectors each request a row holding them; ERA5_C -> 19, DRA4_A -> 17`);

// 1. Environment slices = the peaks' patches.
{ let patches = 0, expected = 0;
  for (const l of manifest.locations) {
    const e = json(`MOUNTAIN/ENV/${l.code}.json`); patches += e.patches.length;
    assert.ok(e.textures.every((t) => t.id >> 12 === l.track), `${l.code}: global texture ids`);
    expected += json(`PEAK${l.peak}/environment.json`).patches.filter((p) => (p.resource & 255) === l.track).length;
  }
  assert.equal(patches, expected); console.log(`environment slices: ${patches} patches of 43 locations`); }

// 2. Streaming across the peak boundaries.
async function mountainCore(codes) {
  const core = await createCore(); let hash = 0, first = true;
  for (const code of codes) {
    const l = byCode.get(code), terrain = json(pkg(l) + 'terrain.json'), world = json(pkg(l) + 'world_collision.json'), rails = json(pkg(l) + 'rails.json');
    if (!hash) hash = put(core, terrain.source_sha256);
    if (first) {
      world.location = 'MOUNTAIN';
      call(core, core._init_terrain, JSON.stringify(terrain)); call(core, core._init_world_collision, JSON.stringify(world), hash);
      call(core, core._init_body_terrain, JSON.stringify(terrain)); call(core, core._init_rails, JSON.stringify(rails), hash);
      core._peak_world_begin(); core._peak_world_reserve(65536, 4096); first = false;
    } else {
      for (const b of locationBatches(terrain, world, rails, 'MOUNTAIN')) {
        core._peak_world_append(1);
        if (b.kind === 'world') call(core, core._init_world_collision, b.text, hash);
        else if (b.kind === 'terrain') { call(core, core._init_terrain, b.text); call(core, core._init_body_terrain, b.text); }
        else call(core, core._init_rails, b.text, hash);
        core._peak_world_append(0);
      }
      core._peak_world_commit();
    }
  }
  call(core, core._peak_world_manifest, JSON.stringify({ streaming: manifest.streaming, residency: manifest.residency }));
  for (const code of codes) core._peak_world_data_ready(byCode.get(code).track, 1);
  return core;
}
// A row held alone in a per-peak world (PEAK<N>/<LOC> packages, SDB track order), every location resident.
async function peakRowCore(world, codes) {
  const core = await createCore(); let hash = 0, first = true;
  for (const code of codes.slice().sort((x, y) => byCode.get(x).track - byCode.get(y).track)) {
    const r = `${world}/${code}/`, terrain = json(r + 'terrain.json'), w = json(r + 'world_collision.json'), rails = json(r + 'rails.json');
    if (!hash) hash = put(core, terrain.source_sha256);
    if (first) { w.location = world; call(core, core._init_terrain, JSON.stringify(terrain)); call(core, core._init_world_collision, JSON.stringify(w), hash); call(core, core._init_body_terrain, JSON.stringify(terrain)); call(core, core._init_rails, JSON.stringify(rails), hash); core._peak_world_begin(); core._peak_world_reserve(65536, 4096); first = false; }
    else { for (const b of locationBatches(terrain, w, rails, world)) { core._peak_world_append(1); if (b.kind === 'world') call(core, core._init_world_collision, b.text, hash); else if (b.kind === 'terrain') { call(core, core._init_terrain, b.text); call(core, core._init_body_terrain, b.text); } else call(core, core._init_rails, b.text, hash); core._peak_world_append(0); } core._peak_world_commit(); }
    core._peak_world_set_resident(byCode.get(code).track, put(core, code), 1);
  }
  return core;
}
async function eventCore(code) {
  const core = await createCore(); const terrain = json(`${code}/terrain.json`); const hash = put(core, terrain.source_sha256);
  call(core, core._init_terrain, JSON.stringify(terrain)); call(core, core._init_world_collision, read(`${code}/world_collision.json`).toString(), hash);
  call(core, core._init_body_terrain, JSON.stringify(terrain)); call(core, core._init_rails, read(`${code}/rails.json`).toString(), hash);
  return core;
}
function samples(code, n) {
  const w = json(pkg(byCode.get(code)) + 'world.json'), [[x0, y0, z0], [x1, y1, z1]] = w.bounds; let s = 12345; const rnd = () => ((s = (s * 1103515245 + 12345) >>> 0) / 4294967296);
  return Array.from({ length: n }, () => [x0 + (x1 - x0) * rnd(), y1 + 5, z0 + (z1 - z0) * rnd()]);
}
let probes = 0;
function same(a, b, code, label) {
  for (const [x, y, z] of samples(code, 150)) {
    const ha = a._height_at(x, y, z), hb = b._height_at(x, y, z);
    assert.equal(Object.is(ha, hb) || ha === hb, true, `${label} height ${code} (${x},${y},${z}): package ${ha} mountain ${hb}`);
    const ra = Array.from(new Float32Array(a.HEAPF32.buffer, a._rail_query(x * 100, -z * 100, (ha > -1e8 ? ha : y) * 100 + 50), 14));
    const rb = Array.from(new Float32Array(b.HEAPF32.buffer, b._rail_query(x * 100, -z * 100, (hb > -1e8 ? hb : y) * 100 + 50), 14));
    assert.deepEqual(rb.slice(1), ra.slice(1), `${label} rail query ${code}`); probes++;
  }
}
const stateOf = (core, code) => { const p = core._peak_world_rows() >> 2, H = new Int32Array(core.HEAPU8.buffer), n = H[p]; for (let k = 0; k < n; k++) if (H[p + 1 + 4 * k] === byCode.get(code).id) return H[p + 3 + 4 * k]; return -1; };
function settle(core) { for (let k = 0; k < 2000; k++) { core._peak_world_tick(); } }
async function crossing({ from, to, connector, before, after }) {
  const codes = [...new Set([...rows[from].locations, ...rows[to].locations])].sort((x, y) => byCode.get(x).track - byCode.get(y).track);
  const m = await mountainCore(codes);
  m._peak_world_start(from); assert.equal(m._peak_world_course(), from);
  for (const [event, locs] of before) { const e = await eventCore(event); for (const code of locs) same(e, m, code, `row ${from}`); }
  m._peak_world_request(to, 0); settle(m); // the connector's Unload volume (22CEA8): the other row's locations leave, the new row reads
  assert.equal(m._peak_world_course(), to, 'the Unload request sets 0x535C08 (22DF50)');
  for (const code of rows[to].locations) assert.equal(stateOf(m, code), code === connector ? 2 : 1, `${code} after the Unload`);
  for (const code of rows[from].locations) if (code !== connector) assert.equal(stateOf(m, code), 0, `${code} evicted after the Unload`);
  m._peak_world_request(to, 1); m._peak_world_tick(); // the Load volume (22D088): the row activates
  for (const code of rows[to].locations) assert.equal(stateOf(m, code), 2, `${code} active after the Load`);
  // the new row answers like that row held alone in its own peak's world (tools/export_peak_world.py, gated against the event
  // packages by test-peak-world / test-peak2-world): eviction and append order change nothing
  const ref = await peakRowCore(after, rows[to].locations); for (const code of rows[to].locations) same(ref, m, code, `row ${to}`);
  const gone = rows[from].locations.find((c) => c !== connector), e = await eventCore(before[0][0]);
  const hidden = samples(gone, 400).filter(([x, y, z]) => e._height_at(x, y, z) > -1e8 && m._height_at(x, y, z) !== e._height_at(x, y, z));
  assert.ok(hidden.length > 0, `${gone} still answers after its eviction`);
  console.log(`crossing ${connector}: row ${from} -> ${to}, ${rows[from].locations.filter((c) => c !== connector).join('/')} evicted, ${rows[to].locations.join('/')} active, course ${to}`);
  return m;
}
await crossing({ from: 4, to: 19, connector: 'ERA5_C', before: [['ERA5', ['ERA5', 'E_ERA5', 'ERA5_C']]], after: 'PEAK2' });
const dra4 = await crossing({ from: 3, to: 17, connector: 'DRA4_A', before: [['DRA4', ['DRA4', 'D_DRA4', 'DRA4_A']]], after: 'PEAK1' });
console.log(`probes: ${probes} height + rail queries across both boundaries: Gravitude's / Intimidator's rows = their event packages before, Yellow / Green station's rows = the PEAK2 / PEAK1 worlds' after`);
// 2b. The game tick 1298C8 at a peak run's crossing (web/peak_world.inc, docs/peak3.md "Past the crash contacts"): the Unload's
// 22DF50 pass requests world state 11 and the background state 10, whose rider reload (1297C8 via 128A10) restarts the tick
// in a Conquer the Mountain time challenge (event type 5): 13 passes into a course (PS2 apr-full C_CRA3 / D_DRA4 / A_ARA1), none
// in a points challenge (6) or free ride (4).
if (dra4._game_tick_restart) {
  const restartAfter = (kind, path) => {
    dra4._peak_world_event_kind(kind); dra4._set_stage_collect_state(path, 0, 0); dra4._game_tick_restart(5000);
    dra4._peak_world_request(dra4._peak_world_course() === 17 ? 3 : 17, 0);
    for (let k = 0; k < 40; k++) { dra4._peak_world_tick(); if (dra4._game_tick() !== 5000) return [k, dra4._game_tick()]; }
    return null;
  };
  assert.deepEqual(restartAfter(5, 0), [13, 0], 'a time challenge into Intimidator: 1298C8 = 0 on the Unload record + 13 (the 22DF50 pass is record + 0)');
  settle(dra4); assert.deepEqual(restartAfter(5, 0), [12, 0], 'into Green Station: the Unload record + 12');
  settle(dra4); assert.equal(restartAfter(6, 0), null, 'All Peak Jam (event type 6): no restart');
  settle(dra4); assert.equal(restartAfter(4, 0), null, 'free ride: no restart');
  settle(dra4); assert.equal(restartAfter(5, 1), null, 'not Conquer the Mountain: no restart');
  dra4._peak_world_event_kind(4);
  console.log('game tick 1298C8: restarted 13 / 12 records after a time challenge Unload (course / station), not in the Jam or free ride');
}

// 3. Environment slices in the core.
if (dra4._environment_add) {
  const env = json('MOUNTAIN/environment.json'); assert.equal(env.streamed, true); assert.equal(env.patches.length, 0);
  const b = Buffer.from(JSON.stringify(env) + '\0'), p = dra4._malloc(b.length), q = dra4._malloc(1); dra4.HEAPU8.set(b, p); dra4._init_environment(p, q, 0); dra4._free(p);
  let added = 0;
  for (const code of rows[17].locations) {
    const doc = json(`MOUNTAIN/ENV/${code}.json`), bytes = new Uint8Array(read(`MOUNTAIN/ENV/${code}.bin`));
    for (const batch of environmentBatches(doc, bytes)) { const t = Buffer.from(batch.text + '\0'), pt = dra4._malloc(t.length), pb = dra4._malloc(Math.max(1, batch.bytes.length)); dra4.HEAPU8.set(t, pt); dra4.HEAPU8.set(batch.bytes, pb); added += dra4._environment_add(pt, pb, batch.bytes.length); dra4._free(pt); dra4._free(pb); }
    assert.equal(added > 0 || doc.patches.length === 0, true);
  }
  const expected = rows[17].locations.reduce((a, c) => a + json(`MOUNTAIN/ENV/${c}.json`).patches.length, 0);
  assert.equal(added, expected); let dropped = 0; for (const code of rows[17].locations) dropped += dra4._environment_drop(byCode.get(code).track);
  assert.equal(dropped, expected); console.log(`environment slices: row 17 added (${added} patches) and dropped`);
} else console.log('environment slices: core without environment_add (rebuild web/runtime)');

// 4. Core memory: the All Peak Race's rows streamed along the route (the browser holds the rows around the rider; this is the
//    whole route, the upper bound without any release).
{ const codes = []; for (const c of PEAK_RUNS[8].route) for (const l of rows[c].locations) if (!codes.includes(l)) codes.push(l);
  const t0 = performance.now(), m = await mountainCore(codes), info = new Int32Array(m.HEAPU8.buffer, m._world_collision_info(), 11);
  console.log(`All Peak Race route: ${codes.length} locations, ${info[0]} instances, core memory ${(m.HEAPU8.buffer.byteLength / 1048576).toFixed(0)} MB, ${((performance.now() - t0) / 1000).toFixed(1)} s`);
  assert.ok(m.HEAPU8.buffer.byteLength <= 256 * 1048576, 'the route must stay far below the whole mountain');
  // Every collision instance of the route is supported (world_collision_info [5]): the five stations' os609 departure models
  // take their free-ride seed (tools/generate_event_seed.py streamed_scripted), so no crash / reset query can meet an
  // unsupported one (the core used to stop on "Reset placement intersects unsupported world resources").
  if (m._world_collision_unsupported) {
    const U = new Uint32Array(m.HEAPU8.buffer, m._world_collision_unsupported(), 1 + 2 * 256), n = U[0], left = [];
    for (let k = 0; k < n; k++) left.push({ resource: U[1 + 2 * k], flags: U[2 + 2 * k] });
    const os609 = []; for (const code of codes) { const w = json(pkg(byCode.get(code)) + 'world_collision.json');
      for (const i of w.instances) if (/os609_full_version_depart/.test(i.name ?? '')) os609.push((i.rid << 8) | i.track); }
    assert.equal(os609.length, 5, 'the five stations on the route hold an os609 departure model each');
    assert.ok(os609.every((r) => !left.some((x) => x.resource === r)), 'the os609 departure models are supported');
    // what stays unsupported are the flag-0 entity instances that only the stage programs and the world's free-ride instance
    // flags route (ARA1 endmode colliders, ERA5 finish fences): a query skips them unless a program gives them an entity
    assert.ok(left.every((x) => x.flags === 0), `only flag-0 stage entity instances stay unsupported: ${JSON.stringify(left.filter((x) => x.flags))}`);
    console.log(`All Peak Race route: the 5 station os609 departure models supported; ${n} flag-0 stage entity instances left to the stage programs`);
  } else console.log('unsupported instances: core without world_collision_unsupported (rebuild web/runtime)'); }

// 5. The PS2's own streaming along whole runs (tools/ps2_autopilot.py closed-loop captures, local/ps2-capture/runs/allpeak;
//    `ps2_autopilot.py analyze` -> RUN.analysis.json: the start rows, then every row / course change record by record): the
//    core's streaming state machine (web/peak_world.inc) driven only by the PS2's trigger ticks (each connector's Unload = the
//    record where 0x535C08 changes, each Load = the record where the new row turns active) must reproduce the current course
//    on every tick and every location row. The All Peak Race is exact on every tick (its measured reads are the manifest's);
//    the Peak 2 Race (Ruthless -> D -> DRA4 -> DRA4_A -> A -> ARA1 -> B -> Metro-City, the run that fell into the void at
//    DRA4_A with the Peak 2 world alone) must hold the same locations on every tick: a read may end a few ticks apart (a
//    read's length depends on the texture sub-chunks in flight), nothing else.
function ps2Streaming(file, label, exactReads) {
  const analysis = new URL(`../local/ps2-capture/runs/allpeak/${file}`, import.meta.url);
  if (!fs.existsSync(analysis)) { console.log(`PS2 streaming (${label}): capture analysis not present (local/ps2-capture/runs/allpeak); skipped`); return null; }
  return (async () => {
    const a = JSON.parse(fs.readFileSync(analysis)), core = await createCore();
    call(core, core._peak_world_manifest, JSON.stringify({ streaming: manifest.streaming, residency: manifest.residency }));
    const byCodeRow = new Map(manifest.streaming.map((s) => [s.code, s.id]));
    const start = a.events.find((e) => e.kind === 'start'); core._peak_world_start(start.course);
    // the PS2 row states record by record (the start record's rows, then every change)
    const ps2 = new Map(Object.entries(start.rows)); const changes = new Map(); for (const e of a.events) if (e.kind === 'rows') changes.set(e.record, e.changes);
    const unloads = new Map(), loads = new Map();
    for (const e of a.events) if (e.kind === 'course') unloads.set(e.record, e.value);
    for (const e of a.events) if (e.kind === 'rows' && e.changes.some(([, f, t]) => f === 1 && t === 2)) { const c = [...unloads.entries()].filter(([r]) => r <= e.record).pop(); if (c && !e.changes.some(([n]) => /SKY$/.test(n))) loads.set(e.record, c[1]); }
    const rowState = (code) => { const p = core._peak_world_rows() >> 2, H = new Int32Array(core.HEAPU8.buffer), n = H[p], id = byCodeRow.get(code); for (let k = 0; k < n; k++) if (H[p + 1 + 4 * k] === id) return H[p + 3 + 4 * k]; return 0; };
    // states: 0 absent, 3 / 4 queued, 6 / 8 reading, 1 read, 2 resident, 7 leaving
    const held = (x) => x !== 0 && x !== 7, reading = (x) => x === 3 || x === 4 || x === 6 || x === 8 || x === 1;
    let checked = 0, first = null, courseMiss = 0, readSkew = 0; const codes = manifest.locations.map((l) => l.code), boundary = new Map();
    for (let i = 1; i < a.records; i++) {
      if (unloads.has(i)) core._peak_world_request(unloads.get(i), 0);
      if (loads.has(i)) core._peak_world_request(loads.get(i), 1);
      core._peak_world_tick();
      for (const [n, , t] of changes.get(i) || []) ps2.set(n, t);
      for (const code of codes) {
        const w = rowState(code), p = ps2.get(code) ?? 0; checked++;
        if (w === p) continue;
        if (!exactReads && held(w) === held(p) && (reading(w) || reading(p))) { readSkew++; continue; }
        if (!first) first = { record: i, code, web: w, ps2: p };
      }
      const pc = [...unloads.entries()].filter(([r]) => r <= i).pop()?.[1] ?? start.course; if (core._peak_world_course() !== pc) courseMiss++;
      if (unloads.has(i) && (unloads.get(i) === 19 || unloads.get(i) === 17)) boundary.set(unloads.get(i), codes.filter((c) => held(rowState(c))).sort().join(' '));
    }
    console.log(`PS2 streaming (${label}): ${a.records} ticks, ${unloads.size} Unload / ${loads.size} Load triggers; ${checked} row states compared, first difference ${first ? JSON.stringify(first) : 'none'}${exactReads ? '' : `, ${readSkew} read-end skews`}; course mismatches ${courseMiss}`);
    for (const [c, set] of boundary) console.log(`  after the ${c === 19 ? 'ERA5_C' : 'DRA4_A'} Unload (row ${c}): ${set}`);
    assert.equal(courseMiss, 0, `${label}: current course 0x535C08 differs from the PS2`);
    assert.equal(first, null, `${label}: a location row differs from the PS2`);
    return a;
  })();
}
{ await ps2Streaming('apr-full.analysis.json', 'All Peak Race', true);
  const p2 = await ps2Streaming('p2r-full.analysis.json', 'Peak 2 Race', false);
  if (p2) assert.ok(p2.events.some((e) => e.kind === 'course' && e.value === 1), 'the Peak 2 Race capture reaches Metro-City (course 1)'); }
