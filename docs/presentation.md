# Presentation leftovers vs PS2 frames (2026-09-26)

Round 2 (the same day): sections 1 (spray and engine), 2 (the sky behind the loop), 3 (the prefetch measured on phones,
the hold proposal) and 11 (painter regions on the core's tick). Round 3: sections 12 (the #163 camera), 13 (the loop's
canvas alpha over a world switch), 14 (terrain sparkle), 15 (the NIS projection) and 16 (the backcountry heli).

Nine presentation / visual gaps closed against ARMSX2 frames. Each change sits behind a switch in `web/pv-flags.js`
(`pv(name)`; `?pv=1` all on, `?pv=0` all off, `?pv=a,-b` the defaults with a on and b off), so the tree stays deploy-safe; the default is on
once the item was compared with PS2 frames in Chrome **and** WebKit (WKWebView harness). Regression checks:
`web/test-presentation.mjs` (npm test).

PS2 runs (derived savestates only): `local/ps2-capture/presentation/` (`boostfx/` the gp+0x1630 trick-FX switch poked,
`rocks/` CRA3, `sky/` ERA5); the new-career / CTM runs of `local/ps2-capture/ctm-parity/runs/` (new-career, f95-after).
Comparison sheets (PS2 | before | after | WebKit): `local/browser-validation/presentation/`.

| switch | item | default |
|---|---|---|
| `plane` | New-career midway plane | on |
| `heli` | Heli / gondola ride over a world switch | on |
| `flyover` | Downloads of the event package under the CTM fly-over | **off** |
| `boostLight` | Trick-boost rider light | on |
| `aiFx` | Computer riders' streamers and aura | on |
| `worldWrap` | World static-model texture clamping | on |
| `skyClear` | Black GS clear behind the sky | on |
| `cheat` | Enter Cheat / Player Name keyboard dimming | on |
| `help` | Select Peak / Mode / Event help line | on |
| `rivalIcon` | The rival's "!" marker | on |
| `planeFx` | New-career plane: snow spray and engine loop (round 2) | on |
| `heliSky` | The departure sky behind the transport loop over a world switch (round 2) | on |
| `regionTick` | Fog / ScreenTint / Sun / glare / rider Lighting switch regions on the core's tick (round 2) | on |
| `planeCam` | New-career plane: the #163 cabin camera on the rider at the door (round 3) | on |
| `acrossLoop` | The transport loop drawn over a world switch writes canvas alpha 1 (round 3) | on |
| `sparkle` | Terrain snow sparkle, patch flag 0x800000, density from the Surface painter (round 3; the plane NIS's "falling snow") | on |
| `nisProjection` | NIS views use the letterbox block's projection (P11 / P00 = 1.12; idle steps the player's) (round 3) | on |
| `bcHeli` | The backcountry heli in the `<bc>_heli_arr` / `heli_arrb_<char>` arrivals (#123-137) (round 3) | on |
| `heliLight` | The heli lit per vertex from the location's object bank, as a flag-0x4000 instance (round 3) | on |
| `heliHover` | The heli hovers on (551..677 loop) after the arrival instead of the world's frame-0 copy (round 3) | on |

## 1. New-career plane (`plane`)

- **PS2** (`ctm-parity/runs/new-career` s2400..3050): #153 `abc1_heli_arr_midway` — the tilt-rotor flies in from ~250 m,
  tilts its nacelles and hovers at the locator with snow spray; #163 `heli_arrb_<char>_midwayabc1` — the ramp lowers, the
  rider stands in the door and jumps.
- **Before:** no plane; the rider jumped from empty sky.
- **Cause:** the plane is ABC1's `mdl_ABC1_os609_full_version_inair2` (track 6 rid 1803, model rid 55, 6 nodes), already in
  the ABC1 / PEAK1 packages but baked static at its LiveComp frame 0, 252 m out. The PS2 moves it with the scripts' kind-7
  channel-0 stage calls: the per-tick handler 0x2808E8 looks the hash up in the course location's stage globals (0x309E50)
  = ABC1 global program 5: `0x0DE99225` (#153 t0) SetNodeState 1 + builtin 3 frames 0..185 once at 30 fps + sound 201;
  `0x0EE53794` (#163 t0) frames 186..550; cleanup `0x07D196B4` (0x2807B0 at the step end) SetNodeState 1 (back to rest).
  `0x0D9F751E` / `0x0D905E74` (program 2) are the spray particles on `ospreySpray_1001` (round 2, below).
- **Fix:** `tools/export_cutscene_sets.py --plane` writes `CUTSCENES/SETS/ABC1PLANE` (3222 vertices, 13 batches per
  LiveComp node, the LiveComp record with the two starts, `world_copy` bounds). `web/cutscene-stage-sets.js` runs the
  channel-0 calls on the set's `LiveCompAnimation` (the pose is a function of the script time, so QA seeks replay it),
  applies the node deltas and hides the world's static copy while the set stands in. `web/cutscenes.js`: `setOf` picks the
  set for anchor 37, channel 0 fires `stage().call`, step ends run the cleanups.
- **After:** `plane-sbs.png` (PS2 | browser pairs at s2400..2925 ≈ script t55..355 / t50..200): position, nacelles and
  hover match; `wk-plane.png` WebKit = Chrome.
- **Test:** the set's starts / cleanup, #153 253 m -> 3.6 m from the locator, #163 hover, seek replay, cleanup to rest,
  the world copy hidden and restored.
- **Round 2: spray and engine (`planeFx`).** The ABC1 track-6 global functions, run through the stage VM (a native harness
  on `engine/stage_script_vm.hpp` with a recording builtin): `0x0DE99225` / `0x0EE53794` = builtin 73 (297EB8 stop) +
  SetNodeState + builtin 3 + builtin 31 (297950: loop script sound 201 on the plane), cleanup `0x07D196B4` = builtin 73 +
  SetNodeState 1. `0x0D9F751E` (#153 t250, #163 t0) = builtin 25 on `ospreySpray_1001` (134662): 60 particles, Duration -1,
  Life 1, Size 0..700 (SizeR 250), radial velocity 4000 cm/s, force z +1000, colour (0.9, 0.9, 1) alpha 1 -> 0, texture 25
  blend 1; cleanup `0x0D905E74` = builtin 69 mode 0 (a Particle's stop is a no-op). 297950 for id 201: TRANSPORT bank
  sound 1, bus 5, volume 127, distance parameter 300 m (400 m only when the location record word is 8), priority 80, the
  voice at the instance's entity matrix, refreshed every frame by 297FA0.
  - **Before:** no spray, no engine.
  - **Fix:** `web/cutscene-plane-fx.js` (from `cutscenes.js` stepAudio / step end / list end). A call the plane set owns
    restarts the engine loop (`sfx.play` TRANSPORT 1, vanish 300, live position = the LiveComp root node) and its cleanup
    stops it. A call it does not own goes to the core: `stage_global_call(track, hash, x, y, z)` (`web/presentation_core.inc`)
    looks the hash up in the track's globals and runs it with the race builtins. Before the world's first run the race VM
    does not exist yet (reset_race builds it), so a private VM runs the track's global handler programs quietly (functions
    only). Builtin 25's 300 m rider test (0x2FEB24; (gp-0x84C)+0x550 = 2 in CTM free ride, PS2 new-career fr) takes the
    plane's position, since the PS2's rider rides in the plane while the page places its rider at the run start.
    `stage_cutscene_tick` advances the effects those calls built once per script tick (the PS2 entity pass runs under the
    NIS). `stage_world_reset` keeps them through the run start (`stage_cutscene_kept`), as the PS2 does.
  - **PS2:** `ctm-parity/runs/new-career/fr.p2s` (free ride after the drop) holds exactly two Particle objects, both alive,
    on 0x20E06 = ospreySpray_1001. The browser builds the same two. The owner matrix and 90 of 100 emitter words are
    identical; words 18..27 (the construct's visual-RNG draws) and 97 (the flip phase) differ (`test-presentation.mjs`).
  - **After:** `plane-spray.png` (PS2 s3300 / 3325 / 3350 | before | after at ride +40 / 60 / 80 ticks): the clouds left
    of the rider, as on the PS2. `plane-spray-163.png`: the scene's "?" marker stays (a first version rebuilt the race VM
    early and hid it). `wk-plane-spray.png` WebKit. The engine requests: #153 t0 at the plane 252 m out, #163 t0 at the locator;
    headless audio is locked and muted, so the voices are checked by the request log and `test-presentation.mjs`.
  - **Left (round 2):** the #163 cabin camera sat further back than the PS2's (fixed in round 3, section 12). The flakes seen
    under the NIS (s2725) are not the weather snowfall but the terrain snow sparkle (section 14).

## 2. Heli ride over a peak change (`heli`)

- **PS2** (`f95-after`, `peak2-arr`): "Go to this peak now?" Yes (and the post-event Transport to another location) ->
  WS14 heli_dep (at a station) -> `heli_inair` #149 once -> `heli_inair_<char>` #113..122 held (flags 8) with "Loading..."
  while the destination streams -> WS10 (e.g. the DBC2 movie).
- **Before:** the page load screen (or the black cover after an event); no ride.
- **Fix:** `web/ctm-transport.js rideAcrossSwitch` (from `career-ui.js goWorld` when the destination is another world):
  plays the departure / in-air ride in the current world, and when the held loop starts runs the course switch under it.
  `cutscenes.js acrossSwitch()` keeps the list alive through `stopRun()` and draws it itself (`host.render` =
  `renderer.render(scene, camera)`, main.js; the TRANSP set and the actor stay in the scene) while main.js draws no world;
  the load screen's world mode stays clear with the loop's bars and caption. `enterWorld` releases the held step.
- **After:** `hp-sheet.png` / `heli-sbs.png`: helipad departure, exterior flight, cabin loop with "Loading..." over the
  2-3 s switch, then Ruthless; post-event (Snow Jam -> Peak 2): heli_inair + loop over the switch. `wkhp.png` WebKit.
- **Round 2: the sky behind the loop (`heliSky`).** The heli cabin's windows are painted into its textures (TRANSP 9-4); the
  gondola's windows (9-10, alpha 148/255) show what is behind them. The PS2 draws the world's sky dome under the held loop too
  (22DE98, camera-anchored, depth off, 382AF0 clears to black): `ctm-parity/runs/to-final` s280 / s320, blue through the
  windows. The page's switch unloads its sky, so the windows showed the clear colour (the last fog colour, pale grey).
  - **Fix:** `cutscenes.js` keeps its own copy of the departure's dome (the course package's SKY `world.json`; a streamed
    world's current sky row 44..48 from the core's `peak_world_sky`) and draws it first in `acrossSwitch` (opaque list,
    renderOrder -11 / -10, depth off, blended alpha batches), on a black clear. `ctm-transport.js` loads it during the ride's
    first steps (`prepareAcrossSky(ui.course.sky)`).
  - **After:** `heli-sky.png` (PS2 to-final s320 | before | after, the loop camera alternative 2 as on the PS2, frozen at
    t40 during the switch): the windows are blue as on the PS2. `wk-heli-sky.png` WebKit (s280 | before | after).

## 3. Hold after the CTM fly-over (`flyover`, off)

- **PS2** (ctm-flow.md 7): the event runs in the streamed world; the riders load under the fly-over; a few ticks of black.
- **Measured** (Chrome, local, Green Base Station -> Snow Jam gate): fly-over 0.65..5.6 s, then 5.6 s black: course load
  3.2 s (world 1.8 s, 5 computer-rider contexts 1.2 s), lineup 0.3 s, pipeline warm-up 1.6 s (TSL node builds ~1 s),
  cutscene actors 0.3 s. Throttled 50 Mbit/s, 40 ms, Metro-City (not cached): **23.7 s** black.
- **Fix (partial):** `ctm-event.js prefetchEvent` downloads the event package (27 files, the boot list per course)
  through web/downloads.js once the fly-over's clock runs: the switch joins the running downloads. Throttled Metro-City
  23.7 s -> **18.2 s**; unchanged on a warm cache. Off by default: it holds ~60 MB of bodies next to the streamed world
  (phones unmeasured).
- **Round 2: phone-class memory.** Green Base Station -> Snow Jam gate, the renderer's macOS phys_footprint (`footprint`)
  every ~0.5 s through the ride-in, JS heap by CDP (`local/browser-validation/presentation/flyover-memory/`). Chrome
  390x844, DPR 3, mobile, CPU 4x (4 runs off, 4 on); WebKit (`web/webkit-driver.mjs` WKWebView, 390x844, WebContent process):

  | | free ride | fly-over max | at the switch | session peak (load / event) | hold |
  |---|---|---|---|---|---|
  | Chrome, off | 797-1022 | 827-855 | 715-825 | 950-1029 MB | 50-62 s |
  | Chrome, on (the round-1 prefetch) | 820 | **1126** | **1223** | **1223 MB** | 58 s |
  | Chrome, on (one copy) | 816-1025 | 965-1031 | 985-1039 | 1050-1102 MB | 44-81 s |
  | WebKit, off | 923 | 924 | 952 | 1059 MB | 30 s |
  | WebKit, on (one copy) | 823 | 905 | 919 | 1130 MB | 28 s |

  The free-ride row varies ±100 MB between runs (footprint after a forced GC); the event load's own peak is ~1.0 GB. The
  round-1 prefetch read each body through `fetch()` + `arrayBuffer()`: the shared bytes, the Response copy and the ArrayBuffer,
  three copies of the 68 MB package at once, **+300-400 MB** under the fly-over and +190 MB on the session peak.
  `downloads.js prefetchDownload` now starts the shared download without a Response and keeps its one copy for the first
  request of the URL (then the usual 5 s, at most 30 s); with pv flyover a body of known size is read straight into one
  buffer. That still adds **+70-80 MB** to the session's peak in both engines (the bytes wait through the first part of the
  event load) for a hold that is shorter only on a slow network (round 1: 23.7 -> 18.2 s at 50 Mbit/s). With the iOS tab limit
  at ~1-1.5 GB and the event already at ~1.0 GB, that is not acceptable on phones: **`flyover` stays off**. At phone speed the hold
  itself is 50-60 s (Chrome 4x) and 30 s (desktop WebKit), against 5.6 s in desktop Chrome: the compute, not the download,
  is the hold.
- **Proposal: remove the compute hold with a prepare / commit split of `switchCourse`** (main.js; owner: the sim-worker
  agent, whose worker switchover already keeps two cores alive). Today `switchCourse` runs `unloadCourse()` then
  `loadCourse(next)` then `afterSwitch` after the fly-over. Split `loadCourse` into:
  1. `prepareCourse(entry)`, started by `ctm-event.js rideIntoEvent` when the fly-over's clock runs (where `prefetchEvent`
     starts now): a detached `newCore()` instance (the free ride keeps its own) gets the event's terrain / world collision /
     rails / stage world / fog / weather / lighting in time slices (the JSON parses in the peak-world worker, as streamed
     locations are already cut, `peak-world-prepare.js`); `asset()` builds the event's render package into a group that is
     not in the scene, `fogRenderer.compileObject` compiles its pipelines (the 1.6 s warm-up), the five computer-rider
     contexts are made on the new core (1.2 s), the lineup and the approach's cutscene actors (`cutscenes.prepare`). Every
     step yields to the fly-over's frames (the load screen's slicing).
  2. `commitCourse(prepared)` at the fly-over's end: `unloadCourse()` of the streamed world, `core = prepared.core`, the
     prepared groups into the scene, `afterSwitch` without its loads: the black between the fly-over and the approach becomes
     the unload and the swap (a few frames), as the PS2's few ticks.
  - **Memory:** both worlds live through the fly-over. The event's own share is ~250 MB (its load peak ~1.0 GB less the
    ~750 MB left after the unload), so the phone-class peak would reach ~1.1-1.2 GB unless `prepareCourse` first releases
    the streamed world's render packages outside the venue's location (the fly-over frames only the venue; the collision
    stays until the commit). A phone tier can keep today's sequential switch.
  - **Pieces that do not need main.js:** the prefetch (done, one copy, off) and worker-side parses of the event's JSON
    (the shape `peak-world-prepare.js` gives streamed locations). The rest needs `loadCourse`'s course-scoped state (`core`,
    `course`, `freeRide`, `sky`, the renderers) as parameters, which is the restructure.

## 4. Trick-boost rider light (`boostLight`)

- **Units:** rider manager 0x128AC0, every tick and rider: 1218D0 clears the control owner's list (rider+0x77C -> +0xD30,
  392D18); 2EADD0 adds one light while rider+0x2EC > 0 (or the gp+0x1630 switch): 392D90(list, direction (0,0,1,0),
  colour (2,2,2)). 1220D8 adds the list through 389558 / 389520 / 389308 — the path `shade_rider_lighting`'s `extra` ports
  (tools/test_rider_irradiance_native.py writes this exact D30 layout into the original 1220D8, 2000 cases, all words):
  the values go in unchanged (bank units, z-up frame).
- **Measured:** PS2 `boostfx` bc-race-tuck normal vs gp+0x1630 = 1, tick 2480 (human on the ground, light only): rider
  pixels **+19.7 / +14.7 / +16.9**; browser **+18.5 / +14.1 / +16.0** (`light2480.png`).
- **Fix:** `web/boost_gameplay.inc` keeps the list (`rider_controller_lights`: count, ambient, direction, colour; cleared
  per FX pass) and the QA switch `set_trick_fx_force` (gp+0x1630, one word for every rider); `web/rider-controller-lights.js`
  passes it to `shade_rider_lighting` for the human (`rider-material.js`) and every computer rider (`opponent-riders.js`).

## 5. Computer riders' streamers and aura (`aiFx`)

- **PS2:** the rider manager runs 2EF6D0 / 2EADD0 for every rider: `cpu941.png` (ARA1, gp+0x1630 poked: the computer
  rider in the air with purple `prbn` beams and the `psmr` board aura; white `strm` without).
- **Fix:** `boost-renderer.js createRiderFxMaterials / createRiderFxMeshes` (the human's code, factored); `opponent-fx.js`
  draws one set per computer rider from its rider-context core, one shared material set.
- **After:** `aifx-zoom.png` (Happiness rival race, exact replay, Mac at 2480/2488: PS2 forced | off | on; normal row);
  `wk-aifx.png` WebKit.

## 6. World scenery clamping (`worldWrap`)

- Only three packages have static-model materials with the CLAMP_1 bits (word+12 & 0x180000): CRA3 (27 `polyrockshit`
  meshes, textures 431 466 475 476 483), ABA1 (one `loggy_1002` mesh on 102, which also repeats on 8 others) and CHP2
  (hidden volumes on 17). Texture 466 is rock over a snow bottom row (opposite edges differ by 106): with repeat the
  bilinear filter drew a light line along the rock band's edge (`cz2-zoom.png`).
- **Fix:** `web/world-batches.py` splits batches by `triangle_wrap` (batch `wrap`), filled by `web/prepare.py` and
  `tools/export_peak_world.py` (new `--batches-only`); re-split ABA1 / CHP2 / CRA3 and PEAK1/ABA1, PEAK2/CHP2, PEAK2/CRA3
  (triangles, other keys and files verified identical; ABA1 gained one batch). `world-material.js staticModelTexture`
  samples a clamped copy of the texture.
- On the PS2 camera path of `cra3-full` 3931..4099 the clamped meshes are not in view (browser frames identical before /
  after there); the seam shows from off-course views. WebKit `wkrock.png`.

## 7. The clear behind the sky (`skyClear`)

- **PS2:** 382AF0 fills the new draw buffer with `RGBAQ = [renderer+6AE0]` (SetClearColour 389098, every caller passes
  0,0,0; `+6AE0` = 0 in five in-race savestates): black. The sky does not write depth, the fog composite fogs those pixels
  with palette entry 0 (the browser does the same: sky depth -> Z ≈ 0).
- **Before:** the sky scene cleared to the fog colour (converted as linear): the ring's 92 %-opaque bottom texels showed a
  pinkish dash. **After:** black; the ERA5 dash turns from (59,72,182) to (43,64,173) next to (62,86,200) (`skydash.png`).
  `main.js` (3 guards). WebKit `wksky.png`.

## 8. Enter Cheat dimming (`cheat`)

- Fullkeyboard has three full-screen dimming shapes with a keyboard-shaped hole (`back_com` / `back_exp` / `back_reg`,
  alpha 153, gradient); they start hidden (flags bit 0x40) and cKeyboardPopup 0x1CD348 shows `back_com` only (compact
  mode). The port drew all three: 1 - 0.4^3 = 0.94 dimming (PS2 fit 0.591). `fe-screens.js` hides the other two.
- Mean |PS2 - browser| on 40-enter-cheat-keyboard: **12.7 -> 5.9** (Chrome), 5.4 (WebKit) (`fe-cmp.png`).

## 9. Select Peak help line (`help`)

- The Map HelpText element is 375 wide at 50 %; the port overrode it to 400 / 56 %, so "mode." wrapped. The element's
  own box reproduces every PS2 wrap of `menus/single` 04..07 (`help-sbs.png`: PS2 | before | after | WebKit).

## 10. The rival's "!" marker (`rivalIcon`)

- **PS2** (Happiness rival race, `boostfx/bcforce.tick480`): Mac's "!" is orange (~215,130,45) with a thick dark outline.
- **Before:** a pale salmon, see-through triangle (~227,163,155) with a faint outline.
- **Cause:** 2D5048 sends r, g, b x 255 and a x 128 (0x437F0000 / 0x43000000) and the GS modulates and blends in bytes:
  Cs = T x V / 128 = (255, 127, 0) for level 3, As = 0.8, (Cs - Cd) x As + Cd. The port drew it with a MeshBasicMaterial
  in three's linear space (and an sRGB-decoded texture), which washes the colour and the outline out.
- **Fix:** `rival-beam.js byteIconMaterial`: the byte-space modulate and blend in the encoded post-rider pass
  (`registerEncodedEffect`), texel read raw. After: body (227,146,65), outline dark (`rival-icon.png`: PS2 | before |
  after; `rival-icon-wk.png` WebKit before | after).

## 11. Painter regions on the core's tick (`regionTick`)

- **PS2:** 2C0778 steps every environment wrapper with the painter record of location gp+0x770, the track of the human's last
  contacted patch, which 2ED490 writes after block 0's wrappers stepped (weather.md 11). Each block has the same nine
  wrappers (+0 type 0x485218, +4 Fog 0x484FE0, +8 glare, +0xC ScreenTint, +0x10, +0x14 Sun, +0x18, +0x1C Lighting, +0x20 Weather,
  blocks at 0x4FA370 + 0xF0 n). The camera block 6 (0x15EBCC, after the rider manager) therefore takes the new record on the
  tick gp+0x770 changes, the human's block 0 one tick later.
- **Before:** Weather switched in the core (weather.md 11); Fog, ScreenTint, Sun, glare and the rider Lighting followed the
  page's region hook (`main.js freeRide.on('region')`, `free-ride.js regionLighting`): the rider's contact read once per
  frame, then fetches, then the switch when they landed (a D -> D_DRA4 crossing in the page: the fog payload ~6 ticks late
  with a warm cache).
- **Fix:** `peak-world-prepare.js` fetches each location's fog-tree / lighting / screen-tint / sun-painter / glare-painter with its
  collision data (so a record is in before its patches can be touched); `peak-world.js` gives Fog and Lighting to the core
  (`fog_location`, `lighting_location`: web/environment_bridge.cpp picks the record of `browser_painter_region_track()` =
  gp+0x770 in the camera block's fog step and in the rider block's lighting step, which runs before the human's gp+0x770
  update; a region without a loaded record takes the class defaults with +0 = 0; until the world's first contact the course
  package's tree stays) and ScreenTint / Sun / glare to `web/painter-regions.js`, which the page painters read in their
  per-tick step right after the core's tick (`game-tick.js present`). The old hook is ignored while records exist.
- **PS2 check:** `weather/frd-regions` re-run with savestates at 2841 / 2842 / 2844 / 2845 (`region/frd-snap.tick*.png`):
  gp+0x770 27 -> 28 at 2842; the camera Sun's target size 390 -> 410 at 2842 with current 390.1 / 390.3 / 390.4 at 2842 / 2844 /
  2845; the camera Fog steps into D_DRA4's payload 1, whose values equal D's (its current 2999.994 / 17999.965 unchanged).
  The browser (`compare-ps2-capture.mjs weather/frd-regions` with a TICK_HOOK feeding the records and stepping the page
  painters): gp+0x770 27 -> 28 at 2842, the Fog record 28 at 2842 (payload 1, current 2999.994 / 17999.965), the Sun 390.098 /
  390.293 / 390.389 at 2842 / 2844 / 2845, the rider Lighting record at 2843; physics still exact on all 2601 ticks. D and
  D_DRA4 have the same Lighting, ScreenTint values and no glare, so those switches are not visible at this crossing.
- **Pages:** a D -> D_DRA4 crossing in Peak 2 free ride (the rider placed at the capture's line): Chrome and WebKit switch the
  Fog record, the Lighting record and the Sun on the frame gp+0x770 changes.
- **Test:** `test-presentation.mjs` (Sun / glare / ScreenTint on the region tick, a region without a record, the old hook
  ignored; the core's records fed, kept, dropped with a new world).

## 12. New-career plane camera (`planeCam`, round 3)

- **PS2** (new ARMSX2 re-run of `ctm-parity/states/new-career.p2s` with the script time read from each snapshot:
  `local/ps2-capture/presentation/nis/new-career`, #163 t = sample - 2704): camera 4 (Target, anchor 37) to t328, then
  camera 5, a Subject camera 1.5 m behind the rider at the ramp, 20 cm off the floor, looking up at her (s3035..3140); she
  jumps out past it, and the camera stays at the door while she falls into the clouds (s3186..3245).
- **Before:** camera 5 sat 4.75 m behind the rider, at the back of the cabin: the rider was small in the door the whole
  time and the outside view never came.
- **Cause:** camera 5 is on anchor 43, a live actor (40 + subject 3 = the actor bound to 4). 0x27A0D8 ids 40..60 ->
  279F18 -> 27B948: position = rider+0x110 through the rider interface (rider+0x6C0 vt+0x2C = 0x1408F0), which the NIS
  actor update writes with the actor's root (0x1237E4: the frame matrix x the curve position, 325 cm along the plane's
  locator frame); yaw / pitch = the heading / elevation of the physical forward rider+0x1B0 (11E098 of the root
  quaternion), roll 0. The frame is taken when the camera is cut to (27D850, the camera's activation, -> 27D970). The page
  used the actor's *object* frame (the locator itself).
- **Fix:** `cutscenes.js liveActorAnchor` / `liveCutFrame`; `cameraPose` reframes a live-actor camera at its cut.
- **After:** PS2 block-6 Weather painter point (= the camera eye after the camera update): #163 t376 (-29808.70, 27454.50),
  t456 (-29879.83, 27464.57); the browser (-29809.21, 27453.67) / (-29879.87, 27461.93): within 1.5 / 3 cm (camera 5's
  shake channel, 0.15, is not modelled), was 325 cm. Camera 4 at t57 is exact (0.003 cm). `plane-cam-sbs.png` (PS2 |
  before | Chrome | WebKit at t330..530): the framing matches in both engines. Still different: the rider leaves the door
  ~15-20 ticks later than on the PS2 (her clip timing), the PS2's sun glare at t480 and the spray clouds at t530.
- The heli_arrb_<char> backcountry arrivals (#128-137) use the same camera; no PS2 frames of them yet.
- **Test:** `test-presentation.mjs` (the PS2 eyes above, camera 4 exact, the old frame 3.25 m off, the runtime switch).

## 13. The transport loop over a world switch (`acrossLoop`, round 3)

- **Report:** the loop drawn during a world switch looked darker in WebKit than in Chrome.
- **Measured** (the held gondola loop over a switch at t40, camera alternative 2; `across.mjs` in the scratchpad): on screen
  WebKit matched Chrome to within a level. Region means for the left window / right window / ceiling were Chrome
  (compositor screenshot) 84/77/88, 78/79/109, 47/48/48 and WebKit 84/76/87, 77/79/109, 47/48/48. The difference came from
  the QA capture. `cutscenes.acrossSwitch` renders straight to the canvas. Unlike the page's fog pipeline, whose last pass
  writes alpha 1, it left canvas alpha below 1: 18 % of the frame under the sky dome's blended batches
  (alpha As^2 + Ad(1 - As)), and the gondola windows over the transparent clear without heliSky. three's output pass
  treats the frame as premultiplied (rgb / a -> sRGB -> x a) and the canvas composites premultiplied over black, so those
  pixels showed sRGB(rgb / a) x a in **both** browsers: Chrome's compositor matched u x a within 0.5 levels. Chrome's QA
  shots, a 2D-canvas readback, were unpremultiplied again (brighter), and WebKit's window snapshots were not, so WebKit
  looked darker.
- **Fix:** a last full-screen draw in the across draw sets alpha to 1 and keeps the colour (`cutscenes.js opaqueAlphaFill`:
  blend Zero / One on RGB, One / Zero on alpha); the displayed colour is then the frame's RGB, as on the PS2 and the page path.
- **After:** canvas alpha 255 everywhere; the Chrome capture now equals its compositor screenshot. Frame means: Chrome
  83.1 -> 83.8 (screen), WebKit 82.8 -> the same regions as Chrome. Without heliSky the windows go from 77 -> 84 in both.
  `across-loop.png` (PS2 to-final s320 | Chrome | WebKit, before / after), `across-loop-exact.png`.
- **QA note:** a translucent WebGPU canvas read back through a 2D canvas is unpremultiplied; compare Chrome against
  Playwright's `page.screenshot` (the compositor) when a frame may hold alpha < 1.
- **Test:** `test-presentation.mjs` (the fill's blend factors and draw order, only in the across draw behind the switch).

## 14. Terrain snow sparkle (`sparkle`, round 3)

- **What the PS2 draws under the plane NIS:** not the weather snowfall. The camera block's Weather painter has snowfall 0
  and all six snowfall layers have count 0 at #163 t57 / t376, and in free ride after the drop (gp+0x770 = 6 / 7). Moving
  both ospreySpray Particles 900 m up mid-NIS left the dots unchanged. Patching the terrain sparkle draw off (0x38D6C4
  `beqz` -> `b`, derived state `nis/no-sparkle`) removed them. The diff of synchronous frames is the sparkle alone: ~80 dots
  at #163 t66, only on the near snow (`nis/`). The same glints run over the snow near the camera in every ride.
- **Which patches (0x38D968, end of the terrain pass 0x38B370):** 0x38D690 runs for each patch of the render list +0x4A60
  (16-byte entries: every edge at tessellation level 8) and of +0x4D84 (20-byte entries with +8 > 0: +8 = 2 when an edge is
  at level 8; +8 = 0 is skipped; 1, which would halve the count, was not seen in 20 states) whose record flag word +0xC has
  bit 23 (on disc: most snow patches, e.g. 1237 of ARA1's 1913). The edge levels come from 0x22C410:
  {4, 4, 6, 8}[trunc(clamp(b + k (dA^2 + dB^2), 1, 3))], with the corner distances dA, dB and the line 22C1B0 through
  (2 r0^2, 3) and (2 r1^2, 1), r0 = 3000, r1 = 15000 cm: a patch sparkles while one of its edges has (dA^2 + dB^2) / 2 <= r0^2.
- **How many:** count = trunc(|bbox min - bbox max|^2 x 8 x 3.333e-7 x renderer+0xC4) (38D6CC..38D734). renderer+0xC4 is the
  current value of **world painter type 10, tWPIGD_Surface** (factory 2C0408 case 10, ctor 2BC980, vtable 4844C8; +8 current,
  +C sample, class defaults 1.0): each payload is (rate, density), found in the location's point tree at the camera X/Y and
  stepped by the painter driver 2C0778 like ScreenTint (the first step jumps, rate >= 0 jumps after that distance, rate < 0
  blends w = rate^2 per tick, the single-float blend 2BCECC / 2BD30C; no record or no section: the defaults). Only five
  locations have one: ABC1 (4.0 / 1.8 by region), BRA2 (2.0), CBA2 (2.5 / 1.0), CHP2 (0.0 / 2.0), CRA3 (1.5 / 3.0).
  PS2 states: ARA1, DBC2, EBC3, ERA5 races 1.0; ABC1 race 1.8; CRA3 race 1.49998; the #163 NIS 4.0 (ABC1's other region).
- **The sprites (VU1 program 4 at 0x1460, one call per patch):** the UNPACK carries the record's 16 power-basis rows, the
  terrain object's +0x3E0 3x3 "twinkle" rows and +0x420 (colour 128, (10, 0, 3, count)). A VU RNG chain (RINIT / RNEXT, LFSR
  taps 4 and 22) seeded from the rows gives per sprite (u, v) and a vector n in [-0.5, 0.5)^3; the sprite sits at P(u, v) +
  12 cm up. The count only ends the loop, so a smaller count is a prefix of the same chain. alpha = trunc(min(n . s, 1) x 128),
  dropped when n . s < 0, with s = T16 a.x + T17 a.y + T18 (a.z + a.w), a = the guard-band clip matrix's z column
  (sx right.z, sy up.z, forward.z, forward.z; sx = 0.25 P00, sy = 0.21875 P11). The twinkle rows (38B794..38BAF0) are
  T = Rz(-0.0045 eye.z) Ry(-0.0045 eye.x) Rx(-0.0045 eye.y) (Rodrigues, row convention), so the glints change as the camera
  moves. Sprite: half size min(10 cm projected, 3 px of the 512 x 448 frame), ST 0..1 over FX 68 `gltr` (32 x 32), vertex RGB
  128. Material state (38D994..38DB04): GS ALPHA 0x49 (Cd + Cd x As), priority 4 (before the fog composite), ZTST GEQUAL,
  ZMSK, ATST GREATER 92 with AFAIL FB_ONLY (the test only keeps Z).
- **Browser (`web/terrain-sparkle.js`, behind pv `sparkle`):** data `terrain-sparkle.bin` next to every terrain package
  (`tools/export_terrain_sparkle.py`, v2: the flagged patches' rows, boxes and ids, then the location's Surface painter tree
  and payloads; 62 files, 8.3 MB in all, 15 KB..540 KB each). Per drawn frame on the CPU: the eye in source cm, the
  Surface painter step (per camera tick with the core's Fog-driver X/Y; in a cutscene at the drawn camera, 60 steps a
  second; the record of the course, or of gp+0x770 in a streamed world), the 22C410 selection (a double-precision corner
  pre-check, then the exact EE-float edge test), the sprites of each selected patch (cached, 256 patches, at most 2 built a
  frame), the glint dot product and alpha; the kept sprites go to one InstancedMesh (vec4: scene position, alpha / 128)
  drawn in the encoded composite (layer 1, `snow-composite.js`) with blend DstColor / One: the GS 0x49 on encoded bytes. It
  is drawn after the fog composite (the PS2 draws it before): fog is small at <= 30 m, measured energy agrees (below).
  Free ride: `web/free-ride.js` attaches each drawn location's set (with its bam.sdb track) and drops it on release.
- **Verified against PS2 gameplay frames** (`local/ps2-capture/presentation/sparkle/`, `ps2pair.py`: a neutral-pad navigate
  state from a capture savestate and the same with 0x38D6C4 patched, 12 frames each with states; browser at each state's
  outer camera, `local/browser-validation/presentation/sparkle/pinshot.mjs` + `report.mjs`; glints = connected dots of
  on - off, energy = their channel sum; hit = share of the browser's sprites (alpha >= 16) with a PS2 dot within 2 px, at the
  camera two ticks before the snap):

  | course (peak, density) | PS2 glints / energy | Chrome | WebKit | hit (control) |
  | --- | --- | --- | --- | --- |
  | ARA1 Snow Jam 426 (1, 1.0) | 24 / 7273 | 38 / 8748 | 39 / 8295 | 59% (3%) |
  | ABC1 Happiness 1209 (1, 1.8) | 10 / 1275 | 12 / 1250 | 13 / 1338 | 67% (0%) |
  | ABC1 Happiness 407 (1, 1.8) | 44 / 19450 | 67 / 20850 | - | 49% (5%) |
  | CRA3 1228 (2, 1.5) | 59 / 26848 | 91 / 29088 | 78 / 28593 | 73% (8%) |
  | EBC3 The Throne 1220 (3, 1.0) | 32 / 8778 | 48 / 11300 | 49 / 11068 | 53% (3%) |

  The browser's densities equal the PS2's renderer+0xC4 in every state (1.0 / 1.8 / 1.49999 / 1.0). Before the Surface
  painter (a constant 4.0, the NIS's value) Snow Jam drew 119-167 glints / 29000-41000 energy against the PS2's 23-37 /
  5900-12500, and only ~25% of its sprites had a PS2 dot. The browser counts ~1.3x more glints at the same energy: the PS2
  frame is upscaled from 512 x 448 (neighbouring dots merge) and its rider (the capture's, not the page's) covers some.
  Sheet: `local/browser-validation/presentation/sparkle/sparkle-ps2-chrome-webkit.png` (PS2 | Chrome | WebKit, dot maps below).
- **Glint count (1.3-1.5x on the page's frames):** not a cull the page misses. The PS2 frames are ARMSX2 screenshots of the
  512 x 448 GS frame scaled to 640 x 480 (bilinear); the page's shots are its 640 x 448 canvas. Run through the same path
  (`ps2pipe.mjs`: area-resample to 512 x 448, bilinear to 640 x 480), the page's glints drop to the PS2's (neighbouring dots
  merge, one-pixel dots fall under the threshold) with the energy unchanged: CRA3 59-67 vs PS2 59-63 (late frames 40-46 vs
  41), ABC1 9-11 vs 9-10, EBC3 20-24 / 38-40 vs 20-24 / 32, Snow Jam 31-42 vs 24-37 (there the capture's rider stands in the
  glint band; with its depth test off the PS2 shows ~20% more glints). Per-patch counts, the LOD lists and the drawn sprites
  equal the PS2's (above); a GS sprite of the smallest size (~1.1 px at 60 m) still covers a pixel centre, so no size cull.
- **Free ride** (Chrome and WebKit, PEAK1 lodge and ABC1, PEAK2 lodge and CRA3, PEAK3 lodge, 900-3600 ticks, `freeride.mjs`):
  the resident locations' sets only (5-7), 13-43 patches selected, 60-400 sprites; density 1.86 -> 1.0 leaving ABC1 for
  ABC1_A (no Surface section: the defaults), 1.5 on CRA3; the same numbers in both browsers.
- **NIS (#163 t66):** density 4.0 from ABC1's tree at the cutscene camera; the dots fall where the PS2's do (VU1 memory: 5
  sprites bit-exact). With the page's old 4:3 NIS projection the glints carried ~2.3x the PS2's energy (124 / 50565 vs 77 /
  21747): the NIS picture was 19% taller than the PS2's (section 15, now fixed).
- **Cost:** update() 0.05 ms per frame (Chrome), 0.07-0.1 ms (WebKit, 1 ms timer); phone model (Chrome 390x844 DPR 3,
  quality=low, CPU 4x) 0.17-0.19 ms mean, 1 ms max, frame interval 16.7 / 16.8 ms with and without; CPU 12x (10-13 fps)
  0.64-0.77 ms mean, 5.5 ms max (a frame that builds two patches), fps within noise. No input latency (render path only, no
  tick work). Memory: the resident locations' files (~0.7-0.9 MB with the derived corners and counts), the sprite cache
  (<= 256 patches, ~0.6 MB at density 1), a 64 KB instance buffer and a 4 KB texture. Shaders: vertex 240 B / fragment 36 B
  private (`test-shader-budget.mjs` compiles it on DBC2 and PEAK1 with `pv=sparkle`).
- **Tests:** `test-presentation.mjs`: PS2 counts (321 / 307 / 279 at density 4), the LOD lists, 5 VU sprites bit-exact, the
  twinkle rows of 2 states, the counts at densities 1 / 1.8 / 1.5 / 4 and the prefix rule, the Surface lookups against the
  PS2 renderer+0xC4 (ABC1 race 1.8, CRA3 1.5, #163 4.0, ARA1 none), one blend tick and the reset;
  `test-shader-budget.mjs` (the pipeline compiles, under budget).
- **Switch:** on.

## 15. NIS projection (`nisProjection`, round 3)

- **PS2:** 376C58 builds the GS projection scales from the camera's half-angle tangent t and the render context's
  (0x61BA60) widescreen block +0x6B94 {mode, top, height, scale x, scale y}: X = 0.5 w / t x sx, Y = X / sx x 1.3333 h / w x sy
  (`web/widescreen.js`: Off {0, 0, 1, 1, 1}, 16:9 {1, 0.125, 0.75, 0.75, 0.75}, Anamorphic {2, 0, 1, 0.75, 1}). A NIS
  letterbox (object 2EA670, 2EAA28) saves the player's block and targets {1, 0.125, 0.75, 0.75, **0.63**} (gp-0x39A8), or
  {2, 0, 1, 0.75, 0.63} in Anamorphic; 2EA900 slides the four floats linearly with its bars (t / duration, the page's 30-tick
  bar slide), 2EA820 restores the block. So a letterboxed NIS view has P11 / P00 = 0.84 / 0.75 = 1.12, not 4:3: the image is
  drawn 0.84x as tall as a square-pixel projection. An idle step (the objectives card) has no letterbox: the player's block
  (P00 = 1 / t, P11 / P00 = 1.3333). All 2824 captured PS2 states: 2763 at the Off block, 50 at the NIS block, 8 mid-slide.
  PS2 RAM clip rows vs the model: #163 cam5a 1.2990 / 1.4549, peak2-arr #137 0.9693 / 1.0856, objectives idle (script 73,
  fov 40) 1.1918 / 1.5890, bra2-arrival mid-slide 0.8160 / 0.9285 (model 0.9288).
- **Before:** every NIS step used tan(vertical half) = tan(fov) with the stage's aspect: letterboxed steps 19% too tall
  (P11 1.732 vs 1.455), idle steps 1.33x too wide, and 16:9 / Anamorphic NIS views 1.33x too wide.
- **After (`cutscenes.js nisTangents`, `applyCamera`):** the tangents of the PS2 block for the step's letterbox fraction
  (the bars' slide), in the player's band (448 lines, 16:9: 336); the horizontal tangent through a camera view offset (the
  stage aspect is unchanged; main.js clears the offset every frame). Page P0 / P5 at #163 t71: Off 1.2990 / 1.4549 (the PS2's),
  16:9 1.2990 / 1.9399 (over the band), Anamorphic 1.2990 / 1.4549.
- **Verified** (`local/browser-validation/presentation/nis-projection/`, `nisframe.mjs` + `compare.mjs`: the vertical scale
  that best maps the page frame onto the PS2 frame of the same script tick, and the mean luma difference):

  | NIS (PS2 run) | before: scale / MAD | after Chrome | after WebKit |
  | --- | --- | --- | --- |
  | #163 t71 (new-career) | 0.855 / 23.6 | 0.995 / 11.8 | 1.005 / 11.8 |
  | #163 t171 | 0.900 / 21.8 | 0.995 / 13.4 | 0.995 / 13.6 |
  | #163 t271 | 0.905 / 20.3 | 0.995 / 13.7 | 0.995 / 13.9 |
  | #153 heli arrival t267 | 0.875 / 20.8 | 1.005 / 13.6 | 1.005 / 13.8 |
  | #137 Peak 2 heli arrival t85 / t185 (peak2-arr) | 0.930 / 27.0, 0.905 / 37.0 | 1.010 / 22.2, 1.005 / 32.9 | - |
  | #96 start-gate intro t147 / t247 (to-final) | MAD 31.5 / 39.9 | scale 1.00-1.01, MAD 27.3 / 34.9 | - |
  | #60 podium t113 (race-f, alternative 1) | 0.870 / 35.6 | 0.995 / 22.7 | 0.995 / 19.5 |

  Sheet: `nis-projection-sheet.png` (PS2 | before | after Chrome | after WebKit). The idle step was checked on the numbers
  (the objectives card covers the PS2 frame). Seen on the way, not changed: the page's #137 (played on its own) draws no
  heli; a WebKit podium shot 1.5 s after the freeze can precede the podium set's load.
- **Test:** `test-presentation.mjs` (P00 / P11 of the four PS2 states, the widescreen bands, the switch in applyCamera).
- **Switch:** on (full npm test with it on: 161/162, the known test-slopestyle-bigair R&B lineup signature).

## 16. The backcountry heli arrivals (`bcHeli`, round 3)

- **PS2:** a heli arrival at a backcountry (#123 / #124 / #127 `abc1_ / dbc2_ / ebc3_heli_arr`, then #128-137
  `heli_arrb_<char>`) is anchored on anchor 29 = the location's locator 5, the instance `mdl_<LOC>_os609_full_version_inair`
  (ABC1 rid 284, DBC2 rid 2266, EBC3 rid 715; a LiveComp, draw 'none'). The scripts' kind-7 channel-0 calls run the
  location's stage globals (0x2808E8 -> 0x309E50), identical in the three locations (the core's stage VM, `stage_global_call`,
  LiveComp log + audio events): 0x0E995385 (#12x t0) = stop + loop sound 201 on the heli + builtin 3 frames 0..185 once at
  30 fps (the fly-in); 0x0DD5F634 (#13x t0) = the same with frames 186..550 (hover, ramp, the drop); their cleanup 0x0AE69AB4
  (0x2807B0 at the step end) = the same with frames 551..677 in mode 1 (loop: the heli hovers on). 0x03E0EA1E (#12x t250,
  #13x t0; cleanup 0x03EFC174) is the snow spray.
- **Before:** the page never animated that instance, so the arrivals showed an empty sky and the rider jumping from nowhere.
- **After (`tools/export_cutscene_sets.py --helis` -> `CUTSCENES/SETS/<LOC>HELI`, the ABC1PLANE mechanism):** `cutscenes.js
  setOf` stands the set in for anchor 29 outside a loop, `cutscene-stage-sets.js` runs the players (a cleanup that has a
  player starts it: the hover loop), `cutscene-plane-fx.js` the engine loop and the spray in the core as for the plane; the
  world's static copy (frame 0) is hidden while the set shows.
- **Verified (Chrome):** DBC2 `peak2-arr` (#124 t231 / t331, #137 t60..460): luma MAD 8.4 -> 6.3, 40.3 -> 16.4, 19.7 ->
  10.4, 26.2 -> 11.5, 41.7 -> 14.7; EBC3 (a new PS2 run from `peak3/nav/out-ctm-to-peak3/arrival-cut.p2s` through the arrival,
  `local/ps2-capture/presentation/heli/ebc3-arrival/`): the page's #127 t150 / t250 / t350 match PS2 samples 176 / 275 / 376
  at MAD 5.0 / 8.6 / 7.0 (the heli over the summit, spray, pose), #137 cabin and ramp. ABC1 #123 / #137 render (no PS2 run).
  Sheets: `local/browser-validation/presentation/heli/{on-sheet,ebc3-sheet,abc1-sheet}.png`.
- **WebKit:** the same frames (luma MAD to the PS2 within 0.5 of Chrome's on every frame).
- **Lighting (`heliLight`).** The heli is a lit instance (instance flags word +8 bit 14 = 0x4000 in PS2 RAM: DBC2 / EBC3
  0x50015105, ABC1 0x50015003; the midway plane 0x10305 is not): the static-model draw 37E238 calls 2F5148 -> 2F5400, a light
  cache per instance (32 entries of 0x180 bytes, keyed by resource). Its environment bank is the Lighting painter's reference
  3, the object bank (painter +0x38, getter vtable+0x140 = 2C15C8; an empty name falls back to reference 0; PS2 RAM: DBC2
  index 19 = DOBR1, EBC3 27 = EOBR1) of a private wrapper stepped at the instance X/Y (2C0778), refreshed after 5 m of
  travel, plus up to 4 local lights from a +-10 m query (2F5AF0; none within kilometres of the three helipads). 37E120 /
  37E098 upload the node's three rotation rows (VU 9..11 -> 25..27) and the ten rows; VU1 program 3 at 0x2170 multiplies xyz
  by 128 (not 255 as the rider's program 2) and sets w = 128 on the constant row only; 0x8B8 evaluates per vertex, on the
  ITOF15 normal through rows 25..27, L = FTOI0(clamp(sum rows x (1, x^2, y^2, z^2, xy, zx, yz, x, y, z), 0, 255)), stored
  over the vertex colour, so TFX MODULATE gives Cs = T x L >> 7, As = Ta. Page: `cutscenes.js litInstanceColour` (byte
  domain, per vertex), the bank rows in each set's world.json (`export_cutscene_sets.py lighting_bank`). Chrome, luma MAD to
  the PS2 (baked -> lit): DBC2 #124 t331 16.4 -> 9.3, #137 t60 10.4 -> 4.3, t160 11.5 -> 9.5, t260 14.7 -> 5.5; EBC3 #127
  t150 / t250 / t350 5.0 / 8.6 / 7.0 -> 4.0 / 7.6 / 6.5, #137 t60 20.2 -> 17.7 (`lit-sheet.png`: DBC2 backlit and dark, EBC3
  light with orange nacelles, as on the PS2).
- **Hover (`heliHover`).** PS2 RAM after the EBC3 arrival (gameplay samples 1000-1100): the heli's player runs mode 1 (loop)
  over frames 551..677 at 0.5 frame a tick (579.5 -> 629.5). Page: at the end of the arrival the set stays in (the world's
  frame-0 copy stays hidden), `cutscenes.linger(dt)` from main.js (0 while paused) advances it and keeps the engine loop at
  the heli; it ends at the next cutscene, a course change (stop) or 3 km from the camera. Chrome: mode 1, 551..677, 587 ->
  617.5 -> 647 -> 551.5 over three seconds.
- **Test:** `test-presentation.mjs` (the sets' players and the cleanup hover, the plane's cleanup unchanged, the DBC2 calls in
  the core = the set, the switch in setOf).
- **WebKit (lighting, hover):** lit frames within 0.3 of Chrome's MAD except #127 t350 (8.2 vs 6.5: the spray cloud's random
  puffs); the hover loops 551..677 (582 -> 612 -> 642 -> 672.5) with the engine voice at the heli.
- **Switches:** `bcHeli`, `heliLight`, `heliHover` on; full npm test with all three on: 167/168 twice (runs of 12:18 and
  12:25), the known test-slopestyle-bigair R&B lineup signature only.
