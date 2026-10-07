# TAS: Metro City race (BRA2)

A tool-assisted run of the Metro City race (BRA2, Peak 1) as a replay the port plays back with the game's own Watch Replay. Tools
in `tools/tas/`, bulky outputs in `local/tas/` (not tracked). SLUS_207.72, gp = 0x4A30F0, recompiled code in `local/output`.
Nothing here is posted to the online boards or the records server.

Status (2026-10-07): the node harness is exact against the page and the search runs. The best time is in "Results" below.

## 1. The rules (from the code)

### Medals and platinum

The award runs once per finished event:

- **The call.** 0x238358 (event over for one player, called from the finish 0x125108) calls 0x154EE8(results, player,
  place = 0x536730[p], value = 0x536640[p]) only when the handler's +0x9C is set (0x2384BC). That flag means the event is
  complete: the career Final, or a rival challenge's round.
- **The medal.** 0x155328 gives it from the place: 1st gold (1), 2nd silver (2), 3rd bronze (3), otherwise none (-1). In the
  rival modes 4 / 5, only the winner gets gold (0x154FA8..0x154FC0).
- **Platinum is Conquer the Mountain only.** With no medal, 0x154EE8 returns. At 0x155044, when 0x5305F9 != 0 (a Single
  Event) it returns before the platinum test and before every award. A Single Event therefore shows no medal at all; the
  finish panel's medal comes from the record obj 0xD that 0x1591E8 writes.
- **Event kind 4.** At 0x155050, event kind 0x5305F8 == 4 skips platinum.
- **The threshold.** 0x1456A0(course, mode) returns -1 for modes 6..11. For mode 0 (and 4) it returns
  `lh 0x440E80 + 4 x course + 2`, in seconds; other modes read the same entry x 100 as a score.
  - Read from the ELF: Snow Jam 150, **Metro City (course 1) 150**, Ruthless Ridge 160, Intimidator 165, Gravitude 160.
- **The test.** 0x155090..0x155098 is `slt (60 x T) < value; movz medal, 0`: **platinum when value <= 60 x T ticks**, on top
  of any medal. value = 0x536640[player] is the rider's finish time +0x478 in ticks.
  - For Metro City: a CTM Final, top 3, with the finish at 9000 ticks (2:30.00) or less.
- **The port has the same rule.** web/career.js completeEvent / platinumThreshold (ev.career only) and placementMedal.

The user's target is the time gate, not the medal screen: 9000 ticks (2:30) first, then about 2:00 (7200), with Kelecat's 1:51
as the stretch goal. The TAS is a **Single Event** run (decision 2026-10-07), so the medal screen does not apply.

### Records and the timer

- The records and the results show whole seconds. The record value is `cvt.w.s(ticks x 0.016666668)`, truncated
  (docs/online-records.md).
- The HUD race clock (0x21F81C, owner +0x17C..+0x184) and the FINISH banner (0x21F660) show "%02d:%02d:%02d" (0x46ED68) as
  hours, minutes and seconds of the finish time 0x536640 (docs/visual-parity.md: 14129 ticks = 00:03:55). Kelecat's
  "FINISH! 00:01:51" is therefore whole seconds.
- In ticks, Kelecat's 1:51 is 6660..6719 race ticks. To beat it outright: under 6660.

### Event, rider, stats, booths

- **Event: Single Event Metro City race.** There are five computer riders. The lineup is the page's roster build at the
  event's load, and the replay file carries it. Round 3 (0x23A174 forces it in a Single Event).
- **Rider: Mac**, as Kelecat rides him (his results and Top 5 list name マック).
- **Stats: maxed.** The seven profile attribute bytes are 55, i.e. level 11. Raw values run 5..55, the cap is the CHARDB
  limit byte 0x5308D8 + char x 15 + 8 + k (11 for every rider), and stat = int(raw / 5) / 11 (docs/career-events.md "Buy
  Attributes"). The user allowed maxed stats.
  - careerUI.play() hands the bytes to the physics in every mode. The PS2's stat getters read the profile bank 0x535538 in
    Single Event too.
- **No booths.** Metro City's phone booths and the water tower are teleports: stage builtin 34, web/stage_teleport.inc, core
  `stage_teleport_info`. Kelecat's category does not use them, and the search rejects any candidate whose teleport count
  changes.
- **Resets are allowed.** Kelecat's run sits at 65-66 % from 1:08 to 1:16, a white fade follows, and he is at 76 % at
  1:17. That looks like a reset, which is part of his category (coordinator, 2026-10-07).
  - The reset places the rider on the retained reset route: 112D58, then 11D660's placement (docs/reset-recovery.md).

## 2. The reference run

Kelecat, "SSX 3 - Metro-City Race (No booths) - 1:51 (WR)": the GameCube JP version, a Conquer the Mountain Qualifier
(予選), Mac. The results list the computer riders at 3:00..3:23. FINISH 00:01:51.

His progress meter, one sample per in-game second (frames from the user's download, local/tas/video/):

| s | % | s | % | s | % | s | % | s | % | s | % |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 5 | 2 | 10 | 6 | 15 | 12 | 20 | 15 | 25 | 23 | 30 | 26 |
| 35 | 34 | 40 | 38 | 45 | 44 | 50 | 48 | 55 | 53 | 60 | 56 |
| 65 | 62 | 70 | 65 | 75 | 66 | 77 | 76 | 80 | 77 | 85 | 81 |
| 90 | 85 | 95 | 88 | 100 | 91 | 105 | 95 | 110 | 99 | 111 | 100 |

- The percent is the HUD's (349050.6 - rider+0x4D0) / 349050.6. The progress steps at most 138.9 cm a frame unless the rider
  was placed (web/progress-meter-hud.js).
- The strategy shows on the HUD:
  - a 1080 with a grab off the first kicker (9500 points by 0:05);
  - rails;
  - the SUPER UBER letters lit from about 0:21;
  - 120 km/h most of the way.

## 3. Speed: what limits it

- **The speed limit.** 11B3F8 (engine/ground_motion.cpp originalGroundSpeedLimit) blends the profile's
  `speed_limit_table`, which is in km/h, by:
  - the top-speed stat;
  - the surface terminal band;
  - the crouch (the tuck: left stick up);
  - the boost amount tier (+0x2FC >= 0.1 / 0.4 / 0.8).
- **The maximum** is 120 km/h (33.3 m/s): max stat, crouched, high terminal surface, boost amount 1.0. Without boost a
  crouched rider tops out at 100 km/h.
- **Boost (114130).** Square with a meter > 0. The amount is 1.0 when the meter is > 2/3, 0.625 when > 1/3, else 0.25. It
  drains 0.0015068 a tick (a full meter is about 11 s of boost).
- **The meter.** 10E098 awards trick points into it, at about 10,000 points per full meter. A full meter starts Tricky: Super
  time 20 s, tier 0 -> 1.
- **Ubers.** In Tricky, an Uber is a grab combination held with Square in the air, and each committed one adds a tier.
  - At **tier 10, Super Uber**: Super time is 60 s and the meter is locked at 1, i.e. unlimited full boost.
  - Then tier 5 with 20 s of Tricky.
  - Several Ubers in one air all count (docs/tricks-scoring.md).
- **Tricks need a jump.** Riding off a lip is the passive air (control 4): grabs only. Spins and flips need the ollie: Cross
  held and released, then control 5. The spins and flips are on the **D-pad** in the air (INPUT.MAP Spin = DPadR - DPadL).

## 4. Tools

| File | What |
|---|---|
| `tools/tas/pad-format.mjs` | the pad format (below), its conversion to the 24 pad channels |
| `tools/tas/page-run.mjs` | plays a pad live in the real page (headless Chrome via web/headless-chrome.mjs startBrowser, --mute-audio, ?mute=1) as the Single Event; writes start.json, dump.json (per tick), trace.json (?simtrace), replay.ssxr, run.json |
| `tools/tas/race.mjs` | the page's race in node from a start state, exact; chunked save / restore |
| `tools/tas/compare-page.mjs` | node against a page-run dump, tick by tick |
| `tools/tas/policy.mjs`, `macros.mjs`, `guide.mjs` | the closed-loop policy the search's macros drive, the macro menu, guide lines |
| `tools/tas/search.mjs`, `search-worker.mjs` | the segment beam search, worker threads (3 at most) |
| `tools/tas/eval.mjs` | runs a pad in node and reports progress, speed, boost, crashes, resets, teleports and the finish |

### Pad format

`# ssx3-tas 1`, then `<tick> <32 hex digits> <lx> <ly> <rx> <ry>` lines, each holding until the next line, and `# end N`.

- **The hex digits** are 16 pressure bytes in web/pad-input.js PAD_BUTTONS order: Select, Start, L3, R3, D-right, D-left,
  D-up, D-down, Triangle, Circle, Cross, Square, L1, R1, L2, R2. Select..R3 are digital (00 / ff).
- **The sticks** are the DualShock's bytes: 128 is centre, 0 is left / up.
- **To the replay.** toChannels() applies the same pressureChannel / stickChannels arithmetic as web/pad-input.js, so every
  value is a pad byte. web/replay.js records it losslessly.
- **To the PS2.** tools/ps2_capture.py segments press buttons at 255 and take stick floats v with axisByte(v) = the byte. The
  TAS uses only 00 / ff pressures so that the same bytes drive the PS2.

### The node harness (exactness)

`race.mjs` sets the race up as Watch Replay does (web/online-replay.js begin / load / play, main.js startRun(R)):

- the course init in the page's order, including what web/set-pieces-renderer.js puts in the core: the sections, the stage
  world, the flags and the avalanche clear. Without these, a computer rider left the page at tick 1184 and the shared RNG at
  1314;
- the rider's settings (character-roster.js humanSettings) and rig;
- the lineup through ai-race.js prepareFixed;
- the attribute bytes;
- the replay snapshot's time limit, input map, camera, collect rows, relationships and the RNG / visual / LCG words.

The tick is web/game-tick.js simulate on the aiActive path, without the presentation. `compare-page.mjs` against a page run
(2000 ticks, a pad with steering, tuck, boost and jumps) is exact for the human's rider state, the shared RNG and all five
computer riders' rider state on every tick.

Speed: 0.7 ms per tick with the six riders on an idle machine. Riding alone is 0.12 ms, but the human then leaves the six-rider
run at tick 2824 (the shared RNG), so the search always runs all six.

## 5. Search

- **Macros** (tools/tas/policy.mjs) are closed loop. Steering follows a guide line (a computer rider's recorded line) with a
  lateral offset and lookahead. The tuck is on the ground, and boost is off, on or auto (meter > 2/3 or Super Uber). A jump is
  Cross held, with an optional D-pad prewind, and the next macro releases it.
- **Air programs** are timed by air tick and armed for the next take-off: a spin or flip on the D-pad, grabs, and Ubers
  (grab + Square) in Tricky. The stick stays centred in the air.
- **Beam search** over segments of 20 ticks:
  - each node tries 16 macros, each followed by 180 ticks of its continuation, all valued at the same tick;
  - the 3 best are kept;
  - value = progress (m) + speed x 1 s + meter x 45 + tier x 30 + Super time x 5 / s - 40 while crashing; a finish beats
    every other value.
- Work goes to 3 worker threads, each with its own race; the workers' start memory is checked to be identical.

## 6. Results

(filled in as runs finish; the replay paths are under local/tas/)

## 7. Proof

(page Watch Replay in Chrome and WebKit; ARMSX2 mode 1 from the matching start state)
