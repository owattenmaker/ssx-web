# Ground-to-air control entry and common filtering

The native explicit-jump lifecycle now loads `original_air_entry` data in any
rider mode, through `reference_air_entry.py` and `seedOriginalAirEntry`.
It retains the verified trick-stat profile, filtered prewind triplets at rider
`+0x2A4/+0x2B0`, and scaled skeleton pivot. Stat zero is never inferred from a
missing profile. The reader uses the original1495A8 progression/maximum getter
and scalar DIV nearest policy.

Original held control2 (`0x12E9B8`) thresholds decoded spin/flip at ±0.2, then
requests -1/0/+1 targets. It sets per-tick approach rates as
`5.000027179718018 * abs(target-current) * float32(1/60)` for ordinary style0;
other styles use `5.0000901222229`. The animation-class10 and animation-index21
branches request zero with the source constant rate0.08333379030227661.
These exact source constants and operations are implemented in `air_entry.cpp`.
The reverse-turn branch114CC0 has a separate physical/animation effect. The
prewind helper signals that boundary and native integration retains an explicit
unsupported diagnostic rather than treating the missing effect as completed.

The first Cross press changes normal control0 to control2 and returns before
requesting charge/prewind targets. On release, original12EA30 snaps the already
filtered prewind using angular step0x3F490FDC and deadzone0.2. It then performs
jump takeoff, requests zero turn/crouch/brake targets and enters control5 via
11FEC8→111538→111630→133128. Entry happens immediately within that release
handler; the newly entered control5 handler is first dispatched next tick.
The native entry performs the same separation. Air translation and presentation
still run on the takeoff frame; no extra angular handler update is inserted.

Original1211F8 approaches all control triplets in every rider motion mode.
The native rider now continues its recovered ground/presentation triplets in air
and advances prewind triplets in the common pass. The release requests are made
once. Consequently crouch decreases by the retained release rate in flight,
rather than having its rate recomputed as exponential decay each tick. This
also preserves the control state needed by later landing recovery.

Airborne frames clear retained ground-body collision volumes and queries and
mark the missing air pose. A cached ground volume cannot certify an airborne
body collision. The animated air pivot is still the seed pivot until the air
animation player supplies the current skeleton point.

Passive takeoff follows a different controller path: original131CC0 changes
normal control0 to control4 on the following frame when motion is airborne.
Control4 entry12F620 is distinct from explicit-jump control5. Its presentation,
commands, and later transition remain separate recovery work; a passive flight
must not be silently labeled as control5.

Verification:

- `tools/test_air_entry_native.py` compares20,000 complete original held-control
  target/rate cases, including606 reverse-animation boundaries, with zero float
  error. External animation/physical reverse effects are explicit oracle inputs.
- Native air-control CTest verifies release entry without same-tick redispatch,
  twelve subsequent logic ticks, retained crouch rate through flight, and stale
  body-volume invalidation.
- Existing live jump, isolated-air, air-spin, replay and race CTests pass after
  the lifecycle change. Broader prewind/reverse/landing captures remain required
  for full gameplay parity.

The joined stable suite now passes14 reference cases with exact position,
velocity, physical quaternion and race fields. Angular fields also match,
including full ground-to-air jump31/60/90. The private report is
`local/native-qa/reference-suite-stable-race-air/comparison.json`. This does
not expand the supported grab/reverse/passive-controller/landing scope.
