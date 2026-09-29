# Original computer-controlled riders

This investigation uses PS2 USA SSX3 snapshots and the owned executable. The
runtime will use native controller functions and authored path data. Captured
commands, poses or opponent trajectories are not a substitute for native AI.

## Identity and initialization data

Snow Jam has six participants. In `snow-jam-glide.p2s` the human is character4
at014701A0. The five computer riders are:

| Slot | Rider address | Character ID | Body scale | Initial AI path | Control |
|---|---|---:|---:|---:|---:|
| 1 |018D0C40|8|0.919999957|0|2|
| 2 |018E1270|2|0.829999924|1|0|
| 3 |018F1D50|0|0.939999938|3|2|
| 4 |01902830|5|0.699999928|4|0|
| 5 |01913310|4|1|5|0|

Their common actor layout contains position110, quaternion120, velocity1E0,
rig780, animation784 and motion owner77C. Common mode/control fields at ownerDE0
andDE4 are valid. Actor interface6C0 is458660, versus human4583A8. Every NPC has
riderType434=0, also the human value; that field cannot identify a human.

Ground contact cache864 is kind2 for both humans and NPCs. NPC body cache868 is
kind0, versus human kind1. Consequently world-body queries must use the coarse
NPC path; reusing the human detailed query changes collision selection.

`tools/reference_npc.py` exports the validated common ground profile/state,
identity, caches, behavior delegate and path state. It has been run on ready,
glide and long-charge240 snapshots. Unknown NPC extension words are labelled
as diagnostic data and are not yet a native initialization contract.

## Provider and behavior layers

`owner+DE8` points to4585F0. Its input method is10A768, with this-adjust0;
serialization methods are10A898 and10A8E8. They serialize the common owner and
then a separate150-byte NPC extension atDF0..F40. The human recorder/context
layout does **not** apply: NPC DF8 is a float parameter, not a recorder pointer.

10A768 clears an8-byte output command, invokes10DEF0 pacing logic and120090,
reads the current control and dispatches through table456A30:

| Control | Producer |
|---:|---|
|0|10A960|
|1|10AD78|
|2|10AA70|
|4|10B590|
|5|10B250|
|6|10B790|
|7|10AED8|
|8|10B750|
|3,9,10,11,12,13|zero command|

When the current control is not5 it clears ownerE34 afterward. These command
producers use the existing original control-specific packed layouts. No call to
26D178/26D2B0 occurs in this provider or its10xxxx routines. Existing NPC
snapshots therefore do not expose the human's accepted-command RLE history.
Recording their actual command output requires a separate verified diagnostic
hook; treating DF8 as a recorder is invalid.

The ground producer10A960 first invokes10D1A0 for path/recovery state, adds
interaction decisions10DBF0/10DA10, then dispatches a C++ member-function
descriptor atF44: signed16 this-adjust, signed16 virtual index and uint32 target.
It is not a command cache. Ready uses100680 for all five NPCs. Glide uses1009E0
for slots1/3/4 and100F88 for slots2/5; long240 uses1009E0 for all five.

Those behaviors call path/event functions10B980,10BB18,10BBF8,10BD10 and
control writers10BFA8/10C0A8/10C140. The actual route/decision driver, recovery,
jump timing and shared RNG consumption still need native implementation;
constant steering, frozen endpoint words or following a captured trajectory
would not reproduce this system.

## Authored routes and start markers

Global4D33A0 contains14 marker records at+4,129 AI paths at+8/+C and eight
course-progress paths at+10/+14. AI paths have64-byte runtime headers and
interface481478. Their first56 bytes share the path geometry/event layout;
+38 is authored flags, not the course path's remaining-distance float, and+3C
is a separate authored field. NPC riderAB8 selects its AI path, ABC its path
cache, and4C0/4C4 hold previous/current path distance. AB4/AC0 independently
track course/race progress.

The extractor verifies all129 path origins, bounds, direction/length segments,
flags and event extents against the owned ARA1 AIP. The observed raw-event to
runtime mappings are100→12,102→14,103→15,110→16,111→17,300→20 andFFFFFFFF→0.
These IDs' action meanings must still be recovered from their consumers.

The14 runtime markers have40-byte records. Position/direction match the raw
AIP region's six floats; their final pointers resolve the raw track and AI-path
indices. Runtime flags can differ from raw marker flags. The first six describe
the lanes, but ready actor positions already differ slightly from their marker
origins. The real initializer/countdown motion must supply those differences.

Existing native26A638 projection and26AB20 sampling can be reused as arithmetic
primitives. NPC route selection is112A50 (26AFB8 candidate filtering,26A428,
26AC48,26AB20, then ABC cache and4C0/4C4 update), separate from track selection
1127F0. Its route flag filtering and state transitions cannot be replaced by the
course-progress selector.

## Native leaves and validation

`npc_input.cpp` now provides the staged `originalNpcControl` runtime facade.
It implements the complete ground Cruise100680, Jump1009E0, Peer100F88 and Designated100B90
behaviors, steering, zone queries, recovery requests, attack/defense decisions,
pacing and controls0/1/2/3/4/5/6/8/9/10/11/12/13. Other controller/behavior branches
throw explicitly until recovered. The source speed threshold is float0x44505557;
above it crouch magnitude uses0x3F7851EC before signed-six-bit packing.

`tools/test_npc_input_native.py` compares20,000 original cases per leaf,
5,000 complete manual commands and cruise/jump behaviors, and1,000 complete peer/designated behaviors,
including125 in-provider route changes. `test_npc_provider_native.py` executes
the complete original10A768 against live glide andglide+1 contexts for allfive
NPCs: commands, pacing, typed mutable state and shared RNG are identical.
No gameplay stubs are used in that complete-provider oracle; unexpected guest
calls or stderr fail the test.

The shared world scheduler must run each original phase over **all** riders
before the next phase: controls/smoothing, motion, local animation, world pose,
contact motion, then later events. The original128AF0 loops are not equivalent
to completing one rider's entire frame before advancing the next. Global317810
random state must likewise be shared and consumed in the original order.

## Verified interaction motivating full opponents

Long-charge240 retains an original rider-pair collision with slot3 at tick522:
both human[3] and opponent3[0] records store522 at actor+slot*24+10. Source
107BCC..107BD4 writes those fields after107E70 impulses. This occurs33 ticks
after landing at489. The native single-rider endpoint differs while retained
flight prediction and impact speed are exact; that endpoint cannot establish
landing error while the recorded opponent interaction is absent. The pair
collision functions are being recovered separately, and full opponents remain
part of the completion requirement.

## Native route selection and progress

`npc_path.cpp` now implements complete26AFB8 candidate ordering,10D410 route
scoring,112A50 human/NPC selection and1125C0 progress. The candidate oracle passes
20,000 cases. The full score oracle passes20,000 cases including7,233 positive
scores and3,074 correctly ordered shared-RNG draws. A combined2,000-case
selector/progress oracle passes with1,615 path switches, checking the current
path, route points, cache, previous/current distances, heading and RNG calls.

The score context is explicit shared-world data: roleE00, allowFlag0E04,
randomizeE08, current/followed path indices and the current AI paths of all NPCs.
It includes route affinity, authored speed thresholds, geometric proximity,
occupied-route penalty, followed-target priority and the source random branch.
Zero-speed scoring remains an explicit exceptional-float boundary rather than
a guessed finite fallback; ordinary moving cases are conformed.

121818 invokes course progress112338 and then AI progress1125C0. The latter
copies4C4 to4C0, projects with ABC's horizontal cache, computes796cm lookahead,
copies it to4B0 and optionally changes routes. Route changes happen near a path
end, after crossing event18/20, or every60 ticks when farther than500cm from the
path. Heading4CC is computed after the possible change. Input production reads
these retained previous-phase values. The application scheduler must preserve
this order across participants.

## Shared routes and pacing

`reference_npc.py` exports `participant_routes` for allsix riders, including the
human. Route selection112A50 uses NPC scores only for computer riders; the
human branch minimizes the sum of two geometric squared distances. Human
route progression still runs1125C0. NPC occupancy scoring excludes humans,
but designated-peer/followed-path lookup includes every participant.

Pacing10DEF0 uses actor4D0 current course remaining for self and the first
enabled human pair record in slot order. It does not use the designated peer.
The signed event variant comes from535C11. Route progress1125C0 instead tests
actor4D4 best remaining after course progress. Time scale approaches the target
by the original120090 increment and is published before control handling.

## Airborne decisions

`npc_air.cpp` implements135CB0/135DB0 rotation time estimates,10B0E8 stop
planning, full10CAD8 grab selection and10B250 control5 output. The chooser
uses normal/tweak/uber mappings, usage counters, capability, boost tier,
stat-dependent timing and shared RNG. `reference_npc_air.py` extracts the
actual150xxx mappings and312820 marker1/2 times. These are authored AFL markers,
not inferred clip durations. Every current grab semantic has one mapping
variant; an unexpected multi-variant semantic fails extraction because the
original marker getter could consume RNG.

`test_npc_provider_native.py --air` verifies allfive riders' mappings/markers
against original150xxx/312820. It also compares10,000 cases per rotation,
stop-plan, complete chooser, passive-air4, active-air5 and start6 command updates, including actual grab selections, all mutable plan/count state and shared RNG.

The first shared-world frame's two missing RNG advances were traced to
115D48 upper-body peer reactions, not NPC input. Executing that entire original
function for normal-control actors gives draws0/0/2/0 for slots0/2/4/5. Slot4's
empty upper channel permits timer35C to advance from1.4999988 to1.51666545,
then115E4C and115FA4 consume two draws. The resulting six-word random state
exactly matches glide+1. `glide-upper-reaction.txt` retains this proof. A fixed
per-frame RNG skip would be incorrect: active upper animations reset35C and
return without those draws. The native animation agent owns that lifecycle.

## Remaining producer boundary

Control7 (10AED8) remains explicitly unsupported. Its no-jump-zone path copies
an uninitialized stack output to NPC E20 because10B980 does not populate output
flags on a miss. That retained byte pattern must not be invented or injected as
console stack state. The rail/control transition must first establish whether
this inactive value is overwritten before any gameplay read, as happens for
several other source fields. Rail physics and lifecycle remain separate work.
The exported semantic context includes physicalRightZ, prewindStyle and current
main animation class for the remaining native control work.

## Browser integration (2026-09-22)

The provider now drives computer riders in the browser (docs/ai-racers.md). Control 7 is implemented
(`originalNpcRailCommand`, 0x10AED8): the no-jump-zone branch copies the unwritten 10B980 stack word to
E20, and every captured rail sequence (three --ai-state captures) shows E20 = 0, so the port stores 0;
the oracle `tools/test_npc_input_native.py` seeds that word with 0 and matches 5000 cases. The 10B250
grab-release branch (remaining <= E2C + 0.3) also clears E24. Zero-speed path scores follow EE
saturation (div by 0 -> +-FLT_MAX, 0 x MAX = 0). Leaf arithmetic uses engine/npc_float.hpp so the WASM
build takes the software toward-zero path.
