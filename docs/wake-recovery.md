# Original snow wake recovery

Current status: the browser now generates a live wake physics ring and supplies its newest tip to snow chunk emission. Empty rings use the original no-wake branch; a retained zero tip remains a valid wake result. The separate wake ribbon rendering and its colour/UV preparation remain unfinished. No captured future wake rows are injected.

## Consumer and layout

`2E02B8` checks the surface's +78 flag and the count at motion-owner+3B4. For a nonempty ring, `2E08FC..2E0938` reads the vector at:

```
(owner+3B0)->records + head*0x70 + (columns-1)*0x10
```

That is the tip vector of the newest retained wake row. There is no new spatial/world query at this point. This corrects the earlier generic “wake-query cache” description. The resulting vector is the optional wakeVelocity consumed by originalSnowChunkEmission, which changes both velocity construction and subsequent visual-LCG consumption.

`2DCF28` configures human/device riders with5 columns and32 rows; nonhuman riders use4 columns and22 rows. Each physics row has0x70-byte stride: column vectors at+00..40, a drag vector at+50, a caller value at+60 and zero-initialized+64. The separate render vertex buffers use48-byte vertices per row/band. Initial count/head/phase are cleared by source reset.

## Recovered and verified row creation

`engine/wake_row.hpp/.cpp` contains:

- `originalWakeNoise` (`2D18B0`) with the161-entry table rooted at445AB0,160-cell periodic indexing and source scalar interpolation. Its supported wake-phase domain is nonnegative with a valid signed source index.
- `originalWakeOctaves` (`2D1928`). Row creation requests3 octaves. Preserve the final addition of the last weighted sample: the original includes it after the loop's delay-slot accumulation.
- `originalWakeAdvanceCursor` (`2DE058..2DE0D4`): phase increases by0.033333335 and wraps once at4; count saturates at configured capacity; head decrements with wrap. Later colour/vertex initialization in2DE058 is not part of this helper.
- `originalWakeRow` (complete2DDD30 payload/render-anchor computation). It computes the noise-scaled fan from the live coefficients at+60/+70 and amplitude+90, including the final-column0.9 factor, the drag term from+80 and rider velocity, repeated point−normal*5cm anchors, and alpha+94 converted to GS scale128. It retains the caller's+60 value and clears+64. Callers own ring storage and unchanged UV/RGB fields.

Some coefficient names deliberately retain source offsets because their upstream construction in2DD0B8 remains unrecovered. Do not infer replacements for them from similar-looking velocity fields.

`python3 tools/test_wake_row_native.py` compares20,000 cursor advances and complete row births against the original executable, including human/nonhuman configurations, every generated fan/drag/payload value, all repeated render anchors/alphas and unchanged UV/RGB.48 additional original noise cases cover period boundaries and0..5 octaves. All compare exactly under the captured chop rounding policy. The cursor oracle stops at2DE0D8; the row-creation oracle executes the complete2DDD30 and its real noise callees. Log: `local/wake-row-reference.log`.

## Remaining integration

Recover the live frame driver2DD0B8, the remaining2DE058 row/colour initialization and2DDAB8's update role. These must produce/age the actual retained rows from current rider pose/contact/control state before snow emission. Then expose the newest row tip to the existing snow context and remove the wake-gap flag only when that path is available. Wake rendering using the original `wake` texture is also not connected. No browser behavior changed in this recovery pass.

## Verified aging and profile initialization (September12)

`originalWakeAgeRows` now implements2DD0B8..2DD2A0 and the2DD378 expiry branch. It processes retained rows newest first, increments age by the original1/60 constant, truncates count at the first row strictly older than the lifetime, and leaves later rows untouched. Surviving render positions receive the decaying drag contribution; columns1..N−1 also receive their velocity term and source per-column vertical increment. The short initial growth term from row+60 and the source EE/VU operation ordering are preserved. Row drag/value fields and render UV/RGB/alpha remain unchanged.

`OriginalWakeRow.positions` holds mutable render-band positions initialized from the birth anchor. `anchor` remains the birth-only anchor, not the current band position after aging.

`originalWakeProfile` recovers2DCF28 configuration constants: device-bound riders use5 columns/32 rows/1.25 lifetime; riders without a device use4/22/0.8000000119. All five stored vertical increments and initial texture-V values are computed with the original division/multiply/subtract order. These are initialization values; the caller must preserve the source quality-change condition when refreshing an existing render buffer.

Validation: tools/test_wake_age_native.py matches20,000 original aging passes exactly, with8,747 expiry cutoffs. It checks every row's positions/velocities/age, unchanged payloads and render fields, and owner-state isolation. The oracle exits at both2DD2A4 and2DD2A8: the empty-cache branch skips2DD2A4. An initial single-boundary harness let that case execute later code; it was corrected and revalidated, with an explicit escaped-stage call guard and byte checks. Latest log:local/wake-age-reference.log contains no missing-target warnings.

The extended tools/test_wake_row_native.py also verifies device/non-device dimensions, lifetime, all vertical increments and initial UVs against2DCF28, while retaining the20,000 birth/cursor and48 noise checks. Log:local/wake-row-reference.log.

Still missing: the remaining2DD0B8 input/coefficient calculation and new-row state machine, remaining2DE058 colour/vertex preparation, and2DDAB8's role. Browser wake input remains unavailable until the live cache is generated by those routines. No browser behavior changed in this pass.


## Live targets, control and browser integration (September12)

`wake_input.hpp/.cpp` recovers2DD2A4..2DD6F0 before mode/eligibility filtering. It uses the scaled board axis from geometry+34 and unit axes/origin from geometry+30, ground normal/lateral, velocity, effective turn1F0, extra lean208, brake214, stance320 and surface48/4C/84. The offset names were checked against reference_ground_profile:208 is extra lean,214 brake and274 presentation roll. The initial “crouch/balance” names were corrected before integration.

`wake_control.hpp/.cpp` recovers2DD6F4..2DDA88: motion/roll/strength/turn/material gates; side reversal;0.8/0.2 vector and amplitude filtering;0.7/0.3 alpha filtering;50cm row advancement; closing rows; and two initial rows with zero amplitude. Callback tests capture the live control state at each row request, not only final values.30,000 cases match exactly, including3,808 advances and16,974 row requests. Target computation separately matches20,000 cases.

`OriginalWakePhysics` composes aging, targets, controls, cursor advancement and row creation. A10,000-frame comparison runs original2DD0B8 with original row/noise functions, substituting only the cursor prefix for2DE058’s later render-colour/UV work. All retained physics rows, render positions, count/head/phase and control amplitudes match;9,980 frames have a tip. This does not validate wake colour/UV initialization or drawing. Scripts/logs:tools/test_wake_{input,control,physics}_native.py and local/wake-{input,control,physics}-reference.log.

The browser imports the original161-float noise table, initializes the device-specific cache, and updates it once per visual/motion tick before chunk emission. It uses live pose/control/material inputs and the current browser unit-scale board convention; full authored-scale parity remains separate. It supplies a tip whenever count>0, even if the vector is zero. Reset clears the cache. snowWakeUnavailable is now false when this implementation is ready. The separate snow secondaryBrake274 input was corrected to presentationRoll.current.

Actual-WASM tests generate30 retained rows and nonzero tip velocity under carving, verify the initial zero-amplitude cap row, expiration after carving stops and clearing on reset. The full browser suite/build pass. A live150-sample carving/reset check observes32 rows, nonzero velocity, no unavailable flag, one completed reset, no new rescues and60–63fps.

Remaining: full2DE058 colour/UV preparation and the separate rendered wake mesh/2DDAB8 role, original renderer parity, authored scaling and complete original-game frame parity. Earlier notes that the live input/control stage or browser wake physics are missing are superseded by this section.


## Render preparation and live ribbon (September 12)

`engine/wake_render.hpp/.cpp` now recovers the remaining 2DE058 colour/UV preparation: doubled/clamped environment channels, GS RGB quantization to 128, signed repeating U with source truncation, and one counter increment per row advance. Row recreation preserves the UV/RGB established at advance. `originalWakeDrawWindow` recovers 2DDC68..2DDCE4: oldest-row lifetime fade, head, capacity-minus-two count limit and per-row fade decrement. The source 2DDAB8 is the render dispatch, not an additional physics updater.

`tools/test_wake_render_native.py` passes 20,000 original-code cases for colour/UV preparation and draw windows, including all five retained render buffers. This is separate from the 10,000-frame composed physics oracle, which still substitutes the colour preparation call. Neither test proves full GS draw parity.

The browser now uploads the retained row positions, source UV/RGB/alpha and adjacent-column strips to `web/wake-renderer.js`, using original texture 56. Human geometry is bounded to 696 vertices. `_wake_info` has a 13-float prefix; index 11 is vertex count and index 12 is a monotonic render generation. The resettable physics-update counter at index 10 must not be used to cache geometry across restarts. Restart clears geometry and advances the render generation.

Validation: rebuilt production demo at port 4173; actual-WASM carving/expiry/restart test passes, including populated-geometry reset invalidation. Live WebGPU carving generated 24 rows / 552 vertices with snow particles, at 60 fps and no emergency respawns. A subsequent 1.8-second neutral-input check reached zero rows and vertices, also at 60 fps with no new respawns. The ribbon is visible beside the board in the gameplay screenshot. Its bright appearance, edge blending, source graphics-state translation, authored scaling and original-game visual/frame parity remain unverified; do not describe this as pixel-identical. These findings supersede earlier statements that wake colour/UV and the mesh were missing.


## Full draw wrapper and VU fade ordering (September 12)

The captured graphics dispatch table resolves the wake callback at virtual slot +2D4 to 3883B8 (inside the generated 387EC0 function). It submits adjacent column pairs through 3885E0, selecting the same VU1 alpha/interleave program at 3A00/3A08 used for board strips. Material word 4 bits 2..6 select blend enum 5, the standard source-alpha operation documented in board-trail-render-passes.md. There is no evidence here for additive blending or an artistic brightness multiplier.

The render oracle now executes all of 2DDAB8, intercepting only its backend dispatch. Across 20,000 cases it checks the GP+1438 suppression gate, one-row rejection, column count, ring capacity/head/draw count, fade arguments, material words and restoration of the material-stack pointer. The prefix-only draw-window check remains separately marked by a scratch register sentinel. This confirms wrapper behavior, not the complete downstream GS packet execution.

Found and corrected a browser discrepancy: row fades were calculated as initial + ordinal * step. VU1 repeatedly adds the step to the unclamped accumulator. `originalWakeRowFades` now follows that sequence before integer alpha quantization. The existing VU oracle was extended to 20,000 cases (10,000 board-strip and 10,000 decreasing wake-strip cases) and passes, preserving RGB/UV/positions and interleaved ordering. The browser upload now consumes these row fades. The full core and production build were rebuilt. Brightness/texture filtering and final GS pixel parity remain open; this rounding fix is not a claim to solve the visibly bright ribbon.


## Texture binder checked (September 12)

Extended the original board-trail GS binder probe to wake texture ID 56. The renderer table stores a handle (675 hex), not a descriptor pointer; the texture manager resolves it through manager+8+handle*4 to descriptor 587300, named `wake`. Its unchanged TEX0 template is 598004000 (64x64, PSMCT32, MODULATE, RGBA).

The glide snapshot has all wake VRAM allocation words unset (FFFFFFFF), so binding that snapshot directly yields an invalid all-ones TEX0. This is a fixture residency limitation, not evidence for a different texture function. The probe explicitly supplies a scratch allocation copied from the resident board-track descriptor, keeping wake's own format/dimensions/function/template unchanged. Original 368970 then emits TEX0 598007AE9, TFX=0 and TCC=1; original blend setter emits ALPHA=44. Both track and wake binding assertions pass. This does not validate actual texture upload or mip/filter settings, and the allocation is not claimed to match a playing PCSX2 frame.

Browser texture modulation and alpha scaling are consistent with these checked settings; no unsupported brightness multiplier or additive blend was introduced. The probe now preserves stdout/stderr before raising on failure so an invalid fixture is diagnosable.

## Encoded-space blending (September 22)

The wake (GS ALPHA 0x44) now renders inside the encoded effect composite
(`snow-composite.js registerEncodedEffect`, renderOrder 650, after the rider and
before boost/snow), so its blend happens in 0..255 encoded space like the GS
instead of in linear light. Its MODULATE colour is clamped to 1 before blending:
wake RGB is the doubled environment colour and can exceed 128, which GS
saturates. With `originalFog=0` it keeps the old linear path.
