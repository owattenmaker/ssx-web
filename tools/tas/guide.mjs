// A guide line and a follow policy (docs/tas.md "Search"): the line is a list of points (metres, the core's native frame: y up) a
// rider followed; the policy steers the left stick towards the point `lookahead` metres further along it, tucks (left stick
// up) and can hold boost. It only seeds the search: the search then edits the pad it produced.
import { neutralFrame, withButton } from './pad-format.mjs';

// points: [[x, y, z], ...] -> {points, cum (metres along the line)}
export function makeGuide(points) {
  const cum = [0];
  for (let i = 1; i < points.length; i++) {
    const a = points[i - 1];
    const b = points[i];
    cum.push(cum[i - 1] + Math.hypot(b[0] - a[0], b[2] - a[2]));
  }
  return { points, cum, length: cum[cum.length - 1] };
}

// The nearest guide index to p in a window after `from` (the rider moves forward along the line).
export function nearestIndex(guide, p, from = 0, window = 200) {
  let best = from;
  let bestD = Infinity;
  const end = Math.min(guide.points.length, from + window);
  for (let i = Math.max(0, from - 20); i < end; i++) {
    const q = guide.points[i];
    const d = (q[0] - p[0]) ** 2 + (q[1] - p[1]) ** 2 * 0.25 + (q[2] - p[2]) ** 2;
    if (d < bestD) {
      bestD = d;
      best = i;
    }
  }
  return { index: best, distance: Math.sqrt(bestD) };
}

export function pointAhead(guide, index, metres) {
  const want = guide.cum[index] + metres;
  let i = index;
  while (i < guide.points.length - 1 && guide.cum[i] < want) i++;
  return guide.points[i];
}

const wrap = (a) => Math.atan2(Math.sin(a), Math.cos(a));

// A stateful follower: next(state) -> pad frame. Heading h = atan2(dx, dz) of the motion; a left stick (byte < 128) raises h.
export function createFollower(guide, { lookahead = 12, gain = 3, tuck = true, boost = false } = {}) {
  let index = 0;
  let last = null;
  return {
    get index() {
      return index;
    },
    next(state) {
      const p = [state[0], state[1], state[2]];
      index = nearestIndex(guide, p, index).index;
      const target = pointAhead(guide, index, lookahead);
      let h = state[3];
      if (last) {
        const dx = p[0] - last[0];
        const dz = p[2] - last[2];
        if (dx * dx + dz * dz > 1e-6) h = Math.atan2(dx, dz);
      }
      last = p;
      const want = Math.atan2(target[0] - p[0], target[2] - p[2]);
      const err = wrap(want - h);
      const steer = Math.max(-1, Math.min(1, err * gain));
      let f = neutralFrame();
      // err > 0: turn left (stick byte towards 0)
      f.sticks[0] = Math.max(0, Math.min(255, Math.round(128 - steer * 127.5)));
      if (tuck) f.sticks[1] = 0;
      if (boost) f = withButton(f, 'Square');
      return f;
    }
  };
}
