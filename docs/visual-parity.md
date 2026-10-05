# Visual parity sweep: browser vs PS2 frames (2026-09-26)

A broad pixel comparison of the browser port against real PS2 (ARMSX2) frames, ranked by how large and how visible each
difference is, with the fixes made from the top of the list. Every fix sits behind a switch in `web/pv-flags.js`
(`?pv=0` all off, `?pv=a,-b`), on by default once compared with PS2 frames in Chrome **and** WebKit.

## 1. Method

**Pairs.** 17 PS2 capture runs with savestate screenshots (`local/ps2-capture/runs/<run>.tick<T>.png`, 640x480), 3-4 snaps
each, 64 pairs: every Peak 2 and Peak 3 event course (CRA3, DRA4, DSS2, CBA2, CHP2, DBC2, EBC3, ERA5, ESS3, EBA3, EHP3)
and Peak 1 (ARA1 Snow Jam, ASS1 R&B, BHP1 The Junction, ABA1 Crow's Nest, ABC1 Happiness, BRA2 Metro City): races,
slope style, big air, super pipe, backcountry, night (Metro City, Gravitude) and the weather courses (DBC2 blizzard,
EBC3, ERA5, ESS3).

**Browser frames** (`local/browser-validation/visual-parity/tools/vpshot.mjs`, driver `drv.mjs`): the page at 640x480 with
`?qa=1&ai=0&cutscenes=0` and 4:3 (localStorage `ssx3.widescreen=0`: WebKit defaults to Anamorphic on a 16:10 screen);
the capture's pad script replayed through `ssxQA.advance` from the countdown; the page's tick offset calibrated against
the record positions at tick 240 (the page runs one tick behind the capture's first record: shift +1, errors of 2-3 mm);
the displayed camera **pinned** to the PS2 record's compositor eye / look / FOV (`_step_camera_head` returns a pinned copy
for the last two ticks: at render alpha 0 the display draws the previous tick's camera). Chrome for Testing (WebGPU,
Metal, `--mute-audio`) and system WebKit (`web/webkit-driver.mjs`, DPR 2).

**Snapshot lag.** A savestate screenshot shows the frame of the state 3 ticks before its memory tick (lags 0-4 tried on
CRA3 418 / 818 / 1218: centre MAD 17.3 / 12.4 / 7.5 / 7.9 / 8.4, 19.3 / 18.4 / 17.2 / 8.2 / 9.1, 15.7 / 13.7 / 11.1 / 5.8 /
7.2). All pairs use lag 3.

**Alignment.** A least-squares affine search (`align.mjs`) finds the PS2 image 1 px lower than the browser's on average
(dy 0.75-1.25, dx 0-0.5, scale 1.000 +/- 0.006) on 14 frames: the projection and FOV match; the 1 px is the GS display
offset / screenshot resampling, not visible.

**Metrics** (`diff.mjs`, `bias.mjs`, `analyze.mjs`): mean |PS2 - browser| over the centre (x 90-530, y 50-390: no HUD),
the low-pass (13 px box) signed bias per channel, and heat maps (red: browser brighter, blue: darker). Sheets (PS2 |
Chrome / WebKit | heat): `local/browser-validation/visual-parity/sweep1/<run>.t<T>.png`, `summary.json`.

**What the harness cannot compare.** `ai=0` (the page's computer riders collide with the human, the PS2 captures are
`--isolate`d, so the human drifts): the PS2 frames show computer riders, their shadows, the place and the rival arrows the
browser frames lack. The page's human leaves the PS2 line after 1000-2500 ticks on most runs (a 1-tick start offset and
chaotic landings; physics is exact in the node comparer), so rider-level checks use the early snaps (rider error < 5 cm:
CRA3/DSS2/DRA4/KD/M2M/EHP3/SJ/R&B/BHP1/ABA1 418-1219). A `?course=X` start is not a career event: the freestyle HUD
(count-down clock, standings, no speed in the pipe) is not drawn, so those HUD items are compared on the race HUD only.
The EA RADIO popup is absent (audio muted under automation). Sparkle / flake / halo particle positions follow the visual RNG
and differ frame to frame.

## 2. Overall result

The world matches closely. Centre MAD 3.9-23 per frame (median ~11) on the frames where the rider tracks; the low-pass
bias is within +/-5 levels on 49 of 64 frames and has no consistent sign (no global gamma, tint or fog error). Chrome and
WebKit agree to 0.3-2.3 MAD on every pair (no WebKit-only rendering difference in the race). Terrain lighting, sky, fog
distance and colour, ScreenTint, glare, rider lighting and skinning line up; the residual error is edges (sub-pixel), the
rider pose, random particles and the HUD.

## 3. Ranked differences

| # | difference | category | where / evidence | visibility | status |
|---|---|---|---|---|---|
| 1 | **Race clock and speed readout**: drawn with the page's own text path (flat colour, no shadow, bigger, speed centred at x 32 so its first digit is clipped at the left edge; "MPH" small) instead of the original HUD text (0x1F16C0 / 0x1F1840 at descriptor 0x1A; the speed widget 0x2200C0 at descriptor 25) | HUD layout, fonts | every race frame; `hud-top.png`, `hud-speed.png` | always on screen | **fixed** (pv `raceHud`, on), section 4 |
| 2 | **HUD text colour**: every PS2 HUD glyph peaks at 204 (white) or 0.8 x its colour (red OPPONENT 158,12,8 for 0x4C88C8); the browser drew 255 / 1.0 x | HUD colour | 4 courses; `score8.png`, `hud-pct.png` | always on screen | **fixed** (pv `hudText`, on), section 5 |
| 3 | **UI atlas alpha 2x too opaque**: FE_1 / OV_1 / SU_1 palette images were exported with alpha = 4 x PS2 alpha (the SSH handler already doubles, the exporter doubled again): every translucent UI texel (Uber coil, menu panels, glows) too opaque | HUD / FE alpha | coil `coil3.png`; FE screens 5-30 % closer | always on screen, every menu | **fixed** (re-derived assets in place), section 6 |
| 4 | **Metro City start light shafts**: two bright vertical beams above the course at the start are faint in the browser (not a static-model blend class: hiding blend classes 1/2/3 leaves them) | set pieces / particles | BRA2 418 `c2.png`, `c4.png` | Metro City start | **fixed** (pv `streamers`, on): they are the rider's air streamers 2EF950, section 12 |
| 4b | **Results panel and in-race cards**: the port drew its own panel (a rectangle with an outline, bigger text, orange human row) where the PS2 draws OV.LUI OV_darkblue (cut-corner frame, header bar and picture, lights) with 43final_standings (rows, medal, message, menu) | HUD layout, fonts | `menus/race/16-results-restart.png`, `ctm-parity/runs/after-final/results.png` | after every event | **fixed** (pv `luiResults`, on): results, records, peak results, race card; rewards list and rival / peak cards staged (export copy pending), section 9 |
| 4c | **Finish / time-up banner**: a race finish showed a small "FINISH!" in the page's text (no time); a freestyle finish the sprite 27 px too wide and its score in the page's text; TIME'S UP 11 px low. PS2: 0x21F660, 'fini' at descriptor 0x42 (213 x 41 at (320, 190)) with the finish time / score under it at 1.9076 x, 'timeup' at descriptor 0x1B | HUD layout, fonts | `ctm-parity/runs/race-f/fin.png`, `runs/pipe-finishov.tick4408.png`, `runs/pipe-brake.tick7519.png` | every finish | **fixed** (pv `finishBanner`, on), section 10 |
| 5 | **Crash recover meter**: PS2 draws "RECOVER - [Square]" and a white bar filling in three sprite pieces; the browser an outline with a red fill (0x21D9A0 not ported: owner sprites +0x48 / +0x4C / +0x4D4 / +0x4D8, text 0x1E95A0 at descriptor 55, bar descriptor 54) | HUD | BRA2 1218 sheet | after every crash | **fixed** (pv `recoverMeter`, on; records export pending copy), section 13 |
| 6 | **Freestyle HUD text** (standings rows, OPPONENT line, freestyle clock): drawn with the page's text path; the OPPONENT line was also the wrong yellow (255,255,204 for the A,R,G,B 1,1,0.8,0 of 0x4C88A8) | HUD fonts, colour | DSS2 418 `opp-cmp.png` | every slope style event | clock and OPPONENT line fixed (pv `raceHud`); standings rows **fixed** (pv `hudStandings`, on), section 14 |
| 7 | **Translucent world models blend in linear space** (the GS blends encoded bytes): 50 % white over black is 188 in the browser, 128 on the PS2 | gamma / colour space | known (terrain-render-fidelity.md "Remaining gaps") | fences, glass, fringes | additive class ported to the encoded pass (pv `byteBlend`, off: no visible gain measured), section 17; port-wide measurement and options, section 38 |
| 8 | **UI resolution**: the PS2 draws the HUD and menus into a 512-wide buffer shown 640 wide (soft, horizontally blurred glyphs and coil); the UI canvas is 640 wide (sharper) | texture filtering | every HUD crop | subtle | kept (design) |
| 9 | **Terrain texture detail at mid distance**: PS2 sharper / more aliased (no or biased mips), browser smoother (trilinear + anisotropy 4) | texture filtering | DRA4 418 `c3.png` | subtle | open |
| 10 | **Board spray shape**: PS2 chunks rounder / more vertical, browser flatter | particles | SJ 1219 `spray1.png` | subtle | open |
| - | Terrain snow sparkle (glints on the snow near the rider, e.g. CRA3 1218) | particles | CRA3 1218 sheet | - | **owned by sparkle** (another agent, pv `sparkle`) |

Not differences (checked): sky dome and clamp, distant fog colour and distance, ScreenTint, terrain lightmaps and the
light pass (CRA3 2018 / 2418 shade bands match once the camera is pinned), rider lighting and skinning (`riders1.png`), the
rider shadow (Snow Jam 418: PS2's second shadow is a computer rider's), HUD sprite positions (orb, progress bar, arrows),
WebKit vs Chrome.

## 4. Race clock and speed (pv `raceHud`)

- **Original:** the race HUD 0x1F03A0 draws the clock at 0x1F0F34: 0x1F16C0(owner, +0x17C / +0x180 / +0x184 = hours,
  minutes, seconds of 0x21F81C, flag (+0x3CC >> 3) & 1, descriptor 0x1A, colour = the red 0x4C8688 (A,R,G,B 1,1,0,0) when
  owner +0x3D0 >= 0.5) -> 0x1F1840: each field "%02d" (0x4A22F0) in the HUD font (owner +0x42C) at the descriptor scale; the
  group is pair x 3 + (gap + 2 + 2) x 2 = 125 wide and the font height 21 high, aligned on the descriptor (320, 20, centred,
  top) with 0x1F10F8; a pair at pen + `one` (4.75) per '1' digit, pen += (pair + 2) x scale; each colon (owner +0x430) at pen,
  pen += (gap + 2) x scale; shadow (2, 2) through 0x391CB0. The speed is the widget 0x2200C0 (called at 0x1F0274 with the
  HUD layout, whose +0x384 = descriptor 25: (20, 460), left / bottom, scale 1): the label "MPH" / "KM/H" (0x4A2310 /
  0x4A2308, profile 0x535610 bit 19) measured (0x391FB0) and aligned (scale 1) and drawn with the shadow; the value
  trunc(|v| x 0.036 (x 0.621) + 0.5) as "%d" at the descriptor scale x 1.3623675 (gp-0x5318, fewer than 3 digits) or
  1.121412 (gp-0x5314), centred on the label (0x392908), its top at the label top - (21 x scale / 2 + 14.003287 (gp-0x5310)).
- **Before:** `ui.text` (the page's glyph blitter: a flat tint, no shadow) at hand-placed sizes: the clock 21 / 17 of the
  font, the speed 27 / 17 centred at x 32 (its first digit clipped at the left edge), "MPH" at 11 / 17.
- **Fix:** `web/trick-hud.js` `raceClock()` / `speed()` build the original draw items; `web/ui.js` (race HUD) and
  `web/career-ui.js` (the freestyle clock, with the red override) render them with the HUD's own renderer.
- **After** (HUD region MAD vs PS2, before -> after, Chrome / WebKit): CRA3 418 clock 57.9 -> 15.7 / 58.5 -> 16.4, speed
  29.6 -> 13.5 / 29.9 -> 14.0; CRA3 1218 clock 62.1 -> 30.2, speed 32.1 -> 14.3; ERA5 419 clock 49.8 -> 16.5 / 50.2 -> 16.9,
  speed 50.5 -> 12.6 / 51.2 -> 13.0; BRA2 418 clock 78.4 -> 22.6 / 79.6 -> 23.5, speed 42.2 -> 11.7; BRA2 1218 clock 71.8 ->
  20.7, speed 55.9 -> 17.9; Snow Jam 1219 clock 51.3 -> 17.1, speed 28.7 -> 14.9. The rest is the PS2's 512 -> 640 resample
  (item 8). Crops: `local/browser-validation/visual-parity/fixes/rt-top.png`, `rt-speed.png` (PS2 | after),
  `hudfix-bra2.png` (PS2 | before | after Chrome | after WebKit).

## 5. HUD text colour (pv `hudText`)

- **Original:** 0x391CB0 hands the font state to the renderer's text draw (vtable +0x284 = 0x378808), which writes the
  glyph RGBAQ as trunc(colour x 204.0) (0x434C0000) for R, G, B and trunc(alpha x 128) for A, over font texels of unity
  0x80: white text lands at 204. The front end's LUI text has the same 0.8 (docs/first-load.md, item 6).
- **Measured:** PS2 HUD text peaks at 204,204,204 on every course (score, clock, speed, percent) and the red OPPONENT line
  (0x4C88C8 = 0.783, 0.061, 0.041) at 158,12,8 (0.8 x). The browser drew 255.
- **Fix:** `TrickHudRenderer.text` tints font glyphs with `hudTextByte` = trunc(colour x 204); sprites keep the 128 unity
  (the orb and the progress bar reach 255 on the PS2 too). The progress meter percent goes through the same renderer.
- **After:** HUD score / percent regions 18.8 -> 15.5 and 22.6 -> 17.9 (CRA3 418); the speed digits' blue channel
  0xC (PS2) vs 0xF before, 0xC after.

## 6. UI atlas alpha (assets re-derived, in place)

- **Cause:** `tools/sam_ps2 --export-browser-ui` wrote A = min(255, 2 x A) for every non-font SSH image, but the SSX
  library's OldShapeHandler already doubles a palette whose alphas are all <= 0x80 (`AlphaFix`): FE_1 / OV_1 / SU_1 (8-bit
  palette images) came out at min(255, 4 x raw) (verified on every texel of all 32 images; MENU, 32-bit, is 2 x raw and was
  right). The GS blends with As = raw / 128, so every translucent texel was up to 2x too opaque.
- **Fix:** 25 re-derived PNGs (RGB unchanged, A = min(255, 2 x raw) from `local/browser-ui/*.SSH`):
  `local/browser-validation/visual-parity/ui-alpha/new/` (the old files in `ui-alpha/old/`), script
  `local/browser-validation/visual-parity/tools/uialpha.py`; the exporter uses `AlphaFix` now (`tools/sam_ps2/Program.cs`).
  Changed: FE_1-7..21, OV_1-0..7, SU_1-0..1.
- **Checked** (the files served in place of the current ones: Chrome by CDP request interception, WebKit through a local
  proxy): Uber coil region MAD ERA5 419 23.1 -> 15.7 (Chrome), 23.7 -> 16.5 (WebKit); BRA2 418 30.5 -> 26.2 / 31.2 -> 26.8
  (`coil3.png`: PS2 | before | after). Front-end screens, whole-frame MAD vs the PS2 (Chrome / WebKit): Main Menu 6.20 -> 5.34
  / 6.28 -> 5.39, Options 8.23 -> 5.81 / 8.36 -> 5.91, Select Character 11.63 -> 10.86 / 11.79 -> 10.99, Setup Character
  6.64 -> 5.76 / 6.62 -> 5.85, Select Peak 9.05 -> 8.30 / 9.17 -> 8.40 (`fixes/fe-shots/`).
- **Switch:** an asset change; the loaders (ui.js and ten screen modules) read the atlases by name, so the switch is the
  copy itself, and `ui-alpha/old/` restores the previous look.

## 7. Regression checks

`web/test-visual-parity.mjs` (npm test): the clock and speed draw lists against the disassembly numbers (group at x 257.5,
pitches 39 / 5, '1' shift 4.75; label at (20, 460 - h), number scale 1.3623675 / 1.121412, centred 14.0033 above the label
middle); the text byte trunc(colour x 204); the switch defaults; a pixel check that software-renders both draw lists
(bilinear glyph blits, shadow first) over the PS2 frame `peak2/cra3-full.tick418.png` (which shows "00:00:03" / "60 MPH")
with the display's horizontal resample: box error clock 12.6 / speed 7.7 (the old text path 36.5 / 12.2, white text 19.7 /
12.9); the OPPONENT line over `peak2/dss2-full.tick418.png` (9.1, the old text 24.2); the finish banner layout (sprite at
(213.5, 169.5) 213 x 41 with the 'fini' UV rows, the value centred at x 320, top 210.5, scale 1.9075785; TIME'S UP at (200,
159.5) 240 x 41) and, with the sprite blitted from OV_1-3, its box error over `ctm-parity/runs/race-f/fin.png` 6.8 (the
same banner at descriptor 0x1B 37.5), `runs/pipe-finishov.tick4408.png` 4.8 (31.2) and TIME'S UP over
`runs/pipe-brake.tick7519.png` 4.0 (at 0x42 19.5); the UI atlas alpha rule against the SSH palettes; the LUI layout of
43final_standings, 61toptimes, 70peakchal_results, 40race_pre, 62reward_list and 68rival_pre against the PS2 positions
(`VP_RESULTS_JSON` points it at a staged export; a screen missing from the served export reports PENDING).

## 8. Crash recover meter (traced; ported in section 13)

0x1EFC34 calls 0x21D9A0(owner, the HUD layout, a2 = the recovery state (+0 a float count, +4 a flag), a3 = the player, t0 =
the button flag, t1 = a pulse scale; f12 = the fill (slot 0xB ratio), f13 = the fade (slot 0x21 'RECOVERED!'). It draws the
button icon (owner +0x48 / +0x4C by the pad type), the label through 0x1E95A0 at descriptor 55 (320, 435) with the owner strings
+0x6AC / +0x7F8, then the bar at descriptor 54 (320, 448, 154 x 12) as sprite quads from owner +0x4D4 (ends) and +0x4D8 (the
filled middle, width fill x 9 x scale), vertex alpha trunc(fade x 128). The browser's placeholder (web/trick-hud.js 'recover':
an outline and a red fill) stays; porting it needs the owner sprite records from a savestate (tools/probe_trick_hud.py style).

## 9. Results panel and in-race cards (pv `luiResults`)

- **PS2:** the results overlay draws two OV.LUI screens: `OV_darkblue` (0x083818F5; the panel shapes, the header picture
  OV_1-6, the blinking lights; open animation to frame 60, lights 65..499, close 500..560) under `43final_standings`
  (0x00286583; track_event (the event, 85 %) and title (the round, 75 %), the Rank / Riders / Time|Score heads in white, six rows
  at 20 px in teal 101,184,201, the gold / silver / bronze hex, helptext, the overlayMenu with focus frames 40 + 10 i: the
  focused row white and ps2x beside it). The human's row is 255,204,153 (PS2 'Zoe' / '03:55' 203,163,122 = 0.8 x).
- **Before:** `career-ui.js panel()` drew a filled rectangle with a 3 px outline and the picture strip; `drawResults` placed
  its text by hand, bigger, with an orange human row; every card (objectives, rival, peak objectives, records, rewards, peak
  results) used the same panel.
- **Fix:** `tools/export_results_screens.py` -> `web/public/assets/UI/results-screens.json` (staged:
  `local/browser-validation/visual-parity/results-lui/results-screens.json`); `web/results-lui.js` (LuiScreen, the lights loop,
  the open animation on the first frame a panel is drawn); `career-ui.js`: `panel()` draws OV_darkblue with the two header
  lines for every card, `drawObjectivesOpen` plays OV_darkblue's own open before the card text, `drawResults` fills
  43final_standings (rows, medal hex on the human's row with the medal's cell, message, menu items, focus, disabled items at
  half alpha). Behaviour (rows, items, order, actions) is unchanged: only the drawing; without the export the old drawing runs.
- **After:** `local/browser-validation/visual-parity/fixes/res-cmp.png` (PS2 | before | after: Snow Jam final and qualifier),
  `res-cmp2.png` (peak results, records, objectives card, a single event), `res-wk.png` (PS2 | Chrome | WebKit). The panel,
  header lines, rows, message wrap and menu land on the PS2's pixels; WebKit = Chrome.
- **Records and peak results** (same switch, second export): `61toptimes` (0x067DF2A3: track_xxx, Top 5 Record Times |
  Scores, Rank / Name / With / Time|Points over five rows at 26 px, the new-top-time message, the Continue / Save Records
  menu with focus frames 40 / 45; the player's row 250,135,18: PS2 'PLAYER 1' / 'Zoe' / '22:10' at 200,108,14) and
  `70peakchal_results` (0x056B3173: title1 / title2, 'Event Results', the right-aligned Time|Score to beat / Your time|score
  labels at 328, helptext, the Transport / Restart / Quit menu with focus frames 50 / 60 / 70). `career-ui.js drawRecords` /
  `drawPeakResults` fill them (items and actions unchanged: without a top time the menu is 'Return' alone, as before).
  `res-cmp3.png`: PS2 | Chrome | WebKit for the peak result, the records and a new top time.
- **Race round card** (same switch, third export): `40race_pre` (0x079D8885: track_event, the round at 116, objTextLine1
  (480 wide, 55 %, teal) with its bullet sprite, 'Riders' right-aligned at 243 over six names 23 px apart, 'Record time:'
  right-aligned at 216 with the value at 222, Continue). The human's name 252,177,101 (PS2 menus/race/state-round-objectives.png
  'Zoe' 202,142,81 = 0.8 x). `career-ui.js drawObjectives` fills it for races (not freestyle / rival / peak cards);
  `card-cmp3.png` (PS2 | Chrome | WebKit).
- **WebKit seams:** OV_darkblue's body is a translucent 19-triangle fan; filled one triangle at a time, WebKit's
  anti-aliasing left light lines along the shared edges (radiating from the fan's corner, `cardz.png`). `lui-player.js` gains
  an opt-in `unionFlat`: a shape of one flat colour fills all its triangles as one path (one winding, nonzero), which the
  results screens use (`cardz2.png`: Chrome | WebKit, no seams). Other LUI screens are unchanged.
- **Rewards list and rival / peak run cards** (same switch, fourth export: staged at
  `local/browser-validation/visual-parity/results-lui/results-screens.json`, 7 screens, copy pending; the code is in place
  and draws the old way until the served export has the screens):
  - `62reward_list` (0x0BD7AB44, overlay 0x10): title_rewardlisting, 'Rewards', the intro line, nine rows 22 px apart from
    (120, 184), the up / down arrows, Continue with ps2x. An award's items are indented "      %s" (0x1FF7B8); `lui-player.js`
    gains an opt-in `keepLead` (the text's leading spaces stay on its first line, as the PS2 font draws them). 0x1FFD08: a list
    of fewer than 10 lines paints every row teal (1, 101,184,201 / 255 from gp-0x553C) and takes no Up / Down (0x1FF798
    returns 0), so no row is white (PS2 `ctm-parity/runs/race-f/final.png`); a longer list shows the focused row white and
    the down arrow (`race-f95/final.png`). `career-ui.js drawAward` fills it (items and actions unchanged).
  - `68rival_pre` (0x0C359585, 0x1FD190): title1 / title2, the headline (wrapped at 475), obj1 / obj2 with bullets, the target,
    Continue. `drawRivalObjectives` (backcountry rival) and `drawPeakObjectives` (peak runs) fill it; PS2
    `peak1/peak1-race-objectives.png`, `peak3/nav/out-era5-load/final.png`.
  - `fixes/cards4/final-sheet.png`: PS2 | Chrome | WebKit for the short and long rewards lists, the Peak 1 Race card and the
    The Throne rival card (staged: the world is not drawn behind a staged card, and the staged career has no rival name).
- **Header lights:** OV_darkblue's two blinking lights (shapes 0760d5c2 / 0760d5c3) take their animations' x / y, which are
  their groups' layout (x 12, y 10; x -280 -> 13), and landed in the top-left corner (24, 20) as a faint slanted mark (seen on
  the staged cards over an empty background) and off screen. The PS2 shows them at the header's top right (x 448 / 463, y 73,
  lit in turn). `results-lui.js` leaves them out until that layout is traced.
- **Left:** ~~the freestyle pre-event card (41freestyle_pre) keeps its hand-placed text inside the new panel~~ (2026-09-27, front-end agent: the CTM heat cards draw 41freestyle_pre and the heat results 42freestyle_standings with pv `fsStandings`, web/fs-standings.js, ctm-parity.md); the header
  lights; the gap the PS2 leaves after the '$' in "Cash: $ 10,000" (not traced).

## 10. Finish / time-up banner (pv `finishBanner`)

- **Original:** the race HUD 0x1EC3F8, with state +0x88 = 2 (time up) at 0x1ECB48 or 1 (finish) at 0x1ECB70, calls 0x21F660
  (t0 = 1 for time up). A finish draws the 'fini' sprite (owner +0x4B4, OV_1-3 u 1.5..132.5, v 1.5..25.5) at descriptor
  **0x42** (320, 190, 213 x 41, centred; `addiu a0, 0x42` at 0x21F6B0); time up draws 'timeup' (+0x49C, u 1.5..170.5,
  v 127.5..150.5) at descriptor 0x1B (320, 180, 240 x 41). The sprite goes through 0x1F10F8 (alignment) and 0x1F1190. For a
  finish the value follows, centred (0x392908) at the sprite's middle x and bottom y in the HUD font at the descriptor scale x
  1.9075785 (gp-0x5340), white, shadow (2, 2): races "%02d:%02d:%02d" (0x46ED68) of the result time 0x536640[slot] (the same
  time as the results: 14129 ticks = 00:03:55 in `ctm-parity/runs/race-f/fin.p2s`), freestyle "%d" of the score.
- **Before:** races drew `s.message` ("FINISH!") in the page's text at size 24, no time; freestyle and TIME'S UP drew the
  sprite at 240 x 41 around (320, 180) in 448-line canvas units (27 px too wide for a finish, 11 px low), the score in the
  page's text.
- **Fix:** `web/trick-hud.js finishBanner()` builds the sprite and value items; `web/ui.js` (races, peak runs) and
  `web/career-ui.js hud()` (freestyle) render them. `main.js` passes `finishTicks` (the finish time with penalties, which the
  results show) with the HUD state.
- **After:** `fixes/finish-banner/finish-cmp.png` (PS2 | before | after Chrome | after WebKit, for a race finish, a freestyle
  finish and TIME'S UP): the letter rows and columns of the orange sprite and the value's box match the PS2 frames to the
  pixel in both browsers (sprite letters 217..421 x 174..205, value 195..443 from y 216).

## 11. The sweep after the fixes

The 65 pairs again (`local/browser-validation/visual-parity/sweep2/`, the same pads, camera pin and lag), mean MAD against
the PS2 frame, before -> after (Chrome / WebKit): world centre 14.35 -> 14.34 / 14.48 -> 14.47 (no world changes); clock
region 62.1 -> 26.5 / 63.1 -> 27.1; speed region 43.7 -> 27.7 / 44.1 -> 28.1; score region 24.8 -> 20.5 / 25.7 -> 20.7. What
the HUD regions keep is the world behind the text and the 512 -> 640 display resample.

## 12. Metro City "light shafts" = the air streamers (pv `streamers`)

Round 2 (2026-09-27). Work files: `local/browser-validation/visual-parity/round2/` (`shafts/NOTES.md`, the PS2 runs and sheets).

- **What they are.** PS2 bisection on `setpieces-bra2/full` tick 418 (`tools/ps2_capture.py build --poke` on the countdown anchor):
  the beams survive with the static models (37E238), terrain (38CA70 / 38CE20), light pass, env layer, halos (2E2868), sun /
  lens / glare, snow / wake / boost draws and every entity draw (0x356298, a MIPS stub bisecting by resource,
  `shafts/bisect/skipdraw.py`) turned off; they vanish with the GS additive enum 7 (0x48) neutralised and with **2EF950**, the
  air streamers (RFX+0xAD0). They are the two ribbons trailing the board after the start jump (`strm`, alpha 0.3). The sky
  beams at the start (metro-event-race 318, metro-event-start) are the class-3 LiveComp searchlights (section 17).
- **Why faint.** (a) The ribbon's T runs scroll + k / count up to ~1.8 (clamped) and the PS2 shows it bright over its whole
  length, brightest at the old end (CRA3 6819 PS2 sheet): the GS samples `strm` with its bright rows at T = 1, where the export
  has them at T = 0. With the exported order the browser drew a short bright stub and a dark tail. (b) 2EF6D0 takes the nose /
  tail offsets (90 x row 0) and 2EF950 the half width (7.5 x row 2) from the board bone's render matrices (rider+0x780 -> +0x34:
  the world rows x the rider geometry scale 0.85; PS2 tick 418: a ring pair 153 cm apart), the port the unit world rows.
  (c) The scroll moves 0.55 per tick at 3333 cm/s, so its phase differs with any timing difference (PS2 0.7676, browser 0.353).
- **Fix.** `web/boost-renderer.js` flips `strm`'s rows (pv `streamers`); `web/boost_gameplay.inc` `set_rider_fx_render_scale`
  (new core export, shared word, allowlisted in check-rider-globals) scales both by the rider's `graph.scale` when on;
  `main.js applyPresentationFast` sets it on the human's and the computer riders' cores each frame.
- **After** (setpieces-bra2 418, centre MAD vs PS2): 18.59 -> 17.66 (Chrome) / 17.78 (WebKit); the beams are where the PS2
  draws them (`shafts/strm-final.png`: PS2 | before | Chrome | WebKit; `strm-fix.png` the streamer contribution PS2 | browser).
- **Test.** test-pickup-fx: with the switch the strip width and the nose / tail spacing are 0.85 x (12.75 cm, 153 cm);
  test-visual-parity R5: over T = s..s+1 the flipped `strm` is bright for any scroll, the exported order dark.

## 13. Crash recover meter (pv `recoverMeter`)

- **Original** (0x21D9A0 from 0x1EFC34; trace `round2/recover/SPEC.md`): the label "RECOVER = @square" (owner +0x6AC / +0x7F8,
  identical in this game) laid out by 0x1E92A8 and drawn by 0x1E95A0 at descriptor 55 in FEFONT x 0.7 — "RECOVER = " at
  (247.2, 414.7), the square icon (OV_1-2, owner +0x4DC / +0x4E0) at (371.8, 414.85) 21 x 20 — only while recovering (no
  RECOVERED! slot) with flags bit 24; the bar at descriptor 54 (320, 448, 154 x 12): the button 20 x 20 at (243, 438), the ends
  9 x 12 (OV_1-4 +0x4D4, u 151.5..159.5, the right one mirrored) and the middle 113 x 12 (+0x4D8), vertex alpha trunc(fade x 128);
  the fill an untextured red quad (0x1F1338) at (269, 445), 125 x fill wide, 6 high, one layer up. RECOVERED!: the bar full red,
  pulsing below ratio 0.3, fading 0.3..0.5.
- **Before:** an outline with a red fill (`trick-hud.js` placeholder).
- **Fix:** `trick-hud.js recoverMeter` / `iconLayout` / `iconLabel`, a `quad` item kind. The owner records come from
  `trick-hud.json` (`tools/probe_trick_hud.py` now exports `recoverEnd` / `recoverBar` / `recoverButton` and `strings.recover`).
  **Asset copy pending:** `round2/recover/trick-hud.merged.json` (the served file + the snippet; without it the placeholder
  stays). Keyboard players see the key cap in the label's icon place (as the Uber hint).
- **After** (setpieces-bra2 1218, meter box MAD vs PS2): 65.2 -> 25.1 (Chrome, gamepad glyphs) / 28.3 (Chrome, keyboard) /
  28.5 (WebKit, keyboard) (`round2/recover/meter-chrome.png`, `meter-wk.png`, `meter-pad.png`); the rest is the world behind it
  (the rider is 0.8 m off the PS2 line there) and the 512 -> 640 resample. Offline over a plate: 68.1 -> 16.9 (tick 1218),
  46.4 -> 20.3 (score-air-tricks 778, the red fill).
- **Uber hint (pv `uberLayout`, on):** `uberHint()` laid its string out 216 wide (the draw-time shadow in the widths and an
  empty last piece) where the PS2 record owner +0x560 is 209.99992 (144.9 / 28 / 16.1 / 21): the text started at x 212 instead of
  215. It now uses `iconLayout` / `iconLabel` (key caps keep the old measure: they are wider than the icons). Hint box error over
  PS2 uber-chain tick 929 (a new capture frame, `local/ps2-capture/runs/uber-chain.tick929.png`): 12.3 -> 6.3; the page's own
  renderer in Chrome: `round2/build/uber/hint-cmp.png` (PS2 | before | after).

## 14. Freestyle standings rows (pv `hudStandings`)

- **Original** (type-7 case 0x1ECFFC, rows 0x1ED104..0x1ED4AC, builder 0x1EB160; `round2/standings/SPEC.md`): descriptor 0x17
  (20, 20), HUD font at 204 grey with its (2, 2) shadow, rows 21 apart; the values at x + 66.997185 right-aligned on the widest
  row; posted rows '1'..'3' (the row index) with 'ST' / 'ND' / 'RD' at half scale, x + 1 + the digit width, 1.5996 px lower;
  the player's row (score >= the posted one, once) shows the rider's name cut to 3 characters over a red 0x4C8628 box one layer
  down (cheat riders from 0x43FA38). Checked against 6 savestates' row texts / offsets / widest width.
- **Fix:** `trick-hud.js standingsRows` / `standings` / `riderName` and a `rect` item; `career-ui.js hud()` uses them.
- **After** (PS2 pipe-brake 2018, MAD over the PS2 glyph pixels): 90.3 -> 23.4 (Chrome) / 25.5 (WebKit)
  (`round2/standings/st-chrome.png`, `st-wk.png`); offline box error 22.1 -> 13.3 (7 frames: 15.3..22.1 -> 13.1..15.3).
- **Player's row** (PS2 `menus/transport-map/heats/h2-scorepoke/sample00303`, the human's score poked to 400000: 'ZOE 402000' over
  the red box, then '2ND 328320' / '3RD 186960'): the box covers x 20..205, y 20..39 of the 480-line frame on the PS2 and x 20..205,
  y 19..37 of 448 lines in Chrome and WebKit (`round2/hrow/cmp.png`, `cmp-wk.png`); checked in `test-visual-parity.mjs` R2.
- **Open:** the PS2 also ranks the player's previous attempt after a
  restart; the round-2 carry's extra condition (0x535C10 in 1..3) is not modelled.

## 15. OV_darkblue header lights (pv `luiLights`) and "Cash: $ 10,000" (pv `cashGap`)

- **Lights:** the exporter paired OV_darkblue's frame-0 records with the element definitions by index; the screen sets
  00e91994 twice (50 records, 49 definitions), so from element 12 every element took its neighbour's record (the lights' groups
  got the lights' vertices). The runtime applies them by name (0x39CD30 -> 0x39D860). `tools/export_results_screens.py` now
  pairs by name (`state0: 'byName'`); with the served export `results-lui.js` takes the two groups' layout from `LIGHT_GROUPS`
  (433, 57) / (449, 63). The six lights sit at (446 / 461 / 498 / 513 / 549 / 564, 73); the light loop (65..500) starts at panel
  frame 50 and turns every 436 frames (c2 peaks at 92 / 172, c3 140 / 220, ...); the shape draw uses the vertex alpha only
  (no squared alpha). Four savestates: the predicted loop position and all 24 light colours match (within 1); pixel position
  error (0.5, 0.25) px. Chrome = WebKit (`round2/lights/panel-burst.png`, `wk-check.png`).
- **Cash:** the rewards line is "Cash: %S" with 0x198AF0's "$ 10,000"; the browser built "$10,000". `career-ui.js` uses
  `money()` with the switch: '$' -> '1' 11.1 px (PS2 11.4 / 11.3, before 7.3). `web/ctm-flow-ps2.json`'s transcription of the
  PS2 rewards screens had dropped the space too (fixed).

## 16. Results menu layout and focus (pv `resultsMenu`)

- **Wrap:** the PS2 wraps a LUI text only when its element has flag 0x80 (0x3A0528 -> 0x3A0D00; otherwise 0x3A0EB0 breaks at
  line breaks only). The 43final_standings items (flags 0x24c, 80 px at 50 %) never wrap; `lui-player.js` wrapped every text,
  so "Next event" (85.9 px) and "Final Round" (90.9 px) broke onto Restart. `lui-player.js` gains an opt-in `flagWrap`, which
  `results-lui.js` turns on for its seven screens. PS2 "Next event" ink x 462..546, browser 461.5..546.9.
- **Focus:** the 43final_standings hook 0x1E7558 (vtable 0x474000 slot 0x40) moves overlayMenu to item 1 (39B960(menu, 1))
  when the game type byte 0x535C11 is 1 (Single Event), with no outcome test: a Single Event opens on Restart (PS2
  menus/replay/bhp1-neutral, nav/bc/out-jam-finish); the career results and peak runs open on item 0, back from Records too
  (the browser went to Records). `career-ui.js resultsFocus`.
- **After:** `round2/results2/menu-cmp.png` (PS2 | before | Chrome | WebKit): one line, cursor on Restart.
- **Not changed:** the PS2 menu wraps around (three Ups from the top land on Replay); `ui.js` clamps at the ends.

## 17. Additive world models in byte space (pv `byteBlend`: removed 2026-09-30, moot with `encodedBlend`, section 38)

The static-model additive class (ALPHA 0x48, model flag 8) could draw in the encoded pass after the fog composite
(the removed `world-material.js registerWorldAdditive` and `web/fog-shared.js`: the contribution scaled by the fog alpha at its depth, as the
composite fogs Cd + Cs*As): Metro City 318 searchlight pixels 62 -> 67 (PS2 94), centre MAD 7.72 -> 7.65. The beams' remaining
gap (40-60 % of the PS2's contribution; geometry and LiveComp phase match) is not the blend space: their texture alpha
(palette 52 -> 104, correct) and vertex colour (c5 7) give Cs*As <= 46 in bytes, the PS2 shows up to +61. The entity draw path
(0x356298 -> virtual calls) reaches the static-model draw 37E238 (PS2 metro-event-race 318: with either 37E238 or 0x356298
off the beams are gone), so the combine is the static one; the beam CLUT is uploaded raw (alpha 32 / 50 found in GS memory, not
doubled) and the additive draws are TCC 1 / MODULATE: the PS2 beams are still 1.77x the browser's over the browser's beam pixels
(metro 318), cause not found. Left off (no visible gain, and it keeps the encoded pass busy on every searchlight course).

## 18. Terrain mip levels and LOD (analysed, not changed)

- **PS2:** world textures carry their own mip levels (673 of 788 BAM.SSB kind-9 records: 128x128 PSMT8 3 extra levels down to
  16x16, 128x128 PSMT4 2 to 32x32, 64x64 PSMT4 1; header byte 15 >> 4 = the count); `world.tex` exports the base level only.
  The terrain draws (EE RAM render queue, setpieces-bra2 tick 418: the A+D blocks with ALPHA_2 0x81) set TEX1_1 = LCM 0 (LOD
  = log2(1/Q) + K: from depth only), MXL 1..3, MMIN linear-mipmap-linear, K per patch -10.8 .. -14.4; the light page TEX1_2 =
  0x61 (level 0, bilinear). Static models use the same form (K -11 .. -14.5).
- **Browser:** a full generated chain, derivative LOD, anisotropy 4.
- **Measured** (DRA4 418, centre MAD vs PS2): now 20.51; no mips 21.01; box mips capped at 16 px 20.53
  (`round2/mip/dra4-mip.png`: PS2 | now | no mips | capped). The PS2's grain at mid distance is closer with no mips, the MAD is
  not: a depth-only LOD needs the per-patch K (not exported) and brings the PS2's aliasing. Left as is; notes and the
  experiment modules in `round2/mip/`. The K is per texture, not per patch: section 28.

## 19. The sweep after round 2, and the ranked list now

The 65 pairs again in Chrome (`round2/sweep3/`, sheets `round2/sweep3-sheets/`): world centre MAD 14.34 -> 14.28 on the same
frames (no regression; setpieces-bra2 418 18.58 -> 17.66 from the streamers, ass1 4420 46.0 -> 42.1). The WebKit pass could not
run: from ~00:45 WebKit windows got no requestAnimationFrame (the display had gone to sleep; offscreen windows too), so the
WebKit checks of this round are the per-fix ones made before that (sections 12-16) and the Uber hint shares the recover label's
draw path. What the pairs still show, ranked:

| # | difference | where | status |
|---|---|---|---|
| 1 | Searchlight beams (class-3 LiveComp models via 0x356298 -> 37E238) 1.77x brighter on the PS2 | Metro City, Kick Doubt starts | **fixed** in part (pv `additiveNoZ`, section 20): glow 1000 PS2 / browser 1.65 -> 1.31; the rest is texture LOD |
| 2 | Board spray: the browser's near chunks look like flat ovals, the PS2's like tall drops | Snow Jam 1219 | **not a difference**: both draw the same chunk sprites; the emitter's shared flipbook frame (tmb1..8, tmb2 / tmb6 are the flat ones) starts at a random phase on the PS2 (370E14 -> 3177F0 at construction), section 20 |
| 3 | Terrain mid-distance grain (depth-only LOD with K per texture, 1..3 mip levels) | every course | analysed, not changed (sections 18, 28: K found, drawing it measured worse) |
| 4 | Results menu wraps around on the PS2 (3 x Up from the top lands on Replay); the browser clamps | results | **fixed** (pv `resultsMenu`, section 20) |
| 5 | Freestyle "Heat1 Standings" screen | career freestyle heats | another agent (pv `fsStandings`) |
| - | Career-only HUD (freestyle clock / standings in the pipe, EA RADIO popup), computer riders, particles, the rider leaving the PS2 line after 1000-2500 ticks | many pairs | harness limits (section 1) |

## 20. Round 2, batch 3: the WebKit sweep, additive models without depth writes, the menu wrap, the spray

- **WebKit sweep** (the 65 pairs, display awake): world centre MAD 14.47 -> 14.41 (Chrome 14.42 -> 14.36 on the same frames);
  Chrome vs WebKit 0.94 mean, 2.66 max. The Uber hint in WebKit: text error over the PS2 frame 66.6 -> 43.6 (Chrome 65.4 ->
  42.6; `round2/build/uber/hint-cmp-wk.png`: PS2 | before WebKit | after WebKit | after Chrome).
- **Additive static models (pv `additiveNoZ`, on):** the render queue of the PS2 (metro-event-race tick 318, EE RAM) shows every
  ALPHA 0x48 static-model block with TEST 0x5114d (ATST GREATER 20, AFAIL FB_ONLY, ZTST GEQUAL) and ZBUF 0x1010000e0 (ZMSK 1: no
  depth write). The port drew the class as two passes, the first writing depth where alpha > 20, so the cones' front faces hid
  their back faces. Now one additive pass, depth tested, no depth write (`world-material.js`). Traced on the way: the searchlight
  beams at the Metro City start are the `searchlightglowa_*` models (ParentModifier on `searchlightbasea_*` node 3), drawn through
  the entity draw 0x356298 -> renderer +0x2F8 (37E238) with the instance colours (+0x98 DMA chain: doubling their V4-5 colour
  words doubles the PS2 beam, 1.94x; the `searchlight_white_sb*` beams are not in view there); their CLUT reaches the GS
  unchanged (alpha 32 / 52 found in GS memory). One glow's contribution, PS2 / browser: sum 1.65 -> 1.31, pixel slope 1.39 ->
  1.18 (`round2/blend/glow-contrib.png`, `glow-contrib-nz.png`: PS2 | browser). The rest: the PS2 samples a coarser level
  of the dithered texture 320 (smooth) where the browser shows its dither (section 18). Metro 318 centre MAD 7.61 -> 7.52
  (Chrome) / 7.88 -> 7.73 (WebKit), `round2/blend/nzs/nz-crop.png` (PS2 | before | Chrome | WebKit); no change on the other 16
  first snaps.
- **Results menu wrap (pv `resultsMenu`):** the LUI menu's Down 0x39AB50 / Up 0x39AC48 step to the next item whose element flags
  lack 0x20 (greyed) and go round the ends when the menu element has flag 0x80 (43final_standings / 70peakchal_results /
  42freestyle_standings overlayMenu 0x2cc / 0x2c9 / 0x2cf, 61toptimes 0x2c0). `career-ui.js key()` does that for the results and
  records screens. Staged final results in Chrome and WebKit: Up, Up, Up, Down x 4 from Transport -> 4, 3, 1 (Replay greyed:
  skipped), 3, 4, 0, 1 (flag off: 0, 0, 0, 1, 2, 3, 4).
- **Spray:** the PS2 frames show the same near chunks (PS2 run with the rider snow draw 371688 off: `round2/spray/ps2/snow-diff.png`);
  their shape is the emitter's shared flipbook frame, whose phase the PS2 draws at random when the emitter is built. Not a
  difference.

## 21. Kicker snow at a free-ride start (pv `sprayReset`) and the rival card's wrap (pv `luiWrap`)

- **Report** (Owen, phone, low tier): right after the first spawn in Conquer the Mountain the snow spray "seemed to be going the
  wrong direction". Reproduced in the real new-career flow (title -> CTM -> Select Character -> movie / plane -> ride, 844 x 390,
  `?quality=low`, `round2/spray/ctmflow.mjs`): for the first ~70 frames of the plane drop a column of snow rises from the
  falling rider (`round2/spray/cmp2.png`: PS2 f02747 | before | after).
- **Not the fast path:** two identical cores, exact vs presentation fast (`web/presentation_fast.hpp`), over 900 ARA1 ticks give the
  same particle counts on every tick and positions within 0.08 cm of 228,000 cm (`round2/spray/fastcmp.mjs`); `?fastfx=0` shows the
  same column as `?fastfx=1` (68 / 72 frames with kicker particles).
- **Not a position delta:** the snow births take the rider's velocity, not a position difference; the birth ring keeps its old births
  over a placement on the PS2 too (the FX reset 2DF3B0 -> 2E2550 only sets the emitter enables).
- **Cause:** the kicker (carry-off, emitter 8) buildup FX+0x10. The port starts every run with the seed state's value
  (`initial.json`, the Snow Jam race start: 1.37), and a rider in the air with more than 277.8 cm/s sprays it off at the board at
  0.4 x its velocity, so the snow rises away from a falling rider. On the PS2 the DynamicSpray reset 2DF3B0 (from the constructor
  2DE4A8 -> 2DF190 and from the rider FX reset 111890 of every placement: 11D390, 11DF18, 123210, 1234D0, 123640) writes
  FX+0x10 = 0; `menus/fr/ctmstart` f02700 / f02747 / f02793 (rider 0x1456D40): 0 through the whole fall, then 0.42 / 0.88 / 1.36
  as it builds on the snow after landing. The station spawns (place_rider_region) sprayed it too while briefly airborne.
- **Fix:** `animation_bridge.cpp` `set_fx_reset_kicker` (per rider context): the placement's FX reset (reset_snow(false)) zeroes the
  buildup, event starts keep the seed's; `rider_fx_reset` (111890's FX set) for a free-ride start that is not a region placement
  (the plane drop), called from `main.js resetPhysics`. Core check (`round2/spray/kick.mjs`): a rider teleported into the air after
  400 ticks sprays kicker snow on 76 of 90 airborne ticks before, 0 after; an event run with the switch on or off gives identical
  snow buffers (2.3 M values). Browser, CTM plane drop: frames with kicker particles 68 -> 0 (Chrome), 0 in WebKit; station 17
  spawn 46 -> 0. The PS2's own white streak at f02747 is the air streamer: its ring (RFX+0xAD0 -> 0x1462570, 25 points of
  board - 0.7 x root) fills from the drop (count 4 at f02700, 25 at f02747 / f02793, 0 after landing; motion mode 1) exactly as the port's does, so
  both draw the air streamers along the fall. What differs is the camera: at f02700 the PS2 looks steeply down the slope from
  behind the rider, the port looks out at the horizon (`round2/spray/zoom2700.png` vs `ctm-live/r000.png`), which is why the
  streamer shows as a line above the rider in the port. Open (the drop's camera, not the spray).
- **Shipped** (sprayReset and luiWrap on, live core 13:22): CTM flow at 844 x 390, `?quality=low`, live defaults: 0 kicker frames in
  the fall in Chrome and WebKit (`round2/spray/ctm-live`, `wk-live`). Checks: `test-kicker.mjs` (PS2 ctmstart FX+0x10 = 0 x 3;
  a rider placed in the air: 60 -> 0 kicker frames), `test-visual-parity.mjs` R2 (the player's row) and R9 (rival wrap).
- **Rival card wrap (pv `luiWrap`):** 0x3A0528 wraps a text with element flag 0x80 through 0x3A0D00, which adds the font's
  advance (392C60) x the element scale +0x50 per character and breaks at the last space once the sum passes the width +0x60, both
  in PS2 units. 68rival_pre's headline (flags 0x2c9, width 475, 73 %): 'Face off against Nate in a Rival Challenge!' = 652 x 0.73
  = 476.0 > 475 wraps after 'Rival', Psymon (511.7) wraps, Griff (464.3) does not; the 60 % bullet (445.2 <= 450) stays on one
  line (PS2 `local/reference/pcsx2/{ruthless,the-throne,happiness-mac}-ready`). The port measured with its canvas font scale
  (x 0.79 x 1.25 = 0.9875), so the headline fitted. Now `lui-player.js ps2Wrap` (results panels, `results-lui.js`): headline rows
  y 168..180 / 193..204, x 94..439 / 94..210 on the PS2; 168..180 / 193..205, x 93..434 / 93..208 in Chrome and WebKit
  (`round2/rival/cmp-wrap.png`). No static flag-0x80 text of the results screens changes its breaks.

## 22. The CTM plane drop's camera (pv `dropCamera`)

- **Before:** the new career's first ride (the drop from the plane over Happiness, `free-ride.js PEAK_STARTS[14]`) opened on the
  horizon: the port's rider started level (heading only) and its camera began behind it looking out; the PS2 looks steeply down
  the slope (menus/fr/ctmstart f02700), and the air streamers showed as a line above the rider (section 21).
- **PS2** (`round2/dropcam/camfields.py`, DEFAULT_3 0x157F010, outer 0x157EB50, rider 0x1456D40): at f02700 the rider lies head
  first: quaternion +0x120 (0.3664, 0.4273, -0.5884, -0.5805), physical forward +0x1B0 (-0.370, 0.039, -0.928) = 68 degrees down,
  head 83 cm towards -x. The camera is 10 ticks into the lock the drop's set-target 0x176FE0 started: lock state 1, blend 0.389
  (0.5 - 10/90), lockView = forward x 250 = (-92.5, 9.8, -232.1), offset direction (-0.23, 0.02, -0.97), pitch 68 degrees. Through
  the fall the quaternion barely moves (air control 5, 0.3664 -> 0.3672 by f02793) and the DEFAULT_3 air update (it reads the
  velocity, not the forward) takes the view to 78 / 81 degrees.
- **Fix:** `web/plane-drop.js` (generated by `tools/export_plane_drop.py` from ctmstart.f02700): the quaternion words and the 271
  camera words of that tick. `main.js resetPhysics` (spawn.drop, PEAK_STARTS[14] `drop: true`): the quaternion goes into
  `_rider_orientation()` before `_set_rider_velocity` commits it, and `_camera_seed_words` gives the camera that tick's state (the
  camera begins after reset_rider and takes the seed, as the event starts take their anchor words).
- **After** (tick-stepped, neutral pad, `round2/dropcam/dropshot.mjs` / `err.mjs`; PS2 f02700 / f02747 / f02793 / f02841 / f02887 /
  f02935 = ticks 1 / 47 / 93 / 141 / 187 / 235): camera pitch 5.8 -> 68.6 (PS2 68.1), 62.6 -> 78.3 (78.3), 81.8 -> 81.1 (81.1);
  eye error 279 / 121 / 39 / 7 / 18 / 18 cm -> 30 / 28 / 38 / 4.4 / 0.9 / 0.5; the rider after the landing 7 / 13 / 16 cm -> 0.5 cm
  (its orientation at the landing now matches, so the whole run follows the PS2). Frame MAD over the 3D view: 67.4 -> 29.8,
  58.9 -> 13.3, 25.8 -> 9.0, 18.0 -> 6.6 (Chrome); 67.4 -> 29.9, 59.0 -> 13.4, 25.9 -> 9.1, -> 6.8 (WebKit)
  (`round2/dropcam/cmp-fix.png`: PS2 | before | Chrome | WebKit). The real new-career flow at 844 x 390, `?quality=low`:
  `round2/dropcam/flow2.png` (Chrome, WebKit on a fresh origin).
- **Test:** `test-visual-parity.mjs` R10 runs the drop on Happiness in node: rider and camera within 0.64 cm of the PS2 at ticks
  187 / 235 (the level start: 20.3 cm), plus `plane-drop.js` = the savestate's words.
- **Open:** during the fall the look-at sits ~30 cm above the PS2's (the head bone of the port's air pose; after the landing it is
  exact); the PS2's air streamer is a long ribbon trailing from the board where the port's is short and zig-zag (f02747); the
  plane's snow cloud at the left of f02700 (pv `planeFx`, another agent's).

## 23. The plane drop's air streamer and air pose (pv `dropStreamer`, `dropPose`)

- **Before** (section 22 open items): at f02747 the PS2's air streamer is a long smooth ribbon from the board's tail; the port's was a
  short zig-zag. During the fall the port's look point sat ~30 cm above the PS2's.
- **Streamer** (2EF6D0 / 2EF950, RFX+0xAD0; the port's update / draw already follow them): the ring rows are board - 0.7 x root at the
  world bones +0x30, the width is the render bones' +0x34 row 2, T = scroll + k / count against strm's bright rows at T >= 1. At
  f02700 the PS2's ring is 10 ticks old (count 4, scroll 0.81: the FX reset 2EF6A0 zeroes it at the placement and the fall's |v|
  moves it); the port's started empty at scroll 0, so the ribbon sampled strm's dark end and was 2 rows short. The zig-zag itself
  came from the pose: the port's body settled over the first ~20 ticks, which kinked the ring.
- **Air pose:** the PS2 rider is in the air controller (+0xDE4 = 5) from the placement, its air clip 287 on channel 2 (0.167 s in)
  and 416 on channel 3, and all the ground-control triplets +0x1F0..+0x2A4 are 0 (11D660's reset). The port's run starts from the
  grounded seed, so its first air tick took a passive departure (control 4, clip 10, 16 ticks in the real flow) and it carried the
  seed's presentation lift (8.5 cm) and lean (0.79) into the fall: the body 28 cm (hands 57 cm) off the PS2's.
- **Fix:** core `drop_air_seed(sequences)` (the crash handoff to air: control 5, a fresh air control, no passive / held flags, the
  placement's triplet reset, then the sequences and requested semantics of that tick) and `rider_fx_streamer_seed` (cursor, count,
  scroll, ring); `web/plane-drop.js` gains `animation` (tools/probe_rider_pose.py layers + current semantics) and `streamer`
  (the ring rows past the count are stale memory and exported as 0); `main.js resetPhysics` calls both after `_set_rider_velocity`.
- **After** (tick-stepped, `round2/dropcam/err.mjs`, `bonecmp.mjs`): skeleton relative to the rider at f02747 29.3 cm mean / 56.6 worst
  -> 0.01 / 0.02 cm (f02793: 0.91 / 2.85); camera eye / look at ticks 1 / 47 / 93: 30 / 28 / 38 cm -> 0.6 / 0.3 / 0.9 cm (0.5 after the
  landing, as before); frame MAD 29.8 -> 25.4, 13.3 -> 10.6, 9.0 -> 7.4, 6.6 -> 6.4 (Chrome), the same within 0.15 in WebKit
  (`round2/dropcam/cmp-pose.png`: PS2 | dropCamera only | Chrome | WebKit; `round2/streamer/z47d.png` the ribbon). The real
  new-career flow: control 5 from the first ride tick, look error 0.3 / 0.9 cm at f02747 / f02793.
- **Test:** `test-visual-parity.mjs` R11 (the drop on Happiness in node: skeleton 0.01 cm, camera 0.31 cm at f02747, 25 streamer rows
  from T 0.875); scratch-core capture run (229 scenarios) passed before the live core was rebuilt (14:41).
- **Open:** the plane's snow cloud at f02700 (pv `planeFx`, another agent's).

## 24. The Throne's summit flag pole (pv `liveCompObject`)

- **Before:** on The Throne's rival ready card (PS2 `local/reference/pcsx2/the-throne-ready`) a tall pole stands at the left; the port
  drew the two summit banners (cloth flags) floating without it (`round2/pole/cmp.png`, the card hidden: `cmp2.png`).
- **Which instance:** `mdl_EBC3_summit_flag_pole_1000` (resource 282410, track 42 rid 1103, 6 m tall): projected through the PS2
  card camera (outer eye / look / fov) it runs from x 162, y 291 to x 96, y 66, where the PS2 frame shows the pole.
- **Why the port skipped it:** its runtime flags are 0x210225 at the ready state and the countdown (0x210125 in the race): the static
  collector 22A5A0 needs flags & 3 == 3, so the countdown audit classed it draw 'none' and `prepare.py` hid its batch. Its instance
  entity is its LiveComp player, a type-1 Object (vtable 0x490B10) whose draw 0x356298 needs flags & 4: the player draws it
  (built by the section trigger, program 13; alive in the ready, glide and race states). The port's LiveComp animated the hidden
  mesh but never put it in the scene. Only two hidden LiveComps in all 17 courses have this pattern: the pole and the os609 heli
  (drawn from SETS/<LOC>HELI, pv bcHeli); the timers and Big Challenge punch targets are DeadNode / type-16 (empty draws). The
  Peak 3 world's free-ride audit draws the pole already.
- **Fix:** `tools/export_livecomp.py draw_class`: 'object' for a 'none' owner with an Object player (0x490B10) and flags & 4, except
  os609. `web/set-pieces-renderer.js`: an 'object' LiveComp's hidden batch joins the scene while its player runs (pv
  liveCompObject). Re-export: EBC3/LIVECOMP/livecomp.json, the pole's draw 'none' -> 'object' (the only change; snapshots equal).
- **After** (re-export routed in Chrome and through the proxy in WebKit): the pole at the card's left where the PS2 has it
  (`round2/pole/crop.png`, `crop-wk.png`: PS2 | before | after).
- **Test:** `test-visual-parity.mjs` R12 (the classifier on the countdown audit: pole 'object', os609 'none'; the renderer rule; the
  deployed EBC3 livecomp.json has draw 'object').

## 25. The rival card's rider brightness (pv `readyLight`)

- **Before:** on every rival ready card the port's rider read brighter and more saturated than the PS2's, which shows a dark blue rider
  (the board lighter grey) through the panel (`round2/pole/rider.png`: PS2 | port).
- **Not the panel:** with the card's HUD layer read out (colour and alpha per pixel) and the scene layer alone, the port's composite is
  reproduced to 0.6 levels, and the PS2 frame inverted through the same panel gives the same snow under it (12.4, 44.1, 95.3 vs 11.0,
  48.6, 94.0) but a dark rider (`round2/pole/unpanel.mjs`, `unpanel.png`). Blend order and panel alpha (0.75 there) match.
- **Cause:** the port drew the card's rider through the unlit menu fallback (texels doubled): the source skin (and with it the rider
  lighting) is shown only on the game / pause / results screens, and nothing captures the rider lighting before the first race tick.
  The PS2 draws the ready rider shaded: its rider irradiance (EE 0x4FA370 slot 0 +0x50, ten RGB rows) is the environment its load
  frames settled at the start spot: row 0 (0.220, 0.307, 0.508), rows 1..9 the same = 0.95 of the dark bank EPDK1 + 0.05 of EPBR1.
  The port's environment is zero until race tick 0, and its ground patch is unknown until the first step.
- **Fix:** core `environment_settle(n)` (the card's ground query on a copy of the contact cache, then the per-tick environment update
  n times; the placed state keeps its patch fields); `main.js readyView` shows the source skin in the ready pose, settles the
  environment (60 updates) and captures the rider lighting with the card's camera; the render shows the source skin on the card.
  Row 0 after the settle: (0.211, 0.298, 0.500).
- **After:** The Throne's rider box 15.5, 53.9, 84.1 -> 13.3, 52.5, 83.1 (PS2 8.3, 48.5, 83.0), card MAD over the rider area 10.69 ->
  10.01 (Chrome), 10.91 -> 10.23 (WebKit); Ruthless 14.03 -> 13.57, Happiness 18.21 -> 18.07 (their PS2 frames have other
  characters, Mac / Nate's challenger, so their colours differ anyway). `round2/pole/rider5.png`, `cards3.png`, `rider-wk.png`.
- **Also:** the race after Continue now starts from the settled environment, as the PS2's does. Scratch-core capture run (229
  scenarios) passed before the live core was rebuilt (15:48). Test: `test-visual-parity.mjs` R13.

## 26. Fresh sweep, CTM free-ride frames, and the switch-stance 'S' (pv `switchIcon`)

- **Sweep** (Chrome, the 65 pairs, `round2/sweep4/`, sheets `round2/sweep4-sheets/`, `round2/cmp34.mjs`): world MAD 14.28 -> 14.36.
  The only large change is peak2/dss2-full 4018 (7.92 -> 17.20): the port's rider leaves the PS2 line differently there now (75 vs 44
  MPH at the same tick; the region's tint follows the rider) - gameplay divergence, not rendering. Checked later (slopestyle
  agent): the core itself does not diverge there. `compare-ps2-capture.mjs peak2/dss2-full --pad --sync-rng --zoe --event`
  (STAGE_WORLD) is exact for all 9000 ticks (physics, bones, score, boost) on the live core and on a core without the
  2026-09-27 slopestyle fixes and the surface-18 crash, and the two reports are identical row for row. The capture's Zoe never
  touches surface 18, keeps +0x324 = 0 and has no control 2. The page run is not a synced replay (2-3 mm off from the start),
  so its path after it leaves the line is chaotic. dss2-full is now a gate in test-ps2-captures. Excluding it the frames are within
  noise or better (dra4 5218 14.91 -> 11.00). The worst frames are still harness limits (aba1 2020 white-out, ass1 4420, chp2 418
  particles and career HUD).
- **CTM free ride** (Green Station, PS2 `runs/peak1-green-start`, `tools/vpshot.mjs` / `vpeval.mjs --course PEAK1 --params peakCourse=17`):
  the shot tool's second `ssxQA.start()` ran without the fresh rider (free-ride.js arms it once per world start), so the rider drove
  6 mph fast and took a rail the PS2 did not. With it re-armed (`--before-start "__freeRide.worldEntry(17)"`) the page is on the PS2's
  line exactly (rider 0 cm, camera < 0.5 m) through the lodge door; frames `round2/fr-green2.png`. Differences left there: the sun
  flare beside the station sign at tick 203 (not an occlusion: the PS2's sun is still fading in, section 27), and the switch 'S'
  below.
- **Switch-stance 'S':** every PS2 frame shows an 'S' under the boost meter, faint while riding regular and bright orange riding
  switch; the port drew none. It is the race HUD 0x1EC3F8 at 0x1F0288..0x1F0368 (HUD flags 0x10000000, set in the race and the
  free-ride flags): owner +0x470's sprite (OV_1-4) at descriptor 0x4F (583, 408, 24 x 22, centred), its alpha x 0.2 (gp-0x55B0) while
  rider +0x320 == +0x324. Fix: `trick-hud.js switchIcon`, `ui.js` (after the gauge, with the frame's flags), `main.js` HUD state
  `stanceRegular` (core rider_stance_info[2]); `tools/probe_trick_hud.py` exports `sprites.switchIcon` (trick-hud.json re-export: that
  sprite only). Icon box error vs PS2 (Chrome; WebKit the same to 0.02): Snow Jam 418 12.20 -> 6.31, Green Station 103 8.48 -> 4.09,
  393 27.84 -> 20.67, riding switch Snow Jam 4419 73.57 -> 19.82, CRA3 1218 55.93 -> 17.29 (`round2/switch/meter-on.png`, `sw2.png`).
  Test: `test-visual-parity.mjs` R14.

## 27. A world load's painters: the sun and fog fade in (pv `painterWorldLoad`)

- **Symptom** (section 26): at Green Station tick 203 the port drew the sun and its flare beside the TRANSPORT / LODGE sign. The PS2
  frame has no sun there. The sign does not hide it: the port's query rect (screen 450..466, 47..63) is sky, right of the sign.
- **PS2** (`runs/peak1-green-start` re-captured from the same baseline with watches on the camera block's Sun painter
  0x58a600, its Fog painter 0x555e80, gp+0x770 and rider+0x430; `ps2_capture.py build --watch`, physics identical to the original):
  every painter starts the ride at its class defaults with +0 = 0. The start location's record then blends in at the record's
  rate. Sun A (rate -0.1, 1 %/tick): elevation / azimuth 0.106 / 1.249 at tick 1, 6.740 / 79.639 at 101, 9.168 / 108.332 at 201.
  At 203 the PS2's sun is at x 620 of 512, off screen. Fog A (rate -0.05, 0.25 %/tick): density 0 -> 1.2, far 300 -> 70 m, colour
  (0.43, 0.55, 0.71) -> 0.99; density 0.268 / far 248.6 m at 101, 0.474 / 209.1 m at 201. Both are still blending at tick 393. At 394
  the lodge door's placement (0x2C03E8, +0 = -99999) snaps them to A's values.
- **Why:** the navigation states before the load (`lodgewall/ps2/navload*`, `navpre/s680..s683`) show the order. The world load
  builds the painters (+0 = -99999). It places the new rider during the load (11DE60 -> 111890 -> 0x2C03E8, -99999 again). The
  load's steps then run in the stale painter region gp+0x770 = 7 (ABC1_A), whose record is not loaded yet, so 0x2C09D8 gives the
  class defaults with +0 = 0. The first ride tick (gp+0x770 = 1) blends from those defaults. The rider block does the same one tick
  later. Its Weather jumps anyway (rate 0), and A's Lighting scalars equal the class defaults.
- **The same state elsewhere:** the Transport arrivals (`peak1/peak1-arrive-{aba1,ass1,bra2,bhp1}.capture.p2s`) hold the class
  defaults with +0 = 0 before their placement too. That case is not changed here: the arrival's 111890 may still snap once the
  record has loaded, and no watched arrival run exists.
- **Port:** before this, `startRun` reset the painters to -99999 and the placement's 0x2C03E8 did the same, so the first step
  jumped to A's values.
  - With pv `painterWorldLoad`, `free-ride.js placeRegion` calls core `environment_world_load()` after the new rider's placement,
    for the fresh rider of a world load (the lodge's Return to Game, and a world start at a station). The CTM plane drop is not a
    region placement and still snaps, as the PS2's does (ctmstart f02700).
  - The core Fog gets its class defaults with +0 = 0 (0x2BE108).
  - The page's Sun / ScreenTint / glare painters follow the core counter `environment_world_loads()`
    (`painter-regions.js followWorldLoad`). Live core rebuilt with the two exports; the scratch-core capture run (230 scenarios,
    switch off and on) passed first.
- **Result** (vpeval `peakCourse=17`, pinned camera, the painter values read with `ssxQA.sun()` and core `fog_info`):
  - The port's painter values equal the PS2's watched ones at every checked tick (101 / 201 / 304: Sun elevation / azimuth, Fog
    density / near / far / RGB to the float).
  - Frame MAD vs PS2 (world, HUD column excluded), Chrome: t103 14.40 -> 11.70, t203 17.67 -> 12.06, t306 24.37 -> 21.23.
  - Sun box at t203: 77.5 -> 13.5.
  - WebKit: 14.43 -> 11.79, 17.70 -> 12.14, 24.39 -> 21.29.
  - At t306 the sun now sits right of the pole, where the PS2's is (the jumped sun was 37 px left).
- **Harness:** `tools/drv.mjs FRESH_WORLD_ENTRY` (vpshot / vpeval run it by default before the second `ssxQA.start()` on a
  `?peakCourse` page; `--no-fresh` skips it). vpeval `--route` and `proxy2.mjs` take `KEY$` for an exact path suffix
  (`/screen-tint.js$` no longer catches `screen-tint.json`).
- Test: `test-visual-parity.mjs` R15 (the Sun painter against the watched PS2 values, the placement hook, the core Fog's first
  blended step).

## 28. Where the world textures' LOD K comes from (analysed, not drawn)

- **Per texture, not per patch.** Section 18 found K varying per terrain draw. It varies with the texture, not the patch:
  the same VRAM slot (TBP 10936) is reused by several streamed textures within a frame.
  - The GS words come from the texture's descriptor. `368970` writes TEX0 / TEX1 / MIPTBP1/2 from descriptor +0x38 / +0x40 /
    +0x48 / +0x50.
  - World textures are `strm_tex` descriptors (header +0x1C = the texels, record header at texels - 0x80).
- **The formula** (streamer `0x37CA30`, when the header's mip count, byte 15 >> 4, is > 0):
  - Read v = header s16 +8. If v = 0, the descriptor keeps its default K = -185 (-11.5625, `0x367880`).
  - Otherwise `K = trunc(-(ln(240 / (v * 2^-14)) * (1/ln 2)) * 16)` (float mul 2^-14, div 240.0, then double log, x 1/ln2, 0 - x,
    x 16, to int), clamped to -2047 .. -135.
  - TEX1 is then `LCM 0, MXL, MMAG 1, MMIN 5, L 0, K`, so level = log2(1/Q) + K = log2(w * (v / 16384) / 240).
  - Read this way, v / 16384 is the texture's texels per cm on the ground and 240 the focal constant: level 0 is where one
    texel covers one pixel of a surface facing the camera.
  - Checked against the live descriptors of 8 PS2 states (event-start, fr-aara1-glide, frc-1800, crows-start, rnb-start,
    the-throne-start, monster-swollen, frdra4-2400): 334 textures, every K equal.
  - `tools/export_world_texture_lod.py` writes it per world texture id: 676 of the 788 have mips, K -350 .. -135.
- **Drawn, measured, not landed.** A depth-only level was built in `world-material.js`, in both the shared and legacy graphs:
  `clamp(log2(depth cm) + K/16, 0, MXL)` by `textureSampleLevel`, with the per-material K in the object update. It is kept in
  `round2/lod/`: `world-material.lod.js`, `world-lod.json`, `sweep.sh`, `mad3.mjs`.
  - Chrome, 15 frames on 7 courses, world MAD vs PS2 13.49 (derivative LOD, now) -> 13.88. With a level bias of +1 it is
    13.76, with +2 13.60. It is worse at every bias, on every frame.
  - It adds high-frequency detail the PS2 frames do not have. Mean |Laplacian| far / mid / near: PS2 6.8 / 5.8 / 3.0,
    now 9.6 / 7.7 / 4.5, depth LOD 12.6 / 10.1 / 5.4.
  - Tree cards alias visibly (`round2/lod/cra3-418.png`: PS2 | now | depth LOD).
  - Left as is. A fair retry needs the PS2's own mip levels (the kind-9 records carry them; world.tex keeps level 0 only) and
    a render at 512 x 448 scaled as the PS2 frames are.
- Test: `test-visual-parity.mjs` R16 (the formula against PS2 descriptors).

## 29. Round 3: the wider sweep, the pause, riders over menus, hints, the shadow atlas

The round-3 sweep covers more than the 65 pairs:
- the CTM free-ride world at the stations and backcountry (`tools/vpworld.mjs`: the rider placed at each PS2 record, the painters
  snapped, the record's camera pinned at the renderer);
- event starts (`tools/batch3.sh`);
- the front end and in-game menus (`tools/fe2.mjs`, `tools/spause.mjs`, the real Single Event flow).

The PS2 menu frames are new `ps2_menu_capture.py run` captures from `menus/single/state-single-pause.p2s`:
- `r3-pause-rows` (each row, then Quit);
- `r3-pause-restart` (the Restart popup);
- `r3-pause-restartyes`;
- `r3-pause-quityes`.

Ranked by how visible each difference is to a player:

1. **Riders over every in-race menu (fixed, pv `menuRiders`).**
   - Under the pause, options, audio screens and the Yes / No popups, the world is hidden. The computer riders (and online
     riders) still drew over the PDA, as floating snowboarders across the menu (`round3/pause/before/pause.native.before2.png`).
   - The player's rider was already hidden there.
   - Now `opponent-riders.js` tags its groups and `main.js` hides them while `playing` is false. `aiRace.update` shows them
     again on the next frame the world is drawn.
   - Results and the replay keep them: 5 riders / 28-29 meshes, Chrome and WebKit.
2. **The Single Event pause (fixed, pv `singlePause`).**
   - A Single Event runs through the career screens (`career-ui.js single` -> `begin(.., career=false)`), so it paused into
     the career pause: Return / Restart / Messages / Audio / Options / Give Up.
   - The PS2 Single Event pause has five rows: Return / Restart / Audio / Options / Quit. Their help lines are
     `kT_OVRHELPGetBoarding`, `kT_OVRHELPRestartComp`, `kT_OVRHELPChangeMusic`, `kT_OVRHELPOptions` and
     `kT_MAPHELPQuitGame` ('Quit out to Title screen.').
   - Restart opens 'Restart / Are you sure?' with No focused; Yes goes to the round's card (`restartToCard`, the same path as
     the career).
   - Quit opens 'Quit Game' with No focused. Yes goes to the title screen, with no save question (PS2 `r3-pause-quityes`: black,
     then 'Press START button'). No or Triangle goes back to the Quit row.
   - Audio and Options open the same screens as the career PDA and come back to their own row (2 / 3).
   - The online race pause and the fallback Single Event pause (ui.js `'pause'`) are drawn in the same PDA. The fallback's
     Quit keeps its old action, so its help is 'Quit out of the current competition.' (online: 'Quit to Online Main Menu.').
   - Every row matches the PS2 frame, apart from the badge temperature, which comes from the game clock
     (`round3/pause/rows-vs.png`).
   - Checked in a race (Snow Jam), a freestyle event (Crow's Nest Big Air) and a finish with results and replay, in Chrome and
     WebKit.
3. **The in-game Options screen (fixed, pv `pdaOptions`).**
   - Every in-game Options (Single Event and career pause, MCOMM, Big Challenge pause) is now the PS2's PDA page OV.LUI
     `37beoptions`, played from the disc layout. It is exported by `tools/export_audio_menus.py` into
     `UI/audio-menus.json` and drawn by `audio-menu.js` `pda-options`.
   - The rows are HUD Options (Full / Minimal / None, the FE HUD setting), Camera 1 (Near / Mid / Far), Camera 2 (greyed,
     one player), the Music/MC / SFX / Character speech sliders, DJ Speech, Arcade SFX, and Save game.
     - Save game is greyed in a Single Event; in Conquer the Mountain it saves.
     - Each row has its help line. Down wraps past the greyed rows. Triangle goes back to the opener's Options row.
   - The port's Widescreen, Keyboard and Display & Touch come after the PS2's rows. They sit behind a "More options" row in the
     online-only EA SPORTS BIG Talk slot (y 325, its focus state 120, with the Save game Cross), on a second page that reuses
     the first three rows.
   - The PS2 references are fresh single-event captures (`menus/single/r3-options`: every row, the wrap, Triangle) and
     `menus/ctm/47-options`.
   - Every row, value, slider and help line matches, in Chrome and WebKit (`round3/options/opts-vs.png`, `mcomm-sheet.png`,
     `more-sheet.png`, `wk-sheet.png`).
   - The value arrows draw white. The group's A 100 belongs to its bar, as with the Sound Options arrows
     (`round3/options/arrows-crop.png`).
   - The page needs the re-exported `audio-menus.json`. Without it, Options falls back to the old list.
4. **Big Air finish HUD: a harness context, not a port difference.**
   - In the 'much-2-much' pairs, the PS2 hides the clock and meter and shows the points under FINISH!.
   - The shot tool's direct `?course=` page runs the event without the career screens. The real Single Event flow
     (`spause.mjs --course EBA3 --finish --banner`) shows FINISH! with the points and no HUD, then the "finishov" panel, as
     the PS2 does (`round3/bigair/banner-vs.png`).
5. **Transport arrival fade (fixed, pv `arrivalFade`).**
   - Watched re-capture of the ABA1 arrival (`menus/fr-courses/aba1-screen10.p2s`, the Sun / Fog / ScreenTint / Lighting
     instances and gp+0x770; snaps every 3 ticks).
   - The loop fades out to black. At the arrival placement (tick ~2020), the world fades in from black over ~30 ticks,
     linearly: snow luma 6 % / 40 % / 84 % / 100 % at +1 / +11 / +24 / +29. The HUD draws over it at full brightness.
   - The fade's opaque first frames reset the painters (weight -99999 at 2020, the 0x2E47E8 rule). They snap to region 2's
     record (fog far 80 m, density 2), then blend into region 5 from ~2110.
   - The port already had the same regions and records through the real transport (`tools/arrive.mjs`: region 2, fog 8000,
     then 5). It cut from black to the full world in one frame.
   - Now `main.js` starts `cutscenes.fadeFrom({ticks: 30, colour: 'black', hud: true})` after a course arrival. `ui.js` draws
     that overlay first and the HUD over it. Stations and backcountry are left out, since their arrival cuts fade
     themselves.
   - Chrome and WebKit follow the PS2 ramp within a tick or two (`round3/arrive/fade-vs.png`: PS2 / now / before).
   - The vpworld pairs could not show this: they teleport the rider without the arrival placement.
6. **Hints after Peak 1 (fixed, pv `hudHints`, R17).** From 0x1EBA10: the RECOVER label and the Uber hint go once the profile
   has visited a Peak 2 or Peak 3 course.
7. **The rider shadow atlas (fixed, pv `shadowAtlasInit`).**
   - The shadow atlas was first rendered into after a world material had bound it. three r186 recreated the target and
     destroyed the bound texture: "Destroyed texture used in a submit" on every later frame. The rider vanished and the frame
     froze.
   - `rider-shadow.js initRiderShadowAtlas` now clears it once when the renderer starts.
   - A device recovery gives a new renderer. The atlas is keyed on it, so the recovery path should call it again (with the
     performance agent).

What matched:
- the main menu, character select, the CTM MCOMM;
- the backcountry Peak 2 / 3 free-ride world (MAD 6-13).

Harness limits, not port differences:
- free-ride teleports into rows that are not loaded;
- event runs that start mid-run.

8. **Pale trees, signs and buildings in streamed free roam (fixed, pv `genericFogOff`).**
   - After a Transport, distant trees and signs at ABA1 were near-white, and the BHP1 city was white. The PS2 shows them dark
     and clear under the same Fog record.
   - Ruled out:
     - the original fog: its palette matches the PS2's CLUT read from GS memory (alpha 104..128, at most ~19 % fog);
     - `fogStage=color`: the colour pass alone was already pale;
     - texture mips: the cut-out textures' transparent texels carry tree colours;
     - static cells and shared materials: toggling `staticWorld`, `staticRefresh` and `sharedWorldMaterials` changed nothing.
   - The cause was three's linear scene fog. The course load clears `m.fog` on the meshes present then, which leaves 396
     materials made later with it on (every location streamed in after the load, static cells, set pieces).
   - Those materials got a second, generic fog on top of the original: fully the painter's colour at its far (80 m in region 2
     at ABA1). The vpworld pairs load at the capture location, so they never showed it.
   - Now `main.js` detaches `scene.fog` once the fog renderer exists and keeps it as `scene.userData.genericFog` for the
     painter bookkeeping.
   - Real transports to ABA1, ASS1, BRA2 and BHP1 now show the PS2's dark trees, readable signs and dark city, in Chrome and
     WebKit (`round3/trees/aba1-trees.png`, `arrivals-trees.png`, `wk-bhp1-trees.png`: PS2 / now / before).
   - Event worlds are unchanged: their meshes were all cleared at load.

9. **Restart: the start gate under the card (fixed, pv `pauseRestart`, `loopFadeOnce`).**
   - New PS2 captures:
     - `menus/race/r3-restart`, from `state-pause`: the pause's Restart shows the lit start gate +30 samples after Yes and
       the card at +170;
     - `r3-results-restart`, from `state-results`: the results' Restart rides the gondola with "Loading..." first;
     - `r3-rr-dense`: every 15 samples for 1600, the gate under the card stays at luma 179.1.
   - The port rode the gondola for both. Under the card, the gate idle faded in from black on every loop. It took the start
     hut's fade-out record, and its clock wraps, so the card sat over a gate dipping to black.
   - Now the pause's Restart plays only the gate idle (cutscene kind `restart`). A looping step's fade-in plays on its first
     pass only (`fadeAt` `loops`).
   - The gate stays at luma 182-184 under the card after both Restarts, in Chrome and WebKit
     (`round3/restart/pause-restart-vs.png`, `results-restart-vs.png`).

Tests: `test-visual-parity.mjs` R17 - R22.

## 30. Browser-only presentation: evidence (nothing landed)

These are items the PS2 never had, because the port draws up to 896 lines on sharp displays. They are measured here for
Owen's decision. None of them is turned on.

- **UI at 2x (`uiHiDpi`, prototype in scratch only).**
  - The two UI canvases go to 1280x896 once the stage shows at 896 or more device lines.
  - What improves: vector-drawn parts (PDA icons, panel outlines) get cleaner edges. Bitmap-font text barely changes,
    because the glyph atlases are at PS2 resolution.
  - Chrome: no frame loss.
  - WebKit: 60 -> ~44 / ~37 fps on the pause and main menu. The 4x canvases are uploaded every frame.
  - Recommendation: off. Crops are in `round3/hidpi/` (`evidence-hidpi.png`, `c-mac-*`, `c-1080-*`, `c-wk-*`).
- **Output low-pass (proposed `ps2Output`).** Measured on 57 round-3 pairs at ~480 lines (`tools/filt.mjs`).
  - High-frequency energy (mean |Laplacian| in the distance band): PS2 6.63, port 10.13. This surplus is the shimmer and
    edge crawl of distant terrain and tree cards.
  - A horizontal [1,2,1] brings it to 6.54, matching the PS2's 512 -> 640 stretch. [1,4,1] in both axes brings it to 5.88.
  - MAD vs PS2 falls on all 57 pairs: 19.02 -> 18.76 (horizontal) / 18.66 ([1,4,1] both axes) / 18.56 ([1,2,1] both axes;
    softer than the PS2).
  - Proposed form: one separable filter in the fog composite's scene-colour sample, one PS2 pixel wide, so it scales with
    the render size. The HUD stays sharp. Crops: `round3/filter/filter-crops.png`.
  - **Built as a player option (Owen: "Add as an option, off"; 2026-09-28):** Options > Display & Touch > "PS2 softness" Off / On
    (the third row). It is saved in `ssx3.quality` with the other picture settings, defaults to Off on every tier, and
    `?ps2soft=0|1` overrides it for one visit. Reset options turns it off.
    - `fog-renderer.js setSoftness(on)` builds the softened composite the first time it is switched on:
      - the scene colour is four bilinear taps at +-0.25 / +-0.75 of a 640-frame pixel, weighted 1/4 each. That is exactly
        [1,2,1]/4 at 640 wide, and [1,2,2,2,1]/8 at 1280, so it stays one PS2-frame pixel wide at any render size;
      - the fog, snow, glow, sun, glare and tint stages then run on it;
      - the snow composite is seeded with the softened world (`snow-composite.js seeded` / `useSeed`).
    - Switching back restores the original node graph. main.js applies the setting when the fog renderer is made, before the
      warm-up, and on every quality change.
    - The HUD and menus are separate canvases and stay sharp. Snow sprites, the sun and glow draw after the filter, as on the
      PS2 they draw after the fog.
    - **Off is today's picture:**
      - Chrome: all 52 WGSL modules the page makes are byte-identical to before (`tools/wgsl.mjs`), and the frames are
        identical (md5);
      - WebKit: the frames are identical;
      - On -> Off in the running page returns the exact Off frame.
    - **On** replaces 4 modules: the composite, 13 -> 19 texture samples; the snow seed quad, 2 -> 8.
    - **Measured with the real shader** (Snow Jam free ride 3946, the jittered views of `vpworld.mjs --jitter`):

      | | crawl | flips | MAD vs PS2 | high-frequency energy |
      |---|---|---|---|---|
      | Off | 1.32 | 0.77 % | 11.06 | 12.49 |
      | On | 0.99 | 0.16 % | 11.02 | 8.87 |

      PS2 high-frequency energy: 8.67. WebKit gives the same numbers (1.00 / 0.16 % / 11.03 / 8.87). On 14 free-roam
      pairs, mean MAD 15.56 -> 15.46, lower on 12 of 14. Crops: `round3/soft/softness-crops.png` (PS2 / Off / On).
  - **Design, ready to build (on hold: the performance agent is reworking pipeline creation, and it waits on Owen):**
    - Switch `ps2Output` in `pv-flags.js`, default off. It could later be a Display & Touch row ("Picture: Sharp / PS2").
    - `fog-renderer.js`: when on, `sceneColor` becomes
      `0.25 * s(uv - (1/640, 0)) + 0.5 * s(uv) + 0.25 * s(uv + (1/640, 0))`.
      - `s` is the world pass texture at `screenUV`. The offset is in UV, so the kernel stays one 640-frame pixel at any
        render width. It is about a quarter of a PS2 pixel narrower than the 512 -> 640 stretch, and matched the PS2's
        high-frequency energy in the study.
      - A vertical [1,4,1] would be a second, optional step (5.88 vs 6.54).
    - Unchanged:
      - the fog CLUT entry, the depth gate and `originalZ` keep reading the centre depth;
      - the snow composite, light glow, sun, glare and ScreenTint still apply after the filtered colour, as in 363490;
      - the UI canvases are not touched.
    - Cost: two more texture samples per pixel in the composite that already runs, with no new pass or target. Check the
      composite's WGSL against WebKit's 8 KB private budget (the stages are `.toVar()` already). The warm-up compiles the
      same pass, so there is no new pipeline.
    - Verify: the 57 pairs (`tools/filt.mjs`) with the real shader, in Chrome and WebKit. MAD should fall and HF energy
      should reach about 6.5. Before/after crops at 480 and 896 lines, and the frame time on the low tier.
  - Owen can already get the PS2-soft picture with Display & Touch -> "PS2 512x448" + "Smooth".
- **Banding:** the PS2 main framebuffer is 32-bit, so the GS does not dither the output. There is nothing to match.
- **Alpha-to-coverage on tree cards (evidence only; not recommended).**
  - Measured from a routed scratch copy of `world-material.js` (`?a2c=1`: `alphaToCoverage` on the alpha-tested,
    depth-writing pass of blend classes 1 / 2). The live file was not edited.
  - Test shot: Snow Jam free ride, PS2 record 3946, camera pinned (`tools/vpworld.mjs --jitter 6 --jyaw 0.03`: the view yawed
    by 0.03 deg, about 1/4 px, per frame). Crawl = mean |luma change| between successive frames in the tree band; flips =
    pixels changing by more than 24.

    | condition | crawl | flips | MAD vs PS2 |
    |---|---|---|---|
    | low (640 x 448, no MSAA) | 1.40 | 1.11 % | 11.32 |
    | medium, MSAA off | 1.33 | 0.90 % | 11.10 |
    | medium, MSAA 4x | 1.32 | 0.77 % | 11.06 |
    | medium, MSAA + a2c | 1.50 | 1.02 % | 11.54 |
    | medium, MSAA + horizontal [1,2,1] (`ps2Output`) | 0.96 | 0.01 % | 10.91 |
    | low + horizontal [1,2,1] | 1.04 | 0.16 % | 11.07 |

  - WebKit gives the same numbers (its frames within 0.02 MAD of Chrome's).
  - Alpha-to-coverage is worse on every measure. The port already emulates the PS2's AFAIL FB_ONLY with two passes: the
    blended fringe pass draws the cut-out edges soft. A2C only adds the 4-sample coverage dither on the branches (visible as
    speckle: `round3/a2c/a2c-crops.png`, PS2 / low / MSAA / a2c).
  - MSAA helps a little. The one-PS2-pixel output filter removes nearly all the crawl, and brings the frame closer to the PS2.
- **16:9 / ultrawide:** the three widescreen modes follow the PS2's render-context factors (`widescreen.js`). On 21:9 the
  16:9 stage is pillarboxed. A wider (Hor+) view would be a look change, not parity.

## 31. When Start pauses, and Start in menus (pv `startRules`)

**Owen's report:**
- "a lot of places like after you finish a race, when you're in the lodge, where you shouldn't be able to pause";
- "if you're in the lodge when you pause it puts you back outside the lodge when you hit resume".

**The PS2 code.**
- The pause opens in one place: the game update 0x2306B8, at 0x230A34 (push overlay 2 via 0x20CA10, music pause 0x289B70,
  snd 0 via 0x294F48).
- It opens only when 0x231840 sees the pause action 0x3C (input.map: Start) on a human pad. 0x231840 also returns 0 when the
  object at gp-0x204 has +8 > 0, or when an NIS runs without flag 4 (0x231AB8).
- Then three gates:
  - 0x230A44: the game phase `*(game+0x28)+0` must be 0 or ≥ 10. Values 1..9 are blocked.
    - Values read from PS2 states (game = `*(gp-0x848)+0x84` = 0xCBFA20): 0 riding / countdown / free ride; 1 race
      results; 10 at the round card and WS10 / WS11; 12 on the split ride; 13 at the finish.
  - 0x230A60: `*(game+0x34)+0x70` must not be 1.
  - 0x20CBE8: 0x20CBA0 must return 0, and either no overlay screen is up, or the top overlay has flags bit 0 clear and its
    vtable +0xCC allows it.
- In the lodge the game object is 0 (`*(gp-0x848)+0x84` = 0 in `ctm/state-lodge-peak1`): it is the front end, and the
  game update does not run.
- Every LUI screen reads Start as UINext 0x7A (input.map binds Cross **or** Start: docs/audio-menus.md). So in the pause,
  the MCOMM, results, cards, the lodge, Transport, prompts and Equip Gear, Start accepts the focused item. The pause's Start
  "resumes" only because Return is focused.

**PS2 pad captures** (`local/ps2-capture/menus/startprobe/`, Start pressed twice in each state; `apr-finish` states cleaned of
the capture hooks):

| state | Start does |
|---|---|
| riding, the countdown, a checkpoint, free ride | opens the pause / MCOMM |
| pause (cursor on Restart), MCOMM (on Transport) | that item (the Restart question, Transport) |
| FINISH! banner | nothing, until the results panel |
| results, records, rewards | Continue / the focused item |
| round card | Continue |
| lodge, lodge prompts, Save game | the focused item (Return to Game -> Save progress?, Buy Gear ...) |
| Transport map, event list, the transport question, Equip Gear | the focused item |
| transport ride / heli / Loading | nothing |

Triangle closes the pause and the free-ride MCOMM (`mcomm-tri`, `pause-tri`).

**What the port did.**
- `main.js` toggled the pause (`pause(!paused)`) on the pad's Start whenever a run was going, whatever screen was up. In the
  lodge, where the run stays paused, Start "resumed", which is Owen's second report.
- It also toggled on Escape / Enter, with no finish check. That is his first report.
- In menus, `gamepad-menus.js` dropped Start while a run was going, so Start never accepted anything there.

**Now** (`web/start-rules.js`):
- Start opens the pause only from the ride screen: running, not paused, not finished, no cutscene. The keyboard is judged on
  the screen as it was before any menu handled the key, and the pad on the previous frame's screen.
- On every menu screen the pad's Start is Enter, the accept. Escape stays back, and it now also closes the free-ride MCOMM
  (`career-ui.js back`), which main.js's toggle did before.
- The touch deck's pause button follows the same rule. Online races still pause over the running race, as before.

**Checked:**
- `tools/startmatrix.mjs`, 17 rows with a fake standard pad and keys: free ride, MCOMM, Messages, the lodge and its screens,
  a Single Event card, the countdown / ride, pause items, the FINISH! banner.
- Chrome 17 / 17 and WebKit 17 / 17 with the switch on; 9 rows fail with it off.

**A spent press (pv `startConsume`, on; 2026-09-28).**
- **Owen's report** (Safari with an Xbox pad, diag session t93ez0j6, t 872.0): BRA2's heat card went to 'game' and then to
  'ctm-pause' in the same moment. So the press that accepted the card also paused.
  - That was the heat card after the results (the card opens at the gondola cutscene's idle).
  - The first card (638.1) and the card after Restart (937.5) did not leak.
  - His eight pauses in the race before it, each about 3 s long, show the pause itself working.
- **PS2:** the pause needs a new Start press (action 0x3C: 0x320C48 -> 0x321108 evaluates the input.map expression on the pad
  record) with no overlay up and no transition running (0x20CBE8 / 0x20CBA0). The Start that accepts a card or a prompt is
  therefore still held, not new, once the overlay has gone, and cannot pause.
- **Port:**
  - The pad is read in two loops: the menus' (web/gamepad-menus.js, its own rAF) and the game frame's. Each keeps its own edges.
  - If the game frame first sees the press after the menu has already switched to the ride, one press does both.
  - A shared fake pad cannot stage that interleaving, and I could not reproduce it; the telemetry and code allow it.
- **Fix:**
  - `gamepad-menus.js` marks a press it sent as a menu key as spent until the button is released (`taken(c)`).
  - `main.js`'s pad path does not pause on a spent Start.
  - The menus' synthetic keys (`ssxPadMenu`) never pause through the keyboard path.
  - Chrome dispatches at-target listeners on `window` in registration order (bubble before capture, checked), so a synthetic Enter
    could be judged on a stale `keyScreen` there. WebKit calls capture first.
- **Checked:**
  - `startmatrix.mjs`: WebKit and Chrome 17 / 17 with the switch on, and WebKit 17 / 17 on the live server. That covers the free
    ride, the MCOMM, the lodge and its screens (no pause, not dropped outside), the round card, the pause items and the FINISH!
    banner.
  - `test-start-rules.mjs`: the late-sighting order.
- Test: `web/test-start-rules.mjs` (the state / Start matrix, the pad stepper, in test:all).
- Open (not confirmed from code): what game phases 2..9 are, and the meaning of the 0x70 flag.

## 32. The MCOMM Session map (pv `sessionMap`)

The PS2's Session screen is a PDA page: the rows, the location's map with its session points, and the rider. The port showed
only a plain list.

**Code (SLUS_207.72, overlay 0x20):**
- **0x2086A8 (setup):** the screen OV.LUI 38session, and the map picture MapPic = `|ses_<x>.ssh` (0x208F10 -> 0x1A37F8, name
  from table 0x440770 +4).
- **Table 0x440770** (stride 0x2C): course, picture, point count, and the map square's corners (x0, y0, x1, y1) in world cm.
  The table is now `web/session-map.js` SESSION_MAP.
- **0x2087F0 (the rows):** "Top of run" / "Session point %d" / "Bottom of run". The focus opens on 0x26B680's point - 1:
  the session point (kind 2, index 1..count) nearest the rider in 3D.
- **0x209970 (per point k):** region bank 0x4D33A0 kind 2 index k (26B5E0). Two sprites at the point's map pixel, both 18 x 18:
  - 'Map Indicator Icon' `dot_visited`, layer 11;
  - 'Map Indicator Highlight' `indicator`, layer 12.
- **0x2096A8 (the rider):** 'Player Indicator Icon' `location`, 17 x 17, layer 13.
- **Map pixel (both functions):** `|(p - c0) / (c1 - c0)| x (356, 266)` (0x43B2, 0x4385), truncated (cvt.w.s),
  + (0xFC, 0x61), - 5.0. That gives the sprite's top-left in the 640 x 480 LUI frame; MapPic is (252, 97) 356 x 266.
- **0x209E78 (on focus):** the focused point shows its highlight, the others their dot.
- **0x208F88 (the question):** the ConfirmPopup "Session this area?" (Popup1 shown, Yes focused). Its 3D Ov shapes are authored
  at 56 %.

**Port:**
- `career-ui.js drawSessionMap` draws OV.LUI 38session, exported by `tools/export_audio_menus.py` with the three OV sprites.
- The map picture is `CAREER/MAPGFX_ses_<x>`.
- The markers come from the location's region rows and the rider's world position (`main.js freeRideSessionMap`).
- Layers are kept: the LUI below layer 11, then the markers, then the rest.
- The menu opens on the nearest point.
- Under the question the rows lose their focus colour, as on the PS2.

**Checked against new PS2 captures** from `ctm/state-mcomm` (Happiness, the rider at the bottom): `menus/ctm/r3-session`,
`r3-session-up` (every row), `r3-session-confirm`.
- Bottom of run is focused on open. Every dot, the highlight and the rider icon are at the PS2's pixels (Top of run highlight
  top-left 489,132; rider 604,281).
- Frame MAD 10.7 (the old list: 38.8); the question 10.3.
- WebKit the same. Sanity shots at Crow's Nest (2 points) and Green Station (1).
- Test: `test-visual-parity.mjs` R24.
- Evidence: `round3/session-map-vs.png`, `session-confirm-vs.png`, `session-wk.png`.

## 33. Stations after a Transport, and the Session fade (pv `stationArrival`, `sessionFade`)

The first item of the CTM Peaks 2-3 pass: the station arrivals. Found at Yellow Station (Peak 2); it holds for every station.

**From the code** (world state 14: enter 0x236250, update 0x236418, arrival 0x236960; world state 15: 0x236058):
- **Arg 0, the lodge door volume** (builtin 68 action 4): lists 22 (`lodge_arr3`, flags 3) and 23 (flags 1), then overlay 0x1F
  "Would you like to enter the lodge?".
- **Arg 2, the transport booth:** list 11 (`hub_trans_arr`), then overlay 0x21 (the Map).
- **Arg 1, the Transport ride:**
  - It queues nothing when it starts. The update plays the ride (27A860).
  - It waits until the rider's in-air loop (list 14 heli, list 20 gondola) runs, then requests the destination rows
    (22CEA8(dest, 7), streamer +0x1C8).
  - At the arrival, 236960 waits for the rows (22D278) and places the rider (123F38 -> 11D390; a station: 11DE60(rider, 0, 2)).
    It then releases the loop (27A9F0) and activates the rows (22D088).
  - It plays no station cinematic and raises no prompt.
- **A Transport to the current location** (not a backcountry) goes to world state 15:
  - It places the rider with 11DE60(rider, 1, 2) and 11DF18, then starts the fade 2E4370(mgr, 1, 0, white, 0, 0, 1.0).
  - 2E47E8 draws the white at alpha 1 - t / 1.0. t grows by 1 / 60 per update (the timer's +0x10 is 60), so the white fades out
    over 60 ticks.
  - MCOMM Session Yes is the same state.

**PS2 captures** (silent, driven by pad input, `ps2_menu_capture.py`):
- `menus/stations-sj-to-green`: Peak 1, from `ctm/state-snowjam-arrival`, through MCOMM > Transport > Peak 1 > Freeride > Green
  Station > Yes.
  - It shows the gondola ride, then the rider's gondola loop with "Loading...".
  - Green Station then fades in with the HUD at full brightness.
  - The rider rides on through the station (rails, the first collectible, $507). There is no walk-in and no prompt.
- `menus/stations-to-c`: Peak 2, from `nav/p2/out-to-c/frprompt`, Ruthless -> Yellow Station. It shows the heli ride, Zoe's heli
  loop, then the station fading in, and the rider rides on.
- `menus/stations-ws15-*`: MCOMM Session > a point > Yes at Happiness, one exact snap per run (a snap costs ~40 samples, so the
  later snaps of a run land late). The MCOMM closes, then the world comes in from white with the HUD over it.

**The port:**
- It played `lodge_arr3` after every station Transport and parked the rider at the lodge prompt.
- That rule had been read from `ctm/60-green-arrive` / `61-cut`. That capture is a Transport to Green Station made while riding
  there: world state 15 (Session point 1), from which the rider glides into the lodge door.
  - The port now does the same: a Transport at Green to Green gives the walk-in and the prompt from the door volume, ~6 s in.

**Now:**
- pv `stationArrival`: a station arrival fades in from black over 30 ticks under the HUD, like a course (the same arrival
  236960). The rider then rides on. The door volume still gives the walk-in and the prompt.
- pv `sessionFade`: MCOMM Session Yes and a Transport to the current location fade in from white over 60 ticks with the HUD
  over it (`cutscenes.js fadeFrom`). Before, neither faded (`freeRide.whiteFade` was never defined).
- `round3/stations/station-vs.png` shows three rows: PS2, before (the cut, then the prompt), and now. The now row matches the
  PS2's frames: the station comes up at 22 MPH under the same signs.
- `round3/stations/ws15-vs.png` shows the white fade: PS2 against now.

**Checked** in Chrome and WebKit: Ruthless -> Yellow Station (with a career free ride), a Transport at Yellow Station to Yellow
Station, one at Green Station to Green Station, and MCOMM Session.

**Open:** the post-event return's white fade (`career-ui.js returnFade`: 58 ticks, covering the HUD) is world state 15 by its
own comment. By the code it is 60 ticks under the HUD. It was left unchanged here.

**Next (captured):** the station and connector frames. `runs/vp-stations/{c-tuck,d-glide,e-tuck}` have records and snaps for
Yellow Station -> C_CRA3 -> CRA3, Red Station -> D_DRA4 -> DRA4, and Black Top Station -> its connectors.

Tests: `test-visual-parity.mjs` R26; the arrivalFade check follows the new condition.

## 34. No rider at its bind pose (pv `riderPoseGate`)

Owen's "T-poses at the start of CTM" and the field bind-pose probe's hit: see [ctm-flow.md](ctm-flow.md) "T-poses on the first
load".
- The page's placeholder rider (RIDER_SAM, built by every course load) was drawn at its bind pose under the held transport loop
  across a world switch, far from the camera.
- The career rider was drawn at its bind pose on the ride's first frame after a switch, in view.
- Now: both start hidden, and the ride shows the rider once a tick has posed it. Test: R27.

## 35. HUD per mode in the real Single Event flow, and the freestyle finish panel (pv `finishLui`)

**The tool:** `tools/modeshot.mjs RUN OUTDIR [SNAPS]` plays the real Single Event flow against a PS2 event capture:
- the Single Event menu call, then the objectives card's Continue;
- the countdown lead-in, then the capture's pad (`web/compare-page-capture.mjs` timing; the lineup anchor words when
  `lineups.json` asks for them);
- the page's own frame at each capture snap, with its own camera and the HUD.

The rider's position was exact at every compared tick except cra3 2418 and dra4 2418.

**HUD per mode** (Chrome, 38 pairs over Peaks 2-3: race, slope style, big air, pipe, Rival Time):
- The HUD matches the PS2 frames: place, the OPPONENT line, combo, clock (the freestyle countdown), score, meter, speed,
  FINISH!, and the red last-10-seconds clock.
- The earlier direct-load sweeps could not show the freestyle clock, the standings or the opponent (section 19). This flow does.
- Differences that are not presentation:
  - **Posted standings** (big air, pipe): e.g. PS2 91620 / 51700 / 29880 against the page's 89940 / 48820 / 30720. The
    posted scores are draws (0x1453D0, a +-5 % jitter from the roster generator), and the page's generator state is not
    the capture's.
  - **Kick Doubt's opponent:** its score matches at 1219 but not at 3219 (+36647 against +28437), so the computer rider
    diverges between those ticks (AI simulation).
  - **EA RADIO BIG:** the popup is missing in these shots. The harness runs ticks faster than the song loads.

**The finish panel** (Single Event and career freestyle, Rival Points):
- The PS2 draws OV.LUI `finishov`, set up by 0x1E8200:
  - Run label: GMM+0 1 '1st run', 2 '2nd run', 3 'Final run'. 'thirdrun' is never used. There is no label for event
    kind 6 (points challenges).
  - Place: 'place%d' for 0x536730[player] + 1 up to 5, 'place6' below that.
  - 'pointstotal' is kT_OVRCMNPointstotal.
  - Medal: obj 0xD +0x14 (0 platinum, 1 gold, 2 silver, 3 bronze) in the final round or a Rival Points run. Platinum also
    shows in mode 4. The career award 0x154EE8 writes this record, so a Single Event shows no medal.
  - 'new_record' shows when the run's record rank (obj 8 +0x18 / +0x1C) is >= 0. 'Menu0000' is hidden.
  - The whole panel is hidden for multiplayer, TIME'S UP, points challenges 9-11, and round 1 when the next round
    (GMM+0x70) is 3.
- The timeline:
  - The 3D Ov outlines grow over frames 1-20.
  - The texts come in over frames 20-45.
  - Frame 45 holds.
- The port drew a flat chamfered box. It is now the LUI panel (`career-ui.js finishLui`, exported by
  `tools/export_audio_menus.py`).
- Frame 1 falls two ticks after the 3 s mark. The PS2 +0x470 reaches 3.0 at record 4582. The best fit to
  pipe-finishov2 4589..4614 is -2 frames (+-1).
- **Panel region MAD, PS2 `pipe-finishov2` against the page** (`round3/finishov/finishov-vs.png`: PS2 / before / now):

  | tick | before | now |
  |---|---|---|
  | 4594 | 20.4 | 2.7 |
  | 4599 | 39.7 | 4.6 |
  | 4604 | 32.3 | 6.7 |
  | 4609 | 19.4 | 8.4 |
  | 4614 | 12.7 | 7.1 |
  | 4634 | 12.6 | 7.1 |

  WebKit is within 0.1 of Chrome.
- The big air final (cba2 2818, 'Final run / 6th place / 0 pts') and the Rival Points layout (PS2
  `nav/bc/out-jam-finish/sample04620`, '2nd place 1210 pts', no run label) match.
- The medal, platinum and New Record! variants are drawn from the code (`round3/finishov/medals.png`). No PS2 capture shows
  them yet.
- The panel needs the re-exported `UI/audio-menus.json` (adds finishov and kT_OVRCMNPointstotal). Without it the old box is
  drawn.

**Seen, not changed:**
- **Pipe finish pose:** at The Junction the page's rider raises an arm while the PS2's stands (4589..4634, the position exact).
  This is animation, not the panel.
- **Lodge sub-screens still drawn generically** (PS2 `menus/lodge/26-trophies`, `35-save-game`):
  - Rider Details > Trophies, where the PS2 uses FE.LUI 127trophyroom (a peak list over the mountain with medal markers).
  - The lodge's Save Game, where the PS2 shows the FE "Save game" screen.
  - Rewards, Ubertrick Setup, Rider Profile and Career Highlights already play their LUI pages in the real flow.

Tests: `test-visual-parity.mjs` R28. R22's regex now accepts the `restartToCard` declaration list.


## 36. The lodge's Trophies (pv `trophyLui`)

Rider Details > Trophies is three FE.LUI screens on the PS2. The page drew a list with four boxes. `web/trophy-room.js` now plays
the three screens for `web/lodge-ui.js`, with the rules of their states.

**Screens and PS2 code:**
- **`ctm-trophies` = 125mountainroom** (vtable 0x46987C: enter 0x1D2990, bind 0x1D2AD0, input 0x1D2F68, markers 0x1D3340).
  - Menu Peak 1 / Peak 2 / Peak 3 / Peak Pass. It wraps (PS2 `trophy-locked`: Up from Peak 1 reaches Peak Pass).
  - Help by focus. Peak Pass shows kT_HELPViewPass. Peak 1 shows kT_HELPViewTrophies. Peaks 2 and 3 show kT_HELPViewTrophies
    when +0xB4 (the highest peak whose lock bit 0x145F90 is clear) reaches them, otherwise kT_MAPHELPLockedPeak2 / 3.
  - Cross on a peak opens its room even when it is locked (PS2 Peak 2 / Peak 3 rooms, `trophy-locked`).
  - **Markers.** Each marker is `mrk_<kind>_<n>`. The kind follows the event's mode:
    - 0 race, 1 slopestyle, 2 superpipe, 3 bigair, 4 racebackcountry, 5 freestylebackcountry;
    - peak events (modes 6+) get none.
    - n counts per kind over the peaks in table order (0x45AAD8, goals 0 and 1).
    - The sprite is the FE texture 'dot_visit arrow' when the event has a medal (0x1CED90), else 'dot_loc arrow'.
  - **Pass popup** (0x1D32E0).
    - Shows 'Group peak pass'. 'Front' shows once the pass texture has loaded.
    - The picture is RWRDPS2 peak pass `character * 3 + (+0xB4)` (0x156B28), e.g. pass_zoe1.
    - The timeline runs from frame 90 to 114. The frame shapes grow from 20 % (`shapeScale`), the veil fades in over 8 frames and
      the pass zooms.
    - Triangle hides the popup first (0x1D2EA0).
- **`ctm-trophy-peak` = 126peakroom** (0x4697AC: bind 0x1D38F8, input 0x1D3C80, help 0x1D3EC8, rows 0x1D3F80).
  - The title is kT_PeakNname.
  - Each goal's thumbnail (`trph_*`, FE texture trc1..ter3 = the RWRDPS2 entry's +8 name) shows once the goal is complete
    (0x157BF0).
  - The focused goal's rows come from 0x1CE6F0 and 0x1CE758.
    - Race / freestyle: course names from 0x144C60.
    - Rival events: "Happiness / Ruthless / Throne %s" with kT_cmnRaceCaps for races, and a literal "... Jam" for jams.
    - Peak events: kT_EventPk1Race and the like.
    - Exploration: kT_OVRCMNCollectibles and kT_TITLEBigChallenges.
    - Earnings: kT_OVRCMNEarnings.
  - Each row shows its text block, the medal back, and then:
    - with a medal: the medal icon (FE texture rac/fst/exp/ern + p/g/s/b) and the check;
    - without: the cross.
  - Help: kT_HELPSelectGoal when the goal is complete, otherwise kT_HELP_LockedGoal. Cross opens only a complete goal.
- **`ctm-trophy-room` = 127trophyroom** (0x4696DC: bind 0x1D43F0, input 0x1D4698, picture 0x1D47A0, stats 0x1D4890).
  - The title is kT_TITLERaceTrophy / FSTrophy / ExploreTrophy / EarningsTrophy.
  - Row 0 is the trophy name (0x156A90). Rows 1..n are the goal's rows.
  - The menu wraps over the n + 1 rows (PS2 `trophy-stats`).
  - The picture ('bitmap') on row 0 is the trophy (trophy_race01, ...). On an event row it is that event's medal picture
    (0x156AE0, goal * 4 + medal). With no medal there is no picture.
  - 'stats' is shown on the event rows only (0x1CE9E0):
    - kT_OVRCMNYourBestTime with "%02.0f:%02.0f" of the record's whole seconds;
    - kT_OVRCMNYourBestScore;
    - kT_CMNYouEarnedNum with "$ 123,456";
    - kT_CMNColllectNum / kT_CMNChalCompNum with the peak totals 155 / 148 / 122 and 40 / 27 / 21 (0x153390, 0x154368; these equal
      the port's platinum thresholds).
  - Earnings has one row, gold when complete (0x158AF0).

**Shapes.** The screens draw each shape at its vertex alpha only, as 139buy_popup does. Without this the popup veil was
0.686 x 0.686: the panel under it measured 121 against the PS2's 147. The rest of the three screens' shapes are at 255 and 100 %.

**Data** (`tools/export_character_select.py`, the scratch export for the coordinator):
- the three screens;
- `trophy_sprites`: trc1..ter3, racp..ernb, 'dot_visit arrow' and 'dot_loc arrow' on FE_1-10 / FE_1-9;
- the strings;
- label names for the elements the code binds by name.

The existing screens only gain labels. Without the new asset the switch keeps the old list.

**PS2 captures** (`local/ps2-capture/menus/`, from `trophy-base.final.p2s`):
- `trophy-medals`: medal bytes patched into Zoe's event records (0x4AAAC8 + 0xAD0 + 8 x slot + 1).
- `trophy-stats`: adds medals, bests (+4) and earned (+0xAC8 = 123456).
- `trophy-earn`, `trophy-locked`.
- `trophy-pop336 / 346 / 360`: each run's first snap, which is exact.

**Check:** harness `tools/trophy.mjs`. It sets the capture's career state, then each shot's screen, and can drive the flow with
keys. Full-frame MAD, Chrome / WebKit:

| Shot | Before (old list) | After |
| --- | --- | --- |
| tour f100 (Trophies, Peak 2) | 47.1 | 5.45 / 5.57 |
| medals f380 (markers) | 51.6 | 5.53 / 5.66 |
| medals f600 (Peak 1, race rows) | none | 7.11 / 7.24 |
| medals f850 (Race Trophies) | none | 5.24 / 5.31 |
| stats f1100 (Snow Jam, 02:05) | none | 5.93 / 6.04 |
| stats f2000 (freestyle rows) | none | 7.65 / 7.72 |
| stats f2550 (Crow's Nest, 67890) | none | 6.09 / 6.18 |
| earn f1600 ($ 123,456) | none | 6.04 / 6.17 |
| locked f330 (Peak 2 room) | none | 7.13 / 7.22 |
| popup f360 | none | 5.01 / 5.20 |

The remaining difference is edges and the snow loop's flakes.

**Popup timing,** with the page clock held:
- PS2 336 matches t = 4 (MAD 3.97).
- PS2 346 matches t = 14 (2.51).
- So the popup's frame 0 is the Cross sample + 2, and the page's timeline runs at the PS2's rate.

**TransitionOut** (leaving; the label at frames 70 / 60 / 70): ported with the other lodge screens' changes in section 42 (pv `lodgeFlash`).

Tests: `test-visual-parity.mjs` R29 checks the rows, medals, stats lines and flow. With the asset present it also checks the
markers, menu and help.

## 37. The lodge's Save Game (pv `lodgeSave`)

The page saved at once and showed "Save complete." in the lodge frame. On the PS2 the lodge's input 0x1F3A38 handles Save Game
(menu value 8). It pushes cFEStateProfileLoad through 0x18ECA0 with mode 3 and the pad's port mask. That state is FE.LUI
93profile_load in save mode.

`web/save-game.js` plays this for `web/lodge-ui.js`; `web/career-ui.js` opens it from the lodge menu.

**The screens, as the PS2 draws them** (PS2 captures `local/ps2-capture/menus/save-s1..s8`, `lodge/35-save-game`):
- **The list:**
  - title 'Save game', help 'Save your progress.', legend Save / Previous;
  - the card in the box above the rows: kT_MEMMemDevicePS2 "MEMORY CARD slot 1";
  - one row per save: the player name, or "< E M P T Y >" (kT_MEMEmpty).
  - With no card: kT "Please insert a memory card (PS2) into MEMORY CARD slot 1.", Previous only and no help line (35-save-game).
- **Cross on a row:** the name keyboard, "Enter player name", holding the player name (the port's Fullkeyboard). Triangle
  returns to the rows.
- **Done:** these memory-card popups follow (the manager is at 0x1B6xxx):
  1. kT_MEMFECheckingCardPS2 (four lines).
  2. Over a save, kT_MEMOVOverWriteFile "Would you like to overwrite PLAYER 1?" with Yes / No, No focused. No returns to the rows.
  3. kT_MEMOVSaveSavingPS2NGC.
  4. kT_MEMOVSaveDone "Save complete." with Continue. Continue returns to the lodge with Save Game focused (save-s5).
- **Unformatted card (PS2 only):** "…is unformatted.", the format question, "Formatting…". These are captured in save-s1..s4 but
  are not in the port: the browser's card is always formatted.

**The browser's card** is `localStorage` (`career-save.js`, one career save):
- row 1 is that save; rows 2..6 are not drawn (as in the port's Load game);
- no card = storage blocked;
- a failed write shows kT_MEMOVSaveFailed with Continue.

The Checking and Saving popups stay up 60 and 90 frames. On the PS2 their length is the card's I/O time: 40..180 frames here,
and the write took 750 frames in ARMSX2.

**Popups:** cFEPopup's own layout (section 39, `web/fe-popup.js`), replacing the first fitted model. The memory-card manager shows its
popup without the opening animation (0x1A9BC0 -> 0x1C5DD8) and keeps it up from Checking to Save complete, so the veil (the popup LUI's
'grey background' with its own 8-frame fade) fades in once.

**Check** (full frame MAD, Chrome / WebKit):

| Screen | Before | After |
| --- | --- | --- |
| rows (s6 f150) | 55.3 (the old screen) | 5.86 / 5.97 |
| no card (35-save-game) | – | 5.05 / 5.11 |
| keyboard (s6 f420) | – | 5.88 / 6.09 |
| Checking (s7 f60) | 15.0 (first try) | 3.98 / 4.01 (fitted: 4.59) |
| overwrite? (s7 f200) | 13.8 | 2.78 / 2.84 (fitted: 3.67) |
| Save complete (s8 f86) | 14.9 | 2.64 / 2.73 (fitted: 3.16) |

The key flow in both browsers goes rows -> keyboard -> Checking -> overwrite? -> Yes -> Saving (the career written, `savedAt`
set) -> Save complete. -> the lodge on Save Game.

**Deliberate difference (Owen, 2026-09-28):** one slot. The single autosaved career is row 1; the PS2's rows 2..6 ("< E M P T Y >",
six card slots) are not drawn.

**Not ported:**
- **Delete (Square):** the port autosaves after every change, so a deleted save would come back at once.
- (Options > Save/Load 'Save game' now opens this screen too, and comes back to Save/Load: `web/fe-saveload.js`, pv `lodgeSave`.)

**Tool:** `tools/ps2_menu_capture.py` gained `MEMCARD=card.ps2 [MEMCARD_OUT=out.ps2]` (a card in slot 1; a missing file = an
unformatted card). It also retries a snapshot while ARMSX2 refuses savestates with the card busy.

Tests: `test-visual-parity.mjs` R30.

## 38. Blending in linear vs encoded space (investigated, nothing changed; Owen's call)

**Why the port is linear.** three r186 colour-manages into a linear working space: every byte-domain material (world, rider,
board trail, cutscenes...) does the GS maths in bytes and ends with `sRGBTransferEOTF` (15 files), the world pass is a half-float
linear target, and the fog composite (`fog-renderer.js`, `outputColorTransform = false`) converts back with the OETF and does the
GS fog in bytes. So opaque pixels round-trip exactly, but every blend inside the world pass happens on linear light. The GS blends
encoded bytes: ALPHA 0x44 = ((Cs - Cd) x As >> 7) + Cd, 0x48 = Cs x As + Cd (engine/SNOW_RECOVERY.md). Since 2026-09-22 the effects
that needed it draw after the fog in the 8-bit encoded composite (`snow-composite.js registerEncodedEffect`).

**Already encoded:** snow / spray / wake, boost strips, opponent FX, rival beams and "!" icon, set-piece particles and halos, weather,
terrain sparkle, glare / sun flare / light glow stages, the fog itself, and the HUD / LUI (2D canvas, composited by the browser in
encoded space). Their remaining gaps to the PS2 are not blend space (e.g. buy-popup veil MAD 13.4 -> 6.3 from the veil's shape;
rival icon (227,146,65) vs PS2 (215,130,45); searchlight 1.77x, section 17).

**Still linear:** world translucent models (blend 1 / 2, fences, glass, fringes), world additive (blend 3; pv `byteBlend` off),
the sky blends, rider hair / `ea*` (riderDrawState), the board trail, crowd flashes and cutscene overlays.

**Measured, GPU (the same draw read back, Chrome and WebKit, WebGPU and WebGL2 identical;
`local/browser-validation/blend-space/blendspace-test.js`, 16,384 blends: every source byte x 8 destinations x 8 GS alphas,
against the GS byte formula):**

| framebuffer | mean abs error | max | within +-1 |
|---|---|---|---|
| today: half-float linear, EOTF in / OETF out | 10.9 | 74 | 26 % |
| 8-bit sRGB-format target (`rgba8unorm-srgb` / `SRGB8_ALPHA8`) | 10.9 | 74 | 26 % (the hardware blends in linear too) |
| half-float, encoded values (no EOTF / OETF) | 0.34 | 1 | 100 % |
| 8-bit unorm, encoded values | 0.41 | 1 | 100 % |

The +-1 left is GS truncation (>> 7) against the GPU's rounding; fixed-function blending cannot truncate.

**Estimated on content** (real textures' partial-alpha and edge texels blended over the colours of PS2 race / Select Character frames,
|linear - encoded| in levels; `blendspace.py`): world blend 1 / 2 edges mean 16-18, p95 48-49, max 73; world additive mean 42-49,
p95 75-76 (linear 31-37 darker on bright backgrounds); rider hair edges mean 15-16, p95 42-48; board trail (vertex alpha
0.05-0.95) mean 15, p95 38, linear 10 lighter; crowd flash mean 25, p95 70. These are per blended pixel, not scene errors: no aligned
PS2 frame isolates these classes.

**Coverage** (Chrome, frozen frames, meshes of each class drawn alone): ARA1 4 s blend 1 / 2 geometry 23.7 % of the frame (only
its partial-alpha texels differ); ARA1 6 s blend 1 / 2 3.4 %, fringe 0.8 %, additive 0.15 %, rider blended 0.21 %; BRA2 6 s
additive 3.2 %, blend 1 / 2 0.4 %, rider 0.2 %.

**Select Character hair (inconclusive).** An encoded front-end prototype (scratch only: no EOTF in the preview materials and
`outputColorSpace = 'srgb-linear'`) did not move the hair consistently against the PS2's (VRAM CLUT alpha 128 -> 64,
`speck/s40-hair-op64`): most FE hair pixels composite over the page background, and the frames did not align well enough.
Opaque pixels were unchanged (rider box mean within 0.4 levels, WebGPU and WebGL2).

**Options.**
- A, port-wide: keep encoded values in the world pass. One shared helper replaces the EOTF at the material outputs, the fog /
  encoded composites drop their OETF, `outputColorSpace = LinearSRGBColorSpace` in the front end (no frame-buffer target: three
  adds one only when the output space differs from the working space), sRGB-decoded textures become NoColorSpace, and three's own
  Color values are fed encoded (or `ColorManagement.enabled = false`, which `ColorSpaceNode` honours). Same on the WebGL2
  fallback. Runtime: no extra pass; the world target could drop to 8-bit (4 instead of 8 bytes / pixel, as the GS's 32-bit frame).
  Cost: about 20 shipped files and a re-check of every colour-checked test and visual-parity frame; MSAA edges resolve in encoded
  space (the PS2 has none). Must switch all at once per render target.
- B, per class into the encoded composite (as `byteBlend` did): each class needs its own fog factor and ordering against the other
  world translucents; `byteBlend` showed no visible gain on Metro City.
- C, leave it: the differences sit on partial-alpha texels (about 1 % of a typical frame, 3 % on additive-heavy views).

### 38a. Option A built: pv `encodedBlend` (off; Owen chose A, 2026-09-28)

**How.** `web/frame-space.js` decides once per page (before any material or Color exists) what the frame holds; every colour path goes
through it, and `test-frame-space.mjs` fails if a shipped module uses `sRGBTransferEOTF` / `OETF` or `SRGBColorSpace` anywhere else.
Rules for a material:
- GS bytes or encoded 0..1 colour -> `toFrame(bytes / 255)` (EOTF when linear, the value itself when encoded);
- a pass reading the frame (fog composite, encoded composite seed) -> `fromFrame(value)`;
- three's own linear-light maths (the cutscene sets / skies / PDA: texture x colour x 31/16 on sRGB-decoded texels) -> `linearToFrame(linear)`,
  textures in `linearTextureSpace`: the same pixels as before, only the blend changes;
- a stock material sampling a texture straight into the frame (riders' menu path, rival beam) -> `texture.colorSpace = frameTextureSpace`;
- encoded frame: `ColorManagement.enabled = false` (a Color set from hex stays the byte it was written as) and `outputColorSpace` = linear
  (three adds no frame-buffer target or output pass for a direct render: the front end, cutscenes).

Touched: `frame-space.js` (new), `main.js` (import, `9-` textures, `configureFrameSpace`), `world-material.js`, `fog-renderer.js`,
`snow-composite.js`, `rider-material.js`, `fe-preview.js`, `opponent-riders.js`, `cutscenes.js`, `rival-beam.js`, `board-trail.js`, `crowd-2d.js`,
`boost-renderer.js`, `impact-fx-renderer.js`, `snow-renderer.js`, `startfire-renderer.js`, `wake-renderer.js`, `weather-renderer.js`,
`set-piece-halos.js`, `set-piece-particles.js`; test pages `rider-material-gpu-test.js` and `snow-colour-gpu-test.js` follow the frame space.

**Composites.** The encoded-composite effects (snow, wake, boost, opponent FX, rival icon, set-piece particles / halos, weather, sparkle)
stay where they are: they are priority 7, drawn by 363490 after the fog composite onto the fogged frame, unfogged; in the world pass the fog
composite would fog them at the depth behind them. Their seed was already the fogged encoded frame. `byteBlend` is moot with the switch
on: the world additives blend in the world pass as the GS does, and the fog composite fogs Cd + Cs x As (world-material.js `byteBlendOn`).

**Verified** (switch on and off; Chrome and WebKit, WebGPU and WebGL2 unless noted):
- The blend itself (`test-frame-space.mjs`, 16,384 blends through `toFrame` into a half-float world target, read back as the fog composite
  reads it): linear mean 10.91, max 74; encoded mean 0.34, max 1 (100 % within 1; the rest is GS truncation vs GPU rounding). Opaque bytes exact
  in both. Same numbers in all four browser / backend pairs.
- All twelve `*-gpu-test.html` pages pass in both states and both backends, in Chrome and WebKit. The 15 node tests of the touched modules
  (board trail, cutscenes, fe-preview, impact fx, opponent riders, presentation, rider draw state, snow, set pieces, sky addressing,
  startfire, weather, world material, visual parity, ctm flow) pass in both states.
- Opaque pixels, race (30 aligned frames from 9 PS2 runs, `aa=0`, Chrome; 26 in WebKit and WebGL2): every pixel the switch changes belongs
  to a blend. 88 % fall in the classes hidden one at a time (world blend 1 / 2 and additive, rider hair / `ea*`, crowd flashes, board
  trail); the other 44.5k of 374k are the sky pass's blended ring at the top of the frame and the rival locator beams in the rival races.
- Opaque pixels, Select Character (fixed pose, direct canvas render): 97 % of the rider's opaque pixels identical, 1.4 % off by 1 (bilinear-filtered fractional values rounded through the old half-float output pass), the rest are the
  blended `ea*` pieces.
- Blended classes against the PS2 (`vpshot --pin` on 9 PS2 runs: ARA1 race / set pieces / event race / rival race, CRA3, EBC3, ERA5,
  BHP1, Metro; `aa=0`; |port - PS2| after a 3x3 box, on the pixels the switch changes; Metro only in Chrome WebGPU):

| class | Chrome WebGPU (30 frames) | WebKit WebGPU (26) | Chrome WebGL2 (26) |
|---|---|---|---|
| world blend 1 / 2 (fences, glass, trees, fringes) | 34.00 -> 34.69 (44 % closer) | 35.32 -> 34.86 (50 %) | 35.62 -> 35.18 (50 %) |
| world additive (searchlights, glows) | 28.00 -> 27.84 (51 %) | 32.67 -> 32.11 (57 %) | 32.58 -> 32.00 (57 %) |
| same without Metro, Chrome WebGPU | blend 1 / 2 35.62 -> 35.19, additive 32.62 -> 32.02 | | |

  Encoded blends are 7.5-8.3 levels darker on world blend 1 / 2 pixels, +0.3-0.6 on additives. The result splits by content. Where those
  models' own colour matches the PS2 (their alpha ~ 1 texels, which no blend space changes, within 10 levels), the switch moves the
  partial-alpha pixels to the PS2: PS2 - port -4.9 -> +2.6 (Chrome, 11 frames), -7.4 -> -1.1 (WebKit, 7), -6.7 -> +0.9 (WebGL2, 8); mean
  |bias| per frame 5.4 -> 4.8, 8.1 -> 3.7, 7.1 -> 3.7. Where the models themselves are darker than the PS2 (+10 to +45 on their alpha ~ 1
  texels: Metro's stadium glass, CRA3's gondola windows and trees, ARA1 event-race 1618 / 2318) the darker, correct blend widens the gap:
  +7.8 -> +15.2 (Chrome). Over all frames the port's translucent world models are 4-7.5 levels darker than the PS2 where the blend does not
  matter: a content issue to fix separately (Metro 79, aligned to 2 mm: the PS2 glass pixel is brighter than the port's own glass texel, so
  no blend reaches it; Metro alone: 30.0 -> 33.5).
  Hair / `ea*` (race, 700-830 px), crowd flashes (180-400 px) and the board trail (6-7k px) move -5 to -13 levels, but the replayed riders
  drift from the PS2's (metres by tick 1000), so their rows (hair 41.2 -> 42.0, trail 91 -> 94 in Chrome) compare different pixels.
- Hair, Select Character (VRAM CLUT alpha 64 experiment, section 9 of xbox-textures.md; port pose fixed): where the hair lies over the body,
  its blend position (0 = no hair, 1 = full hair) averages 0.582 linear -> 0.670 encoded; the PS2 frame 0.813 (all hair pixels).
- MSAA (Metro 79 / 318 / 619, aligned): edges are 2.6-5.1 % of the frame; the encoded resolve is 4 levels darker on them, and |port - PS2|
  on edges is 19.53 -> 19.56, 25.07 -> 24.69, 40.50 -> 40.61 (no AA: 23.5 / 26.7 / 43.3). Neutral.
- Cost (six-rider ARA1 Single Event and Select Character, alternating loads; frames are vsync-bound at 16.7 ms):
  - WebKit desktop (5 + 5): event load median 7735 ms linear, 7639 ms encoded (the first encoded load, 15.9 s, compiled the new
    shaders once); race frames > 20 ms per 10 s: 16-78 linear, 17-74 encoded; FE mean 16.83 / 16.78 ms.
  - WebKit phone tier (`quality=low`, no MSAA; 3 + 3): race frames > 20 ms 69-89 linear, 47-70 encoded; p95 26 / 26 ms.
  - Chrome (2 + 2): 60 fps in both, no frame over 20 ms.
  - GPU textures (three's accounting): -12.2 MB with MSAA (WebKit FE 20.1 -> 7.8 MB, race 80.1 -> 67.9 MB; Chrome 15.8 -> 7.8, 59.9 -> 52.0), -3.2
    MB on the phone tier: the front end's half-float frame-buffer target and output pass are gone.
- Evidence: `local/browser-validation/blend-space/encoded/` (scripts, shots, class masks, JSON).

**Open.** The 8-bit world target (4 instead of 8 bytes per pixel) is a follow-up once the switch is on: check banding in the fog and sky
gradients first. The cutscene sets / skies / PDA keep three's linear-light texture x colour (their blend is encoded now); the PS2 multiplies
bytes (section 17's modulate), a separate fix. The `?originalWorld=0` fallback materials and the old rival icon (`rivalIcon` off) are not
frame-space aware.

## 39. The front end's popup box, decoded (pv `fePopup`)

cFEPopup is the class 0x46CBD8 (ctor 0x1C58E8, setup 0x1C6B08) on FE.LUI 'popup'. It sizes and places its own box, and every FE Yes/No
question uses it. The port had three fitted layouts (the lodge's questions, the Options question, and a first fit for the save popups).
`web/fe-popup.js` now does what the code does.

The element names come from its bind (0x1C6230): whitefg, redbg, grey background, Big / Small shape and outline, Shadow, black bg,
Menu0000, buttons, popupText, popupTitle, popupPost, EnterTextBox, TextBoxPair, StaticTextBoxLabel, EnterBackground, Option0..4.

**Layout** (`fePopupLayout`):
1. **Start width.** +0x260 starts at desc +0xB0, or 300 when compact (desc +0xBC), else 472.
2. **Message** (0x1C7258, compact). W is the widest prefix within the element's 472: 0x1C94B0, advance x scale per character, summed
   in single precision. If W > 300, the text elements take width W (0x1C70F0). The text word-wraps at W: 0x1C9938, where a character
   past W breaks at the last space. Each line is the font box of 0x3921F0: x from min(pen + dx) to max(pen + dx + w), and the width
   is x1 + x0. Lines are 17.4 + 3 apart at 60 % (0x1C9038). The width becomes max(width, widest line); then h += 10.
3. **Options** (0x1C7388):
   - h += EnterTextBox's 22 (0x1C6D30 counts it even when it is hidden);
   - then 20 per option, then 10;
   - +0x29C is the widest option text.
4. **Menu** (0x1C8758). Placed at x = (640 - widest) / 2 - 270 and y = h after the message. The Cross icon ('buttons') goes to the
   same point.
5. **Type 1 box** (0x1C66E8 shows the framed parts).
   - Scale is ((w + 20) / 504 x 1.05, (h + 15) / 204) (0x1C7620; the double 1.05 is at 0x466090).
   - The parts sit at y = 92 + h / 2 - 7.5 (0x1C78C0).
   - Texts are black (0x4C6798).
   - Compact popups are not shifted, and their texts are centred on x 320 (0x1C7BE0 / 0x1C7C20).
   - Type 0 (whitefg / redbg) uses (w + 15) / 516 and (h + 10) / 296, at y = 92 + h / 2 - 5.
6. **Opening** (cFEPopupConfirm, 0x1C5C30 once a frame).
   - The parts step from 0.2 as 0.2 + k (target - 0.2) / 24, up to k = 25, and stay there (one step past the target).
   - Texts and icons are clear (0x1C5F20) until the LUI's 'Start' label (frame 25).
   - The memory-card popups (0x1C5DD8) appear at the target at once.
7. **Veil.** 'grey background' plays its LUI animation 00438b41: A 0 -> 175 over 8 frames, a vertical gradient (111,177,210) ->
   (200,225,238), drawn at its vertex alpha.

**Checked against the live popup objects** (EE RAM of the PS2 states):

| Popup | w | h | menu | scale | parts' y |
|---|---|---|---|---|---|
| save-s7 overwrite? | 345.6 | 102.4 | (33.8, 30.4) | 0.7617 x 0.5755 | 135.7 |
| s4 / s8 Save complete. | 300 | 82.4 | (9.2, 30.4) | 0.6667 x 0.4775 | 125.7 |
| s3 format question (3 lines, text width 466.8) | 445.2 | 143.2 | (33.8, 71.2) | 0.9692 x 0.7755 | 156.1 |
| lodge-quitprompt / saveprompt | 300 | 102.4 | – | 0.6667 x 0.5755 | – |

The port reproduces every value. The lodge questions' parts are at 0.6861 x 0.5911, step 25. The opening was captured with states,
each run's first snap exact: lodge-quitpop88 (A4 8, scale 0.336), 96 (A4 16, 0.492), 108 (A4 25, 0.686). The page matches frames
6 / 13 / 26 after opening, MAD 3.3 / 2.2 / 2.6.

**Where it is used:**
- the save popups (section 37);
- the lodge's Quit / Save progress / Quit-and-save questions (`fe-screens.js drawLodgePrompt`, pv `fePopup`, on).

The lodge questions went from full-frame MAD 11.9 / 11.5 to 2.56 / 2.40 in Chrome and 2.61 / 2.46 in WebKit (PS2 lodge-quitprompt,
lodge-saveprompt; 64-lodge-quit-confirm 11.9 -> 2.56).

**Not this class:**
- **139buy_popup** is cUIStateBuyPopup (vtable 0x46CB08). It only fills widgets (0x1CAC98); its box is the screen's own LUI shapes and
  timeline, already played by `web/buy-popup.js`, so nothing is sized there.
- **The Options "Would you like to save your Options?" question** is a larger, lower box. No cFEPopup object is in the 42-save-options
  state, so its class is not traced yet. It keeps its fitted layout (`fe-screens.js drawPrompt`), open.

Tests: `test-visual-parity.mjs` R31 (the layout against the RAM values, the growth steps, the routing) and R30. Tools:
`tools/lodgeprompt.mjs`, `tools/savegame.mjs`. `tools/fitpopup.mjs` is kept only for the record.

## 40. Lit LiveComp instances: the Gravitude billboard's "dark back" (pv `litLiveComp`)

**The report:** on the Gravitude billboard fall (PS2 `runs/billboard/grav-bb`, frame 1208) the PS2 shows the falling board dark,
where the page draws its poster. The first guess was back-face culling. It is not culling: it is per-vertex lighting.

**No culling on the PS2.** LiveComp and static models are drawn by 0x356298 -> 37E238 with VU1 program 3. That program's vertex loop
(0xDF0..0x1118), its clipper and its env-map routine have no facing or area test; ADC is set only on each strip's first two vertices
(vi9). The page's `DoubleSide` is right. The left fragment at 1208 confirms it: its poster faces away from the camera there and the PS2
still draws it (the bright sliver in the notch). Setting it to FrontSide removed that sliver.

**The poses are exact.** New PS2 run with kept states (`runs/billboard/grav-bb-states.tick1209 / 1230.p2s`, same savestate and pad
script). The LiveComp's clock (entity +0xD4 rate, +0xD8 low, +0xDC high, +0xE0 time) is 2.3667 / 2.7167 s, and its node world
matrices at 0x5A3A00 (0x40 apart) are bit-exact with the page's at the same time.

**The rule: lit instances.** Both crash billboards have runtime flags 0x40214325 (instance +8). Bit 0x4000 is the lit-instance bit
(the backcountry heli, docs/presentation.md 16). In all 17 countdown audits (35,349 instances), authored descriptor flag 0x40000000 is
set exactly when runtime flag 0x4000 is: 296 instances (crashbags, the billboards, ERA5's ice bits, ABA1's cars and buses, CRA3's
falling tram, rockslides, avalanche bits).

The lighting path:
- 37E238 tests the bit (0x37E3B8) and calls 2F5148 -> 2F5400, the instance's light cache (32 entries of 0x180 bytes, keyed by
  resource).
- The cache entry (2F5400): +0x00 the resource, +0x10 the ten rows the VU gets, +0xB0 the position they were made at, +0xC0 the
  light overlap list, +0xD0 the environment bank.
  - The bank comes from the cache's painter wrapper stepped at the instance's x/y: Lighting reference 3 of the payload there
    (vt+0x140), else reference 0 (vt+0x128) when that name is empty, else the course default gp+0x12D4. It is re-read after 5 m
    of travel. (Runtime flag 0x1000 does not choose the getter: it makes 2F5400 recompute the position on every call, so only
    such an instance is relit as it moves; without it the rows are made at the first draw and kept.)
  - Every 10 cm of travel, a +-10 m box re-queries the lights (331450). The rows are then the bank plus up to 4 kind-6 local lights,
    ranked by 2F5D30 and added by 38A6A8 (2F5AF0 -> 2F5B68, capacity 4). These are the same routines as the rider's eight
    (engine/local_light_selection.hpp, rider_irradiance.hpp).
- VU1 program 3 scales the rows by 128 (0x2170) and at 0x8B8 evaluates, per vertex, on the normal through the node's rotation:
  L = FTOI0(clamp(r0 + r1 x^2 + r2 y^2 + r3 z^2 + r4 xy + r5 zx + r6 yz + r7 x + r8 y + r9 z, 0, 255)).
- L replaces the baked colour: Cs = T x L >> 7, As = Ta.

So the colour follows the node as it tumbles. The page drew the rest-pose baked colours.

PS2 RAM (`local/browser-validation/visual-parity/tools/litcache.py`):
- The billboards' entries (0xACC2D0 / 0xACC450) hold EOBR1's ten rows, the same at +0x10 and +0xD0 (no local light).
  ERA5 crashbag_0_00011's entry (0xACC150) holds EOBR1 at +0xD0 but different rows at +0x10, so local lights reach it.
- The CRA3 tram's entry (cra3-full tick10418..11618, 0xACC150) holds COBR1, also with no local light.

At 1209 the tipped panel's poster evaluates to L = (7, 17, 33): near black. Enhanced, the PS2 frame shows the poster's swoosh
strokes on the dark board. The left fragment's poster gets L of about (100, 112, 119), so it stays bright.

**Port:**
- `tools/export_livecomp.py` gives every LiveComp instance with authored flag 0x40000000 a `lighting` record: the location's object
  bank, via `export_cutscene_sets.lighting_bank`, rows x 128. The records are ERA5's two billboards (EOBR1) and CRA3's tram (COBR1).
  The re-export only adds that field.
- `web/world-material.js litWorldMaterial` builds that colour. The bank is evaluated per vertex (a varying) on `normalWorld`; a
  LiveComp mesh's matrix is its node's rest -> animated transform. Bytes go through `toFrame`.
- `web/set-pieces-renderer.js` gives the lit instances' LiveComp meshes this material (pv `litLiveComp`, on).

Results, luma MAD in the billboard region (PS2 | before | after: `local/browser-validation/visual-parity/lit/sheet.png`):

| Tick | Before | After (Chrome) | After (WebKit) |
|---|---|---|---|
| 1169 | 20.2 | 15.5 | 15.7 |
| 1192 | 24.0 | 21.5 | 21.8 |
| 1208 | 28.1 | 24.1 | 24.3 |
| 1229 | 23.3 | 22.4 | 22.5 |

At rest (1169) the posters are darker, as on the PS2. At 1192 and 1229 the fragments are dark where the page drew them bright, and
at 1208 the board is dark with its poster faint. CRA3 renders with the tram lit (4 meshes, no errors).

**Static lit instances, exact (pv `litInstances`, on; 2026-09-28).**
- `engine/lit_instance_lighting.hpp`: 2F5AF0 / 2F5B68 with capacity 4 over the light-tree query of the position +-1000, ranked by
  2F5D30 and added by 38A6A8, the rider's ported routines. `tools/lit_instance_rows.cpp` (native) and `tools/export_lit_instances.py`
  derive each lit instance's bank (the painter payload at its x/y, reference 3 / 0 / the course default) and rows at its instance
  position: `local/event-activation/<LOC>/lit-instances.json` for every audited location.
- Checked against every PS2 light-cache entry in the savestates under `local/ps2-capture` whose bank is the object bank (event
  contexts): 199 instances in 16 locations bit-exact. Ten are exact only at the position the PS2 lit them at: BHP1's four animated
  cars (flag 0x1000, relit as they move), CRA3's two blimp pads (on the moving blimp at their first draw), ERA5's four ice bits
  (MeshAnim pieces, first drawn in flight; 0.002-0.04 off at the instance position). In mountain free ride the cache often holds the
  course default xPBR1 instead (a missing painter section): not ported yet.
- `web/world-batches.py` gives each lit instance its own batches tagged `lighting` (rows, bank, relight): +5 batches over all 16
  locations (CHP2 2, EBA3 1, EHP3 2); the rest already had their own. `web/world-material.js litWorldMaterial` is one shared graph
  per blend class with the ten rows as object uniforms (`userData.litRows`, x128): one program for every lit instance (BHP1: 52 ->
  53 pipelines).
- BHP1 bag-bhp1 1141 / 1144, the crashbag in front of the camera: box MAD 27.3 / 27.1 -> 19.8 / 17.9 (Chrome, before encodedBlend);
  with encodedBlend on 31.9 / 31.7 -> 28.8 / 22.1. ERA5 grav-bb billboard regions (encodedBlend on) 26.0 / 29.4 / 33.4 / 28.8 ->
  23.3 / 27.3 / 30.1 / 28.2.

**Open:**
- **Moving lit instances** (flag 0x1000; 15 in the 16 audits):
  - Which: BHP1's four "anim" cars / bus / truck; the ospreys (ABA1, ABC1, CRA3, DRA4, DSS2); the os609 in-air heli (ABC1, DBC2,
    EBC3); EBC3's cessnas; ESS3's sled.
  - The port does not move any of them: they are in none of the core's set-piece / spline / roller / chairlift / attached seeds or
    the LiveComp data. So it draws them at the instance matrix, where the shipped rows are exact (2F5400 with 0x1000 and no entity
    reads row 3).
  - The PS2 moves at least BHP1's cars: their cache entries are lit away from the instance position. That animation is the gap,
    not the lighting.
  - 2F5400's state machine, for when they move:
    - relight after more than 10 units of travel from +0xB0 (a 4-component distance);
    - re-read the bank only after more than 500 units in one step (a new entry, or reappearing after a cull). When gp+0xA74 is
      nonzero the bank is 2EE010(gp+0x24C0) instead (not traced);
    - re-query the lights (331450) only when the position leaves the strict +-1000 box of the last query (2F5A70 list);
    - re-rank and select (2F5AF0, 4) at every relight;
    - entries and lists come from 32-slot pools with LRU eviction (2F59D0 / 2F5A70).
  - The core export (task d) waits for a consumer.
- **The mountain world's lit instances** (ABA1's cars, the peak packages) need the peak re-export and its painter context.

Tests: `test-visual-parity.mjs` R32 (the exported banks against the PS2 cache rows, the flag rule, the wiring). Tools:
`tools/vpshot-clk.mjs` (vpshot with the grav-bb clock table `grav-bb-clk.json` and `--eval`), `tools/litcache.py`.

## 41. Draw paths the port missed: MultiSpline cables (pv `cables`), fog puffs (pv `fogPuffs`), and the inventory

**The report:** Owen saw no tram cable in the Snow Jam opening, and asked what else is missing like it. This section has the cable,
the whole draw-path inventory from the frame render down, and the fixes in order of visibility.

### 41.1 The frame, traced

- The frame render is 0x22B008 (vtable 0x47D14C). The world draw is 0x22ADD8.
- 0x22A830 walks the octree. Its node lists are:
  - +0x20: instances;
  - +0x24: patches;
  - +0x28: other. From this list only kind 5 (type 7, 0x22A270) and the kind-7 lights (type 8) are drawn. Rails (type 1) and
    kind 6 are never drawn, so a spline is only drawn through a MultiSpline modifier.
- After the octree come:
  - the entities (0x230640 / 0x22C830 -> 0x3550A0 -> entity vt+0x24);
  - the FX pass;
  - the render list flush 0x363490. Its priority bins are: 6, the fog composite 0x36AC00; 7, the particles and fog puffs; 9, the
    glare 0x36C790 and ScreenTint 0x3904A0;
  - the HUD.
- Every renderer primitive (vtable 0x493260) is mapped to its callers (`tools`-side notes in the scratch `callmap.py`; the table
  below is the result).

### 41.2 Inventory

| Path | PS2 | Port | Status |
|---|---|---|---|
| Terrain patches, base + light | 0x22A408 / 0x22A128, 0x38B158 / 0x38B370 -> 0x38CA70 / 0x38CE20 | terrain-mesh, world-material | ported |
| Terrain glint ("Patch Reflection"), patch word 0x600000 (layer type 6) | 0x38D168, UV = normal x E (0x38B370, camera rotation), Cd + Cs x Ad | none | **missing**, faint (41.6) |
| Terrain sparkle | 0x38D968 (VU1 program 4) | terrain-sparkle.js | ported |
| Static models | 0x22A368 / 0x229FC8, mesh 0x37E238 (program 3) | main.js, world-material, static-world | ported |
| Plant sway | environment component 3, 0x38EE78 -> 0x37E238 +0x2F4 (0x36A2C0) | none | known gap, cutscenes only (41.6) |
| **Fog puffs** (kind 5 -> kind 4) | 0x22A270 -> 0x22C708 -> 0x2DC190 / 0x2DBF98 / 0x2DC7B0 | fog-puffs.js | **new**, pv `fogPuffs` (41.4) |
| Light glows (kind-7 lights) | 0x22A4A8 / 0x22A770 -> 0x2E2B00 / 0x2E2868 -> 0x3781A0 | light-glow.js | ported |
| Object / LiveComp entities | 0x356298 -> 0x37E238 | livecomp-animation, set-pieces-renderer | ported |
| MultiSpline cars | 0x35B418 (cars 1..) | moving-instances.js | ported on ARA1, BRA2, BHP1, ABA1; **Peak 2/3 seeds** (41.5) |
| **Cables** | 0x35B418 -> 0x345430 -> renderer +0x25C 0x381F10 -> VU1 program 3 at 0x3BF0 | set-piece-cables.js | **fixed**, pv `cables` on (41.3) |
| Flags | 0x34B9B0, strips 0x37A540 | flag-animation.js | ported |
| MeshAnim | 0x352230 | set-pieces-renderer | ported |
| Emitters | 0x3708C0 / 0x371380 / 0x371688 | set-piece-particles, snow-renderer | ported |
| Halos | 0x3462A0 -> 0x2D1D10 | set-piece-halos | ported |
| ParentModifier | attached-setpieces | attached-setpieces.js | events; CTM: the LiveComp children (pv `peakAttached`), spline LiveComps open (41.6) |
| **Avalanches** (kind 22) | 0x2D7EF8 update, 0x2D9130 -> 0x2D8EA8 -> 0x371688 | none | **missing**: docs/avalanche.md |
| Sky dome | 0x353B10 | sky | ported |
| Rider, shadows, trail, wake, spray, sparks, boost strips, aura, streamers, rival beam, breath, impact FX | 0x310640 / 0x310948, 0x2EA538, 0x2DDAB8, 0x2DABC8 / 0x2DB478, 0x2E7A10, 0x2EB198, 0x2EF950, 0x2E3AF8 | rider-material and the FX modules | ported |
| Snowfall, camera splash | 0x2E55D8 -> 0x381310 / 0x3816F0, 0x2F39E0 / 0x2F3E28 | weather-renderer | ported |
| Crowd, flashes | 0x229BA8, 0x229910 | crowd-2d | ported |
| Sun glow / flare, fog composite, glare, ScreenTint, reset fade | 0x2F4A08 / 0x2F4690, 0x36AC00, 0x36C790, 0x3904A0, 0x2E47E8 | fog-renderer and passes | ported |
| NIS, HUD, LUI | 0x2789C0, 0x1F31E0 / 0x397DF8 | cutscenes, hud, lui | ported |
| Camera shake (builtin 91), scripted lightning (92), rail group off/on (35), tunnel (74) | 0x3050F0, 0x305660, 0x300B20, docs/avalanche.md | – | routed to the physics agent |

**Exporters.** `import_world.py` handles kinds 0/1/2/3/9/10. Kinds 4 and 5 (the puffs) and 22 (avalanches) were the only drawn
data it skipped; `prepare-environment.py` reads only the type-5 texture slot of a patch, so the type-6 glint texture (+0x1A4) is not
exported either.

**Difference masks.** Absent-structure masks (a PS2 edge with no page edge within 3 px) over the 60-frame Chrome sweep and 22 round-3
frames found only HUD, AI-rider, rider-divergence and results-screen items besides the cable: no other missing world geometry. The fog
puffs do not show as edges (soft sprites); they were found in the code.

### 41.3 Cables (pv `cables`, on)

- **The PS2:** the MultiSplineModifier draw (vtable 0x48F168 +0x5C = 0x35B418) ends with `if (mod+0x1C) 0x345430(path +0x48,
  colour +0x20)`. 0x345430 walks the path's kind-8 segments; for each segment whose box passes 0x345638 (renderer +0x2EC 0x37DE88) it
  calls renderer +0x25C = 0x381F10.
- **0x381F10:** A+D RGBAQ = the colour (a, r, g, b) x 128, a GIF tag PRIM 0x2 (LINESTRIP: flat, untextured, no fog, no blend),
  NLOOP 10, XYZF2, the segment's 4 cubic rows, then MSCAL 0x77E: VU1 program 3 at 0x3BF0. It samples t = k/9, clips through the
  guard-band matrix (renderer +0x57C0, built in 0x376C58) and sets ADC on a vertex when it or the one before it is outside.
- **Which locations:** builtin 20 key 10 (default 1 at 0x4FB7D0) switches the cable on; keys 6..9 are the colour (default
  (1, 0, 0, 0) = black). ARA1 tramlores_0/_1; BRA2 movingbina, cargobin_end, cargobin_end_b; ABA1, CBA2, CHP2, ESS3, EHP3
  tramlores_1000; EBA3 tramwindya / tramwindyb. BHP1's traffic and ABA1's box vehicles pass key 10 = 0.
- **When:** every frame while the MultiSpline entity lives and its chunk is resident (0x3550A0), whatever the cars' own culling.
- **Port:** `web/set-piece-cables.js` (hooked in set-pieces-renderer.js and peak-set-pieces.js): screen-space quads one PS2 pixel
  thick (448 lines; never under one device pixel), the VU's t sequence and its drop rule, depth tested, colour through the frame space.
  The life follows the core's MultiSpline state, else the section log and the chunk residency.
- **Checks:** `tools/test_cable_vu_native.py` + `tests/cable_vu_reference.cpp` run the real VU1 program (400 cases, 3807 vertices,
  worst 1/16 px, ADC exact); `web/test-set-piece-cables.mjs` (paths, boxes, the VU oracle, the life rules). PS2 frames: the Snow Jam
  race opening 278 / 319 / 358 (the line on the PS2's dark line to 0-1 px), EBA3 820, BRA2 mill 12477 / 12559; Chrome, WebKit and
  WebGL2. No frame-time change in WebKit (desktop and phone tier).
- The PS2 frames show every one-row feature spread over two rows at half intensity: that is the video output (the "PS2 softness"
  option), so the line stays crisp.

### 41.4 Fog puffs (pv `fogPuffs`, on)

**Data.** SSB kind 5 (144 bytes): +0x10 matrix (qword 3 = translation), +0x50 sphere, +0x64 kind-4 reference (track | rid << 8),
+0x68 / +0x74 box. Kind 4: +0x04 entry count, +0x08 entries; entry +0x04 puff set; set +0x1C count, +0x20 puffs; puff (28 bytes) =
local x, y, z (cm), r, g, b (0..1), size (cm). Loaded by 0x3AB200 -> 0x3AA700 (+0x08 = 7) into the octree's "other" list. 141 instances
(3513 puffs over the packages): ABC1 7, ARA1 7 (103 puffs, blue (0.319, 0.462, 0.656) and white (1, 0.964, 0.931)), ASS1 11, CBA2 4,
CHP2 5, DBC2 24, DRA4 19, DSS2 33, E 7, EBA3 12, EBC3 4, ERA5 7, ESS3 1. Sizes 188..4669 cm.

**Cull 0x22A270** (VU0 0xDB8, pipelined: each call reads the previous instance's result):
- the box's 8 corners through the frustum clip matrix (VU0 mem 64..67 = renderer +22688) with CLIP: all outside one plane -> 1
  (dropped);
- the +0x50 sphere against the node's occluder volumes (VU0 0xC18, 5 planes a volume, vi14 = the node's mask): inside one -> 2
  (dropped);
- otherwise 0 (inside) or 3 (inside the guard band, VU0 68..71) -> list 1, 4 (crosses the guard band) -> list 2.

**Sprites 0x2DC190** (0x22C708: list 1 mode 0, list 2 mode 2, same manager):
- instance: dc = trunc(w) of the box centre (w = the view depth, cm). 25000 <= dc: dropped. Fade f10 = 1 below 18000, else
  (25000 - dc) x 1/7000 (gp-0x3C58 / -0x3C54 / -0x3C50);
- per puff, through C = M x instance: d = trunc(w). d <= 500: dropped. f0 = (d - 500) x 0.00050008 below 2499.667
  (gp-0x3C4C / -0x3C48), else 1. f7 = f10 x f0; 0: dropped;
- sprite: centre (x/w, y/w) in GS units, half extent size x P / w with P = renderer +0x6AF0 [0][0] / [1][1] (272.65 / -318.09 px x 16:
  a view-aligned square of half side `size`); corner a = centre - half (ST 0,0, bottom-left), b = centre + half (ST 1,1); both Z =
  trunc(z / w) of the centre; RGBAQ = trunc(r, g, b, f7 x 128), Q = 1/w;
- mode 2 drops a sprite whose two corners are outside the same side of the screen box (renderer +0x6B70 / +0x6B80);
- the (index, d) pairs go to 0x537AC0. There is no capacity check.
- 0x2DC190 reads entry 0 of the kind-4 record for every entry (t8 is never advanced). Every retail kind-4 has one entry.

**Draw 0x2DBF98:** qsort 0x418EF8 with 0x2DC168 (far to near by d). The material state (renderer +0xE84) is pushed and set as
follows:
- priority 7;
- texture +0xF60 (FX entry 4 = fog0, PARTICLES.SSH), CLAMP;
- blend enum 5 = GS ALPHA 0x44, (Cs - Cd) x As >> 7 + Cd;
- ZMSK (no Z write);
- ZTST field 0 = GREATER (0x3626D8);
- alpha test field 3 = ATST GREATER, AREF 0, AFAIL FB_ONLY.

0x2DC7B0 sends GIF REGLIST sprites (PRIM 0x56: SPRITE, TME, ABE; ST RGBAQ XYZ2 ST XYZ2 each). TFX MODULATE comes from the texture.
The count is cleared after the draw.

**Checked against PS2 RAM** (`tools/export_fog_puffs.py --check-state LOC:state,...`, fixture
`local/reference/fog-puffs/frames.json`, `web/test-fog-puffs.mjs`):
- every exported instance of the 14 states is in RAM with the same matrix, box, sphere and puffs (through the resolved +0x64);
- the last drawn frame's sprites (the manager's +16 + 48 i, the sorted list 0x537AC0) against the model with the state's camera
  (renderer +22400, whose z row is the frustum matrix's w row): 14 states over ARA1 (3618, 4019, 4818), ASS1, ABC1, CBA2, CHP2, DBC2,
  DRA4, DSS2, EBA3, EBC3, ERA5, ESS3, 534 sprites, the same count everywhere, 521 identical in draw order (depth, alpha, colour) and
  13 with the integer depth one off (EE single-precision rounding); EBC3 8001 and ESS3 1618 draw none (every box outside the
  frustum), as the model says;
- two PS2 details the check reads around: the count is cleared after the draw, so a frame that draws nothing leaves the older list
  (recognised by its nearest entry matching no puff at the state's camera); and 0x2DC190 writes a sprite's (index, depth) pair and
  corners before the mode-2 test, so a dropped last sprite stays behind at position n (corners off one side of the screen box,
  colour words stale);
- the GS transform fitted from the matched sprites: x = 4362.44 (x/w) + 32768, y = -5089.51 (y/w) + 32768 (1/16 px, residual 0.3),
  the frustum matrix's own (4096 xA / wA, 3584 yA / wA).

**PS2 frames** (`vpshot-base.mjs` through `proxy2.mjs` serving the scratch files, camera pinned, lag 3):
- ARA1 4818 (the forest): full-frame MAD 39.6 -> 27.8 in Chrome and 39.9 -> 27.9 in WebKit; the haze box top left 51.6 -> 19.9.
  Chrome and WebKit agree to MAD 0.7. The rest of that frame is a global tone difference (the PS2 frame is about 20 levels lighter
  everywhere, also where there are no puffs), not the puffs.
- Peak 2/3 (Chrome, same method): CHP2 2820 (the super pipe's haze over the mountains) MAD 20.5 -> 10.1, DBC2 4800 20.5 -> 12.7,
  EBA3 420 9.2 -> 6.8, DRA4 6018 34.4 -> 33.8, DSS2 6019 57.7 -> 57.6 (that frame's gap is elsewhere).
- ARA1 3618 / 4019 / 4419: no visible puff on either side (the PS2 draws 29 / 94 sprites there, faint or behind terrain); the page
  is unchanged.
- CTM (PEAK1): the ARA1 package's puffs attach with its draw package.

**Cost:** building the sprites takes 0.004 ms a frame in WebKit (0.037 ms on the phone tier); WebKit frame times at 4818 are the
same with and without (median 17 / p90 18 ms, both tiers). Memory: the JSON (4-46 KB a package) and one instanced batch.

**Port.** `tools/export_fog_puffs.py --out <scratch>` writes `<package>/fog-puffs.json` for the 12 event packages and the 13 streamed
packages that have puffs. `web/fog-puffs.js`:
- builds the sprites per drawn frame with the drawing camera (`populated(camera)` of the encoded composite and the mesh's
  `onBeforeRender`): box against the camera frustum (the camera's near / far are the PS2's), the two fades in float32, the stable sort
  far to near;
- draws them as one instanced batch of view-aligned quads in the encoded composite (renderOrder 684, before the set-piece particles),
  fog0 through TFX MODULATE and the 0x44 blend, colour through `toFrame`;
- the cull is 0xDB8's own: the 8 box corners through the camera's projection (its z row mapping near / far to -w / +w, as the
  PS2's frustum matrix) and view, VU CLIP against |w|, dropped when all 8 share a flag;
- not modelled: the occluder volumes (result 2; the occluding geometry hides those puffs), the qsort order of equal integer depths
  (JS sort is stable), and the order between two packages' batches in the CTM world (each package sorts its own puffs; no two
  packages with puffs are seen together in the captures).

**Assets** (copied and switched on by the coordinator, 2026-09-28): `fog-puffs.json` for ABC1, ARA1, ASS1, CBA2, CHP2, DBC2, DRA4, DSS2, EBA3, EBC3, ERA5, ESS3 and PEAK1/{ABC1, ARA1, ASS1},
PEAK2/{CBA2, CHP2, DBC2, DRA4, DSS2}, PEAK3/{E, EBA3, EBC3, ERA5, ESS3} (560 KB in all). Without them nothing is drawn.

### 41.5 Peak 2/3 MultiSpline cars

- **Seeds:** the Peak 2/3 trams had no MultiSpline seeds (only the authored cabin, standing still). Construct replays
  (`tools/test_multi_spline_location.py`, new GROUPS rows) are bit-exact against the PS2 for CBA2 tramlores (built after 661),
  CHP2 tramlores (1341), EBA3 tramwindya (241) and tramwindyb (2009), EHP3 tramlores (521). The seed headers
  (`tools/export_set_pieces.py --location`) change only in their two MultiSpline lines. ESS3's tram is never live in a capture.
- **Checked** with `compare-ps2-capture.mjs` and `set-piece-capture-hooks.mjs` (every kept state's MultiSpline: distance and all
  car matrices; the rider exact through each run): CBA2 6/6, EBA3 tramwindya 7/7 exact with the current core.
- **The section leave:** a MultiSpline entity's leave (0x30A460 -> vt+0xA0 0x356BF0 "first modifier is a MultiSpline" -> vt+0xB0
  0x356CC8 -> 0x35AAE0) only does modifier +0x34 -= 1, and the enter (0x30A3A0 -> 0x356C48 -> 0x35AAD0) += 1. +0x34 counts the
  entity's listed instances (authored and clones) and is never read (its getter 0x361C08 has no reference). So the modifier, its
  cars and its cable run on. The core's `Action::MultiSplineRelease` deactivated the lift: CHP2 (released at 1360), EBA3
  tramwindyb (2028) and EHP3 (1520) then miss every later PS2 state. With the leave a no-op (scratch core): CBA2 6/6, CHP2 6/6,
  EBA3 10/10, EHP3 9/9, all bit-exact. The Metro-City bin 707856 does disappear between 11219 and 11618 (instance flags 0x210345 ->
  0x210023, entity 0: the 0x34FD90 destroy), but not through this leave; that cause is still to be traced (routed to the physics
  agent with the core change).
- **JS side:** `CableLife` and the CTM cable life no longer drop a cable on action 5; only 3 (the entity destroyed) does.

### 41.6 Other findings

- **Terrain glint (layer type 6, pv `terrainGlint`, on):** the patch "reflection" pass 38D168 over the
  patches with word +0x0C & 0x600000 (3,895 on the disc: EBC3 1,712, ERA5 368, ESS3 369, ABC1 235, DRA4 211, ARA1 87, ...).
  - **The PS2:** 38CA70 / 38CE20 call 38D168 after the patch's base record and before its rider-shadow record (38D448). It pushes
    the material state: context-2 texture = patch +0x1A4 (renderer +0x10A8 when negative; no retail patch), CLAMP, ALPHA_2 enum 17 =
    0x58 (Cd + Cs x Ad >> 7) when +0x0C & 0x400000 (every retail glint patch; else enum 2). The VIF packet unpacks E (terrain +0x360)
    to VU1 and re-kicks the base strips with the context-2 template (vertex RGBA 0x80: Cs = T); Ad is the base texel's alpha (the
    base pass writes it; the frame setup 382AF0 masks alpha on FRAME_2).
  - **E** (38B370): camera position = -R^T t of the view; alpha = trunc-reduced -0.0005 camY, beta = -0.0005 camX (gp+0x1350 =
    -0.0005, 1/2pi, 2pi; sin / cos 0x31BE50); E = B(beta) A(alpha) M with A rows (1,0,0,0), (0,sa,ca,0), (0,-ca,sa,0), (0,0,0,1),
    B rows (sb,0,-cb,0), (0,1,0,0), (cb,0,sb,0), (0,0,0,1), M at 0x5049C0. The formula equals RAM in 5 states (ARA1 418 / 4818,
    EBC3 2400, ERA5 1619, ESS3 1618; worst 5e-5, the EE's sin polynomial). UV = (n, 1) x E per vertex.
  - **The normal** (374518): ds x dt over the tier basis tables (s = the coefficient block's outer slot): the opposite of the
    package's patch_mesh normals, which point down (95 % of ARA1's). So n = -(package normal).
  - **Images:** every retail glint texture is one of two 32 x 32 images: 62 / 198 (grey, identical texels) and 297 (blue).
  - **Port:** `tools/export_terrain_glint.py` -> `<package>/terrain-glint.json` (the glint patches by resource, 1 grey / 2 blue, and
    the two images; 38 packages). `web/world-material.js glintBytes`: a `ps2Glint` vertex attribute (the coarse buffer; a constant on
    a refined patch), the UV in the vertex stage from the camera (model space: the world meshes sit at -origin), then
    min(255, lit + floor(T x Ad / 128)) before the rider shadow, in the shared and per-texture terrain graphs.
  - **Checked:** the UV against the PS2's E (`web/test-terrain-glint.mjs`, worst 6e-5). On PS2 frames (EBC3 1200 / 2400 / 4001,
    pinned camera) the high-pass correlation of (PS2 - page without) with the glint is 0.18 / 0.10 / 0.13, above every variant tried
    (normal flipped: -0.02 / 0.03 / 0.08; alpha and beta swapped: 0.13 / -0.03 / 0.09; no base alpha: 0.11 / 0.05 / -0.01). The
    term averages +4.4 levels where it is drawn (max about 70). Chrome and WebKit agree (glint term MAD 0.1).
  - **Cost:** the attribute is 4 bytes a vertex of the package's buffer when on (1.3-1.9 MB on ESS3 / ERA5 / EBC3 / ARA1); WebKit
    frame times unchanged (median 17 / p90 18 ms on both tiers).
- **Plant sway** (environment component 3, 0x38EE78): 8 matrices from the renderer frame counter (sway 0.035 sin(10T/600 + i/8) on
  row 2 y, branch 0.1 sin(sin(12T/600 + i/8) + 5(i/8)/600) on row 1 z, twist 0.12 sin(2.8T/600 + i/8) about z; checked against RAM
  at T = 10819), applied by the static-model draw 0x37E238 (+0x92 bit 0, 1,203 instances; matrix trunc(50 |xy|) mod 8) only while
  renderer +0xA4 is set: the in-engine cutscene start 0x278F68 sets it, 0x27BF90 / 0x278590 clear it; every race state has it 0.
  So no race effect; it matters for in-engine cutscenes only. **Known gap, skipped (coordinator, 2026-09-28):** the port bakes static
  instances into world-space texture batches, so the sway needs the 1,203 flagged instances split into per-instance meshes (local
  vertices, the instance matrix) plus a cutscene-gated sway uniform; not worth the draw calls for the intro flyovers now.
- **Avalanches**: docs/avalanche.md (core playback with the physics agent, then the JS draw).
- **ParentModifier children in the CTM world** (CTM agent, 2026-09-28: the JS part and the exports done; the spline LiveComps open):
  - **Export** (`tools/export_peak_world.py`, `--batches-only --out DIR` writes to scratch):
    - The peak packages now split like web/prepare.py: ParentModifier children on node 0 (and drawn although the free-ride audit
      hid their static draw), drawn spline LiveComps per node.
    - Moving batches: `batch.moving_resource` for the crashbag rollers, the chairlift / MultiSpline cars, the drawn SplineModifier
      pieces and looping splines, and every avalanche group an AvaSpline makes follow its tumbler. Before, the peak packages had none,
      so these were baked into static batches.
    - Merged `<peak>/SETPIECES/attached.json`; MOUNTAIN's is merged by `tools/export_mountain_world.py attached()`.
    - Checked (scratch `ctm/peakexp/verify.py`): 15 location packages change, only in these tags. Every triangle keeps its texture,
      lightmap, blend, chunk, UV group and instance flags; vertex-alpha.bin and instance-flags.json are unchanged.
    - Two draw effects before the movers exist:
      - BHP1's four traffic glows (buswhiteglow, carredglow, truckwhiteglow, caryellowglow) become moving, and lose the stage-script
        tag, as in the event package.
      - The BHP1 / CHP2 blimp ads and lights are now drawn, at rest on the resting blimp.
  - **Draw:** `web/peak-set-pieces.js` (pv `peakAttached`) runs `AttachedSetPieces` over the location groups' LiveComp meshes after the
    LiveComp players, as set-pieces-renderer.js does: the searchlight glows (BRA2 3, BHP1 8, ERA5 8) follow their bases' node 3.
    `main.js` drops a released streamed location's moving meshes from the moving list.
  - **Open, core (for the physics agent):** the spline LiveComps (ravens, blimps) and their children run in the event core only
    (web/attached_setpieces.inc, per-location seeds `attached_seed_<LOC>.hpp`, driven by browser_attach_set_pieces / the spline
    launches). The streamed core needs them per resident location, built by the section activation like the other entities, then
    `set_piece_attached()` entries.
    - `AttachedSetPieces.deltas()` already reads `set_piece_attached()`, so peak-set-pieces.js moves them as soon as the core emits
      them; the packages already split their meshes.
    - Likewise the moving batches follow `moving_instances()` (web/moving-instances.js) once the streamed core emits the cars /
      avalanche pieces.

### 41.7 Final absent-structure sweep (2026-09-28, after cables, fog puffs, terrain glint)

- **Frames:** 29 new mid-course event frames over all 17 event locations (ticks the first sweep did not use; `vpshot`, camera pinned)
  and 35 CTM-world frames (`vpworld`: PEAK2 fr-dbc2 x8, fr-throne-neutral x5, PEAK3 fr-throne-tuck x7, fr-throne-late x6, PEAK1
  fr-aara1 x4, arrive-bra2, green-start x2), Chrome. Detector: PS2 edges with no page edge within 3 px and page edges with no PS2
  edge (scratch `cable/final/absent2.py`), triaged by eye.
- **Nothing new is missing from the world draw.** Every large component is one of:
  - the rider where the page's pad replay has diverged (riderErr 11-956 m on 19 event frames; the camera is pinned, so the world is
    right but everything the rider drives differs: the region fog (ARA1 8018), triggered stage particles (DSS2 2818: `?particles=0`
    removes them), finish bursts (EBA3 2819), a restarted event (ASS1 7219, DSS2 6818));
  - HUD (the page shots have none) and the CTM collectible bursts (fr-throne-neutral 7102; the teleported page rider collects none);
  - snowfall (visual RNG) and 1-2 px misregistration.
- **Open, CTM (unverified):** PEAK1 fr-aara1 4296 / 5046: the rider is inside ARA1's bounds and the PS2 draws Snow Jam's terrain,
  but the page's resident set there is A plus its connectors (no ARA1). `vpworld` teleports the rider, skipping the location
  crossing that requests ARA1, so this may be the harness; it needs a real ride from A_ARA1 into Snow Jam (CTM agent).

### 41.8 The dark snow chunks in the forest: the render list's texture order (pv `snowBuckets`)

**The report:** ABC1 (Happiness) 2000: the PS2 shows dark blue chunks flying off the rider over the snow cloud; the page drew none.
Not a set-piece effect (the only live ones are the snowwind puffs, texture 25): the rider's own snow emitters. The core has them
(LargeChunkySpray / SmallChunkySpray, colour (13, 19, 43): the lit forest terrain), and the page drew them under the cloud.

**The PS2's order** (not the emitter order):
- The snow component draw `0x2E24D0` submits the ten emitters in index order (allocated, count > 0), each through `0x371688` ->
  renderer +0x2A4 `0x380CE0`: one render record, textured with the emitter's current flipbook frame (+500 halfword).
- The flush `0x363490` merges records of equal material state and textures into buckets (`0x362DE8`, hashes `0x394ED0` /
  `0x395000`), keys each bucket with `0x364240` = ~((31 - priority) << 26 | word1 bits 0..1 << 16 | a word0 bits 6..9 mode << 13 |
  (texture handle & 0x3FF) << 3), radix-sorts the keys ascending and stable (`0x364050`) and draws the sorted list. Within one
  priority and mode: descending texture handle, ties in submission order.
- The snow emitters share their state (0x100 / 0xC00296 / 0xE0 in the last frame's records in PS2 RAM at ABC1 2000); their textures
  are FX-table entries 4..25, whose handles fall as the entry rises (renderer +0xF50: 4 fog0 1524, 5 1523, 6 1522, ... 15 1515, ...
  21 1509, 25 1505; entries 9 and 12 empty). So the order is ascending texture id: the cloud (SnowTrail / CloudySpray / BodySnow, 5),
  the impacts (6), then the chunky sprays (tmb1..tmb8, 14..21) on top.
- **The PS2's own sorted list confirms it:** the last frame's (key, bucket) pairs are still in RAM (render list +0x67CA8). Priority 7
  there: fog puffs (1524), two world-texture records (31, 191), FX 56 (1653), FX 43 (1552), the snowwind burst particles (brth 1505 with
  a second texture 1549, drawn before the snow through the extra texture bits), then the snow buckets 1523, 1522, 1515, 1509. The page
  already draws the fog puffs and the set-piece particles before the snow; only the snow's own order differed.
  The page drew in emitter index order (700 + index): CloudySpray (7) and BodySnow (9) over the chunks.

**Port:** `web/snow-renderer.js` (pv `snowBuckets`): renderOrder 700 + 0.02 x the current texture id, every frame (three keeps equal
renderOrders in creation order: the riders, then the emitter index, as the submission order). ABC1 2000 (camera pinned): the dark
chunks are over the cloud in Chrome and WebKit as on the PS2; dark pixels in the chunk region PS2 7125, on 7392, off 6912; 2400
unchanged. `test-snow-renderer.mjs` checks the order against the 0x364240 key.

**The rest of priority 7:** see 41.9 (one shared key for every post-fog effect, pv `effectOrder`).

### 41.9 One draw order for every post-fog effect: the render-list key (pv `effectOrder`, on)

**The rule** (`web/ps2-draw-order.js`, the same key as the world pass's sorted classes, section 44). The flush merges records of
equal state and textures into buckets (`0x362DE8`), keys each bucket with `0x364240` and radix-sorts the keys ascending and
stable (`0x364050`):

  key = ~((31 - priority) << 26 | t0 << 16 | rank << 13 | ((handle | (second handle & 0xF) << 6) & 0x3FF) << 3)

- priority = word2 bits 5..9; priorities 6..8 draw after the fog composite (the page's encoded pass).
- t0 from word1 bits 0..1: 0 -> 1023, 1 -> 1022, 2 -> word2 bits 10..28 (the depth key), 3 -> 0.
- rank = the table at `0x492010` over word0 bits 6..9: 0 -> 0, 1 -> 4, 2 -> 5, 3 -> 3, 4 -> 1, 5 -> 6, 6 / 7 -> 2, 8 -> 1.
- So at one priority: descending t0, then descending rank, then descending texture bits; equal keys keep the first submission.

**The effects' fields**, from the code and checked against the last frame's sorted list in PS2 RAM (render list +0x67CA8; 351
kept states scanned, 115 buckets of 14 states in `local/reference/draw-order/buckets.json`):

| effect | code | priority / t0 / rank | texture (FX id) |
|---|---|---|---|
| fog puffs | 0x2DBF98 | 7 / 1023 / 3 | fog0 (4) |
| snowfall flakes, fluff | 0x381310 / 0x3816F0 (word0 0x140) | 7 / 0 / 6 | sfal (8) |
| wake, boost ribbons, aura, '!' icon, rival beam | 0x2DDAB8, 0x2E7A10, 0x2EB198, 0x2D5048, 0x2E3AF8 (strips) | 7 / 0 / 3 | wake 56, yrbn..prbn 57..61, psmr 62, exlm 23, beam 43 |
| set-piece Particle / DynamicParticle, snow emitters, spark kernel, grind chunks | 380CE0 / 380518 (word0 0x100) | 7 / 0 / 1 | own texture; set pieces + 'spec' (46) |
| light glows, spark glints, fist sparkle | 377CF0 / 3781A0 sprites (base material 0x501420) | 7 / 0 / 0 | shal 53 / mhal 54, sprk 22, ospk 24 |
| streamers | 0x2EF950 | 8 / 1023 / 3 | strm 63 / prbn 61 |
| halos, camera splash, lens, sun | 0x2D1D10, 0x2F2C30 / 0x2F3418 | 8 / 1023 / 0 | blha..whha 37..42, ices 66, icel 67 |

- **Texture handles differ by boot.** The FX table at renderer +0xF50 (index = the tag table `0x4891B0`) holds the texture
  manager's handles: one table in every event boot, another in every Conquer the Mountain state (e.g. wake 1653 vs 1549, spx0 1565
  vs 1510). Both are in the helper; `main.js loadCourse` picks the CTM one for `course.freeRide`.
- **The second texture is inherited.** Effects that push the current material keep its second slot. The world pass sets it to FX
  46 'spec' (renderer +36 = 46 in every state; 0x22B374, 0x22C0BC). Set-piece particles carry it in most frames: every ABC1
  snowwind bucket is brth + spec, which puts them before all snow (key texture bits 993 vs 499). The rider FX never do. Not
  everywhere: BHP1 1619 has spx1..3 both with and without it, and DRA4 9218's sprk / ospk and ERA5's spx2 go without.
  Burst 3708C0 pushes the top; trail 371380 copies its emitter's own material. The exact rule is not traced, so the helper gives set
  pieces the 'spec' key, the majority case.
- Equal keys break ties by submission (`SUBMIT`: the world pass, then each rider's FX in 0x1119F8 order: wake, sparks, boost,
  aura, streamers, icon, beam, snow, fist).

**What changes with the switch** (renderOrder = 660 + 60 x (key - 0x98000000) / 0x0C000000, priorities 6 / 7 / 8 in
[660, 680) / [680, 700) / [700, 720)):
- The fog puffs draw first, before the wake and boost (the page had them after).
- The snowfall draws before the wake, boost and particles.
- The halos (priority 8) draw after the snow and particles (the page had them before).
- The impact chunks and sparks draw after the boost strips; the glints and the fist after all rank-1 sprites.
- The set-piece particles sort by handle among themselves (the page had creation order).
- The snow keeps the 41.8 order.

**Checked:**
- `test-ps2-draw-order.mjs`: the effect specs give the RAM keys for all 115 buckets, in both tables, and sort them in the RAM
  order.
- Page frames with the switch on, camera pinned (`vpshot-base --probe`): the encoded meshes' keys decoded from their renderOrder,
  against the same state's PS2 list. Frames: ABC1 2000, DBC2 weather 1000, BHP1 1619 / 3219 and ARA1 1219, Chrome and WebKit.
  - Every effect both draw sits in the PS2's order.
  - The rest differs in what is live, not in order: snow flipbook frames (the open phase item), set-piece bursts timed differently,
    the rider 8 m off at ARA1.
- Pixels, on vs off: no pixel changes at those frames, at ARA1 1079, or at CRA3 6819 / ERA5 1229 (the rider 58 / 87 m off there)
  in either browser; the effects that moved do not overlap in them. ARA1 1079 in WebKit only: 42 snowfall-sparkle pixels, not
  order (none in Chrome).
- Tests: snow-renderer, fog-puffs, boost, boost-fx, impact-fx, set-piece sprites, startfire, wake, weather, rival-mode and
  frame-space pass.

**Two draws in the wrong layer (switches, off):**
- **Rival beam, pv `beamEncoded`** (`web/rival-beam.js`): the beam is a priority-7 strip (0x2E3AF8), drawn after the fog
  composite, unfogged. The page drew it in the world pass, so the fog composite fogged it. With the switch it is a byte-space node
  material in the encoded pass: GS MODULATE (texel x vertex, vertex rgb x 2, clamped) and ALPHA 0x48. With `effectOrder` it takes
  the beam key (rank 3, after the wake, boost and aura).
  - ABC1 rival frames, camera pinned: the beam is on screen at 2400 only, near the rider where the fog is ~0. There the beam is the
    same on and off to within a level; 64 (Chrome) / 132 (WebKit) edge pixels move by 1..4 levels.
  - No kept frame shows a distant (fogged) beam, which is where the switch matters.
- **Terrain sparkle, pv `sparkleWorld`** (`web/terrain-sparkle.js`): the sparkle is priority 4 (0x38DA40: word2 |= 0x80), a
  world-layer draw before the fog composite, so it is fogged with the terrain. The page drew it in the encoded pass after the fog.
  With the switch it stays in the world pass. Its blend (Cd + Cd x As) scales the destination bytes the same either way.
  - 13 aligned frames (ABC1 400 .. 6800, DBC2 weather 1000), Chrome: 126 sparkle pixels change. 73 end closer to the PS2 and 39
    further (DBC2 1000: 11 / 1 in both browsers).

**Open:**
- The light glows (`light-glow.js`, rank 0 at priority 7) draw in their own pass.
- The set-piece particles' inherited second texture, above.
- The PS2 also has priority-7 depth-sorted static models (e.g. BHP1 / DRA4 world textures with t0 = depth); the page draws them in
  the world pass (section 44).

## 42. The lodge's screen changes: TransitionOut (pv `lodgeFlash`)

Every lodge LUI screen is an FE state:
- 28lodge (cFEStateLodge);
- 155rider_details_conquer;
- 125mountainroom, 126peakroom and 127trophyroom;
- 93profile_load (cFEStateProfileLoad);
- 33buyattribs;
- the Rewards room, Ubertrick Setup and Rider Profile opened from Rider Details;
- Music (140audio);
- the career highlights.

Moving between them is a state change, and the PS2 makes every one the same way.

**Code:**
- **The switch call.** 0x39F400 puts the old state in phase 6 (flags +0x1C, bits 8..13) and the new one in phase 1 (waiting).
- **The phases.** The state manager 0x39ECB0 steps them through a jump table at 0x493FC0 (phases 2..7).
- **TransitionOut.** The old state's LUI plays its 'TransitionOut' label (hash 0e1683a4):
  - the lodge's items play it themselves at 0x1F3C4C, and Buy Attributes' Triangle at 0x1F4BDC;
  - there, control 0x43 is followed by 0x30 ... 0x00AB3C45, which starts the `transition_flash` screen (anim 02ed6393: a white quad,
    A 0 -> 255 over 10 frames, then 255 -> 0 over 9).
- **The switch.** Nine frames on, the end label 0005ab60's control 0x41 (handler 0x39CE98) puts the old state in phase 7.
  - 0x39EEE4 then exits it: vt+0x58 = 0x1865A8 keeps the menu's cursor (1A0708), then vt+0x28.
  - It then activates the next state, whose LUI plays its intro from frame 0.
  - At the intro's label 25 (control 0x42), the state goes active and its menu focus plays.
- **Every screen has the same label.** Each lodge screen's TransitionOut label has the same two controls.
- **Not fully traced.** 125mountainroom's Triangle (0x1D4698) and the lodge's Save Game (value 8, 0x1F3BEC) call 0x39F400 without
  playing the label themselves, and the frames show the same flash. How the label starts there is not traced.

**PS2 frames** (`local/ps2-capture/menus/tout-*`: one run per frame, first snap exact, press at pad sample 20):

| Frames after the press | 5 | 10 | 12 | 16 | 22 | 30 / 36 | 44 |
|---|---|---|---|---|---|---|---|
| white A (fit over the base screen) | 0.29 | 0.79 | 1 | – | – | – | – |
| frame mean, Trophies' Triangle and the lodge's Rider Details | 184.6 / 195.1 | 234.3 / 237.3 | 253.7 | 212.0 | 160.9 | – | – |
| Rider Details | | | | its intro under the fall | intro, no menu | menu texts, no focus | focus bar and help |

The three cases give identical frames: Trophies' Triangle (tout-troA), the lodge's Rider Details (tout-detD) and Save Game
(tout-saveC). The rise starts about 2 frames after the press, the same delay as every other PS2 menu response.

**Port:**
- **The shared object.** `web/lui-flash.js LuiFlash`: `go(to)`, then `draw(c)` after the screen, which runs `to` at full white;
  `fall()` gives the fall alone. It uses Buy Attributes' constants (`FLASH_IN` 10, `FLASH_OUT` 9, `flashAlpha`).
- **Where it runs.**
  - `web/career-ui.js`: `lodgeFlash`, `lodgeGo(to)`, and no input while it runs.
  - The changes that go through it:
    - the lodge's Rider Details and Save Game (Music: the fall on its return; the rise stays audio-menu's own whitefade);
    - Rider Details <-> the lodge, Trophies and Highlights (`lodge-ui.js`);
    - the three trophy rooms (`trophy-room.js go`);
    - the Save game screen's return (`save-game.js leave`);
    - Rewards / Ubertrick Setup / Rider Profile opened from the lodge and their own changes (`fe-screens.js`: `go` and the
      lodge's return use it, the flash is drawn over its screens, input waits).
  - `audio-menu.js` draws the fall on Music.
- **Intros.** The lodge and Rider Details replay their intros on every entry (`fe-screens.js enter`). Their menu focus plays from
  the intro's 0x42 label (frame 25), not at once.

**Results** (Chrome; the page at t = PS2 frame - 2):
- rise and full white: MAD 4.6 / 1.8 / 1.3 at 5 / 10 / 12;
- fall with Rider Details' intro: 1.6 at 16 and 3.6 at 22 (4.4 / 7.4 before the intro fixes). WebKit is within harness precision
  (the page clock creeps 1 % between steps): 3.4 / 3.6.
- Rider Details' menu texts came up 2-3 frames later than on the PS2 (PS2 k30 had them where the page had them at t32); fixed in
  round 2 (pv `introLead`).

**Open:**
- **Menu cursor.** Done in round 2 (pv `stateCursor`).
- **Not routed through the flash:** the page's older non-LUI lodge screens. Equip / Buy Gear followed in round 2; the Player Name
  keyboard and Cheat Characters are child states with no flash on the PS2 (round 2).
- **Buy Attributes** moved onto `lui-flash.js` in round 2.

Tests: `test-visual-parity.mjs` R33 (the flash curve, the switch once at full white, the fall, the wiring) and R30. Tools:
`tools/lodgeflash.mjs` (the page's side, clock stepped frame by frame).

**Round 2 (2026-09-28): cursors, intro timing, the other lodge changes.**
- **Menu cursors (pv `stateCursor`, on).**
  - On exit, every FE LUI state's vt+0x58 (0x1865A8) stores its 'Menu' index in the table at gp+0x1D90 under its state id
    (1A0708).
  - A state whose activation vt+0x30 is 0x186518 reads the index back (1A06F0) and focuses it (39B960). These are the lodge
    (vtable 0x473AA8), Rider Details (0x4739D8), Buy Attributes (0x473908) and the mountain room (0x469858, via 0x1D2A90).
  - The peak and trophy rooms (0x1D3C60 / 0x1D43D0) store the index but do not restore it.
  - Port: `career-ui.js` `cursorMemo`, filled while those screens are drawn (and at each `lodgeGo` switch) and restored after the
    switch and on arrival from 117loadinlodge.
  - PS2 `tout-kbd`: Rider Details reopens on Trophies after a visit there. The page does the same.
- **Intro timing (pv `introLead`, on).**
  - 0x39EEE4 exits the old state and activates the new one in the same pass of 0x39ECB0.
  - The new state's phase-2 case (0x39ED4C) runs enter and the LUI update 0x39E868, then falls into the phase-4 case, whose tail
    updates the LUI again (0x39EED4). So the new screen first shows 2 frames into its intro.
  - A restoring state's focus appears 2 frames after its intro label's 0x42 (frame 25): the next pass's phase-3 case runs vt+0x30,
    and its focus label shows on the following update.
  - Port: `lui-flash.js introStart` (INTRO_LEAD 2, FOCUS_LAG 2), used by `fe-screens.js` (Rider Details, the lodge, the FE screens
    it opens), `trophy-room.js`, `save-game.js`, `audio-menu.js`, `buy-attribs.js` and `wardrobe.js`.
  - PS2 `tout-detD` k27..k44: Rider Details' menu texts now come up with the PS2's (they were 2-3 frames late). The focus bar is
    absent at 36 and present at 38 on both. MAD: Chrome 3.9 / 4.2 / 4.8 / 5.3 at 27 / 30 / 36 / 38; WebKit within 0.1.
- **Equip / Buy Gear through the flash.**
  - The lodge's values 1/2 play TransitionOut (0x1F3C4C) like the other items.
  - Port: `lodgeGo` for entering Equip Gear and the Buy Gear list, for leaving them, and for Square = Buy Gear.
  - `wardrobe.js` draws the flash and ignores input under it.
  - A screen that is still loading holds full white until it is up, then falls (`LuiFlash`: `to` may return a promise). The PS2
    never holds: see "Equip Gear's load" below. Since pv `equipLoading` only the page's own screen data can still hold it.
- **Buy Attributes** now uses `lui-flash.js`: `lodgeGo` from the lodge's career UI, or its own `LuiFlash` without one (tests).
  `FLASH_IN` / `FLASH_OUT` / `flashAlpha` moved to `lui-flash.js` and are re-exported from `buy-attribs.js`.
- **The Player Name keyboard and Cheat Characters stay without a flash.**
  - Rider Details' input (0x1F4400) pushes them as child states with 0x39F290 (cKeyboardPopup, cFEStateCheatCharSelect). That
    clears the parent's active bit and changes no phase, so there is no TransitionOut.
  - PS2 `tout-kbd`: the keyboard is up 5 frames after Cross and Rider Details is back 5 frames after Triangle, with no white.
    Cheat Characters (not owned on the capture's save) uses the same call.
- Colour space: these frames were measured with pv `encodedBlend` on (live since this round).

Tests: R34. Tools: `tools/lodgecursor.mjs` (the lodge's states walked by key presses: cursors, gear, Buy Attributes).

**Equip Gear's load (pv `equipLoading`, on; 2026-09-28).**
- **PS2** (`local/ps2-capture/menus/eqg-k*`: the lodge on Equip Gear, Cross at sample 150, one snap per run; `eqg2-k*`: the second
  entry after Triangle, the same frames):
  - there is no white hold: full white at +12 and Equip Gear under the fall from +14;
  - until +36: "Loading..." over the list with no rider, no board, no help line, no dashes and no row highlight (every row dark on
    the bar's base position);
  - +37: the rider, the board, the help line, the 'equip btm left' dashes and Head's highlight all appear, with "Loading..." still
    behind the rider;
  - +38: "Loading..." is gone.
- **Code:**
  - +37 is phase 3 of CharEquip (vtable 0x46A548), not the end of a load: intro start +10 (the switch at +12, INTRO_LEAD 2), plus
    the 0x42 label (25), plus FOCUS_LAG 2.
  - Phase 3 runs vt+0x30 = 0x1993A0: 19A238 (Equip mode: the title, 'equip btm left' 0b777dd4 shown), the bars (19BC90 / 19BD48),
    the list (19B618), 19E538(slot, 1), then the cursor (186518).
  - 19E538 sets the preview's drawn flag +0xCC8 only once the model is loaded (+0xCB4 / +0xCB8) and the reload stamp +0xCD4 is
    clear; otherwise it leaves +0xCC4 pending.
  - The update 0x199938 shows "loading text" while +0xCC8 is clear, before that pass's phase 3, hence the one-frame lag.
  - The list's help line (19A9B8 -> 19A798) waits for the rider's gear data (19E238 -> +0xA60).
  - A reload after an equip (19E588) clears +0xCC8 again, so "Loading..." shows again.
- **Port** (`web/wardrobe.js EquipGearScreen`):
  - `open` switches as soon as the screen's data is in and builds the outfit package (`prepareOutfit`) in the background (`preparing`);
  - `settled()` is phase 3; `showPreview` draws the rider only from then and once loaded;
  - `loadingText()` hides "Loading..." from the frame after the rider first draws;
  - the row highlight label and 'equip btm left' wait for phase 3, and the help line for phase 3 and the outfit package;
  - an equip made while the entry's package is still building runs after it;
  - `preloadEquipGear` (called by `lodge-ui.js preloadGear` while the lodge menu is up) loads the screen's data and the rider's gear
    lists, which are resident on the PS2.
  - Setup Character's Equip Gear is the same state and gets the same behaviour.
- **Before:** full white was held until the outfit package was built.
  - Localhost: 1-3 frames, then "Loading..." to about +24.
  - Chrome at 200 KB/s with the cache off: a 42-frame hold.
  - The page's focus, dashes and help showed from the start, and the rider as soon as its model was ready.
- **After:**
  - The same throttled link: no hold, then "Loading..." over the list until the model is in.
  - Stepped clock (`tools/eqgflash.mjs`), in Chrome and WebKit alike (page t = PS2 k - 2, the flash's lead after Cross): t14 / t34 match
    k16 / k36, and the rider, dashes, highlight and help come up with "Loading..." behind the rider one frame later.
  - That one frame is the harness's clock creep before the key press: the stepped clock lands a hair before the phase-3 boundary.
- **Open:**
  - Input between the flash and phase 3 is not gated (the PS2 state is not yet active).
  - The PS2 load length on Setup Character (the "~250 frames" in characters.md) has not been re-captured.

Tests: R36 (the phase-3 rule on a fake preview and clock, the wiring). Tools: `tools/eqgload.mjs` (real-clock timeline, `--throttle`),
`tools/eqgflash.mjs` (lodgeflash with the Equip Gear case and per-frame async steps), `tools/eqgflow.mjs` (the lodge and Setup flows).

## 43. The static-model env-map second pass (pv `envMap`), cutscene bytes (pv `cutsceneBytes`), 8-bit frame targets (pv `frame8`)

**The report:** with the encoded frame (section 38a), translucent world models blended to the PS2's maths but still sat 4-45 levels
darker than the PS2 where their own texels were opaque. Metro 79 (aligned to 2 mm): the PS2's glass pixel is brighter than the port's
glass texel, so no blend of that texel could reach it.

**The cause: a second draw of env-mapped materials.**
- 37F2A4..37FD2C switches on the material word (+12, with group flag bit3 -> 0x40000) & 0x660000. The cases 0x200000 / 0x220000 /
  0x260000 and 0x600000 / 0x620000 / 0x660000 set word1 bits 7..11 (ALPHA_2 for context 2, 363C20 -> 362478 with a3 = 1) to enum 2
  (0x100) or enum 17 (0x880). The table at 0x491FB0: enum 2 = 0x68 with FIX 128 (Cs x 128 >> 7 + Cd = Cs + Cd), enum 17 = 0x58
  (Cs x Ad + Cd). Every other material has enum 3 (0x6A, Cd: context 2 writes nothing). The global at gp+0x1404 that would clear
  the bits is 0 in every race savestate.
- The context-2 texture is the material record's halfword +2 (set exactly on these materials: 119 of the 2,575 material records in bam.ssb; world textures
  9-50, 9-62, 9-198, 9-297, all already in `TEXTURES/world.tex`).
- These models have header +0x10 bit 1; 37E238 then builds the UV matrix = (view x node rotation, translation cleared) x the constant
  at 0x504760 ((0.5, 0), (0, -0.5), 0, (0.5, 0.5, 1, 1), a BSS table read from RAM) and flags UV mode 256. VU1 program 3 at 0xCE8
  (`tools/vudis.py`, a VU disassembler from PCSX2's opcode tables): uv = rows 10..13 x (normal, 1) per vertex, so u = 0.5 n.x + 0.5,
  v = -0.5 n.y + 0.5 with n the camera-space normal (x right, y up). The second kick (0x2710 -> 0x3360) reuses the first pass's
  packet (XYZ, vertex RGBA) with ST = uv x Q: TEX0_2 MODULATE by the packet colour, Cs = T x (c5 << 3) >> 7.
- The port never drew it: `web/prepare.py` noted "env-map variants keep their base class".

**Port:**
- `web/world-batches.py` splits batches by `triangle_env` and tags them `env = [second texture, 0x200000 | 0x600000]`;
  `web/prepare.py mesh_env` and `tools/export_peak_world.py` compute it from the material records and add the second textures to the
  package's texture table (`SSX_ENV_SPLIT=0` turns the split off for byte-identity checks). No triangle is added or lost; with the
  switch off the draws are the same, only split. The event packages carry it since the lit-instance re-export (all but ARA1); the peak
  packages get it at their next `export_peak_world.py` re-split. A package without tags draws no pass (checked on PEAK1 free ride).
- `web/world-material.js envPassMaterial` (pv `envMap`): the batch's last material. UV per vertex from the camera-space normal (a
  varying), the second texture (clamped copy) times the packet colour in bytes, then additive (One / One on colour, alpha kept) with no
  depth writes; mode 0x600000 scales by Ad = round(base texel alpha x vertex alpha x 128) >> 7. Drawn in the world pass, so the fog
  composite fogs Cd + Cs as on the PS2.
- `web/test-env-map.mjs`: the ALPHA table and cases from the ELF, the VU routine, the RAM constant and switch, the packages' tags and
  textures, the material.

**Measured (aligned PS2 frames, `vpshot --pin`, 9 runs / 30 frames, `aa=0`, pixels the pass changes; BHP1's pipe run excluded: its
frames are 47-92 MAD off the PS2, a misaligned camera):**

| config | pixels | \|port - PS2\| | PS2 - port |
|---|---|---|---|
| Chrome WebGPU | 108,941 | 31.06 -> 27.56 | +24.91 -> +0.73 |
| WebKit WebGPU | 107,631 | 31.21 -> 27.66 | +25.14 -> +0.93 |
| Chrome WebGL2 | 108,000 | 31.12 -> 27.61 | +25.02 -> +0.97 |

- Metro 79 +28.2 -> -1.1, Metro 619 MAD 36.2 -> 20.5, CRA3 418 (station glass) +23.4 -> +4.3. Per texture (hidden one at a time):
  Metro glass 9-152 +28.6 -> +4.0, CRA3 9-185 +22.1 -> +6.7, 9-254 +39.1 -> -2.5.
- The UV rule from the code also scores best against the PS2 of the four axis sign choices (Metro 79: bias +3.0 / +4.7 / +8.1 / +10.9
  in a per-fragment prototype).
- Left: the stadium crowd behind Metro's glass (9-161, the CrowdMan2d frames) +35.7 -> +13.1; CRA3's crowd +18.3 -> +0.6.
- The ARA1 event-race frames first listed as dark translucent content were a camera offset (a pine 100 px off), not colour.
- Cost (six-rider Metro Single Event, WebKit, alternating loads): 2 more pipelines (106 -> 108), ~1.5k more triangles and the same
  draw calls in the measured view (the pass is a group of the batch's own mesh); event load 7616 -> 7599 ms, race frame mean 16.78 ->
  16.78 ms (desktop 3 + 3), phone tier 16.80 -> 16.78 ms (2 + 2); frames over 20 ms within run-to-run spread.

**pv `cutsceneBytes`** (`web/cutscenes.js modulateBytes`): the cutscene sets, their skies and the PDA prop take the static-model
MODULATE on bytes (Cs = T x (c5 << 3) >> 7; the PDA's PS2 texels x 2) instead of three's linear-light texture x colour x 31/16.
EBC3 heli arrival (#127 t150 / t250 / t350 against PS2 samples 176 / 275 / 376, luma MAD): Chrome 6.0 -> 5.9, 8.5 -> 8.8,
7.1 -> 6.0; WebKit 3.8 -> 4.2, 8.9 -> 6.6, 6.0 -> 5.8.

**pv `frame8`** (`web/frame-space.js frameBufferType`, the renderer's `outputBufferType`): with the encoded frame the world and sky
pass targets are 8-bit unorm, each draw rounded to bytes as the GS's 32-bit frame buffer does. Against half-float at the same
states (9 frames: Metro, CRA3, ERA5, EBC3, ARA1): at most 2 levels on all but 24 pixels (Metro 79's stacked glass, max 11); the
differences sit on blended texels, no contour in any sky or fog gradient. Texture memory (three's accounting, six-rider ARA1, WebKit):
67.9 -> 59.8 MB desktop, 50.0 -> 47.8 MB phone tier; frame times the same.

**Depth bias on the pass (Safari flicker, 2026-09-28):** the pass draws its base pass's triangles again through another pipeline
with LessEqual and no depth writes. WebKit's depth differed between the two pipelines on grazing panels, so the pass vanished for
single frames: Metro's stadium glass flickered at the top of the screen, where the crowd should show. Fix: `envPassMaterial` sets
polygonOffset -1 / -1 (WebGPU depthBias / depthBiasSlopeScale).
- WebKit, 30 consecutive Metro ticks with a moving camera: the tick-170 spike fell from 2743 reversal px to 117. The sequence max is
  403, vs 419 with the pass off.
- Colours are unchanged (Metro 79 frame MAD 13.38 -> 13.37).
- The PS2 has no such difference: the GS draws the second kick with the first kick's own XYZ.

**Helper bypasses closed:** the `?originalWorld=0` fallback materials and the old rival icon (pv `rivalIcon` off) keep their
linear-light combine, written through `frame-space.js linearOutput()` with sRGB-decoded texels (`linearTextureSpace`).

Evidence and scripts: `local/browser-validation/blend-space/envmap/` (per-frame JSON for the three configs, the prototype, cost runs,
cutscene frames, 8-bit diff map). Test: `test-env-map.mjs`.

## 44. Sorted and unsorted translucent static models (pv `sortedClass`, on)

**The report:** after the env pass (43), Metro's stadium crowd (9-161) behind the glass was still +13 off the PS2.

**Traced, the crowd's own draw is right.** CrowdMan2d mode 0 swaps the crowd groups' material for the shared record 0x536690:
- texture handle 1539, the CRWD frames;
- word 0x20000, class 1, no env, repeat.

The frames' 16-entry CLUT sits unchanged in PS2 VRAM, and the crowd instances are not lit.

To check the crowd directly, two captures from `metro-city-countdown-anchor` ran side by side (`runs/crowdtest/`):
- one with all 16 crowd palettes poked to magenta in EE RAM (0xB89990.., 16 x 16 entries, alpha kept);
- one with the original palettes.

The port was driven the same way at the same state (tick 97, camera error 2 mm).

The crowd's colour response matches: the magenta minus original difference is R/B 0.42 on the PS2 and 0.47 in the port. It is
MODULATE with the bluish vertex colours; DECAL would give about 1. Magnitude ratio PS2 / port 0.97 (R), 0.92 (B).

**What differed was where the crowd showed at all.**
- The PS2 draws the crowd band across the whole frame.
- The port missed it on the left. Batch 901215 (crowd, class 1) lies behind glass 892185 (9-152, class 2), and hiding it changed 1
  pixel.
- three sorted both classes together by distance. The large glass pane drew first and wrote depth (texels alpha > 20), so the crowd
  behind failed the depth test.

**The rule:** the render queue key 364240 is the priority, then a sort value from word1 bits 0..1, then word0 bits 6..9 and the
texture:
- mode 0 (opaque) 1023;
- mode 1 (class 1: 37F5EC ori 1) 1022;
- mode 2 (classes 2 / 3: ori 2, with the depth key in word2 bits 10..28) the depth key;
- mode 3: 0.

The key is inverted. So at one priority the queue runs opaque, then every class-1 model, then the depth-sorted translucents back to
front.

**Port:** `web/main.js asset` gives class 2 / 3 static-model batches renderOrder 0.5 (`SORTED_CLASS_ORDER`), so they sort after class
1 and the world's other draws (0), and before the effects (>= 1).

**Measured:**

| | before | after |
|---|---|---|
| Crowd pixels, PS2 and port both (magenta difference) | 9,983 | 12,927 |
| Crowd pixels, PS2 only | 7,028 | 4,084 |
| Crowd pixels, port only | 6,920 | 2,238 |
| Metro 79 glass 9-152, PS2 - port | +4.0 | +0.3 |
| Metro 79 crowd 9-161, PS2 - port | +13.1 | -14.3 |
| Metro 79 crowd 9-161, \|PS2 - port\| | 32.7 | 30.8 |

- Metro 79, all pixels the switch changes: 38.27 -> 34.96 (Chrome), 38.47 -> 35.01 (WebKit). Frame MAD 13.37 -> 13.13.
- CRA3 418: 28.2 -> 26.9.
- All 26 aligned frames (BHP1 pipe excluded): 28,306 px, |port - PS2| 36.63 -> 34.06.
- ARA1 set-pieces frames move a few hundred pixels either way (their rider and crowd timing are not aligned).

**Left:** the crowd is now visible where the PS2 shows it, but 14 levels brighter on its pixels. The colour response matches, so the
cause is elsewhere: the frame phase of the animator, or the glass layers over it. Open.

Tools: `local/browser-validation/blend-space/crowd/`.

## 45. The race HUD at the finish (pv `finishHudHide`)

Playtest report: "The game HUD doesn't disappear when the FINISH text appears."

**Original (dis.pkl):**
- The HUD owner update 1EA930, per player state s0 (stride 0x8C from owner+0x48), each tick:
  - 0x1EB91C: when 12A250 is true (every human's rider+0x470 >= 0), owner+0x3CC &= 0x170000. That cuts the owner-level
    elements: the clock 0x4 (single-player draw at 0x1F0EB0), the mail icon 0x80000 (0x1F0F3C) and the progress meter 0x40
    (gp-0x994 = owner+0x3CC bit 6 at 0x1EC1A0, read by 0x20EDA0).
  - 0x1EB9C0: unless the event type 0x535C10 is 4 (free ride), once rider+0x470 >= 0 (the finish routine 125108 has run;
    FINISH and TIME'S UP alike), state+0x88 = rider+0x480 ? 2 : 1 and **state+0x80 = 0xFFEFFFFF** (0x1EB9FC), then it skips the
    rest of the player's update (to 0x1EC14C; +0x84 stays 0).
  - 0x1EC164: +0x80 / +0x84 / +0x88 are cleared for the next player, so the mask is rebuilt every tick.
- The draw 1EC3F8, per player (0x1ECB04): flags = (owner+0x3CC & ~state+0x80) | state+0x84. At the finish this leaves only
  0x100000, the banner bit: 0x1ECB28 draws 21F660 ('fini' or 'timeup'). Every other element is gated by a cleared bit: the place
  0x1, the boost gauge (slot case 6 at 0x1ECD60 needs 0x200000), the trick slots (0x4000000 / 0x402), the switch 'S' 0x10000000,
  the hints 0x1000000, the collect counter (+0x84 0x80).
- So the whole HUD goes on the tick the banner comes, and stays gone until the results (+0x470 keeps counting). It is a cut,
  not a fade: nothing in this path ramps an alpha.

**PS2 evidence:**
- `setpieces/full` (Snow Jam race from the countdown anchor) re-run from its built state with per-tick snaps
  (`runs/finishhud/fin-hud.json` and its `tick*.png`: 12,306 records, every one byte-equal to `runs/setpieces/full.bin`, so the
  .bin is not kept). rider+0x470 is -1 through
  record tick 12296 and 0.0167 at 12297 (the finish). Snap 12297: the full HUD (2ND/6, 00:03:21, 860, the meter at 100 %, the
  gauge, 47 MPH, 'S') and no banner. Snap 12300 and on: FINISH! 00:03:21 alone. No frame shows the banner with any HUD element,
  or any element part-faded.
- `ctm-parity/runs/race-f` (10-frame samples): sample14341 (total tick 14308) the full HUD; sample14351 (14319) the banner alone.
  The clock phase turns 5 (finished) at total tick 14311. `race-q` sample13720 / 13741: the same.
- Peak runs (event types 5 / 6) take the same path from the code. No PS2 frame of a peak-run banner was found.

**Before:** in races the page drew the banner over the clock, the score, the progress meter, the boost gauge and orb, the speed,
the 'S' and the trick slots. Only the place was gone (main.js passes no racePlace once finished). Freestyle was already right:
career-ui.js draws only the banner, and ui.js returns early.

**Fix (pv `finishHudHide`):**
- `web/ui.js`: `finishHide` = not freestyle, `s.message` set (the port's rider+0x470 >= 0: FINISH and TIME'S UP), and the switch.
  It gates the collect counter, the place, the clock, the score fallback, the speed, the progress meter, the boost gauge, orb and
  letters, the 'S', and the trick slot frame. The slot frame gets the per-player flags & 0x100000, as 0x1ECB04 computes them.
- `web/career-messages.js drawHud(c, racing, draw)`: with draw = false the mail icon's timer (1EB6E4) runs and nothing is
  drawn, so pv mailFreeze's timing holds.
- `web/main.js ui.freeRideHud(c, level, finishHide)`: the peak run's clock and split are cut the same way.

**Check (Chrome, the real Single Event flow, `tools/modeshot.mjs setpieces/full`, position error 0 at every shot):** with the
switch on, the page shows the full HUD at record 12296 and the banner alone from record 12297, the PS2's finish record. With the
switch off, the same frames show the banner over the clock, score, meter, gauge, speed and 'S' (`local/browser-validation/visual-parity/fixes/finish-hud/finish-hud-cmp-chrome.png`:
PS2 12297 / 12300, off, on). In the staged finish / TIME'S UP (`tools/fin.mjs`, Chrome) only the banner is left on the UI canvas.

**Switch state: off.** The WebKit check is still to do. While this was done the machine's screen was locked, so the WebKit driver's
page stayed `visibilityState` hidden (no rAF) and never got past the course load screen (fin.mjs and modeshot `--browser wk`).
To check: `node modeshot.mjs setpieces/full OUT 12297,12298,12300 --browser wk --extra '&pv=finishHudHide'`. Record 12296
should show the full HUD and 12297 the banner alone. Then flip `finishHudHide` to true in PV_DEFAULTS.

**Left (not this switch):** the PS2 also draws "Loading..." bottom right from about 6 ticks after the finish (S+0x94 bit 4,
ctm-decomp-world-states.md rank 7). The page has no caption there yet.

Test: `test-visual-parity.mjs` R37.

## 46. The gondola cabin seen from outside: the cutscene sets' static-model classes (pv `setBlendClass`, off)

**Report** (playtester): "The gondola texture is transparent from the outside." It is the TRANSP gondola cabin
(`mdl_TRANSP_gondola_full_version_inair`, CUTSCENES/SETS/TRANSP) in `gond_inair` #150, the exterior shot of every gondola
ride (the Transport and the between-heats ride-up). From outside, the inside of the far wall (benches, window frames, the
rider's legs) was drawn over the near wall's lower panels. The world's gondolas (ARA1 `tramlores`, the hubs' `depart_gond`)
draw through `world-material.js` and are not affected (ARA1 cars checked from 4 sides and inside).

**Cause.** The cabin's walls *and* windows are one texture (TRANSP 9-10: walls GS alpha 128, windows 72..74) in one
class-2 material (word +0x0C 0x70001). `cutscenes.js ensureSet` drew every set batch with alpha as one blended pass with
no depth write, in index order: the far walls, later in the order, covered the near ones.

**PS2 rules** (the static-model draw 37E238; the sets' instances are world static models):
- Class from material word +0x0C (group flag bit 3 adds 0x40000) & 0x660000, as `prepare.py mesh_blend`: 0x20000 class 1
  (37F604: ALPHA enum 5 = 0x44, test mode 3 = GREATER, AREF 92); 0x40000 / 0x60000 class 2 (37F6B8 -> 37F750..37F7E0:
  ALPHA 0x44, GREATER, AREF 20 from the 0x14000 at 37F208, word1 bits 0..1 = 2: depth sorted). Depth mode 1 = ZTST GEQUAL,
  AFAIL FB_ONLY (TEST builder 3626D8).
- Z is written: word1 bit 22 (ZMSK, 363C20 -> 362660) is set only for an additive model (header +0x10 bit 3, 37ECA0..37ED18).
  So window texels (74 > 20) write depth too.
- The sort key (word2 bits 10..28) is the view depth of row 3 of the matrix at sp+0x158 (37F6E8 / 37F398 -> 37F708..37F750),
  which the node loop (37ED40..37F1BC) fills per node: node matrix x instance matrix (37ED9C..37EDD8), scaled (37F000..37F078).
  The render-list key 364240 is inverted and radix-sorted (364050), so a model's class-2 nodes draw back to front. Records of
  equal state and textures share a bucket, appended in submission order (362978). The cabin is 14 nodes (1..14, one wall
  panel each, pivots 4-5 m out along the panel normal), so the far side draws first.

**Port.**
- `tools/export_cutscene_sets.py` (`export_set`): `static_model_class` per mesh; class-2 meshes go into one batch per
  (instance, node) with `blend` and `sort_pivot` (the node origin in native metres; `model_node_pivots`). `--out` now reaches
  `export_set`. The TRANSP binaries are byte-identical; only world.json changes (10 + 14 batches instead of 11).
- `cutscenes.js ensureSet` (pv `setBlendClass`): a batch with alpha and `blend` 1 / 2 draws the two passes of
  `world-material.js` (texels above AREF blend and write depth, the fringe blends without), and a `sort_pivot` batch gets its
  bounding sphere centred on the node origin, the point three's transparent sort uses. An export without `blend` (the live
  package until the copy, the heli / plane sets) keeps the old pass.
- Draw-order evidence before the export: reversing the cabin batch's mesh order (or its triangles) in the page reproduced the
  PS2's view through the windows; the near-first order showed the sky through them.

**Frames** (PS2 `local/ps2-capture/ctm-parity/runs/to-final` sample00060 / 00100 = #150 t29 / t69, sample00200 / 00300 = #146
t49 / t149, camera alternative 2; scratchpad `gondola/`): `sbs-out-t29-chrome.png`, `sbs-out-t69-chrome.png`,
`crop2-t69.png` (PS2 | before | after): the near walls are solid and the far windows' frames and frost show through the near
windows, as on the PS2. Inside (`sbs-in-t49-chrome.png`, `sbs-in-t149-chrome.png`): the windows show the sky dome, frosted;
the sun glare through the left window is gone (the window now holds depth), and the PS2 frame has none. WebKit: not
captured yet. Its windows were hidden in this session (requestAnimationFrame 0, visibilityState 'hidden', offscreen too), so
the page drew nothing. The switch stays off until WebKit is checked. Still different: Zoe's outfit colours (the worn outfit), the #146 camera a little further back than the PS2's.

**Not done:** the heli / plane sets (`export_plane_set`, batches per LiveComp node) carry no `blend` / `sort_pivot` yet; their
texture-40 windows are the same class 2 and would need the same export.

Test: `test-visual-parity.mjs` R38 (the package part runs once the re-export is in).
