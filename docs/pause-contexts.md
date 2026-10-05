# Pause contexts and screen phases (as built, 2026-09-29)

Replaces the port's ad-hoc pause handling with the PS2's own two mechanisms. It is the only path: pv pauseContexts, cardFreeze and
bcPromptAudio are retired, and the old `paused` flag, `pause()`, `idleFrozen`, the wall-clock lock and the setTimeout outros are gone.
- The pause context stack decides every freeze.
- The screen phase machine always runs. Its PS2 frame rules (intro locks, outros) apply with **pv `ps2MenuInput`** (on).

Sources: [ctm-decomp-world-states.md](ctm-decomp-world-states.md) §1.3 and [ctm-decomp-screens.md](ctm-decomp-screens.md) E7-E11.

## 1. Pause contexts (0x5366E8 stack, masks table 0x4428F0): `web/pause-contexts.js`

On the PS2, whoever freezes something pushes a context and pops it when done. The game frame (0x230B00..0x230D50) runs each system
only when the top context's mask bit is clear.

| context | mask | PS2 pusher | port owner |
|---|---|---|---|
| 1 | 0xFFFFFFDB | WS2 enter 0x236BB0: the round card | the `ctm-objectives` screen for its lifetime (web/screen-phases.js SCREEN_CONTEXT) |
| 2 | 0xFFFFFFDF | the pause menu / MCOMM 0x230A94, the lodge prompt / map WS14 0x236504 | main.js `overlay` (with the audio pause) |
| 3 | 0xFFFFFFDF | a Big Challenge offer / fail 0x2308B4 / 0x230954, no 289B70 | big-challenges.js `hold()` / `resume()` (audio: false) |
| HOLD | 0x01 (port) | none: the PS2 stops the ride through its world states | main.js / the cutscene, named by the world state (below) |
| 12 | [0x442920] | the controller overlay 0x2309BC | not in the port (mask not read) |

| bit | stops |
|---|---|
| 0x01 | the simulation: stage world, riders, timer channels 1 / 5 / 6, the replay manager |
| 0x02 | timer channel 1 and 355028 |
| 0x04 | timer channel 3 |
| 0x08 | the NIS tick: script clock, camera, actors, NIS sounds |
| 0x10 | timer channel 2 |
| 0x20 | the world-state tick |

The view update (render, camera output, view fades) is never masked: a paused frame keeps drawing.

### The API
- `push(ctx, {owner, audio, ends})` returns a handle.
- `pop(h)` is idempotent.
- `stops(bit)` reads the top context's mask.
- `simFrame()` is the game frame's read of bit 0x01. A push stops the frame it happens in. A pop lets the tick run from the next frame, because the PS2 pops in the UI pass after that frame's systems have run (E11).
- `onChange(f)` fires on each change of the audio pause or the simulation bit.
- `audit(screen)` is a dev check. A holder whose `ends(screen)` is true still being open warns once (?qa or a vite dev build).

### main.js
- `paused` is derived: `isPaused()` = `contexts.stops(SIM)`. There is no flag to assign.
- The tick loop uses `contexts.simFrame()`.
- `onChange` handles two things:
  - audio: `gameAudio.pause(on)` while any audio holder is on the stack, which today is only the overlay (289B70 / 289BB8);
  - rumble: `rumble.stop()` when the simulation stops.

**The overlay (context 2)** covers the pause menu, the MCOMM, the station prompt / map and the FAQ. For the FAQ, whether overlay 0x22 pushes a context is not read yet.
- `open(screen)` pushes and sets the screen:
  - Start from the pad, the keyboard or the touch deck (all three through `startOpensPause`: the ride only, no cutscene, no
    `transporting`, not finished, nothing on the stack; the touch deck also not in an online race);
  - the 'station' prompt and the end of a transport's enterlodge;
  - the FAQ (faqOpen / the 'faq' contact: free ride, not paused, screen 'game', and for the deferred path no cutscene or Transport).
- Losing focus or visibility opens nothing (section 5).
- `close()` pops and returns to the ride: `cb.resume`, Give Up, and the in-world Transport's Yes (transportInWorld, before the ride).
- `drop()` pops when the menu goes with its world:
  - `cb.quit` (Quit, and the career Restart's quit);
  - `cb.start` (the race pause's Restart);
  - the lodge's world load;
  - a Transport's Yes into another page world (`cb.freeRide`);
  - the results (game-tick.js `host.leavePause()`).
- The sub-screens (Map, Yes / No, Options, Audio, Messages) push nothing.

**HOLD owners**, each popped by the code that pushed it:

| owner | pushed | popped |
|---|---|---|
| `WS1 ride-in` | the in-world event gate's ride-in (the world stops under the NIS; the music plays on) | startRun / stopRun: WS1 ends when the event's run starts or the world goes |
| `WS1 event transport` | the gate's transport branch | startRun / stopRun |
| `WS10 cutscene hold (<kind>)` | `cb.cutscene` with `o.hold` (arrivals) | the same call's `finally` |
| `WS11 transport (no NIS hold)` | transportInWorld when the rider is not posed (the NIS hold path is PS2-like: no hold) | the end of the ride / its `finally` |
| `WS14 station cut (no NIS hold)` | the 'stationCut' fallback | with the station's overlay (`drop`) |

- **QA:** `ssxQA.start()` opens the overlay on 'game' with a HOLD context (the simulation only, so the tools' cutscene probes still play), and `cb.resume` releases it. `ssxQA.advance()` steps through `host.paused` (`qaStepping`) whatever holds the world.
- **Safety net:** startRun / stopRun call `contexts.clear()` after their named pops and `console.warn` the owners of anything still open (a missing pop).

### cutscenes.js
- `update()` advances the NIS clock only if `!host.nisStopped()` (bit 0x08).
- While it is stopped, the fade advances its own clock (`seq.fadeT`). Once the NIS runs again, the fade clock advances with it.
- The update that first sees a stopped NIS runs it to its next whole tick (`Math.floor(t) + 1`), then holds it. The card's context is pushed as the idle starts, so the idle holds at t = 1.0 exactly, whatever the frame's dt. This matches the PS2: WS2 enter comes one tick into the idle.

## 2. Screen phases (state stack update 0x39ECB0, phase = state +0x1C bits 8..13): `web/screen-phases.js`

Phases: 2 build, 4 wait for the 0x42 activate record, 3 show / cursor restore, 5 active (the only one that reads input), 6 exit (TransitionOut 0x43: drawn, frozen, no input), 7 Stop (0x41).

- **The UI frame counter** steps at 60 Hz from the pad menus' rAF loop (`installPadMenus({frame})` -> `phases.advance(t)`), before the pad's keys. Pad, keyboard and touch are therefore gated in one place: ui.js's capture-phase keydown reads `phases.accepts()`.
- **Entering a screen.** `ui.set(screen)` calls `phases.enter(screen, previous)` on a change of screen.
  - A set from outside the pass (a key, the game frame's Start) builds on the current frame.
  - A set from an exit action inside the pass builds a pass later. That is E9's pop / restart pipeline, not a +2 constant.
  - Returning to a screen replays its intro (E10).
- **Locks.** `lock = introLockFrames(screen, previous)` (web/menu-rules.js data) with ps2MenuInput, else 0.
  - The card's Continue is refused outside phase 5 on every input path (career-ui.js `choose`), which replaces the wall-clock `cardOpenAt` guard. The card's draw animation (`cardTicks`) is still timed by the clock.
- **Frame accuracy:** the PS2 pad model calls `phases.step()` before each of its 60 Hz updates, so on a dropped display frame each key still meets its own frame's phase.
- **Exits.** `leave(screen, action)` runs career-ui.js `choose()`'s action through the outro (`PS2_CHOICE_OUTRO` data): 20 frames of phase 6, phase 7, then the action on the next pass. Presses during phase 6 are swallowed, and a screen change cancels the exit. No timers.
- **Measured** (unit test and browsers): MCOMM +60 dead / +61 on taken, Yes / No +23 / +31, Map +23 / +31, Rider Details +32 / +38, No -> the pause at +83 (+82 dead).
  - The MCOMM rule is activate + 1, from the LUI data. The PS2 measured +60 dead and +62 taken; +61 is not measured.

## 3. What went
- **Deleted:**
  - ui.js `inputLockUntil` and its wall-clock gate;
  - career-ui.js's `PS2_CHOICE_OUTRO` + `setTimeout` outro (`outroFor`) and the `cardOpenAt` Continue guard;
  - main.js's `paused` flag, `pause(p, {audio})`, the overlay's legacy halves and transportInWorld's `if(paused)pause(false)`;
  - cutscenes.js `idleFrozen` and its `seq.t >= 1` rule;
  - big-challenges.js's `pause` parameter;
  - pv `pauseContexts`, `cardFreeze` and `bcPromptAudio`.
- **Fixed on the way (finalpass "session" / "quit-save"):** the overlay screens ctm-quitsave, ctm-bcsure, ctm-session and ctm-sessconfirm played the front end's sound set (accept 0 / move 2 / error 4) instead of the overlay's 9..13.
  - The PS2 evidence: the Session's blocked end is snd 0xD (0x39B5E4), and 87yndialog's Triangle is kind 6 -> ev 9 (0x20DB64).
  - The fix: audio-menu.js PAUSE_SCREENS, and `sfx()` also reads that set.
  - ctm-gopeak and ctm-enterlodge probably need the same, but it is not confirmed from the code, so they are not added.

## 4. Verification (2026-09-29, the single path)
- **`node test-pause-contexts.mjs`** steps frames through:
  - E11: no tick on the open frame, frozen under the Map and the replayed MCOMM, no tick on the closing press's frame, a tick the frame after;
  - each screen's first accepted frame and the phases 2 -> 4 -> 3 -> 5;
  - Yes / No No -> the pause at +83;
  - the card's context 1 and the NIS's whole-tick hold;
  - the Big Challenge prompt through `createBigChallenges` (context 3, no audio pause);
  - an MCOMM Transport popping at the Yes;
  - the pad's per-update UI passes under a dropped display frame;
  - the stack's own rules;
  - that no legacy flag or switch is left in main.js.
- **test-ctm-flow:** the stub steps the phases, with blocks for Restart -> No +21 / +83, Quit -> Yes, and the card's Continue dead at +30 / taken at +31 with its context gone.
- **test-gamepad and test-start-rules:** both pad models, stepped at 60 Hz.
- **Related suites:** 33 in all, passing (ctm-*, cutscenes, big-challenges, audio-*, fe-screens, presentation, visual-parity, replay, rival, ...).
- **`npm test -- ps2-captures`:** passed, 255 scenarios at their baselines.
- **Browser, muted, ?mute=1, WebKit through webkit-driver** (local/ctm-decomp/screens):
  - `pauseprobe.mjs` (MOUNTAIN): 6/6 in Chrome and 6/6 in WebKit.
    - E11 on the UI clock.
    - An MCOMM Transport to Yellow (18): the context pops at the Yes, the ride ticks, it arrives with nothing held, and there are no leak warnings.
  - `cardprobe.mjs`: the gate idle holds at exactly t = 1.0 under the card, with the fade clock running, in Chrome and WebKit.
  - `finalpass.mjs`: 15/15 in Chrome; WebKit 13-14/15 from run to run.
    - The MCOMM checks now read the phase machine's own record. WebKit met the Down at d = 60 (dead) and d = 62 (taken), as Chrome did.
    - WebKit's remaining misses ("repeat 24/12" stick moves and "No+83") come from its headless rAF delivering a scripted press a frame late. The gate decisions themselves are exact.
    - Probe fixes:
      - frames are counted on the UI clock;
      - keys fire at or after their frame;
      - key-made changes are logged on the frame the key met;
      - the first MCOMM check now presses at Start + 60 (it pressed at +61).
    - "session" / "quit-save" were real port bugs (section 3).

## 5. Focus loss (2026-10-04): no auto-pause
The port used to open the pause menu on window `blur` and on `visibilitychange` to hidden. The PS2 has no such thing, and the
playtesters hit it where the PS2 takes no Start at all (WS10 / WS11 / WS14: the Transport ride, the gondola, NIS cuts; PS2 run
local/transport-stall/ps2/run1): the pause menu over the gondola with the race starting without the player, a stuck camera when a
cutscene's start was paused, the FAQ "jumping" to the pause menu. The user decided to remove it; the code path is deleted (no switch).

What main.js does on focus loss now (`releaseInput`):
- **blur, or hidden:** `clearInput()` (held keys, the touch deck, the key log) and `setPadsFocused(false)` (web/gamepad.js: every poll reads
  a neutral pad, no 'use' events, until focus returns; Chromium keeps feeding pad input to a visible unfocused window). gamepad-menus.js
  and cutscenes.js release their own held keys on blur as before. `?perf=1` keeps the input (automation windows lose focus).
- **focus:** `setPadsFocused(true)`. The first poll after it re-reads the rest state, so a button already down is held, not a new press.
- **shown again:** `last = performance.now()`: the frame clock restarts from now. A hidden tab gets no animation frames; without this
  the first frame back would run `ps2FrameTime` of the whole hidden time (up to 12 ticks, a PS2 stall rule that is not meant for this).
- **audio:** unchanged: web/audio-engine.js installAudioUnlock suspends the context while hidden (`suspend('hidden')`) and resumes it on
  show; a blurred visible window plays on.
- **online:** unchanged: a hidden tab's race keeps ticking from mp-game.js's worker timer (neutral input, since the keys and the pad are
  released) and follows the server clock (`pace`), so it does not desync or drop. Before, the blur pause put the online pause menu up
  over the running race with a neutral pad.

Every way into the pause overlay was checked against WS10 / WS11 / WS14: pad, keyboard and touch Start all go through
`startOpensPause` with `cutscene: cutscenes.active || transporting` and the stack's `paused` (WS1 / WS10 / WS11 HOLD contexts), the FAQ
and station opens are guarded as above, and no focus / visibility listener in any page module opens a screen or pushes a context
(test-pause-contexts "losing focus never pauses"). The countdown still takes Start, as on the PS2 (startprobe: Start pauses in the
countdown).

Tests: test-pause-contexts (the listeners of every module), test-gamepad (the unfocused pad), test-ctm-flow (the FAQ's way out).
Browser (scratchpad focusprobe.mjs; blur + hidden + visibilitychange with rAF held, as a hidden tab): the new career's arrival
cutscene, free ride, an MCOMM Transport ride, the round card, the countdown and a race: no pause, the stack unchanged, the ride ticking,
the frames after the return running one tick per 16.7 ms (no catch-up); Chrome 22/22.

The FAQ "?" that could not be left was a separate bug: career-ui.js back() had lost its Message Center / lodge / Big Challenge dispatch
to a reformat (it sat inside a comment), so Triangle / Escape / the touch deck's triangle did nothing on ctm-messages / ctm-message,
every lodge screen and the Big Challenge prompts. The FAQ view's list can only be left with Back. Restored.
