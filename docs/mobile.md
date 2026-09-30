# Phones and tablets (mobile browser)

iPhone Safari first, Android Chrome second. Portrait is the main layout: the PS2's 4:3 frame at the top in a
console-style case, a DualShock-style control deck under it. Landscape keeps the frame centred with the controls at
its sides. Nothing in this layer touches the simulation: touch controls feed the same pad channels as the keyboard
and gamepad, and the quality tier only changes presentation (drawing-buffer size, MSAA, frame pacing).

Files:

| file | what |
| --- | --- |
| `web/touch-controls.js` | deck DOM, multi-touch, layout engine (`computeLayout`, `CRAMPED`), menu key synthesis, ≡ -> the game's Options, fullscreen, prompts, haptics |
| `web/fe-options.js` | Options > Display & Touch (`displayScreen`, `DISPLAY_ROWS`, `optionsWithDisplay`): the settings as original option rows |
| `web/phone-prompts.js` | the Add to Home Screen hint and the turn-your-device card as the game's own message box |
| `web/mobile.css` | the look (console case, bezel, DualShock deck: grey d-pad, black sticks, coloured face symbols, grey shoulders), touch-mode page rules |
| `web/quality.js` | device class, tiers, render scale, upscaling, 30/60 fps presentation, low-tier frame-time cap |
| `web/test-touch-controls.mjs` | touch -> pad channels == keyboard/DualShock, multi-touch chords, layouts, quality sizes (in `npm test`) |
| `web/manifest.webmanifest`, `web/mobile-icon-180.png` | home-screen app (display fullscreen/standalone) |
| `web/index.html` | viewport (`viewport-fit=cover`, no user scaling), apple-mobile-web-app metas, manifest, `mobile.css`; the old `#touch` buttons are gone |

main.js hooks (small, marked with comments): `inputs()` adds `touchControls.held(code)` to the key test and passes
the gamepad through `touchControls.pad()`; `clearInput()` releases the deck; `layoutStage()` sizes the renderer with
`quality.sizeRenderer`; the renderer's `antialias` comes from the tier; the animation loop goes through
`frameGate(quality.fps)`; the fixed clock gets `min(dt, quality.maxFrameDt)` (offline only); a `ResizeObserver` on
`#stage` relayouts when the deck appears. Leftover `$('#touch')` writes in `startRun`/results were removed (the
results one threw once the element was gone).

## Controls

Layout (portrait, "Shoulders on top" = default; "All below" puts the shoulder row under the frame):

```
 [L1] [L2]  (≡) (⛶)  [R2] [R1]      <- shoulder strip (Options, fullscreen in the middle)
 ┌──────────────────────────────┐
 │        4:3 game frame        │
 └──────────────────────────────┘
      SELECT  (•)ANALOG  START
   ┌─d-pad─┐            (R stick)
   └───────┘               △
   (L stick)            □     ○
                           ✕
```

| deck control | PS2 control | how it reaches the core |
| --- | --- | --- |
| left stick (floating: re-centres where the thumb lands in the lower-left zone; "Fixed" in settings) | left stick | virtual standard gamepad axes 0/1 |
| right stick | right stick (BoardPress / BoardPivot) | gamepad axes 2/3 |
| d-pad (8-way, diagonals press two arms) | D-pad (spins/flips in the air, prewind spin) | keys I/J/K/L |
| ✕ □ ○ △ | Cross (jump, hold = crouch), Square (boost/tweak/recover), Circle (handplant), Triangle | Space / Shift / C / Y |
| L1 L2 R1 R2 | shoulders (grabs, the 15 chords, attacks, Uber chords) | Q / Z / E / X |
| SELECT | Select (reset to course) | Backspace |
| START | Start (pause/resume) | gamepad button 9 |

Those are exactly the codes `pad-input.js` maps (KEYBOARD_BUTTONS; IJKL is the D-pad in both keyboard modes), so
`buildPad()` gives identical channels. Stick response: dead zone 0.12 of the stick radius, then the travel is spread
over |value| 0.39..1 — the part of the range past the original input stage's own dead zone (bytes 79..176,
0x327210) — so the rider responds as soon as the thumb leaves the dead zone; a DualShock deflected to the same value
gives the same channels. Multi-touch: one pointer capture per finger; each finger owns the controls it presses (a
thumb rolled between ✕ and □ presses both; sliding across the d-pad changes direction); two fingers on one button
count; every press lasts at least 70 ms so a tap shorter than a frame still reaches `frame()`'s pad read.

Menus: while any screen other than the race is up, the deck also sends the keys the front end reads: d-pad/stick =
arrows (held = repeat), ✕ = Space (choose), △ = Escape (back), □ = Shift, ○ = Backspace, L1/R1/L2/R2 = Q/E/Z/X,
START = Enter when no run is going (during a run Start stays the pad's Start, which pauses/resumes). Tapping the
menu items in the frame still works (the `#game-menu` overlay buttons). The "Basic Controls" loading screen shows
the pad legend while the deck is up (`ui.loading.forceDevice`).

Show/hide: the deck appears on `(pointer:coarse)` or the first touch, and hides on a real key press or gamepad input
("Touch controls: Auto"; "Always"/"Off" in Options > Display & Touch).

Control hints: while the deck is up (or after a touch) every hint shows the PS2 button icons the deck has; a real key
press switches them to keyboard key caps and a gamepad back to the icons (`web/input-glyphs.js`, docs/input-glyphs.md).

## Settings: Options > Display & Touch (2026-09-25)

No HTML panel any more (user: additions must fit the game). The settings are original-style option rows in the
game's own Options:

- **Options** (18options) has one more item above DONE, "Display & Touch" (`fe-options.js optionsWithDisplay`: the
  Credits item's look and help slot, DONE and its focus state move down a row; its focus state is frame 72).
- **Display & Touch** (`fe-display`, `fe-options.js displayScreen`) is Game Options (19game_opt) with its rows
  replaced: same background, title slot, label/value columns, focus bar, left/right arrows, help line and legend
  (Previous / Reset options). Rows (`DISPLAY_ROWS`): Resolution (PS2 640x448 / PS2 512x448 / Screen 1x / Screen full),
  Upscaling (Smooth / Sharp), Frame rate (Auto / 60 / 30), Quality (Low / Medium / High), Touch controls (Auto /
  Always / Off), Touch layout (Shoulders on top / All below), Touch stick (Floating / Fixed), Touch vibration (On /
  Off). Left/Right or Cross cycles a value, Square resets to the detected tier and the touch defaults.
- Values apply at once and are saved in their own keys (`ssx3.quality`, `ssx3.touch`), not in the "save your
  Options?" question (like Widescreen). A Quality change asks first with the game's Yes/No box ("Rebuild the graphics
  for this quality?"; Yes = `quality.js setQuality({tier})`, applied in the running page by `main.js applyAntialias`,
  which re-warms the pipelines, docs/course-switch.md "Quality tier"; no reload). Touch controls Off while the deck is
  up asks too ("Hide the touch controller?", No focused). Reset asks when the tier changes.
- Keyboard / gamepad players see the Display rows live and Touch layout / stick / vibration greyed (alpha 128, like the
  original's unavailable items) while the deck is not up; Touch controls stays live (Always turns the deck on).
- **≡ on the deck** (`touch-controls.js openSettings`): in a race it pauses (`pause()` callback) and opens Display &
  Touch over the pause menu; Triangle goes back to that pause menu, START resumes as usual. On menus it opens Options
  (as Square does); inside Options it opens Display & Touch; on Display & Touch it goes back. Nothing opens on the
  loading screen, in a cutscene, or during an online race (the deck cannot pause those).
- The deck drives the screens like any menu (d-pad / stick = arrows, ✕ select, △ back, □ reset).
- Fix on the way: a whitefade started on a screen another module draws (Square on Main Menu / Select Character, ≡ on
  the title or a pause menu) never finished, because only `FeScreens.draw` resolved it; `web/ui.js drawStreamingNote`
  (called after every frame's UI) now calls `FeScreens.drawForeignFlash`, which draws the fade there and switches.

## Prompts (web/phone-prompts.js)

The Add to Home Screen / full screen hint and the turn-your-device card are the original FE Yes/No popup box (FE.LUI
`popup`: the blue panel, burnt-orange frame and dark shadow shapes, stretched to the message as `drawPrompt` narrows
it), black FEFONT message, one white option with the Cross icon ("OK" / "Continue"). They are drawn on their own
canvas (`#tc-prompt`, above the page) and load what they need themselves (the popup and snow screens from
`UI/character-select.json`, FEFONT, `FE_1-14` for the Cross, `FE_1-11` for the snowflakes; or the running UI's copies
when it has loaded), so they work before the UI, the 3D or a course have loaded. Cross (deck ✕, Space/Enter),
Triangle or a tap closes them; while one is up it swallows the menu keys.

- **Home**: the ⛶ button where the Fullscreen API is missing (iPhone Safari) or refused; the box sits over the game
  frame with the page dimmed. iPhone/iPad text: "For full screen, tap Share, then Add to Home Screen, and start SSX 3
  from the new icon."
- **Turn your device**: when the current orientation is too small to play (`computeLayout(...).cramped`: frame under
  150 px, a portrait deck under 200 px, or a landscape view under 260 px) and the other orientation fits; full-screen
  FE background (sky blue with the FE's drifting snowflakes), a device outline turning to the wanted orientation above
  the box. It goes away when the device turns; Continue keeps the current orientation for the visit. Phones in either
  orientation are well above the limits, so in practice it shows for split-screen / tiny windows.
- QA: `?phonePrompt=home|rotate` shows one at once (rotate stays until closed).

Haptics: `navigator.vibrate(8)` per press (Android). iOS has no vibrate API; iOS 18+ gives a tick when a
`<input type=checkbox switch>` toggles, which the deck clicks per press (unverified without a device).

## Fighting the browser

- `touch-action:none`, `overscroll-behavior:none`, no selection/callout/tap highlight while the deck is up; the deck
  `preventDefault`s its touch events; `gesturestart` (iOS pinch) and `dblclick` are cancelled; multi-finger
  `touchmove` on the page is cancelled. Viewport: `maximum-scale=1,user-scalable=no,viewport-fit=cover` (iOS
  ignores the zoom part; `touch-action` does the work there). Taps on the frame's menu buttons still click (no
  `preventDefault` on the stage).
- Layout uses `visualViewport` size and the `env(safe-area-inset-*)` values (measured with a probe element), relaid
  out on resize, `visualViewport` resize, orientation change and widescreen-mode change; the page body is
  `position:fixed` so nothing scrolls or rubber-bands. iOS 26 Safari's floating toolbar overlaps the bottom of the
  deck background only (controls stay above it).
- Fullscreen button: Fullscreen API where it exists (Android Chrome, iPad), then `screen.orientation.lock` to the
  current orientation. iPhone Safari has no element fullscreen: the button shows the "Share -> Add to Home Screen"
  message box ("Prompts"); the manifest (`display: fullscreen`, `standalone` fallback) and `apple-mobile-web-app-capable` make the
  home-screen app full screen. The manifest link is `crossorigin="use-credentials"` (password gate); the server
  now sends `.webmanifest` as `application/manifest+json`.
- iOS edge-swipe back cannot be disabled in Safari; the home-screen app has none. Both orientations are supported;
  the turn-your-device card only appears when an orientation has no room ("Prompts" above).
- Audio: `audio-engine.js` already unlocks on the first `pointerdown`/`touchend`; the deck also sets
  `navigator.audioSession.type = 'playback'` (Safari 17+) so the silent switch does not mute the game.
- Background: the existing `visibilitychange` / `blur` handlers pause a run.

## Quality tiers (web/quality.js)

Detected: phone (touch, short side <= 540 px) = low; tablet = medium (low with <= 4 GB / <= 4 cores); desktop =
high. `?quality=low|medium|high` overrides, also `?renderScale=native|native512|css|full`, `?upscale=`,
`?fps=auto|30|60`, `?aa=0|1`. A tier chosen in Options > Display & Touch applies in the running page (render scale, frame pacing,
tick cap and MSAA; docs/course-switch.md).

2026-09-25 (docs/sim-performance.md): the simulation itself got ~2.6x cheaper (exact), and the low tier adds
`presentationFast` (`?fastfx=0|1`): the skin palettes and snow sprites use plain float arithmetic (presentation only;
the simulation is bit-identical). Chrome 390x844 at 4x throttle: 9.0-9.6 -> 16.4-19.7 fps (13.9-16.4 with `?fastfx=0`),
36-39 -> 58-60 game ticks/s (a run under heavier machine load: 7-8 -> 12-14 fps).

| tier | render scale | MSAA | frame cap | frame-time cap |
| --- | --- | --- | --- | --- |
| low | native: 640x448 (640x336 in the 16:9 band), CSS-stretched to the frame | off | auto: 60, drops to 30 when a drawn frame's main-thread work has a median > 13 ms over 45 frames, back at < 7 ms (60/30 fixed in settings) | 4 ticks per drawn frame |
| medium | 1x CSS pixels | on | 60 | none |
| high (desktop, unchanged) | devicePixelRatio capped at 1.5 | on | 60 | none |

Render scales: `native` (the PS2 frame buffer; the frame is 4:3 so its pixels are non-square like on a TV),
`native512` (the 512x448 viewport the PS2's 3D actually renders), `css`, `full`. Upscaling smooth (bilinear,
default) or pixelated (`image-rendering`). The HUD canvas is always 640x448. The fog/snow composite, glare, sun flare
and light glow work in PS2 viewport units or at the pass's own target size, so they need no change at 640x448 (checked
visually in the simulator and Chrome, all widescreen modes).

30 fps presentation draws every other animation frame; the fixed clock then runs two 60 Hz ticks per drawn frame
(same ticks, same per-tick inputs). Low tier caps one drawn frame at 4 ticks of game time: the fixed clock keeps debt
so a slow frame never loses game time, which on a device that cannot sustain 60 ticks/s becomes a catch-up spiral
(12 ticks per frame, ~5 fps at 4x CPU throttling with six riders); with the cap the game runs slower than real time
instead. Online races (`mpGame.pace`) are untouched.

## Measurements

Chrome for Testing, iPhone emulation (390x844, DPR 3, touch), Snow Jam race with 5 computer riders, holding tuck,
20 s windows, 2+ runs each; the Mac was loaded by other agents (load average ~7), so single numbers are +-15%.
Scripts: `scratchpad/mobile/perf.mjs` (frame interval, rAF-callback time, GPU-bound share = frames > 20 ms whose
main-thread work was < 70% of the interval, JS heap, wasm, process RSS), `profsplit.mjs` (CPU profile split).

Frame time, 4x CPU throttling (a pessimistic phone; recent iPhones are closer to 1-2x of this Mac):

| config | fps | interval p50 / p95 ms | callback p50 / p95 ms | GPU-bound |
| --- | --- | --- | --- | --- |
| before (`quality=high`: 585x439 buffer, MSAA, no tick cap) | 4.3-5.2 | 225-283 / 257-316 | 224-275 / 252-312 | 0 |
| after (low tier: 640x448, no MSAA, 4-tick cap) | 9.7-12.1 | 107-117 / 117-142 | 103-115 / 113-137 | 0-0.01 |
| low tier, `ai=0` (no computer riders) | 33-38 | 27-33 / 35-43 | 28-32 / 33-40 | 0.01 |
| 2x throttle, high + native buffer | 36 | 32 / 41 | 30 / 37 | 0 |
| no throttle, low / high (120 Hz display) | 129 / 129 | 8.3 / 9.2 | 8.4 / 7.8 | 0 |

The "before" row is the catch-up spiral: the clock kept its debt, so every frame ran 12 ticks. The tick cap turns
that into a slower game clock at ~10 fps instead of ~5 fps; no presentation setting helps beyond that because the
simulation is the cost.

Render scale (full vs 1x vs native), same race:

| render scale | buffer | 4x throttle, WebGPU: interval p50 / p95 | SwiftShader (software GPU): interval p50 / p95, GPU-bound |
| --- | --- | --- | --- |
| full (DPR 1.5) | 585x439 | 108-183 / 124-249 | 109 / 150, 0.04 |
| 1x | 390x293 | 108-209 / 118-250 | 108 / 142, 0.02 |
| native | 640x448 | 107-135 / 123-167 | 108 / 158, 0.06 |
| native512 | 512x448 | 209 / 259 (one noisy batch) | - |

No measurable difference: the frame is CPU-bound (simulation) even on a software GPU; the phone-sized frame is only
~0.2-0.3 Mpx whatever the scale. The GPU only becomes the limit without computer riders on SwiftShader (`ai=0`:
interval 107 ms, callback 46 ms, GPU-bound 1.0). Native is kept as the phone default because it is the PS2's
picture, costs the same, and drops the 3 x 8 MB rgba16float MSAA targets. In the iOS Simulator all four scales run;
smooth upscaling looks like the PS2 on a TV, pixelated shows uneven pixel sizes at the non-integer 1.8x stretch, so
smooth is the default.

Where the time goes (CPU profile, 4x, low tier, 23.5 s): simulation 67.6% (core wasm 60%), everything else in the
frame 24.9%, idle/other 7%.
- Simulation, by wasm entry: computer riders' FX pass `_fx_pass` 12.8%, their `_animation_pose` 10.5%, the
  per-tick bone palette capture `_rider_skin_palette` (rider-skinning.js capture, all riders) 10.4%,
  `_animation_post` 5.3%, their `_step_rider` 4.8%, `_race_begin` 4.1%; the human rider's `_animation_tick` 4.7%,
  camera 1.4%, `_step_rider` 1.2%.
- Presentation: `fogRenderer.render` 18.6% (three's encode of the whole world plus the post chain), rider shadows
  1.9%, set-piece update 1.9%.
- Presentation passes, render ms per frame (median, outside the simulation): 4x low tier baseline 22.8-25.5,
  `sun=0` 20.9, `glow=0` 21.0, `glare=0` 21.4, `originalFog=0` 15.7 (the whole fog/snow-composite/post chain);
  2x high+native baseline 10.2, sun 9.7, glow 11.3, glare 12.3, fog chain 8.4. Only the fog/post chain is clearly
  measurable (~2-8 ms); sun, glow and glare are each within ~2 ms (noise). None was cut: the frame is
  simulation-bound and each pass is part of the PS2 picture; `quality.passes` keeps per-pass switches for a device
  that turns out GPU-bound.

Memory (same race, 1x, macOS phys_footprint of the renderer): 1728 MB live, 1477 MB after a forced GC; with `ai=0`
992 / 743 MB. Each computer rider costs ~147 MB in the renderer plus ~14 MB in the GPU process. ~250 MB is garbage
waiting for a major GC. Ranked:
1. Computer-rider wasm memories: 5 x 128 MB = 640 MB resident (vmmap: every AI memory 128 MB dirty), though each
   core has only 57 MB non-zero (61 MB high-water; 38 MB of it the same world in all five). Cause: copying the world
   template into each fresh core (`ai-racers.js copyTemplate`): its zero-chunk check reads every 64 KB chunk of the
   fresh core, and on XNU reading or zero-writing an untouched wasm page makes it resident (a fresh 128 MB core read
   end to end: +126 MB). Fix: never read/clear chunks above the fresh core's heap top (they are zero), copy only
   non-zero template chunks below the template's high-water mark: ~335 MB saved. Also `ai-world.js:17`
   `HEAPU8.slice()` is a transient 128 MB copy (trim to the high-water mark). Shrinking `INITIAL_MEMORY` alone does
   not lower RSS (an untouched core is ~5 MB).
2. Human core: 70 MB (= its non-zero pages).
3. Blink buffer partition ~180-213 MB: 4.1 MB per `createCore()` instantiation (compiling core.wasm once and
   instantiating the shared `WebAssembly.Module` saves ~20 MB), dev-server script sources ~22 MB, JSON kept as
   strings ~15 MB.
4. Music: the whole `.mus` (20-42 MB) is held in JS (pathfinder.js loadSong via game-audio.js); a large download is
   copied up to 3 times in flight (chunks, joined bytes, the Response copy) plus the `downloads.js` share copy.
5. `ai-race.js` `resources` stays alive through `api.prepare` (collision.bin 9.8 MB and ~14 MB of JSON text) though
   only the rider text, packets and initial text are used after the template is built: ~19 MB.
6. World geometry arrays kept after GPU upload (main.js asset): vertices 19.4 MB, colours 7.8 MB, ~6 MB more; ~33 MB
   if nothing reads them after load (terrain index arrays must stay, terrain-overlays edits them). three's WebGPU
   backend does not call `onUploadCallback`, so this needs an explicit release.
7. Smaller: decoded SFX PCM map 9.8 MB, AudioBuffers 19-26 MB, animation samples 8.5 MB (needed), environment.bin
   4.8 MB, canvas 2D 15.8 MB.
8. GPU: buffers 51 MB (world vertices 20.4, colours 8.1), textures 63 MB of which the MSAA targets (3 x 8.2 MB
   rgba16float + 4.1 MB depth, ~29 MB) are gone on the low tier.
9. Workers during the race: terrain (10 MB used), audio decode (1.6 MB); the AI world worker ends ~0.6 s after start.

Loading at 4x throttle: 14-29 s from navigation to the race depending on machine load (1 clean run: 28.5 s low
tier, 29.0 s high; 97 long tasks totalling 12.6 s, longest 1.2 s), peak JS heap 462 MB (268 MB at race start),
peak renderer RSS 1.77 GB; unthrottled 8.3 s. `web/downloads.js` retention (`SHARE_MS`) was lowered from 20 s to 2 s
for all devices during this session by another agent, which covers the "cap retention on mobile" item.

Ticks: one 60 Hz tick with six riders costs ~5 ms unthrottled, ~20 ms at 4x. At 4x the low tier runs 4 ticks per
drawn frame (~44 ticks/s: the game runs slower than real time); `quality=high` spirals at 12.25 ticks/frame
(~58 ticks/s at 4.8 fps); `ai=0` needs 1.8 ticks/frame (68 ticks/s, real time).

**2026-09-24, later: one core, six riders** (docs/ai-racers.md "One core, six riders"). The five computer riders are
now rider contexts of the human's core (thread-local rider state, shared course geometry), not five cores: the race
has one wasm memory (134 MB in the browser, was 6 memories / 805 MB), items 1, 3 (the per-core instantiations), 5
(collision.bin is no longer fetched for the computer riders; the world template worker is gone) and 9 (the AI world
worker) below no longer apply. Chrome, 390x844, 4x, interleaved runs under the same machine load: renderer RSS after
a forced GC 1020-1075 MB (was 1315-1454; `ai=0` 923-1041), 7.5-10 fps (was 6-7.5); desktop 1108-1235 MB (was
1372-1385), 60 fps both. The computer riders' skin palettes and rider lighting are captured only on the last two
ticks of a drawn frame (presentation only). The remaining simulation cost is the original's per-rider work (FX
pass, pose, physics).

What still limits phones: (1) the simulation of the six riders on the main thread (one core since 2026-09-24: the
computer riders' FX pass, pose and physics are the original's per-rider work) — about 2/3 of the frame at 4x; moving
the core's simulation to a worker is the next lever (see docs/ai-racers.md "Next step: the simulation in a worker"); (2) memory: ~1.0-1.2 GB in Chrome for a
race with computer riders since the one-core change (was ~1.7-2 GB), near the iOS Safari tab limit (~1-1.5 GB) —
needs a real device; (3) a real device is needed for WebGPU vs
WebGL on iOS, thermal throttling, the iOS 18 switch haptic and Safari's real memory ceiling.

## Whole-mountain memory (2026-09-29, whole-mountain memory agent)

The All Peak Race / Jam and the Peak 2 Race always run in the streamed MOUNTAIN world (web/free-ride.js `peakRunWorld`), on every
device. With pv `peakRelease` on, the career free ride uses MOUNTAIN on phones too (`mountainFreeRide`: no tier split).

**Target and headroom.** WebContent `phys_footprint` (macOS WebKit, the same accounting iOS uses for jetsam: WebKit Malloc, JS heap,
wasm, and the page's GPU resources charged to WebContent as "graphics"):
- at most **1.0 GB steady** (p95 over a run);
- at most **1.2 GB for peaks** (loads, hubs).

Why:
- iOS gives WebContent the lower of WebKit's memory-pressure limit and the jetsam limit. That limit depends on the device's RAM and
  on the system's state.
- Reports: about 3 GB on an iPhone 15 Pro. On an iPhone 12 Pro, about 3 GB right after a reboot, falling to about 1.5 GB after a
  few days. iPhones get an ActiveHard limit of 2048 MB. The 4 GB iPhones (11 / 12 / 13 / SE 3) are below the 6 GB figures.
- So 1.5 GB is the floor to plan for. 1.2 GB leaves 20% for what the Mac measurement cannot see: iOS's JSC heap sizing and GPU driver
  allocations differ, and other tabs and system load lower the limit.
- The field logs have an iPhone killed 10 s into a BHP1 race (2026-09-24), when that race measured about 1.0-1.2 GB here. That is
  consistent with a limit near 1.2-1.5 GB on that phone.
- The GPU process (measured below) has its own limit and is not counted in the tab's.

**Tools** (`local/browser-validation/whole-mountain-memory/`):
- `apr.mjs`: WebKit through web/webkit-driver.mjs, with ?mute=1 and the driver's page mute. Audio is unlocked by a synthetic keydown,
  so decoding runs.
  - A lean in-page pilot rides the AIP race paths of the route with a fake standard pad. It keeps frame counters, not per-frame arrays.
  - `footprint -p` of our WebContent and GPU processes is sampled every ~1.5 s outside the page, by category. The page is polled every
    3 s (docs/first-load.md).
  - Modes: `--mode 8|11|7` (a peak run, direct boot, or `--menu 1` from the menus) and `--mode free`. The free mode is the career
    free ride in MOUNTAIN from E, with legs `18,T21,19,T17,18,T20,17` (ride to a course, `T<n>` a Transport). It compares the terrain
    heights under every location's path points on its first pass with every later pass (collision holes).
- `analyze.mjs`: footprint by course segment and timeline.
- `loadpeak.mjs`: the CTM agent's career shape: menu, career, BRA2 event, Give Up, Transport, goWorld(18).
- `chr.mjs`: Chrome heap snapshots. `abuf.mjs` / `retainers.mjs`: ArrayBuffer and node retainer chains.
- Runs and logs are in `runs/`.

**Where the memory went with peakRelease off** (All Peak Race, 844x390):
- collision is never freed in a peak run (the release had a `!route` guard), so core HEAPU8 went 154 -> 319 MB, and 39 locations
  were loaded at the finish;
- the peak run read far beyond the PS2's rows:
  - the windowed prefetch held the next 2 route rows;
  - planAhead fetched every connector's row at a hub (ESS3 / EHP3 / EBA3 at E, CBA2 / CHP2 at C, DSS2 at D, ABA1 at A, BHP1 at B);
  - a station built the nearest connector's course, not the route's;
- draw packages stayed 45 s after their location left (a Transport's departure too).

**With pv peakRelease** (web/free-ride.js, web/peak-world.js; docs/ctm-parity.md "The PS2's location release"): the PS2's rule. A
location no row wants and nothing reads ahead is released at once: draw package, environment slice and collision. This is the
eviction at T+8, which returns every record of the track (3A8528 / 3A8230).
- The 45 s hold stays only for a station's other ways on.
- Unfed read-ahead slices nothing wants are dropped (`peak.dropQueued`).
- A peak run reads ahead only its route's next row (`routeNext`), and a station builds the route's row.
- A single peak world no longer background-feeds the rest of the peak.

| WebKit, quality=low | peakRelease | end | WebContent p50 / p95 / max (MB) | GPU proc max | core HEAPU8 | frames > 34 ms |
| --- | --- | --- | --- | --- | --- | --- |
| All Peak Race 844x390 | off | results | 1036 / 1187 / 1318 | 646 | 154 -> 319 | 3.1% (2nd run beside it) |
| All Peak Race 844x390, alone | on | results | 800 / 878 / 1076 | 640 | 128 | 99 of 125,776, max 82 ms; 0 stalls |
| All Peak Race 390x844 | on | results | 801 / 881 / 1089 | 593 | 128 | 0.11%, max 59 ms |
| All Peak Jam 844x390 | off | results | 981 / 1090 / 1273 | 686 | 319 | 0.6% |
| All Peak Jam 844x390 | on | results | 845 / 1129 / 1243 | 653 | 128 | 0.8% (other runs beside it) |
| Peak 2 Race, from the menus | on | results | 1071 / 1298 / 1319 | 589 | 128 | (other runs beside it) |
| Free ride E -> B, T21, E -> C, T17, A -> B, T20, D -> A (44 min), phone tier | on | done | 1104 / 1337 / 1446 | 959 | 128 | 2.3% (other runs beside it); 1 stall poll |
| Career shape, 4 event cycles (loadpeak) | off (per-peak worlds) | done | lifetime peak 3044 | | | |
| Career shape, 4 event cycles (loadpeak) | on (MOUNTAIN) | done | lifetime peak 2620 | | | |

- Frame counts from runs with another WebKit run beside them are machine contention: the same build alone had 0.08%.
- The free ride's re-entry heights, 19 location passes: 0 lost.
  - E_ERA5 / ERA5 differ at 27 of 73 probes, by up to 28 cm. The same change appears within the first pass with no release at all
    (Chrome, `dyn.mjs`: the heights change once the rider is in Gravitude and stay). It is the world's own moving collision, not a
    release hole; the core frees and re-feeds these four locations exactly (node, every order).
- **Frame counts with two WebKit windows** are not usable: the occluded window runs at about 28.5 fps (b-apr-on: 77,168 frames in
  45 min; j-desk-on-A: 9,692 in 5.7 min). Compare frames from runs alone only.
- **Draw releases wait while a needed build runs.** While the draw package of a wanted row, or of the row a connector leads to, is
  still building, the releases wait (at most RELEASE_MS). An immediate release's disposals competed with that build in WebKit.
  Before this: desktop free ride, 23 stall polls (Snow Jam entered 36 s after its build started, at a load average of 40).
- **Desktop (1280x800, quality high), peakRelease on**, the whole free ride with three Transports:
  - re-entry heights 0 lost / 0 differ on 19 location passes;
  - 1.3% of frames over 34 ms (a load average of about 40);
  - WebContent p50 1245 / p95 1406 MB;
  - core 128 MB.
- **Desktop A/B, run side by side** (off vs on with the deferral):
  - Yellow station -> Snow Jam: 0 stall polls in both;
  - Green station -> Blue station: 0 stall polls in both;
  - WebContent p95 1129 (off) vs 1041 MB (on).
- **pv peakRelease is ON** (2026-09-29).

**Still above the target** (none of these comes from the location release; all show with peakRelease off too):
1. **Course-switch spikes** (career shape): WebKit Malloc +1.2-1.9 GB for 1-3 s, at an event load or about 30 s into the world ride
   after a return. Peaks 2.6-3.0 GB. Attributed in "Load spikes" below: the "30 s into the ride" one is the booth map's LUI garbage under this Mac's
   JSC heap policy; the event-load one was old cores still resident (two plain fixes).
2. **Load transients**: 1.3-1.47 GB for about a second, at a direct boot into MOUNTAIN and at the menu boot. Under the phone heap policy these boots stay at 1.0-1.2 GB ("Load spikes").
3. **WebKit Malloc climbs through a run** and never drops: +170 MB per 30 min direct, +360 MB in the menu-flow Peak 2 Race.
   - Chrome's JS heap is flat over the same run (107 -> 129 MB after GC).
   - A Chrome heap diff shows audio nodes never disconnected: GainNode +605, AudioParam +1264, sources and panners +220 each in
     14 min.
   - The music stream's `rangeBytes` Map keeps about 30 MB.
4. **The menus**: entering from the menus keeps about +200 MB for the whole run, including a second 128 MB wasm memory (probably the
   FE stage core). Fixed: it was the menu boot course's core, kept by two loadCourse closures ("Load spikes").
5. **A WebKit runaway** in 2 of 9 runs (one with the switch off, one on):
   - from the load on: WebKit Malloc +5-8 MB/s, "JS VM Reservations" 200 -> 1200 MB, JIT code 20 -> 360 MB;
   - it reaches 5-8 GB in 25 min, with the game still at 60 fps and three.js counts flat;
   - not reproducible on demand; handed to its own investigation. Data: `runs/*runaway*`, `*.sample.txt`.

## Load spikes (2026-09-29, load-spike agent)

The short WebKit peaks around loads, after the whole-mountain memory work. Scratch tools, copied to `local/browser-validation/load-spikes/`:
- `probe.js`: injected before any module by `vite.spike.config.mjs` (port 5302) or `vite.bisect.config.mjs` (5303, with `?bisect=` source switches). It keeps 250 ms buckets of:
  - fetch bodies, JSON.parse / stringify, TextDecoder / TextEncoder, typed-array allocations and copies, worker messages;
  - WebGPU writes, mapped buffers, shaders, pipelines and bind groups; 2D-canvas calls; wasm instances and grows;
  - performance marks; full collections, from FinalizationRegistry markers held 1.5 s;
  - every instance's WebAssembly.Memory in a FinalizationRegistry (never deref'd).
- `run.mjs`: WebKit through the driver. Shapes `career` (the career shape), `menu`, `direct` (`?course=MOUNTAIN&peakMode=8&autostart=1`), `event` (`?course=BRA2&autostart=1`) and `map` (the station map held).
  - `footprint` is sampled outside the page and the page is polled every 3 s.
  - `--ram 6` sets `__XPC_JSC_forceRAMSize` on the driver: JSC's heap policy for a 6 GB phone.
- `tl.mjs` (footprint per second beside the probe's counters), `sum.mjs` (per-phase max / median), `batch.sh`.
- `bootcore.mjs`: FinalizationRegistry-only core liveness, with forced full collections.
- `chralloc.mjs`: Chrome allocation sampling with garbage included.

**Two heap policies.** This Mac has 64 GB, so JavaScriptCore runs its "Aggressive" growth mode: `Heap.cpp proportionalHeapSize`, 3·e^(-2x) + 1, about 3.9x the live heap before a full collection, with an eden budget to match.
- A phone runs the default mode: 2x below 25% of RAM, 1.5x above.
- JSC options reach WebContent as `__XPC_JSC_*` variables on the driver. Checked: `__XPC_JSC_useJIT=false` makes a loop 40x slower.
- `__XPC_JSC_forceRAMSize=6442450944` gives the phone policy. The earlier numbers in this file are the Mac policy.

**Attribution** (career shape, 2 event cycles, phone tier):
1. **"About 30 s into the world ride" is not a load.**
   - The arrival at station 18 opens the booth map (ctm-peaks). On it, WebKit Malloc climbs 100-110 MB/s with no fetch, typed-array, JSON or GPU traffic (probe), until a full collection frees 1.1 GB at once (Mac policy: 590 -> 1808 MB, WebContent 2226 MB; no full collection for 28 s).
   - Chrome, same screen: 47 MB/s of JS garbage (the game screen: 20 MB/s). About 25 MB/s is the LUI player's per-frame objects (lui-player.js draw / abs / shape / sprite), 4 MB/s ctm-map.js override and 4.5 MB/s peak-set-pieces update.
   - The world load after a Transport shows the same map: 500 of its 875 MB of JS allocations. When a load is slow (machine load), the map's garbage makes the peak: 3090 MB in one Mac-policy run after a 20 s world load.
   - Under the phone policy the map did not spike in any run.
2. **Old cores resident at the next load.** WebAssembly Memory reached 371-429 MB at an event load (3 cores) against 128 MB for the new one, WebContent 1602-1633 MB (phone policy). Two holders:
   - `acrossBefore` (pv switchGate's held draw) kept the released course's objects, and through their closures its core, until the new course went live.
     - **Fixed (plain):** pruned to what is still in the scene right after unloadCourse. renderAcross only tests scene children against it, so the draw is the same. unloadCourse also drops `worldRewarm`.
   - The menu flow's boot course core (128 MB, ~93 MB of data) stayed alive through the whole first career ride and every forced full collection, in every run (FinalizationRegistry on its memory; JIT on, DFG off or JIT off alike). With `?ai=0` it was freed.
     - The holder: loadCourse's non-free-ride branch set `ui.cb.standings` / `ui.cb.lineup` to arrow functions made inside loadCourse, and the streamed world never sets them again.
     - In JavaScriptCore those closures kept that loadCourse call's scope, and with it the boot course's core and about 130 MB of its load. Probably JSC keeps an async function's locals that live across an await in the scope its closures capture (not confirmed from the engine). Chrome freed it.
     - Found by bisection: a vite transform turning off the opponent models, the opponent lighting, the opponent FX and the rider icons changed nothing; setting the two callbacks to null freed it at the next full collection (WebContent 953 -> 810 MB).
     - **Fixed (plain):** module-level `aiStandings` / `aiLineup` (the same module variables; the worldEvent path too). After the fix, menu -> career and menu -> Peak 2 Race, both policies: freed at the first full collection (4 of 4).
     - The runaway agent's "held through its 5 computer-rider groups" were these closures, not the groups.
3. **No full collection for 20-40 s after a load**, both policies. The load's garbage stays resident until one comes: a forced full collection took WebContent from 1020-1259 to 607-842 MB (phone policy) and from 948 to 607 MB (Mac policy, Peak 2 Race), 5 s into the ride.
4. **The load's own transient** (phone policy): the first career load (menus -> MOUNTAIN) reached malloc 1.0-1.2 GB plus graphics ~260 MB, WebContent 1412-1465 MB, for 1-2 s at the end of the load and warm.
   - It lines up with ~30k bind groups, the pipeline warm, 57 MB/s of mapped-at-creation vertex uploads and the load's garbage.
   - Chrome sampling of that load: ~96 MB/s of JS allocations. The largest sites are asset()'s bounds loop and three's compile path (getMaterialCacheKey, NodeBuilder), the render under the load screen, and worker result clones.
   - Chrome's `expandByPoint` / `fromArray` totals are V8 HeapNumber boxing; JSC stores doubles unboxed, so they are not WebKit garbage.
5. **Fetch bodies:** web/downloads.js held 3-4 copies of every asset body while a load read it: the chunk list plus its join (pv flyover off), the shared copy (kept SHARE_MS), `new Response(bytes)`'s copy and `arrayBuffer()`'s copy. That is 100-250 MB of ArrayBuffers per load.

**Switches (off):**
- **pv `switchGC`** (web/switch-gc.js, test-switch-gc.mjs; JavaScriptCore only). It is the gc-watchdog kick: one-page WebAssembly.Memory objects, a full collection ~100 ms after the first.
  - Before a new core: while an earlier core's memory is alive (FinalizationRegistry), kick until it is freed, a full collection has passed without freeing it, or 800 ms have passed.
  - After the load, before `ready`, still under the load screen: one full collection (collectNow, at most 600 ms).
  - Measured: 50-250 ms per switch. Aged FinalizationRegistry markers confirm the collection. `window.__switchGC` keeps the last 20.
  - Liveness never uses WeakRef.deref(): a deref during a concurrent collection keeps its target for that cycle. The first version polled deref() and no core was ever freed inside its wait; the old loadpeak RECORDER derefs `__coreRefs` every 100 ms too.
- **pv `loadCopies`** (web/downloads.js, test-downloads.mjs section 11). A body of known size is read into one buffer, and each caller's Response reads the shared bytes: arrayBuffer / bytes / blob make the caller's one copy, text / json decode them. `body` / clone() copy only when asked, and the Response lets go of the bytes once read.
  - Two earlier versions were worse in WebKit (1647-2031 MB). One gave the Response a JS stream body; the other kept the shared bytes for as long as a caller's scope kept its Response (the texture archive job keeps its archive's Response).

**Results** (WebKit through the driver, phone tier, WebContent lifetime `phys_footprint_peak`, MB; one value per run; other agents'
WebKit / Chrome runs beside them, load average 20-30):

| Career shape, 2 event cycles | phone policy | Mac policy |
| --- | --- | --- |
| before (this morning's tree) | 986, 1633 | 2242 |
| acrossBefore pruned only | 1465, 1479 | |
| both plain fixes | 1347, 1120, 1688, 1266, 1199 | 1370, 2845 (a), 1292, 1231 |
| + pv switchGC | 1174, 1014, 1229 | 3090 (b), 1420 |
| + pv switchGC, loadCopies | 983, 1085, 1225, 1051 | 1487 (c), 1418 (c) |

(a) The event after the first load stayed at 1.96 GB: no full collection (the runaway's stall; the watchdog waits 40 s).
(b) A 20 s world load under machine load with the Transport map up: its LUI garbage, malloc 1589 -> 2657 MB in 9 s.
(c) At the menu boot, before any switch (switchGC changes nothing there).

With both plain fixes, the phone-policy peaks are the first career load (menus -> MOUNTAIN, 1186-1249 MB) or an event load's garbage.
With switchGC the old cores are gone before the new one is made. Phone policy, the last two runs of each (medians per phase): event load
691 / 712 MB against 757 / 806 MB; the rides after a load 697-947 MB against 773-959 MB.

| One load (one run each) | phone: fixes / + switchGC, loadCopies | Mac: fixes / + switchGC, loadCopies | before (Mac, whole-mountain agent) |
| --- | --- | --- | --- |
| menu boot (lazy boot course behind the menus) | 1140 / 1030 | 1395 / 1408 | 1377-1417 |
| All Peak Race, direct boot, 40 s | 1008 / 1058 | 1135 / 1186 | 1307 |
| BRA2 Single Event, direct boot, 30 s | 1200 / 1108 | 1485 / 1580 | |

- A whole All Peak Race (apr.mjs, direct, phone policy, switchGC + loadCopies, one run): results reached, WebContent p50 689 / p95 821 /
  ride max 971 MB, lifetime peak 988 MB, 394 of 122,132 frames over 34 ms (other runs beside it). The earlier Mac-policy run: p95 878 /
  max 1076 / lifetime 1307 MB.
- The single loads are the load's own transient: switchGC has nothing to collect before them. Under the phone policy they are at
  1.0-1.2 GB; under the Mac policy 1.1-1.6 GB.
- The boot core (bootcore.mjs; FinalizationRegistry, forced full collections) was retained in all 11 runs with the computer riders on
  before the closure fix, and freed in 4 of 4 after it (menu -> career and menu -> Peak 2 Race, both policies). Ride at +20 s, then
  after a forced full collection: phone career 1259 -> 842 MB, phone Peak 2 Race 1020 -> 608 MB, Mac Peak 2 Race 948 -> 607 MB (the Mac
  career ride had run one on its own: 978 -> 975 MB).
- Costs: switchGC 150-250 ms before a new core (1-3 memories) and 50-70 ms after the load (1 memory), under the load screen. The
  loadCopies runs had the boot course's load 0.9-1.3 s longer (4 of 4: 3.4-4.0 s against 2.5-3.1 s); not understood, so it stays off.
- Recommendation: switchGC on after a check on iOS hardware (it uses the same fast-memory kick as pv gcWatchdog, which is on).

**Open:**
- The LUI player's per-frame garbage on the map / load screens (Mac policy: it makes the post-Transport and booth-map spikes).
- The load's own transient at the first career load.
- switchGC on iOS hardware: the kick's fast-memory threshold is smaller there (the runaway agent: 2 memories on the iOS Simulator).

## Field crashes and diagnostics (2026-09-26)

From the host's field reports (`~/ssx-host/logs/diag.log`, web/diagnostics.js; 116 sessions 2026-09-24..26):

- **WGSL private memory (iPhone, Metro City / Ruthless / Peak 1):** WebKit rejects a shader whose `var<private>`
  (or function-scope) variables exceed 8192 bytes ("The combined byte size of all variables in the private address
  space exceeds 8192 bytes", then `setPipeline: invalid RenderPipeline`); Chrome and Firefox accept it. three.js r186
  declares every TSL temporary as a module-scope `var<private>`, and a `select()` emits its input in both branches, so
  the fog composite (snow, glow, sun, glare, ScreenTint, and since 2026-09-25 the lightning flash: each one more nested
  select; the flash took it from an estimated ~6.5 KB to 11.8 KB) had grown to 234 KB of WGSL with 945 private vars (11.8 KB), the glare's final pass to 269 KB / 1368 vars.
  The post frame (and the glare frame) did not draw on iPhones. Fixed by keeping each stage's result in a var
  (`.toVar()` in fog-renderer.js, glare-pass.js): 27 KB / 96 vars (1.3 KB) and 13 KB / 66 vars; Chrome frames are
  pixel-identical before/after (Metro City ticks 400/900 with glare and tint, Ruthless 300/1200, Kick Doubt 200).
  `web/wgsl-budget.js` estimates the sizes; `test-shader-budget.mjs` (npm test, headless Chrome) compiles every
  pipeline of three course loads and fails above 6144 B; diagnostics report `shader-over-budget` from any browser.
- **GPU device loss:** iOS reports `destroyed` (in the background, or during a heavy load), Android Chrome "A valid
  external Instance reference no longer exists"; three.js ignores `destroyed`, so every frame threw
  `createCommandEncoder` errors on a black tab. `web/gpu-recovery.js` stops drawing, waits until the page is visible,
  gives the same renderer a new device and fresh managers (everything re-uploads, pipelines recompile, the animation
  loop moves over; the old managers get a no-op backend so later disposes stay harmless), else reloads the page (at
  most twice in 5 minutes), else the title card says "Graphics lost. Reload the page". WebGL context loss reloads.
  `test-gpu-recovery.mjs`: the frame after a recovery equals the frame before the loss.
- **"previous-session-died" on the pause screens was mostly false:** the game pauses when the page hides, and the
  browser fires `pagehide` before `visibilitychange`, so the pause's screen change rewrote the crash marker after it had
  been marked clean (reproduced: a plain reload from a race reported a death on `pause`). The marker is frozen after
  pagehide now; a death also says `cause` (foreground / background / discarded / frozen), how long the tab was hidden,
  the last memory sample and the last 12 events. The real ones in the log were background evictions (iPhone after 47
  and 20 minutes hidden, Android after a minute) and two early iPhone kills in the BHP1 half-pipe on 2026-09-24 (one lost its device 5 s into the load).
- **Memory:** Chrome JS heap stays at 150-450 MB over 80-minute sessions with dozens of pause/restart cycles; eight
  course switches in one page add ~1 MB of textures each, bounded by the cutscene actor pool (8 idle models). Reports
  now carry `texMB` / `tex` (three.js texture accounting, every browser) and `wasmMB`, so iPhone reports show memory.
- **Audio:** back from the background iOS refuses `AudioContext.resume()` without a gesture ("Failed to start the
  audio device", unhandled): caught now, the next key / tap retries.
- **Downloads:** `web/downloads.js` retries network errors, broken or stalled bodies (20 s without a byte) and HTTP
  408/429/5xx (0.5, 1.5, 4, 8 s); the final failure has `network: true` and the title card says "Connection lost".
  `test-downloads.mjs`. The two field fetch errors were downloads aborted by the player leaving a slow first load.
- **Unknown course at boot** (CRA3/ERA5, our own smoke tests while the Peak 2/3 data was being swapped in): a course
  missing from courses.json now falls back to Snow Jam and the menus instead of a dead "Load failed" card.

- **Hangs (pv `hangWatch`, on; 2026-09-28):** a freeze with no error and no pagehide looked the same as a killed
  WebContent process. The page now pings a small blob worker every 250 ms with the screen, the course, the last
  performance marks, and `step` / `recent` (the last events that are not heartbeats). When the pings stop for 8 s while
  the page is visible, the worker posts `{kind: 'hang', since, gapMs, last}` to /mp/diag itself, again every 30 s with
  `lasting: true`, and `{kind: 'hang-end', lastedMs}` when the pings come back. It stays silent while hidden, and it
  ignores a gap in which the worker itself stalled (machine sleep).
  - Chrome (scratch `ctm/hang/hangtest.mjs`, a proxy that keeps the POSTs): no event in 20 s of idle. A 12 s main-thread
    block gives `hang` at +8.0 s and `hang-end` at +12.0 s. A 40 s block gives `hang` at +8 s, `lasting` at +38 s and
    `hang-end` at +40 s.
  - WebKit runs all of a dedicated worker's I/O through the page's main thread: fetch, WebSocket, IndexedDB, Cache
    and OPFS each finished only when a 10 s block ended (Chrome: 1-31 ms; `ctm/hang/offmain.mjs`). So in Safari the
    hang events carry the worker's times but are only sent once the page recovers. A freeze that never ends shows up
    as the next session's `previous-session-died` (cause foreground, `agoS`).
  - A killed process takes the worker with it: no hang event, then `previous-session-died`.
- **The WebKit memory runaway: JavaScriptCore stops running full collections (pv `gcWatchdog`, off; 2026-09-29):**
  - **Symptom** (whole-mountain memory agent, 2 of 9 phone-tier WebKit runs): from the load on, WebKit Malloc +3-8 MB/s,
    "WebAssembly Memory" (vmmap "JS VM Reservations") 200 -> 1200 MB, JIT code 20 -> 360 MB, 5-8 GB in 25 min, at 60 fps with
    flat three.js counts. The GPU process grows too. A phone kills the tab within minutes.
  - **Detector** (scratch `rw/probe.js`, then `web/gc-watchdog.js`): a marker object held 3 s (so it is old generation), then
    dropped and registered with a FinalizationRegistry. Only a full collection frees an old object. Healthy WebKit frees each
    one 3-7 s after its drop. In every runaway the last one is freed during the world load, and none after that (125+ s,
    100+ markers waiting). The `sample`s of the runaways show only EdenGCActivityCallback collections; a healthy one shows
    FullGCActivityCallback. So everything only a full collection frees piles up: old-generation per-frame garbage, dead cores
    (the old courses' 128 MB memories), jettisoned JIT code and stubs (the 512 MB JIT pool fills, then LLInt), GPU / audio
    wrappers. Chrome is not affected.
  - **Cause** (JSC source, `heap/Heap.cpp`, `GCActivityCallback.cpp`, WebCore `OpportunisticTaskScheduler.cpp`):
    - the full-GC timer's delay is `lastFullGCLength / gcTimeSlice(bytes x deathRate)`, where deathRate =
      (sizeBefore - sizeAfter) / sizeBefore of the last full collection, and 0 when sizeAfter >= sizeBefore. With deathRate 0
      the delay is infinite and `scheduleTimer` never arms it again;
    - extra memory (ArrayBuffers, wasm memories) allocated while a concurrent full collection runs is added to the visited
      extra memory (`reportExtraMemoryAllocatedPossiblyFromAlreadyMarkedCell`), but the cycle's allocation counter is reset
      at that collection's end, so it is in sizeAfter and not in sizeBefore. A world load allocates big buffers all the
      time (packages, the new core's 128 MB memory, grows), so the load's last full collection can end with
      sizeAfter > sizeBefore. Timing-dependent: about 1 load in 3-5 in the menu -> MOUNTAIN shape;
    - after that only the eden timer runs, and its requests are Eden-scoped (`shouldDoFullCollection` ignores
      m_shouldDoFullCollection for a scoped request). A full collection then needs an allocation-limit collection
      (more than maxEdenSize allocated between two eden timer firings), which normal play never reaches.
  - **What brings full collections back** (measured on live runaways; scratch `rw/trial.mjs --exp`):
    - 2 x 1 GB and 2 x 4 GB ArrayBuffers (untouched, ~0 footprint): nothing;
    - `new WebAssembly.Memory({initial: 1})`, a single 64 KB one: full collections back at once. 108 waiting markers were
      freed; footprint 1589 -> 927 MB (WebAssembly Memory 494 -> 102: the dead cores go, malloc 819 -> 616, JIT 30 -> 20).
      Another runaway with 2 x 1 GB Memories: 2106 -> 1548 MB, JIT 60 -> 23, and full collections every ~5 s after it.
    - Why: JSC's BufferMemoryManager (`runtime/BufferMemoryHandle.cpp` tryAllocateFastMemory) asks for
      `collectAsync(Full)` once half of the process's fast-memory slots are in use (`maxNumWasmFastMemories` 8 with a large
      gigacage, else 3), and `collectSync(Full)` when none is left. The game's cores already hold slots.
    - Fresh idle page (scratch `rw/kicksrv.mjs`): 1-3 one-page memories do nothing, the 4th gives a full collection
      ~100 ms later, every time. iOS Simulator Safari (iPhone 17 Pro, iOS 26.5): 2 are enough (a smaller threshold), and no
      RangeError. Not checked on iOS hardware.
  - **Where it shows** (scratch `rw/trial.mjs`, WebKit, phone tier, the marker probe; Mac = this 64 GB Mac's JSC policy,
    "Aggressive" growth, about 3.9x the live heap before a full collection; phone = `__XPC_JSC_forceRAMSize=6442450944` on the
    driver, JSC's default 2x / 1.5x policy):
    - menu -> MOUNTAIN free ride (`cb.freeRide(21, {reload})`): Mac about 8 in 43 runs; phone 0 in 22.
    - menu -> Peak 2 Race (`cb.peakRun(7)`, the audio agent's "DRA4 crossing" runaway, 4 of their 6): Mac 1 in 6 of mine;
      phone 0 in 2. It is the same stall and it starts at the load: last full collection 2 s into the ride
      (t = 37.5 s), none in the next 10 min. The footprint crept +1 MB/s, then +5-8 MB/s from the DRA4 streaming at 4.5-5 min
      (3.2 GB, JIT 156 MB at 10 min). Their four runaways have the same JIT climb.
    - minimal repro page (below): Mac 3 in 19; phone 0 in 10.
    - pv worldWarm is not the cause (worldWarm on 0 / 10, off 2 / 9, run side by side).
    - Under the phone policy the stall was never seen. Likely (not confirmed from the engine): the deathRate-0 state is the
      same, but with the default policy maxEdenSize is about the heap size, so allocation-limit (unscoped) collections come
      often and run full. Not checked on iPhone hardware, where the tab limit is also far lower.
  - **Kick cost** (longest rAF gap in the 4 s after it): on a 1.25 GB runaway (fx-4), 62 ms once (normal frames there
    18-30 ms), footprint 1251 -> 838 MB. On the repro page, 21 ms.
  - **pv `gcWatchdog`** (off; `web/gc-watchdog.js`, started in main.js after installYieldShim, JavaScriptCore only by user
    agent, QA `window.__gcWatchdog`):
    - old and young markers once a second. Stalled = an old marker has waited 40 s with nothing old freed, while young
      markers are still freed (eden collections run). An idle page that allocates nothing runs neither, and is not a stall.
      40 s is over twice the longest healthy gap between full collections: 17 s on the Mac (7 x 10-min Peak 2 Races, every
      full collection timed), 13 s under the phone policy; another agent saw 28 s once on the Mac;
    - it waits up to 5 s for a safe moment (the load screen, `isPaused()`, a cutscene), then adds one one-page memory a
      second, held until an old marker is freed, at most 6 per episode, 30 s between episodes. A dropped one-page memory is
      freed by the next eden and its slot comes back (tiny:1 twice did nothing on a runaway), so they are held. It stops
      at the first memory that crosses the threshold, so it never reaches the collectSync(Full) case. The collection frees
      them too (10 rounds of 4 in a row all succeed);
    - `web/test-gc-watchdog.mjs` (in test:all): healthy never fires, idle never fires, a stall recovers at the 4th memory,
      the safe-moment path, gives up after 6;
    - WebKit with it on: 26 game runs (23 menu -> MOUNTAIN, 3 Peak 2 Races x 10 min) with 0 kicks and 0 runaways (no stall
      happened in them either). The repro page with it on: 1 stall in 7, found at 45.5 s, full collections back 3.0 s
      later after 4 memories, longest frame 21 ms, and none after.
    - A stall starting again later in a ride (streaming during a full collection) is possible: after one revival in a
      MOUNTAIN runaway, full collections came only when forced. The watchdog kicks again after 40 s + the cooldown.
  - **Our side:**
    - main.js `acrossBefore` (pv switchGate's scene before a switch) and `loadBefore` (loadCourse) were module-level Sets
      never cleared. They held the previous course's whole scene until the next switch, and through its closures
      (`userData.shadowRider.core` -> `load` -> `human`) that course's core. In the memory agent's Chrome snapshot, the old core
      is reachable only through them. Both are now cleared when the new course goes live (plain fix, no switch). Chrome, the
      same menu flow: only the live core's memory is left.
      Since the load-spike work acrossBefore is also pruned right after unloadCourse (it held the old course through the whole next load).
    - ~~Still open (WebKit): the menu flow's lazy boot course core stays alive through the whole ride~~ Settled (load-spike agent, "Load
      spikes" above): real, not a probe artifact (FinalizationRegistry-only counting, forced full collections, JIT off too). The holder was
      `ui.cb.standings` / `ui.cb.lineup`, arrow functions made inside the boot course's loadCourse that the streamed world never set again;
      in JavaScriptCore they kept that loadCourse call's scope and with it the core (the 5 computer-rider groups hung off the same scope).
      Now module-level functions: freed at the first full collection (menu -> career and menu -> Peak 2 Race, both heap policies).
    - A caution for probes: `WeakRef.deref()` polled while a concurrent collection runs keeps its target for that cycle (a switchGC
      version that polled deref() never saw a core freed); count with a FinalizationRegistry.
    - Bind-group / sampler bursts (~240/s) are new render objects drawn for the first time (streamed content): three's
      `getForRender -> _createBindings`. Not per frame, and the same with pv threeLean off.
    - Seen, not followed: in 2 of 6 180-s MOUNTAIN runs with full collections working, WebKit Malloc climbed 2-3 MB/s from
      ~120 s in ERA5 with JIT flat and no new locations (the other 4 flat on the same path).
  - **WebKit bug report (draft, for Owen to file):**
    - Title: "JSC: full-GC activity timer is never re-armed after a full collection with zero death rate; heap grows without
      bound (only Eden collections run)".
    - `GCActivityCallback::didAllocate` computes `lastGCLength / gcTimeSlice(bytes * deathRate)`. `FullGCActivityCallback::deathRate`
      returns 0 when `sizeAfterLastFullCollection >= sizeBeforeLastFullCollection`, so the delay is infinite and
      `scheduleTimer` returns without arming the timer. Nothing re-arms it until some other full collection happens.
    - sizeAfter can exceed sizeBefore in normal use: extra memory reported during a concurrent full collection for an
      already-marked cell goes to `reportExtraMemoryVisited` (counted in sizeAfter), while `m_*BytesAllocatedThisCycle` is
      zeroed in `updateAllocationLimits`, so those bytes are in no cycle's sizeBefore.
    - After that, the Eden timer's requests are `CollectionScope::Eden`, and `shouldDoFullCollection()` returns Eden for a
      scoped request even when `m_shouldDoFullCollection` is set. So no full collection happens unless an unscoped
      allocation-limit collection occurs, which a steady 60 fps app with an active Eden timer never reaches.
    - Effect: old-generation garbage, jettisoned JIT code, dead WebAssembly memories and DOM / WebGPU / WebAudio wrappers are
      never freed: WebContent reaches 5-8 GB in 25 min (Safari 26 / macOS 27, WebKit 22625.1.29.11.27); iOS kills the tab.
    - Minimal repro (scratch `rw/reprosrv.mjs`, `?mb=4&every=100&touch=1`): per-frame garbage (20k small objects a frame),
      a 4 MB ArrayBuffer filled and kept every 100 ms for 10 s (the "load"), then dropped; an old-generation
      FinalizationRegistry marker every second. 2 of 9 runs stalled: the last marker freed at 7.5 / 8.4 s (inside the load),
      none in the next 42 s; 4 one-page WebAssembly.Memory objects then freed them all at once (both). Untouched buffers
      (32 MB every 100 ms) and wasm memory grows (8 MB every 200 ms): 0 of 7.
    - Both policies: over 29 runs of that repro on this Mac (64 GB RAM, JSC's Aggressive heap growth) 3 stalled (2 of them
      among 10 runs that also register a young object in a FinalizationRegistry each second: no effect). With
      `JSC_forceRAMSize=6442450944` (the default 2x / 1.5x growth, as on phones) 0 of 10, and the game's two shapes 0 of 24
      (Mac: menu -> free ride about 8 of 43). The zero-deathRate state should be reached the same way; the likely reason it does
      not last is that with the default growth an allocation-limit (unscoped) collection comes soon and runs full (not
      confirmed from the engine). So the report matters most for large-RAM Macs, and should be checked on iPhone hardware.
    - Suggested fix: floor deathRate for the full timer (e.g. at a small epsilon) or re-arm it after the next Eden
      collection, and let an Eden-scoped timer request be upgraded when m_shouldDoFullCollection is set.
  - Scratch tools (this agent's `rw/`): `vite.rw.config.mjs` (a dev server on 5281 that injects `probe.js`: instantiation,
    Memory, GPU object and WeakMap counts with stacks, the full-GC markers, load events), `trial.mjs` (repeated WebKit trials
    of a shape: apj / apr / fr / free menu flow; footprint; verdicts; `--exp` kicks; `--find`), `kicksrv.mjs` / `kickwk.mjs`
    (the fresh-page kick test), `reprosrv.mjs` / `reprowk*.mjs` (the minimal repro, `?wd=1` with the watchdog, `?yo=1` young
    markers), `finder.js` / `finder2.js` / `ident.js` (in-page retainer walks), `chrfree.mjs` (Chrome snapshot of the menu flow), `pathto.mjs` (heap-snapshot retainer paths
    with the WeakMap ephemeron edges excluded).

Report format: every event has `screen` and `course`; errors repeat at most 3 times, then `repeat` events count them;
400 events per session with 40 kept for priority kinds (errors, GPU, pagehide); heartbeat every 10 s for 10 minutes,
then every minute; `pipeline-failed` carries the pipeline label, material (`FogComposite#417`, `GlareFinal#...`),
object and the WGSL private sizes; events after pagehide have `unloading: true`.

## Trying it on an iPhone

1. Open the hosted site (docs/hosting.md) in Safari and log in (portrait).
2. The console/controller layout appears on the first touch. Tap START, then ✕ on Single Event; d-pad or stick moves
   through menus, △ goes back.
3. In a race: left thumb on the lower-left stick (push up = tuck, down = brake, sideways = carve), ✕ hold/release =
   jump, □ boost, ○ handplant, L1/L2/R1/R2 at the top = grabs (hold two or more for the chord tricks), d-pad =
   spins/flips in the air. START pauses.
4. For full screen: Share -> Add to Home Screen, then start "SSX 3" from the icon (log in once more there; home-screen
   apps keep their own cookies).
5. ≡ (top centre) opens Options > Display & Touch (in a race it pauses first): layout, stick, vibration, resolution
   (PS2 640x448 default), upscaling, 30/60 fps and quality.
