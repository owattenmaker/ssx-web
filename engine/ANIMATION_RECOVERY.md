# Native rider pose recovery

`animation_motion.cpp` samples the original compressed animation packets directly,
converts their spherical coordinates, and composes original source-space bone
transforms. It contains no console instruction dispatch or runtime. The older
`animation-samples.json` path remains a visual preview; integer-frame interpolation
is not used by the new conformance path.

## Reproduce the current Zoe comparison

```sh
python3 tools/rider_assets.py --rider zoe
python3 tools/animation_bank.py --source ps2
python3 tools/export_animation_samples.py --rider zoe --source ps2
python3 tools/probe_rider_pose.py local/reference/pcsx2/snow-jam-glide.p2s \
  --output local/native-qa/zoe-glide-pose.json
build/metal-engine/ssx3_animation_audit local/assets/native/RIDER_ZOE \
  local/native-qa/zoe-glide-pose.json > local/native-qa/zoe-glide-pose-comparison.json
python3 tools/test_animation_native.py
```

The probe's expected bones are comparison outputs, never gameplay pose inputs.
Native generation uses original rig/AFB data, initial animation sequence state,
physical position/quaternion, body scale and recovered board/leg contact parameters.
Across seven captured neutral/turn/brake/charged-jump states, all27 local
translations and quaternions are bit-identical. At neutral glide, board alignment
plus the two-link leg solve reduces
maximum world-bone position error from 3.43 cm to approximately 0.022 cm. Twenty-three
of 27 positions are bit-identical; all27 local transforms match exactly. This is a measured recovery checkpoint, not a
claim that animation state transitions or all procedural poses are complete.
Four neutral checkpoints reach a maximum world-position error of0.125cm; turn
and charged-jump root presentation is still outside the implemented world-pose
subset. `python3 tools/test_pose_snapshots.py` regenerates the seven-state report.

## Original data and field map

The human is Zoe (character 4). Geometry is `*(rider+0x780)`, animation player is
`*(rider+0x784)`. Geometry+0x0C points to 0x58-byte model-part records; each has file
ID+0, compiled bone base+4, LOD0 list+0x1C, original bone records+0x38, bone count+0x44.
Geometry+0x24/+0x28 hold local position/quaternion arrays, each float4 per bone.
Geometry+0x2C holds 32-byte world transforms (position then quaternion).
Geometry+0x140 contains the three source body scale factors.

Live LOD0 model headers identify TopB, BindingsA, BoardFlexA, BottomB, HeadA, HandsB,
BootsA and Mop. Imported Zoe has 3,058 vertices, 2,954 triangles and 27 bones. The
inactive original eye part adds two compiled bones between board and hair; the
probe maps by file ID and bone index rather than assuming all compiled indices
match the rendered rig. Body indices0..21 and board22/23 match directly. TopB uses
separate `suit` and `eatf` materials, now preserved as separate draw batches.
The mesh selections are verified; texture color variants are still explicit test
choices. Career equipment selection, morphs and hair simulation remain unfinished.

Animation+0x50 points to six 8-byte sequencers (count, linked-list head). Sequence
next is +0xC8. Its first three playback slots have stride0x1C: packed bank/index+4,
time+8, rate+0xC, duration+0x10, weight+0x14, enabled+0x18. Packed ID low8 bits is
bank; remaining bits are animation index. Sequence+0 is a semantic state ID,
**not** a bank index. Sequence+0x84 is priority, +0x88 is a 64-bit bone mask.
`0x314418` emits the model's descriptors; `0x313D70` computes fade weights.

The glide descriptors are:

| Track | Packed clip | Original name | Time | Priority/mask |
| --- | --- | --- | --- | --- |
| 1 | 0x1D00 | HEADCHECK_TS | 0.833332777 s | 2 / 0x870 |
| 2 | 0x2300 | RNORM_FWD_CYC | 1.716665268 s | 1 / all |
| 2 secondary | 0x2400 | RNORM_TURN_HS_1_CYC | 1.283163905 s | weight zero |
| 3 | 0x1E000 | SH_RIGHT_CYC | 0.042636059 s | 0 / secondary model parts |

Head-check replaces neck/head/clavicle bones4/5/6/11. SH_RIGHT is a secondary hair
animation containing part19 among other hair files; it does not animate the body.
The source30 Hz segment lookup is0x311318; scalar sampler0x312C20; quaternion
conversion0x30FC7C..30FD9C (also0x30EFE0 in the single-bone path).

## Procedural world pose

`0x310200` scales each local translation and composes quaternion/position with its
parent, retaining float operation order. The normal first root comes from physical
rider+0x110/+0x120 via `0x11EB98`. The +0x130/+0x140 fields are an optional second
root used only when+0x150 is nonzero; they are not an unconditional render transform.
Original11FA10, airborne134DD0 and conditional+2C8 presentation are now recovered; their dynamic control/sequence coverage is recorded below.

`rider_pose_motion.cpp` recovers board correction from0x11ED80..0x11F148: +0x2BC
alignment weight, +0x390 desired board-plane normal, +0x31C lift, +0x370 contact
normal; rebuild board child after adjustment. +0x390 is a separately filtered
board normal, retained through this tick's pose and updated afterward at13F2E4
as normalize(old390 +0.5*current370). It is not the temporary force leaning normal. Body+0x318 gates leg IK. Bindings at+0x8E0 and+0x910
contain thigh/shin/foot indices, board-relative foot translation and quaternion.
`0x11F3D8` solves those two links. Full-weight IK is implemented; partial quaternion
blending currently throws rather than silently supplying an invented pose.

## Proof and remaining gaps

The development-only original-code harness verifies all 2,759 packets / 219,561
scalar values, 10,000 spherical quaternion cases, and 10,000 FK cases bit-identically.
It links original generated functions only into a separate test binary. Scalar
SQRT.S is corrected in an opcode-checked temporary oracle copy to match PCSX2's
forced nearest rounding; VU operations retain toward-zero. Source-generated AOT
agreement alone is insufficient for PCSX2 fidelity, so captured world/local arrays
are also compared. EE scalar ADD.S/SUB.S use one alignment guard bit; VU zero-subtraction
preserves different zero signs from unary negation. These distinctions closed
the local-pose differences. The optional player feeds both renderer and collision from one native world pose.
The original post-frame ground snapshot can have a late390 normal update newer
than its recorded pose; rerunning11EB98 is idempotent but uses that newer input.
Dynamic clock/state validation below preserves the actual call order. Partial IK
weights, mirrored layered playback, random head-check selection and secondary
motion still need broader original-execution coverage.

## Lean/pivot and dynamic ground pose

Original11FA10 is now a native root-presentation helper, verified against20,000
whole original stages. It cancels lean against twice the brake magnitude, rotates
around the board-local pivot, applies roll, then offsets along rider+3B0 by
`45cm * turn * bodyScale`. The additional+2C8 lift in11EB98 applies to the body root
only; the independent board root keeps the unlifted presentation. This reduces the
captured turn-left30 maximum world-pose error from70.875cm to0.015625cm and charged
jump30 from3.59cm to0.078125cm, with all27 local transforms still exact.

The+2C8/+2CC/+2D0 triplet is now preserved as `presentationLift`. Its original
13E144..13E228 target responds to the old/new position projected onto the previous
filtered board normal, clamped according to crouch and multiplied by body scale; animation
classes10/4/5 or semantic22 suppress it. Twenty thousand original target stages
match exactly. The current value advances through original1211F8 with other control
triplets. Original+31C board lift is a separate cosine oscillator, **not** the+758
contact compression. `ground_pose_motion.cpp` recovers its phase/amplitude update
and the independent31C040 cosine polynomial;20,000 phase/lift cases are exact.

Normal-ground turning now uses original104178/103E28 five-way cycle selection:
HS2, HS1, forward, TS1, TS2. Input is the separate+1FC animation-turn filter, not
physical turn+1F0. Switching the primary clip preserves normalized phase; the
secondary clock is synchronized to the primary after advancing. Twenty thousand
whole original selection/weight/clock cases match. The native player uses this
stateful update rather than keeping the two initial cycle clips fixed. The cycle
asset map is extracted by original clip hashes. Brake and crouch transitions now use original semantic selection, three-way
blend drivers and sequence fade state. Airborne coverage is described below.

Passive departure keeps the rider in ground mode through the final generated-body
query, then initializes the original airborne predictor after final clamping. This
matches13F194/13F264/13F2B0 ordering; a regression checks that the final ground-pose
callback runs before the mode switch.

## Airborne generated pose and runtime sequence work

`test_pose_snapshots.py` covers13 original checkpoints, including reversed stance. All27 local transforms
match in every state. At jump31/60/90 and isolated-air60/120, all27 world positions
and quaternions also match bit-for-bit. The airborne root calls134DD0 with the
current animated local bone selected by rider89C (Zoe0), multiplied by body scale,
then11FA10 and the separate body lift. Even zero angular inputs retain the
original pivot add/subtract operations and their float rounding.

The body provider exposes the generated board-root22 position, pre-lift root frame,
animated air pivot and main animation metadata for landing/collision consumers.
Repeated pose reads use the pure presentation helper; the motion controller owns
mutable axis-blend advancement. Neutral268 A_INTO_AIR,287 A_CYC_1/A_CYC_2 and305
A_OUTOF_AIR assets/state definitions are now available. The prediction-driven
landing-animation branch134B80..134C3C matches20,000 original stages. The coupled
accepted-input replays match all24 body/board positions/quaternions and main
sequence clocks/rates/fades at jump31/60/90, including ordinary ground takeoff. Angular/prewind/grab clip selection remains unsupported.

Sequence completion runs after local sampling:11EB60 calls3123C0/312598 then312490.
Descriptor+8 chooses103918 callbacks. Kind1 removes IntoAir and creates287 while
preserving its weight/fade (312B18); kind3 similarly returns5; kind4 clears end63
and requests5 with ordinary crossfade (312BD0). The sampled local/world pose is
cached for the tick: landing/impact does not rebuild geometry in13AA48.

Reverse stance preserves per-sequence root+60/+70 and mirror flag+80, with the
MNF bone source mapping and signed quaternion/translation selectors. All27 local
transforms match the reversed long-charge240 snapshot. Existing sequences rotate
through311B48/311BF0 at115168; new sequences use the separately published root
and mirror defaults. Landing61/63/64/65/66/67 and switch21 assets are available.
Landing62 has three weighted variants and remains unsupported until original RNG
selection is supplied. Original duration uses(u16 frame_count−1)*float32(1/30)
with toward-zero multiplication, not ordinary division by30; exported duration
values are checked against every captured sequence in the snapshot suite.

Dynamic ground pose tests resolve the earlier post-IK discrepancy: old390 must be
used before its late normal-filter update. Turn30, brake30 and charged-jump30 now
produce all ten collision centers exactly. The post-frame snapshot-only audit
still reports its phase-dependent residual instead of rewriting captured inputs.
At glide120, a missing new randomized HEADCHECK_HS event creates a separate maximum
3.8877cm upper-body discrepancy; lower-body centers are exact. That event selection,
secondary hair dynamics and broader state coverage remain active work.

## Current running checkpoint

`tools/test_air_pose_replay.py` passes five coupled replays: an airborne31 baseline
advanced to60/90, and the original accepted ground input sequence advanced through
charge/release to31/60/90. All24 body/board world positions and quaternions, plus
main sequence IDs, clip IDs, clocks, rates, weights and fade times match exactly.
The report is `local/native-qa/air-pose-replay/results.json`. Secondary hair is
explicitly outside this running comparison.

The long charged jump now lands and retains generated body poses for every one of
240 simulated frames. In the current run, landing61 lasts through202, callback4
selects5 at203, and board alignment returns to1 by240. The original endpoint is
reversed stance/semantic21 while native remains forward/5; physical endpoint errors
are3.149465912m position and2.406189407m/s velocity. No post-landing body contacts
occurred. This remains a physical landing/orientation/reversal discrepancy, not a
claim of complete landing parity. Results: `local/native-qa/zoe-landing-240-latest.json`.

The headless scene tool accepts `--animation-state output.json` to report native
sequence state and cached world transforms alongside ordinary telemetry. Repeated
reads do not advance the clock or rebuild geometry after a same-tick collision.

## Opponent assemblies, reduced bone masks and staged updates

The Snow Jam glide reference has five original opponent appearances: Psymon,
Allegra (resource prefix `arielle`), Moby, Griff (`grommet`) and Luther. The last
uses gameplay character record4 but uniquely matches `luther` model headers;
visual costume identity is preserved separately from gameplay stats.
`audit_rider_assemblies.py` matches all55 active LOD0 resources across six riders
against the owned PS2 archive. Only three loader pointer fields56..67 are excluded;
the source words must be zero and the live values valid EE pointers. All165 active
bind-bone position/quaternion records are identical in the GC models and live PS2
rigs. Higher-detail meshes are retained; exact equipped texture color variants
remain unverified, with that status written into each package.

The original local sampler intersects geometry+150 with the sequence mask. When
rider+B1C is nonzero,30F4F4 also intersects geometry+158. This reduced-bone mask
explains Psymon's antenna remaining in bind pose at the initial checkpoint. Native
initialization remaps those original compiled-bone bits to the imported rig's
indices, excluding inactive eye bones. All138 opponent local transforms now match
exactly; measured post-frame world-position residuals range0.015625–0.046875cm.
`test_opponent_poses.py` also feeds only native-generated world centers into the
production `bodyVolumeFromBones` builder and compares the resulting shape data
against `reference_pair_collision`: all five opponents match ten sphere counts,
ordered bone IDs, individual radii, active masks and broad radii bit-for-bit.

`OriginalRiderAnimation` exposes the original phase boundary:
`prepareLocalPose` advances/samples and dispatches completion (121700/11EB60),
then `finishWorldPose` or `finishBodyPose` performs world FK/IK (121728/11EB98).
The old `poseAt` and `bodyProvider` remain wrappers. Repeated phase calls do not
advance clocks, and same-tick physical collision changes do not rebuild cached
geometry. `native_animation_stages` tests those boundaries and completion ordering.

Driverkind7 at104358 writes slot0 time to
`max(abs(prewindSpin),abs(prewindFlip)) * slot0.duration`, then advances only the
sequence fade. It does not use elapsed clip time or playback rate. All20,000 direct
original driver cases match. Full12EE30 directional selection and43D788's five-style
table also match20,000 original cases across both stances; all23 prewind definitions
245..267 are exported. Control selection consumes filtered currents before1211F8;
local animation preparation consumes the approached currents afterward. Native
opponent initialization must seed those mutable prewind triplets through rider
state rather than substitute a generic playback loop.

`native_animation_stages` additionally checks the two real NPC prewind starts:
Psymon/Moby glide scalar state is seeded, native target update and approach run,
then driver7 produces the glide+1 clip-time float words `0x3ea5d968` and
`0x3ea100e2`. This verifies the player wiring as well as the isolated driver.
The new NPC packages carry `animation-start.json` calibration inputs; the shared
rider initializer must also supply the mutable prewind state through
`original_air_entry`. Exact equipped texture variants and nonneutral airborne
clip selection remain separate gaps.

### Requested channels, soft reactions and charged release

Animator offsets0..14 contain the six requested semantics. Original312AA0 and
311AE8 read those slots;311B20 and312AE8 inspect the first playback sequence.
These differ during fades. The native player now seeds `current_semantics`,
updates them on play/clear/completion, and exposes immediate `playSemantic`,
`fadeChannel`, `setChannelRate` and `consumeRiderEvents` without invalidating the
already sampled local/world pose. All437 valid authored metadata rows are in the
six Zoe/NPC manifests. A metadata-only row cannot silently choose a clip.

Standalone108388 soft collision selection matches20,000 full original cases;
control3/12E778 target and transition requests match another20,000. The controller
uses primary sequence+C0 completion, not an invented clip timer. External106848,
115640 stance restoration and rail1326C8 remain explicit caller responsibilities.
Clips55..60 retain class6 through the recovery tick, then normal control0 selects
its ordinary cycle. Existing classifier/history/RNG regression remains passing.

`upper_reaction.{hpp,cpp}` recovers115D48 at normal-controller131870, after turn
and crouch targets/115B58 but before main-animation selection. All30,000 complete
original cases match timer bits, shared RNG words/draw counts, selected semantic
and bone mask, and all six peer cooldown timestamps. The second percentage test
is always true but consumes a real RNG draw. The cooldown uses1298C8's race logic
tick (338->339 in consecutive glide snapshots), so600 means600logic ticks, not
milliseconds. Source35C idle time separately increments byfloat1/60. Actual shared
world firstframe now also matches the complete shared RNG state (parent report).
Headcheck316/317 have verified single-leaf mappings;319..321 still require original
random weighted variant selection before their playback can be claimed complete.

The charged-release table43D840 uses the same12EE30 angular classifier as prewind.
All20,000 original five-style release selections match. The133128 playback-rate
formula also matches20,000 full original entries through sequence+90. Native
release clips269..286 and completion2 replacements289..296 are exported; class9
and an unextended phase1 cycle are supported. Grab/phase2 and further directional
selection remain explicit boundaries, not neutral-pose substitutes.

### Ordinary grab lifecycle and authored markers

`animation_events` reproduces the complete primary3135B0 event path, with
313868 interval boundaries,313938 seek-time equality and3139A8 completion.
30,000 whole-original cases match float clocks plus B0latched/B8new flags,
C0completion and C4seek state, including reverse and multiple wraps. Authored
AFB event ordinal determines its bit;311F00 uses the first16-bit event word as
frame time multiplied byfloat1/30. The secondword does not select the flag.
All source clips now export `event_times`, and fresh/seeded native sequences
preserve seek and raised-flag state. Driver7 marks a seek while setting time.

`grab_lifecycle` implements ordinary1352A8 states0/1/2/5.20,000 whole-original
cases match state/index, ordered main/upper play and fade requests, main/upper
rates, scoring begin/end requests and the resulting class18..20 active flag.
The test also executes original120038; playback speed is
`1 + grabStat * 0.29988324642181396` with EE scalar arithmetic. All15 normal
semantic mappings71..85 and upper-cycle86..91 are exported from their actual
single lookup leaves. A grab begins through marker0, holds at marker1 with
main sequence rate0, resumes on input release, then resets through marker3.
These are sequential checks, so multiple state changes can occur in one tick.

The caller invokes this lifecycle before angular control, applies animation
requests immediately, and supplies its returned active flag to the recovered
angular grab gates. It also owns119708/1197D8 and10E098 scoring/boost effects;
ordered requests are verified here, but their game effects are not claimed by
this helper. Tweak/Uber states3/4 remain explicitly unsupported. Completion5
uses104B48/312BD0 to fade into287 for the six verified Snow Jam animator+64=0
starts; the alternate436 request is represented and fails if its clip is absent.

### Weighted animation lookup

Standalone `animation_variant` recovers311710.30,000 complete original cases
match chosen leaf and RNG draw count. A singleton bypasses eligibility and
weight and consumes no draw. Multiple authored variants consume exactly one
shared draw, even when only one passes the mask. Weight is a uint32 integer,
not a float. Eligibility requires `(allowedFlags & requiredFlags)==requiredFlags`.
After `draw % sum`, original subtracts each eligible weight then selects when
the signed remainder is <=0; this inclusive boundary is preserved, including a
zero-weight first choice when the draw is0. The filter input comes from104CF8:
normal animator+60 rider+364, or alternate animator+64 actor+CD8.

`reference_animation_variants` extracts the authored leaf/weight/flags and packed
clip mapping separately from runtime selection. Leaf519 is the original no-play
sentinel:3128E8 returns438 without changing the requested channel. Empty ranges
are explicitly invalid for selection. Integration must invoke the shared RNG
before creating/fading the selected sequence, and must not cache a random choice
as a fixed per-semantic clip.


Browser follow-up (2026-09-13): no resampling at contact does not mean geometry
stays untranslated.121750/310530 commits106538's accumulated rider+9D0 translation
to cached bones and skin matrices after contact. Browser now implements that late
translation, along with original0.85 pose/bind scale and pre-motion trajectory
inputs for air animation selection. See latest CAMERA_RECOVERY.md for measured
head/physics errors and validation scope. Native shared-world cached geometry
translation should be audited separately; browser changes do not certify it.

### Browser playback-rate selection during crossfades

BrowserAnimationGraph::setRate used to change every sequence on a channel.
Original grab control135480/135488 and135554/13555C obtains311B20 and writes
only that sequence's+90 rate.311B20 calls314760 with list index0; it returns
the first playback sequence, even when its channel is fading. The native
player already used that selection. Browser setRate now stops after the
first matching sequence, leaving outgoing crossfade sequence rates intact.

The focused tools/test_browser_channel_fade.py regression now covers a held
primary clip with an advancing outgoing clip, reverse/resume writes, other
channels and missing channels, alongside the channel-fade identity checks.
This is a host binding correction, not a change to clip timing arithmetic.

Validation: full npm suite, production build and original jump-input audit
pass after the rate-selection correction. Rebuilt Sam/Zoe native/WASM traces
each pass23 scenarios/9,630 frames with zero mismatches. Logs are
local/browser-validation/channel-rate-*.log. Source gameplay scheduling and
all original crossfade images remain beyond this regression's scope.

### Completion dispatch now uses a fixed batch

Original312490 traverses six channel lists, collects flagged63 nodes at
312500..312510, then dispatches that fixed set at312540..312568. The native
player (and generated browser graph) previously rescanned the mutating vector
after callback replacements, which could discover newly entered completions
or lose pending work whose flags changed.

`originalAnimationCompletionBatch` now captures channel/list order before
callbacks and uses transient host tokens to find surviving sequences after
vector insertion/erasure. New entries have token0 and wait until the next pass.
Flag cancellation after collection does not cancel an already queued callback.
Destroyed nodes are skipped rather than dereferenced; original interactions
where a callback destroys a different pending node remain unverified.

`tools/test_animation_completion_dispatch.py` executes original312490 with
controlled getters/callbacks on63 channel masks. Each callback test clears a
later pending flag and inserts a newly flagged node. Original order retains
the previously collected callback and excludes the new node. The focused
browser graph regression repeats that mutation/reallocation pattern using the
shared batch helper. This establishes dispatch collection/order, not all
completion callback bodies or possible destruction interactions.

Validation after completion batching: full npm suite, production build,
recorded jump-input/head audit and rebuilt native/WASM Sam/Zoe traces pass.
Each rider covers23 scenarios/9,630 frames with zero mismatches. Logs:
local/browser-validation/completion-batch-*.log and
completion-dispatch-oracle.log. Full original frame fidelity remains open.

### Browser posed skeleton vs continuous ARMSX2 captures (2026-09-22)

Ground truth is `tools/ps2_capture.py` (record layout in each `.capture.json`); the
hook now also logs the local pose (geometry+24/+28, record 5440/5952), animator+0..0x80
(6464), up to six sequence nodes of the six channel lists (6592, `[channel,address,0xD0 bytes]`,
count at 7888), motion owner+0x200..0x300 with the air-control state (7892) and the
shared RNG words 0x4FF030 (8896). `local/pose-audit/audit.mjs` (compare-ps2-capture.mjs
plus `funcs.js`) reports per-tick world/local bone error, sequence lists, ground-state
field diffs (`ground_state_dump`) and air-control diffs; `AUDIT_SYNC_RNG=1` copies the
recorded RNG words into the web generator before each tick (diagnostic only).

Before: the first compared tick (339) was 1-10 cm off on every body bone (hands 9-10 cm,
board 9 cm) and 9-20 cm on hair. Root causes, each fixed against original order:

1. The browser never seeded the glide sequence lists; `reset_animation` started a fresh
   semantic 5. It now seeds `original_animation.layers`/`current_semantics` exactly like
   `makeOriginalRiderAnimation` (HEADCHECK_TS, cycle phase/secondary slot, SH_RIGHT).
2. World FK ran relative to the presented root and was composed afterwards (3-6 ulp at
   1e5 cm). 11EB98 runs FK from the physical root (control5: 134DD0 first) with world-space
   lateral/contact vectors; the renderer's relative poses are derived afterwards.
3. Ground selection (131870/104178) consumed post-1211F8/13D818 turn, brake, crouch,
   velocity, lateral and post-1200D0 boost; it now reads the step-start snapshot.
4. Selection also ran after 13D818, whose lift target (13E144) and alignment target read
   the animation class, so jump presses/landings lagged one tick. `step_rider` now calls
   `browser_ground_controller_animation` after its targets/reverse check (115D48 upper
   reaction first, then selection; RNG order restored; brake/crouch targets written back).
5. Prewind (12E9B8) selected from approached currents; selection now uses the retained
   currents and 1211F8 approaches the prewind pair once per tick (also while fading).
6. Control2 release ran the first control5 step (grab, adjust spin, air selection) in the
   release tick; the original starts it next tick. Passive departure: the ground
   controller already ran, the next tick requests control4 (12F730 entry), the tick after
   is the first passive step, and a passive->5 transition does not also step air control.
7. 139A70 copies the retained presentation up (+180, previous board-root up) into +370
   before air motion; the airborne board lift uses it. Ground-motion departure ticks use
   the ground normal and pre-13F2E4 board normal. Air keeps the last ground +3B0 lateral.
8. Crash: no 134DD0 root; 106538 crash-contact displacement is committed to the cached
   bones (310530) like body/landing translation.
9. rider.json bind translations are metres of original float centimetres; `float*100`
   rounded forearm/hand/hair translations by one ulp. Scale in double.
10. New sequences took priority 1; 3128E8 uses table 0x48D808 = {3,2,1,0,0,0} by channel
    (`originalChannelPriority`), e.g. a new HEADCHECK must override the body at priority 2.
11. Secondary motion 120378 (`engine/secondary_motion.hpp`): every tick the first sequence
    of channels 3..5 gets rate clamp(|v-surfaceV|/1666.67, 0.5 floor, 2 cap) from
    pre-motion velocity; when 1298C8 % 6 == rider+86C each enabled channel picks one of
    seven SH_* semantics (still <=138.9 cm/s, up/down |z|>888.9, side |x|>416.7, else
    fore/aft) from the wind in cached bone +8A8 times +950/+960. Profile exported by
    `tools/reference_secondary_motion.py` into `original_animation.secondary_motion`.

After (all 27 bones, positions and quaternions bit-identical on every physics-exact tick):
carve 339-407, jump-tricks 339-664, air-tricks 339-721, neutral-3000 339-1148. Remaining:
- Shared RNG: the original generator is also drawn by the computer riders (~2 draws/tick
  here), so random variants (landing62 at neutral 1149, crash 351/352 at air-tricks 722,
  bob, head checks) diverge. With `AUDIT_SYNC_RNG` neutral-3000 stays within one ulp for
  all 1300 ticks and the air-tricks grab-landing crash is exact through 760.
- Passive departure: `publish_motion` rebuilds +3A0/+3B0 in air, so the departure tick's
  integrated lateral is lost; the pose uses the previous tick's (1.1 cm while turning,
  passive-inputs 626+). Fix belongs in core.cpp (keep the ground basis while airborne).
- carve 408: pre-contact spheres are exact; the platform response translation differs
  (collision, not pose). Rail prewind (rail_gameplay.inc) still approaches before selecting.

### Crash entry/recovery, departure windows and hair in the event race (2026-09-22)

Capture-driven (event-race, 16 KiB records with sequences and the 0x4FF030 RNG words):

- **RNG order.** The computer riders draw between the human's controller-phase draws and its
  motion-phase draws: the 899 landing-crash variant (13A14C, 351/352/353 by `word & 3`) is the
  tick's third draw, the 1472 get-up reaction variant (314) the first. `compare-ps2-capture.mjs
  --sync-rng` now defers `(PS2 draws - browser draws)` to the first draw outside a `ControllerDraws`
  scope (ground controller 131620, crash control, air controller block); per-tick browser counts come
  from child passes. With the old start-of-tick sync the browser used an opponent word and picked 352.
  The 10EB30 entry itself (11FA10 root, 30ECD8 bake of the new clip's preview root) was already exact.
- **Motion 2 rider fields.** Crash motion writes only +370/+3D0 (137D18 sliding contact, 137860 air body
  landing); +380 and the +3A0/+3B0 tangents keep their entry values until the exit's 13C7A8/ground
  motion. The actor's ground normal is rider+370 at entry (landing contact normal / air +180 copy), not
  (0,0,1). 10EB30 clears only the 1F0/208/250 triplets and +330; 11FE78(2) then runs the old motion's
  exit (ground: 13F410 re-arms 208/2BC/2C8 decays and stamps owner+14); brake, crouch, +1FC, lift and
  board alignment keep decaying through the crash ticks. `311B48` rotates roots by -angle (crash
  prewind-style 3/4 compensation now matches the rail path).
- **Control-0 entry 131608** zeroes the 115D48 idle clock +35C (browser: end of tick on a report of control 0; its +360 board-press latch clear is not wired here). 115D48 is not run in start,
  soft (control 3), crash-exit or landing ticks; upper_reaction's `Round` now uses `OriginalRounding`
  (fesetround is inert in WASM, so idle accumulated with nearest rounding).
- **Reaction requests 10E028/115B58.** rider+358 (kind) / +354 (due logic tick). Control-8 exit 12E690
  requests kind 4 on even logic ticks (the +6C0 voice query is assumed true); 115B58 (131868, before
  115D48) plays it on channel 1 with the +8C8 mask when main class is not 5/10 and channel 1 is free,
  and drops it after 180 ticks. Kinds 1-3 -> 315, 4 -> 314, 5/6 -> 318 if |+1FC| < 0.75 (table 0x456CC0).
  Other requesters (10E098 kind 5, 1200D0 kind 6, 10E910 kinds 1-3 race position, 12C678) are not wired.
- **Slow crouch 22.** 131620 selects 22 below 833.3 cm/s from the command crouch axis f22 (1 while jump
  is held), not the jump charge.
- **Departure pose windows.** Passive departure tick: control 0 already ran, so 1211F8 advances +1FC
  (the cycle blend) that tick; the next (control 0 in air requesting 4) does not. Control-2 entry 12E980
  sets +204 = 0 and +200 = 1/30 before that tick's 1211F8 (the fading cycle keeps blending toward 0).
  event-race body bones 0..23 are now exact through 1653 (windows 231-234, 409-421, 819-831, 1472+ gone).
- **Hair.** A `//` comment had disabled 120378 and the grab score tick; restored. The still semantic
  (0x19B+7c) does not need the cached bone, so the event's first tick (logic tick 0) requests 411 as the
  original does; the start push-off velocity feeds 120378 (it runs after the controller). Event hair
  (world bones 24..26 = compiled 26..28) is exact 19..448. Remaining: charged-release ticks (449, 864;
  jump-tricks 374) where 120378's wind is ~0.056 cm/s below the pre-takeoff speed (1e-6 local drift
  until the next 6-tick request).

### Left-stick air adjust: in-flight stance switch, fading adjust clips (2026-09-22)

Report: turning in the air with the left stick (not a D-pad spin) looked less dynamic than on PS2.
Six new ARMSX2 captures (`local/ps2-capture/scripts/air-steer-{lr,fb,diag,passive,tuck,event}.json`,
charged and passive jumps with the stick held left/right/up/down, diagonals, partial magnitudes, a
circling stick, tuck-held control 4 and a 155-tick passive flight; PS2 screenshots via `--snap`)
showed three differences. The +0x28C/+0x298 triplets the board-press agent fixed were already exact.

1. **In-flight stance switch was never ported.** In control 5, `0x133308` calls `0x135BE0` in phase 3
   (before the D-pad input, which a switch skips) and in an unfinished phase 2, when no grab is
   active and the channel-2 request is not 288. `0x114DB8` needs predictor status 1/3; it compares
   the last pose's board-root frame (+0x170 forward, +0x180 up) with the landing normal/heading
   (predictor +0x20/+0x10, or rider +0x180/+0x1E0 on surface 18 or table+0x44): no switch when
   up·n < 0, forward·velocity > 277.78, forward·n > 0.9 or projected forward·projected heading
   > *(0x4A0EA8) = -0.0. The air-adjust spin (+0x28, up to 100 degrees) is what turns the frame
   past 90 degrees. On a switch it moves +0x110 by rotate(q, 2·(scaled pivot xy, 0)), runs 11E098,
   0x115168 (toggle +0x320, 311B48(pi), root sincos(-pi/2 | -0), mirror, negate +0x3A0/+0x3B0 and
   turn/animTurn/lean/brake/+0x280) and 0x1135B8 (new flight, which drops status to 0 for a few
   ticks, pausing the landing alignment). 135BE0 then wraps total spin by pi into [-pi,pi),
   negates the four flip fields and plays 288 (clip 0x7500, 0.3 s, completion kind 1 -> 287);
   while 288 is requested the air selector keeps the adjust clips off. Port: `engine/air_switch.hpp`;
   `OriginalAirControlProfile::airSwitch` hooks the two call sites and `landingAnimation` now means
   "channel-2 request is 288". Oracles: `tools/test_air_switch_native.py` (60,000 whole 135BE0/114DB8
   cases with the recompiled 11E098/115168/31BE50: 17,744 switches, both frames), and
   `tools/test_air_control_native.py` now scripts 135BE0 to check both call sites and the phase-3 skip.
   Browser: the original runs the controller before air motion but the browser translates first,
   so core.cpp keeps the pre-motion state (`translate_air_motion`, prediction snapshot) and a
   switch restarts the flight from it and translates again (`browser_air_switch_redo`).
2. **Adjust clips froze when they faded out on landing.** The kind-11 driver (1043F8) keeps running
   on the fading 297..304 sequence, reading +0x28C/+0x298 as the landing's zero targets approach
   at 1/60 per tick; the graph read a stale animation-side copy. `graph.advance` now uses
   the rider triplets.
3. **Landing clips are not forced.** 13A968/13A8F8 call 3128E8 with a2 = 0, so 311F00 revives a
   still-fading copy of the same landing clip (time/weight kept); the browser forced a restart.
   Also the passive control-0-in-air tick (requests control 4) now approaches +0x1FC like 1211F8,
   which the kind-4 cycle blend reads (tuck-held passive departures).

Evidence (BONE_SCAN, 24 body/board bones bit-identical): air-steer-lr and -diag every tick
(338..843, ..1098), air-steer-tuck every tick (..1057), air-steer-event through 527 (528 crash
landing), air-steer-passive through 712 (713/715-717 sub-ulp local rotation in the landing blend,
1145 crash landing), air-steer-fb through 424. Physics exact: lr/diag/tuck all ticks, event 527,
passive 1144, fb 461. mix-glide's body bones now match every tick (was 855), air-tricks and
neutral-3000 every tick; `test-ps2-captures.mjs` gates these with `bonesThrough`.
Remaining: fb 425 and boardpress-railjump 764 are isolated one-ulp x offsets of the air root
(all bones, one tick, local pose exact); fb 462 is a steeply pitched (flip-adjust) landing where
the browser's air body contact fires one tick early (body-query area, not animation). The QA page
start is not tick-aligned with the countdown savestate, so PS2 screenshots compare only by eye
(same mid-air arms-out pose after the switch).

### passive-inputs 693/731: pose exact, core sphere mask (2026-09-22)

With the current tree the passive departure while the L1 punch
(attack channel) runs is pose-exact: local pose (LOCAL_TRACE) and all world bones including hair are
bit-identical 339-730, and the 731 bodies match the 16 KiB re-capture `passive-inputs16` sphere for sphere
(SPHERE_TRACE). The 731 airborne collision was not a pose error: 139C88 masks the body to spheres 0/1
(word 0x4A1120 = 3) around 13AA48 and the browser queried all ten; see
[obstacle collision](../docs/obstacle-collision.md#hipsrail-contact-106f78-and-the-core-sphere-mask-2026-09-22).
passive-inputs is exact through its end (808). The same mask also covers the air-steer-fb 462
"air body contact one tick early" noted above: air-steer-fb is now exact end to end.

## 2026-09-23: pending rate, controller wind and prewind branches

- **Pending next rate** (animator +0x1C, setter 0x3158E0):
  - `AnimationGraph::nextRate`, with `enter(semantic, rate=-1)`, uses the pending rate. 311F00 gives every new sequence this rate.
  - 311A50 placement and `reset_animation` set it to 1.
  - The air selector sets it through `setNextRate` and plays with the default. The air switch passes it on.
  - The finish reaction plays at 0.75 inside a transient 12C678 window, then the rate goes back to 1.
  - The release rate is -1 when there is no spin or flip.
- **Secondary motion (hair, 120378).** 1211F8 reads the post-controller velocity (+0x1E0) and stance (+0x320). The takeoff velocity is published after each takeoff (charged, passive, rail jumps, handplant) through `browser_controller_takeoff_wind`. Crash control and reset steps publish the crash actor velocity and the rider velocity.
- **Prewind controller 12E9B8.** Branch 1 is class 10 and branch 2 is semantic 21. When +0x328 == 0 and +0x2DC == 0, branch 3 runs the 114CC0 reverse turn, which plays 21 and flips the stance. The index-21 turn/brake targets are skipped while the prewind is held. Held-air frames also run the crouch/brake, prewind and turn targets.
- **Crash from air control** (control 5 only; not held air or passive) zeroes the prewind triplets (134CB0). Crashes from control 3/4 keep the 12F620 rates (tech-oob-hops 3368).
- **Open:**
  - handplant-spring 665: the local pose at the handspring landing differs even though sequences and fields match.
  - score-uber 739: the hair after the reset placement needs the 11D660 -> 11EB98 pose before 1211F8.
