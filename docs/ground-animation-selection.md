# Original normal riding animation selection

`originalSelectGroundAnimation` now implements the complete131878..131C04
selector instead of rejecting low speed, stopped braking or ice. It runs before
the common1211F8 control approach, so decisions use the old filtered controls.
The `RiderInput` turn/crouch arguments are the effective raw cruise requests
(after any preceding controller handplant override).

The caller owns114CC0's physical reversal. It may pass the optional
`reverseTurnTriggered` result, or perform the already-verified reverse action
and supply its updated semantic21. The selector applies the corresponding
animation/target gates; it does not duplicate physical rotation.

Selection preserves this source priority:

1. Semantic22 remains selected and zeros animation-turn/brake/crouch targets.
2. Semantic21 or a successful eligible reversal zeros animation-turn/brake.
3. Class10 retains its landing semantic and zeros animation-turn.
4. Class5 retains its semantic, follows filtered physical turn, and zeros brake.
5. Otherwise choose the ordinary unbraked or braking cycle.

All those target rates are the original float1/30. Ordinary animation-turn
follows the old filtered turn while unbraked and targets0 while braking.

Below float833.333374cm/s, positive raw crouch or abs(raw turn)>float.2 chooses
22. A neutral request at low speed continues through ordinary selection; it
is not an unsupported state. Surface4 selects ice14/15, with boost8 only for
tucked posture and abs(filtered turn)<float.3. Other surfaces may select a
steep-slope bob if abs(animationTurn)>float.6 and abs(lateralZ)>float.7. That
branch consumes exactly one shared RNG word: an even word selects16/17 by
stance-adjusted lean sign, while an odd word continues normal selection.

The ordinary tuck boundary is `.5*(1-abs(filtered turn))`. Below it selects5;
above it selects8 while boosting, otherwise7 below float1666.666748cm/s or on
surfaces2/3, and6 at higher speed on other surfaces.

Braking uses hysteresis: existing12/13 stays eligible for a stopped pose up to
float972.222229cm/s; other semantics use float694.444458cm/s. Above the threshold,
or with abs(brake)<float.8, choose11. Otherwise choose12/13 by brake sign and
reverse stance. Definitions16/17 have class5; other selected ordinary cycles
have class7.

`tools/test_ground_selector_native.py` passes40,000 original block cases,
including4,475 shared RNG draws and all low-speed, stopped, ice, bob and special
retention branches. It checks semantic/class, every affected control triplet
and RNG consumption. Animation request and physical reversal are explicit
subsystem boundaries, with the original selector's arguments/branch behavior
retained. The prewind and air-release selector helpers remain unchanged.
