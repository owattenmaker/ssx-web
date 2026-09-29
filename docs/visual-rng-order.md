# Visual RNG 0x4FF018 (0x3177F0): per-tick consumers, PS2 order and browser gaps

Sources: disassembly of SLUS_207.72, the draw traces from `tools/trace_visual_rng.py`
(`local/ps2-capture/runs/visual-rng/full2.trace.json`, the Snow Jam `setpieces-full` script, ticks 18..12458, 172188
entries, none lost) and the `local/ps2-capture/runs/setpieces/full.tick*.p2s` savestates (same script; stepping the
generator by the change of word 5 between consecutive savestates lands exactly on the next one for all 44 pairs).
2026-09-25 (docs/weather.md): the snowfall layers (LCG), the camera splash spawns / crystal draws (heavy snow: DBC2, ERA5,
EHP3) and the ScreenTint lightning with a chance > 0 (EBA3, ESS3: the strike draw, no draws during a flash) are ported in
web/weather.inc; they draw exactly as below on the other courses.
2026-09-26: the streamed worlds (PEAK1..3, MOUNTAIN) run the flag manager on the stream too (web/stage_world.inc
init_streamed_flags: grid builds in the section pass, the 1 s wind in group 2, mode = course table 0x43D950 [0x535C08] +0x54 + 1
every update; docs/weather.md section 11).
2026-09-23 status: the world consumers (flag grid/wind, splash, lightning, crowd) are ported in web/stage_world.inc
(see set-pieces.md "Visual random stream"); section 3 lists the remaining gaps for the other owners.

## 0. Corrections to the working names

- **0x2306A8 is a getter.** It is 4 instructions: `return *(a0 + 0x68 + 4*a1)`, the camera-splash object of camera a1.
  The race game update is **0x2306B8** (game vtable 0x47D130, slot +0x14 at 0x47D144). The call sites below are in
  0x2306B8.
- **0x111890 is not the main loop.** It builds one rider's FX components: ctors 0x2DCF28..0x2F64E8, 0x2C03E8, then
  0x2306A8 to fetch that camera's splash object.
- **0x244880 is not one function.** The functions.csv entry spans several functions. The alternate update body is
  **0x244E60** (group passes at 0x244F94..0x245074, cameras at 0x2450A0). Its pass order is the same as 0x2306B8's.
- **Generator.** Word 5 of 0x4FF018 is a draw counter (`engine/original_random.hpp` `OriginalRandomState::next`: the
  word goes up by one per draw). Δw5 between two savestates is the exact number of draws.
- **lfsrStep is a different generator.** `engine/set_piece_particles.hpp` `lfsrStep` is the VU RINIT/RNEXT generator,
  not this one.

## 1. What 0x2F39E0 and 0x390C60 are

### 1a. 0x2F39E0: Camera Splash update (snow drops and ice crystals on the lens)

- **Objects.** Class vtable 0x488230: slot +0x0C 0x2F3618, **+0x14 update 0x2F39E0**, +0x24 render 0x2F3E28, 0x2F39C8
  reset. There is one object per possible camera. Both live in **entity group 2** (gp+0x2898 manager).
  - Snow Jam order: 0x58D400 (snowfall object, update 0x2E5DA0), the flag manager 0x155AEB0 (0x34C668), splash
    0x1559E70 (camera 1), splash 0x1574E10 (camera 0).
- **Tweak menu "Camera Splash Menu" (0x24BF60), with the values in the race:**
  - 0x115C Enable = 1
  - 0x1160 Render = 1
  - 0x1164 Max Drops = 30
  - 0x1168 Max Crystals = 24
  - 0x116C Percentage Ice Crystals = 0.6
  - 0x1170 Percentage Ice Crystal Spawn = 0.01
  - 0x1174 / 0x1178 Ice Crystal Min / Max in Group = 1 / 3
  - 0x117C Max Spawn Per Crystal = 2
  - 0x1180 Farthest Impact Distance = 450
  - 0x1184 Lowest Impact Intensity = 105
  - 0x1188 Lowest Snowfall For Impacts = 1.0
  - 0x118C Impact Multiplier = 1.1
  - 0x1190 Lowest Snowfall Amount = 1.5
  - 0x1194 Snowfall Multiplier = 0.015
  - 0x11A4 = 4
- **When it runs.** Once per game update, in the group-2 pass (0x354F98(mgr, 2) at 0x230CCC). The pass is skipped
  when debug word gp-0x6A0 & 0x10 is set. It also runs in the load/overlay updates (docs count 312 draws in 1200 load
  frames).
- **Update math (s1 = the object):**
  1. **Gate.** Return if `gp+0x115C == 0` or `obj+0x10` (camera index) >= camera count (`*(G+0x84)->+0x84->+0x10`).
     In single player, the camera-1 object returns here: no draw.
  2. **Speed.**
     - p = camera +0x20.
     - If `+0x100C` is set: `speed(+0x1020) = |p - prev(+0x1010)| * 59.999996 (gp-0x3904) * 0.036 (gp-0x3900)`, which
       is km/h. Otherwise speed = 0 and `+0x100C` = 1.
     - If speed > 1000: call vt+0x70 (reset) and set speed = 0.
     - Then prev = p.
  3. **0x2F3810 spawn.**
     - n = trunc(pending +0x1024); pending -= n.
     - Per unit, 3 draws of the **other visual LCG gp+0xA0C** (the one the board trail and sparks use):
       `w = ((w*0x18FCD + 0xE9507C) & 0x7FFFFF) | 0x3F800000`. They give x = (f-1)*640, y = (f-1)*480 and t = f-1.
     - If t < 0.6: **one 0x3177F0 draw (ra 0x2F3920)** sets the group size k = 1 + r % (3-1) (at least 1). A crystal
       group is spawned with 0x2F3640.
     - Otherwise a drop is spawned with 0x2F37A8.
  4. **Drops.** 0x2F2810 updates the drops (count +0x14, 0x3C bytes each at +0x1C). Dead drops are replaced by the last
     one.
  5. **Crystal draw (0x2F3BE0).**
     - **One unconditional draw (ra 0x2F3BE8):** u = ((r & 0x7FFFFF) | 1.0f) - 1 and f = u * crystals(+0x18) / 24.
     - If f < 0.01 and crystals > 0: **second draw (ra 0x2F3C4C)**, chosen = r2 % crystals. Otherwise chosen = -1.
  6. **Crystals** (0x4C bytes each at +0x724):
     - Each crystal is updated by 0x2F3030.
     - When the crystal is alive, index == chosen and its spawn count (+0x724+0) < 2: the count goes up by one, and a
       drop is spawned from the crystal (0x2F37A8) if both sizes * 0.6 > 4 (gp+0x11A4).
     - Dead crystals are replaced by the last one.
- **Pending sources:**
  - **Snowfall.** 0x2F4330, from the snowfall object 0x2E5920 / 0x2E5DA0, once per camera. It sets `+0x1028 =
    snowfall`. If snowfall > 1.5: `pending += snowfall * (speed + 10) * 0.015 * 0.0076923 (gp-0x38F4)`.
  - **Impacts.** 0x2F4260, from 0x2F4118. Only when snowfall > 1.0 and the distance d < 450:
    `pending += (1 - d/450) * clamp((I*0.036 - 105)/(120 - 105), 0, 1) * 1.1`.
- **Consequence.**
  - Every sampled savestate on the three courses has snowfall <= 0.1, so nothing spawns: **exactly 1 draw per game
    tick** (0x2F3BE8).
  - The trace has no draws at 0x2F3920 or 0x2F3C4C in 12440 ticks.
  - The spawn draws would only appear with heavy snowfall (blizzard weather).
- **Render.** 0x2F3E28 (render slot +0x24) does not draw from the generator.
- **Browser.** Not implemented. There is no lens-drop system; `web/sun-flare.js` is a different effect.

### 1b. 0x390C60: ScreenTint environment component, the lightning flash

- **Owner.** Environment object 0x588880 (vtable 0x487D28). It is built by 0x2F0548 as 17 components, each a 4-byte
  object holding only a vtable. Order and tweak names:
  0 RenderToggles, 1 VisualEffectsTest, 2 LightGlow, 3 Plants, 4 DepthFog, 5 HeightFog, 6 DetailSystem,
  **7 ScreenTint (vtable 0x492FA0: update +0x14 = 0x390C60, render +0x1C = 0x390F20)**, 8 Shadow, 9 SnowSurface,
  10 BoardTrail, 11 Flags, 12 PowerUpFXToggles, 13 SnowfallToggles, 14 SplashToggles, 15 Tube, 16 PathArrow.
- **Environment update 0x2F0A98 (vtable 0x487D28 +0x14).** It calls the +0x14 slot of components 0..16 in order,
  then 0x2F00A0 (region environment refresh) for each camera.
  - It is called from **entity group 3**: object 0x5439E0, update 0x244478, which calls `*(obj+0x10)`'s +0x44
    vtable, slot 0x14.
  - Group 3 is the 0x354F98(mgr, 3) pass at 0x230CE8, skipped when gp-0x6A0 & 4 is set.
  - Group 3 also runs in the 0x230D24 branch (see 2.12).
- **Tweakables (0x24A228):**
  - Fill/XN Tint colours and XN Lerp (0x1500..0x1518).
  - **Enable Lightning 0x151C**, **Frame Id 0x1520** (-1 normally), **Lightning Chance 0x1524**.
  - Flash Fade In etc.: phase lengths 0x1528..0x1534 = 2, 2, 2, 7 ticks, intensities 0x1538/0x153C/0x1540 =
    0.4/1.0/0.5, colours 0x1544.. = (1.584, 1.905, 1.998), 0x1550.. = (0.333, 0.43, 1.99), 0x155C.. = (0.75, 0.75,
    2.0).
- **0x2F00A0 recomputes these every update, after the component updates** (so the flag the lightning reads is the one
  set in the previous update):
  - `0x1524 = region getter 0x2EE738` (vtable +0x1AC of the region payload). It is **0 in every sampled state**: Snow
    Jam, Metro City, Happiness, Crow's Nest, R&B, The Junction.
  - `0x151C = (S == 1) || (S == 0)`, where S = `*(*(G+0x84)+0x28)` is the game-flow word (0x2F7BE0 tests S == 0).
  - S is 0 in the countdown and the race, 10 or 11 on the ready screen, and **13 after the finish**. In the setpieces
    run the race clock froze at race tick 12118 (total tick about 12298). Lightning draws stop after trace tick 12299,
    while the splash continues.
- **0x390C60 each update:**
  - If `!0x151C`: return. There is no draw and no countdown.
  - If the thunder delay `0x15C4 > 0`: `0x15C4--`.
  - If the phase counter `0x15A8 < 0` (idle):
    - **Draw 1 (ra 0x390CA0).** u = unit(r). If `u < chance*chance`:
    - **Draw 2 (ra 0x390CE0).** dist = (unit(r2)*1.98 + 0.02) * 100000 cm (gp-0x26A4/-0x26A0/-0x269C), then
      0x390EC8(dist).
    - Return.
  - 0x390EC8(dist) sets: counter 0x15A8 = 0, intensity 0x15AC = 0, 0x15C0 = dist, and thunder delay
    `0x15C4 = (int)(dist * 0.0018072289)` ticks (553.3 cm per tick, i.e. 332 m/s at 60 Hz).
  - Otherwise (flash running): counter++ (or = Frame Id when 0 <= Frame Id <= the sum of the phases). No draw. Then:

    | Counter c | Intensity 0x15AC | Colour 0x15B0..B8 |
    |---|---|---|
    | c <= 2 | c/2 * 0.4 | colour 1 |
    | c <= 4 | 0.4 | colour 1 |
    | c <= 6 | 1.0 | colour 2 |
    | c <= 13 | (1 - (c-6)/7) * 0.5 | colour 3 |
    | c > 13 | 0 (0x15A8 = -1) | base 0x14F4..0x14FC |

    Mode 0x15BC is 5 in every row.
- **Other callers:**
  - 0x390EC8 is also called by a WScript builtin (0x305778 in 0x303E60, via the interpreter 0x2FC2C0): a scripted
    strike.
  - 0x390EF8 is polled by the audio function 0x285BF8. It returns 1 once when the delay reaches 0, which triggers the
    thunder (bank 8 sound 16; `web/audio-world.js` `thunder()` is ready but unused).
  - 0x390F20 (render) draws the tint when 0x15AC != 0. It makes no generator draws.
- **Consequence.** With chance 0, the cost is **exactly 1 draw per game update while S is 0 or 1**. Trace: first
  draw at tick 19, last at 12299, 12281 in total; ticks 12300..12458 have none.
- **Browser.**
  - `web/screen-tint.js` ports only the region tint driver (2C0778 / 2ED490, gp+0x14F4.. values). The lightning state
    machine is not ported.
  - `web/audio-world.js` notes "no weather in the port".

## 2. PS2 order of every visual draw in one race game update (0x2306B8, normal path)

**Tick label.**
- The rider-manager object `*(G+0x84)+0x0C` is also the race clock: +0 phase, +8 total ticks, +0xC race ticks.
- 0x128AF0 increments +8 at its end (0x129134..0x129144). This is the counter `trace_visual_rng.py` reads.
- So, in the trace, the draws of steps 1-4 of update U are labelled T, and the draws of steps 5-11 of the same update
  are labelled T+1.
- Savestate `full.tickT` sits between updates: after the camera of one update, before group 1 of the next. This was
  checked for all 41 states: the last draw before the state is lightning or crowd, and the first after is an emitter.

Steps, with PS2 call sites:

1. **0x230C64 group 1** (0x354F98(gp+0x2898, 1)): all world entities, newest first (0x356198, type-13 0x357950,
   0x3608E8 ...).
   - Script programs run inside it: LiveComp slot 5 via 0x341D48, builtin16 0x2FD420 -> **0x3705E0 -> 0x36CCB8: 9
     seeds per static emitter** (0x36CCE4..0x36CDCC).
   - Per entity, the effect list 0x352D20: 0x345B40 Particle (no draw); **0x345F90 DynamicParticle -> 0x3710D0: 1 draw
     per active emitter** (ra 0x3711D0); TexFlip 0x35F410 (not used on these courses).
   - Skipped when gp-0x6A0 & 2 is set. The same gate covers steps 2 and 3.
2. **0x230C70 group 5, 0x230C7C group 6** (0x244948). No draws.
3. **0x230C98 0x355028(gp+0x2898, 1).** Group-1 slot +0x1C, gated. No draws.
4. **0x230CB0 rider manager 0x128AF0** (jalr through the vtable at `*(s1+0xC)+0xCC` +0x18/+0x1C). Each pass runs over
   **all riders in slot order** (human = slot 0, then the computer riders in slot order; checked: 12239 ticks, all
   monotonic):
   - 0x12BB20, 0x113C20 (race clock), 0x10F560, then the controller and physics passes 0x120E30..0x1217F8. The audio
     draws happen in this part: rand15 0x2ADF60 (ra 0x2ADF70) and 0x2A4CA0 (ra 0x2A4CC4), 33 in the run.
   - **0x128EA4 0x121818 for each rider: the selected contact -> 0x30A060 slot-2 stage programs:**
     - builtin13 MeshAnim 0x352230: **9 draws per node**, N x 4 (ra 0x3522B8/E0/300/31C) then N x 5
       (0x3523C0..0x352444);
     - builtin26 0x370DC8 (9 + 1 seeds);
     - builtin16 0x3705E0 (9 seeds).
   - 0x1218D0, 0x121950.
   - **FX passes, each over all riders:**
     - 0x2DD0B8 (+0x3B0), 0x2DABC8 (+0x470);
     - **0x2E8938 (+0x520) board sparks: grind-chunk emitter -> 0x3710D0, 1 draw**;
     - 0x2E66B8 (+0x610), 0x2EADD0 (+0x9C0), 0x2EF6D0 (+0xAD0), 0x2D4C08 (+0xAF0), 0x2E39D8 (+0xB00);
     - **0x2DF920 (+0xB40) snow FX: 1 draw per enabled emitter** (ra 0x3711D0; the human's emitters, then each
       computer rider's);
     - 0x2F1150 (+0xC70) fist sparkle, 0x2F6518 (+0xD20), 0x120E88.
   - **0x129124 section pass 0x101B60** -> 0x30A3A0 -> 0x30A298 slot-1 programs, in section order:
     - flag entity construction 0x34C548 -> 0x34B038 -> **0x34B228: 4 draws per new flag slot** (ra 0x34B288);
     - set-piece emitter seeds 0x36CCB8 and 0x370DC8.
   - 0x1013A8, then the tick counter goes up.
5. **0x230CCC group 2** (skipped when gp-0x6A0 & 0x10):
   - 0x58D400 snowfall 0x2E5DA0: no draw. It feeds the splash's pending count.
   - **Flag manager 0x34C668: 1 draw when its 1 s wind timer wraps** (ra 0x34C71C; 207 in 12440 ticks).
   - **Camera splash 0x2F39E0 x 2: 1 draw** (camera-0 object only; + spawn and crystal draws, see 1a).
6. **0x230CE8 group 3** (skipped when gp-0x6A0 & 4):
   - 0x5439E0 -> environment 0x2F0A98 -> components 0..16 -> **component 7 ScreenTint 0x390C60: 1 draw (+1 on a
     strike)**. Then 0x2F00A0 for each camera, which refreshes the enable and the chance.
   - 0x58D000 (0x2EA900), 0x5AAF00 and 0x5AB800 (0x2E46C8). No draws.
7. **0x230CF0 WScriptMan 0x309270.** Global scripts. They can reach 0x390EC8 (a scripted strike: no draw), builtins
   with seeds, and 0x343C60 via 0x303B38. None drew in this run.
8. **0x230CF8 CrowdMan2d 0x2294C8 -> 0x229530.**
   - For each registered slot (0..127, stride 0x40, slot word != -1): countdown(+0x240) -= 10*|cheer| + 4. The cheer
     comes from audio 0x2A77C8(gp+0x410).
   - If the countdown <= 0: **draws c1 (ra 0x22961C) and c2 (ra 0x229638)** in [-1, 1). A flash spawns at centre +
     axis1*c1 + axis2*c2.
   - **Re-arm draw (ra 0x2296D0): 300 + r % 300.** In total, 3 draws per expiry.
9. **0x230D00 CrowdMan3d 0x3440C8, 0x230D08 MultiParticleMan 0x357BF8, 0x230D14 0x354C98 (flush).**
   - No draws: the MultiParticle static emitters use the VU generator.
10. **0x230D50 group 4** (0x2449F8 -> 0x2D21B0). No draws.
11. **0x230D7C cameras 0x22E840** (unless S == 3). For each camera: vtable at `cam+0x90`, slot +0x1C = **0x15DF98**
    -> 0x15E668 -> 0x15E460:
    - **0x1656B0: 12 draws when a shake starts** (ra 0x165708..0x1658C0);
    - **0x165938: 6 draws per shaking tick** (ra 0x165A54..0x165BAC).
12. **0x230DC0 onwards.**
    - 0x2B7848 audio emitters, each rider's +0x98, 0x237948, 0x3F42A0, HUD 0x1A39F0 / 0x1E1458 / 0x1F3188: no draws in
      the race.
    - **The 0x230D24 branch** runs when s0 = 1 (0x270280(state) true, 0x2379C8 == 0 or 0x26CE50 != 0; probably the
      frozen or pause path, not verified). It runs group 5, **group 3 (lightning still draws)**, group 4 and the
      cameras, and skips groups 1, 2, 6, the riders, WScript and the crowd.

**Render side.** No race draws happen at render time.
- The render slots (0x355028 group render, ScreenTint 0x390F20, splash 0x2F3E28, environment 0x2F0CD0) do not call
  0x3177F0.
- Every other caller of 0x3177F0 in the ELF is a load-time, menu, career or replay consumer that did not draw in the
  race:
  - 0x159CD0 awards and 0x1E1550..0x1E3100 career messages;
  - 0x160028 / 0x160130 camera-director nodes;
  - 0x2E4D88 snowfall set-up, 0x27B0C0, 0x2797A0, 0x2D66A0, 0x2EBE20;
  - 0x2292E0: the 128 crowd timers at the load.
- Every race draw sits inside the game update above. The generator advances only once per game tick, never once per
  displayed frame.

**Measured rates** (Snow Jam, 5 computer riders; `verify.out`, 12002 ticks from 418 to 12420):

| Consumer | Draws | Per tick |
|---|---|---|
| Emitters (0x3710D0) | 45033 | 3.75 |
| – human (snow + grind) | | 1.66 |
| – computer riders | | 1.96 |
| – DynamicParticle | | 0.13 |
| Splash | 12002 | 1.00 |
| Lightning | 11881 | 1.00 (until the finish) |
| Crowd | 4512 | 0.38 |
| Emitter seeds | 918 | 0.08 |
| MeshAnim | 405 | 0.03 |
| Camera shake | 356 + 36 | |
| Flag wind | 200 | 1 per 60 ticks |
| Flag grid | 24 | |
| Audio | 33 | |
| **All** | | **about 6.3** (4.7 to 9.5 per 400-tick window) |

## 3. Browser order today (one `simTick`, web/main.js:137) and the gaps

The shared stream is the human core's `snowParticleRandom` (web/animation_bridge.cpp:376). Its users:
`browser_camera_random()` (:391), `stage_visual_random()` (web/stage_world.inc:84) and `impact_fx_gameplay.inc:48`.

| # | Browser call | Draws on the stream | PS2 step |
|---|---|---|---|
| B1 | `aiRace.beginTick()` | – | (0x10F560 inside 4) |
| B2 | `core._race_begin()` -> `advance_world_entities` (roller_gameplay.inc:46) -> `browser_stage_entities_tick` (stage_script_gameplay.inc) -> `stage_world_tick_entities` (stage_world.inc:600: DynamicParticle updates; slot-5/4 programs -> seeds at :153); `stage_multiparticle_tick` (no draws) | DynamicParticle emitters, group-1 seeds | 1 ✓ |
| B3 | `core._step_rider` | – | 4 (physics) |
| B4 | `core._animation_tick` -> animation_bridge.cpp:904 FX pass: `update_board_sparks` (grind chunk, :48), `update_trail` (LCG only), `update_snow` (:434), fist sparkle, boost | human grind + snow emitters | 4 FX passes (human only) |
| B5 | `core._race_end` (race_bridge.cpp:59): AI stage2 (their FX in their own streams), then `browser_stage_triggers` (human 121818: MeshAnim stage_world.inc:351, builtin16/26 seeds) | contact-program draws | 4, 0x121818 |
| B6 | `aiRace.endTick()`: computer riders' `race_end` (their 121818); `world_event_apply` case 6 replays their stage contacts into the human core (shared_world.inc:50); `_race_world_pass`; **`_section_pass`** (non-AI: main.js `core._section_pass`) | replayed contact draws, section seeds | 4, 0x121818 of slots 1-5, then 0x101B60 |
| B7 | `core._step_camera_head` -> core.cpp:419 `input.visualRandom` -> original_camera.hpp:1148 | 0x1656B0 / 0x165938 | 11 |
| B8 | `screenTint.step` | – | – |
| JS, per rendered frame | `setPieceRenderer.update` -> FlagAnimation (Math.random: wind, grid build), crowd-2d `CrowdFlashes` (xorshift), sfx/audio (Math.random) | not on the stream | 4 / 5 / 8 |

### Gaps and misorderings (in PS2 order)

1. **G1: contact programs run after the FX pass (misorder).**
   - PS2: the 0x121818 of every rider (MeshAnim 9/node, builtin26/16 seeds) runs at 0x128EA4, before any FX pass.
   - Browser: the human's contacts run in `race_end` (B5), after the human's snow and grind draws (B4). The computer
     riders' contacts (B6) come later still.
   - Fix: take the FX pass out of `animation_tick`. Export it (e.g. `fx_pass()`, which runs
     `fx_record/update_pickups/update_board_sparks/update_trail/update_snow/update_fist_sparkle/update_boost_fx`) and
     call it after every rider's 121818 and before `_section_pass`:
     - in ai-racers `endTick`, between the computer riders' `race_end` loop and `_race_world_pass`/`_section_pass`;
     - in main.js without computer riders, between `_race_end` and `_section_pass`.
   - Check first that nothing between B4 and B5 reads the FX outputs.
2. **G2: the computer riders' emitter and grind draws are missing** (about 2 per tick).
   - PS2: 0x2E8938 runs for all riders, then 0x2DF920 for all riders, in slot order, on the one stream.
   - Fix: pass the six words between cores, as `onShared` does for 0x4FF030. Use `visual_rng_words()` and add a
     setter. Run the FX passes phase by phase:
     - `board_sparks` human, then slots 1-5;
     - `snow` human, then slots 1-5.
   - This needs `update_board_sparks` and `update_snow` exported separately, or `fx_pass(phase)`.
   - The gp+0xA0C LCG (`trailVisualRandom`) is also one shared word on the PS2 and needs the same treatment.
3. **G3: the flag grid build is not on the stream.**
   - PS2: 0x34B228 makes 4 draws when a flag's section activates and a new cloth slot is allocated, inside 0x101B60
     at the flag instance's slot-1 program.
   - Browser: JS `FlagAnimation.activate` with Math.random, applied per rendered frame from the section log.
   - Fix: run the slot allocation of engine/flag_cloth.hpp `OriginalFlagManager` (0x34C548/0x34B038/0x34B168) in the
     core section pass. Draw the 4 phases from `stage_visual_random()` at that point, and put the words in the section
     log (it already carries `draws, word`). The JS then activates with `random = () => logged[k++]`.
4. **G4: the post-rider world pass is missing.** Insert one core export, e.g. `world_post_rider_pass()`. Call it from
   ai-racers `endTick` after `_section_pass`, and from main.js after `core._section_pass`, **before
   `_step_camera_head`**. In this order:
   1. **Flag wind 0x34C668**: `originalFlagWindTick(wind, mode, 60, stage_visual_random)` (flag_cloth.hpp:313). It
      draws once when the timer wraps. Export the wind for flag-animation.js instead of its own `flagWindTick`.
   2. **Camera splash 0x2F39E0**: 1 draw per tick while Enable (1) and a camera exists. Port the drop and crystal model
      only with weather, when snowfall is above 1.5 or impact snowfall above 1.0 (not reached on the sampled courses).
      Keep the speed/prev state so the spawn rules can be added later.
   3. **ScreenTint lightning 0x390C60**:
      - gate: game-flow word S in {0, 1}, which is the countdown and the race; it turns off at EndRace, about 2 ticks
        after the finish (still to be measured exactly);
      - 1 draw against chance² (chance = 0 on every sampled course);
      - a second draw and the flash/thunder only when chance > 0 or a script strikes.
      - This belongs in screen-tint.js / environment_bridge as the intensity/colour overlay (0x390F20).
   4. **WScriptMan scripts**: their draws, if any, go here.
   5. **CrowdMan2d 0x229530**: for each registered slot in index order, countdown -= 10*|cheer| + 4; on expiry, draw
      c1, c2, then 300 + r % 300.
      - Move `CrowdFlashes.step` into the core. It needs the cheer (audio 0x2A77C8) passed in before the tick, and the
        slot registrations (`stage_world_crowd`).
      - The 128 load-time arms (0x2292E0) come before the seed savestate. The countdowns must come from the savestate,
        not be re-armed from the stream.
   6. **Camera** (`_step_camera_head`, already at the right place).
5. **G5: audio (0x2ADF60 rand15, 0x2A4CA0) uses Math.random** in web/game-audio.js, sfx-game.js and audio-crowd.js.
   - PS2 draws happen in the rider-manager passes (step 4). They are rare (33 in 12440 ticks) but shift everything
     after them.
   - A fix needs the audio triggers inside the core tick, or a two-phase exchange.
6. **G6: the frozen/pause branch (0x230D24)** keeps drawing the lightning (group 3) and running the cameras while
   groups 1, 2, riders and crowd are frozen. The browser stops the simulation on pause. This only matters when
   pausing; the path still needs confirming.
7. **G7: seed provenance.**
   - The stream starts from `original_snow.particle_random_state`:
     - ARA1 w5 0x6FF7695A, which is 7 draws before snow-jam-glide-1;
     - BRA2 0x6FF76DA0.
   - The set-piece tick-0 words come from the ready savestate: ARA1 0x6FF762A8 (1714 draws earlier), BRA2 0x6FF761D9.
   - Once every consumer draws, the start words must be the 0x4FF018 of the savestate boundary where the browser's
     first update begins: after the previous camera, before group 1. The crowd countdowns, the lightning state and the
     flag wind must come from the same state.

Order that is already correct:
- group-1 DynamicParticle and slot-5 seeds, before the rider (B2);
- the human's snow emitters after the grind chunk (B4);
- the section pass after every rider (B6);
- the camera last (B7).

## 4. Verification against PS2 savestate pairs

`verify.py`, run on `local/ps2-capture/runs/setpieces/full.tick*.p2s` (45 states, ticks 418 to 12668). For each
consecutive pair:
- n = Δw5 (word 5 is the counter);
- stepping `OriginalRandomState::next` n times from state A gives state B exactly: **44 of 44 pairs** (41 in the
  trace range plus 3 after it);
- state A is found in the trace by its w5, and draws[A:B] are grouped by caller.

Results:
- All 41 in-range pairs decompose into the consumers above.
- **splash = Δtick exactly** in every interval (e.g. 418 -> 819: 401; 12018 -> 12348: 330).
- **lightning = Δtick until 12299** (12018 -> 12348: 281; 12348 -> 12420: 0).
- Example window 6418 -> 6818 (400 ticks, 1878 draws): emitters 954, splash 400, lightning 400, crowd 117, flag
  wind 7.
- Windows with set pieces add seeds (9 per emitter), MeshAnim (9 per node) and the flag grid (8 = 2 slots x 4).
- The trace and the states agree draw for draw: every state's w5 is found in the trace, and the last draw before each
  state is the tick-tail consumer (lightning or crowd).
