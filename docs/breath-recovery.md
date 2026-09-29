# Original breath emitter recovery

Browser emitter4 (`RiderBreath`, texture25) is now connected and enabled in snow-renderer meshes (see integration update below). Full source update is2E1120, called from2DF920 after SnowTrail and before impact emission. Do not enable a generic timed puff in its place.

Source inputs/state:
- rider+8A8 indexes the geometry+30 matrix bank. Captured value5; position transforms local(-3,0,4,1) by that matrix.
- FX+BC speed above1111.111083984375 cm/s resets emission accumulator+14 and returns inactive without stepping breath-cycle state.
- Animation class311AE8(channel2)==30 supplies effort target1, otherwise0. FX+1C snaps upward to target; above target it subtracts0.0016666668234393 per call.
- FX+20 phase clock adds1/60; at clock>=duration+24 it resets0 and toggles integer+18. Active duration=.4; other duration=(1-effort)*.6+.4.
-2EE6F0 resolves environment object+30 through environment vtable+184. Captured env0 value9.99995136; physical meaning appears temperature-like but must be confirmed from property definitions. Alpha=clamp((effort+.5)*(1-(value+5)*.04),0,2).
- Phase0 or alpha<=.01999999955 resets accumulator and submits inactive. Otherwise advance shared visual LCG and add(random*6+18)/60 to accumulator. Emit only if accumulator>1, subtracting1; otherwise inactive but retain accumulator.
- Active birth uses original profile velocity/normal/speed scales plus the rotated local(0,0,150) head-frame direction. Correction:2E13BC..2E13D4 loads4FF130=(0,0,0,1) over the copied matrix translation at stack+30 before the second transform. The original world translation is used for the mouth position only.3717C0/3710D0 stores the supplied vector directly; it does not subtract the birth position.

Captured inputs/constants and environment-reader linkage are in `local/browser-validation/breath-source-inputs.json`. OriginalFX parent+ B40 in snow-jam-glide is146FED0; captured state(accumulator,phase,effort,clock,duration)=(0,0,0,.33333325386,1). These are a saved state, not proven constructor defaults. Next recover reset lifecycle and environment/bone input mapping, then enable actual renderer output. The producer is now implemented and source-tested; browser integration and visual completion are still outstanding.


`originalBreathEmission` in engine/snow_emission.cpp ports2E1120. Its explicit context accepts the four original head-matrix columns, velocity, ground normal, speed, animation class and environment scalar.20,000 comparisons run the original generated2E1120, with source helper boundaries mocked only for environment/animation reads and emitter submission. All state fields, emitted positions/velocities/colours, active gates, dt and LCG word match exactly;3,620 active births in the current deterministic suite. The test includes arbitrary translated head frames, speed threshold equality, warm/cold scalar inputs, phase changes and class30/non30. Existing snow emission tests also pass. This validates the producer, not environment lookup scheduling or end-to-end visuals.

Constructor2DE610/618/620/628 explicitly zeros accumulator,phase,clock,duration; saved duration1 is not a constructor default. No explicit constructor store to effort+1C was found in this pass; do not infer allocator or restart behavior. Runtime still leaves emitter4 inactive until state/environment and reset-lifecycle integration are recovered.


## Environment input and snapshot initialization

`tools/export_breath_context.py` now reproduces `local/browser-validation/breath-context.json` from the verified Snow Jam starting savestate, recording state/RAM hashes, actor/FX ownership, head bone/matrix and all19 environment current/target pairs. It verifies rider interface6C0 slot38 adjustment-6C0, target140B80, and its exact getter instructions (load rider+86C). This yields environment0. The environment class pointer is object+4=484058; virtual180/184 returns object+30 via2C1608. Pair5 supplies the breath scalar. Constructor2BCAF8 initializes that current/target pair to10; the captured current9.99995136 is a blend result, while target+34 is10.

`engine/environment_properties.hpp` recovers the complete19-float-pair blend2BD698 (class slot210): square the caller's weight, use `(1-weightSquared)*current + weightSquared*incoming` in original scalar operation order, and store incoming as the target. It does not clamp the weight. Default before/after hook2BDA90 is a no-op. `tools/test_environment_properties_native.py` compares20,000 original calls across all19 current and target values and verifies two hook calls, including weights0/1 and values outside0..1. All are bit exact. The first test attempt ran original multiplication in host-nearest mode; corrected the harness with OriginalRounding before drawing conclusions about the port.

Remaining integration work: recover the caller's blend weight, target selection and update cadence; connect that live property object plus the head bone to the emitter in original producer order; enable emitter4 rendering and validate actual gameplay. Snapshot initialization permits reproducing the demo's chosen initial state without pretending it proves general constructor/reset semantics. No new breath visuals are claimed by this recovery.


## Live environment wrapper path

The source environment wrapper is2C0778 (wrapper vtable483E00 slot10), reached through2ED490's nine-property loop (counter starts8 and includes zero). Rider121928 supplies environment index from140B80, coordinates rider+460/+464, and f14=gp-7910=-99999 sentinel.2C0778 tracks accumulated distance in property object+0 and last coordinates in wrapper+8/+C; it does not use that field as a time clock. Region selection2C0A10 calls2BAF90, which reads a spatial-tree leaf via2C1CD8 and uses leaf+4 as an index into the property-payload table. The wrapper handles uninitialized-99999, explicit-weight, target-distance/negative-weight and reset-default branches. Keep these source branches rather than replacing them with fixed-rate interpolation.

The environment-properties original-code test now executes2C0778->2C0A10->2BAF90->2C1CD8 plus real2BD698/2BDEE0/2BE258 dispatch on the untouched Snow Jam savestate region data. The bounded starting-coordinate update returns normally with breath scalar9.99995 and target10. This verifies the source chain is executable for subsequent porting; it is not yet a translated browser environment wrapper or a runtime feature.

Starting-coordinate source execution selects map descriptor122BB04, payload122BF38 and weight0.1. Raw descriptor/tree header and selected payload are recorded in local/browser-validation/breath-region-source.json for the next spatial-lookup port. These addresses are savestate evidence, not product constants.


## Spatial lookup port

`engine/environment_regions.hpp` now ports2C1CD8 to native data: original scalar coordinate transform/truncation, unsigned0..32767 bounds, the source default leaf, and the encoded four-child traversal with X/Y bit order preserved. Invalid links/cycles fail explicitly.50,000 comparisons against the original function on the actual125-node Snow Jam tree pass, including boundaries and outside coordinates:6,279 outside,38,267 leaf0,2,339 leaf1,3,115 leaf2. Together with20,000 exact19-property blend cases and the real source wrapper check, these validate the lookup and arithmetic separately. The browser host transition wrapper remains to be connected.

`tools/export_breath_context.py` now also emits the125 original nodes and all three class12 payloads into the checked snapshot package. The payload transition values/breath targets are(-.1,10),(-.08,2),(-.15,-2). Therefore a fixed10 scalar would be wrong elsewhere on the course. Export validates counts, root/child indices, class tags and the existing source getter/fixture hashes. Do not treat source address constants used by this fixture exporter as runtime pointers.


## Gameplay integration

`engine/environment_transition.hpp` ports the class12 map transition from2C0778: sentinel, distance accumulation, exact-current reset, outside/default handling, caller override and target transitions.10,000 complete original map/property updates match all19 current/target pairs, distance and coordinates. Actual negative-transition payloads, overrides and outside regions are covered; synthetic positive-transition coverage remains to be broadened.

Browser setup reads original_breath from the checked snapshot package; prepare-ui invokes export_breath_context.py for regeneration. The adapter uses contact X/Y (source460/464), head bone5, current velocity/normal, channel2 animation class and the FX speed cache. Breath runs after SnowTrail and before impacts with the shared effects LCG and particle RNG. The demo's existing snow-reset path restores the captured breath/environment state. General constructor/reset lifecycle, full source environment host scheduling and stage changes remain fidelity questions.

Renderer emitter4 binds original PS2 texture25=brth (verified at4891B0+25*12). Asset export now supports this32x32 indexed SHPS type2 record/type33 CSM1 palette with index bits3/4 swapped, raw GS alpha and normalized preview data. The first browser smoke check caught a missing texture25 manifest entry; importing the texture fixed startup. Startup failures now expose a hidden body data-load-error diagnostic with the existing Load failed UI.

web/test-breath.mjs runs360 actual motion/animation/camera ticks from a low-speed braking fixture:peak2 visible particles, all finite and within8m of the rider, zero emergency resets. It also checks the renderer texture binding/file size. Full npm suite and production build pass, and all5,160 frames/580 fields in native/browser parity remain exact. Normal browser menus into Snow Jam and live race rendering were inspected; the smoke run was paused. Close-up breath/PCSX2 framebuffer comparison is still outstanding. Other effects and the broader goal remain incomplete.
