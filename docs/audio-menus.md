# Audio menus, Now Playing popup and UI sounds (2026-09-23)

Menus that drive the sound settings, the in-game "EA RADIO BIG" popup, and the front-end/pause UI sounds.
The audio itself is web/game-audio.js (docs/audio-logic.md). These pieces only call its API:
`getSettings`, `setSettings`, `songs`, `previewSong`, `endPreview`, `onNowPlaying`, `ui(ev)` and `uiSound(snd)`.

## Files

| File | What |
|---|---|
| `tools/export_audio_menus.py` | Exports the five LUI screens, the checkbox/empty-box/dollar sprites and the locale strings to `web/public/assets/UI/audio-menus.json` (git-ignored; in `npm run setup`). |
| `web/audio-menu.js` | `AudioMenus`: the screens, the pure rules (tested), the UI-sound observer and the Now Playing hook. |
| `web/now-playing.js` | `NowPlayingHud`: the popup, recovered from 0x1E9A30 / 0x1EC2E0 / 0x1EABB0 / 0x2204A0. |
| `web/test-audio-menu.mjs`, `web/test-now-playing.mjs` | Tests (in `npm test`). The popup test checks layout and timeline against three PCSX2 snapshots. |

Wiring edits in other files, kept small:
- `web/ui.js`: dispatch to `audioMenus` for key, items, set, sync, choose, back and draw. It also enables the pause "Audio" item, adds a "Sound" row to the Options screen, and calls `drawHud` on the game screen.
- `web/career-ui.js`: CTM pause "Audio", MCOMM "Audio", and lodge "Music".
- `web/main.js`: `ui.gameAudio = gameAudio`.

## Which screen is which

| Port screen | LUI (pack) | Original state | Opened from |
|---|---|---|---|
| `fe-music` | `140audio` (FE.LUI) | cFEStateAudioOptions 0x195FF0, FE branch (0x28F140 event 4) | Setup Character → Music (CTM flag = career flow, 0x1A181C); lodge → Music (flag 1, 0x1A1360) |
| `fe-playlist` | `16radio` (FE.LUI) | cFEStateRequestLine 0x196B90 | Music → Edit Playlist |
| `fe-sound` | `141advsettings` (FE.LUI) | cFEStateOptionsSound 0x18A258 | Options (Square) → Sound. In game: pause → Options → Sound (a port path, see below) |
| `audio` | `142audio_pda` (OV.LUI) | the same 0x195FF0 while global+0x84 != 0 (in game; created by the pause 0x1F8B3C with flag 0) | pause "Audio" (single event, CTM pause, MCOMM) |
| `audio-playlist` | `143radio_pda` (OV.LUI) | the same 0x196B90 in game | in-game Audio → Edit Playlist |

The three volume sliders, DJ and Arcade SFX are not on the Audio/Music screens.
- In the front end they are in Sound Options (141advsettings).
- In game they are rows of the in-game Options (37beoptions, 0x1F8DF8): HUD, Camera 1/2, Music/MC, SFX, Character speech, DJ Speech, Arcade SFX, Save game.
- The port's in-game Options is its own simplified screen, so it gets a "Sound" row that opens 141advsettings. That row is a port addition.

## Music / Audio (0x195FF0)

- **Menu row → radio mode.** Item values are set in widget create 0x196540 (+0x18). "radio big" 0, "big mountain" 2, "request line" 1, "playlist" 3, "edit req" 4.
  - So the menu rows Radio BIG / BIG Mountain Ambience / Custom Playlist [DJ] / Custom Playlist [No DJ] map to SetRadioMode 0 / 2 / 1 / 3.
  - The boxes are indexed by mode: 0box = mode 0, 1box = mode 2, 2box = mode 1, 3box = mode 3. Box k sits on row k.
- **Cross on a mode (0x196770).**
  - Moves the check: empty box sprite on the old box (+0x60), checkbox sprite (+0x5C) on the new one.
  - Saves the mode (0x1587F8) and calls SetRadioMode 0x28BF78 immediately.
  - Triangle saves and applies it again, then leaves.
- **Disabled rows (query 6, 0x196B08: 0x10 = error sound).**
  - Rows 2 and 3 need a custom playlist (0x1587B8 > 0).
  - Row 4 needs the CTM flag or a playlist.
  - Disabled rows are greyed (0x1962A8 / 0x196378) and skipped by the cursor.
- **Help (table 0x460560 by value).**
  - Modes 0 and 2: kT_HELPAUDIORadioBig and kT_HELPAUDIOBMA.
  - Mode 1: kT_FEHELPCustPlayDJ.
  - Mode 3: kT_FEHELPCustPlayNoDJ.
  - With no playlist, rows 2 and 3 show kT_HELPAUDIOCreateInLodge (in game), kT_HELPAUDIOCreate (CTM) or kT_HELPAUDIOCreateInCTM.
  - Edit: kT_HELPAUDIOEditReq, or the same "create" texts.
- **HUD.** In game the state sends HUD command 3 on enter and 4 on exit, which hides the Now Playing popup.

## Request Line / Edit Playlist (0x196B90)

- **List (0x197AD8).** The 35 songs (audio+0x504), owned songs first, then the rest, each in song order. There are 8 rows; "arr_up" / "arr_down" appear when more songs are above or below.
- **Boxes (0x197CA0).** In the playlist → checkbox; owned → empty box; not owned → yellow dollar.
- **Texts.**
  - "Songs in your playlist:  %d" (0x198118).
  - "Song by:  %S" for the focused song (0x1986D0).
  - In game, "Now playing: %S" / "Artist: %S" for the current song (0x197E70).
- **Cross (event 5 / query 6).**
  - Owned song: toggles it in the playlist. The last song cannot be removed (query returns 0x10 → error, help kT_HELPAudio1SongMin).
  - Unowned song: buyable only with the CTM flag (0x16C).
    - The price is a free credit while fewer than 6 songs are owned, else $5,000 (0x198AE8).
    - Cross opens cUIStateBuyPopup (0x198988).
    - "Yes" buys through 0x1988D8 → 0x158558, which sets both the owned and playlist bits. The port calls `career.buySong`.
  - Without the flag, unowned songs are not selectable. Help: kT_FAQRADIOBA, or kT_CMNHELPBuySongCTM when the byte at 0x535C11 != 0; the port always uses kT_FAQRADIOBA.
- **CTM widgets.** "Cost:" / "You have:" and the six "Free song credits" icons, one per remaining credit.
- **Cross label (0x198340).** Remove song / Add song / Buy song, or hidden.
- **Square = Preview (query 8, 0x197910).** This is Square, not highlight; the legend shows ps2sq "Preview".
  - Front end: 0x2B4978 sends the song's PREVIEW section to charsel (`previewSong`).
  - In game, owned songs only: audio+0x508 = song, PlayMusic(0, 0, -1, 1), and the Now Playing labels. Unowned → error. The port calls `previewSong` and the audio side plays the song.
  - Highlighting a row only updates Song by, help and buttons (event 1).
- **Triangle (event 6, 0x19768C).**
  - Saves both masks to the profile (0x158848 / 0x158820), then 0x28C2D0 sets the custom playlist.
  - With the CTM flag it also sends charsel event 10. The port calls `endPreview` whenever a preview ran.
  - Then it goes back to the Music screen with Edit Playlist focused.
- **Data model in the port.**
  - `settings.playlist` (35 bools) holds only owned songs.
  - Ownership is the career save's per-rider `songState(rider).owned` (the original's R+0xF70).
  - `?unlockAll=1`, or missing career tables, owns everything (like the song cheat 0x187D38).
  - On save the career rider's playlist is kept in step.
  - pv `riderMusic` (2026-09-27): the radio mode and the custom playlist are the rider's, as on the PS2 (R+0xF80 written by
    the Music screen 0x196770 -> 1587F8, R+0xF78 by Triangle 0x19768C -> 158848, both applied by the world load 2867E8):
    `songState(id).radioMode` / `.playlist`, read by `settings()`, written by `set()`, and web/game-audio.js `worldLoaded`
    applies the human's before the song pick (`api.riderMusic` = `worldMusic()`). PS2 local/ps2-capture/lodge/runs l6 / l7 /
    l10; docs/career-events.md "In play".

## Sound Options (0x18A258, 0x18B380)

| Row | Widget | Options word `*(0x535610)` | Setter |
|---|---|---|---|
| Sound Mode | list box `1lb`: Stereo/Surround/Mono/DTS | bits 12..13 | 0x289DC0 |
| Music/MC volume | slider `2sl` | bits 0..3 | 0x287410 |
| SFX volume | slider `3sl` | bits 8..11 | 0x287488 |
| Character speech | slider `4sl` | | 0x287520 |
| DJ Speech | list box `5lb`: Off/On | bit 17 | 0x287558 |
| Arcade SFX | list box `6lb`: Off/On | | 0x2875D0 |
| EA SPORTS BIG Talk | list box `7lb` | bit 18 | online only, hidden offline |

- **Sliders** have 12 steps (+0x78 = 12, values 0..11).
  - The knob sits at track.x + value × (trackW − knobW) / 11 (0x39E130).
  - Left/Right moves by one; at 0 or 11 nothing happens and no sound plays (0x39DFE8).
- **List boxes** wrap (0x399904).
- **Changes apply immediately.**
- **Square** = Reset options. The original shows the 111resetopts popup (cFEPopupOptionReset) first; the port resets at once to 11/11/11, DJ on, arcade on.
- **Sound Mode** is display-only in the port. It is stored in `localStorage` as `ssx3.soundMode`.

## Now Playing popup ("EA RADIO BIG")

See the header of `web/now-playing.js` for addresses and constants.

- **Trigger.** 0x28F478 → HUD command 5 when a song starts and MUSIC is audible. The port uses `gameAudio.onNowPlaying`, ignored while `raceMusic` is false.
- **Lines.** Header "EA RADIO BIG" (hardcoded, not localized) in HUDFONT ×0.8. Title wrapped at 300 px, white FEFONT ×0.6. Artist and album in black.
- **Box.** A three-slice OV_1-4 box at (20, 460 − H).
- **Timeline** (1/60 s steps, frozen while paused):

| Time (s) | What happens |
|---|---|
| 0 – 0.4 | Slides in |
| 0.4 – 0.8 | Text fades in |
| 0.8 – 7.2 | Hold |
| 7.2 – 7.6 | Text fades out |
| 7.6 – 8.0 | Slides out |

- **Repeats.** The same title while it is showing is ignored; a new title restarts it.

## UI sounds (0x294F78 / 0x294F48)

Two listeners translate the LUI engine's sound kinds into events:
- The FE listener "FEUISound" (0x1A2E58, vtable 0x469348) gives kinds 1..6 → ev 2, 3, 1, 4, 0, 0.
- The overlay listener (0x1A2F70, vtable 0x469318) gives ev 11, 12, 10, 13, 9, 9.
- Both produce the same samples. Overlay screens are pause, MCOMM and transport map, results, and the lodge.

What the engine does (full map: the appendix below):
- Up/Down that moves the cursor → kind 1 (move). Blocked at a non-wrapping end → kind 4 (error).
- Cross or Triangle → the screen's query. The default 0x101 gives kind 6 (accept, sound 3). 0x10 gives kind 4 (error).
  - So Triangle plays the accept sound, not "back". Kind 3 (back) only comes from the reward galleries, reward preview and the Mountain Room help popup.
- List box Left/Right → kind 1. Slider change → kind 1.
- Direct calls:
  - Pause opened → `0x294F48` (sound 0), from 0x2306A8 / 0x244880.
  - Results, pre-race and top-times panels appearing → ev 14.
  - FE transition page → ev 15. The port's white-fade transitions are silent, as in the original.
  - Map open → ev 9.
  - Online notifications → ev 7 / 8.

How the port hooks these:
- `AudioMenus.watchSounds()` is one capture-phase keydown observer. It compares screen, index, flash and a value signature before and after each key:
  - move on Up/Down;
  - move on a Left/Right change (rider, options values, pages);
  - accept, or error on a disabled nav button, on Enter;
  - accept on an Escape that leaves the screen, silent on result panels;
  - the keyboard popup's key presses play move.
- `enter()` plays sound 0 when the race pauses and ev 14 when a results or objectives panel appears.
- The audio screens play their own sounds:
  - Music and Request Line Cross: accept or error.
  - Request Line Square: accept, or error on an unowned song in game.
  - Sound Options Cross: silent.
  - Buy popup Triangle: silent.

## Open

- The buy popup is drawn as a simple dialog, not the 139buy_popup LUI. Reset options has no confirmation popup.
- The DJ, arcade and sound-mode bits of `0x535610` and the other music-mode storage live on the audio owner's side (`settings`). The port's sound mode has no effect.
- Front-end charsel sections: the original sends 28F140(4) on entering 140audio. The audio owner's `FE_EVENT` could map `fe-music` to 4.
- `endPreview` sends charsel event 10 whenever a preview ran. The original sends it only with the CTM flag (0x1976E0).
- 0x535C11 (kT_FAQRADIOBA vs kT_CMNHELPBuySongCTM) is not identified.
- It is not known whether the lodge's Music uses the overlay listener (ev 9..13); the port uses the FE numbers.
- The character-select menu's wrap bit was not read. The port wraps, so there is no error sound at the ends.

## Appendix: UI sound map

Source: SLUS_207.72 disassembly (all.asm). 

### 0. Sound chain recap

`listener->vfunc+0x14(kind)`; listener = `screen+0x5C -> +0xD0 (state/handler) -> +0x10 (UI context) -> +0x14`
(0x39FF50 returns the context). A null listener falls back to the static object at 0x4A5A58 (no sound). Kind 0 or
kind > 6 = silent.

| kind | FE listener 0x1A2E58 | in-game listener 0x1A2F70 | bank-0 sound (0x294F78 table 0x482D60) |
|---|---|---|---|
| 1 move | ev 2 | ev 11 | 2 |
| 2 scroll | ev 3 | ev 12 | 1 |
| 3 back | ev 1 | ev 10 | 4 |
| 4 error | ev 4 | ev 13 | 0xD |
| 5 accept | ev 0 | ev 9 | 3 |
| 6 accept | ev 0 | ev 9 | 3 |

FE and in-game produce the same sample; only the ev number differs. The in-game listener (created in 0x22E968) is
on the overlay context, so the lodge screens (cFEStateLodge 0x1F3700, BuyAttrib 0x1F4728, CareerStats 0x1F54B0,
LodgeRiderDetail 0x1F3FF8), pause, results, pre-race, replay and map overlays use ev 9..13.
Other direct sounds: ev 7 -> 0x10, ev 8 -> 0x11, ev 14 -> 0x12, ev 15 -> 7, 0x294F48 -> snd 0.

### 1. Input actions (FEUIInput, vtable 0x46D1D0, methods 0x17FDF0..; bindings from input.map)

| input vfunc | action | pad |
|---|---|---|
| +0x10 IsPressed(id) | id 0..3 = UIUp/UIDown/UILeft/UIRight (0x72..0x75); 4..7 = UIUp2..UIRight2 (right stick) | D-pad or L-stick (repeat) |
| +0x18 | UIStart 0x70 | Start |
| +0x20 | UISelect 0x71 | Select |
| +0x28 | UINext 0x7A | Cross **or Start** |
| +0x30 | UIPrevious 0x7B | Triangle |
| +0x38 | UIOption 0x7C | Square |
| +0x40 | UIMisc 0x7D | Circle |
| +0x88/+0x90/+0x98/+0xA0 | UIUp/UIDown/UILeft/UIRight | (used by list box, slider and states) |

### 2. Generic LUI engine

### Widget defaults
* Base ctor 0x39FB30: item kind table `+0x6C` (kind[0]) = **4**, `+0x70` (kind[1]) = **6**; flags `+0x14`
  get bits 1,3,6 set and bits 0 (vertical), 2, 4, 5 (hidden/disabled), 7 (wrap) cleared. Orientation and wrap
  come from the LUI data (setters 0x3A4AD8 bit0 and 0x3A4AA8 bit7 are never called by game code).
* Only game-code writer of the kind table: cFEStateTitle, 0x1949C0 `3A0330(tReal, 0, 0)` sets kind[0] = 0 on the
  "Press START" item.
* UIMenu buttons (0x39AB00): set 0 (default, used by 0x39A8D8 init) `+0x91`=0 UIUp, `+0x92`=1 UIDown,
  `+0x93`=2 UILeft, `+0x94`=3 UIRight. Set 1 (4..7 = right stick) is used only by cFEStateLobbyMenu 0x1BCE94 and
  cFEStateStats 0x1C31A8.
* UIMenu `+0x90`: bit2 = virtual list (0x39B760; selectability asked through query ev 1), bit3 = **silent**
  (setter 0x3A5390 is unreferenced, so in practice every menu makes sounds).
* Default handler (base vtable 0x4946C8): item notify +0x98 = no-op; queries +0xA0 (menu), +0xA8 (UIPair),
  +0xB0 (list box) all return **0x101**. Query result bits: 0x100 = play the widget's animation,
  0x1 = sound kind[1] of the focused item (default 6 = accept), 0x10 = sound kind[0] (default 4 = error),
  neither = silent.

### UIMenu HandleInput 0x39B000 (vtable 0x494928 slot +0x78)
1. If the focused item (`+0xA0`) has flag bit3 and not bit5, it gets the input first (UIPair 0x39BED8 etc.); if it
   consumes it, the menu does nothing.
2. **Up/Down** on a vertical menu (bit0 = 1; Left/Right on a horizontal one): 0x39AB50 next / 0x39AC48 previous
   skips items with flag bit5 (or items the query rejects for virtual lists) and wraps only when `+0x14` bit7 is set.
   * Cursor moved -> **kind 1** (0x39B17C / 0x39B274) -> ev 2/11, snd 2. Scrolling the visible window (0x39AE98)
     adds nothing.
   * Cursor did not move (end of a non-wrapping menu, or every other item disabled) -> **kind = focused
     item kind[0]**, default **4** (error, snd 0xD) (0x39B5E4). On the title "Press START" item it is 0 (silent).
   * `+0x90` bit3 set -> kind 0.
3. **Cross** (notify 5 / query 6), **Triangle** (notify 6 / query 7), **Circle** (notify 8 / query 9), **Square**
   (notify 7 / query 8): state notify `+0x98(item, n)` then `flags = state+0xA0(menu, q, index)`. Bit 0x1 ->
   kind[1] (6 = snd 3); else 0x10 -> kind[0] (4 = snd 0xD); else silent. **With the default handler, Cross and
   Triangle both play snd 3**; the engine never produces kind 3 (back). **Select**: notify 4, silent.
4. Disabled/unselectable items cannot be reached by the cursor. A "not allowed" Cross is signalled by the state
   query returning 0x10 -> error snd 0xD (AudioOptions 0x196B08, RequestLine 0x1977D0, PeakRoom 0x1D3E08).

### UIPair 0x39BED8 (vtable 0x494868; label plus embedded control at `+0x7C`)
Cross: notify 5, query `+0xA8(pair, 0)`; Triangle: notify 6, query `+0xA8(pair, 1)`; 0x1 -> kind[1] (snd 3),
0x10 -> kind[0]; Select: notify 4, silent; anything else is passed to the child list box or slider.

### UIListBox 0x399970 (vtable 0x494798, "lb" widgets; count `+0x318`, index `+0x319`)
* Left/Right (horizontal) or Up/Down (vertical): index -1/+1; wraps when `+0x14` bit7 is set (0x399904 sets it
  during list-box setup); notify 9; then `q = +0xB0(lb, 0/3 dec, 1/2 inc)`. 0x1 -> **kind 1** (snd 2),
  0x10 -> kind[0], else silent. The query and sound also run when a non-wrapping box is clamped at its end.
* Triangle (when the box has focus): notify 6, `+0xB0(lb, 4)`: 0x1 -> kind[1] (snd 3).

### cUISlider 0x39DFE8 (vtable 0x494348; value `+0x74`, max `+0x78`)
Left/Right changes the value by 1 -> notify 9 -> **kind 1** (snd 2). At 0 or at max-1 it is **silent** (no query).

### Not produced by the engine
Kind 2 (scroll) and kind 3 (back) come only from state code (section 3). Paging comes from states.

### 3. State query overrides (what Cross / Triangle / Square / Circle do on a menu)
From `query_table.txt` (q6 Cross, q7 Triangle, q8 Square, q9 Circle; 0x101/0x1 = snd 3, 0x10 = snd 0xD, 0x100/0 = silent).

| state (vtable) | Cross | Triangle | Square | Circle |
|---|---|---|---|---|
| default handler (CharSelect 0x46D000, most popups/galleries, load states) | snd 3 | snd 3 | snd 3 | snd 3 |
| Title 0x46BAB8 (0x194A48) | snd 3 | - | - | - |
| MainMenu 0x46B9E8 (0x1952E8) | snd 3 | snd 3 | snd 3 | - |
| Options 0x46C7C0 (0x188C68) | snd 3 | snd 3 | - | - |
| OptionsSound/HUD/Controller (0x18A890/0x18C270/0x18CCE0) | silent (A8 Cross 0) | snd 3 | snd 3 | - |
| OptionsGame 0x189958 | snd 3 | snd 3 | snd 3 | - |
| AudioOptions 0x196B08 | snd 3, or 0xD for an unavailable radio item (index 2..4) | snd 3 | 0 | 0 |
| RequestLine 0x1977D0 | snd 3 or 0xD | snd 3 | snd 3 | 0 |
| RiderDetail / CharSetup 0x182C68 | snd 3 | snd 3 | snd 3 | - |
| CheatCharSelect 0x182808, BuyPopup 0x1CAED8, Lodge 0x1F3CC8, 0x46B3D0 popup | snd 3 | - | - | - |
| UberTrick 0x1859D8, BuyAttrib, LodgeRiderDetail, BonusMaterial, Mountain/Rewards rooms | snd 3 | snd 3 | - | - |
| PeakRoom 0x1D3E08 | snd 3 or 0xD | snd 3 | - | - |
| TrophyRoom, PreviewReward, CareerStats, Credits 0x186478, Stats | - | snd 3 | - | - |
| EventSelect 0x186950 | 0x101 unless 0x202738(state+0x48) says the event-select logic owns input (then 0) | same | same | same |
| Pause family 0x472C60..0x473170 (0x1F8908) | **silent** (state plays it, below) | **snd 3** (ev 9) | - | - |
| In-game options 0x472B88 (0x1FAA78; A8 Cross 0x100, B0 L/R 0x101, B0 Triangle 0x100) | snd 3 | snd 3 | - | - |
| yes/no dialog 87yndialog 0x4744C0.. (0x20D568) | snd 3 | snd 3 | - | - |
| pre-race/results/toptimes/bc_fail/progressive (0x20D308, 0x1FDB78, 0x1F80C8, 0x1F77F0, 0x1F7590, 0x1E8E98, 0x1E7B78, 0x1E6630, 0x2453E0, 0x246268) | snd 3 | 0 | 0 | 0 |
| finishov 0x1E88D0/0x1E81B0 | snd 3 or 0 | 0 | 0 | 0 |
| replay 0x474AA8 (0x20DF10) | 0 | 0 | 0 | 0 |

Not evaluated (jump tables): CharEquip 0x199F20, BraggingRights 0x193CF8, 0x1CA180 (0x46C890/0x46CBD8),
message centre 0x1E4578, 62reward_list 0x1FF748.

### 4. Game-code listener calls

| site | state / screen | moment | kind |
|---|---|---|---|
| 0x185528 | cFEStateUberTrick (0x183A98, trick shop) | Cross on a trick when cash (0x150928) < price -> error; otherwise opens the "UITRICKBUY" popup | 4 |
| 0x188BF0 | cFEStateOptions 0x1887A0, Enter Cheat keyboard closes | 0x187D38 == 1 (valid code) -> 6; invalid code -> 4 (0x188C28) | 6 / 4 |
| 0x1C4D2C / 0x1C4E44 | cFEStateStats 0x1C2D18 (online stats) | next/prev stats tab (UIDown 0x1C4D28 / UIUp 0x1C4E40) | 1 |
| 0x1CE254 | cKeyboardPopup 0x1CB030 input 0x1CDB98 | D-pad cursor move, Cross types a key, shift/caps toggles, USB-key typing -> 1; Square opens keyboard help -> 5; Triangle closes the keyboard -> 6; Circle consumed silently | 1 / 5 / 6 |
| 0x1CF28C | reward galleries (upd 0x1CF0B0; Posters/Cards/Concept/Toys/Videos/CheatChars) | Triangle = back | 3 |
| 0x1D2028 | cFEStatePreviewReward 0x1D1AA0 | Triangle closes the preview | 3 |
| 0x1D2EF0 | cFEStateMountainRoom 0x1D2678 | Triangle while the help-text popup (+0x48, bit6 visible) is up closes it | 3 |
| 0x1DEDA8 | cFEPopupVideoCalibration (THX bars) | popup confirmed/closed | 6 |
| 0x1DF074 | cFEPopupScreenPos (24screen_position) | Cross on any pad confirms | 6 |
| 0x1F4B40 | cFEStateBuyAttrib 0x1F4728 | Cross to buy with too little cash (else "UITRICKBUY" popup) | 4 |
| 0x1F4D0C | BuyAttrib | Left with nothing pending to take back | 4 |
| 0x1F4E18 | BuyAttrib | Left (refund, "hl left") or Right (add a point, "hl right") succeeded | 1 |
| 0x1F4E4C | BuyAttrib | Right at max level 11, over cap or not enough cash | 4 |
| 0x1F59EC | cFEStateCareerStats 0x1F54B0 | Up/Down scrolls the list ("free up"/"free down") -> 2; already at the top/bottom -> 4 | 2 / 4 |
| 0x1F8998 | pause menus (item event 0x1F8948, notify 5 = Cross) | Cross on an enabled item (state+0xD8[id] != 0) -> 6, then trans_out and the action; greyed item -> 4 | 6 / 4 |
| 0x2018C8 | EventSelect logic 0x2018A8 (from cFEStateEventSelect 0x186610 item events) | Cross (ev 5 branch 0x20193C..): select event/peak/goal -> 6 (0x2019A4, 0x201BD8, 0x201D34, 0x201DD8, 0x201EA4, 0x2020C8); locked -> 4 (0x201D68, 0x2021C0). Square (0x2022BC..): peak goals list -> 6 (0x202380, 0x2023AC). Triangle branch: no call | 6 / 4 |
| 0x20DF58 | 64replay menu 0x20DF38 | ReplayMenuUp/Down (0xCA/0xCB) slides the panel -> 2; Start opens "ReplayMenu" -> 6; Cross picks continue/exit replay -> 6 | 2 / 6 |
| 0x26FBA0 | replay transport 0x26FB88 | ReplayFForward (0x68) / FBackward (0x69): allowed -> 2, at the end/start -> 4; ReplayCycleCamera (0x6C) -> 2 | 2 / 4 |
| 0x3A426C | not a call: static initialiser that builds the null listener | - | - |

### 5. Direct 0x294F78 / 0x294F48 callers

| site | where | moment | ev (snd) |
|---|---|---|---|
| 0x1946E4 | cFEStateTransition 0x1946A8 (vfunc +0xC0, arg 0; driven from 0x1A0830 case 0) | FE screen-change "transition" page starts (arg 1 = whitefade, silent) | 15 (7) |
| 0x1A8D84 / 0x1A8DB4 | online message pump 0x1A8B88 (chat/lobby/online states, from 0x194154 and 0x1A87B8) | online notification id 0xEC -> ev 7; id 0x109 -> ev 8 (0x109 is also handled by cFEStateReadMessagePopup 0x1A7D40, so probably "message received") | 7 (0x10) / 8 (0x11) |
| 0x1E6594 | 42freestyle_standings (0x1E5B80) | standings rows filled, panel shown | 14 (0x12) |
| 0x1E78F4 | 43final_standings (0x1E6668) | final standings panel shown | 14 |
| 0x1E805C / 0x1E8780 | finishov (0x1E7BB0 / 0x1E8200) | "screendata"/"3D Ov" results groups shown | 14 |
| 0x1E8EEC / 0x1E8FA4 | 70peakchal_results (0x1E8920) | peak-challenge results panel / OV_darkblue page | 14 |
| 0x1E9114 | 0x1E8FC0 help page | helptxt01/02 page switched | 14 |
| 0x1F7E30 | 61toptimes (0x1F7958) | top-times panel filled | 14 |
| 0x1FBB90 / 0x1FC844 / 0x1FD104 | 40race_pre / 41freestyle_pre / 50multifs_pre (0x1FB6B8 / 0x1FBD20 / 0x1FC878) | pre-event info panel (top time / rider panels) appears, followed by DJ 0x2A31C0 | 14 |
| 0x1FD304 | 68rival_pre (0x1FD268) | rival panel appears | 14 |
| 0x28F60C | 0x28F5B8 Map/Transport open (from 0x200ADC; "Map" overlay 0x4733F8) | after pausing SFX | 9 (3) |
| 0x29DBD4 | 0x29DBB0 freeride challenge end, arg 0 (from 0x30AAB4) | challenge ends/aborted; arg 1 from 0x3091EC is silent | 14 |
| 0x230AFC | game update 0x2306A8 | **pause opened**: Start (0x231840) -> push overlay state 2 -> 0x289B70 music/SFX pause -> 0x294F48 | snd 0 |
| 0x244DF0 | game update 0x244880 (copy used by 0x233C10) | same pause-open path | snd 0 |

### 6. Specific answers

* **Pause open**: snd 0 (0x294F48) from 0x2306A8/0x244880, right after 0x289B70. The pause page 31paus_freeride
  (0x1F8168, which also calls 0x289B70) plays nothing itself.
* **Pause navigation**: engine Up/Down -> ev 11 (snd 2); at a non-wrapping end -> ev 13 (0xD).
* **Pause select**: Cross -> ev 9 (enabled) or ev 13 (greyed) from 0x1F8998; the engine stays silent (query 0x1F8908 returns 0 for Cross).
* **Pause close**: Triangle (notify 6 -> 0x1F8BFC, which pops the page and calls 0x289BB8 resume) plays ev 9
  (snd 3) from the engine (query returns 0x101 for Triangle). Resume via Cross = ev 9 from 0x1F8998. There is no
  separate "unpause" sample and snd 0 is not replayed.
* **Results**: every results and pre-race panel uses ev 14 (snd 0x12) when it appears (section 5). ev 7/8 are
  online notifications only; ev 15 is the FE screen transition. Results menus (Cross) use the default snd 3.
* **Loading screens** (cFELoadState / LoadHint / LoadStateInLodge / cGameLoadState*, vtables 0x47C538..0x47C948):
  default handlers, no listener calls and no direct UI SFX. Only the LoadingScreen.bnk music loop plays.
* **Title "Press START"** (06title, Menu0000/tReal): Start or Cross = UINext -> query 0x194A48 returns 0x101 for
  ev 6 -> kind[1] 6 -> **FE ev 0 (snd 3)**, then notify 5 -> 0x39F400 transition (plus ev 15 if the FE transition
  page runs). Up/Down on the title are silent (tReal kind[0] = 0).
* **Character select** (cFEStateCharSelect 0x181090, 08sel_char "Menu"): no game-code sound. Changing rider is a
  UIMenu cursor move -> **kind 1 -> FE ev 2 (snd 2)**. If that LUI menu does not wrap, pushing past the first or
  last rider plays kind[0] = 4 (ev 4, snd 0xD). The wrap bit is in 08sel_char's LUI data and has not been read.
  Cross/Triangle/Square/Circle -> snd 3 (default query).
* **Engine vs state**: Up/Down, list-box and slider changes, and the default Cross/Triangle click (snd 3) come
  from the engine. Back (kind 3, snd 4) only comes from state code: reward galleries, PreviewReward, MountainRoom
  help popup. Error (snd 0xD) comes from blocked cursor moves (engine) or from the states listed above.
