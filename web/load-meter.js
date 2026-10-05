// The load screens' meter (pv loadMeter, docs/loading-screen.md "The meter is the load"). No imports: tested in node.
//
// A load is a list of stages, each weighted by the time it is expected to take:
// - work stages: their measured time (STAGE_MS). A stage reports its own fraction when it can (the warm-up's slices, the course
//   build's milestones); one that has begun without reports creeps towards CREEP_MAX of itself over its typical time and waits
//   there until it is done.
// - download stages ([id, {bytes}]): the bytes the load is expected to download (web/load-files.json), done = the bytes received
//   since the stage began (received(), downloads.js), weighted by bytes / the bandwidth measured while bytes arrive. A file the
//   manifest did not know grows the stage.
// A later plan (a world load that goes on into an event load) starts a new segment over what is left, so the fraction never goes
// backwards. 1 only when every stage is done.
export const CREEP_MAX = 0.9;
// bytes per ms before enough has arrived to measure (~48 Mbit/s), and the floor of the measured value
export const DEFAULT_BANDWIDTH = 6000;
const MIN_BANDWIDTH = 60;
const MEASURE_BYTES = 256 * 1024;
// the shown value's lead over a fraction that stands still: at most 2%, at 0.5% a second
const LEAD = 0.02;
const LEAD_RATE = 0.005 / 1000;
// a stage that stands for a later plan's stages (main.js switchCourse: the event load after a course switch)
export const PENDING = 'pending';

// Typical work times (ms) on this project's reference Mac (Chrome 1x and WebKit, local server, so with no download time;
// docs/loading-screen.md "The meter is the load"): only their ratios matter. course: an in-page course build (PEAK1 2.5 s, ARA1 1.9 s).
export const STAGE_MS = Object.freeze({
  course: 2500,
  rider: 100,
  lineup: 100,
  warm: 3000,
  intro: 600,
  unload: 100,
  ride: 700,
  world: 1800,
  gc: 200,
  // an event load's own stages after a course switch: rider + lineup + warm + intro
  pending: 3800
});

// received(): every byte downloaded so far in the page (cumulative); inflight(): the bytes still to come of the downloads under way
// (their known sizes)
export function createLoadMeter({ now = () => performance.now(), received = () => 0, inflight = () => 0 } = {}) {
  let stages = new Map();
  let base = 0;
  let shown = 0;
  let lastAt = null;
  // the bandwidth: bytes over the time in which bytes arrived
  const bw = { bytes: 0, ms: 0, last: null, lastT: 0 };
  const sample = (t) => {
    const r = received();
    if (bw.last != null && r > bw.last) {
      bw.bytes += r - bw.last;
      bw.ms += Math.max(0, t - bw.lastT);
    }
    bw.last = r;
    bw.lastT = t;
    return r;
  };
  const bandwidth = () => (bw.bytes >= MEASURE_BYTES && bw.ms > 0 ? Math.max(MIN_BANDWIDTH, bw.bytes / bw.ms) : DEFAULT_BANDWIDTH);
  // the bytes a download stage has: the bytes since the plan go to the plan's download stages in order, each up to its total (a done
  // stage keeps its total), the last one taking what is left over (a file the manifest did not know)
  const got = (s, r) => {
    const list = [...stages.values()].filter((x) => x.bytes != null);
    let left = Math.max(0, r - (list[0]?.base ?? r));
    for (let i = 0; i < list.length; i++) {
      const x = list[i];
      const mine = x.done || i === list.length - 1 ? (x.done ? Math.min(left, x.bytes) : left) : Math.min(left, x.bytes);
      if (x === s) return x.done ? x.bytes : mine;
      left -= mine;
    }
    return 0;
  };
  // a download stage's total: its bytes, or (more arrived than the manifest knew, the last stage) what arrived and what is still coming
  const totalOf = (s, r) => {
    const g = got(s, r);
    return g < s.bytes ? s.bytes : g + Math.max(inflight(), g * 0.01);
  };
  // a download stage's weight: its total at the bandwidth
  const weight = (s, r) => (s.bytes == null ? s.ms : Math.max(1, totalOf(s, r) / bandwidth()));
  const creep = (s, t) => CREEP_MAX * Math.min(1, (t - s.start) / Math.max(1, s.ms));
  const stageFraction = (s, t, r = received()) => {
    if (s.done) return 1;
    if (s.start == null) return 0;
    if (s.bytes != null) return Math.min(0.99, got(s, r) / Math.max(1, totalOf(s, r)));
    if (!s.reported) return creep(s, t);
    let x = Math.max(s.f, s.floor);
    // between a report and the next one (ceiling: where the next one will be): creep up that gap over its share of the stage's time
    if (s.ceiling > x) x += (s.ceiling - x) * CREEP_MAX * Math.min(1, (t - s.at) / Math.max(1, s.ms * (s.ceiling - x)));
    return Math.min(0.999, x);
  };
  // floor: where its creep was at its first report (a report below it does not take the stage back); bytes: a download stage's total
  const fresh = (ms, bytes = null) => ({ ms, bytes, base: null, f: 0, floor: 0, ceiling: 0, at: 0, start: null, reported: false, done: false });
  // a download stage counts the bytes from its plan on (its files start downloading at once)
  const downloadsFrom = (r, t) => {
    for (const s of stages.values())
      if (s.bytes != null && s.base == null) {
        s.base = r;
        s.start = t;
      }
  };
  const segment = (t) => {
    const r = sample(t);
    let total = 0;
    let sum = 0;
    for (const s of stages.values()) {
      const w = weight(s, r);
      total += w;
      sum += w * stageFraction(s, t, r);
    }
    return total > 0 ? sum / total : 0;
  };
  const api = {
    get planned() {
      return stages.size > 0;
    },
    // plan([id | [id, ms]]): the stages of the load from here; ms overrides STAGE_MS (the course's remaining share).
    // - The current plan holds an unfinished PENDING stage (a course switch that goes on into an event load): the new stages take
    //   its place, sharing its weight, so the bar continues without a jump.
    // - Otherwise a new segment over what is left, from showing (where the screen already is, 0..1: the pace may hold it below the
    //   work, and a first plan may come after the screen counted without one).
    plan(list, { t = now(), showing = 0 } = {}) {
      const items = [];
      for (const item of list) {
        const [id, w] = Array.isArray(item) ? item : [item, STAGE_MS[item]];
        if (w?.bytes > 0) items.push([id, 0, w.bytes]);
        else if (w > 0) items.push([id, w, null]);
      }
      const r = sample(t);
      const pending = stages.get(PENDING);
      if (pending && !pending.done && items.length) {
        // the work stages share the pending weight; a download stage adds its own (its bytes were not known before)
        const sum = items.reduce((a, [, ms]) => a + ms, 0);
        stages.delete(PENDING);
        for (const [id, ms, bytes] of items) {
          stages.delete(id);
          stages.set(id, fresh(sum > 0 ? (pending.ms * ms) / sum : ms, bytes));
        }
        downloadsFrom(r, t);
        return;
      }
      let from = stages.size ? api.fraction(t) : Math.max(base, showing);
      if (stages.size && showing > 0) from = Math.min(from, showing);
      base = Math.min(0.999, from);
      stages = new Map();
      for (const [id, ms, bytes] of items) stages.set(id, fresh(ms, bytes));
      downloadsFrom(r, t);
    },
    has(id) {
      return stages.has(id);
    },
    begin(id, t = now()) {
      const s = stages.get(id);
      if (s && s.start == null) {
        s.start = t;
        if (s.bytes != null) s.base ??= received();
      }
    },
    // the stage under way (the first begun and not done, in plan order; a download stage only while bytes are expected), for the
    // screen's stage line and the diagnostics: { id, f, got, bytes }
    current(t = now(), { downloading = true } = {}) {
      const r = received();
      for (const [id, s] of stages) {
        if (s.done || s.start == null) continue;
        if (s.bytes != null && !downloading) continue;
        return { id, f: stageFraction(s, t, r), got: s.bytes != null ? got(s, r) : null, bytes: s.bytes != null ? totalOf(s, r) : null };
      }
      return null;
    },
    get bandwidth() {
      return bandwidth();
    },
    // fraction: the stage's share done; ceiling: where its next report will be, if known (it creeps towards it meanwhile)
    step(id, fraction, ceiling = 0, t = now()) {
      const s = stages.get(id);
      if (!s || s.done) return;
      if (s.start == null) s.start = t;
      if (!s.reported) s.floor = creep(s, t);
      else s.floor = Math.max(s.floor, stageFraction(s, t));
      s.reported = true;
      s.f = Math.max(s.f, Math.min(1, fraction));
      s.ceiling = Math.min(1, ceiling);
      s.at = t;
      if (s.f >= 1) s.done = true;
    },
    done(id, t = now()) {
      const s = stages.get(id);
      if (!s) return;
      if (s.start == null) s.start = t;
      s.done = true;
      s.f = 1;
    },
    // the true fraction of the load (0..1)
    fraction(t = now()) {
      if (!stages.size) return base;
      return base + (1 - base) * segment(t);
    },
    // what the screen shows: eases towards fraction() (a stage's report can jump), never backwards. While the fraction stands still
    // (a new download raised the total as fast as bytes arrived) it keeps moving, at LEAD_RATE, up to LEAD ahead of the fraction.
    value(t = now()) {
      const target = api.fraction(t);
      const dt = lastAt == null ? 1000 : Math.max(0, t - lastAt);
      lastAt = t;
      const k = 1 - Math.exp(-dt / 150);
      shown = Math.max(shown, shown + (target - shown) * k);
      if (target >= 1) shown = 1;
      else if (stages.size) shown = Math.max(shown, Math.min(target + LEAD, 0.99, shown + LEAD_RATE * dt));
      return shown;
    }
  };
  return api;
}
