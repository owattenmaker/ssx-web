// Streamed Peak 1 world for the PS2 capture comparer (web/compare-ps2-capture.mjs --course PEAK1; docs/peak-mountain.md).
// Every Peak 1 location is appended to the core (as web/peak-world.js does in the browser); the streaming rows are then
// driven from the capture itself: each record's 0x442168 rows (watch) replace the browser's loader timing (a disc read's
// duration depends on the texture sub-chunks in flight), so the comparison isolates the rider physics across the load
// boundary. Path banks: installed in the world update of the tick whose NEXT record shows the row leaving 6/8 (12A340 at
// the read completion), kept when the bank's location is evicted (12A490 -> 26ADA0 is jr ra) until the next delivery replaces it.
import { loadAvalanches } from './avalanche-load.js';
import fs from 'node:fs';
import { locationBatches } from './peak-world-batches.js';

export function peakWatches(captureManifest) {
  const out = {}; let off = 0;
  for (const w of captureManifest.layout?.watches || []) { out[Number(w.address)] = { offset: captureManifest.layout.watch_offset + off, length: w.length }; off += w.length; }
  return out;
}

export async function loadPeakWorld({ core, root, captureManifest, dv, RECORD, records, arrival = false, world: W = 'PEAK1' }) {   // W: the streamed world (PEAK1 / PEAK3, docs/peak3.md)
  const read = (p) => fs.readFileSync(new URL(p, root));
  const json = (p) => JSON.parse(read(p));
  const put = (text) => { const b = Buffer.from(text + '\0'); const p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
  const call = (fn, text, ...rest) => { const p = put(text); try { return fn(p, ...rest); } finally { core._free(p); } };
  // The whole mountain (docs/peak3.md section 6): MOUNTAIN<x> names the same world (MOUNTAIN/peak.json, locations in their per-peak
  // folders) with its own glide seed (MOUNTAIN2: the Peak 2 Race at Ruthless; web/generate-controllers.py).
  const M = /^MOUNTAIN/.test(W) ? 'MOUNTAIN' : W;
  const manifest = json(`${M}/peak.json`);
  const dirOf = (l) => (l.root ? l.root.replace(/^\/assets\//, '') : `${W}/${l.code}/`);
  let hash = 0, first = true;
  for (const l of manifest.locations) {
    const r = dirOf(l), terrain = json(r + 'terrain.json'), world = json(r + 'world_collision.json'), rails = json(r + 'rails.json');
    if (!hash) hash = put(terrain.source_sha256);
    if (first) {
      world.location = W; // selects the world's glide seed and stage tables
      call(core._init_terrain, JSON.stringify(terrain)); call(core._init_world_collision, JSON.stringify(world), hash);
      call(core._init_body_terrain, JSON.stringify(terrain)); call(core._init_rails, JSON.stringify(rails), hash);
      core._peak_world_begin(); core._peak_world_reserve(65536, 4096); first = false;
    } else {
      for (const b of locationBatches(terrain, world, rails, W)) {
        core._peak_world_append(1);
        if (b.kind === 'world') call(core._init_world_collision, b.text, hash);
        else if (b.kind === 'terrain') { call(core._init_terrain, b.text); call(core._init_body_terrain, b.text); }
        else call(core._init_rails, b.text, hash);
        core._peak_world_append(0);
      }
      core._peak_world_commit();
    }
  }
  // Weather (web/weather.inc): every location record's Weather section, as web/peak-world.js feeds them; the painters pick the
  // record of the region track gp+0x770, seeded from record 0's contact patch (rider+0x430 & 0xFF; its tick-1 rider pass set it).
  // WEATHER=0 leaves it out (the pre-2026-09-26 comparer: the seed location's tree for the rider painter only).
  if (process.env.WEATHER !== '0' && core._weather_location) {
    for (const l of manifest.locations) { const f = new URL(`${dirOf(l)}weather.json`, root); if (fs.existsSync(f)) { const p = put(fs.readFileSync(f, 'utf8')); try { core._weather_location(l.track, p); } finally { core._free(p); } } }
    const patch0 = dv.getInt32(32 + 0x430 - 0x100, true); if (patch0 !== -1 && core._weather_region_seed) core._weather_region_seed(patch0 & 0xFF);
  }
  // The world's flag manager (web/stage_world.inc init_streamed_flags, as web/peak-set-pieces.js): grid builds in the section pass and
  // the 1 s wind on the visual stream, the mode of the current course. Its state at a mid-run baseline is the fresh-load wind
  // (0, 0.5, 0.25, 0) unless the capture's visual-state seed replaces it.
  { const f = new URL(`${M}/SETPIECES/flags.json`, root); if (process.env.FLAGS !== '0' && core._init_streamed_flags && fs.existsSync(f)) { const p = put(fs.readFileSync(f, 'utf8')); try { core._init_streamed_flags(p); } finally { core._free(p); } } }
  call(core._peak_world_manifest, JSON.stringify({ streaming: manifest.streaming, residency: manifest.residency }));
  for (const l of manifest.locations) if (l.id < 22 && fs.existsSync(new URL(`${dirOf(l)}paths.json`, root))) core._peak_world_paths(l.id, put(read(`${dirOf(l)}paths.json`).toString()));
  core._peak_world_manual(1);
  // Section activation 0x101B60 of the streamed world (tools/export_peak_sections.py); the stage/section state of the
  // seed savestate (tools/export_peak_seed.py seed-state.json) replaces the fresh-load state at the first tick.
  if (core._init_sections && fs.existsSync(new URL(`${M}/SECTIONS/sections.json`, root))) call(core._init_sections, read(`${M}/SECTIONS/sections.json`).toString());
  // (--peak-arrival captures start elsewhere: the location's fresh stage/section state)
  // The stage world of the streamed world, as the browser runs it (web/peak-set-pieces.js: init_stage_world with the world's merged
  // SETPIECES exports): LiveComp players built by trigger / timer programs move their instance's collision (EBC3's falling path,
  // allpeak/apj-start 2661), particles, MeshAnim pieces. PEAK_STAGE=0 leaves it out (the comparer before 2026-09-26).
  if (process.env.PEAK_STAGE !== '0' && core._init_stage_world && fs.existsSync(new URL(`${M}/SETPIECES/particles.json`, root))) {
    const sp = (n) => { const f = new URL(`${M}/SETPIECES/${n}`, root); return fs.existsSync(f) ? fs.readFileSync(f, 'utf8') : ''; };
    const ptrs = [sp('particles.json'), sp('livecomp.json'), sp('stage-world.json')].map(put);
    try { if (!(core._init_stage_world(...ptrs) > 0)) throw new Error(`${M}: init_stage_world failed`); } finally { ptrs.forEach((p) => core._free(p)); }
  }
  // Avalanches (web/avalanche_gameplay.inc): the world's locations with a recorded one, as web/peak-set-pieces.js loads them.
  if (process.env.PEAK_STAGE !== '0') await loadAvalanches(core, manifest.locations.map((l) => ({ code: l.code, root: dirOf(l) })), async (p) => { const f = new URL(p, root); return fs.existsSync(f) ? fs.readFileSync(f, 'utf8') : null; });
  const seedState = !arrival && fs.existsSync(new URL(`${W}/seed-state.json`, root)) ? json(`${W}/seed-state.json`) : null;
  const PW = peakWatches(captureManifest), rowsW = PW[0x442168], gameW = PW[0x535C08];
  if (!rowsW || !gameW) throw new Error('PEAK1 capture needs the 0x442168 and 0x535C08 watches');
  const rowState = (i, id) => dv.getInt32(i * RECORD + rowsW.offset + 16 * id + 8, true);
  const course = (i) => dv.getInt8(i * RECORD + gameW.offset), kind = (i) => dv.getInt8(i * RECORD + gameW.offset + 8), mode = (i) => dv.getInt8(i * RECORD + gameW.offset + 10);
  const ids = manifest.streaming.map((r) => r.id);
  const resetPath = (i) => dv.getUint32(i * RECORD + 32 + 0xAB8 - 0x100, true); // rider +0xAB8 (the reset path 112180 attaches)
  const banks = new Set(manifest.locations.filter((l) => l.id < 22 && fs.existsSync(new URL(`${dirOf(l)}paths.json`, root))).map((l) => l.id)), early = new Set();
  core._peak_world_event_kind(kind(0)); core._peak_world_game_mode(mode(0));
  // Stage builtin 108 (the stations' peak-race / jam fences follow the game mode byte) as the page runs it: pv stationFences
  // (web/free-ride.js); STATION_FENCES=1 / 0 forces it.
  core._peak_world_builtin108?.(process.env.STATION_FENCES ? (process.env.STATION_FENCES === '1' ? 1 : 0) : 1);
  core._stage_object_route?.(process.env.FINISH_FENCES ? (process.env.FINISH_FENCES === '1' ? 1 : 0) : 1); // pv finishFences (FINISH_FENCES=0 turns it off)
  core._set_piece_streamed?.(process.env.PEAK_SPLINES ? (process.env.PEAK_SPLINES === '1' ? 1 : 0) : 1); // pv peakSplines (web/set_piece_gameplay.inc; PEAK_SPLINES=0 turns it off)
  core._stage_load_flags?.(process.env.LOAD_FLAGS ? (process.env.LOAD_FLAGS === '1' ? 1 : 0) : 1); // pv loadFlags (LOAD_FLAGS=1 / 0 forces it)
  // 0x535C11 (the watch's byte 9: 0 Conquer the Mountain): with kind 4 the score pays tricks as cash (web/score_gameplay.inc)
  core._set_stage_collect_state?.(dv.getUint8(gameW.offset + 9), 0, 0);
  let bank = -1;
  let autoPrev = null, autoPort = null; const autoLog = [];
  if (process.env.PEAK_AUTO === '1' && process.env.PEAK_AUTO_OUT) process.on('exit', () => fs.writeFileSync(process.env.PEAK_AUTO_OUT, JSON.stringify(autoLog)));
  function applyRows(i) { for (const id of ids) core._peak_world_row(id, rowState(i, id)); core._peak_world_set_course(course(i)); }
  // Record 0: its rows; the bank in use is the course row's location with id < 22 (the hub or course the rider is in).
  applyRows(0); core._peak_world_sync_octree?.();
  const row0 = manifest.residency.find((r) => r.course === course(0));
  const start = row0 && manifest.locations.find((l) => row0.locations.includes(l.code) && l.id < 22);
  // (the seed's initial.json already holds that bank and the retained route, extracted from the baseline savestate)
  if (start) bank = start.id;
  // A location entry (compare-ps2-capture.mjs --peak-arrival; record 0 = the placement record): 11D390 in kinds 4..6.
  // 112180(rider, 1) re-attaches the route at the grid slot (26B5E0(bank, 1, +0x86C) placed by 11D660; 115B08's offset is 0
  // for player 0), then 11DE60 places the rider at the entry row: courses < 14 (1, 2), backcountry (0, 1), stations (0, 2)
  // (runtime kinds; exported kind = runtime kind - 1; 26B5E0 falls back to the first row), 11DF18 the push velocity.
  function arrive(beforePlacement = () => {}, transport = 1) {   // transport 0: a world start's placement (--peak-fresh)
    const c = course(0), row = manifest.residency.find((r) => r.course === c), loc = row && manifest.locations.find((l) => row.locations.includes(l.code) && l.id < 22);
    if (!loc) throw new Error(`--peak-arrival: no location with a path bank for course ${c}`);
    const variant = loc.id >= 17 && loc.id <= 21 ? (kind(0) === 5 ? 1 : kind(0) === 6 ? (mode(0) === 11 ? 1 : 2) : 0) : 0;
    const bankJson = json(`${dirOf(loc)}paths.json`).variants[String(variant)];
    const regionOf = (index, runtimeKind) => bankJson.regions.find((r) => r.kind + 1 === runtimeKind && r.index === index) ?? bankJson.regions[0];
    const grid = regionOf(0, 1), [ei, ek] = c < 14 ? [1, 2] : c < 17 ? [0, 1] : [0, 2], entry = regionOf(ei, ek);
    const [gx, gy, gz] = grid.position; core._reset_rider(gx / 100, gz / 100, -gy / 100, Math.atan2(grid.direction[0], -grid.direction[1]));
    core._peak_world_deliver(loc.id); bank = loc.id;
    beforePlacement(); // the carried pre-placement words (the grid reset above reloads the seed's)
    core._place_rider_region(...entry.position, ...entry.direction, transport); // 1: from the Transport loop (the arrival captures)
    return { location: loc.code, course: c, grid, entry };
  }
  return {
    manifest, arrive,
    beforeTick(i) {
      if (i === 0 && seedState) {
        call(core._peak_world_seed, JSON.stringify(seedState));
        call(core._set_world_visual_state, JSON.stringify({ sections: seedState.sections }));
      }
      // PEAK_SEED_BOOSTS=file (diagnostics): [[resource, [9 entity words +0x20..+0x40 as u32], instance+8 flags], ...] read from the
      // baseline savestate (local/course-limits/seed_boosts.py): the one-way volumes it already holds (core stage_seed_boost).
      if (i === 0 && process.env.PEAK_SEED_BOOSTS && core._stage_seed_boost) for (const [res, words, flags] of JSON.parse(fs.readFileSync(process.env.PEAK_SEED_BOOSTS, 'utf8'))) {
        const p = core._malloc(36); new Uint32Array(core.HEAPU8.buffer, p, 9).set(words); try { core._stage_seed_boost(res, p, flags >>> 0); } finally { core._free(p); }
      }
      // PEAK_AUTO=1 (diagnostics, docs/peak-mountain.md "Course limits"): from record 1 the core's own streamer (22D8D8, one pass a
      // tick) decides the rows instead of the capture; every row transition that differs from the PS2's is logged (tick, row, from, to).
      if (process.env.PEAK_AUTO === '1' && i > 0) {
        if (i === 1) { core._peak_world_manual(0); autoPrev = Object.fromEntries(ids.map((id) => [id, rowState(0, id)])); autoPort = { ...autoPrev }; }
        core._peak_world_tick();
        const p = core._peak_world_rows(), n = new Int32Array(core.HEAPU8.buffer, p, 1)[0], rw = new Int32Array(core.HEAPU8.buffer, p + 4, 4 * n), now = {};
        for (let k = 0; k < n; k++) now[rw[4 * k]] = rw[4 * k + 2];
        for (const id of ids) {
          const ps2 = rowState(i, id), port = now[id] ?? 0, code = manifest.streaming.find((r) => r.id === id).code;
          if (ps2 !== autoPrev[id]) autoLog.push({ tick: records[i].tick, code, who: 'ps2', from: autoPrev[id], to: ps2 });
          if (port !== autoPort[id]) autoLog.push({ tick: records[i].tick, code, who: 'port', from: autoPort[id], to: port });
          autoPrev[id] = ps2; autoPort[id] = port;
        }
        return;
      }
      applyRows(i); core._peak_world_rows_tick?.(); // the eviction countdown: the octree removal at T+7, the record's row 0 at T+8 (web/peak_world.inc)
      // A peak run's location crossing restarts the game tick 1298C8 (1297C8 via 128A10 from world state 10's background rider
      // load, 9..24 ticks after the Unload: the location's NIS script read on the disc). The record's tick field is 1298C8, so
      // the capture says when (as the rows); the core's model (web/peak_world.inc) is for the browser.
      if (i > 0 && records[i].gameTick !== undefined && records[i].gameTick !== records[i - 1].gameTick + 1) core._game_tick_restart?.(records[i].gameTick);
      if (process.env.PEAK_TRACE && i > 0) { const tick = records[i].tick, ch = ids.filter((id) => rowState(i, id) !== rowState(i - 1, id)).map((id) => `${manifest.streaming.find((r) => r.id === id).code}:${rowState(i - 1, id)}->${rowState(i, id)}`); if (ch.length || course(i) !== course(i - 1)) console.error('peak', tick, ch.join(' '), 'course', course(i)); }
      // (the path bank stays when its location is evicted: 3AB498 kind 14 -> 12A490 -> 26ADA0 is jr ra; 12A340 replaces it at the next delivery)
      // 12A340 runs when the location's AIP record is dispatched, which can come before the row leaves 6/8 (the rest of the
      // chunk still reading: peak1-fr-aara1-glide ARA1 bank at record 3664, loader idle 3667, row 1 at 3668). The record shows
      // it as 112180's re-attach: the human's reset path +0xAB8 moves to the new bank while that location reads. Deliver
      // there; otherwise when the row leaves 6/8 (peak1-race-abc1a-glide: A bank and row 1 one record apart).
      if (i > 0 && resetPath(i) !== resetPath(i - 1)) {
        const reading = ids.filter((id) => id < 22 && banks.has(id) && !early.has(id) && [6, 8].includes(rowState(i, id)));
        if (reading.length === 1) { core._peak_world_deliver(reading[0]); early.add(reading[0]); bank = reading[0]; }
      }
      if (i + 1 < records.length) for (const id of ids) {
        const a = rowState(i, id), b = rowState(i + 1, id);
        if ((a === 6 || a === 8) && (b === 1 || b === 2)) { if (!early.delete(id)) core._peak_world_deliver(id); if (id < 22) bank = id; }
      }
    },
    autoLog,
    rowsAt: (i) => Object.fromEntries(ids.map((id) => [manifest.streaming.find((r) => r.id === id).code, rowState(i, id)])),
  };
}
