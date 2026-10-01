// Connected Peak 1 world in the browser (docs/peak-mountain.md): free ride and the whole-peak runs.
//
// The core runs the original location streaming (web/peak_world.inc: residency rows 0x442488 per course index,
// streaming rows 0x442168, stage builtin 68 from the connectors' Load/Unload trigger volumes) and gates every world
// query on it. This module keeps the browser data in step with those rows, off the hot path:
//   - collision: every Peak 1 location is fed into the core once (web/peak-world.js: worker-cut slices, a few ms per
//     frame), the start row under the loading screen, then the rest in adjacency order while riding. A location is
//     only ever made resident by the core, and only once its data is in (peak_world_data_ready);
//   - draw packages: built through web/main.js asset() (course package format, PEAK1/<LOC>/) for the rows around the
//     rider, shown exactly while their location is resident, released some time after they leave (Safari memory).
import { createPeakWorld } from './peak-world.js';
import { peakSplit as peakSplitFn, peakKind, PEAK_RUNS } from './peak-run.js';
import { createPeakSetPieces } from './peak-set-pieces.js';
import { playCutscene } from './cutscenes.js';
import { pv } from './pv-flags.js';
import { forgetGpuRestore } from './gpu-copies.js';

export const ROW = { ABSENT: 0, INACTIVE: 1, ACTIVE: 2, WANTED: 3, WANTED_ACTIVE: 4, UNLOAD: 5, READING: 6, UNLOADING: 7, READING_ACTIVE: 8 };
const WANTED_STATES = new Set([1, 2, 3, 4, 6, 8]);
export const FREE_RIDE_KIND = 4, TIME_CHALLENGE_KIND = 5, POINTS_CHALLENGE_KIND = 6; // 0x535C10
const RELEASE_MS = 45000;

export async function createFreeRide({
  core,
  load,
  asset,
  parent,
  disposeGroup,
  T,
  origin = [0, 0, 0],
  warm = null,
  idle = () => false,
  covered = () => false,
  peakSetup = () => null,
  splitInputs = () => ({}),
  collectStart = null,
  manifestUrl = '/assets/PEAK1/peak.json',
  sparkle = null
}) {
  // Draw packages build in slices of a few ms (main.js asset opts.pause) and their pipelines compile before they are shown.
  // while the world animates, a build yields to the next frame after each ~4 ms slice (one slice a frame), else at once
  // the draw builds (main.js asset's ~4 ms slices, the pipeline batches) share a per-frame budget: STALL_MS while the game is
  // held for a row's data (the +0x1D0 wait: the rider is frozen anyway, the hold ends sooner), WANTED_MS for a row its Unload trigger
  // asked for (the PS2 reads it at full speed from there; the Load trigger is ~6-11 s away), AHEAD_MS for a row read ahead, 24 ms while
  // nothing animates (a movie, a pause). Over the budget, a build waits for the next frame (a task yield per slice while nothing animates).
  const STALL_MS = 36,
    WANTED_MS = 10,
    AHEAD_MS = 4;
  let frameBuildMs = 0,
    wantedCodes = new Set();
  const setTimeout0 = () => new Promise((r) => setTimeout(r, 0));
  const buildBudget = (code) => (stalled ? STALL_MS : !started ? 0 : idle() ? 24 : wantedCodes.has(code) ? WANTED_MS : AHEAD_MS); // (idle / load screen: over it, a task yield per slice)
  // The load screen, a movie, a pause: task yields between the slices, and a frame at least every 20 ms (task yields alone let Chrome
  // run the slices back to back: 100-170 ms load-screen frames at 4x CPU).
  let lastFrameAt = 0;
  function yieldFor(code, usedMs) {
    frameBuildMs += usedMs;
    if (!started || idle())
      return performance.now() - lastFrameAt < 20
        ? setTimeout0()
        : nextFrame().then(() => {
            lastFrameAt = performance.now();
          });
    if (frameBuildMs < buildBudget(code)) return Promise.resolve();
    return nextFrame();
  }
  const pauseFor = (code) => {
    let resumed = performance.now();
    return () => {
      const used = performance.now() - resumed;
      return yieldFor(code, used).then(() => {
        resumed = performance.now();
      });
    };
  };
  const peak = await createPeakWorld({ core, load, manifestUrl });
  const manifest = peak.manifest;
  const byId = new Map(manifest.locations.map((l) => [l.id, l]));
  const byCode = new Map(manifest.locations.map((l) => [l.code, l]));
  // matrixAutoUpdate off: an identity container the page never moves; recomposing it every render forced a matrixWorld multiply on
  // every location mesh below it (docs/web-render-performance.md, docs/sim-performance.md "Free-roam steady state")
  const group = new T.Group();
  group.name = 'peak1-world';
  group.matrixAutoUpdate = false;
  parent.add(group);
  const render = new Map(); // code -> {state, group, lastSeen}
  const heap32 = () => new Int32Array(core.HEAPU8.buffer);
  const listeners = { station: null, residency: null, gate: null, region: null, sky: null, course: null, drawn: null };
  let region = null; // location of the rider's last contacted patch (the world painters' region)
  let stalled = false,
    skyId = -1,
    frameStart = 0;
  let stopped = false; // stop(): the world is gone (a course switch); pipeline warms still in flight end at their next batch (their render
  // targets are the next course's: a compile for a released location failed on a missing depth format)
  // the warms in flight (three's compileAsync builds its deferred objects over time, against the pass targets of the call): settle() lets
  // them finish before the course's passes are disposed (main.js unloadCourse, bootChain), bounded
  const inflight = new Set(),
    track = (p) => {
      const q = Promise.resolve(p).catch(() => {});
      inflight.add(q);
      q.finally(() => inflight.delete(q));
      return q;
    };
  let started = false,
    eventKind = FREE_RIDE_KIND,
    regionPlaced = false; // regionPlaced: placeRegion attached the route itself (afterReset skips 112180)
  let freshPending = false; // a world start's first placement is a new rider's (core fresh_rider_start, freshRider)
  const pathBanks = new Map(); // location id -> paths.json
  let splitCounter = 0,
    split = null; // peak runs
  let setPieces = null; // stage world of the resident locations (web/peak-set-pieces.js)
  const flagsApplied = new Set();
  const manifestRoot = manifestUrl.replace(/[^/]*$/, ''); // /assets/PEAK1/ (the whole mountain's locations live in the per-peak folders)
  // an active row is also held until its draw package is built (never collidable and undrawn); streamAhead: the
  // background reads in the PS2's order (one at a time, the next rows first; docs/ctm-flow.md "Start and streaming").
  const drawReady = (code) => {
    const s = render.get(code);
    return !!s && (s.state === 'ready' || s.state === 'failed');
  };
  let aheadQueue = [],
    aheadJob = null,
    aheadBuild = new Set(),
    aheadKeep = new Set(),
    aheadTarget = null,
    aheadNext = new Set();
  const warmMesh = true; // pipelines compiled for the world pass mesh by mesh across frames (main.js warm = fogRenderer.compileObject)
  const nextFrame = () =>
    new Promise((r) => {
      let done = false;
      const go = () => {
        if (!done) {
          done = true;
          r();
        }
      };
      requestAnimationFrame(go);
      setTimeout(go, 100);
    });
  // The whole mountain (docs/peak3.md section 6): only the rows around the rider are fetched (the run's route ahead, else the rows
  // sharing a location with the current one), never the whole world; a single peak prefetches everything once it has started.
  const windowed = manifest.name === 'MOUNTAIN';
  let route = null,
    prefetched = new Set();
  const painterDocs = new Map(),
    painterDoc = (url) => {
      if (!painterDocs.has(url))
        painterDocs.set(
          url,
          fetch(url)
            .then((r) => (r.ok ? r.text() : null))
            .catch(() => null)
        );
      return painterDocs.get(url);
    };
  // Painter region change: the location's Lighting section (type 11) replaces the rider lighting tree, driver state kept.
  function regionLighting(entry) {
    if (!core._lighting_region) return;
    painterDoc(entry.root + 'lighting.json').then((text) => {
      if (!text || region !== entry.code || !started) return;
      const p = put(text);
      try {
        core._lighting_region(p);
      } finally {
        core._free(p);
      }
    });
  }
  const put = (text) => {
    const b = new TextEncoder().encode(text + '\0'),
      p = core._malloc(b.length);
    core.HEAPU8.set(b, p);
    return p;
  };

  // Core streaming rows: {id, track, state (0..8, see web/peak_world.inc), class 0 location / 1 TRANSP / 2 sky}.
  function rows() {
    const p = core._peak_world_rows() >> 2,
      H = heap32(),
      n = H[p],
      out = [];
    for (let i = 0; i < n; i++) {
      const id = H[p + 1 + 4 * i];
      out.push({
        id,
        track: H[p + 2 + 4 * i],
        state: H[p + 3 + 4 * i],
        cls: H[p + 4 + 4 * i],
        code: byId.get(id)?.code ?? manifest.streaming?.find((r) => r.id === id)?.code
      });
    }
    return out;
  }
  const rowOf = (course) => manifest.residency.find((r) => r.course === course);
  // peakRelease (docs/ctm-parity.md "The PS2's location release"): the PS2 frees a location when its row is evicted (7 -> 0, T+8:
  // every record of the track, 3A8528 / 3A8230), so a location no row wants and nothing reads ahead is released at once (before: 45 s
  // after it left, and a peak run never freed its collision). A peak run reads ahead only its route's next row.
  const routeNext = (course) => {
    if (!route) return null;
    const at = route.indexOf(course);
    return at >= 0 ? (rowOf(route[at + 1]) ?? null) : null;
  };
  // The locations to hold around a course: its row, then the next two courses of the run's route (a peak run), or every row
  // sharing a location with it (free ride / off the route).
  function windowAround(course) {
    const out = new Set(rowOf(course)?.locations ?? []);
    const at = route ? route.indexOf(course) : -1;
    if (at >= 0) {
      for (const c of route.slice(at + 1, at + 3)) (rowOf(c)?.locations ?? []).forEach((l) => out.add(l));
    } else for (const r of manifest.residency) if (r.locations.some((l) => out.has(l))) r.locations.forEach((l) => out.add(l));
    return out;
  }
  function prefetchWindow(course) {
    if (!started) return;
    // a free ride (no run route) with streamAhead reads ahead in the PS2's order already (planAhead: the rows the
    // current row's connectors lead down to); the window's other rows are the ones uphill (the hub the rider came from, where no
    // connector leads), fetched and fed for nothing
    if (!route) {
      prefetched = new Set();
      return;
    }
    // a peak run reads ahead only the route's next row (planAhead, routeNext), as the PS2 reads only what an Unload
    // trigger asks for; the window's two rows ahead held 3 rows' collision and environment for the whole run
    if (route) {
      prefetched = new Set();
      return;
    }
    prefetched = windowAround(course);
    for (const code of prefetched) {
      requestCore(code);
      peak.requestEnv(code);
    }
  }
  function requestCore(code) {
    const l = byCode.get(code);
    if (!l) return Promise.resolve();
    if (flagsApplied.has(code)) return peak.requestCore(code);
    flagsApplied.add(code);
    // DeadNodes / Hide nodes come from the section activation (slot-1 programs, tools/export_peak_sections.py), not from
    // the free-ride audit (instance-flags.json only hides draws at export time).
    return peak.requestCore(code).then(() => {
      core._peak_world_data_ready(l.track, 1);
    });
  }
  // A released location: its meshes, materials, geometries (disposeGroup), and what that traversal does not reach: the hidden
  // batches (never in the group), the location's textures (node materials sample them) and its terrain light atlas.
  function releaseLocation(g) {
    forgetGpuRestore(g); // gpuRelease registered the group's restore (web/gpu-copies.js): it held the whole released tree until a device recovery
    for (const m of g.userData.hiddenMeshes || []) {
      if (m.parent) m.removeFromParent();
      m.geometry?.dispose();
      m.dispatchEvent({ type: 'dispose' });
    }
    const meshes = [];
    g.traverse((o) => {
      if (o.isMesh) meshes.push(o);
    });
    disposeGroup(g);
    for (const m of meshes) m.dispatchEvent({ type: 'dispose' }); // the renderer's per-object bindings / uniform buffers
    for (const t of Object.values(g.userData.worldTextures || {})) t?.dispose?.();
    g.userData.worldAtlas?.dispose?.();
  }
  // the same release in small steps across frames (a location's geometries, materials, textures and per-object
  // bindings in one frame was 35-165 ms at 4x CPU, and the rows a connector leaves are released together 45 s later).
  const releasing = [];
  function releaseSteps(g) {
    forgetGpuRestore(g); // (as releaseLocation)
    const steps = [],
      meshes = [],
      geos = new Set(),
      mats = new Set(),
      texs = new Set();
    for (const m of g.userData.hiddenMeshes || [])
      steps.push(() => {
        if (m.parent) m.removeFromParent();
        m.geometry?.dispose();
        m.dispatchEvent({ type: 'dispose' });
      });
    g.traverse((o) => {
      if (!o.isMesh) return;
      meshes.push(o);
      geos.add(o.geometry);
      for (const x of Array.isArray(o.material) ? o.material : [o.material]) {
        if (!x) continue;
        mats.add(x);
        if (x.map) texs.add(x.map);
        if (x.lightMap) texs.add(x.lightMap);
        if (x.userData.sourceTexture) texs.add(x.userData.sourceTexture);
      }
    });
    if (g.userData.sourceSkin) steps.push(() => g.userData.sourceSkin.dispose()); // (main.js disposeRider's order)
    for (const x of geos) steps.push(() => x.dispose());
    for (const x of mats) steps.push(() => x.dispose());
    for (const x of texs) steps.push(() => x.dispose());
    for (const m of meshes) steps.push(() => m.dispatchEvent({ type: 'dispose' }));
    for (const t of Object.values(g.userData.worldTextures || {})) steps.push(() => t?.dispose?.());
    if (g.userData.worldAtlas) steps.push(() => g.userData.worldAtlas.dispose?.());
    return steps;
  }
  function releasePump(budgetMs) {
    const until = performance.now() + budgetMs;
    while (releasing.length && performance.now() < until) {
      const job = releasing[0];
      const step = job.shift();
      if (step) step();
      if (!job.length) releasing.shift();
    }
  }
  // each mesh's node material and pipelines built for the world pass (warm = fogRenderer.compileObject), a few ms of it per
  // frame (more while nothing animates); the pipelines themselves compile asynchronously and are awaited together.
  // drawn: the group is in view (the start row's rewarm): its materials' side may only change while nothing shows the world (covered:
  // the load screen, a movie); once it shows, the rest compiles DoubleSide (those two-pass meshes build on their first draw instead).
  // BATCH: the meshes of one call on the load screen; riding, a call is sized to what is left of the frame's build budget at the
  // measured cost per mesh (perMesh: a call's synchronous part per mesh, node material builds included; 48 were ~50 ms at 4x CPU).
  const BATCH = 48,
    BATCH_MIN = 4;
  let perMesh = 0;
  async function warmSliced(g, drawn = false, code = null) {
    const meshes = [];
    g.traverse((o) => {
      if (o.isMesh) meshes.push(o);
    });
    g.updateMatrixWorld(true);
    // compileAsync projects its object through the camera frustum (three WebGPURenderer._projectObject): a location away from the
    // camera compiled nothing and built its node materials on its first draw. Unculled for the call (its synchronous part).
    // A transparent double-sided material draws in two passes (BackSide, then FrontSide: three renderObject), but compileAsync
    // builds its deferred render objects after the side is restored, keyed DoubleSide: the draw rebuilt them all (the stations'
    // blend instances). So the location's own two-pass materials (it is not drawn yet) are set to BackSide for a first pass over
    // every mesh, then to FrontSide for a second pass over theirs; each pass is fired mesh by mesh (a few ms a frame, more while
    // nothing animates) and awaited as a whole (one await per mesh took 20-30 s of WebKit pipeline round trips for a hub).
    const twoPass = (x) => x.transparent && x.side === T.DoubleSide && !x.forceSinglePass;
    const mats = new Set();
    for (const m of meshes) for (const x of Array.isArray(m.material) ? m.material : [m.material]) if (x && twoPass(x)) mats.add(x);
    const twoMeshes = mats.size
      ? meshes.filter((m) => (Array.isArray(m.material) ? m.material : [m.material]).some((x) => mats.has(x)))
      : [];
    // pv worldWarm: then once more as DoubleSide, as the draw lists it (a BackSide pass under passId 'backSide', then the FrontSide one):
    // that pass's render object and bindings exist before the first draw, whose pipeline lookup then finds the BackSide build above
    const passes = mats.size
      ? [[T.BackSide, meshes], [T.FrontSide, twoMeshes], ...(pv('worldWarm') ? [[T.DoubleSide, twoMeshes]] : [])]
      : [[null, meshes]];
    try {
      for (const [side, list] of passes) {
        if (side != null && drawn && !covered()) {
          for (const x of mats) x.side = T.DoubleSide;
          if (side === T.FrontSide) break;
        } else if (side != null) for (const x of mats) x.side = side;
        // compiled BATCH meshes per call: each compileAsync call walks the whole target scene for its lights (traverseVisible), so a
        // call per mesh was quadratic in the scene size (ABC1's 3481 meshes); a batch rides in a detached group for the call's
        // synchronous part (listed as its children only: the meshes keep their parent and place) and leaves it right after
        const pending = [],
          tmp = new T.Group();
        let t0 = performance.now();
        for (let i = 0; i < list.length; ) {
          if (stopped) return;
          const n = !started
            ? BATCH
            : !perMesh
              ? 8
              : Math.max(BATCH_MIN, Math.min(BATCH, Math.floor(Math.max(2, buildBudget(code) - frameBuildMs) / perMesh)));
          const batch = list.slice(i, i + n),
            culled = batch.map((m) => m.frustumCulled);
          i += batch.length;
          for (const m of batch) {
            m.frustumCulled = false;
            tmp.children.push(m);
          }
          const c0 = performance.now();
          try {
            pending.push(track(warm(tmp)));
          } finally {
            tmp.children.length = 0;
            batch.forEach((m, k) => {
              m.frustumCulled = culled[k];
            });
          }
          {
            const d = (performance.now() - c0) / batch.length;
            perMesh = perMesh ? 0.7 * perMesh + 0.3 * d : d;
          }
          {
            const now = performance.now();
            await yieldFor(code, now - t0);
            t0 = performance.now();
            if (side != null && drawn && !covered()) for (const x of mats) x.side = T.DoubleSide;
          }
        }
        await Promise.all(pending);
      }
    } finally {
      for (const x of mats) x.side = T.DoubleSide;
    }
  }
  function buildRender(code) {
    if (render.has(code)) return render.get(code).ready;
    const l = byCode.get(code),
      slot = { state: 'building', group: null, lastSeen: performance.now(), times: { start: performance.now() } }; // times: QA (docs/ctm-flow.md)
    slot.ready = asset(code, false, l.root, { pause: pauseFor(code) })
      .then(async (g) => {
        if (render.get(code) !== slot) {
          releaseLocation(g);
          return;
        }
        g.userData.peakLocation = code;
        slot.times.built = performance.now();
        // the start row is built before main.js makes this course's fog renderer: api.rewarm() compiles it once that exists
        // compiled detached: never drawn early
        if (warm && !(warmMesh && !started)) {
          try {
            await (warmMesh ? warmSliced(g, false, code) : track(warm(g)));
          } catch (e) {
            console.warn('Peak 1 pipeline warm-up failed', e);
          }
        }
        if (warm && !(warmMesh && !started)) {
          g.userData.releaseGpuCopies?.();
          slot.released = true;
        } // the compile uploaded its geometry and textures (web/gpu-copies.js)
        if (render.get(code) !== slot) {
          releaseLocation(g);
          return;
        }
        slot.times.ready = performance.now();
        g.visible = false;
        group.add(g);
        slot.group = g;
        slot.state = 'ready';
        setPieces?.attach(code, g); // stage-world meshes of the location (web/peak-set-pieces.js)
        sparkle?.attach(code, l.root, l.track); // the location's terrain snow sparkle patches (web/terrain-sparkle.js; track: its painter region)
      })
      .catch((e) => {
        console.warn(`Peak 1 ${code} draw package failed`, e);
        slot.state = 'failed';
      });
    render.set(code, slot);
    return slot.ready;
  }
  // a location's collision fed without a draw package (a read-ahead 'core' job of the rows the connectors lead to, fed
  // while nothing animates, e.g. under a Transport's ride) is freed once nothing has wanted it for RELEASE_MS: not a wanted row, not
  // read or built ahead, not drawn, not prefetched. The release above only follows the draw packages, so a way not taken kept its
  // courses' collision for the session (MOUNTAIN long ride: ASS1, CHP2, EHP3, EBA3 fed under the Transport, wasm 154 -> 221 MB).
  const coreSeen = new Map();
  // The PS2's rule (the eviction frees the track at T+8, 3A8230): no hold. Freed as soon as nothing wants it (refused while its row
  // is still collidable or in the octree, 5 / 7: tried again next frame); a read ahead fetched but not fed that nothing wants any more
  // drops its slices (peak.dropQueued: the JS copies of its collision documents).
  function releaseIdleCores(now, want) {
    for (const [code, l] of peak.locations) {
      const held =
        render.has(code) || want.has(code) || aheadBuild.has(code) || aheadKeep.has(code) || aheadNext.has(code) || prefetched.has(code);
      if (l.core === 'queued' && !held && peak.dropQueued?.(code)) {
        coreSeen.delete(code);
        continue;
      }
      if (l.core !== 'loaded') {
        coreSeen.delete(code);
        continue;
      }
      if (held) {
        coreSeen.set(code, now);
        continue;
      }
      if (peak.releaseCore(code)) coreSeen.delete(code);
    }
  }
  function drainEvents() {
    const p = core._peak_world_events() >> 2,
      H = heap32(),
      n = H[p];
    for (let k = 0; k < n; k++) {
      const [kind, a, b, c] = H.slice(p + 1 + 4 * k, p + 5 + 4 * k);
      if (kind === 1 && (b === 3 || b === 4)) {
        // builtin 68 actions 3 transport booth / 4 lodge door (state 14): 0x302210 ignores key0 for these (231250(W, 14, 2|0));
        // the station is the current course 0x535C08. Yellow / Red stations carry key0 19 / 18 (Blue / Green; docs/peak2.md).
        const here = core._peak_world_course(),
          station = { action: b, course: here >= 17 && here <= 21 ? here : a };
        // world state 14's enter 236250 ends a running Big Challenge (0x23626C: 30B7F8) and holds the riders (113B10(C, 3))
        // while the cut plays: main.js pauses the ride from 'stationCut'
        core._mission_world_session?.(0);
        listeners.stationCut?.(station);
        playCutscene({
          kind: b === 4 ? 'lodge-walkin' : 'transport-booth',
          id: b === 4 ? 0x16 : 0xb,
          rider: null,
          location: station.course
        }).then(() => listeners.station?.(station));
      } // state 14 enter 236250: the cut, then the prompt / map
      else if (kind === 1 && b === 2 && a >= 17 && a <= 21 && eventKind !== FREE_RIDE_KIND && peakSetup()) {
        // 22D088 into a station: 235868 -> 238510 split
        const setup = peakSetup(),
          s = peakSplitFn(setup, splitCounter, splitInputs());
        splitCounter = s.counter;
        // 1195A8 / 1195D8: the split is a 5 s HUD message of the rider's score object (0x2A time / 0x2B points), drawn by the
        // trick HUD like the PS2's "CHECKPOINT" (web/trick-hud.js split); the old text under the clock stays as a fallback
        if (s.shown) {
          if (core._score_hud_post) core._score_hud_post(setup.time ? 0x2a : 0x2b, s.value, 5);
          else split = { ...s, until: performance.now() + 5000 };
        }
        listeners.residency?.({ course: a, action: b, mode: c });
      }
      // a load request (22D088) for a map id this world has no row for: the only such connectors are Peak 2's DRA4_A (->
      // Green Base Station A, 17) and Peak 3's ERA5_C (-> Yellow Mid Station C, 19); the PS2 streams on (one world), the port switches world
      else if (kind === 1 && a === -1 && b === 2 && eventKind === FREE_RIDE_KIND && { 2: 17, 3: 19 }[manifest.peak] != null)
        listeners.crossWorld?.({ 2: 17, 3: 19 }[manifest.peak]);
      else if (kind === 1) {
        listeners.residency?.({ course: a, action: b, mode: c });
        if (b === 2 && a >= 0 && eventKind === FREE_RIDE_KIND) listeners.arrive?.(a);
      } // a Load trigger (22D088) in free ride: world state 10 at the new location
      else if (kind === 3) listeners.gate?.({ entry: a, flag: b });
      else if (kind === 5) {
        if (windowed) prefetchWindow(a);
        planAhead(a);
        listeners.course?.(a);
      } // 0x535C08 = a (22DF50)
      else if (kind === 6) listeners.faq?.(a); // stage builtin 100 (Green Base Station's "?"): the Message Center on FAQ 1
    }
  }
  const api = {
    peak,
    manifest,
    group,
    rows,
    render,
    core, // core: QA handle (window.__freeRide.core)
    // state reached during start()
    on(name, fn) {
      listeners[name] = fn;
      if (name === 'sky' && skyId >= 0) fn(manifest.streaming.find((r) => r.id === skyId)?.code);
      if (name === 'region' && region) fn(byCode.get(region));
    },
    course: () => core._peak_world_course(),
    // The career's first-FAQ flag (147580(P, 0)) for stage builtin 110 (the station "?" homes in until it was shown); call before start().
    faqShown(shown) {
      core._peak_world_faq_shown?.(shown ? 1 : 0);
    },
    // Region placement 11DE60(rider, index, kind): 26B5E0 returns the row {+0 index, +4 kind} of the current bank 0x4D33A0,
    // or the bank's FIRST row when there is none. The runtime kinds are the disc kinds + 1 (PS2 fr/sj-01, no-a: grid slots
    // kind 1, session points kind 2), so `kind` here is the runtime kind. Then 11DF18: velocity = forward x 833.333 cm/s.
    // The station ride-in, the location arrival (arrivalFor) and the Session points use it. Native spawn for main.js.
    spawnFor(course, index = 0, kind = 2, { grid = false } = {}) {
      const code = manifest.residency.find((r) => r.course === course)?.code,
        loc = byCode.get(code);
      const variant =
        loc && loc.id >= 17 && loc.id <= 21 ? (eventKind === TIME_CHALLENGE_KIND ? 1 : eventKind === POINTS_CHALLENGE_KIND ? 2 : 0) : 0; // 12A340
      const variants = loc && pathBanks.get(loc.id)?.variants,
        bank = variants?.[String(variant)] ?? variants?.['0'];
      const region = bank && regionRow(bank, index, kind);
      if (!region) return null;
      // 11DF18: velocity = forward (+0x1B0, after the placement) x 833.333 with z = 0 (sw zero, 0x1E8; PS2 peak1-arrive-* records).
      const [x, y, z] = region.position,
        [dx, dy] = region.direction;
      return {
        position: [x / 100, z / 100, -y / 100],
        heading: Math.atan2(dx, -dy),
        velocity: null,
        forwardSpeed: 833.333,
        session: { course, index, kind },
        region: { position: region.position, direction: region.direction },
        grid: regionRow(bank, 0, 1)
      }; // PS2 cm rows for placeRegion (core place_rider_region)
    },
    // 11D390 (a location entry in free ride and the peak runs, kinds 4..6): courses < 14 -> 11DE60(rider, 1, 2) (session
    // point 1), backcountry 14..16 -> 11DE60(rider, player 0, 1) (grid slot), stations 17..21 -> 11DE60(rider, 0, 2).
    // pv eventReturnInWorld (web/event-return.js): the location's paths.json variant that spawnFor reads (12A340's variant pick)
    bankFor(course) {
      const code = manifest.residency.find((r) => r.course === course)?.code,
        loc = byCode.get(code);
      const variant =
        loc && loc.id >= 17 && loc.id <= 21 ? (eventKind === TIME_CHALLENGE_KIND ? 1 : eventKind === POINTS_CHALLENGE_KIND ? 2 : 0) : 0;
      const variants = loc && pathBanks.get(loc.id)?.variants;
      return variants?.[String(variant)] ?? variants?.['0'] ?? null;
    },
    // pv eventReturnInWorld: world state 15's return placed the rider itself (web/event-return.js sessionReturn), no reset / re-attach here
    sessionReturned() {
      freshPending = false;
      regionPlaced = false;
    },
    arrivalFor(course, opts) {
      const [index, kind] = arrivalRow(course);
      const s = api.spawnFor(course, index, kind, opts);
      if (s) s.entry = true;
      return s;
    },
    // A world load inside the loaded world (lodgeWorldLoad: the lodge's Return to Game, 0x1A11C0 -> 118loadoutlodge -> WS10):
    // the next placement is a new rider's (0x125EB8, fresh_rider_start) at the location entry 11D390, as after start().
    worldEntry(course) {
      const s = api.arrivalFor(course);
      if (s) freshPending = true;
      return s;
    },
    // main.js resetPhysics for a region spawn: the original placement (gates peak1-arrive-*). A location entry (11D390, entry)
    // first re-attaches the route at the grid slot (112180(rider, 1) placed there by 11D660), then 11DE60 -> 11D660 semantic 5
    // clearance 0 -> 11DF18 (core place_rider_region; transport: from the Transport loop's limbo, +0x370 = +0x380 = (0, 0, 1)).
    placeRegion(spawn) {
      if (spawn.entry && spawn.grid) {
        const [gx, gy, gz] = spawn.grid.position,
          [hx, hy] = spawn.grid.direction,
          tick = pv('gameTickKeep') && core._game_tick_restart ? core._game_tick() : null;
        core._reset_rider(gx / 100, gz / 100, -gy / 100, Math.atan2(hx, -hy));
        if (tick != null) core._game_tick_restart(tick);
        // pv gameTickKeep: main.js resetPhysics set the tick (11D390 leaves 1298C8)
        core._peak_world_reattach();
        regionPlaced = true;
      }
      const session = sessionPlacement(spawn); // world state 15: 30B7F8 before the placement, 11DF18(rider, 1) -> kind 5 after it
      if (session) core._mission_world_session?.(0);
      const fresh = freshPending && !spawn.transport && eventKind === FREE_RIDE_KIND;
      if (fresh) core._fresh_rider_start?.(); // the world load's new rider (limit, heading, surface 0)
      core._place_rider_region(...spawn.region.position, ...spawn.region.direction, spawn.transport ? 1 : 0);
      // the load's painter steps ran in a region not loaded yet, so the world painters start at their class
      // defaults (+0 = 0) and the location's Fog / Sun / ScreenTint / glare blend in at their rates (core environment_world_load)
      if (fresh) core._environment_world_load?.();
      if (session) core._mission_world_session?.(1);
    },
    // Loading screen: the start row's collision (the first location replaces the course world) and draw packages;
    // `step` is awaited between slices so the loading screen keeps animating.
    // `route`: the courses a peak run crosses (web/peak-run.js PEAK_RUNS), the whole mountain's prefetch window.
    async start(
      course,
      {
        kind = FREE_RIDE_KIND,
        mode = kind === FREE_RIDE_KIND ? 12 : kind === TIME_CHALLENGE_KIND ? 6 : 9,
        step = nextFrame,
        route: runRoute = null
      } = {}
    ) {
      // a frame between the feed slices (the load screen draws)
      const row = rowOf(course);
      if (!row) throw Error(`Course ${course} is not in ${manifest.name ?? 'PEAK1'}`);
      eventKind = kind;
      route = runRoute;
      freshPending = true;
      const startCodes = row.locations;
      for (const code of startCodes) peak.requestEnv(code);
      const first = Promise.all(startCodes.map(requestCore));
      let done = false;
      first.then(
        () => {
          done = true;
        },
        () => {
          done = true;
        }
      );
      while (!done || peak.busy()) {
        peak.pump(12);
        await step();
      }
      await first;
      {
        const p = put(JSON.stringify({ streaming: manifest.streaming, residency: manifest.residency }));
        try {
          core._peak_world_manifest(p);
        } finally {
          core._free(p);
        }
      }
      for (const code of startCodes) core._peak_world_data_ready(byCode.get(code).track, 1);
      core._peak_world_event_kind(kind);
      core._peak_world_game_mode(mode);
      core._peak_world_builtin108?.(1); // stage builtin 108: the stations' peak-race / jam fences follow the game mode
      core._stage_object_route?.(1); // the finish barriers' Object collision route (builtin 0 key 1, finishFences)
      core._set_piece_streamed?.(1); // the event locations' Spline pieces launch in the streamed world (builtin 19)
      core._stage_load_flags?.(1); // untouched instances hold the load's runtime flags (34FC1C): the one-way volumes keep their route
      // Rider lighting (web/environment_bridge.cpp): every IRR bank the Peak 1 Lighting painters and the 22E180 defaults use;
      // the painter region's section follows the rider (update), the default bank follows the Load triggers (22D088).
      if (core._lighting_banks) {
        const banks = await fetch(manifestRoot + 'lighting-banks.json')
          .then((r) => (r.ok ? r.text() : null))
          .catch(() => null);
        if (banks) {
          const p = put(banks);
          try {
            core._lighting_banks(p);
          } finally {
            core._free(p);
          }
        }
      }
      // Section activation 0x101B60 over the resident locations (challenge reset planes and mode fences kill themselves
      // in their slot-1 program; main.js runs section_pass after every tick).
      {
        const text = await fetch(manifestRoot + 'SECTIONS/sections.json')
          .then((r) => (r.ok ? r.text() : null))
          .catch(() => null);
        if (text && core._init_sections) {
          const p = put(text);
          try {
            core._init_sections(p);
          } finally {
            core._free(p);
          }
        }
      }
      // Every location's AIP path banks (paths.json), delivered by the core when the location becomes resident (12A340).
      // (sliceLoad: fetched together, handed to the core one per task: 16 parses in one task were ~90 ms at 4x CPU)
      const paths = await Promise.all(
        manifest.locations
          .filter((l) => l.id < 22)
          .map(async (l) => [
            l,
            await fetch(l.root + 'paths.json')
              .then((r) => (r.ok ? r.text() : null))
              .catch(() => null)
          ])
      );
      for (const [l, text] of paths) {
        if (!text) continue;
        pathBanks.set(l.id, JSON.parse(text));
        const p = put(text);
        try {
          core._peak_world_paths(l.id, p);
        } finally {
          core._free(p);
        }
        await step();
      }
      core._peak_world_start(course);
      // The stage world (particles, LiveComp, flags, UV scroll, instance states) of every location, one core stage context.
      setPieces = await createPeakSetPieces({ core, parent: group, root: manifestRoot + 'SETPIECES/', origin }).catch((e) => {
        console.warn('Peak 1 set pieces unavailable', e);
        return null;
      });
      await Promise.all(startCodes.map(buildRender));
      started = true;
      update();
      if (windowed) prefetchWindow(course);
      planAhead(course); // one read at a time: the next rows' collision and draw data, then the rest of the peak (update)
    },
    // One game tick, after the rider pass (race_end): the streaming rows advance, stage requests reach the host.
    tick() {
      if (!started) return;
      core._peak_world_tick();
      drainEvents();
    },
    // A new run placed the rider (main.js startRun): re-attach it to the current location's path banks (112180).
    // Peak runs: the run's setup placed the rider and ran frames before the objectives card's Continue (PS2 peak1-race-objectives:
    // +0x2E4 = 3333.33 retained, +0x380 = +0x370): peak_run_start_seed (web/core.cpp, gate peak1-race-start).
    // Collectibles: the career rows / CTM path after the run's stage reset, then the slot rebuilt with them (web/stage-collect.js).
    afterReset() {
      freshPending = false;
      if (!started) return;
      collectStart?.();
      core._peak_collect_refresh?.();
      if (!regionPlaced) core._peak_world_reattach();
      regionPlaced = false;
      if (eventKind !== FREE_RIDE_KIND) {
        core._peak_run_start_seed?.();
        core._race_clock_restart();
        splitCounter = 0;
        split = null;
      }
    },
    // Peak runs: the station split shown for 5 s (web/peak-run.js peakSplit) and its counter (handler+8).
    split: () => split,
    update,
    // bootChain (main.js unloadCourse): the warms in flight settled, at most `ms` (stop() first: no new ones start)
    settle(ms = 1500) {
      return Promise.race([Promise.allSettled([...inflight]), new Promise((r) => setTimeout(r, ms))]);
    },
    stop() {
      stopped = true;
      for (const job of releasing.splice(0)) for (const step of job) step();
      setPieces?.dispose();
      core._lighting_streamed_off?.();
      core._stage_load_flags?.(0);
      core._peak_world_stop();
      started = false;
      for (const s of render.values())
        if (s.group) {
          group.remove(s.group);
          releaseLocation(s.group);
        }
      // releaseLocation: textures, atlas and render objects too (main.js course change)
      render.clear();
      peak.dispose();
      parent.remove(group);
    }
  };
  // Per frame: draw packages for the wanted rows (and their neighbours), visibility = resident, releases, feeding.
  function update(budgetMs = 3) {
    if (!started) return;
    frameStart = performance.now();
    frameBuildMs = 0;
    const now = performance.now(),
      current = rows().filter((r) => r.cls === 0 && byCode.get(r.code)?.root);
    // Draw packages for every wanted row (built while the location reads), shown exactly while active (track state 6).
    const want = new Set(current.filter((r) => WANTED_STATES.has(r.state)).map((r) => r.code));
    wantedCodes = want;
    for (const code of want) {
      buildRender(code);
      peak.requestEnv(code);
      if (!peak.loaded(code)) {
        requestCore(code);
        peak.prioritize(code);
      }
    }
    const drawn = new Set(current.filter((r) => r.state === ROW.ACTIVE).map((r) => r.code));
    listeners.drawn?.(current.filter((r) => r.state === ROW.ACTIVE).map((r) => r.track)); // light glows etc. of the drawn locations
    let released = false;
    // a draw package the rider needs (a wanted row's, or the row a connector leads to) still building: the releases wait
    // (their disposals compete with that build in WebKit; a stalled Load trigger held the game longer), at most RELEASE_MS
    const building = [...render].some(([c, sl]) => sl.state === 'building' && (want.has(c) || aheadBuild.has(c)));
    for (const [code, slot] of render) {
      if (!slot.group) continue;
      slot.group.visible = drawn.has(code);
      sparkle?.setVisible(code, slot.group.visible);
      // no compile ran (streamWarm off): after its first draws
      if (slot.group.visible && !slot.released && now - (slot.shownAt ??= now) > 2000) {
        slot.released = true;
        slot.group.userData.releaseGpuCopies?.();
      }
      // the core's collision too (one rider context: a free ride or a peak run)
      if (want.has(code) || aheadBuild.has(code) || aheadKeep.has(code)) slot.lastSeen = now;
      // at once (the PS2's eviction), but a row read ahead for a way on not taken (a station's other connectors) keeps
      // the 45 s: the rider may still turn to it
      else if (now - slot.lastSeen > (!building && !aheadNext.has(code) && !prefetched.has(code) ? 0 : RELEASE_MS) && !released) {
        setPieces?.detach(code);
        sparkle?.detach(code);
        group.remove(slot.group);
        releasing.push(releaseSteps(slot.group));
        released = true;
        render.delete(code);
        if (!prefetched.has(code)) {
          peak.dropEnv(code);
          peak.releaseCore(code);
        }
      }
    }
    if (releasing.length) releasePump(idle() ? 12 : 2); // (streamAhead: one location a frame, disposed a few steps a frame)
    releaseIdleCores(now, want);
    // A location that is (or is about to be) collidable without its data in the core: hold the game like the original's
    // +0x1D0 wait (2306B8 skips the gameplay part of the frame) and feed with a large budget.
    // the draw package is part of the location's data (the PS2 reads both in one): an active row whose package is still
    // building holds the game too, so the rider never stands on a location that is not drawn.
    stalled =
      core._peak_world_blocking() !== 0 ||
      current.some((r) => (r.state === ROW.ACTIVE || r.state === ROW.READING_ACTIVE) && (!peak.loaded(r.code) || !drawReady(r.code)));
    // Painter region: the track of the last contacted terrain patch (terrain_contact_info[0] = rider+0x430).
    const patch = new Float32Array(core.HEAPF32.buffer, core._terrain_contact_info(), 12)[0];
    if (patch > 0) {
      const code = manifest.locations.find((l) => l.track === (patch & 255))?.code;
      if (code && code !== region) {
        region = code;
        regionLighting(byCode.get(code));
        listeners.region?.(byCode.get(code));
      }
    }
    const sky = core._peak_world_sky();
    if (sky !== skyId) {
      skyId = sky;
      listeners.sky?.(manifest.streaming.find((r) => r.id === sky)?.code);
    }
    aheadStep(current, want);
    // a wanted row's data feeds at once; the background's (the next rows, the rest of the peak) only while nothing
    // animates (the arrival movie, a pause): its single core calls (a location's rails, the terrain traversal merge) take 100-250 ms
    // on a phone-class CPU (Chrome 4x).
    const wantedMissing = current.some((r) => WANTED_STATES.has(r.state) && !peak.loaded(r.code));
    // (the row read ahead for the rider's way on feeds 2 ms a frame while riding: it must be in before its Unload trigger)
    const aheadFeed = [...aheadBuild].some((c) => peak.locations.get(c)?.core === 'queued');
    peak.pump(
      stalled ? Math.max(12, STALL_MS - (performance.now() - frameStart)) : wantedMissing ? budgetMs : idle() ? 12 : aheadFeed ? 2 : 0
    );
    setPieces?.update();
  }
  // ---- streamAhead: reading ahead in the PS2's order (docs/ctm-flow.md "Start and streaming") ------------------------------
  // The PS2 reads a row's locations from the connector's Unload trigger, one disc read at a time (22D8D8: sky > TRANSP > location,
  // then the lowest chunk), and has them long before the Load trigger (6-11 s later). The page's "read" is a download, a worker
  // cut, the core feed, the draw build and its pipelines: so the rows the current row's connectors lead to are read ahead, one
  // item at a time in that order: their collision, then their draw files (into the HTTP cache only: no copy is kept), then the
  // rest of the peak's collision. A connector X_Y leads from X down to Y (ABC1_A -> A, A_ARA1 -> ARA1, ARA1_B -> B). Once the rider
  // rides on a connector, the row it leads to is built (hidden) ahead of its Unload trigger.
  const chunkOf = (code) => manifest.streaming?.find((r) => r.code === code)?.chunk ?? 1e9;
  function planAhead(course) {
    const onRoute = routeNext(course); // a peak run's route row only (the other connectors' rows: fed for nothing)
    const next = aheadLocations(manifest.residency, course, chunkOf).filter(
      (c) => byCode.get(c)?.root && (!onRoute || onRoute.locations.includes(c))
    );
    // a single way on (Happiness -> Green Base Station, Snow Jam -> Blue Base Station): the rider meets that Unload trigger for sure, so
    // the row is also built (hidden) the first time nothing animates (the arrival movie): the PS2 has its reads done long before the
    // Load trigger, and a page build while riding costs frames (Safari: 7-12 s of pipelines for a hub)
    const single = new Set(onRoute || nextRows(course).length === 1 ? next : []);
    aheadKeep = single;
    aheadNext = new Set(next);
    aheadQueue = [
      ...next.map((code) => ({ kind: 'core', code })),
      ...next.map((code) => ({ kind: 'draw', code })),
      ...[...single].map((code) => ({ kind: 'build', code }))
    ];
  }
  const nextRows = (course) => {
    const row = rowOf(course),
      out = [];
    for (const c of row?.locations ?? []) {
      const d = connectorDest(manifest.residency, c);
      if (d && d !== row && !out.includes(d)) out.push(d);
    }
    return out;
  };
  const warmed = new Set();
  // A location's draw files through the HTTP cache (the same GETs main.js asset() makes: world.json, the vertex / index / colour
  // buffers, the original world material's terrain files, the sparkle patches, its own PNG textures); the bodies are read and
  // dropped, so nothing stays in the page's memory. `fetch` with a signal bypasses web/downloads.js (no shared copy).
  async function warmDraw(code) {
    const l = byCode.get(code);
    if (!l?.root || warmed.has(code)) return;
    warmed.add(code);
    const abort = new AbortController(),
      get = async (url) => {
        const r = await fetch(url, { signal: abort.signal });
        if (!r.ok || !r.body) {
          await r.arrayBuffer?.().catch(() => {});
          return r.ok ? r : null;
        }
        const reader = r.body.getReader();
        for (;;) {
          const { done } = await reader.read();
          if (done) break;
        }
        return r;
      };
    const world = await fetch(l.root + 'world.json', { signal: abort.signal })
      .then((r) => (r.ok ? r.json() : null))
      .catch(() => null);
    const files = [
      'vertices.bin',
      'indices.bin',
      'colors.bin',
      'terrain-render.json',
      'terrain-lighting.json',
      'terrain-light-atlas.png',
      'vertex-alpha.bin'
    ];
    if (sparkle) files.push('terrain-sparkle.bin');
    for (const t of Object.values(world?.textures ?? {})) if (t.pack === undefined && t.path) files.push(t.path);
    for (const f of files) {
      if (!started) {
        abort.abort();
        return;
      }
      await get(l.root + f).catch(() => null);
    }
  }
  function aheadStep(current, want) {
    // a connector being ridden: build the row it leads to now (its Unload trigger will want it), hidden until the row activates
    aheadBuild = new Set();
    const here = core._peak_world_course();
    let dest = connectorDest(manifest.residency, region);
    // A row with several ways on (a station): the one whose connector the rider is nearest (its ground extent, horizontally) is
    // read ahead, so a course's draw package is built while the rider is still at the station (ARA1: 3442 batches, ~25 s of
    // one-slice-a-frame work on a phone at 4x CPU); another connector takes over only once it is 25 m nearer.
    if (!dest || dest.course === here) {
      const row = rowOf(here),
        outs = (row?.locations ?? []).filter((c) => {
          const d = connectorDest(manifest.residency, c);
          return d && d !== row && peak.locations.get(c)?.bounds;
        });
      const nx = routeNext(here); // a peak run's station builds its route's row, not the nearest connector's
      if (outs.length > 1 && nx && outs.some((c) => connectorDest(manifest.residency, c) === nx)) dest = nx;
      else if (outs.length > 1) {
        const st = new Float32Array(core.HEAPF32.buffer, core._rider_state(), 3),
          dist = (c) => {
            const [lo, hi] = peak.locations.get(c).bounds,
              dx = Math.max(lo[0] - st[0], 0, st[0] - hi[0]),
              dz = Math.max(lo[2] - st[2], 0, st[2] - hi[2]);
            return Math.hypot(dx, dz);
          };
        const near = outs.reduce((a, b) => (dist(b) < dist(a) ? b : a));
        if (!outs.includes(aheadTarget) || dist(near) < dist(aheadTarget) - 25) aheadTarget = near;
        dest = connectorDest(manifest.residency, aheadTarget);
      }
    }
    if (dest && dest.course !== here)
      for (const c of dest.locations)
        if (byCode.get(c)?.root && !want.has(c)) {
          aheadBuild.add(c);
          buildRender(c);
          if (!peak.loaded(c)) {
            requestCore(c);
            peak.prioritize(c);
          }
        }
    // one background read at a time, and none while a wanted row still waits for its data (that row's reads go first)
    if (aheadJob) {
      const st = peak.locations.get(aheadJob.code)?.core;
      if (
        aheadJob.kind === 'core'
          ? st === 'fetching' || (aheadJob.feed && st === 'queued')
          : aheadJob.kind === 'build'
            ? !drawReady(aheadJob.code)
            : !aheadJob.done
      )
        return;
      aheadJob = null;
    }
    if (current.some((r) => WANTED_STATES.has(r.state) && (!peak.loaded(r.code) || !drawReady(r.code)))) return;
    while (aheadQueue.length) {
      const job = aheadQueue[0];
      if (job.kind === 'build' && !idle() && !render.has(job.code)) return; // built while nothing animates
      aheadQueue.shift();
      if (job.kind === 'core') {
        if (peak.locations.get(job.code)?.core !== 'absent') continue;
        requestCore(job.code);
        aheadJob = job;
        return;
      }
      if (job.kind === 'build') {
        if (render.has(job.code)) continue;
        buildRender(job.code);
        aheadJob = job;
        return;
      }
      if (warmed.has(job.code) || render.has(job.code)) continue;
      aheadJob = job;
      warmDraw(job.code)
        .catch(() => {})
        .finally(() => {
          job.done = true;
        });
      return;
    }
  }
  api.ahead = () => ({
    queue: aheadQueue.map((j) => `${j.kind}:${j.code}`),
    job: aheadJob && `${aheadJob.kind}:${aheadJob.code}`,
    build: [...aheadBuild],
    warmed: [...warmed]
  }); // QA
  api.region = () => region;
  // pv worldWarm: the start row's locations first (the arrival draws them; main.js warmWorld waits for api.rewarmLead under the load screen), then the
  // rest (read-ahead builds), each part the small ones first. Off: every slot, the small ones first.
  api.rewarmLead = null;
  api.rewarmLeadCodes = null;
  api.rewarm = async () => {
    const bySize = (a, b) => (a.group?.children.length ?? 0) - (b.group?.children.length ?? 0),
      slots = [...render.values()].sort(bySize);
    const lead = pv('worldWarm') ? new Set(rowOf(core._peak_world_course())?.locations ?? []) : null,
      first = lead ? slots.filter((sl) => lead.has(sl.group?.userData.peakLocation)) : [];
    const warmSlot = async (slot) => {
      if (!slot.group) return;
      slot.times.rewarm = performance.now();
      await warmSliced(slot.group, slot.group.visible, slot.group.userData.peakLocation);
      slot.times.rewarmed = performance.now();
      slot.group.userData.releaseGpuCopies?.();
      slot.released = true;
    };
    let leadDone = null;
    if (lead) {
      api.rewarmLead = new Promise((r) => {
        leadDone = r;
      });
      api.rewarmLeadCodes = lead;
    }
    try {
      for (const slot of first) await warmSlot(slot);
    } finally {
      leadDone?.();
    }
    for (const slot of slots) if (!first.includes(slot)) await warmSlot(slot);
  }; // gpuRelease after the compile // streamWarm (main.js loadCourse): the small ones first
  api.prebuild = (code) => buildRender(code); // QA / idle prefetch of a location's draw package
  // QA: put the rider at a PS2 capture spot (source cm, heading from the source forward x/y, source cm/s).
  // QA: switch the streaming to a course's row as its Load trigger would (the data loads, then the row activates).
  api.request = (course) => core._peak_world_request(course, 1);
  api.place = ([x, y, z], [hx, hy] = [0, 1], velocity = null) => {
    core._reset_rider(x / 100, z / 100, -y / 100, Math.atan2(hx, -hy));
    if (velocity) core._set_rider_velocity(...velocity);
    core._peak_world_reattach();
  };
  api.stalled = () => stalled;
  // Source velocity forward x speed with z = 0 from the placed orientation (quaternion x, y, z, w; forward = R(q) (0, 1, 0)).
  api.forwardVelocity = (speed) => {
    const [qx, qy, qz, qw] = new Float32Array(core.HEAPF32.buffer, core._rider_orientation(), 4);
    return [2 * (qx * qy - qw * qz) * speed, (1 - 2 * (qx * qx + qz * qz)) * speed, 0];
  };
  // Transport inside the world (MCOMM Transport / booth -> "Transport to this area now?" Yes: world state 14 arg 1, 236250/236418):
  // - the destination is the current location (outside the backcountry): P[0xA]+0x10 = 1, state 15 = Session point 1;
  // - else the transport cut, then 22CEA8(dest, 7): the crossing streams the destination rows while the loading loop plays
  //   (screen 11, transport arrival +0x1C8: 236960 waits for 22D278 = every wanted row resident), the arrival placement
  //   123F38 -> 11D390 (arrivalFor), then 22D088(dest, 7) activates the rows. The game ticks do not run meanwhile (the world
  //   is paused behind the loop); the streamer passes do, one per 1/60 s, so the reads take their disc time.
  // Returns the native spawn for main.js (resetPhysics + afterReset). Cut hooks: web/cutscenes.js.
  // ticking (pv nisTick, main.js): the world runs under the ride with the rider held by the cut's actor (PS2 f95-after: the game tick runs
  // through the in-air ride and the held loop), so the game ticks run the streamer passes (game-tick.js -> tick()); none by hand here.
  // pv transportFade (docs/ctm-parity.md "The Transport's presentation"): the ride is ONE list (27A860: departure, in-air, the held loop,
  // then WS10's appended heli drop into a backcountry); the destination is requested when the held loop starts (0x2366C4 22CEA8(dest, 7)),
  // the release (0x236AA8 -> 27A9F0) loads it without the dome (22D088), the loop plays on for 30 ticks fading to black and the list
  // stops at R+29 (the rider placed by main.js then); the dome is allowed afterwards (0x235808). onRelease: world state 10 enter (R+1).
  api.transport = async (
    dest,
    { rider = null, step = () => new Promise((r) => requestAnimationFrame(r)), firstVisit = false, ticking = false, onRelease = null } = {}
  ) => {
    const row = rowOf(dest);
    if (!row || !started) return null;
    const here = core._peak_world_course();
    if (dest === here && !(dest >= 14 && dest <= 16)) {
      const s = api.spawnFor(dest, 1, 2);
      if (s) s.sameLocation = true;
      return s;
    } // state 15 (236058): Session point 1 and the white fade (main.js)
    const oneList = pv('transportFade');
    let done;
    const arrived = new Promise((r) => {
      done = r;
    });
    let loop;
    if (oneList) {
      let looping;
      const held = new Promise((r) => {
        looping = r;
      });
      loop = playCutscene({
        kind: 'transport-ride',
        id: null,
        rider,
        location: dest,
        until: arrived,
        firstVisit: !!firstVisit,
        skipLock: 30,
        onStep: (st) => {
          if (st.flags & 8) looping(true);
        }
      });
      loop.then(
        () => looping(false),
        () => looping(false)
      );
      await held;
    } else {
      await playCutscene({ kind: 'transport-depart', id: 0xb, rider, location: dest });
      loop = playCutscene({ kind: 'transport-loop', id: null, rider, location: dest, until: arrived });
    }
    core._peak_world_transport(dest, 0);
    drainEvents();
    for (const code of row.locations) {
      requestCore(code);
      peak.prioritize(code);
    }
    let clock = performance.now(),
      passes = 0;
    while (passes < 60 * 120) {
      // the reads of a whole row are ~10 s at most (ARA1 250 ticks + connectors)
      await step();
      const now = performance.now(),
        n = Math.min(8, Math.floor((now - clock) * 0.06));
      clock += n / 0.06;
      if (ticking) passes += n;
      else
        for (let k = 0; k < n; k++) {
          core._peak_world_tick();
          drainEvents();
          passes++;
        }
      update(12); // draw packages for the wanted rows, collision data with a large budget
      if (
        core._peak_world_request_resident() &&
        row.locations.every((c) => peak.loaded(c)) &&
        row.locations.every((c) => render.get(c)?.state === 'ready' || render.get(c)?.state === 'failed')
      )
        break;
    }
    const spawn = api.arrivalFor(dest);
    if (spawn) spawn.transport = true; // 236960 -> 123F38 -> 11D390 from the loop's limbo
    if (oneList) {
      core._peak_world_transport(dest, 2);
      drainEvents();
      update(); // 22D088(dest, 7) at the release, the dome not yet
      done();
      onRelease?.();
      await loop;
      core._peak_world_transport(dest, 3); // 0x235808: list 1's head is no longer a Transport step
      return spawn;
    }
    core._peak_world_transport(dest, 1);
    drainEvents();
    update();
    done();
    await loop;
    // world state 14 arg 1 queues no cinematic at a station. The walk-in lists 22 / 23 and the lodge prompt 0x1F
    // are arg 0's (236250 / 236418), which only the lodge door volume starts (builtin 68 action 4); the arrival 236960 places the
    // rider (123F38 -> 11D390 -> 11DE60(rider, 0, 2)), releases the held loop (27A9F0) and the world fades in (PS2 Snow Jam -> Green,
    // Ruthless -> Yellow station: the loop, then riding from the station's first row).
    // a transport into a backcountry (the streamer's +0x1C8 transport flag): WS10 update 0x235080 queues the heli drop
    // [16 <bc>_heli_arr, 17 heli_arrb_<char>] (flags 3) for a visited backcountry (a first visit is the world load's cut)
    // a peak's backcountry the rider has not visited is reached by a transport inside the one world ("Go to this peak
    // now?"), and WS10 plays its first arrival (the DBC2 / EBC3 movie, then the heli drop; 0x234F40 / 0x235080 with +0xACC clear)
    if (dest >= 14 && dest <= 16)
      await playCutscene({
        kind: 'arrival',
        id: null,
        rider,
        location: firstVisit ? ['ABC1', 'DBC2', 'EBC3'][dest - 14] : dest,
        firstVisit: !!firstVisit
      });
    return spawn;
  };
  api.setPieces = () => setPieces; // QA
  return api;
}

// streamAhead (docs/ctm-flow.md "Start and streaming"): a connector location X_Y leads from X down to Y (ABC1_A -> A, A_ARA1 -> ARA1,
// ARA1_B -> B; the Unload trigger on its upper side requests Y's residency row). connectorDest: the row a connector leads to (null for
// a course / hub, or a connector out of this world: PEAK2's DRA4_A). aheadLocations: the locations of the rows the course's row
// leads to, not already in it, each row in the PS2's read order (22D8D8: the lowest chunk first).
export function connectorDest(residency, code) {
  const i = code ? code.indexOf('_') : -1; if (i <= 0) return null;
  return residency.find((r) => r.code === code.slice(i + 1)) ?? null;
}
export function aheadLocations(residency, course, chunkOf = () => 0) {
  const row = residency.find((r) => r.course === course); if (!row) return [];
  const here = new Set(row.locations), out = [];
  for (const c of row.locations) {
    const dest = connectorDest(residency, c); if (!dest || dest === row) continue;
    for (const d of [...dest.locations].sort((a, b) => chunkOf(a) - chunkOf(b))) if (!here.has(d) && !out.includes(d)) out.push(d);
  }
  return out;
}
// 26B5E0(bank, kind, index): the region row with runtime kind (+4) and index (+0), else the bank's first row. The exported
// (disc) kinds are the runtime kinds - 1 (12A340 installs them + 1: PS2 fr/sj-01, no-a, race18001).
export const regionRow = (bank, index, kind) => bank.regions.find((r) => r.kind + 1 === kind && r.index === index) ?? bank.regions[0] ?? null;
// 11D390: the region placement of a location entry in kinds 4..6 -> [index, runtime kind].
export const arrivalRow = (course) => (course < 14 ? [1, 2] : course < 17 ? [0, 1] : [0, 2]);
// World state 15 (236058): a Session point k + 1 >= 1 (MCOMM Session, a Transport to the current location, the post-event
// Transport back to the same course): 30B7F8 resets the missions, 11DE60(rider, k + 1, 2), then 11DF18(rider, 1) posts the
// WScript reset event (kind 5; core mission_world_session, web/mission_gameplay.inc). A location entry (11D390: arrivals, the
// station ride-in at session point 0) calls 11DF18(rider, 0) and posts nothing.
export const sessionPlacement = (spawn) => !!spawn && !spawn.entry && spawn.session?.kind === 2 && spawn.session.index >= 1;
// The Conquer-the-Mountain first run starts in the air over Happiness, dropped from the plane (PS2 menus/fr/ctmstart,
// first race tick; source cm and cm/s).
// Only the plane drop has no session point: every other start is the course's session point 0 (spawnFor).
export const PEAK_STARTS = {
  14: { position: [-29710.6, 28535.4, -186362.5], velocity: [0, 0, -253], heading: [-1991, -240], drop: true }, // drop: web/plane-drop.js
};
const nativeStart = ({ position: [x, y, z], velocity, heading, drop }) => {
  const [hx, hy] = heading ?? velocity;
  return { position: [x / 100, z / 100, -y / 100], heading: Math.atan2(hx, -hy), velocity, ...(drop ? { drop } : {}) };
};
// Streamed peak worlds (tools/export_peak_world.py --peak N; docs/peak3.md): the career peak of a course (course table +0x54,
// 0x144C78) names its world; each world's base course package supplies the shared per-course settings.
export const COURSE_PEAK = Object.freeze([1, 1, 2, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 1, 2, 2, 3]);
export const peakWorldOf = (course) => `PEAK${COURSE_PEAK[course] ?? 1}`;
// mountainRide (docs/ctm-parity.md "The whole mountain"): the Conquer the Mountain free ride in the whole-mountain world MOUNTAIN, as
// the PS2 has it (one world: the bottom of Intimidator streams into Green Base Station, Gravitude into Yellow Mid Station), on every tier:
// the core frees a released location's collision, so a phone's session stays bounded too. ?mountain=0|1 overrides (QA).
export function mountainFreeRide() {

  let q = null; try { q = new URL(globalThis.location?.href ?? 'http://localhost/').searchParams.get('mountain'); } catch {}
  if (q === '0' || q === '1') return q === '1';
  return true;
}
// The world a career free ride at `course` loads: MOUNTAIN (mountainRide, desktop) or the course's peak world.
export const freeRideWorldOf = (course) => (mountainFreeRide() ? 'MOUNTAIN' : peakWorldOf(course));
// Whether the page's world `here` (its course object) holds a career free ride at `course`, so a Transport stays inside it (main.js
// cb.freeRide -> transportInWorld; ctm-transport.js switchesWorld). The PS2 is one world: a Transport is world state 14 arg 1 (enter 0x236250)
// in the loaded mountain, never a world load. mountainRide: a free ride keeps a world that already holds the destination (a peak world
// left from a tier change in Options > Display & Touch) instead of reloading into MOUNTAIN for a station of the same peak.
export function freeRideHolds(here, course) {
  if (!here?.code) return false;
  if (freeRideWorldOf(course) === here.code) return true;
  return here.freeRide?.kind === FREE_RIDE_KIND && (here.code === 'MOUNTAIN' || peakWorldOf(course) === here.code);
}
export const PEAK_DEFAULT_STATION = Object.freeze({ PEAK1: 17, PEAK2: 19, PEAK3: 21, MOUNTAIN: 21 });
export const PEAK_BASE_COURSE = Object.freeze({ PEAK1: 'ARA1', PEAK2: 'CRA3', PEAK3: 'ERA5' });
// Streamed worlds: one peak (PEAK1..3) or the whole mountain (MOUNTAIN, tools/export_mountain_world.py, docs/peak3.md section 6).
export const isStreamedWorld = (code) => /^PEAK\d$/.test(code ?? '') || code === 'MOUNTAIN';
// Peak runs: modes 6/9 Peak 1 and the Peak 2 Jam (10: Ruthless -> Style Mile) stay on their peak; the Peak 2 Race (7: Ruthless ->
// Intimidator -> DRA4_A -> Green station ...) and the All Peak Race / Jam (8 / 11: The Throne -> ... -> ERA5_C -> Yellow station
// ...) end at Metro-City (10E5D8), so they run in the whole-mountain world (docs/peak3.md section 6, docs/peak2.md).
export const peakRunWorld = (mode) => (mode === 7 || mode === 8 || mode === 11 ? 'MOUNTAIN' : `PEAK${[1, 2, 3][(mode - 6) % 3]}`);
// Course entry for ?course=PEAK<N>[&peakCourse=N]: the peak's base course package (Snow Jam for Peak 1) supplies the shared
// per-course settings (animation settings, environment lattice, lights, fog, sky), the world itself is the streamed locations.
export function peakCourseEntry(courses, params) {
  const world = isStreamedWorld(params.get('course')) ? params.get('course') : 'PEAK1', number = world === 'MOUNTAIN' ? 0 : Number(world[4]);
  const peakMode = params.has('peakMode') ? Number(params.get('peakMode')) : null;
  const run = peakMode != null ? PEAK_RUNS[peakMode] : null;
  // The whole mountain takes the shared per-course settings of the start course's peak (The Throne: Gravitude's; Ruthless:
  // Ruthless Ridge's) like the peak worlds do.
  // (mountainRide: a free ride takes the start course's peak too; before, always The Throne's)
  const freeStart = !run && params.has('peakCourse') ? Number(params.get('peakCourse')) : null;
  const baseCode = world === 'MOUNTAIN' ? PEAK_BASE_COURSE[`PEAK${COURSE_PEAK[run?.start ?? freeStart ?? 21] ?? 3}`] : PEAK_BASE_COURSE[world];
  // light glows: every location's kind-7 sources of the peak, drawn for the drawn locations (PEAK<N>/LIGHT_GLOW, light-glow.js setTracks)
  const base = { ...(courses.find((c) => c.code === baseCode) ?? courses.find((c) => c.code === 'ARA1')), lightGlow: `/assets/${world}/LIGHT_GLOW/`, lightGlowCount: undefined };
  const name = world === 'MOUNTAIN' ? 'All Peaks' : `Peak ${number}`;
  if (run) {
    // A peak run (modes 6..11): the start course's row, the grid slot 0 of its AIP (region kind 0), no countdown.
    return { ...base, code: world, name, label: run.name, event: 'peakrun', eventStart: false, environmentRoot: `/assets/${world}/`,
      freeRide: { course: run.start, kind: peakKind(peakMode), mode: peakMode, spawn: null, grid: true, route: run.route } };
  }
  const start = Number(params.get('peakCourse') ?? PEAK_DEFAULT_STATION[world]);
  return { ...base, code: world, name, label: `${name} - Freeride`, event: 'freeride', eventStart: false, environmentRoot: `/assets/${world}/`,
    freeRide: { course: start, kind: FREE_RIDE_KIND, mode: 12, spawn: PEAK_STARTS[start] ? nativeStart(PEAK_STARTS[start]) : null } };
}
export const stationStart = (course) => (PEAK_STARTS[course] ? nativeStart(PEAK_STARTS[course]) : null);
