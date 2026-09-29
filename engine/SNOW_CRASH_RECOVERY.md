# Crash snow: BodySnow producer and crash impact triggers

Recovered from the PS2 SLUS_207.72 recompiled source (`local/output`) and ELF
constants. Native code: `engine/snow_crash.hpp` (header-only). Oracle:
`tools/test_snow_crash_native.py` + `tests/snow_crash_reference.cpp` (20,000
whole-original `2E2260` cases, bit-exact). Unit checks: `engine/snow_crash_tests.cpp`.

## Where crash-time snow comes from (verified)

While a rider is in motion mode 2 (wipeout), `2DF960..2DFB5C` sets `FX+B0`
(`groundEmission`) false, so SnowTrail 0, Cloudy 7, Chunky 1/2 carve/brake and
the deceleration impact request (`2DFCxx`, kind=true) are all off. The visible
crash spray in the original comes from exactly two producers:

1. **Impact bursts** (`2E1598`, emitters 5/6) fed by `2E23E0` triggers that the
   crash physics raises through `111AA0` (kind=false). Chunky 1/2 also fire their
   impact burst from the same retained strength with the extra ±55 cm jitter when
   `motionMode==2` (already native: `originalSnowChunkEmission`).
2. **BodySnow** (`2E2260`, emitter 9): a per-tick puff at a cycling body bone
   while the impact buildup `FX+4` is positive. `2E1598` raises that buildup up
   to 2.0 on each burst (the `motionMode==2` branch adds `2*amount*scale`).

Kicker 8 (`2E1F70`), Rock 3 (`2DFE88`) and RiderBreath 4 (`2E1120`) are not part
of the crash path and remain unrecovered.

## `2DF920` per-rider update prefix (verified)

```
2DF938  if (FX+60 != 0) 2DF448(FX)     ; deactivate all ten emitters, FX+60 = 0
2DF94C  if (FX+80 == 0) 2DF4D0(FX)     ; bind the thirty BodySnow bones, FX+80 = 1
2DF960  FX+12C = 0 ; rider cache ...
...
2DFE4C  2E2260(FX)                      ; BodySnow 9   (after Cloudy 7, before Kicker 8)
2DFE64  2E1F70(FX)                      ; Kicker 8
```

`2DF3B0` (reset) zeroes `FX+4` (buildup), `FX+78` (bone cursor), `FX+80` (bones
bound), `FX+E0` (impact strength) and sets `FX+120 = 0.5`, `FX+90 = (1,1,1,1)`.

## Bone binding `2DF4D0` (verified)

`FX+7C` is a 30-word heap table (allocated in the constructor at `2DF174`).
`2DF4D0` fills it by `310C48(rider+780 skeleton, part 0, name)` which is a
linear `strcmp` (`4165A8`) over the skeleton's bone names and returns
`part.firstIndex + i`, or **-1** when the name is absent. Table order:

| i | name | i | name | i | name |
|---|---|---|---|---|---|
| 0 | shinleft (4879D8) | 10 | neck (4A3B08) | 20 | thighright |
| 1 | footleft (4879E8) | 11 | clavicleleft (487A68) | 21 | hips |
| 2 | shinright (4879F8) | 12 | handright (487A78) | 22 | lowerspine |
| 3 | footright (487A08) | 13 | bicepleft (487A88) | 23 | middlespine |
| 4 | thighleft (487A18) | 14 | biceptwistleft (487A98) | 24 | forearmleft (487AA8) |
| 5 | thighright (487A28) | 15 | shinleft | 25 | handleft (487AB8) |
| 6 | hips (4A3B00) | 16 | footleft | 26 | clavicleright (487AC8) |
| 7 | lowerspine (487A38) | 17 | shinright | 27 | bicepright (487AD8) |
| 8 | middlespine (487A48) | 18 | footright | 28 | biceptwistright (487AE8) |
| 9 | upperspine (487A58) | 19 | thighleft | 29 | forearmright (487AF8) |

Legs, hips and lower spine appear twice, so 20 of 30 ticks land on the lower
body. In the shipped rigs (`RIDER_ZOE/MAC/SAM/rider.json`, original skeleton
order) this resolves to indices
`17,18,20,21,16,19,0,1,2,3,4,6,15,7,8,17,18,20,21,16,19,0,1,2,9,10,11,12,13,14`.
`head` (5) and the board bones are never used.

## BodySnow `2E2260` (verified, bit-exact)

Inputs: `FX+4` buildup, `FX+B8` speed (the cache speed, `sqrt` of rider+1E0 so
non-negative; `+BC` is its `abs`), `FX+78` cursor, `FX+7C` bone table, `FX+90`
colour `(2R,2G,2B,1)`, `FX+D4` motion mode (`11FE98`), `FX+2C+900` = emitter 9
`VelScale` (`3F4CCCCD` = 0.8, from `gp-3B9C`), rider+1E0 velocity, rider+780 →
geometry+30 world matrices (64 bytes each, row 3 = translation cm).

```
if !(0 < buildup)            goto inactive              ; c.lt.s 0,FX+4
if !(83.33333587646484 < speed) goto inactive           ; gp-3A4C = 42A6AAAB (2.5/0.03 cm/s = 3 km/h)
scaled = buildup * 1.5                                  ; 3FC00000
alpha  = (0 <= scaled) ? min(scaled, 1.0) : 0
bone   = FX+7C[FX+78]
request(emitter 9, position = worldMatrix[bone].row3,
        velocity = rider+1E0 * VelScale,                ; vmulx (xyzw)
        colour   = (FX+90.rgb, alpha),
        active   = 1, dt = 1/60)                        ; gp-3A48 = 3C888889
FX+78 = (FX+78 + 1 < 30) ? FX+78 + 1 : 0
if FX+D4 != 2: FX+4 = max(FX+4 - 1/60, 0)               ; gp-3A48
return
inactive:
request(emitter 9, position = worldMatrix[rider+89C].row3, velocity = null,
        colour = null, active = 0, dt = 1/60)           ; gp-3A44 = 3C888889
```

Properties worth knowing when integrating:

- No random draws: `2E2260` never touches the shared LCG `4A3AFC`; the particle
  program's own seeds are consumed inside `3717C0` exactly as for other emitters.
- The buildup does **not** decay while `motionMode == 2`, and never decays on an
  inactive tick (speed below the gate or buildup already 0). A crash that stops
  the rider keeps its buildup until the rider moves again. After get-up the
  puffs fade over `buildup*60` ticks (up to 2 s) at a diminishing alpha.
- Alpha saturates at buildup ≥ 2/3. The emitter's authored `StartColA = 0.25`
  and `Life 0.4 s`, `Damp 1.5`, `SizeFinal 51.22`, random velocity ±200 cm/s per
  axis, texture 5 (`spry`), 2 particles per birth.
- The inactive request position (rider+89C = primary/root bone, posed bone 0 in
  the shipped riders) matters only as the emitter's retained origin.

## Crash impact triggers (verified addresses; native mapping inferred)

`111AA0(rider, position*, normal*, surface, f12 strength)` is a pure wrapper:
`2E23E0(*(rider+77C)+B40, position, normal, strength, surface, kind = 0)`.
`2E23E0` takes `|strength|`, retains it doubled (`kind==0`), and only accepts a
report that is stronger than the retained one and ≥180 cm (100 with wide
scatter) from the retained point. The five callers:

| Caller | Site | position | normal | strength | surface | Native equivalent |
|---|---|---|---|---|---|---|
| `10EB30` crash entry | `10EC04` | event+0 | event+20 | event+30 closing speed | s2 | `crash_entry.hpp` `reportImpact(event, type)` → `OriginalCollisionEvent::pointCm/normal/closingSpeedCmps/surface` |
| `137860` motion-2 airborne body/terrain landing | `1379D4` | `138960` probe point (sp+3A0) | probe normal (sp+3B0) | `dot(vel, normal)` ≤ 0 | probe surface (sp+3EC) | `crash_collision.hpp` `OriginalCrashAirContactEffects{landed,impact,impactSpeed}`; actor `contactPoint/groundNormal/surface` are the same values |
| `137D18` sliding contact | `138454` | hit point (s2) | hit normal (sp+C0) | deep branch `|surfaceVelocity|`, shallow branch `|velocity|` before the correction | hit surface (sp+FC) | `crash_ground.hpp` `originalCrashSlidingContact` `effects.impact/impactSpeed`; actor `contactPoint/groundNormal/surface` |
| `10E910` ordinary landing award | `10EA08` | record+0 | record+10 | f21 | record+4C | existing `gameplay_effects.mm` `landed` path (`lastLandingContact`) |
| `1242B0` cruise scenery probe | `1245BC` | (probe) | sp+60 | `|sp+70|` | sp+9C | not connected (cruise-mode scenery hit; outside this task) |

Gates worth restating: `137860` reports only when the probe fraction ≥ `gp-7268`,
the normal speed is ≤ 0 (closing) and the body is within 10 cm of the hit;
`137D18` reports only on penetration (`distance < 0`). Ragdoll body-vs-scenery
collisions (`105D98` → control `+54/+60`) are consumed by `12D4E8/12D160/12CB68`
through `28B180/296xxx` observers only: **they never call `111AA0`**, so the
native `reportImpact(float)` callbacks in `crash_control/crash_recovery` must not
be routed to the snow trigger.

`riding.hpp` currently records only `lastCrashImpactCmps`/`lastCrashTick` from
these events. The point/normal/surface are the crash actor's `contactPoint`,
`groundNormal`, `surface` after the same tick for `137860`/`137D18` (assigned
from the hit before the report), and the collision event fields for `10EB30`.
The integrator should capture those alongside the speed (see the report).

## Constants (hex bit patterns)

| value | pattern | source |
|---|---|---|
| 83.33333587646484 speed gate | `42A6AAAB` | `49F6A4` (`gp-3A4C`) |
| 1/60 dt and buildup decay | `3C888889` | `49F6A8`, `49F6AC` (`gp-3A48`, `gp-3A44`) |
| 1.5 alpha gain | `3FC00000` | immediate `2E22AC` |
| 1.0 alpha clamp | `3F800000` | immediate `2E22B4` |
| 0.8 BodySnow VelScale | `3F4CCCCD` | `49F554` (`gp-3B9C`), constructor `2DF15C` |
| 30 bones | `0x1E` | `2E2358` |
| emitter 9 offset | `0x1290` = 9·0x210 | `2E2290` |

## Verified vs inferred

Verified (instruction-level, oracle-tested): the whole `2E2260` request,
state updates, constants, bone-name table and its order, `111AA0` semantics,
the `2DF920` call order and prefix, all five `111AA0` call sites' arguments.
Inferred: that the native pose index equals the original skeleton bone index
(true for the shipped `rider.json` order and already relied on for board bone
23); the mapping of `137860`/`137D18` arguments to the native actor fields
(read from the native helpers, not re-run against the original).
Not recovered: Kicker 8, Rock 3, RiderBreath 4, the `1242B0` cruise scenery
trigger's exact position argument, and `310C48`'s behaviour when part 0 has no
bones (returns -1; native throws).
