// Screen phases: the PS2 UI state stack's per-screen phase machine (docs/pause-contexts.md 2; docs/ctm-decomp-screens.md E7..E10).
// The state stack update 0x39ECB0 (phase = state +0x1C bits 8..13, jump table 0x493FC0) moves each screen through:
//   2 build (0x39E2A0 writes 0x218) -> 4 wait for the LUI timeline's 0x42 activate record (0x39CE20) -> 3 show / cursor restore (+0x34)
//   -> 5 active: the only phase whose update reads input (+0x64 -> 0x20E900 -> 0x39E510)
//   -> 6 exit: TransitionOut 0x43 (0x39CE48, menu bit 4 off): drawn, frozen, no input -> 7 Stop 0x41 (0x39CE98) -> the next screen.
// One pass per UI frame (60 Hz; the timeline advances one LUI frame per update, thread tick 0x39C978), counted in frames, never
// wall-clock. The frame numbers per screen are web/menu-rules.js's (the LUI timelines' activate / TransitionOut / Stop frames); they
// apply with pv ps2MenuInput (without it every screen takes input at once and acts on the press, as before).
//   - A screen entered from outside the pass (a key, the game frame's Start) builds on the current frame; one entered by an exit action
//     inside the pass builds on the next one. That is E9's pop / restart pipeline: the Yes / No stops, the next pass pops it and
//     restarts the pause at phase 2 (0x39EA90(stack, 8)), which builds a pass later, so No -> pause input comes at +20 +2 +61 = +83.
//   - Returning to a screen replays its intro (E10: 0x39EA90 restarts the MCOMM at phase 2 after every sub-screen).
//   - A screen can own a pause context for its lifetime (web/pause-contexts.js): the round card pushes context 1 at
//     its open (WS2 enter 0x236BB0) and pops it when it closes (WS2 exit 0x236C88).
import { CTX } from './pause-contexts.js';
import { introLockFrames, PS2_CHOICE_OUTRO } from './menu-rules.js';

export const PHASE = Object.freeze({ BUILD: 2, SHOW: 3, WAIT: 4, ACTIVE: 5, EXIT: 6, STOP: 7 });
const SCREEN_CONTEXT = new Map([['ctm-objectives', CTX.CARD]]);
const FRAME_MS = 1000 / 60;

// rules(): the PS2 frame rules are on (pv ps2MenuInput). contexts(): the page's pause contexts (null in tests without them).
export function createScreenPhases({ rules = () => true, contexts = () => null, lockFrames = introLockFrames, outroFrames = (s) => PS2_CHOICE_OUTRO.get(s) || 0 } = {}) {
  let frame = 0, screen = null, startedAt = 0, lock = 0, exit = null, inPass = false, screenCtx = null, last = null;
  function phase() {
    if (exit) return exit.stopped ? PHASE.STOP : PHASE.EXIT;
    if (!lock) return PHASE.ACTIVE;
    const d = frame - startedAt;
    return d >= lock ? PHASE.ACTIVE : d <= 0 ? PHASE.BUILD : d === lock - 1 ? PHASE.SHOW : PHASE.WAIT;
  }
  function step() {
    frame++; inPass = true;
    try {
      if (exit?.stopped) { const run = exit.action; exit = null; run(); }       // phase 7: pop, then the choice acts
      else if (exit && frame - exit.at >= exit.frames) exit.stopped = true;      // the TransitionOut reached its Stop record
      contexts()?.audit?.(screen);
    } finally { inPass = false; }
  }
  return {
    PHASE,
    get frame() { return frame; },
    get screen() { return screen; },
    get phase() { return phase(); },
    // ui.set: the new screen starts at phase 2 (a change of screen only; setting the same screen again keeps its phase)
    enter(next, previous) {
      exit = null; screen = next; startedAt = inPass ? frame + 1 : frame;
      lock = rules() ? lockFrames(next, previous) : 0;
      const c = contexts(), want = SCREEN_CONTEXT.get(next) ?? null;
      if (screenCtx && screenCtx.ctx !== want) { c?.pop(screenCtx.h); screenCtx = null; }
      if (want != null && !screenCtx && c) screenCtx = { ctx: want, h: c.push(want, { owner: next }) };
    },
    // the capture-phase key gate (web/ui.js): only phase 5 reads input
    accepts() { return phase() === PHASE.ACTIVE; },
    // A choice on a screen with an exit: play phase 6 for its frames, then run the action (queued here, not on a timer). False when the
    // screen has none (act at once). While an exit plays, further choices are swallowed (no input in phase 6).
    leave(from, action) {
      if (from !== screen) return false;
      if (exit) return true;
      const n = rules() ? outroFrames(from) : 0; if (!n) return false;
      exit = { at: frame, frames: n, action, stopped: false }; return true;
    },
    get leaving() { return !!exit; },
    // frames until the current screen takes input (QA; Infinity while it exits)
    framesToActive() { return exit ? Infinity : Math.max(0, startedAt + lock - frame); },
    step,
    // The page's 60 Hz UI frame clock is the pad menus' rAF loop (web/gamepad-menus.js installPadMenus), so pad, keyboard and touch are
    // gated in one place: step() once per PS2 pad update, before its keys; advance(t) by time for the port's pad model (60 passes a
    // second whatever the display rate; a long gap, a background tab, counts one).
    advance(now) {
      if (last === null || now - last > 250) last = now - FRAME_MS;
      const n = Math.min(4, Math.round((now - last) / FRAME_MS));
      for (let k = 0; k < n; k++) { last += FRAME_MS; step(); }
      return n;
    },
  };
}
