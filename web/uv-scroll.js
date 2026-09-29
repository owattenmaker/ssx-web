// UVScroll texture scrolling (builtin 21; engine/uv_scroll.hpp, bit-exact):
// fencebuv course fences, speed boosts, blue pinlights, podium/stage backdrops,
// e70slight, snowsheet/snowstream, river/bigriver/waterfall. Data:
// assets/UVSCROLL/uv-scroll.json (tools/export_uv_scroll.py).
//
// Call tick() once per 60 Hz game tick; offsetOf(resource) gives the texture
// translation (u, v) the original adds to every texture coordinate of that
// instance's model (0x35FC20 row-vector texture matrix, uv' = uv + (u, v)).
// With RepeatWrapping a three.js map.offset of (u, v) reproduces it.
import { add, sub, div, mul } from './ee-scalar-float.js';

// 0x35F7D0. s: {mode, timer, onTime, offTime, angle, spin, u, v, stepU, stepV, active}.
export function uvScrollTick(s, fps = 60) {
  if (!(0 < s.onTime) && !(0 < s.offTime)) return;
  const timer = add(s.timer, div(1, fps)); s.timer = timer;
  if (!s.active) {
    if (!(s.offTime <= timer)) return;
    s.timer = 0;
    if (0 < s.onTime) s.active = 1;
    return;
  }
  if (s.spin !== 0) {
    const twoPi = 6.2831854820251465, rate = s.spin;
    s.angle = add(s.angle, rate);
    if (twoPi <= rate) s.spin = sub(rate, twoPi); else if (rate <= twoPi) s.spin = add(rate, twoPi);   // source wraps the step
  }
  if (s.onTime <= s.timer) {
    s.timer = 0;
    if (0 < s.offTime) s.active = 0;
    if (s.mode === 2 || s.mode === 6) { s.stepU = -s.stepU; s.stepV = -s.stepV; }
  }
  if (!s.active) return;
  if (s.mode !== 6) { s.u = add(s.u, s.stepU); s.v = add(s.v, s.stepV); }
  else {
    const su = s.stepU, on = s.onTime, t = s.timer; let k;
    if (su < 0) k = t < mul(on, 0.5) ? t : sub(on, t);
    else { const left = sub(on, t); k = left < mul(on, 0.5) ? left : sub(on, left); }
    const inv = div(1, on);
    const dv = mul(mul(s.stepV, k), inv), du = mul(mul(su, k), inv);
    s.v = add(s.v, dv); s.u = add(s.u, du);
  }
  if (1 < s.u) s.u = sub(s.u, 1); else if (s.u < -1) s.u = add(s.u, 1);
  if (1 < s.v) s.v = sub(s.v, 1); else if (s.v < -1) s.v = add(s.v, 1);
}

export class UvScroll {
  // data = uv-scroll.json; instances start at their creation state (the lead may call
  // activate(resource) when a section streams in to restart one, as the original does).
  constructor(data, { fps = data.fps } = {}) {
    this.fps = fps;
    this.instances = data.instances.map((x) => ({ ...x, state: { ...x.initial } }));
    this.byResource = new Map(this.instances.map((x) => [x.resource, x]));
  }
  activate(resource) { const x = this.byResource.get(resource); if (x) x.state = { ...x.initial }; }
  tick() { for (const x of this.instances) uvScrollTick(x.state, this.fps); }
  offsetOf(resource) { const x = this.byResource.get(resource); return x ? [x.state.u, x.state.v] : null; }
}
