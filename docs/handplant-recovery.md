# Handplant recovery (control 11 + motion 5)

Holding Circle near a rail lip performs the original SSX3 handplant. Every grindable
Snow Jam rail qualifies: its runtime flags are `0x30003`, and bit 1 is the handplant
mask. The browser now runs the port of the original code (`engine/handplant.hpp`) and
wires it into the rider through `web/handplant_gameplay.inc`. Ten PS2 captures confirm
it (see Verification).

## Original code (SLUS_207.72)

| Part | Address | Port |
|---|---|---|
| Entry test and requests | `0x107578` | `originalHandplantEntryTest`, `originalHandplantEnter` |
| Callers | cruise `0x131620`, natural air `0x12F730`, spin `0x133308` (only when air-control `+0x0C == 3`) | `hp_try_entry` |
| Motion 5 setup | `0x138BA0` | `originalHandplantMotionSetup` |
| Motion 5 enter / update / exit | `0x138B48` / `0x1391A8` / `0x139178` | `originalHandplantMotionEnter/Update/Exit` |
| Launch velocity | `0x139548` | `originalHandplantLaunch` |
| Control 11 setup / enter / update / exit | `0x1329B0` / `0x1328B0` / `0x132A30` / `0x132F98` | `originalHandplantControl*` |
| Play (with reflect) | `0x132FB8`, table `0x459FC8` | `originalHandplantPlay`, `originalHandplantClip` |
| Scoring | `0x119BF0` (begin), `0x119C38` (launch commit) | `originalHandplantScoreBegin/Launch` |

The motion tables are ordered exit `0x456B10`, enter `0x456B30`, update `0x456B50`, post
`0x456B70`. For motion 5 these are exit `0x139178`, enter `0x138B48`, update `0x1391A8`
and post `0x139528` (`0x11E150`, which only updates body query points). Air motion enter
`0x1399E0` sets air `+0` when the previous motion was 5. The air orientation tail reads
that value as `airModeFlag`.

The per-tick stage list `0x128C00` gives the order the browser follows: controller
update, boost timers `0x1200D0`, the triplet approach `0x1211F8`, the motion update, the
pose, and then the motion post/contacts.

### Corrections to the earlier notes

- **Setup bone.** `0x138BA0` measures its direction to the lip from bone `rider+0x8A8`
  (= 5), not from the board root.
- **Airborne entry check.** The test rejects animation classes 18..20
  (`0x311AE8(anim,2)`), not semantics.
- **Balance storage.** `rider+0x250` is the presentation-roll triplet that `0x11FA10`
  reads. Control 11 reuses it as the balance `b`, so the phase-2 exit bake leans the
  rider by `b` before it zeroes the triplet. `rider+0x244` is a separate triplet: the
  kind-9 seek amount (`HandplantBalance`).
- **Launch velocities in `0x139548`.** Each case sets the lateral push, then overwrites
  the vertical velocity:

  | Phase | Horizontal | Vertical |
  |---|---|---|
  | 3 | `0.7·entrySpeed` along the tangent, minus a lateral `2` (rail: `15`) `×27.78` signed by the lip direction | `0` (rail: `10×27.78`) |
  | 4 / 5 | lateral `5` (rail: `15`) `×27.78`, toward (4) or away from (5) the lip | `0` (rail: `5×27.78`) |
  | 6 | `833.33` along the tangent | `+416.67` |

  833.33/416.67 belongs to phase 6, not phase 1.
- **Launch timing.** The pinned motion update computes the launch once (`+0x10` becomes
  nonzero). It then seeds the shared predictor at lip + 5 cm·Z and steps it 8 times, and
  keeps stepping until the predictor status is 1 or 3. On later pinned ticks it aligns
  the rider with `0x121AA0`, using gain `2·ts` and max rate `6.632·ts`.
- **Speed limit.** Frame-begin `0x11B3F8` special-cases only motion 1 (3333.33). Motion 5
  uses the ordinary formula with terminal 1 and crouch 1.

## Browser integration

- **`web/handplant_gameplay.inc`** (included from `animation_bridge.cpp`) holds the
  adapter:
  - `step_handplant` runs at the start of `step_rider` through the `browserHandplantStep`
    hook, before the automatic rail attach.
  - It tries the entry for pad control 0/4/5 while Circle (command bit `0x2000`) is held.
  - A failed cruise attempt clamps turn to ±0.5, sets crouch 1 and brake 0, and skips
    `0x106848` for that tick.
  - On entry it runs the old control's exit: `0x134CB0` bake (`landing_air_exit`) for
    control 5, or the upper-channel fade for control 0/4. For a ground entry it also runs
    `originalLandingGroundLeave`.
  - While control 11 is active, it runs the control update, boost timers, the approach and
    motion 5. After launch it runs air motion (predictor + orientation tail with control 11
    and `airModeFlag`).
  - An exit event (bit 1) calls the ordinary rail attach (`step_rails`, which now accepts
    control 11).
  - When the clip finishes it plays 287 and requests control 4, or plays 61 and requests
    control 0.
- **Animation.** Kind-9 seeks (semantics 40/45, HPTS/HPHS `BAL_THROUGH/REFLECT`) now work
  in the shared graph (`engine/rider_animation_player.*`, `handplantBalance` = `+0x244`).
  `animation_tick` treats handplant ticks like rail frames. In the air it still runs the
  body query and posed landing.
- **Scoring.** Scoring uses the existing score state (`capture_score`/`apply_score`).
  Launch commits through `commit_score_state`. The names come from table 13 of the trick
  identity: kind 1 "Handplant", kind 2 "Handspring".
- **Query.** `browserHandplantQuery` in `rail_bridge.cpp` is the mask-2, 300 cm query.
- **Exports.** `handplant_info()` exports phase, motion, balance and counters.

## Verification

### Instruction oracles

The oracles run recompiled PS2 code against the port, comparing all written memory and
the ordered calls. Run them with
`python3 tools/test_handplant_{entry,motion,control,score}_native.py`.

| Routine | Cases |
|---|---|
| Entry `0x107578` (with the real setups; all seven rejects) | 60,000 |
| Motion setup | 60,000 |
| Control setup | 30,000 |
| Motion enter | 30,000 |
| Launch/exit (phases 0..7) | 40,000 |
| Motion update (real `0x121AA0`/`0x11E098`) | 60,000 |
| Control enter | 20,000 |
| Play | 28,000 |
| Control exit | 20,000 |
| Control update (every stop kind and threshold) | 120,000 |
| Scoring | 80,000 |
| **Total** | **548,000** |

Every case matches bit-for-bit. The `relative.w <= 0` branch of `0x138BA0` cannot be
reached with finite inputs, so it was reviewed by reading the instructions.

### Original-game captures

The captures run from `snow-jam-glide.p2s` (Zoe) through the production pad path
(`compare-ps2-capture.mjs --pad --zoe`). Scripts are in `local/ps2-capture/scripts/handplant-*.json`.

| Capture | Exercises | Result |
|---|---|---|
| `handplant-ground` | 11 failed cruise attempts, entry 569, balance, phase 5, launch 640, control 4, landing 682 | 499/499 ticks exact |
| `handplant-spring` | phase 3 handspring | 500/500 exact |
| `handplant-leanL` | HandplantBalance input, phase 5 on \|b\| ≥ 1 | 499/499 exact |
| `handplant-leanR` | phase 4 through exit | exact through 795; 796 is a soft collision (pre-existing) |
| `handplant-nat36` | failed cruise and air attempts, natural-air (control 4) entry | 499/499 exact |
| `handplant-nat44` | receding rejections, then cruise entry | 499/499 exact |
| `handplant-spin` | charged jump, then spin (control 5) entry after the `0x134CB0` bake | 499/499 exact |
| `handplant-rail` | phase 6 → rail attach (control 11, style 4) and grind | 499/499 exact (was exact through 637 before the 2026-09-22 rail attach fix) |
| `rail-air-fence` | no handplant; the same rail attach from ordinary air | exact through 784; 785 is a web-only instance contact after the rail |
| `air-release/handplant-flip` | the `handplant-spin` jump with a back flip and Circle held: no entry, because `0x133308` tests only in air phase 3 (and `0x107578` rejects presented up z < 0); the rider flies past and lands at 723 | 625/625 exact (2026-09-28) |

Positions and velocities are compared bit-for-bit, together with control and command
words. Both rail captures were fixed by the attach-order and `0x13AF28` corrections in
[RAIL_RECOVERY.md section 3.8](../engine/RAIL_RECOVERY.md).

Six of these captures are baselines in `test-ps2-captures.mjs`. `test-handplant.mjs`
replays six scenarios and checks bit-exact original words for entry, pin, launch,
landing, phase 6 and each entry controller, without needing the capture files.
`jump-tricks` is still exact through 664.

## Remaining gaps

- **Rail attach placement.** Fixed on 2026-09-22 (see RAIL_RECOVERY.md section 3.8).
- **Unported cruise preconditions.** Cruise preconditions `0x116378` and `0x131CC0`,
  which run before the handplant test, are not ported.
- **Unmodelled side effects.** The `0x2F6AC8` attempt flag (`owner+0xD27` bit 6) is only
  counted. The `0x29DC48` results stat and the SPOKE handplant camera (`0x161AB0`) are not
  modelled. Motion 5's post stage `0x11E150` is skipped because it only updates body query
  points.
- **Landing during control 11.** Landing while control 11 is still in the air goes through
  the browser's `finish_landing` (control 0). The original keeps control 11 on the ground
  until the clip completes, then plays 61. No capture has hit this.
- **Ground speed limit.** After the handplant landing in `handplant-ground`, the
  frame-begin ground speed limit differs by 1 ulp at tick 684, with positions unaffected.
  Crouch decays from 1 on landing there. The cause is in the ground speed-limit filter,
  not the handplant.
