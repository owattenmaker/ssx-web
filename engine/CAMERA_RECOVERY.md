# Original chase camera recovery (DEFAULT_3)

Native translation of the SSX3 PS2 (`SLUS_207.72`, SHA-1 `77114dfd…`) automatic
in-game chase camera. Files:

- `engine/original_camera.hpp` — header-only, C++20, `namespace ssx`, no AppKit.
- `engine/original_camera_tests.cpp` — assert-based test executable.
- `local/reference/original_camera_oracle.hpp` (local-only, not in the repo) — raw words from five PCSX2 savestate pairs
  (`local/reference/pcsx2/snow-jam-{glide,glide-1,jump-30,jump-31,turn-left-29,turn-left-30,charge-1,charge-2,charge-3}.p2s`): human rider fields, the live
  DEFAULT_3 object and the outer camera at frames N and N+1.

Everything is computed in source units (centimetres, Z up) and converted for the
renderer with `nativeCameraView` = `(x, z, -y) * 0.01` metres / Y up.

## Scope

Reproduced per 60 Hz tick:

| Stage | Address | Role |
|---|---|---|
| Driver | `0x176E10` (vtable slot 0x28) | calls the 15 base helpers with DEFAULT_3's gp constants |
| Step 1 | `0x162568` | velocity acquisition, vertical-dependent lag filter, scratch context |
| Step 2 | `0x162998` | mode-5 fade counter (`+0x2C0`) |
| Step 3 | `0x162A20` | airborne latch, wall-launch arm, 15-frame take-off countdown |
| Step 4 | `0x162B80` | look-at = head bone position |
| Step 5 | `0x162C78` | filtered travel direction with 5-tap PID-adapted coefficient |
| Step 6 | `0x163010` | follow distance from speed/boost, eye placement |
| B1 | `0x163270` | filtered eye vertical offset (speed drop, boost, crouch) |
| B2 | `0x1633B0` | FOV filter |
| B3 | `0x163450` | mode-5 hold/blend and extra eye height |
| B4 | `0x162B90` | look-at height |
| B5 | `0x1635F8` | jump camera: two natural cubic spline pairs (phase A/B), landing settle |
| C1 | `0x164878` | wall-launch swing-around (clamped 9-knot ease spline), return blend, 60 % guard |
| C2 | `0x1646A0` | mode-4 pull-behind filter |
| C3 | `0x1641C0` | rider+0x5AC proximity push (300 fwd / 100 up / 600 side, 0.98) |
| C4 | `0x1643A8` | slow-speed lock state machine (15 km/h, 1.5 s / 1.5 s) |
| Finish | `0x166228` → `0x166640`, `0x1662A0`, `0x166530`, `0x166F90` | angles, pitch lag filter with angle limit, snap guard, output eye |
| Snap guard | `0x168150` | look-at/eye reset when \|lookat−eye\| < 20 or > 1e20, sets `+0x2F0` |
| Set-target | `0x176FE0` → `0x166C60`, `0x166550` | field reset, update+finish, offset direction, update |
| Compositor | `0x15E668` | single algorithm at weight 1 → exact copy; fov/near/far clamps |
| Collision | `0x15EE00` | vertical ±halfLen probe, accumulated lift (×0.97 decay), eye re-projection |
| Shake | `0x15E460`, `0x15E360`, `0x1656B0`, `0x165938` | boost/speed-scaled random-walk look-at displacement |
| Spline | `0x31D5E8/0x31D660/0x31D700/0x31D738/0x31D7E0/0x31D8B0/0x31D5A8/0x31DEE0` | `cCSICubicSpline` |
| Math | `0x31BE50` sincos, `0x31C128` asin, `0x31C228` atan | reused from `collision_scalar.hpp` (asin added) |

### Out of scope (documented, not implemented)

- Algorithm blending between several camera objects (`0x161BB8` node weights, smoothstep fades):
  only DEFAULT_3 at weight 1.0 exists in normal play, and the compositor copy is then exact.
- SPOKE (`0x177E50`, entered by `0x161AB0` on motion mode 5 and left above 35 km/h),
  POST_RACE_1, Manual, Spline, SUPERPIPE, Relative/replay clock cameras, Near/Far
  variants (DEFAULT_2/DEFAULT_4 share the code with different gp constants — not wired),
  override selection (`0x161FA0`) and the auto-revert timer.
- The override-only line-of-sight occlusion branch of `0x15EE00` (`0x15D020` gate is false
  for the automatic camera). `+0x490/+0x4A0/+0x4A4` are kept in the state for completeness.
- The output quaternion/matrix of `0x166F90` and the render axis permutation `G` (0x4C53A0):
  the renderer only needs eye, look-at (zero roll) and fov; `forward` is provided from the
  same VU arithmetic as `0x1668B8`.
- `0x2ED490` render-listener notifications; the far cap `0x2EE3B8` is an input (`farCap`).
- Global debug flag `*(gp-0x6A0)&1` (disables controller update and shake).

## API

```cpp
struct OriginalCameraInput   // per-tick rider/world data (see header comments for offsets)
struct OriginalChaseAlgorithmState   // the 0x390-byte object, one member per decoded offset
struct OriginalCameraCompositorState // outer camera fields + shake + fov/near/far
struct OriginalCameraState { algorithm; compositor; void begin(const OriginalCameraInput&); };
struct OriginalCameraOutput { eye, lookAt, algorithmEye, algorithmLookAt, yaw, pitch, forward, fov, near, far, resetFired; };
OriginalCameraOutput originalChaseCameraStep(OriginalCameraState&, const OriginalCameraInput&);
NativeCameraView nativeCameraView(const OriginalCameraOutput&);
```

`begin()` = constructor defaults (`0x1622B0/0x162318/0x176D68`) + the set-target sequence,
which already runs two updates and one finish with the seed input. `originalChaseCameraStep`
mirrors `0x1624E8`: if `+0x2F0` is set it re-runs set-target first, then update, finish,
compositor copy, collision, shake, angle recompute and the fov/near/far clamps.

Timing: no time scale enters the camera; every constant is per frame (`1/60` = 0x3C888889).
Run it once per fixed 60 Hz gameplay tick after the rider's frame phases, never per render
frame. Interpolate the two latest outputs for rendering if the display rate differs.

## Rider field mapping

| Input | Source | Engine |
|---|---|---|
| `headPosition` | slot 0x08 → `0x11FF48` head bone (rider+0x89C) world row, w=1 | world pose of the head bone |
| `riderForward` | rider+0x1B0 | physical forward column (ORIENTATION_RECOVERY) |
| `velocity` | slot 0x18 → rider+0x1E0 (w=0) | `groundState.velocity` / air state velocity, cm/s |
| `previousContactNormal` | slot 0x48 → rider+0x380 | `groundState.previousNormal` |
| `wallNormal` | slot 0x80 → rider+0x3C0 (set when contact normal z < 0.05) | not yet tracked; zero until a wall is touched |
| `motionMode` | `0x11FE98` = motion owner +0xDE0 | `currentMotionMode()` |
| `boostLevel` | slot 0x88 → rider+0x2FC | `groundState.boost` (0/.25/.625/1) |
| `jumpCharge` | slot 0x90 → rider+0x220 | `groundState.crouch.current` / `jumpCharge` (savestate: 0x3F7626B5 = jump fixture charge) |
| `surfaceId` | slot 0xA0 → rider+0x438 | `GroundSurface::id` |
| `riderType` | rider+0x434 | `OriginalJumpState::riderState` |
| `launchValue` | slot 0xA8 → rider+0x5A4 (649.68 in the jump-31 savestate) | UNCERTAIN: jump launch magnitude; ramp 300..500 |
| `proximityFlag` | slot 0xC0 → rider+0x5AC | UNCERTAIN; false in all savestates |
| `trajectoryStatusActive` | slot 0x98: `*(rider+0x788)+0xAC ∈ {1,3}` | `OriginalAirTrajectory::status` |
| `predictedAirTime` | slot 0x60: `*(rider+0x788)+0x98` | `OriginalAirTrajectory::predictedTime` |
| `trajectoryHeading/Normal` | `*(rider+0x788)+0x10 / +0x20` (slot 0xB8 landing angle) | UNCERTAIN naming; savestates show +0x20 = (0,0,1,0) |
| `tick` | `0x1298C8` = `*(*(*(gp-0x848)+0x84)+0xC)+8` | `RiderFrameTiming::tick` (savestate: +0x2CC == tick) |
| `terrainProbe` | `0x32E100/0x336850` swept segment | world collision query returning t and normal |
| `visualRandom` | `0x3177F0` six-word RNG at 0x4FF018 | the snow-particle `OriginalRandomState` |

## Object layout (this = algorithm, 0x390 bytes)

| Offset | Field | Init (ctor / set-target) |
|---|---|---|
| +0x00/04/08 | fov, near, far | pi/4, 10, 30000 |
| +0x20 / +0x40 | look-at / eye quads | — / (0,0,0,1) |
| +0x50 / +0x54 | yaw / pitch | 0 |
| +0x60 | output eye (w=1) | — |
| +0x80 | filtered travel direction (not unit) | normalize(fwd·559.744 − Z·300) |
| +0xA0 / +0xB0 | head copy / mode-5 saved offset | — |
| +0xC0 | pull-behind filter | 0 |
| +0xF0 | Z × fwd (init only) | |
| +0x100 | lag-filtered horizontal velocity | vel with z=0 |
| +0x110..+0x180 | swing camera vectors | 0x170 zeroed |
| +0x1A0 | slow-lock view vector | fwd·250 |
| +0x1B0 / +0x1C0 | last velocity / last raw velocity | fwd·250 / fwd·2 |
| +0x1D0 | follow distance | 200 (ctor only) |
| +0x1D4 | eye vertical offset | 61.687962 |
| +0x1D8 | filtered pitch | 0 (ctor only) |
| +0x1DC | fov scale | 1 |
| +0x1E0 | mode-5 blend timer | 10 |
| +0x1E4 | look-at height | 0 |
| +0x1E8 / +0x1EC | boost bump / last boost | 0 |
| +0x1F4..+0x228 | jump camera clocks, gains, offsets, landing decay 0.92, take-off ramp 1 | |
| +0x22C..+0x244 | swing timers/angle | 0 |
| +0x248 | lock blend | 0.5 |
| +0x2B8 | extra eye height | 0 |
| +0x2C0 | mode-5 frames | 5000 |
| +0x2C4 | take-off countdown | −1 |
| +0x2CC | last airborne tick | 0 |
| +0x2D0/+0x2D4/+0x2D8 | wall-launch / airborne latch / swing initialised | 0 |
| +0x2E0/+0x2E4/+0x2E8 | phase B / landed / phase A | 0 / 1 / 0 |
| +0x2F0 | reset pending | 0 |
| +0x300 | lock state (0 in, 2 locked, 1 out, 3 idle) | 1 |
| +0x304/+0x310/+0x31C/+0x328/+0x334 | splines: look B, eye B, look A, eye A, swing ease | |
| +0x340/+0x354/+0x368, +0x37C | PID rings X/Y/E, index | 0 |
| +0x380/+0x384/+0x388 | Kp 0x3C3AB019, Ki 0, Kd 0x3AA793A0 | |

Outer camera: +0x00/04/08 fov/near/far, +0x0C pi/4, +0x10 30, +0x14 30000 (savestate
evidence; the decode's 20000 was wrong), +0x450..+0x45C shake request, +0x460 lift,
+0x470 last probe normal, compositor DEFAULT_3 at +0xC0 (+0xE0 look-at, +0x100 eye,
+0x254..+0x2B4/+0x2EC shake state).

## Constants (DEFAULT_3)

gp = 0x4A30F0. Velocity filter gp-0x6120..-0x6104: 0.6, 0.27136135, 0.9, 0.29420722,
0.85139298, 0.85, 0.97, 0.1. Direction: 0.98 (gp-0x6100). Distance gp-0x60FC..-0x60E4:
0.92397803, 0.90741497, 300.92493, 11.549837, 76.68663, 10.755208, 1.7668397. Vertical
gp-0x60E0..-0x60D0: 0.97839177, 11.678784, 31.225603, 11.392387, 9.33469. FOV: 0,
0.95990783, 1, 0.95. Hold: 2.0036807, 0, 0.98, 0.96. Look-at height gp-0x60AC..-0x609C:
0.92419082, 31.102573, 0.37469128, 12.332657, 10.589058. Jump gp-0x6098..-0x608C:
0.1364145, 0.85366827, 2.4947209, 1.8110173. Swing gp-0x6088: 1.5269116. Pull gp-0x6084/80:
146.389, 0.97069818. Push: 600, 300, 100, 0.98. Lock: 15, 1.5, 1.5. Finish (0x176DE0):
16.140757°, 6.6548877°, 0.92642629, 0.84256876. Set-target: 61.687962 (gp-0x6134),
559.74402 (gp-0x607C), 300. Bit patterns for all of these and for every literal inside the
helpers are in the header (`default3` namespace and inline `bits(0x…)` calls). The jump
spline knot tables (gp-0x6A98.. / gp-0x69F0..) and the swing ease knots (gp-0x6904..) are
embedded verbatim.

## Numerical rules applied

- One `FE_TOWARDZERO` scope per `begin`/`step`; VU0 quad ops are plain chop `mul/add/sub`
  (four lanes, w included in every dot product); `vsqrt` = `sqrt` under chop; VU DIV/RSQRT
  of zero saturate to 0x7F7FFFFF.
- EE `add.s/sub.s` use `originalScalarAdd/Subtract` (one guard bit), EE `mul.s` is a chop
  multiply, EE `div.s/sqrt.s` use `collision_scalar::divide/squareRoot` (nearest, ±FLT_MAX
  on x/0), `min.s/max.s` are plain compares, `cvt.s.w` is exact.
- Delay-slot side effects (stores that always execute, `bc1fl/bc1tl/beql/bnel` likely forms)
  are preserved where they change state: e.g. `+0x2C4` is decremented before the branch,
  `+0x1E0` stores before the `< T` test, `+0x2EC = requested` in the shake fade branch.

## Verified vs inferred

Verified bit-exact against PCSX2 (single tick from the frame-N object with frame-N+1 rider
state, every decoded word of the object plus the compositor eye, lift and fov/near/far):

- `glide` (cruise 63 km/h), `turn` (hard left, PID ring updating), `charge12` (crouch held,
  charge 0) and `jump` (first airborne frame: countdown 15→14, phase-A spline pair built,
  offsets at 2/60 s, `+0x2CC` = tick) — 0 mismatching words each.
- Compositor: lift decay 0.97 and eye re-projection reproduce `outer+0x20` exactly; far
  = min(30000, farCap 29999.34) matches `gp+0xA78`.

Divergent: `charge23` (the crouch-start frame, charge 0→0.19). The camera evidently ran
before a late velocity/charge change of that frame: no single velocity coefficient
reproduces `+0x100` from the end-of-frame rider velocity, and `+0x1E4` implies a charge of
≈0.28. The test tolerates its 11 words (≤0.09). Feeding the camera the rider state at the
point where the original camera update runs (after motion, before the crouch adjustment)
is an integration question for `riding.hpp`, not a camera math error.

Corrections to the decode documents discovered while testing:

- `0x163450` mode-5 condition is inverted in decode_B: `beqz ctx+0x5C` jumps to the timer
  path, so the offset is captured **during** mode 5 and the blend runs after it ends
  (savestates confirm `+0x1E0` stays 10.0 and `+0xB0` is never written in cruise).
- `0x165938`: only the set-1 shake timers (+0x278..+0x280) are advanced; the second
  octave (+0x2A8..+0x2B0) never ticks in the original and is therefore inert.
- Outer `+0x14` (far maximum) is 30000, not 20000.
- `0x31C128` is asin (decode_C/D correction applied everywhere; the PID "alignment" is the
  angle itself, confirmed by the `turn` savestate ring X ≈ 0.54..0.57 rad).

Inferred / uncertain (exposed as inputs with the values seen in the human-rider savestates):

- rider+0x5A4 (`launchValue`, 649.68 while airborne), rider+0x5AC (`proximityFlag`, 0),
  rider+0x434 (`riderType`, 0), rider+0x438 (`surfaceId`, 0), motion modes 4/5, the two
  `*(rider+0x788)` vectors of the landing angle, the race-state shake suppression, the
  signedness of the `tick - lastAirTick < 31` compare (int32 used), and the exact
  accumulation order of the VU quaternion rotation in the swing camera (only reachable
  after a near-vertical wall launch). The shake random walk is exact in operation order
  but its RNG stream depends on sharing the particle generator (`visualRandom`).

## Tests

`engine/original_camera_tests.cpp` covers: constructor and set-target seeds; natural spline
coefficients/evaluation against a double-precision Burden–Faires reference and the cache
tie rule; the clamped swing ease curve; the five savestate oracles above; a deterministic
600-frame synthetic cruise → 650 cm/s jump → landing run (finite, eye behind and above the
rider, 100 ≤ |lookat−eye| ≤ 1600, distance ≈380 and fov = 0.95990783·π/4 in settled
cruise, jump camera raises the eye); collision lift with a synthetic floor hit and its 0.97
decay; boost shake request/apply/fade; the native conversion.

Compiled and run with
`clang++ -std=c++20 -O1 -frounding-math -ffp-contract=off -I engine engine/original_camera_tests.cpp`
(also passes at -O2).

## CMake

Add to `engine/CMakeLists.txt` (no library dependencies; header-only):

```cmake
add_executable(ssx3_original_camera_tests original_camera_tests.cpp)
target_compile_options(ssx3_original_camera_tests PRIVATE -UNDEBUG -frounding-math -ffp-contract=off)
add_test(NAME native_original_camera COMMAND ssx3_original_camera_tests)
```

Browser integration update: launchValue is no longer left at0. `originalJumpCameraLaunch` recovers114B78..114C6C;20,000 original takeoff cases also match rider+5A4 exactly. Ground and rail departure retain this value for the chase camera's takeoff ramp, and riderType is supplied from physics state. Charged-jump browser integration asserts launchValue>300. Other input/algorithm gaps above remain open.

### Ground crash camera feedback and shared particle RNG

Source audit of12D160 found12D218 calls15E360 (camera requestShake), index4, fade0, scale=clamp(speedCmps*.03599999845/100,0,1). It is gated by rider+870>=0 and rider+87C!=0. The recovered callback had been mislabeled `rumble`; browser converted it to a discarded observer number and also left the device gate at its disabled defaults. Renamed it `cameraShake`, wired captured original_reset device_index/device_enabled through BrowserCrashHost, and queue the request into the DEFAULT_3 compositor before its tick. Rider reset clears the queued request. This is grounded crash-recovery feedback, not a newly invented ordinary landing impulse. Native PrototypeRider's frontend callback remains a no-op; the browser implementation is connected.

The browser chase camera now receives the same six-word3177F0 random state used by snow particle births instead of the camera's private fallback. This does not prove original scheduling or complete RNG parity with all original producers, several of which remain unported.

camera_inputs retains indices0..13 and adds14=shake index,15=active request,16=fade timer,17=cumulative crash-camera callback count. Crash gameplay tests require active index4 feedback in all three recovery fixtures. The original crash oracle now also checks the source request's index4/fade0 contract:20,000 air and20,000 ground recovery cases pass. Full browser tests and production build pass. The5,160-frame native/browser trace matches all580 fields per frame, including camera and particle-buffer fingerprints. This is implementation agreement; no new continuous PS2 jump/landing framebuffer comparison has been completed. Ordinary jump camera phase scheduling, wall/proximity inputs, alternative algorithms, and full visual fidelity remain open.

### Terrain-edge passive takeoff restored

The browser ground-departure branch previously entered air motion without114298. Original13F178 explicitly invokes114298 with charge-1 at13F194..13F1A0 when the ground controller's departure flag is set. The browser now calls the recovered routine with the current source position/velocity, contact normal, previous normal, surface-forward vector, speed limit and rider type before its existing air transition. It preserves held crouch. The routine supplies the camera launch value directly; charged ground jumps likewise retain the returned value instead of recomputing after host coordinate conversion. This restores source passive impulse/projection/speed-limit behavior. Existing host body-contact/animation scheduling is unchanged and is not established as fully source-identical by this fix. The existing browser takeoff flags remain73; generalized rider flag-state recovery remains separate.

Source trace also narrows the previously generic description of rider+5AC:13AF28 clears it at each rail-motion tick, queries within300cm using the board bone, then sets it only when the hit instance has an entity whose virtual170 predicate returns nonzero. This is not simply an arbitrary nearby-wall test. Static authored spline queries have no such instance entity. Do not populate the flag from a generic collision-distance heuristic. Current dynamic rail entities remain unsupported.

Validation for passive takeoff:20,000 original takeoff cases (including negative-charge cases) match position, velocity and camera launch value;120 charge ticks match. Full npm suite and production build pass. The5,160-frame,580-field native/browser trace remains exact. Continuous PS2 gameplay/visual comparison is still outstanding.

### 2026-09-12 jump/landing investigation

User specifically identifies jump and landing movement as the camera problem. Current
browser camera integration passes finite-output, projection, clock, launch amplitude,
and takeoff-wall-vector checks across charged and held-ledge departure/landing.
These are input/sanity checks, not original-game visual fidelity tests.

Expanded `web/test-air-steering-direction.mjs` to cover the rail-test spawn as well
as normal riding, with and without prewind before jump release. All four fixtures
produce left/right board-nose rotation with the expected sign relative to the chase
camera at release, and are airborne at measurement. Ground steering also passes
normal and post-rail fixtures. This does not reproduce the reported reversal and
does not establish correctness after arbitrary spins, switch landings or camera
swings. No gameplay signs or camera constants were changed in this investigation.

Rechecked the early prediction/countdown branch against original1636BC..163708:
the source also shortens the countdown when trajectory status becomes active.
Changing that branch to force a different takeoff phase is not justified by this
inspection. The outstanding task is a continuous original-vs-browser takeoff,
airborne and landing comparison, including host scheduling and target inputs.

### Filtered crouch input repaired

Original15F710..15F718 reads rider+220 for camera target slot90. This is the
`GroundControlValue::current` of crouch, not the browser's separate button-charge
accumulator. `camera_for_head` previously used that accumulator, which is zeroed
on charged takeoff and airborne release. It now reads the controller state used
for the sampled pose through `browser_camera_crouch()` in animation_bridge.cpp.
This also follows rail/start/crash/reset owners, whose triplets are copied into
the pose controller before sampling.

DEFAULT_3's163270 eye-height stage and162B90 look-at-height stage use this value
on the ground and retain their previous terms in the air. Thus this fixes the
crouch/landing input and its retained offsets; it does not prove that clearing
button charge directly caused an airborne snap, or resolve all camera timing.
Camera inputs append index21=filtered crouch (previous indices unchanged).
`web/check-camera.mjs` checks every tick against the active controller, including
charged and held-ledge departure/landing. Charged release specifically has zero
button charge while the camera's filtered value remains above0.5.

Validation: full npm suite, native/browser gameplay comparison (9,630 frames,
23 scenarios,634 fields,zero mismatches) and production build pass. No camera constants, steering signs, physics or animations changed.
Original continuous jump/landing video comparison remains outstanding.

### 2026-09-13 landing-stage conformance and reproducible transition trace

Extracted the existing touchdown/offset-decay block into `stepJumpLanding`, keeping
its operation order and lazy landing-angle getter. `tools/test_camera_landing_native.py`
executes original163E8C through163FD4 (stops at163FD8, the common successor;
163FD4 alone is skipped by the unlatched branch). It loads constants from the
SHA1-verified owned executable and controls only the virtual landing-angle getter.
All nine retained state fields match bit for bit over20,000 cases:6,666 first
landings and16,666 frames applying offset decay. Includes airborne/already-landed
branches and exact1.2/2.2-second and1.05/1.4-radian boundaries. This validates
neither the supplied angle nor the original full-frame scheduling.

`node web/check-camera.mjs --trace` and `--zoe --trace` now write full per-tick
head, eye, look target, relative eye, motion/posed-ground state, crouch and
trajectory inputs to `local/browser-validation/camera-transitions-RIDER_*.json`.
Both packages reproduce the largest relative-eye step on touchdown:0.50135m at
fixture tick773 (charged fixture) and0.56153m at tick782 (held fixture).
At both ticks camera motion mode=0 but posedGrounded=false; the next pose is
fully grounded. animation_bridge.cpp captures posedPhysical/head before body
contact and resolve_posed_landing; core.cpp then builds camera inputs from the
post-contact velocity/mode/orientation and the pre-contact head snapshot.
This is a reproducible mixed-phase input, not yet proof that the original does
otherwise or that it explains the whole visible jolt. Next: compare source frame
ordering and the landing-angle getter, then correct the binding if warranted.
Do not conceal it with invented smoothing or an unverified one-frame delay.

Validation: native camera suite (existing charge23 tolerance unchanged), original
landing-stage comparison, rebuilt WASM camera checks for Sam/Zoe and four airborne
steering fixtures pass. This change adds direct source conformance and traces;
it does not claim to fix jump/landing feel or the intermittent steering reversal.

### Landing input follow-up: cached pose is intentional

Rechecked the source-backed animation ordering (ANIMATION_RECOVERY.md, staged
121700/11EB60 ->121728/11EB98 ->second motion/contact). Original13AA48 consumes
cached posed geometry and does not rebuild it on landing/impact;11FF48 reads the
cached world bone. The mixed pre-contact pose/post-contact physics identified
above is therefore consistent with the original architecture. It is not evidence
for delaying the camera's motion mode or resampling animation after contact.
The0.50/0.56m relative movement remains measured browser behavior, with no matched
original continuous camera trace yet establishing whether it is erroneous.

`tools/test_camera_landing_angle_native.py` now executes the complete15F780 getter
with actual31C128 asin and ELF constants, comparing its output bits to landingAngle.
20,000 cases pass, covering zero/tiny vectors, arbitrary velocity magnitudes,
parallel/antiparallel approach and generic angles. No mocked math or angle result
is used. Together with the landing-stage oracle, this removes the controlled
angle math from the outstanding questions; live trajectory input and full-frame
camera continuity remain unverified. No runtime camera behavior changed here.

### Continuous original camera replay: 2026-09-13

Launched the original ISO with the existing isolated PCSX2 configuration
(local/browser-pickups/pcsx2, PINE slot28022, memory cards disabled), loaded
snow-jam-jump-31.p2s and advanced with the configured FrameAdvance=B action.
`tools/capture_camera_reference.py` reads only paused PINE state, validates the
SSX3 title and baseline camera/rider signatures, checks the clock before/after,
and writes exclusive tick-named files. The game clock uses the1298C8 pointer
chain, not rider+2CC (which is not that clock). It retains both DEFAULT_3
allocations;0x1A58650 is the live updating algorithm,0x1597DD0 is retained too.

`local/camera-continuous/jump/tick-00000369.json` through tick-00000490.json are
122 consecutive checkpoints. The first is the already-airborne baseline; the
121 updates cover81 airborne ticks and40 grounded ticks, touchdown451.
Every compared algorithm field matches bit for bit on every update, both when
seeded from each preceding original checkpoint and when seeded once at369 and
carried continuously to490. Output eye and look-at errors are zero throughout.
The initial spline coefficients/caches are restored, with no per-frame spline or
state replacement in the continuous path. `tools/test_camera_sequence_native.py`
rebuilds/runs the comparison and verifies provenance, continuity and zero errors;
it emits jump-comparison.json and a SHA256 manifest for all capture files.

Scope: the DEFAULT_3 algorithm with original post-frame rider/head/trajectory
inputs and original initial camera state. This is NOT browser physics parity,
compositor terrain collision/shake parity, image parity or camera set-target
initialization equivalence. Next investigate those host/input differences rather
than changing the now-continuously-verified landing/takeoff math.

The earlier neutral run retained ticks479..599 but hit scenery before flight.
Its frame-0000..0021 files contain a gap after an initial filename reuse error;
those files are excluded from the continuous comparison. Tick-based exclusive
writes prevent recurrence. No game RAM, original states or ISOs were patched.
PCSX2 was left paused at jump tick490 for further read-only inspection.

### Browser release and landing crouch ownership repaired

`web/audit-camera-inputs.mjs` matches the verified accepted input timeline
(one neutral frame, hold jump on frames2..30, release31) to the original jump
capture. The first-frame movie input is not yet accepted by the original game;
using held input at frame1 made the comparison stimuli differ.

Found and fixed three host binding issues in core.cpp/animation_bridge.cpp:
- First jump press preserves existing crouch target, matching the control-entry
  return. It previously requested full crouch immediately.
- Charged release now reads the retained rider crouch for takeoff. Air entry sets
  its targets before the single filter tick. Previously ground physics approached
  crouch, then animation re-targeted and approached it again on that same frame.
- Air turn/brake/crouch filters commit to shared physics state before contacts.
  Previously the pose controller reached zero crouch but physics retained the
  takeoff value, which reappeared in ground/camera input after landing.

Browser filtered crouch now matches original bits at all122 capture checkpoints,
including release0.8653735518455505 and zero through touchdown/settling. The audit
asserts that full sequence, and reports the other input errors without calling
those correct. Remaining launch value629.1393 vs649.6843, browser touchdown449
vs original451, release head error12.27cm, final head error18.39cm and final
position error7.16cm show broader host/pose/launch differences still need work.
The previous matched-input implementation's landing happened at451 but had a
wrong release crouch and stale landing rebound; matching landing time alone was
not proof of the correct underlying motion.

Validation: full npm suite and production build pass; rebuilt native/browser
host trace comparison passes. This is a crouch ownership/order fix, not full
physics, takeoff amplitude or final camera image fidelity.

### Takeoff contact-history ordering: 2026-09-13

The browser wrote current normal into previousNormal at the start of every
step_rider, before processing charged release. Original13D20C/13D218 performs
that copy in ground contact. It must not run before controller takeoff, and air
frames retain the historical normal. Moved the browser copy into its ground
contact phase. The retained takeoff normal now matches the preceding contact
frame, as asserted by web/audit-camera-inputs.mjs. The legacy earned-boost test
assumed a normal-history update every tick; it now checks ground-contact writes,
landing writes and retention through air/release, following the verified source.

Added jump_takeoff_info (21floats, private diagnostic export) capturing the actual
charged-takeoff inputs before114298. It records position3, velocity3, normal3,
previousNormal3, forward3, crouch, speed limit, ground-focus age, rider type,
flags and motion mode. Reset clears it. Camera audit stores browser-takeoff.json.
This makes input errors inspectable without changing the original jump kernel.

The fix reduces previous-normal differences from approximately0.0031 to at most
0.000023 per component in the matched fixture. It does not change launch629.1393
vs original649.6843: before takeoff the browser velocity already differs by
(4.861,0.384,-20.240)cm/s. Crouch0.9615281224, rider type0 and flags73 agree;
speed limit differs2.366cm/s and ground-focus age210 vs167 (both past its ramp).
Incoming position differs only(-0.078,0.193,0)cm. Further work should inspect
pre-release ground velocity/contact scheduling rather than tune the camera or
add an arbitrary jump impulse. The122 original crouch comparisons still pass.

### Reference spawn and retained ground basis repaired

Ground force reads13D9E4/13DA14/13DD40 consume rider+3A0/+3B0 retained by
originalGroundContact. The browser rebuilt them from the newly aligned physical
quaternion both at force entry and in publish_motion (which should publish
telemetry). Removed those extra ground rebuilds. Explicit custom-spawn/crash
setup uses originalGroundContact with preserved distance; air publishing keeps
its current separate orientation path. This alone had only a small effect in the
neutral reference jump and is not claimed as the launch-error cause.

The larger error came from reset_rider raycasting the original authored spawn:
it replaced the supplied source position/normal with a terrain snap. At the first
checkpoint, browser contact distance was+2.1248cm vs original-0.757567cm; that
excited a different normal-force transient while charging. When the supplied
float position matches the configured source start, reset now restores the exact
source position and normal. Matching heading also retains the supplied quaternion
and surface basis. Custom positions/headings still receive terrain placement and
orientation setup. Event initialization continues to replace this with its own
full authored event seed.

The matched accepted-input replay now has maximum rider-position error0.04866cm
(0.487mm) across122 checkpoints, correct touchdown tick451, launch649.6841431 vs
649.6843262cm/s, and final position error0.02628cm. At touchdown position error is
0.02024cm. Crouch remains bit-exact at all checkpoints. Posed head error remains
up to17.45cm (13.06cm at final checkpoint); camera visual fidelity is not complete.

Also publishes originalLandingResolveContact's cleared trajectoryPredictionTime
back to browserTrajectory; the camera now sees0 after landing as in the original.
The earned-boost camera test now samples predictor state after pose/contact, when
the camera runs, while its earlier physics assertions still inspect pre-contact
integration. web/audit-camera-inputs.mjs asserts matched motion modes, <1mm rider
position error, <0.001cm/s launch-value error, exact crouch and cleared grounded
prediction time. It also retains before-motion/after-motion/after-pose traces in
browser-motion-phases.json for further investigation.

Validation on this revision: full npm suite, matched original jump-input audit,
rebuilt native/browser host trace comparison and production build all pass.

### Authored pose scale, controller prediction, and late geometry translation

The reference geometry scale is(0.8499999642,0.8499999642,0.8499999642).
Browser local samples at jump31/60 already matched the original exactly, but
world posing, procedural root/leg contact and mesh binding used unit scale.
The runtime now loads original_animation.scale into graph.scale and uses it for
world pose, root presentation, air pivot and scaled crash roots. Body broad
radius uses the authored physics bodyScale. Renderer vertex positions and bind
bone translations use the same uniform scale before skin binding; menu sampled
translations also use it. Current Sam/Zoe share the existing Zoe gameplay profile.
Nonuniform render scales explicitly fail rather than silently distort the model.

Air animation selection now snapshots trajectory status/time/elapsed before the
motion update, following source controller-before-motion scheduling. Previously
it observed the freshly advanced predictor from the same tick. At jump90, the
local root mismatch falls from0.962cm to0.0000077cm. Mid-flight camera head error
falls to0.0265cm at60 and0.0379cm at90 after scale/timing corrections.

The remaining7.53cm touchdown head error equalled collision displacement.
Source106538 accumulates translations in rider+9D0 while moving physics/collision
bodies. After second motion,121750 calls310530 to add that displacement to cached
bone positions and skin matrices. This is translation of sampled geometry, NOT
another animation sample. Earlier notes correctly said no pose resampling but
missed this late geometry commit; retaining an untranslated visible pose was wrong.

OriginalLandingState now exposes its exact computed translationCm. Browser tracks
landing and ordinary body-contact displacements, commits them to rendered pose
origin/head/cached world bones before camera/effects, and completes matching body
volume/board-center translation. The contact reaction frame remains pre-impact.
pose_translation exposes that final displacement for diagnostics. Landing probes
are checked against the pre-commit board, while rendered/collision positions are
checked after commit. Existing tests that assumed no post-contact translation
were corrected to match121750/310530, retaining animation/clock continuity checks.

Afterward the reference touchdown head error is0.02764cm (0.276mm), final error
0.05902cm. From frame60 through152, maximum head error0.0734cm stays below1mm;
release retains a smaller0.8cm transient. Rider position remains below1mm across
all122 captured checkpoints. The audit now asserts these scoped limits, motion
transition timing, crouch and grounded prediction reset. This does not establish
all animations, all riders, compositor/shake or full rendered-image parity.

The earned-boost and forced-reset tests now bank the captured early jump/grab
before testing spending/reset retention. Their old later jump crashes with the
correctly sized geometry; no fake reward or disabled collision was substituted.
Crash lifecycle remains exercised by its dedicated original-clip suite. Browser
selection Sam->Zoe and live Snow Jam rendering were inspected with scaled mesh,
board and bind skeleton; material/lighting fidelity remains separate unfinished work.

Validation: full npm suite, original jump-input/head audit,80,000 original landing
stage comparisons, rebuilt native/browser host trace and production build pass.

### Charged-release presentation filter order repaired

Ground leave1399E0/13F210 targets (via originalLandingGroundLeave) must be set
before1211F8 advances the presentation filters on charged release. Browser ran
that filter first, leaving board alignment1 instead of0.95 and body lift2.8665cm
instead of original2.0384cm on the first airborne pose. Normal charged release
now defers those six presentation triplets until immediately after ground leave;
other controllers retain their existing ownership and timing. No extra filter
tick is added. The prior source-backed crouch release fix remains separate.

Maximum posed-head error is now0.07967cm (0.797mm) across ALL122 original capture
checkpoints, including takeoff, touchdown and settling. The camera-input audit
now enforces<1mm head and rider-position error for the entire sequence, plus
matched motion mode, original crouch and cleared grounded prediction time.
This fixture still does not establish arbitrary jump/trick/camera trajectories
or final compositor/image parity.

### Output transform numerical recovery

`camera_transform.hpp` now reproduces166F90 and31B748/31B7A8, including the
conjugated output quaternion. It also reproduces the renderer's395750 Euler view
setter.20,000 cases match all source matrix and transform output words through
`tools/test_camera_transform_native.py`. This supersedes the earlier statement
that the output transform has not been translated, but these helpers are not yet
wired into camera rendering. The renderer view stack's active producer/angle
mapping and timing still need to be established. See the final lighting-recovery
section for getter/setter evidence and the distinction between these matrices.

### Fresh browser camera versus retained original jump history

`web/audit-camera-inputs.mjs` now measures algorithm eye and target outputs,
in addition to rider/head inputs. The read-only `camera_algorithm_info` export
returns source-centimetre eye/target before compositor collision lift and shake.
The existing 122-checkpoint input assertions still pass, but this is **not**
end-to-end camera parity: the fresh browser camera differs from the retained
PS2 camera by 57.76cm at tick369, 34.18cm at touchdown451 and 15.22cm at490.
Maximum target error is5.35cm. The report explicitly records
`outputWithin1mm: false` in local/camera-continuous/browser-output-comparison.json.

The independent native test still matches121 consecutive algorithm updates
bit for bit when initialized from original state and given original inputs.
Different initialization/history is therefore a lead; the output comparison
does not prove it is the sole cause. Do not tune smoothing or substitute a
saved airborne camera into normal event starts to hide this discrepancy.
Next: compare matched pre-jump camera history and browser-driven algorithm
state before auditing the final compositor/rendered jump and landing.
Air-direction regression passes all four existing ordinary/rail and
prewind/no-prewind fixtures; intermittent reversal remains unreproduced.

### Camera history isolated by browser-input replay

`tools/test_camera_browser_history.py` now controls the initialization variable.
The browser exports all38 scalar words consumed by DEFAULT_3's algorithm
(seven quads and ten scalar inputs) through `camera_source_input`. The camera
audit records these for all152 ticks without changing simulation behavior.
The native sequence runner replays that actual input stream twice:

- Fresh initialization at the same first browser tick reproduces the WASM
  algorithm eye and target exactly at every compared tick370..490.
- Initialization from the original tick369 camera, followed by browser inputs
  for370..490, stays below1mm of the original eye and target throughout.
  Maximum eye error is0.0733918cm; touchdown eye error is0.0270633cm.

This isolates the prior34.18cm touchdown discrepancy to different camera
history in this fixture. It is not evidence for changing the chase algorithm
or injecting a saved airborne camera into event startup. The new test checks
original provenance and continuity, original algorithm parity, exact cold
native/WASM output replay and submillimetre matched-history output. The
previous fresh-vs-retained report remains explicitly nonmatching.

Remaining jump/landing investigation should compare final compositor terrain
lift/shake and displayed interpolation against original frames with matched
camera history. Normal event-start camera lifecycle and other camera modes
remain distinct scope. Intermittent airborne reversal remains unreproduced.

### Terrain-clearance compositor fixes (15EE00)

The full original15EE00 routine now runs against the native clearance stage
in `tools/test_camera_collision_native.py`. Its32E100 query-construction and
336850 terrain-result callbacks are controlled;15D020 selects the ordinary
chase path (override occlusion remains outside this test).20,000 cases compare
all eye/normal words, lift and final flags, plus exact query endpoints and
construction arguments.17,121 cases return a positive hit.

This exposed two actual port errors:

- Both large-push rejection branches used350cm. Original15F160 and15F198
  load0x43160000, **150cm**. The wrong cutoff produced5,908 mismatched
  output fields in the20,000-case oracle; restoring150cm yields zero.
- Browser terrain probing passed preferredFraction0 to sourceTerrainSegment.
  Original15F0C8 passes F12=0.5 to32E100 (kind2). The browser now uses the
  verified midpoint preference, so intersecting surfaces compete by distance
  to the camera's probe midpoint rather than its upper endpoint.

No authored camera spline or smoothing parameter was retuned. This establishes
clearance math and the query contract with controlled hits; whole-world terrain
query parity, override occlusion, shake and complete rendered frames remain
separate verification requirements.

Validation after clearance fixes: full npm suite, production build, original
clearance oracle and matched-history camera replay pass. Rebuilt native/browser
Sam gameplay trace also passes. Live Snow Jam was loaded, jumped and paused
with no reported load/terrain errors; this was a smoke check, not a matched
original-image comparison. Logs: local/camera-continuous/collision-*.log.

### Camera kind2 terrain refinement restored

Camera query32E100 receives kind2. At32B6E0 (e.g.32BE28..32BE70), this
invokes the query's+0x44 refinement callback32E9A0 after coarse intersection.
The camera adapter incorrectly requested coarse-only kind1 behavior. It now
requests refined point/normal/UV, preserving the original coarse fraction
and the corrected midpoint preference. No rider-motion query was changed.

`tools/test_camera_terrain_reference.py` executes original32E100/32B6E0
with kind2 and F12=0.5 against captured terrain caches. It compares3,000
queries (1,110 hits) exactly for point, normal, coarse fraction and UV.
1,109 hits differ from coarse geometry, so this is not an inert flag change.
It covers three cached patches, not the whole world or cache-order behavior.
World eligibility continues to use authored bit0; original runtime bit0x40
is residency, as previously documented in terrain-contact.md.

Validation after kind2 correction: full npm suite and production build pass.
Rebuilt native/WASM gameplay comparisons pass for both Sam and Zoe:23 scenarios,
9,630 frames each, zero mismatched frames (including final camera, view matrix
and fog outputs). This is runtime equivalence, not original framebuffer parity.
Logs: local/camera-continuous/refined-*.log and terrain-oracle.log.

## Director, fades and the finish camera (2026-09-22)

`engine/original_camera_director.hpp` ports the camera director (vtable 0x45B908) that owns the
algorithm list, and the multi-algorithm path of the compositor 0x15E668. `original_camera.hpp` gained
POST_RACE_1 (type 0x44) next to DEFAULT_2/3/4 (`variantType` is now the algorithm type +0x0C).

| Address | Native | Behaviour |
|---|---|---|
| 0x161BB8 | `original_camera_director::update` | 0x161AB0 transitions, node pass, auto-revert |
| 0x161AB0 | `decideTransitions` / `transitions` | guard reset to 0x3D; motion 5 latches SPOKE (0x1621A8, rate 1.2/60 or 0.6/60); release above 35 km/h (0x162218, rate 1/150) |
| 0x161BF0..0x161D7C | `advanceNodes` | head steps (0x1624E8, a1 0) and ramps to 1 by its rate; older nodes ramp to 0 by the **head's** rate; smoothstep 3w²−2w³ (capped at 1) into +0x0C; type 0x4A forced to 0; weight ≤ 0 or smoothed ≤ 0 unlinks (0x15CA50) before stepping |
| 0x161D80..0x161E30 | `advanceRevertTimer` | 10 s auto-revert only for a 0x5D request in game states 1..9 |
| 0x161E58 / 0x15C988 / 0x15CB08 | `insertAlgorithm` / `push` | +0x18 = new type; empty list → rate 1; rate exactly 1 clears first; push runs set-target then inserts at weight 1 (rate 1) or 0 |
| 0x15D078 | `activate` | factory (DEFAULT_2/3/4, POST_RACE_1; others counted in `unsupportedRequests`); lock bit 1 forces rate 1; **set-target runs twice** (push + factory tail) |
| 0x162060 / 0x161EF0 / 0x162290 / 0x162258 | `request` / `originalDirectedCameraSelect` / `…Restore` / `…Finish` | override request, camera select (instant cut), restore, finish fade at 0.016793445 unless override bit 0 |
| 0x15DB58 | `originalDirectedCameraRestart` | clear flags/latch, instant cut (2 set-targets) + director vtable 0x20 (third set-target), lift 0, +0x4A4 = 1 |
| 0x15E668 | `composite` | one node **or a type-0x4A head** → copy; otherwise running means over nodes with smoothed > 1e-4 (k = s/Σs, eye/look-at at +0x60/+0x20); fov/near/far running means over raw weights ≠ 0 |
| 0x1789E8 | `postRaceConstruct` | direction = normalised horizontal velocity (|v| > 1) or rider forward; > 0.01 → unit, else +X |
| 0x178BB0 | `postRaceUpdate` | 0x162568 stage, phase = (phase+1) mod 1100, yaw = −0.748278856·cos(2π·phase/1100) (0x31C040), half-angle sincos 0x31BE50, rotate direction about Z; look-at = head − 3 cm Z; eye = look-at − dir·(126 + 200) + Z·(−27 + 150) when game-info +0x78 (roster) ≥ 2, else 126 / −27 |
| 0x178E90 / 0x178B98 / 0x178BA8 | `setTarget` / `finish` | base set-target with eye seed 0 and offset direction (100, 100); finish has no pitch lag filter |

**Set-target bug fixed (affects DEFAULT_3 too):** 0x166C60 re-reads target slot 0x20 into the
fwd·250 scratch just before storing +0x1B0, so `lastVelocity` is the rider forward itself, not
fwd·250 (confirmed by every countdown/ready savestate: +0x1B0 = (−0.986, −0.165, 0)). It matters
whenever |v| < 1 km/h (countdown). Event-start browser/PS2 final-eye error fell from 2–4 cm to
0.004–0.03 cm between ticks 69 and 189.

### Verification

- `tools/test_camera_director_native.py` (recompiled originals, EE scalar FP corrected): 20,000
  POST_RACE_1 steps through 0x1624E8 including 4,976 pending set-targets (every decoded word,
  direction, phase); 20,000 ctor runs; 20,000 director updates (0x161BB8 + 0x161AB0 + 0x15CA50:
  latch, transition calls, step order/a1, weights, unlinks, revert timer); 20,000 list insertions;
  20,000 compositor gathers + lens blends. All bit-exact.
- `tools/ps2_capture_director.py` adds a hook at the 0x15DF98 epilogue (0x15DFC8) and a finish
  event on the human path. `tools/test_camera_director_capture.py` checks
  `local/ps2-capture/runs/finish-neutral` (glide state, finish 30 m ahead, neutral pad): the finish
  pushes POST_RACE_1 in the same frame as the camera update that sees the same rider state; the
  complete construction (ctor + two set-targets + step, every decoded word), 58 POST steps, 1,093
  weight/smoothstep/unlink updates and 1,093 fov/near blends match bit for bit. DEFAULT_3 is
  unlinked on the 60th update; the roster count is 6 (326 cm / 123 cm).
- `engine/original_camera_director_tests.cpp` (ctest `native_original_camera_director`) and
  `web/test-finish-camera.mjs` (npm test: physics ride through the authored finish).

### Observed original sequence after the finish

Race phase 6 (EndRace) starts one tick after the finish; the rider enters control 10 and coasts to
a stop (~4 s). POST_RACE_1 stays for 409 camera updates, then the game starts a **replay** (tick
counter back to 1, director locked: flags bit 1, +0x2C = 0x5D via 0x161FA0, lift not reset). The
browser keeps simulating with a neutral pad behind the results panel for those 409 ticks and then
freezes (no replay; control 10 is not ported, so the browser rider keeps control-0 physics).

### Browser integration

`web/core.cpp` steps `OriginalDirectedCameraState` (API unchanged; `camera_director_info` added for
QA). `web/race_bridge.cpp` calls `browser_camera_finish(roster)` on the finish pulse; main.js and
compare-ps2-capture.mjs now run `race_end` (course events) before `step_camera_head`, matching the
original order. Motion mode 5 (handplant) is passed to the camera. Begin = outer-ctor request 0x4C
(two set-targets).

### Remaining gaps

- SPOKE (0x42) is not ported: a handplant sets the latch/flag bit 2 and counts an unsupported
  request; the 35 km/h release then fades a fresh DEFAULT_3 in over 150 frames as the original does.
- Event start: the original countdown DEFAULT_3 carries **four** set-targets (empirical: 4 reproduces
  the event-start capture to 0.006 cm at tick 19, 2 → 2.6 cm, 3 → 1.2, 5 → 1.0). The decoded
  mechanisms give 2 (ctor) or 3 (restart 0x15DB58); the fourth source is unidentified, so the
  browser keeps the decoded 2. Browser restarts also use begin, not 0x15DB58.
- Camera triggers: in the normal race the director never left a single DEFAULT_3 node (event-start
  capture through tick 890, glide segment 339–440). In the replay a kind-2 trigger (0x16E1D8 →
  0x162060(0x5B)) fired at GO (tick 202, rider at the grid) and replaced the camera with type 0x5B
  (object 0x90 bytes, ctor 0x174190, because the game state is 1..9). Not ported; the volume
  records at manager 0x4C5830 were not enumerated.
- Replay, results overlay 0x234008 (0x162290 restore) and control 10 are not ported.

## Widescreen modes (2026-09-22)

Front-end Options "Widescreen" (FEAMER: "Widescreen", "16:9", "Anamorphic"; CMNAMER "Off") is profile word
0x535610 bits 20..21 (setter 0x15BEE8, menu store 0x189D14). The profile bits alone do nothing: 0x228C08 (called at
boot 0x152DBC and after the menu store 0x189D34) passes the mode to the render context 0x61BA60 vtable+0x140 =
0x377950, which stores ctx+0x6B94 = mode and:

| mode | +0x6B98 top | +0x6B9C height | +0x6BA0 x scale | +0x6BA4 y scale |
|---|---|---|---|---|
| 0 Off | 0 | 1 | 1 | 1 |
| 1 16:9 | 0.125 | 0.75 | 0.75 | 0.75 |
| 2 Anamorphic | 0 | 1 | 0.75 | 1 |

- 0x376A70 (set viewport x,y,w,h): y = max(y, top*448), h = min(h, height*448), so mode 1 draws 3D into lines
  56..392 with black bars (letterbox inside the 4:3 signal).
- 0x376C58 (set projection): GS x scale = 0.5*w/tan(fov), y scale = x * 1.3333 (gp-0x27B0) * 448/512, then x *= +0x6BA0,
  y *= +0x6BA4 (also the normalized copy +0x5860/+0x5874). Off 272.65/318.09 px, 16:9 204.49/238.57, Anamorphic
  204.49/318.09; the GS centre stays 2048/2048. Verified live in ARMSX2 (ctx+0x5930 = 3271.82/-3817.13 and
  3271.82/-5089.50 = 16x the scales; viewport [0,56,512,336] in mode 1).
- Both widescreen modes keep the 4:3 vertical view angle and widen tan(horizontal) by 1/0.75 (Hor+ to 16:9). Mode 1
  zoomed on a 16:9 TV and mode 2 stretched by a 16:9 TV show the identical picture.
- HUD: mode 1 maps the in-race HUD into the band (y' = 56 + 0.75y, x unchanged: glyphs 0.75 tall); mode 2 leaves the HUD
  in its 4:3 framebuffer position (a 16:9 TV widens it 4/3). Measured on PS2 frames
  local/ps2-capture/snaps/event-start-wsmode{1,2}.tick{79,268,619}.png against event-start.tick268.png (bars 59/60 of
  480 rows; timer rows 23..35 -> 78..86).
- Reproduce: derived copies of the event-start capture state with ctx+0x6B94..+0x6BA4 and the profile bits patched as
  0x377950 would (local/ps2-capture/snaps/event-start-wsmode{1,2}.p2s + .patches.json), run with
  `tools/ps2_capture.py run ... --snap 60,250,600`.

Browser: web/widescreen.js (mode table, `originalProjection`, `widescreenView`, localStorage "ssx3.widescreen"),
pause Options "Widescreen" row (web/ui.js), `layoutStage` in web/main.js, CSS in web/style.css. Off = 4:3 stage
(default); 16:9 = the PS2's literal 4:3 frame (3D canvas and in-race HUD canvas in the 12.5%/75% band); Anamorphic =
16:9 stage, 16:9 camera aspect, HUD canvas stretched 4/3 as a 16:9 TV shows it. Camera vertical fov is unchanged in all
modes. Test: web/test-widescreen.mjs (in npm test). Not verified: whether the pause/front-end menus letterbox in mode 1
(capture log stops while paused); snow billboard cap (snow-billboard.js) still assumes a 448-line source viewport, so
in mode 1 its projected-size cap uses the 4:3 y scale instead of 0.75x.

## Word-level PS2 parity (2026-09-23)

Every camera word is now compared with the PS2 on every tick, and each gated capture matches.

- **Layout.** `engine/original_camera_words.hpp` holds the shared word mapping, which was moved out of the oracle tests:
  - `toWords`/`fromWords`/`assignWords` cover the 228 words of the DEFAULT_3 object (0x390 bytes). Spline flag words are compared on bits 0..26, which are the only modelled bits.
  - `compositorOffsets` lists the 43 words of the outer camera: lookAt +0xE0, eye +0x100, lift +0x460, last probe normal +0x470, the shake request +0x450..+0x45C, and the shake walk comp+0x254..+0x2B4 and +0x2EC (= outer +0x314..+0x374, +0x3AC).
- **Core exports.**
  - `camera_state_words()` returns the 271 words.
  - `camera_seed_words(words, mask)` overwrites the masked words right after the camera is constructed.
  - `visual_rng_words()` gives the visual RNG 0x4FF018 (snow, stage effects and camera shake draw from it).
- **Event start.** `tools/generate_event_seed.py` extracts the countdown-anchor camera words for each course into `generated/event_instance_seed.hpp` (`browser_event_<LOC>::camera`). The anchor is at game tick 18. The core applies these words before the camera step that follows its 18th event tick; `motionTick` counts that tick, so the test is `motionTick > anchor`. This removed the old 2.5 cm event-start offset. The four set-targets that could not be explained were really the PS2's pre-anchor history, which the browser cannot have.
- **Fixes found by the word comparison:**
  - A crash from the air restarts the rider predictor (136C40) before that tick's camera step. `browserTrajectory` is now synced at crash entry, so the landing angle is 0 and the landing decay is 0.93 (event-race 899).
  - During a crash, the camera surface (rider+0x438) is the crash actor's contact surface (pipe-air 793).
- **Comparer** (`web/compare-ps2-capture.mjs`):
  - Summary fields: `cameraWordTicks`, `cameraWordTicksExact`, `firstCameraWordMismatch`, `cameraWordFirst`.
  - `--camera-seed` seeds mid-run baselines from record 0. The upper words come from a `--watch <camera>:0x390` window.
  - `--camera-variant` is now applied before the first step.
  - `--camera-shake-sync` adopts the PS2 shake walk when only the visual RNG stream differs.
  - `--sync-visual-rng` copies the watched 0x4FF018 at each tick start.
  - Debug switches: `CAM_TRACE`, `CAM_IN_DUMP`, `CAM_INPUT_TRACE`, `VISUAL_TRACE`.
- **Gates** (`web/test-ps2-captures.mjs`, `cameraThrough: END`): cam-event-start, cam-event-race, cam-mix-glide (Mid), cam-mix-glide-0x3C (Near), cam-mix-glide-0x3E (Far), cam-air-tricks, cam-rail-balance-lr, cam-pipe-air and cam-metro-mix-glide. Every word matches on every tick. Captures for other courses use the camera address from the manifest.
- **Shake.** Given the same RNG state, the shake is exact. At the event-race 940 shake start, the PS2 walk equals the visual RNG draws 8..19 of that tick. The PS2 draws about 10 visual numbers per tick; the headless comparer draws 2, because stage-world effects are not loaded there. Exact shake in the browser therefore depends on every visual-RNG consumer (stage scripts and particles) drawing in the PS2 order.

### Shake and the visual RNG order (open, 2026-09-23)

- **Record boundary.** Each record is taken at the human provider exit, which comes after that tick's world object pass. `--sync-visual-rng` therefore copies the words after `race_begin`, the browser's world pass. Copied at the start of the tick instead, the stage-effect draws appear one tick late.
- **With that alignment,** single-rider captures (cam-pipe-air) draw the same number of visual numbers per tick as the PS2, except at crash entries. The PS2 draws 7 numbers before the camera's shake start (pipe-air tick 713); the browser draws 2. The snow crash burst is missing before the camera.
- **Computer riders.** With computer riders in the capture (mix-glide, air-tricks, event-race), their snow draws from the same PS2 stream between the human's provider exit and the camera (offset 11 at mix-glide 648). The browser gives each rider core its own visual RNG.
- **Current gate.** The shake math is exact given the same stream position; the event-race 940 and mix-glide 648 offsets were checked by hand. Until the visual-RNG consumers draw in the PS2 order, the camera gates adopt the PS2 shake walk (`--camera-shake-sync`).
