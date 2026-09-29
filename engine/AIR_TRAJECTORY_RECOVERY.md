# Native airborne trajectory prediction and physical alignment

`air_trajectory.cpp` implements original `0x113200`, `0x113618`, and
`0x113648`. It preserves source Z-up float centimeters and original scalar/VU
rounding. Native code has no guest CPU or generated-code dependency.

The predictor retains separate predicted and current integrated positions and
velocities. Each prediction extension advances 60Hz steps until its count exceeds
`270000/speedLimit`, or its velocity direction diverges enough that the initial
unit direction dotted with the new velocity is less than `0.99*newSpeed`.
It tracks the last non-descending position/time as the apex. It expands the
resulting chord by 2% at each end and queries backwards from the predicted end
towards the beginning. Original query construction is `0x32E100`, kind 1, preferred fraction 1;
status 0 calls `0x336850`, while status 2 calls `0x3378C0` (instances only). The kind 1 terrain path uses
the coarse 9×9 grid, with no Newton refinement. Ranking selects the reverse
fraction closest to 1, prioritizing instances over terrain on a tie.

An accepted nonnegative hit fraction also requires the expanded forward chord
to face against the returned normal. The reverse fraction interpolates heading
between the old and new velocities and adjusts prediction time. The original
fraction/time arithmetic is preserved; it does not compensate for the 2%
segment extension. Status2 hits become status 3 with surface 13 and world up;
ordinary hits retain surface/normal and patch ID/flags/coordinates.

`0x113648` extends the prediction lazily, and reseeds a status 1 prediction when
it is more than 0.2 seconds behind current elapsed time. A prediction beyond 60s
switches to the secondary query status 2, then falls back to status 3 if that also
exceeds the horizon. Actual motion advances a separate fixed-step cache and
uses the original interpolation branch only when the remainder exceeds 0.01s.
The original reseed operation deliberately preserves elapsed and integrated
cache time; it copies current motion into both state pairs and sets prediction
time to elapsed.

`air_alignment.cpp` implements complete physical approach `0x121AA0`, including
up alignment `0x31BB30`, optional heading correction, shortest-sign quaternion
delta, gain/capped angular rate and final normalization. Its standalone sine
`0x31BF60` uses a different polynomial from the shared sine/cosine function.
`originalAirAlignmentStage` additionally implements the entire orientation tail
`0x139A64..0x139C68`, including the prediction status/surface/flag gates,
remaining-time gain, heading suppression during manual adjustment, and the
caller's unconditional additional normalization.

The state returned by `reference_air_control.py` includes `trajectory` and
`alignment_context`. A checkpoint's prediction must continue updating against
native world queries. Freezing its normal/status is incorrect: the isolated
spin baseline starts with status 0 and ends 30 ticks later with status 1 and a
predicted ground contact 5.263 seconds ahead of the initial epoch.

`PrototypeRider` runs these helpers when a trajectory seed, native reverse-query
callback, and native surface-property lookup are present. Otherwise it keeps the
physical-orientation pending diagnostic set. Angular presentation reads remain
immutable and are updated once per logic tick. Query callback binding and the
world-query implementations are separate from these arithmetic/state helpers.

## Verification

- `tools/test_air_alignment_native.py`: 30,000 complete original `0x121AA0`
  cases including quaternion and matrix axes, plus 20,000 complete original
  caller-tail cases across status/gate/rate/heading variations. All float results
  match exactly.
- `tools/test_air_trajectory_native.py`: 10,000 complete original `0x113648`
  calls. Every retained state/output float and integer field, and the sequence
  of expanded query requests, match exactly. Query returns are controlled oracle
  fixtures; these checks do not certify the separate world query implementation.
- Scalar ADD/SUB use the verified EE single alignment guard bit; scalar DIV/SQRT
  use the verified nearest policy. Development oracle copies are opcode corrected.
  VU arithmetic remains chop. The `0x113A74` speed-cap divide is scalar nearest.
- Existing live airborne, jump, air-spin and replay CTests pass after the speed-cap
  correction. A full live trajectory/physical-orientation comparison still needs
  the native world query adapter to reproduce its changing predictions.

`ray_instance_collision.hpp` recovers the query's node primitives: original
32E288 bounds, 32E398 transform, 32E690 box entry/exit contacts, and
32E4D0/32E5E8 authored-normal triangle intersection. Original 32E688 returns
zero sphere-tree ray contacts. `tools/test_ray_instance_native.py` checks
20,000 cases for each primitive against original scalar instructions and VU0
barycentric microcode, including 6,312 box and 10,349 triangle contacts. Every
returned float matches exactly. The separate world dispatcher chooses nodes,
transforms results and ranks instance/terrain contacts.

The joined14-case race/air suite matches physical position, velocity, orientation
and active prediction data. It retains one explicit checkpoint distinction:
jump31 `hit_position[0]` is8.437386e-39 in the original and0 in the native seed.
Original113198/1135B8 leaves this inactive field untouched; the captured original
bytes correspond to prior storage. The native constructor likewise retains its
existing native value, initially zero. This distinction remains visible in the
comparison report; it is not masked or presented as an active-contact match.
