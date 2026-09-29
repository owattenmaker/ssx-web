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
