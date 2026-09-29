# Event instance activation recovery

The browser still imports the full ARA1 instance set. Event-specific visibility
and collision eligibility are incomplete; spatial SDB chunks are not event layers.

`tools/export_event_instances.py` resolves all3,052 imported instances through the
original resource registry in the audited Snow Jam countdown. It verifies resource
IDs, descriptor flags, snapshot provenance against event-start.json, and original
ELF identity. Output:local/event-activation/countdown-instances.json. It records
81 type6 DeadNodes. Original allocation tag4896B8 explicitly says DeadNode;
4896C8 says RestoreNode (type19). Both vtables' draw20 and contact140 entries point
to empty callbacks360790/3609F0. This is node callback evidence; it does not by
itself prove all static renderer or collision query eligibility.

`engine/instance_state.hpp` ports2FC2C0, the fallback invoked by builtin2/2FC420
when the instance event6 handler returns false (call site2FC590):

| Mode | Behavior |
| --- | --- |
| 0 | Preserve type6; otherwise destroy existing node with mode3 and construct type6. |
| 1 | Preserve type6; otherwise destroy existing node with mode3, then restore instance flags from the authored high half and retained runtime bits. |
| 3 | Preserve type6 or19; otherwise destroy existing node with mode3 and construct type19. |
| Other | No action. |

The restoration reads flags **after** the destruction callback. The callback
can mutate them. The port retains this ordering and shares the already recovered
34FC1C flag operation. Constructor/destructor ownership remains in host callbacks.

`python3 tools/test_instance_state_native.py` runs22,400 cases against the original
routine: all branches, absent/present nodes, signed entity types, full-width mode
values, random flags, randomized destruction effects and signed vtable adjustment.
It verifies destruction/allocator/constructor order and the allocation contracts.
Allocator and constructor calls are boundary stubs, so this does not validate the
constructor bodies or the full stage VM. Test output is local/event-activation/state-test.log.

Next: trace static instance submission and body/ray eligibility with DeadNode
ownership. Then connect the original stage program calls to consistent render and
collision state. Do not bake the countdown's81 IDs into a universal map filter:
free-roam/challenge configurations and later lifecycle behavior remain unverified.
No browser map geometry or collision was changed in this recovery pass.

## Body collector eligibility verified

`originalInstanceBodyRoute` now ports the333EF8 collector's3340F4..3341A0
runtime flag routing. Bit0x20 selects static geometry first. Otherwise bit0x40
requires an entity, whose virtual160/168 predicates supply the dynamic path.
If neither path applies, the collector advances without testing geometry.
The static branch subsequently checks authored bounds; entity routing is not
permission to query a static approximation of that entity.

`tools/test_instance_body_route_native.py` executes the original branch instructions
and delay slots for every16-bit runtime flag pattern, with/without an entity:
131,072 cases match. Execution deliberately stops at the three branch destinations
before bounds/narrow phase/virtual callbacks. This verifies routing, not contact math.
The audit exporter records body_route per instance and requires every captured
DeadNode to take skip. Countdown counts:1,798 static,2 entity,1,252 skip. Counts
include instances without collision geometry and must not be described as counts
of removed physical obstacles. All81 DeadNodes skip this collector independently
of the earlier empty contact140-method evidence (body routing uses160/168).

The browser still needs event-state application, rendering eligibility, and ray
query eligibility. Do not substitute this body collector gate for the ray gate:
ray queries have different callbacks and masks. No gameplay geometry changed.

## Mode0/2 ray routing verified

`originalInstanceRayRoute` ports335BB0 (335B90, mode0) and336D64 (336D40,
mode2). Both prefer bit0x20 static geometry over bit0x40 entity dispatch. Mode0
checks the entity pointer and skips if absent. Mode2 does not check before
reading its vtable, so the native host must treat missing ownership on that path
as unavailable/invalid, rather than quietly skip it. Subsequent virtual160/168
predicates and bounds/narrow-phase remain outside this helper.

`tools/test_instance_ray_route_native.py` runs the original branch instructions
through boundary stops for both query modes, every low16-bit flag pattern and
present/absent ownership:262,144 cases pass. The countdown audit now records
both ray routes alongside body_route, and validates the mode2 ownership invariant.
The81 DeadNodes skip body and both ray collectors. This completes their collision
eligibility audit; static renderer eligibility and actual event-state application
remain open. The browser has not yet been changed to apply these captured flags.

## Snow Jam collision integration

Normal `start_event()` now calls `apply_snow_jam_dead_nodes()`. The event seed
compiler checks the audit's snapshot/course identity and collision-package hash,
then generates the 81 captured DeadNode records. The runtime validates world hash,
resource IDs and authored flags for the whole set before applying any overrides.
Body and mode0/2 ray queries consult the verified routing helpers before candidate
bounds and unsupported-geometry handling. Terrain and other instance owners are
unchanged. The new optional override is currently used only for these verified
DeadNodes, which have an entity and skip all three collectors.

Overrides survive rider respawns and are reapplied idempotently on event restart;
loading a collision world creates a fresh unmodified set. Existing QA spawns that
do not call start_event retain their prior map state. world_collision_info appends
three counts at indices8..10 for body/mode0/mode2 skipped event instances. The
real-WASM event-start test checks zero overrides initially and81 in each collector
after each start, while preserving countdown, push-off and restart behavior.

This is a captured Snow Jam event initialization correction, not full LUN execution
or complete event/free-roam/challenge selection. The corresponding extra visual
geometry remains: static rendering eligibility still needs recovery. Do not claim
that the event map now looks fully correct, or that dynamic callbacks are ported.

Validation: full npm suite and production build pass. Native/browser trace: 9,630
frames across23 scenarios, zero mismatches. The event-start WASM test separately
exercises the new event exclusions; trace parity is not an original PS2 capture.

## Captured DeadNode meshes excluded in Snow Jam

The browser now preserves triangle ownership for the 81 captured DeadNodes in
spatial batching (`event_dead_resource`). A normal event start hides these meshes
at the same boundary that applies their collision exclusions. QA spawns that
bypass start_event retain the original full-area visibility. Source interpretation:
DeadNode allocation identity plus empty draw dispatch supports omitting its mesh;
the general static renderer's eligibility/culling routine is still unrecovered.
Do not describe this bounded correction as a port of that renderer or full LUN
execution. Other event-specific owners may still need different visibility rules.

`tools/test_event_mesh_ownership.py` checks the actual source and packaged assets:
all 510,343 triangle tuples preserved with multiplicity; vertices/colors identical;
each event-tagged range exactly matches its original instance provenance; no tagged
range includes a live pickup. The 81 instances account for 13,199 triangles.
The manifest and batch compiler validate event snapshot/course and collision package
identity. Asset files retain geometry; only event rendering disables those meshes.

Validation after visibility integration: full npm suite and production build pass;
asset ownership check passes; rebuilt browser menu flow, countdown and gameplay
rendering were exercised. This was a startup smoke check, not an exhaustive
frame-by-frame visibility comparison against the original game.

## Start-gate spark fountains (startfirePop) (September 22)

Source chain (checked against stage data and the ELF; no oracle run yet):
`mdl_ARA1_startfireTrig_1000` (8,644) handler slot2 -> LUN program 116
(Debounce 3, target `mdl_ARA1_startfireTimer` (8,1262), builtin 3 with key05=90).
The timer's slot5 program 118 creates six emitters with builtin 0x10 (0x2FD420,
MakeParticleData, keys = snow profile FIELDS order) in three left/right pairs
(906/1973, 1120/1534, 1005/1119), gated by builtin 0x37 with 1, 20 and 29, and
plays sound 82 between pops. Profile: 150 particles, Duration .5, Damp 1.5,
Life 1.5, Size 25+-15 -> 100, Vel (0,0,3000), R0V z1200, R1V x800, R2V y1200,
Force z-3000, colour ARGB (1,1,1,1)->(0,1,1,1), texture 28 `spx2`
(PARTICLE.SSH), BlendMode 0 -> GS ALPHA 0x48 additive.

Emitter model: 0x3705E0 sets remaining time Duration+Life+LifeR/2 and passes
Duration (not lifetime) into 0x36CBF8, giving an age step of
Duration*Damp/NumParticles; with Duration>=0, 0x370788 only advances the
kernel age (0x36D3E8, +dt*Damp) and never runs the continuous birth path.
Particles use the snow VU A00 equations with the instance matrix applied in
0x370058.

`tools/export_startfire.py` derives everything above from the stage rows,
the disassembled programs, world_collision.json and the ISO into
`web/public/assets/STARTFIRE/` (also run by `npm run setup`).
`web/startfire-renderer.js` simulates and draws them in the encoded composite;
`test-startfire.mjs` checks the decoded schedule, pairs, lifetime and ranges.

Inferred, not yet source-verified: builtin 0x37 argument as timer time in 1/30 s
since the trigger; particle i emitted at i*ageStep (matches the remaining-time
formula); per-emitter LFSR seeds (0x36CCB8 uses the 3177F0 stream; a fixed
per-instance seed stands in); birth tint 1.0; the trigger is the instance AABB
rather than the authored slab; only the player triggers (no AI riders exist), and
returning 10 m uphill of the trigger re-arms it. The blue glow on the gate
fences is separate world material work and not produced by these emitters.
