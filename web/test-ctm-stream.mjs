// Conquer the Mountain start and free-ride streaming (docs/ctm-flow.md "Start and streaming", pv ctmWorldAudio / streamGate /
// streamAhead): the read-ahead order of the streamed world against the PS2's reads, and the new career's world-load audio
// (pktrans under the ABC1 movie and the plane NIS, 28E8C0(19) at the ride start) against the PS2 music log.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { aheadLocations, connectorDest } from './free-ride.js';
import { PV_DEFAULTS } from './pv-flags.js';

for (const k of ['ctmWorldAudio', 'streamGate', 'streamAhead', 'streamWarm', 'sliceLoad', 'lodgeWorldLoad']) assert.ok(k in PV_DEFAULTS, `pv ${k}`);

// ---- read-ahead order ----
{
  const peak = JSON.parse(fs.readFileSync(new URL('./public/assets/PEAK1/peak.json', import.meta.url), 'utf8'));
  const chunk = (c) => peak.streaming.find((r) => r.code === c)?.chunk ?? 1e9;
  // Connectors lead down to the location after the underscore; courses, hubs and connectors out of the world lead nowhere.
  assert.equal(connectorDest(peak.residency, 'ABC1_A')?.course, 17);
  assert.equal(connectorDest(peak.residency, 'A_ARA1')?.course, 0);
  assert.equal(connectorDest(peak.residency, 'ARA1_B')?.course, 18);
  assert.equal(connectorDest(peak.residency, 'A'), null);
  assert.equal(connectorDest(peak.residency, null), null);
  // Happiness leads to Green Base Station only: its row's reads in the PS2's order (peak1-race-abc1a after the ABC1_A Unload:
  // A done at t71, then A_ABA1 100, A_ARA1 132, A_ASS1 154, DRA4_A 180; 22D8D8 = the lowest chunk first).
  assert.deepEqual(aheadLocations(peak.residency, 14, chunk), ['A', 'A_ABA1', 'A_ARA1', 'A_ASS1', 'DRA4_A']);
  // Green Base Station leads to Snow Jam, R&B and Crow's Nest (not back up to Happiness, nor to Peak 2 through DRA4_A).
  assert.deepEqual(new Set(aheadLocations(peak.residency, 17, chunk)), new Set(['ARA1', 'ARA1_B', 'ASS1', 'ABA1']));
  assert.deepEqual(aheadLocations(peak.residency, 0, chunk).sort(), ['B', 'B_BHP1', 'B_BRA2']);
  assert.deepEqual(aheadLocations(peak.residency, 5, chunk), []); // R&B: the end of its line
  // Every peak world: each connector of a row leads to a row of that world or out of it (then nothing is read ahead for it).
  for (const w of ['PEAK2', 'PEAK3', 'MOUNTAIN']) {
    const m = JSON.parse(fs.readFileSync(new URL(`./public/assets/${w}/peak.json`, import.meta.url), 'utf8'));
    const codes = new Set(m.locations.map((l) => l.code));
    for (const r of m.residency) for (const c of aheadLocations(m.residency, r.course)) assert.ok(codes.has(c), `${w} ${r.code}: ${c}`);
  }
  console.log('ctm stream: read-ahead order OK');
}

// ---- pv sliceLoad: the load screen's long core calls in parts give the same core state ----
{
  const { stageWorldParts, environmentParts } = await import('./peak-world-batches.js');
  const createCore = (await import(process.env.CORE || './runtime/core.js')).default;
  const probe = await createCore();
  if (!probe._stage_world_part || !probe._stage_world_load_hash || !probe._environment_load_hash) console.log('ctm stream: slice-load equivalence skipped (core without stage_world_part: web/build-core.sh)');
  else {
    const root = new URL('./public/assets/', import.meta.url);
    const txt = (p) => (fs.existsSync(new URL(p, root)) ? fs.readFileSync(new URL(p, root), 'utf8') : '');
    const put = (c, t) => { const b = Buffer.from(t + '\0'); const p = c._malloc(b.length); c.HEAPU8.set(b, p); return p; };
    // the stage world: init_stage_world (one call) == stage_world_part over the cutter's parts (every loaded container, bit for bit)
    for (const [name, pt, lt, st] of [['PEAK1', 'PEAK1/SETPIECES/particles.json', 'PEAK1/SETPIECES/livecomp.json', 'PEAK1/SETPIECES/stage-world.json'],
      ['ARA1', 'ARA1/PARTICLES/particles.json', 'LIVECOMP/livecomp.json', 'ARA1/STAGE/stage-world.json'], ['ABC1', 'ABC1/PARTICLES/particles.json', 'ABC1/LIVECOMP/livecomp.json', 'ABC1/STAGE/stage-world.json']]) {
      const p = txt(pt), l = txt(lt), st2 = txt(st); if (!p) continue;
      const a = await createCore(), b = await createCore();
      const na = a._init_stage_world(put(a, p), put(a, l), put(a, st2));
      let nb = 0; for (const [k, t] of stageWorldParts(p, l, st2)) nb = b._stage_world_part(k, put(b, t));
      assert.equal(nb, na, `${name}: stage world count`);
      assert.equal(b._stage_world_load_hash(), a._stage_world_load_hash(), `${name}: stage world state`);
    }
    // the environment lattice: init_environment == the globals + environment_add parts cut from the text (the textures by id)
    for (const code of ['ARA1', 'PEAK1', 'ABC1']) {
      const text = txt(`${code}/environment.json`); if (!text) continue;
      const bytes = new Uint8Array(fs.readFileSync(new URL(`${code}/environment.bin`, root)));
      const a = await createCore(), b = await createCore(), data = (c) => { const d = c._malloc(bytes.length); c.HEAPU8.set(bytes, d); return d; };
      a._init_environment(put(a, text), data(a), bytes.length);
      const { head, parts } = environmentParts(text), d = data(b);
      b._init_environment(put(b, head), d, 0); for (const part of parts) b._environment_add(put(b, part), d, bytes.length);
      assert.equal(b._environment_load_hash(), a._environment_load_hash(), `${code}: environment state`);
    }
    // the streamed world's rail catalogs: each appended location's rails.json whole == its parts (railBatches), after every location
    // (the teeters of ARA1 bind once the last part is in; the segment indices go on across the parts)
    if (probe._rail_load_hash) {
      const { locationBatches } = await import('./peak-world-batches.js');
      const pj = (p) => JSON.parse(fs.readFileSync(new URL(`PEAK1/${p}`, root), 'utf8'));
      const stream = async (railParts) => {
        const c = await createCore(); let hash = 0; const out = [];
        for (const code of ['A', 'A_ARA1', 'ARA1', 'ARA1_B', 'B', 'B_BRA2', 'BRA2', 'ASS1']) {
          const terrain = pj(`${code}/terrain.json`), world = pj(`${code}/world_collision.json`), rails = pj(`${code}/rails.json`);
          if (!hash) hash = put(c, terrain.source_sha256);
          const batches = locationBatches(terrain, world, rails, 'PEAK1', { railParts });
          if (!out.length) { const r = batches.find((b) => b.kind === 'rails'); c._init_terrain(put(c, JSON.stringify(terrain))); c._init_world_collision(put(c, JSON.stringify(world)), hash);
            c._init_body_terrain(put(c, JSON.stringify(terrain))); c._init_rails(put(c, r.whole ?? r.text), hash); c._peak_world_begin(); c._peak_world_reserve(65536, 4096); }
          else { for (const b of batches) { c._peak_world_append(1); if (b.kind === 'world') c._init_world_collision(put(c, b.text), hash); else if (b.kind === 'terrain') { c._init_terrain(put(c, b.text)); c._init_body_terrain(put(c, b.text)); } else c._init_rails(put(c, b.text), hash); c._peak_world_append(0); } c._peak_world_commit(); }
          out.push([code, batches.filter((b) => b.kind === 'rails').length, c._rail_load_hash()]);
        }
        return out;
      };
      const whole = await stream(false), parts = await stream(true);
      assert.ok(parts.some(([, n]) => n > 10), 'the large catalogs are cut');
      for (let k = 0; k < whole.length; k++) assert.equal(parts[k][2], whole[k][2], `${whole[k][0]}: rail catalog state (${parts[k][1]} parts)`);
    } else console.log('ctm stream: rail parts skipped (core without rail_load_hash: web/build-core.sh)');
    console.log('ctm stream: slice-load equivalence OK (stage world PEAK1 / ARA1 / ABC1, environment ARA1 / PEAK1 / ABC1, rail catalogs A..ASS1)');
  }
}

// ---- the new career's world-load audio (web/game-audio.js freeWorldLoaded) ----
const mod = await import('./game-audio.js').catch((e) => { if (/pathfinder/.test(String(e))) return null; throw e; });
if (!mod) { console.log('ctm stream: game audio skipped (web/pathfinder.js not present)'); process.exit(0); }
{
  const root = new URL('./public', import.meta.url).pathname;
  let T = 1000;
  const make = () => mod.createGameAudio({ now: () => T, fetchJson: async (p) => JSON.parse(fs.readFileSync(root + p, 'utf8')), fetchBytes: async (p) => new Uint8Array(fs.readFileSync(root + p)) });
  const step = async (ga, ms) => { T += ms; ga._director.pump(); await new Promise((r) => setTimeout(r, 0)); };
  const codes = (ga) => ga.timeline().filter((e) => e[1] === 'code').map((e) => e[2]);
  const FREE = { kind: 4, mode: 12 };
  // PS2 music/runs/newcareer: FadeOut + loading (s50, s82), pktrans event 1 (s573), the world load with no song pick (s594),
  // WS10 resumes pktrans (s629) and the ABC1 movie plays under it (s650..2325), then the plane NIS; the cinematic end sends
  // 28E8C0(19) (s3248); the spoke DJ 2.5 s and the Peak1 hub song 3 s after it.
  let ga = make();
  await ga.loadingStart({ courseCode: 'PEAK1', character: 'zoe' });
  assert.equal(ga.debug().loading, false, 'the loop is only wanted while the engine is locked (no context in node)');
  await ga.freeWorldLoaded({ courseIndex: 14, courseCode: 'PEAK1', character: 'zoe', freeRide: FREE });
  assert.equal(ga.debug().music, 'pktrans', 'pktrans when the load screen closes (234F40), before the arrival list');
  ga.arrivalCinematic(true);                        // main.js ui.cb.cutscene kind 'arrival': the ABC1 movie + #153 + #163
  await step(ga, 44000);                            // ~28 s movie + ~15 s plane
  assert.equal(ga.debug().music, 'pktrans', 'pktrans plays through the movie and the plane NIS');
  assert.deepEqual(codes(ga), [], 'no 28E8C0(19) during the arrival list');
  ga.arrivalCinematic(false);
  await ga.runStart({ courseIndex: 14, courseCode: 'PEAK1', character: 'zoe', freeRide: FREE }); // ride() -> startRun: WS1..4
  assert.deepEqual(codes(ga), [19], '28E8C0(19) at the ride start');
  await step(ga, 2500);
  const d = ga.debug().director; assert.equal(d.pending.firstSpoke, 1); assert.equal(d.pending.textMessage, 1);
  await step(ga, 500); assert.equal(ga.debug().music, 'Peak1');
  ga.leaveWorld();
  // A later career at the station (no arrival list): the world load plays the hub song and posts DJ kind 2 at once; the ride start
  // changes nothing.
  ga = make();
  await ga.freeWorldLoaded({ courseIndex: 17, courseCode: 'PEAK1', character: 'zoe', freeRide: FREE });
  assert.equal(ga.debug().music, 'Peak1');
  await ga.runStart({ courseIndex: 17, courseCode: 'PEAK1', character: 'zoe', freeRide: FREE });
  assert.equal(ga.debug().music, 'Peak1');
  assert.equal(ga.timeline().filter((e) => e[1] === 'worldload').length, 1, 'one world load');
  ga.leaveWorld();
  console.log('ctm stream: new-career world-load audio OK (pktrans under the arrival list, 19 at the ride start)');
}
