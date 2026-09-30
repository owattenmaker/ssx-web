// The per-event data of a Conquer the Mountain event run inside the streamed world (docs/ctm-events-in-world.md, stage 2, pv
// eventWorldData; used from stage 3, pv eventInWorld). On the PS2 the event runs in the world already resident: its location rows are the
// event package's world (web/prepare.py merges the course's residency row), and its race / reset paths are the location's own bank
// (all 17 courses: the event package's initial.json paths == the streamed location's paths.json variant 0, local/ctm-events/paths-eq.py).
// What the streamed world lacks is only the event's own small data, read here from the event package folder while the fly-over plays:
//   the computer riders (npc-riders.json; the lineup parts lineups.json; rivals.json for a backcountry rival), the course's race event
//   document (initial.json: participants, checkpoints, clock; Snow Jam's is ANIMATIONS/initial.json), the grid spawn (start.json), the
//   route, the progress meter, the slope-style checkpoint list (freestyle-event.json), the replay's camera triggers, and the event's GO
//   LiveComp starts (the start-gate doors: livecomp.json starts with trigger 'go', which the streamed worlds' SETPIECES leave out,
//   tools/export_peak_world.py). Nothing here touches the core or the scene.
const FILES = {
  npcRiders: 'npc-riders.json', lineups: 'lineups.json', rivals: 'rivals.json', start: 'start.json', route: 'route.json',
  progressMeter: 'progress-meter.json', freestyle: 'freestyle-event.json', cameraTriggers: 'camera-triggers.json',
};

// The course's race-event document: its own initial.json when the package has one, else the shared ANIMATIONS/initial.json (Snow Jam).
export function eventInitialUrl(entry) { return entry.initial || `${entry.root}initial.json`; }

// The GO starts of a livecomp document: [{resource, start}] for every start whose trigger is 'go', plus the program starts.
export function goStarts(livecomp) {
  if (!livecomp) return { instances: [], programs: [] };
  const instances = [];
  for (const x of livecomp.instances || []) for (const s of x.starts || []) if (s.trigger === 'go') instances.push({ resource: x.resource, start: s });
  return { instances, programs: (livecomp.program_starts || []).filter((s) => s.trigger === 'go') };
}

// entry: the event course's entry (courses.json: code, root, initial, event). fetchJson(url) -> parsed JSON or null (a missing file).
export async function loadEventPlan(entry, { fetchJson, fetchText } = {}) {
  const root = entry.root || `/assets/${entry.code}/`;
  const json = fetchJson || (async (u) => { const r = await fetch(u); return r.ok ? r.json() : null; });
  const text = fetchText || (async (u) => { const r = await fetch(u); return r.ok ? r.text() : null; });
  const out = { code: entry.code, root, event: entry.event ?? null };
  const keys = Object.keys(FILES);
  const got = await Promise.all(keys.map((k) => json(root + FILES[k]).catch(() => null)));
  keys.forEach((k, i) => { out[k] = got[i]; });
  out.initialText = await text(eventInitialUrl(entry)).catch(() => null);
  // Snow Jam's LiveComps are the shared LIVECOMP/livecomp.json (web/set-pieces-renderer.js loads the course's, else that one)
  const live = await json(entry.code === 'ARA1' ? '/assets/LIVECOMP/livecomp.json' : `${root}LIVECOMP/livecomp.json`).catch(() => null);
  out.go = { ...goStarts(live), tick: live?.go_tick ?? null };   // set-pieces-renderer.js fires 'go' at liveData.go_tick
  out.ready = !!(out.initialText && out.start);
  return out;
}
