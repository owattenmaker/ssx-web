# Original visual boost controller

engine/boost_effect.hpp ports2E66B8..2E68A4, before history/geometry generation. Activation requires motion!=1 and positive rider2FC boost or2E8 speed-pickup window. Leaving airborne state clears history count and phase14/18. Texture selection is57/58/59 by tier thresholds5/10,60 for speed pickup/override,61 for tier11. Source table4891B0 maps these to yrbn/orbn/rrbn/brbn/prbn. Tier11 width is50cm; other active widths are6+9*clampedBoost. Trail length is(400+400*clampedBoost)*(speed/3333.3335). Alpha is1 while active; otherwise length/alpha/width each multiply.95 per original update.

Scroll steps are.002 low,.005 medium,.01 high/pickup/tier11. The initial port missed the branch into2E677C for an active pickup, incorrectly retaining.002. Direct source comparison caught this and the delay-slot path is now preserved.

tools/test_boost_effect_native.py executes the original prefix with a deliberate stop before2E68A8.20,000 randomized cases match stored fields, activation, speed and scroll step, including tier and debug override branches and landing resets. This validates parameters only. Trail history, camera clipping, textured geometry and browser integration are not implemented by this helper. No visible boost effect is claimed yet.

## Side history and owned ribbon textures recovered

engine/boost_history.hpp now ports2E68A8..2E6BEC. The enabled branch updates a20-slot ring of paired four-component positions; emission copies the previous pair into the next slot, grows count up to the configured capacity, and spreads all20 slots along the physical right column by1.4. Non-emission shrinks the count using the source branch (including its signed-count behavior). The newest endpoints are ordered by projection on physical right and subtract0.7 times the third supplied bone translation. UV scroll subtracts speed*(1/60)*scrollStep and performs the original single +/-1 correction, not fmod. Disabled or empty-history paths skip that scroll stage. Rendering-space interpretation of these bone translations remains caller-owned and is not yet resolved by this numerical port.

Constructor2E6698..2E66A8 sets capacity18 for deviceIndex<0 and20 otherwise. originalBoostHistoryCapacity preserves this. tools/test_boost_history_native.py runs the actual generated instruction block, stopping before2E6C08, against20,000 cases including both capacities, all ring samples, ties, enabled/disabled, emission/decay and scroll wrap; all are bit-identical. Existing20,000 boost parameter cases remain separate. This history is distinct from the main30-entry ribbon history/geometry in2E6C08, which remains to be recovered and connected.

Exported PS2 EFFECTS.SSH textures57yrbn,58orbn,59rrbn,60brbn,61prbn, all32x32. export_snow_assets.py now verifies the packed four-byte tags at4891B0+12*id directly against the owned ELF program segments. Short indexed CLUT chunks (e.g.147 logical colors fororbn) are decoded using their stored chunk extent and checked swizzled indices, rather than assuming1024 palette bytes and accidentally reading into a following member. Original GS alpha and converted PNG/RGBA outputs are retained. The prior six textures and ten emitter profiles were checked byte-for-byte unchanged.

Native and browser SNOW_FX manifests now contain11 textures, and production assets were rebuilt. A visual inspection ofyrbn shows the expected yellow luminous stripe; this is texture inspection only. Neither side-history geometry nor the main boost ribbon is drawn by the browser yet. Do not report visible boost effects as complete.

## Complete main ribbon update recovered

engine/boost_ribbon.hpp now ports the full2E6C08 update, separate from drawing2E7A10. It exposes30 packed28-byte rows (distance plus two xyz endpoints), ring cursor, previous target position and continuous side vector. OriginalBoostEffectState fields formerly called phase14/phase18 are now correctly typed primaryCount (integer+14) and primaryDistance (float+18); landing-reset behavior and the parameter oracle remain intact.

Seed stage2E6C08..2E71B8 advances the cursor backward modulo30, subtracts the overwritten oldest interval when full, measures movement from the prior target position, and constructs four board-relative anchors at +/-75 along boardX and +/-17.5 along boardZ. Switch stance negates boardX. Nonzero rider330 shifts the source-selected end by150 along boardX; semantics23..28/37/38 select the positive branch, the others negative. The virtual rider position and board bone8A4 matrix inputs remain explicit caller inputs.

Endpoint stage2E71BC..2E7948 uses velocity cross groundNormal, sign-continuous against the previous side vector. It projects four anchor offsets from rider460 onto that side, normalizes by the maximum absolute projection, and follows the original asymmetric0.5 corner preference and strict/non-strict tie branches. Selected anchors are projected10cm above the contact plane. Tail2E794C..2E79F4 discards oldest distance intervals while over the requested length; trailLength>1 retains at least3 rows, otherwise it may reach0. It preserves the original sample array when trimming.

Validation:20,000 seed-stage source cases;20,000 endpoint cases including zero side vectors and coincident anchors;20,000 trim cases (189,404 rows removed); and, decisively,20,000 complete calls to the original2E6C08 with no internal stops match all rows, counts, distances, prior position and side orientation. The complete fixture controls only virtual target-position and semantic getters. Native seed oracle's shared checkpoint is2E71BC, not2E71B8, because branch delay-slot paths skip the latter. Existing20,000 parameter and20,000 side-history tests pass after the field-type correction.

No browser runtime hookup or GPU draw path is enabled yet. Next recover2E7A10: it consumes both side count3A0 and primary count14, gates on trail length/alpha, and builds textured draw data. Determine source pose-coordinate and camera inputs before binding the new history/geometry to gameplay. These helpers do not establish rendered boost fidelity by themselves.

## Boost strips now connected to gameplay

engine/boost_draw.hpp reproduces2E7C3C..2E81B8 vertex packets for two side strips and the main ribbon.20,000 source cases compare1,437,404 vertices including position, UV and integer RGBA. Side strips restore0.7*head position, widen along physical right, and fade quadratically. Main strip distance is read in linear row order while endpoint positions use the ring; that source asymmetry is retained. Alpha quantizes to GS0..128 before conversion. Original texture binding verifies MODULATE/RGBA for57..61; blend enum7 emits ALPHA0x48 (source-alpha additive), verified with the original binder/setter.

web/boost_gameplay.inc samples original pose indices0/23/10/15 and the physical right/contact frame, runs parameter/side/main updates once per posed tick, and publishes three triangle buffers. Virtual target-position slot28 is1408F0 returning rider110. Reset clears FX history; warp/reset phase timing still needs broader original host comparison. Entirely zero-alpha triangles are omitted; non-finite visible geometry is diagnosed. The supported browser rider330 flag remains0, while the pure geometry helper supports its source branches.

web/boost-renderer.js uploads the original textures and additive strips through Three/WebGPU. Source channel factors are preserved, but the current renderer uses its existing linear/sRGB pipeline: encoded-space GS blending/quantization, complete render-state equivalence and continuous PS2 pixel comparison remain open. This is visible effect integration, not a claim of pixel-perfect rendering.

Gameplay tests cover no-boost suppression, normal boost (78emitting frames), speed-pickup palette60, fade-out, reset and finite nearby vertices. Texture60brbn actually contains a green luminous stripe; earlier descriptive references to a blue palette were incorrect. A live speed-pickup browser capture showed the green strips; assets were not recolored. Full npm tests and9,030-frame634-field native/browser comparison, now including FX buffers, pass. Production rebuilt. This supersedes older notes that no boost effect is visible.

## Encoded-space additive blending (September 22)

The boost strips (ALPHA 0x48, Cs*As+Cd) now blend in encoded 0..255 space via the
shared encoded effect composite, as the GS does. Linear-light additive blending
was measurably dimmer: two 50% encoded contributions sum to 100% on the GS but to
about 69% encoded after linear addition. MODULATE colour is clamped to 1 before
blending (GS saturation), preventing EOTF over-range values. Draw order stays
wake 650 -> boost 660..662 -> startfire 690 -> snow 700..709.

## Power-up aura and air streamers (2026-09-25)

The rider FX blocks after the boost (RFX+0x610) in the FX pass: RFX+0x9C0 power-up aura (ctor 2EADC0, update 2EADD0,
draw 2EB198, `psmr`, priority 7) and RFX+0xAD0 air streamers (ctor 2EF6A0, update 2EF6D0, draw 2EF950, `strm` or `prbn`,
priority 8). Ported in `web/boost_gameplay.inc` (`rider_fx_info` / `rider_fx_vertices`, called from update_boost_fx) and
drawn by `web/boost-renderer.js`; details and PS2 frames in [pickup-recovery.md](pickup-recovery.md#pickup-effects-and-hud-vs-ps2-frames-2026-09-25).
Material words: word0 bits 2..3 = CLAMP_1 mode via 3625C0 (0 repeat, 1 S clamp, 2 T clamp, 3 both), word1 bits 2..6 blend
enum (7 = 0x48), bits 23..24 ZTST, word2 bits 5..9 priority.
