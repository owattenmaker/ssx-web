// pv lodgeFlash (docs/visual-parity.md 42): the lodge's screen changes as the PS2 makes them. Every lodge LUI screen is an FE state
// (28lodge cFEStateLodge, 155rider_details_conquer, 125mountainroom / 126peakroom / 127trophyroom, 93profile_load cFEStateProfileLoad,
// 33buyattribs, 128rewardsroom / 129 / 130, 66ut_btnmap, 14rid_prof, the career highlights), and a change of state is 0x39F400: the
// old state goes to phase 6 (flags +0x1C bits 8..13 = 6), the new one waits in phase 1. The old state's LUI plays its 'TransitionOut'
// label (0x1F3C4C for the lodge's items; 0x1F4BDC Buy Attributes' Triangle): control 0x43 there, then 0x30 ... 0x00AB3C45 starts the
// transition_flash screen (anim 02ed6393, a white quad: A 0 -> 255 over 10 frames, 255 -> 0 over 9), and 9 frames on, the end label
// 0005ab60's control 0x41 (0x39CE98) puts the old state in phase 7: 0x39EEE4 exits it (vt+0x58, vt+0x28) and activates the new one,
// whose LUI plays its intro under the fading white. PS2 local/ps2-capture/menus/tout-*: 125mountainroom's Triangle (0x1D4698, no
// TransitionOut call of its own), the lodge's Save Game (value 8, 0x1F3BEC) and the lodge's Rider Details (value 4, 0x1F3C4C) give the
// same frames: white 0.29 / 0.79 at 5 / 10 frames after the press, full at 12, Rider Details' intro under it at 16 and clear by 22.
// A child state pushed over a screen (0x39F290: the Player Name keyboard cKeyboardPopup, Cheat Characters, the popups) changes no
// state and plays no TransitionOut. One object per UI (web/career-ui.js lodgeFlash; Buy Attributes' own for its tests); every lodge
// screen change goes through go(to). draw(c) is called after whichever screen is up (career-ui.js for its screens, fe-screens.js /
// audio-menu.js / wardrobe.js for the ones they draw for the lodge) and runs `to` at full white.
import { pv } from './pv-flags.js';

// transition_flash's A track (anim 02ed6393): 0 -> 255 over 10 frames, 255 -> 0 over 9 (docs/career-events.md "Transitions")
export const FLASH_IN = 10, FLASH_OUT = 9;
export function flashAlpha(t) { return t < FLASH_IN ? Math.max(0, t) / FLASH_IN : Math.max(0, 1 - (t - FLASH_IN) / FLASH_OUT); }
// pv introLead: the new state's LUI is 2 frames into its intro when it first shows. 0x39EEE4 exits the old state and activates the
// next in the same pass of the state manager 0x39ECB0, and the new state's phase-2 case (0x39ED4C) runs its enter (vt+0x20) and the LUI
// update 0x39E868, then falls into the phase-4 case, whose tail updates the LUI again (0x39EED4). PS2 tout-detD: Rider Details' menu
// texts at 27 / 30 / 36 frames after the press match the page's intro 2-3 frames later than a frame-0 start.
export const INTRO_LEAD = 2;
// pv introLead: a restoring state's menu focus shows 2 frames after its intro label's 0x42 (intro done: phase 3 during that LUI update):
// the next pass's phase-3 case runs vt+0x30 (0x186518 -> 39B960 sets the focus), whose label shows on the following update. PS2
// tout-detD: Rider Details' focus bar absent 26, present 28 frames into its intro.
export const FOCUS_LAG = 2;

export class LuiFlash {
  constructor(now = () => performance.now() * 60 / 1000) { this.now = now; this.flash = null; this.switchedAt = null; }
  get active() { return !!this.flash; }
  // Start the state change: `to` switches the screen (runs at full white). A change already under way keeps its own.
  go(to) { if (this.flash) return false; this.flash = { at: this.now(), to, done: false }; return true; }
  // Only the fall: a screen another module left at full white (its own whitefade: web/audio-menu.js back) comes back here.
  fall() { this.flash = { at: this.now() - FLASH_IN, to: () => {}, done: true }; this.switchedAt = this.flash.at + FLASH_IN; }
  alpha() { return this.flash ? flashAlpha(this.now() - this.flash.at) : 0; }
  // The time a screen's intro starts from, for a screen that starts drawing at `now`: INTRO_LEAD frames before the switch when the
  // switch opened it (it starts within 2 frames of it), else `now` (pv introLead).
  introStart(now) { return pv('introLead') && this.switchedAt != null && now - this.switchedAt >= 0 && now - this.switchedAt <= 2 ? this.switchedAt - INTRO_LEAD : now; }
  // Over the frame (640 x 448 UI canvas): the old screen under the rising white, the switch at full white, the new one under the fall.
  draw(c) {
    const f = this.flash; if (!f || !c) return;
    const t = this.now() - f.at;
    if (!f.done && t >= FLASH_IN) {
      f.done = true; this.switchedAt = f.at + FLASH_IN; const r = f.to();
      // a screen whose own data is not in yet (web/wardrobe.js open): full white until it is up, then the fall. The PS2's screens are
      // resident, so it never holds: pv equipLoading preloads Equip Gear's data and builds the outfit behind its "Loading..."
      if (r && typeof r.then === 'function') { f.wait = true; const done = () => { f.wait = false; const now = this.now(); f.at = now - FLASH_IN; this.switchedAt = now; }; r.then(done, done); }
    }
    if (f.wait) { c.save(); c.fillStyle = 'rgba(255,255,255,1)'; c.fillRect(0, 0, 640, 448); c.restore(); return; }
    if (t >= FLASH_IN + FLASH_OUT) { this.flash = null; return; }
    c.save(); c.fillStyle = `rgba(255,255,255,${flashAlpha(t)})`; c.fillRect(0, 0, 640, 448); c.restore();
  }
}
