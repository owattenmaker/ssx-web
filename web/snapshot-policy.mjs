// The rider-context snapshot's policy (docs/replay.md §2a; web/generate-snapshot-registry.mjs): the RIDER_LOCAL variables a restore
// leaves as they are, with the reason. The default is snapshot: every other registered variable is put back as the snapshot holds
// it. A kept variable's hash is taken at each save and checked at each restore (web/rider_context.cpp snapshot_keep_changed): a
// kept table that a race changes fails there (QA, the gates) instead of drifting.
const STATIC = 'loaded with its location or world, not changed by a race';
const STREAM = 'the streamed world (web/peak_world.inc); the PS2 re-derives its world cache after a restore (3A6800), not saved';
export const SNAPSHOT_KEEP = {
  // The per-context collision world: its geometry is the location's; its run-time state (instance flags, entity placements,
  // answer boxes) is snapshotted by web/world_bridge.cpp's hook (the PS2's entity buckets 26D988 / 26DDC0).
  browserBodies: 'collision geometry (' + STATIC + '); run-time state: world_bridge.cpp body snapshot hook',
  // The stage VM and the avalanche world: their loaded programs / definitions are the location's; their run-time state is
  // snapshotted by hooks (web/stage_script_gameplay.inc, web/avalanche_gameplay.inc).
  stageVm: 'loaded programs (' + STATIC + '); run-time state: stage_script_gameplay.inc hook (WScriptMan 0x30BB10)',
  avalancheWorld: 'loaded definitions (' + STATIC + '); the playing slots: avalanche_gameplay.inc hook, the PS2\'s 0x2D9CB0 / 0x2D9D68',
  bodySourceHash: STATIC,
  // rails (web/rail_bridge.cpp): the location's rail records and walk caches; the moving (dynamic) rails and teeters are snapshotted
  records: STATIC + ' (the rail records)', staticRecords: STATIC, walkAll: STATIC + ' (walk cache of the records)', walkStatic: STATIC,
  teeterVelocityTables: STATIC, railLocation: STATIC, locationTeeters: STATIC, railGroupSlots: STATIC,
  // the streamed world
  'peak_stream::pathBanks': STREAM, 'peak_stream::rows': STREAM, 'peak_stream::residency': STREAM, 'peak_stream::mapIds': STREAM,
  // stage world tables and models
  stageWorldModels: STATIC + ' (models)', stageMeshModels: STATIC + ' (mesh models)', setPieceInstanceIndex: STATIC + ' (instance index)',
  sectionTemplate: STATIC + ' (the section list template)',
  // rider model / animation tables
  trickNameTables: 'constant tables', scoreRules: 'constant tables', sourceSkinBind: 'the rider model',
  sourceSkinGroups: 'the rider model', sourceSkinIndices: 'the rider model', fastSkinBones: 'the rider model',
  // (resetPaths / evictedResetPaths are snapshotted: a location unload changes them, peak3/fr-throne-unload)
};
// The snapshot hooks' own storage (web/world_bridge.cpp, stage_script_gameplay.inc, avalanche_gameplay.inc): neither restored nor
// checked (a save changes it).
export const SNAPSHOT_OWN = ['bodySnapshot', 'stageVmSnapshot', 'avalancheSnapshot'];
// Left as they are because the PS2's snapshot does not hold them (0x26D818: the shared RNG 0x4FF030, not the visual stream
// 0x4FF018 or the LCG gp+0xA0C): a replay draws them on from where the run left them. Not restored, not checked.
export const SNAPSHOT_CURRENT = {
  snowParticleRandom: 'the visual stream 0x4FF018', stageWorldVisualOwn: 'the visual stream 0x4FF018 (before the snow data)',
  trailVisualRandom: 'the LCG gp+0xA0C',
  // the rider model's skinning of the drawn frame: re-derived by the next pose (the PS2's restore re-poses, 0x312598 / 0x3103F0)
  sourceSkinMatrices: 'the drawn frame\'s skin matrices', sourcePalette: 'the drawn frame\'s skin palette',
  // the streaming's event queue, drained by the page every frame (web/peak_world.inc): a restore would hand it stale events
  'peak_stream::events': 'the streamed world\'s event queue (allpeak/apr-start and peak3/fr-throne-unload change it)',
};
// Restored, then re-derived by a restore hook as the PS2 does (excluded from the save / restore self-check): the painter wrappers
// (0x2C03E8: web/weather.inc hook) and the painter trees' views of their node lists (web/environment_bridge.cpp hook).
export const SNAPSHOT_REDERIVED = ['breathEnvironment', 'weatherCamera', 'fogDriver', 'lightingDriver', 'weatherPainterResets', 'fogTree', 'lightingTree'];
