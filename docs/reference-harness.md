# Deterministic original-game comparison harness

The user configured PCSX2 v2.8.2 and authorized it as a development reference, including ROM/RAM hacks where useful. It is not a dependency of the native Metal app.

## Implemented

`tools/reference_replay.py` builds version-1 P2M2 files from a complete machine-state snapshot plus an explicit per-frame controller timeline. The format was checked against the official PCSX2 v2.8.2 source (`fd9d310ccbb6b8b62c976da8886a3c8fd3a10ff3`). Both ports, digital buttons, all four shoulders, pressure bytes and analog axes are represented. PCSX2 pauses exactly at the final replay frame.

The tool also reads snapshot memory and compares outcomes, and applies guarded RAM patches to derived snapshots. All expected bytes are validated before mutation. The isolated airborne experiment below uses these patches; neither original disc image has been modified.

## Verified repeatability

The original intro fixture restored one snapshot, pulsed Start on frames 20–79, and stopped at frame 360. Its immutable inputs now live under `baselines/`; `intro-anchor.p2m2` is a reusable UI launch filename whose manifest identifies the current scenario. Two independent replays produced **byte-identical**:

- 32 MiB Emotion Engine memory
- I/O processor memory
- Graphics Synthesizer state
- Captured screenshot

The private report is `local/reference/pcsx2/title-repeatability.json`; the outcome states are `title-outcome-1.p2s` and `title-outcome-2.p2s`. On September 8, the same four components also matched byte for byte across two **30-frame active Snow Jam glide** runs, recorded in `glide-repeatability.json`. PCSX2 reported MTVU enabled. Repeatability is established for these cases, not universally for every scenario.

## Gameplay checkpoints and native comparison

`snow-jam-ready.p2s` is the loaded race information overlay with six live actors. The scenario `snow-jam-start-360.json` presses Cross on frames 20–34, then waits until frame 360. Its outcome `snow-jam-glide.p2s` is a post-gate, upright Zoe moving at approximately 42 mph. This avoids repeatedly navigating Single Event and avoids the closed start gate in the current native static world. The earlier 600-frame outcome is in wipeout mode and is retained separately.

`tools/reference_probes.py` locates derived rider objects through their verified interface pointers and exports source Z-up position in centimeters, quaternion, native Y-up position/velocity, rider slot and active motion mode. Original source `(x,y,z)` maps to native `(x,z,-y)`, with centimeters converted to meters. Slot zero is the human; slot is not character identity.

Consecutive `snow-jam-glide` and `snow-jam-glide-1` states establish rider+0x1E0 as centimeters/second: the old vector divided by 60 predicts each position component within 0.014 cm. This agrees with original cruise integration using old velocity before acceleration. Remaining differences include float quantization and contact correction. Rider+0x77C points to a motion owner whose +0xDE0 dispatches cruise mode 0 and wipeout mode 2; confusing these routines would produce incorrect physics constants.

`tools/compare_reference.py prepare` adds measured initial position, velocity, board heading and grounded/airborne mode to an input scenario. Native terrain supplies the ground normal; hidden original controller state is not transferred. `compare` verifies stimulus bytes and measures position/velocity errors at named checkpoint frames:

```sh
python3 tools/compare_reference.py prepare local/reference/pcsx2/glide-30.json \
  local/reference/pcsx2/snow-jam-glide.p2s local/reference/pcsx2/glide-native-30.json
python3 tools/native_replay.py run local/reference/pcsx2/glide-native-30.json local/native-qa/glide-30
python3 tools/compare_reference.py compare local/reference/pcsx2/glide-native-30.json \
  local/native-qa/glide-30.json --checkpoint 30=local/reference/pcsx2/snow-jam-glide-30-a.p2s \
  --output local/reference/pcsx2/glide-native-comparison.json
```

The first joined 30-frame neutral case starts with zero position/velocity error and ends at 0.0435 m position error and 0.318 m/s velocity error. These are measurements, not a fidelity claim: 30-frame full-left steering differs by 0.953 m and 5.898 m/s; braking differs by 0.241 m and 1.961 m/s. A 120-frame glide exposed a false native obstacle contact: the original resource names identify the offending model as `mdl_ARA1_startfireTrig_1000`. The native importer now retains named trigger geometry separately from visible/collidable scenery. This removes the abrupt stop; remaining glide error reflects provisional movement rather than that barrier.

`tools/reference_suite.py --output local/native-qa/reference-suite` runs all captured input cases headlessly against a stable native binary. The report fingerprints the binary and ARA1 package, verifies every input byte, and retains per-checkpoint original/native state and errors. It rejects a run if the binary or assets change during measurement.

## Isolated airborne experiment and recovered integration

`tools/reference_air_fixture.py` derives a state from the already-airborne `snow-jam-jump-31.p2s`. It moves the actor and trajectory origins 50 meters above the course, sets upward velocity to 10 m/s, and reproduces the verified trajectory-reset writes from original functions 0x113198 and 0x113618. Sixteen guarded RAM writes and the source hash are recorded alongside the derived file. Existing course, AI and rendering state remain present; elevated placement isolates this short flight from their contacts.

```sh
python3 tools/reference_air_fixture.py local/reference/pcsx2/snow-jam-jump-31.p2s \
  local/reference/pcsx2/snow-jam-air-isolated.p2s
```

Airborne motion dispatch 0x139A20 calls trajectory updater 0x113648, whose 60Hz integration helper is 0x1139A0. Its original source-coordinate equations are:

- Update position using **old** velocity times float32(1/60).
- Add velocity times float32(-0.2/60) to each horizontal component.
- Add approximately -850/60 cm/s to rising vertical velocity, or -1900/60 cm/s when vertical velocity is zero/negative.
- Clamp speed using the trajectory's configured maximum (3333.333496 cm/s in the captured cases).

Exact constant bits are recorded in `tools/air_motion_reference.py`. Applying per-operation float32 rounding toward zero reproduced all three position and all three velocity fields **bit for bit** at the 29- and 59-frame falling checkpoints, and at the 60- and 120-frame isolated checkpoints spanning ascent, apex and descent. Reports are `air-falling-conformance.json` and `air-rising-conformance.json`. The integrated native controller also matches those checkpoints exactly. A further derived state with upward velocity30m/s exercises the speed cap; the native controller matches all six fields exactly after that tick (`air-capped-comparison.json`). The offline Python helper intentionally remains scoped to the uncapped path. Launch impulse, landing response and surface interactions still require separate conformance.

With the recovered airborne code, full jump/release error at frame90 fell from6.346m/11.453m/s to1.915m/1.842m/s. Starting at the already-airborne frame31 eliminates that error entirely at the measured checkpoints. This isolates the remaining full-jump discrepancy to the provisional launch/ground state rather than the recovered free-flight equations.

```sh
python3 tools/air_motion_reference.py local/reference/pcsx2/snow-jam-air-isolated.p2s \
  --checkpoint 60=local/reference/pcsx2/snow-jam-air-isolated-60.p2s \
  --checkpoint 120=local/reference/pcsx2/snow-jam-air-isolated-120.p2s \
  --output local/reference/pcsx2/air-rising-conformance.json
```

## Use

```sh
python3 tools/reference_replay.py build scenario.json local/reference/pcsx2/case.p2m2
python3 tools/reference_replay.py inspect local/reference/pcsx2/case.p2m2_SaveState.p2s
python3 tools/reference_replay.py compare outcome-1.p2s outcome-2.p2s
python3 tools/reference_replay.py patch baseline.p2s patched.p2s patches.json
```

A scenario contains `baseline`, `frames`, and `events`. Each event has a half-open `[start,end)` frame range, optional button names and optional `left`/`right` analog byte pairs. The neutral analog value is 127. Patches contain `address`, `expected` hex bytes and same-length `replacement` hex bytes.

Load the generated movie with PCSX2 Tools → Input Recording → Play, then unpause. A matching `movie.p2m2_SaveState.p2s` is required; the builder creates it. Stop recording after each completed case before selecting another. Do not resume past the end: PCSX2 can switch into rerecord mode and extend the movie.

## Next

Extend the existing course-start, glide, steering and braking cases with jump/landing and grab checkpoints. Recover native surface/contact rules and original motion equations against these measured cases. Add targeted patches where needed to isolate AI, timing, randomness or a specific animation; verify original bytes and keep the patch manifest with each case.

## UI notes

The running reference app uses a macOS translocation path. Its bundle identifier is ambiguous with the downloaded app, so use the full running path returned by CUA. CUA keyboard taps did not reliably reach emulated game input; P2M2 playback successfully delivered exact inputs. Native macOS file dialogs work reliably through Go To Folder with a full filename. Qt text fields bound to `textEdited` need a paste/type edit rather than AX `setValue` alone. Creating a recording through the radio-button dialog produced a power-on recording despite the displayed Save State selection; the working harness therefore uses an explicit Save To File snapshot and writes the movie's state-start flag itself.
