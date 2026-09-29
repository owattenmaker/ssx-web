// Differential test of two core builds (docs/sim-performance.md "Simulation core, round 3"). QA tool, not in npm test, not
// imported by the game. The same race (the human and the course's computer riders in one core, rider contexts, shared
// visual stream; courses without npc-riders.json ride solo) runs in core A and core B in one process with the same pads;
// every tick a wide set of each rider's words is compared and the first differing export and tick are named:
// state, posed frame, world state, progress, animation info, crash / reset / route / trajectory records, camera words,
// both RNG streams, the score object, posed bones, the renderer poses (animation_post's return), skin matrices, pose
// controls, and the renderer buffers (snow sprites, trail, wake, skin palettes) every tick or every --fx ticks.
//   node sim-diff.mjs COREDIR_A COREDIR_B [--course ARA1,BRA2,ABA1] [--pads tuck,script1,rand1,trick1] [--ticks 3000]
//        [--fx every|N] [--fast 0|1] [--fastB 1] [--mem 0|1] [--hostbit4 0|1] [--aiA FILE --aiB FILE] [--stats 1] [--perturb T] [--skip NAMES]
// COREDIR: a directory with core.js / core.wasm (web/runtime, or a CORE_OUT= build). Pads: tuck; scriptN (net/test-core.mjs
// scriptedPad); randN (random held sticks / buttons); trickN (jumps with flips released mid-rotation, spins, grabs, ubers).
// --hostbit4 0 makes the computer riders compute their renderer poses (rider_host bit 4 off) so those are compared too;
// --aiA / --aiB load a different web/ai-racers.js per side; --perturb T toggles tuck on core B at tick T (sensitivity check:
// with T after the countdown, e.g. 320, the run must report a mismatch at T); --mem 1 also compares the whole wasm memory every 50 ticks (same data layout only).
import fs from 'node:fs'; import path from 'node:path'; import { pathToFileURL } from 'node:url';
const W = path.dirname(new URL(import.meta.url).pathname);
const argv0 = process.argv.slice(2), aiArg = (k) => { const i = argv0.indexOf('--' + k); return i < 0 ? null : argv0[i + 1]; };
const aiA = (await import(aiArg('aiA') || W + '/ai-racers.js')).createAiRacers, aiB = (await import(aiArg('aiB') || W + '/ai-racers.js')).createAiRacers;
const { scriptedPad } = await import(W + '/net/test-core.mjs');
const argv = process.argv.slice(2); const opt = (k, d) => { const i = argv.indexOf('--' + k); if (i < 0) return d; const v = argv[i + 1]; argv.splice(i, 2); return v; };
opt('aiA', null); opt('aiB', null);
const COURSES = opt('course', 'ARA1').split(','), PADS = opt('pads', 'tuck,script1,rand1').split(','), TICKS = +opt('ticks', 3000), FX = opt('fx', 'every'), FAST = opt('fast', '0') === '1', MEM = opt('mem', '0') === '1';
const HUMAN = opt('human', 'RIDER_ZOE'), HOSTBIT4 = opt('hostbit4', '1') === '1', STATS = opt('stats', '0') === '1', PERTURB = +opt('perturb', '-1'), FASTB = opt('fastB', '0') === '1';
const SKIP = new Set(opt('skip', '').split(',').filter(Boolean)); // --skip a,b: leave those words out (e.g. a cache that a deliberate change moves, to see where the physics parts)
const [dirA, dirB] = argv;
const root = W + '/public/assets/'; const read = (p) => fs.readFileSync(root + p), text = (p) => read(p).toString('utf8');
function randPad(seed) { let s = seed >>> 0; const rnd = () => ((s = (Math.imul(s, 1664525) + 1013904223) >>> 0) / 4294967296);
  let cur = new Float32Array(24), until = 0;
  return (t) => { if (t >= until) { cur = new Float32Array(24); until = t + 8 + Math.floor(rnd() * 50);
      const st = rnd() * 2 - 1; if (st > 0.2) cur[21] = st; else if (st < -0.2) cur[20] = -st;
      for (const b of [5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 22]) if (rnd() < 0.18) cur[b] = 1; if (rnd() < 0.3) cur[22] = 1; }
    return cur; };
}
// trick:SEED - a cycle of about 5 s: tuck for speed, crouch (Cross) with an optional prewind spin, jump on release, then one
// air trick chosen per cycle: front / back flip held partway and released mid-rotation (left stick up / down, or D-pad up /
// down), spin + grab, flip + spin + grabs, an uber attempt (all four shoulders + Square), a long spin held to the landing.
function trickPad(seed) { let s = (seed * 2654435761) >>> 0 || 1; const rnd = () => ((s = (Math.imul(s, 1664525) + 1013904223) >>> 0) / 4294967296);
  let plan = null, start = 0;
  return (t) => {
    if (!plan || t - start >= plan.len) { start = t; const kind = Math.floor(rnd() * 8); plan = { kind, len: 240 + Math.floor(rnd() * 120), crouch: 40 + Math.floor(rnd() * 40), hold: 8 + Math.floor(rnd() * 40), steer: rnd() * 2 - 1, prewind: rnd() < 0.3 }; }
    const p = new Float32Array(24), k = t - start, jumpAt = 60 + plan.crouch;
    if (k < 60) { p[22] = 1; if (Math.abs(plan.steer) > 0.4) p[plan.steer > 0 ? 21 : 20] = Math.abs(plan.steer); }
    else if (k < jumpAt) { p[10] = 1; if (plan.prewind) p[20] = 1; }
    else { const a = k - jumpAt; // airborne phase
      switch (plan.kind) {
        case 0: if (a < plan.hold) p[22] = 1; break;                       // front flip, released mid-rotation
        case 1: if (a < plan.hold) p[23] = 1; break;                       // back flip, released mid-rotation
        case 2: if (a < plan.hold) p[6] = 1; break;                        // D-pad up flip
        case 3: if (a < plan.hold) p[7] = 1; break;                        // D-pad down flip
        case 4: if (a < 30) p[21] = 1; if (a > 5 && a < 45) p[12] = 1; break; // spin + grab
        case 5: if (a < plan.hold) { p[23] = 1; p[20] = 1; } if (a > 10 && a < 50) { p[13] = 1; p[15] = 1; } break; // flip + spin + grabs
        case 6: if (a > 3 && a < 60) { p[12] = p[13] = p[14] = p[15] = 1; } if (a > 10 && a < 20) p[11] = 1; break; // uber attempt
        case 7: if (a < 120) p[20] = 1; if (a > 20 && a < 26) p[10] = 1; break; // long spin held to the landing
      }
      if (a > 150 && a < 200) p[22] = 1; }
    return p;
  };
}
const padFn = (name) => name.startsWith('trick') ? trickPad(+name.slice(5) || 1) : name === 'tuck' ? (() => { const p = new Float32Array(24); p[22] = 1; return () => p; })() : name.startsWith('script') ? ((t) => scriptedPad(t, +name.slice(6) || 1)) : randPad(+name.slice(4) * 7919 + 17);
async function race(dir, course, createAiRacers) {
  const createCore = (await import(pathToFileURL(path.resolve(dir, 'core.js')).href + '?' + dir)).default;
  const human = await createCore();
  const put = (bytes) => { const p = human._malloc(bytes.length); human.HEAPU8.set(bytes, p); return p; }, str = (s) => put(Buffer.from(s + '\0'));
  const resources = { packetsJson: text('ANIMATIONS/animation-packets.json'), packetsBin: read('ANIMATIONS/animation-packets.bin'), initialText: text(course === 'ARA1' ? 'ANIMATIONS/initial.json' : `${course}/initial.json`),
    collision: read(`${course}/collision.bin`), terrainText: text(`${course}/terrain.json`), worldCollisionText: text(`${course}/world_collision.json`), railsText: text(`${course}/rails.json`), riderText: {} };
  resources.terrainHash = JSON.parse(resources.terrainText).source_sha256;
  const solo = !fs.existsSync(root + `${course}/npc-riders.json`);
  const doc = solo ? null : JSON.parse(text(`${course}/npc-riders.json`)); if (doc) for (const r of doc.riders) resources.riderText[r.package] = text(`${r.package}/rider.json`);
  human._init_animation(str(resources.packetsJson), str(text(`${HUMAN}/rider.json`)), str(resources.initialText), put(resources.packetsBin), resources.packetsBin.length);
  human._init_race(str(resources.initialText)); human._animation_use_physics(1); human._init_world(put(resources.collision), resources.collision.length / 4);
  const hash = str(resources.terrainHash);
  human._init_terrain(str(resources.terrainText)); human._init_world_collision(str(resources.worldCollisionText), hash); human._init_body_terrain(str(resources.terrainText)); human._init_rails(str(resources.railsText), hash);
  const racers = solo ? { npcs: [], start() {}, beginTick() {}, endTick() { human._fx_pass?.(-1); } } : await createAiRacers({ human, resources, document: doc, sharedVisual: true });
  const padPtr = human._malloc(96), f32 = (ptr, n) => new Float32Array(human.HEAPF32.buffer, ptr, n);
  human._reset_pad_history(); human._start_event(); racers.start();
  const cores = [human, ...racers.npcs.map((n) => n.core)];
  if (FAST) for (const c of cores) c._set_presentation_fast?.(1);
  for (const n of racers.npcs) n.core._rider_host(HOSTBIT4 ? 6 : 2); // bit 4: computer riders skip renderer poses (web/ai-racers.js sets it); 2: they compute them
  return { human, racers, cores, padPtr, f32 };
}
// The words compared per rider: [label, (core) -> Uint32Array]
const U = (c, p, n) => (p ? new Uint32Array(c.HEAPU8.buffer, p, n) : new Uint32Array(0));
const vecF = (c, p) => { if (!p) return new Uint32Array(0); const n = new Float32Array(c.HEAPU8.buffer, p, 1)[0]; return U(c, p, 1 + (n | 0) * 7); };
const STATE = [
  ['rider_state', (c) => U(c, c._rider_state(), 16)], ['pose_physical', (c) => U(c, c._pose_physical(), 12)], ['rider_world_state', (c) => U(c, c._rider_world_state(), 16)],
  ['race_progress_info', (c) => U(c, c._race_progress_info(), 8)], ['animation_info', (c) => U(c, c._animation_info(), 19)], ['reset_info', (c) => U(c, c._reset_info(), 9)],
  ['crash_info', (c) => U(c, c._crash_info(), 12)], ['crash_trajectory_info', (c) => U(c, c._crash_trajectory_info(), 32)], ['rider_trajectory_info', (c) => U(c, c._rider_trajectory_info(), 32)],
  ['route_info', (c) => U(c, c._route_info(), 14)], ['terrain_contact_info', (c) => U(c, c._terrain_contact_info(), 12)], ['rider_query_caches', (c) => U(c, c._rider_query_caches(), 10)],
  ['rail_gameplay_info', (c) => U(c, c._rail_gameplay_info(), 8)], ['rail_score_info', (c) => U(c, c._rail_score_info(), 8)], ['boost_hud_info', (c) => U(c, c._boost_hud_info(), 13)],
  ['upper_request_info', (c) => U(c, c._upper_request_info(), 12)], ['rider_stance_info', (c) => U(c, c._rider_stance_info(), 9)], ['camera_state_words', (c) => U(c, c._camera_state_words(), 271)],
  ['animation_rng_words', (c) => U(c, c._animation_rng_words(), 6)], ['visual_rng_words', (c) => U(c, c._visual_rng_words(), 6)], ['visual_lcg_word', (c) => U(c, c._visual_lcg_word(), 1)],
  ['score_object_dump', (c) => U(c, c._score_object_dump(), 0x1d0 / 4)], ['reference_motion', (c) => U(c, c._reference_motion(), 20)], ['physics_info', (c) => U(c, c._physics_info(), 6)],
  ['landing_info', (c) => U(c, c._landing_info(), 6)], ['boost_info', (c) => U(c, c._boost_info(), 8)], ['world_pose_bones', (c) => vecF(c, c._world_pose_bones())],
  ['rider_orientation', (c) => U(c, c._rider_orientation(), 4)], ['rider_fx_info', (c) => U(c, c._rider_fx_info(), 8)], ['impact_fx_info', (c) => U(c, c._impact_fx_info(), 17)],
  ['boost_fx_info', (c) => U(c, c._boost_fx_info(), 12)], ['start_info', (c) => U(c, c._start_info(), 7)], ['stance_restore_info', (c) => U(c, c._stance_restore_info(), 6)],
  ['rail_uber_info', (c) => U(c, c._rail_uber_info(), 6)], ['rail_exit_info', (c) => U(c, c._rail_exit_info(), 4)], ['rail_jump_info', (c) => U(c, c._rail_jump_info(), 3)],
  ['moving_instances', (c) => { const p = c._moving_instances(); return U(c, p, 1 + (new Float32Array(c.HEAPU8.buffer, p, 1)[0] | 0) * 17); }], // roller / chairlift car / spline piece draw deltas
  ['pose_controls_info', (c) => U(c, c._pose_controls_info(), 17)], ['pose_translation', (c) => U(c, c._pose_translation(), 3)],
  // renderer poses (animation_post's return when nothing is pending: the posed bones relative to the presented root);
  // computer riders' only when they compute them (--hostbit4 0)
  ['renderer_poses', (c, k) => (k && HOSTBIT4) ? new Uint32Array(0) : U(c, c._animation_post(), (new Float32Array(c.HEAPU8.buffer, c._animation_info(), 19)[4] | 0) * 7)],
  ['rider_skin_matrices', (c) => { const n = c._rider_skin_matrix_count(); return n ? U(c, c._rider_skin_matrices(), n * 16) : new Uint32Array(0); }],
];
const humanOnly = [['npc_world_x', () => new Uint32Array(0)]];
function presentation(c) { // renderer outputs (built lazily on read), exact sizes from their info records
  const out = [];
  const si = new Float32Array(c.HEAPU8.buffer, c._snow_info(), 23).slice(); out.push(['snow_info', U(c, c._snow_info(), 23)]);
  for (let e = 0; e < 10; e++) { const p = c._snow_particles(e); if (p && si[e]) out.push(['snow_particles' + e, U(c, p, si[e] * 8)]); }
  const ti = new Float32Array(c.HEAPU8.buffer, c._trail_info(), 16).slice(); out.push(['trail_info', U(c, c._trail_info(), 16)]);
  if (ti[0]) out.push(['trail_ribbon', U(c, c._trail_ribbon(), ti[0] * 9)]); if (ti[1]) out.push(['trail_roof', U(c, c._trail_roof(), ti[1] * 9)]);
  const wi = new Float32Array(c.HEAPU8.buffer, c._wake_info(), 13).slice(); out.push(['wake_info', U(c, c._wake_info(), 13)]);
  if (wi[11]) out.push(['wake_vertices', U(c, c._wake_vertices(), wi[11] * 9)]);
  if (c._rider_skin_palette_count()) { const n = c._rider_skin_palette_count(); out.push(['rider_skin_palette', U(c, c._rider_skin_palette(), n * 16)]); }
  return out;
}
const fnv = (h, u) => { for (let i = 0; i < u.length; i++) h = Math.imul(h ^ u[i], 16777619) >>> 0; return h; };
const eq = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
function memHash(core) { const u = new Uint32Array(core.HEAPU8.buffer); let h = 0x811c9dc5; for (let i = 0; i < u.length; i++) h = Math.imul(h ^ u[i], 16777619) >>> 0; return h; }
let failures = 0, compared = 0;
for (const course of COURSES) for (const padName of PADS) {
  const A = await race(dirA, course, aiA), B = await race(dirB, course, aiB), pad = padFn(padName);
  if (FASTB) for (const c of B.cores) c._set_presentation_fast?.(1);
  let bad = null, words = 0; const tricks = new Map(); let air = 0, inverted = 0; const td = new TextDecoder();
  for (let t = 0; t < TICKS && !bad; t++) {
    const p = pad(t);
    for (const R of [A, B]) {
      R.racers.beginTick(); R.human.HEAPF32.set(p, R.padPtr >> 2);
      if (R === B && t === PERTURB) { const k = (R.padPtr >> 2) + 22; R.human.HEAPF32[k] = R.human.HEAPF32[k] ? 0 : 1; } // sensitivity check: a tiny stick change in B only
      const o = R.f32(R.human._pad_tick(R.padPtr), 24).slice(); R.human._race_begin();
      const s = R.f32(R.human._step_rider(o[0], o[6], o[2] ? 1 : 0, o[7]), 16).slice();
      R.human._animation_tick(s[7], o[10], o[11], s[9], s[8], o[6], o[8], o[7], o[7], 0, s[15], o[13]);
      const pose = R.f32(R.human._pose_physical(), 12).slice(); R.human._race_end(); R.racers.endTick(); R.human._step_camera_head(pose[9], pose[10], pose[11]);
    }
    { const h = A.human, st = new Float32Array(h.HEAPU8.buffer, h._rider_state(), 16); if (!st[8]) air++;
      const q = new Float32Array(h.HEAPU8.buffer, h._rider_orientation(), 4); const upz = 1 - 2 * (q[0] * q[0] + q[1] * q[1]); if (upz < 0) inverted++;
      const np = h._trick_name(), name = td.decode(h.HEAPU8.subarray(np, h.HEAPU8.indexOf(0, np))); if (name) tricks.set(name, (tricks.get(name) || 0) + 1); }
    const readFx = FX === 'every' || t % +FX === +FX - 1;
    for (let k = 0; k < A.cores.length && !bad; k++) {
      const ca = A.cores[k], cb = B.cores[k];
      const list = STATE.filter(([n]) => !SKIP.has(n)).map(([n, f]) => [n, f(ca, k), f(cb, k)]);
      if (readFx) { const pa = presentation(ca), pb = presentation(cb); pa.forEach(([n, u], i) => list.push([n, u, pb[i]?.[1] ?? new Uint32Array(0)])); }
      for (const [n, ua, ub] of list) { words += ua.length; if (!eq(ua, ub)) { const i = ua.findIndex((x, j) => x !== ub[j]); bad = `tick ${t} rider ${k} ${n}[${i}] ${ua[i]?.toString(16)} vs ${ub[i]?.toString(16)} (len ${ua.length}/${ub.length})`; break; } }
    }
    if (!bad && MEM && t % 50 === 49) { const ha = memHash(A.human), hb = memHash(B.human); if (ha !== hb) bad = `tick ${t} whole memory differs`; }
  }
  compared += words; if (STATS) console.log(`  human: airborne ${air} ticks, board up-vector below horizontal ${inverted} ticks, ${tricks.size} trick names: ${[...tricks.keys()].slice(0, 30).join(', ')}`);
  console.log(`${course} ${padName}: ${bad ? 'MISMATCH ' + bad : `identical over ${TICKS} ticks x ${A.cores.length} riders (${(words / 1e6).toFixed(1)} M words)`}`);
  if (bad) failures++;
}
console.log(failures ? `${failures} mismatching runs` : `all runs identical (${(compared / 1e6).toFixed(1)} M words compared)`);
process.exit(failures ? 1 : 0);
