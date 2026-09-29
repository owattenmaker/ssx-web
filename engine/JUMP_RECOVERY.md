# Original jump takeoff

`jump_motion.cpp` implements the position and velocity arithmetic of PS2 USA
`0x114298`. It has no emulator, console memory access, or console instruction
execution in the product. The original generated routine is linked only into
`tools/test_jump_native.py`, a development conformance executable.

## Control and ordering

Control handler `0x12E9B8` checks bit `0x2000` of its decoded input record at
`0x12EA24`. While held, it calls `0x113F88(rider, 1, 0)` at `0x12EB70`.
On release, it calls `0x114298(rider, rider[0x220])` at `0x12EAFC` when the
motion mode is not already airborne. Then `0x11FE78(rider,1)` at `0x12EB08`
sets air mode, whose gain-focus handler `0x1399E0` initializes the trajectory
from the updated position and velocity. The release path then requests charge
and brake zero through `0x113F88(rider,0,0)`.

The charge's `(current, rate, target)` triplet is rider `+0x220/+0x224/+0x228`.
For the default non-braking input, `0x113F88` sets the target to zero or one,
with rate `abs(target-current)*0x3DCCCDC2` when the gap is at least
`0x3DCCCCCD`, otherwise rate `0x3C23D7CF`. `0x12130C..0x121348` moves current
by that rate toward target, snapping without overshoot. All are original float
bit patterns, not decimal retuning. The broader crouch/brake mutual exclusion
is in the ground-control recovery; this helper covers its zero-brake charge
path.

The launch uses the **previous charge before that tick's filter advancement**.
The seeded native controller performs launch before any ground integration,
then one original airborne tick. Physics now executes at 60Hz; the public
accumulator can still accept 120Hz half-steps without executing two input
updates. The first pressed tick changes control state 0 to 2 through `0x1162C8`,
called at `0x131794`; the nonzero return makes `0x13179C` exit before
requesting charge. Gain focus `0x12E980` leaves the charge triplet unchanged.
The captured charge counts at nominal movie frames 1/2/3 are respectively
0/0/0.19000327587127686, so they cannot be used as clean logic-tick goldens.
PCSX2's movie counter advances and switches back to record mode at the terminal
poll before controller override; paused EE snapshots may also straddle game
update phases. The reference harness needs an original logic-boundary anchor.
Do not compensate with a fabricated extra input delay in the native game.
The current controller covers the default
zero-charge onset; transitions with residual brake/filter state require the
broader control-state implementation.

## Impulse and direction

Let `v` be source Z-up velocity in cm/s, `speed=length(v)`, and `q` be charge.
For active takeoff (`q>=0`):

```
base = speed < 972.1808471679688
     ? speed*0.357146680355072 + 361.203125
     : 708.4142456054688
impulse = clamp((q*q)*base, 361.203125, 708.4142456054688)
```

For passive takeoff (`q<0`), impulse is `0.16339834034442902`; the original
branch bypasses the active-charge clamp. The function also writes motion-owner
`+0x14=-1`, an event/timing side effect the arithmetic helper does not implement.

In mode 4 the impulse follows rider `+0x1C0`. Otherwise, the ordinary launch
direction is `normalize(normal + 0.2*forward)`, using rider `+0x370/+0x3A0`.
For descending forward tangent, descending velocity, or the slope factor below
zero, add that direction times impulse to velocity.

The rising ramp branch uses
`slope=(normal.z-cos(50°))/(cos(70°)-cos(50°))`, with exact original polynomial
results `cos50=0x3F248DBA`, `cos70=0x3EAF1D41`, `sin20=0x3EAF1D45`.
It removes the horizontal normal component from forward and velocity, normalizes
them, and forms `blend=clamp(slope*projectedForward.z/sin20,0,0.95)`.
It normalizes the blend between ordinary launch direction and projected forward,
blends original velocity with `projectedVelocity*(blend*speed)`, then adds half
the impulse in that blended direction. This is the actual `0x1144C4..0x114794`
arithmetic, not a fitted launch curve.

The remainder preserves original handling for horizontal velocity lost by
launch, nearly vertical takeoff surfaces (`rider+0x380`), speed cap `+0x2E4`,
and reduced takeoff soon after landing. The last factor applies to active
charge except rider state `+0x434` in 11..13. The timer is seconds since motion
owner `+0x10` gained ground focus: factor `0.6939882636070251` through
`0.5388872623443604s`, then linear blending to one using multiplier
`1.2326725721359253`.

The original global tick accessor `0x1298C8` returns
`*(*(*(*(gp-0x848)+0x84)+0x0C)+8)`, with `gp=0x4A30F0`.
The jump-30 fixture has tick 368 and ground-focus tick 201.

## Evidence and limits

`python3 tools/test_jump_native.py` executes the original whole takeoff routine
for 20,000 deterministic randomized cases. Native velocity and position match
with **zero numerical error** across active/passive charge, ramp and ordinary
branches, modes 0/4, near-wall surfaces, flags, speed limits, and focus timers.
It also compares 120 charge ticks against the actual original setter and
triplet-update routine, exactly. External sound/animation/event calls in this
oracle are stubbed and are not claimed to be recovered by this arithmetic test.

`jump_motion_tests.cpp` uses two direct PCSX2 snapshots as a stronger independent
check on the actual downhill launch. It verifies the complete controller release
from jump-30 to jump-31: all three position and three velocity float bit patterns,
and the post-release charge, match exactly. Before the airborne tick the launch
velocity is `(-2201.252685546875,-364.5838623046875,-198.91314697265625)` cm/s.
After that tick it is `(-2193.9150390625,-363.368560791015625,-230.579803466796875)`.

This does not establish full playable-game fidelity: ground contact, animation
and trick/event transitions, landing response, and roster/stat initialization
must also provide their original state. `seedOriginalJump` retains raw reference
normals and tangents for a checkpoint release. After unverified native ground
movement those vectors come from provisional collision geometry. An unseeded
inspector rider retains the old provisional 55m/s cap until a recovered roster
initializes its stats; the reference replay must supply the actual speed limit.

## Scalar floating-point policy update

The current native and development-oracle paths distinguish EE scalar DIV.S/
SQRT.S (nearest) from surrounding chop arithmetic and VU DIV/RSQRT (unchanged).
`engine/original_float.hpp` and `tools/original_fp_oracle.py` centralize this
correction, with opcode audits on private generated-code copies. Earlier blanket
round-toward-zero descriptions must be read with these scalar exceptions.
The original comparison suites were rerun successfully under this policy.

The scalar ADD.S/SUB.S sites now preserve the verified EE single alignment guard bit; VU arithmetic is unchanged. The opcode-corrected original-code conformance suite passes with this policy.
