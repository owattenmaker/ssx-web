# Original animation and snow gameplay integration

The native preview’s ARA1 development start now binds GameplayAnimation and GameplayEffects. Original disc animation packets run through recovered ground, passive-air, grab and air-selection controllers, rather than a clip-name chooser. The same evaluated pose drives collision and the rendered character; Mac and Sam retarget these original layers to their own bind rigs. The standalone animation browser is not the gameplay path.

Snow uses recovered trail/chunk/impact/cloud emitters, authored effect profiles and textures, source particle simulation, and the six-band board-track geometry. Track masking/depth passes use Metal stencil; raw UNORM composition preserves source-style colour arithmetic. Dynamic environment sampling uses original PS2 terrain texture data. Unknown texture guard samples retain the last valid light colour and log a diagnostic.

Validation during integration: all 15 shoulder-button grab combinations completed 90 gameplay frames without unsupported-animation errors. A 180-frame Sam jump/grab replay took off at frame31 and landed at116, with no missing airborne collision poses. Mac braking and Zoe air steering also completed with native effects and rendered images. These are focused gameplay tests, not full-game or pixel-perfect parity claims.

Remaining work includes complete crash/rail/handplant/attack gameplay, score commits and progression, original camera and broader presentation fidelity. Rock/breath/kicker/body producer families and live wake entities are not connected. The current development start uses Zoe’s source controller/profile even when retargeting Mac or Sam. Other course areas lack equivalent original event initialization. Neither the original game’s source code nor an emulator is bundled into the native app; recovered behavior is implemented in native code with locally extracted data.

## Held jump / ledge fix (2026-09-09)

Control2 now keeps its prewind animation when riding off a ledge while holding jump. Releasing while airborne enters control5 without another launch impulse or predictor reset. Landing while still holding jump preserves control2 and uses the original impact gate for landing-animation selection. `native_held_jump_ledge` covers both paths through the gameplay session; all18 native tests passed (17 existing plus this new test). The installed preview includes the fix. See [the handoff](HANDOFF.md) for source addresses and reproduction evidence.

## Crash, recovery, sky and lighting (2026-09-09)

Hard crashes now run in gameplay: inverted landings and hard scenery impacts enter the original control 8 / motion 2 ragdoll, slide, recover and return to riding (see [crash motion](crash-motion.md)). Soft scenery impacts enter control 3 through `105D98`/`108388`; hazard surfaces, the direction-change accumulator and the recover input raise a reset request that the preview honours by respawning. Attack and handplant inputs are ignored with a one-time log rather than pausing play.

Presentation: the original per-area sky dome (`tools/import_sky.py`, drawn around the camera before the world) replaces the flat clear colour, the painted start fog colour tints the horizon (half-strength preview blend), and GameCube lightmaps are applied with a 1.75× gain so shadowed snow is no longer navy. The start-area orange chevrons and blue lane stripes are opaque terrain patches in the source data, not blended decals.

## Rails (2026-09-09)

Grinding runs in gameplay. `tools/import_rails.py` exports the authored SSB kind-8 rail splines (ARA1: 171 rails, 777 segments) into `rails.json`; `engine/rail_motion.hpp` holds the recovered spline query, attach test, motion 4 and control 7 (see [RAIL_RECOVERY.md](../engine/RAIL_RECOVERY.md)). The controller tries `0x106848` attachment from the cruise, crouch, air and passive-air controllers each tick, runs `0x13AF28` while on the rail (balance drift, lateral slide, lean/heading alignment, boost acceleration along the rail), leaves through `0x13BFA8` into the airborne controllers, and rotations use the spin axis. The rail cycle semantics 18/19/20 run the recovered kind-5 driver: `RS[_FS|_BS]_BAL_R/FWD/BAL_L_CYC` weight blends on the rider+238 balance (`engine/RAIL_ANIMATION_RECOVERY.md`). `native_rail_grind` places the rider before `spline_ARA1_RAIL_3007` and checks attach, 190+ grinding frames, exit and landing. Transfers, trick identity on rails, dynamic-instance rails and rail scoring remain unrecovered (scoring is recorded as events only).

Crash snow (same day): the original BodySnow producer and crash impact triggers are integrated (`engine/SNOW_CRASH_RECOVERY.md`), so sliding wipeouts spray snow from the body while the impact buildup lasts.
