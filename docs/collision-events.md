# Original collision reactions

`collision_event.hpp` translates the decision logic of PS2 USA `105D98` and `108388`. It remains separate from runtime dispatch until the required control and motion modes can execute their real update/recovery paths. Assigning a crash ID while continuing ordinary grounded physics would be incorrect.

The input contact contains the world point, pre-bounce incoming direction, adjusted collision normal, closing speed in cm/s, and raw surface ID. Classification uses the pre-impact presentation frame at rider+160..190 and the rebuilt physical frame separately. The presentation frame must precede the body-only presentation lift. `BodyCollisionVolume::reactionFrame` carries this native-generated frame.

Each contact updates rider+3E0 (previous normal) and +3F0 (direction-change accumulator). The accumulator adds `1 - max(dot(normal,previousNormal),0)` in the original scalar operation order. Surface property+44 can request a forced reset. Other contacts update the peak impact scalar and choose a reaction according to motion/control mode, presentation axes, contact height, post-bounce speed, and original authored thresholds. Some glancing side reactions change manual spin and consume one original random draw. Selection does not consume random values on paths that did not do so in the game.

| Reaction path | Original consequence |
| --- | --- |
| Control state 9 | Ignore the contact before updating history |
| Surface property+44 nonzero | `116120` requests control state 9 / motion mode 3 |
| Existing motion mode 2 | Mark another crash collision and optionally reseed trajectory |
| Soft grounded impact | Play semantic 55..60, possibly alter manual spin, enter control state 3 |
| Hard impact | `10EB30` ultimately enters control state 8 / motion mode 2; state 13 is transient |

`1210B0` decays the previous normal and counters once per original tick. The normal factor is 0.980000019, direction-change factor 0.955999970, and secondary-counter factor 0.978333354. A decayed direction-change accumulator above 4.5 requests recovery reason 2. This is a fixed-tick operation, not a guessed wall-clock decay.

`test_collision_event_reference.py` compares complete original decision routines against the typed helper with external dispatch effects intercepted. Thirty thousand cases agree on reaction, animation semantic, history, peak intensity, manual spin, predictor reseed, and random-draw count/order: 13,061 ignored, 130 hazard resets, 5,225 existing-crash impacts, 4,194 soft reactions, and 7,390 crash selections. This validates decisions, not the full crash simulation.

Remaining lifecycle addresses: control state 3 updates through `12E778`; crash control state 8 through `12CB68`; reset control state 9 through `12F398`. Motion mode 2 uses `136E98` / `136EE0`, including airborne and sliding submodes. Motion mode 3 uses `136958` / `136978`. Animation clocks, trajectory state, source random-generator state, and physical recovery transitions must remain synchronized before these reactions can be activated as fully supported gameplay.

`12E778` on a grounded soft impact only retargets turn, crouch, and brake. It does not write rider velocity. On the Snow Jam 240-frame left carve the wall response itself matches (361 cm/s near −40°), and the unmatched piece is the next three velocity headings (−31°, −20°, −12°). That search is recorded in [obstacle collision](obstacle-collision.md) under "Snow Jam long left turn". The shipped step was not changed to fit it.
