# Native engine status

> **Route change — 2026-09-09:** The user has now authorized resuming the GameCube engine route, preserving original game code compiled ahead of time with platform/graphics adapters. `gamecube/` is the active delivery path; `engine/` remains a preserved native research/playable prototype. First milestone: original menus and one playable run, not another full-engine rebuild. See [GameCube continuation](gamecube-continuation.md).


> Current integration checkpoint: [Agent handoff — 2026-09-09](HANDOFF.md). The measurements and research below describe their recorded stages; older missing-feature notes and test counts are not the latest app status.

The product target is a full SSX3 port on macOS: direct Metal rendering and native
gameplay, input, audio, saves and game systems. User-owned PS2 and GameCube assets
may be combined. All four PS2 shoulder buttons remain independent. An emulator,
console renderer or guest CPU runtime is not part of the product architecture.

The current build is a controllable course/rider engine with substantial recovered
movement and animation. It is not the complete game, and full gameplay parity is
still an active goal.

## Native implementation

- `engine/` builds an ARM64 AppKit/MetalKit application. Rendering uses native Metal
  buffers, shaders, textures, indexed meshes, skinning, MSAA and depth testing.
- Six imported areas contain original terrain and placed scenery. Native world
  coordinates are meters, Y-up; import applies `(x,y,z) → (x,z,-y)` after original
  source instance transforms. Base geometry/textures come from PS2; matching GC
  lightmaps retain their own UVs and atlas IDs. Source coefficients are checked.
- Original analytic terrain contact uses authored patch flags, original coarse
  cells/triangle order, cache identity, candidate selection and refinement.
  Separate authored world collision geometry and body-sphere response are being
  integrated. Visible scenery is not presumed to be authoritative collision data.
- The profile-backed rider runs recovered cruise, charge/release, trajectory,
  orientation and input equations at 60 Hz. `riding-start.json` provides owned
  per-area initialization data; ARA1 has a reference-backed profile. Areas without
  the required original profiles retain an explicitly provisional fallback.
- Native keyboard/controller mapping follows original PS2 input processing and
  preserves all 16 L1/L2/R1/R2 combinations. GameController supplies hardware input.
- Replays accept deterministic 60 Hz input events and emit JSON/CSV physical,
  input, contact and body-collision telemetry. Accepted original controller traces
  can also be replayed for numerical comparisons.

See [movement recovery](../engine/MOTION_RECOVERY.md),
[replay interface](../engine/REPLAY.md), and [reference harness](reference-harness.md)
for current evidence, fields and remaining algorithm gaps.

## Rider and animation

Mac remains available. Zoe is the default for a fresh configuration and the
reference-backed body-pose work; saved rider selections remain respected.
`tools/rider_assets.py --rider zoe` imports the original live LOD0 model selection:
TopB, BottomB, HeadA, HandsB, BootsA, bindings, board and Mop hair. It preserves
material-specific batches and source bone float bits. Zoe's package contains
3,058 vertices, 2,954 triangles and 27 bones. Texture color variants are still test
choices, not a recovered career equipment system.

The new native animation path reads original compressed curve packets directly.
All 497 basic entries are available; the separate frontend bank is inventoried.
The native sampler, spherical quaternion conversion, priority/mask mixing and
source-space FK generate the pose. Across thirteen recorded ground, airborne and reverse-stance states, all 27 local bone
positions/quaternions are bit-identical to the original. Original-code tests also
cover every curve packet and randomized FK.

Board alignment, two-link leg IK, lean/pivot presentation and body lift are
recovered. Normal, brake and charged-jump selection, cycle clocks and fades run
natively. Dynamic turn30, brake30 and charged-jump30 runs generate all ten body
collision centers exactly. Reconstructing ground poses directly from a post-frame
snapshot retains up to 0.125 cm error because the original updates board normal
`+0x390` after that frame's pose; the native controller preserves this ordering.
The longer glide still misses a new randomized head-check event, producing up to
3.89 cm head/arm error despite exact lower-body centers.

Five airborne snapshots now reproduce all 27 world positions and quaternions
bit-for-bit using the generated animated pivot and original presentation transform.
Full accepted charged-jump replays through frames31/60/90 reproduce all24
body/board world transforms and main sequence clocks/fades exactly. Neutral
landing continuation now preserves generated poses after touchdown; longer-run
physical and reverse-stance comparisons remain active.
Unsupported animation branches remain explicit. Renderer and collision use the
same generated world pose; expected captured bone arrays are test outputs only.

Original opponent packages are also available for Psymon, Allegra, Moby, Griff
and Luther. Their selected models and bind rigs match the owned source, and all
138 opponent local bone transforms match the initial reference poses. Body sphere
counts/radii are verified for each opponent. Shared multi-rider runtime integration
is underway; these assets alone do not establish opponent gameplay parity.

See [animation and procedural pose recovery](../engine/ANIMATION_RECOVERY.md).
Original morphs, full state selection, secondary hair dynamics, the complete roster
and equipment rules still need work. The older expanded-frame clips remain available
as inspector previews.

## Build and check

```sh
.venv/bin/cmake -S engine -B build/metal-engine -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$PWD/.venv/bin/ninja" -DCMAKE_BUILD_TYPE=RelWithDebInfo
.venv/bin/cmake --build build/metal-engine -j8
.venv/bin/ctest --test-dir build/metal-engine --output-on-failure
python3 -m unittest discover -s tests -p 'test_*.py'
```

The app is `build/metal-engine/ssx3_metal.app`. `SSX_LOCATION` selects an area;
`SSX_MODE=inspect` starts in inspection mode. `SSX_RIDER_PACKAGE` selects an available
rider package. Assets remain ignored under `local/`; standalone asset preparation
and distribution packaging are unfinished. Original-code conformance executables
are separate development targets and never link into the application.

Headless Metal checks create no window and do not change application focus:

```sh
build/metal-engine/ssx3_scene_audit ARA1 /tmp/ssx-riding.png 240
build/metal-engine/ssx3_scene_audit --replay scenario.json \
  --telemetry /tmp/native-result --image /tmp/native-result.png
```

WASD/arrows or left stick steer/crouch/brake; hold/release Space or Cross/A charges
and jumps. Shift or Square/X boosts; Q/Z/E/X expose L1/L2/R1/R2. R respawns, P/Escape
pauses, V switches inspection/riding, K reloads. Focus loss pauses motion. UI and unsupported clip transitions remain development behavior.

## Camera, crash recovery, rails, sky and fog

The riding view uses the recovered original DEFAULT_3 chase camera
(`engine/original_camera.hpp`, `engine/CAMERA_RECOVERY.md`), stepped once per
60 Hz gameplay tick.

Hard crashes (control 8 / motion 2) are integrated: landing classification and
scenery impacts enter the original ragdoll, sliding and get-up phases, with the
recovery meter fed by holding jump; see [crash motion](crash-motion.md).
Authored rails are loaded per area and the original control 7 / motion 4 grind
lifecycle attaches, balances and exits (see `engine/RAIL_RECOVERY.md`).
Each area draws its original sky dome around the camera before the world
(`tools/import_sky.py`, `engine/sky_asset.mm`) and tints distant terrain with the
painted start fog colour. Lightmaps use a 1.75× gain approximation of the
GameCube TEV scale.

## Remaining game systems

Complete surface/course coverage, obstacle callbacks, rail transfers/tricks, handplants,
attacks, forced resets (control 9), trick selection/scoring, event logic, opponents/AI, menus, progression, audio,
music transitions and native saves are not complete. Full playthroughs and hardware
controller QA remain necessary. Isolated numerical matches do not establish full
gameplay or visual fidelity. The old PS2/GameCube AOT work is reference tooling only.
