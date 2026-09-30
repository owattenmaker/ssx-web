// Conquer the Mountain: an event started from free ride by riding into a course's RaceRideState gate (stage builtin 67,
// 0x22D6C8). The PS2 runs the event in the streamed Peak 1 world with no load screen (docs/ctm-flow.md, PS2
// scratchpad ps2b/intro): world state 4 -> 1 at the gate, the free-ride HUD goes, the cinematic bars slide in with the
// world's "Loading..." caption while the rider rides on and the view fades to black (the fly-over's fade-in record:
// out 30), the venue fly-over (list 2, flags 0, "Loading..." over it; music code 21-25 = a new song, PA venue intro at
// t30), the approach (list 3, Cross skips; PA rider race intro), the round's objectives card over the start-gate idle
// loop (list 5, PA sponsor intro), then the countdown.
//
// The browser's event is a separate course package with its own core (computer riders, race session, start-gate set
// pieces), so the fly-over plays in the streamed world and the page's course switch (main.js navigateCourse) runs
// behind it: the load screen's world mode draws the black hold with the same caption instead of the Basic Controls
// screen, the fly-over's song plays on through the switch (game-audio carryWorld), and the event's intro starts at the
// approach (cutscenes.js 'career-ridein'). The fly-over ends in black (its fade-out record) and the approach starts
// bright (PS2 s1045), so the switch sits exactly on that cut.
import { playCutscene, COURSE_CODES } from './cutscenes.js';
import { pv } from './pv-flags.js';
import { prefetchDownload } from './downloads.js';
import { loadEventPlan } from './ctm-event-plan.js';

// pv flyover (docs/presentation.md "Hold after the fly-over"): the PS2 loads the event's riders under the fly-over and
// holds at most a few ticks after it; the browser's switch to the event package runs after the fly-over. Its downloads
// start once the fly-over plays instead (~5 s before the switch), so the switch finds them in flight or local: the event
// package files of every course (the ARA1 boot list of web/boot-files.json, per course).
export const EVENT_PACKAGE_FILES = ['start.json', 'route.json', 'collision.bin', 'world.json', 'vertices.bin', 'indices.bin', 'colors.bin',
  'terrain-lighting.json', 'terrain-render.json', 'vertex-alpha.bin', 'terrain-light-atlas.png', 'SECTIONS/sections.json', 'SECTIONS/ready-state.json',
  'PARTICLES/particles.json', 'STAGE/stage-world.json', 'CROWD/crowd.json', 'environment.json', 'environment.bin', 'terrain.json',
  'world_collision.json', 'fog-tree.json', 'weather.json', 'local-lights.json', 'light-tree.json', 'rails.json', 'screen-tint.json', 'glare.json'];
export function prefetchEvent(root, { prefetch = prefetchDownload } = {}) {
  if (!root || !prefetch) return [];
  // web/downloads.js prefetchDownload: one copy of each body, shared with the switch's own requests (which join a download still
  // running or take the finished bytes, kept until then, at most 30 s), else the HTTP cache (a revalidation)
  return EVENT_PACKAGE_FILES.map((f) => prefetch(root + f).catch(() => false));
}

export const PREFADE_TICKS = 30;   // fade_in.out_ticks of every venue fly-over (ra/ss/ba/hp_sga_<loc>)

// ui: OriginalUI; cu: CareerUI; pause(): freezes the world (main.js pause(true)). Resolves when the course switch has
// been requested (the event continues in career-ui.js resume -> begin with rideIn).
export async function rideIntoEvent({ ui, cu, mode, course, pause = null }) {
  const cs = ui.cutscene, code = COURSE_CODES[course], entry = code && cu.courseEntry(code);
  if (!cs || !entry || !ui.cb.course) { pause?.(); cu.transport(mode, course); return false; }
  if (cu.ridingIn) return false;
  cu.ridingIn = true;
  try {
    // world state 1 enter: bars + caption + fade out over the running world (the rider keeps riding)
    await cs.preFade({ ticks: PREFADE_TICKS, loading: true });
    pause?.();
    // the venue fly-over in the streamed world (the location's scdat / scfilter list 2)
    let flown = false;
    const fly = playCutscene({ kind: 'intro', mode: 'flyover', location: code, restore: false }).catch((e) => { console.warn('Fly-over failed', e); }).finally(() => { flown = true; });
    // pv eventWorldData (docs/ctm-events-in-world.md stage 2): the event's own data (riders, race event, grid spawn, GO starts) read while
    // the fly-over plays, for the in-world event (stage 3); nothing uses it yet
    if (pv('eventWorldData')) cu.eventPlan = loadEventPlan(entry).catch((e) => { console.warn('Event plan failed', e); return null; });
    // pv eventInWorldAi (stage 4): the event's computer riders made under the fly-over (the PS2 makes them at gate + 2), for the in-world event
    if (pv('eventInWorld') && pv('eventInWorldAi')) ui.cb.eventAiPrepare?.({ entry, mode });
    // pv flyover: the event package downloads once the fly-over's own files are in and its clock runs
    if (pv('flyover')) { await new Promise((r) => { const f = () => (flown || (cs.state?.t ?? 0) > 0 ? r() : requestAnimationFrame(f)); f(); }); prefetchEvent(entry.root || `/assets/${code}/`); }
    await fly;
    // pv eventInWorld (docs/ctm-events-in-world.md stage 3): the event is set up in the streamed world itself (22D6C8 / WS1 in place, as
    // the PS2 does): no course switch, the approach follows the fly-over's cut at once
    if (pv('eventInWorld') && ui.cb.eventInWorld && cu.eventPlan) {
      const plan = await cu.eventPlan;
      if (plan && await ui.cb.eventInWorld({ plan, mode, entry })) { cs.clearOverlay?.(); cu.begin(mode, course, true, { rideIn: true, inWorld: true }); return true; }
    }
    // black hold with the caption while the page switches to the event course; the round resumes at the approach
    cs.clearOverlay?.();
    ui.loading.world = (c, b) => cs.drawCover(c, ui);
    ui.gameAudio?.carryWorld?.(true);
    cu.career.save.pending = { rider: cu.riderId, mode, course, career: true, rideIn: true };
    cu.career.persist();
    if (ui.cb.course(entry) === false) return true;   // switching: careerUI.resume() picks the pending round up
    // (already on that course: no switch)
    ui.loading.world = null; ui.gameAudio?.carryWorld?.(false);
    delete cu.career.save.pending; cu.career.persist();
    cu.begin(mode, course, true, { rideIn: true });
    return true;
  } finally { cu.ridingIn = false; }
}
