// Conquer the Mountain: the transport ride over a world switch (heli; docs/presentation.md "Heli ride over a peak
// change"). PS2 (ctm-parity runs f95-after, peak2-arr): Transport > Select Peak > "Go to this peak now?" Yes (and the
// post-event Transport to another location) -> WS14: the departure at a station (heli_dep / gond_dep, 0x27A860), the in-air
// ride heli_inair #149 / gond_inair #150 once, then heli_inair_<char> #113..122 / gond_inair_<char> held (flags 8) with
// "Loading..." while the destination streams -> WS11 -> WS10 at the new location (its arrival list, e.g. the DBC2 movie).
// Heli when either end is a backcountry, else the gondola (cutscenes.js playCutscene 'transport-depart' / 'transport-loop').
//
// The browser changes world with an in-page course switch (main.js navigateCourse) under a load screen: the ride plays in
// the current world, and when its held loop starts the switch runs under it. The list draws itself meanwhile
// (cutscenes.js acrossSwitch: the TRANSP set and the rider stay in the scene) with its bars and "Loading..." on a clear
// load screen (loading-screen.js world mode); career-ui.js enterWorld releases it once the new world is in.
import { playCutscene, COURSE_CODES } from './cutscenes.js';
import { freeRideHolds } from './free-ride.js';

// Whether going to `course` changes the page's world (a peak change, or leaving an event package for the peak world).
export function switchesWorld(ui, course) {
  const here = ui.course; if (!here) return false;
  return !here.freeRide || !freeRideHolds(here, course); // as main.js cb.freeRide (mountainRide: MOUNTAIN holds every course)
}

// Whether cu.goWorld(dest) plays the ride: free ride or the post-event map, into another world; after an event the same
// location again is WS15 (session point 1 and a white fade, ctm-flow.md 4), not a transport.
export function rideWanted(ui, cu, dest) {
  if (!ui.cutscene || !(cu.freeRide || cu.afterEvent) || !switchesWorld(ui, dest)) return false;
  // world state 15 applies only when the destination is not a backcountry (0x2365CC..0x236640): the same backcountry
  // after its rival event rides the heli again
  if (cu.afterEvent && dest >= 14 && dest <= 16) return true;
  return !(cu.afterEvent && (cu.active?.course === dest || (!!ui.course?.code && ui.course.code === COURSE_CODES[dest])));
}

// Waits (on animation frames, up to `ms`) for cond().
function until(cond, ms) {
  return new Promise((resolve) => {
    const t0 = performance.now();
    const f = () => {
      if (cond()) resolve(true);
      else if (performance.now() - t0 > ms) resolve(false);
      else requestAnimationFrame(f);
    };
    f();
  });
}

// cu: CareerUI; dest: the destination course index; go(): the course switch (returns what ui.cb.freeRide returned).
// Resolves go()'s value, or null when the ride is not played (switch off / no cutscenes / same world) and go() ran alone.
export async function rideAcrossSwitch({ ui, cu, dest, go }) {
  const cs = ui.cutscene;
  if (!rideWanted(ui, cu, dest) || !cs?.acrossSwitch) return go();
  // the departure's sky dome, drawn under the held loop over the switch (cutscenes.js prepareAcrossSky)
  const sky = cs.prepareAcrossSky?.(ui.course?.sky ?? null) ?? null;
  // the departure (from a station) and the in-air ride, in the world being left
  // after an event the course being left is the event's (a backcountry rival event leaves by heli, 0x2365CC)
  const from = cu.afterEvent && cu.active?.course != null ? cu.active.course : undefined;
  await playCutscene({ kind: 'transport-depart', location: dest, restore: false, from }).catch(() => null);
  let ended = false;
  playCutscene({ kind: 'transport-loop', location: dest, restore: false, from }).catch(() => null).then(() => { ended = true; });
  // the in-air ride once, then its held loop (step 1 of the 'transport-loop' list)
  const held = await until(() => { const s = cs.state; return ended || (s?.kind === 'transport-loop' && s.step >= 1 && s.t > 0); }, 30000);
  if (!held || ended || !cs.acrossSwitch({ sky })) return go();
  // the load screen stays clear: the held loop (drawn by the list itself) with its bars and "Loading..."
  ui.loading.world = (c, b) => { b.clearRect(0, 0, 640, 448); cs.draw(c, ui); };
  return go();
}
