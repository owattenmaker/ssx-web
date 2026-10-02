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
  sourceSkinGroups: 'the rider model', sourceSkinIndices: 'the rider model',
  // (resetPaths / evictedResetPaths are snapshotted: a location unload changes them, peak3/fr-throne-unload)
};
// The snapshot hooks' own storage (web/world_bridge.cpp, stage_script_gameplay.inc, avalanche_gameplay.inc): neither restored nor
// checked (a save changes it).
export const SNAPSHOT_OWN = ['bodySnapshot', 'stageVmSnapshot', 'avalancheSnapshot'];
// Left as they are because the PS2's snapshot does not hold them (0x26D818: the shared RNG 0x4FF030, not the visual stream
// 0x4FF018 or the LCG gp+0xA0C): a replay draws them on from where the run left them. Not restored, not checked.
const RCAM = 'the replay camera (trigger manager 0x4C5830, the view director): not in the PS2 snapshot';
export const SNAPSHOT_CURRENT = {
  snowParticleRandom: 'the visual stream 0x4FF018', stageWorldVisualOwn: 'the visual stream 0x4FF018 (before the snow data)',
  trailVisualRandom: 'the LCG gp+0xA0C',
  // the rider model's skinning of the drawn frame: re-derived by the next pose (the PS2's restore re-poses, 0x312598 / 0x3103F0)
  sourceSkinMatrices: 'the drawn frame\'s skin matrices', sourcePalette: 'the drawn frame\'s skin palette',
  // presentationFast's (the low tier's) skin matrices of the drawn frame, rebuilt with each new animation tick (animation_bridge.cpp
  // rider_skin_palette); kept, the phone tier's replay failed the keep check (it changes every frame)
  fastSkinBones: 'the drawn frame\'s fast-path skin matrices',
  // the streaming's event queue, drained by the page every frame (web/peak_world.inc): a restore would hand it stale events
  'peak_stream::events': 'the streamed world\'s event queue (allpeak/apr-start and peak3/fr-throne-unload change it)',
  // the replay view (web/replay_camera.inc): 0x26D818 / 0x26DBF0 hold neither the camera trigger manager 0x4C5830 (the loaded triggers,
  // the entered stack, the active volumes) nor the view's director; the restore only steps the cameras (22E840 -> 0x15DF98) and the
  // loop's 0x1620D0 keeps the trigger lists (a loop carries the last loop's active trigger, as the event-load path's startRun(R))
  replayTriggers: RCAM, replayActive: RCAM, replayStack: RCAM, replayAlgorithm: RCAM, replayCompositor: RCAM, replayMode: RCAM,
  replayPreferred: RCAM, replayOverrideType: RCAM, replayOverrideAction: RCAM, replayOverride: RCAM, replayCutPending: RCAM,
  replayRevertTimer: RCAM, replayFired: RCAM, replayLastTrigger: RCAM, replayCuts: RCAM, replayOut: RCAM,
  manualYaw: 'the Manual camera\'s session global 0x4A127C', manualPitch: 'the Manual camera\'s session global 0x4A1280',
  manualDistance: 'the Manual camera\'s session global 0x4A1284',
};
// Restored, then re-derived by a restore hook as the PS2 does (excluded from the save / restore self-check): the painter wrappers
// (0x2C03E8: web/weather.inc hook) and the painter trees' views of their node lists (web/environment_bridge.cpp hook).
export const SNAPSHOT_REDERIVED = ['breathEnvironment', 'weatherCamera', 'fogDriver', 'lightingDriver', 'weatherPainterResets', 'fogTree', 'lightingTree'];
