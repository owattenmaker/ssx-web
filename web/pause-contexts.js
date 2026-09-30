// Pause contexts: the PS2's freeze stack (docs/pause-contexts.md; docs/ctm-decomp-world-states.md 1.3).
// The stack is 0x5366E8 [gp-0x69C]; the live mask gp-0x6A0 is the table 0x4428F0[top context]. The game frame 0x230B00..0x230D50 runs
// each system only while its mask bit is clear. The view update 0x22E840 (render, camera output, view fades) is never masked, so a
// paused frame keeps drawing. The audio pause 289B70 / 289BB8 is not a mask bit: the pusher asks for it (the pause menu does, the Big
// Challenge prompt does not).
//
// Whoever freezes something pushes a context, keeps the handle and pops it; pop is idempotent. Nothing else changes the pause state.
// `stops(bit)` is the derived read (main.js: paused = stops(SIM)).

export const BIT = Object.freeze({
  SIM: 0x01,    // stage world 309270, 2294C8, 3440C8, 357BF8, timer channels 1 / 5 / 6, the replay manager 26F4A8
  T1: 0x02,     // timer channel 1 and 355028
  T3: 0x04,     // timer channel 3 (purpose unconfirmed)
  NIS: 0x08,    // the NIS tick (nis vt+0x28 at 0x230BDC): script clock, camera, actors, NIS sounds
  T2: 0x10,     // timer channel 2
  WORLD: 0x20,  // the world-state tick
});

// Contexts the port pushes. The numbers are the PS2's (table 0x4428F0).
export const CTX = Object.freeze({
  CARD: 1,      // WS2 enter 0x236BB0: the round card
  MENU: 2,      // the pause menu / MCOMM (0x230A94), the lodge prompt / map (WS14 0x236504)
  PROMPT: 3,    // a Big Challenge offer / fail (0x2308B4 / 0x230954), pushed without the audio pause
  // Port only. The PS2 does these freezes through the world-state machine: the ride's systems do not run outside WS4 (WS1 intro
  // lists, WS10 arrival cuts, an event's load). The port's tick loop has no world-state gate, so these holders stop the
  // simulation only (the NIS keeps playing).
  HOLD: 'hold',
});
const MASKS = new Map([[0, 0], [1, 0xFFFFFFDB], [2, 0xFFFFFFDF], [3, 0xFFFFFFDF], [5, 0xFFFFFFDF], [4, 9], [7, 9], [9, 1], [10, 0xFFFFFFD7], [CTX.HOLD, BIT.SIM]]);
// (context 12, the controller overlay 0x2309BC, masks with [0x442920]: not read yet, and the port has no such overlay)

export function createPauseContexts({ warn = (m) => console.warn(m) } = {}) {
  const stack = [];                      // handles, bottom first
  const listeners = [];
  let audioHeld = false, simHeld = false, simLatched = false, serial = 0;
  const top = () => stack[stack.length - 1] ?? null;
  const mask = () => { const t = top(); return t ? MASKS.get(t.ctx) >>> 0 : 0; };
  function changed() {
    const audio = stack.some((h) => h.audio), sim = stops(BIT.SIM);
    const audioChanged = audio !== audioHeld, simChanged = sim !== simHeld;
    audioHeld = audio; simHeld = sim; if (sim) simLatched = true;
    if (audioChanged || simChanged) for (const f of listeners) f({ audio, sim, audioChanged, simChanged });
  }
  function stops(bit) { return (mask() & bit) !== 0; }
  return {
    // ctx: a CTX value. owner: a name for diagnostics. audio: this holder pauses the music and SFX (289B70).
    // ends(screen): true on the screens where the holder must already be gone (audited once a UI frame, web/screen-phases.js).
    push(ctx, { owner = '?', audio = false, ends = null } = {}) {
      if (!MASKS.has(ctx)) throw new Error(`Unknown pause context ${ctx}`);
      const h = { ctx, owner, audio: !!audio, ends, id: ++serial, warned: false };
      stack.push(h); changed(); return h;
    },
    pop(h) {
      const i = h ? stack.indexOf(h) : -1; if (i < 0) return false;
      stack.splice(i, 1); changed(); return true;
    },
    stops,
    // The game frame's read of the simulation bit. A push stops the frame it happens in; a pop lets the simulation run from the next
    // game frame. The PS2 pops in the UI pass, after that frame's systems have run, so the tick stops on the frame the pause opens
    // and resumes on the frame after the closing press is accepted (docs/ctm-decomp-screens.md E11).
    simFrame() { const s = stops(BIT.SIM), held = s || simLatched; simLatched = s; return held; },
    get audioPaused() { return audioHeld; },
    get top() { return top()?.ctx ?? 0; },
    has(h) { return !!h && stack.includes(h); },
    // f({ audio, sim, audioChanged, simChanged }) after a push / pop that changed the audio pause or the simulation bit
    onChange(f) { listeners.push(f); },
    // The safety net at a run start / stop (web/main.js): every handle still open there is a missing pop. Returns the owners.
    clear() { const gone = stack.splice(0).map((h) => h.owner); changed(); return gone; },
    // Dev check: a holder still open on a screen where its owner is gone is a bug (a frozen ride under 'game').
    audit(screen) {
      for (const h of stack) if (!h.warned && h.ends?.(screen)) { h.warned = true; warn(`pause context ${h.ctx} (${h.owner}) left open on ${screen}`); }
    },
    list() { return stack.map((h) => ({ ctx: h.ctx, owner: h.owner, audio: h.audio })); },
  };
}
