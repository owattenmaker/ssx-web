# First load: the title screen from the first paint to "Press START button" (2026-09-26)

The first visit to the site downloads about 36 MB before the title screen can take a key (the Snow Jam world, the
animations, the front end, the core, five computer riders). Until now the page was black until the 2 MB game bundle had
arrived, the front end had loaded file by file and the GPU had started (1.2-1.9 s on a fast line, up to 22 s in the
field), and then the title card sat frozen on "Loading..." until everything was in (11.6 s on a 50 Mbit/s line).

Now the title is the first thing the page paints, and it keeps animating with a load meter until it is ready.

## What the player sees

The title is the original FE.LUI `06title`, played by `web/lui-player.js` like the other front-end screens:

- the FE blue, the white ramp at the bottom, the two pale FE_1-18 mountains, the FE_1-20 SSX 3 logo, the copyright;
- the front end's falling snow (`bg_snow_loop`, the flakes of FE_1-11, the same loop as Select Character).

While the game loads:

- the Select Character "Rider ranking" meter (`08sel_char`: the 10-cell bar widget, its black frame and cell dividers,
  the orange fill) sits above the "Press START button" line; its fill is the load (the bar widget's value,
  0x39C428 background width * value / max);
- its value text, right of the meter as "1.0" is on Select Character, shows the percentage;
- the "Press START button" line names the phase: "Loading game...", "Loading game engine...", "Loading menus...",
  "Starting graphics...", "Loading Snow Jam..." (the course's name), "Loading riders...", "Starting...".

When the game is ready the meter fades out (12 frames) and "Press START button" shows ("Press Enter" with the keyboard,
docs/input-glyphs.md). A load error shows "Load failed" in the same line.

The in-game load screen between the menus and an event (`web/loading-screen.js`, GL.LUI 110ctrl_load) is unchanged.

## How it works

| file | role |
| --- | --- |
| `web/boot-screen.js` | The boot title: bundled into a classic script at the end of `index.html`'s body, so it runs while the page parses. It paints the title on the UI canvases at once (FE blue and the white ramp; the logo, mountains, snow and FEFONT appear as their ~200 KB of pictures decode), then every animation frame until `main.js` starts its frame loop (`ssxBoot.handoff()`). From then on `ui.js` draws the title through `ssxBoot.draw()`: one screen, one renderer and one clock from the first paint to "Press START button". |
| `web/title-screen.js`, `web/title-data.js` | The title as one LUI screen: 06title + the snow loop + the meter elements (moved to `METER_AT`); the load state as overrides (meter fill and value, the phase line, the fade). |
| `web/boot-progress.js` | The load fraction (below). No imports; tested in node. |
| `web/boot-plugin.js` (vite.config.js) | Bundles the boot script (rolldown, IIFE, minified, ~13 KB; lui-player's key-cap and loading-screen/download imports cut to stubs) and inlines `window.__SSX_BOOT__`: the title screens, FEFONT metrics, course names and the manifest. Adds `<link rel=preload fetchpriority=high>` for FE_1-18, FE_1-20, FE_1-11 and FEFONT-0. Only `index.html`; nothing when the game data is absent. |
| `web/boot-files.json`, `web/boot-files.mjs` | The files the title waits for, by phase. `node boot-files.mjs record` re-records them (headless Chrome + a private Vite server); `check` verifies they exist. |
| `web/downloads.js` | `onDownloadBytes(fn)`: each game file's bytes as they stream (url, received, decoded size). |
| `web/main.js` | Reports the steps: code started (`js`), core compiled (`core`), GPU ready (`gpu`), world build (`world`, per batch of the course's world), then `ready()` and `handoff()`; `diagnose('boot', ...)` sends the timeline (script, first paint, logo, ready, ms since navigation) with the field diagnostics. |
| `web/ui.js` | `load()` loads side by side (it was one file after another); the title's pictures come from the boot. The title draws through `ssxBoot.draw()`. |

### The fraction

Every piece of work has a weight and a fraction done:

- **Files**: every file of `boot-files.json`, plus the built code bundle, its CSS and the core (from the build),
  weighted by their bytes on the wire: the gzip copy when the host serves one (`server/precompress.mjs` rules: not PNG,
  video or music, at least 16 KB, 10% smaller), else the file. `boot-plugin.js` measures them at build time (the gzip
  sizes are cached in `node_modules/.cache/ssx-boot/`). Fetched files report their bytes as they stream
  (`downloads.js`); pictures and the code bundle report when they finish (Resource Timing), and while they are in
  flight they creep towards 90% of their weight at the bandwidth measured so far, never further.
- **Steps**: work that downloads nothing, weighted by its typical time at 6 KB per ms (`BYTES_PER_MS`, about
  48 Mbit/s): the code starting (250 ms), the core compile (400), the GPU (250), the world build (2500), the computer
  riders (1100, from the `ai:<rider>:done` marks of `ai-race.js`), the last setup (500). A step that reports progress
  uses it; the others creep towards 90% of their typical time.
- The meter eases towards the total and never goes backwards. It reaches 100% only when the title is ready, and the
  last steps carry their own weight, so it does not wait at 98-99%.
- The phase line shows the first phase, in load order, with work under way (files not yet asked for do not hold it),
  held at least 400 ms.

A `?course=` deep link to another event course maps Snow Jam's course files to that course (same file names); other
worlds (the peaks) count the steps and the files they share with Snow Jam.

## Loading faster

- The core (`core-*.wasm`, 6.1 MB, 1.4 MB gzip) downloads and compiles while the front end loads; it used to start after.
- `ui.js load()`: the pictures, fonts and tables load together, then the screens side by side (Select Character first,
  whose pages and snow the other screens reuse). It was about 15 round trips one after another; on a repeat visit each
  was a revalidation.

## Measurements (2026-09-26)

Chrome for Testing 1280x720, a cold profile (first visit) and then the same profile relaunched (repeat visit). The
production build (`vite build --config server/vite.online.config.js`) is served by `web/server/mp-server.mjs` behind a
local stand-in for the edge worker: HTTP/2 over TLS, the worker's cache headers (game files `private, no-cache`,
revalidated with the ETag on every visit; hashed bundles immutable), gzip like the host. The network is throttled in
the browser (CDP). "Paint" is the first screencast frame that is not black; "longest freeze" is the longest time the
picture did not change between the first paint and "Press START button". Scripts: scratchpad `firstload/`
(`edge-sim.mjs`, `measure.mjs`, `summary.mjs`).

| network | visit | | first paint | title art | longest freeze | Press START | requests | wire |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 40 ms, 50 Mbit/s | first | before | 1.85 s (black until then) | 1.85 s | 11.6 s | 13.5 s | 235 | 36.4 MB |
| | | after | 0.12 s | 0.16 s | 0.3 s | 10.5 s | 233 | 36.4 MB |
| | repeat | before | 1.23 s (black until then) | 1.23 s | 4.5 s | 5.7 s | 229 | 0 (revalidations) |
| | | after | 0.07 s | 0.12 s | 0.3 s | 4.8 s | 229 | 0 |
| 150 ms, 6 Mbit/s | first | before | 7.5 s (black until then) | 7.5 s | 55.9 s | 63.6 s | 237 | 36.4 MB |
| | | after | 0.4 s | 0.85 s | 1.1 s | 60.1-60.4 s | 237 | 36.4 MB |
| | repeat | before | 3.8 s (black until then) | 3.8 s | 11.2 s | 15.2 s | 234 | 0 |
| | | after | 0.17 s | 0.32 s | 0.4-0.8 s | 12.7-13.1 s | 234 | 0 |

The meter never went backwards; its largest single step was 2.5% on a first visit (9-15% on a repeat visit, when a
batch of cached files completes at once); 95% was reached 0.5-3.5 s before the title was ready (no wait at 98-99%). Its
longest stretch below 1% of movement: 0.9 s on 50 Mbit/s, 2.7 s on 6 Mbit/s (a chain of small files fetched one after
another, 150 ms each).

Firefox 156 (1280x720 at DPR 2, WebDriver BiDi, local server): first paint 57 ms after navigation, logo 65 ms, ready
3.6 s, the meter monotonic, phases code / course / riders / final. Safari could not be driven (safaridriver: "session
timed out while connecting to a Safari instance", as in docs/firefox-load.md).

Field telemetry before the change (`diag.log`, 61 sessions without autostart): from the game code starting to the first
title draw, first visits p50 3.5 s / p90 15.4 s / max 22.1 s, repeat visits p50 0.9 s / p90 4.6 s / max 6.4 s (Firefox
4.6-6.4 s), on top of the time to download the code. That was the black screen. With this change the first paint no
longer waits for the code; `diagnose('boot', ...)` now reports script / paint / logo / ready times from navigation.

### Repeat visits and the edge

A repeat visit downloads nothing (the code is immutable, every game file answers 304), but the loaders ask for about
46 files one after another before the title can take a key, and every game file is revalidated (`private, no-cache`).
With the local edge stand-in adding 100 ms to each game-file request (an edge copy older than its 5 minutes, which then
waits on the tunnel to the home host), "Press START" moved from 5.1 s to 9.7 s (first paint 62 -> 78 ms, the title
animating throughout). The origin now lets the edge answer from a stale copy while it refreshes
(`CDN-Cache-Control: max-age=300, stale-while-revalidate=604800`, web/server/mp-server.mjs; docs/hosting.md), which
should bring the stale case back to the fresh one; asset deploys bump the worker's `CACHE_GEN`.

### Not done (the next levers)

- ~~**Title before the course.**~~ Done 2026-09-27 (pv `lazyCourse`, below).
- **Computer riders on the first event load** instead of at boot (~1.3 s): the event's warm-up would create them; QA
  paths (`ssxQA.start()`) that expect them after boot would need the same call.
- **The code bundle's progress** is estimated from the measured bandwidth (Resource Timing only reports when it
  finishes); the core, the game data and the pictures report real bytes.

## Title before the course (pv lazyCourse, 2026-09-27)

"Press START button" no longer waits for a course. The title waits for the front end only: the code, the core
(downloaded and compiled), the menus' screens, pictures, fonts and tables, and the GPU. The page's course (Snow Jam, or
`?course=`) and everything a race needs load behind the menus from Press START. The switch is `web/pv-flags.js`
`lazyCourse` (on; `?pv=-lazyCourse` = the old start).

### What loads when

| when | what |
| --- | --- |
| before "Press START" | the code bundle and CSS, the core (compiled, not instantiated), the front end (`boot-files.json` phase `fe`: UI / CAREER / LOADING / AUDIO / MOVIES, riders.json, courses.json), the GPU. 4.0 MB on the wire (36.2 MB before) |
| at the title | nothing more (the attract movie when it plays) |
| Press START, or the first thing that needs a course | first what the front-end rider previews need: the FE clips and samples (ANIMATIONS/library.json and samples.f32, which the course then reuses) and a core instance of their own with the course's light world (the FE lighting 0x19EE88 runs through `shade_rider_lighting`, which needs one; the course's core is not touched before its own load). Then the course, as an in-page course load without the load screen (`main.js backgroundCourse`, a job on the course-switch chain): the world and its textures, the sky, the race rider, the core's course data, the computer riders, the effects |
| the first frame with the course live | the previews light through the course's core; the preview core is dropped |
| an event, a Conquer the Mountain start, an online start | the event load screen (GL.LUI `110ctrl_load`, unchanged) covers what is left: `ui.cb.warmup` waits for the course, then the rider, the lineup, the warm-up and the intro run as before |

- **Another course first.** A course asked for while the page's course loads behind the menus (another Single Event
  course, Conquer the Mountain's peak world, an online lobby on another course, Back / Forward) abandons that load at its
  next file or world slice (`courseLoadGuard`), the load screen opens at once and the chosen course loads first; what
  the abandoned load built is released by that course's unload. The same course asked for again (its Single Event)
  waits for the running load.
- **The load screen stays honest and unchanged.** Its percentage (the original curve over the 7 s minimum, held at 98%
  until the work is done, at most 90% of the downloads in flight) is also held to 90% of the course work done while the
  course is still loading behind it: `web/boot-screen.js` keeps a second `createBootProgress` for the phases the title no
  longer waits for (`course`, `riders`; the steps `world`, `ai`, `final`), fed by the same byte counts, Resource Timing
  and `ai:*` marks, read through `ui.loading.workFraction`. Nothing new is drawn.
- **The title's meter stays honest.** With `lazyCourse` the boot manifest is split: the meter counts the phases `code`,
  `core`, `fe` and `gpu` (files by bytes on the wire; the steps `js`, `core`, `gpu`) and reaches 100% as the title
  becomes ready. `boot-files.json` was re-recorded (`node boot-files.mjs record` now forces `pv=-lazyCourse` and skips
  the optional files a course lacks); the computer riders' icons (`UI/rival-exclaim.png`) count with the riders.
- **The menus stay live.** The file loads are asynchronous, the world build runs in 4 ms slices with input and frames in
  between (a `MessageChannel` yield, no timer clamp), `_init_world_collision` and `_init_body_terrain` run as two tasks.
  What is left are single calls into the core and the computer riders' set-up: while the course builds, the menus miss
  a few frames (the longest gaps between frames in the menus: 108-142 ms in Chrome, 59-75 ms in WebKit on this Mac; on
  the local dev server, where the whole build lands in the menus at once, up to 276 ms in WebKit at the computer riders'
  start). The menus' "Loading..." note is not drawn for this load (the menus do not wait for it).
- **Under the load screen** the page draws an empty frame of the hidden scene every animation frame while the course is
  still loading behind it (`main.js feRender`). Without it, WebKit (macOS 27) kept ~12 MB/s of the page's memory for as
  long as a course loaded under the load screen with no WebGPU frame presented, and did not give it back (+0.8-1.2 GB
  by a 6 Mbit/s race start). With it the race starts at the same memory as before.
- **Unchanged:** `?course=...&autostart=1` links (the course loads before the event, as before). `?qa=1` starts the
  course at once and publishes `ssxQA` once it is live. Scripts that wait for `ui.ready` and then use the course should
  wait for the `course:live` performance mark (or add `&pv=-lazyCourse`).
- **Telemetry:** `diagnose('boot')` carries `lazy: true`; the course load behind the menus reports
  `diagnose('course', {background: true, loadMs, atMs})`; `performance.mark('course:live')`.
- **Tests:** `web/test-lazy-course.mjs` (npm test): the title before the course and no course download at the title,
  Press START loading it, the Select Character preview lit before and after the course is in, Snow Jam picked at once
  (the load screen opens at once, its percentage never backwards and <= 90% until the course is in), another course
  abandoning Snow Jam, `?qa=1`.

### Measurements (2026-09-27)

Production build (`vite build --config server/vite.online.config.js`) served by a local stand-in for the edge: HTTP/2
over TLS, the worker's cache headers (game files `private, no-cache` with ETags, hashed bundles immutable), gzip like
the host, and a shaped link (one token bucket for every response, the round trip added before each response's
headers). Chrome for Testing 1280x720 headless (the certificate trusted by its SPKI so the HTTP cache works); WebKit
= the system WebKit of macOS 27 in a WKWebView (a variant of `web/webkit-driver.swift`: its own persistent data store
per run, the local certificate trusted, occlusion detection off, the web / GPU process ids for `footprint`). First visit = empty profile / data store, repeat visit = the same one relaunched. The
player presses START as soon as it shows, spends 5 s in the menus, then picks Snow Jam (Single Event, `ui.startSingleEvent`);
"first race" = the event load screen closing (the intro next). Medians of 2-3 runs; "before" = the old start (the
same build with `?pv=-lazyCourse`, and the build before the change).

| link | browser | visit | Press START before -> after | on the wire to Press START | first race before -> after |
| --- | --- | --- | --- | --- | --- |
| 6 Mbit/s, 150 ms | Chrome | first | 60.8 s -> **6.5 s** | 36.2 -> 4.0 MB | 78.0 s -> 72.9 s |
| | | repeat | 12.2 s -> **1.7 s** | 0 -> 0 MB (revalidations) | 24.9 s -> 19.1 s |
| | WebKit | first | 61.0 s -> **6.2 s** | 36.2 -> 4.0 MB | 80.1 s -> 74.3 s |
| | | repeat (see below) | 48.8 s -> **3.7 s** | 27.0 -> 1.7 MB | 66.9 s -> 54.5 s |
| 50 Mbit/s, 40 ms | Chrome | first | 10.1 s -> **1.0 s** | 36.2 -> 4.0 MB | 22.8 s -> 14.9 s |
| | | repeat | 4.6 s -> **0.5 s** | 0 -> 0 MB | 17.2 s -> 13.6 s |
| | WebKit | first | 10.7 s -> **1.0 s** | 36.2 -> 4.0 MB | 24.4 s -> 17.2 s |
| | | repeat (see below) | 8.5 s -> **0.6 s** | 25.4 -> 1.6 MB | 21.5 s -> 15.1 s |

- WebKit repeat visits: the WKWebView's own disk cache keeps only part of the files (23-27 MB were downloaded again on
  every repeat visit, before and after), so they are between a first and a real Safari repeat visit.
- The course is live 64 s after navigation on 6 Mbit/s (~57 s after Press START); a player who picks the event at once
  waits under the load screen instead of at the title. The first race comes 4-6 s sooner there because the time in
  the menus now overlaps the download; on 50 Mbit/s it comes 4-8 s sooner.

Memory (phys_footprint of the page's web content process + the GPU process, what Activity Monitor shows; 6 Mbit/s first
visit, medians):

| browser | start | at Press START | in the menus (START + 5 s) | at the race | race + 10 s | peak |
| --- | --- | --- | --- | --- | --- | --- |
| Chrome (2 runs each) | before | 874 MB | 879 MB | 1480 MB | 1451 MB | 1539 MB |
| | lazyCourse | **567 MB** | **625 MB** | 1459 MB | 1421 MB | 1509 MB |
| WebKit (3 runs each) | before | 770 MB | 704 MB | 1484 MB | 1378 MB | 1487 MB |
| | lazyCourse | **537 MB** | **468 MB** | 1469 MB | 1451 MB | 1472 MB |

The WebKit numbers were taken with the driver asking the page for its state once a second: asking ten times a second
(`callAsyncJavaScript`) through the ~60 s of a slow load kept memory in the WebContent process by itself (+0.5-0.9 GB,
in whichever phase the driver waited on), which first looked like a difference between the two starts.

### The event's riders and the course parse (pv riderPrefetch, sharedParse, 2026-09-27)

After the course is in, an event load still downloaded its riders one chain after another: the human rider's package
(world.json, then its texture archive and geometry, then rider.json, then animation-samples.json; 1.3 s at 6 Mbit/s), then
the lineup's five riders (rider.json, then each package and texture archive; 3.2 s), then after the warm-up the intro's
cutscene scripts and animation banks (0.9 MB, 1.9 s), all competing with the load's music. `web/pv-flags.js`:

- **riderPrefetch** (on). The human rider's files start downloading when the event is picked (also on a course switch into
  an event: alongside the course). The lineup is planned before the human rider loads (`web/ai-race.js plan`), so its
  riders download alongside it. The intro's cutscene data loads under the warm-up (`web/cutscenes.js prepareData`, the
  data half of `prepare`). `downloads.js prefetchDownload(url, {keepMs})` keeps each body once until its loader takes it
  (at most 180 s here); `peekDownload` reads a prefetched world.json for its texture archives. Nothing is decoded or built
  early, so the menus and the load screen do no extra main-thread work.
  - The plan is prepare()'s roster build, moved in front of the human rider's load: one roster seed per event load. Nothing
    draws on the presentation generator in between, so the lineups are the same. Checked with the reference presentation
    seed: two Snow Jam events and a Big Air one in a row, flag off and on, gave the same lineups and the same first
    300-400 race ticks (human, all computer riders, game RNG).
- **sharedParse** (on). The human core is fed the course packages' own text (terrain.json, world_collision.json), as the
  node gates do (`web/ai-race-node.mjs`), instead of `JSON.stringify(JSON.parse(text))`. The computer riders' contexts
  pass that same text, so the first of them now copies the human's parse from the core's parse cache
  (`web/world_bridge.cpp`) instead of parsing both packages again. Before, the cache key (the text) differed.
  - This removes two of the menu hitches while the course loads behind the menus: 100-160 ms (world collision) and
    130-200 ms (body terrain). It also removes about 0.3-0.6 s of computer-rider set-up and two 1.8 MB `JSON.stringify` calls.
  - Checked: both set-ups raced the same pad on nine courses (ARA1, BRA2, CRA3, DRA4, ERA5, ASS1, DBC2, ABC1, EBC3) for
    3600 ticks. The human state, every rider's reference motion and the game RNG were bit-identical; a one-seed change of
    the pad shows at tick 228. `web/test-shared-parse.mjs` repeats it on three courses.

Measured like the table above, first visits, medians of 2. "Early" = Snow Jam picked 5 s after Press START (the course
still loading); "late" = picked after the course is in (72 s / 14 s after START).

| browser | link | pick | START -> race (off -> on) | pick -> race (off -> on) |
| --- | --- | --- | --- | --- |
| Chrome | 6 Mbit/s | early | 66.8 -> **63.5 s** | 61.2 -> **57.8 s** |
| | | late | 86.9 -> **80.3 s** | 10.4 / 18.6 -> **7.5 / 7.9 s** |
| | 50 Mbit/s | early | 14.3 -> **13.4 s** | 8.8 -> **7.9 s** |
| | | late | 22.1 -> 22.1 s | 7.6 -> 7.6 s |
| WebKit | 6 Mbit/s | early | 71.9 -> **67.7 s** | 66.8 -> **62.6 s** |
| | | late | 87.1 -> **82.7 s** | 15.0 -> **10.6 s** |
| | 50 Mbit/s | early | 18.7 -> **16.3 s** | 13.6 -> **11.3 s** |
| | | late | 22.6 -> **21.8 s** | 8.5 -> **7.7 s** |

The load screen's 7 s minimum is the floor for pick -> race when the course is already in. On 50 Mbit/s a late pick was
already at it, and on 6 Mbit/s the prefetch brings a late pick down to it (Chrome) or within 3.6 s of it (WebKit). An early
pick on 6 Mbit/s still waits for the course (~57 s after Press START). Memory at the race is unchanged within the runs'
spread (Chrome 1448-1500 MB, WebKit 1420-1604 MB either way).

**The menu hitches left.** The course's own four big calls into the core remain, each one C++ parse of a course package
and not divisible from JS: `_init_world` 55-90 ms, `_init_terrain` 130-180 ms, `_init_world_collision` 100-160 ms,
`_init_body_terrain` 130-190 ms (this Mac, busy). They already run as separate tasks. Cutting them needs a core change:
terrain.json is parsed twice (height field and body terrain), and the parses could be incremental or binary. The other
option is moving them under the event load screen, which would add ~0.5 s to a late pick's load.

**Left:** the lineup of an early pick can only be planned once the course (its computer riders) is in. The load's music
(2.1 MB for Snow Jam) and the intro actors' FE packages (fetched in the preview worker) still download in the event load.

## Front-end texture fixes (2026-09-26)

PS2 ground truth: ARMSX2 from a cold boot of the disc (`local/ps2-capture/menus/fe-texture/`, `index.json`: boot,
title, Main Menu, Options and every sub-screen, Select / Setup Character, Rider Details, Previews, the title-to-menu
transition; 36 derived savestates). The browser was captured at the same screens (Chrome, 640x480, pad glyphs) and
compared side by side and pixel by pixel (mean |PS2 - browser| per channel; snow flakes move with the clock, so no
screen reaches 0). Sheets PS2 | before | after: `local/browser-validation/fe-texture/`.

| # | bug (PS2 vs before) | fix | files |
| --- | --- | --- | --- |
| 1 | **Title** was a hand-made card: flat colour, no white ramp or mountains, the logo lower and smaller with "Press START button" over it, 14 oversized translucent flakes whose FE_1-11 rectangle took in the neighbouring icons (grey smudges beside each flake), "All rights reserved." (PS2: "All Rights Reserved."). Mean difference 42.2. | The original 06title + the FE snow loop (above). 4.8. | title-screen.js, title-data.js, tools/export_fe_menus.py |
| 2 | **Title bottom line**: the white ramp ends on line 479 of 480; the GS fills the last line (it samples pixel corners), the canvas left most of the last row blue (a blue line under the title on large screens). | The ramp ends on 480 (title only). | title-data.js |
| 3 | **Press START**: the PS2 plays a snow flake burst over the title (FE.LUI `transition`, 40 frames, a white wave at the end) before the Main Menu builds in; the port cut straight to the menu. | ui.js `leaveTitle()` plays the exported `transition` screen. | ui.js, tools/export_fe_menus.py |
| 4 | **Draw order (z-order)**: Main Menu's dark help panel (layer 7, in a layer-8 group) was drawn under the mountain (layer 7, in a layer-0 group); the PS2 draws the panel over it. Same on Previews-focused Main Menu. | LuiScreen sorts by layer, then (new) by the layer of the element's top-level group, then definition order. Checked on 20 screens: Main Menu 11.0 -> 6.0, every other screen unchanged (a flat "group first" or "max layer" rule broke Select Character and the credits). `?luiorder=flat` = the old order. | lui-player.js |
| 5 | **FEFONT glyphs squashed**: LUI text was 1/1.17 as tall as on the PS2 (glyph heights 11 / 15 / 19 px vs 13 / 17 / 22, widths equal): the glyphs keep their aspect in the 640x480 LUI frame. A 1.0-1.2 sweep of the vertical factor against the PS2 text regions is best at 640/512 : 480/448. | Vertical glyph scale x PS2_X / PS2_Y. | lui-player.js |
| 6 | **Text colour**: white LUI text peaks at 204 on the PS2 (255 in the port), the title's 37,7,5 shows as 29,5,4: the font's texels are 0xCC. | LUI text colour x 0.8. `?luitext=old` = old glyphs and colour. | lui-player.js |
| 7 | **Greyed menu rows** at alpha 96 (0.376); the PS2 frames (Main Menu Multi Play / Online on three captures, Rider Details Cheat Characters) show 0.5. | Alpha 128 in fe-main-menu, fe-options, fe-saveload, fe-event-select, audio-menu. | 5 files + docs/characters.md, docs/mobile.md |
| 8 | **Select Peak / Mode / Event**: the white ramp on the right was hidden (the executable names that shape 'whitefade', and the port took it for the screen transition). Mean 16.4 -> 7.5. | Drawn. | fe-event-select.js |
| 9 | **Enter Cheat keyboard**: the disabled Up / Down arrow keys were light grey (alpha 90); on the PS2 they are a dark arrow (the key x 0.61). | Black at alpha 100. | fe-screens.js |

Mean difference to the PS2 (before -> after): title 42.2 -> 4.8, Main Menu 11.0 -> 5.2, Options 7.6 -> 7.0, Game
Options 7.0 -> 6.4, Sound Options 8.3 -> 7.6, Controller Settings 9.3 -> 8.4, HUD 6.9 -> 6.6, Save/Load 7.6 -> 7.1,
Enter Cheat 12.3 -> 11.6, Credits 6.8 -> 6.5, Select Character 9.3 -> 8.8, Setup Character 6.3 -> 5.8, Rider Details
6.6 -> 6.0, Select Peak 16.4 -> 7.5, Previews 7.0 -> 6.4.

**Checked, not changed:**
- Firefox software sprite canvases (`sprite-canvas.js`): the title, meter and menus render the same in Firefox 156 (DPR 2)
  as in Chrome; the boot script creates the UI canvases' contexts with the same attributes as ui.js (a test checks it).
- Window sizes and DPR (390x844 @3, 1280x800 @2, 1920x1080 @1): the stage and the title scale as before. The UI canvases
  stay 640x448 and are scaled by the browser (soft on large screens, as the PS2's picture is on a TV); rendering them at
  device resolution would cost 4x the canvas work in Safari / Firefox and was not done.
- No seams along the diagonals of the gradient quads (title ramp, DPR 2, contrast-stretched).

**Left:**
- ~~Enter Cheat washed out~~ fixed 2026-09-26: one of the three dimming shapes (back_com), 12.7 -> 5.9; and ~~Select Peak
  help wrap~~ fixed: the element's own 375 / 50 % box (docs/presentation.md sections 8, 9).
- Select Character: the roster strip is one silhouette wider (Sam, a port addition), which moves the right arrow 45 px.
- Designed differences kept: "Press Enter" on the title with the keyboard, Online enabled, Options "Display & Touch",
  Save/Load Export / Import, Controller Settings "Gamepad".

**Tests:** `web/test-fe-texture.mjs` (in npm test; headless Chrome + a private Vite server): the title (FE blue, the
ramp down to the last line, the text colour and glyph height), Main Menu (the panel over the mountain, greyed rows at
0.5, text x 0.8), Options (title colour and height), Select Peak (the right-hand ramp), and with the PS2 frames present,
the mean difference of each screen under a bound. `FE_QUERY='luiorder=flat&luitext=old' node test-fe-texture.mjs`
fails as it should.
