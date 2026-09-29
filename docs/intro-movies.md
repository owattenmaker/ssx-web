# Boot and attract movies (2026-09-27, front-end agent)

Which full-motion videos the PS2 plays outside the career, when, and how the browser port plays them. Source: SLUS_207.72
(`dis.pkl`, gp = 0x4A30F0) and ARMSX2 frames from a cold boot (`local/ps2-capture/menus/fe-texture/`, runs `r10`, `r11`).

## The PS2

The front end's update, 0x1A27A0 (FE controller vtable 0x46D7F4), plays a movie whenever the **movie mask** gp-0x1724
(0x4A19CC) is not empty. The lowest set bit goes first. While a movie plays, the FE screens are frozen: their update is
skipped, and 0x1A2208 draws the movie instead of them.

| bit | file (path table 0x441248) | skip |
|---|---|---|
| 0 | `data\movies\eabig.mpc`: EA SPORTS BIG, 2.5 s | none (+0x5AD8 = 0) |
| 1 | `data\movies\thx.mpc`: THX Certified Game; 11.1 s of picture, 13.0 s of sound (the last frame holds) | none |
| 2 | the intro, 207.4 s: `intro_dj.mpc` or `intro.mpc` (below) | Start (UIStart 0x70) or Cross (UINext 0x7A) on either pad (320C48) |

- **Power-on.** The mask starts at **7** in the executable's data, so a power-on plays EA SPORTS BIG, then THX, then the
  intro, before the title.
  - PS2 `r10`: memory card check, then EA at sample 241, THX from about 391, the intro's first frame by 1200.
- **The DJ intro.** The intro is `intro_dj.mpc` when two things hold: the language (0x535610 bits 22..24) is 0, and the audio
  object's one-shot flag +0x6C88 is still set. Its init (0x285014) sets that flag, and 289DE0 reads and clears it. So the
  power-on intro is the DJ cut: the same picture, with DJ Atomika on the centre channel. Every later intro is `intro.mpc`.
- **Between movies.** After each movie +0x5ADC holds the FE for one frame (0x1A2208 counts it down). The sound system is
  paused from the first movie (2B3A70) until the mask is empty again (2B3A98).
- **Attract.** The title's update, 0x1948A8 (title vtable 0x46BB1C), counts frames at +0x48, starting when "Press START
  button" shows.
  - Any button event on either pad resets the count.
  - At **1801** (`slti 0x709`) the count resets and the update sets bit 2. The title freezes under `intro.mpc`.
  - When the intro ends or is skipped, the title comes back as it was, and the count starts again.
  - PS2 `r11`: "Press START" between samples 330 and 346, the intro's first frame at 2146 (2146 - 345 = 1801). The cut is
    immediate, with no transition. After a skip, the title is back at once (`10-title-a-skip-transition`, sample 45).
- **Other movies.** The rewards room plays `intro.mpc` / `mtnalive.mpc` (MoviePlayer 0x1D23E0), and Main menu > Previews
  plays the EA trailers. The backcountry first arrivals play `ABC1` / `DBC2` / `EBC3` inside the NIS lists. All of these are
  already ported (docs/characters.md, docs/cutscenes.md).

## The browser

`web/fe-attract.js` models the mask (`createAttractState`, a pure state machine tested in node). `web/ui.js` calls
`attract.frame()` from the title's draw. A capture-phase key listener swallows every key while a movie plays, as the frozen
FE does. On the title it counts any key or pointer press as a pad event. The files play through `web/fe-movie.js`, which has
a new option `skippable: false`: no click-to-skip on the logos.

The two switches are in `web/pv-flags.js`:

- **`attract`**: after 1801 NTSC frames (30.05 s) on the title with "Press START button" up and no input, `INTRO.mp4` plays.
  - Start (Enter), Cross (Space) or a click skips it. Other keys are eaten.
  - The title then continues, and the count restarts.
  - Only foreground frames count: a step is clamped to 100 ms, so a background tab does not trigger it.
- **`bootMovies`** (on; Owen's choice, 2026-09-27): the power-on intro without the logos.
  - **When:** once per page load, after `boot:ready` (web/boot-screen.js). Nothing streams before that mark, so it never
    competes with the first-load downloads.
  - **What:** `INTRO_DJ.mp4` over the title, falling back to `INTRO.mp4`. Afterwards the title shows as it does today.
  - **The mask:** `DEFAULT_BOOT_MASK` = the intro bit only. EA SPORTS BIG and THX stay in the code behind the mask;
    `?bootlogos=1` plays the PS2's full mask 7.
  - **Skip:** Start (Enter), Cross (Space), a click or a tap skips it, and the first press only skips, as on the PS2: the
    next press starts the game. There is no unmute step: browsers play it muted until the first gesture.
  - **Slow start:** the video stays transparent until its first frame plays. If it has not started within 2 s
    (`BOOT_START_MS`: a slow link, a decode error, a refused autoplay), it is dropped and the title simply stays.
  - **Not played for:**
    - `?course=..&autostart=1`;
    - online links (`?online=1`, `?lobby=`);
    - `?qa=1`, and automated browsers (both unless `?femovies=1`);
    - a page whose first screen after the load is not the title;
    - a later return to the title, for example after a race.
- With `attract` on and `bootMovies` off, the attract plays `intro.mpc`, as a PS2 attract always does: on the PS2 the boot
  intro has always used up the DJ flag by then.
- Automated browsers get neither switch unless the page asks for it with `?femovies=1`. They are `automatedBrowser()`:
  - `navigator.webdriver` or `?mute=1` (web/audio-engine.js `testMuted`);
  - headless Chrome, detected by `HeadlessChrome` in the user agent. The npm test harness's pages report `webdriver` false;
    without this, the boot intro covered the title in `test-fe-texture.mjs`.

**Files.** `tools/export_movies.py EABIG THX INTRO_DJ` writes these (new `--out DIR`: a staging folder). They are streamed
only when played; nothing is added to the first load. All three are on the host (2026-09-27). The default boot plays only
INTRO_DJ.

| file | size |
|---|---|
| `EABIG.mp4` | 0.78 MB |
| `THX.mp4` | 0.43 MB |
| `INTRO_DJ.mp4` | 99 MB |

**Verified.**

- **The PS2 order** (`?femovies=1&pv=attract,bootMovies&bootlogos=1`), in Chrome and WebKit:
  - EA, then THX, then the DJ intro, then the title.
  - Enter and Space do nothing during EA / THX, and Space skips the intro.
  - Setting the idle count to 1790 plays `INTRO` about 0.2 s later. ArrowDown is eaten, Enter skips.
  - A key resets the count, and Enter on the title then goes to the Main Menu.
- **The default boot,** in Chrome and WebKit (`local/browser-validation/fe-attract/boot-check.mjs`):
  - The first `.mp4` request comes after the `boot:ready` mark.
  - `INTRO_DJ` shows once it plays (WebKit muted, as a fresh page would be). ArrowDown is eaten. Space skips to the title,
    and the next Enter goes to the Main Menu.
  - Returning to the title later plays nothing.
  - A click on the "Press START" area skips only.
  - A movie held back 5 s by the server is dropped after 2 s, and the title stays.
  - A broken file ends at once.
  - `?course=ARA1&autostart=1`, `?online=1` and `?qa=1` record their skip reason and play nothing.

**Tests.** `web/test-fe-attract.mjs` (npm test) covers:

- the order, the skip rules, the one-frame hold, the DJ cut once, the 1801-frame idle, input resets and the clamp;
- the default boot (the DJ intro only, marked as a boot movie, then the plain intro in the attract), `?bootlogos=1`, and
  every skip case of `bootSkip`;
- with the executable present: 0x709 at 0x1948C4, the mask's initial 7, the path table, the skip buttons 0x70 / 0x7A, and
  the idle's bit 2.
