// pv padRing: the gamepad's samples between drawn frames, so each catch-up tick reads its own pad (docs/crash-motion.md "High-level
// play").
//
// The PS2 (ps2-frame-pacing.js): the pad writer 0x326B88 stores one sample per vblank into a 30-slot ring, and when a frame takes
// several vblanks every catch-up update consumes its own sample (0x326B48 -> 0x321298). The page reads the pad once per drawn frame,
// so at 40 fps (alternating 1- and 2-tick frames) about a third of the ticks repeat the tick before's pad. Browsers refresh the pads
// off the main thread every 4 ms (Chromium gamepad_provider.cc kPollingIntervalMilliseconds, Firefox WindowsGamepad.cpp
// kWindowsGamepadPollInterval); a poller on the main thread sees those refreshes whenever the thread is free between frames.
//
// The ring keeps a copy of the standard-layout pad (web/gamepad.js) each time it changes, with the time it was seen. A frame that runs
// several ticks asks padForTick for each: a tick whose time (the previous frame + (k + 1) / 60 s) is before the frame's own time gets
// the newest sample at or before it; the frame's last tick (at or past the frame time) gets the live pad, so no latency is added.
export const PAD_RING_SLOTS = 64;
export const PAD_RING_POLL_MS = 4;
export const PAD_RING_IDLE_MS = 100;

// A copy of a standard pad's buttons and axes (pad-input.js buildPad reads only these).
export function copyPad(pad) {
  const buttons = [];
  for (const b of pad.buttons) buttons.push({ pressed: !!b.pressed, touched: !!b.touched, value: +b.value || 0 });
  return { id: pad.id, index: pad.index, mapping: pad.mapping, connected: true, timestamp: pad.timestamp, buttons, axes: Array.from(pad.axes) };
}

function samePad(a, b) {
  if (a.axes.length !== b.axes.length || a.buttons.length !== b.buttons.length) return false;
  for (let i = 0; i < a.axes.length; i++) if (a.axes[i] !== b.axes[i]) return false;
  for (let i = 0; i < a.buttons.length; i++) {
    if (a.buttons[i].pressed !== !!b.buttons[i].pressed || a.buttons[i].value !== (+b.buttons[i].value || 0)) return false;
  }
  return true;
}

// read(): the active pad in the standard layout, or null (web/gamepad.js pollPads); now(): milliseconds on the rAF / event clock.
export function createPadRing({ read, now, slots = PAD_RING_SLOTS }) {
  const ring = [];
  return {
    // one poll: keeps the pad when it differs from the newest kept sample
    sample() {
      const pad = read();
      if (!pad) return;
      const last = ring.at(-1);
      if (last && samePad(last.pad, pad)) return;
      ring.push({ t: now(), pad: copyPad(pad) });
      if (ring.length > slots) ring.shift();
    },
    // the newest sample at or before t; before the oldest kept sample, the oldest one; null when empty
    at(t) {
      for (let i = ring.length - 1; i >= 0; i--) if (ring[i].t <= t) return ring[i].pad;
      return ring.length ? ring[0].pad : null;
    },
    clear() {
      ring.length = 0;
    },
    get size() {
      return ring.length;
    }
  };
}

// The pad a frame's tick reads: tickMs = the tick's time, frameMs = the frame's time, live = the pad read for this frame.
export function padForTick(ring, tickMs, frameMs, live) {
  if (!ring || tickMs >= frameMs - 0.5) return live;
  return ring.at(tickMs) ?? live;
}

// Polls every PAD_RING_POLL_MS while active() (pv padRing on, a ride running), else checks again after PAD_RING_IDLE_MS.
// Returns stop().
export function startPadPoller(ring, active, timer = setTimeout, cancel = clearTimeout) {
  let id = null;
  let stopped = false;
  const loop = () => {
    if (stopped) return;
    const on = active();
    if (on) ring.sample();
    else if (ring.size) ring.clear();
    id = timer(loop, on ? PAD_RING_POLL_MS : PAD_RING_IDLE_MS);
  };
  loop();
  return () => {
    stopped = true;
    if (id != null) cancel(id);
  };
}
