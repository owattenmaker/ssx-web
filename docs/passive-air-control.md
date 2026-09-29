# Passive-air controller4

`engine/passive_air_control.hpp/.cpp` implements12F620 entry,12F730 update,
and12FB68 exit with explicit callbacks for external world transitions.
`tools/reference_passive_air.py` extracts the original state at motion-owner+210:
entry angle0, entry magnitude4, upper latch8, identity latchC and signed last
identity10; the actor backlink is14.

131620 calls131CC0 before ordinary input handling. If motion is already1,
131CC0 requests control4 and returns1;131620 then returns immediately. Therefore
run12F620 entry on that tick but defer12F730 until the next controller tick.
The common1211F8 smoothing pass still runs in between.

Entry captures `atan2(currentCrouch,currentTurn)` and
`max(abs(currentTurn),currentCrouch)`. Both latches start1, last identity−1.
It sets the prewind2A4/2B0 rates to float1/30 and targets to0; current values and
ordinary turn/crouch/brake controls survive. Entry requests no animation.

The human writer127998 case128280 uses CruiseTurn(action0), CruiseCrouch(action1),
AttackLeft5, AttackRight6, Handplant25 and1276F0's signed identity. Packet fields
are word0 bits24..29 turn, word1 bits0..5 crouch, word0 bits16..23 identity,
bit12 recovery, bit13 handplant and bits14/15 attacks. These are not the state5
spin/flip/boost mappings. Identity is compared as a signed byte;−1 is no grab.

Update calls recovery116120, upper action1163B0, optional handplant107578 and
automatic rail attachment106848 in source order. The upper return is checked
only while the upper latch was active; after it releases,1163B0(false,false)'s
return is deliberately ignored. Failed handplant changes the crouch request
to1 before later calculations. Boost114130 receives(false,false).

The identity latch tracks a continuously held identity. Once that identity
changes/releases, or directional input drops sufficiently/turns more than a
quarter-turn from entry, the magnitude becomes−1 and both targets become0.
Target rates for turn, animationTurn, crouch and brake are float0x3D4CCCCE.
AnimationTurn targets the old filtered physical turn; brake targets0.

While settling, class9 clips remain untouched. Otherwise the controller chooses
9/10 from the same filtered turn/crouch blend boundary. When filtered turn,
brake and crouch are all exactly0 and the identity latch is released, it requests
287 unless287/class9 is already current, then requests control5. This transition
is ordered after the target writes; the next control's actual entry remains the
host controller lifecycle's responsibility.

On active exit, upper class3 or13 triggers a0.1-second fade on channel0 and sets
the channel1 sequence rate to1. The differing channels are original behavior;
the helper does not substitute a channel1 fade.

`test_passive_air_native.py` passes20,000 complete original entry/update/exit
cases, including114 transitions to5. It checks state, all control triplets,
signed command fields and ordered callback arguments. Recovery, upper actions,
handplants, rail queries, boost and animation requests are explicit subsystem
boundaries; this proof does not claim those external gameplay handlers are
implemented by the passive controller itself.
