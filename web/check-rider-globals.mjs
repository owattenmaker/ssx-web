// Build check of web/build-core.sh (docs/ai-racers.md "One core, six riders"): the six riders are rider contexts of one core,
// so every mutable global of the core's own sources must be RIDER_LOCAL (thread-local: .tdata/.tbss) unless it is on the
// shared list below (course geometry, constant tables, parse caches). A new plain mutable global (.data/.bss of a web/
// or engine/ object) would be shared by all six riders: the build fails and names it. Usage: check-rider-globals.mjs core.map
import fs from 'node:fs';
const SHARED = [
  /^(world|cameraTerrain)$/,                                   // web/core.cpp: course geometry, immutable after the load
  /pendingWorld/,                                               // web/core.cpp: that geometry while it builds (init_world_step, the load)
  /^_ZZN3ssx15original_camera/, /^_ZGVZN3ssx15original_camera/, // camera constant tables (jump knots, swing spline, shake, chase variants)
  /^_ZN3ssx2[0-9]_?(multi_spline|spline_modifier|magnet_modifier)_constants/, /^_ZGVN3ssx2[0-9]_?(multi_spline|spline_modifier|magnet_modifier)_constants/,
  /(multi_spline|spline_modifier|magnet_modifier)_constants/,
  /worldTrackResidency|worldTrackOctree/,                      // Peak 1 residency of the shared geometry
  /set_piece_tables/, /^_ZL11scoreTables$/, /^_ZL15audioScoreHooks$/, // tables built from compiled seeds (constant)
  /bodyParseCache|terrainParseCache|railParseCache|environmentTexture|preparedDocuments/, // parse caches / immutable textures shared by the contexts
  /setPieceInstanceCalls/, /riderStatics[A-Za-z]*Ready/,    // parse-cache bookkeeping; static-init flags
  /tlsSize|tlsTemplate/,                                       // web/rider_context.cpp: the pristine TLS block itself
  /^__em_js_ref_/,                                             // EM_JS references
  /originalRoundingMode/,                                      // thread_local already (stripped name in old builds)
  /trickFxForce/,                                              // web/boost_gameplay.inc: gp+0x1630, one word for every rider
  /riderFxRenderScale/,                                        // web/boost_gameplay.inc: pv streamers (rider geometry scale), every rider
  /^_ZN10__cxxabiv1L10eh_globalsE$|^_ZN12_GLOBAL__N_18freelistE$|^_ZN12_GLOBAL__N_14heapE$|^current_timeout_ms$|^current_intervals_ms$/, // libc++abi / malloc / runtime state (shared as always; -flto puts it in the LTO object)
  /chairEntityShared|multiSplineShared/,                         // web/set_piece_gameplay.inc: chair entities / lift evaluations reused only on bit-identical inputs
  /avalancheShared/,                                           // web/avalanche_gameplay.inc: the loaded avalanche definitions, taken by the other contexts of that location
];
const map = fs.readFileSync(process.argv[2], 'utf8');
const bad = new Set();
for (const line of map.split('\n')) {
  const m = line.match(/\s(\S+\.o):\((\.data|\.bss)\.([^)]+)\)$/);
  if (!m) continue;
  const [, object, , raw] = m, symbol = raw.replace(/\.\d+$/, ''); // -flto (web/build-core.sh) renames an internalised global to name.N
  if (/\/(lib[^/]*\.a)|sysroot|libc|libcxx|dlmalloc|compiler-rt|emscripten\/system/.test(object) && !/emscripten_temp/.test(object)) continue;
  if (SHARED.some((re) => re.test(symbol))) continue;
  bad.add(`${object.split('/').pop()}: ${symbol}`);
}
if (bad.size) {
  console.error(`check-rider-globals: ${bad.size} plain mutable global(s) would be shared by every rider context (web/rider_local.hpp):\n  ${[...bad].join('\n  ')}\nMark them RIDER_LOCAL (or RIDER_LOCAL_LAZY), or add them to the shared list in web/check-rider-globals.mjs with the reason.`);
  process.exit(1);
}
console.log('rider globals: every mutable core global is per rider or on the shared list');
