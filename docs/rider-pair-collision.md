# Original rider pair collision

The native helpers in `engine/rider_pair_collision.hpp` recover the shared rider collision phase from PS2 USA instructions. They are standalone; the shared-world scheduler must run them after all participating actors have completed the preceding motion and pose phases. A sequence of complete, independent actor updates is not equivalent.

## Ordinary contact order

`0x107888` caches the initiating actor's effective collision weight before looping over six opponent slots. Each slot has a 36-byte record at the actor base. The reciprocal record belongs to the other actor at the initiating actor's slot.

1. Skip a disabled owner record or another actor with `+878 != 0`.
2. If the reciprocal `+14` last-checked tick is older than the current signed tick, write the current tick into both records before the query.
3. `0x329F98` rejects disjoint broad spheres and selects the first overlapping child-sphere pair in authored order. It ignores active mask bits. Coincident centers and tangent contact retain original float behavior.
4. Apply `-0.55 * penetration` to A and `+0.55 * penetration` to B, projecting each displacement against that actor's ground normal when its motion mode is 0 or grounded ragdoll mode 2/submode 0. Translation uses `0x106538`: actor position, secondary presentation offset, AA0 spheres, and AABB move; posed geometry bone arrays do not.
5. If A's `+10` last-contact tick is strictly less than `currentTick - 3`, normalize the penetration vector and compute both signed impulses from the pre-response velocities and effective weights. Dispatch A first, then B. Only after both responses finish, write the current tick into both `+10` records. The cooldown suppresses impulses, not separation.
6. The attack branch is evaluated separately, including when the ordinary query missed or the reciprocal actor already checked the pair.

`originalPairShouldCheck`, `originalBodyPairContact`, `originalPairSeparation`, and `originalPairImpulseDue` expose this ordering without hiding lifecycle callbacks. The caller must commit timestamps at the positions above.

## Weight, impulses, and reactions

`0x11FF98` resolves integer weight from `0x530970 + characterId * 0x88 + 0x40`, multiplies it by `1 + resolvedStat * 1.5003352165222168`, then by `1 + boost2FC * 10`. The stat getter is `0x148F50`; it uses progress byte 5 and maximum byte 13, with the usual override and fixed NPC-stat case. Weight units are retained as authored attributes.

`0x107E70` ignores control 10. Otherwise it caps the signed impulse at ±555.555542 cm/s, applies its direction to velocity, and projects against the ground tangent when appropriate. Airborne mode 1 and airborne ragdoll mode 2/submode 1 reseed the trajectory predictor. In those airborne cases, an upward impulse suppresses the subsequent reaction magnitude while preserving the velocity change.

After velocity changes, motion 2 and control 9 suppress reaction dispatch. The remaining reaction gate is 39.995327 cm/s of the uncapped magnitude. Above 599.974 cm/s it selects the original crash semantic from presentation-axis dominance and shared RNG; otherwise it calls the soft-impact routine. The knockdown cheat replaces the magnitude with exactly the hard threshold, so its strict comparison takes the soft branch.

`originalPairReactionRequest` emits the original point, normal, normalized post-impulse velocity, signed capped impact scalar, attack flag, semantic, and RNG count. The point uses the cached planar distance, not the body overlap point. A soft request calls `0x108388`; a crash request calls `0x10EB30`. These routines' animation and control/motion lifecycle effects must actually be implemented before treating the request as handled. There is no direct heading rotation in `0x107E70` itself.

## Shared proximity and attack data

The table initializer `0x10F3B8` enables an owner's records only when `owner+880 == 7`, the other slot exists, and it differs from the owner slot. Other actors still receive reciprocal timestamps. Record `+8` starts at 1e10. `0x10F560` refreshes reciprocal planar XY distances and bearings once every six manager ticks; it also maintains separate proximity/ranking fields not used by ordinary pair overlap. `originalPairProximity` preserves scalar nearest SQRT/DIV and the original bearing wrapping.

The attack branch requires owner animation channel 1 class 13 with marker 0 active and marker 1 inactive, and excludes an opponent in class 3 during its marker-0-active/marker-1-inactive window. It checks its own reciprocal `+18` cooldown, distance in [0.001, 150] cm, and the initiating actor's facing vector at `+340`. It combines the source attack stat and `+350` strength scalar, then blends half the separation direction with half the owner's velocity direction without renormalizing. It dispatches the response to the other actor with attack flag 1, then updates that actor's reciprocal attack timestamp. `originalPairAttack` exposes the original calculation; caller-provided animation markers must come from the native animation sequence.

## Evidence and remaining integration

`tools/test_rider_pair_reference.py` compiles ignored local copies of the original instructions with opcode-correct scalar floating point policy. Independent suites compare first-sphere overlap, physical impulse and gates, resolved weight arithmetic, complete reaction requests, ordinary dispatcher order/timestamps, complete proximity refresh, and the complete attack branch. Each suite runs 20,000 deterministic original/native cases with exact float equality. The event and crash handlers are intercepted at their call boundary, so these tests do not claim that native crash physics or animation recovery is implemented.

`tools/reference_pair_collision.py` exports manager and participant state for initial seeds and one-frame fixtures. It records source hashes, collision weight inputs, pose-derived spheres, presentation frames, six reciprocal records, and shared RNG. Captured sphere arrays are fixture/seed evidence; the runtime must generate all participants' poses natively. The long-charge-240 checkpoint is tick 578 and retains reciprocal actor 0 ↔ actor 3 last-contact timestamps of 522. No tick 521→522 fixture has yet been captured, so the resulting live impulse has not yet been independently checked from a pre-contact snapshot.

## Shared native dispatcher

`engine/rider_pair_system.hpp` provides `OriginalRiderPairSystem`. It owns six-slot records and exact ordinary/attack ordering. `refreshProximity(tick)` runs the planar record update every sixth shared tick; `resolveActor(slot, tick, knockdownCheat)` belongs inside that actor's second motion phase, after the shared pose phase and before its remaining speed/board-normal/passive-leave work. The caller continues to own proximity ranking/AI observers outside the pair-physics fields.

`OriginalPairCallbacks` connects live actor views, translation, velocity/predictor updates, typed reactions, and the world's shared RNG. The dispatcher caches the initiating actor's collision weight and attack animation window at entry, then re-reads live participant state after callbacks. It computes both ordinary impulses before dispatching either, writes contact timestamps after both responses, and evaluates the attack branch afterward. A callback can affect later participants' weights and reaction eligibility. Missing native body poses or required callbacks raise an explicit error.

The translation callback runs once for each displacement. It must update cached AA0 broad/child centers; the dispatcher does not also translate them. `translateOriginalPairBody` is available for callback implementations. It preserves `reactionFrame`, `landingCenterCm`, `airPivotCm`, and animation metadata. Source `0x106538` also moves `+9D0` and AABBs at `+400/+410`; these fields are not yet modeled by the current `PrototypeRider` world-contact adapter and remain a gap for the separate scenery phase.

`tools/test_rider_pair_system_reference.py` compares multiple live mutable participants over consecutive shared frames. It executes the original pair dispatcher, overlap kernel, full impulse/reaction request routine, actual `0x106538/0x329B40` translation, and proximity refresh. Animation lifecycle, score observers, attribute getters, and RNG access are controlled boundaries; event callbacks deliberately clear boost to verify fresh participant reads and the initiating actor's cached weight. The native test uses the same callback-side mutation, not captured frame-by-frame state. Physical positions, velocities, broad/child centers, translated offsets/AABBs, stale geometry metadata, reciprocal records, reaction ordering, and shared RNG state are compared exactly.
