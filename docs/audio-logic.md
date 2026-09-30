# SSX 3 audio logic (PS2 original, for the browser port)

Read-only research of how the original game drives music, mixes, the DJ, speech and sound effects. File formats
(MPF/MUS Pathfinder data, SCHl speech, BNK banks) are in [audio-formats.md](audio-formats.md); this document is the
behaviour those files are played with. Addresses are EE virtual addresses in `SLUS_207.72` (file offset = vaddr -
0xFF000); `gp` = 0x4A30F0. Function names are descriptive, not recovered symbols. "?" marks an unconfirmed reading.

Sources: static analysis of `local/output/*.cpp` (disassembly comments) and the ELF data, plus `DATA/CONFIG/*.INF` and
the audio BIG archives from the user's ISO. Section 7 lists the questions and how ARMSX2 captures answered them
(2026-09-23); section 9 is the browser implementation.

## 1. Objects

| Object | Where | Notes |
|---|---|---|
| `SSXAudioSystem` | `*(gp+0x410)` = 0x4A3500, 0x7780 bytes, created by `284BB0` -> ctor `284C68` | `28B180` returns it; every game-side audio call is `jal 28B180` then a method. |
| Music manager | `audio+0x118` (vtable `0x4838F8` at `audio+0x118+0x5440`) | 64 song slots; see 3.2. The "audio" pointer in this doc is the SSXAudioSystem. |
| Current song instance | `audio+0x520` (= mgr+0x408) | `SongInstance` (ctor `2B1B98` -> `2B1C50`): Pathfinder stream + async overlay. |
| Music listener vtable | `audio+0x5558` -> `0x483688` | entry +0x10 `28F478` PlaySong, +0x18 `28F328` SendEventForced, +0x20 `28F3C8` SendEvent. Every "music event" below goes through +0x20 unless noted. |
| Speech/DJ player | `audio+0x5560` | see section 4 (`2B0E60` SpeechInstance, `2B1758` start, `2B11B0` stop). |
| Timer/callback queue | `*(audio+0x118)+0x1D8` | `2ADCA0(queue, delayMs, pmf, a3, arg1, arg2, ...)` posts a delayed member-function call; `2ADDE0` cancels by callback. Keys: 0x4A3648 -> `28E088` (music request), 0x4A3650 -> `28E548` (DJ line), 0x4A3658 -> `28E068` (retry song change), 0x4A3850 -> `2B3E20` (song volume ramp tick, 40 ms). |
| Suppression `288AE0` | `*(G+0x84+0x28)` in 1..9 | Blocks PlayMusic (unless `audio+0x6294`), countdown beeps, the big-air duck, and every speech category. **= instant-replay playback** (session+0x28 is the Replay object, ctor `26EFB8`; states 1-9 = playing; 0 = recording in a live race; every results screen shows 1 because the run replays behind it). Not demo, not splitscreen (ARMSX2). |
| Game object G | `*(gp-0x848)` | `28B1B0` = in game (`G+0x84 != 0`); `28B1C8` = session `G+0x84`; `28B1D8` = race `G+0x84 -> +0xC` (riders at `+0x28[i]`). |
| Game state bytes | `0x535C08` course index (row of the course table 0x43D950); `0x535C10` event kind (ELF tables 0x43E7D0 / 0x43E978, ARMSX2: 0 race, 1 slope style, 2 big air, 3 half pipe, 4 free ride (incl. missions), 5 time challenge (rival time, peak races), 6 points challenge (rival points, peak jams)); `0x535C11` path (0 Conquer the Mountain, 1 single event, 2 multiplayer); `0x535C12` game mode (career-events.md; 4 rival time, 5 rival points, 6-8 peak races, 9-11 peak jams, 12 free ride, 13 mission) | Single event Snow Jam = 0/1/0, slope style 1/1/1, pipe 3/1/2, big air 2/1/3; CTM race 0/0/0. |

## 2. Buses, volumes and MIX.INF

### 2.1 Channels

The mixer has 11 channels, one 0x10-byte record each at `audio+0x62BC+16*ch` = {user volume, user x scale,
x duck, x mix = effective}. Effective volume `audio+0x62C8+16*ch` is what streams and voices read (`287968(audio,ch)`
returns a pointer to it).

| ch | Bus | User slider (`287410`/`287488`/`287520`) | Notes |
|---|---|---|---|
| 0 | master | - | `287700` ch0 -> `3B8160(vol*127)` |
| 1 | MUSIC | Music | Pathfinder songs, loading/map loop, peak hub/pktrans/charsel/chartune |
| 2 | DJ | Music, only while DJ is enabled (`audio+0x62B8`) and radio mode != 2; else 0 (`287558`) | DJ Atomika |
| 3 | PA | Effects | announcer |
| 4 | CHARACTER | Speech | rider speech; x per-character scalars gp+0x43C..0x460 (all 1.0) into `audio+0x636C[10]` |
| 5 | (unnamed) | Effects | countdown beeps, UI/HUD sounds; not in MIX.INF |
| 6 | BOARD | Effects | |
| 7 | COLLISION | Effects | |
| 8 | AMBIENT | **Music** | ambience loops and the Peak*Amb streams |
| 9 | ARCADESFX | Effects, only if arcade audio is on (`audio+0x62B4`, `2875D0`) | |
| 10 | ARCADESPEECH | Effects, same gate | |

Per-channel base scales `288D18` (gp+0x414 + channel slot, applied by `287700`: effective = slider x scale x duck x
mix): FE table (`285FB0`, a1 = 0) all 1.0 except CHARACTER 0.6; world table (`2862A8`, a1 = 1) MUSIC 1.0, DJ 0.55,
PA 0.43, CHARACTER 0.8, ch5 0.8, BOARD 0.75, COLLISION 0.8, AMBIENT 0.5, ARCADESFX 0.75, ARCADESPEECH 0.75. CHARACTER
lines and grunts use a per-speaker gain (`287968` bus 4 with a CHARDB id: audio+0x636C + 4 x id = ch4 x gp+0x43C + 4 x
id): moby 0.9, kaori 0.9, allegra 0.9, mac 0.9, zoe 0.8, griff 0.85, elise 0.9, nate 1.0, psymon 1.0, viggo 0.95 in the
world, 1.0 in the FE. (ARMSX2 effective values at default sliders: ch2 0.5, ch3 0.391, ch4 0.727, ch5 0.727, ch6 0.682,
ch8 0.455, ch9 0.682 = 0.909 x these.)
User volumes are menu steps 0..11: `2873D8` = clamp(step x 1/11, 0, 1), linear. Profile defaults (`14F458`): music,
speech and SFX steps 10. Channel 0 (master) is set once to 10/11 by the audio ctor (`285094`, gp-0x4660) and no
slider changes it: the SND master byte `0x50AA5D` = trunc(0.909 x 127) = 115 scales every voice and stream. The
volume law is linear end to end (ARMSX2: halving a group's channel drops its output 6.0 dB; the SPU2 stage is unity,
core0 BVOL / core1 AVOL / both MVOL at 1.0), SPU voices at byte x 129 per side, IOP-mixed voices at byte x 258 in
Q15, centre pan 181/255 per side. Measured PS2 output (Snow Jam, countdown to GO + 19 s, default sliders): full mix
-13.1 dBFS RMS (0.5 s windows -17.2 / -13.6 / -10.2), music + DJ + ambience -15.6, effects -15.9, rider speech
-23.7 while speaking; about 0.015 % of samples exceed full scale (hard saturation, no limiter). Getters `287670`/`2876A0`/`2876D0`
return round(vol x 11). `web/audio-engine.js` uses this grouping.

### 2.2 MIX.INF

Parser `287FC8` (path `*(gp+0x470)` = "data/config/mix.inf"): up to 25 `[Mix n]` records of 0x28 bytes at
`audio+0x647C`, count `audio+0x6478`, defaults 100 and TIME 0. Columns MUSIC, DJ, PA, CHARACTER, BOARD, COLLISION,
AMBIENT, ARCADESFX, ARCADESPEECH map to channels 1, 2, 3, 4, 6, 7, 8, 9, 10; TIME is stored as seconds (ms x 0.001).

`2883B0 SetMix(audio, idx)`: idx < 0 -> 0; ignored if idx == current (`audio+0x646C`). Level -1 means "keep".
- TIME == 0: targets written immediately.
- Otherwise `2887A8`: per channel target `audio+0x6440[ch]`, step `audio+0x6414[ch]` = (target - current) / TIME x
  (1/60). `2887F8` (every frame from `285BF8`) adds the step and clamps: **linear in amplitude, 60 Hz steps**, same
  duration for every bus.

Who selects a mix:
- `289C98` from the **tWPIGD_Mix world painter** (type 1) at the primary rider's position (`2898A8`, called per rider
  from `121950`). In the retail data only two regions are non-zero: ABC1 (Happiness) uses Mix 1 (effects and speech
  70%, music/ambient 100) and BRA2 (Metro-City) uses Mix 2 (music 70, DJ/PA 50, arcade 70), both with 3000 ms fades.
- `286A80` (world teardown) -> SetMix(0).
Nothing else calls SetMix: menus, pause, countdown and results do not change the mix.

### 2.3 Duck layer (separate from mixes)

`287F00(audio, mode, seconds, level)` fades a group of channels through the per-channel duck factor
`audio+0x6394[ch]` (state: active `0x63C0`, mask `0x63C4`, step `0x63C8`, target `0x63CC`, current `0x63D0`,
mode `0x63D4`; ticked by `287C48` each frame). Mode 0 = mask `0x173` (`284BA0`) = MUSIC, ch5, AMBIENT, BOARD,
COLLISION, ARCADESFX (not DJ, PA, CHARACTER or ARCADESPEECH); mode 3 = "release whatever is ducked".
Mask bits (`287A10`; each block is governed by the `andi` in the previous branch's delay slot): 0x1 MUSIC, 0x2 ch5,
0x4 CHARACTER, 0x8 DJ, 0x10 AMBIENT, 0x20 BOARD, 0x40 COLLISION, 0x80 PA, 0x100 ARCADESFX, 0x200 ARCADESPEECH.
- **DJ talk-over:** `29F000` (speech stream volume request, vtable 0x4836D8+0x18) calls `287F00(0, 0.9999 s, 0.65)`
  when a stream starts on channel 2 (DJ): the group ducks to 65% over ~1 s.
- **Release:** `285BF8` waits until `2AB028` reports the stream playing (`0x63E0`), then when it stops calls
  `287F00(3, 0.5 s, 1.0)`. The same release runs at FE init (`285FB0`), world init (`2867E8`) and `2871B0`.
- A second gate `289688` (every 2 frames while `audio+0x6C58`) would drive the MUSIC duck (mask 1) to 0.0/1.0 from
  a per-line byte envelope at `audio+0x6868` (>= 30 -> 0.0) when the speech bus (`29F088` = `audio+0x5738`) is 2.
  No code writes `audio+0x6868`/`0x6C50` or sets `0x6C58`, so it is inert in retail: do not implement it.

## 3. Music system

### 3.1 MUSIC.INF (`2B28C0`, class `cSongConfiguration`, 0xAC bytes, ctor defaults `2B1A98`)

Keys are case-insensitive (`41AA88`). Per song the config holds: +0x00 PATHDATA (.mpf), +0x20 MUSDATA (.mus),
+0x40 loop file count, +0x44 LOOPDATA, +0x64 BeatsPerMeasure (4), +0x68 MeasuresPerBar (2), +0x6C PhrasesPerBank (4),
+0x70 BeatsPerPhrase (8), +0x74 PhraseAlign (16), +0x78 DelayCount (0), +0x7C DelayFeedback (90), +0x80 DelayTime
(100), +0x84 DelayLevel (50), +0x88 PathLevel (100), +0x8C AsyncLevel (100), +0x90 BPM (120.0), +0x94 DUCKTOLOOPS
(1), +0x98 SEDVALUE (-1), +0x9C LOWPASS (0xFFFF), +0xA0 PREVIEW (-1), +0xA4 SONGBIG (1). TITLE/ARTIST/ALBUM go to
64 x 100-byte tables at mgr+0x934/+0x2234/+0x3B34 (getters `2B40F0`/`2B4120`/`2B4150`); the section name (max 10
chars) to mgr+0x124+11*i. CATEGORY lines set flags mgr+0x420+0x14*song+4*cat (set membership, order irrelevant).
ADDTOFE counts front-end songs (mgr+0x3EC = 35): the first 35 sections are the licensed songs, in PLAYLIST order.
The Whole/Quarter/Eighth/Sixteenth switches and the Delay* keys are not used by any retail song (DelayCount stays 0,
so the async delay/echo parameters copied by `2B2850` do nothing).

Special songs (not ADDTOFE) and the "song id" the game stores for them (`2B49C0`/`2B49E0`, song+0x58):

| Section | Id | Played by | Bus |
|---|---|---|---|
| charsel | 301 (0x12D) | FE init `285FB0`, FE return `28FC58` | MUSIC |
| chartune | 201 (0xC9) | podium `28CDF8` | MUSIC |
| Peak1/2/3 (spokehub/sphub2/sphub3) | 1/2/3 | freeride hub music `28CF98`, `28E100` | MUSIC |
| Peak1Amb/2Amb/3Amb | 101/102/103 (0x65-0x67) | radio mode 2 (`28CF98`, `2899F8`, `28D5A0`) | AMBIENT |
| pktrans | 401 (0x191) | peak transitions `28CD48`, `28E8C0` | MUSIC |
| mapsel | - | never: the string "mapsel" is not in the executable | - |
| playlist songs | -1 | `28CF98` via the playlist index | MUSIC |

### 3.2 Playlist and song choice

Music manager fields: mgr+0x3E4 song count, +0x3EC FE count, +0x3F0 current index, +0x3F8 u64 playlist mask,
+0x3E8 songs in list, +0x920[5] songs per category in list.
- `289520 LoadPlaylist(section)`: PLAYLIST.INF `[SSX Mix]` (the only section; section names are cached at
  `audio+0x60A4` by `289470`) -> `2B3EE8` adds each SONG by name. The resulting mask is saved as the **Radio BIG
  playlist** `audio+0x518` (all 35 songs).
- Custom playlist = profile mask `158750` -> `audio+0x6098` (`28C2D0`, from the lodge/Request Line screen `196B90`
  and world init `2867E8`).
- `2B4070 SetPlaylist(mask)`: clear, then add every FE song whose bit is set.
- `2B42B8 NextSong(useCategory, cat)`: index = (index+1) mod count until the song is in the mask, and, if
  `useCategory` and the list has songs in `cat`, also in that category. (`2B41E8` is the same backwards; no caller.)
- **`28D488 PickNextSong`** (the only chooser):
  1. index = rand15 x FEcount / 0x7FFF (`2ADF60` -> `3177F0` rand, masked to 15 bits) - a random start slot;
  2. repeat (rand15 x 10 / 0x7FFF) + 1 times (1..11): `NextSong(1, CurrentCategory())`;
  3. if the result equals the previous song (`audio+0x62A4`), step once more with the category, then once more
     without it. Not a shuffle: repeats are possible after one other song.
- `28D8A0 CurrentCategory()` from the course index and mode: time challenges (mode 6..8) -> 0 Race; points
  challenges (9..11) -> 1 SlopeStyle; course 0-4 -> 0 Race; 5-7 -> 1 SlopeStyle; 8-10 -> 2 BigAir; 11-13 -> 3
  HalfPipe; anything else (backcountry 14-16, stations 17-21) -> 4 BackCountry.
- Then `28CF98 PlayMusic` plays "the current index" (PlaySong with a NULL name).

### 3.3 Radio modes (`audio+0x608C`, `28BF78 SetRadioMode`)

| Mode | Meaning | Playlist | DJ |
|---|---|---|---|
| 0 | Radio BIG | `audio+0x518` (SSX Mix) | options word `*(0x535610)` bit 17 (copied in `28BF78`) |
| 1 | Custom playlist, DJ | `audio+0x6098` | same bit 17 |
| 2 | BIG Mountain Ambience | none: Peak1/2/3Amb streams on AMBIENT | off |
| 3 | Custom playlist, no DJ | `audio+0x6098` | off |

`28D960` = mode != 2 ("music mode"); `2899E8` = mode == 2. Changing mode in game restarts the music when needed
(e.g. leaving ambience, or when the current song is not in the new list: `2B4AF0`). The internal order is not the
lodge menu order; verify the menu -> mode mapping of `195FF0` (checkbox index `obj+0x78`) against a capture.

### 3.4 Song instance, levels and filters

`2B35A0`/`2B3838 PlaySong(name|NULL, 0xC, &channelVolume, t0)`: stops the current song (`2B3AC0`), picks the slot by
name (or keeps mgr+0x3F0), builds `SongInstance`, starts the loop-bank loader `2B43D8`, sets mgr+0x418 = 1
(active; `audio+0x530`), mgr+0x41C = -1 (last event). `28F478` also pops the HUD "now playing" element (hud vfunc
0xC0 with 5) when the MUSIC channel is audible.

Per frame `2B21E0` (from `2B4388`/`2AB958`):
- first call after load: Pathfinder data `3D2350`, main stream `3D25F0(0x11000001, MUSDATA, ...)`, and if LOOPDATA
  exists the async overlay `3D25F0(0x11000002, LOOPDATA, ...)`; a pending event (song+0x30) is sent at once.
- then every frame: **LOWPASS** -> `2B2160` -> `3B80A0(stream, value)` (stream param +0x100, sent only on change;
  values 30000-42000 in the data, 0xFFFF = open); **volume** = clamp(pathVol x 0.01 x PathLevel x channelVolume,
  0, 127) -> `3B7E70`, where pathVol = song+0x40 (int8, 127 normally).
- The async overlay volume is set by `2B3D10(v)` -> `3D5508(0x11000002, v)` only while song+0x54 is set.
- Stream param setters: `3B7E70` volume 0..127, `3B7FB8` pitch (4.12 fixed, 0x1000 = 1.0), `3B80A0` filter,
  `3B7EB0(ms/10 ticks, target)` fade.

Controls on the current song:
- `2B3D48 FadeOut(seconds)` -> `2B2418`: `3B7EB0(stream, sec x 1000, 0)`, song+0x48 = 1. Used with 2.0 s and 1.0 s.
- `2B3A70 Pause` -> `2B2018`: pitch 0 and overlay pause (`3D1038(0x11000002, 1)`). `2B3A98 Resume` -> `2B2070`:
  pitch 0x1000, overlay resume. No fade.
- `2B3AC0 Stop` (hard).
- `2B3C98(v)` path volume song+0x40; `2B3C28` / `2B3C60` -> `3D0D70` / `3D15C8` on the stream (called as
  `2B3C28(mgr, 0x7F)` after starting hub songs; exact meaning ?).
- LOWPASS has no runtime modulation: the only setter (`2B3CD8`) has no caller. No music filter sweep on pause,
  crash or water.

### 3.5 Pathfinder events

The MPF graph reacts to integer events. `2B3BC0 SendEvent(e)` ignores e if it equals the last one; `2B3B88` forwards
to `2B20C8`, which calls `3D16F0(-1, e)` once the stream runs (otherwise it queues the first event in song+0x30).
Transitions are quantised by the MPF itself (not by game code). Events used by the game (meaning = context; the
graph decides what actually plays):

| Event | Sent by | Context |
|---|---|---|
| 0 | `28CF98` initial (a1=0), GO `29C7B0`, `2872A8`, `28F5B8`, painter A=0/B=0 | start / main body |
| 36 (0x24) | `28CF98` initial when a1=0x24; `2867E8` (single event/MP world load); requests kind 3; between heats `286EA0`; `28F2C0` | "idle/pre-run" section: loads, results, between runs |
| 37 (0x25) | GO `29C7B0` on a later run (song kept from the previous run) | resume from the idle section |
| 10 (0xA) | finish `286EA0` (last run); request-line exit `196B90` | ending |
| 7 / 8 | `28C8C8` take-off / landing of a long jump (direct, `+0x18`) | loops on / off (3.6) |
| 12 (0xC) | hub songs, Peak*Amb, pktrans start | default start of non-playlist songs |
| 18 (0x12) | `28DF18` song change | outro before the next song |
| 1 / 2 / 3 | pktrans (`28CD48`, `28E8C0`: 2 for Peak 2, else 3) | destination peak |
| 30 (0x1E) | `28E8C0` code 20 in ambience mode | ambience restart |
| 33 / 34 / 38, 39 | Big Challenge start by type 1/2/3 (`29D6E0`), end (`29D8E0`, `29DBB0`) | the challenge stinger and challenge part (3.9) |
| 9, 11, 13, 42 | MusicTrigger painter A=0 (`28D988`) | course sections (freeride) |
| 1/2, 5/6, 40/41 | MusicTrigger zones A=1/2 (`28D630`, `28D7D8`): event B on enter, B+1 on leave | zones |
| 0..9 | FE screens `28F140` on charsel (table 0x482A70) | menu sections |
| 11..45 | Request Line preview `2B4978`: song's PREVIEW sent to charsel | song previews |
| 1..10 | chartune per character (table 0x482A10) | podium theme |

### 3.6 Big air: DUCKTOLOOPS (`28C8C8`, every frame while a race/world is active)

Rider = primary player (`285D98(audio,-1)`), predictor = `*(rider+0x788)` (+0x98 predicted flight time T, +0x9C
predicted landing time, +0xA0 elapsed, +0xAC state). Flag `gp+0x52C` = "long air" (previous value copied to
`gp+0x530`), latched T `audio+0x6244`, start `audio+0x6248`, duck end `audio+0x624C`. Per frame:
1. T = 0 unless motion (`11FE98`) == 1. In the air with predictor state 1 or 3: T = the latched value if the flag is
   set, else T = +0x98 (latched, start = 0). Other states keep the latched T only while the flag is set.
2. musicVol = MUSIC effective volume (`287968(1)`, kept in `audio+0x60A0`); prev = flag; flag = 0.
3. s3 (loops allowed) = T > 3.9995 (gp-0x45D4) and the song's DUCKTOLOOPS == 1 (`2B4878`) and not Big Air / Half
   Pipe (`2A4168` / `2A41D8`) and no MusicTrigger zone (`28D898` = `audio+0x623C` == 0). `288AE0` -> return.
4. **T > 2 s**: flag = 1; e = max(+0xA0, start), start = e; on the first frame duckEnd = e + max((+0x9C - +0xA0) x
   0.5, 0.5). floor = clamp((1 - T x 0.8425197 x 0.5) x 127, 20, 127).
   - e < duckEnd: p = e / duckEnd; v = int8(clamp(127 - 107p, 20, 127)); if s3: loops level (`2B3D10`) =
     int8((127 - (v - 20)) x musicVol / 127 x 100); stream = int8(clamp(127 - (127 - floor) p, floor, 127)).
   - else: stream = int8(floor); if s3: loops level = int8(musicVol x 100).
   - `audio+0x5830` = (127 - stream) / 127 x ch5 volume (the whoosh gain); if s3: path volume (`2B3C98`) = stream.
   - First frame: `2910E0` whoosh (bank 0 snd 0x20, 2D, gain pointer `audio+0x5830`, base volume 127); if s3 and
     `2B4620` attaches the loop bank and music mode: event **7** (forced, `+0x18` = `2B3B88`, not the SendEvent filter).
5. **T <= 2 s after a long air** (prev == 1): `2913D8` stops the whoosh (250 ms); if loops were requested
   (`2B46E8`) and music mode: event **8** (forced); `2B4708` clears the request; path volume 127; loops level
   musicVol x 100 (the overlay stream itself keeps running).
ARMSX2: the whoosh starts 12-13 ticks after take-off once T > 2 s and its sent volume tracks 127 x `audio+0x5830`
(22 vs 24.7, 45 vs 46.5, 72 vs 76.4, plateau 77 vs 77.8). The loops never engage on Snow Jam / Metro-City / The
Junction (longest predicted flights 3.96 s / 3.16 s); with the threshold poked to 2.5 s the stream fell 127 -> 20 and
the loop level rose from 15.7 % to the music volume. The game-side slice scheduler (`2B4568` / `2B4740` / `28C430`)
is dead code (its only entry `2B4470` has no caller): the loops are the Pathfinder overlay voice (section 8).

Nothing else in gameplay touches the music: tricks, grabs, uber tricks, rails, boost/tricky meter, crashes and
knockdowns only call SFX/speech/crowd entry points (section 5). (All callers of the event, play, pause and level
functions are listed in this section and 3.7.)

### 3.7 What plays where

| Situation | Code | Behaviour |
|---|---|---|
| Boot / FE init | `285FB0` (from `18BEF8`/`1A1CE8`/`1D64C0`) | loads `[FE]` banks, releases ducks, plays **charsel** (MUSIC, id 301, no event yet); FE music state `audio+0x6290` = 10 ("none"). |
| FE screens | `28F140(state)` from each front-end screen's enter (not the in-world overlays: MCOMM, Messages, pause, Transport map; 9.11); ignored if the state is unchanged | charsel event by screen: title 0 (9 if an event was already sent), main menu / character setup / rider details / online menu / lodge 1, equip 2, buy attributes 3, audio options 4, uber-trick button map 5, rewards room 6, trophy room 7, career stats 8; states >= 10 send nothing. |
| FE -> game | `286200` | radio mode 2 (or FE-entered mode 2): stop; else FadeOut(1.0 s); unload FE banks. |
| FE re-entry | `28FC58` (from `1B3F78`), `28FC38` stop | charsel again at `audio+0x6290`. |
| FMV | `1D25E8` / `1D2638` (MoviePlayer) | music Pause / Resume. |
| Request Line (custom playlist) | `196B90`, `197E70` | highlighting a song sends its PREVIEW to charsel (FE) or, in game, plays the song now (`audio+0x508` = song, `PlayMusic(0,0,-1,1)` + "Now Playing" popup); exit saves the masks, `28C2D0`, and sends event 10 if a preview ran. |
| Loading screen | `233048`/`233640` -> `28F768` / `28FA98(1.0)` | `LoadingScreen.bnk` (resident, bank slot 13, `28F700`) looped on MUSIC; stopped with a 1 s fade when loading ends. Same loop for the map. |
| Map / Transport | `28F5B8` (open), `28F678` (close) | pause SFX, UI sound 9, FadeOut(1 s) in music modes, loading loop on, event 0; close: loop off (1 s), restart music at a station. In free ride the MCOMM Transport map returns early (+0x5FB4 / 28B1B0): nothing changes (PS2, 9.11); the Transport confirm (0x202068) calls `28F520` = Stop in music modes, then 28F558(dest). |
| World load | `2867E8` (from `22F6B0`) | radio mode and custom list from the profile, then (unless pktrans is playing) PickNextSong and PlayMusic with event 0 (Conquer the Mountain) or 36 (single event / MP). Creates the painter queries, resets triggers, releases ducks. |
| PlayMusic in a station (course >= 17, CTM, not forced) | `28CF98` | Peak1/2/3 hub song for the location's peak (`2A1E20`: courses 0,1,5,8,11,14,17,18 -> 1; 2,3,6,9,12,15,19,20 -> 2; 4,7,10,13,16,21 -> 3), event 12. |
| Countdown "3" | `29C420(3)` | if the current song is a hub song (1-3) or chartune and `audio+0x627C`: FadeOut(2.0 s). |
| Countdown "1" | `29C420(1)` | same condition: PickNextSong, PlayMusic(event 0), **Pause** (preloaded, silent). |
| GO | `29C7B0` | Resume; event 0 if `0x627C`, else event 37 and `0x627C` = 1; DJ artist intro request (section 4). Countdown beeps: ch5 sound 0x4E per second, GO sound 0x5E on ARCADESFX. |
| Race | `28C8C8` | big-air loops (3.6); painters (2.2, 3.5). |
| Pause menu / pause | `289B70` / `289BB8` (from `1F8168`, `2306A8`, `244880`); auto-resume in `285BF8` | Pause: SFX voices paused (`29CE28`), music Pause (pitch 0). Resume: music Resume, SFX resume, cancel queued speech. No mix, fade or filter change. |
| Finish (player) | `286EA0` (from finish routine `125108`) | crowd `2A73A8(2)`, stops the rider loops; Big Air/Pipe with runs left: event 36 (music keeps playing, `0x627C` = 0); otherwise event 10 (ending); results DJ lines. |
| Podium | `28CDF8` (from `236EE8`) | final round (`G+0xC0` state 3) and place < 3 (`0x536730`): chartune with the **winner's** character event: `0x536708[place]` = rider slot in that place (so `[0]` = winner), `0x536730[slot]` = that slot's place; the table 0x482A10 is indexed by the CHARDB base character `G+0xC0+0x18+4*[0x536708]` (savestates hold 4, 8, 2, 0, 5, 4 for Zoe, Psymon, Allegra, Moby, Griff, Luther): **Moby 10, Kaori 2, Allegra 8, Mac 1, Zoe 5, Griff 9, Elise 3, Nate 7, Psymon 4, Viggo 6** (ARMSX2). An AI winner's theme plays when the human is 2nd or 3rd. Previous playlist index saved in `audio+0x62AC`. |
| Restart / replay / quit menus | `28F200` (`26F8A0`) then `28F2C0` (`26F980`) | if chartune was playing: restore the index and queue a playlist song (kind 3); else event 36. |
| NIS music codes (kind-7 channel 1, `280640` → `28E8C0(code, 0)`) | `28E8C0` | 21-25 (the venue fly-overs: ARA1/DRA4/ERA5 21, freestyle 23, BRA2/CRA3/ESS3 24, pipes 25): in music modes PickNextSong, FadeOut(1 s), request kind 3 (forced) after 3000 ms, whose PlaySong brings the EA RADIO BIG box (PS2 ps2b/intro #94 t≈227). 19 / 20 need a2 != 0, so the code 20 in #148/#152/#166 does nothing (the travel change is 27A860's own `28E8C0(20, 1)`). Other codes are sent to the song as a Pathfinder event (0x28EF58). Browser: `gameAudio.cutsceneMusic`. |
| Travel (transport / lodge / return from an event) | `28E8C0(20)` from `27A860` (the transition cinematic) | cancels timers, stops rider loops and the location ambience; free ride: a destination (+0x6284 from `28F558`) gets PickNextSong + request kind 2 (hub song of its peak, or a playlist song with event 36 in the backcountry) after 10 ms, an unvisited backcountry destination pktrans event 2/3 (paused); ambience mode: the destination's Peak*Amb, event 30. **Not called at a location crossing** (9.9). |
| Leaving the world | `286A80` | Resume if paused, **Stop** (no fade), stop loops, destroy painters, SetMix(0), cancel timers, stop speech. |

### 3.8 Deferred requests and song changes

`28E088` (callback) sets the request `audio+0x6274 = 1`, `audio+0x6278 = kind` when there is no song, when
`2B2550` reports it finished (-1), or when `force` (arg2) is 1; otherwise it returns 0 and the queue retries.
`28E100` (every frame) executes it: kind 0 -> hub song; kind 1 -> PlayMusic(0); kind 2 -> in backcountry
(course 14-16) PickNextSong + PlayMusic(36, a2 = 1) else hub song; kind 3 -> PlayMusic(36); kind 4 -> PlayMusic(0).
`28DF18 ChangeSong` (painter B = 18, radio): if a DJ line is busy (`2A10C0(10)`) retry in 100 ms; else, without DJ,
FadeOut(2 s); event 18; PickNextSong; request kind 1 (forced) after 2000 ms; DJ intro callback (`28E548`, kind 3,
SEDVALUE) after 1500 ms.
The end of a song (`2B2550` -> `3D11B8` == -1) is only looked at by this callback; no other code advances the
playlist when a song ends, so in a race a song plays until one of the transitions above replaces it (presumably the
MPF graph loops; confirm on hardware).

### 3.9 Big Challenges: the start stinger (Wobble's own mix)

The sound at a Big Challenge start is not an SFX voice: it is the playing song's **challenge stinger**, a "long ending" of
its loop bank (LOOPDATA) that the song's own Pathfinder graph plays on the overlay track when the game posts the challenge
event. Every licensed song has the same two stingers in its bank (A 2658 ms, 58624 samples at 22050 Hz; B 2786 ms, 61440
samples; Go and Leave Home carry another 2786 ms B). **Wobble (Aphrodite) is the only song with a third stinger of its
own**: `wbloops0.mus` (MUSIC.BIG) has 67 entries, 64 = A, 65 = B and **66 = Wobble's mix** (2766 ms, 61003 samples,
not correlated with A or B). No other song has a song-specific one. There is no per-song table in the executable.

**Game side.**
- `29D6E0(audio, id)` from the mission pending-op runner `309118` (op 1, the start: `0x30917C`) and the restart (`30A868`
  op 3: `0x30AA2C`):
  1. +0x5FD8 = 0; the first rider's crash slide loops stop (`296E20` / `297438`), the pending-Uber sound (`29B3C0`);
  2. loops requested (`2B46E8`): in a music mode forced event 8, then `2B4708`;
  3. radio mode 2 (ambience): return;
  4. **a hub song playing** (`2B49E0` song id 1..3): cancel the request / DJ / retry timers, latch 18 (`28DEF0`),
     `28D488` PickNextSong, `28CF98` PlayMusic(0, 0, -1, 0): a playlist song starts instead, no stinger;
  5. type = `1547A0(id)` = row +0x22 (s16) of table 0x43EE10 -> +0x5FD0; jump table 0x482DE0: 1 -> **33**, 2 -> **34**,
     3 -> **38**, 0 / 4 -> -1 (no event). Of the 88 challenges 30 are type 1, 7 type 2, 2 type 3, **49 type 0** (no
     stinger);
  6. `2B4620` attaches the song's loop bank to Pathfinder track 1 (`3D2DF8(0x11000002)`, song+0x38 = 2); only if that
     succeeds: SendEvent(e) (listener +0x20 = `2B3BC0`, the repeat filter) and **+0x5FD4 = 1**; then `2B4708`.
- `29D6D0` (the offer prompt's Yes, `1F7548` / `1F7738`): +0x5FD8 = 1. `28D630` (MusicTrigger zone enter) returns while
  +0x5FD4 or +0x5FD8 is set.
- `29D8E0` (completed, `0x307294`): event 39 if +0x5FD4; +0x5FD0 / +0x5FD4 = 0; then always bank 0 sound **0x6D** on
  ARCADESFX (volume 127, not positional) and `2A3C00`(rider, **1**) = Arcade_Prompts 0x20A8.
- `29DBB0(audio, 0 fail / 1 quit)` (`0x3091EC`, `0x30AAB4`): only while +0x5FD4: UI event 14 on a fail, the slide loops
  and the pending Uber stop, event 39, flags cleared. `28BF78` switching to radio mode 2 in a world clears +0x5FD0 / +0x5FD4.

**Song side** (every licensed song, `web/public/assets/AUDIO/music/*.json`): 33 / 34 / 38 = save register 4 (current
node), track 0 jump 0 with flush (the music cuts), **track 1 jump to the stinger head**, wait 1200 ms, track 0 jump to the
challenge part of that type; 39 = track 0 back to register 4. The head (w4 bits 17-19 = 1) picks a branch with
(now_ms / 23) & 0x7F (3D3B08): Wobble head 646: 0-33 -> node 648 (entry 65), **34-80 -> node 649 (entry 66)**,
81-127 -> node 650 (entry 64), so Wobble's own stinger plays on 47 of 128 values (37 %). Two branches elsewhere (Man, Ride,
Screw Up: A only; Buffet, Like This, Poor Leno choose by the intensity byte, not randomly; Clockworks' are entries 0 / 1).

**Level.** A track-1 entry plays at the byte `3D41A8` latches: trunc(voice+4 % x 0.01 (double 0x495B98) x voice+0x34),
stored at the stream entry +0x30 (`2AE7A8`). The live MUSIC channel is not in it: `2B21E0` writes `3B7E70` for the
stream voice only (`2B2488`). voice+4 starts at 100 (byte 0x44F42E, `3D25F0`) and `28C8C8` sets it to MUSIC effective x 100
at a long-air landing (90 at default sliders; 59 while the DJ duck is on). So at default sliders the stinger plays at
127 / 127 (114 after a long air) and the stream at 115 / 127, both under the SND master 115 / 127.

**PS2 runs** (ARMSX2, silent; derived from `peak1/bc-sj-arrival`: playlist mask poked to one song, the Speed Demon
challenge (id 74627175, type 3) started with the prompt's pokes C+0x2A0 / +0x2AC / +0x2B0, C = 0xAD0370; Pathfinder voices
and the audio fields watched per tick; scratch `ps2/`):
- stage 1 (Peak1 hub song on Snow Jam): `29D6E0` -> `29B3C0` -> `28D488` -> `28CF98`(0) -> PlaySong: the playlist song
  (EA RADIO BIG box "Wobble / Aphrodite / Aftershock"), no event 38.
- Wobble, four starts: SendEvent(38), +0x5FD0 = 3, +0x5FD4 = 1; within 0-4 ticks track 0 is cut and track 1 commits
  **649 (entry 66)** at ticks 3197 and 3526, 650 (entry 64) at 3076 and 3436; the entry byte 114 (voice+4 = 90); track 0
  commits 592 (head 590) 75 ticks later; the stinger ends 166 ticks after it started (2766 ms).
- control, Avalanche: 488 (entry 49) and 489 (entry 48), then 432.
- completion (`peak1/bc-sd-lastgate`, Leave Home): `29D8E0` -> SendEvent(39) -> sound bank 0 #109 (0x6D) ->
  `2A3C00`(mask 1) -> speech event 0x20A8, all at tick 7376.
- earlier savestates agree: `bc-sd-accepted` has last event 38, +0x5FD4 = 1, song+0x38 = 2 (Leave Home).

**Port** (`pv bigChallengeAudio`, off until verified). Before it, `challengeStart` sent 33 / 34 / 38 without `2B4620`, so the
player never scheduled track 1 (the core commits the stinger, `reconcile` skips voice 1 while not armed): the music cut and
1.2 s of silence preceded the challenge part, for every song, unless a > 4 s air had armed the loops before. Now
`web/game-audio.js` challengeStart / challengeEnd / challengeStop / challengeAccepted follow the four functions above
(`web/big-challenges.js` passes the core's kind-7 prompt event), `web/sfx-game.js` challengeStops / challengeComplete, and
the player (`web/pathfinder.js`) sends track 1 to the engine master (`loopDestination`) at the 3D41A8 byte and draws random
heads from the audio clock (`clockRandom`: (ms / 23) & 0x7F). Checked: `web/test-challenge-audio.mjs` (all 35 songs' events,
the Wobble branch table, the PS2 starts replayed on the core, the player's overlay output and byte, the director's hub
branch, attach + 38, 39, type 0, the switch); headless Chrome (muted), Snow Jam free ride with Wobble: switch off -> track 1
never scheduled, switch on -> node 649 / entry 66 scheduled at the event, 592 1200 ms later. Known difference: the port acts
on the event at once; the PS2's service commits 0-4 ticks later and the challenge part ~3 ticks after 1200 ms.

## 4. DJ Atomika, PA announcer and rider speech

### 4.1 One speech voice, events and priorities

- The speech manager is `audio+0x5560` (ctor `2B07F8`/`2B0AE8`); its vtable (set to `0x4836D8` by `284C68`) calls
  back into the audio system: +0x08 `2854F8` start stream, +0x10 `29EEE0` bank name -> bus (stored in
  `audio+0x5738`), +0x18 `29F000` volume pointer (+ DJ duck), +0x20 `2A43B8` OnIdle dispatcher.
- Only SpeechInstance 0 exists (`2B0E60`): **one speech voice** shared by DJ, PA, rider and arcade speech.
- Request pattern of every category function (0x29FCC8-0x2A3EB8): bus gate (`29F0A0` CHARACTER, `29F0D8` DJ,
  `29F128` PA, `29F160` ARCADESPEECH: bus volume != 0 and not `288AE0`), then `2B1458(speech, ch 0, eventId, rider,
  speaker, f12, callback, refresh)` admits it into a 10-slot request table (ttl 180 frames) and
  `3D8008(1, 0, eventId)` + `*(gp+0x173C)` (= `3D76F0` -> `3D7418`) posts the event with bitmask arguments.
  Speaker: character index, 0xA DJ, 0xB PA, 0xC arcade.
- Bus by bank name (`29EEE0`): "DJ_" -> DJ (2) in a world, else a fixed-volume slot `audio+0x5730` (FE, never ducked);
  "PA_" -> PA (3); "Arcade" -> ARCADESPEECH (10); otherwise CHARACTER (4, per-speaker gain `audio+0x636C+4*speaker`).
- The EA speech engine (0x3CF000-0x3E3000) posts an event only if rand(100) < its chance byte (100 everywhere);
  of 16 event slots an expired one (age > W) is reused, else it **pre-empts an active event of priority <= P**,
  else the request is dropped. Priorities: DJ 900-1000, arcade 890-892, PA 800-801, rider 500-601. Rider and
  position lines additionally refuse while the voice is busy unless < 500 ms remain (`2B0290`).

### 4.2 Line addressing (for the audio-formats index)

- headers.big (SPEECH.BIG) / langhead.big (ENGLISH.BIG) hold one `.hdr` per bank plus `eventdat\Events.evt`
  (`2B0088`, `2AFD40`); lines stream from `"data/" + <language>.big + <bank>.dat` (`2B04D8`).
- Events.evt: +0x10 count (59), +0x18 u16 offsets x4; event = u16 id, u16 W (+2, max wait), u16 P (+4 priority),
  +9 chance %, u16 record count at +0xE, then records (condition = argument index + mask; u32 bank ids = the .hdr
  u16 id).
- .hdr: u16 bank id, 0xFFFF, u8 tag count, u8 line count, ..., u32 size/512, then per line [u16 BE offset (x512?)]
  [tag bytes]. **A line matches when `arg & (1 << tag)`.** Choice among matches: uniform random (`3DB5C0`, RNG
  `3DB4D0`) with no-repeat history (32-entry ring at 0x450B88 keyed by bank; values from the last min(n/2, 10)
  picks are rejected); `3D95B8` also has a shuffle mode, `3D8CB0` a weighted pick.
- Argument masks: language `29F198` (options word `0x535610` bits 22-24: 1 English, 2 French, 4 German, 8
  Spanish; always arg0 of DJ/PA events; the USA disc only has English); speaker character `29F330`/`29F2B0` and
  subject `2A1D48`/`2A1DA0` share one table by **CHARDB** id (123128: moby, kaori, allegra, mac, zoe, griff, elise,
  nate, psymon, viggo -> 0x4, 0x10, 0x200, 0x2, 0x20, 0x100, 0x1, 0x80, 0x8, 0x40; guest: speaker 0, subject 0x1000).
  Events.evt sends 0x40 to the `*_nat` banks and 0x80 to `*_vig`, so on the PS2 Nate speaks Viggo's lines and
  Viggo Nate's (the `Hey_<x>` banks confirm the bank names; the swap is in the event file; section 9.4); peak `2A1E20` (1/2/4); event bit `2A1BD8` (course 0-4 -> bits 0-4,
  5-7 -> 6-8, 8-10 -> 9-11, 11-13 -> 12-14, 14-16 -> 15-17); **song** (`2A2860`): SEDVALUE s < 0 or 999 -> generic
  (DJ_Artist_Intro_gen), s < 100 -> lo = 1 << s (DJ_Artist_Intro tags 0-29; no lines for 15 and 28), s >= 100 ->
  hi = 1 << (s - 100) (DJ_Artist_Intro_2 tags 0-2).

### 4.3 DJ categories and triggers (event id, function, trigger)

DJ lines play only in radio modes 0/1 with the DJ option on (bus DJ volume = Music volume, else 0: `287558`).

| Bank | Event | Function | Trigger |
|---|---|---|---|
| DJ_Radio_Big_Intro (+_FX) | 0x20E5 | `2A26F0` | `28E548` kinds 0-4, 7; rival start `2872A8`; hub entry `28E8C0`; single event / MP GO `2A3170` |
| DJ_Artist_Intro / _2 / _gen | 0x20CF (starts with DJ_Silence_500) | `2A2860(SED)` | pending `0x5774`/`0x5778` set at event start `2872A8`, GO `29C7B0` (career round 1, or round 2 in race/slope), `2A3170`, song change `28DF18` -> `28E548` kind 3 at 1500 ms |
| DJ_Event_Intro | 0x20CC | `2A2568` / `2A24B0` | MusicTrigger event gates B = 14-17 (`28D988`), post-event fallback |
| DJ_Radio_Big_Outro | 0x20E6 | `2A27D8` | same gates |
| DJ_First_Spoke | 0x20BA | `2A2638` | arriving on a hub spoke (`28E548` kind 0) |
| DJ_Text_Message | 0x212C | `2A2B88` | kind 0 on Peak 1 (variant 2 first time, then 1) |
| DJ_Free_Ride_Intro | 0x212D | `2A2C30` | kinds 3-5 on Peak 1, first visit of a race/slope location (`audio+0x579C[loc]`) |
| DJ_BC_Intro | 0x212B | `2A2AD0` | arriving in backcountry (kind 7) |
| DJ_Hub_* (Char_Stories 0x20CA, Terrain_Info 0x20E9, Weather 0x20C8, Peak_Boss 0x210B, Going_Ons 0x20E7, Local_Stories 0x20E8, Mtn_History 0x20C9), Text_Message | | `2A2E50(pool)` | hub chatter: pending `0x574C`/`0x5750`/`0x5754` from `28E548` kinds 1/2, painter B = 13, returning from an event (`28E8C0`) |
| DJ_Hub_Char_Progress 0x2102, DJ_Aggression 0x212E, DJ_High_Trick_Score 0x212F | | `2A2938`/`2A2CF0`/`2A2DA0` | post-event commentary `2A4770` after a Conquer-the-Mountain event (recorded at the finish by `2A45C0`/`2A4660`): progress if place 0, aggression if hits >= 5, high score if >= 27 (24 in big air); random, avoiding the previous one |

- `2A43B8` (speech OnIdle) fires **one** pending request per idle edge, in this order: Radio_Big_Intro (`0x5768`) ->
  BC_Intro (`0x5784`) -> First_Spoke (`0x576C`) -> Text_Message (`0x577C`) -> Free_Ride_Intro (`0x5770`) ->
  rider BC_Challenge (`0x5788`) -> Artist_Intro (`0x5774`) -> hub chatter (`0x574C`, `0x5750`, `0x5754`) ->
  Event_Intro (`0x5758`) -> Radio_Big_Outro (`0x5740`) -> rider Finish_Line (`0x5744`). Chains such as
  "Radio BIG intro -> artist intro" therefore play back to back.
- Hub chatter `2A2E50`: the very first call plays Char_Stories and forces Peak_Boss next; otherwise post-event
  commentary first, then a uniformly random unused category of the pool (8, or 5) with a used-mask
  (`audio+0x573C`) reset when exhausted (shuffle without repeat).
- Time/points challenges (mode 6-11, `2A42C8`) never queue hub chatter, First_Spoke, Text_Message or artist intros.
- Ducking: section 2.3 (65% over 1 s, 0.5 s release, DJ only). The DJ does not wait for musical boundaries; the
  song-change intro is simply scheduled 1.5 s after the new song is requested.

### 4.4 PA announcer (PA bus, speaker 0xB)

| Bank | Event | Function | Trigger |
|---|---|---|---|
| PA_Venue_Intro | 0x20BB | `2A39E0` | event world audio init `286E20`, single event / MP only |
| PA_Sponsor_Intro | 0x20C0 | `2A31C0` | pre-event screen `1FB588`, career, round < 2, kind != 4 |
| PA_Medal_Run_Intro | 0x20C3 | `2A3400` | GO `29C7B0` in Conquer the Mountain (0x535C11 == 0) when GMM+0x98 != 0 (GMM = G+0xC0 = *0x4A2C70): set to 1 by the race (`23A108`) and freestyle (`238E20`) handler inits for round 3 only and by the one-round handlers; 0 in the rival challenges. Followed by the `2B1758` flush. ARMSX2: round 1 no call; final: 0x20C3 posted at the GO tick with (English, course bit), the artist intro after it. |
| PA_Rider_Postition | 0x20C4 | `2A34D0` | finish `286EA0` (career, round < 3, non-race), `2B6550` (watrig special record) |
| PA_Finish_Line | 0x20C5 | `2A3708` | finish `286EA0` (race courses or final round); rival modes use the rider's own Finish_Line after 1000 ms |
| PA_Rider_Intro / PA_Rider_Race_Intro | 0x20C1 / 0x20C2 | `2A32B0` / `2A3358` | NIS kind-7 cue 1 / 2 at t0 of every freestyle / race **approach** (CTM intro); args (language, subject mask of the first human `28B1D8()+0x28`). Correction 2026-09-25: `2A19D8` is the NIS PA/DJ cue dispatcher (0x280B3C), not a debug selector; PS2 ctm/audio NIS-AUDIO.md logs 0x20C2 at #66 t0 |
| PA_Medals | 0x20C7 | `2A3860` | NIS cue 7 at t0 of every podium scene; args (language, subject mask, place 0/1/2 -> 1/2/4, 8 outside the career); PS2 log: #60 t0 |
| (other 2A19D8 cues) | | | 0 Sponsor_Intro (+ a forced song request when +0x530 = 0), 3 Medal_Run_Intro, 5 Finish_Line, 8 DJ_Hub_Weather, 0xA DJ_Hub_Char_Stories, 0xC DJ_Event_Intro, 0xE DJ_First_Spoke, 0xF PA_Venue_Intro (every venue fly-over at t30, so it also plays in CTM) |

Place mask `2A1E68`: MP race place 0->1, 1->2, 2->4, 3->8, else 0x10/0x200; otherwise from `0x536730` (table 0x483070).

### 4.5 Rider speech (CHARACTER bus) and arcade speech

Friend or foe `2A1820(A, B)`: 2 (foe) with two riders or two humans; else 2 if `155B50(A,B)` >= 2 or `155B50(B,A)`
>= 2; else `155AB0(A,B)`: 0 -> 0, 1 -> 1, other -> 2. Records: `0x4A6CA8 + bank*0x9B50 + char*0xF88 + other*3 +
0xBC1` = {kind, level, score}; `155B50(a,b)` = level (+0xBC2) and `155AB0(a,b)` = kind (+0xBC1) of b's record about a
(3 / 2 when a is the peak rival `145750`); bank `14A0E0` = 2 for computer riders, 0 for the human. Levels change in
the race through `155BF0` on rider-pair contacts (+1/+2/+4/+6), after the hit speech of that contact. ARMSX2 (Snow
Jam, 5000 ticks): Allegra/Zoe (kind 1, level 0) passes gave the friend lines 0x2080/0x207D; Psymon -> Zoe went from
00 00 00 to 00 02 0c after 12 hits and his pass at tick 3138 gave the foe lines 0x2081/0x207F.

Gate: CHARACTER volume != 0, not `288AE0`, rider not crashed, local human (`285D98`), voice free, listener within
range (`288B40`). Tiered banks (Amateur/Pro/Expert) are chosen by the peak (1/2/else).

| Bank | Event(s) | Function | Trigger |
|---|---|---|---|
| Big_Air_* | 0x2084/0x2083/0x2091 | `29FCC8` | take-off `294678` when predicted air >= 4.0 s, not in backcountry |
| Trick_Easy_* / Trick_Difficult_* | 0x2082/0x209A/0x209C, 0x2090/0x209B/0x209D | `29FF80` | trick scored `115B58` |
| Wipeout_* | 0x2088/0x20A0/0x20A1 | `2A02D8` | crash exit `12E690` |
| Pass / Passed_by (friend/foe) | 0x2080/0x2081/0x207D/0x207F | `2A0560` | overtake `299E28` <- `10F998`; game tick - audio+0x582C >= 600 where 0x582C = the GO tick (written by GO `29C7B0` at 0x29C7FC; ARMSX2: gate opens at game tick 780 with GO at 180); 670.56 < relative speed < 1788.16 cm/s; friend/foe = `2A1820` (below) |
| Hit_* / Aggression_Response | 0x208A/0x208E/0x208F/0x2078/0x207E | `2A0A30` | rider-pair attacks `10E228`..`10E468` |
| Post_Object_Collision | 0x2077 | `2A0E70` | soft collision `108388` |
| Whooh | 0x20B8 | `2A1560` | board press `130DD0` |
| Finish_Line_<char>, Hey_<char>, BC_Challenge_<char> | 0x20B9, 0x20D4, 0x20D3 | `2A1400`, `2A1280`, `2A1138` | rival finish, stage-script builtins (`1235F8`), rival BC start |
| Post_Selection / Customize | 0x20BC / 0x20BD | `2A16B0` / `2A1778` | FE character select `1A0358` |

Arcade speech (ARCADESPEECH, speaker 0xC): Arcade_Power_Ups 0x20A7 (`2A3B18`, pickups), Arcade_Prompts 0x20A8
(`2A3C00`: Tricky countdown masks 0x10/0x8/0x4, time's up 0x2), Arcade_Bonus 0x20A9 (`2A3CE8`), Arcade_Uber 0x2133
(`2A3DE0`), Arcade_Icons 0x2145 (`2A3EB8`). Rider grunts (GRNT_<char>.bnk) are SFX, section 5.4.

## 5. Sound-effect dispatch map (for the SFX pass)

### 5.1 Plumbing

Every SFX call site fills one request record in the sound manager `mgr = *(audio+0x118)` (struct of arrays at
mgr+0x1D8, slot index mgr+0x1F4; defaults pitch 100, delay {0, 90, 50}, volume 127) and calls `2906B8(audio)`,
which returns a voice handle (< 0 = failed):
- `*(audio+4)[i]` = bank slot, `*(audio+8)[i]` = sound index in the bank, mgr+0x1D8+0x38[i] = volume 0..127,
  mgr+0x214[i] = gain pointer (`287968(audio, bus, player)` = bus effective volume), mgr+0x1FC[i] = position
  (usually rider+0x110), mgr+0x238 owner rider, mgr+0x25C player (rider+0x870), mgr+0x1D8+0x40+8i per-voice update
  callback (pointer-to-member).
- Voice control: `2AD5F0(queue, handle, fadeSeconds, 1)` stop with fade; `2ADDE0(queue, callbackKey)` stop by
  callback; `2ABB38` stop; `2ABE78(voice, bend)` pitch (0x1000 = 1.0, max 0x3FFF); callbacks write volume at
  voice+0x64. `2ADCA0` = delayed play/callback.
- Gates used everywhere: listener within 100 units (`288B40`), repeat FIFO of 5 (obj, tag) pairs (`295028`/
  `295628`), local human only (rider+0x874 && rider+0x87C), muted after finishing (rider+0x470 >= 0).
- Curves `290B58(table, x)`: 5-point piecewise linear (x[0..4], y[5..9]); `290C10(i, x)` tables at 0x445898:
  0 = speed 200/400/550/800 -> 33/70/100/127; 1 = 100/1000/2000/4000 -> 20/70/100/127; 2 = 500..2000 -> 0/10/20/40;
  3 = 500..2500 -> 60/75/90/100/127.

Bank slots (loaders `285FB0` FE, `2862A8` WORLD, `29F3F8` grunts, `28F700` loading):

| Slot | Bank | Slot | Bank |
|---|---|---|---|
| 0 | MAIN: zbxsfx.bnk in game, SSX3Menu.bnk in FE | 7 | TRICKY (trickyut.bnk) |
| 1 | BOARD zboard.bnk | 8, 9 | per-location banks in the world data (SSB kind 20, rid 0 -> 8, rid 1 -> 9, loader `286CA8`; ARMSX2 confirmed the ARA1/BRA2/BHP1/ASS1/ABC1 pairs) |
| 2 | MOUNTAIN Mtn.bnk | 0xA | LAND land.bnk |
| 3 | dynamic, named world-trigger banks (`29B818`) | 0xC | dynamic |
| 4 | TRANSPORT | 0xD | LoadingScreen.bnk |
| 5 | CROWD Crowd.bnk | 0xE / 0xF | GRNT_<char>.bnk / GRNT_AI.bnk |
| 6 | AUX (unused) | 0x10 | SPUBOARD zboardSPU.bnk |

**SNOW.INF is not used**: "snow.inf", "AssignVol", "GLIDE", "Slip", "Lean" are absent from the ELF, SLUSOVF.BIG and
the IRX; the board curves below are hard-coded. CROWD.INF CROWDPATCH1-6 is also never read.

### 5.2 Board

- Surface class `291710` (rider+0x438; on rails the global `audio+0x598C` written at rail attach `106848`), jump
  table 0x482AA0: snow -1,0,5,6,12,14-17,19 -> 0; 2,3,13 -> 1; 1 -> 2; 4 -> 3; 10 -> 4; 9 -> 5; 7,8,18 -> 7;
  11 -> 8. Family `2934D0`: class 0 -> 0; 1-2 -> 1; 3 -> 2 on a rail else 0; 4-8 -> 2.
- Three loops per rider on **BOARD** (bus 6), started by `292A50` (from `2929D8` <- world start `286E20`),
  stopped by `292B48`; each per-frame callback sets volume and pitch and forces volume 0 in crash (motion 2),
  after the finish and under a pause/NIS gate (`270280`):
  - A `2917D0` (human only): SPUBOARD snd 0, callback `292CB8`: ground only; vol = curve 0x482AF8 of speed
    (x 0/.003/.027/.15/.2 -> 0/101/61/0/0), pitch = curve 0x482B20 (1.0..2.0).
  - B `291C88` glide: snd class+4 (human, `292DA0`) or class+6 (AI, `292EF8`), bank SPUBOARD if class <= 3 else
    BOARD; x = compression x (1 - (brake + turn)), x *= 10 x speed below speed 0.1, smoothed per player
    (`audio+0x59B8`); vol/pitch families: 0 = 0x482B78/0x482BA0, 1 = 0x482BC8/0x482BF0, 2 = 0x482C18/0x482C40.
  - C `292508` carve (human): snd class+3, callback `293018`; y = brake + turn smoothed (`audio+0x5990`, down 50%,
    up 12.5%); family 0 vol 0x482C68, family 1 vol 0x482CB8, family 2 vol 127; pitch from compression (0x482C90 /
    0x482CE0 / 0x482D08).
  - Inputs written by ground contact `13F178`: speed rider+0x75C = clamp(|v| / 3333.33, 0, 1), brake rider+0x750,
    turn rider+0x754, compression rider+0x758.
  - **The pitch curves are inaudible.** The loops are positional; the voice update `2AB6B0` runs the callback and
    then the 3D update `2AC868`, whose doppler bend (1 + v/8000, about 1.0) overwrites the callback's bend every
    frame. ARMSX2 (4902 ticks, speed 0-0.81): the pitch sent was always 4066-4112 (0x1000 = 1.0); at speed 0.108
    the curve asks 5112 and 4095 is sent. Volumes follow the curves.
- Take-off `294170` (ground `13F178`, air `12E9B8`, ollie `1307B8`, rail leave `13BFA8`): LAND snd class x 8 + 2,
  vol clamp(speed x 127, 64, 127); then `294678`: take-off speech or crowd anticipation (predicted air > 2 s).
- Landing `2948D0(rider, trickValue)` (from `10E910`, crash get-up `12D848`): crowd cheer first (5.4), then
  LAND snd class x 8 + 1 (human) or + 5 (AI), vol = curve 0x482D30 of the impact speed rider+0x770
  (100/300/800/1200/2000 -> 10/28/34/80/127).
- Rails: no grind sounds of their own; the loops restart with the rail surface class, entry = landing, exit =
  take-off.
- Air: `2910E0` whoosh (bank 0 snd 0x20, gain = `audio+0x5830`, see 3.6) on long jumps; `2913D8` stops it.

### 5.3 Gameplay one-shots (bank 0 zbxsfx unless noted; ARCADESFX = bus 9)

| Sound | Dispatcher <- game caller | Bus |
|---|---|---|
| boost loop 0x7A (+ one-shot 0x4A if a2), refcount `audio+0x59E4` | `298D90` <- `114130` | 9 |
| boost empty 0x6C / release stop | `299368` / `2992D8` <- `114130` | 9 |
| grab 0x77 | `29A530` <- grab lifecycle `1352A8` | 9 |
| Uber trigger 0x6C | `299B70` <- `132620` | 9 |
| handplant 0x61 | `29DC48` <- `1328B0` | 9 |
| points tick 0x4D | `29A7D8` <- score HUD `117FE0` | 9 |
| pending Uber 0x66 (stopped by `29B3C0`) | `29B0E0` <- `117FE0` | 9 |
| Tricky start 0x6B after 400 ms (+ arcade speech 0x20A8) / end 0x69 | `299638` / `2997B8` <- `117FE0` | 9 |
| combo commit 0x67 (+ speech 0x2133) | `29B430` <- `11A228` | 9 |
| Super Uber speech Arcade_Uber 2 (focus rider's tier becomes 10, audio+0x581C = last tier) | `29B738` (per frame, 285D40) | - |
| monster trick speech Arcade_Uber 8 (only when 11B1A8 > 0) | `29B7E0` <- `11A168` / `11A228` | - |
| boost-meter fill tick 0x65, pitch +0x100 per step from 0x1000 | `29AB40` (per frame) | 9 |
| pickups 0x74/0x71/0x75/0x76/0x70 by kind | `29CED8` <- `10E770`/`10E7D0`/`10E8B8`/`30B9A0` | 9 |
| overtake 0x6D | `299E28` <- ranking `10F998` | 9 |
| forced reset 0x7B | `29A220` <- `116120` | 5 |
| animation events 0x51/0x50/0x52 | `289B18` <- `104CC8` | 5 |
| countdown beep 0x4E, GO 0x5E (+ UI player snd 0x62) | `29C420` / `29C7B0` <- `234A30`/`234BE8` | 5 / 9 |

### 5.4 Collision, crash, crowd

| Dispatcher | Caller | Sound | Bus | Level |
|---|---|---|---|---|
| `296088` | instance contact `105398` | watrig.adl record via `2B5F60` | 7 | `290C10(0, |v|)` 33..127 |
| `296310` | crash entry `10EB30`, ragdoll `12D4E8` | 0x30 + rand(3); LAND 9 on surface 2/3 | 5 | 127 |
| `296868` / `296E80` | crash control `12CD20` (phase 0), `12D4E8` (phase 1), on entering the slide phase 2 | loops 0x33 / 0x34 by crash clip class (handles rider+0x774/+0x778; a running loop is not restarted) | 5 | vol curve 0x482DA0 (127); stopped (0.75 s, `296E20`/`297438`) by **both exits of phase 2** (`12D160`: -> 3 get-up, -> 1 air), reset, finish, pause (`29ED18`); ARMSX2: loops end 45 ticks after the get-up |
| `29F660` | crash, pair hits | GRNT grunt (bank 0xE rotating 3..5 / random 0..1; AI bank by character) | 4 | `290C40` |
| `298488` / `298138` / `2989A8` | rider pair: soft `10E228` / crash `10E2E8` -> 298488, soft attack `10E3A8` / crash attack `10E468` -> 298138 (107E70 picks by 108388 soft / 10EB30 crash and the attack flag); obstacle `13F488` | 0x36 / 0x5A / 0x36 | 5 | bump `290C10(1, \|v_a + v_b\|)` (the two velocities are added, `vadd`), others 127; repeat tags 7 / 5 / 6 |
| `29E970` / `29E590` | crash recovery meter `12CB68` | 0x60 / 0x5F | 9 | |

Crowd (`2A6D78` instances, `2A7040` CROWD.INF parse, MIDI .eam on bank slot 5, bus 5): landing cheer `2A73A8` by
trick value (pending points / (repeats+1), 1.0 = 10000 pts): < 0.25 Cheer10, < 0.65 Cheer20, else Cheer30; race
finish always Cheer30 (`286EA0`); fall `2961F0` (only 0 or -0.25 reach it, so only Ahh20 or nothing);
anticipation armed at take-off when predicted air > 2 s and graded < 0.25 / < 0.5 / else by the pending trick value
in the last 3 s (`290FD0`); ATTACK/DECAY/MAXBEND bend the pitch of the ambient crowd loops (world emitters, watrig
ids 41-43), not the .eam.

### 5.5 UI and world

- UI player `290CC0(audio, bank 0, snd)` (vtable `audio+0xC` +0x8), bus 5, not positional; dispatcher
  `294F78(audio, ev)` table 0x482D60: ev 0/9 -> 3 accept, 1/10 -> 4 back, 2/11 -> 2 move, 3/12 -> 1 scroll,
  4/13 -> 0xD error, 7 -> 0x10, 8 -> 0x11, 14 -> 0x12 results panel, 15 -> 7; `294F48` -> snd 0.
- World sounds: watrig.adl (`2B5A18`, 92 records: bank slot + sound, or a named bank loaded into slot 3) played by
  world-instance emitters (`2B7908` -> `2B5C68` -> `2B5D78` -> `2B4C38`, bus 5, volume = falloff x 127) and by
  contacts (`296088`, bus 7); stage-script sounds `2974A0` one-shot / `297950` loop / `297EB8` stop (id 1-99
  Mtn.bnk, 100-149 slot 8, 150-199 slot 9, >= 200 TRANSPORT; bus 5); location ambience `29D290` (bank 9 snd 0,
  faded over 5.03 s on location change); tWPIGD_Speech painter data exists but no query is ever created.

## 6. Browser implementation plan (biggest audible win first; done, see section 9)

1. **Mixer and volumes** (small, unblocks everything): 11 channels as in 2.1 with the original slider grouping
   (Music -> MUSIC, AMBIENT, DJ-when-enabled; Effects -> ch5, PA, BOARD, COLLISION, arcade; Speech -> CHARACTER),
   steps 0..11 linear; MIX.INF fades as linear ramps of TIME seconds (Web Audio `linearRampToValueAtTime` matches the
   60 Hz stepping closely); the duck layer (2.3) as a separate gain stage per channel.
2. **Music with the right song in the right place** (the main win): MUSIC.INF + PLAYLIST.INF parsing (3.1-3.2),
   `PickNextSong` with CATEGORY exactly as 3.2 (seeded RNG), radio modes 0-3 from the career save, and the screen
   flow of 3.7: charsel + FE events per screen, LoadingScreen.bnk loop with the 1 s fade, PlayMusic at world load
   (event 0 in Conquer the Mountain, 36 in single events), preload paused at countdown "1" and resume at GO,
   pause = freeze (not fade), event 10 at the finish, event 36 between Big Air/Pipe runs, hard stop on leaving the
   world, chartune on the podium. Stream gain = pathVol/127 x PathLevel/100 x MUSIC channel (clamped); LOWPASS as a
   fixed low-pass per song (mapping open, see 7). Until the Pathfinder graph player exists, a stand-in that maps
   each event to a start node of the MUS stream and loops is enough for correct song selection and timing.
3. **Pathfinder events**: once audio-formats.md exposes the MPF node/event tables, implement `SendEvent` with the
   MPF's own transition quantisation (the game code never quantises; it only sends event numbers, 3.5).
4. **DJ Atomika**: artist intro at GO / pre-race (SEDVALUE of the song), event intros and results lines (section 4),
   with the 65% / 1 s duck and 0.5 s release (2.3). Only when radio mode 0 or 1 and the DJ option is on.
5. **Big-air loops** (3.6): stream level ramp 127 -> 20 over the first half of any flight predicted longer than 4 s,
   loop overlay beat-locked to BPM, events 7/8, and the air whoosh. Needs the LOOPDATA .mus decoded.
6. **SFX** (section 5): UI sounds (bank 0 of SSX3Menu.bnk via the `294F78` table), countdown/GO beeps, board loops
   with the hard-coded curves (not SNOW.INF), take-off/landing from land.bnk, crash sounds and grunts, boost, trick
   HUD sounds, crowd cheers.
7. **Freeride extras**: Peak1/2/3 hub songs at stations, pktrans, MusicTrigger zones, the ABC1/BRA2 mix regions,
   BIG Mountain Ambience mode (Peak*Amb on AMBIENT with ambience-painter events).

## 7. Open questions and how to check them

Most can be answered by reading memory through ARMSX2/PINE (`tools/pcsx2_pine.py`, `tools/ps2_capture.py`) at known
moments instead of recording audio: audio = `*(0x4A3500)`; song id `*(audio+0x520)+0x58`; last event `audio+0x534`;
path level `*(audio+0x520)+0x40`; radio mode `audio+0x608C`; mix `audio+0x646C`; duck `audio+0x63D0`; playlist index
`audio+0x508`.

All answered (2026-09-23; ARMSX2 captures in the scratch notes of the audio pass, static RE for the rest):

1. **Pathfinder events**: section 8 (the graph semantics of every event number are in the MPF data; the game only
   sends numbers).
2. **LOWPASS**: no filter for every retail value (section 8).
3. **`288AE0`** = instant-replay playback (section 1). In the browser the results screen sets it (the run replays
   behind the results).
4. **Menu row -> radio mode**: Radio BIG 0, BIG Mountain Ambience 2, Custom Playlist [DJ] 1, Custom Playlist
   [No DJ] 3 (item values set at `196540`, passed by `196770` to `28BF78`); docs/audio-menus.md.
5. **Songs loop forever** unless an event (10, 18) sends them to an ending part (section 8).
6. **chartune**: the winner's CHARDB character (section 3.7).
7. **`0x535C10`**: 4 free ride, 5 time challenge, 6 points challenge (section 1).
8. **`3B7EB0`**: linear in the 0..127 domain, 10 ms steps (section 8).
9. **Bank slots 8/9**: per-location banks in the world data, loader `286CA8` (section 5.1).
Also settled: the board-loop pitch curves are overwritten by the doppler bend (5.2), the crash slide loops stop on
both exits of phase 2 (5.4), the big-air loops never engage on the three browser courses (longest predicted flights
3.96 s Snow Jam, 3.16 s Metro-City, threshold 3.9995 s; with the threshold poked to 2.5 s the path ran exactly as
3.6 says), and the air whoosh follows 127 x `audio+0x5830`, starting 12-13 ticks after take-off.

## 8. Pathfinder runtime (recovered; port: `web/pathfinder.js`)

Answers open questions 1, 2, 5 and 8 above and corrects two details of 3.4/3.6. Sources: the EE code
`3CFC30..3D6000` (`local/output/sub_003D*.cpp`) and an ARMSX2 run of `local/reference/pcsx2/snow-jam-glide.p2s`
(Avalanche) polling slot 0 over PINE. Field layout of the `.mpf` records: audio-formats.md section 11.

**Slots and voices.** Four Pathfinder slots at `0x517610 + 0x928*i` (+0x30 id, +0x34 mpf, +0x38.. table pointers,
+0xBC 16 s16 registers, +0xDC 16 pending event copies, +0x91C their count; zeroed by the loader `3D2350`, id =
`0x1000000 << mpf[0xC] | 0x10000000 << instance`, so the game's `0x11000001`/`0x11000002` address track 0/1 of the
first slot). One voice per track at `slot+0x58[track]` (0x110 bytes, `3D25F0`): +1 branch value ("intensity"), +2
previous value, +4 level % (default 100, `-0xBD2`), +0x20 bits 0-3 loop counter / 4-8 slice / 14-18 queue size /
19-23 write index, +0x24 latency ms, +0x2C loop node, +0x2F sync flag (latency < 50), +0x30 current part head,
+0x34 volume scale (127), +0x36 last committed node, +0x48/+0x10C next slice / pending start time, +0x54 EA stream,
+0xFC queue ring. `2B21E0` creates track 0 on MUSDATA with latency 500, 2 queue entries, and track 1 on LOOPDATA
with latency 10, 1 entry (so track 1 is beat-synced). `3D2DF8` (from `2B4620`) attaches the resident loop bank that
`2B43D8` loaded to track 1.

**Clock and service.** Time is ms since boot (`2AF370`). `3D5128` runs from two callbacks registered by
`2AE048(3D54C8, 3D54E8)`: reason 0x20 (margin slot+0xC = 50 ms, processes events first, `3D1E80`) and 0x42 (margin
slot+0x10 = 10 ms, voices only). SendEvent only queues; the next tick executes.

**Commit rule (`3D4F10`).** A voice decides and queues its next node when the audio still queued for it (`3D4950` =
sum of the queue entries' remaining ms) is <= its latency and the EA stream has room (`2AEBD0`). The stream is
therefore always exactly one bar ahead: on ARMSX2 every Avalanche bar (1818 ms) was followed by the commit of the
next one 420-490 ms before its end; the IOP plays the queued files back to back (gapless).

**Next node (`3D3D20` -> `3D37A0` -> `3D3900` -> `3D41A8`).**
- `3D37A0` picks a branch with value v: the first branch with lo <= v <= hi (inclusive), else the branch with the
  nearest bound; 0xFFFF or no branch = stop. v is the voice intensity byte (+1), not random. The game sets it for
  every voice with `3D0D70` (`2B2120`/`2B3C28`): 127 for hub, podium, map and FE songs; in races `28F000` (every
  frame while state 6) writes trunc(min(counter/N, 1) x 127), counter reset to 0 at PlayMusic, falls (`2961F0`) and
  forced resets (`29A220`) via `28F108`, N = 800/1000/1200 frames by the tier `289C18` returns. The loader leaves it
  0. So Go's "0-50 / 50-100 / 100-127" three-way parts are low/mid/high-intensity variants, and the music builds
  over the first ~13 s of a run and drops back after a crash. Verified on ARMSX2: after GO the committed bars were
  36 -> 5 -> 6 -> 7 -> 24 -> 25 -> 26 -> 43 -> 44 while the byte rose 0, 17, 35, 52, 70, 87, 104, 121.
- If v's node is the voice's loop node (+0x2C), v = counter instead and, if the node's flags>>12 is 1..7, the counter
  counts down; it is loaded from flags>>12 when an end node (sample -1) is first reached. flags 0x6040 = 7 passes
  then branch "0-0"; 0xF (and 12, 13) never count down, so the part loops until an event moves the voice.
- `3D3900` walks control nodes until an audio node: sample 0 = part head (sets +0x30; w8 bits 10-14 would post an
  event, unused; w4 bits 17-19 == 1 makes this node's branch value random = (now_ms / 23) & 0x7F, the only random
  source); sample -1 = part end (loop counter); -2 = random; -3 = post event (w12>>2)&0xFFF (neither occurs). Routers
  (`3D3C90`) remap targets by the previous node; no song has routers.
- `3D41A8` queues the node's sample (stream offset value x 0x80, or bank entry). A target on another track is handed
  to that track's voice by a synthetic event (`3D4050`, unused by songs).

**End of song.** A branch to 0xFFFF (Go's ending part 379 -> 381, 382 -> end node 380 without branches) stops the
voice; the queued bars drain and `3D11B8` then reports -1 (question 5: songs loop forever unless an event, e.g. 10
or 18, sends them to an ending part).

**Events (`3D16F0`, `3D2068`, `3D1D28`, `3CFC30`).** The event number is the index into the event table (>= count:
ignored); the record (head + actions) is copied into the slot queue (max 16) with its post time. Every tick each
pending event runs its actions in order; an action that is not complete blocks the rest of that event (other events
continue). Action = 16 bytes: byte 0-2 track mask, +4 timeout, byte 9 opcode (jump table 0x495A30), +0xC operand.
- 0x02 wait: complete when now - (time the previous action completed) >= u32 operand ms.
- 0x04 jump: s16 value = node; -16..-31 = register (-value)&15; byte 14 (s8) = section: -1 absolute node, >= 0 the
  value-th part head (sample 0) of that section, -32 current section; byte 15 bit 0 = flush (`2AF838`: stop what
  is queued and playing on that track first). The target is resolved (`3D3900`) and queued at once (`3D41A8`),
  i.e. appended after the audio already queued: an event that arrives more than ~500 ms before the end of the
  current bar plays its target right after that bar, later ones after the following bar. Value 0 with section 0 (no
  part) = stop; Go's events 44/46/47 point past the node table and also stop. On the sync track the jump waits for
  the master's next beat (3D41A8 returns -9999, the action is retried every tick and re-rolls the random pick).
- 0x06 ramp: +0x34 = target (s8 byte 12), 3D5A98 moves the stream volume from its current value to it over u16
  byte 14 ms, curve byte 13 & 0x7F: 1 linear, 2 quadratic (1 - t'^2), 3 hyperbolic (1/(25 t')), t' = t falling /
  1 - t rising; 0 = no ramp.
- 0x0A save register: byte 14 = register, value -33 = the voice's last committed node (+0x36), -32 = section.
- 0x05 pause voice, 0x09 set +0x34, 0x0C post another event, 0x0D-0x0F cancel events, 0x01/0x03 sync waits,
  0x07/0x08/0x0B/0x10 stream parameters: none occur in retail data (only 0x02, 0x04, 0x06, 0x0A).
- Typical data: event 0 = jump to the start part, then save registers 1-5; 1/3/5/36/40 save a register and jump to
  an interlude (36 = the endless idle part); 2/4/6/37/39/41 jump back to the saved bar (-17..-21); 7 = track-1 jump
  to the random loop head, 8 = track-1 stop with flush; 10/11/18 = ending part; 19 = fade 3 s, wait 3.1 s, stop;
  33/34/38 (challenges) = save, cut the stream, track-1 long ending (the challenge stinger, 3.9), wait 1200 ms, jump to a
  section-3 part.
- 2B3BC0 drops a SendEvent equal to the previous one; the first event of PlaySong goes straight to `2B20C8`, so the
  same number sent right after start (GO's event 0) is executed again.

**Loop overlay (track 1).** The game path is: take-off `2B4620` (attach bank) + event 7, landing event 8 +
`2B4708` (clears song+0x4C only). The game-side slice scheduler `2B4568`/`2B4740`/`28C430` described in 3.6 is
dead code: its only entry `2B4470` has no caller and no pointer to it exists. Event 7 jumps track 1 to a random
head (Go 511: 0-43 / 43-90 / 90-127 -> three 8-bar slice chains). Their group heads have sync mode 0x100 and the
audio nodes w12 bit 1 ("sliced"): `3D3658` computes the slice as (master beat index + 1) mod beats-per-measure + 1
for the master's next beat, `3D41A8` sets +0x10C to that beat (rounded down to 10 ms) and, once reached, queues
one beat (dur / (beats x measures)) of bank entry value + slice - 1 and sets +0x48 = the following master beat;
the voice stays on the node until the slice passes beats x measures, then advances (successor re-synced the same
way). The master is the playing voice with the largest latency (track 0, `3D4B00`). Nodes under a sync-0 head
(the long endings 539/540) are queued whole, at once. Slices 2..n are queued when now + 10 + margin reaches +0x48,
so they may start up to ~20-60 ms before the beat on hardware (not reproduced; the port starts every slice on the
beat).

**Levels.** `2AF6C0` (Pathfinder volume writes: `3D62C8`, ramp `3D5A98`) does nothing unless audio+0x6280 is set,
which `28CF98` only does for the BIG Mountain ambience (radio mode 2). In the music modes: the stream level is
`2B21E0`'s per-frame write, clamp(pathVol x 0.01 x PathLevel x channel, 0, 127) through `3B7E70` (song+0x54 is
always 1); loops.setLevel (`2B3D10` -> `3D5508`, clamped 0..100 %) only changes voice+4, which each slice latches
at its start (`3D41A8`: % x 0.01 x +0x34); op 0x06 ramps have no audible effect. In ambience mode ramps and level
changes apply continuously. The EE mixer (`3C2A50`) scales by gain/127 (linear amplitude). `3B7EB0` fades in 10 ms
steps linearly in the 0..127 domain (16.16 step per tick, `3B8530`).

**LOWPASS.** `3B80A0` -> `3B8978` -> `3C3178`: normalised cutoff = value / (mixer rate / 2), mixer rate = 36000
(`0x50A8E8+0x2A`, same in every savestate). `3C8D00` removes the filter when the ratio is >= 1, so every retail
value (30000-42000, 0xFFFF) means no filter (question 2). Values < 18000 would enable an EE filter module
(`3CA8A0`, response not recovered).

**Port (`web/pathfinder.js`).** `createPathfinderCore` reproduces the above as a timed state machine (song-clock ms,
segment times from the exact sample counts); `createPathfinderPlayer` schedules AudioBufferSourceNodes from the
committed queue, simulates up to 1 s ahead and, when an input (event, intensity) arrives, rewinds to the last
real-time snapshot and re-simulates with the same random draws, so inputs take effect exactly at their arrival time.
Pause freezes the song clock (event waits and ramps included; on the PS2 only the stream position freezes).
Tests: `web/test-pathfinder.mjs` (`--render DIR` writes WAVs).

## 9. Browser implementation (2026-09-23)

Everything in sections 2-5 now runs in the browser. Files (all under `web/`):

| File | Role |
|---|---|
| `audio-engine.js` | 11-channel mixer, MIX.INF, duck layer, sliders; `busGain(bus)` = slider x mix target (the "bus volume != 0" gates, 287968). |
| `game-audio.js` | Director: music flow (3.7), radio modes 0-3, playlist, podium chartune, restart rules, MusicTrigger / Mix / Ambience painters, speech OnIdle flags, settings API for the audio menus, UI sounds, per-tick / per-frame entry points. |
| `sfx.js` | Voice layer (2906B8 / 2AC868 / 3BB588): every layer of an entry, patch tags, envelopes, LFOs, 3D gain and pan, doppler bend overwrite, fades, pause, voice warm-up. |
| `sfx-game.js` | The 29xxxx dispatchers: board loops, take-off / landing, crash, grunts, boost, HUD sounds, countdown / GO, overtakes, rider pairs, big air (3.6), anticipation / crowd reactions. |
| `audio-speech.js`, `audio-speech-events.js` | The one speech voice and the EA speech-event engine (Events.evt interpreter, 16-slot scheduler, 10-slot request table), every category function (4.3-4.5). |
| `audio-crowd.js` | CROWD.INF reactions (MIDx .eam on Crowd.bnk), crowd loops fed by emitters, the ATTACK/DECAY/MAXBEND bend swell. |
| `audio-world.js` | World emitters (watrig.adl), instance contacts, location ambience, named banks (slot 3), stage-script sounds, thunder. |
| `audio-painters.js` | tWPIGD MusicTrigger / Mix / Ambience point-tree query (2C1CD8, EE float), stateless and stateful (2C0778). |
| `audio_events.hpp` / `audio_events.inc` | Core observers: the compact event queue and per-tick telemetry. |

Assets (git-ignored, `web/public/assets/AUDIO/`): `tools/export_audio.py` (songs, speech, banks),
`tools/export_speech_events.py` (`speech/Events.evt`, `speech/registry.json` = every .hdr in registration order),
`tools/export_world_audio.py` (`world/<EVENT>.json` for the 22 courses and hubs: emitters, contacts, painters,
watrig; `banks/<LOC>_slot8/9.bnk`), `tools/export_animation_audio.py` (`anim-events.json`). All four run in
`npm run setup`.

### 9.1 Core event queue (bit-exact physics untouched)

The game-side call sites post `{type, a, b, c, d}` into a 64-entry queue per core (`audio_events()` /
`audio_events_clear()`); `audio_telemetry()` returns 24 floats (speed 0x75C, turn 0x754, brake 0x750, compression
0x758, motion mode, surface 0x438, rail surface 0x598C, predictor state / T / landing time / elapsed, impact 0x770,
position and velocity (source cm), human, control state, crash, dropped events, Tricky time +0x2F0, boost tier
+0x2F4); `audio_instance_position(resource)` gives a stage instance's translation. Observers only: nothing writes
rider state or draws RNG (the capture gates stay exact). Events and their original sites: `audio_events.hpp`
(take-off 294170 sites, landing 10E910/12D848, crash 10EB30, slide loops 12CD20/12D4E8, slide exit 12D160, crash
grunt, crash exit 12E690, recovery meter 12CB68, boost 114130 (macro wrapper on originalBoostControl), fill reset
10E098, grab 1352A8, uber denied 132620, handplant 1328B0, combo 11A228, named trick 11A168, trick speech 115B58,
whooh 130DD0, soft collision 108388, obstacle 13F488, instance contact 105398, reset 116120, pickups 29CED8,
animation events 103AA0 (every newly latched sequence event with the clip id; `anim-events.json` keeps the ids,
0x8050/0x8051/0x8052 play 0x51/0x50/0x52), rider pairs 107E70, overtakes 10F998, stage-script sounds (builtins 30,
31, 73)). `game-audio.gameTick` drains the human core and every computer rider's core after each 60 Hz tick.

### 9.2 Voices (`sfx.js`)

- Every layer (0xFE-separated patch) of a bank entry starts together. Tag values are signed big-endian, table tags
  self-relative (parser `parsePatchTags` keeps the value addresses).
- Per layer: patch volume = clamp(0x0E +- rnd(0x0F)) x velocity / 127; cents = 0x10 + (note - 0x07) x 100 +-
  rnd(0x11) + wheel (0x0A range); pan = 0x0C +- rnd(0x0D) mapped with the 0x44C590 sine law and added to the 3D
  azimuth (equal-power StereoPannerNode); envelope 0x19 ({ticks, level} x 0x09 segments at 100 Hz, start level 0x1C,
  hold segment 0x7FFFFFFF, hard stop after the last segment); volume LFO 0x1D/0x1E and pitch LFO 0x20/0x21/0x22
  (random start 0x23) as looped 100 Hz step buffers into gain / detune.
- Game volume (2AC868, per frame): positional voices trunc(base x ((V - max(d - 0.5, 0)) / V)^2), V = 30 m (300 for
  stage-script sounds, 2A9988), d from the camera view matrix with the listener 1 m ahead and 25 cm off
  (285930); changes <= 4 are not sent. Positional voices get bend 0x1000 after their callback (the doppler write),
  so the board-loop pitch curves are inaudible, as on hardware.
- Stops: 250 ms linear fade (2ABB38), `stop(s)` = 2AD5F0 fades. Pause (29CE28 -> 2AD2A8 -> 2ACAC8) gives every
  active, non-fading voice bend 0 and volume 0 (no frozen DC), except UI sounds; resume restores both.
- Hardware voice pools (3BA0B0): SPU 48 (PS-ADPCM), IOP-mixed 8 (MicroTalk, EA-XA, PCM8), one per channel of each
  layer; free voice first, else steal the lowest priority (tag 0x06) <= min(new, 100), oldest on ties, hard stop; a
  layer without voices fails the whole request. Delayed requests (2ADCA0) allocate when the timer fires.
- Master gain 115/127, default sliders 10/11, the 288D18 channel and per-speaker scales (2.1).
- SPU voice interpolation (`web/spu-interp.js`): PS-ADPCM patches (hardware voices) are pre-resampled to 48 kHz with
  the SPU2 4-tap Gaussian table (512 entries; out = sum of (g x s) >> 15 over s[k-1..k+2], i = 8-bit fraction), so
  Web Audio's linear interpolation does not keep the extra 4-11 kHz energy. The IOP-mixed voices (MicroTalk, EA-XA,
  PCM8) and the music streams keep the browser resampler (the IOP mixer's filter is not recovered; the music spectrum
  matches the PS2 within 2 dB up to 11 kHz).
- Level check against the ARMSX2 output recordings (Snow Jam, Avalanche forced with `?audioSong=Avalanche`, GO-aligned,
  `scratchpad/perf/group-levels.mjs`): Chrome / Safari full mix -12.5 / -12.9 dBFS vs PS2 -13.1; music -15.1 / -14.9 vs
  -15.6; rider speech -22.4 .. -31.8 (content dependent) vs -30.8 overall; GO sound window (effects without PA, 0.5 s)
  0.30 / 0.31 vs PS2 0.28 RMS; GO-window band energies within 0.3 dB from 0 to 8 kHz. The only remaining burst excess
  at GO is the PA venue intro, which the browser starts 2.7 s before GO because its race goes from PreRace to the
  countdown at once, while the PS2 waits for Cross on the pre-race overlay (race flow, not audio). Sample jumps > 0.4
  per 20 s: browser full 32-38 (Safari master tap 1-61 by run) vs PS2 full 55 (music 11, effects 5), so no extra
  clicks; voice starts are sample-exact on both (no SPU ramp needed). `warm(slots)` decodes every patch of the world banks in 4 ms slices during the loading screen.
- Sound Mode: mono (audio+0x62B0) removes the azimuth.

### 9.3 Speech (`audio-speech.js`)

One voice; `update()` runs per 60 Hz frame (2B0C78 -> 2AFC88): busy -> 1; free (idle or < 500 ms left) after busy
-> OnIdle (game-audio: one pending DJ flag, in the 2A43B8 order) then the scheduler resolves the best pending event
(highest priority, newest; older ones flushed unless flag 0x04; W expiry in 60 Hz ticks); the next free frame streams
every resolved line back to back, only while its request is still in the 10-slot table (180 frames). Lines route by
bank name (DJ / PA / ARCADESPEECH / CHARACTER); a DJ line ducks the mode-0 group to 65 % over 1 s and the duck is
released 0.5 s after the stream ends. Silences are the `*_Silence_*` banks. Category functions and their argument
recipes are the ones in 4.3-4.5 and the evt notes: rider speech (Big_Air, Trick easy/difficult, Wipeout, Pass /
Passed_by friend/foe with the 670.56..1788.16 cm/s window and fast flag, Hit / Aggression from 10E3A8 only,
Post_Object_Collision, Whooh, Finish_Line, Hey, BC_Challenge, Post_Selection / Customize from the front end with
refreshDup: web/rider-speech.js names `Post_Selection_<abbr>` / `Customize_<abbr>`, and `frontEndCue` turns that into the
event for the rider's CHARDB id (Events.evt then picks the bank: a 250 ms Arcade silence on ARCADESPEECH, then the
rider line on CHARACTER at the FE scale 0.6 with per-speaker 1.0)), arcade (Power_Ups, Prompts incl. Tricky and time up, Uber, Icons), DJ (Radio_Big_Intro, Artist_Intro by
SEDVALUE, Event_Intro, Radio_Big_Outro, hub chatter 2A2E50), PA (Venue_Intro at world load, Sponsor_Intro on the
career objectives panel (1FB588), Medal_Run_Intro at GO in a career final round, Finish_Line / Rider_Postition at
the finish, id 77 emitter zones). Nate / Viggo: the event file's 0x40/0x80 swap is reproduced by feeding the masks
as the game computes them. Friend / foe for pass and hit lines: `2A1820` on the live relationship records (`web/ai-racers.js`
relation(a, b), kept live through the 155BF0 contact events by web/ai-race.js): hit lines read them before the
contact's own update (`onPairAudio` hook), pass lines as they are at the tick-start ranking; the pass gate counts 600
ticks from GO. Medal-run PA: Conquer the Mountain, round 3 of a race / freestyle event (not the rival challenges). `288AE0` is set while the results screen (replay) is up.

### 9.4 Gameplay dispatch (`sfx-game.js`) and crowd

As 5.2-5.4 with these port notes: board loops A/B/C per rider (B for computer riders), class restart, class 9 in the
air; take-off sound only for the human (listener rider), landings for every rider in range; the 2947B0 retry when the
predictor is not in flight yet; crash sound / grunt / Ahh by |penalty| and the intensity reset; slide loops started on
entering phase 2 and faded on leaving it (core event on every phase-2 exit); pause stops the human's slide loops,
the pending-Uber sound and the boost loop (29ED18); HUD sounds from the score HUD slots (pending-points tick 0x4D on
each hundred, slot 10 pending Uber 0x66, slot 9 Tricky 0x6B + prompt / end 0x69, fill ticks 0x65 with the rising
bend, Tricky countdown 0x68); countdown 0x4E on each digit, GO 0x5E + UI 0x62; TIME'S UP arcade prompt 2; big air
exactly as 3.6. Crowd: emitters with watrig bank 5 register loops per patch group (loudness = max + sum(others)
>> 2, weighted centroid), trick / fall reactions play the .eam note-ons (10 ms ticks) on Crowd.bnk with all layers,
anticipation levels from 290FD0, the loop bend swell per 2A68B0.

### 9.5 World sounds and painters

`audio-world.js`: resident locations of the event (the course plus its connectors); emitters per frame at the camera
(2B7908 shapes and falloffs, volume x 127, bus 5, 0.25 s fade when out of range); contacts (list[node] -> watrig
type 1, bus 7, curve 290C10(0), per-(instance, node) repeat block); location ambience bank 9 #0 on the focus
rider's contact track (5.03 s fade on connectors); named banks one at a time in slot 3; script sounds with the id
routing, the per-instance loop table, the repeat block and the Snow Jam gate on id 102 (144BC0 location word 0).
Painters per tick at the human's position, region = track of the last contacted patch: Mix -> SetMix (Metro-City
mix 2, Happiness mix 1, 3 s fades), MusicTrigger (radio modes 0/1/3) -> the 28D988 state machine (zones 1/2,
B = 0, 9, 11 with the 2 s request, 13, 14-17 event-intro flags, 18 ChangeSong in free ride, 42 forced), Ambience
(radio mode 2) -> Peak1Amb / Peak2Amb / SetEvent.

### 9.6 Verification

- `ONLY=event-race,boardpress-rail,metro-event-race,event-race-ai node web/test-ps2-captures.mjs` exact,
  `node web/test-ai-racers.mjs` unchanged, `cd web && npm test` (includes `test-audio-sfx.mjs`: curves, tables,
  dispatchers with a fake voice layer, patch tags, Events.evt resolutions incl. the Nate/Viggo swap, Cheer30,
  CROWD.INF, Metro-City mix 2 / Happiness mix 1, animation event ids).
- Chrome (Playwright, headed so audio runs): Snow Jam race with computer riders; every dispatcher category seen
  (board loops, landings, crash + slide loops + grunts, contacts, crowd loops + Ahh, script sounds, whoosh, GO, points
  tick, boost denied, recovery meter, overtake 0x6D), speech PA venue intro -> Radio BIG intro -> artist intro ->
  Pass_Foe -> Arcade_Uber; podium chartune at Moby's event 10; `gameTick` 0.12 ms average, 1.3 ms max after warm-up.
- Safari (safaridriver, real key gestures): context running, music, speech and SFX voices, no errors.

### 9.7 Not reachable in the browser (implemented where it can be)

- Hubs, free ride, peak runs: in since 2026-09-24 (9.9). Still missing: the post-event commentary (needs CTM events
  returning into the world), BC_Intro / pktrans of Peak 2 / 3 (not in the port), the in-world transport (the browser
  reloads the page; `travel` / `arrived` are ready for it).
- Rival modes: now in (docs/backcountry.md): 2872A8 at the rolling start queues the rival's BC_Challenge (0x5788, rival slot 1
  to the human) and the artist intro (`game-audio.js rivalStart`); the finish 2A4078 plays PA_Finish_Line for Rival Time
  (kind 5) and PA_Rider_Position for Rival Points (kind 6, round 1). Finish_Line_<char> / Hey / BC_Challenge by speech id come
  from 123E30 (flag 0x10 contacts, list from 27C9B0; id < 100 -> 2A1B58: 2 = Finish_Line of the contacting rider; ids
  100/101 -> 2A1B88: BC_Challenge / Hey with a character from 279F18); that dispatch is not wired in the browser yet.
- Thunder and avalanche objects: the port has no lightning / avalanche entities (`audio-world.thunder()` is ready).
- Arcade_Bonus 0x20A9: only from 0x1194C0, the slope-style checkpoint trick bonus. 0x112FB0 calls 0x10E558 when
  the rider's progress (+0x4D4) passes an entry of the checkpoint list 0x4D33B8; 0x10E558 copies the place (+0xE8 =
  +0xEC) and, unless bit 9 of 0x5308D0 is set, only when 0x535C12 == 1 (slope style) calls 0x1194C0: if the rider's
  +0x6C0 vfunc +0x44 and 0x238510(G+0xC0, rider id, checkpoint) pass, 0x117B88(score, kind 0x29, x2.5, checkpoint)
  and, for a human rider, 0x2A3CE8(mask 1). No slope-style course is in the browser (ARA1, BRA2 race; BHP1 pipe),
  so it cannot occur yet; porting it belongs to the score/physics side (then post an audio event for the speech).
- Split screen: one listener.

### 9.8 Uber / Tricky / monster dispatch verified against PS2 call logs (2026-09-23)

`tools/ps2_audio_log.py` + `web/uber-audio-compare.mjs` (docs/tricks-scoring.md): every bank-0 voice start, arcade speech
call and pending-Uber stop matches per tick on nine BHP1 captures. The per-frame dispatchers 29AB40 (fill ticks 0x65,
Tricky countdown 0x68), 29B738 (Super Uber speech) and 28C8C8 (air whoosh) run in 285BF8 before the frame's game tick, so
the browser feeds them the previous tick's HUD bank / telemetry.

### 9.9 Free ride / Peak 1 (2026-09-24)

The streamed Peak 1 world (`?course=PEAK1`, docs/peak-mountain.md) runs the director of sections 3.7, 3.8 and 4.3 in
`game-audio.js` (the SSXAudioSystem fields are named in `dir`); the timer queue `2ADCA0` / `2ADDE0` is a real queue with
cancel keys (`request` 28E088, `dj` 28E548, `retry` 28E068) pumped every frame. `debug().director` shows the game state,
pending OnIdle flags, queued timers and a trace of every director action.

- **Entry points.** `runStart({courseIndex, freeRide: {kind, mode}, courseCode: 'PEAK1'})` = the Conquer-the-Mountain
  world load (2867E8: PickNextSong + PlayMusic(0); 579C[23] = first-visit flags from 145D38 = !(P+0xACC bit loc)) plus
  2A4A78 (not after a backcountry intro): at a hub `latch(13)` and DJ kind 2 after 2 ms (Radio BIG intro (0,1) + hub
  chatter), elsewhere DJ kind 4 with the playing song's SEDVALUE (Radio BIG intro (0,0), artist intro, Free_Ride_Intro on
  the first visit of a Peak 1 race / slope location); then the ride start (234F40 + 28E888 -> `28E8C0(19, 1)`). The load
  notification without `freeRide` (main.js, courseIndex 0) only prepares the peak world. `go()` is ignored in these
  worlds (no countdown); kinds 5 / 6 get the 2872A8 rolling start when the race phase reaches 5 (Radio BIG intro +
  artist); `finish` uses PA_Finish_Line (kind 5) / PA_Rider_Position (kind 6).
- **PlayMusic 28CF98**: radio mode 2 -> Peak<n>Amb (ids 101-103, AMBIENT); course >= 17 outside the challenges (mode
  6-11, 2A42C8), not forced -> the peak's hub song Peak1/2/3 (ids 1-3, event 12, intensity 127, kept by 28F000);
  else the current playlist index (event, or 36 with t0 at a station). 28D8A0: modes 6-8 Race, 9-11 SlopeStyle.
- **Location crossing** (22DF50 via `freeRideCourse(course)`): 0x535C08 changes, nothing else. PS2 check (savestates
  `menus/fr/sj-01..22`, audio object read directly): across Snow Jam -> Blue Base Station +0x6254 and +0x5814 stay 0
  (28E8C0(20) would set them); the song is still 'Way' at the crossing (sj-15/16, screen 11); at sj-17 the request
  kind 0 has played **Peak1** (latch 11 = ARA1_B's MusicTrigger 11), Text_Message is pending with variant 2 (DJ kind
  0: Radio BIG intro (1,0) + First_Spoke + Text_Message, +0x5790 = 0 after the new-career intro); sj-19 hub chatter
  (+0x5750, B's MusicTrigger 13) pending; sj-20 +0x5790 = 1 (Char_Stories played first). So the station song and the
  spoke DJ are painter-driven:
  - MusicTrigger **11** (the connectors into a hub: ABC1_A, DRA4_A, ARA1_B): rearm + latch 11 -> event 11, request kind 0
    after 2000 ms (forced when the DJ is on, +0x62B8), DJ kind 0 (+0x5790 == 0) / 1 at 1500 ms when (a, b) changed; in a
    time / points challenge ChangeSong instead (28D988 jumps into the B = 18 code with latch 11).
  - **13** (hubs): latch -> event 13; hub chatter 2 (+0x5750) when changed, not in a challenge. **14-17**: event intros.
  - **18** (courses, free ride only, 2A4040): ChangeSong 28DF18 = FadeOut(2 s) without DJ, event 18, PickNextSong,
    request kind 1 (forced) at 2000 ms, DJ kind 3 with the *new* song's SEDVALUE at 1500 ms (Radio BIG intro (1,0),
    Free_Ride_Intro on a first visit, artist intro); a busy DJ line retries every 100 ms.
  - Ambience (radio mode 2): 31 / 32 -> PlayMusic with Peak1Amb / Peak2Amb unless playing, 33 nothing, else the event.
- **DJ timer 28E548** kinds 0 (spoke), 1 / 2 (hub), 3 / 4 / 5 (song change, free-ride start first / later; nothing
  when SEDVALUE is -1), 7 (backcountry); 146008 (P+0x278 bit 12) is the Peak 2 lock: set until the Peak 2 pass (corrected 2026-09-29, see 9.14; the port treats it as 1 unless pv djVisited).
- **Speech OnIdle 2A43B8**: Radio_Big_Intro (0x5768), BC_Intro (0x5784, [peak]), First_Spoke (0x576C, [peak]),
  Text_Message (0x577C, [+0x5780], then 1), Free_Ride_Intro (0x5770, [2A1BD8 course bit]), BC_Challenge (0x5788),
  Artist_Intro (0x5774), hub chatter 0x574C / 0x5750 (pool 8) / 0x5754 (pool 5), Event_Intro, Radio_Big_Outro (falls
  through to) Finish_Line. Hub chatter 2A2E50: Char_Stories first only when +0x5790 == 0 and not just arrived
  (+0x6258), then Peak_Boss forced once; otherwise a random unused category (used mask +0x573C).
- **New career on Happiness** (`&peakCourse=14`, first visit): 234F40 sets +0x578C / +0x6254, +0x5790 = 0; the plane
  intro plays pktrans (28CD48: event 1); 28E8C0(19) follows when the plane cinematic ends: on Peak 1 latch 11, the Peak1
  hub song after 3 s (request kind 0, forced) and DJ kind 0 at 2.5 s. **Corrected 2026-09-25 (9.11):** pktrans starts
  before the world load, so 2867E8 picks no song and 2A4A78 posts no DJ timer (the old "world-load DJ kind 4" reading was
  wrong: 579C[14] = 0 comes from the visited mask); the browser now holds 28E8C0(19) until the plane cutscene ends.
- **Radio mode change 28BF78** in a world: 2 -> PlayMusic (the ambience) unless it plays; 0 -> PickNextSong + PlayMusic
  when leaving the ambience; 1 / 3 -> the same when the song is not in the custom list or is charsel (hub songs stay).
- **Travel** (`travel(dest)` = 28F558 + 28E8C0(20); `arrived()` = 28E888 / 28E8C0(19)): implemented for an in-world
  transport; the browser's transport reloads the page, which is the world-load path above.
- **World sounds.** `tools/export_world_audio.py` also writes `world/PEAK1.json` (and PEAK2 / PEAK3): every location
  of the peak (16 for Peak 1). `audio-world.js setResident(tracks)` gates emitters, contacts and the painter region
  by the streaming rows (`_peak_world_rows`, states 1, 2, 5, 7; read every tick in `gameTick`); a course / hub whose
  data arrives loads its kind-20 banks into slots 8 / 9 (286CA8), and the location ambience (bank 9 #0) starts once
  that bank is in.
- **First visits.** 579C comes from the profile's visited mask: the UI can pass `context().visited` (mask) and
  `context().markVisited(loc)`; without them the page session keeps its own mask (every location unvisited at load).
- Tests: `web/test-game-audio.mjs` (station start, crossing, ChangeSong + Free_Ride_Intro, OnIdle order, hub approach,
  the Happiness intro sequence of the PS2 savestates, peak-run song change at a hub, radio mode 2, PEAK1 residency
  and banks).

### 9.10 Cutscenes (NIS) and the in-world event start (2026-09-25)

PS2 ground truth: session scratchpad `ctm/audio/NIS-AUDIO.md` (every 0x2906B8 voice start and 0x2B1458 speech request with
the NIS script time, bus from the gain pointer) and `ctm/caps/CAPTURES-ctm.md` section 4 (a recording of the CTM intro).
- NIS sounds (kind-7 channel 3 at 0x280F3C, actor sounds at 0x12584C) go to the **CHARACTER bus at speaker 0's gain**
  (287968(audio, 4, 0)), volume 127, from the NIS bank slots 17 + group (17 scdat_main, 18 the location's scdat_<LOC>, 19-26
  per-script groups). #94: slot 18 sound 0 at t0 (3.24 s, ends on its own); #60: slot 19 sound 0 at t240, positioned on the
  winner; #146: slot 19 sounds 1 and 0, sound 0 hard-stopped at the range ends. A Single Event start-hut intro (#89/#96 then
  #73) has **no** CHARACTER sound on the PS2 either: what plays is the location ambience (bank 9), the crowd loops (bank 5),
  two stage-script loops (bank 4 #17) and an ARCADESFX one-shot at #89 t10.
- Browser (web/cutscenes.js): the step's clock starts once its bank is loaded (the requests used to come before the bank and
  the failure was latched, so the CHARACTER bus stayed at 0); actor sounds at the actor root in PS2 cm; failed-for-now
  requests retry. The world sounds tick under a cutscene when the game ticks do not (`gameAudio.cutscene(on)`,
  `nisWorldTick`: the course location's ambience, emitters and crowd loops).
- In-world event start ([ctm-flow.md](ctm-flow.md)): no world load, so no Radio_Big_Intro / Free_Ride_Intro / venue PA from
  286E20; the fly-over's code picks the song that plays on through the browser's course switch (`carryWorld`: leaveWorld keeps
  the music and its request timer, no load-screen loop, the next worldLoaded starts no song; cleared at runStart).
- Measured per bus (Chrome, peak per second; scratchpad `ctm/chrome-*.txt`): CTM fly-over CHARACTER 0.20-0.35, PA at t30,
  approach PA at t0, card PA; Single Event intro UI bus (ambience + crowd) 0.12-0.42, CHARACTER 0; podium #60 PA at t0,
  CHARACTER from t240; gondola #146 CHARACTER 0.09-0.67.

### 9.11 Conquer the Mountain free ride and stations: PS2 recordings and parity (2026-09-25)

**Report:** a player (new career) heard no "you can check messages on your MCOMM" routine at the drop-in, an immediate
Andy Hunter announcement, the menu song while riding into the station, then two seconds of "Go" and the menu song again.

**PS2 ground truth.** `tools/ps2_music_drive.py` runs ARMSX2 headless on a derived savestate with the
`tools/ps2_menu_capture.py` pad hook plus `tools/ps2_music_log.py`: 33 entry hooks (the music director 28E8C0 / 28CF98 /
28D488 / 28E088 / 28E548 / 28DF18, PlaySong 2B35A0 / 2B3838 and 28F478 (HUD 5), SendEvent 2B3BC0 / 2B3B88, FadeOut /
Stop / Pause / Resume, 28F140, 28BF78, loading / map / game pause / world load / leave, pktrans 28CD48, 28E888, ambience
29D370, speech 2B1458 / 29EEE0 (bank name per line) / 2B1758) writing 128-byte records into a ring at 0xF4000, with the
tick, world state, song id, playlist index and the course word; the director state (+0x5738.. DJ flags, +0x6230..
trigger / latch / request fields) is polled over PINE. A closed-loop autopilot rides AIP track paths. Plans:
`tools/ps2_music_plans/*.json`; logs and derived states: `local/ps2-capture/music/runs/`; `tools/ps2_music_timeline.py`
reduces the logs to the browser's timeline vocabulary (`web/ps2-audio-timelines.json`). Timelines (ms from the anchor):

| Flow (run) | PS2 | Browser before | Browser now |
|---|---|---|---|
| New career (`newcareer`, `stationA`, `stall`; anchor = the plane intro's end, 28E8C0(19)) | FadeOut 1 s (286200), loading, **pktrans + event 1** (paused, resumed by state 10), loading off, world load with **no PickNextSong / PlayMusic** and **no DJ timer**; plane ~15 s; 0: code 19; +2502 DJ kind 0 → Radio_Big_Intro **FX**; +3003 request 0 → **Peak1** event 12; +5205 First_Spoke; +25658 Text_Message (variant 2, "check your messages on your MCOMM"); ~+33.5 s the station "?" opens the **Message Center on FAQ 1** (game paused) | world load picked a playlist song (Now Playing box, e.g. Go / Andy Hunter) and posted DJ kind 4 (Radio BIG intro + **artist intro of a song that never played**); pktrans after; 28E8C0(19) at once (during the plane cutscene); no FAQ | pktrans first, no pick / DJ kind 4, 28E8C0(19) when the arrival cutscene ends; DJ 0 at +2.5 s, Peak1 at +3 s, First_Spoke, Text_Message, then the "?" FAQ popup |
| In-world MCOMM / Messages / Options / Transport map (`mcomm`, `map`) | 289B70 pause (song pitch 0) … 289BB8 resume; **no 28F140, no charsel**; 28F5B8 / 28F678 change nothing in free ride | **charsel (the menu song) started** on `ctm-mcomm` / `options` and kept playing in the world after Return; a request timer then played the next song for ~2 s and the next MCOMM visit restarted charsel | pause / resume only |
| Lodge (`lodge`) | Yes: 286A80 leave (Stop, no fade), loading, charsel, 28F140(1); Return to Game: FadeOut 1 s, loading ~9 s, world load: pick, **Peak1** 12, DJ kind 2 → Radio_Big_Intro, hub chatter | charsel over the paused world with the timers and DJ still running; Return to Game kept **charsel playing in the world** (no world load) | leave (Stop, timers, speech), charsel 1; Return to Game = fade 1 s + the world load at the station (hub song, DJ 2, hub chatter) |
| MCOMM Transport (`transport`, Happiness → Snow Jam) | 28F520 **Stop**, resume, code 20, pick, Radio_Big_Intro, +16 request 2 → **Peak1** 12 (not backcountry), hub chatter (pool 5); arrival: code 19 | no Stop; the song stayed paused under the transport | Stop, resume, code 20, pick, Radio_Big_Intro, request 2 → Peak1, hub chatter; code 19 at the arrival |
| Course MusicTrigger 18 (`sjB`, Snow Jam first visit) | event 18, pick; +1501 DJ 3 → Radio_Big_Intro FX; +2002 request 1 → playlist song (event 0, **Now Playing**); Free_Ride_Intro; Artist_Intro | same | same |
| Connector MusicTrigger 11 into a station, +0x5790 = 0 (`sjB`, into Blue Base) | event 11; +1485 DJ 0 → Radio_Big_Intro FX; +1985 request 0 → Peak1 12; First_Spoke; event 13 (hub); Text_Message; Char_Stories; B = 17: Event_Intro | same | same |
| Single Event start (`single`, Snow Jam) | FadeOut 1 s, loading, world load: pick, playlist song **36**, PA_Venue_Intro, the EA RADIO BIG box over the intro NIS; GO: resume, **event 0**, Radio_Big_Intro, Artist_Intro | event **37** at the first GO | event 0 |

- **Now Playing.** 28F478 sends HUD command 5 after every PlaySong (pktrans, Peak1 and charsel too), but the HUD shows the
  box only for a non-empty title (1EC2E0): PS2 frames after Peak1 starts (`nowplaying` run, +0.1..+10 s) have no box.
  So the box appears for playlist songs only: a song change (MusicTrigger 18), a transport into the backcountry, the
  Single Event / CTM event start (over the intro NIS), the in-game Request Line. The new career's first minute has none.
- **GO event.** 29CB08 sends 0 while audio+0x627C is set; every world load sets it (0x2867AC), so the first GO of a
  Single Event / online race sends 0; only after a Big Air / Half Pipe heat (286F94 clears it) does the next GO send 37.
- **The MCOMM routine ("?" at Green Base Station).** The Text_Message line is followed by the Message Center: station
  A's `mdl_A_questionmark_1000` (handler row 6) runs program 8 at its section load: stage builtin 110 (307020 → 1E3570:
  Conquer the Mountain, 0x535C12 not 6 / 9, and 147580(P, 0) == 0 = the career's first FAQ not shown yet) → LiveComp
  (builtin 3) + MagnetModifier(10, 600) (builtin 90): the "?" flies to the rider. Its contact program 9: DeadNode
  (builtin 2) + builtin 100 (306300 → 1E3510: gp-0x1024 = 0, 1475C0(P, 0, 1)); world state 4 then opens overlay 0x22
  (0x2309A4) and the Message Center (1E3C00) opens the Progression/Rewards folder while shown and views FAQ 1 "How do I
  open up other peaks?" (PS2 `stall` run: tick 2003 at station A, ~13 s after the A crossing). Only these two stage
  programs call builtins 100 / 110 (scan of every stage). Browser: `web/stage_script_gameplay.inc` builtins 110 / 100 →
  `web/peak_world.inc` (flag `faqShown` from the career via `peak_world_faq_shown`, event 6), `web/free-ride.js` 'faq',
  main.js (pause, `careerUI.me.faqShown`), `web/career-messages.js openFaq`.
- **Peak_Boss** hub chatter (2A2A10) takes 2A1DA0(145750): the peak rival's subject mask (Mac / Nate / Psymon, Griff /
  Zoe / Elise when the player rides that one); the browser passed 0 (no line matched, silent).
- **Browser changes** (`web/game-audio.js`): `screen()` ignores front-end themes while a world is loaded except the lodge
  (`enterLodge`: 286A80 then charsel + 28F140); `runStart` after the lodge is a world load (`state.lodge`); `introStart`
  (the backcountry first visit, pktrans) runs before the loading screen ends and the world load (also pktrans 2 / 3 for the
  first arrival at The Throne / Ruthless, which the page reaches by a world load: from 28E8C0(20)'s travel branch, not
  recorded here); `arrivalCinematic(on)`
  (main.js ui.cb.cutscene kind 'arrival') holds 28E8C0(19); `travel` = 28F520 Stop + map close resume + 27A860 (or a world
  load when leaving the lodge); `pause(false)` without a pause is a no-op (no 289BB8 speech cancel); GO event per 0x627C;
  Peak_Boss argument. `timeline()` / `timelineReset()` record the director events above.
- **Test:** `web/test-audio-timeline.mjs` (npm test) drives these flows with a manual clock and compares the music /
  director events in order, the DJ timer and request offsets (±60 ms), the DJ / PA speech order and the Radio BIG intro
  variant with `web/ps2-audio-timelines.json`.
- **Known differences:** the page has no loading screen between the lodge and the world (the PS2 loads ~9 s; the timer
  offsets are checked from the world load); the Message Center shows "Message # FOLDER" and a Delete button for FAQ views
  (the PS2 overlay shows "Message #FAQ 1", Previous / Keep message) - web/career-messages.js' existing FAQ view; after the
  lodge the page's Return to Game goes through its in-world transport and shows the lodge prompt again (career flow).
  Safari automation was refused in this workspace (sandbox), so only Chrome was measured.
- **Reference:** [GlitcherOG/SSX-Library](https://github.com/GlitcherOG/SSX-Library) (GPL-3.0; read, no code used)
  documents the MUSIC.INF keys and the speech `.hdr` headers (its `MusicINFHandler` / `HDRHandler`); its MUSIC.INF field
  list agrees with 3.1. It has nothing on the runtime director, the DJ scheduler or the music triggers. Its `.hdr` reader
  names the byte our 4.2 calls the tag count "EntryTypes" (entry layouts 0-4, 24-bit offsets for 3 / 4); the exported
  lines decode and match the PS2 either way.

### 9.12 Music streaming (pv musicStream, 2026-09-27)

The PS2 streams a song's MUSDATA from the disc as it plays. The browser used to download the whole `.mus` before the song
could start: charsel.mus is 44 MB (the menu song, requested at Press START), race songs 17-29 MB, pktrans 15.5 MB,
spokehub 10 MB. On a 6 Mbit/s link the menu song took ~70 s to start and shared the link with the course the whole time.

With `web/pv-flags.js` `musicStream` (on), a song's `.mus` streams bar by bar:

- **Bars as byte ranges.** Every stream sample (graph `samples[]`, one bar or a longer piece) is its own byte range
  (`offset`, `size`). `web/pathfinder.js createMusStream` holds only the bars asked for, fetched with `Range` requests
  (`web/game-audio.js rangeBytes`; `web/server/mp-server.mjs` already answers ranges for `.mus`, which are never gzipped). A
  server that answers 200 hands over the whole file, which then serves every bar (today's behaviour).
- **A bar when it is committed.** The Pathfinder player asks `decodeSegment` for a bar when it commits it (~0.5-1 s ahead). A
  bar not in yet decodes to null and is asked for (the player asks again on its next 50 ms pump); a bar still missing when
  it is due counts in `song.mus.late`. The decoded PCM is the same as from the whole file (`web/test-music-stream.mjs`).
- **Read-ahead along the graph.** Each decoded bar asks for the bars its node can lead to (through the control nodes): two
  steps ahead, one while the game's own downloads run (`downloads.js downloadProgress().active`). Read-ahead requests are
  `priority: 'low'`, and the range requests bypass `web/downloads.js`, so music never counts as load work on the load
  screens.
- **The opening first.** `streamSongStart` (the streaming counterpart of `prefetchSongStart`) dry-runs the start event
  (jumps included) and loads the bars of its first 4 s. The player starts once those of the first 1.5 s are in, and its
  random replays the dry run's draws, so it commits those bars.
- **Memory.** Only the song playing and the next one (the two asked for last: `prefetchPicked`) keep their bars; a song played
  before them lets them go and streams again if it is played again (9.15). Until 2026-09-29 the last 4 songs kept theirs (38.7 MB of
  bars 15 min into a Peak 2 Race). Before streaming, every song played kept its whole file for the session (`game-audio.js bytes`
  cache: 44 + 17-29 MB per race song).
- **The live edge:** `deploy/edge-worker.js` passes the `Range` header through to Cloudflare's cache; whether a cache miss
  answers 206 or the whole file (200: then as before) should be checked on the live site.
- **Unchanged:** loop banks (LOOPDATA, ~0.3 MB) still load whole; a long piece (the peak ambience samples, 85 s and
  ~4 MB each) is still one range, so the ambience starts once its first piece is in.

Measured with the edge stand-in of docs/first-load.md (first visits, a real Enter at "Press START", 5 s in the menus,
then Snow Jam or a new Conquer the Mountain career; medians of 2 runs). "Menu music" = charsel audible after Press START
(before: its file complete). "Race / ride music" = the race's or the ride's song audible after the race / ride starts.

| browser | link | to | menu music after START | START -> race / ride | its music after it starts | .mus on the wire |
| --- | --- | --- | --- | --- | --- | --- |
| Chrome | 50 Mbit/s | CTM ride | 13.5 s -> **0.1 s** | 64.7 s -> **60.3 s** | 2.8 s -> **0.3 s** | 69.9 -> 3.5 MB |
| Chrome | 50 Mbit/s | Snow Jam race | 12.5 s -> **0.1 s** | 18.2 s -> **13.6 s** | 3.3 s -> **0.4 s** | 63.0 -> 2.6 MB |
| Chrome | 6 Mbit/s | CTM ride | 72.0 s -> **1.2 s** | 189.6 s -> **128.5 s** | > 15 s -> **0.9 s** | 44.4 -> 2.9 MB |
| Chrome | 6 Mbit/s | Snow Jam race | 69.5 s -> **1.3 s** | 123.8 s -> **65.9 s** | > 15 s -> **1.2 s** | 44.4 -> 2.4 MB |
| WebKit | 50 Mbit/s | CTM ride | 13.7 s -> **0.3 s** | 72.0 s -> **66.4 s** | 2.9 s -> **0.4 s** | 70.0 -> 3.1 MB |
| WebKit | 50 Mbit/s | Snow Jam race | 12.6 s -> **0.2 s** | 27.2 s -> **23.1 s** | 4.1 s -> **0.9 s** | 66.4 -> 2.5 MB |
| WebKit | 6 Mbit/s | CTM ride | 74.3 s -> **2.3 s** | 194.2 s -> **137.7 s** | > 15 s -> **1.3 s** | 44.5 -> 2.9 MB |
| WebKit | 6 Mbit/s | Snow Jam race | 72.0 s -> **2.3 s** | 143.9 s -> **74.7 s** | > 15 s -> **1.6 s** | 44.5 -> 2.3 MB |

The Conquer the Mountain "ride" is the first free-ride frame after the plane arrival NIS (~45 s, the same either way).
Memory (phys_footprint, page + GPU process) at the race / ride is unchanged within the runs' spread.

### 9.13 Audio glitches: clicks, late sounds, interruptions, field counters (2026-09-27)

**Measuring silently.** The master gain stays at 0 (`?mute=1`, `--mute-audio`, the muted WebKit driver).
- A probe records every AudioBufferSourceNode start and stop against the context clock, and tags the music player's bars.
- An AudioWorklet records the MUSIC bus into memory (after its slider, before the master).
- A **click** is the bus jumping to or from digital silence with more than 0.02 full scale on the far side.
- A **level jump** is the largest sample step in the 60 ms after a level change, divided by the p99.9 step of the next 400 ms.

The tools are in the session scratchpad `audio/`: `probe.js`, `analyze.js`, `drive.mjs` (scenarios), `table.mjs`, `calib.mjs`.

The scenarios:
- the menus;
- a Snow Jam race with computer riders;
- ASS1;
- CTM free ride across A_ARA1, also at 6 Mbit/s once riding;
- a Big Challenge ×3 (the prompt's Yes, then 29D6E0 type 3 = event 38);
- a big air driven by hand (the duck, 2B4620 + event 7, then event 8 and the level back at the landing);
- pause ×6 and a tab hide ×4;
- an iOS-style interruption: the context stops and refuses to resume for 4 s while the race runs;
- a movie hold with the page hidden and shown under it;
- 2 simulated GPU device losses.

They ran in headless Chrome (desktop, and a phone at 4x CPU) and in WebKit.

`calib.mjs`: start(when) at or before `currentTime` lands exactly on `currentTime` in both browsers (0 frames late). A late start therefore costs only its gap.

**Found** (switches off, per run):

| glitch | Chrome | WebKit | phone 4x | cause |
|---|---|---|---|---|
| pause clicks (6 pause / resume) | 10, step ≤ 0.29 | 11, ≤ 0.24 | 11, ≤ 0.18 | the playing bar stopped hard at the pause and restarted mid-waveform at the resume |
| tab hide clicks (4) | 8, ≤ 0.12 | 7, ≤ 0.50 | | the same (the game pauses when hidden) |
| Big Challenge start (3) | 4 clicks, ≤ 0.13 | 7, ≤ 0.27; stinger 2.7 ms late ×2 | 5, ≤ 0.31; stinger 5.3 ms late ×3 | events 33 / 34 / 38 flush the stream mid-bar; the stinger's decode and the event processing let the clock pass its start, so its attack was skipped |
| song switch (Stop, PlaySong) | step 0.43 at a free-ride ChangeSong (6 Mbit/s) | | | a hard Stop of the playing song |
| big-air landing | the stream level set 20 -> 127 in one write: over Avalanche's transients no measurable click (0.1-0.7 of its p99.9 sample step) | | | a step, audible in quiet music |
| interruption | 27 / 30 voices started together | 42 / 27 | | every voice asked for while the context was stopped was scheduled at the frozen clock |
| movie hold | context running under the movie after a hide / show | same | | the page-shown resume ignored the movie's suspend; a key press too |
| main-thread music decode | ≤ 13 ms | ≤ 14 ms | ≤ 49 ms, 24 over 8 ms in 60 s of menus | 6-channel EA-XA charsel bars decoded in the scheduler's pump |
| scheduler margin | p50 1.47 s | 1.47 s | 1.45 s; a 1.6 s stall in the menus (lazy course load) | 1 s look-ahead |
| crowd loop first play | 18 ms decode at the intro | 13-16 ms | 97 ms | the warm-up had not reached the crowd bank when the intro started it |
| song change at 6 Mbit/s | 4.4-4.8 s of silence between the songs | | | the new song's opening bars start downloading only at the request |
| late / missed bars | 0 / 0 | 0 / 0 | 0 / 0 | |

**Fixes** (switches in `web/pv-flags.js`; all on since 2026-09-27 except `sfxWarmFirst`):

**`audioDeclick`** covers `pathfinder.js` `declickMs`, `audio-speech.js` `declickMs` and `game-audio.js`. A source that stops or starts mid-waveform ramps over 5 ms instead of stepping to or from 0:
- the pause: the sounding bar fades out;
- the resume: the restarted bar fades in, or the next bar when the pause was on a bar line;
- an event's flush and a kill (`halt`), a Stop (both outputs), a late bar;
- every level write (the big-air duck and its jump back at the landing);
- a speech line stopped while it plays.

A fader before the master (`audio-engine.js declickMs`) fades the whole mix out over 5 ms before the context suspends (a hidden page, a movie) and in again when it runs. The suspend waits for the fade; showing the page again during it cancels the suspend. After an OS stop (iOS 'interrupted') the mix fades in when the context runs again. Without it, the device stopped and restarted mid-waveform.

A ramp ends at its cut time when the cut is known 5 ms ahead. An event that cuts now fades over the next 5 ms. Starts, offsets and the song timing are unchanged. The Big Challenge stinger starts on the same sample; the cut bar fades under its first 5 ms.

A song's first bar is scheduled 40 ms ahead (its decode used to make it start late). The prompt's Yes (29D6D0, `challengeAccepted`) prepares the challenge ahead (`player.warmEvents([33, 34, 38])`): it decodes the song's stingers into the buffer cache, one per task, and fetches (and, with `musicWorkerDecode`, decodes) the stream bars those events jump to.

PS2 code: the pause is 2B2018 -> 3B7FB8(stream, pitch 0) and 3D1038 (overlay pause); the resume is 2B2070. Stops (2AF838) and volume writes (3B7E70 -> 3B85F0: +0x38 set, no step) are immediate in the EE mixer.

PS2 output: silent ARMSX2 dumps (SDL `disk` driver, file only). The derived states have channels 2-10 poked to 0, so only the music plays. The tools are in the session scratchpad `ps2audio/`: `rec.py` (audio_record.py with pads, pokes and watches), `summary.py`, `freeze.py`, `towav.py`.
- **Pause** (Snow Jam race, 3 pauses and 3 resumes):
  - The stream holds its last sample as a DC level. The step into the hold is 0.0006-0.0017. The largest step within ±20 ms is 0.41-0.55 of the music's own p99.9 step, so there is no click.
  - The emulator's DC filter then decays the hold with a 200-sample (4.2 ms) time constant. This is probably ARMSX2's, not the game's.
  - At the resume the music continues from the held sample (0.02-0.30 of p99.9).
  - So the PS2 has no click either way. The port's 5 ms fade out and in is its stand-in for the hold and the continuation.
- **Challenge cut** (Wobble, event 38): the stream's flush holds too (step 0.0001; the largest step is 0.21 of p99.9). The stinger starts 21.6 ms after the cut, with no step. In the port the stinger starts at the event and the cut bar fades under it.
- **Big-air landing:**
  - The path volume went 127 -> 20 over 30 ticks, then back to 127 in one write.
  - The output level rose about x6 within about 5 ms, with no step above 0.3 of p99.9.
  - This is one landing only, and the sample at the change was near 0, so smoothing versus a step is not settled.
- **Also seen:** the PS2 does not pause the DJ speech; a line played on for 2.2 s into the pause. The port does the same.

**`musicLookahead`**: the player simulates and schedules 2.5 s ahead instead of 1 s.
- Inputs still re-simulate from the last real-time snapshot.
- A provisional source that a re-simulation changes is stopped before it starts.
- Audio that an input makes due at once (the stinger) starts right after the input, before the look-ahead pass (`eventFirst`: `startDue`). If the clock passed it by one or two render quanta, it starts whole.

Events, intensity, the big air and the challenge keep their exact timing. `test-audio-glitches.mjs` checks this: a scripted race session (the intensity ramp, a big air, a Big Challenge, a pause) has the same audible schedule with 1 s and with 2.5 s. The schedule is every bar and loop slice, with its start, offset and stop. In the browser, the stinger starts at the event, the next part 1200 ms later, and every slice lands on the master's beat grid (0 µs error). A stall of up to ~3 s can no longer make a bar late.

**`musicWorkerDecode`**: every stream bar decodes in the existing `audio-decode` worker ahead of its time, not only MicroTalk bars.
- A 6-channel bar is folded to stereo in the worker, with the same floats as `toAudioBuffer`.
- A bar still in flight 80 ms before it is due decodes on the main thread.
- A failed or missing worker falls back to main-thread decoding.
- At most 12 decoded bars wait to be handed out (bars a re-simulation dropped do not pile up).

**`audioInterrupt`** (`audio-engine.js interruptGate`): once the context has run, a stopped context takes no new voices. `engine.live` is false, and SFX and speech requests are dropped (counted in `sfxGated`). The engine counts its holds (`suspend('hold')` for a movie, `'hidden'` for the page), so neither showing the page nor a key press during a movie resumes the game audio under it.

The context is resumed again:
- on focus and pageshow;
- by a retry timer while the page is visible (500 ms doubling to 5 s, for as long as it takes);
- on every key, tap and pad press. The pad listener stays after the unlock, and input clears a stale 'hidden' hold.

A refused resume therefore never leaves the audio off for good.

**`sfxStartAfterDecode`**: a voice's layers decode before its start time is read, so a first play that decodes starts with its whole envelope.

**`sfxWarmFirst`** (off): the loading screen's warm-up decodes what the intro starts first: the location ambience (slot 9), the crowd loops (slot 5) and the world bank (slot 8). On the 4x phone the crowd loop still decoded at its first play under the intro (70-97 ms, `sfx:Crowd/1`), so this is inconclusive. The load screen does not wait for the warm-up.

**`musicPrefetchNext`**: a song picked for a later request starts streaming its opening bars when it is picked (`prefetchPicked`, the same caches playSong uses). That covers ChangeSong's 2 s and a fly-over's 3 s.

**`heatSong`** (Owen's report: every heat of a CTM race event replayed the same song). World state 13 is a Conquer the Mountain event's Next heat, and also the results' Restart. Its handler 0x235A18 calls 27A860 at 0x235C44, which calls 28E8C0(20, 1). The event branch of code 20 (0x28EC90..0x28ED18) runs when:
- it is not free ride (2A4040);
- the event is a race (2A4078) or slope style (2A40E8);
- the round (`*(G+0xC0)+0`, G = `*(gp-0x848)`) is 2 or more.

It then calls PickNextSong (28D488) and a forced request kind 3 (PlayMusic 36) after 10 ms (2ADCA0). So heats 2 and 3 each start a new song, and round 1 keeps its own.

The PS2 check is a silent ARMSX2 run with the music-director log: `tools/ps2_music_drive.py local/ps2-capture/ctm-parity/runs/race-q/res.p2s heat2-audio PLAN` (Cross on the qualifier's Next heat, then Cross on the card). The log (`local/ps2-capture/music/runs/heat2-audio`) shows:
1. 28E8C0(20, 1) from 0x27A9E0;
2. PickNextSong, with the playlist index going 14 -> 15;
3. the request callback;
4. two ticks later, PlayMusic(36): PlaySong, Stop, event 36;
5. code 19 at the card;
6. Resume and event 0 at GO.

The port had the branch in `travel()`, but only the transport called it. `gameAudio.heat()` is now called by the 'heat' cutscene (`web/cutscenes.js play`).

**`ctmRestartAudio`** (the CTM Restarts and the countdown rule). The PS2 music logs are in `local/ps2-capture/music/runs/`; the plans are in the session scratchpad `ps2heat/`.
- **The pause's Restart** (0x20D1D8 -> 2302A8; run `pause-restart-audio2` from `menus/race/state-pause`):
  - 2302A8 resets the world and sets world state 2 directly (231250(S, 1, 2, 1)). There is no world state 13 and no 28E8C0.
  - 2870A0 -> 2871B0 stops the riders' loops (296E20 / 297438), 29B3C0 and 2948A0, releases the ducks (287F00) and fades a voice (2AD5F0).
  - The song is resumed at the Yes (285D78 game resume, 289BB8) and plays on under the card, with its playlist index unchanged.
  - Nothing happens at the countdown. GO sends Resume and event 0.
- **The results' Restart** (run `results-restart-audio` from `menus/race/state-results`, round 1): world state 13 -> 28E8C0(20, 1), with no pick in round 1. Then the countdown rule picks a new song.
- **The countdown rule 29C420(digit)**, with audio+0x627C set:
  - "3" (0x29C6B4..0x29C704): FadeOut(2 s) of a hub song (ids 1-3) or chartune (0xC9).
  - "1" (0x29C710..0x29C784): when no song is going (audio+0x530 clear), or a hub song / chartune plays, it calls PickNextSong, then PlayMusic(event 0), then Pause (2B3A70). GO resumes it with event 0.
  - In the log: at "1", PickNextSong (index 15 -> 30), PlayMusic 0 and Pause; at GO, Resume and event 0.
- **The port's mismatch:** `career-ui.js restartToCard` quits the run (`ui.cb.quit` -> `leaveWorld`: Stop, all world banks unloaded), so every CTM restart did a world load with a new song and decoded every bank again. `musicAdapter.countdown` was empty.
- **Now:**
  - `gameAudio.restartRun({fromResults})` (called from restartToCard before the quit) does the 2870A0 stops and resumes the song. The quit's leaveWorld then keeps the world's audio, and runStart makes no world load and sends no event 36.
  - The 'heat' cutscene of the results' Restart runs `heat()`.
  - `countdown(d)` is the rule above. audio+0x530 clear is taken as no song, or the song has played its ending (event 10), while none is loading.
- **Test:** `test-audio-glitches.mjs` drives the director with a running context through both Restarts and checks the logs' sequences:
  - pause: `restart, resume, event 0` at GO, and the same song;
  - results: `code 20, pick, play 0` at "1", then `event 0` at GO.

**After** (the MUSIC bus's hard steps of 0.08 full scale or more after the scenario start; switches off -> on):

| scenario | Chrome | WebKit | phone 4x |
|---|---|---|---|
| pause ×6 | 8, ≤ 0.29 -> **0** | 5, ≤ 0.24 -> **0** | 5, ≤ 0.18 -> **0** |
| tab hide ×4 (race) | 5, ≤ 0.12 -> **0** at the suspensions | 5, ≤ 0.50 -> **0** | |
| context suspend (whole mix at the suspend point, menus / race) | 0.02-0.23 / 0 -> **0 / 0** (in the race the game's pause silenced the mix before Chrome's suspend landed) | 0.11-0.20 / 0.02-0.25 -> **0 / 0** | |
| Big Challenge ×3 | 3, ≤ 0.13 -> **0** (Avalanche: 2, ≤ 0.39 -> **0**) | 4, ≤ 0.27 -> **0** | 2, ≤ 0.31 -> **0** |
| stinger start | whole; Avalanche 1 of 3 5.3 ms skipped -> **whole** | 2.7 ms skipped ×2 -> **whole** (≤ 1 render quantum after the event) | 5.3 ms skipped ×3 -> **whole** |
| the next part after the stinger | 1200 ms | 1200 ms after the event | 1200 ms |
| ChangeSong at 6 Mbit/s | a 0.43 hard stop, 4.8 s of silence -> **no step, 1.8 s** | | |
| interruption burst (voices at once) | 27 / 30 -> **0 / 0** (36 dropped, counted in `sfxGated`) | 42 / 27 -> **0 / 0** (96 dropped) | |
| movie hold + hide / show | running under the movie -> **suspended** | same -> **suspended** | |
| music look-ahead margin p50 | 1.47 s -> **2.93 s** | 1.47 -> **2.92 s** | 1.45 -> **2.79 s** |
| main-thread music decode | ≤ 13 ms -> **off the thread** | ≤ 14 ms -> **off** | ≤ 49 ms (24 over 8 ms in the menus) -> **off** |
| SFX start late (max) | 16 / 27 ms -> **5 ms** | 21 -> **3 ms** | 48 / 69 -> **5 ms** (the rest is render-quantum granularity) |
| late / missed bars | 0 / 0 -> 0 / 0 | 0 / 0 -> 0 / 0 | 0 / 0 -> one bar 181 ms late (see below) |
| GPU loss ×2 | pump max 461 ms, 0 late -> 457 ms, 0 late | | |

**Left open:** on the phone, during a Big Challenge's 1.2 s wait (track 0 silent), a free-ride painter event jumped the song at once to a bar not streamed yet. That bar started 181 ms late, once in 3 challenges. The read-ahead follows the song graph, not the events' targets. `musicLate` counts it in the field.

**Field counters** (`web/audio-stats.js`, `globalThis.__ssxAudioStats`). `web/diagnostics.js` sends the non-zero ones to /mp/diag with the heartbeat when they change (`frames` / `memory` events, field `audio`), and the totals at `pagehide`. The hot paths only increment. The counters:
- `musicLate` / `musicLateMaxMs`: a bar or slice started after its time (resumes excluded);
- `musicMissed`: a committed bar that never got audio;
- `musicPumpMaxMs`: the longest gap between scheduler pumps while visible;
- `decodeSlow` / `decodeMaxMs` / `decodeWorst`: main-thread decodes over 8 ms, the longest, and what it was (`music`, `speech`, `sfx:<bank>/<sound>`);
- `sfxStolen` / `sfxDropped` / `sfxGated`;
- `speechLate`;
- `ctxInterrupted` / `ctxResumeFailed`.

**Left as they are:**
- Voice steals (IOP 8 / SPU 48, 3BA0B0): a hard stop on the PS2 too, 0-4 per race.
- The crowd `.eam` note timers (setTimeout; the PS2's 2ADCA0 queue is frame-granular).
- The location ambience decoded at its first play during the load (69 ms at 4x, under the load screen).
- Charsel's 6-channel fold peaks at 1.73: 1606 of 56 M samples clip at the output after the slider and master. The EE mixer's 6-channel mix was not checked.
- The CTM music at 6 Mbit/s stays behind the world's downloads by design (9.12).

Test: `web/test-audio-glitches.mjs` (in test:all).

### 9.14 Conquer the Mountain: world switches, post-event commentary, first visits, DJ queue rules, mail icon (2026-09-29)

Source: docs/ctm-decomp-freeride.md ranked 2, 9, 10, 11 and 12. PS2 capture: `local/ps2-capture/ctm-decomp/audio/postevent2` (the Transport
after the Snow Jam final, Snow Jam -> Metro-City). Every change is behind a default-off switch in `web/pv-flags.js`. Test:
`web/test-ctm-audio.mjs` (in test:all); `web/test-messages.mjs` covers the mail icon.

**pv worldSwitchAudio: a Transport across a page world switch runs code 20 in the world.**
- The PS2 is one world, so the post-event return and a Transport to another peak are world state 14. At the confirm
  (postevent2, ticks 15440-15442):
  - 28F520 stops the song (song 201, the podium's chartune, still playing under the map);
  - 28E8C0(20, 1) runs PickNextSong and Radio BIG intro (0, 1) with its flush, then 2A4718 queues the pool-5 hub chatter (+0x5754);
  - the destination song follows 10 ms later (request kind 2 -> Peak1, event 12, for Metro-City);
  - there is no loading loop and no world load;
  - 0x2102 at 15614, during WS11 (the ride);
  - 28E8C0(19) at the cinematic end (15963).
- The port changed page world instead: `loadingStart`'s LoadingScreen loop, then 2867E8 / 2A4A78 at the world load (a new song, DJ
  kind 2 / 4). `travel()` returned at once after an event, because `state.free` was null.
- Now:
  - `career-ui.js goWorld` calls `gameAudio.travelSwitch(dest)` at the confirm whenever the destination is in another page world
    (`switchesWorld`): the post-event map, or another peak on the per-peak worlds. It is not called for a reload or for the second,
    ridden call.
  - `travelSwitch` restores free ride (kind 4, mode 12: 2018A8, as the poll at the confirm shows), clears the results' replay flag and
    runs `travel(dest)`. That includes 28EF90 for the same location (WS15).
  - It then carries the audio (`state.switchCarry`):
    - `leaveWorld` stops only the world's sounds and banks. The song, the timers, the speech, the pending DJ flags and the `'nis'` voices
      of the ride (`sfx.stopAll({ keep })`) stay.
    - `loadingStart` starts no loop.
    - The next free-ride `worldLoaded` skips riderMusic, 2867E8, 2A4A78 and the 579C read, keeps the MusicTrigger state, and runs world
      state 10 (`ws10`).
    - A `freeWorldLoaded` that arrives after the ride start has already taken the carried world in makes no second world load. The
      unswitched path does load twice when main.js starts the run before its load-screen notification: seen in Chrome and WebKit with
      `?autostart`.
  - `career-ui.js transportAfterEvent` calls `eventMap()` before its quit, so the chartune plays on under the map. An event picked
    from the map is an ordinary event load: `loadingStart` ends the carry.
  - `crossWorld` carries the audio without code 20: a riding crossing makes no director call (22DF50).
- `ws10(course)` is world state 10:
  - the push enter 234FE0..235058: an unvisited Conquer the Mountain backcountry sets +0x578C, +0x5790 = 0 and +0x6254;
  - the enter 0x2355C0: 2B3A98 resumes the song, and 2A4B68 stops the speech while pktrans (0x191) plays.

  It also runs from `freeRideCourse`. That fixes the in-world Transport (MOUNTAIN, desktop) into an unvisited backcountry: code 20's
  pktrans had stayed paused, and code 19 took the arrival branch (DJ 4, event 0) instead of the BC intro (request 1 at 2 s, DJ 7 at
  1.5 s).
- Checked:
  - Chrome and WebKit (`?pv=worldSwitchAudio,...`, ARA1 -> the results' map carry -> the heli ride -> Green Base Station in MOUNTAIN):
    `leave map`, `carry travel`, `stop`, `code 20`, `pick`, `request 2`, `play Peak1 12`, `leave travel`, `carried travel`, `code 19`,
    with no `loading` and no `worldload`.
  - With the switch off: `fade 1`, `loading 1`, then `worldload` twice.
  - Not checked: the `'nis'` voices over the switch. In headless Chrome the byTag count stayed 0 during the held loop, with or
    without the switch.

**pv postEventDj: the post-event record and commentary (2A45C0 / 2A4660 / 2A4770).**
- The record is audio +0x57F8 armed, +0x57FC final, +0x5800 place, +0x5804 hits, +0x5808 score, +0x580C last commentary and
  +0x5810 course.
- Writing it: 287060 calls 2A45C0 at every CTM finish (0x535C11 == 0), timed out too.
  - Round 1 resets it (2A4590(audio, 0), which also sets +0x5814 = 23 and keeps +0x580C).
  - 2A4660 arms it at a medal run (GMM+0x98) or when no round follows (rival challenges, peak runs).
  - It sets final when the rider finished round 3 (+0x480 clear).
  - It always adds the KOs, from the score object +0x128 (races only: 2A4078 = course < 5 or kind 0 / 5), and +0x114. The score object
    port names +0x114 the Uber count (docs/tricks-scoring.md); this agent did not confirm it separately.
  - The port records in `finish()`. main.js passes `stats` {ko, ubers} from `score_object_dump`, and the freestyle place from
    `freestyleFinishPlace` (238B70), since a freestyle run has no AI place.
- Reading it: 2A2E50 runs 2A4770 first unless Char_Stories is due (0x2A2EBC).
  - Nothing armed: return 0.
  - Final:
    - 1st -> Char_Progress 0x2102 [subject, peak of the current course];
    - hits >= 5 -> Aggression 0x212E [course bit];
    - score >= 27 -> High_Trick_Score 0x212F [course bit]. The threshold is 27 on race, slope style and half pipe courses and 24
      elsewhere (2A40E0 / 2A4158 / 2A4238).
  - With two or more earned, the last one said is dropped, then one is picked at random (2ADF60 & 0x7FFF). There is no flush
    (0x2A2EC4 branches past 2B1758).
  - Nothing earned, or not the final, with a travel pending (+0x6254, +0x5814 != 23): a hub -> 2A2E50(pool 0), a backcountry ->
    Terrain_Info [peak], a course -> Event_Intro [course bit, 3].

**pv djVisited: first visits from the save.**
- 579C comes from the ridden rider's saved mask P+0xACC (main.js `gameAudio.context().visited` = `careerUI.visitedMask(rider)`),
  combined with the marks made since the world load. A reload therefore no longer replays Free_Ride_Intro or the pktrans / BC_Intro
  first-visit routine.
- 146008(P, 0, 1) at 0x28E730 gates Free_Ride_Intro and its 579C clear on Peak 2 being locked (`context().peak2Locked` =
  `!peaks[1]`). When it is clear, kinds 3 / 4 / 5 queue only the artist intro.

**pv djQueueRules.**
- 2A26F0 stops the current line (2B11B0 at 0x2A272C) after its 29F0D8 gate and before it posts (`audio-speech.js radioBigIntro`).
- 289BB8 stops the speech and clears the pending DJ flags (2A4550) only when +0x5828 is set, and then clears it. 28FAE0 sets it: an
  in-game song change reached from the menu case at 0x208C28 (0x208CA4). That case's port equivalent is not identified, so
  `state.songChanged` is never set, and a pause resume leaves the DJ queue alone. Before, it cancelled every queued line.

**pv mailFreeze (web/career-messages.js).**
- The PS2 posts the event's messages on the finish tick. The port posts them at the results, so the icon now starts at 182 frames
  (race-f: fin 0.050, res 3.033, f95-after 3.050). The next ride shows the last ~2 s.
- The Message Center freezes the icon (HUD events 3 / 4) instead of clearing it.
- Still missing: the ~3 s blink under the finish HUD. That needs the result decided at the finish tick.


### 9.15 Audio memory: node lifetime, the streamed songs, speech lines by range (2026-09-29)

Nothing here changes what plays or when: scheduling, gains and automation are untouched. The switch-off timeline tests,
test-audio-timeline and test-ctm-audio, give the same events in the same order.

**Voices let go of their nodes (no switch).** A Web Audio node that is still connected stays alive: Chrome keeps it, and WebKit's
audio thread keeps processing its automation (`AudioParamTimeline::valuesForFrameRangeImpl` in WebContent samples). Before this fix:
- `sfx.js finish()` disconnected only the voice output. The source, patch gain, envelope gain, LFO gain / LFO sources and panner
  stayed wired together.
- The 3BB588 pool-failure path never stopped a started layer's looping LFO sources.
- Game code held ended voices: `audio-world.js contactVoices` / `scriptVoices`, one entry per instance ever touched. Chrome root
  path: `__perfAudio -> world.contact -> contactVoices -> voice.layers[0].pan`.
- Speech lines (`audio-speech.js`), music bars (`pathfinder.js startSource`: source -> declick gain -> slice gain), a stopped song's
  ramp / level chains and the LoadingScreen loop were never disconnected.

Now:
- A voice disconnects its whole chain 50 ms after its last layer ends or its fade ends. Its handle drops its layers and output, so a
  stale handle holds nothing.
- The failure path stops and cuts its layers.
- A bar or line disconnects at its `ended` event, and at once when it is stopped now (a source cut off the graph may never fire ended).
- `Stop` cuts the song's chains.
- `audio-world.js` sweeps entries that no longer play once a map reaches 64 (an entry that is not playing blocks nothing).
- Tests: test-audio-glitches "node lifetime".

**Streamed songs keep only what the player needs.** `pathfinder.js musStreamFor` keeps the bars of the song playing (loadSong role
`play`, from `playSong`) and of the next one (role `next`, from `prefetchPicked`). Every other song lets go of its bars. Until now
the last 4 songs kept theirs. Inside a song:
- Bars are kept up to `MUS_BUDGET_BYTES` (24 MB). Past that, played bars go first, then the least recently used; bars used in the
  last 20 s and a 200 whole-file answer are never evicted.
- 24 MB is a safety net. A song's graph reaches a bounded set: in 15 simulated minutes, Ride reaches 21.7 MB, Emerge 18.6 MB and
  Clockworks 12 MB.
- A tighter cap makes the read-ahead fetch evicted bars again. For Emerge over 15 min: a 12 MB cap fetches 84 MB and a 16 MB cap
  42 MB, against 18.6 MB fetched once uncapped.
- A song played again streams its opening again, as on its first play (~0.5 MB).

**Speech lines by range (pv `speechRange`, off).**
- Before, a speech bank's `.dat` was downloaded whole and kept for the session, twice (game-audio `once()` cache and speech `lines`).
  DJ_Hub_Char_Stories_eng.dat alone is 35 MB, and 15 min into a Peak 2 Race 21.6 MB of speech `.dat` files were held.
- With the switch, `audio-speech.js lineData` fetches only the line: the bank json's `offset` / `size`, one Range request through
  game-audio `rangeBytes`, priority high. Lines go into a 2 MB LRU.
- Lines are fetched when they are resolved (dispatch / flush) and at every post, from a prediction: the scheduler's next dispatch on
  a copy of its slots, EA random and history. A line queued while another plays is therefore fetched seconds ahead.
- A server that answers the range with the whole file (200) keeps that bank whole, as before.
- `web/server/mp-server.mjs` now serves Range requests from the file itself, never from its precompressed .gz / .br copy.
  precompress.mjs gzips `.dat` files (80-84 %), so before this change every range on the host got the whole gzip body.
- Not checked: Cloudflare's answer to a Range request on a cache miss (`deploy/edge-worker.js` passes the header through). A 200
  there falls back to the whole bank.
- Timing (test-speech-range): the same lines start on the same ticks with the same PCM when a range answers within ~30 ms (the
  prediction's 2-frame lead for a free voice). Every ~16 ms beyond that delays the line by 1 tick (40 ms: +1 tick, 70 ms: +3 ticks).
  Before, a bank's first line waited for its whole `.dat` (0.4-35 MB).
- Tests: test-speech-range (new, in test:all). test-audio-timeline, test-ctm-audio, test-game-audio and test-challenge-audio give the
  same output with the switch on (a file-backed Range fetch).

**Measured** in headless Chrome at the phone tier: 844x390, `quality=low`, a Peak 2 Race entered from the menus and ridden by the
whole-mountain pilot. Counts are live objects after a forced GC (`Runtime.queryObjects`). Buffers are the ArrayBuffers retained per
path in heap snapshots. Scripts: scratchpad `leak/nodes.mjs`, `bigpaths.mjs`, `mapkeys.mjs`.

| | before (0 -> 16 min) | after (final code, speechRange on) |
| --- | --- | --- |
| AudioBufferSourceNode | 17 -> 227 | 12 -> 10 (9-20) |
| StereoPannerNode | 8 -> 218 | 5 -> 5 (2-7) |
| GainNode | 58 -> 220 | 67 -> 69 (60-86) |
| whole files in the game-audio cache (speech `.dat`, banks) at 11-15 min | 21.6 MB | 4.0 MB (banks) |
| speech lines | (in the above) | 1.3 MB (LRU) |
| song bars | 38.6 MB (3 songs; 4 kept) | 15.9 MB (the song playing) |

Free ride (MOUNTAIN, 12 min, speechRange on): the nodes stay flat (sources 9-32, panners 3-15). Speech lines take 1.7 MB and the song
playing (Emerge, fspoon.mus) 19.6 MB.

**Still growing / not audio:** the game-audio `once()` cache keeps every sfx bank's bytes after its slot unloads (~0.1 MB per streamed
location bank). WebKit phone-tier footprint (apr.mjs: `footprint` sampled outside the page, the page polled every 3 s, 10 min):
- In 4 of 6 runs WebContent ran away from the ~5 min crossing into DRA4: malloc +150-200 MB/min, with WebAssembly Memory
  (200 -> 740 MB) and JIT growing alongside.
- It happened with the old audio (1 of 2 runs) and the new audio (3 of 4). The audio code allocates no wasm memory, so the cause
  is elsewhere.
- In the flat runs, old vs new audio: WebContent p50 1191 / 1164 MB, max 1496 / 1397 MB, malloc at 9-10 min 869 / 907 MB. That
  is within the runs' spread.
