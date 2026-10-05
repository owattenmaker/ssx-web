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
  - With pv `loadMeter` (off until verified on the live site), there is no minimum and the percentage is the load's own work,
    with a line under it saying what the load is doing: see "The meter is the load" below.
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

## The meter is the load (pv loadMeter, 2026-10-04 / 10-05)

**The reports:**
- The screen "freezes" at 98% and feels stuck.
- The user wants the number to be real progress, not a timer, and the level fully loaded behind the screen so it is ready the
  moment the screen goes. A plain line under the % says what is loading, so players see progress and a "stuck" report says where.

**Cause:** the meter was a clock.
- The Snow Jam curve was stretched over the port's 7 s minimum, so it reached 98% after 4.6 s.
- It then held 98% for the rest of the minimum and for the real work.
- On a CTM world load, which sets no continuation, it cut to the arrival cut without ever showing 100%.

**PS2 truth:**
- The PS2's screens show the loader's progress and have no minimum. cGameLoadState (vtable 0x47C948) has its update at 0x2454F8.
  Each frame it formats the loader's float as "%3d%%" (string 0x4A2BD0) into the screen's text element, and keeps no frame count.
  cGameLoadStateConquer's update is 0x2458B8.
- FL.LUI 117loadinlodge likewise "lasts as long as the I/O" (docs/ctm-decomp-screens.md section 5).
- On the new-career CTM load (ARMSX2 `local/ps2-capture/ctm-parity/runs/new-career`, every 25 samples) the number climbs
  unevenly by work: 0 5 9 12 15 18 21 23 25 27 29 30 38 45 50 55 60 63 69 85%, then black.
- The 7 s minimum was the port's own choice (2026-09-22 entry: "minimum display is 7 s").
- Not traced: who reads the loader's float, and the screen's exit.

**With the switch:**
- **No minimum.** The screen stays exactly as long as the load. `?loadingMs=` still forces one.
- **The number** is floor(99 x the meter) until every promise of the load has settled, then 100%, the 12 frames and the fade.
  No curve paces it.
- **The meter** (web/load-meter.js) is a plan of stages, each weighted by the time it is expected to take:
  - **Downloads** (`files`, `worldFiles`, `lazyFiles`, `eventFiles`): the bytes this load is expected to download, weighted by
    bytes over the bandwidth measured while bytes arrive (6 KB/ms until 256 KB have been seen). Done is the body bytes received
    since the plan (downloads.js `downloadProgress().total`, counted as they stream). The plan's download stages take the bytes
    in order, each up to its total. A file the manifest did not know grows the last stage by what arrived and what is still in
    flight (the downloads' known sizes), so the stage keeps moving instead of reaching its end.
  - **Work:** `unload`, `course` (loadCourse milestones; the start row's locations as they arrive), `ride`, `world` (warmWorld's
    compiles, then its frames), `rider`, `lineup`, `warm` (warmupRender: the material variants built, "31 / 351"), `intro`,
    `gc`. These are weighted by their measured times on this Mac (`STAGE_MS`). A stage without reports creeps towards 90% of
    itself over its typical time; between two reports it creeps towards the next one. Its own end only comes from the work.
  - A course switch into an event keeps a `pending` share that the event's stages take over without a jump. Another plan
    continues from the number shown.
  - The shown value eases towards the fraction and never goes back. While the fraction stands still, for example when a new
    download raised the total as fast as bytes arrived, the value keeps moving at 0.5%/s, but never more than 2% ahead of it.
- **The expected bytes:** web/load-files.json, made by `node load-files.mjs record` from cold-profile runs (an ARA1 Single
  Event after the course is in, a BRA2 Single Event after it, a new career).
  - It stores path templates: a course code becomes {course}; a rider package's files, its outfit included, form one
    template.
  - It stores sums per course for the course kind and the event kind, per rider package, and for the world (MOUNTAIN).
  - Streamed audio (.mus, speech .dat) is not counted: it is read by range as it plays.
  - main.js loads the file with a dynamic import from Press START, a 2.7 KB gzip chunk, and plans:
    - a course switch: the course or world sum, plus the event's when an event follows;
    - an event: the event sum plus five mean rider packages;
    - the lazy course: the course sum x the share left.
  - `node load-files.mjs build` refreshes the sums after an asset export. `web/test-load-files.mjs` (npm test) fails when they
    no longer match public/assets.
- **The stage line:** one line in FEFONT, in the screen's text colour at 38%, right-aligned to the frame's margin under
  "Loading..." (628, 448), so it stays off the art. The wording is plain:
  - "Downloading course 42 / 77 MB", "Downloading world", "Downloading riders";
  - "Building course", "Preparing rider(s)", "Building shaders 31 / 351", "Preparing intro", "Releasing course", "Freeing
    memory".
- **Diagnostics:** `globalThis.ssxLoadStage` (for example "warm 63%") rides on the 'stall' / 'hitch' events and on the hang
  worker's pings, so it is in 'hang' events too. A stage that lasts over 5 s sends 'load-stage' {stage, ms, percent, text} once,
  and again {ended} when it ends.
- **Off:** unchanged: the curve over the 7 s minimum, the 90% download caps, 98% until done.

## Everything under the screen (2026-10-05)

**Measured on a cold profile:** the whole first run after each load screen, with every pipeline, async pipeline, shader module,
node build, texture creation, /assets fetch and frame recorded (`local/load-meter/qa/loadprobe.mjs` RACE / RIDE, `racean.mjs`):

| run | after the screen | pipelines / node builds / textures | worst frame |
| --- | --- | --- | --- |
| Snow Jam Single Event, whole race, Chrome 1x (90% of the course, 208 s) | intro, card, race | 0 / 1 (1 ms) / 2 | under 50 ms |
| the same, WebKit, to the finish and the results (272 s) | | 0 / 0 / 1 | 53 ms |
| the same, WebKit, loadMeter on (257 s) | | 0 / 0 / 1 | **47 ms**, none over 50 |
| new career, WebKit: the arrival cut, then 150 s of ride | streamed rows | 3 sync + async; 14 builds (6 ms) at the ride start | 90 ms (the movie's first frame), none over 100 |
| the same, Chrome | | 6 sync + 69 async; 14 builds (3 ms) | under 50 ms |

The event warm-up (warmupRender: every scene drawable unculled, the encoded effects, set pieces, riders, shadows, post passes),
the intro cast's compile, rideWarm and worldWarm had already moved every first-use build under the screen.

What still happens after the screen:
- **The streamed world's next rows** (A_ARA1, A_ASS1, DRA4_A, about 10 MB) download during the arrival movie and compile with
  async pipelines (free-ride.js streamWarm). The rows the ride reaches later do the same, a few textures and pipelines a frame
  (frames 11-25 ms in WebKit).
  - These were not moved under the screen: they cause no hitch, and holding rows beyond the PS2's residency costs memory
    (phones peak near 1.2 GB).
- **Audio streams:** the music and the speech lines, read by range as they play, like the PS2's disc streams, and a few small
  sound banks when their sound first plays (FlapLoop2, Waterfall1, Cheer30, chartune: 2-60 KB).
  - The speech .dat files fetched whole after the screen (5-10 MB each) are a dev-server artefact: Vite answers a range with the
    whole file.
- **Moved under the screen (with the switch):** the card's cutscene bank (scdat_<course>) and the replay's camera triggers, which
  were fetched at the card and the race start. main.js prefetchIntro keeps them up to 120 s until taken. Neither is fetched after
  the screen any more.
- No audio is decoded with decodeAudioData (the engine decodes its own).

## Measurements (2026-10-05, this Mac, local dev server, cold profiles; load = screen open to the next screen)

| browser | load | off | loadMeter |
| --- | --- | --- | --- |
| Chrome 1x | Snow Jam, early pick | 7.6 s, 98% held 2.4 s | **4.5 s**, never still for over 0.2 s |
| | BRA2 switch + event | 7.6 s, held 2.4 s | **5.3 s**, still 0.2 s |
| | CTM world | 3.6 s, ends at 57%, cut | 4.1 s, 100% and the fade, still 0.2 s |
| Chrome 4x CPU | Snow Jam | 14.4 s, held 7.7 s | 14.4 s, held 0.3 s, still at most 1.7 s |
| | BRA2 | 13.9 s, held 8.7 s | 13.4 s, held 0.5 s, still 1.2 s |
| | CTM | 13.7 s, ends at 98%, cut | 13.8 s, 100%, still 2.0 s |
| Chrome 20 Mbit/s, 40 ms | Snow Jam | 48.5 s, held 3.7 s | 48.6 s, still 3.3 s (the course's last files, before the 2% lead) |
| | BRA2 | 35.2 s, **held 29.7 s** | 35.1 s, still 1.0 s, "Downloading course 63 / 77 MB" |
| | CTM | 48.9 s, ends at 98% | 49.3 s, still 2.9 s |
| WebKit | Snow Jam | 7.6 s, held 2.3 s | **5.8 s**, still 0.2 s |
| | BRA2 | 7.5 s, held 2.3 s | **6.2 s**, still 0.2 s |
| | CTM | 4.2 s, ends at 85%, cut | 4.8 s, 100%, still 0.3 s |

Frames under the screen and in the 5 s after are the same with the switch on and off: it adds only reports and the small
prefetches.

**Checked:**
- `web/test-load-meter.mjs`: the weights, creep and ceilings, download stages (order, overflow, in flight, bandwidth), the
  pending expansion, the bounded lead, the stage line's words, the 'load-stage' reports, no minimum, `?loadingMs=`, 99% until the
  promises settle, and the world-load finish.
- `web/test-load-files.mjs`: the templating and the sums against public/assets.
- Also passed: test-loading-screen, test-lazy-course, test-world-warm, test-downloads, test-ctm-stream, test-rider-prefetch,
  test-fe-preview(s), test-input-glyphs, test-fe-screens, line length, comment code, and `npm run online:build`.

**QA:** `local/load-meter/qa/` holds:
- `loadprobe.mjs`, with FLOW single|late|ctm|transport|auto, BROWSER chrome|webkit|firefox, THROTTLE, NET, PV, COURSE, SHORT,
  RACE, RIDE;
- `an.mjs`, `sumload.mjs` (TEXT=1: the stage lines), `racean.mjs` (first-use work after the screen), `resan.mjs` (fetches
  before / during / after), `batch2.sh`, and the runs' JSON.

**Left:**
- The manifest's world sum is the new-career start (Happiness). Other stations' rows grow the total as they arrive.
- The rider packages are counted as five mean packages until the lineup is set.
- The online lobby's load wasn't measured.
