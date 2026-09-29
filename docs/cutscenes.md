# In-engine cutscenes (NIS), transitions and movies (2026-09-24)

SSX 3's "iconic" cutscenes are not movies. Almost all of them are in-engine scripts (EA's "NIS"): the start-hut
intro before every event, the podium celebrations, the rival challenges, the helicopter and gondola rides, the
lodge and station arrivals and the backcountry heli drops. They play the real riders (the race model with its
cinematic head, eyes and hands) with per-character animation, inside the world, with a script camera. Only the
backcountry first-arrival shots (ABC1/DBC2/EBC3) and the boot/reward videos are movies.

The browser plays the original scripts: `tools/export_cutscenes.py` exports them from the disc and
`web/cutscenes.js` replays them in the course world with the FE-preview rider models. See "Status" at the end for
what is complete and what is not.

## Files

**Disc (read-only):** `DATA/SCRIPTS/SCDAT.BIG` (BIGF).

| member | contents |
|---|---|
| `scmaster.dat` | `u32 167`, then 167 × `{u32 play count (0 on disc), u32 game_hash(name), u32 offset into scmasterdbg}`. Record *i* is script *i*. |
| `scmasterdbg.dat` | the 167 original names (`race\ra_sgb_var1`, `podium\win_ps_zoeb`, `transport\heli_inair_mac`, …). `game_hash` = `tools/animation_bank.py` `game_hash`; all 167 match. |
| `NNNNNNNN.big` | one load group per script: `scrN.isb` (the script), optional `anmN.afl` (its animation bank, the ANM.BIG AFL format) and `sndN.bnk` (its sound bank, BNKl v5). Streamed on demand (`%s%08d.big` 0x481C68, loader 0x2797C8). |
| `scdat_main.big` | resident 149 `heli_inair`, 150 `gond_inair`. |
| `scdat_<LOC>.big` | scripts preloaded with a location (0x278B98 → 0x27B370), with ONE combined `anmscdat_<LOC>.afl` / `sndscdat_<LOC>.bnk`. A script that is in both its own group and a location's has different clip/sound indices in the two copies. |
| `scfilter<LOC>.dat` | the location's 28 choice lists (see "Selection"). |

Old code names in script names: arielle = Allegra, deiter = Viggo, grommet = Griff, rocco = Nate.

**Movies:** `DATA/MOVIES/ABC1.MPC`, `DBC2.MPC`, `EBC3.MPC` (+ `WS` widescreen masters): silent MPEG-2
(`tools/export_movies.py`, needs ffmpeg). The pktrans music stream plays over them.

**Tools and browser assets** (all git-ignored output):

| tool | writes |
|---|---|
| `tools/cutscene_isb.py` | the `.isb` decoder (also a research dump: `local/cutscenes/isb/NNN.json`, `index.json`, `scfilter.json`) |
| `tools/cutscene_locators.py` | anchor locators (world record kind 18) and AIP start grids of every location |
| `tools/export_cutscenes.py` | `web/public/assets/CUTSCENES/`: `index.json` (scripts, containers, scfilter lists), `scripts/<container>.<NNN>.json` (runtime form of every copy, exact float32 curve coefficients), `anim/<container>.json` + `.f32` (the AFL clips in the `ANIMATIONS/library.json` shape: per-part streams, 30 fps), `banks/<container>.json` + `.bnk`, `locators.json` |
| `tools/export_cutscene_sets.py` | `CUTSCENES/SETS/TRANSP/`: the terrain-less TRANSP location (tilt-rotor and gondola cabin models) the in-air loops play in |
| `tools/export_movies.py ABC1 DBC2 EBC3 ABC1WS DBC2WS EBC3WS` | `MOVIES/<key>.mp4` (video only) |

`npm run setup` runs the first two exporters. Size: ~101 MB, mostly clip samples.

## Engine (SLUS_207.72, gp = 0x4A30F0)

**Script (`.isb`, little endian).** 0x2C-byte header: version 1, track count, magic 0x05E68A2A, size, `+0x0C` loop
flag, `+0x10` location mask, `+0x14` fade-in and `+0x1C` fade-out `{s8 type (1 colour fade, 8 none), s8 colour
(0 black, 1 white), s16 out, s16 hold, s16 in}` ticks, `+0x24` script number. Then the tracks: `{u32 size,
u32 n, u32 offset[n]}` + n alternative objects. One alternative per track is used per playback (0x279A70: actor
tracks keep the alternatives whose character mask matches the bound rider; then `rand() % n`).

Object `{u16 header size, u8 channels, u8 kind, u32 duration ticks, ext}` + channels `{u16 type, u16 size, payload}`.
Channel types: 0 piecewise cubic curve `{u16 t, f32 a, b, c, d}`, value `((a·u + b)·u + c)·u + d`, `u = t − key.t`;
1 ranges `{slot, t0, t1, value, start offset, f32 speed, blend ticks, tail[4]}`; 2 cut ranges; 3 point events.

| kind | object | channels |
|---|---|---|
| 0 | camera, Manual algo (0x1694B8/0x169570) | eye xyz, target xyz, roll, fov, shakes |
| 1 | camera, Target algo (0x169CF8/0x169DB0) | target xyz, distance, pitch, yaw, roll, fov, shakes |
| 2 | camera, Subject algo (0x16A458/0x16A510) | eye xyz, subject forward/up offset, roll, fov, shakes |
| 4 | cut list (director 0x281018/0x2816A0) | active camera id per range (ext +0x21 of the camera); shipped data: hard cuts |
| 5 | actor = a rider (handler rider+0x6D0, start 0x27F9F8/0x1241C0, clips 0x124788) | clip ranges, positional sounds, speech events (2 Finish_Line, 100 BC_Challenge, 101 Hey), x y z, rot x y z |
| 7 | audio / control (0x280640) | stage-script calls, music codes (0x28E8C0), PA/DJ cues (0x2A19D8), sounds, weather off |

**Time.** One tick per game update, 60 Hz (0x274A30). Verified: script 89 at t = 75 and 374 in two RAM dumps
300 samples apart, and the world tick equals the script time. A script lasts as long as its longest object.

**Anchors** (0x27A0D8, table 0x481D00). Every camera and actor position is in an anchor frame followed by the
object's ext offset: `p = Rz(yaw)Ry(−pitch)[Rz(e.yaw)Ry(−e.pitch)Rx(−e.roll)p + e.t] + pos` (cm, z up, degrees
in the data).

| anchor | frame |
|---|---|
| 0 | world origin |
| 1–18 | start-grid node of subject id + 2 (0x27B750: AIP kind-0 row of the rider's slot; 1 = the human, 13–18 = race riders 0–5) |
| 19, 23, 26, 27, 28 | locators 7 (`NIS_Lodge`), 0, 2 (helipad), 3 (gondola station), 4 (`NIS_Transport`), ground-snapped |
| 20 / 21 / 22 | podium steps 1st / 2nd / 3rd: locator 0 + (−20, 0, 340) / (−20, −275, 280) / (−20, 275, 280) |
| 24 | locator 8 (podium floor) |
| 25 | locator 1 (start gate), z − 1000, then ground-snapped (0x3369D8: nearest \|Δz\| hit) |
| 29 / 30 | locator 5 (helicopter) / 6 (gondola); the TRANSP location's when the script loops |
| 37 | locator 15 (ABC1's second plane) |
| 40–60 | a live actor |

Locators are the world records of kind 18 (one per location: 18 instance handles `track | rid << 8`; lookup
0x27BB08: pos = matrix row 3, yaw = atan2(m01, m00), pitch = asin(m02)). Verified: ARA1 locator 1
`mdl_ARA1_startgate_mainmodules_1000` = (−131779.234, 13946.858, −228782.719), yaw 0.150099 (live RAM identical;
anchor 25 after the snap −228770.812), grid slot 5 = (−131950.92, 14290.22, −228770.83), −170.49°.

**Cameras.** Manual: eye and target through the anchor frame. Target: eye = target − distance·(cos p cos y,
cos p sin y, sin p) with yaw/pitch + the frame's. Subject: eye through the frame, target on the subject rider.
FOV: the script value is a half-angle; the NIS view uses the 16:9 widescreen projection scales (the widescreen
table `Off 272.65/318.09` → `16:9 204.49/238.57`, both × 0.75) in a letterboxed 512 × 336 viewport, so
tan(vertical half-angle) = tan(fov) (not × 3/4 as for the race camera). Verified on PS2 frames: with × 3/4 the
browser view is 1.33× too zoomed, with × 1 it lines up.

**Letterbox.** Non-idle steps draw 60-line bars (of 480) that slide in over the first 30 ticks (PS2 zoe-sj:
content rows 11..466 at the first cutscene frame, 58..419 from 28 ticks on). The idle loop under the objectives card
has none.

**Actors.** Binding = subject + 1: 1–3 the script participants (podium places, the rival), 4/5 human players,
6–15 computer riders, 16–21 race riders 0–5. The rider shows the NIS head/eyes/hands (parts 5, 6, 8, 9) instead of
the race head/hands; faces and hands are animated by the clip's part streams 5/6/8/9 (no morph channel in the
script). Clip time = (start offset + (t − t0)·speed)/60 s, 30 fps, looping at (frames − 1)/30; clip index = AFL
index of the load group's bank. Root: position from the curves in the actor frame; rotation =
Euler(rotY + pitch, rotX + roll) then yaw rotZ + frame yaw − 90° about Z.

**Lists and skip.** Two NIS list players (`NISLists`, 0x278358; manager `[gp−0x84C]`), each a 5-step FIFO
`{list or script, flags, D, E, F}` (0x278E50 add from list, 0x278E20 add movie 29–31, 0x278F38/0x278F68 start,
0x279070 advance, 0x2790A0 clear). Flags: 1 skippable (`NISSkip = Cross.pressed`, INPUT.MAP; 0x276F48 cuts to
the next step, no fade), 2 dropped by a skip, 4 Start-pause allowed (never set: Start does not pause during a
cutscene, 0x231AB8), 8 held/looped until the game advances it (transport). "Press ✕ to skip" (`%s @skip %s`,
kT_OVRCMNPress + kT_OVRCMNToSkip, 0x1E9A30) blinks while the step is skippable.

## Selection

`scfilter<LOC>.dat`: `u32 28`, 28 × `{count, offset}`, entries `{u32 script, u16 mask[21], pad}`. ScriptChoice
0x27B0C0 keeps the entries whose non-zero masks all intersect the condition words (0x27BDB8: 0–2 participants
D/E/F, 3–4 humans, 5–9 computer riders, 10–14 second list, 15–20 race riders 0–5; word = `1 << CHARDB` of that
rider's base character, so a cheat skin counts as its base rider), then the least played, then `rand % n`. Scripts
listed three times (the `_fast` approaches) are three times as likely.

| list | contents |
|---|---|
| 0 / 1 | ABC1 midway plane / `heli_arrb_<char>_midwayabc1` (new career) |
| 2 | venue fly-over `*_sga_<loc>` (per location) |
| 3 | approach `*sga_approach*` (random) |
| 4 | start hut `ra_sgb_var1` / `var2` (race courses) |
| 5 | start-gate idle `ra_sgb_static` / `sgb_static` |
| 6 / 7 / 8 | podium `placeshow_ps` / `placeshow_2nd` / `placeshow_3rd` |
| 9 | winner `win_ps_<char>{a,b,c}` |
| 10 | winner in a cheat skin 10–20 or 28: `win_ps_cheat` |
| 11 | transport arrives: `endevent_trans_arr` (events) / `hub_trans_arr` (stations) |
| 12 / 13 / 14 / 15 | `heli_dep` / `heli_inair` / `heli_inair_<char>` / `heli_inair_multiplayer` |
| 16 / 17 | backcountry heli `<bc>_heli_arr` / `heli_arrb_<char>` |
| 18 / 19 / 20 / 21 | `gond_dep` / `gond_inair` / `gond_inair_<char>` / `gond_inair_multiplayer` |
| 22 / 23 | `lodge_arr3` / (empty) |
| 24 / 25 | rival challenge: `bc_chal_<loc>` / the rival's `ra_bc_<char>` (race) or `ss_bc_<char>` (freestyle) |

## Inventory: triggers, assets, per character

Function addresses are the original's. "All 10" = one script per base rider (Moby, Kaori, Allegra, Mac, Zoe,
Griff, Elise, Nate, Psymon, Viggo). **Sam** (this port's 11th rider) takes Mac's CHARDB slot (character 3) in the
port and in the Sam PS2 build (`tools/sam_ps2`, `local/sam-ps2/roster/sam-density-v5-test.iso`), so he gets Mac's
scripts with his own model; his NIS head is the one in his FE package (`RIDER_SAM/fe`). There are no Sam-specific
scripts on the disc. Cheat skins use their base rider's scripts except the podium (see list 10).

| # | cutscene | trigger | scripts / assets | per character | audio | skip | browser |
|---|---|---|---|---|---|---|---|
| 1 | Boot EA/THX, intro movie | FE boot 0x1A1CE8, title idle 0x1948BC | EABIG, THX, INTRO(_DJ).MPC | – | own | Start/Cross | intro video already in Rewards (`tools/export_movies.py`); 2026-09-27: the title's idle attract (intro.mpc after 1801 frames, pv `attract`) and the power-on EA / THX / DJ intro (pv `bootMovies`), [intro-movies.md](intro-movies.md) |
| 2 | Event load screen | cGameLoadState 0x232E20 | GL.LUI 110ctrl_load / 99QPEvent / 102MPMatch, LoadingScreen.bnk | – | loading loop | no | done earlier (`docs/loading-screen.md`, 110ctrl_load for every load) |
| 3 | **Event intro, Single Event / online** | WS10 enter 0x234F40 → 0x27AAF8, list table 0x481E68 | [4 `ra_sgb_var1` #89 / `var2` #96 (flags 3)] → [5 `ra_sgb_static` #73/#74 (0)]; actors = the 6 race riders at their grid nodes, clip *i* per slot | the lineup's riders (the human is slot 0) | EA RADIO BIG box, world song (event 36), PA venue intro | Cross → idle | **done** |
| 4 | **Event intro, Conquer the Mountain** | same, table 0x481E48 | [2 fly-over `ra_sga_<loc>` #94/95/88/97/98, `ss_sga_*` #92/93/77, `ba_sga_*` #75/76/80, `hp_sga_*` #81–83 (0)] → [3 approach (3)] → [5 idle (0)] | human + computer riders | fly-over: music code 21-25, cue 0xF PA_Venue_Intro, its bank sound; approach: cue 2 PA_Rider_Race_Intro (1 for freestyle) | fly-over no, approach Cross | **done**; from free ride (world state 1, the gate) see [ctm-flow.md](ctm-flow.md), compared frame by frame at Snow Jam and Metro-City |
| 5 | Objectives card over the idle loop | WS2 (0x236BB0), its exit 0x236CD8 stops the NIS | list 5 | – | – | Cross = Continue | **done** |
| 6 | Countdown, gate, GO | WS3 0x234AD0 / 0x234C68 | not NIS (race camera) | – | 29C420/29C7B0 | – | done earlier |
| 7 | Finish shot, results | WS5 0x233C50, WS7 0x236DA0 | not NIS (finish camera, overlay) | – | results DJ | – | done earlier |
| 8 | **Podium** (CTM only: final round, one human, 1st–3rd) | WS5 update 0x233CD8 → 0x27AC60 (table 0x481E90) → WS12 (chartune 0x28CDF8) → WS7 | 1st: `win_ps_<winner>{a,b,c}` #30, #33–62 (Mac: maca #49, macb #50, macc #30), cheat winner `win_ps_cheat` #36; 2nd `placeshow_2nd` #31; 3rd `placeshow_3rd` #32; fallback `placeshow_ps` #29; D/E/F = places 1–3 | **all 10 × 3 variants** (Sam = Mac's) | winner's chartune (Moby 10, Kaori 2, Allegra 8, Mac 1, Zoe 5, Griff 9, Elise 3, Nate 7, Psymon 4, Viggo 6), script banks | Cross | **done** (`career-ui.js finishCutscenes`); PS2 comparison pending (see Verification) |
| 9 | **Rival challenge issued** (once per peak and discipline, not for cheat skins 10–20) | queued after the podium by 0x27AC60 (0x1464D0 unlocked, 0x146150 not shown, 0x146320 marks it) | [24 `bc_chal_<loc>` #0–2, 21–26 (1)] → [25 rival's `ra_bc_<char>` #3–10, 28 / `ss_bc_<char>` #11–20 (3)] | **all 10** (the rival's) | speech 101 Hey (rival → player) at t 200, script banks | Cross | **done** (`career-ui.js`, saved as `rivalShown`). The rival D is the race's **first computer rider** (list +0x48), not the peak's named rival: PS2 Zoe at Snow Jam got `ss_bc_psymon` for Psymon |
| 10 | Rival run rolling start | 0x2872A8 | not NIS | – | BC_Challenge_<char> | – | done earlier (`docs/backcountry.md`) |
| 11 | **Transport ride** | results / MCOMM / booth → WS14 arg 1 → 0x27A860; heli when either end is a backcountry, else gondola; departure only from stations | [12 `heli_dep` #147 / 18 `gond_dep` #126 (1)] → [13 `heli_inair` #149 / 19 `gond_inair` #150 (0)] → [14 `heli_inair_<char>` #113–122 / 20 `gond_inair_<char>` #138–146, 125 (8, held until loaded)] | **all 10** (the in-air loop) | 28E8C0(20) course change, LoadingScreen loop, script banks | departure only | **done** in the streamed world (`main.js transportInWorld`), with the TRANSP set |
| 12 | **Transport arrives** (then the Map) | booth = stage builtin 68 action 3 → WS14 arg 2 (0x236250) | [11 `hub_trans_arr` #166 / `endevent_trans_arr` #152 (3)] | generic | – | Cross | **done** for the station booth; not after event results |
| 13 | **Lodge / station walk-in** | door volume `mdl_<hub>_NIS_Lodge_0` → builtin 68 action 4 → WS14 arg 0 (0x236208) | [22 `lodge_arr3` #148 (3)] → prompt 0x1F | generic (the player's rider, PDA) | script bank | Cross | **done** (door and station arrival) with the PDA prop (`board_PDA_NIS`, `tools/export_cutscene_props.py`: rigid on `handright`, shown by clip event id 0, its flip-open morph driven by the clip's part-11 stream) |
| 14 | Lodge load in / out | cFELoadStateInLodge / cGameLoadStateOutLodge | FL.LUI 117loadinlodge, GL.LUI 118loadoutlodge | – | loading loop | no | **done** (`web/transition-screens.js`, `tools/export_transition_screens.py`: the handheld with the percentage over the ice, on lodge Yes and Return to Game) |
| 15 | **New career: plane FMV + drop into Happiness** | WS10 0x234F40/0x235080, course 14 first visit | FMV 29 ABC1.MPC → [0 `abc1_heli_arr_midway` #153] → [1 `heli_arrb_<char>_midwayabc1` #154–163], flags 3 | **all 10** (the jump) | pktrans Peak 1; DJ_BC_Intro | Cross ends the chain | **done** (`career-ui.js enterWorld`: the list plays before the ride and keys on the visited mask +0xACC; the movie between the NIS bars with the skip prompt; the rider camera's anchor 43 = binding 4, 2026-09-25 [ctm-parity.md](ctm-parity.md)); the midway plane flies in and hovers (SETS/ABC1PLANE, [presentation.md](presentation.md) 1) |
| 16 | Backcountry heli drop (revisit; Peak 2/3 first visit with DBC2/EBC3 FMV) | same, courses 15/16 | [16 `<bc>_heli_arr` #123/124/127] → [17 `heli_arrb_<char>` #128–137] | **all 10** | DJ_BC_Intro | Cross | **done**: first arrivals by `career-ui.js enterWorld` (DBC2 / EBC3 movie + 16 + 17), a transport into a visited backcountry by `free-ride.js transport` (16 + 17, 0x235220) |
| 17 | Next round (CTM gondola ride-up) | WS13 0x235AA0 | 19/20 gondola in-air, then 4+5 or 5 | all 10 | 28E8C0(20,1) | var: Cross | **done** for race heats (`career-ui.js` next heat -> `playCutscene({kind:'heat'})`: `gond_inair` -> `gond_inair_<char>` held 340 ticks, the PS2 semi/final's measured release -> `ra_sgb_static` under the heat card) |
| 18 | Character select / setup personality | FE preview 0x1A0358 | FE clips FE_GEAR_<X>_CYC / FE_CHARSEL_<X>, speech Post_Selection/Customize | all 10 + Sam (his own clips) | charsel events | – | done earlier (`docs/characters.md` "Front-end preview") |
| 19 | Medals, awards, unlocks, peak unlock | overlays after WS5 | not NIS | – | – | – | done earlier (`career-ui.js` award screens) |
| 20 | Ending | – | **none on the disc** (no NIS, no movie; only per-character messages kT_MSGEN<Char>1..3 and a popup) | – | – | – | n/a |

Unused on the disc: #27 `bc_chal`, #151 `gond_inair_down`, list 23, the idle lists and stage builtin 36.

## Browser implementation

**`web/cutscenes.js`** (`createCutscenes(host)`, `playCutscene({kind, id, rider, location, ...})`):
- `playCutscene` resolves `{played, skipped, scripts}` and is safe to call before the system is up (`{played:
  false}`). kinds: `intro` (mode `single`/`career`), `podium` (place, `roles` = [1st, 2nd, 3rd] rider ids, `null`
  = the human), `rival` (`roles` = [rival]), `transport` (`heli`, `departure`, `multiplayer`; `cutscenes.advance()`
  releases the held in-air loop), `transport-arrive`, `lodge`, `heat`, `arrival` (`location`, `firstVisit`),
  `script` (`id`), and the free-ride kinds below. The overlay screen `cutscene` is set for the duration and the
  previous screen restored (unless `restore: false`). Steps can also be given directly (`steps: [{group|script|fmv, flags, idle}]`).
- Selection = the exported scfilter lists with ScriptChoice and play counts; the cast = `{roles, humans, ai, race}`
  of rider records `{id, slot, entry (riders.json), character (CHARDB base), scale (model_size / 100)}`.
- Each step: one alternative per track, the load group's clips/bank, then per rendered frame `update(dt)` advances
  in 60 Hz ticks, poses the actors and returns the camera; `applyCamera` sets the three.js camera (PS2 cm →
  scene (x, z, −y)/100 − origin).
- Actors are `FrontEndPreview` models (`web/fe-preview.js`: the worn outfit with the NIS head, eyes and hands and
  their morphs), one per actor from a pool (≤ 8 kept idle), posed by `apply()` with the clip's streams (board
  kept), lit like the FE preview (IRR record + the 389CB8 rim) but with the cutscene camera's view matrix
  (`light(T, core, view)`, new optional argument).
- Sets: the TRANSP models are drawn for the looping in-air scripts (texture × baked colour).
- Audio (2026-09-25, [ctm-flow.md](ctm-flow.md) section 5): the load group's bank on sfx slot 0x11 (the PS2 uses slot 17 + the
  bank's group: 17 resident scdat_main, 18 the location's scdat, 19-26 per-script groups); every NIS sound on the CHARACTER bus at
  speaker 0's gain (0x280F3C); actor sounds positional at the actor root in PS2 cm; a step's clock starts only when its
  actors, bank and set are in (the sounds used to be requested before the bank existed and never played). PA cues of
  0x2A19D8 0 / 1 / 2 / 3 / 7 / 0xC / 0xF, music codes through `gameAudio.cutsceneMusic`. Fades chain like 0x277980
  (`fadeAt`): a later step fades in with the previous step's fade-out record, not its own fade-in record. The world's
  sounds (ambience, emitters, crowd) keep running under a cutscene that plays before the run. Speech events through the speech
  engine (`finishLineRider`, `bcChallenge`, `hey`), PA cues (sponsor, medal run, event intro, venue intro).
- Overlay: letterbox, blinking "Press ✕ to skip", header fades (white for `lodge_arr3`), the world's yellow
  "Loading..." caption with a spinning snowflake on held/looping steps (the in-air rides; PS2 ctm-transport), drawn by
  `ui.js` when the screen is `cutscene` together with the EA RADIO BIG box.
- Skip: Cross only (Space, gamepad button 0, the touch deck's ✕, which sends Space off the race screen); Start does
  not pause during a cutscene.
- Movies: `/assets/MOVIES/<key>.mp4` (the WS master when the display is widescreen), skipped when missing.

**Hooks** (small, additive):
- `ui.js`: `loadEvent` also runs `cb.introPrepare` (scripts + actor models load under the load screen) and then
  `cb.intro(next)`; the `cutscene` screen ignores menu keys and draws the overlay.
- `main.js`: `setupCutscenes()` (host, cast from the lineup, ground snap through `core._height_at`), the cutscene
  camera after the chase/original camera, the world drawn while a cutscene is active, the race rider hidden,
  `startRun` stops the idle loop, no Start-pause, `ui.cb.cutscene(o)`, `freeRideCourse` for the host.
- `free-ride.js` (the Peak 1 agent's call sites): `playCutscene` kinds `lodge-walkin`, `transport-booth`,
  `transport-depart`, `transport-loop` (`until` = the destination rows are in) and `station-arrival`, with course
  indices as locations; the departure and the in-air loop use the lists of the location being left.
- `career-ui.js`: `finishCutscenes` (podium + rival, then results/award), `enterWorld` (new-career drop).
- `game-audio.js`: the podium chartune is not restarted when the results screen opens after the podium.
- `?cutscenes=0` disables the system.

**Quality / phones:** the cutscene uses the same frame gate and render path as the race; on the low tier the
actors are the same FE models (a few thousand vertices each). The touch deck's ✕ skips.

## Verification

Tooling (scratchpad, not in the repo): `cdp.mjs` drives headless Chrome for Testing (Metal/WebGPU) against the dev
server, `window.__cutscenes.freeze()/seek(t)` holds a script at a tick and a step's `alts` forces track alternatives;
`sbs.py` / `nisbatch.py` pair each browser frame with the ARMSX2 frame of the same **script time** (read from the
request slot in each snapshot's RAM by the capture agent: `scratchpad/cs/ps2b/`, see its CAPTURES.md; the podium and
rival runs were forced from a real CTM Snow Jam finish by poking the place tables and the list map).

**Random alternatives.** Most scripts carry 2-3 alternatives per camera (and per 2nd/3rd-place actor) track, and each
track picks one independently with `rand % n` (0x279A70), so two playbacks differ. For the comparisons every
alternative was rendered and the one closest to the PS2 frame (mean absolute difference) is shown; e.g. PS2 Zoe
`win_ps_zoeb` used wide-camera alternative 1 and close-up alternative 2.

- `web/test-cutscenes.mjs` (npm test): scripts 89/73 cameras = live PS2 RAM (eye/target within 0.03 cm, fov
  0.6981317), anchors 25 and grid slot 5 = live, bindings/clip index/clip time (1.25 s at t 75) = live layer-2 state,
  ScriptChoice (Zoe's and Mac's podium triples, var1/var2 with play counts, Zoe's rival scene), containers, the step
  lists, the PDA prop.
- Side-by-side at matching script times (images in `scratchpad/cs/web/`):

| cutscene | PS2 run | riders | result |
|---|---|---|---|
| start hut `ra_sgb_var1` (89) | zoe-sj, psymon-sj, kaori-sj, sam-sj (derived ISO) | 4 lineups incl. Sam | camera path, framing, per-slot riders and poses line up (`{zoe,psy,kao,sam}89-sbs*.png`) |
| gate idle `ra_sgb_static` (73) | zoe-sj card | Zoe | same shot under the card (`idle-sbs.png`) |
| CTM fly-over `ra_sga_ara1` (94), approach `ra_sga_approach3a_ara1fast` (66) | ps2b/intro | Zoe + CTM lineup | camera paths match (`ctm-sbs.png`); PS2 shows "Loading..." (world still streaming) where the port shows nothing; the port's sun flare is visible where the PS2 has none (sun painter region) |
| podium `win_ps_zoeb` (61), `win_ps_psymona` (54), `win_ps_mobya` (51), `placeshow_2nd` (31) | ps2b/podium-* | Zoe/Psymon/Allegra, Psymon/Zoe/Allegra, Moby/Zoe/Psymon | cameras, step placement, each winner's personality animation (Zoe's dance, Psymon lying on the ground, Moby's Union-Jack pose) match frame for frame (`pz61/pp54/pm51/p2nd31-sbs.png`) |
| rival `bc_chal_335` (22) + `ss_bc_psymon` (18) | ps2b/podium-rival | Zoe vs Psymon | match (`riv-sbs.png`) |
| heli `heli_inair` (149) + `heli_inair_zoe` (122), gondola `gond_inair` (150) + `gond_inair_zoe` (146) | ps2b/transport, final-intro | Zoe | match incl. the TRANSP cabins (`tr-sbs.png`); Zoe's board colour differs (worn outfit vs the capture's career board) |
| `lodge_arr3` (148), `hub_trans_arr` (166) at the Green station | ps2b/lodge-arr, hub-booth | Zoe (and Sam) | match incl. the PDA opening (`station-sbs.png`, `pda-zoom.png`) |
| flows | – | Zoe, Kaori, Psymon | Single Event load -> intro -> Cross skip -> card over the idle -> countdown (`flow*-sheet.png`); next heat -> gondola -> idle under the card at 7.8 s |

Differences that are not the cutscene code: the actors are lit with the FE preview's IRR record + rim (bluer/darker
than the PS2's world lighting of riders); the hut's pink floor and some light-rig beams come from the world renderer;
the podium building streams in with the rider's section (texture chunk 27): after a real finish it is resident, the
isolated comparisons used `?chunks=0`.

## Status and gaps

Complete: the NIS engine (all kinds used on the disc, anchors, selection, alternatives, skip/flags, fades, letterbox,
Loading caption), the exporters, the Single Event and CTM event intros with the idle loop under the card, the podium and
rival challenge flow, the between-heats gondola, the transport ride, the booth and lodge arrivals with the PDA, the
lodge load screens, the new-career drop.

Partial / missing:
- The new-career drop (ABC1 midway, 153 + 154-163) has no PS2 side-by-side; the ABC1/DBC2/EBC3 movies need ffmpeg to
  export (none on this machine), so the drop starts at the plane.
- Stage-script calls from kind-7 objects are run for sets with a LiveComp record (the ABC1 midway plane, 2026-09-26,
  docs/presentation.md section 1: `web/cutscene-stage-sets.js`); the TRANSP helicopter's own calls (heli landing / rotor
  wash, TRANSP program 2) are not exported yet, so it does not move; heli_inair's rider is hidden (no clip).
- (Done 2026-09-25: music codes 21-25 and the PA cues 1 / 2 / 7 incl. PA_Medals; NIS bank sounds audible; see ctm-flow.md.)
- Kind-4 transitions other than cuts and fade types 2-7 are not drawn (unused by the shipped data except three
  zero-length type-1 cuts).
- Peak 2/3 heli arrivals are not triggered (the port has only Peak 1).
- The actors use the FE lighting, not the world's irradiance and local lights.
