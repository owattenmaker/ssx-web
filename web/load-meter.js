// The event load screen's honest meter (pv loadMeter, docs/loading-screen.md "The honest meter"). No imports: tested in node.
//
// A load is a list of stages, each weighted by its measured time (STAGE_MS). A stage reports its own fraction when it can
// (the warm-up's slices, the course build's milestones); one that has begun without reports creeps towards CREEP_MAX of
// itself over its typical time and waits there until it is done. A later plan (a world load that goes on into an event
// load) starts a new segment over what is left, so the fraction never goes backwards. 1 only when every stage is done.
export const CREEP_MAX = 0.9;
// a stage that stands for a later plan's stages (main.js switchCourse: the event load after a course switch)
export const PENDING = 'pending';

// Typical times (ms) on this project's reference Mac (Chrome 1x and WebKit, local server; docs/loading-screen.md "The honest meter"):
// only their ratios matter. course: an in-page course build (PEAK1 2.5 s, ARA1 1.9 s); lazyCourse: the page's first course still
// loading behind the menus when an event is picked (downloads included; field p50 14 s), weighted by the share of it left.
export const STAGE_MS = Object.freeze({
  course: 2500,
  lazyCourse: 6000,
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

export function createLoadMeter({ now = () => performance.now() } = {}) {
  let stages = new Map();
  let base = 0;
  let shown = 0;
  let lastAt = null;
  const creep = (s, t) => CREEP_MAX * Math.min(1, (t - s.start) / Math.max(1, s.ms));
  const stageFraction = (s, t) => {
    if (s.done) return 1;
    if (s.start == null) return 0;
    if (!s.reported) return creep(s, t);
    let x = Math.max(s.f, s.floor);
    // between a report and the next one (ceiling: where the next one will be): creep up that gap over its share of the stage's time
    if (s.ceiling > x) x += (s.ceiling - x) * CREEP_MAX * Math.min(1, (t - s.at) / Math.max(1, s.ms * (s.ceiling - x)));
    return Math.min(0.999, x);
  };
  // floor: where its creep was at its first report (a report below it does not take the stage back)
  const fresh = (ms) => ({ ms, f: 0, floor: 0, ceiling: 0, at: 0, start: null, reported: false, done: false });
  const segment = (t) => {
    let total = 0;
    let got = 0;
    for (const s of stages.values()) {
      total += s.ms;
      got += s.ms * stageFraction(s, t);
    }
    return total > 0 ? got / total : 0;
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
        const [id, ms] = Array.isArray(item) ? item : [item, STAGE_MS[item]];
        if (ms > 0) items.push([id, ms]);
      }
      const pending = stages.get(PENDING);
      if (pending && !pending.done && items.length) {
        const sum = items.reduce((a, [, ms]) => a + ms, 0);
        stages.delete(PENDING);
        for (const [id, ms] of items) {
          stages.delete(id);
          stages.set(id, fresh((pending.ms * ms) / sum));
        }
        return;
      }
      let from = stages.size ? api.fraction(t) : Math.max(base, showing);
      if (stages.size && showing > 0) from = Math.min(from, showing);
      base = Math.min(0.999, from);
      stages = new Map();
      for (const [id, ms] of items) stages.set(id, fresh(ms));
    },
    has(id) {
      return stages.has(id);
    },
    begin(id, t = now()) {
      const s = stages.get(id);
      if (s && s.start == null) s.start = t;
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
    // what the screen shows: eases towards fraction() (a stage's report can jump), never backwards
    value(t = now()) {
      const target = api.fraction(t);
      const dt = lastAt == null ? 1000 : Math.max(0, t - lastAt);
      lastAt = t;
      const k = 1 - Math.exp(-dt / 150);
      shown = Math.max(shown, shown + (target - shown) * k);
      if (target >= 1) shown = 1;
      return shown;
    }
  };
  return api;
}
