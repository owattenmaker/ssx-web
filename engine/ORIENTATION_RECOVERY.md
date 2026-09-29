# Original physical orientation recovery

`orientation_motion.cpp` implements the source Z-up arithmetic of world-axis
quaternion rotation `0x11DFE0`, arcsine `0x31C128`, and the ground surface-alignment
stage `0x13ED68..0x13EE9C`, full cruise heading stage
`0x13E22C..0x13EB98`, and physical quaternion normalization/matrix rebuilding
`0x11E098`. It reuses the recovered `originalSinCos` from
`ground_motion.cpp`; product code executes native float math only.

The rotation constructs `(axis*sin(angle/2), cos(angle/2))` and pre-multiplies
this delta by the physical quaternion. It retains the original operation order
and round-toward-zero float arithmetic. The rotation arithmetic itself does not normalize the quaternion.
The original caller immediately rebuilds its orientation matrix through
`0x11E098`; `originalRebuildOrientation` implements that normalization and
produces the original right/forward/up columns at `+0x1A0/+0x1B0/+0x1C0`.
It preserves the VU operation order instead of substituting a library matrix.

Ground alignment is conditional on the contact-result value at caller
stack+0x148 being below 5cm. It normalizes
`target=(normal.x, normal.y, 1.5*normal.z)`, forms
`axis=cross(target, physicalBoardUp)` and obtains `sine=length(axis)`.
When `sine>0.001`, it rotates around normalized axis by
`-originalAsin(min(sine,1))*float(1/60)*surfaceRecord[0x38]`.
The contact/ground-validity gates preceding this isolated stage remain the
caller's responsibility.

`python3 tools/test_orientation_native.py` compares 20,000 deterministic cases
for each recovered sin/cos, arcsine, rotation, normalization/matrix, heading,
and whole alignment stage against
the original generated instructions. All float outputs have zero error. The
alignment oracle resumes at the original `0x13ED68` and returns at its two exit
labels; heading resumes at `0x13E22C` and stops before contact handling. Their
instructions are otherwise unchanged. Rotation and normalization/matrix
arithmetic are tested independently against their whole original functions.

## Corrected development oracle operand

The PS2Recomp checkout contains a SQRT.S operand bug in
`ps2xRecomp/src/lib/fpu_translator.cpp`: it emits Fs as the source. The EE
instruction uses Ft, as the PCSX2 v2.8.2 interpreter's `pcsx2/FPU.cpp::SQRT_S`
confirms. In original sin/cos at `0x31BEEC`, opcode `0x46050044` means
`sqrt f1,f5`, but the generated file used `f0`. That incorrectly takes the root
of the sine polynomial rather than `1-sin²`.

The conformance script checks the opcode/comment and exact generated statement,
then writes a private development-only copy correcting this one operand. The
generated tree and original game executable remain unchanged. The native
implementation was written from the original operation and was not adjusted to
reproduce the erroneous oracle. Earlier jump and air arithmetic uses VU roots
and is unaffected; original arcsine's EE roots use Ft=Fs=0 and are unaffected.

## Cruise heading

`originalGroundHeading` receives **old relative velocity before integration**,
physical/surface axes, current filtered turn and charge, control state, the
original frame time, source stance flag, and the transient angle at rider+0x2DC.
Its profile preserves surface+0x28/+0x2C/+0x30/+0x34 and all three original
four-point curves from `*(gp-0x1FC0)`, `*(gp-0x1FB8)`, `*(gp-0x1F98)`.

Velocity alignment applies across control states. State 2 has the additional
normal.z>0.65 gate; all cases require nonzero speed. This distinction was checked
by the original-stage comparison after a preliminary branch interpretation was
found incorrect. The desired rotation uses the signed arcsine of the normalized
velocity/forward cross product about the surface normal, with turn and charge
biases from the profile. Response depends on speed, surface slope, steering
sign, and surface response coefficients, and is bounded by a per-tick angle.

The +0x2DC path computes orientation relative to velocity in the ground plane,
respects source stance, handles angle wrapping and rotations beyond pi, and
combines that correction with automatic alignment. It only reduces the +0x2DC
value by the rate step when its magnitude exceeds pi; a smaller value is cleared
when heading error is sufficiently small. Both the resulting angle and +0x2DC
writeback are included in exact conformance tests.

The final direct steering term is separate. It uses the body's forward-facing
fraction of speed and the curve from `*(gp-0x1F98)`. The captured Snow Jam
profile has zero ordinates for all four points. A generic yaw-speed constant
would therefore alter original behavior even though steering still turns the
rider through the other recovered terms.

These helpers do not themselves establish complete rendered-rider parity. The
controller must preserve original update order, contact results, animation
transitions, and physical versus presentation pose. Actual full-frame emulator
comparisons are still needed after integration; instruction-level conformance
and the direct jump golden are not substitutes for that broader validation.

## Scalar floating-point policy update

The current native and development-oracle paths distinguish EE scalar DIV.S/
SQRT.S (nearest) from surrounding chop arithmetic and VU DIV/RSQRT (unchanged).
`engine/original_float.hpp` and `tools/original_fp_oracle.py` centralize this
correction, with opcode audits on private generated-code copies. Earlier blanket
round-toward-zero descriptions must be read with these scalar exceptions.
The original comparison suites were rerun successfully under this policy.

The scalar ADD.S/SUB.S sites now preserve the verified EE single alignment guard bit; VU arithmetic is unchanged. The opcode-corrected original-code conformance suite passes with this policy.
