# Original obstacle collision

The original cruise collision phase is `0x13F488`, called after cruise contact/motion. Its collision body is not the native prototype's three swept rays: it is a broad sphere plus ten bone-attached spheres. Native source-operation kernels now exist in `engine/body_collision.hpp` and `engine/obstacle_collision.hpp`. The kernels now feed a native authored-scene loader/query path. Original pose generation and collision-event integration are still being completed.

## Rider body

The original body is at the pointer stored in rider+0xAA0. Its broad center is +0x10, broad radius +0x20, enabled sphere mask +0x28, and sphere count +0x2C. Twenty sphere slots are allocated. Each 32-byte slot starts at +0x30: center float4, radius float, bone index u32, and two retained auxiliary words. The current body uses ten slots:

| Bone | Radius (cm) |
| --- | --- |
| 1 lower spine | 25 |
| 5 head | 20 |
| 7 left upper arm | 15 |
| 9 left forearm | 15 |
| 12 right upper arm | 15 |
| 14 right forearm | 15 |
| 16 left thigh | 25 |
| 17 left shin | 20 |
| 19 right thigh | 25 |
| 20 right shin | 20 |

These IDs/radii agree across all 24 rider volumes exported from four captured states, including all six original racers. The centers follow the original posed skeleton; a guessed capsule or fixed-height ray does not reproduce that body. Captured Zoe's broad radius is 78.199989 cm.

The initial gate skips this phase for rider state 9. Otherwise it skips only when rider+0x874 is zero, rider+0x4C8 < 50, and the absolute difference between source position Z and rider+0x498 is < 50 cm. Meanings of the two latter fields are not renamed speculatively.

The query constructor is `0x32F650`, using the body pointer. The obstacle caller enables the normal-direction filter and supplies the ground normal. Terrain primitive candidates whose normal dot the supplied normal exceeds 0.8000000119 are excluded. Instance/box handling has additional paths and must not be replaced by that terrain filter.

## Authored instance classification

SSB record kind 16 contains an authoritative instance-to-collision descriptor table. `tools/world_collision.py` decodes it with bounds checks:

- Header+0x40: descriptor data offset.
- Header+0x44: instance count.
- Header+0x48: offset of u16 descriptor index per instance RID.
- Header+0x4C: descriptor count.
- Header+0x50: offset of u32 descriptor offsets relative to descriptor data.
- Each descriptor has a 16-byte header followed by 12 bytes per model node. +0x08 is retained as resource08; +0x0C is the separate collision-mesh resource and becomes a cache pointer in the original runtime (confirmed by0x3A6CC8). Raw type, flags, both resource fields, and node fields are preserved.

Original `0x334888` dispatches types 0/1/2/3 as no collision, separate triangle collision, model-node box collision, and alternate geometry. Snow Jam has 1,136 / 1,497 / 404 / 15 instances in those categories. Names containing “trig” are not an authoritative physical classification. The extracted data keeps every binding and makes no name-based exclusions.

The original instance's +0x88 points into the loaded descriptor data. Sixteen live instance bindings agree exactly in descriptor type, flags, and collision-resource identity with offline lookup through this table. Instance flags are constructed from descriptor flags and augmented by runtime state; their full lifecycle remains to be integrated.

Separate geometry is SSB kind 12. The extractor retains 285 collision resources containing 8,959 triangles for Snow Jam/shared resources, along with render-model node transforms/bounds and instance transforms. Triangle indices are byte triples, vertices/normals are float4, and bounding boxes group 16 triangles. These are dedicated collision meshes, not tessellated visual scenery. Their local coordinates remain source Z-up centimeters until original model/instance transforms are applied.

## Verified kernels

`originalBodyTriangleContact` follows `0x32A1C0`: reject against the broad sphere, then test enabled body spheres. It first tests a center's plane projection against the triangle, then vertex/edge overlap. It selects the sphere with the smallest **signed center-to-plane distance**, not the largest penetration. The returned contact point is the center's plane projection even when an edge test admitted the overlap.

`originalBodyBoxContact` follows `0x329590` and `0x32B2B8`. It tests box face projections and rejects overlaps confined to edges/corners. The source loop repeatedly tests the broad sphere while consulting enabled mask bits; it does not advance to child sphere coordinates. The native helper preserves that behavior.

`originalObstacleResponse` follows the linear portion `0x13F5C4..0x13F7C4`. It rejects normals almost opposite the ground normal. A slightly downward normal is projected onto the ground tangent plane and normalized. Position translation is normal × 1.1000000238 × penetration. When the relative velocity points into the surface, the added normal velocity is:

`closingSpeed + max(0.0500000007 × closingSpeed, 27.7777786255 cm/s)`

The helper returns the translation, resulting velocity, adjusted normal, incoming speed, and relative direction. The controller translates the collision body together with the rider and applies the recovered `0x1065B0` orientation response followed by the original quaternion rebuild. Original `0x105D98` collision/event notifications remain to implement. Dynamic surface callbacks can alter the queried hit before this response.

## Validation and remaining work

```sh
.venv/bin/python tools/test_obstacle_reference.py
.venv/bin/python tools/world_collision.py --location ARA1 \
  --output local/assets/collision-reference/ARA1.json
.venv/bin/python tools/obstacle_probe.py \
  local/reference/pcsx2/snow-jam-glide.p2s \
  local/reference/pcsx2/snow-jam-glide-120.p2s \
  local/reference/pcsx2/snow-jam-turn-left-30.p2s \
  local/reference/pcsx2/snow-jam-ready.p2s \
  --output local/reference/terrain/obstacle-bindings-audit.json
```

Original-instruction conformance passes 10,000 linear responses (9,900 pushes and 4,963 velocity responses), 5,000 body/triangle cases spanning both original sidedness modes (2,921 contacts), and 5,000 body/box cases (233 contacts). Accepted response vectors/points match exactly. The linear-response harness isolates later callbacks. A separate 10,000-case check validates collision orientation exactly, including parallel-normal inputs. Collision-event notification is still outside the implementation.

The native runtime path is implemented in `world_body_collision.hpp`, `collision_transform.hpp`, and `world_collision_asset.mm`. `SSXMeshAsset::bodyCollision` loads authored type-1 triangle, type-2 box, and type-3 sphere-tree geometry plus original coarse terrain body surfaces. The cruise controller queries a `BodyPoseProvider` at the end of the current 60 Hz tick, applies the original linear response, then applies the original final speed clamp. Recovered cruise no longer runs the visual-mesh swept rays. Missing pose/backend, unsupported nearby geometry, and unfinished collision events are explicit diagnostics.

Transforms scale local node translations, compose the node hierarchy with the instance rotation/translation, transpose the rotation for the inverse, and apply reciprocal uniform scale separately to body positions/radii. Result points are scaled before the world transform. The source's local penetration scalar is retained for response/selection rather than silently rescaled. The original retains the query direction for local triangle filtering; the native path preserves that operation order.

Additional validation: 1,000 three-node hierarchy/instance transforms and body-volume transforms (including negative scales) match original instructions exactly. The full original-instance-query harness initially covered77 queries/28 contacts and now includes14 sphere-tree queries, totaling91 queries/42 contacts with zero point/normal/penetration error. Five dynamic cases are excluded explicitly. All six collision packages include decoded sphere trees; remaining unsupported dynamic-callback counts are A:1, ARA1:21, BRA2:8, CRA3:5, DRA4:0, ERA5:20.

The world renderer importer now applies the previously omitted instance uniform scale after node hierarchy and before instance rotation/translation. All six course packages are regenerated with `instance_scale_version=1`, retaining custom riding-start data and adding authored collision data. Negative authored scales are preserved.

Remaining work includes full animation-state coverage, populated sphere-cache checkpoint restoration, dynamic callbacks, collision-event/animation response, and broad gameplay comparison. None of the new data is used to heuristically suppress unknown objects.


Full scene conformance command:

```sh
.venv/bin/python tools/test_world_collision_live.py
```

The harness adds C++ dispatch labels for verified original virtual-function entry points in a local generated-code copy; it does not change the original instructions. Missing original callees stop the test.


Scalar arithmetic follows the installed PCSX2 policy: scalar DIV.S and SQRT.S use nearest rounding independently of FPU chop mode; VU operations retain their existing rounding. R5900 SQRT reads Ft. `reference_scalar_lowering.py` corrects that source-register lowering and applies instruction-specific rounding to local oracle copies, including generated inline division statements. The original game instructions remain unchanged. All kernel, hierarchy, and full-instance checks above pass with this policy.

`originalObstacleOrientation` implements the impact heading correction, limited to 20 degrees per call toward the original 110 degree wall-facing limit. It uses the original post-multiplied quaternion convention; substituting the cruise controller's pre-multiplied rotation would be incorrect. Original collision notifications remain visible as `obstacleEventPending` rather than being silently counted as implemented.


Sphere-tree format3 is now decoded from authored kind12 resources. Each model retains root center, radius/offset/stride levels, and the original bounded signed-byte RLE occupancy stream. `originalBodySphereTreeContact` preserves ancestor overlap rejection, ascending child visitation, first successful bone sphere, and the original omission of the active-sphere mask. Twenty thousand complete original327F18/32CDB0 tests match hit/push/point exactly (7,957 contacts).

The original32DF28 cache has a significant resource alias: its key omits the model-node index. The first visited node's masks are reused by later nodes of the same resource, even when their authored masks differ. The native world preserves this behavior with the original ten shallow and two deep round-robin entries (depth threshold5). Fourteen additional original-instance queries through loaded collectabreak and crumbTimer resources now match exactly; the complete scene suite totals91 queries and42 contacts with zero point/normal/depth error. Its captured cache pools begin empty; the fixture resets native cache for each independent saved-state query. Restoring populated original cache state remains necessary for arbitrary checkpoint parity.

Ray queries are distinct: original32E688 returns zero sphere-tree contacts. Air prediction uses coarse terrain in mode0 and scenery only in mode2. Dynamic callbacks, streaming traversal order under capped candidate buffers, full pose coverage and event response remain unfinished.


`test_generated_body_collision.py` passes only original rig/AFB assets and sequence/contact/root inputs to the native Zoe pose factory; captured bone arrays are removed. Two supported neutral snapshots cover2/24 captured rider volumes (NPC, turn, and ready states remain outside the provider). At glide and glide120, the broad sphere and8/10 bone centers match exactly; maximum shin errors are0.214844mm and1.25mm. Across1,892 scene/controlled geometry probes,516 contacts have zero hit or resource/node selection disagreement. Maximum penetration difference is0.900402mm, contact-point difference1.259766mm, and normal-component difference1.79e-7. These are measured residuals, not perfect pose parity. Pose clocks are seeded separately at each snapshot; this test does not validate continuous animation advancement.

The airborne world adapter now combines the coarse terrain path with verified authored static instance ray kernels. Source transforms preserve separate reciprocal scaling, triangle normals are authored, boxes can return entry and exit, and sphere-tree rays preserve the original zero-hit stub. Dynamic callbacks and saturated candidate buffers remain explicit incomplete coverage. `test_world_ray_live.py` compares full original instance queries, including exact returned surface IDs.

The complete predictor-mode ray suite now verifies 1,092 original-instance queries (220 contacts), including the original surface filter: mode0 includes only nodes with surface!=-1, while mode2 includes only nodes with surface==-1.

Generated-body comparison now also covers turn-left30, brake30 and jump30 pre-release anchors. Across all five snapshots, 4,730 scene/controlled geometry probes produce 1,021 matched contacts with zero hit or resource/node-selection disagreement. The new anchors have maximum body-sphere center errors of 0.15625 mm,0.3125 mm and0.78125 mm respectively. The comparison seeds each anchor's sequence/contact/root inputs independently and does not inject captured bone outputs or claim continuous animation-clock parity.

## Snow Jam long left turn (stopped 2026-09-22)

The shipped web ride step matches the 30-frame left carve (`snow-jam-turn-left-30`) bit-exactly, and it matches the wall hit in the 240-frame carve. It does not match the rest of `snow-jam-turn-left-long-240`. No source change was made for that remaining gap, and the long turn is not asserted in `web/test-ps2-ride.mjs`.

What matches: from the Snow Jam glide seed, full left meets `mdl_ARA1_element_platform1` at frame 173. Penetration is 2.62 cm. The original response leaves the rider at 361 cm/s with horizontal velocity near −40°. The shipped board center is about 8 cm from that trail sample. The rider stays on snow for all 240 frames and enters control 3 on animation 57.

What does not: the PS2 board trail then yaws that same velocity −40.1°, −31.4°, −19.9°, −11.6° on three consecutive frames (8.5°, 11.4°, 8.2°). Those edges are rider `+0x1E0`, read at `2E8F24`, not the posed board. The shipped integrate on that state yaws about +1.6° per frame. The 240-frame replay ends near (−135439.5, 12233.5), facing +8.5°, velocity +40° at 400 cm/s. The tick-578 save is near (−135538, 12513), facing +100°, velocity +107.5° at 642 cm/s, about 3 m and 636 cm/s off.

The three steps need roughly 3200–4400 cm/s² perpendicular to the 361 cm/s exit, along the wall normal near +45°. The live 43° lean of the 2289 cm/s² snow spring, even aimed fully sideways and with lateral drag added at the slip that maximizes it, tops out near 7.5° per frame and in this geometry produces the measured +1.6°. Lateral drag is off because post-bounce lateral speed is positive while the stick is left; flipping that gate yaws the other way and would break the 30-frame carve.

Checked and left out of the shipped step, because each one either does not run on these frames or does not write this yaw:

- The separating test (`f20 < 0`) skips the bounce impulse, `1065B0`, and `105D98`. Frames 174–179 still overlap the same face but are separating, so they only translate.
- Ground contact keeps the snow. The wall's dot with the snow normal is about 0.28, under the 0.3 gate, and the turn-offset probe misses the platform on the three burst frames.
- `12E778` only retargets the scaled turn. Control 3's word `0x04308000` does not hold boost, so `114130` clears `+0x2FC`.
- `2F6518`'s later blend can yaw toward board forward by weight `+0x310` (0.18), which is 8.5° for a 51° slip, but only while boost is on. That direction is clockwise, toward the post-snap nose near −91°. The trail turns counterclockwise. The blend does not run here.
- `13C7A8` is the landing speed scale. It multiplies velocity and keeps the heading. This carve never leaves the snow, so the factor is 1.
- `125970`'s steered add stays behind its 277 cm/s gate. The indexed stat at `0x5BD628` is 0 in the glide, turn-left-30, glide-120, and long-240 saves.
- Heading `11DFE0`, the contact correction `13ED24`, clip 57's root, the rail spline, and the other riders (56–66 m away) do not supply the burst. Rotating velocity by the full control-3 heading delta was tried and removed: it overshoots to facing −156° at 1259 cm/s.

## Instance-contact phase 105398 (2026-09-22)

Most authored scenery nodes have descriptor surface -1 (Snow Jam: 276 solid flag-3 nodes, 892 flag-0, 97 flag-2 triggers; only five nodes have a real surface). The original 334888 filter (0x334D00..0x334D6C) is: filter 2 skips **every** surface -1 node, filter 1 keeps only surface -1 nodes, filter 0 keeps all. `world_body_collision.hpp` now uses that rule; the earlier invented "surface -1 with flag bit0 stays in filter 2" rule is gone. Solid surface -1 scenery is therefore not answered by 13F488/13AA48/13C140/crash body responses but by the separate instance-contact phase `0x105398`, which 13F178 runs right after 13F488 (with body mask FFFFFFFF), 139C88 after 13AA48 (a1 = the bounced hit normal, else 0), 137860/138640 in crash motion and 13BFA8 on rail leave (mask from `rail_exit_mask.hpp`).

`engine/instance_contact.hpp` ports it:

- `originalInstanceContactQuery` = 104E70 after the 334458 filter-1 query: existing ranking/unique list (`pickup_contact_selection.hpp`), then, with two or more contacts, the sum of the priority (node flag bit0 and value != 0) normals starting from 0x4FF120 = 0, normalized only when longer than 0.001 (VDIV/VMULQ), the selected-entity callback (instance+0xC vtable+0x154) and the returned +0x40 depth (-1 when empty).
- `originalInstanceContactPhase` = 105398: skip in control 9; 106F78 hook; query; nothing more unless depth >= 0. 1231A8 (motion 0, or motion 2 with owner+0x30 == 0) projects the hit normal off rider+0x370, otherwise off the caller's a1; with neither and motion 4 the normal becomes ±owner+0xB0 (rail direction). Projection length < 0.001 takes the axis itself. The record sp+0x80 = [point, normalize(rider+0x1E0) by VRSQRT, normal, closing = max(0, dot(-n, v - hit+0x20))]; its +0x30 y/z/w lanes are stale stack words and are not modeled. Node flag bit1 and !(instance+8 & 0x2000) copies hit/record to rider+9E0/+A60 (rider+A30 then drives pickup collection 121818); flag bit0 calls 1057B8; !(instance+8 & 0x2000) calls the contact audio 296088.
- `originalInstanceContactResponse` = 1057B8: node value 0 goes to standalone 108388 (soft collision) and returns. Otherwise closing = max(0, n·(hit+0x20 - v)); restitution = clamp(node+4, 0, 1). Static instances (or entity vtable+0x74 != 0) push by n·(depth·1.1) (106538, note the operation order differs from 13F5C4), impulse = closing + clamp(restitution·closing, 55.5556, 1388.8889), 1065B0 steering only in motion 0, then 11E098. Entity contacts that return 0 use the mass formula sqrt(max(0, h² - (1-r)/(100/m+1))) - h with h = closing/(100/m+1). Air (motion 1, or crash-air motion 2 with owner+0x30 == 1) adds (2.5 - up·n)·max(n.z, 0.2) to rider+3F4 below 833.33 cm/s. Motion 1 with channel-2 class 1/2/9/11 and presentation-up·n > 0.3 takes the "land on top" path (velocity rebuilt from the tangential part, peak impact, 10E910/119E38/control 13→5/clip 268 unless clip 268 is already past 0.2, predictor restart). Every other case adds n·impulse to the velocity and calls 105D98(rider, record, hit+0x4C). There is no separating-contact skip: a closing speed of 0 still bounces by 55.5556 cm/s (carve ticks 409/410).

Oracle: `tools/test_instance_contact_native.py` (`tests/instance_contact_reference.cpp`) runs the recompiled 104E70, 105398 (with the real 104E70 and 1231A8) and 1057B8 with recording stubs for every other callee: 20,000 + 30,000 + 40,000 randomized cases match bit-for-bit (selection, aggregated normal, entity callback, projection, record, 9E0/A60 copies, velocity/+3F4/+438, and the callee order and arguments).

Browser: `web/instance_contact_gameplay.inc` runs the phase once per tick after the ground/air body response (animation_bridge), inside crash contacts (`crash.host.instanceContacts`) and as the first rail-exit phase. It owns the tick's filter-1 query, so pickup collection now happens through the flag-bit1 copy instead of a second query (the standalone pickup pass remains only for ticks without the phase). The browser keeps the pre-impact +3A0/+3B0 tangents after 1057B8 (unlike its 13F488 path) and soft entry now requests control 3 for the next input tick. `instance_contact_info()` and `INSTANCE_TRACE`/`INSTANCE_EVENTS` in `compare-ps2-capture.mjs` expose the diagnostics. Result: carve is exact through tick 604 (was 407); its three platform contacts at 408-410 match the hooked 105D98 records.

Still missing: dynamic entities other than the crashbag RollerModifier (next section; for example the `mdl_ARA1_tramlores_*` gondolas, which are entities at the countdown but still load as static), the air "land on top" side effects (10E910, 119E38, control 13→5, clip 268; they are counted as unsupported), and contact audio 296088. 106F78 is ported (next section).

## Hips/rail contact 106F78 and the core sphere mask (2026-09-22)

**106F78** (`engine/rail_body_contact.hpp`, wired as `boundaryContacts` in `web/instance_contact_gameplay.inc`) is the first step of every 105398 call; its return value is ignored and the instance query runs afterwards on the moved body. It is not an instance query: it returns 0 in motion 4, otherwise it takes the "hips" bone (rider+0x89C, bone 0), its local Y axis (the same EE sequence as 1086B8's axes) and queries the rider+0x860 spline layers (type 1 = grind rails) with 334680(list, hips, out, mask 1, 300), which is `originalRailWorldQuery` (335128 golden-section closest point; out+0x10 is the unit rail tangent, out+0x20 = 0x4FF120 zero, out+0x50 = 0). Gates: no rail; speed > 0.001 and (rail point - hips)·v < -0.2·speed (rail behind); the rail point farther than 50 cm from the hips segment clamped to ±50 cm along the axis; 108A48 would attach (the board-root rail attach test, so a rider who can grind does not bounce). Response: n = tangent, flipped to point along v, record closing = |v·tangent|; contact normal e = hips - point with its tangent component removed and, when 1231A8 holds, its rider+0x370 component, normalized (the tangent below 0.001); push 106538 by e·(50 - gap·e) when inside 50 cm; v += e·(max(0, e·(0 - v)) + 55.5556); in motion 0 1065B0 with the hit normal replaced by e, then 11E098; 105D98(rider, record, rail surface). Constants gp-0x7EA4/-0x7EA0/-0x7E9C/-0x7E98 = 0.001/-0.2/0.001/55.5556. Oracle: `tools/test_rail_body_contact_native.py` (`tests/rail_body_contact_reference.cpp`) runs the recompiled 106F78 with the real 1231A8 and recording stubs: 100,000 randomized cases (27,027 contacts, 6,676 attachable, 2,708 tangent fallbacks) match result, velocity and callee order/arguments bit for bit (xyz lanes; the original's -0.0 w lane after the flip is not modeled). Hooked PS2 notifications with ra 0x107554 are these contacts.

In the browser the attach test reuses `rail_view()` with the actor's motion and velocity and rider+9D0 = this tick's landing + accepted body-response translation. Two further original details were needed:

- **1211F8 approaches +0x22C/+0x238/+0x25C every tick**, also off the rail (`step_rails` wrapper in `web/rail_gameplay.inc`). The +0x25C tolerance leaves a rail at -0.25 and climbs by 1/30 per tick; the browser had frozen it, so 1086B8 used a 30 cm half width instead of 50 cm and the ground rail re-attach (boardpress-railjump 825) failed.
- **13F358 clamps after 13F488 and 105398.** core.cpp clamps the ground speed before the pose; the post-pose contact phase now restores this tick's unclamped velocity (`groundUnclampedVelocity`) for 13F488/105398 and clamps afterwards (a no-op without contacts; a contact that enters a crash clamps the crash actor too). boardpress-rail 821: a 106F78 hard crash at the speed limit.

**Core sphere mask.** 13F178 (0x13F278, word 0x4A115C) and 139C88 (0x13A734, word 0x4A1120) store 3 into the body's active sphere mask (+0x28) around 13F488 and 13AA48 and store 0xFFFFFFFF again before 105398. Both words are 3 in the ELF and in the captured states, so the ground and air obstacle queries test only spheres 0/1 (lower spine, head) against terrain and surfaced instances; 105398 (and 106F78's hips) use every sphere. The browser passed all ten spheres (`originalCoreBodyQueryVolume` in animation_bridge.cpp now masks the query). passive-inputs 731 was an arm/leg sphere touching terrain in the air that the PS2 never tested; the pose was exact.

Results: event-race exact for all 2399 compared ticks through 2417 (was 1988; 106F78 soft collisions at 1989 and 2003: closing 1874.66 and 1706.82, web previously answered 1989 with a 1057B8 hit at 835.75 on another instance), passive-inputs through its end 808 (was 730), boardpress-rail through 837 (was 820), boardpress-railjump through 838 (was 824). A 16 KiB re-capture `passive-inputs16` (same script) is also exact end to end.
## Crashbags: scripted instances and the RollerModifier (2026-09-22)

Snow Jam has 16 authored dynamic instances (descriptor flag 0x40000000): 13 `mdl_ARA1_crashbag_*` (type 3 sphere tree, one node: flags 3, value 1e30, auxiliary 0, surface -1) and 3 `mdl_ARA1_snowcrumb_*` (type 1). They are not entities at load: the audited countdown gives every one runtime flags 0x40214023 (the 34FC1C restore of the authored flags) and instance+0xC = 0, so 334458 routes them STATIC (bit 0x20) with their authored bounds and matrix. `tools/export_scripted_instances.py` verifies this and classifies the contact handler (authored handler table slot 2, the slot 121818/30A060 runs for a selected contact):

* crashbags: program 49 = `builtin0() builtin15() return`. builtin0 (2FC0D0) constructs an Object entity (356DB0, vtable 0x490E80; 34FB00 sets instance+0xC, 1032C0 sets flag 0x100 and puts it in the renderer's dynamic list, 0x200 is renderer bookkeeping), and with its default arguments turns flags&3==3 into bit 2. builtin15 (2FD250 -> 355DB8) allocates a 0x2D0-byte `RollerModifier` (ELF string 0x48E908, ctor 35DA70, vtable 0x48F080) with the rider's contact packet (rider+9E0, the selected 334888 packet; +0x48 is the hit node's prebuilt collider, +0x50 the instance) and 105398 record (rider+A60). The ctor clears bit 0x20 and sets 0x40 (entity route), clears 2, sets 4 and 1: 0x40214023 -> 0x40214345.
* snowcrumbs: no slot-2 handler (3A6B78 returns -1); a contact runs nothing and they stay static.

The export is compiled into `web/generated/event_instance_seed.hpp` (runtime flags, contact class, and the roller trees' mass properties: sphere-tree model header +0x1C centre of mass, +0x28 inertia, +0x4C inverse inertia, which the runtime tree object holds at +0x2C/+0x38/+0x5C). The browser loads these instances with their countdown flags; only unclassified dynamic instances stay unsupported.

### Entity route (334458/334888/104E70/1057B8/121818)

After the contact the instance takes the entity route. Every entity call ends in the RollerModifier (entity+0x1C -> container -> modifier, vtable 0x48F080):

| Original | Target | Result |
| --- | --- | --- |
| 334458 collidable/bounds (vtable+0x164/+0x16C 3569D0/356A00 -> 352B88) | modifier vtable+0x64 361D18 | bounds = modifier+0x220/+0x230 |
| 334888 override (vtable+0x134 356A28) | modifier vtable+0xA4 360BC8 | 0: ordinary node path |
| 334888 base matrix (vtable+0xD4 360990 = 0, vtable+0xCC 356128) | modifier vtable+0x9C 361D38 | modifier+0x240 replaces instance+0x10 as the hierarchy root |
| 104E70 selected-entity callback (vtable+0x154 356AE0 -> 353098) | modifier vtable+0xB4 360BD8 | no-op |
| 1057B8 rigid predicate (vtable+0x74 355420) | modifier vtable+0x44 361CA8 | 0: the mass path (value 1e30 makes the impulse ~ -1/(2c)) |
| 121818 entity contact (vtable+0x144 355770) | modifier vtable+0x54 361CD8 | modifier+0x20 = 0, 35DDE8 re-kick with rider+A60/9E0; the script re-run gate (vtable+0x4C 361CF8) returns 0 |
| 30A060 end callback (vtable+0x14C 355858) | - | entity+0x20 = 30 ticks (only gates the never-taken re-run) |

`WorldCollisionInstance::entity` (engine/world_body_collision.hpp) carries the entity bounds and the node matrices composed on the entity matrix (`composeWorldCollisionEntity`, same scaledNode chain as the static load); `WorldBodyHit::entity` feeds the 104E70/1057B8 entity branches.

### Tick order

Every game tick, before the rider's provider and controllers, each roller runs 35E850 and its entity runs 3568B0 (bounds = +0x240 translation -/+ radius +0x24; 3291E0 relocates the instance in the spatial tree). A bag created during the rider physics of tick T is first updated in tick T+1. The browser runs this in `race_begin` (web/roller_gameplay.inc `advance_world_entities`).

### RollerModifier dynamics (`engine/roller_modifier.hpp`)

A rigid body: +0x30 position, +0x40 quaternion, +0x50 momentum, +0x60 angular momentum, +0x70 inverse mass (script mass 1.0), +0x80 world inverse inertia, +0xB0/+0xC0 velocities; +0xE0 its own 0x32C508 sphere-tree collider (the hit node's tree header copied to +0x1A0: centre of mass, inertia, inverse inertia); +0x220/+0x230 bounds, +0x240 world matrix, +0x280 body inertia (header inverse inertia x 0.5), +0x2B0 centre-of-mass offset, +0x2C0 terrain cell cache. Constants are the gp-0x2A00..-0x29D0 bit patterns.

* Construction 35DA70 (from builtin15 355DB8): quaternion from the instance matrix (31B748/31B7A8), centre of mass rotated in, 35E770 transform, 35E248 world inverse inertia, 35D288 velocities, restitution 0.1, friction 0.4, bounce threshold 277.78 cm/s, then the kick 35DDE8: velocity = rider direction x closing / 2 + (0, 0, min(5000/mass + 100, 600)), angular velocity = normal x 10 (35CFF0 solves L). The attach 3554B0 -> 356780 -> 350570 sets the bounds to the authored AABB and the +0x24 radius to the largest corner distance from the instance translation (sqrt.s).
* Update 35E850: timer += 1/60. Below 10 s: energy blend, gravity -980 x mass and a contact-time damping force/torque, 35D340 integrate, inertia/velocities, 32C648 collider, then 35EDC8: the world query, push out by (depth - leaf radius/2 + 2) along the normal, angular damping 0.975, and 35D4A0/35D908 impulse response (restitution only above 277.78 cm/s; friction with the 0.001 slip threshold); a contact after 5 s sets the timer to 10. Energy below 10000 (or timer >= 10) zeroes momenta and sets the timer to 11; 11..16 s frozen; from 16 s the bag sinks 10 cm per tick.
* Rider re-contact 361CD8: timer 0 and the 35DDE8 kick again.

Oracle `tools/test_roller_modifier_native.py` (`tests/roller_modifier_reference.cpp`): 245,000 randomized cases against the recompiled originals (35D340, 35E248, 35D288, 35E770, 32C648, 35CFF0, 35D908, 35DDE8, 361CD8, 35D4A0, 35EDC8 and whole 35E850 with the query stubbed, 31B748/31B7A8, 35DA70 and builtin15 355DB8 with the attach, 3568B0), every byte of the 0x2D0 object compared. Live oracle `tools/test_roller_modifier_live.py` (`tests/roller_modifier_live.cpp`, built with `tools/original_live_build.py` + `tests/original_live_runtime.hpp`, which link every recompiled function reachable from the roots and run them on a savestate EE image): from the carve tick-607 savestate both rollers stay byte-identical to the original 35E850 + 3568B0 for 130 ticks (and a 1000-tick run through the rest, frozen and sinking paths) with the original 336850 as the query; 4,000 original constructions from the tick-600 savestate and the real tick-606 construction of bag 0003 match byte for byte.

### Sphere-tree world query (`engine/roller_world_query.hpp`)

35EDC8 asks 336850(2D1BE0(), 3303F0(collider, 1), &packet, modifier+0x2C0). 3303F0 builds a coarse (query+0 = 0), unfiltered query with bounds = collider centre +- level-0 radius; 336850 keeps the packet whose depth is closest to -1 (3306D8), ties: instance before terrain, lower instance+0x78, lower patch+0x150. Terrain: 335960 (patch flags 0x41, strict bounds) and 32B6E0 in coarse mode (3x3 cells, stride 3, cached cell first) with the tree-vs-triangle callback 330788 -> 32CA78 (32DF28 mask cache shared with the rider queries, 32DB40 child order by the corner directions with the 32DA40 shell sort, 32D470 recursion, 32D440 -> 32B6A8 point in triangle). Instances: 335B90 always uses filter 2, so every surface -1 node (all crashbags and almost all scenery) is skipped; the Snow Jam surface nodes are type-1 meshes (loga, snowcrumb), handled by 330540 and 32C0F8 -> 330828 -> 328030 -> 32CA78. Type-2/type-3 surface nodes throw (none exist).

The octree walk (335D78/33CCF8/340FA0) visits children in the order 0,1,3,2,6,7,5,4; patch order matters only for the cell cache and the collider scratch (+0x94/+0x9C), so the port sorts overlapping patches with `originalRollerSpatialBefore`. There is no 64-packet cap on this path (the port throws instead of overflowing), the 32DF28 mask cache is resolved on every 32CA78 call, and the scratch collider scale accumulates 1/instance-scale per processed node (0x327CC8 does not copy +0x90). The zero cases follow PCSX2 (VRSQRT/VDIV of 0 = FLT_MAX, VSQRT |x|, DIV.S by 0 = FLT_MAX): the savestates hold the (1e11,0,0,0) degenerate marker that only VRSQRT(0) = FLT_MAX produces in zero-area terrain cells.

Oracle `tools/test_roller_world_query_native.py` (`tests/roller_world_query_reference.cpp`): 275,200 randomized cases (120,000 330788/330828 tree-vs-triangle kernels with 60,756 hits, 12,000 32C0F8 meshes, 40,000 335960/32B6E0 coarse patches incl. zero-area cells, 40,000 336850 selections, 24,000 334888 filter-2 instances), comparing point, 4-lane normal, depth, triangle, collider scratch, terrain cache and the scratch collider. Live `tools/test_roller_world_query_live.py`: the complete original 3303F0 -> 336850 on the 607..738 savestates vs the port on the native world package, for both rollers, 346 recorded trajectory states and random poses around the bags, the loga/snowcrumb meshes and the slope: 10,246 queries (4,320 hits) identical.

### Browser integration

* `web/world_bridge.cpp` loads the 16 scripted instances with their countdown runtime flags and keeps their hierarchy; `WorldBodyCollision::query` takes the entity route when the runtime flags say so.
* `web/roller_gameplay.inc` (included by `web/instance_contact_gameplay.inc`): the 105398 store step calls `browser_scripted_contact` (121818: construct + attach on first contact, re-kick afterwards); 104E70/1057B8 entity callbacks; `advance_world_entities` from `race_begin` (35E850 + 3568B0, then the entity geometry is recomposed); reset with the pickups on a new race.
* Rendering: `web/prepare.py` splits the crashbag draw batches (`moving_resource`, `web/world-batches.py`), core `moving_instances()` returns entity matrix x authored^-1 in three.js space and `web/moving-instances.js` applies it every frame.
* Diagnostics: `roller_info()`, `roller_bytes(k)`; `compare-ps2-capture.mjs` compares the rollers after `race_begin` with a capture's roller-pool watch window (`ROLLER_TRACE`, `ROLLER_BYTES`, summary `rollerTicksExact`/`firstRollerMismatch`).

### Ground truth and results

`tools/ps2_capture.py` gained `build --watch ADDR:LEN` (raw EE windows copied into every record from offset 10240) and `run --keep-states` (keeps the `--snap` savestates). `local/ps2-capture/runs/bag/carve-bag.*` is the carve script with the roller pool (0x1CEF540, 0x8A0 bytes), both crashbag instances and entities watched, plus savestates at ticks 600/607/612/621/642/682/738. The carve capture is now exact through its last tick (777; was 604): the crashbag hits at 605 and 607 take the static rigid path, the rollers are created, and the rider's re-contacts with the flying bag 0003 at 676-679 take the entity mass path and re-kick it. In `bag/carve-bag` both rollers match the capture bit for bit on all 344 roller ticks. On The Junction, pipe-uber and `bag-bhp1/uber-bag` build the BHP1 roller (next section).

Remaining gaps: ~~the browser terrain package has only track 8~~ (fixed: the package now holds the whole event world, see "Snow Jam race-event world membership" below; the roller live test has no excluded queries any more). The crashbag_end_build bags (the other roller tree, resource 61704) have oracle coverage only through the randomized construction/dynamics cases, not a capture; instance flag 0x200 (renderer bookkeeping in 1032C0) is not modeled; 3291E0's spatial relocation is not needed by the browser's flat instance list (the query order argument is in the header); the snowcrumbs have no contact handler and stay static. The chairlift (tramlores) entities are ported in [set-pieces.md](set-pieces.md).

### BHP1 rollers: the departure-tick flight seed (pipe-uber 1143, 2026-09-22)

The Junction's `mdl_BHP1_crashbag_12` (resource 108559, instance 0xE28530, a different sphere tree from the ARA1 bags)
takes the same program-49 route: the rider's soft contact at tick 1142 builds the Object entity (0x54B240) and the
RollerModifier (0x197AF90 in the pool 0x197AF80). The browser then re-contacted the new roller during tick 1143
(mass path, depth 16.87 cm, reaction 343 crash) where the PS2 has no contact. There is no roller or instance-query
rule behind it; the rider was in the wrong place:

* Tick 1142 is a passive departure in ground motion. 13F178 runs 13F488, 105398 (the crashbag contact bounces the
  rider: velocity (-680,-938,-460) -> (-489,-318,405) cm/s), 107888 and only then, at 0x13F2B0..0x13F2CC
  (departure flag and 11FE98 == 0), `11FE78(rider,1)` -> 1112B8 -> motion-1 enter `1399E0`, which seeds the flight
  `1135B8(rider+0x788, +0x110, +0x1E0)` from the post-contact state.
* The browser seeds the flight when the departure is detected (core.cpp `begin_airborne`) and re-seeds after a
  13F488 response (`apply_body_contact`), but not after a 105398 response. Tick 1143's air motion followed the
  pre-bounce trajectory (-11,-16,-8 cm instead of the PS2's -8.1,-5.3,+6.8), the posed body stayed ~19 cm deeper
  in the bag and hit it.
* Fix (`web/instance_contact_gameplay.inc`, `run_rider_instance_contacts`): when a 105398 contact changed the
  rider in ground motion after it left the ground (`motion == 0 && !grounded`, no crash), `begin_prediction` from the
  new position/velocity. `OriginalAirTrajectory::begin` only initialises (the flight is extended on the next
  advance), so a second seed before the first advance equals the single 1399E0 seed.

Evidence: a scratch live oracle (the `tools/original_live_build.py` harness with roots 32F650/334458/104E70 and the
query/Object/RollerModifier vtables) on `bag-bhp1/uber.tick1144.p2s` (after tick 1143's rider physics; the savestates
precede the tick's entity update, the capture records follow it) runs the original `32F650(q, body, 1)` ->
`334458`/`334888` on the PS2's own body: 0 contacts, the entity bounds come from modifier +0x220 (roller +/- 271.3 cm)
and the base matrix from vtable+0xCC = modifier+0x240. The port's `scaledNode`/`toLocal`/
`originalBodySphereTreeContact` on the same body, matrix and BHP1 tree agree (node AABB overlap, no sphere contact),
so the query, entity bounds and composition were already right; only the body position differed.

Result: pipe-uber is exact through its last tick 1158, score included (was 1143); the new gated capture
`bag-bhp1/uber-bag` (same script plus `--watch 0x197AF80:0x8A0` and savestates at 1141/1143/1144/1146/1147) is exact
through 1158 with all 15 roller ticks byte-identical (construction, 35E850 flight, no re-kick). An immediate
re-contact (a rider still inside a bag the tick after building it) remains untested by any capture.

### Type-16 node entities skip every collector (rail-air-fence 785)

The countdown audit and the mid-race glide savestate agree that 74 instances with authored flags 0x210000 carry a type-16 node entity (vtable 0x491800) and runtime flags 0x210005/0x210305: neither 0x20 nor 0x40, so 333EF8/334458 bodies and the 335B90/336D40 rays never test them. One is the fence `mdl_ARA1_bigchal_white_int_06` (688904) that the browser alone hit in the air at rail-air-fence tick 785. `tools/generate_event_seed.py` now emits them (`browserCourseSkipNodes`, verified vtable and skip routes) and `web/world_bridge.cpp` applies their flags at load; rail-air-fence is exact through its last tick 788 (was 784). DeadNodes stay event-start state as before.

## Snow Jam race-event world membership (2026-09-22)

The browser must load exactly the world of the Snow Jam RACE EVENT (not free-ride). Ground truth is
the original's memory in the event savestates; `tools/export_event_membership.py` audits it and writes
`local/event-activation/event-membership.json` (also the hand-off list for the set-piece port).

**Which locations are live.** The ELF location table `0x43E250` (24-byte `{id, name[16], kind}`, read by
`144D38`/`144D50`; kind 0 course, 1 peak hub, 2 connector, 3 TRANSP, 4 sky) feeds the streaming table
`0x442168` (50 x 16 bytes `{id, SDB location index = resource track, state, kind}`, built by `22CD40`,
driven by the loader state machine `22D8D8`). In every event savestate (load-900, countdown, glide,
carve, and the 20 savestates of the full-race capture `setpieces/race` up to tick 7919) exactly five
locations are resident (state 2) and active (world manager `W=**(gp+0x16C8)`, `W+0x24+8*track` = 6):
**ARA1 (track 8), A_ARA1 (3), ARA1_B (9), TRANSP (0), ASKY (10)** - the course and its two connector
locations. They are loaded before the countdown and never unloaded during the race
(`world_assets.event_locations`: the course plus the SDB connectors whose `_`-separated name contains it).

**What of them is in the event octree** (`*(*(*(gp-0x848)+0x84)+0x20)`): every authored patch
(132 + 1913 + 193 = 2238) and every authored instance (240 + 3052 + 204, plus 2 TRANSP, 1 ASKY and 4
runtime-created track-128 tram cabins that share the `mdl_ARA1_tramlores_*` entities), every rail segment
of the three tracks (822 type-1 entities = 32 + 777 + 13 segments, 7 + 171 + 4 kind-8 records), the
kind-7 glow sources (128 ARA1 + 5 ARA1_B type-8 entities) and 188 of the 190 ARA1 kind-6 local lights
(the connectors' kind-6 records are not inserted). Only the course painter record is instantiated
(8 `tWPIGD_ScreenTint` objects = the 8 ARA1 payloads); the connector painters are resident data only.
The sets are identical in all event savestates; only runtime instance flags evolve.

**Event versus free-ride is runtime instance state**, set by the course scripts at race load, not a
different file set. Drawn, per the original collectors:
* static `22A5A0` (and patches `22A698`): runtime `(flags & 3) == 3`, the item's location active and its
  chunk (`instance+0x7D/+0x7E`, `patch+0x155/+0x156`) resident, then the frustum;
* dynamic: flag 0x100 puts the entity in the renderer list, vtable slot 0x20 draws: `0x356298`
  (Object 0x490E80, type-1 0x490B10, type-13 0x48EE60) needs flags & 4; DeadNode, RestoreNode and
  **type-16 nodes use the empty 0x360790**; type-10 cloth flags (`0x34AF18` empty) are drawn by
  `cFlagManager` near the rider and statically once their entity is dropped.

At the countdown this leaves 2933 static, 35 entity, 15 flag-manager and 513 undrawn instances. The
undrawn set is the bit0-clear helpers (reset planes, trigger/load volumes, emitters, cameraflash) and the
**74 free-ride Big Challenge objects** (`mdl_ARA1_BigCGate_*`, `flg_ARA1_BigCFlag_*`, `bigchal_*_ext/int`
04/06 groups, `bcarrow`, `challenge_gate_b`: type-16 nodes, flags 0x210005/0x210305), which the browser
used to draw (it hid only bit0-clear instances). The PS2 frames confirm them absent (e.g.
`setpieces/race.tick2719`: `BigCGate_1022` 17 m ahead, not drawn). The `bigchal_white` 01/02/03/05 gates
are ordinary static instances in the event (0x210023) and stay.

**Browser.** `tools/import_world.py` (and `world_collision.py`, `import_rails.py`,
`export_environment_lighting.py`, `export_light_glow.py`, `web/prepare.py`) import the resident course +
connector locations in load order (A_ARA1, ARA1, ARA1_B: the octree inserts at list heads, so this is the
`insertionOrder` the spatial ports expect). TRANSP (two dynamic-flagged transport models 6.4 km below the
course) and the ASKY SkyTop (the sky dome is `sky.json`) are not imported; the tram cabins belong to the
set-piece port. `export_event_instances.py` records the `draw` class per instance and `prepare.py` hides
every `draw == 'none'` instance (DeadNodes keep their `event_dead_resource` batches). Rail segment links
(`+0x60/+0x64`) index the segments of one location, so `web/rail_bridge.cpp` numbers segments per track.
`export_event_membership.py` fails unless the native package equals the live set (terrain resources and
flags = authored | 0x40, instances of tracks 3/8/9, rails, glow sources).

**Streaming chunks are not membership.** `W+0x3F0+24*chunk` (state 3) streams the texture chunks by
race progress: ARA1 chunks 30-32 cover the first ~1.3 km, 29/26/25/28/27 follow; A_ARA1's chunk 5 is
dropped at ~1900 ticks, ARA1_B's chunk 34 is not resident before the finish. Items of non-resident chunks
are not drawn (and `2EDB20` terrain lighting treats them as ineligible), but collision ignores chunks:
`335960`/`33CCF8`/`335D78` test only the patch flags (+0xA). The browser keeps everything resident.

**Oracles (connector terrain).** Tracks 3/9 are now in the collision package; the query oracles probe
them with `tools/terrain_probe_targets.py` (141 targets over and beside connector patches, including the
ones sharing octree cells with track 8). Because the recompiled `0x327C00` VU0 grid program is not
bit-exact, the oracles run the original and then store the console-exact grid (the savestate grids
already equal `terrain_original::coarseGrid`).
* `tools/test_crash_world_query_reference.py` (whole-world `336850`): 331 queries (49 before), 114 contacts.
* `tools/test_roller_world_query_live.py`: 2238 octree patches, 0 absent natively, flags/bounds/order equal,
  0 excluded queries (was: every query reaching tracks 3/9), 10,358 queries identical.
* Rider queries read the rider's query scope (rider+0x860), which `120E50` rebuilds from the rider query
  bounds (+0x400/+0x410) with `332DB8`; the plain landing/body oracles keep their rider-local samples and
  `tools/test_rider_scope_query_live.py` (`tests/rider_scope_query_live.cpp`) moves the query onto the
  targets, runs the original scope refresh and the complete `13A7B0` landing probe and `32F650 -> 3342D0`
  body query on the `glide` and `glide-120` savestates: 282 landing probes (222 contacts) and 282 body queries (69 contacts) identical, 290 of the contacts on connector patches. One landing probe leaves a different overlapping patch in the rider contact cache (same contact): the native `CollisionWorld` visits patches in the fixed 0..7 octree order while the `332DB8` scope list uses Gray order for partially covered cells; a pre-existing approximation, counted, not fixed.

## Metro-City contact differences closed (2026-09-22)

The Metro-City (BRA2) captures stopped at contacts whose response arithmetic was already exact. In each case
an input was wrong. After these fixes all eight Metro captures are bit-exact end to end
(`web/test-ps2-captures.mjs`). The rail and animation causes are in `engine/RAIL_RECOVERY.md` section 3.10.

* **metro-event-race 1094 ("soft collision push-out 0.08 cm").** The push-out itself (1057B8/108388) was
  exact, but the posed body was wrong. In that tick the rider is tucked (crouch 0.9) and presses boost. Control 0
  `131620` runs `114130` at `0x131844`, which sets +0x2FC = 0.25, before the `131878` main-animation selection
  reads +0x2FC (`0x131A4C`/`0x131B24`). So the PS2 selects semantic 8 (boost tuck) and the new sequence blends in
  under the collision clip 58. The browser selected with the boost value from the start of the tick
  (`controllerGround.boost`) and kept semantic 6. The different pose moved the body spheres by about 1 cm, and
  that changed the penetration depth. Fix: `browser_ground_controller_animation` takes +0x2FC after the
  controller's boost dispatch (`web/animation_bridge.cpp`).
* **metro-glide-carve 1208, soft collision inherits the fading copy.** `108388` plays the soft-collision
  semantic through `3128E8(anim, semantic, 0, -1)`, an ordinary play. `311F00` therefore inherits a fading-out
  sequence with the same clip, root, mask and mirror, keeping its clock and weight: its target becomes 1 and its
  fade becomes `blend*(1-weight)`. The browser forced a restart (`enter(...,force=true)`), so a second soft
  collision during the fade of the first (59 -> 22 -> 59) started a new sequence at t=0 and the pose was wrong.
  After the pose fix the 1222 push-out matches. Fix: `soft_collision_begin` plays without force.
* **metro-mix-glide 719/726, crash predictor restart.** `105D98` in crash motion 2 with owner+0x30 == 1
  (ragdoll air) calls `1135B8(rider+0x788, pos, vel, rider+0x2E4)` (`0x105EA0..0x105EEC`). That is `113198`
  followed by `113618`: a full restart that clears the times, the hit, the heading and the normal. Both crash
  notification paths (`web/crash_runtime.hpp` body impacts and `web/instance_contact_gameplay.inc` crash
  instance contacts) used `reseed` (113618 alone). The kept elapsed time made the first alignment step after
  the next prediction hit (726) one ulp different. The same restart for motion 1 is handled in
  `dispatch_body_event` (core.cpp). `BrowserCrashHost::speedLimit` supplies rider+0x2E4, which also sets the
  predictor's speed cap and the `extend` chord limit.
* **metro-mix-glide 1096, crash landing probe uses the rider's contact caches.** `137860` queries the body with
  cache `rider+0x868` and probes terrain with `138960` through `3342D0(..., rider+0x864)`. These are the same
  caches that the ground contact, the landing probe `13A7B0` and the `13F488`/`13AA48` body queries use, and
  entering control 8 does not clear them. The browser crash runtime kept private caches and reset them when a
  crash began. Its landing probe therefore resolved another overlapping patch at a seam: the contact point was
  0.09 cm off and the normal 1.5e-5 off, which changed the landing velocity. Fix: the crash runtime uses
  `browser_rider_terrain_cache()` (core.cpp `groundCache`) and `browser_rider_body_cache()` (world_bridge
  `bodyContactCache`).

Snow Jam: no gated or ungated Snow Jam capture now stops at a push-out. The "same class" divergence noted for
event-race 1989 was the `106F78` hips/rail contact (previous section). `handplant-leanR` (796, soft collision)
was already exact end to end in this tree, including a build without the fixes above. The fixes above are
generic, so Snow Jam captures of the same situations (boost pressed while tucked, a soft collision during the
previous soft clip's fade, crash landings after a cache-warm ride) now follow the PS2 as well. All 64 gated
captures pass.

## Inert "endmode" collision instances (2026-09-23)

ARA1 has five collision instances named mdl_ARA1_endmode_collide_1000..1005. Their authored flags are 0: neither the static bit 0x200000 nor the entity bit 0x40000000.

- **PS2 behaviour.** The countdown audit (`local/event-activation/countdown-instances.json`) shows runtime flags 2: no 0x20 and no 0x40 bit. Every body and ray collector (3340F4, 335BB0, 336D64) therefore skips them.
- **Previous browser behaviour.** The browser marked them "dynamic entity callbacks unavailable". A crash query or air prediction that overlapped them then threw OriginalCrashWorldUnavailable or OriginalAirTrajectoryUnavailable (cause 2, resource 564488), which stopped the game.
- **Fix.** `tools/generate_event_seed.py` now adds every collision-bearing instance without those authored bits to the course skip-node table, using its audited runtime flags. It refuses to build if any of them is not a skip in all routes.
- **Test.** `web/test-long-runs.mjs` (npm test) runs 8 full Snow Jam races from grid slots 0..5 with varied inputs. Each must finish without a core error.

## 1210B0 collision timers and the reset request (2026-09-23)

1210B0 runs after the controller dispatch and before the motion. It requests 116120(rider, 0, 2) when either condition holds:
- The decayed direction-change counter +0x3F0 is above 4.5 in any motion.
- In the air (motion 1) or in crash air (motion 2, submode 1): a live prediction (status 1/3) has more than 45 s left, or the air bounce counter +0x3F4 is above 5.

What follows the request:
- The reset begins immediately, so that tick's motion is already the frozen motion 3.
- The reset controller 12F398 first steps in the next tick.
- A new request while the reset is already running (3F0 still above 4.5) restarts the progress from 0 (`rerequest_reset`).

Captures: tech-oob-dance (requests at 3297..3299, placement at 3321) and tech-oob-hops (crash-air bounces, placement at 3451). Both match to the end.

## Instance walk order, LiveComp-animated collision, 13AA48 after a touchdown (2026-09-26)

Found at the whole-mountain crash contacts (docs/peak3.md section 6, "Past the crash contacts").

- **Walk order.** The PS2 keeps instances in its octree, like the terrain patches. 328660 / 328C20 insert at the head of
  the list +0x20 of the instance box's cell (+0x60/+0x6C); 332DB8 / 3309D8 and 334458 visit a node's own list before
  children 0..7.
  - `WorldBodyCollision::collidableInstances()` now returns that order: `originalSpatialCell` / `originalSpatialBefore`,
    and later insertions first within a cell. Every instance walk uses it: the body queries (13F488, 13AA48, 104E70,
    crash) and `queryOriginalWorldSegment`.
  - It matters wherever the result depends on order. 104E70 sums the priority contacts' normals (1 ulp at allpeak/apr 785
    and peak3/the-throne-tuck 697), and equal metrics are tie-broken by visit order.
- **LiveComp-animated collision.** 334888 composes an instance with an entity that answers vt+0xD4 (every LiveComp,
  0x361090) from the entity's node matrices (vt+0xE4 -> 0x361098), on either broad-phase route.
  - `stage_livecomp_collision()` (web/stage_world.inc) applies this to every core LiveComp: those built by trigger, timer
    or contact programs, such as falling paths, tree bumps, falling rocks and breaking bridges.
  - Section-started LiveComps (the collectibles' spin and bob) are drawn by the JS players and are not modelled here.
- **13AA48 after a touchdown.** 139C88 runs 13AA48 after its own touchdown too. It filters with rider+0x180 and uses its
  own air response; only the following 105398 / 105D98 / 108388 see motion 0. The browser used the new ground normal and
  the ground response there, and dropped a steep wall the PS2 bounced off (allpeak/apr 3581, peak3/gravitude-race-ai 1505).

## A soft collision in the departure tick (air-release/pipe-depart-soft 503, 2026-09-28)

- **PS2.** At The Junction, a hard left carve leaves the ground at tick 502, in the same tick as a 13F488 body contact.
  - 13F194 flags the departure, but the rider stays in motion 0 until 11FE78(1) runs at 13F2CC, after the contacts.
  - 105D98 classifies the hit with motion 0, and 108388 accepts: its gate is 11FE98 == 0 or 4, then control 0 / 2 / 7, with control 1 cancelled through 131348.
  - The rider enters control 3 in the air (record 503: control 3, motion 1; speed limit 3333.33 from the 0.03 s soft clip). Control 3 runs until 535 (control 4), and 536 is a control-5 air.
- **Browser, before core39.** `web/animation_bridge.cpp` produced the same Soft reaction (clip 59, next control 3) but entered it only when `grounded || poseLanded`.
  - A departure clears `grounded`, so the rider stayed in control 0: the words differed at 503 and the positions from 504.
- **Fix.** The gate also accepts `groundDeparturePending`, the port's "motion 0 until 13F2CC" flag, which `previousMotion` and `npc_gameplay.inc` already use.
  - The 105398 instance-contact path already had this right, through its motion argument.
  - pipe-depart-soft is now exact on all 1268 ticks.
  - The full capture suite and sim-diff (ARA1 tuck / script1 / rand1 / trick1, 3000 ticks x 6 riders) are unchanged.

## Back-to-back soft collisions (course-limits/p3b-right3000 14776, 2026-09-29)

- **PS2.** Hard right into The Throne's EBC3_E ice blocks (`ice_block_small_b_00033`): soft collisions at 14701, 14739 and 14774,
  each 32 ticks long; the third comes 3 ticks after the second ended. The soft control 12E778 leaves control 3 when 312AE8
  (the first channel-2 playback sequence, +0xC0) is complete. That is the new soft clip (57, t = 0.03 s), so control 3
  continues, and the next contacts (14779..14782) bounce the rider off the blocks.
- **Browser, before.** `web/animation_bridge.cpp` fed `soft_animation_complete` with any channel-2 sequence of the soft
  semantic that had completed. The previous soft's clip 57 (completed, fading out behind the new one: new plays are inserted
  first in their channel) answered, so control 3 ended at 14776 and the rider rode on into the blocks (31 m off by 16940).
- **Fix.** The first channel-2 sequence of that semantic decides, as the rail's `primaryCompleted` does.
- **Checked.** p3b-right3000 exact on all 3000 ticks (was 474; now a gate), the full capture suite (253 scenarios) unchanged.

