// Field reports of the gamepad's update rate as the page sees it (docs/crash-motion.md "pv padRing (removed) and the pad report rate").
// Firefox on macOS delivers an Xbox Wireless pad over Bluetooth at ~9.4 Hz where Chrome sees ~50 Hz from the same pad; this measures it
// on players' machines (Windows Firefox included) with the pad-rate probe's metric, from the page's own per-frame pad reads.
//
// Only frames with a stick off centre (one of the first four axes past OFF_CENTRE) count: distinct stick states per second of that time (hz), the % of those
// frames whose stick value changed (changedPct), and the longest run of unchanged frames. Reporting only; nothing personal: the pad's
// name (60 characters), vendor / product, the browser's mapping and the port's layout.
export const OFF_CENTRE = 0.2;
export const REPORT_AFTER_S = 10;
export const MAX_PADS = 4;

// The sticks: the first four axes, in the standard mapping and in the raw layouts the port knows (Firefox's descriptor order puts
// the triggers and the hat after them; a trigger resting at -1 must not count as a stick off centre).
const STICK_AXES = 4;
function key(pad) {
  let k = '';
  for (let i = 0; i < Math.min(STICK_AXES, pad.axes.length); i++) k += (+pad.axes[i]).toFixed(4) + ',';
  return k;
}

// send(kind, data): diagnostics.js diagnose; heartbeat(summary): diagnostics.js diagPadRate (the next heartbeats carry it).
export function createPadRateWatch(send, heartbeat) {
  const pads = new Map();

  function summary(s) {
    const seconds = s.offMs / 1000;
    return {
      pad: s.name,
      vendor: s.vendor,
      product: s.product,
      mapping: s.mapping,
      layout: s.layout,
      hz: seconds > 0 ? Math.round((10 * s.changes) / seconds) / 10 : 0,
      changedPct: s.frames ? Math.round((100 * s.changes) / s.frames) : 0,
      longest: s.longest,
      frames: s.frames,
      seconds: Math.round(seconds)
    };
  }

  // pad: the raw Gamepad the page read this frame (its axes as the browser reports them); info() -> { name, vendor, product, layout },
  // asked once per pad; dtMs: the time since the last frame.
  function frame(pad, info, dtMs) {
    if (!pad || !pad.axes) return;
    const id = String(pad.id ?? '');
    let s = pads.get(id);
    if (!s) {
      if (pads.size >= MAX_PADS) return;
      info = typeof info === 'function' ? info() : info;
      s = { name: String(info?.name ?? id).slice(0, 60), vendor: info?.vendor ?? null, product: info?.product ?? null,
        mapping: pad.mapping || '-', layout: info?.layout ?? '', last: null, frames: 0, changes: 0, run: 0, longest: 0, offMs: 0, sent: false };
      pads.set(id, s);
    }
    const k = key(pad);
    const changed = k !== s.last;
    s.last = k;
    let off = false;
    for (let i = 0; i < Math.min(STICK_AXES, pad.axes.length); i++) if (Math.abs(+pad.axes[i]) > OFF_CENTRE) off = true;
    if (!off) return;
    s.frames++;
    s.offMs += Math.min(Math.max(dtMs || 0, 0), 100);
    if (changed) {
      s.changes++;
      s.run = 0;
    } else {
      s.run++;
      s.longest = Math.max(s.longest, s.run);
    }
    if (!s.sent && s.offMs >= REPORT_AFTER_S * 1000) {
      s.sent = true;
      send('pad-rate', summary(s));
    }
    if (s.sent) heartbeat(summary(s));
  }

  return { frame, summary: (id) => (pads.has(id) ? summary(pads.get(id)) : null) };
}
