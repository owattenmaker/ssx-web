# Original movement recovery

This map records verified original PS2 USA code and live Snow Jam observations. The current controller uses recovered ground/air/jump equations when an authoritative profile is loaded. ARA1 now loads its owned calibration profile by default. World-body collision and complete event/control orchestration remain active recovery work; this is not a claim that all gameplay matches.

## Motion dispatch

`owner = *(rider + 0x77C)` and `mode = *(owner + 0xDE0)`. The human in the saved Snow Jam fixtures has rider address `0x014701A0` and owner `0x0146F390`. Dispatcher `0x111408` reads the six-entry jump table at `0x456B50`:

| Mode | Target | Receiver | Verified role |
|---|---|---|---|
| 0 | `0x13D818` | owner | Normal cruise; clean glide fixture |
| 1 | `0x139A20` | owner + `0x20` | Airborne; jump/isolated fixtures |
| 2 | `0x136E98` | owner + `0x30` | Wipeout; RECOVER fixture at frame 600 |
| 3 | `0x136958` | owner + `0xA0` | Active at race-ready gate; full state meaning unfinished |
| 4 | `0x13AF28` | owner + `0xB0` | Not yet classified here |
| 5 | `0x1391A8` | owner + `0x110` | Not yet classified here |

`0x137D18` is **wipeout motion**, not ordinary grounded riding. The named `cWipeoutMotion_gainFocus` at `0x136C40`, dispatcher branch and live mode 2 jointly establish its scope. `0x136E98` calls `0x136F30`, then selects `0x137D18` when its local state at +0 is zero, otherwise `0x137750`. Its companion stage `0x136EE0` selects `0x138640` or `0x137860`. Wipeout's fixed 1800cm/s² gravity and special drag must not be copied into normal cruise.

## Physical fields

| Rider offset | Meaning/evidence |
|---|---|
| `+0x110` | Position, original Z-up float centimeters |
| `+0x120` | Physical quaternion, original XYZW |
| `+0x1E0` | Velocity in cm/s; consecutive clean glide displacement agrees with old velocity/60 within 0.014cm per component |
| `+0x2E4` | Cruise speed limit read before force assembly at `0x13D89C..0x13D8C4` |
| `+0x300` | Frame-time multiplier; 1.0 in ready/glide/wipeout fixtures; multiplied by float(1/60) |
| `+0x308` | Contact/sticking branch flag; full semantic name unresolved |
| `+0x370` | Contact normal; used for relative velocity and position projection |
| `+0x3A0` | Forward surface tangent; dot(relative velocity) is 1871.76cm/s in clean glide |
| `+0x3B0` | Lateral surface tangent; dot(relative velocity) is 1.47cm/s in clean glide |
| `+0x3D0` | Contact surface velocity subtracted from physical velocity; zero for static ground fixtures |
| `+0x438` | Surface parameter record index; 0 in supplied clean glide |
| `+0x454` | Signed contact distance used by normal response, in cm |
| `+0x758` | Nonnegative contact compression ratio written by `0x13C878` |

Surface records have stride `0xB0`, reached through `*(*(global + 0x84) + 0x44)`, with global from `*(gp - 0x848)` and `gp=0x4A30F0`. In clean glide the selected record is `0x00BAC490`. Record+0 supplies cruise gravity 1300.850341796875cm/s². Record+0x14/+0x18 are powder-depth parameters and +0x1C is damping 5.005756855010986. Controller+4/+8 approach the scaled depth targets at a maximum 100cm/s; clean-glide values are 0.4249999821cm and 2.1285459995cm. These observations do not yet justify treating every imported terrain patch as this surface.

## Cruise call and integration map

`0x13D818` calls `0x125970` first, applies the rider speed limit, computes `dt = rider[0x300] * float(1/60)` at `0x13D8E0..0x13D8F0`, and projects `velocity - surfaceVelocity` onto normal/forward/lateral axes.

| Call | Arguments identified | Role/scope |
|---|---|---|
| `0x13C878` at `0x13DA38` | Controller, surface record, contact distance, normal relative speed | Piecewise normal acceleration and compression ratio |
| `0x13C948` at `0x13DA48` | Controller, surface record | Additional forward term; input/animation/stat dependencies remain |
| `0x13CCF0` at `0x13DA64` | Controller, surface record, forward relative speed, normalized normal term | Forward friction/control term; calls player/stat helpers |
| `0x13D028` at `0x13DA80` | Controller, surface record, lateral speed, forward speed, negative rider+0x1F0 | Lateral term with speed curve/stat dependencies |
| `0x13D1B8` at `0x13EBA0` | Controller and contact-result data | Collision/contact update path; complete decoding pending |
| `0x11E098` at `0x13F11C` | Rider | Rebuild physical orientation matrix |

The assembled source acceleration includes explicit vertical `(0,0,-surfaceGravity)` at `0x13DCD0..0x13DCE4` and terms along all three surface axes. Position corrections and contact clipping precede integration. The integration itself is **position first using old velocity** at `0x13E0A8..0x13E0DC`, followed by `velocity += acceleration*dt` at `0x13E0E8..0x13E120`. This translation path is implemented in `ground_motion.cpp` and used by the seeded/default-profile runtime branch. Profiles are required; the old approximation remains only as an explicitly unseeded fallback.

The normal-response routine `0x13C878` is implemented, directly compared with original code, and wired into recovered ground contact. Let distance be `d`, outward normal speed `vn`, smoothed depths `h1/h3`, surface gravity `G`, and damping `D`:

- Above the surface (`d>0`): `a = G*(d*float(-0.03333299979567528))`; subtract `D*vn` only when `vn>0`.
- Between surface and first depth (`-h1<d<=0`): `a = (-G*d)/h1 - D*vn`.
- Deeper: clamp `d=max(-h3,d)` and use `a = G*(1-2*(d+h1)/(h3-h1)) - D*vn`.
- Write compression `max(-d/h3,0)` to rider+0x758.

The implementation preserves operation order and original float rounding. Authoritative surface values, contact distances and depth/stat scaling accompany the runtime profile; these are not guessed tuning constants.

## Recovered airborne integration now running natively

Airborne updater `0x139A20` calls trajectory updater `0x113648`, which advances `0x1139A0` in 60Hz ticks. `air_motion.cpp` implements this equation with source Z-up float centimeters retained throughout flight:

1. `p' = p + v*dt`, with separate float multiply/add rounding.
2. Source horizontal `vx/vy` each add their previous value times drag.
3. Source vertical `vz` adds the rising increment when `vz>0`, otherwise the falling increment.
4. Cap the updated speed to trajectory+0xA8 when exceeded.

Exact constant bits: dt=`0x3C888889`, drag=`0xBB5A740F`, rising increment=`0xC162AAAB`, falling increment=`0xC1FD5556`. They correspond to 1/60s, horizontal drag 0.2/s, upward gravity 8.5m/s² and downward gravity 19m/s². The captured trajectory cap is 3333.33349609375cm/s.

Every float add/multiply uses round toward zero. The native helper scopes `FE_TOWARDZERO`, restores the caller's mode immediately, and compiles with `-frounding-math -ffp-contract=off`. It advances once per two controller half-steps, scheduling both ground and air physics on the original 60Hz clock. Recovered jump takeoff occurs before the first airborne integration; original body/world contact integration is still being completed.

`air_motion_tests.cpp` embeds numerical golden states and their original EE-memory hashes. All three position and three velocity float bit patterns match falling checkpoints after 29/59 steps, rising/apex checkpoints after 60/120 steps, and one captured over-limit speed-cap step. Tests also check rounding restoration and full controller cadence. The joined reference suite reports zero position/velocity error for those isolated-air scenarios. Additional cap boundaries, collision interactions and complete jump initiation remain separate validation work.

## Ground runtime implemented and tested

`ground_motion.{hpp,cpp}` now implements:

- `0x13C878`: normal acceleration/compression.
- `0x13CCF0`: longitudinal friction/braking, including surface-specific depth factors, stance and stat factors.
- `0x13C948`: forward drive/auto-boost, heading alignment, crouch and animation-state effects.
- `0x13D028`: lateral response and speed curve.
- `0x113E80`, `0x113F88`, repeated `0x1211F8` triplet updates: turn and mutually exclusive crouch/brake targets, rates and smoothing.
- `0x31C228` and `0x31BE50`: original atan and sin/cos arithmetic used by steering.
- The cruise translation prefix through `0x13E120`: depth smoothing, force assembly, normal-force rotation, gravity, penetration correction, low-speed braking, position-first integration.
- `0x13EBF4..0x13ED24`: post-contact velocity correction. For surface IDs other than 2/3/13, add `normal * (-0.4 * dot(relativeVelocity,normal))`, clamp the added source-Z component to at least −40cm/s, preserve the original relative speed, then add surface velocity.
- `0x11B3F8`: dynamic maximum speed from the 48-float table at `0x4A6310`, normalized top-speed stat, surface factor, charge/boost and stance. It interpolates standing/crouched min/max-stat limits, converts km/h to cm/s, then uses 0.9 retention when increasing and 0.97 when decreasing. The runtime updates this at frame begin.
- `0x13F358..0x13F3A8`: the second cruise-phase final speed clamp. This is distinct from the initial clamp before force assembly.
- Passive takeoff: no contact, or `flags308 != 0` and contact distance above surface+0x10, requests negative-charge takeoff in the second phase. The original takeoff helper runs at the end of this ground tick; airborne integration starts on the next tick.

`orientation_motion.cpp` supplies independently tested full heading, quaternion rotation, normalization/matrix rebuild and ground alignment. Heading runs before the new terrain query; velocity correction and alignment follow the query. Rebuild ordering matches the source's calls inside `0x11DFE0` and final unconditional `0x11E098`.

`sourceGroundContact` now uses original source-float probe generation, authored collision flags, 9×9 cell/triangle order, coarse-fraction ranking, prior-normal preference and four Newton refinements. A persistent per-rider `terrainContactCache` retains the original patch/cell/triangle selection state. The query center includes `priorLateral * 45cm * filteredTurn * bodyScale`; both signed distance and planar clearance use that center rather than bare rider position. `RayHit.contactSignedDistanceCm` and `contactLateralDistanceCm` carry the exact source-float results. Rider+0x380 is the previous +0x370 copied before querying, including failed queries.

### Direct-original conformance

`python3 engine/test_original_ground.py` compiles development-only original routines and compares 12,000 randomized/boundary cases per helper and per translation/velocity/speed-limit stage. Results match float bit patterns, not a loose tolerance. External stat getters are fixture inputs to the isolated force tests; the runtime profile resolves their actual values from the owned snapshot. The Snow Jam wind scalar is 0, so the original wind pre-step exits without modification; nonzero wind is not included in this recovered runtime path yet.

The oracle has one opcode-guarded correction in a temporary copy: original `SQRT.S` at `0x31BEEC`, opcode `0x46050044`, reads Ft=f5. The earlier PS2Recomp output incorrectly read Fs=f0. Native sin/cos uses the actual original operand. Shared generated reference files are not patched.

The conformance run also writes 32 private golden translation cases under `local/native-qa/ground-golden.bin`; the `native_original_ground` CTest checks all nine position/velocity/depth/distance floats exactly and verifies passive-launch tick timing. Without those private fixtures it reports a CTest skip and instructs regeneration, rather than claiming a pass. Re-run the conformance script after public profile/state layout changes.

### Profile and runtime binding

`OriginalGroundProfile` contains the surface parameters, original tuning curves, geometry scale, resolved stat values, dynamic speed-limit table, alignment/heading profile and air-height threshold. `OriginalGroundState` contains source-float position/velocity/contact axes/physical orientation, control triplets, contact depths and original state flags. `seedOriginalGround(profile,state)` activates recovered ground motion. The replay initializer accepts these under `native.initial.original_ground`, plus optional original contact cache data.

The data-only extractor is `tools/reference_ground_profile.py`. Stat normalization is original integer progress-byte division by 5 followed by float division by the character maximum. In the supplied Zoe baseline the result is nearest(1/11), not 1 or 0.5. Getter `0x1493D8` uses progress byte+1/max byte+9; `0x148D80`/`0x148E68` use+3/+11; dynamic top-speed getter `0x1494C0` uses +0/+8. The special player flag can return 0.5.

The ordinary app and R respawn now read optional per-area `riding-start.json` through the same native data initializer as replay. ARA1's file is exported by `tools/export_riding_start.py` and records its source/reference provenance. It initializes the recovered physics without loading an emulator or guest memory at runtime. The native Zoe rig now matches the reference character; equipment color selection and the complete roster/customization system remain separate work.

### Measured progress and remaining scope

After trigger geometry classification, the 120-frame neutral glide had 2.36m position error and 5.26m/s velocity error with provisional ground movement. Recovered translation/contact/heading/dynamic-limit/final-clamp behavior reduces these to approximately 0.000646m and 0.012121m/s. This is a measured improvement, not an exact-ground-gameplay conformance claim. Remaining collision-phase direction changes are being recovered from `0x13F488`; accepted input/control timing is being checked with live traces. Other courses/surface transitions need their authoritative profiles, and nonzero wind, full events, scoring, roster, audio and progression are not complete.


### Scalar arithmetic and generated rider pose

EE scalar ADD.S/SUB.S preserve one alignment guard bit, as PCSX2's unconditional
`FPU_CORRECT_ADD_SUB` path does. Scalar DIV.S and SQRT.S use nearest rounding;
VU operations retain their separate arithmetic policy. `original_float.hpp`
provides those scalar helpers and `tools/original_fp_oracle.py` applies opcode-
checked corrections only to temporary development oracle copies. Ordinary IEEE
roundtowardzero arithmetic alone did not reproduce the game's local bone poses.

The generated Zoe pose and procedural collision-sphere work is described in
[ANIMATION_RECOVERY.md](ANIMATION_RECOVERY.md). All27 local transforms match twelve live ground/air states bit-for-bit. Five air
states additionally match all27 world positions/quaternions exactly. Dynamic
turn30, brake30 and charged-jump30 runs match all ten generated collision centers.

`OriginalGroundDiagnostics.leaningNormal` is a temporary force direction, distinct
from rider+390. `boardNormalForPose` retains old390 through body/board pose queries.
Only afterward does13F2E4..13F354 write
`normalize(old390 + 0.5 * current370)` for the next tick. This ordering explains
snapshot-reconstruction shin residuals and is covered by a live one-frame golden.
