# Original rail (grind) recovery

Recovered from the PS2 USA executable `local/disc/SLUS_207.72` (recompiled
evidence in `local/output/sub_*.cpp`) and the SSB course container
`DATA/WORLDS/BAM.BIG`. Native code: `rail_motion.hpp` (header-only,
`namespace ssx`), tests `rail_motion_tests.cpp`, data exporter
`tools/import_rails.py`. Nothing here executes console code in the product.
Confidence levels are explicit; unrecovered branches throw.

## 1. Authored rail data: SSB record kind 8

The original world loader `0x26DED8` dispatches record kinds 0..22 through the
table at EE `0x4816C0` (kind 8 → constructor `0x341548`, kind 14 → `0x34ED88`
AIP paths, kind 16 → `0x350F08` collision bindings, kind 21 → `0x349DB0`
course spine, kind 17 → `0x356E60` course-progress objects). Kind 8 records are
named in the PHM/PSM "splines" array (group 3): every ARA1 record has a name
such as `spline_ARA1_EventRail_0`, `spline_ARA1_GondolaRail_0`,
`spline_ARA1_LogRail_0`, `spline_ARA1_RAIL_wood_1003`; the rail-named scenery
instances (`mdl_ARA1_EventRail_2002`, `mdl_ARA1_bcrail_1002`,
`mdl_BRA2_polyrailslides_*`, …) sit 0.2–7 cm from the exported curves.

Counts (PS2, all verified against the header count and the chaining rules):

| Area | Rails (records) | Segments | Total length |
|---|---:|---:|---:|
| ARA1 | 171 | 777 | 4991.8 m |
| BRA2 | 252 | 983 | 7394.7 m |
| CRA3 | 190 | 764 | 5125.7 m |
| DRA4 | 80 | 501 | 3372.7 m |
| ERA5 | 138 | 824 | 3419.0 m |

Example ARA1 endpoints (native meters, Y-up):
`spline_ARA1_EventRail_0` 44.1 m, `[-1716.8,-2763.0,-383.4] → [-1748.4,-2785.5,-379.7]`;
`spline_ARA1_EventRail_1` 53.3 m, `[-1953.8,-2927.3,-438.8] → [-1976.0,-2936.0,-394.4]`.

### Record layout (verified on all 831 records of the five race areas)

Header, 48 bytes:

| Offset | Meaning |
|---|---|
| +0x00 | u32 packed id `(rid<<8) \| track` (ARA1 track 8, BRA2 track 16). Stored to motion+0x24 at `0x13B054`. |
| +0x04 | float3 bounds min, +0x10 float3 bounds max (source cm, Z-up) |
| +0x1C | u32, 0 in the file. At runtime this is the descriptor flag word read by `0x334680`/`0x3411B8` (`desc+0x1C & mask`, rider mask 1). The loader must set bit 0; it is not authored. Bit 2 doubles the imbalance term (`0x13B66C`). |
| +0x20 | u32 segment count |
| +0x24 | u32 `0x412Dxxxx`, differs per record; retained raw (tool pointer residue, unused by the recovered code) |
| +0x28 | i32 surface id: −1 on disc, patched at load (Snow Jam 10 metal/fence, 9 wood; `rails.json` `runtime_surface`) → query result +0x4C → rider+0x438 (read by the rider FX passes: board sparks, snow impacts) |
| +0x2C | u32 0 |

Segment, 144 bytes (stride confirmed by `size = 48 + 144·count`):

| Offset | Meaning |
|---|---|
| +0x00..+0x0B | three words patched by the loader (list link, type = 1 at +0x08 for the `0x334680` walker); file values are residue |
| +0x0C | float arc length of this segment |
| +0x10 | float4 row for t³, +0x20 row for t², +0x30 row for t, +0x40 row for 1 (= start point, w = 1). `point(t) = c0·t³ + c1·t² + c2·t + c3`, `0x335128` evaluation order. |
| +0x50 | float4, small values (order 1e‑3); not consumed by the recovered code, retained raw as `row50` |
| +0x60 | i32 previous global segment index (−1 at a rail start), +0x64 next global index |
| +0x68 | i32 rail index (= rid); at runtime replaced by the pointer to the record header (descriptor used at `0x33592C`/`0x335940`) |
| +0x6C | float3 segment bounds min, +0x78 float3 bounds max |
| +0x84 | float cumulative distance at t = 0 (= previous distance + previous length) |
| +0x88 | pointer residue; +0x8C u32 flags, always 15 |

`point(1)` of segment k equals segment k+1's constant row within float rounding
(exporter enforces ≤ 1 cm, cumulative distance ≤ 0.5 cm). The GameCube `bam.gsb`
uses the same records big-endian; the exporter currently reads the PS2 file.

`local/assets/native/<AREA>/rails.json` keeps the raw source cm/Z-up rows and
bounds, the native meters/Y-up `(x,y,z)→(x,z,−y)·0.01` conversion, packed ids,
names, sha256 of the SSB/PHM/PSM and of every record, plus a scenery proximity
summary.

## 2. Runtime structures

| Object | Fields |
|---|---|
| Motion owner `rider+0x77C` | +0xDE0 motion mode, +0xDE4 control state, +0xB0 rail motion (mode 4), +0x2B0 rail control (control 7). Getters `0x11FE98`/`0x11FEE8`; requests `0x11FE78(rider,motion)`/`0x11FEC8(rider,control)` → `0x1112B8`/`0x111538`. |
| Rail motion `owner+0xB0` | +0x00 rail direction (travel-oriented), +0x10 lean (rad), +0x14 time on rail, +0x18 balance −1..1, +0x1C heading offset, +0x20 lost-rail flag, +0x24 rail packed id, +0x30 entry position, +0x40 word |
| Rail control `owner+0x2B0` | +0 accumulated spin (±π/2 per rotation), +4 last identity byte, +8 rider |
| Rider | +0x110 position, +0x120 quaternion, +0x1A0/+0x1B0/+0x1C0 right/forward/up, +0x1E0 velocity, +0x22C/0x230/0x234 rail steer triplet (written by `0x113F38`), +0x238/0x23C/0x240 balance triplet, +0x25C/0x260/0x264 attach-tolerance triplet, +0x2E4 speed limit, +0x2FC boost, +0x300 time scale, +0x320 switch flag, +0x324 reference stance, +0x328 style, +0x330 transfer flag, +0x370 contact normal, +0x3D0 surface velocity, +0x438 surface, +0x460 contact point, +0x9D0 board query offset, board root bone `*(+0x780+0x2C) + (+0x8A0)·32` (position +0, quaternion +0x10) |
| Query result (`0x334680` out) | +0 point, +0x10 unit tangent, +0x20 zero normal, +0x30 zero velocity, +0x44 kind 2, +0x4C surface, +0x50 instance (0 for splines), +0x58 descriptor (record header), +0x68 t |

Control dispatch (tables `0x456C10` update / `0x456BD0` enter / `0x456B90` exit):
control 7 = `0x131D08` enter, `0x131D30` update, `0x132048` exit. Control 13 has
no handlers (transition placeholder requested before 7). Motion dispatch
(`0x456B50`/`0x456B30`/`0x456B70`/`0x456B10`): motion 4 = `0x13AD20` enter, `0x13AF28`
update, `0x13BFA8` post (every tick, after the pose), `0x13C5A0` exit. Requests are
immediate: `0x1112B8`/`0x111538` run the old handler's exit, store the mode, then run
the new handler's enter, all inside the caller (see section 3.8).

## 3. Algorithms

### 3.1 Spline query `0x334680` → `0x335128` (verified, exact transcription)

`0x334680(world, point, out, mask, radius)`: box = point ± (radius, radius,
radius); iterate world layers `world+0x214[0..world+0x210)`: type 2 → `0x35C698`,
type 3 → `0x348290` (sphere-tree instances, not represented natively), type 1 with
`desc+0x1C & mask` → `0x335128`. Every rider caller uses mask 1, radius 300.

`0x335128` per segment: reject if the box misses `+0x6C..+0x80`; P0 = row3;
best = |q−P0|, index 0; for i = 1..4: t = i·0.25, Pi = M·(t³,t²,t,1);
fraction = dot(q−Pprev, seg)/dot(seg,seg) (EE DIV), clamp to [0,1] (0 when
negative), closest on chord, replace when strictly closer, index = i.
Bracket from EE tables `0x48E560` low = {0,0,0,.25,.25}[index] and `0x48E578`
high = {.75,.75,1,1,1}[index]. Golden section with c1 = `0x3EC3910C`
(0.381966), c2 = `0x3F1E377A` (0.618034), tolerance `0x3A03126F` (0.0005),
≤ 24 iterations: x1 = low+(high−low)·c1, x2 = low+(high−low)·c2; evaluate
P(low), P(x1) (d1), P(x2) (d2), P(high); loop while |high−low| > tol:
if d2 < d1 { low = x1; x1 = x2; d1 = d2; x2 = x1·c2 + high·c1 } else
{ high = x2; x2 = x1; d2 = d1; x1 = x2·c2 + low·c1 }. Output point is the last
evaluated curve point, t the last chosen abscissa, distance its distance; the
tangent is M·(3t², 2t, 1, 0) normalized with VRSQRT. Result replaces the
accumulator only when strictly closer. Cross-checked on 400 random probes
around real ARA1 rails: all found, nearest distance within 0.21 cm of a
brute-force search.

### 3.2 Attach test `0x108A48` + `0x1086B8` (verified)

Motion must be 0 or 1. q = bone + rider+0x9D0; query. If |v| > 0.001:
reject when dot(hit−q, v) < −0.2·|v| (rail behind). Channel-2 class gating:
class 18 needs sequence flag bit 2 or not bit 0; classes 19/20 need bit 2
(`0x1446A0` = bit test on sequence+0xB0). Then `0x1086B8`: board x-axis from the
bone quaternion; f2 = rider+0x25C; halfWidth = 50·f2 + 30·(1−f2);
reach = 170·f2 + (rider+0x330 ? 50 : 100)·(1−f2); clamp the projection of
hit−q on the board axis to ±(reach−halfWidth); accept when the clamped board
point is within halfWidth of the hit. Otherwise, unless the rail is parallel to
the board (1−dot² < 0.001 → reject), re-project through the rail direction,
query again at the new board point and accept when within halfWidth.

### 3.3 Attach `0x106848` (verified except instance branch)

Called from controllers 0/1/2/3/4/5/11/12 (`0x131620`, `0x12FC80`, `0x12E9B8`,
`0x12E778`, `0x12F730`, `0x133308`, `0x132A30`, `0x136508`) and passive-air
`access.rail`. Velocity: d = tangent oriented along v; along = max(|dot|,
555.5555 = `0x440AE38E`); v = d·min(|v|, along); v.z ·= 0.1. (An instance hit
receives −Δv through its virtual +0x158/+0x15C; not representable natively.)
Style from angle = atan2(dot(boneZ, d), dot(boneX, d)) via `0x31C228`:

- control 1: style = switch ? 2 : 1
- rider+0x330 ≠ 0: |angle| > 90° ? 2 : 1
- control 12: 3
- bone y-axis z < 0 (inverted): dot(boneZ,d) < 0 ? 4 : 3
- |angle| > 150° (`0x40278D37`): 2; |angle| < 30° (`0x3F060A93`): 1;
  angle < −30°: 4; else 3

Then rider+0x438 = surface; scoring: motion 1 → `0x10E910(rider,0,style,
+0x330, out, speed)`, else `0x119D40(rider+0x790, +0x320≠+0x324, 0, style, +0x330)` →
`0x10E098(rider,1,value)`; request motion 4; `global(gp+0x410)+0x598C = surface`.
Controls 0/4/5/11: request control 13, `0x115358`, then if +0x330 = 0 play
`0x1326C8` semantic (styles 1/2 → 18, 3 → 19, 4 → 20 grounded; 68/70/69 from
controls 4/5) and request control 7; +0x330 = 1/2 → semantics 26/34 and
control 1 (gap, throws). Other controls with a changed style: old style 3/4
rotates the physical quaternion ±90° about up (`0x11DFE0`), `0x311B48` and
resets the animation root; then `0x115358`. Finally `0x13ADC0` heading offset.

**Correction (2026-09-22).** `requestMotion(4)` runs `0x13AD20` right there (before the
control requests, `0x115358` and `0x13ADC0`), and `requestControl(13)` runs the old
controller's exit (control 5 `0x134CB0`, 0 `0x131C30`, 4 `0x12FB68`, 11 `0x132F98`).
`0x11DFE0` ends in `0x11E098`. With +0x330 set, the 0/4/5/11 branch plays 26/34 and
requests control 1. The whole sequence is oracle-verified (section 3.8).

### 3.4 Stance alignment `0x115358`/`0x115168` (verified by instruction oracle)

Style 2 needs switch = 1, style 1 needs 0, styles 3/4 need 0. A mismatch multiplies the
quaternion by the pure quaternion (up, 0) (VU product) and runs `0x11E098`. `0x115168`
then toggles +0x320, calls `0x311B48(anim, π)`, sets anim+0x30 = `0x4FF130` (0,0,0,1)
and anim+0x40 = sincos(−π/2 or −0) about `0x4FF160`, writes anim+0x18 (mirror) =
+0x320, and negates +0x3A0 and +0x3B0 and the current and target values of +0x1F0
(turn), +0x1FC (animation turn), +0x208 (extra lean), +0x214 (brake) and +0x280.
Styles 3/4 then run `0x11DFE0` by +π/2 (4) or −π/2 (3) about up. `0x11DFE0` ends in
`0x11E098`, so the basis that `0x13ADC0` reads is already rotated. They also run
`0x311B48` by the same angle and set the root from sincos(−π/4) (4) or (+π/4) (3).
Every path ends in `0x116930`, which is empty. `0x311B48(anim, a)` rotates the sequence
roots by **−a**: it takes sincos(−a·0.5), scales `0x4FF160` = (0,0,1,0) by the sine, and
calls `0x311BF0`.

### 3.5 Motion 4 (verified; dynamic-instance callbacks omitted)

`0x13AD20` gain: `0x11FA10`, rebuild, zero turn/extra-lean/presentation-roll
triplets, lean/time/balance/heading/lost/word40, +0x2DC = 0, tolerance
rate 0.05 (`0x3D4CCCCE`) target 0, entry position.

`0x13ADC0` heading: normal from `0x13BD80`; lateral = normal × direction;
angle = atan2(dot(forward, lateral), dot(forward, direction)) clamped to
±0.7853982 (`0x3F490FDC`).

`0x13BD80` normal: |tangent.z| ≤ 0.92 → normalize(Z − tangent·tangent.z),
sign-matched to rider up (lean unused); otherwise lateral =
normalize(up × tangent), perp = tangent × lateral, normal = perp·cos(lean) −
lateral·sin(lean).

`0x13AF28` update, per tick (dt = timeScale/60):
1. control 12 zeroes the heading offset; clamp |v| to +0x2E4.
2. lost = 1; query at the bone position (radius 300). No hit → rebuild only.
3. rail id, surface, contact point, zero surface velocity.
   f20 = (|v| < 555.5555 ? time : |v|)·0.0009; f24 = f20².
   time < 0.6 → rider+0x25C = 2.
4. `0x1086B8` fails → detach: dir = hit − bone normalized (tangent when tiny
   or |dot(dir,tangent)| > 0.5); if dot(v,dir) < 0: dir = normalize(dir −
   v·(dot/|v|)) (fallback tangent / v/|v|); v += dir·277.7778 (`0x438AE38E`);
   rebuild; return.
5. lost = 0; time += dt; tangent oriented along v − surfaceVelocity;
   store direction. lateral = dot(right, hit−bone); vertical = dot(hit−bone,
   up) clamped to ±300·dt → position += up·vertical.
6. v = ((tangent·along)·30dt + rel·(1−30dt)) rescaled to |rel|, + surface
   velocity. accel = tangent·dot((0,0,−980), tangent) + (boost > 0 ?
   tangent·(±2450·boost) : 0); v += accel·dt; position += v·dt.
7. slide = steer(+0x22C, negated when up.z < 0)·180·clamp(f20, .5, 2).
   Imbalance: −5 (skipped) when time < 0.6 or |tangent.z| > 0.92; else 0.3
   (styles 1/2) or 1.8 (3/4), doubled when descriptor bit 2, min(·, −1) when
   |lateral| < 2.5, divided by (1 + 1.650076·stat `0x149208` bytes +6/+14).
   When ≥ 0: slide −= clamp(lateral, ±20)·imbalance·clamp(1/f24, 1, 4);
   inverted rider with |lateral| < 20 forces |slide| ≥ 360 away.
   position += right·slide·dt.
8. Styles 3/4: balance = clamp(lateral/70), target lean = balance·30°,
   heading clamp ±40°; styles 1/2: balance = clamp(lateral/30), target =
   balance·20°, clamp ±20°. Target negated when up.z < 0; lean approaches at
   300 rad/s·dt (effectively snaps). Normal (`0x13BD80`) → rider+0x370;
   targetUp = normal·cos(lean) + (normal × tangent)·sin(lean); targetForward =
   normalize((lateral·cos(h) − tangent·sin(h)) × targetUp);
   gain = ((dot(targetForward, forward)·0.5 + 0.5)·15 + (1−…))·timeScale;
   `0x121AA0(rider, targetUp, targetForward, gain, 1e10)`; `0x11E098`.

`0x13BFA8` loss: `0x11E150`; lost → steer target 0 rate 1/30, `0x114298(−1)`;
score kind 22 (control 12 uses table `0x458230` by owner+0x394 — gap);
`0x105398`, `0x13C140`, `0x107888`; lost && motion 4 → request motion 1;
`0x294170`; clamp speed. Only the decisions are native; the effect block is
the mandatory `leaveEffects` callback.

**Corrections from the `0x13AF28` instruction oracle (2026-09-22).** These fix steps 3,
5, 7 and 4 above:
- Step 3: f20 starts at 0.5, and becomes |v|·0.0009 only when |v| ≥ 555.5555. Below that
  speed, time on rail plays no part.
- Step 5: the vertical correction is clamped to [−1000·dt, 1000·dt] (`0x447A0000` at
  `0x13B1D8`), not ±300·dt.
- Step 7: descriptor bit 2 doubles the imbalance, and only otherwise does
  |lateral| < 2.5 cap it at −1. The offset term is **always** subtracted. A negative
  imbalance (−5 on entry or on near-vertical rails, or the −1 cap) uses the unclamped
  lateral offset, which pulls the board onto the rail. The inverted ±360 push is also
  unconditional.
- Step 4 (detach): the push direction is bone − hit, away from the rail. The
  web implementation had it reversed.

`0x13BFA8` is motion 4's **post** stage, table `0x456B70[4]`. It runs every rail tick after
the pose, not only on a loss. Its order is `0x11E150`, then (if lost) the steer reset and
`0x114298(−1)`, then `0x105398`, `0x13C140` and `0x107888` against the freshly posed
body, then (if lost and still motion 4) `requestMotion(1)`, then `0x294170`, then the
speed clamp.

### 3.6 Control 7 (verified except identity/transfer)

Command (RLE bits removed): bit 12 recovery → `0x116120`; bits 13/14 upper →
`0x1162C8(bit14, bit13)`; bits 15/16 boost → `0x114130(held = bit16, pressed =
bit15)`; bits 17..24 signed identity → `0x132620` (control 12 / trick naming,
gap); bits 25..30 turn ×1/31 → `0x113F38`; word1 bits 0..5 rotate, bits 6..11
transfer (×1/31, `0x3D042108`).

Enter `0x131D08`: control+0 = 0, +4 = −1, rider+0x360 = 0, animation-turn
rate 1/15 target 0. Exit `0x132048`: balance target 0 rate 1/15.

Update `0x131D30` order: recovery; `0x132770` (motion 1: wait for a class-14
clip's bit 0, `0x115640`, control 4 if rotate = 0 else 5); upper; identity;
boost; `0x113F38`; `0x113F88(0,0)`; `0x115B58`; `0x115D48`. Class 14 playing:
return until bit 0; then `0x1161D0(transfer)`; rotate ≠ 0 → balance target 0,
`0x132060(left = rotate < 0)`. Otherwise `0x1161D0`; input = rotate, or
transfer when rotate = 0; nonzero → rotation. Else balance target = class 10 ?
0 : clamp(motion+0x18·1.2, ±1), rate 1/15; ensure semantic 18/19/20 by style
(`0x312AA0` compare, `0x3128E8(…, −1, 0)`). **Correction:** the class-10 branch
(`0x131F7C..0x131F98`) sets the balance target and returns. No cycle is requested while
the 68/69/70 air entry plays, so the entry clip keeps its fade-in.

`0x113F38`: rate = clamp(|input − current|·7, 0.1, 8)/60, target = input
(also the soft controller's `requestBalance` in motion 4).

`0x132060` rotation table (semantics class 14, `RailSpin`):

| Style | Left → | Right → |
|---|---|---|
| 1 | 3, sem 49, root sincos(+π/4) | 4, sem 50, root sincos(−π/4) |
| 2 | 4, sem 53, switch 0, balance negated, root −π/4 | 3, sem 52, switch 0, negated, root +π/4 |
| 3 | 2, sem 51, switch 1, negated, root −π/2 | 1, sem 51, root −0 |
| 4 | 1, sem 54, root −0 | 2, sem 54, switch 1, negated, root −π/2 |

control+0 ∓= π/2; then `0x119918(rider+0x790, style, spin)` → `0x10E098(rider,1,·)`.

`0x1161D0` transfer: only when input ≠ 0, style 1/2, and (motion ≠ 0 or
dot(v, rider+0x3A0) ≥ 0): semantic 24 (input > 0) or 32, +0x330 = 1/2,
control 1. The positive outcome is a gap (throws); the declines are native.

### 3.7 Animation semantics (state table `0x446990`)

18/19/20 rail cycles (class 15), 68/69/70 airborne entries (class 10), 49–54
rail spins (class 14, completion kind 6), 26/34 transfer clips (class 10).
Clip names in the animation library: `RSFS_GRIND1..4_CYC`, `RSREG/RSFAKIE/
RSBS/RSFS_INTO_FS_GRIND1..4`, `RSFS_GRIND1..4_BAL_L/R`, `RSFS_OUTOF_GRIND1..4`.
The mapping of semantic → clip variant is the animation exporter's job.

### 3.8 Attach tick, motion 1 → 4 (2026-09-22, verified by instruction oracles and captures)

What the original does from the attach test to the end of the first rail tick. The case
here is an air attach: control 5, motion 1. Controls 0/4/11 differ only in the old
controller's exit.

1. **Controller stage.** The control-5 update `0x133308` calls `0x106848`, which does
   the following in order:
   1. Velocity (tangent·min(|v|, max(|along|, 555.5555)), z·0.1) and style from the
      cached board bone.
   2. `0x10E910` (motion 1) or `0x119D40`/`0x10E098` scoring.
   3. `requestMotion(4)`, which runs immediately: `0x1112B8` runs motion 1's exit
      `0x139A18` (a bare `jr ra`), then motion 4's enter `0x13AD20`. `0x13AD20` runs
      `0x11FA10(rider, rider+0x110)`, which bakes the turn/lean/roll presentation around
      the sampled board bone into the physical root, with no `0x11EB98` lift. It then
      runs `0x11E098`, zeroes the +0x1F0/+0x208/+0x250 triplets, +0x2DC and the motion's
      lean/time/balance/lost/heading, sets the +0x25C rate to 0.05 with target 0, copies
      entry = position quad, and zeroes word40.
   4. `surface → global(gp+0x410)+0x598C`.
   5. `requestControl(13)`, which runs control 5's exit `0x134CB0`: fade channel 1 by
      0.33, zero the prewind, bake the `0x134DD0` air presentation, `0x11E098`, and
      quantize +0x2DC (now already 0). Control 13 has no enter.
   6. `0x115358` (section 3.4).
   7. `0x1326C8`, which plays 68/70/69 for an air entry or 18/19/20 otherwise.
   8. `requestControl(7)`, which runs `0x131D08`: control +0 = 0, +4 = −1,
      rider+0x360 = 0, +0x1FC rate 1/15, target 0.
   9. `0x13ADC0`, the heading offset from the rebuilt forward.
2. **`0x1200D0`/`0x1211F8`.** `0x1211F8` approaches every triplet, including +0x280,
   +0x28C, +0x298, +0x2BC and +0x2C8.
3. **Motion update `0x13AF28` in the same tick.** The velocity recorded at the attach
   tick is the attach velocity only because the step's blend and gravity terms are small
   there.
4. **Pose.**
5. **Motion post `0x13BFA8`.**

The old browser ran the presentation and `originalRailMotionBegin` after
`originalRailAttach`, which caused three errors:
- The zeroing in `0x13AD20` wiped the `0x13ADC0` heading offset.
- The stance rotation used the unpresented quaternion.
- The `0x134CB0` bake never ran.

Other bugs in the old browser:
- `0x13AF28` clamped the vertical correction to 300·dt.
- The entry pull was missing.
- f20 came from time on rail.
- The detach push had the wrong sign.
- `0x115358` did not rebuild after `0x11DFE0`, and the flip did not run `0x115168`'s
  negations or the mirror flag.
- `0x311B48` rotated the roots by +angle.
- Control 7 requested cycle 18 over the class-10 entry clip.
- `0x13BFA8` ran only on a loss.
- The attach zeroed the +0x2C8 lift triplet.

Oracles:
- `tools/test_rail_motion_native.py motion` (`tests/rail_motion_reference.cpp`) runs
  60,000 cases of `0x13AF28` against `originalRailMotionStep`, byte for byte over the
  rider and motion state and the query sequence. Recompiled originals: `0x1086B8`,
  `0x13BD80`, `0x121AA0`, `0x11E098`, `0x11FEE8` and the math routines. The cases cover
  riding, lost and detach, the re-probe, and the entry, near-vertical, bit-2 and
  inverted branches.
- `tools/test_rail_motion_native.py entry` (`tests/rail_entry_reference.cpp`) runs
  60,000 cases of `0x106848` with the immediate `0x13AD20` enter and the `0x131D08`
  control-7 enter, the recompiled `0x115358`/`0x115168`/`0x11DFE0`/`0x311B48`/
  `0x1326C8`/`0x13ADC0`, and scripted `0x11FA10` and old-controller exit bakes. It
  checks the attach result, the query and call order (including `0x311BF0` root
  quaternions) and the rider, motion, control-7, animator and global bytes. It covers
  controls 0/1/2/3/4/5/11/12, motion 0/1, all styles, flips, restyles, and +0x330
  board-press entries.

The native side is `originalRailAttach` with `requestMotion(4)` performing the
presentation plus `originalRailMotionBegin`, as `web/rail_gameplay.inc` `step_rails` and
`engine/riding.hpp` `tryRailAttach` now do.

Browser, `web/rail_gameplay.inc`:
- `step_rails` overrides `requestMotion`/`requestControl` for the attach. The overrides
  run `0x13F410` for a ground attach, `0x11FA10`, `originalRailMotionBegin`, and
  `landing_air_exit` for control 5.
- The rail view carries +0x3B0 and +0x280.
- `rotateAnimation` uses −angle.
- `rail_motion_post()`, called from `animation_tick` after the body volume is rebuilt,
  runs `0x13BFA8` every non-lost rail tick and translates the cached pose by the
  contact displacement.
- The rail tick approaches +0x280/+0x28C/+0x298/+0x2BC/+0x2C8.
- Control 7's `0x115B58`/`0x115D48` point gates the idle upper reactions.

Captures:

| Capture | Before | After |
|---|---|---|
| `rail-air-fence` | exact through 672; attach placement 11.7 cm off | exact through **784**: attach, 33-tick grind, rail loss, control 7 → 4 → 5, 78 air ticks |
| `handplant-rail` | exact through 637 | exact for **all 499 ticks** (through 837): control-11 attach, style 4 sideways |

`rail-air-fence` tick 785 is a web-only instance contact: 105398 path 3 against
instance 688904 in the air after the rail. It is not a rail issue.

Synthetic rail fixtures were extended because the rider now stays on RAIL_3007 for about
650–700 ticks (the entry pull keeps the board on the rail). The affected fixtures are
`web/test-rail-gameplay.mjs`, `web/test-ps2-rail-boost.mjs` and
`tools/test_rail_gameplay.py`. `test-rail-gameplay.mjs` also counts only the release
tick's exit pass, because the post contact passes now run every tick. The hand-written
detach checks in `engine/rail_motion_tests.cpp` use the corrected push direction.

Still open:
- The lost path (`114298` takeoff and `requestMotion(1)`) still runs in `step_rails`
  before the pose, with the previous tick's body volume. The original runs it in the
  post.
- A jump release from control 2 still calls `originalRailMotionLeave` (the original's
  motion-1 post is `0x139C88`).
- The +0x360 board-press latch that `0x131D08` clears is not cleared by the browser.

### 3.9 Rail balance direction (2026-09-22, verified by captures)

A user reported that the rail balance controls felt inverted. Two ARMSX2 captures now
record the left stick on a rail: `rail-balance-lr` (a ground attach, style 1 50-50,
lx −0.6 then +0.6) and `rail-balance-slide` (a handplant exit to a style-4 boardslide,
lx −0.6 then +0.6). Both have `--snap` PS2 screenshots.

**What the original does.** INPUT.MAP defines RailBalance as `LStickR − LStickL`, so a
stick pushed left gives a negative value. With lx −0.6, control 7's word0 bits 25..30
are 54 (−10/31). Every controller passes RailBalance to `0x113F38` **unnegated**, and
only the scale step `mul.s` sits between the decode and the call:

| Controller | Call site | Field |
|---|---|---|
| Control 7 | `0x131E00` | word0 bits 25..30 |
| Control 12 (Uber) | `0x1365DC` | word0 bits 23..28 |
| Soft control 3 on a rail | `0x12E850` | word0 bits 21..26. This is RailBalance, not CruiseTurn (which includes the D-pad). |
| Control 2 on a rail (style ≠ 0) | `0x12EC1C` | clamp(word1 bits 6..11, ±0.5) |

On a rail, the control-2 prewind spin/flip targets still come from PrewindSpin and
PrewindFlip (the D-pad, word0 bits 15..20 and 21..26).

Step 7 then slides by `right · steer · 180`. So stick left moves the rider toward
−right, which is screen left because the chase camera sits behind the rider. The rail
then lies to the rider's right, so balance `clamp(dot(right, hit − bone)/30)` rises and
the lean and balance clips follow it. The captured `+0x22C` target was −0.323 while the
stick was left, and `+0x238` went from −0.2 to +0.4. The PS2 HUD draws no rail balance
meter. **In the original, left goes left.**

**Browser bug (fixed).** `web/rail_gameplay.inc` passed `-steering` in all four places.
The negation was left over from the old host steering convention; the neutral-stick
rail captures could not detect it. With the stick held, the web rider slid the
opposite way, toward +right: 0.44 cm off on the second tick and 10 cm after eight
ticks. The rail-held-jump path also used the negated stick as the prewind **spin**
input, where the original uses PrewindSpin. After the fix:

- Both captures are bit-exact for every tick (800 and 600).
- `rail-balance-slide` body bones are exact through the whole grind (806 is the
  exit).
- Web frames match the PS2 screenshots.
- `test-ps2-rail-boost.mjs` had asserted the inverted sign. It now asserts "left stick
  raises balance".

Input devices: a gamepad's `axes[0] < 0`, and the keyboard's A/Left in both Simple and
Classic modes, produce PS2 byte < 79. That is channel 20 (LStickL), the same as a
DualShock pushed left (`web/pad-input.js`, `test-pad-input.mjs`).

### 3.10 Metro-City rail and air-release fixes (2026-09-22, verified by captures and an instruction oracle)

The Metro-City captures `metro-jump-tricks` (1493, "rail grind 0.8 cm"), `metro-air-tricks` (888, "0.3 cm")
and `metro-event-race` (2049) left the PS2 on rails. The rail motion itself was exact every time. The inputs
around it were wrong:

- **Completion table `0x456990` was off by one for kinds 6 and 7.** The ELF words are
  `0x103978, 0x103990, ..., 0x103A68`. Kind 6 (the air rail entries 68/69/70 and rail spins 49..54) goes to
  `0x104C38`, not `0x104B98`. `0x104C38` plays 20 for 69, 19 for 70 and 18 otherwise through `0x312BD0`:
  `0x144670` moves bit 63 from +0xB0 to the raised word +0xB8, then `0x3128E8` plays with the state blend, so the
  finished entry clip stays and fades out. Kind 7 (semantic 4) is `0x104B98` -> `0x312B18(...,3)`, a
  replacement. The browser had replaced the finished entry with semantic 3 (clip 0x1E00) for one tick, and
  control 7 played 18 over that clip one tick later. The blend weights matched, but the fading clip was wrong,
  so the board bone was 0.7 cm off and the next rail query moved the rider (jump-tricks 1491, air-tricks 887).
  `engine/animation_completion.hpp` now holds the whole dispatch: remove (0), keep (kind 2 outside table
  `0x4569E0`), replace via `0x312B18` (1, 2, 3, 7, 8) and play via `0x312BD0` (4, 5, 6, 9, 10).
  `engine/rider_animation_player.mm` (and the generated browser graph) uses it. Oracle:
  `tools/test_animation_completion_native.py` (`tests/animation_completion_reference.cpp`) runs the
  recompiled `0x103918` with all eleven handlers, `0x312B18`, `0x312BD0` and `0x144670` against it: 60,000
  cases match. It checks the callee order and arguments, the weight copy, the flag words and the value stored
  in the animator's requested slot (`0x312B18` returns its semantic, `0x312BD0` the play result).
  `originalRailCompletionReplacement` takes the semantic now.
- **A jump on a rail runs control 7's exit.** `0x1162C8` requests control 2 through `0x11FEC8`, which runs
  `0x132048`: +0x238 target 0, rate 1/15. The browser's `Stop::Upper` branch skipped it. Balance then kept its
  target on the jump tick, and the fading 18/19/20 three-way cycle (weights by +0x238) posed differently
  (air-tricks 936).
- **The air release resets the default root and clears the style.** On a release, `0x12E9B8` stores
  anim+0x30 = `0x4FF130` and anim+0x40 = sincos(-0) about `0x4FF160` when rider+0x328 is 3 or 4 (`0x12EA4C..
  0x12EAC0`), before `0x12EE30` plays the release clip. It clears +0x328 afterwards (`0x12EAD8`). A held jump that
  left a style-4 rail and was released in the air kept the rail's -45 degree root on the release clip 277
  (air-tricks 981, a pose error) and kept style 4 until the crash entry. `0x10EB30` then rotated the crash root by
  the style, which put the crash 63 cm off (air-tricks 1011). Fix: `release_air_control`.
- **Soft control attaches to rails.** Control 3 `0x12E778` runs `0x116378`, `0x116120` and then `0x106848`, and
  returns if the attach succeeds. The rider stays in control 3 on the rail (no 13/7 requests for control 3, so
  no `0x131D08`), until the soft clip ends and 12E778 requests control 7. The browser refused attaches while
  soft (event-race 2049). `step_rails_tick` now tests the attach in control 3. It skips `originalRailControlBegin`
  for control 3 and does not run the soft rail step on the attach tick.

Captures after these fixes: `metro-jump-tricks` exact through 1623 (end; was 1492), `metro-air-tricks`
1821 (end; was 887), `metro-event-race` 2318 (end; was 1093, with the tuck/boost selection fix in
docs/obstacle-collision.md). All Snow Jam rail captures are unchanged and exact.

## 4. Constants (bit patterns)

555.5555 `0x440AE38E`; 0.1 `0x3DCCCCCD`; ±π/2 `0x3FC90FDB`/`0xBFC90FDB`,
1.5707965 `0x3FC90FDC`; π `0x40490FDB`; 150° `0x40278D37`; ±30°
`0x3F060A93`/`0xBF060A93`; 45° `0x3F490FDC` (heading clamp), `0x3F490FDB`
(roots); 0.0009 `0x3A6BEDFB`; 0.6 `0x3F19999A`; 0.001 `0x3A83126F`; −0.2
`0xBE4CCCCD`; 0.92 `0x3F6B851F`; 0.3 `0x3E99999A`; 1.8 `0x3FE66666`; 1.650076
`0x3FD335B2`; 1/70 `0x3C6A0EA1`; 40° `0x3F32B8C4`; 30° `0x3F060A93`; 1/30
`0x3D088889`; 20° `0x3EB2B8C4`; 1e10 `0x501502F9`; 277.7778 `0x438AE38E`;
±2450 `0x45192000`/`0xC5192000`; 1/60 `0x3C888889`; 1/15 `0x3D888889`; 1/31
`0x3D042108`; 0.05 `0x3D4CCCCE`; 1.2 `0x3F99999A`; golden 0.381966
`0x3EC3910C`, 0.618034 `0x3F1E377A`, 0.0005 `0x3A03126F`; gravity −980
`0xC4750000` at `0x4A5E50`; Z axis constant `0x4FF160`, zero `0x4FF120`, root
translation `0x4FF130` (initialized by `0x320550`).

## 5. Verified vs inferred

Verified from instructions: record layout and chaining, `0x335128`/`0x334680`,
`0x108A48`/`0x1086B8`, `0x106848` style table and requests, `0x115358`,
`0x13AD20`/`0x13ADC0`/`0x13BD80`/`0x13AF28`, `0x131D08`/`0x131D30`/`0x132048`,
`0x132060`, `0x132770`, `0x1326C8`, `0x113F38`, `0x1161D0` gates, control and
motion dispatch tables, constants above. Runtime numbers were not compared
against emulator captures; native fidelity is transcription-level plus the
brute-force query cross-check.

Inferred: runtime setting of header+0x1C bit 0 (the walker requires it and the
file has 0); world list order = record order × segment order (only affects
equal-distance ties); +0x50 row purpose; `0x149208` = "balance" stat (bytes
+6/+14; string `RailBalance` exists but the link is unproven); `0x3128E8`
fourth argument 0 in `0x1326C8`.

Gaps (throw): identity/uber (`0x132620`, control 12, `0x136508`), transfers
(`0x1161D0` positive path, semantics 24/26/32/34, control 1 `0x12FC80`), rail
attach with +0x330 set, control-12 exit event table, the `0x13BFA8` effect
block (`0x114298`, `0x105398`, `0x13C140`, `0x107888`, `0x294170`), dynamic
instance rails (types 2/3 layers, virtual +0x150..+0x174), control 11
(`0x1328B0`/`0x132A30`), reset-onto-rail `0x13AA48`, `0x11FA10`.

## 6. Integration plan for `riding.hpp`

1. Load `rails.json` into `std::vector<OriginalRailRecord>` (source cm rows,
   `flags = 1`, `surface = −1`, `packedId`) next to the collision world;
   `access.query = [&](p){return originalRailWorldQuery(records,p);}`.
2. Build an `OriginalRailRider` view from `OriginalGroundState` each tick
   (position/velocity/quaternion/basis/normal/surfaceVelocity, `boost`,
   `prewindStyle`→`style`, `reverseStance`, `state320Equals324`→`state324`,
   speed limit, timeScale, control/motion) plus the posed board-root bone
   from the animation player (`BodyCollisionVolume` pose boundary) and
   +0x9D0. Add the new triplets (steer +0x22C, balance +0x238, tolerance
   +0x25C) to the shared `0x1211F8` approach pass.
3. Controls phase: `controllerCallbacks.railAction` (passive 4) and the
   grounded/air controllers call `originalRailAttach`; on success apply the
   returned requests through the existing control/motion transition code:
   motion 4 → `originalRailMotionBegin`; control 13 then 7 →
   `originalRailControlBegin`; play the entry semantic through
   `requestControllerAnimation`. Soft control 3: `requestBalance` →
   `originalRailSteerTarget`, `resetRail` → play `originalRailEntrySemantic`
   and return to control 7.
4. Motion phase while motion = 4: `originalRailMotionStep`; `RailLost` marks
   `lostRail`; on the next motion change call `originalRailMotionLeave`
   (request motion 1 = airborne, i.e. existing `airEntry` seeding with the
   current velocity). `Detached` keeps motion 4 until the next query misses.
5. Control 7 tick: decode with `originalRailDecodeCommand` (or map native
   input: turn → `turn`, stick left/right tap → `rotate`, up/down →
   `transfer`), `originalRailControlStep` with the callbacks wired to the
   existing recovery/upper/boost/animation helpers; `Airborne` stop → enter
   control 4/5 exactly as the passive-air exit does today.
6. Scoring hooks (`railEntryScore`, `awardScore`, `airborneRailEvent`,
   `railSpinScore`, `recordRailSurface`) are observer callbacks; record them
   in `GameplayAnimation` requests, do not implement scoring.

## 7. Build

```
add_executable(ssx3_rail_motion_tests rail_motion_tests.cpp)
target_compile_options(ssx3_rail_motion_tests PRIVATE -UNDEBUG -frounding-math -ffp-contract=off)
target_link_libraries(ssx3_rail_motion_tests PRIVATE ssx3_ground_motion ssx3_orientation_motion ssx3_air_control)
add_test(NAME native_original_rail COMMAND ssx3_rail_motion_tests)
```

Ad hoc: `clang++ -std=c++20 -frounding-math -ffp-contract=off -UNDEBUG -Iengine
engine/rail_motion_tests.cpp engine/orientation_motion.cpp engine/air_alignment.cpp
engine/ground_motion.cpp`. Data: `python3 tools/import_rails.py` (all five race
areas; `--locations ARA1` for one).


## September12 correction:1162C8 is the crouch request

The older “upper” description above is a legacy naming error. Direct1162C8 code checks rider+360 held/pressed gate and calls11FEC8(control2) at116344. Command bit14 is held and bit13 pressed at the rail call site. OriginalCrouchRequest already represents the gate. The browser adapter now uses that route for charged rail jumps; existing native gameplay's attack-labeled callback is not a faithful implementation of that request. See docs/browser-rails.md.

## Browser spline coverage audit

`tools/audit_rail_coverage.py` runs the actual WASM catalog query and a native
ideal-approach attachment audit using the browser rail loader. Source SHA256
is retained in both reports under local/browser-validation.

All16,317 samples (21 per segment,777 segments,171 records) find a rail within
1.231cm; zero misses or distances above2cm.53 samples select a neighboring
record. This establishes catalog query coverage, not gameplay latchability.

The attach audit samples19 interior points per segment, both directions,
placing the board10cm before the sample at900cm/s with neutral grab class,
air motion and identity bone rotation.29,273/29,526 approaches are accepted.
All253 rejections across44 rails hit the not-behind gate; selected points lie
0.2..1.019cm behind the board. These are ideal diagnostic poses, not gameplay
spawn fixtures. The test does not assert that all approaches should latch.

Original108B1C reads gp-7E34 (49B2BC), whose pinned ELF word isBE4CCCCD
(-0.200000003). Thus enlarging the behind tolerance would depart from source.
Next verify the335128 chosen-point calculation itself against original
instructions for these rejected cases. Existing rail-queries.bin tests only
native/WASM agreement. Dynamic-object rails and full gameplay attachment
coverage remain open. No latch rules were altered in this audit.

The rail-queries.bin oracle was regenerated on 2026-09-28 (`tools/test_browser_rails.sh`, which now links
`tests/rail_query_browser_stubs.cpp` for the symbols rail_bridge.cpp takes from the rest of the core).
- ARA1/rails.json now holds the whole event residency (`tools/import_rails.py`, re-exported 2026-09-22): A_ARA1 7 + ARA1 171 +
  ARA1_B 4 = 182 rails and 822 segments, where the oracle had 171 / 777. It also carries the runtime flags of snow-jam-glide.p2s.
- 11 ARA1 rails have runtime_flags bit 0 clear (no query mask 1), so their own samples need not hit.
- `web/test-rails.mjs` reads the counts from the file: 4110 queries, native = WASM.

### Original335128 segment-search conformance

`tools/test_rail_segment_native.py` now executes335128 from the pinned USA
ELF for29,526 bidirectional approaches over all777 authored ARA1 segments.
Found flag, distance, chosen point, tangent, parameter and surface all match
the native implementation bit for bit. These are independent segment queries;
world-layer iteration and accumulated-result replacement remain separate scope.
The small behind-board offsets found by the coverage audit are therefore not
evidence of a port error in the segment search. Do not alter its original
golden-section result or loosen the original attachment threshold.

A separate host-binding error was corrected in web/rail_gameplay.inc:
rail entry called originalRiderRootPresentation with unit scale. It now uses
graph.scale, matching the original pose scale and the other live presentation
paths. The discrepancy affects the lean pivot and45cm turn shift while entering
a grind; it does not add dynamic-object rail support. The presentation helper
itself is covered by20,000 original root-presentation cases with varied scale
in tools/test_rider_pose_native.py.

Validation after scale correction: original pose oracle passes (including
20,000 varied-scale root presentations), full npm suite and production build
pass, and rebuilt native/WASM Sam and Zoe traces each pass9,630 frames with
zero mismatches. Logs are local/browser-validation/rail-scale-*.log; direct
original segment results are in rail-segment-oracle.log. These checks do not
prove full original gameplay attachment scheduling or dynamic rail coverage.

### Object-rail paths and saved-state census

The earlier shorthand “sphere-tree instance rails” obscures the actual query
paths.334680 dispatches nearby layer+8 type2 to35C698 with layer+0C, and type3
to348290. Type2 resolves object+30's packed rail descriptor through2D1BD8;
checks descriptor+1C against mask;35C5A0 composes the instance/binding transform
(34FED8 using object+40/+34) with object+50. It transforms the descriptor's
linked cubic segments before searching, and returns the instance and binding.

Type3 checks object+24 flags, loops object+20 minus one segments, and samples
64-byte cubic coefficient matrices at object+50. Its result includes object+18
instance, segment index, and the virtual+188/+18C binding getter; surface is
resolved through that binding. These paths need ownership and motion data,
not merely an extra static spline copied into the catalog. No type2/type3
implementation has been substituted into production yet.

`tools/audit_original_rail_layers.py` reads33 saved snow-jam human-rider nearby
caches (rider+860, count+210, entries+214; validates the human allocation's
4583A8 vtable). None contain type2 or type3. It writes snapshot hashes, source
positions, layer counts and any object records to
local/browser-validation/original-rail-layers.json. This is limited to saved
paths, with some isolated probe states; it is not a full-course/event census.
Missing object-rail support remains real but cannot yet be blamed for latch
failures along these captured paths. Next prioritize original host attachment
call timing and animation-class gates on a reproducible gameplay approach.

### Attachment gates and fading animation identity

`tools/test_rail_attach_native.py` executes108A48 and1086B8 for20,000 varied
inputs: motion0..3, animation classes17..21, all combinations of flags0..2,
board orientations, positions, offsets, velocities and tolerance values.
Acceptance and the entire query-position sequence match;170 accept and3,664
issue the secondary board proximity query. World-query results and animation
getters are controlled boundaries, not original live gameplay scheduling.

The browser graph's fade binding was inconsistent with311E88 and the native
player: it faded weights but retained the requested semantic and completion.
It now clears the requested slot to438/class0 and disables pending/future
completion callbacks for the fading sequences. Ordinary flags0..2 and visible
pose weights are retained during the fade. Original311E88 calls314718 to
remove marker63,3146D0 to fade the channel list, then writes438 at311EE0.
Our graph models completion callback cancellation with completionEnabled=false
and clearing its dispatch flag63. This is separate from ordinary grab markers.

`tools/test_browser_channel_fade.py` checks fading multiple sequences, neutral
requested class with retained playback flags, unaffected other channels, and
fade duration shortening. This fixes stale class/completion state; it is not
yet a reproduced explanation of the user's missing rail latches.

Validation after channel-fade correction: full npm suite, production build,
original jump-input/head comparison, and rebuilt Sam/Zoe native/WASM traces
pass. Each rider trace covers23 scenarios/9,630 frames with zero mismatches.
Logs: local/browser-validation/channel-fade-*.log; attach oracle separately
in rail-attach-oracle.log. Full original live scheduling remains unverified.
