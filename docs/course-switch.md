# Course changes in the page (2026-09-25)

Switching course, event mode or peak never reloads the page. Single Event course picks, the Conquer the Mountain world
(free ride, transport into a race, peak runs), an online lobby's course and Back / Forward all run in the page:
the original load screen shows, the old course is released, the new one is built, and the History API keeps the URL a
reload would use. The quality tier applies at run time as well.

Code: `web/main.js` (the "Course lifecycle and in-app navigation" block: `navigateCourse`, `switchCourse`,
`unloadCourse`, `loadCourse`, `onPopState`, `applyAntialias`), `web/net/mp-game.js` (lobby course), `web/loading-screen.js`
(`cancel`), `web/snow-composite.js` (`pruneEncodedEffects`), `web/fe-preview.js` (`releasePreviewCores`),
`web/lineup.js` (`settleRaced`), `web/rider-shadow.js` (dispose), `web/touch-controls.js` / `web/quality.js` (tier).

## Page vs course

| Page (made once in `init`) | Course (`loadCourse`, released by `unloadCourse`) |
| --- | --- |
| renderer, scene / camera objects, UI (menus, HUD, load screen), audio engine, input (keyboard, pad, touch), cutscene player, online session (socket, lobby seat), render loop, compiled wasm module | wasm core instance, world and sky, set pieces, terrain refinement (worker), rider model + animation core state, computer riders (rider contexts of the core) and their FX / icons / rival beam, streamed Peak 1 world (free-ride, peak-world worker, peak set pieces, Big Challenges), post passes (fog, sun flare, light glow, glare, screen tint, rider shadows), board trail / snow / wake / boost, environment + lighting, audio world (`gameAudio.leaveWorld`) |

Each course gets a **fresh core instance** (`newCore`: `WebAssembly.instantiate` from the module compiled once,
`compileCore`); every `init_*` runs on a pristine instance exactly as on a first load, so the simulation is unchanged.
The old instance is released with the objects holding it: after a GC the `WeakRef`s in `window.__coreRefs` are empty
for every earlier course (checked below). Things that held an old core and were fixed: `FrontEndPreview.core`
(cutscene actors, Select Character) and `CharacterSelect.core` (`releasePreviewCores`), `game-audio` `state.humanCore`
(cleared in `leaveWorld`), `lineup.js` `pendingRace` (the raced event, `settleRaced`), `globalThis.ssxEffects.core`
(QA handle, deleted).

The rider model is rebuilt like a first load (RIDER_SAM on the course's `initial.json`, then the chosen rider in the
event load's `ensureRider`), so every course starts from the same state as a page opened on it.

## Switch sequence

1. A callback (`ui.cb.course` / `freeRide` / `peakRun`, the lobby's course, popstate) calls
   `navigateCourse(url, {history, after, screen})`. The callbacks keep their contract: they return `false` ("the page is
   changing course"); `ui.courseReady` is a Promise that resolves when the switch is done. Requests are queued and
   coalesced: a newer one replaces a queued one; a switch in progress finishes, then the latest runs.
2. History: `push` records the screen the player leaves from in the current entry (`history.state.screen`) and pushes
   `{ssx3:1, online}` with the same query as the old reload (`course`, `rider`, `base`, `autostart=1`, `peakCourse` /
   `peakMode`; online: `course`, `rider`, `base`, `online=1`, `lobby`). Online lobby changes after the first one
   replace the entry (the lobby, not the player, moved).
3. `switchCourse`: the run stops (`stopRun`: audio `leaveWorld`, cutscene stop, rumble, input), the frame loop goes
   idle (`live=false`: `idleFrame` draws the UI and follows the screen audio, nothing touches the world), the load
   screen opens (`ui.loading.cancel(); ui.loading.open()`: its own animation frames; a CTM ride-in's world mode carries
   over), `unloadCourse`, `loadCourse`, then the boot rider state.
4. After: `autostart` = as a page loaded with `?autostart=1` (`careerUI.resume()` for a pending career / world round,
   else `ui.loadEvent` + `startRun`; the load screen then runs the warm-up, pipelines, audio prep and intro as on a
   first load); `lobby` = back to the lobby screen (a host start that arrived during the load keeps the load screen
   for its own event load: `mpGame.session.starting`); `menu` = Back / Forward.

`unloadCourse` releases, in order: Big Challenges, the streamed world (`freeRide.stop`: peak worker, locations, set
pieces, core streaming), the raced event's lineup draws, the computer riders' renderer, rival beam, terrain refinement
worker + overlays, the post passes (`dispose`), the extra area skies, then every scene object the course added
(`courseRoots`, recorded at the end of `loadCourse`), the rider, hidden world meshes and the sky scene through
`disposeRoots`: each object's `dispose` event (three frees its render objects: bindings, uniform buffers), then
materials, geometries, skeletons and textures (map slots, TSL texture nodes, `userData.warmTextures`, the world's
texture set / atlas; render-target textures stay with their owners). The disposal yields every ~8 ms so the load
screen keeps animating. Encoded-pass effects of the old scene are pruned (`pruneEncodedEffects`).

Nothing may keep the old course after unloadCourse (docs/mobile.md "Load spikes"): the held draw's set of the scene before the switch
(`acrossBefore`, pv switchGate) is pruned to what is still in the scene right after it, `worldRewarm` is dropped, and callbacks that
outlive a course (`ui.cb.standings` / `lineup`) are module-level functions, not closures made inside loadCourse (JavaScriptCore kept
the whole loadCourse scope, and the course's core, through them). pv `switchGC` (web/switch-gc.js, JavaScriptCore only) then asks for a
full collection before the new core is made while an earlier core's memory is alive, and once more under the load screen before the
ride / event starts.

## Back / Forward

`onPopState` leaves what runs (online race: DNF + leave; online lobby when the entry is not an online one; a career
session: pending round dropped, career saved, `careerMode` off; the run), then loads the entry's course if it differs
and lands on a menu, never mid-event: the screen recorded when the player left that entry (title, main, character,
setup, details; any Single Event selector screen -> Select Peak via `ui.set('event')`), else Select Peak for an event
entry (`?course=<event>&autostart=1`), else the main menu. An online entry (`&online=1` / `&lobby=`) re-enters Online.
The rider in the entry's URL is selected again.

## Online

`net/mp-game.js`: a lobby update with another course calls `switchCourse(url)` (main.js `navigateCourse(url,
{after:'lobby'})`) after `leaveRace()`; the WebSocket and the lobby seat stay (no reconnect / resume). `pendingCourse()`
is the course being loaded, so repeated lobby updates during the load do not queue it again; a `start` waits for
`courseReady()`. Checked with two Chrome clients on 5174/8787: host cycles ARA1 -> BRA2 -> BHP1, both clients switch in
~2.3 s, same client ids and slots, one `/mp` socket each for the whole session, then a race on BHP1 (both racing,
packets flowing).

## Quality tier

`setQuality({tier})` (touch settings panel) applies render scale, upscale, fps and frame-time cap at once (as before),
and `applyAntialias` sets `renderer._samples` (4 / 0; 0 in WebGPU compatibility mode): the canvas and the fog-renderer
pass targets follow `renderer.samples` on the next frame. If the course was already warmed, the warm-up runs again at
once so the pipelines for the new sample count are built then, not on later race frames (it draws the scene slices
behind the pause / settings panel for ~1.5 s). No reload, no renderer re-create.

## Measurements

Chrome for Testing (WebGPU, 900x660, dev server, other agents' tests running), Single Event from the menus, Metro-City
and Snow Jam alternating, 6 switches (`scratchpad/nav/flow5.mjs`, memory after `HeapProfiler.collectGarbage`, 4 s into
each race):

| | per switch |
| --- | --- |
| unload | 9-51 ms (BRA2: 4,238 objects, 4,151 geometries, 371 materials, 381 textures released) |
| course load | 1.9-2.6 s (one 3.4 s) |
| warm-up (under the load screen) | 1.3-1.6 s |
| click -> objectives card | 14.7-14.9 s (7 s load-screen minimum + intro cutscene; 7.7-7.8 s with `?cutscenes=0`) |
| long tasks during a switch | 5-6, longest 170-274 ms (a first load of the same event: 7, longest 177-182 ms) |

GPU objects at the same point of each race (`renderer.info.memory`, `?cutscenes=0`): Metro-City geometries
2745 / 2746 / 2747, textures 490 / 492 / 494, uniform buffers 5430 / 5434 / 5437, render objects 4129 / 4131 / 4133;
Snow Jam 3484 / 3485 / 3485, 497 / 499 / 499, 8457 / 8460 / 8460, 6549 / 6551 / 6551: flat. With cutscenes on, the
cutscene actor pool (up to 8 idle FE preview models kept for the next event, `cutscenes.js trimPool`) adds its models
until it is full. Old cores: all released after GC. JS heap after GC at the same point: Metro-City 174 / 189 / 186 MB,
Snow Jam 201 / 207 / 200 MB (flat). Before the last two fixes it grew ~25 MB per event: `game-audio.js` kept every song
file it had played in its byte cache (a page reload per course used to drop it; now `leaveWorld` evicts `b:music/`).

Peak 1 (free ride <-> Peak 1 Race, 3 cycles, `scratchpad/nav/peakloop.mjs`, `?cutscenes=0`): Peak 1 Race geometries
3711 / 3711 / 3711, textures 282 / 284 / 286; free ride 1581 / 1582, 359 / 362. Found on the way: `free-ride.js stop()`
disposed the resident locations with `disposeGroup` only (their node-material textures and light atlas stayed, ~330
textures per world); it now uses `releaseLocation` like the streaming release, and `unloadCourse` disposes every object
the course had before the modules detach parts of it (the peak set pieces' particle / halo meshes and textures).

Safari 26 (safaridriver, WebGPU, trusted pointer clicks so the pushed entries carry user activation; WebKit skips
entries pushed without it on Back), 5 switches (`scratchpad/nav/safari-nav.mjs`): unload 16-54 ms, load 1.86-1.99 s,
warm-up 4.3-5.7 s, click -> objectives 16.1-17.3 s, load-screen frames p50 17 ms, p95 43-55 ms (longest 338-578 ms);
GPU objects as in Chrome (Metro-City geometries 2733 / 2862 / 2884 with the cutscene pool filling); WebContent
`phys_footprint` at the menu after each event 2504 / 2525 / 2061 / 2238 / 2014 MB (1214 MB at boot); the old cores'
WeakRefs clear as JSC collects. Free ride PEAK1: load 2.9 s; Peak 1 Race from it: 2.8 s to the objectives card (the
race start after Continue checked in Chrome; Safari runs need an unlocked screen: a hidden page gets no animation frames
and the load waits). Online with a Chrome host and a
Safari guest: ARA1 -> BRA2 2.6 s, -> BHP1 0.6 s, same client id, no new WebSocket, then a race on BHP1.

Audio failures are contained (`main.js audioSafe`): a song whose stream uses codec 4 (microtalk) made `pathfinder.js`
throw on every tick, which stopped the frame (the race froze on the give-up / results path). The game now goes on and
logs it once; the decoder gap itself is the audio module's.

## Warms that outlive their course (pv `compileAbort`, 2026-09-28)

**Cause.** three r186 `Renderer.compileAsync` has two phases:
- It lists the scene's render items synchronously.
- It then builds them one at a time with `yieldToMain()` between items. Each item is built against the render context
  captured at the call.

The world warms queue thousands of items: `free-ride.js warmSliced`, cutscene `host.compile` and the ride's rider, all
through `fog-renderer.js compileObject`. `unloadCourse` waited for the free-ride warms for at most 1.5 s (pv
`bootChain` settle). On WebKit they were often still building then, so the remaining items ran after the course was
disposed:
- They uploaded the disposed geometry, bindings and textures again, and nothing disposed them a second time.
- They held pipelines.
- Their pipelines failed on the old world pass's destroyed depth-stencil texture: "Async render pipeline creation
  failed ... GPUDepthStencilState.format is required". Traced with hooks on `getCurrentDepthStencilFormat` and
  `backend.destroyTexture`: PEAK1's world-pass depth, compiled from the item loop after the switch back to the event.

Chrome usually finishes the warms inside the settle, so it showed neither the growth nor the errors.

**Fix.** `web/compile-abort.js`:
- `trackCompiles(renderer)` is installed by `createFogRenderer`. It wraps `compileAsync` and captures each call's item
  list: the array three assigns to `renderer._compilationPromises` during the synchronous part.
- `unloadCourse` calls `abortCompiles(1000)` first, before `freeRide.stop()`. This empties every list still being
  built, so three's `for...of` ends after the item in flight, and it waits for that item. A call made during that wait
  (a free-ride slice before its stop) builds nothing.
- The next course's warms are not touched.

The synchronous part only lists descriptors, so a dropped item leaves no state. An object whose compile was dropped
builds its pipeline on its first draw, as three always does for an object not compiled: a build hitch at worst, never a
missing draw. All dropped items belong to the course being disposed.

**Measured.** WebKit (the shared driver, `?cutscenes=1`), 25 loads: EBA3 event, then 6 cycles of PEAK1 free ride
(`cb.freeRide(17)`) -> EBA3 -> MOUNTAIN peak run (`cb.peakRun(7)`) -> EBA3. Page polled once per 3 s. WebContent
`phys_footprint` (vmmap) and three's live textures, sampled 6 s after each load (`scratchpad/enc/memwk.mjs`, kept
under `local/browser-validation/blend-space/hang/`):

| | WebContent footprint at the event, loads 2 / 10 / 18 / 24 | live textures, load 2 -> 24 | depth-format errors |
| --- | --- | --- | --- |
| off | 1229 / 1536 / 1843 / 1741 MB | 245 -> 277 | 2-3 per event return (11 of 12 runs) |
| on | 832 / 1229 / 1126 / 1024 MB | 245 -> 250 | 0 (8 runs) |

- With the old 4-per-second polling, the same pair came out at 1229 -> 2253 MB (off) and 1126 -> 967 MB (on).
- On, 37 warm calls were dropped over the 25 loads, 5,404 items.
- Load times are unchanged: 8-15 s per switch in both.
- Chrome, on: textures flat (243), no errors.

**Left:**
- One DataTexture per peak-world visit (created by an `updateBindings` in a warm during the free ride) is never
  destroyed.
- three's pipeline cache grows about 9 entries per cycle with the switch on or off. The GPU process footprint stays
  flat (280-470 MB).

## The course being built runs nothing (pv `switchGate`, on since 2026-09-29)

**Owen's freeze** (host diag.log session t93ez0j6, Safari 27, 2026-09-29): BRA2 event, results, post-event Transport,
MOUNTAIN/18 loaded (`course`, then `screen` -> game at t=1141.9), then nothing: no stall report, no heartbeat, no pagehide.
The page never ran another task.

**Cause.** During an in-page course switch the page's global `core` is the new course's wasm instance from the start of
`loadCourse` (`core = await newCore()`), while its init runs in slices across frames (pv eventSlices / sliceLoad). The
Transport's held loop keeps drawing across the switch (`cutscenes.js acrossSwitch`, every animation frame while the main loop
is idle), and each of its frames:
- lit the actors through `host.core`: `fe-preview.js light()` -> `_malloc` x4 (new pointers for the new core),
  `_rider_lighting_info`, `_reset_rider_lighting`, `_shade_rider_lighting`;
- ran the fade's painter reset `host.core._weather_fade_reset` (`fadePainterReset`);
- drew the whole scene with `renderer.render(scene, camera)`, so the new course's objects drew (and ran their render hooks)
  as `loadCourse` added them: set-piece fog puffs asked `resident()` before `chunkResident` existed (TDZ, every Transport;
  the coordinator moved that declaration up), and ~25 canvas pipelines per switch were built for objects nobody sees.

All of it on a core whose init had not run yet. Counted with each new core's exports wrapped (`scratchpad fz/corewrap.js`):
97-247 lighting calls per Transport in WebKit (MOUNTAIN and per-peak PEAK1 alike), 145-1498 in Chrome. In WebKit 3 of about 12
Transport loads crashed inside the new core: "Out of bounds call_indirect" in `_init_environment` / `_reset_rider`, "Out of
bounds memory access" in `_init_animation`; the fallback ARA1 load was hit the same way and the page ended on the title.
A corruption that does not trap at once can leave the core looping forever over corrupted data: no frame, no timer, no
pagehide, no report, as in Owen's session (his 22:51 PDT bundle has the same code). Every Transport that changes world is
exposed (post-event returns to a station or peak, "Go to this peak now?" into another peak world); crossWorld and plain
event loads stop the cutscene (`stopRun`) and are not.

**Fix** (main.js):
- `coreLoading` is set from `newCore()` to `live = true` at the end of `loadCourse`.
- The cutscene host's `core` getter returns null while it is set (as it already was during the unload, after `core = null`),
  and `freeRideCourse()` returns -1. The actors keep their last lighting for the load's seconds; the fade reset waits.
- The held loop's own draw (`host.render` -> `renderAcross`) leaves out the scene children the switch added
  (`acrossBefore`, recorded when the switch starts); the list's TRANSP set, actors, skies and alpha fill still draw.
- QA: `window.__coreLoading` with `?qa`.

**Checked:** switchGate on, 0 calls into a new core during its `loadCourse`: WebKit 3 of 3 Transports (MOUNTAIN), Chrome 6 of
6 (3 MOUNTAIN, 3 PEAK1 with -mountainRide); every load fine. The few calls left come after `loadCourse` (under rideWarm,
the core complete). Tests: presentation, cutscenes, ctm-flow, ride-warm, lazy-course, ctm-stream, gpu-recovery.

**Open:** `loadCourse` could keep the new instance in a local until its init completes and assign the global only then (the
most robust form), but its own helpers (resetPhysics, bootRider, the animation, free-ride, set-piece and AI setup) read the
global; that refactor is left for a separate, test-gated change. hangWatch cannot report a wasm infinite loop in WebKit (its
worker's I/O goes through the frozen main thread); a Service Worker could (it has its own thread and network), not built.

## World arrivals warm under the load screen (pv `worldWarm`, off)

**What the first frames built.** A render-object / pipeline creation log (three's `RenderObjects.createRenderObject`,
`Pipelines._getRenderPipeline` wrapped; `scratchpad fz/buildlog.js`) in WebKit, MOUNTAIN from the menus: after the load screen
closed, the first frame made 2898 render objects (A, A_ASS1, A_ABA1, A_ARA1) and 28 pipelines synchronously (sky background,
post quad, terrain sparkle, set-piece particles / halos, rider parts): Owen's "3051 bufs, 7012 binds, 20 pipes" stall. A
Transport arrival: ~800 render objects, 15 pipelines.

**Why the compile had not done it.**
- `loadCourse` fires `freeRide.rewarm()` (pv streamWarm) and never waits for it.
- three r186 `yieldToMain()` is `scheduler.yield()` in Chrome but a whole `requestAnimationFrame` elsewhere, and
  `compileAsync` yields after every item, the async node build after every shader stage. So in WebKit the rewarm of a
  start row (thousands of meshes) advanced one mesh per frame: 30-50 s.
- A compile lists a transparent DoubleSide material in one pass; its draw makes a second, BackSide render object (passId
  'backSide'), which only a draw creates.
- Effects of the snow composite's encoded pass (layer 1, 1-sample target) never match a world-pass compile.

**The switch (main.js, free-ride.js, cutscenes.js, web/yield-shim.js):**
- `web/yield-shim.js` `installYieldShim()` (main.js init): where `scheduler.yield` is missing, a yield continues as the next
  task (MessageChannel) until `yieldBudget.ms` of wall time since the last frame wait, then waits for an animation frame.
  4 ms a frame normally, 12 under an opaque load screen, 8 under the Transport's visible held loop. Chrome keeps its own.
- `free-ride.js rewarm` compiles the start row's locations first (`rewarmLead`, `rewarmLeadCodes`), then the rest; a
  two-pass material gets a third, DoubleSide pass after its BackSide and FrontSide ones: listed as the draw lists it, it
  creates the 'backSide' render object (and bindings), and the draw's pipeline lookup finds the BackSide build.
- `switchCourse` -> `warmWorld(job, rideJob)` for a career world arrival (not a peak run), started alongside rideWarm:
  - compiles everything else the world pass draws (courseRoots outside the locations and the rider, world layer) in 8 calls
    that build side by side (`compileFor`: fog-renderer compileObject, unculled; per side only when nothing shows the frame);
  - waits (at most 30 s) for those, the lead rewarm and the rider;
  - behind the opaque load screen, then the race warm-up's frames (`warmupRender` with `skip` = the locations, left as they
    are, and a new-variant cap of 60 a slice) for what only a draw builds: the sky pass, post passes, rider shadows, the
    encoded effects. Under the Transport's held loop (world mode, the frame visible): no frames.
- Cutscene sets are compiled before they join the scene (`host.compileHidden`) and actors per side (`host.compile`).
- Event loads are unchanged (a per-slice async compile was tried: 2.8 -> 5.4-7.5 s of warm-up for BRA2, dropped).
- The simulation is untouched (nothing ticks); the warm frames draw only behind the opaque load screen.

**Measured** (WebKit through the shared driver, 1600x1000, audio running and silent, load ~34 from other agents; load = load
screen open -> ride / intro; base 4 runs, on 2 runs of the final code; scratch `fz/owen2.mjs`, `report.py`, `arr2.py`):

| | load, off -> on | after the load screen, off -> on |
| --- | --- | --- |
| first MOUNTAIN arrival (menu -> career) | 3.8-5.0 -> 6.2-8.2 s | 400-520 ms hitch every run (2897 render objects, 28 blocking pipelines) -> none (0 render objects, 1 pipeline), worst frame 406-518 -> 69-111 ms |
| Transport arrival (BRA2 results -> MOUNTAIN/18) | 3.7-4.8 -> 4.1-5.1 s | 800 render objects / 15 pipelines -> 27 / 11, worst frame 96-125 -> 52-70 ms |
| BRA2 event load | 7.5-8.5 -> 7.6 s | unchanged |

On the first arrival the warm is 3.5-4.5 s of compile (overlapping the rider's 1.4 s) and 0.5 s of frames, with one
250 ms frame under the load screen (the post passes and the sky). Cutscenes: the lodge-door cut's blocking pipelines 5 -> 1.

**Checked:** `web/test-world-warm.mjs` (headless Chrome, in test:all): a new career off vs on, 300 ticks identical, the frame
after them 0 pixels different (off vs off differs by up to 41 pixels, 6 levels: the rider's real-time interpolation). The
test uses the desktop tier: on the auto fps tier the frame gate drops to 30 fps after heavy frames, and with the test's
frozen animation-frame clock it then never draws again.

**Left:** the Transport's held loop draws straight to the canvas (`acrossSwitch`), a context nothing compiles for (~20
blocking pipelines for the TRANSP set, actors, sky and alpha fill at the switch); the Transport arrival's last 11 pipelines
(encoded-pass effects, sky, post quad) are built by its first frame; the character select's FE model builds.

## Gaps

- Save import (Options > Save/Load, `web/fe-saveload.js`) still reloads the page: a dozen modules read their part of
  the save only at start-up (career, wardrobe, options, selection, audio, touch, quality, ...); an in-page re-read would
  need each of them to reload its state.
- Back / Forward always land on a menu (never resume an event mid-way), and leave a career session to the main menu.
- `peakRun` / `freeRide` URLs: `selectFreeRide` now drops a stale `peakMode` (a peak-run URL followed by free ride kept
  it before, so the free ride would have loaded the peak run's start).
