# Online records (course records from the server, name entry, leaderboards, replays)

Status: decomp done, design sent to the coordinator 2026-09-30, build pending its reply. Switch (planned): `onlineRecords`
in `web/pv-flags.js` (default off). SLUS_207.72, gp = 0x4A30F0. Recompiled code: `local/output/sub_<addr>.cpp` (the MIPS is in
the comments).

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

- Whether a Give Up / TIME'S UP run reaches 0x238358 (the finish path 125108 was traced for real finishes only).
- What the timed test reads for the peak runs and rival time (0x5305F8 is the event kind; the port's `isTimed` maps modes 0, 4,
  6..8 and was matched on the PS2 screens, not re-derived here).

### Found while reading: the port's local table is not the PS2's rule

`Career.addRecord` keeps ticks and inserts on `value < entry.ticks` (strict). The PS2 compares whole seconds with "not worse":
a 02:57.50 run against BOMBER's 177 s enters at rank 1 on the PS2 and not in the port. To be fixed with the online records
(behind the same switch, the local table follows 0x154D58).

## 2. Design (proposed; see the HANDOFF entry for what was built)

To be filled in once the coordinator has decided (sent 2026-09-30): online top 5 per event, name entry through Save Records,
the full board from the records screen, a main-menu Leaderboards entry through the Select Event maps (a deliberate addition:
the PS2 main menu has no such item), uploads with the run's replay, verification by re-simulation, core-hash handling.
