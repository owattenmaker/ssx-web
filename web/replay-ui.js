// The full replay's overlay (the results' Replay item): cOVState_REPLAY, OV.LUI '64replay' (tools/export_replay_screens.py ->
// /assets/UI/replay-screens.json; docs/replay.md). The replay itself is web/replay.js.
//   top      'Replay', the timeline (green flag .. checkered flag) and its marker percentcomplete at frame / length x 315 + 170
//            (0x20E818 setupTicker); Square hides / shows it (ReplayTimeLine)
//   bottom   the help panel: Camera - <name> (0x20E6B0 setupCameraName, ASCII in the ELF), Skip backward L1 / Skip forward R1,
//            Change camera, Slow motion, Play/Pause, Timeline, Manual cam, Exit; D-pad down / up slides it off / back
//            (ReplayMenuDown / Up, 4 px a frame between y 245 and 480)
//   popup    Start: the Replay Menu (Save replay / Exit replay / Continue; Triangle closes it). Exit replay goes back to the
//            results and the replay behind them starts again (0x26F980 then 0x2706B8).
// Pad (the keys the menus read, web/touch-controls.js MENU_KEYS / web/gamepad-menus.js): Cross Space, Circle Backspace,
// Triangle Escape, Square Shift, L1 Q, R1 E, Start Enter, D-pad arrows; the Manual camera takes the pad's sticks (or J L / I K /
// U O on a keyboard).
import { LuiScreen } from './lui-player.js';
import { pollPads } from './gamepad.js';

const SY = 448 / 480;
const OPEN = 39;                                    // the screen's settled layout (the open animations end by frame 39)
const EL = {
  popup: '0bf64340', items: ['0158af09', '065e0089', '04575695'], cursor: '007a531c', menu: '028924b5',
  bottom: '0696bb5d', camera: '06983c81', top: '00007b60', marker: '077e42c5',
  hideHelp: '0fabe9f0', showHelp: '0f6de710', hideTimeline: '05e77905', showTimeline: '0306f505',
};
const PANEL_UP = 245, PANEL_DOWN = 480, PANEL_STEP = 4;
const MENU_FOCUS = [50, 60, 70];

export class ReplayUi {
  // replay: web/replay.js; host: { exit() back to the results, save() (no memory card: disabled) }
  constructor(ui, replay, host = {}) {
    this.ui = ui; this.replay = replay; this.host = host; this.lui = null;
    this.openAt = 0; this.panel = PANEL_UP; this.panelDir = 0; this.timeline = true; this.menu = -1; this.circle = false; this.keys = new Set();
  }
  async load() {
    try {
      const data = await (await fetch('/assets/UI/replay-screens.json')).json(), images = this.ui.images;
      await Promise.all(data.pages.filter((p) => !images[p]).map(async (p) => { const im = new Image(); im.src = `/assets/UI/${p}.png`; await im.decode(); images[p] = im; }));
      this.lui = new LuiScreen(data.screens['64replay'], images, this.ui); this.lui.shapeScale = true; this.lui.unionFlat = true; this.lui.keepLead = true;
    } catch (e) { console.warn('Replay overlay missing (python3 tools/export_replay_screens.py)', e); }
  }
  get ready() { return !!this.lui; }
  owns(screen) { return screen === 'replay'; }
  now() { return performance.now() * 60 / 1000; }
  // The screen opens (dialog 0x11 pushed): panel up, timeline shown, no popup.
  open() { this.openAt = this.now(); this.panel = PANEL_UP; this.panelDir = 0; this.timeline = true; this.menu = -1; this.circle = false; this.keys.clear(); }
  // Per frame (main.js): the panel slide and the Manual camera's sticks.
  frame() {
    if (this.panelDir) { this.panel = Math.max(PANEL_UP, Math.min(PANEL_DOWN, this.panel + this.panelDir * PANEL_STEP)); if (this.panel === PANEL_UP || this.panel === PANEL_DOWN) this.panelDir = 0; }
    let x = 0, y = 0, z = 0;
    const pad = pollPads();   // the active standard-layout pad (web/gamepad.js)
    if (pad?.axes?.length >= 4 && this.menu < 0) { const dz = (v) => (Math.abs(v) < 0.2 ? 0 : v); x = dz(+pad.axes[0] || 0); y = dz(+pad.axes[1] || 0); z = dz(+pad.axes[2] || 0); }
    const k = this.keys; if (this.menu < 0) { if (k.has('KeyJ')) x = -1; if (k.has('KeyL')) x = 1; if (k.has('KeyI')) y = -1; if (k.has('KeyK')) y = 1; if (k.has('KeyU')) z = -1; if (k.has('KeyO')) z = 1; }
    this.replay.setManual(x, y, z);
  }
  key(e) {
    const r = this.replay, code = e.code;
    if (['KeyJ', 'KeyL', 'KeyI', 'KeyK', 'KeyU', 'KeyO'].includes(code)) { this.keys.add(code); return true; }
    if (e.repeat) return true;
    if (this.menu >= 0) {   // the Replay Menu popup
      if (code === 'ArrowUp') { this.menu = (this.menu + 2) % 3; this.ui.gameAudio?.ui?.(2); }
      else if (code === 'ArrowDown') { this.menu = (this.menu + 1) % 3; this.ui.gameAudio?.ui?.(2); }
      else if (code === 'Escape') { this.menu = -1; this.ui.gameAudio?.ui?.(3); }
      else if (code === 'Space' || code === 'Enter') {
        const pick = this.menu;
        if (pick === 0) { this.ui.gameAudio?.ui?.(4); return true; }   // Save replay: no memory card in the browser (greyed)
        this.ui.gameAudio?.ui?.(6); this.menu = -1;
        if (pick === 1) this.host.exit?.();
      }
      return true;
    }
    switch (code) {
      case 'Space': r.playPause(); break;                                   // ReplayPlay (Cross)
      case 'Backspace': this.circle = true; r.stepOnce(); break;            // ReplayStepForward (Circle), held: ReplayStepSlow
      case 'KeyQ': r.skip?.(-1); break;                                     // ReplayFBackward (L1)
      case 'KeyE': r.skip?.(1); break;                                      // ReplayFForward (R1)
      case 'Escape': r.cycleCamera(); this.ui.gameAudio?.ui?.(2); break;    // ReplayCycleCamera (Triangle)
      case 'ShiftLeft': case 'ShiftRight': this.timeline = !this.timeline; break;   // ReplayTimeLine (Square)
      case 'ArrowUp': this.panelDir = -1; this.ui.gameAudio?.ui?.(2); break;        // ReplayMenuUp
      case 'ArrowDown': this.panelDir = 1; this.ui.gameAudio?.ui?.(2); break;       // ReplayMenuDown
      case 'Enter': r.pause?.(); this.menu = 0; this.ui.gameAudio?.ui?.(6); break;  // ReplayExit (Start): the Replay Menu
      default: return false;
    }
    return true;
  }
  keyup(e) {
    this.keys.delete(e.code);
    if (e.code === 'Backspace' && this.circle) { this.circle = false; this.replay.setSlow(false); }
  }
  // Circle held for 10 ticks: slow motion (main.js polls this each frame while the key is down).
  held(frames) { if (this.circle && frames >= 10) this.replay.setSlow(true); }
  draw(c) {
    if (!this.lui) return;
    const t = Math.min(OPEN, this.now() - this.openAt), r = this.replay;
    const events = this.lui.screen.events.filter((ev) => ev.frame <= t || (this.menu >= 0 && ev.frame === MENU_FOCUS[this.menu])).map((ev) => ({ ev, start: ev.frame }));
    const shift = this.panel - PANEL_UP;
    c.save(); c.scale(1, SY);
    this.lui.draw(c, events, t, (e) => {
      const n = e.name;
      if (n === EL.popup) return this.menu >= 0 ? null : { hidden: true };
      if (n === EL.items[0] && this.menu >= 0) return { alpha: 128 };   // Save replay: no memory card
      if (n === EL.camera) return { text: r.cameraName };
      if (n === EL.bottom) return { props: { 1: this.panel } };
      if (n === EL.hideHelp) return this.panel === PANEL_UP ? { props: { 1: 241 + shift } } : { hidden: true };
      if (n === EL.showHelp) return this.panel === PANEL_DOWN ? null : { hidden: true };
      if (n === EL.top) return this.timeline ? null : { hidden: true };
      // 'hidetimeline' is the prompt under the shown bar (y 86, PS2 replay/rmenu1), 'showtimeline' the one left when it is hidden
      if (n === EL.hideTimeline) return this.timeline ? { alpha: 255 } : { hidden: true };
      if (n === EL.showTimeline) return this.timeline ? { hidden: true } : null;
      if (n === EL.marker) return { hidden: true };   // drawn below: its texture is set at run time
      return null;
    });
    // percentcomplete (0x20E818 setupTicker): an 11 x 11 marker at frame / length x 315 + 170 in the bar's group (-6, -52) under
    // 'top' (0, 20), flat grey 128 (PS2 replay/rmenu2.f00700: 128,128,128), fading in with the bar (frames 16..21)
    if (this.timeline && t >= 16) { c.globalAlpha = Math.min(1, (t - 16) / 5); c.fillStyle = 'rgb(128,128,128)'; c.fillRect(164 + Math.round(r.progress * 315), 54, 11, 11); c.globalAlpha = 1; }
    c.restore();
  }
}
