# Pickup recovery

Current browser status: every stage-script pickup kind is live in the stage world (docs/set-pieces.md); the effects and HUD were matched against PS2 frames on 2026-09-25 (section "Pickup effects and HUD vs PS2 frames" at the end). Historical disabled-status notes below are superseded.

User requires original powerups enabled with executable-derived behavior. No pickup gameplay handler exists in the browser yet. Do not claim a visual model or inventory enables collection.

`tools/import_pickup_catalog.py` inventories original ARA1 authored instances from the existing source-derived render and collision manifests. Output `local/browser-pickups/ara1-catalog.json` retains raw transforms, source-centimeter and native-meter positions, model IDs, complete collision bindings and geometry counts. It verifies every candidate has a render instance. Counts:3 trickboost,2 speedboost,30 collecta,1 collectabreak. These are authored-name categories, not a recovered runtime type dispatch.

Trick boosts: track8 instance IDs847,938,1065, model resource91144. Binding372: type2, flags2162688 (0x210000), resource08=5384, collision_resource=-1, node flags2/surface-1. Speed boosts: track8 instances1383,1878, model resource128264. Binding528 has same type/flags but resource08=5128. Neither resource08 semantics nor collection threshold/effect is proven; do not treat these as verified script function IDs. `world_bridge.cpp` currently builds static collision from these descriptors; audit trigger handling before adding pickup collision.

Source name tables: `local/assets/source/ps2/bam.phm`/`bam.psm`, decoded via world_resource_names. ARA1 chunk33 contains kind16 collision/binding table; corresponding kind3 instance records retain additional unknown fields after matrix/model/scale. SSB resource IDs are reused across kinds; finding rid20/21 in another kind does not establish a linkage. Original executable strings are largely hashed/unnamed; direct pickup-name searches do not identify executable collection routines.

Next: decode instance/entity dispatch and kind16 non-collision fields, find original pickup contact callback and gameplay effects, preserve event eligibility/respawn/collection states, then connect per-instance visibility and original FX. Current world geometry is batched across instances, so independently hiding collected objects will require retaining instance ranges or separating pickup meshes. No guessed boost amount/radius/respawn timer has been introduced.

## Captured runtime linkage

`tools/probe_pickup_bindings.py` resolves each of the five boost instances uniquely in snow-jam-glide EE memory by exact authored64-byte matrix and resource at instance+120. Collision binding pointer is instance+136, confirmed by matching type/flags/resource08 and surface-1 to the offline descriptor. Output `local/browser-pickups/runtime-bindings.json` includes snapshot hash and addresses. All five checks pass. Trickboost1000: instanceFA5FA0, binding1289080; speedboost1001: instanceFF1A00, binding128A394. Initial instance+0/+4 words form spatial instance links (other matching instance structures), not established pickup-entity/vtable pointers.

`WorldBodyCollision::query` defaults surfaceFilter2 and explicitly excludes node.surface==-1; filter1 selects these non-solid contacts. Thus the current physical response pass deliberately skips pickup trigger geometry. A separate trigger/contact pass and original callback dispatch are required; do not change pickups into solid obstacles or invent a radius. This discovery does not prove every surface-1 node is a pickup; retain authored-instance/event classification.

Kind16 header count5 at+10/offset5C is NOT proven to be five pickups: values8,108,308,1408,1A08 do not enumerate their instance IDs. Resource IDs repeat across SSB kinds. Continue tracing runtime contact/event handling and stage logic; no collection gameplay has been enabled yet.

## Trigger geometry probe

`tools/test_pickup_contacts.sh` builds `tests/pickup_contact_probe.mm` against the actual browser collision loader and existing source-derived body collision routines. It isolates each of the five authored boosts (no terrain competing for the best contact), places a synthetic30cm single-sphere body at its bounds center, and compares filter1/non-solid with filter2/physical. All five trigger queries hit the expected resource/surface-1, physical queries reject them, and probes1000cm outside the bounds miss. Log `local/browser-pickups/contact-probe.log`.

This is a geometry/filter integration test, not recovery of the game's pickup activation volume or reward behavior. The30cm sphere is solely test input; it is not a new gameplay pickup radius. The original model-node transforms and collision bounds are used. Production collection remains disabled pending original event dispatch, state/eligibility, effects and visibility. A future trigger pass must avoid mixing terrain's best-contact selection with per-pickup event reporting, and must report multiple eligible triggers independently.

## Original non-solid contact interface

Original104E70 calls334458 with rider+860 instance list, body query, capacity64 and t0=0.334458 selects filter1 when t0=0 (filter2 otherwise), then calls334888 for eligible instances.104E70 records distinct instance resource IDs (instance+78) in rider+5B8 and appends FFFFFFFF. It also chooses a contact, blends some normals, and can call instance+0C entity vtable+154. None of those additional semantics is yet ported as a pickup dispatcher. Caller105398 builds the body query through32F650 before invoking104E70.

Original108C28 tests whether instance+78 occurs in rider+5B8, bounded to64 entries and stopped by FFFFFFFF. `engine/rider_trigger_contacts.hpp` ports this lookup; `tools/test_rider_trigger_contacts_native.py` / `tests/rider_trigger_contacts_reference.cpp` compare20,000 generated lists/IDs against the executable routine, including duplicates, full arrays and terminator cases. All match; log `local/browser-pickups/trigger-membership-reference.log`. The generated108A48 container lacked an arbitrary entry case for108C28, so the oracle copy adds only an entry label/switch branch; otherwise it accidentally executes108A48. Missing-target diagnostics are rejected, and only the corrected run is valid.

Next trace the consumers of this interface and the entity/event callbacks. Direct JAL search for108C28 found no callers (likely indirect dispatch; unverified). Do not equate successful non-solid membership with authorized collection/reward. No gameplay pickup state has been enabled by this helper.

## Indirect contact predicate consumers identified

`tools/trace_pickup_dispatch.py` verifies captured rider14701A0 interface+6C0 points to vtable4583A8; slot18 signed this-adjustment restores the rider pointer and slot1C targets108C28. Scanning source register uses of interface+6C0 / slot1C identifies callers30C10C..30C120 (within30BFC0) and301EE0..301EF4 (within generated2FC2C0 container). Output `local/browser-pickups/contact-dispatch.json` preserves snapshot hash, adjustment and instruction sequences.

301E5C..301EA4 resolves an authored resource through the world table at GP+16C8.301EAC..301EDC obtains the current rider via the player manager/current-player index.301EF0 calls its instance-contact predicate;301EF8 onward writes a boolean-like result through226610 (script value handling; exact type semantics still need tracing).30BFC0 resolves up to three authored resources and uses the contact predicate before additional position/geometry tests. These are source predicate consumers, not proven reward actions. Next connect the stage script records to the matching condition and action opcodes; do not assume contact alone awards a boost.

## Script opcode mapping recovered

Interpreter2228C4 masks the instruction low byte and dispatches through4797B0 (43 entries). Opcode21 maps223F58. That handler extracts builtin index from bits16..23, argument count from24..31, destination from8..15, indexes function table441F38 and calls it. Builtin41's entry44203C points301D78, the contact predicate identified earlier. This is confirmed through original instruction/table data, not an inferred numeric convention.

`tools/index_pickup_script_calls.py` asserts these ELF table mappings and indexes17 raw matching one-argument calls(01410221) in ARA1 kind16 data, retaining nearby words and offsets in `local/browser-pickups/contact-script-calls.json`. Calls range ABA0..E910. Treat them as call candidates until bytecode block boundaries are decoded; do not assume they all belong to pickups. Some nearby literal resource IDs differ from pickup instance IDs, so global/shared scripts, implicit current-context arguments and stage dispatch still need tracing. The kind20 animation payload also contains a coincidental matching byte pattern and is deliberately excluded from this stage candidate index.

## Bounded LUN program extraction

`tools/import_stage_scripts.py` decodes ARA1 kind16 header+38 program count238, +3C offset-table1FB8, and +40 following collision-data boundaryE9A0. The238 offsets are strictly increasing; first2370 equals table end. Each points to magic004E554C (`LUN`), +4 relative code end, +8 extent and +C duplicate extent. Header is16 bytes; bytecode ends at+4; remaining trailer words preserved without semantic guesses. Every extent exactly reaches the next program and every code region is word-aligned. Output `local/browser-pickups/ara1-scripts.json` preserves all code/trailer words and per-program/stage hashes. All238 validate.

The contact-call index now requires each candidate to lie inside one of those validated bytecode regions and records its program index/relative offset. All17 satisfy this. Full variable-length instruction walking and operand typing remain necessary before treating every candidate as an executed call. Example candidateABA0 belongs to program157 atAB88. No direct literal references to the five boost instance IDs were found in the earlier kind16 scan, so this stage's contact calls are not yet linked to those pickups. Continue tracing shared/global scripts and built-in pickup handling; extraction alone does not enable collection.

## Instruction boundaries decoded

`tools/disassemble_stage_scripts.py` walks all238 extracted LUN programs:8,522 instructions, all terminating exactly at their code-region end. Opcodes14/15/16/17/1D/24/25/26/27 consume one inline operand word; the tool verifies their source interpreter s6 increment instructions before decoding. Other opcodes are one word in the observed interpreter layout. Unknown opcodes, truncated operands and builtin indices outside the observed111-entry table fail explicitly. Builtin calls retain original function address, argument count, destination and raw word. No opcode execution is emulated.

Output `local/browser-pickups/ara1-disassembly.json` includes all instructions and17 contact builtin41 calls, now confirmed on instruction boundaries rather than raw byte candidates. Reachability and pickup-specific action linkage remain unverified. Builtin usage includes shared stage/event operations; no direct boost-state writes were found in the large2FC2C0 source container during the latest search. Continue tracing invoked manager actions/shared resource context. Run import_stage_scripts.py before disassemble_stage_scripts.py when regenerating.

## Authored reward linkage found

Builtin27 points2FF850, which obtains player/default current-player, effect type and float amount, then calls10F1C0 at2FF95C.10F1C0 dispatches types0..7 through456AF0. Type1 calls rider interface6C0 slot6C→10E770; type2 calls slot74→10E7D0.10E770 adds f12 to rider+2E8 (ground boostWindow);10E7D0 adds f12 to+2EC (boost modifier). Both request feedback through28B180→29CED8 (kind0/1, f12=1), then28B180→2A3B18 (kind1/2). These are counter additions, not direct stored-meter+2F8 awards.

Kind16 header+18/+1C describes231 six-word rows at70, ending exactly at header+34 offset1618. Collision binding resource08's rid20 selects row20: FFFFFFFF,1B08,1C08,FFFFFFFF,FFFFFFFF,FFFFFFFF. Rid21 selects row21: FFFFFFFF,1D08,1E08,FFFFFFFF,FFFFFFFF,FFFFFFFF. Slot2 therefore references program28 for speedboost and30 for trickboost. Each starts opcode28 arg1 literal1/2, opcode28 arg2 literal5, builtin27 with two arguments. `tools/link_pickup_rewards.py` verifies this linkage for all five authored boost instances and writes `local/browser-pickups/reward-links.json`.

Collection/event-slot timing and lifecycle remain unverified: do not yet assume slot2 fires every overlapping frame or that a pickup should respawn immediately. Trace program27/29 (other populated slot), builtin69 and subsequent visibility/state/FX actions in programs28/30. Need original counter/feedback oracle and correct per-instance state before enabling rewards. Prior generic-contact builtin41 investigation was useful for event predicates but does not itself establish pickup event dispatch.

## Verified reward primitive and follow-up component path

`engine/pickup_reward.hpp` ports the source EE counter addition used by10E770/10E7D0. `tools/test_pickup_reward_native.py` / `tests/pickup_reward_reference.cpp` execute both original routines, compare40,000 exact counter results, and assert callback order28B180→29CED8→28B180→2A3B18, correct rider/manager arguments, feedback kind0/1, FX kind1/2 and feedback scalar1. Both authored amount5 and randomized amounts/counters covered. Log `local/browser-pickups/reward-reference.log`. Helper is not yet wired to production trigger collection.

Programs28/30 call builtin69 (target302490) with argument1=1 immediately after the reward. It resolves the current/explicit instance, checks instance+0C entity, queries vtable+84, then accesses entity+1C. Argument1=1 calls353278; zero calls353228.353278 iterates attached entries through head+10, queries each entry's type via vtable+14 and requests state3 through vtable+C only for type3 entries. Exact component semantics and effect on collection eligibility remain unverified; do not call this proven object hiding or respawn behavior yet.

Programs27/29 (the other populated authored handler slot) query builtin99 then conditionally invoke shared property/FX-related builtins. Need decode event-slot dispatch and these attached component types to establish when rewards can repeat and how objects change state. Do not add a guessed respawn timer.

## Debounce lifecycle located

Reward programs28/30 next call builtin1 with argument2=3 and argument3=0. Builtin1 (2FC7D0) supplies defaults: target=-1/current instance, duration float1, restore/state argument1, mode=-1. Authored arguments overwrite restore/state to3 and mode to0. It updates an existing component through2FAE38 or allocates the named `Debounce` component (ELF string4896E8) with constructor342C08. This is separate from builtin69's attached-component operation.

Constructor stores ticks=integer(duration*clockRate), with captured clock GP+2A74→4C9428 and rate60 at+10. Mode0 clears instance flags' low four bits; restore/state3 retained at component+30. Update342D88 decrements a strictly positive count and requests vtable+114 completion(1) only when it reaches zero. Initial zero/negative counts remain unchanged and return active; do not collapse them into immediate expiry. `engine/pickup_debounce.hpp`, `tools/test_pickup_debounce_native.py`, `tests/pickup_debounce_reference.cpp` compare20,002 count values and completion callback/return behavior against original execution. All pass; log `local/browser-pickups/debounce-reference.log`.

Completion slot+114 resolves342E98. It checks34FCC0 and then restore/state; value3 requests removal of this component and allocates a RestoreNode through350F60. Scheduler/removal/RestoreNode effects and exact frame ordering remain to trace before production integration. The one-second duration is now source-derived, not guessed, but adding an arbitrary one-second visibility toggle would still omit original scheduling semantics.

Builtin99 in initialization programs27/29 is not an instance-collection flag:306260..3062C8 queries global5308D0 bits6/8/7 for selectors0/1/2, returning enabled when the corresponding bit is clear (other selectors return1). Both programs request selector1. This resembles an FX/debug gate; confirm its global writer before naming it as a gameplay eligibility rule.

## RestoreNode construction verified

`engine/pickup_instance_flags.hpp` retains the authored Debounce mode0 flag mask and RestoreNode construction mask.350F60 invokes base34FB00(kind19, instance), assigns vtable491680, clears instance bits5/6 and bit1, and sets bit2. It does not directly restore the prior low flags or complete collection eligibility. `tools/test_pickup_restore_native.py` / `tests/pickup_restore_reference.cpp` compare20,000 original constructor flag transitions, base arguments, vtable assignment and returned object pointer; all pass. Log `local/browser-pickups/restore-reference.log`.

RestoreNode update slot7C is common360910; completion114 is no-op360AE0. Specialized slot10C points350288, which walks model nodes and reconstructs transformed matrices from the source hierarchy. Generic world/entity scheduling and consumption of the changed instance flags still need tracing. Do not substitute an immediate visible/collidable=true transition after60 ticks. Helpers remain isolated from production collection until lifecycle scheduling is established.

## Base destruction restores authored flags

34FBF0 detaches the component through34FC80, masks instance flags withFFFF0300, ORs the arithmetic right-shifted high half back into the live low bits, then sets bit1 before base354920 destruction. For ordinary pickup flags210004 this returns210023. `originalPickupRestoredFlags` in pickup_instance_flags.hpp preserves even the original signed high-half behavior. `tools/test_pickup_restored_flags_native.py` / `tests/pickup_restored_flags_reference.cpp` compare20,000 masks and ordered detach/base-destruction calls against original code; all pass. Log `local/browser-pickups/restored-flags-reference.log`.

Debounce completion's34FCC0 guard delegates2D19E8. That queries30A598→3A6B78 for authored handler slot4; if found,30A5C0 temporarily sets current instance+290, executes through309C88 and clears context. The five boost rows have slot4=FFFFFFFF, so the standard Debounce completion/RestoreNode path is selected rather than a custom override. RestoreNode's default update returns1 at360910; generic manager processing/removal timing still needs verification before production collection can use these pieces together.

## Component queue timing

Base construction354648 registers through354C08 at manager GP+2898. Group stride44 hex; bit0 of group flags chooses pending vs active list. Pending sentinel is group+24 (tail+28), active sentinel group+4 (tail+8). New nodes append before the sentinel, preserving insertion order. `tools/test_pickup_scheduler_queue_native.py` / `tests/pickup_scheduler_queue_reference.cpp` execute original insertion with three nodes for each flags0..15, verifying both forward/back links, order and untouched alternate list. All pass; log `local/browser-pickups/scheduler-queue-reference.log`.

Frame path26DBF0 invokes354C98(group1) before stepping and26DDC0 after its26DED8 tick loop invokes354C98 again.26DE58 then walks active group nodes through vtable+1C.354C98 drains/merges pending entries using ordering/equality virtual methods and may destroy replaced entries. Default ordering360800/equality360840 compare IDs at node+14; these are not assumed to compare instance IDs. RestoreNode uses base update3608E8→360910 returning1; Debounce uses356198 wrapper and342D88 countdown. Need finish26DED8 and pending-drain/destruction ordering for the composed pickup lifecycle before enabling it.

## Scheduler correction and actual update-loop probe

Correction:26DBF0/26DDC0/26DED8 are saved-component loading paths, not established gameplay frame ticking.26DED8 reads a type and invokes deserializing constructors. Earlier "frame path" claims for these functions are superseded. Also, active iteration uses group+8 as first node and follows node+4, ending at group+14. The earlier insertion test traversed the opposite link direction; it proves link construction, not chronological iteration order. Direct insertion places a new node first in the actual traversal direction. Pending drain354C98 subsequently orders nodes through virtual ID comparisons.

Actual update traversal is354F98: fetch head354EA8, save next354EF8 BEFORE node vtable+14 update, then visit the saved next. `tools/test_pickup_scheduler_tick_native.py` / `tests/pickup_scheduler_tick_reference.cpp` execute these original routines with real queue insertion354C08. A synthetic current-node callback unlinks itself and registers a child while group flag1 is set. Verified original visitation order, next-node survival, updated active head, deferred child, and returned head; child is not updated in that traversal. Log `local/browser-pickups/scheduler-tick-reference.log`. This is scheduler mutation behavior, not full pickup restoration timing.

Real callers of354F98 occur in generated2306A8 (e.g.230C64 onward) and244880 (244F94 onward). Trace their group1 update/flush ordering and original RestoreNode processing next. No production pickup dispatch enabled by this probe.

## Gameplay group order and flag-semantic correction

`tools/trace_component_frame.py` checks source calls and delay-slot group arguments in the normal230C54..230D18 branch. With suppression flags clear: update groups1,5,6; secondary phase355028 group1; other world callback; update groups2,3; stage/other managers; drain354C98 group1. Output `local/browser-pickups/component-frame-order.json`. This is static source order, not execution of the entire gameplay frame or proof of exact collection timing. Pause/debug branches differ.

Important terminology correction: calling instance bit2 (`4`) "restoration pending" was a hypothesis, not recovered semantics.356298's rendering path explicitly tests it at35636C before render submission. RestoreNode also supplies original model matrices via350288; it may be a persistent render/controller state. Do not infer a one-frame delay or wait for its destruction solely from that bit. The original flag values and tested constructor/destructor transforms remain valid; their broad lifecycle interpretation is still under investigation. Need composed runtime capture from debounce expiry through world rendering/contact reactivation.

## Live reference setup verified

Isolated PCSX2 v2.8.2 configuration at `local/browser-pickups/pcsx2/PCSX2/inis/PCSX2.ini`, PINE slot28022, separate caches/logs/patches and disabled memory-card slots. macOS appends `PCSX2` beneath -datapath; the first launch used the wrong inis level and was stopped, then the corrected launch loaded snow-jam-glide against the original ISO. PINE first reported paused but later running: StartPaused did not keep the loaded state paused. Do not treat this as an atomic paused snapshot or a controlled collection trace.

Read-only live checks verified SLUS-20772/CRC08fff00d, six32-byte code regions against the extracted ELF, and all five authored boost instance IDs at their expected addresses. Evidence `local/browser-pickups/live-reference-baseline.json` records the actual status. No RAM writes or collection observations. The isolated test process was stopped after verification; normal emulator configuration and disc remain unchanged. Next launch must coordinate explicit pause/frame advance or deterministic recording, then sample a controlled pickup collection and debounce expiry. Do not poll the stopped process as a live wait.

## First controlled UI/PINE pickup probe

Computer-use controls were initialized for the isolated PCSX2 slot28022 run. With PINE-confirmed pause, temporarily changed only human rider14701A0 position+110 to authored speedboost1000 origin and velocity+1E0 to zero, using guarded writes. Saved pre-write bytes in `live-placement.json`. The configured B hotkey requests frame advance; early GS startup was slow, and exact count of completed gameplay ticks was not established. Do not claim a deterministic per-frame trace from these samples.

Original instance10381E0 changed from flags210023/entity0 to flags210125/entity592A30. Live component is type1, vtable490B10, owner10381E0, subcomponent56E780; evidence `live-activated-component.json`. Rider powerup counters stayed0, so collection/reward/expiry were NOT observed. Screenshot still showed the initial race view despite the temporary actor-coordinate placement: cached pose/camera or update phase needs investigation before this is a valid collection fixture.

A settings window was opened during the run; further UI stepping was stopped to avoid interfering. The attempted guarded rollback did not pass its preconditions, so no rollback writes were performed; original bytes remain in live-placement.json. Recheck live state before further changes. This is an isolated configuration; no ISO or normal emulator configuration modifications. Next inspect the live type1 component and body/transform update stages or use a properly initialized original reset/placement path, rather than assuming position+velocity writes establish a playable teleport.

## Actual collection contact callback identified

Live component vtable490B10 slot144 targets355770 (guarded forwarding through34FE00→2D19B8), reached by rider121818 when rider+A30 selected instance is non-null. If it has an attached entity,121840..121858 calls that interface with rider+A60, rider+9E0 contact packet and rider+6C0 interface. Without entity,121864..121870 calls30A060 directly.2D19B8 delegates30A060.30A060 installs current player/instance/contact context, applies player/ghost restrictions, looks up authored slot2 via3A6B78, and runs through309C88 (or nested-context handling). At exit it calls an optional entity callback and clears context. This links selected collision contact to the verified programs28/30 rewards; it does not yet prove the browser's contact selection/lifecycle is equivalent.

Vtable method this-adjustments matter: the live component's slot7C uses-48 and slot154 uses-20. Slot154→34E698 adjusts contact surface motion/angular contribution and is NOT the reward callback. `local/browser-pickups/live-component-dispatch.json` records original vtable targets/adjustments and collection entry. Preserve adjustments in any original-code probe; do not call these subobject routines with an unadjusted entity base.

Next obtain/port selected-contact routing through rider+A30 and9E0, plus the actual entity/script gate, then compose with reward and Debounce/RestoreNode. Previous trigger-membership builtin41 is a script predicate and is not the only/direct collection route.

The slot144 target was initially hypothesized as direct2D19B8, but the ELF assertion disproved that. Actual target355770 optionally calls the attached component's methods+54/+4C, using rider identity from2D1B30, and aborts when its predicate rejects the contact. It also rejects while node+20>0. Otherwise it sets node+20 to clockRate/2 (30 at60Hz) before forwarding through34FE00→2D19B8→30A060. This separate contact cooldown must be preserved in addition to the script-created Debounce timer. Event packet/rider argument forwarding has been traced, not yet executed as a composed pickup fixture. Updated live-component-dispatch.json records the verified chain.

## Contact guard verified

`engine/pickup_contact_gate.hpp` ports355770's decision after the optional component predicate: reject if present predicate rejects, reject positive node+20 cooldown, otherwise write integer(clockRate/2) and forward. `tools/test_pickup_contact_gate_native.py` / `tests/pickup_contact_gate_reference.cpp` execute355770 for20,000 cases with absent wrapper, empty wrapper and live component, both predicate outcomes, cooldown values including0/negative/positive, and clock rates0..90. Exact cooldown, ordered component-refresh/rider-identity/predicate calls and34FE00 forwarding arguments match. Log `local/browser-pickups/contact-gate-reference.log`. The predicate itself is supplied by the fixture, not claimed recovered.

Read-only live sample `live-contact-predicate.json` confirmed paused speedboost1000 entity592A30, wrapper56E780, contained component pointer0. Therefore355770 skips the optional component predicate in that observed state. Selected-contact routing/body placement and lifecycle remain the actual integration gaps. No further RAM writes were performed in this step; production collection still disabled.

## Original reward observed live; capture timing corrected

`live-body-routing.json` showed the temporary rider placement had propagated to the body center, but the selected contact and nearby-instance list were empty at that point. Game tick address5BC508 moved only338→339 despite repeated B requests; requested hotkeys must not be counted as completed simulation frames. Using the System/Pause menu to resume then produced tick340 and rider+2E8=5,+2EC=0 in the read-only tool output: the original speedboost reward was observed. This confirms the authored amount/type in live gameplay after the spatial/contact refresh.

The subsequent polling capture (`sample_pickup_live.py`, output `live-lifecycle.json`) began too late: ticks2358..2536,181 changed samples, entity already absent and counter0. It does NOT prove expiry/restoration frame timing. The live UI tool subsequently reported ongoing user interaction; further UI manipulation was stopped. The isolated reference state remains under the user's interaction, not controlled by the agent.

Sampler now supports `--arm`: requires initial pause, waits up to a bounded deadline for running, then samples. This permits arming BEFORE a separate resume action and avoids the observed late-start error. Timeout raises rather than writing a misleading completed capture. Its rows preserve begin/end game ticks and coherent-read markers. It remains a polling tool, not deterministic replay. No further RAM changes were made this step.

## Reward effects connected to browser controllers

Added `award_boost_pickup(type,amount)` runtime endpoint using the source-tested counter addition: type1 stacks boostState.window and synchronizes physicsState.boostWindow, type2 stacks boostState.modifier. Unsupported types/nonfinite amounts fail explicitly. This endpoint is for the authored collection dispatcher; automatic trigger collection remains disabled and feedback/FX callbacks are not yet forwarded.

Found/fixed missing consumption: attached air control now sets profile.boostModifier from live modifier>0, and air animation selection receives the actual modifier scalar. Both source algorithms already implement this behavior; their algorithms/constants are unchanged. Previously the browser left both inputs at zero, so collecting a trick modifier would have had no angular effect.

`web/test-pickup-effects.mjs` (in npm test) exercises the endpoint through the real controller pipeline, confirms stacked speed counters, isolation from stored boost/trick counter, timer advancement and restart clearing. Air test ordinary peak spin4.7042971 vs trick-powerup9.4074192. Ground30-tick fixture speed20.005701 vs22.116871 m/s. Animation rate remains1 in that short sampled maneuver; do not claim that test covers every boosted animation timing branch. Full browser tests,4,800-frame native/browser parity, core build and production build passed. No automatic pickup collection, visibility/respawn lifecycle, or pickup FX completion claimed.

## Composed owner/component timer ordering

Source356198 decrements positive node+20 BEFORE the component predicate and before virtual78 component update. Zero/negative counters remain unchanged (the decrement in the branch delay slot is not stored for these cases). For Debounce the component update is342D88: decrement positive+2C; on transition1->0 invoke virtual110(reason1) and return0; otherwise return1. Thus contact cooldown30 and script debounce60 advance at distinct fields in the same owner update. Neither timer may replace the other.

`engine/pickup_timers.hpp` exposes that normal, non-removal path. `tools/test_pickup_timers_native.py` runs the full original356198 wrapper with actual342D88 and controlled owner-predicate/refresh/cleanup boundaries.20,000 cases match both timers, callback order and expiry reason, including cooldown INT32_MIN/MAX and positive/zero/negative debounce counts;2,951 completion calls occur. Predicates observe the already-decremented cooldown. This verifies composition of timer arithmetic/order, not teardown, script dispatch, respawn timing, or the optional owner-removal branch.

Type19's update3608E8 delegates to360910 returning1;354F98 ignores that return. Therefore it is not evidence of automatic next-frame deletion. Corrected the misleading pending comment on originalPickupRestorePendingFlags; the historical function name remains for compatibility. Need resolve type19 visibility/contact participation and subsequent replacement/removal before claiming automatic collection lifecycle parity. Production pickup collection remains disabled.

## Authored mode3 expiry verified; replacement is not automatic respawn

Added tools/test_pickup_completion_native.py: executes original342E98 and350F60 for20,000 cases with authored node+30=3, notification/veto outcomes and negative/nonnegative countdown values.14,857 cases follow ordered guard -> old component destruction(mode3) -> allocation(1C,flags20000000) -> base construction(group1,type19,same instance) -> vtable491680/flag transform. Guard and allocation/base attachment are controlled external boundaries; the old destructor callback uses the separately source-verified34FBF0 flag transform. Thus this is completion routing/constructor composition, not full allocator/list/world teardown execution. All cases pass (local/browser-pickups/completion-reference.log).

Added tools/probe_pickup_replacement.py. It verifies the owned ELF hash, type19 vtable slots/adjustments, and exact stub instructions: draw slot20->360790 is jr/nop; contact slot140->3609F0 is jr/nop; delegated update78->360910 returns1. Outer update10->3608E8 calls that delegate;354F98 ignores its return. No timed restoration occurs in this replacement's own update. The term RestoreNode in earlier notes is not a verified class name or evidence of respawn. Local/browser-pickups/replacement-dispatch.json preserves addresses, instructions and ELF provenance.

This resolves the immediate mode3 endpoint: a replacement with no contact-dispatch work, not re-enabled collection at timer expiry. Stage reset/other later replacement or deletion is still separate; do not claim permanence across all game events. Browser implementation should retain the consumed interaction state until an actual reset/reactivation path, rather than adding a guessed respawn timer. Remaining integration includes original selected-contact routing, executing the authored pickup reward/disable sequence, and renderer/collision instance-state propagation. Automatic browser collection remains disabled.

## Selected instance-contact ranking recovered

Added engine/pickup_contact_selection.hpp for104E70's nonempty instance-contact ranking and unique instance list. It respects collision-node priority (node descriptor flags bit0 and nonzero scalar), the sign of dot(contact-center,normal), absolute distance from the query radius, and unsigned resource-ID tie breaking. Source control flow retains its facing latch when a higher-priority node wins; the translation preserves that behavior rather than simplifying to a freshly computed tuple sort.

Added tools/test_pickup_contact_selection_native.py: executes the full original104E70 with controlled334458 result packets and the shape-radius getter, comparing the selected packet marker and unique contact list/terminator.20,000 cases with1..64 input contacts, duplicate IDs, random priority/facing/distance and forced ties pass. Original GP and normalization constant are initialized even though final normal aggregation is not compared by this test. Query geometry, final aggregate normal and selected entity surface-motion callback are outside this helper's scope. This does not yet prove real browser contact collection/dispatch.

Confirmed105398 constructs its query from rider+AA0 with32F650(mode1), then calls104E70.334458 receives t0=0 and converts it to filter1 internally. Existing broad/body geometry can supply these contacts, but actual candidate order, node metadata, unsupported-instance handling and complete output packet forwarding must be connected before claiming pickup collection fidelity. No arbitrary fixed-radius pickup sphere should replace the source body.

## Non-solid world contact data path

WorldBodyCollision now exposes raw instanceContacts for filter1, preserving point/normal/depth, resource/node and priority metadata. Native and browser asset loaders retain collision descriptor node value/flags; priority follows the recovered bit0/nonzero-float rule. Filter1 excludes terrain (334458 scans instances) and disables the physical directional triangle exclusion, consistent with32F650(mode1) query+8=0. Unsupported-instance and capacity diagnostics remain explicit. The existing best-hit API remains available; consumers of104E70 selection must use its source metric input:32F650(mode1) sets query+0=0, so32F8C0 returns-1 rather than twice the broad radius.

Loader comparison passes for3,052 instances/2,884 nodes including the added metadata. Authored pickup geometry probe passes all five boost instances, checks raw contact metadata and keeps them out of solid response. A genuinely intersecting solid terrain control is included and verified separately; filter1 excludes that terrain from both count and hit list. This probe still uses a synthetic30cm body, not a live posed gameplay run.

The source334458/334888 full contact ordering/grouping and dynamic flags still need a composed comparison, followed by posed-body dispatch, reward lifecycle and renderer visibility. Do not treat the new raw list as proven original collector parity or enable silent partial collection when a query is incomplete. The synthetic earned-boost obstacle fixture now supplies explicit zero node metadata required by the strict loader.

Validation after contact-list changes: full browser suite passes, production rebuilt, and5,520-frame native/browser comparison has zero mismatched frames. Automatic pickup dispatch remains disabled pending collector-order and posed-body integration.

## Original collector comparison with posed bodies

Added tests/pickup_world_query_reference.mm and tools/test_pickup_world_query_reference.py. They execute32F650(mode1)->334458->334888 with actual captured rider volumes from glide, jump30 and jump60, translated near each of the five authored boost instances with eight offsets each. Each source nearby list is restricted to the same instance as the native query; geometry, model hierarchy and original query code remain real.120 queries match contact counts and ordered point/normal/depth/resource/node fields exactly (99 contacts), including outside misses. No fixed-radius replacement body is used. This proves bounded per-pickup collector agreement; mixed-instance/source-neighborhood ordering, dynamic entity callbacks and live frame dispatch remain broader checks.

## Draw ownership retained

The browser spatial repack merged pickup geometry with scenery and discarded individual ownership. New web/world-batches.py retains pickup_resource on draw batches using original collision_sources triangle provenance and the verified five pickup IDs. web/prepare.py uses that helper, and main.js carries the ID into mesh.userData.pickupResource. This permits future collected-state visibility without hiding neighboring scenery. Materials, source vertices, colours and triangle winding are preserved.

tools/test_pickup_draw_batches.py proves complete course triangle multiset preservation and exact per-pickup ownership:200 triangles for each trick boost and54 for each speed boost. Optional --apply updates only packaged browser batches/indices; test-only runs are read-only. Current assets have been updated and production rebuilt. Note collision_sources provenance refers to original native index order; repacking must always start from local native indices, not from already spatially packed browser indices. Automatic collection/visibility state changes are not yet enabled.


## Live boost pickup collection enabled

web/pickup_gameplay.inc now queries the live posed BodyCollisionVolume with non-solid filter1 and the source -1 selection metric, then uses104E70 ranking. Incomplete queries produce explicit diagnostics and no reward. The original full contact list is retained for ranking, including already-consumed pickups, rather than removing hidden geometry and changing competing-contact selection.

The verified five authored records are exported by tools/export_browser_pickups.py using reward-links/runtime-bindings plus owned ELF and starting-RAM checks. Programs28/30 supply type1/type2, amount5, default60-tick Debounce and completion mode3. The captured initial entities are null, so handler2 is dispatched directly. Debounce's140 callback was additionally verified as3614F8 jr/nop; type19's is also empty. Browser therefore awards once, hides the owned draw batches immediately, advances the verified timer, and keeps the replacement non-interactive. New race resets this state; animation/physical reset alone does not resurrect pickups. This specializes the verified boost actions, not a full generic script VM or every dynamic entity predicate.

main.js updates only mesh batches carrying pickupResource. The hidden ?pickupTest=<verified resource> fixture uses the original instance position, with source-validated resource selection, for repeatable browser checks. No invented pickup radius or replacement graphic was added.

web/test-pickup-collection.mjs exercises all five through real motion/pose/world queries, verifying correct5-second counter at collection, exactly one award, hidden state, consumed replacement, no re-award when returning after physical/animation reset, and availability after a new race. Full npm suite passes. Native/browser trace now includes26 pickup fields plus five150-frame collection scenarios:6,270 frames/15 scenarios/606 fields, all exact. Production rebuilt; normal menu flow and live race rendering in the speed-pickup fixture were inspected and paused.

Limits: source scheduler creation-frame timing and broader mixed dynamic neighborhoods remain unproven beyond the existing component/collector oracles. No generic script side effects, original pickup feedback audio/particles, opponent eligibility, all collecta items or cross-course/save restoration is claimed complete. The broader goal remains active.

## Pickup feedback admission queue recovered

Source10E770/10E7D0 invoke two distinct feedback paths after awarding:29CED8 with kind0/1 and scalar1, then2A3B18 with pickup type1/2.29CED8 selects IDs70/71 (other kinds use75/76/74) and submits through its event stack; bank/sample resolution is not yet established. Do not substitute a generic sound or flash based solely on those numbers.

2A3B18 checks29F160, non-null rider, rider+470<0, current-rider lookup285D98(-1), and288AE0 suppression. It requests2B1458 on manager+5560 with key(0,20A7), args(0,12), scalar0,arg2=0,duplicate-refresh0. Only admission success packs resource010020A7 via3D8008 and invokes GP+173C (captured3D76F0) with argument1 and the pickup type. The visual/playback resource behind that event remains to be decoded.

Added engine/feedback_queue.hpp for full2B1458 admission. Ten32-byte slots retain exact field data; first empty slot wins, new entries receive180 ticks, duplicate keys reject when refresh is false, and refresh changes only the existing timer. Full queues reject new keys. tools/test_feedback_queue_native.py verifies20,000 original calls against all320 slot bytes and return values (16,809 accepted). No browser feedback playback or event expiry scheduler is connected yet; pickup rewards/visibility remain working as previously tested.

## Feedback lifetime and exact event record

Added originalFeedbackTick (2B1720) and originalFeedbackClear (2B1428). Active timers decrement with EE32-bit wrapping, then clear active when the new signed value<=0; other payload fields are retained. Inactive entries do not tick. Clear only resets active flags. The source test now verifies20,000 admission+tick cases and6,667 clears against all slot bytes, including INT32_MIN/MAX timer behavior. All pass. Runtime invocation cadence and event playback remain to be joined;180 stored ticks alone is not proof of visible duration.

Resolved packed event010020A7 through3D7110's actual eight-bank search and bank entry offset table in the glide savestate. It matches bank slot0 atB63200 (59 entries), entryB65E94/offset11412. Source state hash, header and64-byte entry prefix are captured in local/browser-pickups/feedback-event.json.3D76F0 builds an argument packet and delegates3D7418; describing this as merely a direct property setter was premature. The packed ID addresses an event record; it is not by itself a texture ID. Decode this record and its handler before making further claims about its visual/audio behavior. No new visible feedback is enabled by this pass.

## Feedback record identified as speech, not particle/HUD artwork

The exact captured event prefix was located on the owned ISO by tools/locate_pickup_feedback_event.py. Both DATA/AUDIO/SPEECH.BIG -> data/speech/char/headers.big and DATA/AUDIO/ENGLISH.BIG -> data/speech/char/langhead.big contain data/speech/char/eventdat/Events.evt. Its23,016 bytes are identical in both containers (SHA256735d7e0cd6636cde7fe7a6f57b776b81589e25f15e96de6e95ed4d32861a67a8) and byte-for-byte identical to the loaded bank atB63200. Pickup event20A7 is at offset11412. Evidence/files: feedback-disc-locations.json, feedback-bank-source.json, feedback-speech-source.json, feedback-speech-bank.bin in local/browser-pickups.

This settles the interpretation of2A3B18's branch: it selects spoken pickup feedback via the speech event system. Earlier tentative HUD/property/visual labels for this event are superseded. Callback44FF90 points2AF8A8 ->2B0F48 ->2B04D8, where the script and queued event are processed. The next audio step is decoding the event's type1/type2 branches and referenced dialogue/sample data. Visual boost particles must be traced separately; do not fabricate a visual from speech event20A7. No speech playback or new visual effect is enabled yet.

### Contact-pass timing preparation

Browser pickup timers and contact dispatch are now separate. advance_pickup_timers(tick) deduplicates logical ticks; dispatch_pickup_contacts(body) can query a supplied masked body without advancing lifetimes. The ordinary post-pose call keeps its existing timer-then-contact order. All five pickup gameplay fixtures pass, including eight extra contact passes after collection with unchanged timers/rewards. No rail-exit contact callback is installed yet. Any future pre-motion contact integration must use the same logical tick as the normal pass so Debounce is not advanced again in the collection frame.

## Pickup effects and HUD vs PS2 frames (2026-09-25)

Every pickup kind of the stage worlds (tools/export_stage_world.py handler rows over all 17 event locations and the
three peak worlds): **collecta** snowflakes (slot 1: LiveComp spin, MagnetModifier, HaloModifier; slot 2: builtin39
award, SetNodeState collectabreak, 69 halo delete, MeshAnim break pieces, Debounce DeadNode), **speedboost** (LiveComp,
UVScroll; award type 1, burst 25), **trickboost** (LiveComp, halo; award type 2, burst 25), **pointa..d** (Object
entity, UVScroll, halo, magnet; award type 6, burst 25) and **multia..d** (LiveComp, UVScroll, halo, magnet; award type 3,
burst 25). Big Challenge icons (bigc_collecticon / bigc_target / bigc_punch) only hide and play a sound.

PS2 frames (tools/ps2_capture.py, dense snaps; browser frames from a pad replay / record-seeded teleport in headless
Chrome, same ticks): runs pipe-finish 2837..2902 (BHP1 pointa), peak3/fr-throne-tuck 1541..1640 and 3382..3441
(collectibles), setpieces/race 4422..4583 (ARA1 trick boost, taken by a computer rider first), peak2/cra3-full
8201..8401 (CRA3 trick boost). Idle look (spin, halo, UV scroll, magnet flight, break pieces, speed-boost green ribbon)
already matched. Fixed:

- **Award burst position** (points, multipliers): the Debounce that replaces the magnet pickup's entity (0x355F10) keeps
  the frozen magnet matrix as its entity matrix, so the slot-2 builtin25 burst is born where the pickup reached the rider.
  The browser used the authored instance (the burst appeared at the lamp post in pipe-finish). `stage_world.inc`
  `stageFrozenMagnetMatrix` / `stage_entity_matrix`; PS2 savestates `local/ps2-capture/runs/pickups/bhp1-burst.tick{1089,
  1096,2331,2341}.p2s` -> `bhp1-pickup-burst.snapshots.json`, gated word-exact in test-stage-world (BHP1 case).
- **HUD** (web/trick-hud.js, web/free-ride-hud.js, web/ui.js): 'Collect +$ n' (slot 0x31, 0x1EF624, descriptor 1, needs flags
  0x100000 and the pre-pass mode +0x88 = 3), the Conquer the Mountain cash popups 0x2E (point pickups) / 0x2F (tricks)
  (0x1EF924: the 0x23 popup as money from descriptor 0x12 to the cash 0x38, flags 0x4000000 | 0x200); gated against the
  original draws (tools/probe_trick_hud.py cases with the free-ride flags, test-trick-hud 385/385). The counter pulses
  and turns green 0x4C8648 from the 0x31 ratio (0x1EF630 f29 -> 21F9B0: pulse below 0.44, green up to 0.44) and the cash
  turns green 0x4C8528 and pulses while the award flash (slot 0x18) lives (0x1EF850). In a career race the pre-pass
  (0x1EBB00) shows the snowflake counter (0x80) and hides the place/standings while the popup lives (no PS2 frame yet).
- **Trick boost visuals** (web/boost_gameplay.inc, web/boost-renderer.js): the air streamers RFX+0xAD0 (0x2EF6D0 /
  0x2EF950: nose/tail ribbons relative to 0.7 x the root, ring 25 (18 without a device) advanced on even frames, shrink by 2
  on the ground, `strm` clamp alpha 0.3 width 15 normally, `prbn` repeat alpha 1 width 37.5 grey 0.5 with rider+0x2EC > 0)
  and the power-up aura RFX+0x9C0 (0x2EADD0 / 0x2EB198: the last three board outlines 140 x 36 cm and caps at +/-12 Z,
  `psmr`, only with the trick boost in the air). Both are ALPHA enum 7 (0x48) additive; no random draws. Textures FX 61..63
  (tools/export_fx_textures.py). test-pickup-fx.

Sounds were already right (29CED8 kinds 0..5 -> 0x74/0x71/0x75/0x76/0x70, web/sfx-game.js). Since 2026-09-26
([presentation.md](presentation.md) 4, 5): the trick boost's extra rider light (392D90: (2,2,2) along +Z in the RFX+0xD30
list, bank units unchanged; PS2 gain +19.7/+14.7/+16.9 vs browser +18.5/+14.1/+16.0) for every rider, and the streamers /
aura of the computer riders (web/opponent-fx.js).

## Collected snowflakes: break pose and staying removed (2026-09-26)

User report: "the collectables are visually bugged when you pick them up, and then don't disappear from the map". PS2
reference: `tools/ps2_capture.py` snaps of runs/peak2/fr-dbc2 (DBC2 free ride, collecta 1012 at 8466 and 1013 at 8632,
every 3 ticks), runs/peak3/fr-throne-tuck 1541..1640 and runs/setpieces-ass1/full 801..1001 (ASS1 multic multiplier);
frames and kept savestates in `local/ps2-capture/runs/pickups/` (`dbc2-collect.tick*.p2s` ->
`local/reference/set-piece-particles/dbc2-collect.snapshots.json`: the MeshAnim of the break, all nodes). On the PS2 the
snowflake flies to the rider (magnet), the counter / cash turn green with "COLLECT +$ n", the snowflake is replaced by the
11 collectabreak pieces that start where the spinning, bobbing snowflake is drawn and fly apart (+1000 cm/s up plus
random, gravity 0.1, life 2 s, end mode 2), the halo is deleted, and the instance is a DeadNode for good. The PS2
screenshot of a snap shows the frame of about 2..3 ticks earlier (exact replays: the browser's burst / break appear 2..3
ticks before the PS2 frame of the same tick).

- **Break pose** (`web/stage_world.inc` `stage_section_livecomp_nodes`): builtin13 (key13 = the collectible, key14 = node 0)
  builds the pieces on the source LiveComp's node matrix. The collectible's LiveComp is section-started and drawn by a
  JS player (web/livecomp-animation.js: one node, spin 0..359 deg and a bob of up to ~70 cm per second), so the core had
  only the bind pose: the pieces popped up to ~0.7 m down and to another rotation at the collect. The section start now
  records its builtin3 words and tick (`stageSectionLiveComps`), and the MeshAnim rebuilds that player (0x341AA0 with the
  section's drawn word, on the magnet matrix) and advances it to the current tick like the entity pass: the pieces start
  on the snowflake as drawn. No draws, no shared state; the old bind pose remains the fallback (no section start, or a
  later section leave). DBC2 check: the piece centroid starts 87 cm above the rider root (PS2 95, before 56).
- **Staying removed** (`web/stage_script_gameplay.inc` `stage_collectible_award`): 30B9A0 -> 30C3E0 -> 153B00 sets the
  career stats bit at the award, and a location's next stage setup (30C4A8 -> 1538E8) makes that collectible a DeadNode.
  The core's copy of the career row (`set_stage_collect_row` from web/stage-collect.js at the run start) was not updated,
  so a streamed location that unloaded (rows 5 -> 7 -> 0: `browser_stage_track_reset` drops its instance states) and read
  again (`browser_stage_track_setup` -> builtin38) restored the collected snowflake. The award now sets the bit in the
  slot track's row (event worlds: the course mask) too; the career save (web/career.js, the CTM side) is unchanged.
  `web/test-collect-restream.mjs` (hub A: mdl_A_collecta_0003 collected, A unloaded, evicted and read again).
- Multipliers (ASS1 multic, exact pad replay): the X icon with its halo flies in, "X5" under the combo, the orange builtin25
  burst at the rider: matches. Side-by-side sheets (PS2 | browser): `local/ps2-capture/runs/pickups/compare/`. Point pickups and boosts: see
  the section above.
