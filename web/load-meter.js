// The event load screen's honest meter (pv loadMeter, docs/loading-screen.md "The honest meter"). No imports: tested in node.
//
// A load is a list of stages, each weighted by its measured time (STAGE_MS). A stage reports its own fraction when it can
// (the warm-up's slices, the course build's milestones); one that has begun without reports creeps towards CREEP_MAX of
// itself over its typical time and waits there until it is done. A later plan (a world load that goes on into an event
// load) starts a new segment over what is left, so the fraction never goes backwards. 1 only when every stage is done.
export const CREEP_MAX = 0.9;

// Typical times (ms) on the reference desktop (Chrome 1x, M-series Mac; docs/loading-screen.md "The honest meter"): only their
// ratios matter. The course is weighted by the share of it still to load when the plan is made.
export const STAGE_MS = Object.freeze({
  course: 6000,
  rider: 250,
  lineup: 300,
  warm: 3200,
  intro: 900,
  unload: 300,
  ride: 600,
  world: 1200,
  gc: 200
});

export function createLoadMeter({ now = () => performance.now() } = {}) {
  let stages = new Map();
  let base = 0;
  let shown = 0;
  let lastAt = null;
  const stageFraction = (s, t) => {
    if (s.done) return 1;
    if (s.start == null) return 0;
    if (s.reported) return Math.min(0.999, s.f);
    return CREEP_MAX * Math.min(1, (t - s.start) / Math.max(1, s.ms));
  };
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
    // plan([id | [id, ms]], t): the stages of the load from here; ms overrides STAGE_MS (the course's remaining share)
    plan(list, t = now()) {
      const from = stages.size ? api.fraction(t) : base;
      base = Math.min(0.999, from);
      stages = new Map();
      for (const item of list) {
        const [id, ms] = Array.isArray(item) ? item : [item, STAGE_MS[item]];
        if (!(ms > 0)) continue;
        stages.set(id, { ms, f: 0, start: null, reported: false, done: false });
      }
    },
    has(id) {
      return stages.has(id);
    },
    begin(id, t = now()) {
      const s = stages.get(id);
      if (s && s.start == null) s.start = t;
    },
    step(id, fraction, t = now()) {
      const s = stages.get(id);
      if (!s || s.done) return;
      if (s.start == null) s.start = t;
      s.reported = true;
      s.f = Math.max(s.f, Math.min(1, fraction));
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
