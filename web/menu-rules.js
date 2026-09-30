// pv ps2MenuInput: which CTM menus wrap and which stop with the error sound, by the port's screen ids (docs/ctm-decomp-screens.md).
// Wrap is the runtime UIMenu +0x14 bit 7 (read from the live menu object in PS2 savestates where noted "live", else the LUI element
// flag 0x80). A menu that does not wrap plays the focused item's error kind at a blocked end: kind[0], default 4 (snd 0xD, 0x39B5E4).
export const PS2_WRAP_SCREENS = new Set([
  'ctm-mcomm', 'ctm-pause', 'ctm-bcpause', 'ctm-lodge', 'ctm-enterlodge', 'ctm-peaks',          // live (bcpause: 31paus_freeride LUI)
  'ctm-quit', 'ctm-quitsave', 'ctm-giveup', 'ctm-restart', 'ctm-bcsure', 'ctm-saveprompt',       // 87yndialog Yes / No (saveprompt: the lodge FE popup)
  'ctm-results', 'ctm-records', 'ctm-details', 'ctm-attributes',                                 // 43/42/70 standings, 61toptimes, 155, 33buyattribs (62reward_list: 0x1FF65C clears the wrap bit)
  'ctm-rewards', 'ctm-gear', 'ctm-uber', 'ctm-buy',                                              // lodge LUI menus (MnuGroups, 66ut_btnmap, 139buy_popup)
]);
export const PS2_END_ERROR_SCREENS = new Set(['ctm-goals', 'ctm-events', 'ctm-confirm', 'ctm-gopeak', 'ctm-session', 'ctm-sessconfirm']);   // gopeak / confirm: the Map's own ConfirmMenu (flags 0x255)
// Up / Down on a generic list of n items: the new index (wrapping on the PS2's wrap menus), or null for "use the port's rule".
export function ps2MenuStep(screen, index, n, d) { return PS2_WRAP_SCREENS.has(screen) && n > 0 ? ((index + d) % n + n) % n : null; }

// pv ps2MenuInput: the screen intro's input lockout, in frames (web/screen-phases.js runs it). A screen's LUI timeline (cUIScreen_playFrame 0x39C870, one LUI frame per game frame)
// activates its menu at the frame of its 0x42 label record (0x39CE20: state +0x1C status 3); until then UIMenu 0x39B000 returns at once
// (menu +0x14 bit 4 clear, no focused item +0xA0), so every menu input (Up / Down, Cross / Start, Triangle, Square, Circle) is dead.
// Accepted from the activate frame + 1 (ARMSX2: MCOMM activate 60, a press at +60 dead, +62 taken; 87yndialog activate 30, +31 taken).
// Frames from the LUI data (local/ctm-decomp/screens/lui_menus.json); only ctm-mcomm and the 87yndialog screens are measured so far.
// The MCOMM replays its intro (the full lockout) whenever it comes back from a sub-screen (0x39EA90 restarts it at phase 2), as any
// change into a screen here does. The Map's input opens 31 frames after the Cross (measured).
const INTRO_ACTIVATE = new Map([
  ['ctm-mcomm', 60], ['ctm-pause', 60], ['ctm-bcpause', 60],                              // 31paus_freeride (measured)
  ['ctm-quit', 30], ['ctm-quitsave', 30], ['ctm-giveup', 30], ['ctm-restart', 30], ['ctm-bcsure', 30],   // 87yndialog, the 0x20D338 family only (measured)
  ['ctm-saveprompt', 26],   // the lodge's FE cFEPopupConfirm: 25-step box growth, about 27 frames dead (ctm-gopeak is the Map's own confirm: not gated here)
  ['ctm-enterlodge', 38], ['ctm-session', 48],
  ['ctm-objectives', 30], ['ctm-results', 30], ['ctm-records', 30], ['ctm-award', 30],
  // the lodge's FE.LUI screens activate at 25; ui.set runs at the FE flash's switch (about Cross + 11), so 25 + 2 from there puts input at
  // about press + 38 (ARMSX2 lodge-details: +32 dead, +40 taken; the lodge code trace: focus bar at press + 38). The buy popup and the
  // cheat list open without a flash: 30 + 2 / 31 + 2 from their own open.
  ['ctm-lodge', 26], ['ctm-details', 26], ['ctm-attributes', 26], ['ctm-gear', 26], ['ctm-trophies', 26], ['ctm-rewards', 26], ['ctm-uber', 26], ['ctm-buy', 31],
]);
const MAP_SCREENS = new Set(['ctm-peaks', 'ctm-goals', 'ctm-events', 'ctm-confirm', 'ctm-gopeak']);   // the Map ('Q& '): 30 when it opens; its levels are states of one screen
export function introLockFrames(screen, previous) {
  if (MAP_SCREENS.has(screen)) return MAP_SCREENS.has(previous) ? 0 : 31;
  if ((screen === 'ctm-session' && previous === 'ctm-sessconfirm') || screen === 'ctm-sessconfirm') return 0;   // the Session's confirm is in-screen
  const a = INTRO_ACTIVATE.get(screen); return a === undefined ? 0 : a + 1;
}

// pv ps2MenuInput: the Yes / No outro, in frames (web/screen-phases.js leave). After the choice the popup plays its TransitionOut (0x43, menu bit 4 off, phase 6: no input) to its
// Stop (0x41) before the choice acts: 87yndialog 50 -> 70 = 20 frames, 98enterlodge 105 -> 130 = 25; then the next screen runs its own
// intro (the MCOMM / pause: 61) a pass after the pop, so No -> pause input comes at +83 (docs/ctm-decomp-screens.md E9).
export const PS2_CHOICE_OUTRO = new Map([['ctm-quit', 20], ['ctm-quitsave', 20], ['ctm-giveup', 20], ['ctm-restart', 20], ['ctm-bcsure', 20], ['ctm-enterlodge', 25]]);
