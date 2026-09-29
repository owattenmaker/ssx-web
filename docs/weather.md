# Weather: wind, snowfall, blowing snow, lens snow, lightning

Owner: the weather agent (2026-09-25). Source: SLUS_207.72 disassembly (`dis.pkl`, `local/output`), PS2 captures in
`local/ps2-capture/runs/weather/`. Code: `engine/wind_push.hpp`, `engine/weather.hpp`, `web/weather.inc`,
`web/weather-renderer.js`, `tools/export_weather.py`.

## 1. The data: world painter type 12, tWPIGD_Weather

Every SDB location's world painter record (SSB kind 15) has a type-12 section (painter class names at 0x483CB8:
MusicTrigger, Mix, Ambience, Speech, Camera, Fog, LightGlow, ScreenTint, SkyBox, Sun, Surface, Lighting, **Weather**,
Danger): a point tree over the location and payloads of `{transition, 19 values}` (class 0x484058, current/target pairs
at +0x08.., reset 2BE108 = the defaults below). The painter is stepped by the world painter driver 2C0778 (blend
weight² per update with a negative rate, `engine/environment_transition.hpp`) in every **environment block**
0x4FA370 + i×0xF0 (+0x20 = the Weather wrapper): blocks 0..5 = riders (2ED490 from 0x1218D0, the rider's contact point
+0x460/+0x464), 6/7 = cameras (2ED490 from 0x15EBCC, the camera eye +0x20/+0x24, after the camera update).

| # | vtable getter | meaning | readers |
|---|---|---|---|
| 0 | 0x158 (2EE448) | snowfall intensity | snowfall flakes (count ×500), camera splash (> 1.5 spawns) |
| 1 | 0x160 (2EE4C0) | flake size (world cm, sprite half size) | flakes |
| 2 | 0x168 (2EE508) | snowfall wind (gust speed ×500 cm/s) | flakes |
| 3 | 0x170 (2EE570) | wind direction, degrees | rider wind push, snowfall gusts |
| 4 | 0x178 (2EF0A0) | wind speed, km/h | **rider wind push 0x125970** |
| 5 | 0x180 (2EE6F0) | breath | RiderBreath 2E1120 |
| 6 | 0x188 | flurries (gust randomness 0..1) | snowfall gusts |
| 7 | 0x190 | gravity multiplier | snowfall fall speed |
| 8 | 0x198 (2EE600) | fluff intensity (puffs ×6) | fluff ("blowing snow" puffs) |
| 9 | 0x1A0 | fluff wind (×250 cm/s) | fluff |
| 10 | 0x1A8 (2EE738) | lightning chance | ScreenTint lightning 0x390C60 (via 0x2F00A0) |
| 11..14 | 0x1B0.. | flake A, R, G, B | flakes |
| 15..18 | 0x1D0.. | fluff A, R, G, B | fluff |

Defaults (no payload): 0, 6, 0, 0, 0, 10, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0.06, 1, 1, 1.

Per course (max over the payloads; `tools/export_weather.py` prints them):

| Course | Wind km/h (dir) | Snowfall | Lightning | Notes |
|---|---|---|---|---|
| Peak 1: ARA1, BRA2, ABC1, ASS1, ABA1, BHP1, hubs | 0 | 0.3 / 0.4 / 0.15 / 0.3 / 0 / 0 | 0 | light snow areas only |
| CRA3, CBA2, CHP2, C, D, connectors | 0 | 0 | 0 | |
| DRA4 / DSS2 | 0 | 0.1 / 0.3 | 0 | |
| **DBC2 Ruthless** | **40 (90°)**, 8 (186°) | **6.0** (2.0 around) | 0 | the blizzard: pushes, lens snow, 3000 flakes a layer |
| ERA5 Gravitude | 8 (48°) | 3.0 | 0 | below the 10 km/h push gate; lens snow |
| EHP3 Perpendiculous | 7.5 | 2.2 | 0 | lens snow |
| **EBC3 The Throne** | **14.5 (300°)**, 6 (270°) | 0.76 | 0 | pushes in payload 1 (reached ~7650 on the tuck line) |
| ESS3 Kick Doubt | 0 | 0.68 | **0.02** | lightning in one area |
| **EBA3 Much-2-Much** | 0 | 0 | **0.06** | lightning everywhere (a strike every ~280 ticks) |
| ERA5_C, E_EHP3 | 8 / 6.5 | 3.0 / 2.2 | 0 | |

## 2. Wind as gameplay: 0x125970

The first call of the ground motion 0x13D818 and of the air motion 0x139A20 (also motion 1 under the handplant):

```
if rider+0x874 (human flag, set by the human rider ctor 0x125C70; computer riders 0):
  speed = min(kmh × 27.777779, 1388.8889)           // Weather property 4 of the rider's block (rider+0x86C)
  if 277.77777 <= speed:                              // 10 km/h
    i = trunc(deg × 0.017453294 × 81.48733) & 0x1FF   // property 3
    dir = (sin[i + 0x80], sin[i], 0, 0)               // table 0x504FB8 (engine/flag_cloth.hpp)
    f = speed − dot(v, dir)                           // VU dot, EE sub
    if 0 < f: rider vtable +0x20 = 0x1250A8(dir × f × 0.015): v += dv; in the air (11FE98 == 1) 0x1135B8 restarts the predictor
```

It reads the rider painter as the previous tick's 1218D0 left it. **The port had none of it** (ai-racers.md listed it as
"not ported, 0 in these captures"): DBC2 left the PS2 at 2074 (1 ulp, the first push of the blizzard's rising wind).
Now: `engine/wind_push.hpp` (`originalWindPush`), `web/core.cpp browser_wind_push` before `originalGroundIntegrate` and
before the air translation (and in the air-switch redo), `web/handplant_gameplay.inc`; the rider's Weather painter
(`breathEnvironment`, RIDER_LOCAL) is stepped every rider-pass tick in `update_trail` next to the Lighting wrapper
(`weather_painter_step`), no longer inside `update_snow` (which returns early on some surfaces). Online races: each
client simulates its own rider, so the push is local and deterministic.

**Painter resets 0x2C03E8.** 0x111890 (a rider's FX reset: reset placement 11D660, location entry 11DE60, teleports
1234A4 / 123598 / 1238AC) sets +0 = -99999 on **every** painter wrapper of the global list gp+0x790, so each wrapper's
next step blends with −1 (jumps to its payload): a computer rider's reset re-seeds the human's painters. Ported:
`weather_rider_fx_reset` (reset placement and the mission teleport) resets the rider and camera Weather painters, Fog and
Lighting (`browser_environment_painters_reset`), the human's camera splash, and logs shared-world event 8 so the other
rider cores do the same (`web/shared_world.inc`). The rolling start (backcountry) places the riders the same way. This is
also the "unknown trigger" of the Metro-City Lighting re-seed at 1551 (rider-lighting notes). JS painters (ScreenTint)
follow `weather_info()[46]`.

The two other callers of 0x2C03E8 (2026-09-26):
- **Fades: 0x2E47E8**, the render of the camera fade object (vtable 0x488158: ctor 0x2E4228, start 0x2E4370, stop
  0x2E4578, update 0x2E46C8 in group 3). For the camera being drawn (+0x74), while the fade is active (+0x44) its opacity
  (elapsed +0x48 against out +0x54 / hold +0x58 / in +0x5C; the +0x4C / +0x50 effect branches) is clamped to [0, 1] and
  **at opacity >= 0.93 (gp-0x3A1C) every painter is reset, on every rendered frame** (the painters then jump at their next
  step). In the simulation the fade is the rider reset's: 12F230 → 2E4370(out 0.5 s, hold 0, in 0.5 s) with its clock bound
  to the reset control's progress (12F398, +0.025 a tick), only for a rider with a camera (rider+0x870: the human). Its
  opacity is `engine/reset_fade.hpp originalResetFadeAlpha`, >= 0.93 on the ticks of progress 0.475, 0.5 and 0.525
  (ticks 19, 20 and 21 after the request: the two before the placement and the placement tick itself). Ported:
  `web/weather.inc weather_fade_render` after each camera update (`browser_weather_camera_step`, the frame's render),
  shared-world event 8 for the computer riders' painters. The world's other fades (NIS step fades 0x277980, the world-state 15
  white fade 0x236058) are the page's (`web/cutscenes.js`): `fadePainterReset` calls the core's `weather_fade_reset` on every
  drawn frame at opacity >= 0.93 (real-time, like those fades themselves). `web/test-weather.mjs` 4.
- **0x26DBF0 is not the world load.** It restores a saved world snapshot: 0x18 bytes of 0x4FF018 (0x317908), the entity id
  counters gp+0x2A88 / +0x2A8C, 0x2C03E8, then every component from the stream (14DFA8, 2D9D68, WScript 30BD20, CrowdMan2d
  229E58, 344240, 357D28, the entity groups 26DDC0 / 26E340 / 26DE58, 103578, 12B788, the cameras 22E840). Its callers:
  the **replay** (26F8A0 saves the live world into +0x3D0 when a replay starts, from the results / pause overlays
  1FDEE8 / 20CE38; 26F980 → 26F850 → 26DB88 restores it when the replay ends, 20E184 / 20E214; 270478 restores the replay's
  start during playback) and the checkpoint restore 26CDF8 → 26CD20 / 26CDD0 → 26CC18 of the Big Challenge prompts, which is
  unreachable on the disc: its snapshot flag +0x70 is set only by pending op 5 (26CC48), and only 30B590 posts op 5 — a
  function nothing calls. A real world load builds fresh wrappers (property ctor 2BCBD0: +0 = -99999, gp-0x42AC), which the
  port's default `OriginalEnvironmentTransition` already is; the free-ride placement (11DE60 → 11D660 → 0x111890) resets them
  too. **The port has no replay** (the results menu's Replay is greyed in `web/ui.js` / `career-ui.js`), so nothing reaches
  26DBF0; a future replay must call `weather_painters_reset` (and restore the visual stream) when it restores the live world.

**PS2 match.** `peak2/dbc2-race-tuck`: human exact through **3910** (was 2073), Nate through 6425 (was 2852), RNG through
3932 (was 2200); 3911 was the PS2 crashing out of a control-3 soft collision (control 8) where the browser stayed in
control 3 (not wind: payload 0 there). Fixed 2026-09-27: the surface-18 crash of 13F178 (docs/crash-motion.md), and the
capture is now exact through 4435 / 5141 / 4581. `weather/dbc2-weather` (new capture, same pad, `--ai-state` + watches on the
human's Weather painter, the camera block 6 painter, the six layers, the splash, 0x4FF018, gp+0xA0C): human, Nate, RNG,
ranks and pair records exact on all 3299 ticks with 519 pushes from 2074, the rider and camera painters' 19 properties
and the layer counts / splash snowfall exact on every tick, including Nate's reset placement at 1656 (painter re-seed).
Instruction oracle `tools/test_weather_native.py`: 0x125970 200,000 cases (81,052 pushes), 0 mismatches.

Other courses: Peak 1 has no wind; ERA5 / EHP3 / ERA5_C / E_EHP3 stay under the 10 km/h gate (no push). **EBC3's 14.5 km/h
area (payload 1, 300°) is gated since 2026-09-26:** `weather/ebc3-wind-rail2` (The Throne Rival Time from the-throne-ready,
`--ai-state`, the weather watches; an autopilot line, scripts/wx-ebc3-rail2.json: the first 4010 frames, a hard right so the
rider takes a soft collision instead of the 4024 rail, then the autopilot, braking for 300 ticks 250 ticks into the area).
The human is in payload 1 on 6293..7900 (the rider painter's wind >= 10 km/h on 6475..7957) and **stays bit-exact on all 7999
ticks through 324 pushes (6606..6929)**, with both Weather painters (values and the rider's distance), the layer counts and the
splash counts exact on every tick; a reset at 4136 (fade painter resets 4155-4157) included. Psymon leaves at 2941. Earlier
autopilot lines left at 1007 (a crash whose +0x460 jump the port lacked then) and 4096 (the rail landing).

## 3. Snowfall 0x2E5920 / 0x2E5DA0 and its layers

The snowfall object (vtable 0x487FD8, group 2 first) builds per camera 4 flake layers (kind 0, extents 1500, 1500, 2000,
3000; gravity −200) and 2 fluff layers (kind 1, extent 2000, half size 600 cm, gravity −100); 0x2E4D88 draws 3 visual RNG
seeds each at the load. Each update (0x2E5DA0): the camera block's record (16 floats from the getters above; shelter =
block +0x24), 0x2F4330 into each camera's splash, then 0x2E4F50 per layer: count/speed/size/colour from the record, the
gust timer (4 draws of the visual LCG gp+0xA0C at expiry: a new gust around the wind direction, flurries mix, 1..5 s), the
offset += lerp(gusts) × dt + (0, 0, gravity × rec[7]) × dt, wrapped into ±extent/2. The render 0x2E6008 moves every layer
by −(camera movement) (0x2E5430) and draws through VU1 program 5 (0x000 flakes, 0x408 fluff): flake k = the (k+2)-th
RNEXT of each seed + offset/extent − 1.5, wrapped into the box [forward×0.6 − 0.5, +0.5) (camera+0x80 forward), placed
at eye + n × extent; a screen-aligned sprite of half size = the layer size (row 2 = the view's projection scale, so a world
radius), FX 8 'sfal', RGB × 128, GS ALPHA 0x44 (alpha blend), priority 7. Fluff fades by z' = depth/extent (× 3.33 z' below
0.3, 1 − 3.33 (z' − 0.8) above 0.8).

Port: `engine/weather.hpp` (`originalWeatherRecord`, `originalSnowfallLayerUpdate`, `originalSnowfallCameraShift`),
`web/weather.inc` (group 2 in `stage_world_visual_pass`, the camera block stepped in `_step_camera_head`), the ready
state from `weather.json`. Renderer `web/weather-renderer.js`: one instanced mesh per layer, the LFSR positions in a
static attribute, the wrap / billboard in the vertex shader (no per-flake CPU work), registered as an encoded effect
(after the fog composite, unfogged, `web/snow-composite.js`). Oracles: 0x2E4F50 / 0x2E5430 200,000 cases exact;
VU1 program 5 run on synthetic uploads (`tools/test_snowfall_vu_native.py`, 40 layers, 6230 sprites) = the renderer's
positions and sizes (`web/test-weather.mjs`).

## 4. Camera splash 0x2F39E0 (snow and ice on the lens)

Group 2 after the flag manager, camera-0 object (vtable 0x488230). Speed = |camera +0x20 − previous| × 60 × 0.036 km/h
(reset above 1000); pending += snowfall × (speed + 10) × 0.015/130 when snowfall > 1.5, + impacts (0x2F4260 from the
snow FX's large impacts 0x2E162C, < 450 cm, snowfall > 1.0); 0x2F3810 spawns drops (3 LCG draws each + 0x2F2598) or,
60%, a crystal group (one 0x4FF018 draw for the group size 1..2, 0x2F3640/0x2F2D70); drops 0x2F2810 and crystals
0x2F3030 jiggle, grow and get pushed off the screen centre by speed; one unconditional 0x4FF018 draw picks a crystal
that sheds a drop (second draw when < 1%). Render 0x2F3E28: 640×480 rotated sprites ('ices' drops, 'icel' crystals),
alpha × (1 − (age/life)²), priority 8. Tweakables gp+0x115C.. equal the ELF's .sdata (oracle). Port: `engine/weather.hpp`
(`originalSplashUpdate` ...), 60,000 oracle cases (33,676 with crystal draws, 128,915 visual draws) exact.

## 5. Lightning 0x390C60

With chance > 0 (EBA3, ESS3): per update while the game flow allows (0x151C), one 0x4FF018 draw against chance²; on a
hit a second draw gives the distance (0.02..2 km), the flash runs 2/2/2/7 ticks (0.4 × c/2, 0.4, 1.0, fading 0.5) in
colours (1.584, 1.905, 1.998), (0.333, 0.43, 1.99), (0.75, 0.75, 2.0), with no draws while it runs; the thunder waits
distance × 0.0018072 ticks (audio poll 0x390EF8 sets −1). Render 0x390F20: a full-screen untextured sprite, RGB
trunc(I × colour × 255) & 255, A trunc(I × 255), blend mode 5 = GS ALPHA 0x44 ((Cs − Cd) × As >> 7 + Cd, As 255 ≈ ×2).
PS2 `weather/eba3-lightning` (Much-2-Much from the 1620 state, watches gp+0x15A8..): a strike at 1898 (29,485 cm,
delay 53), counter/intensity/colours as ported; PS2 frames 1901/1904/1905/1908 match the blend model within a few levels
(e.g. (59,100,121) → (138,176,190) at I 0.4). Port: `web/weather.inc` (state, strike, thunder poll, seed from the ready
state), `web/screen-tint.js` (flash after the tint), `game-audio.js thunder` → audio-world 291438.

## 6. Visual RNG order

The world visual pass (visual-rng-order.md §2 steps 5/6) now runs: snowfall layers (LCG), flag wind, the camera splash
(1 draw + spawn / crystal draws in heavy snow), ScreenTint lightning (1 draw, +1 on a strike, none during a flash), crowd.
Courses without heavy snow or lightning draw exactly as before. Without `weather.json` the old path (timers only) runs.
Known: the DBC2 visual stream itself diverges from tick 1 in the rider FX passes (pre-existing, not weather), and so do the
layers' gust timers / offsets (the LCG); the rolling-start camera, which used to differ from the first camera update, is fixed
(section 12): the camera eye, the camera painter and the splash speed are gated on dbc2-weather. The streamed worlds' flag
manager now draws on the stream too (section 11).

## 7. Quality and cost

No tier changes the PS2 look: the flake counts, sizes and fluff are the original's (`createWeatherRenderer` keeps a
`density` knob, 1 on every tier). The heaviest case is the Ruthless blizzard (race tick 2600: 10,348 flakes in 4 layers,
8 fluff puffs, 18 lens sprites = 8 instanced draws, 20.7k triangles). Measured 2026-09-26 in headless Chrome (WebGPU,
Apple GPU), with a load average of 7-15 from other agents; scripts in `local/browser-validation/weather/`
(`perf_weather.mjs`):

| Tier (3D buffer) | Weather draws alone, GPU (`--bench 200`) | World scene alone, same way | Running game, weather on / off (`--abrt`) |
|---|---|---|---|
| high (1280×960 here) | 0.024 ms | 0.78 ms | 4.2 / 4.4 ms (paired −0.14 ± 0.28) |
| medium (1280×960) | 0.024 ms | 0.65 ms | 4.5 / 4.5 ms (paired −0.09 ± 0.26) |
| low (640×448) | 0.012 ms | 0.66 ms | 5.1 / 5.1 ms (paired −0.15 ± 0.33) |

- "Alone": 200 renders of a scene holding only the weather group (no depth: every fragment drawn), minus an empty
  scene, to `onSubmittedWorkDone`: 2-4 % of the world scene drawn the same way.
- "Running": the game runs in real time from tick 2400 with vsync off. The weather group is hidden and shown every 0.4 s
  (8 pairs); the value is the median rAF interval, 6 render passes a frame either way. The difference is below the noise
  on every tier.
- CPU: renderer update 0.006 ms a frame (the flake positions are a static attribute; the wrap and billboard run in the
  vertex shader), core weather < 0.01 ms a tick.
- Safari was not measured: safaridriver session creation times out, as it does for the other agents (firefox-load.md).
- Shaders: no weather, fog or ScreenTint shader declares a private or function-scope array (iPhone Metal's 8 KB
  private limit). All 60 WGSL modules of a DBC2 race were scanned (`--wgsl`); the only arrays are uniform-buffer ones:
  instance matrices up to `array<mat4x4<f32>, 1024>` (set-piece particle sprites) and the rider's bone palette.

## 8. PS2 / browser frames per course

`local/browser-validation/weather/tri-*.png`: the PS2 snapshot | the browser with `?weather=0` (before) | the browser
(after), headless Chrome driven by the PS2 pad script (`weather_cdp.mjs`). The label gives the distance between the two
riders. On the countdown events the page run drifts from the PS2 line (by 3 m at t1000 on Snow Jam, with or without
computer riders and for any race offset; not traced), so some pairs show the same spot at a later browser tick.

| Course | Weather (PS2) | Before | After | Frames |
|---|---|---|---|---|
| DBC2 Ruthless | blizzard (snowfall to 6, fluff, lens snow), 40 km/h wind | nothing; no push | flakes, puffs, lens drops / crystals; pushes | `tri-dbc2-1700/2300/2600` (exact spot), `-3001` (page run crashed at ~2800, below) |
| ERA5 Gravitude | snowfall 3, lens snow; 8 km/h (no push) | nothing | flakes, lens snow | `tri-era5-1218` (3 m), `-1619` |
| EBC3 The Throne | snowfall 0.7-0.76; 14.5 km/h wind area | nothing; no push | flakes, fluff; 148 pushes by t8801 | `tri-ebc3-3202/8400/8801` (10-28 m) |
| EHP3 Perpendiculous | snowfall 1.6-2.2, lens snow | nothing | flakes | `tri-ehp3-418/4018` |
| EBA3 Much-2-Much | lightning 0.06 | no flash | flash + thunder | `tri-eba3-lightning`, `eba3-ps2-flash-phases` (PS2 1901/1904/1905/1908) |
| ARA1 Snow Jam, ASS1 R&B | snowfall 0.3 areas | nothing | ~600 flakes | `tri-ara1-5218/6018`, `tri-ass1-3224` |
| DSS2 Style Mile, DRA4 Intimidator | snowfall 0.15 / 0.05 | nothing | ~300 / ~100 flakes | `tri-dss2-2818/3218`, `tri-dra4-818` |

Not framed: ESS3 Kick Doubt (snowfall 0.68 and lightning 0.02 lie off the captured line), BRA2 / ABC1 (their snow
areas are off the captured lines). CRA3, CBA2 and CHP2 have no weather. The fog is the existing Fog painter port
(fog-painter-recovery.md). This work only adds its re-seed at 0x2C03E8, and the frames show the same distance fog.

**PS2 captures of 2026-09-26** (`local/ps2-capture/runs/weather/`; the ARMSX2 run logs and tick PNGs beside them): ebc3-wind-rail2
(The Throne wind, `.tick6700.png` during the pushes), frd-regions (Peak 2 free ride, regions + flag manager), era5-reset
(Gravitude resets), ess3-weather / ess3-lightning (Kick Doubt's payload 1 with the time limit poked to 4:00: GMM+0x78 = 0x59FE78
:= 14400; `.tick7236.png` / `.tick9018.png` in the area). ESS3 payload 1 (snowfall 0.68, lightning chance 0.02) covers the
course's far east (ESS3 patches at source x −238610..−181743): 60 s plus the two checkpoints (+70 s at 1349.6 m to go, +60 s
at 654 m) never reached it on an exact line. ess3-weather: physics exact on all 9999 ticks through the finish (control 10 at
9863); payload 1 from 6922; the lightning chance (camera property 10 → gp+0x1524) > 0 from 6932, 0.02 from 7401; no strike.
ess3-lightning (the same line, then standing in the area for 6000 ticks): PS2 strikes at 8266 (176604 cm, thunder after 319
ticks), 8581 (126396 cm, 228) and 12491 (27796 cm, 50) — delay = distance × 0.0018072 each; with the synced visual stream the
port strikes at 8581 and 12491 exactly (tick, distance, delay) and misses 8266, where the PS2 draws 6 words before the lightning
draw and the port 3 (the rider-FX stream gap of section 6). Kick Doubt's area was framed only on the PS2 (the PNGs above).

## 9. Tests and tools

- `web/test-ps2-captures.mjs`: `peak2/dbc2-race-tuck` (3910 / 6425 / 3932), `weather/dbc2-weather` (`--weather`
  field gate: rider / camera painters incl. the camera eye and distance since the rolling-start camera fix, layer counts, splash
  snowfall on every tick), `weather/ebc3-wind-rail2` (human 7999 through the pushes, painters, counts), `weather/frd-regions`
  (Peak 2 free ride: the rider painter by region track, camera painter values, flag timer, the first flag wind draw),
  `weather/era5-reset` (the fade resets, the rider painter through 1938), `weather/ess3-weather` / `weather/ess3-lightning`
  (Kick Doubt's area: rider painter through 6616, lightning chance through 7706, lightning state through 8265 with the synced
  stream). The physics comparer now has `--weather MAP` (rider / camera painters, flag manager `flags`, ScreenTint lightning
  `lightning`), `--time-limit N` (a poked GMM+0x78) and `--sync-visual-rng` on the streamed worlds; physics cases take
  `weatherExact` / `weatherThrough` / `weatherFields`.
- `web/test-weather.mjs` 4-6: the fade resets on reset ticks 19-21, a located record = the single package, a missing record =
  the class defaults, the streamed flag manager's fresh wind and course modes. `web/test-course-spawn.mjs`: every character
  grounded at the grid row of the three rolling-start courses.
- `web/test-weather.mjs` (npm test): packages for 62 locations, VU1 program 5 fixture, the core loading every course.
- `tools/test_weather_native.py` (wind push, layers, splash, tweakables), `tools/test_snowfall_vu_native.py`.
- `tools/export_weather.py [--location X] [--peaks] [--state P --json]`; `web/compare-ai-capture.mjs --weather MAP.json`
  (`WEATHER_TRACE=N`); `CORE_JS=` for private cores.

## 10. Open

- ~~DBC2 3911: control-3 soft collision → crash (control 8) on the PS2, not in the browser~~ fixed 2026-09-27 (the surface-18 crash, docs/crash-motion.md); next open difference: 4436.
- ~~The browser game's DBC2 run leaves the PS2 at ~2650~~ fixed 2026-09-26 (docs/backcountry.md "Rival computer rider").
- dbc2-weather 1657: the PS2's camera-0 splash has speed 0 (its +0x100C / speed reset during tick 1656, Nate's reset
  placement: not 0x111890, which skips a rider with +0x870 < 0, and not a camera cut); pending / crystals / drops follow from
  1658. Cause not found.
- The snowfall layers' gust timers / offsets follow the visual LCG gp+0xA0C, which leaves the PS2 from tick 1 in the rider FX
  passes (section 6); the flag manager's later wind draws the same way (section 11).
- Countdown events (era5-reset): the camera painter differs from the first compared record (the countdown / event camera,
  its eye and distance); the rider painter is exact until a computer rider's reset placement, whose 0x2C03E8 reaches the human's
  painters in the same tick on the PS2 (single-rider comparisons run no computer riders; the six-rider world replays it through
  shared-world event 8 before the human's FX-pass painter step, exact in dbc2-weather at Nate's 1656 placement).
- ~~Fog, ScreenTint, Sun, glare and the rider Lighting painter of the streamed worlds still switch through the page's
  asynchronous region hook~~: they take the record of gp+0x770 on the core's tick since 2026-09-26 (pv regionTick,
  docs/presentation.md section 11; PS2 frd-regions: the camera Sun at 2842).
- ~~EBC3's push / ESS3's area / rolling-start camera / streamed Weather regions / 26DBF0 and fade resets / the flag manager's
  wind~~: sections 2, 11, 12 (2026-09-26). Split-screen camera 1: **the port has no split screen** (Multi Play is greyed,
  web/ui.js), so the camera-1 snowfall layers and splash (which return before any draw in 1P) are not ported.

## 11. Streamed worlds: the painter record of gp+0x770, the flag manager (2026-09-26)

**Painter record selection.** 2C0778 (every wrapper step, riders' blocks 0..5 and the cameras' 6/7) reads the painter
record of the location **gp+0x770**: the track table entry G+0x84 → +0x10 → +8 → +4 + track × 12 must be loaded (short
+0 = 3) with a record (+8, whose +4 > 0) holding a section of the wrapper's type (+0x10 byte), else the property object
takes its class defaults with +0 = 0 (vt+0x228; for Weather 2BE258 = the defaults of section 1) — the same result as a
point outside the tree. gp+0x770 is written only by 2ED490 for block 0 (the human, from 0x1218D0) **after** that block's
nine wrappers have stepped: rider+0x430 & 0xFF, unless +0x430 is -1 (off the terrain: it keeps the old track). So on the
tick the human first touches a new location's patch, its own painters still step in the old record and switch on the next
tick, while the computer riders' blocks and the camera block (0x15EBCC, after the rider manager) switch on the same tick.
(2ED524: with gp+0xA6C set the region comes from the camera instead — a mode no race or free ride sets.)

Port (`web/weather.inc`): `weather_location(track, weather.json)` makes a location's Weather section the record of its
track; `web/peak-world.js` feeds it with the location's collision data (`web/peak-world-prepare.js` fetches weather.json
with terrain / world / rails), so the record is there before any of the location's patches can be touched, and the swap
happens in the core on the PS2's tick instead of when the page's asynchronous fetch lands. `weather_located_step` (both
the rider's Weather painter and the camera's) and `weather_region_update` (after the human's painter step in
`update_trail`, from `browserGroundPatch` = rider+0x430). A new world (`init_weather` without keep_state) drops the
records and sets gp+0x770 to -1; the old region hook in web/main.js (`init_weather` with keep_state) is ignored while
records exist. Course events have no records and keep their one package. Mid-run comparisons seed gp+0x770 from record
0's rider+0x430 (`weather_region_seed`, `web/peak-capture.mjs`), which also feeds every location's weather.json now
(WEATHER=0: the old comparer). Fog, ScreenTint, Sun, glare and the rider Lighting painter take the
same record since pv regionTick (docs/presentation.md section 11).

**Flag manager.** The free-ride world has the same flag manager as an event (group 2, 0x34C668): built with the world
(0x34C428: wind 0, base 0.5, delta 0.25, timer 0, no slots), grid builds (4 draws, 0x34B228) when a flag's section
activates, one wind draw a second — with the mode 0x2D1BA0 reads **every update** from the course table 0x43D950 row
[0x535C08] +0x54 + 1 (amplitude 0.15 for Peak 1, 0.3 Peak 2, 0.45 Peak 3; the row changes at a crossing, 22DF50). The
page used a Math.random stand-in (`web/flag-animation.js` defaultRandom) and the core's visual pass drew nothing for the
flags. Now `init_streamed_flags(SETPIECES/flags.json)` (`web/peak-set-pieces.js`) runs the manager in the core on the one
visual stream (section pass, then group 2 after the snowfall object, before the splash: the same code as a course event,
`web/stage_world.inc`), mode = `stage_flag_course_mode(peak_world_course())`; `stage_world_flag_words` appends each word's
wind mode (0 for grid words) and the JS cloth takes the mode of its next wind word. `stage_flag_wind()` exposes the wind for
captures. The peak comparer loads it too (FLAGS=0: not).

**PS2 check (weather/frd-regions, 2026-09-26).** The Peak 2 free-ride gate line (peak2/fr-d-glide, D → D_DRA4 at the 2842
crossing) re-captured from local/ps2-capture/peak2/frd-1800.p2s with watches on both Weather painters and the flag manager
(+0x10..+0x1F). With the baseline's weather state (`frd-regions.visual-state.json` from `tools/export_weather.py --state`) and
the manager's wind from record 0: physics exact on all 2601 ticks; the rider painter's 19 values and distance exact on every
tick (the web gp+0x770 changes 27 → 28 at 2842 exactly when the PS2's rider+0x430 does); the camera painter's values exact on
every tick (its distance not: the free-ride camera is not seeded); the flag manager's timer exact on every tick and its first
wind draw (1772) exact with the synced visual stream. Offline (every one of the 43 wraps): each new delta is one 0x4FF018 word
of that tick at the mode-2 amplitude 0.3 (−0.3 + 0.6·u, clamped to keep base + delta in [0, 1]) — the Peak 2 course table row.

**The painters' point rider+0x460.** Found with the new captures: 2ED490 steps at rider+0x460/+0x464, the rider's last
contact. The port stepped at `browserTrailContact` (or the crash actor's contact in crash submode 0), which differs in three
cases, now modelled in `update_trail` (`painterPoint`): a crash body keeps its last terrain contact while it flies and after
the crash until the next ground contact (frd-regions 1925: the old point jumped the distance by 181 cm; era5-reset 1139: a
crash into a reset, 3922 cm); a reset placement sets +0x460 = +0x110 after that tick's painter step (era5-reset 1159: the
painters step at the old point on the placement tick) unless a ground contact writes it again first (the rolling start's
placement before tick 0); the countdown grid's contact is the ready state's +0x460 (`weather_event_contact_seed`, the
rider painter's last point: 0.01 cm from the seed position, era5-reset 187). Only the painters use it (the trail, impact FX
and ribbons keep their inputs).

## 12. The rolling-start camera (2026-09-26)

On every rolling-start event (the backcountry rivals: Happiness ABC1, Ruthless DBC2, The Throne EBC3; the Peak 1 Race start) the
browser's camera differed from the PS2's from tick 1 (e.g. dbc2-weather lookAt y −244770.31 vs −244780.67 cm, pitch 0.51963 vs
0.51840), converging over 55..135 ticks, and the posed root (the camera's head input) was 1.2 cm (EBC3) .. 23 cm (DBC2) off on
ticks 1..13. Cause: the Continue runs the rider manager's 0x1297C8 → 0x11D390 (rider+0x880 == 7 in every ready state), which
places the rider twice before tick 0 — 0x112180 → 0x11D660 and 0x11DE60 → 0x11D660 at the course's region row (26B5E0 runtime
kind 1, the player's slot) — and each 0x11D660 tail (the human, +0x870 < 2) is a camera director set-target (15CCF0 → 166F28 →
the DEFAULT_3 set-target 0x176FE0) with the rider just placed (velocity 0: 0x11DF18 sets the start velocity after both calls;
the ready head and forward). 0x11D660 also enters the placement animation (semantic 5) and steps it once, then 0x11DF18 sets
the animator rate to 0. The port applied the ready savestate and stepped without either. Fix:
- `web/start_gameplay.inc`: the rolling branch of `start_event` runs `place_rider_region` at the region row (the human only;
  `npc_start_event` runs `start_event` too), then `weather_event_seed_apply` again (the placement's `reset_snow` put the glide
  seed's rider painter back; 0x111890 only sets +0 = −99999). Rows: `tools/generate_event_seed.py rolling_region_row` →
  `browser_event_<C>::rollingPlacement` / `browserEventRollingPlacement` (generated at every core build).
- `web/core.cpp`: after the rolling start's event camera seed, two `setTarget` on every director node with the placed rider.
Result (all 3299 ticks of weather/dbc2-weather): the camera eye, the camera painter (values and distance) and the splash speed
exact (except 1657, section 10); the rider painter exact from record 0. Every rolling-start gate unchanged or better: bones of
peak3/the-throne-tuck exact through 699 (gated 696; they were off from tick 1) and of peak1-race-start to the end. Found by
the camera investigation (native check: the savestate words plus the two set-targets equal PS2 record 0 on every decoded
camera word 0..0x15C except the pitch +0x54, off by 2e-5..1.6e-4 rad for 6..10 ticks without reaching the eye).
