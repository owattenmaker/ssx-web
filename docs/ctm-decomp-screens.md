# Conquer the Mountain screens and menus: PS2 decomp against the port (2026-09-29)

Owen: CTM "doesn't quite feel like the real full game", and the menus feel a little off. This doc covers every CTM screen,
overlay and menu of SLUS_207.72, decompiled far enough to say what the port does differently:
- the MCOMM and the pause family, and their Yes / No popups;
- the Transport map and Session;
- the lodge and its sub-screens, and the memory-card screens;
- the round card, finish panel, results, records, reward list and replay menu.

Method (AGENTS.md): code first, with addresses, then PS2 emulator parity. The PS2 side was captured with silent ARMSX2
runs from derived savestates: scripted pads, the memory values and the UI sound calls logged per pad sample. The port side was
measured headless: Chrome with --mute-audio and ?mute=1, and WebKit through web/webkit-driver.mjs, with a fake pad played frame by frame.
Screenshots were used only as supporting evidence.

Status words: **match**, **differs**, **missing**, **unconfirmed** (the code does not settle it, or it is not captured).
"pv X" is a switch in web/pv-flags.js. A rule behind an off switch is "differs" in the shipped port. Port line numbers are
as of 2026-09-29 around 11:00. The coordinator was landing fixes in ui.js, career-ui.js, gamepad-menus.js, audio-menu.js and
the new web/menu-rules.js during this work, so some numbers will have moved.

**Already fixed while this was written**, all behind **pv `ps2MenuInput`** (off; web/gamepad-menus.js, web/menu-rules.js,
ui.js, career-ui.js, audio-menu.js, big-challenges.js). They were checked against the PS2 numbers with the probes below
(section 8):
- the 24 / 12 repeat and the edge debounce;
- the stick threshold;
- held-key repeat;
- wrap versus error-at-end;
- the intro input lock and the Yes / No outro;
- the cursor landings after Give Up, Restart and the results confirms;
- the Big Challenge pause's "Are you sure?";
- Triangle on "Save progress before quitting?" and on bc_start / bc_fail.

The Chrome silent-move bug (ranked item 4) is fixed with no switch.

## Ranked differences (what a player feels most first)

Input feel and timing first. "Fixed (pv ps2MenuInput)" means the fix exists behind the off switch and was checked against the PS2.

1. **No screen intro / outro input lock** (every screen). Fixed (pv ps2MenuInput), except item 1a.
   - PS2: a screen takes no input at all until its LUI timeline reaches the 0x42 "activate" record, and none once 0x43 "TransitionOut" has played (E8, E9). Per-screen windows are in section 1.3.
     - MCOMM / pause: 61 frames (captured: +60 dead, +62 taken).
     - Yes / No: 31 frames, plus a 20-frame frozen outro after the choice. No -> pause input takes 82-83 frames (captured).
     - Map 31, Session 49, lodge prompt 39. Cards, results, records and rewards 31. Finish panel 46. Replay menu 40.
     - Messages / Audio 61, in-game Options 51. The lodge's FE screens take about 27 frames from their switch (press + 38 after the flash).
   - The MCOMM replays its whole intro, lock included, every time it comes back from a sub-screen (E10).
   - Port (shipped): input is taken about 10 frames after any screen appears, and popups close on the keypress. A double Start closes the MCOMM at once, and a held or repeated Cross can act on the next screen.
   - 1a. Under the switch, the lodge FE lock starts at `ui.set`, which is the flash's switch at about press + 11, and runs 40 frames. So Rider Details is still dead at press + 40, where the PS2 takes it.
     - Fix: 27 frames from ui.set for ctm-lodge / details / attributes / gear / trophies / rewards / uber. Files: web/menu-rules.js.
2. **Menu auto-repeat is 1.8x too fast on the pad, and absent on the keyboard** (E1, E2). Fixed (pv ps2MenuInput).
   - PS2: 0 / 400 ms / every 200 ms.
   - Port: gamepad-menus.js repeats every 110 ms; ui.js:52 drops `e.repeat`, so a held arrow key moves once.
3. **Menus clamp silently instead of wrapping, or instead of the error sound** (E5, E6). Fixed (pv ps2MenuInput, web/menu-rules.js).
   - Every CTM menu wraps on the PS2 except the Map's goal and event lists, both Map / Session confirms, the Session list, the message folders and the reward list. Those play the error (snd 0xD) at the ends.
   - The port's generic path (ui.js:52) clamps with no sound.
4. **Chrome played no move sound on the generic menus** (MCOMM, Yes / No, lodge questions ...). Fixed, no switch.
   - Cause: Chrome runs window at-target listeners in registration order, so the watchSounds capture listener (audio-menu.js:431) took its "before" snapshot after ui.js had already moved the index.
   - Fix: ui.js now calls `ui.preKey(e)` first. WebKit was already right.
5. **The finish panel can't be skipped.**
   - PS2: finishov (overlays 0xC / 0xD) takes Cross / Start from its activate frame 45 (notify 0x1E8160 / 0x1E8880, +0xA0 input flag). The results can therefore come about 226 frames after the finish instead of the natural ~406.
   - Port: always 408 ticks (game-tick.js:50, :121-126).
   - Fix: Enter / Space from finish + 180 + 46 ticks, rider not given up, should end the wait. Silent: the menu is silent (0x1E7C4C). Files: game-tick.js, main.js / ui.js key path, career-ui.js.
   - Evidence: code. Indirectly, the startprobe run's records screen came up by sample 280 after a Start at 240. A direct timing capture is still open.
6. **Edge debounce: quick double taps are dropped on the PS2** (E3). Fixed (pv ps2MenuInput).
7. **Triangle does things the PS2 ignores.**
   - Results' Restart confirm Triangle **opens the pause menu over the results**.
     - PS2: 0x20DB64 -> TransitionOut -> back to the results.
     - Port: back() maps ctm-restart -> ctm-pause (career-ui.js:385).
     - Fixed (pv ps2MenuInput: Triangle = No, then the results with their default focus).
   - Round card Triangle leaves the loaded event (ctm-objectives -> ctm-events / event; career-ui.js:385).
     - PS2: dead (query 0x20D308 / 0x1FDB78, notify handles Cross only).
   - Records Triangle goes back to the results. PS2: dead (query 0x1F80C8).
   - Lodge questions ("Save progress?", "Quit to Title screen?", "Save progress before quitting?"): the port goes back to the lodge with the accept sound.
     - PS2: nothing, silent (cFEPopupConfirm 0x1CA3EC: popup+0x310 zeroed at 0x1D9A80; the lodge handler 0x1F3CF0 ignores 0x14).
   - The save overwrite popup (save-game.js:80) and the keyboard's pad Circle (Backspace, fe-screens.js:612).
     - PS2: Triangle dead (+0x310 = 0); Circle eaten (0x1CDBC0).
   - Files: web/career-ui.js back(), web/save-game.js, web/fe-screens.js.
8. **Transport cursor forgets the chosen event.**
   - PS2: after "Transport to this area now?" No / Triangle, the event list comes back on the chosen row (0x2021E4 / 0x201CF4).
   - Within one map visit, every list keeps its cursor: setupMenuFocus 0x2030A0 never sets an index. A reopen builds a new overlay.
   - Port: row 0 after the confirm (career-ui.js:340, :385), and goals / events always open on row 0.
   - Fix: a per-visit cursor memo, cleared when the map opens. File: web/career-ui.js.
9. **"Go to this peak now?" asks for the wrong peaks.**
   - PS2: only for Peak 2 / 3 whose backcountry visited bit (course 15 / 16, 145D38 at 0x2019E8) is clear. A visited peak opens its goals, and any event there transports directly.
   - Port: asks for any peak other than the ridden one (career-ui.js:336).
   - File: web/career-ui.js, behind a new switch.
10. **The save flow is skipped.**
    - PS2: the full Save game screen follows each of these:
      - lodge "Save progress?" Yes (0x1F3EB4 -> cFEStateProfileLoad mode 3, +0x27C = 1);
      - "Save progress before quitting?" Yes (0x1F3EF0, +0x27C = 2);
      - the MCOMM Quit save (OV 121Profilesave_pda, 0x211088).
      The Save game screen runs Checking, then the list, the keyboard, overwrite?, Saving (at least 60 frames) and Save complete. Continue or Triangle then goes to the station load / the title.
    - Port: persist() and on (career-ui.js:345, :353).
    - Files: web/career-ui.js, web/save-game.js (a PDA variant for the MCOMM quit).
11. **Buy popup Yes / No acts on the keypress (a double action).**
    - PS2: 139buy_popup plays TransitionOut 50 -> 59 before the parent gets message 0x16, and it is dead until frame 30 + 2.
    - Port: buys and closes at once, so a quick second Cross hits the now-owned item (it opens the poster).
    - Files: web/fe-options.js:440, fe-screens.js:540, buy-attribs.js choosePopup.
12. **The results cursor after a return.**
    - PS2: every return rebuilds the results with the default focus: 0 in a career, 1 in a Single Event (43 only, 0x1E7558). That covers Restart / Quit No or Triangle (0x39F190, then 0x39EA90(8)), Exit replay (rmenu2), and Records / rewards, which are fresh overlays.
    - Port: Restart No -> 1, Quit back -> 4, Exit replay -> 2 (main.js:251).
    - Fixed (pv ps2MenuInput), except Exit replay: main.js:251 should call careerUI.resultsFocus(0).
13. **Results Quit skips "Quit Game" (No focused).**
    - PS2: 0x20CE60 -> overlay 0x18 (87yndialog type 1) -> Yes -> CTM: 0x26 "Save progress before quitting?" (Yes focused); Single Event: cGame_exit.
    - Port: goes straight to ctm-quitsave; a Single Event quits with no question.
    - File: web/career-ui.js:433, :351.
14. **Screens appear "flat": no intro replay on returns, and content from frame 0.**
    - PS2: the MCOMM replays 31paus_freeride from frame 0 on every return. The PDA frame does not replay (0x20D110 builds it once).
    - PS2: every results / records / rewards overlay (and every rebuild) reopens OV_darkblue from frame 0, and the rows pop in at LUI 30.
    - Port: ctm-pda.js replays only after 250 ms without a PDA draw (career-ui.js:723). results-lui.js reopens only after 15 frames without a draw (:84) and draws rows from frame 0 (:110).
    - Files: web/ctm-pda.js (a menu-open time separate from `opened`), web/results-lui.js clock(), audio-menu.js (move the results ev 14 to frame 30).
15. **Lodge cursor memory lives for the page session.**
    - PS2: the cursor table gp+0x1D90 is zeroed at every FE module entry (0x1A1DC0 -> 0x1A0648), so each lodge visit starts at item 0. Savestates confirm it (curtab.py).
    - Port: cursorMemo survives across visits (career-ui.js:347 restoreCursor).
    - Also:
      - 'fe-rewards' is missing from CURSOR_STATES; the PS2 restores it (0x186518).
      - The peak / trophy rooms restore on the port (trophy-room.js:130) but not on the PS2.
      - The cheat list opens on the current skin (character-select.js:164); the PS2 opens it on index 0.
      - "Quit to Title screen?" No should keep Quit focused (career-ui.js:351 -> index 7).
16. **Reward gallery grid** (129 rewardgallery, input 0x1CF270).
    - PS2:
      - Left / Right wrap inside the row, clamped to the last item.
      - Up / Down wrap inside the page.
      - L1 / R1 page with first <-> last wrap.
      - Sounds: kind 2 on Up / Down, kind 1 on Left / Right / L1 / R1, kind 4 when blocked, kind 3 on Triangle.
      - Cross on an owned cheat character is an error.
    - Port (fe-options.js:371-379): linear, stops at the ends, silent.
17. **Career Highlights** (35car_stat, 0x1F54B0).
    - PS2:
      - Up / Down scroll the window at once, kind 2, error at the ends (0x1F57E8..0x1F59E4).
      - Left / Right switch four pages with wrap.
      - Cross is silent and does nothing; Triangle plays snd 3.
    - Port: an invisible index (two dead presses at each end), the move sound, one page.
    - Files: web/lodge-ui.js:147-156, career-highlights.js.
18. **Lodge and Transport sounds.**
    - Silent where the PS2 plays sound:
      - every lodgeGo-driven Triangle back (audio-menu.js:431's snap ignores careerUI.lodgeFlash);
      - the Square INFO toggle (ev 9);
      - the buy-popup and overwrite-popup Up / Down;
      - the cheat list Left / Right;
      - the uber "not enough cash" error (0x185528; fe-screens.js:429);
      - the peak-room incomplete-goal error.
    - Accept played where the PS2 is silent:
      - the lodge's Save Game item (query 0x1F3CC8 -> 0x100);
      - the 40 / 41 card Continue (0x20D1D8 sets the menu silent);
      - the short reward list (under 10 lines, +0x90 bit3);
      - Career Highlights, trophy room and preview Cross;
      - All Mountain Cross (the PS2 plays an error).
    - web/replay-ui.js:58-78 passes listener *kinds* as *ev* numbers. The Start / Cross popup is therefore silent (the PS2 plays snd 3), and Triangle plays snd 1 (the PS2 plays snd 4).
19. **Transport INFO leaks.**
    - PS2: INFO is reset on every level change, the confirm open and the map open (0x2030D4, 0x2020E4, 0x202260..80). Square is ignored on All Mountain and on Earnings, and plays snd 3 otherwise.
    - Port: `this.info` persists and toggles on held-key repeat (career-ui.js:308).
20. **The crossing MCOMM** (overlay 4, world state 11) has 5 rows: Return, Messages, Audio, Options, Quit (0x478190). The port shows 7 rows with Transport / Session greyed (career-ui.js:222, :242). Queued by the coordinator.
21. **Lodge exit:** "Save progress?" No plays the lodge's TransitionOut (the white flash) before 118loadoutlodge (0x1F3DB0). The port cuts to the black fade (career-ui.js:345).
22. **Music section events missing** for the lodge's sub-screens (game-audio.js:22-23 FE_EVENT): equip-gear 2 (0x199330), fe-uber 5 (0x184B38), fe-rewards 6, ctm-highlights 8 (0x1F55A4; the table's 'ctm-stats' key is dead). For the audio agent.
23. **Booth and post-event Transport audio** (0x200AC0 -> 0x28F5B8, 0x200AF0 -> 0x28F678): pause SFX unless the booth flag is set, ev 9, a 1 s music fade plus the loading loop, undone on close. Missing in the port. Unconfirmed on the PS2 until captured.
24. **Low.**
    - The map picture fades in over 10 frames (0x202888).
    - The mode-4 Triangle plays snd 3 plus its press animation (0x2022A4 + 0x39B3E0); the port is silent.
    - Records from the results: the PS2 shows "Return" + "Save Records" (0x1F7E68); the port shows Return only.
    - The 117loadinlodge percentage is fake in the port (transition-screens.js).
    - Save game's "Checking" step is missing.
    - Buy-popup cost spacing: the PS2 shows "$ 20,000"; the port shows "$20,000".
    - The stick threshold (E4). Fixed (pv ps2MenuInput).

## 1. The menu engine: rules every CTM screen shares

The CTM screens are LUI screens run by one engine:
- the pad history;
- the UIMenu widget;
- the UI thread that plays a screen's timeline;
- the state stack.

The port re-implemented each screen by hand, so the "a little off" feel comes mostly from these shared rules.

| # | rule | PS2 address(es) | port | status | evidence |
|---|---|---|---|---|---|
| E1 | Menu directions are `UIUp/Down/Left/Right = DPad*.repeat \|\| LStick*.repeat` (INPUT.MAP). `.repeat` fires on the first held update, again after 24 updates, then every 12. There is one update per game frame (60 Hz), so 0 / 400 ms / every 200 ms. | 0x321298 (0x3213A0..0x3213F0); bit-exact in engine/original_input.cpp originalUpdatePad | gamepad-menus.js MENU_REPEAT {first 400, next 110}: moves at frames 31, 56, 63, 70, 77 | differs; fixed (pv ps2MenuInput: 60 Hz time-based updates, gamepad-menus.js createPs2PadMenus) | PS2 caps/mcomm-repeat.json: moves at 31, 55, 67, 79, 91, 103, 115, 127. G+0x1C advanced 1:1 with the samples. FE lodge: the same cadence (caps/lodge-repeat.json). Port caps/port-mcomm-repeat.json. |
| E2 | A held keyboard arrow key should repeat like the pad. The PS2 has only the pad. | as E1 | ui.js:52 `if(e.repeat)return`: a held key moves once | differs; fixed (pv) | caps/port-lodge-repeat.json |
| E3 | Edge debounce. The rule is per channel (24 channels: each button and each stick direction). After any edge (press or release), the next 3 updates don't look at the input; the 4th compares it with `held` again. A change inside the window is neither queued nor kept: it counts only if it is still there at the 4th update. It applies to Cross / Start / Triangle too. | 0x321320..0x321394 (edgeAge +0x1C < 3) | every keydown counts | differs; fixed (pv) | PS2: six Up taps (1-2 frames, 3-5 frames apart) gave 3 moves (201, 231, 261). The port gave 6. |
| E4 | The stick counts as held from raw < 79 / > 176 (0..255), about 0.38 deflection. | 0x321320 `value > 0` after originalDecodePad | MENU_REPEAT.stick 0.5 | differs; fixed (pv) | PS2 moved at ly 0.45 and not at 0.35. |
| E5 | Wrap is UIMenu runtime flag +0x14 bit7 (LUI element flag 0x80). Vertical is runtime bit0 (LUI flag 0x40): Down = next, else Right = next. Next / previous skip items with bit5. | HandleInput 0x39B000, next 0x39AB50, prev 0x39AC48 | ui.js:52 clamps on every screen no module handles itself. Results / records (pv resultsMenu), Rider Details (pv lodgeDetails), the audio screens and Buy Attributes wrap. | differs; fixed (pv, web/menu-rules.js) | Live UIMenu objects (memscan.py): MCOMM 0x129DB, lodge 0x120DB, lodge prompt 0x18120DB, results 0x12CDB, race pause 0x129DB, lodge save question 0x120DB (all wrap). Session 0x1350B / 0x1295B, Map goals 0x4290B, Map ConfirmMenu 0x1355B, reward list 0x12C5B (no wrap). Captured: MCOMM Quit -> Return, race pause 5 -> 0, lodge 7 -> 0, the Quit Game popup No -> Yes -> No. |
| E6 | Sounds (docs/audio-menus.md appendix). A move plays kind 1: ev 11 on overlays, ev 2 on FE screens (sample 2). A blocked end of a non-wrapping menu plays kind[0] = 4, the error (sample 0xD). Cross / Triangle with the default query play kind 6: ev 9 / ev 0, sample 3. A greyed pause item plays the error from the state. The engine never plays "back" (kind 3). | 0x39B17C / 0x39B274 move, 0x39B5E4 blocked, 0x1F8998 pause Cross; listeners 0x1A2F70 (ev 9..13) and 0x1A2E58 (ev 0..4) | audio-menu.js watchSounds: move when the index changed; nothing at a blocked end; accept / error on Enter; accept on an Escape that leaves | differs at blocked ends; fixed (pv PS2_END_ERROR_SCREENS). Chrome lost the move sound entirely (ranked 4, fixed). | Sound hooks 0x294F78 (ev) / 0x2906B8 (voice): a move is ev 11 + sample 2. MCOMM Cross / Triangle and Map Triangle are ev 9 + sample 3. Lodge Cross / Triangle is ev 0 + sample 3, with no ev 15 on the lodge flash. |
| E7 | A screen's LUI timeline advances 1 frame per UI update (60 Hz). Label ops (jump table 0x493E20, kind - 0x10): 0x42 activate (0x39CE20: owner state phase 3); 0x43 TransitionOut (0x39CE48: 0x398638 turns menu bit4 off, phase 6); 0x41 Stop (0x39CE98: phase 7); 0x10 stop the thread; 0x11 goto a label; 0x12 play a label on a second thread. | thread tick 0x39C978 (+0xC += step +0x12 = 1) | 60 LUI frames a second (ctm-pda.js now(), lui-player.js) | match (rate) | live menu flags during the MCOMM intro |
| E8 | **The input gate.** The state stack update 0x39ECB0 (phase = +0x1C bits 8..13, jump table 0x493FC0) calls the update that reads input (+0x64 -> 0x20E900 -> 0x39E510) only in phase 5. A state starts in phase 2 (build, 0x39E2A0 writes 0x218), waits in phase 4 until its 0x42 record, then phase 3 (+0x34 show / cursor restore) and phase 5. UIMenu also returns at once while +0x14 bit4 is off or nothing is focused (0x39B01C..0x39B040). Every menu input is dead until then: directions, Cross / Start, Triangle, Square, Circle. | 0x39ECB0, 0x39E510, 0x39B000 | no lock on overlays; the lodge flash only (lui-flash.js, career-ui.js:306); the round card's 30-frame guard (career-ui.js:357) | differs; fixed (pv, web/menu-rules.js introLockFrames), lodge FE numbers pending (ranked 1a) | PS2: MCOMM +60 dead / +62 taken (caps/intro89/91/93.json). Triangle at +19 and Cross at +39 dead, world still frozen (caps/mcomm-tri.json). Yes / No +23 dead / +31 taken (caps/yn-intro.json). Map +23 / +31 (caps/map-open.json). Rider Details press +32 dead / +40 taken (caps/lodge-details.json). Live MCOMM menu at open+10: +0x14 = 0x129CB (bit4 off), no focus; at open+50: 0x129DB, focused. |
| E9 | The way out. After a choice the popup plays TransitionOut to Stop with no keyframes, so it stays up, frozen, with input dead. 87yndialog 50 -> 70 = 20 frames; 98enterlodge 105 -> 130 = 25; 63bc_start 90 -> 102 = 12; 90bc_fail 80 -> 100 = 20; the lodge FE screens about 9. The MCOMM's trans_out has 0x43 and 0x41 on one frame (369), so it closes at once. A push through 0x39F320 cuts the current overlay to phase 7 (no out-animation). | label tables; 0x39F320, 0x39F190 | the port switches on the keypress | differs; fixed for the 87yndialog screens and the lodge prompt (pv, web/screen-phases.js leave() with web/menu-rules.js PS2_CHOICE_OUTRO, frame-counted: docs/pause-contexts.md) | PS2: race pause -> Restart -> No at 140; pause input at +83 (+81 dead) (caps/yno81..87.json). Port (pv): +81 dead, +83 taken. |
| E10 | The MCOMM replays its whole 60-frame intro, lock included, whenever it comes back from a sub-screen. pushSpecial 0x39F320 makes the pause dormant (phase 7 -> 8); 0x39EA90(stack, 8) restarts it at phase 2. Init 0x1F8168 replays frame 0 and sets the cursor from byte 0x4A2468, which Cross saves (0x1F8A18). 0x20CA10 / 0x20CB48 zero it on a fresh Start open, so a fresh open lands on Return and a return lands on the item used. | 0x39F320, 0x39EA90, 0x1F8168, 0x1F8A18, 0x20CB48 | ctm-pda.js replays only after 250 ms without a PDA draw (career-ui.js:723). The cursor lands per branch (career-ui.js:335, :351, :365-371, :389-393; audio-menu.js:371). | lock fixed (pv); visual replay differs; cursor match (Give Up / Restart landings fixed, pv) | PS2: back from the Map, the first accepted press came at +63 (caps/map-open.json); the cursor on Transport (caps/map-back.f00195.p2s); a reopen lands on Return (caps/mcomm-memory). |
| E11 | Underneath the pause stack, the game tick stops on the frame the pause opens, stays stopped through every sub-screen (Map, Yes / No), and resumes the frame after the closing press is accepted. Music pauses (pitch 0) and SFX voices pause (0x289B70 / 0x289BB8, docs/audio-logic.md); the pause-open sound is snd 0. The HUD is hidden (0x20A380). | 0x230A34 -> 0x20CA10; 0x230AFC -> 0x294F48 | main.js pause: the world is frozen; audio per audio-logic.md | match | caps/pause-open.json: tick 1500 from sample 30 to 151, snd 0 at 31. caps/map-open.json: tick constant. |

### 1.1 Which overlay the pause opens (0x20CA10)

| condition | overlay | rows (table) |
|---|---|---|
| online (0x534B30) | 6 | - |
| a Big Challenge running (0x30B8C0) | 2 | Return, Restart Challenge, Messages, Audio, Options, Quit Challenge (0x478178) |
| event type 0x535BC8+0x48 != 4 and +0x49 == 0 (a CTM event) | 1 | Return, Restart, Messages, Audio, Options, Give Up (0x478160) |
| free ride, world state 11 (crossing) | 4 | Return, Messages, Audio, Options, Quit (0x478190) |
| free ride | 3 | Return, Transport, Session, Messages, Audio, Options, Quit (0x478140) |
| otherwise (Single Event) | 5 | Return, Restart, Audio, Options, Quit (0x4781A8) |

The overlay factory 0x20AB50 maps each id to its vtable:
- 1 -> 473098, 2 -> 472FC0, 3 -> 472EE8, 4 -> 472E10, 5 -> 472D38, 6 -> 472C60, 7 -> 472B88, 8 -> 472850, 9 -> 472778
- 0xA -> 474088, 0xB -> 473FB0, 0xE -> 474B80, 0xF -> 4734D0, 0x10 -> 472108, 0x11 -> 474AA8
- 0x14 -> 4725C8, 0x15 -> 4726A0, 0x16 -> 473ED8, 0x17 -> 4749D0
- 0x1D -> 473688, 0x1E -> 4735B0, 0x1F -> 473760, 0x21 -> 4733F8, 0x23 -> 473C50

### 1.2 Wrap / error-at-end per port screen

**Wrap:**
- The pause family: ctm-mcomm, ctm-pause, ctm-bcpause.
- The Yes / No popups: ctm-quit, ctm-quitsave, ctm-giveup, ctm-restart, ctm-bcsure (87yndialog), ctm-saveprompt (FE popup, live 0x120DB), ctm-enterlodge.
- ctm-peaks.
- The results and records: ctm-results (43 / 42 / 70), ctm-records (61toptimes, LUI only).
- The lodge screens: ctm-lodge, ctm-details, ctm-attributes, trophies, rewards room, gear (12equ_char), uber (66ut_btnmap), cheat list, buy popup, Save game list.
- The PDA pages: in-game Options (37beoptions), Audio (142 / 143), bc_start / bc_fail.

**No wrap, error at the ends:**
- The Map lists and confirms: ctm-goals, ctm-events, ctm-confirm, ctm-gopeak (both the Map's ConfirmMenu, flags 0x255).
- The Session: ctm-session, ctm-sessconfirm.
- Messages folders (112messagecenter MenuFroms 0x240, LUI only; the virtual-list details of 0x1E4578 were not evaluated).

**No wrap, and silent below 10 lines:** ctm-award (0x1FF65C clears the wrap bit; +0x90 bit3 makes it silent).

The full screen dump (flags, labels and label ops) is local/ctm-decomp/screens/lui_menus.json and .txt. Wrap is LUI 0x80 and vertical is LUI 0x40. The txt file's V / H column from before the 0x40 fix is wrong.

### 1.3 Input lock after open, and the outro, per screen

Lock = the frame of the 0x42 record + 1, from the push / set. Outro = 0x43 -> 0x41.

| port screen | LUI screen | lock (frames) | outro | measured |
|---|---|---|---|---|
| ctm-mcomm, ctm-pause, ctm-bcpause | 31paus_freeride | 61 | 0 | yes (+62) |
| ctm-quit / quitsave / giveup / restart / bcsure | 87yndialog | 31 | 20 | yes (+31; No -> pause +83) |
| ctm-enterlodge | 98enterlodge | 39 | 25 | no |
| ctm-session (open only) | 38session | 49 | 10 (150 -> 160, not played: phase 7 set directly 0x208DC0) | no |
| ctm-peaks..confirm (open only; no lock between levels) | Map | 31 | closes directly (0x2085F0) | yes (+31) |
| ctm-objectives | 40race_pre / 41freestyle_pre / 68rival_pre | 31 | 25 / 20 / 20 | no (port guard 30) |
| finish panel | finishov | 46 | 5 | no |
| ctm-results | 43 / 42 / 70 | 31; OV_darkblue re-activates at 60 (unconfirmed: a second ev 14, a 1-frame gap) | 10 / 10 / 20 | no |
| ctm-records | 61toptimes | 31 | 20 | no |
| ctm-award | 62reward_list | 31 | 9 | no |
| replay menu | 64replay | 40 | 10 | no |
| Messages, Audio | 112messagecenter, 142 / 143audio_pda | 61 | 30 | no |
| in-game Options | 37beoptions | 51 | 13 | no |
| Save game (PDA) | 121Profilesave_pda | 41 | 5 | no |
| bc_start, bc_fail | 63bc_start, 90bc_fail | 61 | 12, 20 | no |
| lodge FE screens | 28lodge, 155, 33, 12equ_char, 127, 128, 66, 140, 16radio, 35car_stat | 27 from the switch (press + ~38 after the flash) | ~9 | yes (Rider Details: press +32 dead, +40 taken; the back to the lodge the same) |
| lodge questions | FE cFEPopupConfirm | ~27 (25-step growth, vt+0x3C 0x1C5C30) | - | no |
| buy popup, cheat list | 139buy_popup, 131cheat_char | 32, 33 | 9 | no |

## 2. MCOMM and the pause family (overlays 1..5, 31paus_freeride)

| rule | PS2 | port | status | evidence |
|---|---|---|---|---|
| Rows per overlay | 0x1F8EE0..0x1F90E8, tables above; each row's icon group at y = 40 x i (0x1F8448, table 0x441C30) | career-ui.js:222, :214; big-challenges.js:190 | match for 1 / 2 / 3 / 5; overlay 4 differs (ranked 20) | live item lists in menus/ctm/state-mcomm, menus/race/state-pause, menus/single/state-single-pause |
| Greyed rows (state+0xD8[id]) | all enabled; Give Up greyed at world state 11; all but Return greyed when 0x231AB8() == 0 or S+0x224 != 0 (0x1F8314..0x1F83DC) | career-ui.js:243-249 grey for port reasons (Session empty, Messages / Audio not loaded) | differs (edge); S+0x224's meaning unconfirmed (0 in every state) | code |
| A greyed row stays reachable; Cross = error | not skipped (no bit5); focus colour 4C8AE8 (0x1F8C88); Cross -> kind 4, snd 0xD (0x1F89D8), no action | reachable; audio-menu.js:443 error; choose() returns (career-ui.js:332) | match | code |
| Cursor on open / return | E10 | E10 | match | captures |
| Wrap, sounds, repeat, lock | E1..E10 | as there | fixed (pv) | captures |
| Opening | Start -> the overlay at +1 sample; 0x289B70; snd 0; HUD hidden; world tick frozen (pause context mask 0xFFFFFFDF, 0x230AB0); G+0x1C keeps counting | main.js:221 pause() | match | caps/pause-open.json |
| Row focus animation | notify 1: colour + `<icon>_S` looping 390..470 on a second thread; notify 2: jump to `_R` (0x1F8DF8); help0 = 0x441BF8[id] | ctm-pda.js:71-77 (FOCUS_LEN 90) | match (visual) | code |
| Cross | engine silent (query 0x1F8908 returns 0); enabled -> kind 6 snd 3, trans_out 369, the action (0x46F8A0) on the next pump | Enter -> accept; the switch happens at once | match | ev 9 from ra 0x1A3040 |
| Start | UINext = Cross or Start | pv startRules | match | docs/visual-parity.md 31 |
| Triangle | notify 6 (0x1F8BFC): remove the PDA, pop, 0x289BB8; query 0x101 -> snd 3; no out delay; tick resumes at +2 | career-ui.js:387-388 | match | caps/pause-open.json |
| Square / Circle / Select | notify 7 / 8 / 4: nothing; query 0x100: silent | ignored | match | code |
| Item actions | Return 0x20D190 + 0x39F190 + 0x289BB8; Restart 0x4A2708 = 1 -> 0x17; Options 7; Audio cFEStateAudioOptions; Quit 0x18; Give Up 0x1C; Restart Challenge 0x19; Quit Challenge 0x1A; Session 0x20; Transport 0x21 (map mode 2); Messages 0x22 | career-ui.js:335, :358-369 | match | code |
| The event pause (overlay 1) | 6 rows, wraps, ev 11 | ctm-pause | match (wrap under pv) | caps/race-pause-wrap.json |

## 3. Yes / No and the other popups

| rule | PS2 | port | status | evidence |
|---|---|---|---|---|
| 87yndialog kinds (+0x9C) | 0 Restart (0x17), 1 Quit (0x18), 2 Restart Challenge (0x19), 3 Quit Challenge (0x1A), 4 quit event (0x1B, unreachable), 5 Give Up (0x1C), 6 Save before quit (0x26). Only this family (0x20D338) uses 87yndialog (its name 0x471AF8 has one reference, 0x20D350). | the port also drew "Go to this peak now?", "Session this area?" and the lodge's "Save progress?" with it | differs (art) | code |
| Texts (0x20D3E4, table 0x471B60) | title_quit: kinds 0 kT_OVRCMNRestartEvent "Restart", 1 kT_OVRCMNQuitGame, 2 kT_OVRCMNRestartChal "Restart Challenge", 3 kT_OVRCMNQuitChal "Quit Challenge", 4 kT_CMNQuitEventQues, 5 kT_CMNGiveUp, 6 kT_CMNSaveBeforeQuit; else kT_OVRCMNQuitComp. title_quitchallenge keeps the LUI's "Are you sure?" (hidden for kind 1). | career-ui.js promptLines | match | code + LOC |
| Default focus | 0x20D4F0: No for kinds 0..5, Yes for 6 | career-ui.js:335, :364, :368, :351 | match | caps/yn-intro.json (No) |
| Vertical, wraps | 0x2C0: vertical + wrap | clamp (shipped) | fixed (pv) | quit-ctm capture (Up No -> Yes) |
| Cross | TransitionOut 50 -> Stop 70 (20 frames frozen), then the next screen; query 0x20D568 -> snd 3 | closes at once | fixed (pv) | E9 |
| No / Triangle | pop -> the pause restarts on the item used (Restart 1, Give Up 5, Quit 4 / 6); kinds 0..5 Triangle = No (0x20DB64); **kind 6 Triangle does nothing** (only snd 3) | Give Up No -> 0; Triangle on restart / giveup -> 0; quitsave Triangle -> MCOMM | fixed (pv) | caps/qs-tri.json (Triangle: ev 9, stays; Down then moves Yes -> No) |
| Big Challenge pause Restart / Quit Challenge | overlays 0x19 / 0x1A: "Are you sure?" with No focused, over the dormant pause, in the PDA. Yes -> overlay 0x1D offer / 0x30B758. No / Triangle -> bcpause on 1 / 5 with its intro. | big-challenges.js:208 / :212 act at once | fixed (pv, ctm-bcsure) | caps/bcpause.json: a 2-row wrap popup on No after the Cross |
| 98enterlodge (0x1F7198 / 0x1F72E0) | vertical, wraps, Yes focused; lock 39; outro 25. Yes -> 0x4A19C4 = 0x27; No / Triangle -> the done flag S+0x200+8, pop. Cross / Triangle snd 3; Square / Circle silent (0x1F72B8). | career-ui.js:347, :380 | fixed (pv) for wrap / lock / outro | live 0x18120DB |
| 63bc_start / 90bc_fail | vertical, wrap, lock 61. Only Cross makes a sound (queries 0x1F7590 / 0x1F77F0). Triangle declines (0x30B658), silent. 90bc_fail: Yes retry 0x30B6F8, No decline, Info -> 0x1D. Its default focus is set by no code (the LUI default); the port opens it on Info (big-challenges.js:127): unconfirmed. | Escape played accept | fixed (pv) | caps/bcprompt-tri.json: no UI sound call |
| In-game Options 37beoptions (0x1FA238) | vertical, wraps; lock 51; Triangle pops (no out) -> the pause restarts on Options | audio-menu.js:370-371 | match, except the lock and replay | code |
| Message centre 112messagecenter | MenuFroms 0x240: no wrap, error at the ends; lock 61; outro 200 -> 230 | generic clamp, silent (career-messages.js:292) | differs (end sound); virtual-list details unconfirmed | LUI |

## 4. Transport map (overlay 0x21) and Session (overlay 0x20)

Globals:
- gp-0xB5C (0x4A2594) = the map mode: 0 FE, 1 Single Event results, 2 MCOMM Transport, 3 booth (0x2364E8), 4 post-event.
- gp-0xB54 (0x4A259C) = the level: 0 Select Peak, 1 goals, 2 events.
- The map object (overlay +0x9C): +0 INFO flag, +0x2CC confirm up.
- Items: 0x2085B8 -> 0x2018A8, table 0x470780 (n 1 focus in, 2 out, 5 Cross, 6 Triangle, 7 Square).
- Query: 0x208610 -> 0x202738. Cross / Square / Circle -> 0; Triangle -> 0x101 (snd 3 plus the row's press keyframe, 0x39B3B8..41C).

| rule | PS2 | port | status | evidence |
|---|---|---|---|---|
| Intro: pieces 5..31, list and legend at 30 | Map labels (fe-menus.json) | ctm-map.js:30 INTRO = 30 | match | LUI |
| Input locked about 31 frames on open; level changes and in-screen popups instant and unlocked; closes skip TransitionOut (phase 7 at 0x2085F0) | 0x39EE14 / 0x39EE78 | none shipped | fixed (pv) | caps/map-open.json |
| Map picture: hidden until loaded, then a 10-frame fade (+0.1, 0x49DBC0), reloaded on a 3-peak <-> 1-peak change | 0x202950, 0x202888, 0x203990 | drawn at once (ctm-map.js:106) | differs (low) | code |
| Open audio in modes 3 / 4: pause SFX unless the booth flag is set, ev 9, a 1 s music fade + loading loop; close undoes it. Mode 2 skips it (0x5FB4 already set by 0x289B70). | 0x200AC0 -> 0x28F5B8; 0x200AF0 -> 0x28F678 | none for booth / post-event | differs (unconfirmed, capture T7) | code; mode 2 captured: only the listener's ev 9 |
| World underneath | frozen: the MCOMM pause (mode 2) or pause context 2 (modes 3 / 4, 0x236510) | paused | match | caps/map-open.json |
| Select Peak start row | the ridden peak (index 2 - career+0x54) on every open (0x2009D0 -> 0x200A4C) | 3 - this.peak (career-ui.js:335, :170, :427) | match (stale if the ridden peak changes without a Transport) | live +0x95 = 2 |
| Select Peak wraps | 0x2C9 + live | clamp | fixed (pv) | live |
| Locked peak: reachable, Cross = error | 0x2021B8 | disabled -> error | match | code |
| All Mountain Cross = error; Square does nothing and is silent | 0x201984 (row id >= 3), 0x2022E4 | accept; toggles INFO | differs | code |
| Go to this peak now? | only an unvisited Peak 2 / 3 (0x2019BC..0x201BB0, visited bit 145D38) | any other peak (career-ui.js:336) | differs (ranked 9) | code |
| Triangle, mode 2 | the map closes, the MCOMM is rebuilt on Transport with its intro (0x2022AC) | go('ctm-mcomm', 1) | match (lock pv) | caps/map-back |
| Triangle, mode 3 | world state 15 (0x20220C..58) | career-ui.js:382 | match | code |
| Triangle, mode 4 | ignored, but snd 3 and the press animation still play (0x2022A4 + 0x39B3E0) | silent return (career-ui.js:381) | differs (low) | code |
| Square | INFO toggle + snd 3, except All Mountain and Earnings; INFO is reset by setupMenuFocus (0x2030D4), on the confirm (0x2020E4), and on Triangle back (0x202260..80) | Shift / KeyI toggle, silent, never reset, repeats (career-ui.js:308) | differs (ranked 19) | code |
| Circle / Select / L1 / R1: nothing; Start = Cross | table 0x470780 | same | match | code |
| Goals (4 rows, no wrap): first entry row 0; the cursor is kept per peak while the map is open; ends = error | 0x39A928 -> 0x39B7B0; 0x2030A0; 0x39B5E4 | always 0 on re-entry; silent clamp | differs (memory); fixed (pv, error) | live 0x4290B |
| Earnings always locked (Cross = error); Freeride open in CTM | 0x201720, 0x201D60 | disabled -> error | match | code |
| Events (no wrap; only DONOTUSE is skipped): locked rows reachable, Cross = error; the cursor is kept per list | 0x201628, 0x2021B8, 0x2030A0 | reachable; row 0 on re-entry | memory differs | code |
| Confirm (Map ConfirmMenu 0x255, no wrap): opens on Yes every time; "Go to this peak now?" at level 0, else "Transport to this area now?"; No / Triangle -> back to the chosen row, INFO off | setupPopup 0x203A58 / 0x203AAC; 0x2021E4 / 0x201CF4 | pda.dialog (87yndialog art); back to row 0 | differs (ranked 8; art) | live 0x1355B |
| Session: lock 49; rows Top of run / "Session point k+1" / Bottom of run (unused rows hidden and skipped, value k+1) | 0x2087F0 | sessionItems | match; lock fixed (pv) | code |
| Session start row = the nearest point - 1, every open | 0x208A20 / 0x26B680 | sessionFocus (career-ui.js:681) | match | visual-parity 32 |
| Session no wrap: ends = error; its confirm the same | flags 0x249 / 0x255 | silent clamp | fixed (pv) | live 0x1295B / 0x1350B |
| Session Cross: the question, Yes focused, snd 3; Yes -> world state 15 at the point; No / Triangle -> the list with its cursor kept, no replay | 0x208D1C, 0x208FFC, 0x208C84..0x208D10, 0x208D80 / 0x208D90 | match | match | code |
| Session Triangle on the list: instant close, the MCOMM rebuilt on Session with its intro | 0x208DA0..D4 | go('ctm-mcomm', 2) | match (lock pv) | 40b-after-triangle |
| The Session popup's "Session Point: N" and "$cash / $500" | written (0x209080..0x2091AC) but not visible in the PS2 frame; nothing is charged | question only | match by frame; element visibility unconfirmed | frames |

## 5. The lodge (FE states under the FE controller 0x1A0830, jump table 0x460F30)

Entering: 98enterlodge -> FL.LUI 117loadinlodge (cFELoadStateInLodge, vtable 0x47C538, vt+0x3C 0x245DC0).
- It has no minimum time: it lasts as long as the I/O and shows the real loader progress as "%d%%" (about samples 90-230 in lodge-return).
- It then shows the lodge, which fades in from black (unconfirmed, CR-L1).
- The port: transition-screens.js, minMs 1500, a fake 0 -> 98 %, then 12 + 20 fade frames. The timing is about the same; the percentage source differs.

| rule | PS2 | port | status | evidence |
|---|---|---|---|---|
| Lodge items | option_0..5 = values 0..5, option_5b = 8 (Save Game), option_6 = 6 (Quit); option_7 hidden (bind 0x1F3840); nothing greyed (0x441AF8 all 1) | career-ui.js:229, :248 | match | 28lodge dump |
| Title | kT_TITLELodgePeakN from the last lodge (146D98): 17-18 -> 1, 19-20 -> 2, 21 -> 3 (0x1F3988) | career-ui.js:347 | match | code |
| Vertical, wraps | live 0x120DB | clamp | fixed (pv) | caps/lodge-repeat.json (7 -> 0, ev 2) |
| Cross / Start | only notify event 5 acts. Values 1-5: TransitionOut (label 85), vt+0x24 = the next state, 0x39F400 switches. 0 -> cFEPopupConfirm "Save progress?" (+0x50 = 0x10). 6 -> "Quit to Title screen?". 8 -> cFEStateProfileLoad(0x280) mode 3. (0x1F3A38, 0x1F3C4C, 0x1F3AA8, 0x1F3B3C, 0x1F3BEC) | career-ui.js:341 | match | code |
| Save Game item's Cross is silent (query 0x1F3CC8 -> 0x100) | | accept | differs (minor) | code |
| Triangle / Square / Circle / Select: nothing, silent | notify handles only event 5; query 0x100 | no back entry | match | code |
| Cursor on entry after 117: item 0 (the table gp+0x1D90 was just zeroed, 0x1A1DC0 -> 0x1A0648) | curtab.py over the lodge states | restoreCursor() brings back the page-session memo | differs (ranked 15) | savestates |
| Cursor back from a sub-screen: the stored index (0x1865A8 / 0x186518) | | career-ui.js:75-77 | match within a visit | tout-kbd |
| Input lock | E8: about 27 after the switch; the old state is deaf from the press (0x39F400 -> phase 6) | lodgeFlashing ends at flash t = 19 (PS2 +21), so the port takes presses between PS2 +21 and +37 on an invisible cursor. After 117: at once. | differs; pv lock 40 from ui.set is too long (ranked 1a) | caps/lodge-details.json |
| Music event charsel 1 on entry (0x1F3814) | | game-audio.js:23 | match | code |
| Underneath | the world is torn down (world state 6), the FE module runs | the world stays loaded and paused, behind an opaque screen | ok (no visible difference) | code |
| "Save progress?" No (message 0x10) | the popup closes, option_0 re-notified (+0x4C = 1), TransitionOut (white flash), then cGameLoadStateOutLodge (0x1F3DB0) | straight to 118 | differs (ranked 21) | code |
| "Save progress?" Yes (0xF, then 0x16) | cFEStateProfileLoad mode 3, +0x27C = 1, pushed by 0x39F320 (no flash); the full Save game screen; Continue or Triangle -> the pre-game load (0x1F3EB4, 0x18FB68) | persist() and 118 | missing (ranked 10) | code |
| "Quit to Title screen?" (Yes focused) No | closes; the lodge cursor stays on Quit (0x1F3F58) | ui.set resets to 0 (career-ui.js:351) | differs | code |
| Quit Yes -> "Save progress before quitting?" (+0x50 = 0x11) | Yes -> Save game with +0x27C = 2 -> the title; No -> TransitionOut -> the title (0x1F3EF0, 0x1F3DF8, 0x18FC14) | persist, quitToTitle | missing (screen) | code |
| Triangle on the three lodge questions | nothing, silent (0x1CA3EC, 0x1CA254) | back to the lodge + accept (career-ui.js:385 / :389) | differs (ranked 7) | code |
| Question lock | ~27 (0x1C5C30 box growth) | none shipped; pv 26 | fixed (pv) | code |
| Transitions | the flash curve and switch at full white; the new intro starts 2 frames in; child states (keyboard, cheat list, popups) change without a flash (0x39F290); each item's words[2] / words[3] are the labels its Cross / Triangle play when the query sets 0x100 | lui-flash.js | match (curve) | visual-parity 42 |
| Rider Details (0x1F3FF8, 155) | option_1..7 = values 9..15: Rewards, Trophies, Cheat Characters (disabled when 0x1577A0 == 0, 0x194498), Ubertrick Setup, Career Highlights, Player Name (cKeyboardPopup 0x444: max 8, an empty name keeps the old one, 0x416810), Rider Profile. Vertical, wraps, skips the greyed row. Triangle -> the previous state with snd 3. | lodge-ui.js:35, :54, :80-83, :167; fe-screens.js:600 | match, except the Triangle sound (silent) and cursor across visits; keyboard OS repeat is taken there (no e.repeat filter) | code |
| Career Highlights (0x1F54B0, 35car_stat) | 4 pages (+0x48) on Left / Right with wrap (hll / hlr, kind 1); Up / Down scroll the window (+0x4C 0..21 on page 0; +0x50 / +0x54 on 1 / 2), kind 2, kind 4 at the ends; opens on page 0, top 0; Cross silent; Triangle snd 3; music event 8 | lodge-ui.js:147-156: one page, an invisible index | differs (ranked 17) | code |
| Buy Attributes (0x1F4728) | career-events.md rules; the 139buy_popup is dead until 32; its Yes / No plays TransitionOut 50 -> 59 first; Triangle back is snd 3 | buy-attribs.js:127-160 | match, except the lock, the popup outro and the silent Triangle | code |
| Ubertrick Setup (0x183A98) | owned -> select + save; not owned with cash < price -> error (0x185528) and the query's snd 3; else the UITRICKBUY popup. Triangle: list -> categories -> previous. Square previews. Fresh on every entry. Music event 5. | fe-screens.js:338-339, :403, :428-429 | match, except the missing error and music event | code |
| Equip / Buy Gear (0x199F20) | UICamRotate (right stick, 3 degrees a frame, wraps 360), UICamZoom = L1 - R1 (+0xAA4 in [0, 1]), UICamPan (+0xAA8 in [-1, 1]); music event 2 | characters.md: turn and zoom | pan / tilt unconfirmed; music event missing | code |
| Rewards room 128 | items, counts, wrap match; Triangle snd 3; cursor restored (0x186518) | silent; 'fe-rewards' not in CURSOR_STATES | differs | code |
| Galleries 129 / preview 130 / buy 139 / trophies 125-127 / cheat list 131 | ranked 11, 15, 16 and 18 | fe-options.js, trophy-room.js, character-select.js | differs | code |

## 6. Memory-card screens

The save state pointer is 0x4A2028. The manager is cMCOverlayManager at 0x4A2C74 (+0x130 state, +0x108 timer).

| rule | PS2 | port | status |
|---|---|---|---|
| 93profile_load mode 3: title, help, legends; the rows are vertical and wrap (live 0xD12CDB); first item focused, no memory | 0x18ECA0, 0x1D9258 | save-game.js:100-126, one row (deliberate) | match / deliberate |
| Entry shows Checking first (display state 1, event 0x31); the list only after the card callback | 0x1D8E58, 0x1D9470 | the list at once | missing |
| Cross on a row -> the keyboard (Done focused, max 8, min 1) | 0x18FECC | save-game.js:75 | match |
| Overwrite? No focused, wraps, Up / Down kind 1, Triangle dead | 0x1D68E8, 0x1CA3EC | toggles silently; Triangle back to the list (save-game.js:78, :80) | differs |
| Saving is up for at least 60 frames (state 0x11 timer, 0x23D2F8 -> 0x23FBB8); Save complete + Continue -> exit | 0x1D66B0 | a 90-frame hold | about right |
| Keyboard: Circle eaten; Triangle closes with snd 3 | 0x1CDBC0, 0x1CE278 | pad Circle deletes a character (fe-screens.js:612); Triangle silent | differs |
| +0x27C = 1 / 2 exits (Continue or Triangle): the pre-game load at the last lodge / the title | 0x18FB68 / 0x18FC14 | the flow is skipped | missing |
| OV 121Profilesave_pda (the MCOMM Quit save; ctor 0x211088, exit 0x211AA8 -> 230488 -> Hints load -> title) | | career-ui.js:353 persist only | missing |
| 122Autosave (0x212138): no reachable caller from CTM (0x20D8A4 is dead) | | none | ok (factory 0x25 unconfirmed) |
| 25saveload items; New game with No focused | 0x18D2E8, 0x18D724 | fe-saveload.js:78, :90 | match |

Correction for visual-parity.md section 37: the lodge save's popups come from cFEMemCard 0x1D8530 -> 0x1D99C8, not 0x1B6xxx.

## 7. Round card, finish panel, results, records, reward list, replay menu

Engine notes specific to these screens:
- The push 0x39F320 cuts the current overlay to phase 7 (no out-animation). The new one starts at phase 1 and goes to phase 2 when nothing else is current (0x39EA90).
- The choice latency: notify writes gp-0x9F4 = 1 and gp-0x9F0 = the item id. The pump 0x20CFD0 (from the game update 0x230864) acts on the next frame (20CCF8 -> table 0x471A50 -> 0x471AC0). Measured: final-top Cross s16954 -> world state 14 at s16957.
- Every overlay here except finishov and 64replay adds OV_darkblue, whose own 0x42 at 60 enters phase 3 a second time. That means a second show (+0x34, which on 43 / 42 / 70 plays ev 14) and a 1-frame input gap. Unconfirmed (capture C2).

| rule | PS2 | port | status | evidence |
|---|---|---|---|---|
| Round card opens at world state 2 enter | 0x236BB0 -> 0x20A8F8(2): +0x4A == 0 -> 8 (40race_pre); 4..11 -> 0x14 (68rival_pre); else 9 (41freestyle_pre); free ride -> no card | ctm-objectives at the cutscene idle | match | code |
| Card lock | 31 | career-ui.js:357 30-frame guard from the first draw (cardTicks :969) | match within 1 frame | LUI |
| Card content appears | LUI 30 | text at t 33 (:970-972) | differs by 3 frames | results-screens.json |
| Card Cross / Start | 40 / 41 notify 0x20D318 -> pending 1 -> 2 -> 0x20D03C (world state 2 obj+8 = 1, auto replay stopped), about 2 frames; 68 notify 0x1FDB88 | play() at once | match | code |
| Card Triangle / Square / Circle | dead (query 0x20D308 / 0x1FDB78) | Triangle -> ctm-events / event (career-ui.js:385) | differs (ranked 7) | code + node probe |
| Card sounds | 40 / 41: the menu silent (0x20D1D8 sets +0x90 bit3; live qual-top 0x...08); 68: Cross snd 3; ev 14 + the DJ line at build (0x1FBB90, 0x2A31C0; 0x1FD304) | accept on every card; ev 14 at set | 40 / 41 differs | memscan |
| FINISH! hold | world state 5 enter 0x233C50: timer +0x14 = 180, then 0x20CC60 pushes 0xC (race) / 0xD (freestyle) | race: the banner stays until the results (ui.js:102); freestyle: finishov at 3 s (career-ui.js:550) | race finish panel missing (known) | code |
| Finish panel input | +0xA0 = 1 from the ctor (0x20B008), cleared by +0xC4(0) at the push while world state 5 phase < 2, set again when phase 1 ends; lock 46 | none | missing | code |
| Finish panel auto-advance | update 0x1E8098 / 0x1E87B8: +0x9C += 1/60; at 3.0 s (1.0 s on give up / time up) -> pop + 231320 -> the next world state (7, or 12 podium) | results at 408 ticks (288 on time up) (game-tick.js:50, :121-126) | match (PS2 406 / 407 / 287) | ws_log |
| **Finish panel Cross / Start skip** | notify 0x1E8160 / 0x1E8880: Cross with +0xA0 set pops at once | no skip | differs (ranked 5) | code; startprobe finish-ws5 (the records screen at +280 after Start at +240) |
| Results order at world state 7 | 0x236DA0 -> 0x20A8F8(7): a record rank >= 0 -> 61toptimes first; else a record (158F30) or cash (+0x10) -> 62reward_list; else the results (0x20A8B0: 6..11 -> 0x16; 0 / 4 / 5 or round 3 -> 0xB; else 0xA) | career-ui.js:499 | match | code |
| Results lock / content | 31 / rows at 30 | none shipped; rows from frame 0 (results-lui.js:110) | lock fixed (pv); content differs (ranked 14) | code |
| Results buttons | Cross / Start (queries 0x1E7B78 / 0x1E6630 / 0x1E8E98: 0x101 for 6 only); Triangle / Square / Circle silent no-ops | Escape does nothing on the results | match | code |
| Results wrap | live vertical + wrap | career-ui.js:315 (pv resultsMenu) | match | live 0x12CDB / 0x...2FDB |
| Results default focus | 0; Single Event 1 (43 only, 0x1E7558) | resultsFocus :289 | match | code |
| Back to the results after a confirm / replay | rebuilt, default focus (E-notes above) | Restart No 1, Quit 4, Exit replay 2 (main.js:251) | fixed (pv) except Exit replay (ranked 12) | rmenu2.final.png |
| Results item 0 | 1 Next heat (Final Round at GMM+0x70 == 3), 2 Transport | :300 | match | ctm-parity.md |
| Results Quit | 0x18 "Quit Game" (No) -> CTM 0x26 "Save progress before quitting?" (Yes); Single Event: Yes -> cGame_exit | straight to ctm-quitsave; Single Event quits at once | differs (ranked 13) | code |
| Results Restart confirm | kT_OVRCMNRestartEvent, No focused. Yes (not a peak run, gp-0x9E8 == 0) -> 0x20D7EC: stop the auto replay, 238348, world state 13, pending 3. No -> TransitionOut 20 frames -> the results rebuilt. Triangle = No (0x20DB64). | the gondola; No -> 1; Triangle -> the pause (bug) | Yes match; No / Triangle fixed (pv) | code + node probe |
| Doc fix | ctm-flow.md section 4 swaps the paths: 0x20D7DC -> 2302A8 is the pause's Restart (gp-0x9E8 != 0); the results use 0x20D7EC -> world state 13 | | | code |
| Records 61toptimes (0xF) | after a top time: help "Congratulations...", Continue / Save Records. From the results (gp-0x9F0 = 1): help hidden, **Return + Save Records** (0x1F7E68 relabels only "continue"). Continue / Return -> 0x1F8118 -> rewards if a record > 0 or cash != 0, else the results (158E30 clears them, so no second list). Save Records -> overlay 0x13. Triangle dead (0x1F80C8). Lock 31; ev 14 at build (0x1F7E30). | Return alone (:236); Triangle -> the results (:385-386) | differs | menus/race/18-records.png, code |
| Reward list 62reward_list (0x10) | 0x1FF65C clears the wrap bit. Under 10 lines: silent (+0x90 bit3), no white row. 10 or more: a virtual list with the error at the ends. Continue on any row -> 0x1FF700 (158E30 clears the record / cash / medal) -> a new results overlay (focus 0). Lock 31, no ev 14. | pv ps2MenuInput no longer wraps it (menu-rules.js) | fixed (pv) for the wrap; sounds differ (ranked 18) | live race-f 0x12C5B / +0x90 = 8; race-f95 +0x90 = 4 |
| Replay menu 64replay (0x11) | lock 40. Popup open clears the menu's silent bit; the first focus is 0 unless +0xB4 (focus 2, Save replay hidden). Exit replay -> 0x39F190 -> the results rebuilt (focus 0). Sounds: ReplayMenuUp / Down kind 2; Start / Cross kind 6 (snd 3); Triangle kind 3 (snd 4). | replay-ui.js:58-78 passes kinds as ev (ranked 18); focus 0 on every open | differs (sounds) | code |

## 8. Captures and probes

PS2 (silent ARMSX2). Tool: `local/ctm-decomp/screens/menuprobe.py BASE.p2s EVENTS.json OUT.json --watch name=0xADDR[:b|h|w] [--snap S,...]`.
- It uses tools/ps2_menu_capture.py's pad hook, plus entry hooks on 0x294F78 / 0x294F48 / 0x2906B8 (tools/ps2_audio_log.py stubs).
- It polls PINE every 2 ms and logs every change against its pad sample.
- Snapshots pause the emulator, so the timing is not valid around them.
- Captured states with hooks installed must be cleaned first (tools/clean_capture_state.py).

The outputs are in `local/ctm-decomp/screens/caps/`:

| file | what it shows |
|---|---|
| mcomm-repeat.json | MCOMM: 24 / 12 repeat, wrap with ev 11, debounce, stick threshold |
| lodge-repeat.json | lodge: the same, ev 2, wraps 7 -> 0 |
| pause-open.json, mcomm-tri.json | the world tick frozen through the pause; the snd 0 open; Triangle / Cross dead in the intro |
| mcomm-intro.json, intro89 / 91 / 93.json | the MCOMM input opens at +62 |
| mcomm-open.f000xx.p2s (in local/ps2-capture/menus/ctm-decomp/) | the menu inactive during the intro (+0x14 bit4, focus 0) |
| yn-intro.json, yn-outro.json, yno81..87.json | the Yes / No intro (+31) and No -> pause input (+83) |
| map-open.json, map-back.json | the Map at +31; the MCOMM replay at +63; cursor on Transport |
| mcomm-memory.json | a reopen lands on Return |
| race-pause-wrap.json | the event pause: 6 rows, wraps |
| lodge-details.json | Rider Details: press +32 dead, +40 taken; back to the lodge the same |
| qs-tri.json | Triangle on "Save progress before quitting?": ev 9, stays |
| bcpause.json | the Big Challenge Restart Challenge confirm on No |
| bcprompt-tri.json | 63bc_start Triangle: no UI sound |
| fin-*.json | the finish-skip timing capture did not run: the cleaned WS5 state did not sample through the hook; still open |

Live memory: `local/ctm-decomp/screens/memscan.py STATE.p2s` (UIMenu / UIPair / ListBox runtime flags, index, focus). The LUI dump is `lui_menus.py`.

Port. `local/ctm-decomp/screens/portprobe.mjs` (one frame-scripted fake pad + keys, logs screen / index / UI sounds per rAF) and `finalpass.mjs` (15 checks against the PS2 numbers, Chrome and WebKit, ?pv=ps2MenuInput). Results: caps/port-*.json, port2-*.json, port3-*.json, finalpass-chrome2.json, finalpass-wk.json.
- Final pass result: everything in E1..E10 matches, except the lodge FE lock (ranked 1a).
- WebKit is 1-2 frames early at times on the time-based locks, and reads the stick 1 frame late: rAF jitter, not the model.
- A one-minute random press storm left nothing locked, in both browsers.

Capture requests still open (from the sub-researchers' reports):
- T1-T10 (the Map / Session lists, INFO, the go-to-peak rule, the booth / post-event audio);
- CR-G1..G3, CR-L1 / L2, CR-C1 / C2, CR-S1, CR-U1 (the lodge gates, the fade after 117, the cursor across visits, Career Highlights, the uber error);
- C1..C7 (finish skip timing, the results' second ev 14, the rebuild focus, results Quit, the 61toptimes flags, the reward-list ends, the card sounds);
- the memory-card list (Checking timeline, overwrite Triangle, the +0x27C exits, keyboard Circle).

Scratch notes, dumps and scripts per group are in `local/ctm-decomp/screens/{mcomm,transport,lodge,results}/`.

## Overlaps

- World-state machine / event lifecycle (docs/ctm-decomp-world-states.md): the world states behind the card / results / Transport (2, 5, 7, 13, 14, 15) are theirs. Only the overlay rules are here.
- Free-ride scripted content (docs/ctm-decomp-freeride.md): the Big Challenge logic is theirs. Only the popups' menu rules are here.
- The Transport freeze / area-entry stalls and course limits are other agents'. This doc does not touch loads.
- The audio agent (docs/audio-logic.md): the lodge music events (ranked 22) and the map-open audio (ranked 23).
