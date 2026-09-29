// Stage world of the streamed Peak 1 world (docs/peak-mountain.md "Set pieces"): the same entity/modifier systems as a race
// event (web/set-pieces-renderer.js, docs/set-pieces.md), for the locations the streaming makes resident.
//
// The original keeps ONE stage context for every resident location (one set of entity lists; resource ids carry the SDB track),
// so the core runs a single stage world for PEAK1: init_stage_world with the merged exports of tools/export_peak_world.py
// (PEAK1/SETPIECES: particle owners/carriers, LiveComp models, MeshAnim models, script-changed instances). Nothing is seeded
// from a race-tick-0 savestate: in free ride the section activation (0x101B60, web/section_gameplay.inc) builds the entities
// when a section is entered and its leave / the location's unload destroys them (web/peak_world.inc).
// This module draws what the core runs, over the location draw packages web/free-ride.js builds and releases:
//   - particles (builtins 16/25/26/69, MultiParticle 105/106) and halos (97): one pass for the whole world;
//   - LiveComp players (builtin3, section / contact / timer starts from the core's logs), UV scroll (21), flag cloth (12):
//     JS players keyed by resource, applied to the meshes of every attached location group;
//   - instance states (DeadNode / Hide / RestoreNode, breaking MeshAnim pieces) and point-pickup magnets.
//   - pv peakAttached: ParentModifier children (attached.json, parentKind 'livecomp'), with their parent's node matrix.
// Not here: the race GO doors (no race start in free ride), the chairlift / MultiSpline cars, the spline LiveComps (ravens,
// blimps: the core's set_piece_attached) and the hubs' own set pieces (no event export yet).
import * as T from 'three/webgpu';
import {FlagAnimation, flagTriangleIndices} from './flag-animation.js';
import {UvScroll} from './uv-scroll.js';
import {worldUvScroll} from './world-material.js';
import {LiveCompAnimation, syncSectionClocks} from './livecomp-animation.js';
import {AttachedSetPieces} from './attached-setpieces.js';
import {createSetPieceParticles} from './set-piece-particles.js';
import {createSetPieceHalos} from './set-piece-halos.js';
import {createCrowd2d} from './crowd-2d.js';
import {organizeStaticWorld, releaseStaticWorld} from './static-world.js';
import { loadAvalanches } from './avalanche-load.js';
import {pv} from './pv-flags.js';
import { initStageWorldSliced } from './load-slices.js';
import { CABLES, CABLE_OWNERS, createLocationCables, cableProxy, streamedCableRoot } from './set-piece-cables.js';
import { createFogPuffs, streamedFogRoot } from './fog-puffs.js';

const params = new URL(globalThis.location?.href ?? 'http://x/').searchParams; // ?particles=0 / ?livecomp=0 / ?flags=0 / ?uvscroll=0
const sortedJson = (value) => JSON.stringify(value, Object.keys(value).sort());
const FLAG_TEXTURES = [138, 268, 351];
const CROWD_COURSES = new Set(['ARA1', 'BRA2', 'BHP1', 'ASS1', 'ABA1', /* Peak 2, docs/peak2.md */ 'CRA3', 'DRA4', 'DSS2', 'CBA2', 'CHP2', /* Peak 3, docs/peak3.md */ 'ERA5', 'ESS3', 'EBA3', 'EHP3', 'EBC3']); // event packages with CROWD/crowd.json // cloth batches of the flag models (a flag instance's other batches are its pole)

export async function createPeakSetPieces({core, parent, root = '/assets/PEAK1/SETPIECES/', origin = [0, 0, 0]}) {
  const text = (name) => fetch(root + name).then((r) => (r.ok ? r.text() : null)).catch(() => null);
  const [particleText, liveText, stageText, flagText, uvText] = await Promise.all(['particles.json', 'livecomp.json', 'stage-world.json', 'flags.json', 'uv-scroll.json'].map(text));
  if (!particleText || !core._init_stage_world) return null;
  const put = (t) => { const b = new TextEncoder().encode(t + '\0'), p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
  const ptrs = [particleText, liveText ?? '', stageText ?? ''].map(put);
  let coreStarts = false;
  // pv sliceLoad: the same load in small parts cut off the main thread (web/load-slices.js; the merged peak's one call: 0.3-0.4 s at 4x CPU)
  if (pv('sliceLoad') && core._stage_world_part) { ptrs.forEach((p) => core._free(p)); coreStarts = (await initStageWorldSliced(core, { particles: root + 'particles.json', livecomp: liveText ? root + 'livecomp.json' : null, stage: stageText ? root + 'stage-world.json' : null })) > 0 && !!liveText; }
  else try { coreStarts = core._init_stage_world(...ptrs) > 0 && !!liveText; } finally { ptrs.forEach((p) => core._free(p)); }
  const liveData = liveText ? JSON.parse(liveText) : null, flagData = flagText ? JSON.parse(flagText) : null, scrollData = uvText ? JSON.parse(uvText) : null;
  // pv peakAttached (docs/visual-parity.md 41.6): the ParentModifier children of every event course of the world, split on node 0 by the export
  const attachedData = pv('peakAttached') && params.get('attached') !== '0' ? await text('attached.json').then((t) => (t ? JSON.parse(t) : null)) : null;
  const attached = attachedData ? new AttachedSetPieces(attachedData, {core}) : null;
  // Avalanches (web/avalanche_gameplay.inc): every location of the world with a recorded one (the world's peak.json roots).
  { const manifest = await fetch(root.replace(/SETPIECES\/$/, '') + 'peak.json').then((r) => (r.ok ? r.json() : null)).catch(() => null);
    await loadAvalanches(core, (manifest?.locations ?? []).map((l) => ({ code: l.code, root: l.root }))); }
  // The world's flag manager in the core (web/stage_world.inc init_streamed_flags): its grid builds and 1 s wind draw from the one
  // visual stream in the original order (0x34B228 in the section pass, 0x34C668 in group 2, the mode of the current course); the
  // JS cloth consumes the same words in the same order (stage_world_flag_words: the words, then each word's wind mode).
  const coreFlags = !!(flagText && core._init_streamed_flags && core._stage_world_flag_words) && (() => { const p = put(flagText); try { return core._init_streamed_flags(p) > 0; } finally { core._free(p); } })();
  let flagWordCursor = 0;
  const flagWords = () => { const U = new Uint32Array(core.HEAPU8.buffer), at = core._stage_world_flag_words() >> 2; return { U, at, n: U[at] }; };
  const flagRandom = () => { const { U, at, n } = flagWords(); if (flagWordCursor < n) return U[at + 1 + flagWordCursor++]; return (Math.random() * 0x100000000) >>> 0; };
  const nextWindMode = () => { const { U, at, n } = flagWords(); for (let k = flagWordCursor; k < n; k++) { const m = U[at + 1 + n + k]; if (m) return m; } return 0; };
  const group = new T.Group(); group.name = 'peak1-set-pieces'; parent.add(group);
  // pv cables (web/set-piece-cables.js, docs/visual-parity.md 41): the MultiSpline cables of the attached locations, drawn while the owner's
  // section start (0x101B60 slot 1 builds the modifier) has no leave after it; the proxy lets the loading warm-up build the pipeline.
  const cablesOn = pv('cables'), cableAlive = new Map(), alive = (owner) => params.get('cables') === 'all' || cableAlive.get(owner) === true; // ?cables=all: every cable (QA)
  if (cablesOn) group.add(cableProxy());
  const particlesDoc = JSON.parse(particleText);
  const particles = params.get('particles') === '0' ? null : await createSetPieceParticles({core, particlesDoc, origin}).catch((e) => { console.warn('Peak 1 particles unavailable', e); return null; });
  if (particles) group.add(particles.group);
  const halos = params.get('halos') !== '0' && core._stage_world_halos ? await createSetPieceHalos({core, origin}).catch((e) => { console.warn('Peak 1 halos unavailable', e); return null; }) : null;
  if (halos) group.add(halos.group);
  // UV scroll groups: first-appearance order of distinct initial states (tools/export_peak_world.py numbers the batches the same way).
  const scrollGroups = new Map(), representative = new Map();
  if (scrollData) for (const x of scrollData.instances) { const key = sortedJson(x.initial); if (!scrollGroups.has(key)) { scrollGroups.set(key, scrollGroups.size); representative.set(scrollGroups.size - 1, x.resource); } }
  const representativeSet = new Set(representative.values());

  // Location draw groups (web/free-ride.js attach / detach): their LiveComp, script, MeshAnim and flag meshes.
  const locations = new Map(); // code -> {group, liveMeshes, scriptMeshes: Map, pieceMeshes: Map, hidden}
  const liveMeshes = new Set(), scriptMeshes = new Map(), pieceMeshes = new Map(), hiddenByResource = new Map();
  const index = (map, key, value) => { if (!map.has(key)) map.set(key, []); map.get(key).push(value); };
  const unindex = (map, key, value) => { const list = map.get(key); if (!list) return; const k = list.indexOf(value); if (k >= 0) list.splice(k, 1); if (!list.length) map.delete(key); };
  let flags = null, scroll = null, live = null, ticks = -1, sectionCursor = 0, startCursor = 0, entityCursor = 0;
  const flagMeshes = new Map(); // resource -> mesh (cloth redraw)
  const scriptState = new Map(), magnetOffsets = new Map(), seenScratch = new Set(), activeScratch = new Set(); // per-frame sets, reused
  const applyVisibility = (mesh) => { mesh.visible = mesh.userData.scriptVisible ?? true; };
  const flagStatic = (resource, on) => { for (const mesh of hiddenByResource.get(resource) || []) { const g = mesh.userData.peakGroup; if (on && !mesh.parent) g.add(mesh); else if (!on && mesh.parent === g) mesh.removeFromParent(); } };
  function addFlagMesh(resource) {
    const slot = flags?.clothOf(resource); if (!slot || flagMeshes.has(resource)) return;
    const sources = hiddenByResource.get(resource) || [], source = sources.find((m) => FLAG_TEXTURES.includes(m.userData.batch?.texture)) ?? sources[0];
    if (!source) return; // the location's draw package is not built yet: addFlagMesh runs again when it attaches
    const n = slot.width * slot.height, g = new T.BufferGeometry();
    const position = new T.BufferAttribute(new Float32Array(n * 3), 3); position.setUsage(T.DynamicDrawUsage); g.setAttribute('position', position);
    g.setAttribute('uv', new T.BufferAttribute(Float32Array.from(slot.uv), 2));
    const colour = new Float32Array(n * 4); for (let i = 0; i < n; i++) { colour[i * 4] = slot.colour[i * 4 + 1]; colour[i * 4 + 1] = slot.colour[i * 4 + 2]; colour[i * 4 + 2] = slot.colour[i * 4 + 3]; colour[i * 4 + 3] = slot.colour[i * 4]; }
    g.setAttribute('color', new T.BufferAttribute(colour, 4)); g.setAttribute('ps2VertexAlpha', new T.BufferAttribute(new Float32Array(n).fill(1), 1));
    g.setIndex(new T.BufferAttribute(flagTriangleIndices(slot.width, slot.height), 1));
    const mesh = new T.Mesh(g, source.material); mesh.position.copy(source.position); mesh.frustumCulled = false; mesh.userData.flagResource = resource; mesh.userData.peakGroup = source.userData.peakGroup;
    source.userData.peakGroup.add(mesh); flagMeshes.set(resource, mesh); flagStatic(resource, false);
  }
  // A mesh removed for good also drops its render objects (bindings, uniform buffers, the material listener): dispose event.
  const dropMesh = (mesh) => { mesh.removeFromParent(); mesh.geometry.dispose(); mesh.dispatchEvent({type: 'dispose'}); };
  function removeFlagMesh(resource) { const mesh = flagMeshes.get(resource); if (!mesh) return; flagMeshes.delete(resource); dropMesh(mesh); flagStatic(resource, true); }
  function reset() {
    ticks = 0; sectionCursor = 0; startCursor = 0; entityCursor = 0; cableAlive.clear();
    // pv setPieceSkip: only each UV-scroll group's representative is read (the draw offset of its group, below); the other instances of
    // the peak (728 in Peak 1) ticked for nothing. Each instance's state is its own, so the representatives' values are the same.
    scroll = scrollData && params.get('uvscroll') !== '0' ? new UvScroll(pv('setPieceSkip') ? { ...scrollData, instances: scrollData.instances.filter((x) => representativeSet.has(x.resource)) } : scrollData) : null;
    live = liveData && params.get('livecomp') !== '0' ? new LiveCompAnimation(liveData) : null;
    if (live && coreStarts) live.chainStarts = false; // trigger / timer starts come from the core's stage VM
    for (const r of [...flagMeshes.keys()]) removeFlagMesh(r);
    flags = flagData && params.get('flags') !== '0' ? new FlagAnimation(flagData, coreFlags ? { random: flagRandom } : {}) : null; flagWordCursor = 0;
    for (const mesh of liveMeshes) restMesh(mesh);
  }
  // pv liveRest: the rest matrix composed once (userData.lcRest) instead of three recomposing it every frame; a player's write clears lcRest
  const liveRest = pv('liveRest');
  const restMesh = (mesh) => { mesh.position.copy(mesh.userData.restPosition ??= mesh.position.clone()); mesh.quaternion.identity(); mesh.scale.set(1, 1, 1);
    if (liveRest) { mesh.updateMatrix(); mesh.matrixAutoUpdate = false; mesh.matrixWorldNeedsUpdate = true; mesh.userData.lcRest = true; } else mesh.matrixAutoUpdate = true; };
  const playing = (mesh) => !mesh.matrixAutoUpdate && !mesh.userData.lcRest;   // a player's matrix (else the rest draw)
  const U32 = () => new Uint32Array(core.HEAPU8.buffer);
  const haloNodePosition = (resource, node, out) => { const m = live?.matrices?.(resource); if (!(m && m[node])) return null; const r = m[node][3]; out[0] = r[0]; out[1] = r[1]; out[2] = r[2]; return out; };

  const api = {
    group, particles, halos,
    // A location's draw package became available / was released (web/free-ride.js buildRender / release).
    attach(code, g) {
      if (locations.has(code)) return;
      const entry = {group: g, live: [], script: [], pieces: [], hidden: []};
      g.traverse((o) => { if (!o.isMesh) return; o.userData.peakGroup = o.parent; o.userData.peakLocationGroup = g;
        if (o.userData.liveComp) { liveMeshes.add(o); entry.live.push(o); }
        if (o.userData.scriptResource !== undefined) { index(scriptMeshes, o.userData.scriptResource, o); entry.script.push(o); const drawn = scriptState.get(o.userData.scriptResource); if (drawn !== undefined) { o.userData.scriptVisible = drawn; applyVisibility(o); } } });
      for (const mesh of g.userData.hiddenMeshes || []) { mesh.userData.peakGroup = g; const b = mesh.userData.batch;
        if (b?.meshanim_resource !== undefined) { const piece = {mesh, node: b.meshanim_node}; index(pieceMeshes, b.meshanim_resource, piece); entry.pieces.push([b.meshanim_resource, piece]); }
        else { index(hiddenByResource, mesh.userData.hiddenResource, mesh); entry.hidden.push(mesh); } }
      locations.set(code, entry);
      if (cablesOn && CABLES[code]) createLocationCables({code, root: streamedCableRoot(code), load: (u) => fetch(u).then((r) => { if (!r.ok) throw Error(u + ': ' + r.status); return r.json(); }), origin})
        .then((c) => { if (!c) return; if (locations.get(code) !== entry) { c.dispose(); return; } entry.cables = c; g.add(c.group); c.update(alive); }).catch((e) => console.warn('Cables unavailable', code, e));
      // pv fogPuffs (web/fog-puffs.js, docs/visual-parity.md 41): the location's kind-5 fog-particle puffs, with its draw package.
      if (pv('fogPuffs') && streamedFogRoot(code)) createFogPuffs({root: streamedFogRoot(code), origin})
        .then((f) => { if (!f) return; if (locations.get(code) !== entry) { f.dispose(); return; } entry.fogPuffs = f; g.add(f.group); }).catch((e) => console.warn('Fog puffs unavailable', code, e));
      // CrowdMan2d (builtin 88, web/crowd-2d.js): the event course's crowd texture (world texture 9-161) animates in the location's
      // own draw package; the flash areas are the course's (the core registers the slots as the sections build them).
      if (CROWD_COURSES.has(code) && params.get('crowd') !== '0' && core._stage_world_crowd)
        createCrowd2d({core, group: g, root: `/assets/${code}/`, origin}).then((c) => { if (locations.get(code) === entry) entry.crowd = c; }).catch(() => {});
      // A flag without its entity (section not entered) draws statically (0x22A5A0); the cloth redraw replaces it while active.
      if (flags) for (const mesh of entry.hidden) if (flags.byResource.has(mesh.userData.hiddenResource) && !mesh.parent) g.add(mesh);
      if (flags) for (const inst of flags.instances) if (inst.slot >= 0) addFlagMesh(inst.resource); // flags active before their package was built

      // pv staticWorld: the location's plain static batches frozen and grouped in frustum-culled cells (web/static-world.js; same draws)
      if (pv('staticWorld')) organizeStaticWorld(g);
    },
    detach(code) {
      const entry = locations.get(code); if (!entry) return; locations.delete(code);
      releaseStaticWorld(entry.group);
      entry.cables?.dispose();
      entry.fogPuffs?.dispose();
      for (const m of entry.live) liveMeshes.delete(m);
      for (const m of entry.script) unindex(scriptMeshes, m.userData.scriptResource, m);
      for (const [r, piece] of entry.pieces) unindex(pieceMeshes, r, piece);
      for (const m of entry.hidden) { const r = m.userData.hiddenResource; if (flagMeshes.get(r)?.userData.peakGroup === entry.group) { const f = flagMeshes.get(r); flagMeshes.delete(r); dropMesh(f); } unindex(hiddenByResource, r, m); }
    },
    get state() { return {ticks, locations: [...locations.keys()], flags: flagMeshes.size, liveComps: live ? live.active.size : 0, liveMeshes: liveMeshes.size, scroll: scroll ? scroll.instances.filter((x) => x.state?.active).length : 0, particles: particles?.state, halos: halos?.state, coreStarts}; },
    // Once per rendered frame, after the ticks of the frame (the core's set-piece tick counter drives every player).
    update() {
      const info = core._set_piece_info ? new Float32Array(core.HEAPF32.buffer, core._set_piece_info(), 2) : null;
      if (!info) return;
      const target = info[1];
      if (ticks < 0 || target < ticks) reset();
      const steps = Math.min(target - ticks, 3600);
      const startBase = startCursor, sectionBase = sectionCursor;
      const startLog = coreStarts ? (() => { const U = U32(), p = core._stage_world_livecomps() >> 2, n = U[p]; return U.slice(p + 1 + startBase, p + 1 + Math.max(startBase, 16 * n)); })() : null;
      let sectionLog = null;
      if (core._set_piece_sections) { const p = core._set_piece_sections(), n = new Uint32Array(core.HEAPU8.buffer, p, 1)[0]; sectionLog = new Uint32Array(core.HEAPU8.buffer, p + 8 + 4 * sectionBase, Math.max(0, 6 * n - sectionBase)).slice(); }
      for (let k = 0; k < steps; k++) {
        if (live) {
          live.tick();
          while (startLog && startCursor - startBase < startLog.length && startLog[startCursor - startBase] <= ticks) {
            const e = startLog.subarray(startCursor - startBase, startCursor - startBase + 16); startCursor += 16;
            const inst = live.byResource.get(e[1]); if (!inst) continue;
            const random = live.random, drawn = [e[13], e[14]]; let n = 0; live.random = () => drawn[n++] ?? 0;
            try { live.start(inst, Array.from(e.subarray(2, 13))); } finally { live.random = random; }
          }
        }
        // Section scan at the end of the tick (0x101B60): slot-1 starts, leaves.
        while (sectionLog && sectionCursor - sectionBase < sectionLog.length && sectionLog[sectionCursor - sectionBase] <= ticks) {
          const [, resource, action, , draws, word] = sectionLog.subarray(sectionCursor - sectionBase, sectionCursor - sectionBase + 6); sectionCursor += 6;
          if (cablesOn && CABLE_OWNERS.has(resource) && (action === 1 || action === 3)) cableAlive.set(resource, action === 1); // 5: a MultiSpline's leave only counts down +0x34 (0x35AAE0)
          if (action === 1) {
            if (live) { const random = live.random; if (draws) live.random = () => word; try { live.fire('section', resource); } finally { live.random = random; } }
            scroll?.activate(resource);
            if (flags && flags.activate(resource) >= 0) addFlagMesh(resource);
          } else if (action === 3 || action === 5) {
            live?.active.delete(resource);
            if (flags?.deactivate(resource)) removeFlagMesh(resource);
          }
        }
        if (flags && coreFlags) { const m = nextWindMode(); if (m) flags.mode = m; } // the mode of this tick's wind draw (0x2D1BA0)
        flags?.tick(ticks + 1); scroll?.tick(); ticks++;
      }
      ticks = target;
      if (live && core._stage_world_section_clocks && pv('sectionClock')) syncSectionClocks(core, live); // builtins 28 / 54 on the section players
      // Point pickups in flight (MagnetModifier): the pickup's batches follow the magnet (source cm).
      if (core._stage_world_magnets) {
        const F = new Float32Array(core.HEAPU8.buffer), q = core._stage_world_magnets() >> 2, n = F[q];
        if (n || magnetOffsets.size) { const now = new Map();
          for (let k = 0, at = q + 1; k < n; k++, at += 10) { const d = [F[at + 1] - F[at + 7], F[at + 2] - F[at + 8], F[at + 3] - F[at + 9]]; if (d[0] || d[1] || d[2]) now.set(F[at], d); }
          for (const r of new Set([...magnetOffsets.keys(), ...now.keys()])) {
            const d = now.get(r) ?? [0, 0, 0]; if (!now.has(r)) magnetOffsets.delete(r); else magnetOffsets.set(r, d);
            for (const m of scriptMeshes.get(r) || []) { const o = m.userData.magnetRest ??= m.position.clone(); m.position.set(o.x + d[0] / 100, o.y + d[2] / 100, o.z - d[1] / 100); m.updateMatrix(); m.matrixWorldNeedsUpdate = true; }
            const inst = live?.byResource?.get(r);
            if (inst) { const base = inst.magnetBase ??= inst.matrixRows[3].slice(); inst.matrixRows[3] = [base[0] + d[0], base[1] + d[1], base[2] + d[2], base[3]]; const st = live.state(r); if (st) st.dirty = true; }
          } }
      }
      // pv setPieceSkip: a hidden location's LiveComp meshes are not drawn; the pass runs again the frame its group shows (free-ride.js sets the
      // groups' visibility before this update), so the drawn matrices are the same.
      const skipHidden = pv('setPieceSkip');
      if (live) for (const mesh of liveMeshes) {
        if (skipHidden && mesh.userData.peakLocationGroup?.visible === false) continue;
        const [resource, node] = mesh.userData.liveComp, deltas = live.nodeDeltas(resource);
        if (!deltas || !deltas[node]) { if (playing(mesh)) restMesh(mesh); continue; } // a leave destroyed the player: static draw
        mesh.matrixAutoUpdate = false; mesh.userData.lcRest = false;
        const e = mesh.matrix.fromArray(deltas[node]).elements, o = mesh.userData.restPosition ??= mesh.position.clone();
        e[12] += o.x; e[13] += o.y; e[14] += o.z; mesh.matrixWorldNeedsUpdate = true;
      }
      attached?.apply(liveMeshes, live); // the children on their parents' node matrices (a child without a player keeps the rest draw)
      if (core._stage_world_instances) {
        const U = U32();
        // Instance draw states (DeadNode / RestoreNode / Hide / breaking pieces hide the static draw).
        const ip = core._stage_world_instances() >> 2, count = U[ip], seen = seenScratch; seen.clear();
        for (let k = 0; k < count; k++) { const r = U[ip + 1 + 4 * k], drawn = U[ip + 2 + 4 * k] !== 0; seen.add(r);
          if (scriptState.get(r) !== drawn) { scriptState.set(r, drawn); for (const m of scriptMeshes.get(r) || []) { m.userData.scriptVisible = drawn; applyVisibility(m); } } }
        for (const r of scriptState.keys()) if (!seen.has(r)) { const drawn = scriptState.get(r); scriptState.delete(r); if (!drawn) for (const m of scriptMeshes.get(r) || []) { m.userData.scriptVisible = true; applyVisibility(m); } }
        // Break pieces in flight: node deltas rest -> current on the per-node copies.
        const mp = core._stage_world_meshanims() >> 2, F = new Float32Array(core.HEAPU8.buffer), active = activeScratch; active.clear();
        for (let k = 0, at = mp + 1; k < F[mp]; k++) {
          const r = F[at], nodes = F[at + 1]; at += 2; active.add(r);
          for (const piece of pieceMeshes.get(r) || []) {
            if (piece.node >= nodes) continue; const mesh = piece.mesh;
            if (!mesh.parent) mesh.userData.peakGroup.add(mesh);
            mesh.matrixAutoUpdate = false; const o = mesh.userData.restPosition ??= mesh.position.clone(), e = mesh.matrix.fromArray(F.subarray(at + 16 * piece.node, at + 16 * piece.node + 16)).elements;
            e[12] += o.x; e[13] += o.y; e[14] += o.z; mesh.matrixWorldNeedsUpdate = true;
          }
          at += 16 * nodes;
        }
        for (const [r, list] of pieceMeshes) if (!active.has(r)) for (const piece of list) if (piece.mesh.parent) piece.mesh.removeFromParent();
        // A destroyed LiveComp (builtin2 / builtin16 conversion / MeshAnim on its instance) stops its JS player.
        const lp = core._stage_world_entity_log() >> 2, events = U[lp];
        if (entityCursor > events) entityCursor = 0;
        for (; entityCursor < events; entityCursor++) { const at = lp + 1 + 3 * entityCursor; if (U[at + 2] === 1 || U[at + 2] === 2) live?.active.delete(U[at + 1]); }
      }
      if (scroll) for (const [id, resource] of representative) { const offset = worldUvScroll.get(id); if (!offset) continue; const x = scroll.byResource?.get(resource); if (x) offset.value.set(x.state.u, x.state.v); }
      particles?.update();
      for (const entry of locations.values()) { entry.crowd?.update(ticks); entry.cables?.update(alive); }
      halos?.update(haloNodePosition);
      for (const [resource, mesh] of flagMeshes) { const attribute = mesh.geometry.getAttribute('position'); if (flags?.writeWorld(resource, attribute.array)) attribute.needsUpdate = true; }
    },
    dispose() { for (const code of [...locations.keys()]) api.detach(code); group.removeFromParent(); },
  };
  return api;
}
