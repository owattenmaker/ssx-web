# Event loading screen (2026-09-22)

The browser shows the original SSX 3 "Basic Controls" load screen when an event is loaded, played from the
disc's own layout and art, with a controls panel for the current input device and one original load hint.

## What the original does

**Files (read-only from the user's ISO; exported to git-ignored `web/public/assets/LOADING/`):**

- `DATA/UI/GL.LUI` + `DATA/UI/GL_1.SSH` is the "game load" package. Its pages are: 0 MCOMM art, 1 Widg (snowflakes),
  2 trees, 3 mountain, 4 `help` (the DualShock 2 picture and button icons), and 5 the orange title swoosh and the SSX 3
  logo. The GL_1 CLUTs go through the CSM1 swizzle, including the tree page's short CLUT with 98 entries. Without it,
  the white trees come out speckled.
- `DATA/UI/FL.LUI` + `FL_1.SSH` is the front-end-bound set: 101QPMPHints, the stats screens and 117loadinlodge.
- `DATA/FONTS/FLOAD.SSH`, `GLOAD.SSH` and `SPLASH.SSH` each contain only the 512x512 SSX 3 boot logo.

**Screens in GL.LUI:** 99QPEvent (course card), 102MPMatch, 110ctrl_load (Basic Controls), 118loadoutlodge, and one
unnamed screen (0x044FAE42).

**Selection, `cGameLoadState` init 0x232E20:**

- If flag `gp-0x1718` is set, or if the game mode byte `0x535BC8+0x48` is 5 or 6, it builds `cGameLoadStateOutLodge`
  (vtable 0x47C878, screen 118loadoutlodge).
- Otherwise, if `0x535BC8+0x49` is 0 (Conquer the Mountain), it builds `cGameLoadStateConquer` (vtable 0x47C7A8). Its
  enter function 0x245730 opens **110ctrl_load**.
- Otherwise (Quick Play), it builds `cGameLoadState` (vtable 0x47C948). Its enter function 0x245418 opens
  **99QPEvent**, or 102MPMatch when `0x535C11` is 2.

**Hints:** 101QPMPHints enter 0x245950 shows `kT_FEHINTTitle%d` / `kT_FEHINTDES%d` (CMNAMER, 15 hints) with
n = counter + 1. Hint 12 (Bragging Rights) is skipped. Then counter = (counter + 1) % 15. The counter is byte
`gp-0x4B8` (0x4A2C38), which starts at 0.

**LUI format, decoded by `tools/export_loading_screen.py`:**

- Coordinates use a 640x480 frame.
- The frame-0 state is either:
  - a `0x21` record of property pairs: 0 x, 1 y, 3/4 pivot, 5 rotation (degrees), 6 w, 7 h, 9/10 scale %,
    12 anchor, 13..16 A R G B, and for shapes 4 vertices at 21+9k (x, y, then A R G B at 26..29+9k); or
  - a `0x20` record (animation, element, mode) that binds an animation.
- Later states are timeline events at frames 1..500. They start animations (mode 1 holds, mode 9 loops) or set
  properties.
- The animations are in the GL.LUI u1 table. Each is a `0x50` record with tracks, and each track is a `0x51` line
  (property id, n values, with n-1 frame counts between them), interpolated linearly.
- The draw layer is the definition flags & 0x3F.
- Anchor bits: 8 left / 16 centre / 32 right, and 1 top / 2 middle / 4 bottom.
- LUI text at 100% is FEFONT at 0.79 of its native size in PS2 pixels. This was measured against the frames.
- The screen contains:
  - the sky quad and white ramp;
  - three rows of trees that scroll sideways and loop (parallax);
  - the mountain;
  - twelve snowflake flight paths;
  - the controller picture with its white leader lines;
  - the "Default" and "Pro" label sets;
  - the "NN%" (element 0x25, text set at runtime) and "Loading..." text.

**PS2 truth (ARMSX2):**

- **Single Event, Snow Jam.** Used `tools/ps2_navigate.py`: `snow-jam-rules.p2s` + Cross, snaps every 20 samples. The
  pad script is `local/browser-validation/loading-screen/sj-load.json`.
  - The screen is 99QPEvent and is up from sample about 55 to about 870, roughly **13.6 s**.
  - It fades in from black over about 20 frames.
  - The percentage goes 2% at frame 20, 17% at 120, 27% at 240, 52% at 400, 97% at 520, then holds at 98% for about
    4.4 s, then 100%. It is black 20 frames later, and then the intro cinematic plays ("Press X to skip").
  - No input skips the load screen.
- **Conquer the Mountain.** From `local/ps2-capture/menus/ctm/07..09`, after rider select: **110ctrl_load**, "Basic
  Controls / Default". The labels are Grab board (L1/L2, R1/R2), Boost/Tweak (Square), Jump (Cross),
  Turn/spin/flip (D-pad), Turn (left stick) and Reset (Select).
- The career transport inside the game shows a movie with "Loading..." instead. That is not this screen.

## Browser implementation

**`web/loading-screen.js`** (`LoadingScreen`, drawn by `ui.js` when the screen is `'loading'`):

- It plays the exported 110ctrl_load for every event load, with its elements, animations and timeline, from
  `loading.json`. The original shows it only for Conquer loads, and Single Event shows the course card. The browser
  always shows the controls, because that is the point of the request.
- **Gamepad:** the Default label set plus three records from the screen's own "Pro" set, each with its leader line.
  The Pro "No function" record points at Circle and is relabelled Hand plant, because the port's INPUT.MAP puts
  handplant on Circle. The other two are Board press (right stick) and Pause (Start).
- **Keyboard:** the DualShock and its labels give way to a keycap table in the same label style. It follows the current
  Simple/Classic mode from `pad-input.js`: WASD/arrows, IJKL (Classic), Space, Shift, Q Z E X, C, T F G H (+V ollie),
  Backspace and Esc.
- The device is the one that was used last (shared with every control hint: `web/input-glyphs.js`,
  docs/input-glyphs.md). A gamepad counts once a button goes down or a stick moves; a real keydown counts as the
  keyboard; touch (the deck) shows the pad layout. With no input yet, a connected pad counts as a gamepad. The key caps
  are `input-glyphs.js drawKeyCap`, which every keyboard hint uses.
- One original hint is shown ("Hints and Tips: title" and the body), rotated as in 0x245950. The counter is kept in
  `sessionStorage`.
- **Timing:**
  - The minimum display is 7 s (`?loadingMs=` overrides it; `?loading=0` disables the screen).
  - The percentage follows the measured Snow Jam curve, stretched over the minimum. It holds at 98% until the work
    promises have settled and the minimum has passed.
  - Then it shows 100% for 12 frames, fades to black over 20 frames, and continues.
  - As in the original, it cannot be skipped.
  - With pv `loadMeter` (off until verified on the live site), the percentage is the load's own work instead: see "The honest
    meter" below.
- **Hooks:**
  - `ui.loadEvent(next, work)` is the entry point.
  - `career-ui.js` `begin()` goes through it before `ctm-objectives`. This covers Single Event, transport, and the
    resume after a course change (in the page since 2026-09-25: main.js `switchCourse` opens the screen before the old
    course is released and keeps it up through the new course's load; `cancel()` drops a session without its
    continuation, docs/course-switch.md).
  - In `ui.js`, the non-career event pick goes through it before `game`.
  - In `main.js`, `?autostart=1` opens the screen right after `ui.load()`, so it covers the real course asset load
    in `init()`. The autostart branch without career resumes through `loadEvent`. `R` is ignored while the screen is
    loading, and a load error returns to the title.
  - Restarts do not show it.
- **Widescreen:** the UI canvases stretch, so Anamorphic shows the frame stretched to 16:9, as a 16:9 TV shows the
  original front end. Off and 16:9 show it at 4:3.

**Tools and checks:**

- `tools/export_loading_screen.py` runs in `npm run setup`.
- `web/test-loading-screen.mjs` runs in `npm test`. It checks the hint order, LUI track playback, the percentage
  curve, the keyboard rows and the exported layout.
- `web/loading-preview.html` is a dev fixture: `?frame=N`, `?device=gamepad|keyboard`, `?mode=Classic`, `?wide=1`,
  `?ref=/png&opacity=.5` (PS2 overlay), `?crop=x,y,w`.

**Verification:**

- The PS2 110ctrl_load frame (23%) was overlaid on the web render at 50%. The controller, leader lines, swoosh, logo,
  mountain, title, labels, "NN%" and "Loading..." coincide to within about 1 px.
- Headless Chrome was driven over CDP through the real app: `?autostart=1` gives loading, then game, at 4:3 and at
  Anamorphic 16:9. The Single Event pick gives loading for 7.5 s, then ctm-objectives, then game.
- Screenshots are in `local/browser-validation/loading-screen/`.

**Not done:**

- 99QPEvent (the course card) and the other GL/FL screens are not played.
- Snowflake rotation ignores the pivot properties 3/4.
- Gouraud shapes are limited to flat quads and two-colour ramps, which is all this screen uses.

## The honest meter (pv loadMeter, 2026-10-04)

**The report:** after the assets load, the screen "freezes" at 98% and feels stuck.

**Cause:** the meter was a clock, not a measure of the load. The Snow Jam curve was stretched over the 7 s minimum, so it reached
98% after 4.6 s (66% of the minimum). It then stayed at 98% for the rest of the minimum and for the load's own work (the course
still downloading, the warm-up, the intro's preparation). On a CTM world load, which never sets a continuation, it sat at 98% and
then cut straight to the arrival cutscene.

The PS2 meter does measure work. On the new-career CTM load (ARMSX2 `local/ps2-capture/ctm-parity/runs/new-career`, every 25
samples) it climbs unevenly, 0 5 9 12 15 18 21 23 25 27 29 30 38 45 50 55 60 63 69 85%, and then goes black. It has no 98% hold
there. The Single Event's 4.4 s at 98% is that load's real last step on the PS2.

**Measured before the fix** (cold Chrome profile, headless, 1280x720, this Mac, local dev server; WebKit through
web/webkit-driver.mjs). Times are from the load screen opening:

| load | Chrome 1x | Chrome 4x CPU | WebKit | after the load (first 5 s) |
| --- | --- | --- | --- | --- |
| Snow Jam Single Event, picked right after Press START (course still loading) | 98% at 4.7 s, held 2.4 s | 98% at 10.3 s, held 7.3-11.5 s (warm-up 5.5-9.4 s, intro prep 1.6 s) | 98% at 4.6 s, held 2.4 s | intro max 29 / 42 / 38 ms, card max 27 ms |
| the same at 20 Mbit/s, 40 ms | 86% while the course downloads (44 s), then 98% held 3.8 s | | | intro max 17 ms |
| BRA2 Single Event (an in-page course switch) | | | smooth | intro max 35 ms |
| CTM new career (PEAK1 world load) | 95% at 4.5 s, then the cut | 98% from 4.7 to 15.4 s (10.7 s), then the cut | 92% at 4.3 s, then the cut | arrival max 100-125 ms (the music start), ride max 48 ms |
| in-world Transport (MOUNTAIN to Snow Jam) | no load screen (held loop) | | | max 100 ms in the ride, 17 ms after the arrival |
| in-world event gate | no load screen (since 10-01): the lag agent's pv eventRiderWarm | | | |
| Restart | no load screen | | | Chrome 4x: the race's steady 58 ms frames |
| online race | the same ui.loadEvent path | | | not measured (needs a lobby) |

Nothing after 100% stalls on this machine: earlier work (worldWarm, rideWarm, riderPrefetch, the warm-up) had already moved
first-use work under the screen. A 466 ms "first race frame" and 741 ms "first load frame" seen in the first profiles were the
CDP profiler starting, not the game.

What still makes the screen stutter under it (Chrome 4x, by frame):
- the warm-up's first frame builds the post passes: 11 pipelines and 11 node builds, 225-483 ms;
- each warm slice takes about 100-125 ms, because a new material variant means about 4 node builds;
- the intro cast's FE compiles (cutscenes.prepare through host.compile) take 140-216 ms each;
- the course's core init calls take 180-325 ms.

At 1x these are 50-110 ms. Left: splitting a variant's passes over frames, which needs three.js internals.

**The fix (web/load-meter.js, wired in web/loading-screen.js, ui.js, main.js and free-ride.js):**
- A load is a plan of stages, each weighted by its measured time (`STAGE_MS`). A stage either reports its own fraction or
  creeps towards 90% of itself over its typical time.
  - event load (`ui.loadEvent`): `course` (only if the page's first course is still loading behind the menus, weighted by the
    share left; fed by `workFraction`), `rider`, `lineup`, `warm`, `intro`;
  - world / course switch (`main.js switchCourse`): `unload`, `course`, `ride` (rideWarm), `world` (worldWarm), `gc`
    (switchGC), and `pending` for an event course whose event load follows. The event load's own stages then take that
    `pending` stage's place and share its weight, so the bar does not jump.
- Progress reports:
  - `warmupRender` reports its pre-steps, slices (drawables revealed) and final frames by `WARM_SHARE`;
  - `warmWorld` reports its compiles as they settle (`WORLD_WARM_SHARE`), then its warm frames;
  - `loadCourse` reports milestones (`COURSE_SHARE`, one table for event courses and one for streamed worlds; the stage creeps
    towards the next milestone in between);
  - free-ride.js `start` reports the start row's locations as they arrive.
- The displayed percentage is min(pace, 99 x work):
  - The pace is the PS2 curve up to its first 98%, stretched over the minimum, so it reaches 98% as the minimum ends.
  - The pace applies only once the session waits for its minimum (`run()` gave it a continuation). It continues from the
    number already shown, so a course switch that turns into an event load does not hold.
  - A world load that may close into a cutscene shows only its work.
  - 100% only when every promise has settled; then the 12 frames at 100% and the fade, as before.
- Off: unchanged (the curve, 90% caps for downloads and the lazy course, 98% until done).

**Measured with the switch** (same set-up):

| load | before | with loadMeter |
| --- | --- | --- |
| Single Event, Chrome 4x (back to back) | 98% held 7.3 s; the number was still for up to 7.3 s | 98% held 0.5 s; never still for more than 1.7 s; load time the same (13.6 / 14.6 s) |
| Single Event, Chrome 1x / WebKit | 98% held 2.4 s | 97 -> 100 in 0.2-0.3 s |
| Single Event at 20 Mbit/s | 86 -> 98, held 3.8 s | climbs to 95, then 100 |
| BRA2 switch + event, WebKit | | 0 -> 97 over 7 s, 100 at the end |
| `?autostart=1`, Chrome | | 0 -> 97 -> 100 |
| CTM world load, WebKit / Chrome 4x | 92-98%, held up to 10.7 s, cut | climbs steadily to 96-97%, cut at the end of the work |

Frames on and off are the same: the switch adds no work, only the reports.

**Checked:** `web/test-load-meter.mjs` (npm test) covers the weights, creep, the creep floor and ceiling, the pending expansion
and segments, the monotonic value, the climb curve, and 100% only when the load is done; with a LoadingScreen, 98% with every
stage done but a promise pending, and the pace continuing from the number shown. Also passed: test-loading-screen,
test-lazy-course, test-world-warm, test-ctm-stream, test-stage-world, test-peak-release, test-rider-prefetch,
test-ctm-event-world, test-career-rider.

**QA:** `local/load-meter/qa/` holds the probe and its traces:
- `loadprobe.mjs`, with FLOW=single|late|ctm|transport|auto, BROWSER=chrome|webkit, THROTTLE, NET=mbit,latency, PV, COURSE,
  PROFILE;
- `an.mjs` for frame stats, the percentage, stages and the worst frames, with CPU-profile attribution;
- `recorder.js` and `server.mjs` (start a new server after edits: it does not watch).

**Not confirmed from the code:** the PS2's own meter weighting (which loader steps move it); the port's weights are measured
times, not the PS2's. Whether the PS2's CTM load shows 100% before its black: the 25-sample captures go from 85% to black.

