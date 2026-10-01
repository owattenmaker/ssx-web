# Online records (course records from the server, name entry, leaderboards, replays)

Status (2026-09-30): built behind `onlineRecords` in `web/pv-flags.js`, **on** since 2026-09-30 (host `MP_RECORDS_DIR` set); Chrome and WebKit checked; the server side
is inert until the host sets `MP_RECORDS_DIR` (docs/hosting.md "Online records"). The verifier (re-simulation of submitted runs) is
the second pass. SLUS_207.72, gp = 0x4A30F0. Recompiled code: `local/output/sub_<addr>.cpp` (the MIPS is in the comments).

## 1. What the PS2 does (from the code)

### The records table

- **Storage:** 26 slots x 5 entries at 0x535C18 (slot stride 100 = 0x64, entry stride 0x14), copied from the defaults at
  0x43FB28 (exported as `career.json rules.records`). Entry: `+0 u32 value`, `+4 u32 character`, `+8 char name[12]`.
- **Slot of an event:** 0x14AB20(course, mode) -> 0x14AB68: course >= 17 -> 26 (none); modes 6..11 (the peak runs) -> slots 0..5;
  else table 0x45A2F8 (two {mode, slot} pairs per course) -> 26 when neither pair matches. 0x154AB8 first maps course id 0x16 to 0
  (`xori / movz` at 0x154CAC). Result (`web/career.js recordSlot`, already ported): Snow Jam..Gravitude races slots 12..16,
  slope style R&B / Style Mile / Kick Doubt 17..19, big air Crow's Nest / Launch Time / Much-2-Much 20..22, super pipe The Junction /
  Schizophrenia / Perpendiculous 23..25, the backcountry rivals Happiness / Ruthless / The Throne Rival Time 6 / 8 / 10 and Rival
  Points 7 / 9 / 11, the peak runs 0..5.
- **When a run is entered:** 0x238358 (event over for one player; called from the finish, 125108) -> 0x23843C `jal 0x154AB8`
  (results object, player, source, value = `*(0x536640 + 4 x player)`). Gates on the way:
  - 0x2383D4: the course table row's id < 17 (0x144BC0);
  - 0x2383E4..0x2383FC: not while a replay plays (replay manager `*(G+0x84)+0x28`, state 1..9 skips; 0 or >= 10 records);
  - 0x154D14 -> 0x1557E0(result, player): player < 2 (human players only; split-screen players 0 and 1), and 0x14F810(0) == 0:
    no Single Event rule active. 0x5308B8 is the rules block (labels table 0x441018: Super AI, Head Start, Start event with full
    boost, Unlimited boost, No boost, KO boost only, No uber tricks, No uber rails, No knockdowns, Multipliers, No point icons,
    Power-ups, No checkpoints, Leak adrenaline; setter 0x192088). 0x14F810 returns 1 if player 0's three rule words (+0/+4/+8)
    or the shared words +0x18 / +0x1C are non-zero. The port has no Single Event rules screen, so this gate is always open.
  - No Conquer the Mountain check: CTM (0x5305F9 == 0) and Single Event (1) both enter records. 0x5305F9 only gates the medal
    call 0x152460 at 0x154E84.
  - Every heat, not only the final (0x238358 runs at each finish; PS2 ctm-left runs show it for a qualifier).
- **Value:** time events when the event kind byte 0x5305F8 is 0 (race) or 5 (time challenge) (0x154CB4..0x154CC4, the port's
  `isTimed`): `value = cvt.w.s(f32(ticks) * f32 0x3C888889)` (gp-0x6DEC = 0x49C304 = 0.016666668), i.e. **whole seconds,
  truncated**. Score events: the score as is.
- **Rank (0x154D58..0x154E44):** walk the 5 entries; time: skip while `entry.value < value` (0x154D68 `sltu`); score: skip while
  `value < entry.value` (0x154D6C). The run goes in at the first entry it is **not strictly worse** than, so **a tie ranks the new
  run above the old one**. Entries below shift down one (the 5th drops). The entry gets the value, the character
  0x14A080(player) = setup slot `0x535B20 + 0x1C i + 0x11` (the **base** rider: a cheat skin is +0x12, so the record names the
  base rider) and the name `strcpy(entry+8, 0x147170(profile, player))`.
- **The rank** goes to results +0x18 + 4 x player (-1 when not entered); with two players the other player's rank moves down
  (0x154DF4..0x154E34). 0x20A8F8(7) opens overlay 0xF (61toptimes, "Top 5 Record Times / Scores") before the results when the
  rank >= 0, in every event (career-ui.js `topTime`).
- **The name** (0x147170): the profile's player name at `0x534FE0 + 0x1C x player`; an empty one becomes
  `sprintf(kT_MEMPlayerName "PLAYER %d", player + 1)` and is written back. **There is no name entry at a new record on the PS2**:
  the name is the one typed earlier in Rider Details > Player Name. The record field holds 11 characters + NUL; the keyboard
  limits names to 8.
- **The keyboard** (cKeyboardPopup 0x1CB030, LUI Fullkeyboard; `web/fe-screens.js`): 8 characters (1CD088(kb, 8) at 0x1837A4 /
  0x1F4494); a letter at the limit replaces the last one (1CD2F0); Tab, Up, Down off (0x1CE3C8) and, for Player Name, the
  punctuation keys ~ - = [ ] | : " < > ? off (0x1837C4..0x1838A0). So a name is letters (Caps / Shift), digits, the shifted
  digits ! @ # $ % ^ & * ( ) and space.
- **The records screen's menu** (61toptimes): after a new record, Continue / Save Records (Save Records writes the memory card);
  from the results' Records item, Return.

### Not confirmed from the code

- What a Give Up / TIME'S UP run enters. 0x125108 (the finish, which calls 0x238358 at 0x1251B8) is also reached from the forced
  finishes 0x12B340 (0x12B3E0) and 0x12BB20 (0x12BC48 / 0x12BD78), which set rider +0x480 first (the DNF / time's-up flag), so the
  PS2 most likely runs 0x154AB8 for them too; the value 0x536640 holds then (a DNF time?) was not captured. Online, such runs are
  never submitted (the replay's `giveUp` call or a DNF result keeps them out).
- What the timed test reads for the peak runs and rival time (0x5305F8 is the event kind; the port's `isTimed` maps modes 0, 4,
  6..8 and was matched on the PS2 screens, not re-derived here).

### Found while reading: the port's local table is not the PS2's rule

`Career.addRecord` keeps ticks and inserts on `value < entry.ticks` (strict). The PS2 compares whole seconds with "not worse":
a 02:57.50 run against BOMBER's 177 s enters at rank 1 on the PS2 and not in the port. To be fixed with the online records
(behind the same switch, the local table follows 0x154D58).

## 2. What was built (pv onlineRecords)

Decisions (coordinator, 2026-09-30): D1 online submissions only from Single Event and load-screen Conquer the Mountain events (in-world
CTM events and the peak runs keep the local PS2 table); D2 one entry per name per event, the best kept; D3 rank by ticks, then the
earlier submission (the top-5 screen still shows mm:ss); D4 every board seeded with the PS2 defaults as replay-less rows; D5 the
verifier is a headless-Chrome job running the page's own replay path (second pass); D6 listed at once, pulled if verification fails;
D7 a replay recorded on another core plays on the current one with a note, and the verifier re-checks entries after each core deploy;
D8 the order below.

### Deliberate additions (not on the PS2)

- **61toptimes' third item "Online Records"** (the screen has Continue / Save Records, or Return): a copy of Save Records one 15-line
  pitch lower with its own focus frame 50 (web/results-lui.js `topTimesThree`).
- **Save Records uploads**: on the PS2 it writes the memory card (the port saved at once and greyed it). Now it opens the game's
  keyboard (cKeyboardPopup) prefilled with the Player Name, with Player Name's rules (8 characters, 1CD2F0's overwrite, the punctuation
  keys off), and Done uploads the run. The PS2 has no name entry at a record; its record takes the Player Name (0x147170).
- **The full board** (`ctm-board`): 61toptimes again, "Online Records" as its subtitle, 5 rows a page with their rank numbers, the
  focused row white, your own rows in the record row's orange, "Page n of m" in the help line, Watch Replay / Return. Up / Down move
  the row, Left / Right or L1 / R1 (Q / E) page, Cross watches, Triangle returns. Times with hundredths (the results' format).
- **Main menu "Leaderboards"**: a sixth row under Online, built from 07main_men's own Online row (one pitch lower in every state,
  black, its own focus state frame 75 with the bar and a help text one pitch lower; web/fe-main-menu.js `withLeaderboards`). It walks
  the Single Event Select Peak / Mode / Event maps with every ported peak open and every event with a course selectable (the peak
  runs greyed), then opens that event's board over the front end's sky blue.
- **Watch Replay**: the board's run plays in the full replay ('64replay', docs/replay.md), loaded like a Single Event (the original
  load screen, no intro), and Exit replay returns to the board.

### The records screen (web/online-records-ui.js, driven by web/career-ui.js)

- A finished run (career-ui `finish`) of an event with a board, not in-world, with its replay, not a Give Up or DNF: its rank in the
  online top 5 (`rankAmong`: strictly better than the row it passes; your own listed name keeps a run out unless it beats it) decides
  whether 61toptimes opens first (the local table is still entered, as on the PS2: 0x154AB8 rules, now exact, see 1.).
- The rows are the online top 5; before the upload the run sits at its rank in the record row colour; after it, the server's rows
  with the run's entry coloured. The message line says "Congratulations, ..." (PS2), then "Your record is number N online." / "Your
  best time under this name is already listed." / "Online records unavailable.".
- The objectives card's "Record time:" and the INFO panels' "Top time:" (Select Event, the MCOMM Transport) show the board's first.
- Offline: the last good boards from localStorage (`ssx3.onlineRecords.v1`, status 'cached'); none ever loaded: the PS2 table as
  before, Save Records greyed, "Online records unavailable.".

### The replay file (web/server/replay-file.mjs, web/online-replay.js)

`'SSXR'`, version 1, a JSON meta and the pad stream of web/replay.js `createRecording` (`exportBytes` / `importBytes`), uploaded
deflate-raw. The meta: event, mode, course, round, name, claim (ticks or score), recorded ticks and finish tick, the out-of-band calls
(Give Up, camera), the highlight buckets, main.js `snapshot()` (time limit, input map, camera option, collectible rows, relationship
tables after the start's ageing, the shared RNG, the visual RNG, gp+0xA0C), the lineup (ai-race `lineup`: the roster values or the
freestyle opponent or the rival, the round, and the relationship tables the lineup was assembled with), the human (rider id, a cheat
skin's base, the outfit key of web/wardrobe.js `outfitKey`, the 7 attribute bytes main.js set at the run's start, the uber rows), the
core (16 hex of core.wasm's SHA-256) and the build id. Sizes: a scripted tuck run on Snow Jam (15,000 ticks) records ~1.1 KB of pad
stream; an analog stick moving every tick ~26 KB a minute (synthetic), deflated roughly in half; the meta is ~4-6 KB of JSON.

Watch Replay (`ui.cb.watchOnlineReplay`): download, resolve the rider like a remote racer (`remoteOutfitRider`), set it as the human
with `replayFixed` (main.js `riderStale` leaves it alone), switch the course when needed (`navigateCourse(url, {after: 'replay'})`),
give ai-race the fixed lineup (`fixedNext` -> `prepareFixed`), run the warm-up under the load screen, apply the attribute bytes,
`replay.load()` the downloaded run and start the full replay. Exit replay: the replay stops, the viewer's rider and modes come back,
the page's own run replay is cleared (the core holds the uploader's rider and lineup now: its Replay item greys), back to the board.
A run recorded on another core shows "Recorded on an earlier version of the game." under the replay bar (D7).

### The server (web/server/records.mjs, mounted by mp-server.mjs)

Imports only node: and web/server/; loaded with a dynamic import only when `MP_RECORDS_DIR` is set; any failure leaves the records
off (503) and the rest running. Storage: `<dir>/board.json` (written tmp + rename) and `<dir>/replays/<id>.bin` (the uploaded bytes).
Boards: modes 0-5 of the 17 courses with a record slot (career.json `record_slots`), seeded with the slot's defaults. API and rules:
see the module header. Tier-0 checks: the file decodes (inflate capped at 4 MB, upload 512 KB), the meta's event matches, the name is
the keyboard's characters (1-8, a small blocklist), the claim is a positive integer, no Give Up / DNF, the pad stream walks and ends
by the finish tick, recorded ticks = finish tick + 1, the race clock is within 400 ticks of the recorded ticks, a race is not under
the course floor (route length / plausibility.mjs MAX_AVERAGE, from `<code>/npc-riders.json`), at most 64 pad bytes a tick, a
character 0-9, a hex core id. Rate limits per client address, in memory only: 6 a minute and 30 an hour. At most 100 runs per board;
the slowest drop with their replays. Nothing about the client is stored; `at` is the UTC day.

### Not done / open

- The verifier (D5, second pass): `records.verify(id, ok)` and the `verified` field are in place; the job (headless Chrome on the
  host running the page's replay path in a verify mode, outside the server sandbox) is not built. Until then entries are listed with
  only the tier-0 checks.
- Runs in-world (pv eventInWorld, off today) and the peak runs are not submittable (their replays need the streamed world's
  snapshots).
- Single Event outside the career leaves the core's attribute bytes as the last career run set them (main.js `runAttributes` returns
  null there; the PS2 uses the profile's bytes in every mode). The replay file carries the bytes in effect, so playback is exact
  either way; the rule itself is a separate fix.
- career-ui.js line "record=...findIndex(r=>score>r.value)" (the freestyle finish panel's new-record flag) still compares strictly;
  what the finish panel reads on the PS2 was not traced.

## 3. Tests

- `web/test-records-server.mjs` (npm test): boards and defaults, submit / rank / ties / one entry per name, bad input (event, name,
  claim, floor, Give Up, finish tick, rider, core, pad stream, garbage, inflate bomb, size), rate limits, the 100-run cap and its
  replays, persistence across a restart, the verifier pulling an entry, the HTTP mount (503 when off, 401 behind the gate), and the
  deployed layout: every web/server module imports only node: / web/server/, and a copy of web/server/ alone starts with the records
  on, on with the tables missing, and off.
- `web/test-online-records.mjs` (npm test; Chrome with WebGPU and the game data, else skipped): a Single Event Snow Jam tuck run to
  the finish against a local records server whose Snow Jam defaults are poked to 15:00-15:04 (as the PS2 captures poked them); the
  records screen opens with the three items and the run first; Save Records' keyboard (the Player Name, 8 characters), typing "OWEN",
  Done; the server lists it first with its replay; the focus moves to Continue; Online Records' board (a default row's Watch Replay
  greyed); Watch Replay: every replayed tick's `?simtrace` equals the live run's to the finish (15,000+ ticks); Exit replay back to
  the board, the page run's own Replay greyed; the main menu's sixth row, the Select Event maps, the board over the front end; Watch
  Replay from Metro-City (a course switch) exact for 1200 ticks; a viewer on Psymon watching Zoe's run exact for 600 ticks, Psymon back
  after; the server gone: the cached boards, then 'offline'. `SHOTS=dir` saves the screens.
- WebKit (web/webkit-driver.mjs, scratch script `web/.online-records-webkit.mjs`): the same run, upload, board, Watch Replay (2000
  ticks exact), Exit, the main menu and the Leaderboards board; screens checked by eye against the Chrome ones.
- Full npm test 208/208 with `SSX_PV=onlineRecords` (2026-09-30). Without it: 206/208, the two failures from other agents' edits in progress at the time (test-mountain-world: a SyntaxError mid-edit, passes again; test-world-warm: pv worldWarm retired while the test still passes `-worldWarm`), unrelated to this feature. With no board loaded (no server) the records screen and decision are exactly the PS2's (test-ctm-left covers both switch states and the online no-upload case).
- With the switch off: test-replay, test-ai-racers, test-lineups, test-career, test-career-rider, test-ctm-left (new: the whole-second
  tie rule), test-fe-screens, test-fe-attract, test-mp-gate pass.
