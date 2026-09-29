# Control hints for the input in use (2026-09-25)

Every control hint shows the indicator of the input the player is using. With the keyboard, the PS2 button icon
becomes a key cap: a dark rounded key with light FEFONT text, the same cap as the keyboard panel on the load screen.
With a gamepad or the touch deck, the hint keeps the PS2 icon, because the deck is a DualShock. This is presentation
only; the simulation and the pad channels are unchanged.

## Active input device (`web/input-glyphs.js`)

The device is whichever input was used last:

- **Keyboard:** a trusted `keydown`. The touch deck drives menus with synthesized key events, which are untrusted,
  so they don't count as keyboard use.
- **Gamepad:** a button goes down, or a stick crosses half travel. The check is edge-triggered, so a pad resting with
  an axis off centre can't take the hints back from the keyboard every frame. A button that is already held when the
  pad is first seen does count.
- **Touch:** a touch or pen `pointerdown`, or the deck being shown (`touch-controls.js show()` →
  `noteTouchShown`).
- **Default:** if nothing has been used yet, the device is touch when the deck is up, otherwise gamepad when a pad is
  connected, otherwise keyboard.

Where the device is read or set:

- `inputDevice()` polls `navigator.getGamepads()` because the menus never run `main.js inputs()`. In a race,
  `inputs()` also feeds the pad it reads to `pollGamepads`.
- Hints are drawn every frame, so switching device mid-screen changes them on the next frame. `onInputDevice(fn)`
  notifies on changes.
- `?glyphs=keyboard|gamepad|touch` forces a device for QA. `forceInputDevice` does the same in tests.
- The load screen (`loading-screen.js`) and Options > Controller Settings (`fe-options.js inputDevice`, now a
  re-export) use the same device. Both show the pad picture for gamepad and touch.

## PS2 button → key

`keyFor(button, { context, mode })` maps a PS2 button to a key. The mapping was checked against the handlers that
really read the keys.

| button | menu (ui.js / fe-screens.js / career-ui.js keydown; = touch deck `MENU_KEYS`) | race (`pad-input.js KEYBOARD_BUTTONS`), Classic | race, Simple |
| --- | --- | --- | --- |
| Cross | Space (Enter also chooses) | Space | Space |
| Triangle | Esc | Y | Y |
| Square | Shift | Shift | Shift |
| Circle | Backspace | C | C |
| Start | Enter | Esc (main.js: Esc or Enter pauses) | Esc |
| Select | - | Backspace | Backspace |
| L1 R1 L2 R2 | Q E Z X | Q E Z X | Q E Z X |
| D-pad | arrows | I J K L | W A S D (the D-pad in the air) |
| left / right stick | arrows | W A S D / T F G H | same |

Rules behind the table:

- **Two keys for one action:** the key shown is the one the load screen panel shows (`keyboardRows`). Pause shows Esc,
  and the stick shows WASD before the arrows.
- **Which context applies:** every legend drawn by the FE and career screens uses the menu table. This includes the
  pause, quit and give-up menus, the objectives card, Big Challenge prompts and the lobby, because those screens read
  menu keys. The race table is used by the in-race hints: the cutscene skip (Space, `crossDown`) and the Uber trick
  hint (R1/L1 + Square = E/Q + Shift).
- **Name entry (Fullkeyboard) overrides** (`fe-screens.js kbLui.keys`). Typing goes straight in there, so:
  - L1 "Shift" shows Shift;
  - R1 "Caps" shows Caps Lock;
  - Cross shows Enter (Space types a space).

## Where the icons are replaced

`glyphButton(page, sx, sy, sw, sh)` recognises the icon sprites of the FE controller page. FE_1-14 and OV_1-2 are the
same 256x256 page:

- **Face icons:** 20x20 boxes at x = 12 + 22k, y = 123, for Triangle, Square, Cross and Circle.
- **Shoulder badges:** 28x15 boxes at x = 121 for R and 151 for L, y = 157 for 1 and 174 for 2.

Call sites cut the icons with different source rectangles: LUI uses (56.5, 123.5, 20, 19), `ui.sprite` uses
(55, 122, 24, 24), and the Uber setup uses the `SHOULDER` rects. So the button is identified from the centre of the
rectangle, and the controller picture and the Start/Select bar don't match.

Hook points:

- **`lui-player.js LuiScreen.sprite`:** covers every LUI screen. That is the main menu, Select/Setup Character, Enter
  Cheat, Select Peak/Mode/Event (Show INFO), Options and all its sub-screens, the FE Yes/No popup, Previews,
  Save/Load, Rewards, Credits, Uber setup ("Hold L1/R1 ... then press Square"), the audio menus, the name-entry
  keyboard and the career highlights.
  - Where a screen stacks two icons on one spot (19game_opt keeps its Cross under the Triangle), the covered icon is
    dropped in keyboard mode. A cap of a different width would otherwise show from underneath.
- **`ui.js OriginalUI.sprite` (OV_1-2 cells):** covers the ui.js legends and all the career screens.
  - The ui.js legends are main, character, pause, results and options.
  - The career screens use `help()`: pause/MCOMM, messages, lodge (Equip Gear / Buy Gear), songs, peaks, goals,
    rewards, trophies, attributes, the objectives card "Continue", the quit and give-up dialogs, and the results menu.
  - The rest are the Big Challenge prompts (`big-challenges.js`), the audio Yes/No confirm (`audio-menu.js`) and the
    online lobby (`mp-ui.js`).
- **Title:** "Press START button" reads "Press Enter" in the same font with the keyboard.
- **`cutscenes.js`:** "Press [Space] to skip". The cap takes the icon's place, and "Press" is right-aligned to the cap.
- **`trick-hud.js`:** the Uber trick hint "UBER TRICK = [E] + [Shift]". `frame(slots, { keys })` lays the caps out at
  their own widths, so the line stays centred on x = 320, and `TrickHudRenderer` draws them with its own FEFONT
  atlas.
- **`phone-prompts.js`:** the option icon of the phone prompt boxes.

**Layout rules:** a cap is centred on the icon's box and is 0.85 of the box high. That gives 16 in an 18-high FE
legend slot, the panel's own cap height. The cap is never lower than 14 lines of the 448 frame (`MIN_CAP`), so the
label stays readable beside the 13-line career icons. It keeps the icon's right edge, because hint text follows the
icon, and long labels grow to the left.

Two hand-laid legends make room for the wider caps, using `glyphKeyRect`, which gives the cap a draw would make:

- The message viewer's row (`career-messages.js`) shifts an entry right, clear of the previous label.
- `career-ui.js help()` wraps its help text short of the leftmost cap.

## Checks

- **Tests:** `web/test-input-glyphs.mjs` (in `npm test`) covers:
  - the menu and race mapping for Classic and Simple, against the touch deck's `MENU_KEYS` / `PAD_KEYS`;
  - that `buildPad` with each race key presses that PS2 channel;
  - that the key names match the load screen panel;
  - atlas icon recognition;
  - that cap placement keeps the right edge, is centred and respects the minimum height;
  - that cap widths equal the load screen's `keycapWidth`;
  - the Uber hint keys and centring;
  - device switching: default, connected pad, edge-triggered buttons and sticks, touch deck, forced device.
- **Screens:** checked in headless Chrome for Testing over CDP, with real key presses for the keyboard runs and a
  `navigator.getGamepads` override for the gamepad runs. In gamepad mode every menu legend region is pixel-identical
  to before the change. The screenshots and scripts are in `local/browser-validation/input-glyphs/`:
  - `capture.mjs TAG none|kb|pad fe ctm race extra lodge` takes the shots;
  - `diff.mjs` compares two runs;
  - `hud.mjs` renders the Uber hint.
