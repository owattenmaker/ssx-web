# Selectable characters and the classic Select Character screen (2026-09-23)

The browser offers the whole original SSX 3 roster: the ten riders of the Select Character screen, the twenty
cheat characters (unlockables), and Sam (the port's own rider) as the eleventh select entry, the way the Sam PS2
build adds him. Every one plays as the human with its own original model, textures, skin, bind matrices and the
per-character gameplay values of its own live PS2 human actor. The Select Character screen is the original
`08sel_char` screen from `FE.LUI`, with the original 3D preview and front-end poses.

## The original roster (SLUS_207.72, read-only)

- **Ten riders.** `DATA/BE/CHARDB.DBL` has 10 x 0x88 rows. Loader 0x149C84 copies them to 0x530970.
  - Fields: long name, first name +0x20, nickname +0x30, weight +0x40, stance +0x44 (1 = goofy), model size
    +0x48 (scale = size x 0.01), blood type +0x4C, female +0x5C, age +0x60, height +0x64, nationality +0x74.
  - The Select Character order is table 0x440F68 = `[4,0,8,5,9,6,7,3,2,1]`: Zoe, Moby, Psymon, Griff, Viggo,
    Elise, Nate, Mac, Allegra, Kaori. Left from Zoe wraps round to Kaori.
  - Name: `kT_CHAR<Name>` (CMNAMER, getter 0x14EEC8). Card text: 08sel_char widget 0x69020 + screen index.
    Bio: `kT_FULLBIO1<Name>`. DNA page: `kT_DNA1..8<Name>`.
  - Model prefixes: moby, kaori, arielle (Allegra), mac, zoe, grommet (Griff), elise, rocco (Nate), psymon,
    deiter (Viggo). Textures: `DATA/CHAR/<X>TXP.BIG`.
- **Twenty cheat characters, ids 10..29.** They are not select entries: each is a **skin on the chosen base rider**.
  - Setup slot 0x534FE0 + slot x 0x1C: +0x11 base character (getter 0x14A080), +0x12 cheat id (0x14A0B0).
  - Gameplay (stats, stance, weight, uber table, bio) comes from the base rider. The model, texture set, scale
    and skeleton come from the cheat character. Confirming a rider on Select Character clears the cheat.
  - Picking one: Setup Character > Rider Details > **Cheat Characters** (`131cheat_char`, 0x006AA322,
    cFEStateCheatCharSelect 0x182220). The list is the base rider's face first, then the owned cheat
    characters in id order. The item is enabled only when the base rider owns at least one (0x1577A0).
  - Unlock bits: profile 0x4A6CA8 + profile x 0x9B50 + base x 0xF88 + 0xF57, bit id-10, per base rider. All
    zero on a fresh profile, so none is available by default.
  - Unlocked by lodge purchases (Brodi, JP, Marisol: $20,000 Peak 1; Eddie, Luther, Marty, Seeiah: $40,000
    Peak 2), collection bonuses (Hiro cards, Svelte Luther toys, Stretch posters, Bunny San art) and awards /
    Peak 3 goals (the rest). See `riders.json` `unlock.help`.
  - **Cheat codes** (Options > Enter Cheat, handler 0x187D38; lowercase, hashed with 0x317670): zenmaster Brodi,
    worm Eddie, bronco Luther, back2future Marty, slicksuit Hiro, brokenleg Jurgen, notsosvelte Svelte Luther,
    windmilldunk Stretch, milkemdaisy Cudmore, wheresyourtail Bunny San, tankengine Churchill, boneyardreject
    Gutless, betyouveneverseen Snowballs, callhimgeorge NW Legend, finallymadeitin Unknown Rider. A code unlocks
    its character for all ten riders of profile 0. JP, Marisol, Seeiah and Far East Myth have no code; Canhuck's
    code hash is 0x0DD36D88 (text not recovered). **There is no all-characters code.**
  - Names: the RWRDPS2.DAT catalog names; 8-character short names at 0x43FA38 + id x 8 (HUD/records).
  - Model scale: jump table 0x45A660 (0x14EFA8), e.g. Brodi 0.98, Stretch 1.2, NW Legend 1.5, Far East Myth
    2.0. In Conquer the Mountain the human keeps the base rider's CHARDB scale.
  - Uber overrides (0x150198 / 0x1502C8 / 0x1503F8): Stretch, Gutless and Canhuck replace the Nose Grab uber
    (semantics 160 / 159 / 161), Snowballs the Tail Grab uber (162).
- **Select-screen stats** (0x181BD0): raw attribute bytes at 0x535538 + player x 70 + char x 7 (NumAttr0..6 =
  Acceleration, Edging, Speed, Spin, Stability, Toughness, Tricks read bytes 1,3,0,4,6,5,2), shown as raw x 0.2
  with "%.1f"; bar = raw / 55. Rider ranking = (sum - sum % 5) / 35, bar sum/7. On a fresh profile every rider
  has raw 5 (limits 0x5308D8: 11), so every value is 1.0 for everyone.
- The computer riders of an event depend on the human: 0x23A4F0 picks two Tricky riders (ids 10..16) and base
  riders other than the player and the peak rival, and a cheat entry rides on the **human's** base character
  (why Snow Jam's Luther has gameplay character 4 with Zoe as the human).

## Per-character packages and settings (tools/export_characters.py)

**Evidence: one derived savestate set per character**, in `local/reference/pcsx2/characters/<id>/`
(`select`, `setup`/`cheatlist`, `ready`, `countdown`, `glide` .p2s + PNGs + `navigate.json`). They are made by
`tools/ps2_navigate.py` from `character-selection.p2s` (never modified):

- Base riders: DPadRight x N from Zoe (Kaori: one DPadLeft), then Cross through Setup Character, Peak 1, Race,
  Snow Jam and My Rules, wait for the race overlay, Cross, save the countdown at total tick 18..20 and the
  glide 319 samples later. Script generator: `characters/scripts/make_script.py`.
- Cheat characters: a copy of `character-selection.p2s` with all 20 unlock bits poked for every rider and profile
  (`characters/scripts/unlocked-selection.patches.json`), then Zoe > Setup > Rider Details > Cheat Characters >
  DPadRight x k > Cross, back to Setup and the same event path (`make_cheat_script.py`). Each countdown was
  checked: the human's race entry has base 4 and cheat id 10..29.
- Zoe's derived countdown gives exactly the same character extraction and identity as the reference
  `snow-jam-countdown-anchor.p2s`.

For every character the tool reads the **human actor** of its own states:

1. **Assembly:** every active LOD0 part of the actor's geometry (+0x780, parts +0x0C, 0x58 stride), matched
   byte-exactly to an `MDLPS2.BIG` header (runtime fixup bytes 56..67 masked), decoded from its GameCube MNF twin
   (`rider_assets.decode_high_model`). A few parts are byte-identical across riders (Moby/Nate HandsA): those
   take the rider's own prefix.
2. **Textures:** the equipped colour variants are the TXP.BIG members whose pixel data is resident in EE RAM
   (EA keeps loaded textures in main memory and uploads them per draw; three 48-byte probes). This picks the
   original combined suit (e.g. Mac `mac_suit_a01_a02`, Allegra `arielle_suit_a02_a02`, Nate `rocco_suit_c01_a01`),
   and per-part item colours such as Mac's beanie `mac_hats_a01_a01` on an `alph` material (the wardrobe colour
   children of the part's own BOLT item). Every texture of every package is now a verified live one. The kept
   packages were corrected in place: Zoe's boots `zoe_boot_a01_b01` (was a01_a01) and Moby's helmet
   `moby_hats_a01_d01` (was his hair texture); only batch texture indices and PNGs change there.
3. **Skin and bind:** raw source weights, the live bind matrices and bone slots
   (`export_rider_bind_matrices.derive`, now also on savestate paths), identical in countdown and glide.
4. **Settings** (`web/public/assets/RIDER_<ID>/settings.json`): the same extractors as `export_npc_riders.py`
   (landing, boost, air entry, rail context, grab control, trick identity, animation inputs, secondary motion,
   reset stance), kept as the **leaf differences from Zoe's extraction on her own derived countdown**, so Zoe
   keeps `initial.json` exactly. Contact normals and other grid-spot state are excluded (only the contact legs).
   The differences found are character values: body scale (landing body_scale, animation scale), air-entry
   pivot, uber table rows, stance (trick identity, reset), bone mask, default mirror/root rotation, variant
   flags, secondary motion (hair), contact legs.
   - Identity: the channel-1 bone masks rider+0x8C0/+0x8C8/+0x8D0 (11C298 bone lists), the race-copy base
     character and cheat id, and the rider-pair inputs (CHARDB weight +0x40 through 0x11FF98).
   - Cheat characters ride on Zoe in their states, so their differences are the skin's own, plus `uber_rows`
     (the overridden uber rows only).

The five Snow Jam computer riders (Psymon, Allegra, Moby, Griff, Luther) keep their packages (the AI cores read
their bone order); the tool checks their human assembly is the same default outfit. New packages go to
`local/assets/native/CHARACTERS/RIDER_<ID>` (the old `local/assets/native/RIDER_MAC` test outfit is the source of
`tools/sam_mesh.py` and stays).

| Rider | id | char | prefix | scale | stance | mask 8C0 | bones | parts | textures | uber |
|---|---|---|---|---|---|---|---|---|---|---|
| Zoe | zoe | 4 | zoe | 0.85 | regular | 0x8000fffe | 27 | TopB, BottomB, HeadA, HandsB, BootsA, Mop | 6 | |
| Moby | moby | 0 | moby | 0.94 | goofy | 0x20000fffe | 29 | TopA, BottomA, HeadA, HandsA, BootsA, HeadBoltB4, Tshirt, HeadBoltD2, MobyEarrings | 7 | |
| Psymon | psymon | 8 | psymon | 0.92 | goofy | 0x4000fffe | 26 | TopA, BottomA, HeadA, HandsA, BootsA, Antenna | 6 | |
| Griff | griff | 5 | grommet | 0.70 | regular | 0x8000fffe | 27 | TopA, BottomA, HeadA, HandsA, BootsA, Mop | 5 | |
| Viggo | viggo | 9 | deiter | 0.89 | regular | 0x20000fffe | 29 | TopA, BottomA, HeadA, HandsA, BootsA, Mop2, Tshirt | 7 | |
| Elise | elise | 6 | elise | 0.96 | goofy | 0x4000fffe | 26 | TopA, BottomA, HeadA, HandsA, BootsA, PonyTail | 5 | |
| Nate | nate | 7 | rocco | 1.00 | regular | 0x2000fffe | 25 | TopC, BottomA, HeadA, HandsA, BootsA, HoodieB, HeadBoltA | 5 | |
| Mac | mac | 3 | mac | 0.85 | regular | 0x4000fffe | 26 | TopA, BottomB, HeadA, HandsA, BootsA, Tshirt, HeadBoltB1, HeadBoltD5, HeadBoltH3 | 6 | |
| Allegra | allegra | 2 | arielle | 0.83 | goofy | 0x20000fffe | 29 | TopA, BottomA, HeadA, HandsA, BootsA, PigtailsB, AssHangerB, TopBoltA, TopBoltD | 7 | |
| Kaori | kaori | 1 | kaori | 0.80 | regular | 0x4000fffe | 26 | TopA, BottomA, HeadA, HandsA, BootsA, PigtailsA | 5 | |
| Sam | sam | (3) | sam | 0.85 | regular | 0x8000fffe | 26 | the Sam mesh (Zoe's gameplay, Mac's career slot) | 15 | |
| Brodi | brodi | 10 | brodi | 0.98 | base | 0x2000fffe | 26 | TopA, BottomA, HeadA, HandsA, Boots, Dangle, HeadBoltC | 5 | |
| Eddie | eddie | 11 | eddie | 0.85 | base | 0x2000fffe | 26 | TopA, BottomA, HeadA, HandsA, Boots, Antenna, HeadBoltB | 5 | |
| JP | jp | 12 | jp | 0.90 | base | 0x2000fffe | 26 | TopA, BottomA, HeadA, HandsA, Boots, Antenna, HeadBoltC | 5 | |
| Luther | luther | 13 | luther | 1.00 | base | 0x4000fffe | 27 | TopA, BottomA, HeadA, HandsA, Boots, Dreads, HeadBoltB | 5 | |
| Marisol | marisol | 14 | marisol | 0.85 | base | 0x4000fffe | 27 | TopA, BottomA, HeadA, HandsA, Boots, Dreads, HeadBoltB | 5 | |
| Marty | marty | 15 | marty | 0.75 | base | 0x2000fffe | 26 | TopA, BottomA, HeadA, HandsA, Boots, Dangle, HeadBoltC | 5 | |
| Seeiah | seeiah | 16 | seeiah | 0.90 | base | 0x2000fffe | 26 | TopA, BottomA, HeadA, HandsA, Tshirt, HeadBoltC | 5 | |
| Hiro | hiro | 17 | hiro | 0.75 | base | 0x2000fffe | 26 | TopA, BottomA, HeadA, HandsA, Boots, Dangle, TopBoltA | 6 | |
| Jurgen | jurgen | 18 | jurgen | 1.00 | base | 0x800fffe | 24 | TopA, BottomA, HeadA, HandsA, Boots, HeadBoltA | 5 | |
| Svelte Luther | sveltluther | 19 | luthern | 1.00 | base | 0x4000fffe | 27 | TopA, BottomA, HeadA, HandsA, Boots, Dreads, HeadBoltC | 5 | |
| Stretch | stretch | 20 | stretch | 1.20 | base | 0x1000fffe | 27 | TopA, BottomA, HeadA, HandsA, BootsA, AntennaA, SpecialAB, SpecialA | 5 | Nose Grab 160 |
| Cudmore | cudmore | 21 | bessy | 1.00 | base | 0xfffe | 29 | TopA, HeadA, BootsA, Elephant, Necklace, Tail | 5 | |
| Bunny San | bunnysan | 22 | bunny | 1.30 | base | 0x2000fffe | 28 | TopA, HeadA, HandsA, Bunny, Suspenders | 5 | |
| Churchill | churchill | 23 | churchill | 1.30 | base | 0x400fffe | 24 | TopA, BottomA, HeadA, HandsA, BootsA | 5 | |
| Gutless | gutless | 24 | skel | 0.90 | base | 0x400fffe | 24 | TopA, BottomA, HeadA, HandsA, BootsA, TopBoltA | 5 | Nose Grab 159 |
| Snowballs | snowballs | 25 | snowman | 0.90 | base | 0x400fffe | 24 | TopA, BottomA, HeadA, HandsA, BootsA, HeadBoltA | 5 | Tail Grab 162 |
| NW Legend | nwlegend | 26 | yeti | 1.50 | base | 0x800fffe | 24 | TopA, BottomA, HeadA, HandsA, BootsA | 5 | |
| Unknown Rider | unknownrider | 27 | unknown | 0.90 | base | 0x1000fffe | 27 | TopA, BottomA, HeadA, HandsA, BootsA, DreadsA | 5 | |
| Canhuck | canhuck | 28 | beaver | 0.70 | base | 0x10000fffe | 28 | TopA, HeadA, HandsA, BootsA, Antenna, Tail | 5 | Nose Grab 161 |
| Far East Myth | fareastmyth | 29 | abom | 2.00 | base | 0x1000fffe | 24 | TopA, BottomA, HeadA, HandsA, BootsA | 5 | |

The live scales equal CHARDB model size x 0.01 for the riders and the 0x45A660 table for the cheat characters.

## Browser

- **`web/public/assets/riders.json`** (`tools/export_roster.py`): an array. The ten riders in screen order
  (`kind: 'rider'`: CHARDB fields, name, card, full bio, DNA texts, FE clips, IRR record, stats), Sam (`custom`,
  screen index 10), then the twenty cheat characters (`cheat`: catalog name, short name, face image, unlock help,
  lodge price/peak, Enter Cheat code). `settings` says whether the package has a `settings.json`. The code texts
  are checked against the handler's hash constants.
- **`web/character-roster.js`**: `loadCharacter(entry)` fetches the settings (none for Zoe and Sam),
  `humanSettings(initial, doc)` deep-merges the differences over the course `initial.json` and adds
  `original_rider_identity` (the masks). A cheat entry carries `base`: `composeCheat(base doc, skin doc)` lays the
  base rider's differences first, then the skin's, then the skin's uber rows; the rider-pair weight stays the base
  rider's; in career (`entry.career`) the skin's scale is dropped. `applyHumanPairInputs(aiRace, doc)` sets the
  human slot of the six-rider world.
- **Core** (`web/animation_bridge.cpp` `init_animation`, rebuilt): the channel-1 masks come from
  `settings.original_rider_identity`, else Zoe's (0x8000fffe/0x8000fff8/0x870). `npc_configure` still sets the
  computer riders' own. Capture gates, the AI gates and npm test are unchanged.
- **`web/main.js`** `selectRider(entry)` accepts any package, merges its settings before loading the model (the
  geometry scale comes from them), restores everything on failure, and treats a cheat on another base as a new
  rider. `?rider=<id>` uses the riders.json entry (a cheat alone rides on Zoe).

### The classic Select Character screen (`web/character-select.js`, `web/lui-player.js`)

- `tools/export_character_select.py` exports `08sel_char`, `09set_char`, `131cheat_char`, the white Cross flash
  and the always-on snowflake screen from FE.LUI (menus, bars, shape triangle lists, the object-table sprites,
  every cheat face) to `UI/character-select.json`, and copies the Sam build's 64x128 roster page to
  `UI/sam-roster.png`. The FE_1 pages are the existing `UI/FE_1-N.png`.
- `web/lui-player.js` plays such a screen: frame-0 properties, timeline events with their own start frames,
  group and menu offsets, gouraud shapes and line loops, bars (fill = value / max), FEFONT text with wrapping.
- The screen runs its own timeline: the intro (dashes slide in, stats fade in, frames 1..25), the focused rider's
  state (frame 40 + 5i: name, orange silhouette, card), the arrow flash (hll/hlr), the snowflakes (600-frame
  loop), and the white flash on Cross before Setup Character. Layers 0..9 (sky, mountains, snow, white ramp,
  frame, the big "3") go to the UI background canvas, behind the 3D rider; layers 10+ go in front.
- Runtime fields as 0x181620 / 0x181BD0 fill them: the names, the seven stats and the ranking from the career
  profile's attributes when there is one (else the fresh-profile 5s), the bars. The 2-player title is hidden.
- Sam: Mac's rider state with Sam's name, card and silhouettes (white always, orange when focused, the Sam
  build's page at x=514, group offset (-4,12), 130%/110%), and the right arrow at x=556.
- **3D preview** (0x181EF0 / 0x19EE88): camera eye (0,200,0) -> origin, 25 degrees half-horizontal; the rider root
  at (136,-250,-82) cm turned 100 degrees, **unscaled** (the preview geometry's +0x140 scale is 1.0), board moved out
  of view. The right stick (J/L on the Classic keyboard) turns the rider 3 degrees per frame on Select Character.
- **Front-end poses** (the fe.afl bank, already in `library.json`): the idle loop `FE_GEAR_<PREFIX>_CYC`
  (semantic 434) on Select Character; the cheer `FE_CHARSEL_<NAME>` (435) every time Setup Character or Rider
  Details is entered (also from each other), from t=0, held at its end, then a linear 0.23 s crossfade into the
  restarted idle. `FE_A_CYC` (436) belongs to Ubertrick Setup, not Rider Details (see "Front-end preview").
  Sam uses Mac's clips; a cheat skin uses its base rider's.
- **Cheat Characters**: Rider Details gets the original item; it opens the `131cheat_char` list over the page
  (six face slots, the focused face enlarged, name bar). Picking one loads the skin on the current base rider.
  Owned = career rewards of that rider (`web/career.js`), the original Enter Cheat codes typed on the Select
  Character screen (stored in `localStorage` `ssx3.cheatCharacters`), or the browser switch `?unlockAll=1` /
  typing `unlockall` (the original has no all-characters code).
- Hooks: `ui.js` creates `CharacterSelect` and routes the `character` screen and the Rider Details item to it;
  `main.js` calls `place()` and `pose()` on the front-end rider screens (`frontEndRider()`).

### Checks

- `web/test-characters.mjs` (npm test): every packaged rider and cheat skin (and Stretch on Psymon's table)
  initialises as the human with its settings, has the right channel-1 mask, is released from the Snow Jam grid
  at 180..240 and rides away with finite poses; Zoe and Sam keep `initial.json`.
- PS2 capture gates (event-race, boardpress-rail, metro-event-race, the three AI gates) exact; `test-ai-racers`
  unchanged.
- Browser screenshots against the ARMSX2 select frames for all ten riders (model, textures, pose, placement).


## Grid spot per rider (2026-09-23, follow-up)

- The human's countdown ground state is a function of its body scale (geometry+0x140) and its base rider's stance:
  position, normals, lift (presentation lift = (0.85 - scale) x 90 - 1.5), forward/lateral follow the scale;
  `reverse_stance` follows the stance. Mac (0.85, regular) sits exactly on Zoe's spot; Luther, Jurgen, Svelte Luther
  and Cudmore (1.0) share one spot; Nate (0.99999994) differs from them by 1.5 mm.
- `tools/export_characters.py` writes `settings.original_event_start = {state, body_scale}` (the whole
  `extract_ground` state; for a base rider only when it differs from Zoe's, always for a cheat skin).
- Core (rebuilt): `init_animation` (web/animation_bridge.cpp) keeps the state and installs a grid-seed callback run at
  the end of `begin_event_rider` (like the computer riders' `npc_seed_rider`); `browser_apply_ground_attributes`
  (web/attribute_bridge.cpp) puts the body scale into the ground profile and every surface material after each
  re-seed (event start and reset). Before, a non-Zoe human kept Zoe's compiled 0.85 ground-profile scale.
- Cheat skins on another base: evidence state `local/reference/pcsx2/characters/brodi-on-psymon/` (Brodi worn by
  Psymon). Composing Psymon's and Brodi's documents reproduces that state's own extraction exactly once the two
  stance-dependent values follow the base: the air-entry pivot x changes sign and the grid seed is reverse stance
  (`composeCheat`; checked by `export_characters.validate_compositions()` on every full export).
- In Conquer the Mountain the human keeps the base rider's CHARDB scale (0x14EFA8, 0x535C11 == 0): the skin's scale,
  landing body scale, grid seed and pivot are dropped (base rider's). This path is from the code only: a career
  state with a skin was not captured (the career overlay savestates don't take the navigation pad hook).
- `web/test-characters.mjs` checks every rider's post-`start_event` position against its seed and that Mac = Zoe.

## Rider speech on Setup Character

- 1A0358 polls the FE preview slot flags: +0xC20 (set by Select Character's Cross, 0x18185C) -> 2A16B0
  Post_Selection (0x20BC), gated by 0x535C11 != 0 (not in Conquer the Mountain); +0xC1C (set by the Equip Gear commit,
  0x199EA4) -> 2A1778 Customize (0x20BD), every mode. CHARACTER bus, the slot's character (a skin's base rider).
- `web/rider-speech.js` maps a rider to `Post_Selection_<abbr>` / `Customize_<abbr>` (ari eli grf kao mac mob nat psy vig
  zoe); `character-select.js` speaks Post_Selection when Setup Character opens; the wardrobe calls Customize.
  `main.js` passes `gameAudio.speak` as `ui.cb.speech`.
- **Needs an audio API change (web/game-audio.js, audio agent):** `speak(bank)` always loads `speech/<bank>_eng.json`;
  the rider banks are language-less (`speech/Post_Selection_zoe.json`). speak() should fall back to `<bank>.json`
  (or take `{ file }`). Until then these calls return false (no sound, no error).

## Online races with any rider

- **Done (multiplayer, 2026-09-23):** base rider, outfit key and per-rider grid spots in every slot; see [multiplayer.md](multiplayer.md) "Riders: any character, skin and outfit". The notes below are the request.

- web/net/mp-game.js sends `rider().id` and loads `RIDER_<ID>`: every riders.json id has that package, so remote riders
  of any character or skin resolve. Needed changes there (multiplayer agent): (1) send and restore the cheat skin's
  base rider (`selectedRider.base`; main.js now reads `&base=` next to `&rider=` and `selectCourse` keeps it), else a
  reload rides the skin on Zoe; (2) `beforeStart()` calls `human_grid_seed('')` off-line, which clears the core's
  grid callback when it was the MP one; the per-rider seed is installed by `init_animation` and survives that, but an
  online slot seed replaces it and a later off-line race falls back to Zoe's spot until the next rider load:
  re-applying the character (or skipping the clear when no MP seed was set) keeps it; (3) an online slot k seed is the
  Zoe-anchor computer rider's state for slot k, whose position follows that rider's scale: for exact spots use the
  slot's state with the local rider's scale (see the lineup section).

## Setup Character, Rider Details, Options, Load game

The screens around Select Character are the original FE.LUI screens, played by `web/lui-player.js` from
`UI/character-select.json` (`tools/export_character_select.py`) in `web/fe-screens.js` (`FeScreens`, `ui.feScreens`).

**PS2 ground truth.** `tools/ps2_navigate.py` runs from `characters/zoe/select.p2s` (Cross is already latched in that
state, so it lands on Setup Character) and from the unlocked-bits selection copy. Scripts and the reference frames
are in `local/reference/pcsx2/characters/fe-screens/` (`scripts/*.json`, `frames/<screen>.png`).

| screen | FE.LUI | code | items |
|---|---|---|---|
| `setup` | 09set_char 0x0CA31FA2 | 0x18293C (widgets Option_Continue/_Equip/_Rider/_Radio = 0..3, Option_GBA hidden) | Continue, Equip Gear, Rider Details, Music |
| `details` | 154rider_details 0x0B32CE73 | 0x1832BC (Option_1..5 = 5..9) | Rewards, Cheat Characters, Ubertrick Setup, Player Name, Rider Profile |
| lodge `ctm-details` | 155rider_details_conquer 0x0FFAD5A2 | 0x1F4064 | Rewards, Trophies, Cheat Characters, Ubertrick Setup, Career Highlights, Player Name, Rider Profile |
| `fe-profile` | 14rid_prof 0x0FA0EE56 | 0x1908D4 | pages Rider DNA / Faves / Q&A / BIO |
| `fe-music` | 140audio 0x0768BC7F | cFEStateAudioOptions | Radio BIG, BIG Mountain Ambience, Custom Playlist [DJ] / [No DJ], Edit Playlist |
| `fe-options` | 18options 0x067AAFB3 | FEOptions 0x1887A0 | Game Options, Sound Options, Controller Settings, HUD Options, Save/Load, Enter Cheat, Credits, (port: Display & Touch, `fe-display`, docs/mobile.md), DONE |
| `fe-load` | 93profile_load 0x06488254 | cFEStateProfileLoad 0x18EDDC | rows 1..6: player%d, date%d, time%d |
| `fe-uber` | 66ut_btnmap 0x05E8EAC0 | cFEStateUberTrick 0x1849C4 | Mute, Indy, Stalefish, Method, Nose Grab, Tail Grab, DONE, then the category's ubers |
| keyboard | Fullkeyboard 0x05413EA4 | cKeyboardPopup 0x1CB030 | over Rider Details (Player Name) and Options (Enter Cheat) |
| `fe-game` | 19game_opt 0x083DF9D4 | cFEStateOptionsGame | Speed units, Widescreen, Screen position, Video calibration (Language hidden) |
| `fe-control` | 22control 0x0A65D25C | cFEStateOptionsController | Vibration 1P/2P, Controller 1P/2P + the DualShock picture |
| `fe-hud` | 21hud_opt 0x0FBA4C94 | cFEStateOptionsHUD | Full / Minimal / None with its HUD picture |
| `fe-sound` | 141advsettings 0x0255B2C3 | cFEStateOptionsSound | web/audio-menu.js (audio agent) |
| `fe-credits` | 26credits 0x0A8BDB33 | cFEOptionsCredits 0x185AAC | kT_CREDITS roll (CRAMER.LOC) |
| `fe-rewards` / `fe-gallery` / `fe-poster` | 128rewardsroom 0x034B045D / 129 rewardgallery 0x0C495039 / 130rewardposter 0x046FCA42 | cFEStateRewardsRoom | categories, 8x4 pages, one reward |

- **Menus.** Each item has a focus state (frames 35 + 5i; 140audio 31/36/41/46/50) that turns its text white, moves
  the orange bar and shows its own help text. Up/Down wrap and skip disabled items (PS2: Down from Music goes to
  Continue; on Rider Details Down from Rewards skips a disabled Cheat Characters, Up from Rewards goes to Rider
  Profile). A disabled item is drawn at alpha 128 (0x194498; 0.5 on the PS2 frames, docs/first-load.md "Front-end texture fixes"). A change of screen plays the
  white fade (whitefade, `TransitionOut` state), like Cross on Select Character.
- **Buttons.** Cross (event 5) passes the item index to the flow; Triangle (6) goes back; Square (7) opens FEOptions
  on Setup Character and Rider Details (0x182D58 / 0x183A2C); on Select Character Square is Options and Circle is Load
  game. Browser keys: Enter/Space, Escape, Shift = Square (everywhere); Circle on Select Character is Backspace,
  because letters there type Enter Cheat codes. The legend entries (Options, Load game) are clickable.
- **Setup Character** (Single Event and online only: in Conquer the Mountain Select Character goes straight to the world load, 0x1A0B00, `character-select.js selectDone`; docs/ctm-parity.md). All four items are enabled (0x182AA8..0x182BD8). Continue goes to the event flow (Select Peak
  on the PS2; in the browser the `event` screen, the career (`careerUI.enter()`) or the online lobby, as before). Equip
  Gear opens `openEquipGear(ui)` from `web/wardrobe.js` when that module exists (loaded through `import.meta.glob`, so
  a missing file is not an error), else `ui.cb.equipGear(rider)`; with neither it is disabled. Music opens 140audio.
  The 3D rider shows through: layers 0..9 on the UI background canvas (as 08sel_char).
- **Music (140audio).** Radio BIG is checked by default; the check follows the chosen mode (`localStorage`
  `ssx3.musicMode`: radio / ambience / custom-dj / custom-nodj). The custom playlists are enabled only with a
  playlist (career `songState(rider).playlist`), else grey boxes and "Create a custom playlist in Conquer the Mountain
  mode...". **Audio hook:** choosing an item calls `ui.cb.music(mode)`; Edit Playlist calls `ui.cb.music('edit')`
  and is disabled while no `cb.music` exists (16radio, the Edit Playlist screen, is not played here). Help texts per
  item are the original strings (help_radio_big ... in `character-select.json` `strings`).
- **Rider Details.** Cheat Characters is enabled only when the base rider owns one (0x1577A0 > 0, disabled by 0x183550
  through 0x194498) and opens the existing 131cheat_char list (`character-select.js`). Rewards opens the original
  rewards room (below). Rewards and Ubertrick Setup need the career data (`careerUI.career`, `shop.json`); the lodge
  viewer (`lodge-ui.js` `ctm-rewards`, view-only through `lodge.feReturn`) remains the fallback without those screens. The frame group is A=0 in 154rider_details's own data; the PS2 shows it, so
  it is forced on. The career lodge's `ctm-details` is drawn with 155rider_details_conquer (same states, help texts)
  and the lodge's own item rules.
- **Player Name** (0x183764): cKeyboardPopup with the current name, focus on Done, Tab and the punctuation keys disabled
  (the ten 0x1CE3C8 calls; Up/Down are always dim). Accepting a non-empty name stores it (0x147138 copies it into the
  setup slot 0x534FE0; browser: `localStorage` `ssx3.playerName`, `ui.playerName`). It is shown on Load game.
  pv `playerName` (2026-09-27, docs/career-events.md "In play"): a fresh profile reads "PLAYER 1" (0x147170 fills an
  empty name with kT_MEMPlayerName "PLAYER %d"; the earlier frame read `PLAYER1`), the popup takes 8 characters
  (1CD088(kb, 8)), the caret sits on the last one when the name is full and a letter there replaces it (1CD2F0; PS2
  local/ps2-capture/lodge/runs/l4: 'a' x 3 -> "PLAYER a"), and the name is the records' player entry (0x154DDC). The
  lodge's Rider Details opens the same keyboard (0x1F45C4).
- **Rider Profile (14rid_prof).** Opens on Rider DNA; Left/Right cycle DNA -> Faves -> Q&A -> BIO with wrap (PS2:
  Down/R1/L1 do nothing), flashing the pressed arrow (hll/hlr). The texts are `kT_DNA1..8`, `kT_FAVES1..12`,
  `kT_QNA1..4` and `kT_FULLBIO1` + first name (`profiles` in `character-select.json`; Sam's from
  `tools/sam_ps2/rider_bio.py`). The code shows one page's groups and hides the others; the BIO text box (kind 0x20)
  wraps at 440 with a 17.5 px pitch, as measured on the frame. No 3D rider on this screen (nor on Music, Options, Load
  game, Ubertrick Setup: the whole screen goes on the front canvas).
- **Ubertrick Setup (66ut_btnmap).** Categories with "Hold <button> for 1 second while in air" (Mute L2, Indy R2,
  Stalefish R1, Method L1, Nose Grab L1+L2, Tail Grab R1+R2 on the PS2 frames), Cross enters the category's list
  (check = selected, box = owned, $ = locked; Cost / You have for a locked one; "Buy this trick in Conquer the Mountain
  mode." - buying stays in the lodge). **What it changes:** the category's selected uber (career save `uber[cat].selected`
  of the rider). The in-race grab profile has two uber rows per category: set 0 = the rider's hidden base uber, set 1 =
  the selected uber. A row is {semantic = trick id A, upper_semantic = trick id B, score_id = the entry's name index,
  begin/hold points = the trick score table 0x530600 by score id} (0x150198 / 0x1502C8 / 0x1503F8 read the same list
  entry; the table is exported from Zoe's derived countdown RAM). This reproduces every one of Zoe's 12 rows in
  initial.json exactly. A choice that differs from the rider's default becomes `[1, category, row]` through
  `setUberChoice(baseId, rows)` in `web/character-roster.js`; `loadCharacter` prepends those rows to the rider's
  `uber_rows` (a skin's own Nose/Tail Grab override still wins, as 0x150198 returns it first), `humanSettings` writes
  them into `original_grab_control.profile.uber`, and the screen reloads the rider (`ui.cb.rider(human)`). With pv
  `careerRider` (2026-09-27) the rows come from the profile record at every world / event load and run start instead
  (web/career-rider.js; docs/career-events.md "In play"), so the lodge's selection and a reload keep them. The list
  opens on its first row and wraps (PS2); Cost / You have show the focused row's price, owned ones too.
- **Ubertrick Setup trick preview (0x184C60).** The FE preview slot shows the rider **on the board** in the panel:
  camera eye (0,660,80) -> (0,0,80) (0x184A78, 25 degrees), root (-228,-205,63) cm turned 62 degrees about Z (0x184D08,
  half-angle constant gp-0x59F0 = 31 degrees), no 0x19F548 board move. It loops semantic 436 = FE_A_CYC (fe:66) and
  Square plays the focused uber: the animator's semantic becomes the uber's **trick id A** (animator +8 = 0x7B for
  Indian, 0x9E for Pommel Me in the derived states `local/reference/pcsx2/characters/fe-screens/` via prev1/prev2),
  whose FE variant (runtime table *(gp+0xD0C), all masks) is fe:(id-93): 113..148 = UBER_<GRAB>_<n>_L<k>, 149..158 =
  UBER_TAIL_/UBER_NOSE_<PREFIX>. It plays once, then crossfades back into FE_A_CYC (0.23 s). The panel (layers 0..11)
  goes on the UI background canvas behind the rider. Implementation: `FeScreens.ownsRider/place/pose/showPreview` in
  `web/fe-screens.js` (main.js `frontEndRider()` picks it on `fe-uber`), the FE package through
  `ui.characterSelect.preview3d` with the new `apply(..., {board: true})` option in `web/fe-preview.js` (board
  meshes shown only there), the race rig as fallback. Checked against the PS2 frames (pair-uber-preview.png).
- **Options (18options).** All eight items are enabled in the original (no enable calls). Game Options, Controller
  Settings, HUD Options and Credits open their original screens (`web/fe-options.js`, below); Sound Options calls
  `ui.cb.audioOptions()`, which the audio agent's `web/audio-menu.js` sets to open 141advsettings (`fe-sound`, its
  sliders and modes via `gameAudio.setSettings`); Save/Load opens Load game; Enter Cheat opens the keyboard (empty,
  focus on q, only Tab/Up/Down disabled) and passes the text to `ui.characterSelect.enterCheat(text)` (0x187D38 rules,
  see above); DONE goes back. The pause menu keeps the browser `options` screen (camera / widescreen / keyboard).
- **Option screens (`web/fe-options.js`, `FeExtraScreens`, routed by fe-screens.js).** Rows are the screens' 0x15
  option rows (label + value widget; exported as kind 'option'); Left/Right change the value with the hll/hlr arrow
  flash, Square resets the screen's options, Triangle returns to Options. Values persist in `localStorage`
  `ssx3.feOptions` (`loadFeOptions()`: speedUnits 0 Mph / 1 Km/h, vibration1/2 0 On / 1 Off, controller1/2 0 Default /
  1 Pro, hud 0 Full / 1 Minimal / 2 None, screenX/screenY -20..20) and every change calls
  **`ui.cb.feOptions?.(options)`**. Changes are kept only when leaving Options answers Yes to "Would you like to save
  your Options?" (the FE.LUI `popup` 0x007767C0, drawn over Options as on the PS2 frame); No keeps them for the session
  (the race still uses them, as on the PS2). What the race reads is under "Options in the race" below.
  - Game Options: Speed units, Widescreen (the browser's own mode, `ui.cycleWidescreen`), Screen position (below) and
    Video calibration (its popup needs a TV: disabled). Language (and Auto save) are hidden as on the US PS2 frame.
  - **Screen position** (cFEPopupScreenPos 0x1DEE10, vtable 0x46B230, screen 24screen_position 0x0746461E): the popup
    over Game Options ("Position the screen to fit your TV.", the four arrows, Confirm / Cancel legend). x / y are the
    profile's signed bytes 0x535616 / 0x535615 (0 after a reset, 0x14F5D0); each D-pad press moves one step (Right/Up
    +1, Left/Down -1, clamped to -20..20, the pressed arrow pulses 'hl left/right/up/down' = frames 80/85/70/75), Cross
    keeps, Triangle restores. The popup sets the display live (vtable +0x60 = 0x393FB8); the NTSC display set-up
    0x382DC0 turns x / y into DISPLAY DX = 636 + 5 + 8x VCK (2560 per line) and DY = 50 - 2y (480 lines): **one step =
    1/320 of the width right, 1/240 of the height up**. The browser moves `#stage` by those fractions (CSS `translate`,
    `SCREEN_POS` / `applyScreenPosition` in fe-options.js), live while the popup is open and from the saved options on
    load. PS2 check (nav `scrpos`, PCSX2 applies DY but not DX): after 10 Up presses the picture is 20 lines higher
    (2 lines per press: frames 1221/1280/1340/1480 = 2/6/10/20); the popup matches the PS2 frame
    (`pair-screen-position.png`: underlying rows hidden, legend white, the message wraps after 'your').
  - Controller Settings: with a gamepad the DualShock picture with the Default or Pro labels, as on the PS2 frame;
    with the keyboard as the last input device the pad gives way to the keyboard table of `web/loading-screen.js`
    and Controller 1P switches Simple / Classic (`ui.cycleKeyboard`).
  - HUD Options: the three states with their pictures (FE_1-15/16/17) and help texts.
  - Credits: the 160 `kT_CREDITS*` lines of CRAMER.LOC in the order of 0x185C54..0x185F04 ('\\\\' breaks, '^' heading:
    60% light, 40 px, 20 px gap above unless after a blank or a heading; names 50% dark, 20 px), centred, scrolling
    up 1 px per frame inside the TextScroll box (62,95,525,300), the Dolby bitmaps (Bitmap0000/0001) after the text;
    positions fitted to the PS2 roll at samples 570 and 870.
- **Rewards room (128rewardsroom / 129 rewardgallery / 130rewardposter).** Categories with owned / total for the
  rider; a category opens its gallery: 8 x 4 slots per page (Page n/m, L1/R1 = Q/E keys when there are more pages),
  'questmark' boxes until owned (bright on the focused slot, 151 elsewhere - PS2 frame), the thumbnail when owned
  (REWARDS picture, cheat characters their face), the item label and name, You have / Cost for a priced item and
  its help ("Buy this item in Conquer the Mountain mode."). Cross on an owned item shows it (130rewardposter).
  **Videos.** An owned Video plays full screen instead (the MoviePlayer state 0x1D23E0: 'MoviePlayer' object, the
  sound system paused 0x2B3A70 while it plays, back to the gallery 0x1D2638 when it ends or on the skip buttons
  0x1D2518): `playMovie` in fe-options.js puts a `<video>` over `#stage` (object-fit fill, 4:3), suspends the game
  audio engine (`ui.gameAudio.engine.suspend/resume`), Enter/Space/Escape (or a click) skips. Files come from
  **`tools/export_movies.py`**: "SSX 3 Intro Video" = DATA/MOVIES/INTRO.MPC, "E3 Video" = MTNALIVE.MPC (career.json
  rewards.video). MPC = EA SCxl chunks: `MPCh` one MPEG-2 access unit each (512x448, 29.97 fps, 6219 / 6060 frames),
  `SCHl` 'GSTR' audio header (44000 Hz; INTRO 6 channels L C R Ls Rs LFE - INTRO_DJ.MPC differs only in channel 1, the
  DJ voice; MTNALIVE stereo), `SCDl` one EA-XA R3 block per frame. ffmpeg's `ea` demuxer reads the video but not this
  audio (GSTR has no revision element; more than 2 channels refused), so the tool rewrites each SCHl as a 'PT'
  revision-3 header and splits the 6 channels into 3 stereo streams, folds them to stereo (L + 0.707 C + 0.707 Ls /
  R + 0.707 C + 0.707 Rs, peak -1 dBFS), scales the picture to 640x480 and writes H.264 + AAC MP4 with faststart:
  `web/public/assets/MOVIES/INTRO.mp4` (94 MB, 207.4 s), `MTNALIVE.mp4` (70 MB, 202.2 s) + `movies.json` (git-ignored).
  ffmpeg: `--ffmpeg PATH` / `$SSX3_FFMPEG` / PATH / imageio_ffmpeg (this machine: an existing local imageio_ffmpeg
  7.1 binary; nothing downloaded). Without the files the poster view is shown and the console says how to export.
- **Load game (93profile_load).** ARMSX2 has no memory card, so the PS2 frame shows "Please insert a memory card (PS2)
  into MEMORY CARD slot 1.". The browser's card is `localStorage` `ssx3.career.v2` (`career-save.js`; v1 saves migrate, see "Saving progress" below): with a save,
  row 1 shows the player name and the save's date and time (`career-save.js` now stores `savedAt` on every write;
  older saves load and show none) as 0x2C7038 `%02d:%02d:%04d` (month first) and 0x2C6F78 `%02d:%02d:%02d` (the
  24-hour form, the one that fits the 70-wide time1 widget), Load / Cross; loading puts the stored save into
  `careerUI.career.save` ("Loading...", "Load complete."), then goes back. Without one: "<Empty>".
- **Keyboard (Fullkeyboard).** The LUI keyboard at its frame-45 layout (the one on the PS2 frames); the key labels
  (set by code) come from the PS2 frame; the focused key is drawn opaque black. Browser: typing on the computer
  keyboard inserts the character and moves the focus to Done; arrows move the focus; Enter presses the focused key;
  Backspace = Back; Escape cancels.
- **lui-player.js changes (backward compatible):** zero-height/width UV boxes sample their own texel row (the FE_1-11
  dash rows at v 253.5 now draw as dashes, also on Select Character); gouraud triangles follow the plane of the
  channel that varies (alpha or colour), so split quads keep straight ramps (the white ramp, the 18options sky grid);
  text overrides may set `pitch` and `sy`.
- **Checks:** `web/test-fe-screens.mjs` (npm test): exported screens and menu texts, focus states and help texts,
  profiles for all riders, enable rules and wrap/skip, the uber rows against initial.json and through
  `loadCharacter`/`humanSettings`, the keyboard, and the whole flow with a stub UI (Continue in single/career/online,
  Music hook, Rider Details, Profile paging, Player Name, Options -> Game Options and back, Enter Cheat, Load game,
  Ubertrick Setup incl. the preview clip mapping), the option screens (values, persistence, reset, Simple/Classic),
  the credits layout against the PS2 roll, the save timestamp, the preview hide windows. Browser screenshots next to
  the PS2 frames: scratchpad `chars/screens/pair-*.png`; the PS2 frames are kept in
  `local/reference/pcsx2/characters/fe-screens/frames/` with the nav scripts.
- **Preview hide windows.** Entering Setup Character or Rider Details calls `ui.characterSelect.hidePreviewFor(frames)`
  (26 after the Select Character flash, 30 between Setup and Rider Details; PS2 slot +0xCC8 = 0); the cheer is set at
  t = 0 and its clock starts when the model shows (`character-select.js` holds `anim.t` at 0 while hidden). Re-entering
  Setup from Music / Options / Rider Details replays the cheer (ARMSX2 frames), so the screens reset `lastScreen`.
- **Rider Details pose:** re-checked after the FE-preview agent's change (cheer on entry): the pose at 118 frames after
  Cross matches the PS2 frame (pair-y-details-118.png).

## Main menu, Single Event track selector, Previews, Save/Load (2026-09-25)

Four more original FE.LUI screens, exported by `tools/export_fe_menus.py` (in `npm run setup`) to
`UI/fe-menus.json` (git-ignored) with the widget names the executable spells out (`ara1path`, `Peak1RaceLocations`,
`MapPic`, ...): `Map` 0x00005380, `femap_template` 0x0437FEC5, `07main_men` 0x08065FDE, `146Bonusmat` 0x063496A4,
`25saveload` 0x08CE8C54 (and `122Autosave`, not used yet). PS2 references: `local/ps2-capture/menus/ctm/01-main-menu.png`,
`local/ps2-capture/menus/single/04-peak-select.png` .. `07-fs-events-*.png`, `local/ps2-capture/menus/previews/`.

- **Main Menu** (`web/fe-main-menu.js` draws 07main_men; `web/ui.js` keeps the logic): intro to frame 21, focus
  states 35/40/50/60/70 (white text, the orange `hl` bar, the item's own help text in the left panel). Enabled:
  Single Event, Conquer The Mountain (career data), Previews (trailers exported), Online (the lobby, `ui.mpUI`).
  Multi Play (local split screen, 86multiplayermode) is not ported and stays greyed (alpha 128, 0x194498). Up/Down
  wrap and skip greyed items; Square (Shift, touch □, or the clickable legend) opens Options as on the PS2 legend.
  Hover selection keeps the real-mouse-movement gate of `sync()`.
- **Previews** (`web/fe-previews.js`, 146Bonusmat, cFEStateBonusMaterial input 0x195600): Need for Speed Underground /
  NFL STREET / NBA STREET Vol. 2 play `DATA/MOVIES/NFSXSELL/NFLXSELL/ST3XSELL.MPC` (path table 0x441128) full screen
  (`web/fe-movie.js`, shared with the Rewards videos). `tools/export_movies.py NFSXSELL NFLXSELL ST3XSELL` exports them
  (git-ignored, ~60 MB); a missing trailer greys its row. The disc's own INTRO ("SSX 3 Intro Video") and MTNALIVE
  ("E3 Video") are rewards (Rider Details > Rewards > Videos), as in the original.
- **Single Event track selector** (`web/fe-event-select.js`, screens `fe-peak` / `fe-mode` / `fe-event`; `ui.set('event')`
  redirects to `fe-peak` when the Map screen is exported, else the old list stays): the `Map` widgets over
  `femap_template`, FE art only (not the MCOMM `pda` art or the confirm popup).
  - *Select Peak*: SP_PeakList Peak 3 / Peak 2 / Peak 1 (All Mountain hidden), `MAPGFX_map_mtn` in MapPic, the focused
    peak's outline (`peak<n>outline`, vertex alpha 0 in the data, shown by the code), the `phl` bar at y 103 + 25 i,
    lock sprites (Locks group) on Peak 2 and Peak 3. **Peak 2 and 3 are locked for now** (no courses ported): the text
    stays black with the lock (PS2 frame), Cross does nothing, help `kT_10HELPUnlockPeakN`.
  - *Select Mode*: `SPG_Peak<n>Goals` Race / Freestyle (Freeride / Earnings are career goals), `MAPGFX_map_peak<A|B|C>`,
    the map tab "Peak 1", every route of the focused mode red (227,42,9, the LUI colour) and the others orange
    (224,134,32; PS2 216..222,126..132,30); the backcountry belongs to Race here (PS2 05-select-mode / 05b).
  - *Select Event*: `Peak<n>RaceLocations` / `Peak<n>FreestyleLocations` with the goal-list order 0x45AAD8: Snow Jam,
    Metro-City, Happiness (Rival Time) / R&B, Crow's Nest, The Junction, Happiness Jam (Rival Points); Peak 1 Race /
    Jam only once the career opened them (`careerUI.singlePeak`). The focused route red, the start indicator from the
    Map state of that course (frames 200..490, matched by indicator position to each route's end: `INDICATOR_STATE`,
    checked in the test), the event type on the map tab, the help `kT_HELP<code>` (Metro-City: `kT_HELPBRA2Blah`,
    because `kT_HELPBRA2` hashes like `kT_HELPCBA2` = Launch Time; the MCOMM Transport had the same bug, fixed).
    Additions from the same screen: the medal column shows this rider's best career medal, and Square toggles the
    SE_Info table (Run / Event / Medal / Top time or Top score, as the MCOMM Transport shows it).
  - Cross on an event calls `ui.startSingleEvent(entry)` -> `careerUI.single` (objectives card, final round) ->
    `cb.course`; `cb.course` / `cb.peakRun` may return false (navigating), true, or a Promise of either
    (`careerUI.afterCourse`). The last peak / mode / event is remembered (below).
- **Save/Load** (`web/fe-saveload.js`, 25saveload; Options > Save/Load): Save game (writes the career now), Load game
  (93profile_load, returns here), Save options / Load options (`ssx3.feOptions`), Load replay (greyed: no replays),
  New game ("Erase all saved progress?" with the FE.LUI popup, No focused: a fresh career; settings and free-play
  outfits stay), and two browser rows in the same style: Export save file (downloads `ssx3-save-YYYYMMDD-HHMM.json`)
  and Import save file (validated, migrated, written atomically, then the page restarts with it).

### Saving progress (browser memory card)

Everything a returning player expects comes back without a Load: the original loads the profile at boot and the
career autosaves; the port does the same in `localStorage` (`web/save-store.js`).

| key | what | writer |
|---|---|---|
| `ssx3.career.v2` | Conquer the Mountain per rider: cash, earnings, medals, best values, events/rounds in progress, attributes bought, peak passes, gear owned/equipped (`gearFlags`), ubers, songs, rewards, collectibles, Big Challenge status words, career messages, run stats; records; roster seed | `career-save.js`, after every result, purchase, pickup, lodge change, message |
| `ssx3.outfit.free.v1` | free-play (Single Event / online) outfit per rider | `wardrobe.js` |
| `ssx3.selection.v1` | last rider (+ cheat skin base), Single Event peak / mode / event | `ui.js` (Cross on Select Character), `fe-event-select.js` |
| `ssx3.relationships.v1` | rider relationship bank | `ai-race.js` |
| `ssx3.feOptions`, `ssx3.widescreen`, `ssx3.keyboard`, `ssx3.soundMode`, `ssx3.audio`, `ssx3.musicMode`, `ssx3.playerName`, `ssx3.cheatCharacters`, `ssx3.touch`, `ssx3.quality` | options, controls, audio, player name, cheat unlocks, touch deck, quality | their screens |

- **Restore:** the career and every setting load at start-up; the last rider is loaded on the first Select Character
  of a visit (a `?rider=` URL wins); the track selector opens on the last peak / mode / event.
- **localStorage, not cookies:** cookies travel to the server with every request (all ~100 MB of `/assets` fetches
  behind the gate), hold ~4 KB each (the career save is larger) and would need server code; localStorage stays in the
  browser, holds ~5 MB per origin, and reads synchronously at start-up. IndexedDB would only add an async API for a
  save of tens of KB. One save per browser profile and origin (the hosted site's); Export/Import moves it.
- **Schema and migration:** versioned keys; the save file is `{format:'ssx3-save', version:2, exportedAt, keys}`.
  Career v1 (`ssx3.career.v1`) migrates on first load to v2 (version 2, `migratedFrom`, riders checked); the v1 key is
  left as a backup; a v1 export imports as v2.
- **Atomic writes:** `writeText` puts the value in `<key>~tmp`, then the key, then removes the temp copy; `readJSON`
  finishes an interrupted write (a complete temp copy wins, a torn one is dropped). With no room for the temp copy the
  key alone is written (setItem is atomic per key).
- **Errors:** blocked storage (sandboxed frames, disabled site data, some private modes) reads as empty and writes
  report false (in-memory for the session); Save game says "Save failed: browser storage is full or blocked.".
- **Checks:** `web/test-save-store.mjs` (npm test, storage mocks): atomic write order, recovery, full and blocked
  storage, v1 -> v2 migration, a whole career reload (attributes, cash, medal, collectible, gear, messages, Big
  Challenges, pending round), selection, export/import round trip and rejections, the Single Event lists and the
  indicator states. Browser (2026-09-25): bought attributes, cash, a medal, a collectible, the HUD option, the rider
  (Moby) and the track cursor (Freestyle / The Junction) all came back after page reloads; Cross on The Junction from
  the new selector reached its objectives card.

### Options in the race

- **Speed units** (profile word 0x535610 bit 19 = Km/h; 0x220260 prints cm/s x 0.036 = km/h or x 0.621 of that = mph):
  the race HUD speed reads `ui.feScreens.speedUnits()` (m/s x 3.6 KM/H, x 2.237 MPH). Checked on a derived Snow Jam
  countdown with Km/h (`local/reference/pcsx2/characters/fe-screens/options/snow-jam-countdown-kmh.p2s`).
- **HUD Options** (0x535610 bits 30..31: 0 Full, 1 Minimal, 2 None): `ui.js` game branch reads `hudLevel()`. Minimal
  keeps timer, score, place, boost gauge, career HUD and the trick HUD with the owner flags masked like the PS2
  (owner+0x3CC from table 0x478078 & ~0x0510C040: 0x1530C047 -> 0x10200007, `minimalHudFlags`), and drops speed and
  progress; None draws nothing but the countdown and the banners. Checked against PS2 race frames (Full / Minimal /
  None countdown states in the same folder).
- **Controller 1P = Pro** (race copy 0x535B30 = 1 -> compiled input map context+4 = 0x542580, INPUT2.MAP): `main.js`
  startRun calls `core._set_input_map(ui.feScreens.inputMap())`. The engine keeps INPUT.MAP bit-exact and adds the Pro
  table as a second expression set selected by that flag (`engine/original_input_provider.cpp` `proExpression`:
  PrewindTurn 0, PrewindSpin = Turn, PrewindFlip = Tilt, RailBalance = !Cross ? Turn : 0, HandplantBalance = Turn,
  LateSpin = Cross, RailSpin = |Tilt| < 0.5 || |Turn| > 0.2 ? 0 : Tilt, Spin = Flip = 0, AirAdjRotLR = Turn,
  AirAdjRotFB = Tilt, Handplant = Triangle; LookBack = Circle is never read by the ELF). The air controller 0x1333E0
  late-spin modes are recovered (`engine/air_control.cpp`: mode 0 + LateSpin with an adjust -> 2; spin/flip -> 0;
  mode 2 without LateSpin -> 1; modes 1/2 spin/flip from the adjust axes), fed by control-5 word0 bit 15
  (`web/input_bridge.inc` `rideLateSpin`). Gate: `pro-event` in `web/test-ps2-captures.mjs` (a Snow Jam race with
  Controller = Pro, `local/ps2-capture/scripts/pro-event.json`, `compare-ps2-capture.mjs --pro`) is exact through all
  950 ticks; with the Default map the words differ from tick 364. The Controller Settings keyboard table has a Pro
  variant (`keyboardProRows`: no IJKL spins, W steers and spins/flips with Space held, Y hand plant).
- **Vibration 1P** (`web/rumble.js`, `main.js` simTick -> `rumble.tick` just before `gameAudio.gameTick`, Gamepad
  `vibrationActuator` 'dual-rumble'): the original motor model of 0x125B18 (decay v0 x 0.9133333 - 18.518518 and
  v1 x 0.9916667 per tick, single precision; large motor f = max((v1 - 0.5) x 0.01, (v0 - 2) / 972.2222) -> byte
  f x 205 + 50, small motor on above (v1 - 70) x 0.00625 or (v0 - 100) / 5000), only while racing and the option is On
  (0x127900: option 0x1474E8 and game state < 10); pause, quit and Off stop the motors (0x326CF0). The inputs are the
  core's own rumble calls, posted on the audio event queue (read before `web/sfx-game.js` drains it):
  `AE_RUMBLE_IMPACT` 30 = rider vtable +0x88 -> owner +0xDFC = max(v0, a) (landing award 10EAD8 with 0.5 x the 10E910
  speed for board / rail / instance landings, crash entry 10EB78, obstacle notifications 105E7C, instance contacts
  105C30, ragdoll impacts 12D28C / 12D6F8 / 12D8A0) and `AE_RUMBLE_SLIDE` 31 = +0x90 -> owner +0xE00 = a (crash slide
  12D23C 0.5 x the 12E528 playback base; ground get-up 12D8E8 2 x (1 - clip progress)). **Exact vs the PS2**
  (`web/test-rumble.mjs`): v0 against the recorded +0xDFC on every tick of mix-glide, air-tricks and event-race, and v1
  against +0xE00 plus both motors on every tick of the derived `local/ps2-capture/runs/rumble/{mix-glide,air-tricks}-rumble`
  captures (same scripts, `--watch 0x147018c:8` = owner 0x146F390 +0xDFC; the default record window stops at +0xDFF).
- **Screen position** applies everywhere (the whole `#stage`), see Game Options above.

## Front-end preview (2026-09-23)

The 3D rider on Select Character, Setup Character and Rider Details is the original **FE preview model**, not the
race rider: `tools/export_fe_preview.py` -> `web/public/assets/RIDER_<ID>/fe/`, drawn by `web/fe-preview.js`.

- **Slot** (`*(*(gp-0x848)+0x7C)` = 0xBC5910 in every state, gp 0x4A30F0): slots at +0xB0 + 0xCE0 x player.
  - Slot fields: +0 char id, +8 geometry, +0xC animator, +0xC30 root position, +0xC40 root quaternion, +0xCD8
    variant mask.
  - Geometry: parts at +0x0C, count +8, 0x58 stride. Per part: +0 file id, +4 first bone slot, +0x18 active,
    +0x38 bones, +0x44 bone count. Bind bank at +0x38, world bones (32 B) at +0x2C.
  - **The part's model header is entry +0x50 of its variant table +0x1C** (16-byte entries {header, material
    names, flags, ..}), not always entry 0. Zoe's TopB/BottomB/HandsB_NIS/DummyB_NIS and Nate's TopC use variant
    1..5. Each matches an MDLPS2.BIG LOD0 header byte-exactly (bytes 56..67 masked).
  - The MNF header's file id (+94) equals the live part file id.
- **Assembly** (`select.p2s` of all ten riders): the race outfit, both board parts, and the cinematic parts:
  - `HeadX` -> `<prefix>_HeadA_NIS` (file 5, 36 morphs);
  - `<prefix>_Eyes_NIS` (file 6, bones `eye_r`/`eye_l`; shared bytes, e.g. arielle/kaori/abom);
  - `HandsX` -> `<prefix>_HandsX_NIS` (file 8, left hand, 27 morphs) + `<prefix>_DummyX_NIS` (file 9, right hand,
    27 morphs).
  - Kaori: kaori_TopA, board_BindingsA, board_BoardFlexA, kaori_BottomA, kaori_HeadA_NIS, kaori_Eyes_NIS,
    kaori_HandsA_NIS, kaori_DummyA_NIS, kaori_BootsA, kaori_PigtailsA (28 bones).
  - Zoe: TopB, BottomB, HeadA_NIS, Eyes_NIS, HandsB_NIS, DummyB_NIS, BootsA, Mop.
  - Every FE bone slot is active. The FE bind matrices are bit-identical to the race binds for every shared bone,
    and the geometry is unscaled.
- **Morphs** (GameCube MNF; `rider_assets.decode_high_model` now returns `morphs`, `vertex_positions`, `file`):
  - Records are 16 bytes at `morph_data`: {count, compact, deltas, indices}. Then come count u16 raw-position
    indices and count xyz deltas: **int16 in position units (1/12700 m), int8 (compact = 1) in millimetres
    (1/1000 m)**. See "Morph units" below.
  - `morph_ids[i]` is the channel of record i. The weight of morph i is that channel of the FE clip's stream for
    the part's file (`library.json` fe streams '5' = 36, '8'/'9' = 27). Weights are linear, unclamped, and
    applied before skinning.
  - Kaori's peace signs, Zoe's fists and the faces come from these channels. The eyes are bone channels (stream '6').
  - `FE_GEAR_MAC_CYC` has no face stream, so Mac's idle keeps the neutral face.
- **Morph units, measured against the PS2** (`rider_assets.decode_high_model`):
  - The PS2 MPF twins keep their morphs in VIF packets after each vertex chunk: `UNPACK V4-8`
    {count,0,0,0} then {dx, dy, dz, slot}, with slot = 3 x the chunk vertex (positions are `V3-32` floats in cm).
  - Matching every PS2 packet vertex to the GameCube raw position by its float position and fitting per morph:
    - GC int16 = 50.8 x PS2 (Kaori and Zoe hands, residual ~0.001);
    - GC int8 = 4.0 x PS2 on the best-fitting vertices (hands and head; 4.1 to 4.5 on the rest, PS2 rounding).
  - So one PS2 unit is 4 mm, a GC int16 unit is 1/12700 m (the vertex unit), and a GC int8 unit is 1 mm, a ratio
    of 12.7.
  - The int16 anchor is also physical: Kaori's finger-curl tip moves 11 cm on a 13 cm hand.
  - The first export treated int8 as position units, 12.7x too small (a 0.9 mm blink). Faces now open the mouth
    and close the eyes like the PS2 frames.
  - Tool: scratchpad `chars/fepreview/morphscale.py`.
- **Animation timing, measured against the PS2** (derived states from `characters/<id>/select.p2s` with Cross,
  then Down x2, Cross (Rider Details), Triangle (back); ARMSX2 snaps every 5 samples with the savestates kept).
  The sequence behind animator head +0x14 has clip +4 and time +8:
  - Cross immediately starts 435 at t = 0, held while the model is hidden (slot +0xCC8 = 0). Its clock runs from
    the frame the model shows (~30 samples later).
  - At the cheer's end (Zoe 2.6333 s) the idle restarts at t = 0. The head then lists two sequences (the cheer
    copy held at its end) while idle t is 0.05..0.2167, and one again at 0.3833, which fits the linear 0.23 s
    crossfade.
  - Rider Details (Zoe sample 456: 435 at t = 1.2667 under the "Rider Details" title) and the return to Setup
    Character (435 at 1.7167) both restart the cheer.
  - `FE_A_CYC` (436) is played by 0x184C60 in the vtable 0x46CCC8 of the Ubertrick Setup state (string
    "66ut_btnmap"), with the model at (-228, -205, 63). It was never Rider Details' clip; the browser now plays the
    cheer there. `rider.json fe.clips.ubertrick` keeps FE_A_CYC for that screen.
  - Browser frames pinned to the PS2 clip times match in pose, hands and face for Zoe (9 frames incl. crossfade),
    Kaori (8) and Mac (8). See scratchpad `chars/fepreview/cheer-{zoe,kaori,mac}.png` (browser / PS2 pairs) and
    `details-compare.png`.
- **Textures:** the outfit rule of the race packages (`export_characters.rule_textures`); each one is checked resident in the select state (`resident_textures`) or equal to the race package's.
  - EA's PS2 rider textures hold colour at **half intensity** (8-bit CLUT, 128 = 1.0). The GameCube twins are
    ~2x (CMPR).
  - The FE package stores the PS2 texels (RGB from the PS2 texture, alpha from the GameCube one;
    `texel_domain: 'ps2'`). GS HIGHLIGHT2 `tex x light / 128 + light.a` then gives the PS2 brightness.
  - Sam's GameCube-domain textures are halved in the shader.
- **Lighting** (0x19EE88, taken when slot+0xCD4 < 0):
  1. Clear a bank (0x389260).
  2. Add the IRR.DAT record with weight 1.0 and modulation (1,1,1,1) (0x389590; the bank equals the record).
  3. Run 0x389CB8(bank, world position of the `hips` bone (0x310C48), 1.0) with the FE camera's view matrix
     (camera block +0x40; exact bits in `rider.json fe.view_matrix_bits`).
  4. Copy the bank to the renderer (vtable+0x22C = 0x3954D0 copies it to renderer+0x6BB0). VU program 2 scales
     it x255.
  - There is no environment bank and there are no local lights.
  - Record by id: 0 moby, 1 kaori, 2 allegra, 3 mac, 4 zoe, 5 griff, 6..8 elise, 9 viggo, >= 10 `fe_map`
    (slot+0xCD0 = 0 in every state, so the Moby-to-elise branch is not taken).
  - Verified: VU1 data qwords 7..16 of Kaori's select state are her record x255 plus the rim lane (row 0
    = 210.649, 151.216, 161.555, 94.749). `web/fe-preview.js` gets the same rows from the core's own port
    (`_shade_rider_lighting` with an empty local-light selection; no C++ change).
  - The VU1 normal matrices are world skin rotations, so the vertex evaluation uses world normals (Z up), as the
    race rider does (`rider-lighting-nodes.js`).
- **Browser:**
  - `CharacterSelect` (`web/character-select.js`) asks `FrontEndPreview` for the entry's package: the base rider, or
    the cheat skin with `?feSkin=1`. It plays the same FE clip state machine on the preview skeleton and morphs, and
    places it at the recovered root.
  - main.js passes `core` to `pose()` and hides the race rider while `showPreview()` is true. Like the original,
    nothing is drawn while a preview loads.
  - Without a package, the race rider stands in as before.
  - Camera: `place()` now sets the 4:3 aspect and the vertical tangent tan25 x 3/4. main.js's
    `setViewOffset(640,448)` had left aspect 640/448, which stretched the preview vertically by 7%.
  - With that fix, the bone world positions equal the PS2's (Kaori hips 147.39, -245.70, 23.17 cm) and project to
    the spec's pixels.
- **Cheat skins:** after Rider Details > Cheat Characters, the original Setup Character preview keeps the base
  rider (`characters/<cheat>/setup.p2s`: setup slot +0x12 = the cheat id, preview = Zoe's FE assembly, idle
  t = 0.2833). The browser does the same.
  - Their FE packages are built by the base riders' rule (race outfit + their own `_NIS` parts from MDLPS2.BIG,
    race textures and binds, NIS-only eye bones from the inverse rest, `fe_map`) and are marked `evidence: 'rule'`.
- **Sam:** his own race package, copied to `RIDER_SAM/fe`, with Mac's clips and `fe_map`.
- **Checks:**
  - `web/test-fe-preview.mjs` (npm test) covers all 31 packages. It checks:
    - vertex, index and morph sizes;
    - the bind mapping;
    - for the ten riders: the live NIS assembly, files 5/6/8/9, 36/27/27 morphs, and IRR records by id;
    - the FE clips' stream channel counts against bone and morph channels;
    - core lighting RGB lanes equal the record, with a nonzero rim;
    - Kaori's exact assembly and curled fingers.
  - Browser screenshots of all ten riders, with the animator clock pinned to the PS2 state's (sequence +8, e.g.
    Kaori 1.0 s, Zoe 0.667 s), match the ARMSX2 select frames in pose, hands, face and colour. Kaori's jacket
    reads 140,31,24 against the PS2's 138,29,23.
  - Scratchpad: `chars/fepreview/compare-all.png`, `pair-setup.png`, `zoom-kaori.png`.

## Computer-rider lineups (2026-09-23)

The five computer riders of a race are now picked the original way for any human: the ten riders, Sam and the twenty
cheat skins (on any base rider). Psymon meets Nate, Allegra, Moby, Zoe and Luther in the reference sessions, exactly as
on the PS2. Zoe with the anchor lineup races the unchanged `npc-riders.json`.

### The original (SLUS_207.72)

- **When.** The race rules object's event start 0x23A108 (vtable 0x47CF4C) builds the roster.
  - A Single Event (0x535C11 != 0) builds it whenever an event is chosen: 0x238C80 resets the rules object, which
    sets roster+0x84 = 1 and round 0.
  - A restart keeps the roster.
  - Conquer the Mountain builds it in round 1 and reuses it in the semi and the final.
- **Build 0x23A4F0.** The ten roster entries are made in this order:
  1. Two distinct "Tricky" cheat riders: `10 + r % 7`, then `10 + r % 6` (+1 when it is at or above the first). These
     are Brodi, Eddie, JP, Luther, Marisol, Marty and Seeiah.
  2. The base riders 0..9 in id order, except the human's character and the peak rival 0x145750.
  3. A shuffle 0x23C770: 25 swaps of entries `r % 10`, `r % 10`.
  - The peak rival uses course table 0x43D950 + event*100 + 0x54. Peak 0 (Snow Jam, Metro-City): Mac, or Griff when the
    human is Mac. Peak 1: Nate, or Zoe. Peak 2: Psymon, or Elise. So Mac never races as a computer rider on these
    two courses.
- **Heats (0x23A108).**
  - Single Event and career round 1: entries 0..4 go to slots 1..5.
  - Semi: the qualifier's top three without the human (0x536708 finish order), then entries 5..7.
  - Final: the rival, the semi's top three without the human, then entries 8 and 9.
- **Copy 0x23A668.** It writes *(gp-0x480)+0x18[]/+0x40[] and then 0x2342B8 the race copy 0x535B20. A value below 10
  is that base rider. A value of 10 or more is that cheat skin on the **human's** base character.
- **Random numbers.**
  - Every draw is 0x237CD8 = 0x317A08 on the roster generator at 0x4C9548 (six words, the same add-with-carry generator
    as the shared game RNG). One build makes exactly 52 draws.
  - The game construction seeds that generator at the event load (0x22EFE8 -> 0x237CB0 -> 0x317958). The seed is one draw
    (0x3177F0) of the presentation generator 0x4FF018.
  - The presentation generator is seeded once at boot (0x31AE94) from `sceCdReadClock` (0x402520): `w0 ^ (w1 << 3)` of
    the BCD clock words. Front-end and loading effects then draw from it.
  - The replay path 0x25861C seeds the roster from a draw of the restored shared RNG instead.
- **Timing.** The lineup does not depend on the frame count in the menus.
  - Every reference session has boot seed 0x182200, including all 31 derived character states and both course anchors.
  - Their roster seed is presentation draw **130** = 0xB57109A9, whatever the menu timing, the character or the cheat path.
    The select states are at draw 1, and the setup, peak, mode, event, rules and load screens make 128 more before
    the seed.
  - The lineup of the first race after boot is therefore a function of the boot clock (and the human). A later event
    continues the generator after the previous race's per-frame presentation draws.
- **Check.** Stepping each savestate's 0x4C9548 words back (the generator is invertible) gives a seeded state after
  exactly 52 draws. With that seed, 0x23A4F0 gives the state's lineup in all 70 countdown states.
  - To test the causal chain, the 0x4FF018 words were poked in a copy of a select state. The run then produced exactly
    the predicted lineup.

### Browser policy (web/lineup.js)

- **Seed.** One presentation generator per page, seeded like the boot from the local clock (`clockSeed`).
  - The first event chosen from the menus takes its draw 130 as the roster seed. Each later event takes the draw 129
    further on. The original's draws between two loads (per-frame effects of the previous race) are not modelled.
  - `?presentationSeed=0x182200` replays the reference sessions: the first race gets the anchor lineups.
  - `?lineupSeed=0x...` fixes the roster seed.
- **Where it runs.** `ai-race.js prepare()` runs from the main.js warm-up hook of `ui.loadEvent`, so it is part of the
  loading screen.
  - It builds the roster, assembles the documents and sets up again only the cores whose record or relationship row
    changed (`init_animation` + `npc_configure` in place, `ai-racers.js setDocument`).
  - It rebuilds the opponent renderer when a model changed and keeps the human's pair inputs.
  - It skips online races.
  - Career rounds 2 and 3 reuse the round-1 roster and advance by the last results order.
  - A restart keeps the lineup.
- **Human character.** From riders.json: Sam is Mac's slot (character 3, as in the Sam build), and a cheat human uses
  its base rider.

### Computer-rider data for every possible rider in every slot

`tools/export_lineups.py export` writes the full npc-riders document of every countdown state to
`local/assets/native/<course>/lineups/*.json`.

- **Code.** It uses `export_npc_riders.extract_document`, refactored from its `main()`, whose output is unchanged. The
  participants come from the race copy 0x535B20.
- **Snow Jam states.** The 30 character states, Brodi on Psymon, and 18 coverage states.
- **Metro-City states.** Its anchor and 19 coverage states.
- **Coverage states.** `characters/scripts/make_lineup_states.py` makes them from `characters/<human>/select.p2s`: the
  0x4FF018 words are poked so that draw 130 gives a chosen seed, then the original menu path runs (for Metro-City, D-pad
  down on Select Event). They are saved in `characters/lineups[-BRA2]/<human>-<seed>/`. The seeds were chosen so that
  every possible computer rider (9 base riders, 7 Tricky skins) is seen in every slot, with cheat skins on 10 bases.

`build` sorts every leaf of a computer-rider record by the first key that fixes its value in every observation:

| Part | Key | Snow Jam leaves | Contents |
|---|---|---|---|
| slot | grid slot | 27 | AI path, route words, NPC score/pacing words, secondary-motion slot |
| skin | rider (base rider or cheat) | 13 | body scale, presentation lift, masks 8C0/8C8, bone mask, contact legs, hair |
| base | gameplay character | 13 | stance words, default mirror/root, uber table, ids, variant flags |
| grid | slot x body scale | 9 | position (~90 cm x scale along the grid line), ground frame, contact normal, lateral route distances |
| state | snapshot | 5 | provenance words, the landing tick |
| moment | snapshot x slot | 15 | start controller, 0xDF0 steering amount, previous ground normal, pose pivot, addresses |

- **Metro-City** has a flat grid. Its ground frame is constant, and the route words follow the spot: grid 22, slot 16,
  moment 14.
- **Cheat computer riders.** Their base leaves always equal the human's base rider's. No record depends on the human
  in any other way.
- **Document level.**
  - `relationships.scores` (0x155B50): the fresh profile records of `character-selection.p2s` (0x4A6CA8 +
    bank*0x9B50 + char*0xF88 + other*3 + 0xBC1: kind, level, score). Before the countdown, every participant's
    records age at the load (0x155E58, see the second pass below). This holds for the whole table in every state.
  - `characters`, the rival, `pair_inputs` by base character, and the pair records' +0x1C flags (relationship >= 2).
  - Pair distance and bearing are recomputed by the 0x10F560 refresh at the first tick (game tick 18).
- **Output.** `web/public/assets/{ARA1,BRA2}/lineups.json` holds these parts, the anchor as the template (key order,
  constants, anchor moment), the fresh relationship records and the observed lineups.
- **Build checks.** Assembling each of the 70 observed lineups with that state's own moment words gives its document
  exactly. 0x23A4F0 gives each observed lineup, and the relationship rule holds.
- **Browser moment.** The browser uses the anchor's moment words per slot. The start controller is diagnostic only
  (`npc_start_event` enters it fresh), and 0xDF0 is rewritten by the provider's zones.

### Checks

- `web/test-lineups.mjs` (npm test, about 80 s) checks:
  - the algorithm against all 70 states and the reference draw 130;
  - the 30 characters' reference lineups, including Psymon -> Nate, Allegra, Moby, Zoe, Luther on Psymon;
  - Sam as character 3 and the career heats;
  - the JS assembly: all 70 documents exact against the local per-state files, and pair records exact through the core
    refresh;
  - Zoe's anchor lineup against `npc-riders.json`, byte for byte, on both courses;
  - 28 distinct Snow Jam lineups (every human's, plus coverage) and 21 Metro-City lineups racing 600 ticks;
  - a core set up again against a fresh core, identical on every tick for 600 ticks.
- The capture gates (event-race, boardpress-rail, metro-event-race, event-race-ai, ai-idle, event-race-ai-pairs),
  test-ai-racers and test-characters are unchanged and exact.
- **Browser.** Screenshots are in `scratchpad/chars/lineup/shots/`:
  - Psymon with `?lineupSeed=0xb57109a9`: Nate, Allegra, Moby, Zoe, Luther (on Psymon). The grid matches the ARMSX2
    countdown frame.
  - Brodi on Psymon: the same lineup.
  - Stretch on Zoe with a clock seed: Viggo, Moby, Elise, Seeiah, Psymon.
  - Zoe with `?presentationSeed=0x182200`: the anchor lineup, no core reconfigured.

### The countdown moment, relationships and consecutive events (2026-09-23, second pass)

**Moment words.** The anchor record's moment leaves are the start controller, the 0xDF0 amount, the previous ground
normal, the pose pivot and addresses. They do not change the race:

- The anchor was raced with the moment words of other states (Bunny San at tick 18, Seeiah at tick 20) on the
  event-race-ai-pairs and ai-idle captures. Every rider, the RNG, the ranks and the pair records stay exact to the end.
- The start controller is entered fresh (12BE20). 0xDF0 is redrawn by the start command before it is used. The previous
  normal is overwritten before it is read.

**The game RNG at the anchor is what matters.**

- 0x4FF030 is seeded 0 at the event load. The load then draws N times, where N depends only on the course and the human:
  - Snow Jam: 10; Metro-City: 13.
  - Zoe draws one less, Moby one more.
  - A cheat skin adds 4.
  - Sam (not on the PS2) is taken as Mac's slot: the base value.
- At the first countdown pass each computer rider's start command draws 2: the 0xDF0 amount and its sign. These are draws
  N + 2k and N + 2k + 1.
- The rule was fitted on the 30 character states. It holds for all 80 countdown states (the coverage states, Brodi on
  Psymon at Metro-City, nine Metro-City cheat humans) and the capture baselines (Snow Jam 19, Metro-City 22).
- The browser sets these words at the anchor tick for every lineup: `lineup.js anchorRandomWords`,
  `ai-racers.js setAnchorRng`. Before, it raced with initial.json's glide RNG.

**Held-out lineups against new PS2 captures.** `tools/ps2_capture.py --ai-state` ran 900 ticks from derived countdowns;
`SSX3_CAPTURE_DERIVED=1` allows their heap addresses. Human, RNG, ranks, pair records and all five computer riders are
bit-exact for all 900 ticks in:

- Psymon, Moby, and Stretch on Zoe, each with its own reference lineup;
- `lineups/psymon-78e51061`: Brodi and Seeiah as computer riders on Psymon;
- `lineups/kaori-684c4645`: JP and Marty on Kaori;
- Zoe without isolation.

On Metro-City (`viggo-1818e811`, `allegra-4b8545e9`, Brodi on Psymon) the computer riders, RNG, ranks and pairs are
exact for 900 ticks. The idle human's velocity leaves at about tick 751. That is a course gap: a Zoe anchor idle capture
on Metro-City leaves earlier, at computer-rider tick 506.

Fixes found on the way:

- **Human grid spot on Metro-City.** A non-Zoe human raced from its Snow Jam grid spot there, because `settings.json`
  `original_event_start` is Snow Jam's. `lineups.json human_grid` holds the human's countdown ground state per body
  scale, with `reverse_stance` per base rider. It comes from 30 Metro-City states covering every human scale: the ten
  base riders and ten cheat scales. `ai-race.js prepare()` applies it through `human_grid_seed` on courses other than
  Snow Jam.
- **Pair-record distance/bearing.** Now computed from the grid positions at `world.reset` (the 10F560 arithmetic), so
  disabled (isolated) records hold the original values too.
- **Restarts.** A restart (start() again) kept the previous run's NPC provider state. start() now runs `npc_configure`
  again, and a restart races exactly like the first start.

**Relationships.** The records live at 0x4A6CA8 + bank*0x9B50 + char*0xF88 + other*3 + 0xBC1: kind, level, score.
Bank 0 is the human's profile, bank 2 the computer riders'.

- **0x155E58** (called by the event setup 0x233F20 / 0x2348AC) ages every participant entry's ten records at each load
  and at each restart:
  - score = max(score - 3, 0), level = score / 5;
  - kind 3 with level 0 becomes kind 1, level 3, score 15 (the fresh nemesis 03 02 00 -> 01 03 0F -> 01 02 0C -> 01 01 09);
  - kind 1 with level >= 4 becomes 3/2/10; kind 0 with level >= 3 becomes 0/2/10;
  - kind 2: level <= 0 -> level 1; level >= 4 -> 3/15;
  - kind 3 with level >= 5 becomes 4/20.
  - A character raced twice in a bank ages twice. This replaces the first pass's observed "nemesis step".
- **0x155BF0**, in race: 107E70 -> 10E228 / 10E2E8 / 10E3A8 / 10E468 adds 1, 2, 4 or 6 (soft bump, crash, soft attack,
  crash attack) to the other rider's record about the one knocked, with the same kind rules (kind 2/3 with level <= 0 ->
  1/5). A level change feeds 10F560 (+0x1C at >= 2, designated peer at 4) and the computer riders' 10DBF0 attacks
  (>= 3).
  - The browser applies it in `ai-race.js` `racers.onReact`. The core now passes the other rider to `js_pair_react`, and
    new exports `race_world_set_relationships` / `npc_set_relationships` push the new levels.
  - `document.relationships.kinds` (0x155AB0) is kept live for the audio agent.
- **Storage.** Bank 0 is kept in localStorage (`ssx3.relationships.v1`, like the profile). Banks 1 and 2 are kept in
  sessionStorage (RAM from power-on in the original).
- **Checks.**
  - A 918-tick neutral race (5 pair events) ends with all three banks equal to the PS2 state `two-event/zoe-quit-900/race1`.
  - The ageing gives `ready2` exactly after a quit and a new event, and `restart-b` after a restart.
  - Records changed in race 1 by riders absent from event 2 are kept: mob>zoe 01 00 02.

**Consecutive events (presentation RNG 0x4FF018).** Derived sessions in `characters/two-event/`: race 300 or 900 frames,
then pause > Quit > Yes > title > Start > Single Event > the same path; or pause > Restart.

- The pause menu, quit dialog, title, main menu and all select/setup screens make **no** draws (select2 = race1).
- Each load makes 128 draws, then the roster seed.
  - Second seeds: draw 3000 = 2871 + 129 and draw 6489 = 6360 + 129.
  - Second lineups: Luther and Eddie on Zoe, Kaori, Allegra, Viggo; Marty on Zoe, Griff, Allegra, Psymon, Elise.
    Both follow from 0x23A4F0.
- A restart keeps the roster: no rebuild, same seed.
- After the seed, the load, overlay, countdown and race draw every frame. The count depends on what is on screen and for
  how long:
  - seed -> countdown tick 18: 1051..1261 over the reference states;
  - countdown -> glide: 5.22..5.57 per tick.
  - The browser does not run all these consumers (snow, sprays, crowd, flags, UI and audio of six riders), so this part
    is estimated.
- `lineup.js noteEventDraws`: seed + 129, plus the measured mean (1139 + 5.33 per race tick) for each earlier event.
  - It hits quit-300 within 3 draws (predicted 2997 vs 3000).
  - It misses quit-900 by 294 draws.
- Bank 0/2 and the presentation state persist per page (relationships also in storage). Since 2026-09-25 a course switch
  stays in the page (docs/course-switch.md), so the presentation generator carries on across events like the PS2 session
  (before, a course switch reloaded the page and restarted it from the clock); `lineup.js settleRaced` notes the raced
  event's draws when the course is released.

**Checks added to web/test-lineups.mjs:**

- the anchor RNG rule on all 80 states;
- the two-event and restart sessions: menus draw nothing, second seeds, second lineups, relationship ageing;
- in-race 0x155BF0 against the PS2 after 318 and 918 ticks;
- the nine held-out captures through compare-ai-capture.mjs (new `--human ID --base ID --document PATH`; applies the
  human grid on other courses);
- restart = first start.

**Who draws from the presentation generator (0x4FF018 via 0x3177F0).** Measured with a caller histogram hook
(`characters/scripts/presentation_hook.py`, a derived copy of `zoe/select.p2s`; counts in `characters/presentation-draws/`).
The hook totals equal the generator's own draw count at every save. Counts are cumulative from Select Character:

| Caller (function) | What | load 1200 f | ready | countdown t19 | race t318 | race t919 |
|---|---|---|---|---|---|---|
| 0x229340 (0x2292E0) | 128 random timers armed at the load | 128 | 128 | 128 | 128 | 128 |
| 0x22EFF0 (0x22EBC8) | the roster seed | 1 | 1 | 1 | 1 | 1 |
| 0x22961C/638/6D0 (0x229530) | per timer on expiry: 2 draws, then re-arm 300 + r % 300 | 25x3 | 36x3 | 36x3 | 93x3 | 209x3 |
| 0x2F3BE8 (0x2F39E0) | per frame, counter-gated | 312 | 420 | 439 | 738 | 1339 |
| 0x3711D0 (0x3710D0) | particle emitter update, one per active emitter per frame (snow, sprays and set pieces of all six riders) | 213 | 268 | 271 | 1019 | 2746 |
| 0x390CA0 (0x390C60) | one per race frame, a second draw when the first falls under a chance | 1 | 1 | 18 | 317 | 918 |
| 0x36CCE4..DCC, 0x370E1C (0x36CCB8, 0x370DC8) | set-piece emitter seeds when sections activate (9 + 1 per emitter) | 240 | 240 | 240 | 240 | 294 |
| 0x165708..8C0, 0x165A54..BAC (0x1656B0, 0x165938) | camera effects (engine/original_camera.hpp) | 0 | 0 | 0 | 0 | 128 |
| 0x34B288, 0x34C71C (0x34B228, 0x34C668) | flag cloth (flag-animation.js) | 21 | 22 | 23 | 28 | 38 |
| 0x2ADF70 (0x2ADF60) | audio random slot (rand15) | 3 | 3 | 3 | 3 | 11 |
| 0x2E4E14/1C/24 (0x2E4D88), 0x27B214, 0x1EA800 | load-time audio/HUD set-up | 21 | 21 | 21 | 21 | 21 |

The next roster seed depends on the draw *values*, not only on counts:

- 0x229530 re-arms its 128 timers with `300 + r % 300`.
- 0x390C60 draws a second time only when the first draw is under a chance.
- 0x3710D0 runs for every emitter of all six riders and the set pieces.
- The load and the overlay run a PS2-timed number of frames (disc streaming, the player's wait).

So the count cannot be ported without the values. Landing the next seed exactly would need one shared generator that
every consumer above advances, in the original order, every frame from the load on. That means:

- the particle emitters of the five computer-rider cores, which run no snow system;
- the 128 timer objects;
- the per-frame consumers 0x2F39E0 and 0x390C60;
- the camera, flags and audio calls;
- the PS2's frame counts for the load and the overlay.

The browser ports the snow, camera and flags, but each with its own generator copy. It is not done; the estimate stays.

**Relationship notices (0x1E2A08).** They are not a text notice. 0x1E2A08(character, score) runs from 0x155BF0 (only
when the human's own record rises a level) and from 0x155E58 (every aged record). It posts a **career message** to the
profile inbox (0x147908: up to 25 entries of item index + variant, with read bits) and signals the in-race `hud` object
(hash of "hud" under G+0x48, virtual +0xC4 with 8). The pause-menu Messages screen uses the same folder, btext, arr_up
and From%d strings (0x4A20D0).

- **Gates:**
  - 0x5305F9 == 0, which holds only in Conquer the Mountain (1 in every Single Event state);
  - not the human's own character;
  - no message of that character already posted (0x1E3A30);
  - score > 15 + (r & 7), a presentation draw.
- **The message:** the character's category (0x1E2B38: character -> 12 + character) in table 0x441630 (runtime: first
  item, count). The item is first + r % count, another presentation draw.
- A score below 5 clears the posted flag (0x1E3A78).
- Single Events never show them. The browser implements them (next section).

### Relationship messages (2026-09-23)

In Conquer the Mountain a rival's grudge reaches the rider's inbox, as on the PS2: the mail icon blinks in the race
HUD and the message waits in the pause menu's Messages screen (also MCOMM > Messages).

**Original (SLUS_207.72).**
- **Inbox** at profile 0x4A6CA8 + bank*0x9B50 + char*0xF88 + 0xE38:
  - 25 x {item, variant};
  - +0xC8 read bits; a new message clears its bit;
  - +0xCC count;
  - +0xD0 one posted bit per category (0x147980 test, 0x147A30 set). Bits 40..51 are pending flags (empty kind-7
    categories); bits 7..11 are the open FAQ folders (0x1E4338 toggles them), so a folder stays open in the save.
- **Posting:**
  - 0x1E2A08 applies the gates above.
  - 0x1E2FE0 adds the entry; a full inbox drops its oldest first (0x1E31B8, entries and read bits shift up).
  - 0x1E3100 picks the variant of a flag-1 record: kinds 3/4 r % 10, 5 r % 11, 8 r & 3. 0x1E4F80 formats the subject:
    kind 3 kT_MSGSubjectBCFinishWin/Loss<v> (Win for the odd items from 0x74, jump table 0x46E1E0), 4 Foreshadow<v>,
    5 Aggression<v>, 8 BeatThePeak<v>; other kinds use the record's subject.
  - 0x1E2EA0 gives the From name: senders 0..9 the riders, 10 kT_CMNGameTitle ("SSX 3", the FAQs), 11
    kT_CMNDJAtomica ("Atomika", the mountain notices), 12 kT_FAQFolder.
  - HUD event 8 (0x1EC3C4 -> hud +0x160).
- **The icon:** 0x1EB6E4 advances a 1 s phase and a 5 s timer. 0x1F0F3C draws OV.LUI `mail_icon` (OV_1-4) at
  (15, 384) of 640x480. It is white (0x4C8788) while the phase is <= 0.5, else orange (0x4C87A8 words 1..3 =
  0.861, 0.381, 0).
- **Tables:**
  - categories 0x441630 (61 x 20 bytes: type, first, count, kind, folder);
  - message records 0x4C6C08 (256 x 0x18: item, category, flag, sender, subject/body key hashes);
  - screens OV.LUI `112messagecenter` / `113ViewMessage`.

**Browser.**
- `tools/export_messages.py` -> `web/public/assets/CAREER/messages.json`. It reads the runtime DB from
  `local/ps2-capture/menus/ctm/state-mcomm.p2s`, plus the texts, the names, the icon sprite and the two LUI layouts.
- `web/career-messages.js`:
  - `MessageInbox` holds the rules above.
  - `CareerMessages` draws the list, the view and the HUD icon.
  - The inbox is the career rider record's `messages` and is saved with the career save (`ssx3.career.v1`).
    Saves without it load with an empty inbox.
  - The draws are the session presentation generator (`web/lineup.js presentationDraw`, the estimate above).
- **Hooks:**
  - `web/ai-race.js` calls `onRelationshipNotice(character, score)`: from the ageing at every start (every record, in
    participant order) and from 0x155BF0 when the human's record rises a level.
  - `web/main.js` routes it to `careerUI.messages.notify`. It posts only while a career event is active
    (0x5305F9 == 0).
  - `web/ui.js` draws the icon after the race place.
  - `web/career-ui.js` delegates `ctm-messages` / `ctm-message` and enables pause "Messages" and MCOMM "Messages".
- **List rows:**
  - the inbox, newest first, numbered, with an envelope while unread;
  - the five FAQ folders (orange '?' folder, bullet);
  - an open folder ('-' folder, '-') lists its FAQs ('?' icon, '>', sender "SSX 3"). Open folders are the posted
    bits 7..11, saved with the inbox.
- **Message view:** drawn from the `113ViewMessage` LUI shapes. The grey background is a gradient at alpha 175/255.
  The notched "3D Ov" frame is its shadow, big and small shapes (alpha 200/255, by layer) and dark outlines; then the
  white separator. PS2 pixel samples of the frame match this blend (the shadow shows 0.78 over the background).
  Header labels are right-aligned at x 175 with the values at 179; the body text starts at (61, 203), 522 wide.
- **Buttons:** Cross View / Expand / Collapse, Triangle Previous, Square (Shift) Delete. The message view has Previous,
  Delete message and Keep message.
- **Verification:**
  - PS2 frames from a derived Message Center state with Zoe's inbox poked to (0x6D, 3), (0x58, 0):
    `local/ps2-capture/menus/lineup-messages*.png`.
  - PS2 HUD icon frames from a derived race state with hud +0x160 poked (scratchpad `chars/lineup/mail/out`).
  - Browser screenshots of the same inbox: `chars/lineup/shots/msg-*.png`. They match row for row: "Kaori / Where's
    the love?", "Psymon / Hate Mail", the folder names, the expanded Progression/Rewards FAQs, Kaori's body "Guess
    what?  Nice just went nasty.", and the white and orange icon.
  - `web/test-messages.mjs` (npm test) checks:
    - the tables;
    - add, drop-oldest and remove with read bits;
    - the 0x1E2A08 gates and draw order;
    - Conquer-the-Mountain-only posting;
    - the save round trip and old saves;
    - the icon blink;
    - the ageing notice order.

### Career messages (2026-09-23)

Every other message the original posts to the inbox is implemented as well. Each one is found through its caller of
0x1E2FE0 (33 call sites in 12 functions), and each fires from the browser career state where its condition lives.
All of them need Conquer the Mountain (0x5305F9 == 0). A tutorial counter (gp-0x848 +0x84 +0x28) also gates them
while it is 1..9. It is 0 or 10 in every career state, so the browser ignores it.

**Event completion.** 0x154EE8 runs once per career event. It makes these calls in this order:
1. **Backcountry result** (0x1E25D8 -> 0x1E2648 rival race / 0x1E2828 rival jam, course 14 + peak). This runs unless
   the result bit 22+peak / 25+peak is set:
   - it removes the rival reminders (category 1+peak / 4+peak) and earlier results (0x1E32C8);
   - it posts a loss (0x73 / 0x7F + 4*peak, +2 for the rival's own rider);
   - or, on a win, it sets the bit and posts the next item.
   - The subject is BCFinishWin/Loss.
2. Without a medal, it stops.
3. **Peak-challenge notices** (0x1E1EB8). Pending flags 40..51 post the peak event's "your time has been beaten"
   message (category 34 + jam + 2*peak): the first text after bronze, the second after silver, which also stops
   further ones.
4. **Rival reminders** (0x1E1550; not in rival events). The first unposted of categories 1..3 (rival race) and of 4..6
   (rival jam) posts once. It needs the rival event to be open (+0x278 bit 6+peak / 9+peak clear, as it stood before
   this event) and to have no medal (0x145EF0).
   - The text is first + r % 3; the rival's own rider gets the second three (Griff's texts for Mac, and so on).
5. **Awards** (0x159CD0 inside 0x1591E8). These notices come from Atomika:
   - award 0: all golds, item 251;
   - award 1: the whole mountain, 255;
   - awards 2..4: all goals of peak 1..3, 252..254. Award 3 tests bit 58 but sets 57, so it can repeat, as on the PS2.
   - awards 11/12: the free-ride goals (0x1E38B8). Items 249/250 need the next peak still locked.
6. **Peak-challenge flags** (0x1E1DD0). Silver or bronze in a peak event (modes 6..11) sets flag 40 + 4*peak + 2*jam
   (+1 for silver), unless the notice is already posted.
7. **Goal-list walk** (0x1591E8, lists 0x45AAD8):
   - While a standard event before the rival has no medal, a rival taunt posts (0x1E1C10). The taunt is a random
     category 28/29 + 2*peak item that is not from the human and not already in the inbox; its subject is
     Foreshadow.
   - When the rival opens, taunts stop (0x1E2370 sets bits 28..33).

**Collectible cash.** 0x119EF8 -> 0x1E3760 runs before the cash is added. If earned + amount reaches the peak 1
(peak 2) earnings goal while peak 2 (peak 3) is locked, it posts 247 (248).

**Browser.**
- `MessageRules` (`web/career-messages.js`) holds each poster as a pure function.
- `CareerMessages.hook` wraps the career instance's `completeEvent`, `grantAward` and `markCollected`, so
  `web/career.js` is unchanged.
  - The wrap snapshots the rival locks and peak passes before the result, and posts in the 0x154EE8 order.
  - Award notices queue during `completeEvent` and post at step 5.
- Posts from event completion do not start the HUD icon, because the race HUD is gone at the results. Relationship
  notices in a race still start it.
- `tools/export_messages.py` now exports all 61 categories and 256 records, the 13 sender names, the kind 3/4/8
  subject lists and the kind-3 win items.

**PS2 evidence.**
- A derived Message Center state holds one message of each kind: (0, 0), (235, 0), (0x73, 3), (0x74, 2), (141, 4),
  (252, 1). It is `scratchpad chars/lineup/mail/mcomm-kinds.p2s`, with frames in
  `local/ps2-capture/menus/lineup-message-kinds.f*.png`.
- The PS2 shows, row for row:
  - "Atomika / Oh yeah!"
  - "Viggo / The Big Boss"
  - "Mac / Way to go!"
  - "Mac / Practice, practice, practice"
  - "Atomika / Peak Challenge"
  - "Mac / Backcountry Challenge"
- It also shows Jurgen's award text in the view. The browser (`shots/msg-kinds*.png`) matches, including the line
  breaks.

**Tests.** `web/test-messages.mjs` checks:
- each poster, including the 0x1E32C8 early stop and the award-3 repeat;
- the PS2 kinds frame;
- a career run through the hooks. Snow Jam gold posts a taunt. Metro-City gold opens the rival, with no post. The
  next event posts Mac's rival reminder. A rival-race loss posts the result and removes the reminder. Nothing posts
  without a medal or in a Single Event.

### Peak 2 lineups (2026-09-25)

Ruthless Ridge (CRA3), Intimidator (DRA4) and Style Mile (DSS2) have `lineups.json` too (peak index 1: the rival is Nate,
or Zoe when the human is Nate, so Nate never races as a computer rider).

- **States.** `characters/scripts/make_peak2_lineup_states.py` copies `characters/<human>/select.p2s` (cheat humans:
  `<cheat>/setup.p2s`, on Zoe; `brodi-on-psymon` on Psymon), pokes the Peak 2 pass (profile block +0x278 bit 12 cleared,
  banks 0..2, characters 0..9) and the 0x4FF018 words (draw 130 = the chosen roster seed), then runs Setup, Select Peak
  (D-pad up), Race / Freestyle, the event, My Rules, the load and the overlay. It keeps the countdown save at game tick 18.
- **Coverage.** 20 states per event, in `characters/lineups-{CRA3,DRA4,DSS2}/`: the ten base riders (Nate included, the
  peak-rival case) and ten cheat humans (every human body scale). On the races, the seeds were chosen so that every
  possible computer rider (9 base riders and 7 Tricky skins) is seen in every slot, with cheat skins on all ten bases. On
  Style Mile, every possible opponent is seen (all base riders but Nate). CRA3 also has `zoe-b57109a9`: the reference seed
  reached from Select Character.
- **Differences from Peak 1** (handled in `tools/export_lineups.py build`, additively):
  - The computer riders' pair collision / attack stats come from attribute bank 2 (raw 20), the human's from bank 0.
    `lineups.json human_pair_inputs` keys slot 0 by character. `web/lineup.js assembleLineup` uses it when present.
  - The reference anchors (reached through `peak-1-selection` and a re-entered Setup Character) make one load draw more
    than the same Zoe lineup reached from Select Character (`zoe-b57109a9`: 20 draws, anchor 21). The rule is fitted
    without the anchor (Zoe -1, Moby +1, cheat +4, as on Peak 1). The anchor's offset is kept as
    `load_draws.reference_anchor_extra`, which the browser does not use.
- **Checks.** The build reproduces every state's document. `web/test-lineups.mjs` now runs its race-course checks on
  CRA3 and DRA4 (0x23A4F0, documents, pair records, anchor RNG), races the anchor, Nate and two cheat humans on each, and
  checks Style Mile's 0x239938 opponents and documents.
- **Career finals (2026-09-30, career-rival agent).** A career race final rides the rival in slot 1, so CRA3 / DRA4 need Nate's records,
  which no Single Event state has. `tools/export_lineups.py export-career` / `build` take them from the derived career finals
  (`characters/career/{CRA3,DRA4}-final-zoe`): skin `nate`, `skin_scale.nate` 3f7fffff, `grid[1][3f7fffff]`, `career_skins.nate`
  (additive). The page uses them under pv careerRival (web/lineup.js careerSkinGated). docs/career-events.md "The peak rival in career events".

### Limits

- The per-frame presentation draws between events are an estimate (above). Only the menu and load structure is exact.
- The source of the load's N draws (the human-dependent part) is known only by its rule, not by call site.
- Metro-City idle races leave the PS2 at ~500-750 ticks with any lineup (course-level; Zoe's anchor too).
- Career messages whose conditions need the free-ride world have their rules but never fire here: the free-ride goal
  awards (items 249/250) and the all-goals awards (items 251..255) need free-ride goal medals, which the port does not
  have. Earnings notices fire from collectible cash (`markCollected`) only, as in the original.
- 0x1E3C00 opens the Progression/Rewards folder for as long as the Message Center is shown, when it is entered with
  gp-0x1024 != -1. Which path sets that is not known: the MCOMM path in the captures leaves every folder closed. It is
  not implemented.
- Event-completion posts do not blink the HUD icon (above).
- The message draws use the estimated presentation generator, so which of the three texts and eleven subjects a
  message gets is not bit-exact across a session. The rules are.
- `lineups.json` exists for Snow Jam, Metro-City, R&B and the Peak 2 events Ruthless Ridge, Intimidator and Style Mile
  (below); other courses race their anchor's riders.

## Equip Gear and outfits

Every rider can be dressed in Setup Character > Equip Gear and in the career lodge, with the items the original
allows. The ten riders use the original wardrobe; Sam uses the Sam PS2 build's (below); the twenty cheat skins
keep their fixed outfit. The rider wears the outfit in the menus (the FE preview model with its cutscene head and
hands) and in the race: its parts, textures, skeleton, bind matrices, bone masks and secondary motion are the ones
the original builds for that outfit.

### The original rules (SLUS_207.72)

- **Item database** `DATA/CHAR/BOLTPS2.DAT` (runtime DB 0x4A6750), 30 character buckets (0..9 riders, 10..29
  cheat skins).
  - A 56-byte entry holds:
    - +0 character, +2 tier, +3 texture group (-1 = no texture), +4 item id.
    - +6 class: the model entry an item belongs to. For example, the top colour Junker belongs to the Midriff Top
      model, which belongs to the Tops CLASS.
    - +8 menu parent, +0xA menu order, +0xC item-limit weight, +0xE price/10.
    - +0x10 low byte: the part slot (geometry file id; 0xFF = no model).
    - +0x14 name. +0x18..+0x24: LOD models H/M/L/Shdw, or one `_NIS` model. +0x28 model path.
    - +0x2C texture, with `$` wildcards. +0x30 icon.
    - +0x34 flags: 0x1 = equipping unequips the class siblings; 0x4 = listed leaf; 0x8 = multi; 0x10 = fills a
      required slot; 0x20 = folder; 0x100..0x800 = reward pools; 0x1000 = re-apply rules.
  - Table 1: 7327 equip rules of 12 bytes: on/off, item, condition item and state, target, desired.
  - Table 2 (getters 0x14DC00/0x14DC10): the NIS model -> race model pairs, e.g. Zoe's HeadA_NIS 84 -> HeadA 86
    and HandsB_NIS 128 -> HandsB 130.
  - Table 3 (0x14DC40/0x14DC50): default outfit rows (class A, item B).
- **Inventory** R = 0x4A6CA8 + profile x 0x9B50 + char x 0xF88. +0x288 is the item -> row lookup, +0x28C the
  count, +0x290 the rows {s16 item, u16 flags}.
  - Row flags: 0x2 owned, 0x10 equipped, 0x4 committed, 0x20 default.
  - 0x151A88 builds the rows; 0x1513B8 applies the default outfit.
  - Single event and career use the same profile-0 record.
- **Equip:**
  - 0x151C90 (with rules 0x151EF0, selected by 0x14DB40): the lodge port `GearInventory` in web/lodge.js.
  - The UI calls it through **0x14AFB0**:
    1. Back up the rows, then call 0x151C90.
    2. Force the item's own flag.
    3. 0x1521F0: for every table-3 row, if no class descendant of A (walker 0x14D608, which follows +6, not the
       menu tree) is equipped with entry flag 0x10, equip B.
    4. 0x1520E8: equipped flag-0x1000 entries re-apply their rules.
    5. If a step fails, the rows are restored.
  - Equip Gear Cross 0x19BEE8 toggles on the committed bit (`on = !(flags & 4)`), then commits (0x14AEA8, 0x4 = 0x10).
  - 0x14AF10 (front-end transition 0x1A1F1C) resets 0x10 from 0x4.
  - Conflicts resolve through the rules, never through a dialog. Examples: a beanie swaps Zoe's Mop for SplitHair2;
    Tiara removes the Peacekeeper and brings back Banger; Bedhead removes Mac's hat.
- **Item limit:**
  - 0x14B478 sums the +0xC weights of the equipped entries.
  - 0x199D30 refuses an equip when `*(gp-0x1818)` (0x4A18D8 = 3392) < weight + change. 0x19B8F0 precomputes the
    change by simulating 0x14AFB0.
  - Bars 1/2 ("1 Your current outfit / 2 Your possible outfit") fill (w - 1500) / (3392 - 1500) (0x19BC90,
    0x19BD48).
- **Lists** 0x19B180 in equip mode:
  - Named owned leaves, plus folders that hold one (0x19AFD0); children come from 0x14D7E8, sorted by +0xA.
  - Only owned items appear, so a fresh profile's lists are short: Zoe has 2 hairstyles, 1 eyewear, 3 tops,
    3 hands, 2 bottoms, 1 boots and 1 board.
- **Race assembly:**
  - 0x14D068 (0x14BD98 from 0x22ED5C) sets equipped = committed, equips the race model of every committed NIS
    entry of table 2, and equips item 4 (the PDA).
  - 0x11BBE8 then collects the equipped entries of the base rider's record. A cheat skin (setup slot +0x12)
    collects **all entries of its bucket** instead, which is its fixed outfit.
  - 0x11C138 adds each entry's four LOD models by name at its part slot (0x30D8B8). The first model of a file id
    makes the part.
  - Rider init 0x11C61C hides slots 5, 6, 8, 9 and 11 (NIS head, eyes, NIS hands, NIS right hand, PDA).
  - The FE preview assembly is the equipped set as it is: the NIS head, eyes and hands, no race models, no PDA.
- **Textures** 0x11BE88 / **0x14B988**: a `$` in an entry's texture takes the characters at the same positions
  from another equipped entry's texture of the same group:
  - top `zoe_Suit_C01_$$$` + bottom `zoe_Suit_$$$_D01` -> `zoe_suit_c01_d01`
  - boots `zoe_Boot_A01_$$$` + hands `zoe_Boot_$$$_B04` -> `zoe_boot_a01_b04`
  - Cheat skins use the raw names.
  - A model material binds the loaded texture whose SSH entry name is the material name (`suit`, `boot`, `head`,
    `bord`, `alph`, `eat*`, ...).
- **Skeleton** (compile 0x30DBD0, recovered bit-exactly):
  - Parts are sorted by file id (0x418EF8). A part's base slot is the running bone count over all parts, hidden
    ones included (Eyes_NIS holds 2 slots).
  - Each part with morphs gets the next morph index.
  - Inverse binds: 0x30E560 local, 0x30E5F4 world = parent x local, 0x30E748 rigid inverse. Every VU0 multiply and
    add rounds toward zero with DaZ/FtZ. For example, Zoe's hips quaternion 2 x 7.1e-20 x 7.1e-20 flushes to 0.
  - Channel-1 masks 0x11C298 via 0x310CE8:
    - +0x8C0 = the 0x457A90 bones | the file-7 hands "morph" bit at slot_count + morph index.
    - +0x8C8 = 0x457B38 | the same bit.
    - +0x8D0 = 0x457BC8 = 0x870.
  - `bone_mask` = all browser bones.
  - Pivot, secondary bones [5,2,0], contact legs and board bones are file 0/1 names, so they never change with
    the outfit.
- **Secondary motion enables** (0x11CF70..0x11D1B8, 0x30EBC0 = part active):
  - Channel 0 = any part in slots 0x0F..0x1D except 0x12 (hair and hats).
  - Channel 1 = 0x1E/0x1F/0x20/0x22 (hoodie, necklace, wings, backpack).
  - Channel 2 = 0x24/0x26/0x27/0x2A (T-shirt, tail, suspenders).
- **Equip Gear preview** (update 0x19BFE8, FE preview slot `*(*(gp-0x848)+0x7C)+0xB0`):
  - Camera 0x15E050: eye (-110,394,0) -> (-35,0,0) cm, 25 degrees.
  - State +0xAE0: 1 = rider view, 2 = board view, 3 = easing to the board view, 4 = easing back.
    - Entering the Boards folder (id `*(gp-0x1F10)` = 3, 0x199D14) sets state 3; leaving it (0x199E30) sets 4.
    - Each eased frame: p += (target - p) x 0.2 in VU arithmetic, until the squared board (3) or rider (4)
      distance < 0.2.
  - Rider: (-105, -100 + 320z, -80 - 65z), turned qZ((+0xAAC + 80) degrees). In the board view: (-250,-650,-215).
    - Right stick X: +3 degrees per frame of turn (0x199AAC). Stick Y: zoom z += -0.1 x stick, clamped to [0,1]
      (0x199B64).
  - Board (0x19F548 +0xC50/+0xC60): at (265.75,-1073.5,-232), or (-100,45,15) in the board view.
    - q = rotZ(2 x 2.138029) rotY(2 x 2.792527) rotX(spin + 90 degrees), spin +1 degree per frame (mod 360).
      This equals the live +0xC60 of two PS2 states.
  - "Loading..." shows while the preview isn't drawn (+0xCC8): list shown, no rider. Phase 3 of the state (vt+0x30 = 0x1993A0,
    the intro's 0x42 label + 2) switches the preview on (19E538(slot, 1)); it draws once the model is loaded (+0xCB4/+0xCB8).
    From the lodge the PS2 shows the rider 37 frames after Cross, "Loading..." until 38: the intro's phase 3, not a disc load
    (docs/visual-parity.md 42 "Equip Gear's load"). The earlier "about 250 frames" for Setup Character is not re-captured.
  - The rider updates as soon as an item is equipped; the cursor only changes the icon and bar 2.
  - Gear icons come from the character's TXP archive (0x14B700; none for ids >= 10).

### Assets (`tools/export_wardrobe.py`)

For each character id 0..29 the tool writes `web/public/assets/WARDROBE/<ID>/` (git-ignored, about 187 MB):
- `wardrobe.json`:
  - entries: item, class, parent, order, flags, price, tier, name, group, slot, model, texture pattern, icon, weight
  - rules, default rows, NIS->race pairs, hidden slots, item limit, secondary slot lists
  - parts: per model resource, the authored bones and material batches, offsets into `parts.bin`, bone/morph
    counts, and for the NIS parts their morph targets
  - textures: SSH name, size, resource, PS2 texel domain; icons
  - `fe`: the FE preview block of the default `RIDER_<ID>/fe` package (IRR record, camera, root, rim, clips)
- `parts.bin`: vertices, local indices, skin records, dense morph deltas (NIS head and hands).
- `textures/<stem>.png`: every texture any item pattern can resolve to, in the PS2 texel domain (RGB of the
  same-named TXP `.ssh`, `export_characters.ps2_texel_rgba`), like the `RIDER_*` packages.
- `icons/iNNN.png`.
- Since 2026-09-25 the PNGs are packed at the end of the run (tools/export_rider_textures.py) into the rider's
  archives, like the disc's `<X>TXP.BIG`: `textures.tex` (the default outfits; the `RIDER_<ID>` / `RIDER_<ID>/fe`
  packages point into it), `gear.tex` (every other item texture) and `icons.tex`; id = stem. `wardrobe.json` gains
  `texture_pack` / `gear_pack` / `icon_pack` and `textures[stem].pack` (docs/asset-formats.md "Rider texture archives").

It also writes `WARDROBE/SAM/` (below), `WARDROBE/equip-screen.json` (FE.LUI `12equ_char`, cFEStateCharEquip,
plus `preview`: the Equip Gear 3D constants read from the ELF and checked against a PS2 Equip Gear state) and the
ground-truth fixture described below. Geometry comes from the GameCube twins; bone records and morph counts are
identical to the PS2 MPFs.

**Default packages use the same texture rule.** `tools/export_characters.py rule_textures` (also behind
`choose_textures`, which `tools/export_fe_preview.py` imports) replaces the old part-based heuristic.
`retexture` (`--retexture`) applies it to every `RIDER_<ID>` package without touching geometry or `rider.json`.
Only Moby changed: HeadA and MobyEarrings `alph` now bind `moby_hats_a01_d01`, the only loaded `alph`, instead of
`moby_alph_a01`, which is not resident live. The batches now carry their `material`.

### Browser (`web/wardrobe.js`)

- **Rules:** `Wardrobe` wraps `GearInventory` and adds:
  - `set` (0x14AFB0 with 0x1521F0/0x1520E8)
  - `toggle` + `commit` (0x19BEE8/0x14AEA8) and `restore`
  - `raceEquipped` (0x14D068) and `feEquipped`
  - `weight` / `delta` (0x14B478 / 0x19B8F0)
  - `menu` (0x19B180)
  - Also exported: `resolveTexture` (0x14B988), `assembly` (0x11BBE8/0x11C138/0x11BE88), `bindMatrixWords` (VU0
    chop port), `geometryMasks`, `secondaryEnables`.
- **Outfit state:** one record per rider, like the original profile-0 record that single event and career share.
  The record is the browser career save (`career.rider(id).gearFlags`, `localStorage ssx3.career.v1`).
  - Old saves load. A first-version single-event outfit (`localStorage ssx3.outfit.v1`) moves into the record
    while the record still has the fresh outfit.
  - Rows equipped by the old lodge screen without the commit bit are committed (0x14AEA8).
  - Without the career data (no `CAREER` assets), single event falls back to `ssx3.outfit.v1`.
- **Packages:** `buildPackage` assembles `world.json`, `rider.json` (with source skin, slots and bind words),
  `vertices.bin`, `indices.bin` and `colors.bin` in the layout of `tools/export_characters.py` build_package.
  - With `fe: true` it builds the **front-end preview** package in the `tools/export_fe_preview.py` layout: the FE
    set, every part drawn; parts with vertex/index ranges, file, board flag and morphs, plus `morphs.bin`; the `fe`
    block.
  - `prepareOutfit` builds both packages for a worn outfit (race `o<hash>/`, FE `f<hash>/`), served by
    `wardrobeFile`. The default outfit keeps the verified `RIDER_<ID>` and `RIDER_<ID>/fe` packages, so Zoe's
    capture gates are untouched.
  - `outfitRider(ui, entry)` gives `selectRider` the root plus `outfit_settings` (`bone_mask`, secondary enables)
    and `outfit_identity` (the three masks). For a cheat skin it prepares the base rider's outfit for the preview
    and keeps the bucket.
  - `outfitPreviewEntry(entry)` gives `web/fe-preview.js` the FE root.
- **Hooks:**
  - `main.js`: `load()` serves `wardrobeFile(path)` first; `selectRider` awaits `outfitRider` and loads
    `asset(package, true, rider.root)`; `frontEndRider()` and the FE placement and visibility include the
    Equip Gear screen.
  - `character-roster.js` `loadCharacter` lays the outfit values over the character's settings, keeping every
    other key (event start, uber rows, ...). `settings_package` lets Sam's outfit packages use `RIDER_SAM`'s
    settings.
  - `character-select.js` `previewEntry` returns `outfitPreviewEntry(base)`. Select, Setup and Rider Details show
    the outfit's FE model: the NIS head, eyes and hands with the FE clips' morphs.
    - With a cheat skin it is the base rider in its outfit, as on the PS2
      (`brodi/gear/zoe-gear-dangerous-trouble-setup-after-equip.p2s`).
  - `fe-preview.js`: `fetchOk` serves `wardrobeFile` first, and `previewRoot` takes `entry.fe_root`. Board parts
    are built hidden (`userData.board`) so Equip Gear can show them.
  - `lodge-ui.js` opens the lodge's Equip Gear on this screen, with Square = Buy Gear.
- **Screen** `openEquipGear(ui, rider, {onExit, onBuy})`, called by `web/fe-screens.js` Setup Character:
  - It plays 12equ_char with the snow screen: the intro, row highlight states 35..60, and the arrow flashes on
    scrolling.
  - Categories -> folders -> items with checked/empty boxes, the item icon, bars 1/2 and the two help lines.
  - **3D:** the outfit's FE preview model with the FE idle (FE_GEAR_<X>_CYC) and its morphs, lit by the rider's IRR
    record with the Equip Gear view matrix. It is placed by the port of 0x19BFE8 above: states 1..4, the Boards
    view, stick turn and zoom (Classic J/L and I/K), and the spinning board.
  - **Loading:** the list and "Loading..." with no rider until phase 3 and the FE model is loaded (pv `equipLoading`: the
    outfit package is built behind it; the row highlight, the 'equip btm left' dashes and the help line wait for phase 3).
  - Each equip rebuilds the packages, the preview wears the outfit at once, and the Customize speech plays
    (`speakFrontEnd`, 0x199EA4 -> 1A0358). The race rider is reloaded with the outfit when the screen is left.
  - The screen joins OriginalUI's dispatch by wrapping its `items/choose/back/draw` (no ui.js edit).
  - A cheat skin's Equip Gear dresses and shows its base rider, as on the PS2; the race keeps the skin's bucket.

### Sam's Equip Gear

- **Lists and rules:** the Sam PS2 build's bucket 30 (`local/sam-ps2/roster/sam-assets/BOLTPS2-preserved.DAT`,
  `tools/sam_ps2/SamWardrobe.cs`). That is Mac's hierarchy, rules and defaults, with the
  `sam_character/gear-names.json` names; unconverted accessories are disabled.
  - A fresh profile lists: Hair Styles Unc Cut; Hats O'Reilly; Eyewear Eye Candy; Tops Midwest Unc, Uphill Club,
    Lodge Legend; Hands Squad; Bottoms Liftline Khakis, Tow Rope; Boots Curbmasters; Boards Midwest Mileage.
  - Icons: su01/su02 only (the build's `sam_icons.ssh`, 0x14B700 hook); the other items have none, as in that
    build.
  - `tools/export_wardrobe.py` writes `WARDROBE/SAM/wardrobe.json` (no parts). The record is the shared one
    (`career.rider('sam')`).
- **Model:** the browser Sam is one baked mesh per outfit, so an item set selects a whole package. The source is
  `config/characters/sam.json` outfits with `native_package`.
  - The winner is the outfit whose `items` (Sam-build item ids) are all committed, the most specific first, and
    only when its browser package exists (`/assets/<pkg>/world.json`, checked at run time). Otherwise it is
    `RIDER_SAM`.
  - The FE preview uses `/assets/<pkg>/fe/` when present.
  - Gameplay settings stay `RIDER_SAM`'s (`settings_package`). All four native Sam packages have the same 26-bone
    skeleton.
  - `items` come from the outfit's `equip_items` in `sam.json`, else these defaults:
    - `rope_tow_regular` <- [49 Midwest Unc]: the default of both.
    - `lodge_legend` <- [59 Lodge Legend]: the same name.
- **Built (2026-09-23, Sam agent):** each top × back kit is one whole-outfit package:
  - Tops 49 Midwest Unc, 50 Sunday Unc (Packers), 55 Uphill Club and 59 Lodge Legend.
  - Backpacks 180 Catch & Release (chest pack + landing net) and 181 Packed for the Creek (chest pack, fly box, rod
    tube, amber sunglasses).
  - That gives 12 packages: `RIDER_SAM`, `_PACKERS`, `_UPHILL`, `_LODGE`, each also with `_FISHING` and
    `_FISHING_TUBE`. They are built by `tools/sam_mesh.py --all` and `tools/build_sam_web.py --all`, every one with
    its `fe/` copy.
  - `config/characters/sam.json` outfits carry `native_package` + `equip_items`; the combinations are
    `<top>+<net|tube>` entries.
  - `sam_character/gear-names.json` renames 50/180/181. It also makes all four new items owned by a fresh profile
    (price 0) and takes the kits out of the award pools. Items 50, 59, 180 and 181 get icons su03..su06, rendered
    from the packages by `tools/sam_icons/render.mjs` and packed by `SamTextures.cs`.
  - `tools/export_wardrobe.py` now copies every `su*` icon the Sam rows use.
  - `web/test-sam-gear.mjs` covers the lists, the ownership, top and kit exclusivity, taking a kit off, the
    package choice for all 12 combinations, and the files and rig of every package.
  - Verified in the browser: Setup Character > Equip Gear, the lodge's Equip Gear (the same record), and races in
    the chosen outfit after a reload.
- **Still open:** `earn_your_turns` has no item or mesh, and the Sam build's per-part items (Unc Cut vs Unc Cap &
  Flow, other bottoms, boards) keep `RIDER_SAM`'s look. On the PS2 disc the new rows are not built yet (the v5
  disc has the older wardrobe). Sunday Unc, Lodge Legend and the two kits would list there, but the PS2 has no
  model variants for them.

### Online races (interface for web/net)

The multiplayer agent owns `web/net/*`; nothing there was changed. The interface:

- **Send:** `const key = await outfitKey(ui, rider)` (web/wardrobe.js). Put it in the lobby profile next to
  `rider`/`pkg`, e.g. `client.setProfile({..., outfit: key})` in `web/net/mp-game.js`, and refresh the profile
  when it changes.
  - `null` means the default outfit; cheat skins (fixed bucket) also give `null`.
  - Riders: `'w1:<committed item ids, ascending, comma-separated>'`, the flag-0x4 rows that are the only input of
    the race assembly (0x14D068). Zoe with Dangerous + Trouble:
    `w1:63,66,77,84,85,87,128,137,138,139,190,191,202,455,515,556,607,629,633`.
  - Sam: `'sam:<package>'`.
- **Resolve** (every other client): `const entry = await remoteOutfitRider(riderEntry, key)`. It returns the entry
  to load:
  - `package`, `root` (a generated `/assets/WARDROBE/<ID>/o<hash>/` or `/assets/<package>/`) and `outfit`
  - `outfit_settings` and `outfit_identity`: the remote core's `bone_mask`, secondary enables and channel-1 masks;
    merge them like `character-roster.js loadCharacter` does
  - `settings_package` (Sam)
  - `files`: the six package URLs `world.json`, `rider.json`, `vertices.bin`, `indices.bin`, `colors.bin`,
    `animation-samples.json`. The original riders' gameplay clip table is one shared file,
    `/assets/ANIMATIONS/animation-samples.json` (web/wardrobe.js riderSamplesUrl); Sam's packages keep their own.
  - Texture paths in `world.json` are relative to `root`.
  - An unknown, malformed or incomplete key (one where 0x1521F0 would have to fill a slot) gives the default
    package.
- **Load:** `opponent-riders.js` / `remote-riders.js` should take `entry.root ?? '/assets/' + entry.package + '/'`
  as the package base, and read files through `wardrobeFile(url, 'json' | 'buffer')` first; `main.js load()`
  already does this. The skin palette they stream is the package's own, so both ends must use the same resolved
  package: the root hash is the same on every client (checked).
- Until web/net sends the key, `outfitState` returns nothing while `ui.onlineMode` is set: online races keep the
  default outfit.

### Evidence and checks

- **PS2 ground truth** (tools/ps2_navigate.py), in `local/reference/pcsx2/characters/{zoe,mac,elise,brodi}/gear/`
  (scripts in `characters/scripts/gear/`). Equip Gear screens, and outfits taken into Snow Jam:
  - Zoe: Dangerous + Trouble + Bare Back + Zennish + Wicked Spex; Bandito + Stuff + Flaming Board; Peacekeeper then
    Tiara + Element (the last two from `zoe/gear/select-owned.p2s`, owned bits set).
  - Mac: Bedhead + Infiltrator + Supertweak.
  - Elise: Roughrider + Blue Zip Up + Shadow Like + Jacked In.
  - Brodi with Zoe's gear changed.
  - Also default dumps and a lodge Equip Gear run. `tools/export_wardrobe.py --ground-truth` gathers them into
    `characters/gear-ground-truth.json`; `characters/gear-fresh-lists.json` holds the fresh Equip Gear lists of
    the ten riders.
- **`web/test-wardrobe.mjs`** (npm test):
  - The fresh inventory equals the lodge port for the ten riders. The Equip Gear lists equal the PS2 lists.
    Rule cases pass.
  - The 30 default assemblies rebuild their `RIDER_<ID>` packages: parts, bones, skin, vertices, textures,
    **bit-exact bind words**, slots, masks, `bone_mask`, secondary enables. Every batch's texture equals the rule.
  - All 17 PS2 gear states (countdown + glide) assemble from their own rows exactly: parts, slot count, masks,
    secondary, bit-exact live bind bank rows, only resident textures.
  - The five Equip Gear sequences replayed from a fresh profile give exactly the live committed rows.
  - The FE assembly of the fresh outfit rebuilds the ten riders' live-derived `RIDER_<ID>/fe` packages: parts,
    bones, **bit-exact FE bind rows** and slots, morph targets byte for byte, per-batch textures.
  - Equip Gear preview: the board quaternion against the two PS2 states; the Boards ease (37 frames in, 34 out)
    ends on the board and rider views.
  - One shared record: a legacy single-event outfit moves in, single event and career see the same outfit, and old
    lodge rows are committed.
  - Sam: the Sam build's tops and names, the default package, and the outfit choice.
  - Online: `outfitKey` -> `remoteOutfitRider` gives the same root and `rider.json` as the local package; bad keys
    and cheat skins fall back to their default packages.
  - Six changed outfits initialise in the core with the right channel-1 mask and ride the Snow Jam start.
- **Browser:** Chrome, Playwright; screenshots in `scratchpad/chars/gear/shots`. Comparison sheets:
  `compare-equip2.png` (categories, tops, Boards, Setup against the PS2), `boardcheck.png` (the board at the
  PS2's spin 13), `cheat-compare.png`, `lodge-sheet.png`, `loading-sheet.png`, `sam-sheet.png`, `compare-race.png`.

### Gaps

- Sam: the per-part items other than tops and back kits keep `RIDER_SAM`'s look (above).
- Online: the key and resolver exist, but sending and loading them in `web/net` is the multiplayer agent's.
- Cheat skins' own FE packages stay the FE agent's rule assembly. The original never draws a cheat preview; its
  bucket would include both head variants.

## Board flex (2026-10-04)

Playtester report: the board never bends (board presses, rail leans). It is a morph-target part, not bones.
- **PS2 (SLUS_207.72, gp 0x4A30F0):**
  - The board, `board_BoardFlex<X>` (part file 2), has 8 morph targets.
  - Their weights are an output of the pose blender 30F2B0 (from 312598, geometry = animator+0x54), in the same pass as the
    bones: the clip's file-2 stream (8 channels; 177 clips have one: BP_* presses, RSFS/RSBS rail balance, grinds, L_*
    landings, grabs, Ubers) goes into `*(geometry+0x3C) + part+0x8`.
  - Geometry part (0x58 stride): +0x8 weight offset, +0xC morph index (-1 without morphs), +0x10 slot mask, +0x40 mirror
    table, +0x4C morph count. The morph slot bit is geometry+0x10 (slot count) + the morph index. The board is always index
    0 (files 0 / 1 have no morphs), so the channel-1 upper-body masks (+0x8C0 / +0x8C8) never cover it.
  - Per layer (0x30F7E0..0x30F97C), the weight is the bones' coverage weight. A layer only covers the slot when its clip has
    a file-2 stream (0x30F4B0) and its mask has the bit. Weight 1 samples straight into the weights (311318) and marks the
    part written. A smaller weight blends `(1 - w) out + w s` (sub.s / mul.s / mul.s / add.s). A mirrored layer (+0x40)
    takes channel `mirror[i]`, with part+0x40 = the MNF morph_ids, [4,5,6,7,0,1,2,3] on every board.
  - A part no layer wrote at weight 1 is zeroed (0x3100A4..0x3100D0).
  - 11EB60 passes rider+0xB1C, which ANDs the slots with geometry+0x158 (bones 0..23: no morphs, no hair). 0x122570 sets
    it when the rider is not drawn (+0xB18 = 0) or is at LOD 2 (+0x898). The human is 0 in play. The port computes the
    weights whatever the LOD, a difference that cannot be seen.
  - Snow Jam Zoe: geometry 0x5DC600, slot count 29, weights at 0x5DC000 (board 0..7, NIS head 8..43, hands 44..61).
- **Morph targets:** the PS2 MPF keeps a chunk's morphs after its positions, one UNPACK V4-8 (VIF 0x6E) per morph:
  {count,0,0,0}, then count x {dx,dy,dz,slot} in 4 mm units, with slot = 3 x the chunk vertex.
  - A chunk lists only the morphs that move it, in morph order and without an index. `tools/export_board_flex.py`
    therefore gives each packet the increasing morph whose GameCube deltas fit it best, and keeps the PS2 values.
  - The GameCube MNF twins (int8 mm) differ from the PS2's by up to 0.8 cm (board A; 1.03 cm on N; fit 0.987).
  - BoardFlexB (Gutless) has unmorphed and morphed copies at one position, so it keeps the GameCube deltas.
  - Board A's export equals an independent packet decode (local/board-flex/qa/check_deltas.py): 808 entries, 0.0 cm.
- **Port (pv `boardFlex`, off until the WebKit visual check):**
  - Assets: `python3 tools/export_board_flex.py OUT` -> RIDER_<X>/board-flex.{json,bin} (29 packages, in web/public/assets since
    2026-10-04) and hand-morphs.{json,bin} (29 packages, also in web/public/assets). Without them (or off) the rest shape.
  - Core: `engine/animation_motion.cpp` `originalAnimationMorphWeights` and `web/animation_bridge.cpp`
    `board_morph_configure(slotBit, count, mirror)` / `board_morph_slot` / `board_morph_count` / `board_morph_weights`.
    The weights are sampled with every pose: the tick's pose, the 11D660 reset placement (shown by the stage teleport),
    the mission placement. `init_animation` clears the configuration. Nothing in the simulation reads them.
  - Page: `web/board-flex.js` loads `RIDER_<X>/board-flex.json` + `.bin` and configures the human's core after each
    init_animation (main.js), and a computer rider's at its capture (opponent-riders.js).
  - `web/rider-skinning.js` adds `sum(w x delta)` to the source position before each palette's skin, with that palette's
    weights (the previous and current ticks, interpolated like the palette). The shadow uses the same node.
  - Wardrobe outfits: `web/wardrobe.js` builds the same files from WARDROBE parts.bin (GameCube deltas, within 1.05 cm).
  - Online remote riders (web/net) draw the rest shape.
- **Evidence:**
  - New captures `local/board-flex/runs/bf-{boardpress-nose,boardpress-rail,rail-balance-lr,tech-land-mid}` (`--watch
    0x5dc000:256`, linked as runs/boardflex). Gates `ps2-captures boardflex/*` (`web/board-flex-compare.mjs`): the 8
    weights are bit-equal on every tick (239 / 499 / 800 / 274; 179 / 74 / 17 / 0 non-zero ticks).
  - In those runs the PS2 board bends during presses (nose press ~1.07 summed, morph 5 at 0.99) and in rail presses. It
    does not bend on the plain rails of rail-balance-lr (semantics 68 / 18 have no file-2 stream). It bends on that run's
    landing clip (semantic 62).
  - PS2 frames `local/board-flex/snaps/{nose,rail}.tick*.png`; Chrome / WebKit side and chase frames under
    `local/board-flex/shots`.
  - `web/test-board-flex.mjs` checks package files, wardrobe parity and the wiring.
- **The race hands (2026-10-04, same switch):** HandsX (file 7, 18 morphs: fists, grab and bar grips) are blended the same
  way from the clips' file-7 stream (292 clips). Snow Jam Zoe: morph index 2 (the NIS head, file 5, is 1), slot bit 31, weights
  +44, part+0x40 = [9..17, 0..8]. Their bit is in the upper-body masks, so the channel-1 reactions cover them (unlike the board).
  - Core: `morph_part_add(file, slotBit, count, mirror)` adds a part after `board_morph_configure` (which clears).
    `morph_upper_bit(slotCount)` is the lowest rider+0x8C0 bit at or above the slot count: 11C298 ORs the file-7 "morph" bit
    into it (Zoe 31, Allegra / Moby 33). `board_morph_weights` returns every part's weights in order.
  - Assets: `hand-morphs.json / .bin` (same format, 29 packages). The PS2 packets cover only some morphs per chunk (Zoe HandsB:
    95 packets over 7 chunks), so `assign_packets` assigns them in order by dynamic programming against the GameCube deltas. The
    fit is within 0.39 cm on every hand (Gutless's skel_HandsA keeps the GameCube deltas, as its board does).
  - Renderer: 26 weight columns (board 0..7, hands 8..25) in one delta texture and seven vec4 uniforms per palette, so every
    rider keeps one shader. `flex.layout` maps the core's weight order onto the columns.
  - Gates: the same 4 captures, hands bit-exact on every tick (195 / 305 / 461 / 197 non-zero ticks), in ps2-captures
    boardflex/* (summary `handMorphs`).

### Animation channel audit (2026-10-05)

30F2B0 maps each clip packet part (file id) to a geometry part (geometry+0x1C). Bone channels go to the local pose (+0x24 / +0x28)
and morph channels to the weights (+0x3C + part+0x8). Slots outside +0x150 (active) are skipped, and so are slots outside +0x158
when rider+0xB1C is set (hidden / LOD 2). Sources: Snow Jam Zoe's geometry, Stretch's glide.p2s, the WARDROBE part slots and the
497 gameplay clips (animation-packets.json).

| file / part | gameplay clips | PS2 | port |
|---|---|---|---|
| 0 body, 22 bones | 476 | bones | driven; world bones gated in every capture (BONE_SCAN) |
| 1 bindings, board_rootg / childg | 441 | bones (board-off / board-grab Ubers too) | driven, gated |
| 2 BoardFlex, 8 morphs | 157 | morphs | boardFlex |
| 7 race hands, 18 morphs | 247 | morphs | boardFlex |
| 15..42 hair, hats, hood, wings, backpack, tail, ... | 7 each (SH_ / SL_ / ST_) + UBER_NOSE_BEAVER (39) | bones, secondary channels 3..5 | driven (in the rig), gated for Zoe's mop |
| 45 Stretch SpecialAB, 1 bone | UBER_NOSE_STRETCH (+ _CYC) | bone | driven |
| 46 Stretch SpecialA, 1 morph (index 5, bit 32, +81) | the same 2 | morph | boardFlex (special-morphs.json), gated |
| 5 / 6 / 8 / 9 / 11 NIS head, eyes, NIS hands, PDA | FE / NIS clips | inactive in the race geometry | FE preview, cutscenes.js |

- **Tricks** (gates `ps2-captures boardflex/bf-*`, 22 captures: `local/board-flex/recapture.py` re-ran the trick runs' scripts and
  pokes with the weights watched): board and hand weights bit-exact on every tick.
  - Ubers bend the board: L1+R1 (semantic 95) a sum of 2.0, rail Uber (216) 1.18, R1 (131) 1.06, L2 (128) 0.86, L1 (122)
    0.46, R2+Square (116) 0.33. Tweaks (TW_*) bend it too.
  - Plain grabs (G_*) carry no file-2 stream: the board stays straight in the air (score-grabs, pipe-air-grabs: 0 on every
    air tick).
  - Landings (L_*, 62 / 63) and crash clips (WH_*) bend it most.
- **Stretch's SpecialA:**
  - Exported with the live geometry's slot bit (`tools/export_board_flex.py live_slot_bits`; wardrobe `geometryMasks().morph_bits`).
  - Gate `boardflex/bf-stretch-nose-uber` (`--event --human RIDER_STRETCH`): his nose Uber, from his countdown.
    - The Uber is L1+L2 + Square (grab slot 4, mask 3), semantic 160, UBER_NOSE_STRETCH, on the long air 1299..1411.
    - A Super Uber is poked (tier 10) to keep the meter full; the weights are watched at 0x200 bytes.
    - SpecialA is at +81, past a 0x100 watch: the hook refuses a part outside its window.
    - Board, hands and SpecialA weights are bit-exact for 1599 ticks; SpecialA's peak is 1.063.
  - SpecialA is a 6 cm part on bone 26 (speciala) that moves up to 7.6 cm, only during that Uber.

## Race rider texels (PS2 domain)

(Restored: this section was lost when the file was overwritten concurrently.)
- The PS2 rider textures (DATA/CHAR/<X>TXP.BIG .ssh) hold colour at half intensity (GS 128 = 1.0); the GameCube copies
  the packages were decoded from are about twice that. GS HIGHLIGHT2 (`web/rider-material.js` riderHighlight2Node:
  tex x light / 128 + a) multiplies the PS2 texel bytes, so race riders were about 2x too bright on their lit sides.
- `tools/export_characters.py` `ps2_texel_rgba` / `ps2_texel_pngs` (CLI `--ps2-texels`, also run by `web_package`,
  `retexture` and `tools/export_opponent_packages.py`): every `RIDER_<ID>/9-N.png` with a GameCube resource gets the RGB
  of the same-named `.ssh` (`export_career.shps_image`), alpha kept; `world.json` textures get `texel_domain: 'ps2'`.
  30 packages (Zoe and the five opponents included); texture choice, geometry and rider.json unchanged.
- `web/rider-material.js`: PS2-domain maps are used as bytes x 255; GameCube-domain maps (Sam, generated outfits) are
  halved (x 127.5). The unlit branch (menus, opponents before lighting) draws PS2 texels x 2, as before.
  `main.js asset()` and `web/opponent-riders.js` copy `texel_domain` to `tex.userData.texelDomain`.
- Checked at countdown tick 18 against the PS2 `countdown.png` for Psymon, Zoe, Kaori, Griff and Brodi: e.g. Psymon's
  jacket 94,69,84 vs PS2 105,76,92 (was 163,117,137); his board identical.
- **CLUT past its count (2026-09-28):** the PS2 RGB is read with `shps_image(gs_clut=True)`: the full 256-entry CLUT
  the GS gets, not only the block's count. 535 TXP textures (hair / hats, `ea*`, `ext*`, a few boards and boots) index
  entries past the count; the old read made them black specks (532 port textures, 60,065 texels). Evidence (GS VRAM,
  recoloured-CLUT frames) and the rider draw state (`alph` / `ea*` blended 0x44 with ATST GREATER 92, the rest opaque)
  in docs/xbox-textures.md section 6.

## Rider switches off the main thread (2026-09-23)

The user reported the whole game freezing when Select Character switched to a rider not loaded yet (Safari: one
66-153 ms frame per first switch, one 548 ms freeze; Chrome ~50 ms).
- **The race model is no longer loaded on a switch.** `main.js selectRider()` only records the choice
  (`selectedRider`, `pendingRider`); `ensureRider()` loads the race package, its settings and `init_animation` when a
  race needs it: the event load's warm-up (`ui.loadEvent` -> `cb.warmup`), `ssxQA.rider/start` and `startRun()` (which
  loads first when a rider is pending). The front-end screens never show the race model.
- **The front-end preview is prepared in a worker** (`web/fe-preview-worker.js`): fetch, JSON parse, per-part
  vertex/skin/index/morph arrays and texture decode (`createImageBitmap`, no premultiply, no colour conversion) are
  transferred to the main thread, which only wraps them in three.js objects (`FrontEndPreview.build`). Generated
  Equip Gear packages (web/wardrobe.js memory) are posted to the worker.
- **Compiled and uploaded before it shows:** `FrontEndPreview.fetch()` runs the owner's `compile` hook
  (`renderer.initTexture` for every map + `renderer.compileAsync(group, camera, scene)`, with the group visible:
  compileAsync skips invisible objects) and caches the model per package root; a switch to a cached rider is a swap.
- **Prefetch:** once the shown rider is up, Select Character warms the rest of the roster, nearest neighbours first,
  one package at a time.
- **One skinning pipeline:** every preview skeleton is padded to 32 bones (identity dummies, unreferenced by the skin),
  so all riders share the bone-array size of the skinning shader.
- Measured (scratchpad perf/chrome-switch.mjs, perf/safari-switch4.mjs; machine load 10-30 from other agents):
  Chrome: every switch <= 18 ms frames (was 33-50 ms, one 117). Safari: cached switches <= 18-27 ms; switches during the
  first ~15 s of prefetch mostly 20-40 ms with occasional 80-190 ms frames under load (was 66-548 ms on every
  first switch). Entering Select Character the first time still compiles the first preview (one 100-200 ms frame).

## Sam in the roster row (2026-09-23)

The user found Sam's outline greyed out and set apart from the row. Before: the Sam PS2 build's 64x128 atlas (a
generated mask, GS alpha 0..128 left undoubled = half transparent) drawn after Kaori with a gap, on the front canvas.
Now he is the eleventh figure of the group, made the way the originals are:
- The originals (FE_1-20) are flat silhouettes of the riders' models in front-end standing poses with ~1 px soft
  edges: white strip 0x00A79FCC (layer 9) and one orange (194,76,0) highlight per rider (layer 12), all at 130% x 110%
  with the group offset (-4,12), feet on one line (y + 12 + h x 1.1 = 364.5).
- `tools/render/sam-silhouette-pose.mjs` renders Sam's own front-end preview (Mac's idle FE_GEAR_MAC_CYC at t=0, the
  preview camera and yaw) on a magenta and a green clear colour; `tools/export_sam_roster.py` recovers exact coverage
  from the pair, scales it to 78 page pixels (Elise's height: Sam is 5'11" like her, model size 96), box-filters it,
  keeps thin limbs solid, and writes `UI/sam-roster.png` (white and orange cells) plus the layout:
  x 504 (overlapping Kaori like neighbours overlap), y 267, 38x78, right arrow at 560. `tools/export_character_select.py`
  calls it (the old atlas is only a fallback).
- `web/character-select.js` draws his white figure with the strip (ui-bg canvas, the strip's own alpha) and his orange
  figure with the highlights when he is focused, like every rider state; his name and card use Mac's state.
- Checked in Chrome and Safari with Sam, Zoe and Kaori focused (scratchpad chars/samrow/row-before.png, row-after.png,
  row-safari.png).

## Not done / gaps

- Computer-rider lineups: done (see "Computer-rider lineups"). Between-event presentation draws are estimated.
- The human's grid spot differs by a few cm per character in the original; the browser uses Zoe's event seed.
- FE preview: cheat skins keep the base rider's preview, as the original does (`?feSkin=1` shows the skin's
  rule-built package, no PS2 evidence). The hidden-model windows on Setup/Rider Details transitions are reproduced
  (`hidePreviewFor`, see above).
- Front end: Select Peak / Select Mode / Select Event and the rest keep the browser layouts. Around Select
  Character: the option values (speed units, vibration, Pro layout, HUD level) are stored and announced
  (`ui.cb.feOptions`) but nothing in the race applies them yet; Screen position / Video calibration are disabled;
  the 130rewardposter video items do not play the video.
- Equip Gear: see "Equip Gear and outfits".
- The rider speech lines on Setup Character are not played.
