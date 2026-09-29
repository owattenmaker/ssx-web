// When Start opens the pause (pv startRules; docs/visual-parity.md section 31). From the PS2 code:
//  - The game update 0x2306B8 opens the pause at 0x230A34 (push overlay 2, 0x20CA10; music pause 0x289B70; snd 0 0x294F48) only
//    when 0x231840 sees the pause action 0x3C (Start) on a human pad, and
//      * 0x231840: the object at gp-0x204 does not hold the game (+8 > 0: no pause), no NIS without flag 4 (0x231AB8);
//      * 0x230A44: the game phase *(game+0x28)+0 is 0 or >= 10 (1..9 blocked: 1 = results);
//      * 0x230A60: *(game+0x34)+0x70 != 1;
//      * 0x20CBE8: no transition (0x20CBA0) and no overlay screen, or the top one allows it (flags bit 0 clear, vtable +0xCC).
//    The lodge and the front end are not the game (game object 0 in the lodge state): Start never pauses there.
//  - Every LUI screen (the pause and MCOMM too, results, cards, the lodge, Transport, prompts, Equip Gear) reads Start as UINext
//    0x7A (input.map: Cross or Start): it accepts the focused item. The pause's Start "resumes" only because Return is focused.
// Pad captures (local/ps2-capture/menus/startprobe/*): riding, the countdown, a checkpoint: Start pauses; FINISH! banner: nothing
// until the results, where it is Continue; pause / MCOMM with the cursor on Restart / Transport: Start = that item; lodge, lodge
// prompt, Transport map, event list, Equip Gear, round card, results: Start = Cross; a transport ride / loading: nothing.
export function startOpensPause({ screen, running = false, paused = false, finished = false, cutscene = false } = {}) {
  return screen === 'game' && !!running && !paused && !finished && !cutscene;
}
// A screen where the pad's Start is the menu's accept (UINext): every screen but the ride itself and the ones with no menu.
const NO_MENU = new Set(['game', 'loading', 'cutscene', 'transition', 'title']);
export function startAccepts(screen) { return !NO_MENU.has(screen); }
