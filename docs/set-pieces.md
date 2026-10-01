# Snow Jam race-event set pieces (2026-09-22)

Moving scenery in the Snow Jam race event is driven by the original entity/modifier system: a
stage (LUN) program attaches a *modifier* to an instance's entity, and every game tick, before
the rider's provider and controllers, the modifier's update (vtable+0x14) and the entity update
(vtable+0x194) run. This document covers what is live in the race event, what is ported, how it is
verified, and what is missing. Crashbags (RollerModifier) are in
[obstacle-collision.md](obstacle-collision.md#crashbags-scripted-instances-and-the-rollermodifier-2026-09-22).

## Entity and modifier system (reference)

* instance +0x8 runtime flags (0x20 static route, 0x40 entity route, 0x100/0x200 renderer
  bookkeeping), +0xC entity, +0x10 matrix, +0x60/+0x6C bounds, +0x78 resource id.
* entity +0xC vtable, +0x10 node type, +0x18 instance, +0x1C modifier container (container+0 =
  modifier). Classes: 0x490E80 Object (type 17), 0x490B10 LiveComp (type 1), 0x48FC10 type 10
  (`Flag Verts` cloth flags), 0x48EE60 type 13 (emitters), 0x491800 type-16 node, 0x491B00 DeadNode.
* Modifier classes (ELF names at 0x48E8D0..): Spline 0x48F250 (ctor 0x359460), MultiSpline
  0x48F168 (0x359F88), AvaSpline 0x48F338, Magnet 0x48F420, Parent 0x48F508, Position 0x48F5F0,
  Roller 0x48F080, Rail 0x4911D0 (0x35B708), Halo 0x491220, Particle 0x4912B0, DynamicParticle
  0x491268, Boost 0x490898, UVScroll (0x35F6E8), TexFlip (0x35F0B8). Builtins (table 0x441F38):
  19 Spline, 20 MultiSpline, 21 UVScroll, 22 TexFlip, 23 Position, 26 DynamicParticle, 48 Rail,
  90 Magnet, 92 Halo, 95 AvaSpline.
* Stage handler rows (bam.ssb chunk 33 kind 16, six words per row at +0x70, row = collision
  descriptor resource08 >> 8): slot 1 runs when the instance's course section is activated, slot 2 on
  a selected rider contact (121818 -> 30A060).
* The paths are the kind-8 spline records (the grind-rail records):
  `engine/original_spline_path.hpp` ports 0x3451C0 (bind), 0x3454E8 (length), 0x345248 (wrap and
  segment cursor) and 0x345048 (arc length -> parameter cubic +0x50, VU0 row products).
  `rails.json` now carries +0x50 (`row50`) into `OriginalRailSegment::arcToParameter`.

## Chairlift (tramlores, MultiSplineModifier) - ported

**What drives it.** Handler rows 134/135 (mdl_ARA1_tramlores_0 / _1) run programs 69/71 in slot 1
at course load: `MultiSpline(self, spline 0x608 / 0x708 = GondolaRail_0/1, count 3, 35 / -35 km/h,
rotation 90 deg)` (builtin20 0x2FE0C0 -> factory 0x355B30 -> ctor 0x359F88). The ctor clones the
authored instance three times (0x35A3F0/0x351170; 0x35A458 copies the authored 0xA0 bytes into
clone 0 and registers clones 1.. with 0x351270, flags &~0x40|0x20): clone resources
0x3F80/0x3E80/0x3D80 and 0x3C80/0x3B80/0x3A80. Six chairs, three per cable, evenly spaced on the
31,644 cm closed paths, moving at 972.22 cm/s.

**Per tick** (before the rider; the capture records at the provider exit already hold this tick's
distance with +0x30 = 0):

| Original | Port (`engine/multi_spline_modifier.hpp`) |
| --- | --- |
| 0x35A560 modifier update: distance += speed x clock dt (+0x14 of gp-0x848), wrap by length, dirty | `originalMultiSplineUpdate` |
| LiveComp 0x3568B0 -> 0x356078 -> modifier+0x94 0x361C20 -> 0x35A5D8 -> 0x35AC20 | `originalMultiSplineEvaluate` |
| 0x35AC20: car k at distance + k L/3, 0x345248, yaw/pitch from the derivative (0x31C228, 0x31BE50), Euler matrix into clone +0x10, then R(axis 0x4FF160 = z, +0xC) x M (VU0) | same |
| 0x3568B0 car-0 record bounds (radius +0x18 via 0x361C10, 0x361BF0), 0x3291E0 relocation; modifier+0xC4 0x35A918: records and clone bounds of cars 1.. | `originalMultiSplineBounds` |
| selected contact: LiveComp 0x34E698 -> 0x356AE0 -> 0x353098 -> modifier+0xB4 0x35B200 (surface velocity of the car at the contact point into packet +0x20, angular velocity into +0x30) | `originalMultiSplineContactVelocity` |

**Collision.** The authored instance keeps the entity route (countdown flags 0x210345): entity
bounds = car-0 record bounds, hierarchy root = clone 0 matrix. Clones are static-route instances
(0x210325 / 0x212325) on their own +0x10 matrix; clone 0 keeps the stale authored bounds
(0x35A918 relocates only cars 1..). All of them carry the LiveComp entity, so 1057B8's rigid
predicate (entity+0x74 0x355420 -> modifier+0x44 0x360B60) returns 1 and 104E70 runs the
selected-entity callback. `WorldCollisionInstance::entityPointer` marks static-route instances
with an entity; `runtimeClone` keeps the clones out of the package counts.

**Browser.** `tools/export_set_pieces.py` verifies rows/programs/vtables and seeds the modifiers,
car records and clone instances from `snow-jam-ready.p2s` (race tick 0; the load happens long
before the countdown, so the load-time state cannot be recomputed) into
`web/generated/set_piece_seed.hpp` and `local/event-activation/set-pieces.json`.
`web/set_piece_gameplay.inc` (included by `roller_gameplay.inc`) creates the clone collision
instances, runs update + entity update in `race_begin` before the rollers, recomposes the
geometry, answers the entity callbacks (rigid; velocity added to `surfaceVelocity`) and appends the
draw deltas: `moving_instances()` key = resource + car x 2^20; `web/moving-instances.js` moves the
authored tramlores batches with car 0 and clones them for cars 1 and 2. `web/prepare.py` marks the
tramlores batches as moving. Diagnostics: `set_piece_info()`, `set_piece_bits()`.

**Verification.**
* `python3 tools/test_multi_spline_live.py [ticks] [cases]` (`tests/multi_spline_live.cpp`): the full
  original 0x35A560 + LiveComp 0x3568B0 from snow-jam-ready vs the port, both lifts, 8,200 ticks:
  every modeled modifier word, all car records and clone matrices/bounds byte-identical; the same
  state equals the `setpieces/race` capture watch windows on all 8,182 records (record T = port tick
  T); randomized 20,000 x 0x345248 (negative/over-length distances, random cursors), 20,000 x
  0x35AC20 (random distance, angle, modes 0..3) and 20,000 x 0x35B200 identical. The browser
  `rails.json` matches the 95 runtime gondola segments bit for bit.
* Production browser path: `node compare-ps2-capture.mjs ../local/ps2-capture/runs/setpieces/race.bin
  --pad --zoe --event --sync-rng` reports `chairliftTicksExact` 8199/8199 (distance and all six
  car matrices, bit-exact).

**Gaps.** The LiveComp part of 0x34E698 (per-node velocity table entity+0x4C via 0x34E798/0x34E600)
and packet +0x30 (angular velocity) are not modeled; no capture has the rider touching a chair,
so the collision routing is verified by code reading only. 0x3291E0 spatial relocation is not
needed by the flat instance list. The modifier draw path (+0x5C 0x35B418 frustum test per car)
is replaced by three.js culling.

## Waving flags and streamers (flag entity 0x48FC10) - ported

Programs 61-64 (slot 1, section load) call builtin12 (0x2FC9C8 -> 0x34AC88) for the 41 race flags
(flg_ARA1_flaga/flagb 0/sb/eb, streamera/b). The flag entity (ctor 0x34ADD8) hides the static draw
and registers with a 15-slot flag manager (0x34C548); instances with the same model and parameters
share one 8 x 5 sine-wave grid (0x34BCA0, not a cloth simulation), built from the first registered
instance (0x34B228). Every game tick 0x34C668 moves a random wind value toward a new target once per
second (+-0.15 on Snow Jam) and 0x34B818 advances each slot's phases; a grid is recomputed only on
ticks whose parity matches its slot; every instance draws the shared grid with its own matrix
(0x34B9B0). Flags use amplitudes 70/0/30/30 fixed at the pole, streamers 80/0/10/0 hanging.

Port: `engine/flag_cloth.hpp` (oracle `tools/test_flag_cloth_native.py`, `tests/flag_cloth_reference.cpp`,
randomized vs 0x34BCA0/0x34B818/0x34C668/0x34B228/0x392DF0 sine table), `tools/export_flags.py` ->
`assets/FLAGS/flags.json`, browser `web/flag-animation.js` (bit-exact EE float emulation in
`web/ee-scalar-float.js`, `test-flag-animation.mjs`). All 20 live flag slots in the race snapshots
match parameters, grid sizes and corners; the vertex buffers match exactly (11 from the current tick,
9 from the previous one, the parity update). `web/prepare.py` hides the flag instances' static batches;
`web/set-pieces-renderer.js` redraws each flag with its hidden batch's world material.

Gaps: the wind and the random start phases draw from the shared visual random stream (0x4FF018),
which the browser does not share with the snow/camera draws (Math.random stand-in); all 41 flags
are activated at race start instead of on section streaming (8 cloth setups, well under 15 slots).

## Texture scrolling (UVScrollModifier, builtin21) - ported

324 instances scroll their texture every tick (ctor 0x35F6E8, tick 0x35F7D0 from the modifier
container 0x352C70, draw 0x35FC20 adds (u, v) to every texture coordinate): 245 chevron course fences
(texture 9-30, -0.025 u per tick), speed boosts, blue pinlights, podium/stage backdrops, e70slight,
snowsheet/snowstream, rivers and waterfalls. Port: `engine/uv_scroll.hpp` (125,000 randomized oracle
cases, `tools/test_uv_scroll_native.py`), `tools/export_uv_scroll.py` -> `assets/UVSCROLL/uv-scroll.json`,
`web/uv-scroll.js` (`test-uv-scroll.mjs`: 625 consecutive PS2 snapshot pairs replayed bit-exactly,
250,039 ticks). `web/prepare.py` batches scrolling instances per scroll group (9 distinct initial
states, `uv_scroll_group`), `web/world-material.js` adds the group's uniform offset to the instance
UV (`worldUvScroll`), and `web/set-pieces-renderer.js` ticks the groups from the core tick counter.
Gap: the original starts each instance when its section loads, so distant sections can be out of
phase; the browser starts all at race tick 0. TexFlip (builtin22) is not used on ARA1.

## LiveComp animation players (builtin3) - ported

The LiveComp entity (vtable 0x490B10) is an animation-time player: builtin3 (0x2FBCB8 -> ctor
0x341AA0; keys 1 mode once/loop/ping-pong, 2 reverse, 3/4 time range in 1/30 s, 5 rate, 6 random
rate spread, 7 start time, 8 random start from the gameplay RNG 0x4FF030, 9 hide static draw). The
tick 0x341D48 advances 1/60 s per game tick in the entity pass, then runs the owner's handler slot 5
(0x34EBA0 -> 0x30A688; builtin55 0x3019C8 is true when the time crossed key1/30 s this tick,
0x34EBE0); a finished once-mode player runs slot 4 (0x34FCC0 -> 0x30A598). Node matrices
(0x361098 -> 0x34DC90) sample cubic keyframe segments per masked channel (translation, Euler degrees)
and compose the hierarchy (the channel/compose code shared with `engine/rail_modifier.hpp`).

| Set piece | Start | Timing (browser tick = capture record tick) |
| --- | --- | --- |
| start-gate doors startgatedoorbig_1000..1011 | stage global handler 2 (program 3) at race GO | built in tick 181 (race phase starts at 180) before the entity pass, rotate 0 -> 90 deg about local Z over 1/6 s, end at 191, done at 192 |
| searchlights sb/eb | slot 1 (section) | 4 s loop, Y rotation 1 -> 76 -> 0 deg |
| blue pinlights | slot 1 | 2 s loop, random start |
| speed/trick boost pickups | slot 1 | spin loop |
| ravens taking off ravenanima_1000..1002 | raventriggera_1000 contact (program 56) | 5-node birds, 4.23 s; slot-5 program 57 starts _1001 at 5/30 s (+11 ticks) and _1002 at 7/30 s (+15) |
| rock slide rock_roll_fall_1000..1002 | rockslidetriggera_1000 contact (program 59) | 1.4 s each; slot-5 program 60 starts _1001 at +13 and _1002 at +21 ticks |
| snow crumbs snowcrumb_1000..1002 | crumbTrig_1000 contact (program 105 -> crumbTimer 2 s) | slot-5 program 104 starts _1001 +3, _1000 +17, _1002 +33 ticks |

Port: `engine/livecomp_animation.hpp` (oracle `tools/test_livecomp_animation_native.py`,
`tests/livecomp_animation_reference.cpp`: 10,000 constructor cases, 40,000 tick + node-matrix cases),
`tools/export_livecomp.py` -> `assets/LIVECOMP/livecomp.json`, browser `web/livecomp-animation.js`
(`test-livecomp-animation.mjs`). Against PS2 memory (132 savestates of the race, full-course and GO
captures): 634 snapshots bit-exact node matrices, 892 consecutive snapshot pairs replayed tick by tick
(52,256 ticks), 516 door states from the GO captures, and the raven chain from its contact (tick 3404
in the full-course run) reproduced exactly.

Browser: `web/prepare.py` splits LiveComp batches per animated node (`livecomp_resource`,
`livecomp_node`); `web/set-pieces-renderer.js` fires the section starts at race tick 0, GO at tick
181 before the entity pass, runs the entity pass once per game tick, then fires the slot-2 starts
of the trigger volumes contacted in that tick, and draws each node's rest -> animated delta.
Trigger contacts come from the core: the 105398 store step (rider+A30 -> 121818) logs every
selected instance contact (`browser_set_piece_contact`, `set_piece_contacts()`; trigger volumes are
type-2 boxes with node flags 2 / surface -1, the same path as the speed boosts). On the full-course
pad script the browser rider contacts triggerRockets_1100 at 878 (PS2 human entry 880), crumbTrig
at 1298, spintwintrig at 3904 and rockslidetriggera at 4308; `?livecomp=0`, `?flags=0`,
`?uvscroll=0` disable the systems for comparisons, `ssxQA.setPieces()` exposes the state.

Gaps: section starts happen at race tick 0 instead of at section load, so loop phases (searchlights,
pinlights, pickups) differ from the PS2 by the unknown load time; the random starts do not share the
gameplay RNG. Builtins 2, 30, 31, 73, 77 (sounds, hide/dead-node, other side effects) are not
modeled, so the ravens keep their final pose instead of their slot-4 state change. **Computer riders
fire triggers too** (the ravens at 3404 and the rock slide before 4215 in the full-course run were
fired by AI riders, not the human): the browser's computer riders (`web/ai-racers.js`, separate core
instances) are not yet wired into `main.js`; when they are, forward their `set_piece_contacts()` to
`setPieceRenderer.fireContact(resource)`.

### Section LiveComps with timer programs, builtin 28 / 54 (2026-09-28)

- A LiveComp started by a section scan (slot-1 program → builtin 3) is drawn by the JS player. When its instance also has a slot-4
  or slot-5 program, the core now builds the same LiveComp as an entity (`sectionPlayer`, web/stage_world.inc). The PS2 entity
  pass ticks it and runs slot 5 each update tick (0x34EBA0), slot 4 when a once-mode player ends. Only constructs without draws
  qualify (key8 0, key6 0); 14 instances on the mountain have slot 1 plus slot 4/5.
- Builtin 28 (0x2FF9A8, keys instance / property / value, types {1, 1, 2}) calls entity vt+0x120. For a LiveComp that is 0x341FE8:
  0x64 delay = int(value × 60), 0x65 time = value / 30 inside [low, high], 0x66 rate = value / 30 / 60, 0x67 high = clamp(value / 30,
  0, length) (time down to it), 0x68 low = the same clamp (time up to it), 0x69 once mode.
- Builtin 54 (0x301680) returns entity vt+0xF0: a LiveComp's time × 30 (0x361068), 0 for other entities, -1 without one.
- Builtin 91 (0x3050F0) shakes the camera of riders near the instance (15E360): ported 2026-09-28 (see [avalanche.md](avalanche.md) "Related stage builtins").
- Gravitude's crash billboards (332333 / 669741): the trigger program 83 sets high = 149/30 s and once mode, then program 85 breaks
  the ice pieces (builtin 13) with builtin-77 randoms. PS2 ERA5 six-rider capture: exact to the end (docs/ai-racers.md).
- The JS player takes the core's clock (pv `sectionClock`, on; 2026-09-28). `stage_world_section_clocks()` lists each alive
  section-player entity (resource, mode, enabled, done, delay, rate, low, high, time, sample time). After each frame's ticks,
  `syncSectionClocks` (web/livecomp-animation.js; called from set-pieces-renderer.js and peak-set-pieces.js) copies these into the
  JS player of that resource. The player then draws the builtin-28 play-on as the PS2 does.
  - Checked against PS2 run `local/ps2-capture/runs/billboard/grav-bb` (the Gravitude race-ai state, snaps 1169..1389). The page's
    human leaves the PS2 line by about 1100 and hits the trigger later, so the check fed the page the exact per-tick clocks of the node
    run, which is exact to the PS2 (the tool is a copy of the visual-parity vpshot with a clock-table override).
  - At 1192 / 1208 / 1229 / 1249, Chrome and WebKit draw the billboard falling where and when the PS2 does. Switched off, it stands
    and keeps wobbling.
- Visual follow-up, resolved (2026-09-28, visual-parity.md section 40): at 1208 the PS2 draws the falling board dark where the page
  showed the poster. It is lighting, not culling. The billboards are lit instances (flag 0x4000): their colours come from the object
  bank per vertex on the node-rotated normals, and at 1208 that lights the poster near black. The node matrices are bit-exact
  (PS2 states grav-bb-states.tick1209 / 1230). Fixed by pv `litLiveComp`.

## Log teeters (AnimTeeter + RailModifier) - ported

Programs 65-68 (rows 130-133, slot 1) build an AnimTeeter (builtin6, ctor 0x3421A0, vtable 0x4908F8,
a damped spring on the model animation time 0..1: logbreakteeter node 1 rotates about local Y by
0 -> 40.68 deg, logteetera_3000 0 -> 62.15 deg) and RailModifiers (builtin48 0x2FF1C8 -> ctor
0x35B708, vtable 0x4911D0) that bind rails 0x1108/0x1D08/0x6608 and 0x8008/0x8108/0x8208 to node 1.
The update 0x342358 (entity pass, before the rider) integrates accel = -0.5 (time - rest) +
0.001 torque - 0.6 vel with the step clamped to +-1/15 per tick; torque comes from entity
vtable+0x15C 0x342538 (dot(node Y, (hit - node origin) x F), only when |F| > 100), called by the rail
attach 0x106848 (F = v_before - v_after) and the rail snap 0x106F78 (x60). Grinding alone applies no
force, so **the logs move only when a rider lands on their rail**: in every race snapshot and
full-course sample torque, angle and speed are exactly 0.

Port: `engine/rail_modifier.hpp`, `tools/export_rail_teeters.py` -> `web/generated/rail_teeter_seed.hpp`,
oracles `tools/test_rail_modifier_native.py` (160,000 randomized cases) and
`tools/test_rail_modifier_live.py` (constructions of all four logs, 3 x 600-tick lockstep with random
forces, 3,600 rail queries, byte-identical).

Browser (`web/rail_bridge.cpp`, ARA1 only): the four teeters are built from the seed at rail load and
on every new race; their rails leave the static query set and 0x334680 answers them through the
modifier layer (0x35C698 with the node transform) after the static layers, when the modifier bounds
overlap the 300 cm query box; `browser_advance_teeters` runs 0x342358 in the entity pass before the
rider; the rail attach (`originalRailAttach`, new optional `attachForce` hook) applies 0x342538 with
v_before - v_after to the bound log; `set_piece_teeters()` returns each log's node-1 draw delta and
`web/prepare.py` splits the teeter batches per node so `set-pieces-renderer.js` moves node 1.
All PS2 capture gates (including the rail scenarios) stay exact with the logs bound.

Gaps: the original binds on section activation (the browser binds for the whole race), so a log is
answerable by the modifier path even far from its section; no capture has a rider landing on a log;
the rail snap 0x106F78 force is not ported; program 144 (bcvolume_1001 -> falling billboard
fallingbb_1000) never fired in any capture and is not ported.

## Spline set pieces (SplineModifier / PositionModifier) - ported

builtin19 (0x2FDED0, defaults 0x4FB778) -> 0x355AD0 allocates 0xF0 bytes, ctor 0x359460 (binds the
path 0x3451C0, speed = km/h x 27.777779, start at 0 or at L for negative speed, one shared-RNG draw
0x317830 even with zero jitter), attach 0x3554B0 (flags &~0x20|0x40, radius). Per tick 0x359698:
distance += speed/60; end mode 1 loops, 2 ping-pongs, 0/4 stop at 0 / L - 0.1 and set finished.
The matrix (0x359830, lazy via 0x361B90) takes yaw from the normalised first derivative and pitch from
the raw one (0x31C228), an Euler matrix (0x31BE50) with the path point as translation, then the roll
about Z. At the path end the entity update 0x356198 clears finished and runs handler slot 4 (entity
vt+0x114 0x34FD00): builtin16 turns the entity into a type-13 emitter (0x3578A8 -> 0x355F10), frees
the spline and attaches a PositionModifier (0x356F10) holding the L - 0.1 matrix.

| Piece | Program / start | Spline | Mode, speed | Drawn |
| --- | --- | --- | --- | --- |
| raven flyby ravensplineanima_1000 | 54, slot 1 (section load) | 0x5408 ravensplinea_1000, L 17,614 | loop, 45 km/h, roll -90 | yes |
| brocket_1000/1001 | 120, triggerRockets_1100 contact | 0xA908 / 0xAA08 | stop, 150 km/h | no (smoke trail carrier) |
| spintwin_1000/1001 | 131, spintwintrig_1000 | 0xA208 / 0xA108 | stop, 160 km/h | no |
| chasingdragon_1000/1001 | 136, dragontrig_1000 | 0xA608 / 0xA508 | stop, 90 km/h | no |
| chasingdragon_1100/1101 | 137, dragontrig_1100 | 0xA408 / 0xA308 | stop, 120 km/h | no |
| EZrocketCore_1000/1001 | 142 (EZseqTimer slot 5) | 0xA708 / 0xA808 | stop, 150 km/h | EZ_1001 only; not wired |

Only the raven is drawn: the rockets, spintwins and dragons are carriers of their DynamicParticle
trails (the smoke, sparks and dragon fire seen in the PS2 frames), and all of these instances have
collision type 0. Slot-2 triggers are guarded by builtin52 (launched once) and the trigger's flags go
0x200022 -> 0x200322 (Debounce) -> 0x200104 (RestoreNode) -> 0x200022. The selected-contact path
(rider+A30 -> 121818 -> 30A060) is the same for computer riders.

Port: `engine/spline_modifier.hpp` (instruction oracle `tools/test_spline_modifier_native.py`,
145,000 cases; live `tools/test_spline_modifier_live.py`: the raven from race.tick4319 for 1,300
ticks and against three later PS2 savestates at +400/+799/+1199 ticks, the real programs 120/131/54/
136/137 through the original dispatchers, 9 constructions and every spline -> Position handover
byte-identical), `tools/export_spline_setpieces.py` -> `local/event-activation/spline-setpieces.json`,
browser seeds `browserSplinePieceSeeds` (tools/export_set_pieces.py). `web/set_piece_gameplay.inc`
launches slot-2 pieces from the selected-contact log in the rider phase (first update next tick),
the raven when the human's course remaining drops to 243,000 cm (the PS2 section activation was at
242,764 / 243,244 cm in the two captures), draws one value from the shared RNG per piece
(`browser_shared_random_next`), updates them in the entity pass, freezes stop-mode pieces into
PositionModifiers, and draws the raven through `moving_instances()`. On the full-course pad script
the browser's frozen rocket and spintwin positions equal the PS2 PositionModifier translations to the
printed precision; `SET_PIECE_CONTACTS=1 node compare-ps2-capture.mjs ...` prints the contacts and
pieces.

Section leave of a moving piece (2026-09-25, R&B raven B; slopestyle-bigair.md "Gaps"): the leave
destroys the entity (0x34FD90) and the entity dtor 0x3553C0 -> 0x3567E0 moves the instance back into
the octree cell of its own bounds, so a piece whose authored cell is still in the box re-enters at the
next scan and its slot-1 program builds it again (one builtin19 draw). `section_stop_piece` restores
that cell for every destroyed spline piece and drops its LiveComp (`attached_destroy`); resident
looping Splines with a slot-1 seed of their own resource are relocated and relaunched the same way.

Gaps: the particle trails/bursts (builtin26/16) are not drawn, so the rockets, spintwins, dragons and
finish fireworks are invisible in the browser; the raven's flap animation (AnimObject builtin31 181)
is missing; the section activation of slot 1 is approximated by course progress; EZrocketCore
(program 142 timeline) is not wired; triggers fired by computer riders need their core instances'
contact logs forwarded (the PS2 fired the rockets at 869 and the spintwin at about 3723, i.e. by AI
riders, before the human's contact).

## Capture evidence

* `local/ps2-capture/runs/setpieces/race.*`: event-race pad script plus tuck segments from the
  countdown anchor, 8,200 ticks, snapshots every 400 ticks (`--keep-states`), chairlift watch windows
  0x592E80/0x593580/0x5CF200/0x5CEE00/0xBAB330; reaches about 53% of the course.
* `local/ps2-capture/runs/setpieces/full.*` (script `local/ps2-capture/scripts/setpieces-full.json`,
  198 open-loop segments found with a closed-loop PINE explorer): the human crosses the finish at tick
  12,297 (FINISH 00:03:21); 45 snapshots plus dense spline-phase snapshots `fulldense.tick*`; the
  per-snapshot inventory is `full.inventory.json`. `--isolate` only removes human/AI pair contacts,
  so computer riders fire triggers too.

Trigger entries (human root / first AI): startfireTrig 419 / 411, triggerRockets_1100 880 / 870,
crumbTrig 1289 / 1315, triggerFire_1200/1300/1400 1373/1963/2582, raventriggera ~3283 (body) / 3252,
spintwintrig 3828 / 3685, midfireTrig 4095 / 3894, rockslidetriggera 4274 / 4052, dragontrig_1000
11160 (AI0 passed at 10356 without firing), dragontrig_1100 11673 / 10847, EZseqTrig 12259 / 11651.
Not fired by anyone: bcvolume_1001 (falling billboard fallingbb_1000), 14 tree-top triggers,
shortcut_03, EZrocketCore_1001. There is no avalanche (AvaSpline, builtin95 is unused on ARA1);
the "falling rocks" are the rock_roll_fall LiveComp animations above.

## Not ported / next

Superseded by "Stage world" below (particles, section streaming, computer-rider contacts, MeshAnim, pickups). Still
open: the rider side of the BRA2 teleport beams (builtin 34, docs/stage-scripts.md) and the shared visual RNG order
(0x4FF018 consumers, see "Visual random stream").

## Metro-City (BRA2) and The Junction (BHP1) (2026-09-22)

The exporters take `--location` (default ARA1, outputs unchanged): `tools/set_piece_location.py`
resolves the stage like `import_stage_scripts.py` (BRA2 track 16 / chunk 56, 284 rows; BHP1 track 15 /
chunk 49, 100 rows at +0x6C; global handler 2 = program 3 = start-gate doors in both). Outputs:
`web/public/assets/<LOC>/{FLAGS,UVSCROLL,LIVECOMP}/`, `local/event-activation/<LOC>/{spline-setpieces,set-pieces}.json`,
`web/generated/set_piece_seed_<LOC>.hpp` (namespace `browser_set_pieces_<loc>`). The JS tests take the
location as argument (`node test-flag-animation.mjs BRA2`). Flag wind mode = course table 0x43D950 row
+0x54 plus 1 (0x2D1BA0): 1 for all three courses. No AnimTeeter/Rail (builtin 6/48) or TexFlip on either.

PS2 evidence: `local/ps2-capture/runs/setpieces-bra2/full.*` (closed-loop explorer script
`scripts/setpieces-bra2-full.json` replayed open loop from metro-city-countdown-anchor, 0 cm drift,
finish at tick 13316, 34 kept snapshots every ~400 ticks), `setpieces-bhp1/full.*` (tuck script, 11 kept
snapshots 418..4420; the event restarts at 4697, restart snapshots moved to `restart/`), and
`setpieces-{bra2,bhp1}/go/` (snapshots every 2 ticks around GO).

| Location | Set piece | Driver | Timing | Status |
| --- | --- | --- | --- | --- |
| BRA2 | movingbina_1000 (3 bins), unique_cargobin_end (2), unique_cargobin_end_b (4, LiveComp) | MultiSpline (programs 47/48/49, slot 1), splines mill_a/mill_end/mill_end_b, 35/-22/35 km/h, mode 3, no rotation | section streaming: built at capture ticks 9581 / 11761 / 12201 (human remaining 96,843 / 41,169 / 32,112 cm); movingbina unloaded between 11219 and 11618 (remaining 55,282..43,094) | ported, verified |
| BRA2 | traina/trainb | LiveComp once, 2 s (programs 41/44 on trainatrigger/trainbtrigger contact) | AI riders fired them first (b ~4733, a ~5157 and ~5950); human entries 5673 / 7115 | livecomp.json, verified |
| BRA2 | dragon_1100/1101 | Spline 180 km/h stop (program 245, dragonTrig_1100 contact) + DynParticle | human entry 6501 | seeds only (Snow Jam spline-piece layout) |
| BRA2 | 28 flags, 200 UVScroll, 104 LiveComp (doors, searchlights, pinlights, boosts, dumpster lids, timers) | as Snow Jam | doors: GO tick 181 | verified |
| BHP1 | bus/car red/car yellow/truck anim + glow (8 modifiers, 1..6 cars) | MultiSpline on traffanim_1000, 80 km/h, start 0/1200/2400 cm (programs 44/47/50/53) | resident from the load (seeded at race tick 0) | ported, verified |
| BHP1 | blimpa_1000 | Spline loop 10 km/h (program 39) + LiveComp; blimpad x2 / blimplights Parent | resident | Spline verified; Parent not ported |
| BHP1 | 12 flags, 30 UVScroll, 33 LiveComp (small start doors, searchlights, pinlights, timers) | as Snow Jam | doors: GO tick 181 | verified |

Verification: `python3 tools/test_multi_spline_location.py --location BRA2|BHP1`
(`tests/multi_spline_location_live.cpp`, logs `local/reference/multi-spline/<LOC>-<group>.log`): full
original 0x35A560 + 0x3568B0 (and 0x356198 for the blimp Spline) in lockstep with the ports from the
start savestate (BRA2 3749/1350/950 ticks, BHP1 4420 ticks: every tick exact), every later kept snapshot
of the same run bit-exact (BRA2 14, BHP1 99), 20,000 randomized 0x345248/0x35AC20/0x35B200 per group,
`rails.json` equal to the runtime path segments, and the BRA2 construction state (d0 then N updates)
reproducing the savestates. Flags: 28 / 31 live manager slots reproduced; UVScroll: 715 / 271 snapshot
pairs (one BRA2 fence rebuilt by section streaming is matched from its exported creation state);
LiveComp: 283 / 458 clean snapshots, 386 / 502 replay pairs, 168 / 56 GO door states.

Browser: `web/set_piece_gameplay.inc` builds per-location tables (`set_piece_tables(location)`: ARA1 from
`set_piece_seed.hpp`, BRA2/BHP1 from `set_piece_seed_<LOC>.hpp`, selected by the collision world's
`location`), so every location runs only its own MultiSplines (N cars each), looping splines (the BHP1
blimp, drawn only), spline pieces and trigger owners. BHP1 traffic is resident from race tick 0; the
BRA2 bins become active when the human's course remaining drops to their captured activation distance
(an older modifier holding the same clone ids is unloaded first). Collision-type-0 set pieces (the
Junction traffic) keep their authored matrix for drawing (`world_bridge.cpp` type-0 branch).
`web/prepare.py` marks their batches moving and packages flags/UV scroll/LiveComp per location
(`/assets/<LOC>/FLAGS`...), which `set-pieces-renderer.js` loads from `course.root`.
`test-set-pieces-locations.mjs`: the browser BHP1 traffic (distance and every car matrix) is bit-exact
against 88 PS2 modifier snapshots (11 savestates of `setpieces-bhp1/full`, snapshot tick T = T entity
passes); BRA2 bins stay inactive without course progress.

Gaps: section streaming is not modeled (activation by human remaining distance is the proxy; the
original also unloads sections, and the clone resources of different modifiers reuse the same runtime
pool ids); ParentModifier (0x48F508, ctor 0x357038, eval 0x357108: child matrix = parent node matrix
with translation + R x offset(+0x30)) is not ported (BHP1 blimp ads/lights, searchlight glows on both);
builtins 2/13/16/26/30/31/88/105/106 are now in the stage world (below; sounds go to the audio agent).

## Stage world (2026-09-23)

`web/stage_world.inc` (included by `web/stage_script_gameplay.inc`) runs the entities that stage programs build, in the
browser core, in the original entity pass (0x356198, newest entity first, before the riders) and the section pass
0x101B60 (after every rider). Data: `PARTICLES/particles.json` (tools/export_set_piece_particles.py: blocks, owner
matrices, carrier node chains, the ready savestate's effects, MultiParticle groups, magnets and halos),
`LIVECOMP/livecomp.json`, `STAGE/stage-world.json` (tools/export_stage_world.py: MeshAnim models, script-changed
instances, teleports, collections), loaded by `set-pieces-renderer.js` (`init_stage_world`).

* **Particles** (builtins 16/25/26/69; Particle 0x4912B0 / DynamicParticle 0x491268): `engine/set_piece_particles.hpp`,
  drawn by `web/set-piece-particles.js` (VU1 burst/trail sprites 0x380518 / 0x3807A0, 64 px cap, GS 0x48/0x44/0x42 in
  the encoded composite; pipelines warmed during loading). A LiveComp's effects skip the tick its once-mode player
  finishes (0x341D48 returns 0).
* **MultiParticle** (105/106): one static emitter per group updated once per tick (0x357BF8 after the entity passes),
  drawn at every member's translation; the race-start scan registers the load's roadflares a second time and each
  leave removes one entry (0x3581F0 first match).
* **MeshAnim** (13): 0x351B40 construction (pose from the source LiveComp node or the magnet), 0x352230 visual draws,
  0x352500 update (life with the EE sub.s operand mask: 1-ulp exact over 120 ticks), end modes 0/1/2 and slot 4; the
  renderer moves per-node copies of the model (`prepare.py` meshanim batches).
* **Node states** (2/29/58, Debounce 1): DeadNode / Hide / RestoreNode flags reach the collision world
  (`eventRuntimeFlags`) and the renderer (`stage_world_instances` -> `script_resource` batches).
* **Magnets** (90, `engine/magnet_modifier.hpp`): the entity-route box answers filter 1 only
  (`WorldCollisionInstance::answerBox`), the first human contact acquires, the slot-2 program runs once the pickup
  reached the rider (0x357660), the Debounce freezes its matrix (0x355F10). BHP1 pointa: award at 1085 and 2327 exact
  (setpieces-bhp1/full), pipe-finish 2862 exact (`STAGE_WORLD=1` gate). The renderer moves the pickup batches / the
  LiveComp root by the magnet offset. While the award's Debounce lives its entity matrix is the frozen magnet matrix
  (`stageFrozenMagnetMatrix`), so the builtin25 burst is born at the rider (PS2 `bhp1-pickup-burst` snapshots, 2026-09-25).
* **Halos** (97): `web/set-piece-halos.js` (angle += spin, reset at +/-360; FX 37+key; P from the magnet / LiveComp
  node / instance; GS 0x48, depth tested, priority 8 = encoded composite).
* **Collectibles** (37/38/39): 0x535C11 (`set_stage_collect_state`: 0 career, 1 single event) and the career collect
  row: single events turn every listed collectible into a DeadNode; career races keep the uncollected ones without
  entity until their section builds them (LiveComp + magnet + halo, one gameplay-RNG draw), and each collect is
  queued (`stage_collect_events`) for `web/stage-collect.js` -> `Career.markCollected` (bit + cash + earnings, saved
  in `ssx3.career.v1`). `prepare.py` batches the collectibles as `script_resource` (not `event_dead`).
* **Event-kind test** (43): the ARA1 finish reset planes kill themselves in their section-enter program (Race !=
  free ride), which closed the setpieces/full 12199 divergence (the gate now runs the stage world to the end).
* **One-way volumes** (7): `engine/one_way_volume.hpp` (0x341388 / 0x3415D0 / 0x341818 / 0x1250A8), the rider list
  rider+0x5B8 from 104E70; unreachable from the race routes (ARA1_B lies past the finish), entity words exact.
* **Crowd** (88): the crowd2d / crowdpod material slots are world texture 9-161 on all three courses; `web/crowd-2d.js`
  swaps its image through CRWD.SSH an00..an15 every 3 ticks and draws the camera flashes (FX flsh) from the 64 slots
  the core registers (`stage_world_crowd`). The flash timers use a local generator (see below).

Verification (`web/test-stage-world.mjs`, in `npm test`): ARA1 six riders 59 savestates / 912 effects, BHP1 11 / 853
(+ score exact), BRA2 16 / 175 + 10 MeshAnim states + 16 MultiParticle groups, ARA1 one-way volume words; the fast
sprite evaluators vs the VU1 model. `web/test-stage-collect.mjs`: single event vs career collectibles and the save.
PS2 snapshots: `tools/export_particle_snapshots.py` (effects, MeshAnims, MultiParticle groups, Boosts) ->
`local/reference/set-piece-particles/*.snapshots.json`; new capture `local/ps2-capture/runs/setpieces-bra2-break`.

### Visual random stream (0x4FF018)

PS2 order of one race update 0x2306B8 (from the draw trace `local/ps2-capture/runs/visual-rng/full2.trace.json`):
group-1 entity pass (DynamicParticle births, script emitter seeds) -> rider manager (each rider's 121818 contact
programs, then the FX passes for all riders: grind chunks, snow emitters; then 0x101B60 with the flag grid builds) ->
group 2 (flag wind once per second 0x34C668, camera splash 0x2F39E0: 1 draw) -> group 3 (ScreenTint lightning
0x390C60: 1 draw while the game flow is countdown/race, chance 0 on these courses) -> CrowdMan2d 0x229530 (3 draws per
flash timer expiry) -> cameras (shake). No draws at render time.

The browser core now draws the world consumers on the human core's stream in that order: entity pass (race_begin),
section pass + `stage_world_visual_pass` (section_pass(), after every rider): flag grid builds (core flag manager
bookkeeping; `web/flag-animation.js` consumes the logged words), flag wind, splash, lightning (ends two updates after
the finish update), crowd timers (ready-savestate countdowns and slots). `web/test-stage-world.mjs` checks them
against the trace: wind 212, splash, lightning and 6 flag grid builds on the PS2 updates, crowd expiries equal until
the first re-armed timer. Still not on the one stream (other owners): the FX pass runs before the contact programs
(`animation_tick` vs `race_end`), the computer riders' emitters draw in their own cores' streams, audio draws use
Math.random, and the stream's start words come from the snow seed instead of the ready savestate. Full consumer map
and the remaining gaps: [visual-rng-order.md](visual-rng-order.md).

## Timer splines after the finish (Schizophrenia, 2026-09-28)

The PS2 draws the shared RNG at 3609 / 3615 and 3849 / 3855 of peak2/schizo-event-tuck, after the 3492 finish. The draws come from
builtin 19 (0x35955C, the Spline constructor) in the entity pass. Trigger 24087's slot-2 program Debounces itself for 2 s and
builds a once-mode timer LiveComp on 101655. The timer's slot 5 launches the splines 123159 / 111895 at its crossings, and its
slot 4 (program 98) Hides 101655 when it ends. The rider stands in the trigger, so every 120 ticks the program runs again. When the
timer has been hidden, a new one starts and relaunches the splines 240 ticks after the last launch. The port had three gaps:

- **30A060's contact gate** (0x30A0CC..0x30A0F0) refuses a human rider only when rider+0x480 is set: a DNF from the 125228 time-out
  or a Give Up. A normal finish (125108) leaves +0x480 at 0. The port refused every finished human, so the post-finish contacts
  never ran. Checked in the captures: +0x480 becomes 1 only at Kick Doubt's time-out, and stays 0 at the Schizophrenia and
  Junction finishes.
- **A LiveComp replaces a Hide / RestoreNode node** at instance+0xC. 101655 was a Hide node before the timer was built, and the
  port kept that node state, so program 98's Hide saw type 16 and left the finished LiveComp alive. The later builtin3 then found
  a LiveComp (vt+0x12C 0x3609D8 is `jr ra`) and built nothing.
- **Relaunching a finished spline.** 2FAE38 re-initialises a Spline that is still flying (vt+0x12C), but replaces the Position
  entity of a finished one (end mode 0) with a new Spline, which draws. `stage_launch_spline` refused any existing piece.

Result: the real page (compare-page-capture) keeps the shared RNG exact through the end of Schizophrenia (3881). The draws are
sampled one tick apart from the record, so they show as one-tick blips. crows-invert, perpendiculous, Launch Time and
Much-2-Much stay exact. The 243 capture gates pass and sim-diff is identical. Live core sha256 f556dfb6.

## Moving lit instances and the Object spline pieces' draw (2026-09-28, visual-parity agent)

The visual agent listed 15 lit instances with runtime flag 0x1000 ("relit as it moves": their PS2 light caches sit away from the
instance matrix) that the port drew static. How the PS2
moves each, from the stage programs (tools/set_piece_location.py + export_startfire.decode_program), and what the port does:

| instances | PS2 | port |
|---|---|---|
| BHP1 caryellowanim / buswhiteanim / truckwhiteanim / carredanim _1000 | slot 1: builtin 0 + 20 (MultiSpline on the owner, key 10 = 0), then 0 + 20 on the glow copy | seeded from the ready state (8 modifiers), moving batches: they move (page 419 -> 1619) |
| ospreys ABA1 / CRA3 / DRA4 / DSS2 (contact trigger), ABC1 (timer), EBC3 cessnas a / b | builtin 3 + 19 (LiveComp + Spline) | spline pieces (drawn) drawn per node as spline LiveComps (attached.json) |
| ESS3 sleda_1000 | sledatrigger slot 2: builtin 0 + 19 (Object + Spline), path 42288, 120 km/h | the core moved it, the page drew it static: fixed below |
| os609 in-air heli ABC1 / DBC2 / EBC3 | slot 1: builtin 3 (LiveComp, frames 551..677, 30 fps) | in livecomp.json but hidden: open (below) |

**Object spline pieces outside Snow Jam (fixed).**
- `tools/export_location_set_pieces.py` wrote `spline_pieces` as names only, and `web/prepare.py` moves only `drawn_spline_pieces`.
  So no Object spline piece outside ARA1 had moving batches, although the core emits its delta (`chairlift_moving`,
  browserSplinePieces with `drawn` and phase 1).
- The exporter now writes `drawn_spline_pieces` (the pieces drawn while on their path). The pieces drawn as spline LiveComps are
  split per node by prepare.py's attached.json path as before.
- Newly moving: ESS3 sleda; ASS1 locomotive, flatbed, boxcar, tanker, trainboxes x 2; CBA2 sleda / sledb; DRA4 dragonworks x 6 and
  EZrocketCore x 2; EBA3 EZrocketCore x 2. Re-split packages: triangle sets identical, only these batches gain moving_resource.
- Checked: ESS3 kick-doubt-full 818 / 1219 / 1618: the core's delta equals the PS2 Spline modifier +0x60 matrix (the Object's draw
  matrix) within 1e-4; the page's 3 sled meshes are static at 418 and moved at 1219 (Chrome, WebKit).
- Re-running the exporter today also changes the seeds of BHP1 / CHP2 / CRA3 / EHP3 (new pieces) and the multisplines of CBA2 /
  CHP2 / EBA3 / EHP3; the headers were left as they are (physics agent).

**os609 helis (pv `heliWorld`, off).**
- **Where:** the heli is a LiveComp (0x490B10) whose section program 3 loops frames 551..677 (the hover). Its node matrices in PS2
  RAM (*(entity - 0x30 + 0x60), world) put it about 5 m from the start camera, above and behind it. It stays there the whole race
  (ABC1 ticks 2 .. 3600), so no race frame shows it: every kept camera projects it behind.
- **The draw rule, settled on the PS2:** derived state `local/ps2-capture/runs/heli/abc1-moved2` (happiness-ready + the pad script
  `heli-abc1-look.json`; pokes of the heli instance +0x40 translation to 30 m ahead of the tick-40 camera; silent).
  - The node matrices follow the instance matrix, and the PS2 draws the heli at ticks 40 and 60 (`abc1-moved2.tick40/60.png`).
  - The flags are 0x50015305 at 40 and 0x50015105 at 60: drawn at both parities.
  - So an entity in the renderer's dynamic list carries 0x100 or 0x200: 1032C0 / 101B60 add it with 0x100 and flip the pair each
    rebuild (0x1030F4..0x103160), and 103358 clears 0x100 at a list reset. The countdown audit's 0x50015205 (0x200 alone) was
    classed 'none' only because `draw_class` required 0x100.
- **Fixes:**
  - `tools/export_event_membership.draw_class` now takes 0x100 or 0x200. A re-audit also turns these entities from 'none' to
    'entity': ASS1 ravensplineanimb, CHP2 blimpa / blimpad x 2, BHP1 blimpad x 2 / blimplights, ABC1 snowsheet_1000, EBC3 summit
    flag pole. Their packages change only when they are re-prepared.
  - Page, pv `heliWorld`: `set-pieces-renderer.js` draws the os609 LiveComp meshes while their player runs, as the liveCompObject
    class. While an arrival set (SETS/<LOC>HELI, pv bcHeli / heliHover) stands in for the heli, the world meshes stay hidden with its
    copy (`cutscene-stage-sets.js adoptWorldCopy`, also for meshes added after the set showed).
- **Checked (Chrome, WebKit):**
  - The same move applied in the page (a wrapper group at the poke's offset), camera pinned to the derived run's records: the heli
    is at the PS2's place and pose at 40 / 60. The rotor phase differs, because the section loop starts at race tick 0 (the LiveComp
    gap above).
  - Switch off: absent.
  - ABC1 race frames 60 / 181 / 400: 13 heli meshes in the scene, behind the camera, no pixel change (as on the PS2).

## MultiSpline section count and the Object-entity destroy (2026-09-28, core46)

- **PS2.**
  - A section leave (0x30A460) of an entity whose first modifier is a MultiSpline (vt+0xA0 = 0x356BF0: modifier vt+0xD0 == 2) only calls vt+0xB0 (0x356CC8 -> 0x35AAE0): the modifier's +0x34 goes down by 1. An enter (vt+0xA8 -> 0x35AAD0) adds 1.
  - The construction (0x35A0E0) starts +0x34 at 1 when the owner instance is listed (flag 0x100), else at 0. The modifier itself never reads it.
  - The Object entity's update 0x356198 (vtable 0x490E80, the entity pass) does read it, through vt+0xB8 = 0x356D48. At <= 0 it calls 0x34FCE0 -> 0x2D1A30: the instance's slot-3 program if it has one, otherwise vt+0x118 0x34FD90 destroys the entity. This happens before the modifier update.
  - A LiveComp owner's update (0x341D48) has no such test.
- **Metro-City bin 707856** (`runs/multispline/bra2-bin2`, watching the instance, the entity, its modifier list and the modifier; savestates `bra2-snap.tick11338/11341/11343`):
  - The MultiSpline is live through 11321.
  - The leave scan at the end of tick 11341 clears the listed bit (flags 0x210245) with the entity still alive.
  - The entity pass of 11342 destroys it (flags 0x210023, entity 0, by 11343).
  - Texture chunk 53 stays resident throughout, so the destroy does not come from chunk eviction.
- **Owners, from the slot-1 programs:**
  - builtin 0 + 20, an Object entity (destroyed at count 0): BRA2 707856 / 252432, ABA1 227589.
  - builtin 3 + 20, a LiveComp: BRA2 820240 (unique_cargobin_end_b), EBA3 91433 and the Peak 2/3 trams. Their leaves only change the count, and they stay live.
- **Port** (`web/section_gameplay.inc`, `web/set_piece_gameplay.inc`):
  - Actions 4 / 5 (MultiSplineAcquire / Release) only change `BrowserChairlift::refs`; they no longer call `section_stop_piece`.
  - A section-built lift starts at refs 1 and records whether its owner is an Object entity (`stage_slot_calls(resource, 1, 3)`: no builtin 3).
  - In the entity pass, an Object-owned lift at refs <= 0 runs its slot-3 program, or is released (`browser_release_section_lift`, shared-world event 7). The section model then forgets its entity, so the next enter runs slot 1 again.
  - Before, the bin went at the leave tick itself (11341). Now it goes at 11342, as on the PS2.

## Regenerated location seeds: section-streamed ravens / eagles, the blimps' guarded programs (2026-09-28, core62)

- **Regeneration.** `tools/export_set_pieces.py --location` for BHP1 / CHP2 / CRA3 / EHP3 adds seven slot-1 seeds to `web/generated/set_piece_seed_<LOC>.hpp`:
  - BHP1: blimpa 63247.
  - CHP2: ravensplineanima 203799, ravensplineanimb 152343, blimpa 231191.
  - CRA3: eaglesplineanima 332056.
  - EHP3: eaglesplineanima 19756, eaglesplineanimb 28460.

  The old rows are kept verbatim, and everything outside `splinePieces` is byte-identical. `local/event-activation/<LOC>/set-pieces.json` (BHP1, CBA2, CHP2, CRA3, EBA3, EHP3) gain `drawn_spline_pieces` and the tram `multisplines` entries: CBA2 152086, CHP2 226327, EBA3 91433, EHP3 38444. The previous jsons had `multisplines: []` although the headers carried the trams.
- **PS2, ravens and eagles.**
  - Each is a LiveComp with a looping Spline (end mode 1), resident from the load, whose own slot-1 program is builtins 3, 31, 19.
  - A section leave destroys the entity (0x34FD90), and the next enter rebuilds it. This is the R&B raven B path (`loop_relaunch_seed`).
  - Kept savestates (the modifier addresses stay the same between states, so the distance drops are loop wraps):
    - `peak2/chp2-full`: 203799 lives through 2820 (modifier 0x5B8000) and is gone by 3218. 152343 lives through 3618.
    - `peak2/cra3-full`: 332056 lives through 818 and is gone by 1218.
    - `peak3/perpendiculous-full`: 19756 lives through 3618 and is gone by 4018. 28460 lives through 4018.
  - Without the seeds, the port kept these loops flying, and drawn, after the destroy. Now they go at 3161, 1161 and 3881.
- **PS2, the blimps.**
  - BHP1 fencecollision_1001 (52495, program 39) and CHP2 96279 (program 82) start with `if builtin52(blimp) == 1 return`. The rest of the program rebuilds the blimp: builtin 0 on the blimpads, builtin 21, builtin 3 + 19 on the blimp, then builtin 18 parents.
  - The blimp is resident all run, so the program returns at its head.
  - The section model (`engine/section_streaming.hpp` runProgram) already skipped its draws. The port still ran `section_start_piece`: the VM program (builtin-0 entity marks on the blimpads and blimplights) and, with the new seeds, a second blimp Spline at tick 19.
  - Now `Event::guarded` marks such an enter, and `section_scan` (`web/section_gameplay.inc`) skips `section_start_piece` for it.
  - No other program is guarded with a live target: the Peak 1/2 streamed copies start without the blimp, and the other BHP1 guards belong to MultiSpline owners, whose enter runs no program.
- **Check.** `web/test-set-piece-seeds.mjs` (new, in test:all) compares every Spline / Position / MultiSpline entity of the kept savestates. Launched pieces are bit-exact on distance and translation; resident loops are checked by presence.

  | Capture | States | Spline entities | MultiSplines |
  |---|---|---|---|
  | setpieces-bhp1/full | 11 | 13/13 | 88/88 |
  | chp2-full | 9 | 27/27 | 6/6 |
  | perpendiculous-full | 10 | 21/21 | 9/9 |
  | cra3-full | 3 (until the solo replay leaves at 1384) | 5/5 | none |

  Core60 fails it: raven 203799 is still flying at 3218. Other checks, all passing:
  - The capture gates of BHP1, CHP2, CRA3 (including AI), EHP3, CBA2, EBA3 and peak1-arrive-bhp1: 33 scenarios.
  - Sim-diff core60 against core62 is identical.
  - stage-world, set-pieces, set-pieces-locations and attached-core pass.
- **Rendering.** The moving set of spline pieces is unchanged, because every new piece was already a resident `spline_modifiers` resource. The trams join `tools/export_peak_world.py event_moving` through the jsons' `multisplines` when the packages are prepared again.
- **Open.** None of these captures shows a raven or eagle being rebuilt at a later enter after its destroy. That code path is the same as R&B raven B (`lineups-ASS1`).

## Streamed worlds: Spline pieces and spline LiveComps (pv `peakSplines`, off; 2026-09-29, CTM fixes agent)

**The gap.** `locationBatches` renames every streamed package's location to PEAK1..3 / MOUNTAIN. `set_piece_tables("PEAKn")` was
empty, so `setPieceCourse` stayed false and none of the free-ride worlds' 74 + 19 Spline / MultiSpline programs moved: the dragon,
osprey, raven, eagle, blimp, cessna, rocket and train flybys, the chairlift chairs, the BHP1 / ABA1 / BRA2 traffic and bins. Their
trigger sounds still played. Decomp: [ctm-decomp-freeride.md](ctm-decomp-freeride.md) item 3.

**PS2 rules.**
- builtin 19 = 0x2FDED0 -> 0x355AD0 -> constructor 0x359460: exactly one shared-RNG draw (0x317830), even with zero jitter.
- builtin 20 (0x2FE0C0 -> 0x359F88) and builtin 18 (0x2FDC60 -> 0x357038) make no draw.
- A location's unload (0x230360 -> 0x3551A8(gp+0x2898, 1 / 8, track)) deletes its entities: no slot-3 program, no draw.
- The resource ids of the event and the streamed packages are the same (source hash f4952d8a...).

**Built (core, behind `set_piece_streamed(on)`; free-ride.js next to stage_object_route; peak-capture.mjs env `PEAK_SPLINES`):**
- `set_piece_tables` for a streamed world is the union of its event locations' Spline pieces and trigger owners:
  - PEAK1: ARA1, BRA2, BHP1, ASS1, ABA1, ABC1;
  - PEAK2: CRA3, DRA4, DSS2, CBA2, CHP2, DBC2;
  - PEAK3: ERA5, ESS3, EBA3, EHP3, EBC3;
  - MOUNTAIN*: all 17.
  - So the contact, timer and gated builtin 19 launches run there as in the events. The section pass already draws for the slot-1
    enters.
- `attached_reset` loads every location's spline LiveComps and ParentModifier children. None is resident (the free-ride locations load
  without entities), and a section-run launch takes no second draw (`attached_launch(resource, draw)`; the LiveComp's random start
  word is 0 there, unconfirmed).
- `browser_set_piece_track_teardown(track)` runs from browser_stage_track_teardown (peak_world set_state 7). It drops that track's
  pieces (collision back to the countdown flags), their owners' builtin 52 guards and its attached LiveComps.

**Checked.**
- Streamed gates with PEAK_SPLINES=1, all at their baselines: apr / p2r / apj-start, fr-dra4a-full 8403, fr-throne-unload, p3b-right3000,
  fr-d-glide, frd-regions, fr-aara1-glide, arrive-ass1 / -bhp1.
- Event set-piece gates with it off, all at their baselines: metro-event-race, uber-bag, rnb-event-tuck, setpieces/full, dra4-race-ai,
  parity-ai/ass1, eba3-rock-hit, ebc3-wind-rail2.
- RNG draw counts per tick (`RNG_NO_ALIGN=1 RNG_DRAWS_DUMP`, against the capture's RNG words; tool: the research `draws.py`):
  - fr-dra4a-full's spline trigger ticks 356 / 1082 / 1551 / 4017 / 4049 now equal the PS2's (they were 1-2 short; `--sync-rng` hid it);
  - allpeak/apr-start is draw-count exact on all 13,999 ticks (its only gap was the EBC3 cessna at 9925);
  - no new mismatch.
- Page (Chrome, PEAK1 at R&B, the scratch re-split packages served as an overlay): the ravens (mdl_ASS1_ravensplineanimb/c) launch
  at their section, and their 10 LiveComp meshes draw on their splines (pv peakAttached).

**Data (not in web/public/assets yet).** `tools/export_peak_world.py --peak N --batches-only --out <scratch>` for N = 1..3 and
`export_mountain_world.attached(src, out)` for MOUNTAIN wrote the `moving_resource` / `livecomp_resource` split packages and
SETPIECES/attached.json (local/ctm-fix/export). Without them there is nothing to draw: the switch then only moves the core state
and the RNG. peak-set-pieces.js now survives a missing attached.json: the dev server answers a missing file with index.html.

**Not built yet:**
- the MultiSplines (the chairlifts, BHP1 traffic, BRA2 bins, the Peak 2/3 trams): section-built from construction states, with clone ids
  per world;
- the resident loops (blimps);
- the ESS3 tram (no seed);
- CRA3 ospreyfocus, the ABC1 tumbler owner 80902, and DSS2's five mission-start targets.

## Streamed worlds: the trigger RestoreNode and the riders' guards (2026-09-30, CTM events-in-world agent; c0a-ws13)

- **The EZrocketCore launch is on the PS2's tick.** The PS2 record is written at the human's provider exit, so the entity-pass draws
  of tick T+1 fall in the capture's window labelled T. Probes on 0x30A688 (slot-5 dispatch) and 0x34EBE0 (builtin 55's crossing) and a
  savestate read of the timer (rate 0x3BB60B61, time / previous / unclamped after 9 steps 0x3D4CCCCC / 0x3D360B60) match the port.
- **342E98's restore 3 makes a RestoreNode** (0x350F60, type 19 at instance+0xC), which is section-listed; its leave (0x34FD90 -> the
  0x34FBF0 destructor) puts the authored flags back. Program 136 (dragontrig_1000) is `Debounce; if builtin52(0x85308) == 1 return;
  b19 x2`, so the trigger fires again once the dragons' entities are gone (Griff at 11944, probe_contact.py on 0x12186C: rider
  0x1a6c060, resource 326408). The port's Debounce completion now sets node state 19 and tells the section model
  (web/stage_script_gameplay.inc); before, the trigger stayed un-contactable after its first firing (every world).
- **Shared world event 10** (pv peakSplines): the streamed world's section programs run in the human's context only, so a piece its
  section leave destroys is released in the other contexts too (`browser_spline_piece_released`: the piece and its owner's launch
  guard), and web/ai-racers.js forgets the owner key so the next launch replicates again.
- **Suite with PEAK_SPLINES=1** (core-suite4): all 266 scenarios pass; ctm-events/c0a-ws13-splines is exact to the live stop.

## Streamed worlds: page QA of peakSplines, peakAttached and the re-split packages (2026-09-30, peakSplines QA agent)

Tools and frames: `local/peak-splines-qa/` (overlay-server.mjs serves a scratch export over web/public/assets; run.mjs: headless
Chrome with --mute-audio or web/webkit-driver.mjs; qa.mjs: the frame clock frozen from the first frame, so tick 0 is the ready
screen and every configuration gets the same neutral-pad ticks and views; matrix.mjs, analyze.mjs (pixel diffs), trigger.mjs (the
rider dropped into a trigger, then the piece's draw checked), memtour.mjs (WebKit footprint with in-world Transports), diverge.mjs
(per-tick rider / RNG / contact log)). Configurations: A switch off + live assets; B on + live; C on + the new packages
(local/ctm-fix/export); D off + new; `p` = with pv peakAttached; D0 / C0p = the new packages with `env` / `lighting` stripped.

**What the 42 changed area files are.** Every triangle keeps its texture, lightmap, blend and instance flags (multisets equal in
all 42 + their indices.bin). Besides the `moving_resource` / `livecomp_resource` split they carry what the live streamed packages
predate and the event packages already have: the env-map second pass (`env`, 1..418 batches per location; pv envMap) and lit
instances (`lighting`, up to 198; pv litInstances), plus one texture reference (9-50). vertex-alpha.bin is unchanged everywhere.

**Frames (Chrome and WebKit, R&B, Junction, Ruthless Ridge, Intimidator, Perpendiculous; MOUNTAIN at Intimidator and R&B):**
- A2 = A, B = A and D0 = A to 0 px on every frame before a piece launches: the re-split loses or doubles nothing (D0 max delta
  <= 6 on single pixels: batch order).
- D / C differ from A only where env / lit batches are (the R&B stand glass, 5.8 % of the t900 frame; the Ruthless Ridge
  billboard; the Perpendiculous stadium). Against the event packages at the same camera (local/peak-splines-qa/evcmp/crop-*.png,
  tri-*.png) the new streamed frame matches the event's glass / billboard sheen and A lacks it. The env and lit rules are the
  models' own material words (37F2A4..37FD2C) and the event and streamed resources are the same models, so this is a fix. The only
  streamed-world PS2 frames near such objects (peak1-arrive-ass1 2101..2311, the stand far off) do not contradict it.
- B after a launch: the rider leaves A's path (Intimidator: 0.22 m at 2230). Traced (div-A/B.jsonl): the dragontrig contact at
  2193 is the same in both, B's builtin 19 takes one shared-RNG draw, the rider stays bit-equal to 2215 and then drifts. It is the
  RNG, not a contact: the dragonworks has no collision (descriptor type 0, collision_resource 0xFFFFFFFF) and never appears in
  the contact log. The launched rockets' sparks fly in B (particles.json is live), but their bodies stay at rest.
- Flybys on their splines (C + peakAttached, moved meshes checked, pinned views): the R&B ravens, the Ruthless Ridge eagle and
  osprey, the Junction blimp, the Perpendiculous eagles, both Throne cessnas, the ABC1 tumbler; the moving-batch pieces (the R&B
  train, the Kick Doubt sled, the Intimidator dragonworks) under C alone. Without peakAttached the spline LiveComps stay at rest.
- **Draw / collision.** Spline pieces with collision (browser_entity_rigid: Spline): ASS1 locomotive / boxcar / flatbed / tanker,
  ESS3 sleda, CRA3 osprey and blimpa, ABC1 ava2Tumbler. Under B none of them is drawn moving (trigger.mjs: no moving mesh), and
  under C without peakAttached the osprey, the blimp and the tumbler stay at rest. Only C + peakAttached keeps them together.
  Already today (A, switch off) a crashbag the rider hits moves in the core (R&B t900, crashbag_ssb_1027 in moving_instances)
  while the live package draws it static; the new packages move crashbags, avalanche pieces and trams.
- Console: the same messages in every configuration (missing terrain-glint / camera-triggers / freestyle-event jsons, "Replay
  cameras unavailable"); peakAttached with the live packages adds the expected "attached.json unavailable".
- Memory and load: ready 8.2-8.7 s everywhere; wasm 128 / 154 MB alike. WebKit phone policy (`__XPC_JSC_forceRAMSize`, 844x390,
  quality=low, 2 runs each), A vs C + peakAttached: MOUNTAIN (R&B -> CRA3 -> DRA4 -> EHP3) lifetime peak 1135 / 1057 vs 1096 /
  1118 MB, steady 607 / 615 vs 619 / 624; PEAK2 (CRA3 -> DRA4 -> DSS2) peak 1408 / 1208 vs 1159 / 1220, steady 1370 / 693 vs
  662 / 711 (the 1370 is a late collection). The files are +1.95 MB on the wire. Area build times are within run-to-run noise.

**Ship rule.** The 42 changed + 4 new files (local/peak-splines-qa/ship-files.txt) and pv peakSplines + peakAttached together.
Never: B (switch on, old data: moving collision drawn still, sparks without bodies), C without peakAttached (osprey / blimp /
tumbler). D (new data, switch off) shows no mismatch (it also fixes the crashbags) but leaves every flyby at rest.

**Open.** The eagle at the Ruthless Ridge start and the R&B ravens relaunch every 40 / 120 ticks while the rider is near
(builtin 19 each time; the draw counts are capture-gated), so they restart their path: not yet compared with a PS2 frame sequence.
In QA (no career) some in-world Transports did not arrive or reloaded the page (MOUNTAIN -> ABA1 / The Throne / BRA2), with
either data; not investigated.
