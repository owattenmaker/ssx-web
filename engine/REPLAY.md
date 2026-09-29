# Native deterministic replay

The headless scene audit accepts the same `frames` and `events` fields as `tools/reference_replay.py`. Each frame is 1/60 second. Ground/contact scheduling uses two native 1/120-second half-steps; airborne motion uses one recovered original 1/60-second trajectory step. It never creates or focuses a window. Grounded movement, launch/contact response, camera, analog normalization and clip selection remain **provisional**. The airborne integrator matches isolated original trajectories bit-for-bit; equal input bytes alone do not establish full original-game behavior fidelity.

```sh
python3 tools/native_replay.py run scenario.json local/native-qa/run-a --image local/native-qa/final.png
```

This runs the existing native Metal scene audit and verifies every applied pad against the reference movie encoder. It writes `run-a.json` and `run-a.csv`. The image is optional. The equivalent direct call is:

```sh
build/metal-engine/ssx3_scene_audit --replay scenario.json --telemetry local/native-qa/run-a
python3 tools/native_replay.py verify-inputs scenario.json local/native-qa/run-a.json
```

The scenario may retain the reference harness's `baseline` field; the native run does not read or modify that machine state. Native initialization is an additional object:

```json
{
  "frames": 180,
  "events": [
    {"start": 0, "end": 120, "left": [127, 0]},
    {"start": 30, "end": 60, "buttons": ["Cross"]},
    {"start": 60, "end": 75, "buttons": ["L1", "R2"]}
  ],
  "native": {
    "location": "ARA1",
    "initial": {
      "position": [-1318.65234375, -2287.708125, -138.585],
      "velocity": [0, 0, 0],
      "heading": -1.405,
      "normal": [0, 1, 0],
      "grounded": true
    }
  }
}
```

The example position is the measured human Snow Jam start from the live PS2 reference, converted from source Z-up centimeters to native Y-up meters. The heading is provisional until forward-axis behavior is confirmed. Position uses meters, velocity meters/second, heading radians, and world coordinates are Y up. Heading zero points toward +Z; positive angles point toward +X. `native.initial.position` restores the exact position without projection or a hidden board-height offset. `velocity`, `heading`, `normal` and `grounded` are optional. Normals are normalized. Without explicit `grounded`, a short terrain probe infers contact for an exact position.

For geometric placement, `native.spawn: [x,y,z]` projects onto terrain from 5m above to 25m below the requested point, equivalent to `SSX_SPAWN=x,y,z`. This is separate from exact initial position. The default geometric spawn includes the controller's 0.025m clearance. `SSX_VELOCITY=x,y,z` and `SSX_HEADING=radians` also work in the replay path. Explicit scenario initialization overrides these environment defaults. Resolved initial state and relevant environment values are recorded in the telemetry. Reproducibility requires identical assets, native binary, scenario and environment.

Event ranges are zero-based and half-open: `[start,end)`. Overlapping events union buttons; the last active event specifying a left or right stick pair overrides that pair independently. Supported names match the reference harness: Select, L3, R3, Start, Up, Right, Down, Left, L2, R2, L1, R1, Triangle, Circle, Cross and Square. All 16 shoulder combinations survive. Start toggles native pause on its press edge; recording frame/time advances while paused, but simulation time/state remains paused.

Analog input keeps its exact source byte pairs in telemetry. Native normalization treats 127 as neutral, scales the negative range by 127 and the positive range by 128, and flips Y so byte 0 means up. No original deadzone is claimed. The input mapper then applies the recovered PS2 INPUT.MAP relationships.

Every run has `frames + 1` telemetry rows. Row 0 is the state before any input (`input_frame = -1`). Row N+1 is the state after applying input frame N. Rows include position, velocity, speed, heading, normal, grounded/contact state, jump held/pressed/released, jump charge, pause state, mapped control values, grab mask, raw stick bytes, active-high PS2 button mask and all 18 reference pad bytes as hex. Jump edge fields describe input edges; actual launch/landing is reflected by position/velocity/grounded state. Timeline time and simulation time are separate for pause analysis.

Validation includes synthetic-terrain tests for range boundaries, overlapping axes, all shoulder chords, pause edges, exact initial-state restoration, invalid input rejection and byte-identical telemetry serialization. Two independent 300-frame ARA1 runs produced byte-identical CSV/JSON, and all 300 pad frames matched the reference encoder. These checks do not compare native movement to PS2 rider state yet.

`obstacle_contacts` in each JSON row preserves individual contact substeps: triangle source index, hit point/normal, probe origin/height, simulation time and velocity before/after response. Resolve triangle spans through the package's `collision_sources` table to original instance/model names. `airborne_ticks` counts recovered trajectory steps and `air_speed_capped` identifies the most recent airborne tick's speed-cap branch. The emitted metadata distinguishes original 60Hz airborne integration from provisional 120Hz grounded simulation; see [motion recovery scope](MOTION_RECOVERY.md).
