// Ambient set-piece animation in the course scene, advanced by the core's game ticks:
// * waving flags / streamers (flag entity 0x48FC10, web/flag-animation.js): the flag instances'
//   static batches are hidden (prepare.py) and redrawn from the shared cloth grids with their
//   instance matrices, using the hidden batches' own world materials;
// * LiveComp animation players (builtin3, web/livecomp-animation.js): start-gate doors at GO,
//   searchlights/pinlights/boost pickups (section starts, all at race tick 0), and the contact-
//   triggered ravens, rock slide and snow crumbs; the trigger contacts come from the core's
//   selected-contact log (set_piece_contacts(): 105398 store step -> 121818, rider+A30);
// * UVScrollModifier texture scrolling (builtin21, web/uv-scroll.js): one texture translation per
//   scroll group (world-material.js worldUvScroll, prepare.py uv_scroll_group batches).
// The tick source is the core's set-piece tick counter (set_piece_info()[1]: race_begin calls since
// the race-tick-0 attach), so every 60 Hz game tick is applied exactly once, in order, and a new race
// (counter reset) restarts both systems. The shared visual random stream (0x4FF018) that drives the
// original wind is not reproduced (Math.random stand-in; docs/set-pieces.md).
import { loadAvalanches } from './avalanche-load.js';
import * as T from 'three/webgpu';
import {FlagAnimation, flagTriangleIndices} from './flag-animation.js';
import {UvScroll} from './uv-scroll.js';
import {worldUvScroll, litWorldMaterial} from './world-material.js';
import {LiveCompAnimation, liveCompConstruct, syncSectionClocks} from './livecomp-animation.js';
import {AttachedSetPieces} from './attached-setpieces.js';
import {createSetPieceParticles} from './set-piece-particles.js';
import {createSetPieceHalos} from './set-piece-halos.js';
import {createCrowd2d} from './crowd-2d.js';
import { initStageWorldSliced } from './load-slices.js';
import { pv } from './pv-flags.js';
import { createLocationCables, CableLife } from './set-piece-cables.js';
import { createFogPuffs } from './fog-puffs.js';
import { createAvalancheDraw } from './avalanche-state.js';

const params = new URL(globalThis.location?.href ?? 'http://x/').searchParams; // ?livecomp=0 / ?flags=0 / ?uvscroll=0 for comparisons
const sortedJson = (value) => JSON.stringify(value, Object.keys(value).sort());

export async function createSetPieceRenderer({core, group, load, course = null}) {
  // Snow Jam's exports live in /assets/{FLAGS,UVSCROLL,LIVECOMP}/; other locations under their root.
  const base = !course || course.code === 'ARA1' ? '/assets/' : course.root;
  const [flagData, scrollData, liveData, attachedData] = await Promise.all([load(base + 'FLAGS/flags.json').catch(() => null), load(base + 'UVSCROLL/uv-scroll.json').catch(() => null), load(base + 'LIVECOMP/livecomp.json').catch(() => null), load(base + 'LIVECOMP/attached.json').catch(() => null)]);
  // Attached set pieces: raven flap / blimp (core set_piece_attached), ParentModifier children (?attached=0 disables).
  const attached = attachedData && params.get('attached') !== '0' ? new AttachedSetPieces(attachedData, {core}) : null;
  // pv liveRest (as web/peak-set-pieces.js): a LiveComp mesh at rest keeps its composed matrix (userData.lcRest); a player's write clears it
  const liveRest = pv('liveRest');
  const restMesh = (mesh) => { mesh.position.copy(mesh.userData.restPosition ??= mesh.position.clone()); mesh.quaternion.identity(); mesh.scale.set(1, 1, 1);
    if (liveRest) { mesh.updateMatrix(); mesh.matrixAutoUpdate = false; mesh.matrixWorldNeedsUpdate = true; mesh.userData.lcRest = true; } else mesh.matrixAutoUpdate = true; };
  const playing = (mesh) => !mesh.matrixAutoUpdate && !mesh.userData.lcRest;   // a player's matrix (else the rest draw)
  // Section activation (web/section_gameplay.inc, tools/export_sections.py): the core scans for the human like
  // 0x101B60 and logs slot-1 starts / leaves; the pieces built before the race start from their ready state
  // (tools/export_section_ready_state.py) instead of restarting at race tick 0. ?sections=0 keeps the old model.
  const sectionRoot = course?.root ?? '/assets/ARA1/';
  const [sectionText, readyState] = params.get('sections') === '0' || !core._init_sections ? [null, null] :
    await Promise.all([fetch(sectionRoot + 'SECTIONS/sections.json').then((r) => (r.ok ? r.text() : null)).catch(() => null), load(sectionRoot + 'SECTIONS/ready-state.json').catch(() => null)]);
  let sectionsNative = false;
  if (sectionText && readyState) {
    const bytes = new TextEncoder().encode(sectionText + '\0'), ptr = core._malloc(bytes.length);
    core.HEAPU8.set(bytes, ptr); try { sectionsNative = core._init_sections(ptr) > 0; } finally { core._free(ptr); }
  }
  // Stage world (web/stage_world.inc): trigger/timer LiveComp players with their slot-4/5 programs, the particle
  // effects of every stage program (drawn by web/set-piece-particles.js) and the sound requests. ?particles=0 disables.
  let particles = null, halos = null, coreStarts = false, stageData = null;
  if (core._init_stage_world && params.get('particles') !== '0') {
    const liveCompUrl = (!course || course.code === 'ARA1' ? '/assets/' : course.root) + 'LIVECOMP/livecomp.json';
    const [particleText, liveText, stageText] = await Promise.all([fetch(sectionRoot + 'PARTICLES/particles.json').then((r) => (r.ok ? r.text() : null)).catch(() => null),
      fetch(liveCompUrl).then((r) => (r.ok ? r.text() : '')).catch(() => ''), fetch(sectionRoot + 'STAGE/stage-world.json').then((r) => (r.ok ? r.text() : '')).catch(() => '')]);
    if (particleText) {
      const texts = [particleText, liveText, stageText].map((t) => new TextEncoder().encode(t + '\0')), ptrs = texts.map((t) => core._malloc(t.length));
      texts.forEach((t, k) => core.HEAPU8.set(t, ptrs[k]));
      // pv sliceLoad: the same load in small parts cut off the main thread (web/load-slices.js; the one call took 0.1-0.4 s at 4x CPU)
      if (pv('sliceLoad') && core._stage_world_part) { ptrs.forEach((p) => core._free(p)); coreStarts = (await initStageWorldSliced(core, { particles: sectionRoot + 'PARTICLES/particles.json', livecomp: liveText ? liveCompUrl : null, stage: stageText ? sectionRoot + 'STAGE/stage-world.json' : null })) > 0 && !!liveText; }
      else try { coreStarts = core._init_stage_world(...ptrs) > 0 && !!liveText; } finally { ptrs.forEach((p) => core._free(p)); }
      stageData = stageText ? JSON.parse(stageText) : null;
      const start = await load(sectionRoot + 'start.json').catch(() => null); // main.js origin = start.json position
      particles = await createSetPieceParticles({core, particlesDoc: JSON.parse(particleText), origin: start?.position ?? [0, 0, 0]});
      group.add(particles.group);
      // Pickup glow sprites (HaloModifier, web/set-piece-halos.js); ?halos=0 disables.
      if (params.get('halos') !== '0' && core._stage_world_halos) { halos = await createSetPieceHalos({core, origin: start?.position ?? [0, 0, 0]}).catch((e) => { console.warn('Halo sprites unavailable', e); return null; }); if (halos) group.add(halos.group); }
    }
  }
  // Avalanches (web/avalanche_gameplay.inc): the location's recorded tumbler paths for the core's builtin-94 playback.
  await loadAvalanches(core, [{ code: course?.code ?? 'ARA1', root: sectionRoot }]);
  // CrowdMan2d (web/crowd-2d.js): the animated crowd texture and the camera flashes; ?crowd=0 disables.
  const crowd = params.get('crowd') !== '0' && core._stage_world_crowd ? await createCrowd2d({core, group, root: sectionRoot, origin: (await load(sectionRoot + 'start.json').catch(() => null))?.position ?? [0, 0, 0]}).catch((e) => { console.warn('Crowd unavailable', e); return null; }) : null;
  // pv cables (web/set-piece-cables.js, docs/visual-parity.md 41): the MultiSpline cables of this location (0x35B418 -> 0x345430),
  // drawn while the owner's modifier is active (core set_piece_multi_bits) and its instance's texture chunk is resident (0x356298).
  const cables = pv('cables') ? await createLocationCables({code: course?.code ?? 'ARA1', root: sectionRoot, load, origin: (await load(sectionRoot + 'start.json').catch(() => null))?.position ?? [0, 0, 0]}).catch((e) => { console.warn('Cables unavailable', e); return null; }) : null;
  const cableActive = new Map(), cableLife = cables ? new CableLife(course?.code ?? 'ARA1', sectionsNative ? JSON.parse(sectionText) : null) : null;
  if (cables) group.add(cables.group);
  // pv fogPuffs (web/fog-puffs.js, docs/visual-parity.md 41): the kind-5 fog-particle puffs (0x22A270 / 0x2DC190 / 0x2DBF98), each
  // instance while its record's texture chunk is resident (chunkResident below; an unknown chunk counts as resident).
  const fogPuffs = pv('fogPuffs') ? await createFogPuffs({root: sectionRoot, origin: (await load(sectionRoot + 'start.json').catch(() => null))?.position ?? [0, 0, 0], resident: (chunk) => chunkResident.get(chunk)}).catch((e) => { console.warn('Fog puffs unavailable', e); return null; }) : null;
  if (fogPuffs) group.add(fogPuffs.group);
  // pv avalanche (web/avalanche-state.js, docs/avalanche.md): visibility of the AvaSpline pieces (moved by web/moving-instances.js).
  const avalanche = pv('avalanche') && core._avalanche_pieces ? await load(sectionRoot + 'avalanches.json').then((d) => {
    const followers = new Set(d.avalanches.flatMap((a) => a.groups.filter((g) => g.ava_spline).map((g) => g.resource)));
    return followers.size ? createAvalancheDraw({core, group, followers}) : null; }).catch(() => null) : null;
  // Flag manager bookkeeping in the core (web/stage_world.inc): grid builds and the wind draw from the shared visual
  // stream in the original order; the JS cloth consumes the same words (stage_world_flag_words).
  const coreFlags = !!(flagData && sectionsNative && readyState?.flags && core._init_stage_flags) && (() => {
    const texts = [JSON.stringify(flagData), JSON.stringify(readyState)].map((t) => new TextEncoder().encode(t + '\0')), ptrs = texts.map((t) => core._malloc(t.length));
    texts.forEach((t, k) => core.HEAPU8.set(t, ptrs[k])); try { return core._init_stage_flags(...ptrs) > 0; } finally { ptrs.forEach((p) => core._free(p)); }
  })();
  let flagWordCursor = 0;
  const flagRandom = () => { const U = new Uint32Array(core.HEAPU8.buffer), at = core._stage_world_flag_words() >> 2; if (flagWordCursor < U[at]) return U[at + 1 + flagWordCursor++]; return (Math.random() * 0x100000000) >>> 0; };
  const liveMeshes = group.userData.liveCompMeshes || [];
  // pv litLiveComp (web/world-material.js litWorldMaterial, docs/visual-parity.md 40): a lit LiveComp instance (livecomp.json
  // `lighting`: authored flag 0x40000000, runtime 0x4000) is lit per vertex from the object bank on its node-rotated normals, as
  // 37E238 -> VU1 program 3 draws it (Gravitude's crash billboards: the poster near black as it tips away, the panel pale as it faces up).
  if (pv('litLiveComp') && liveData?.instances) { const lit = new Map(liveData.instances.filter((x) => x.lighting).map((x) => [x.resource, x.lighting]));
    for (const mesh of liveMeshes) { const l = lit.get(mesh.userData.liveComp[0]); if (l) mesh.material = litWorldMaterial(mesh.material, l); } }
  // Stage-program instance state (web/stage_world.inc): per-instance batches of instances the programs hide / restore
  // (DeadNode, RestoreNode, Hide, breaking) and the per-node copies of the MeshAnim break pieces (prepare.py).
  const scriptMeshes = new Map(), pieceMeshes = new Map();
  group.traverse((o) => { if (o.isMesh && o.userData.scriptResource !== undefined) { if (!scriptMeshes.has(o.userData.scriptResource)) scriptMeshes.set(o.userData.scriptResource, []); scriptMeshes.get(o.userData.scriptResource).push(o); } });
  for (const mesh of group.userData.hiddenMeshes || []) { const b = mesh.userData.batch; if (b?.meshanim_resource === undefined) continue;
    if (!pieceMeshes.has(b.meshanim_resource)) pieceMeshes.set(b.meshanim_resource, []); pieceMeshes.get(b.meshanim_resource).push({ mesh, node: b.meshanim_node }); }
  const scriptState = new Map(); let entityCursor = 0;
  // Magnet flight (see the frame update): authored instance translations (source cm) of the magnet pickups.
  const magnetOffsets = new Map();
  const applyVisibility = (mesh) => { mesh.visible = (mesh.userData.chunkVisible ?? true) && (mesh.userData.scriptVisible ?? true); };
  const chunkMeshes = params.get('chunks') === '0' ? [] : (group.userData.chunkMeshes || []).filter((m) => !m.userData.liveComp && m.userData.hiddenResource === undefined && m.userData.pickupResource === undefined && m.userData.eventDeadResource === undefined && m.userData.movingResource === undefined), chunkResident = new Map(); // ?chunks=0 draws every chunk
  const contactOwners = new Set(liveData ? liveData.instances.flatMap((x) => x.starts || []).filter((st) => st.trigger === 'contact').map((st) => st.ownerResource) : []);
  const hidden = group.userData.hiddenMeshes || [];
  // UV scroll groups: first-appearance order of distinct initial states (prepare.py assigns the same ids).
  const scrollGroups = new Map(), groupOf = new Map();
  if (scrollData) for (const x of scrollData.instances) { const key = sortedJson(x.initial); if (!scrollGroups.has(key)) scrollGroups.set(key, scrollGroups.size); groupOf.set(x.resource, scrollGroups.get(key)); }
  const representative = new Map(); for (const [resource, id] of groupOf) if (!representative.has(id)) representative.set(id, resource);
  const flagMeshes = [];
  let flags = null, scroll = null, live = null, ticks = 0, contactCursor = 0, sectionCursor = 0, startCursor = 0;
  const readyFloat = (entry, key) => (entry.bits && typeof entry.bits[key] === 'number' ? new Float32Array(new Uint32Array([entry.bits[key]]).buffer)[0] : entry.state[key]);
  const fired = new Set(); // a slot-2 trigger runs once (its flags leave the collectors after firing)
  // A flag without its entity (section not active yet, or left) draws statically (0x22A5A0); the cloth
  // redraw replaces it while the flag manager holds it.
  const flagStatic = (resource, on) => { for (const mesh of hidden) if (mesh.userData.hiddenResource === resource) { if (on && !mesh.parent) group.add(mesh); else if (!on && mesh.parent === group) mesh.removeFromParent(); } };
  function addFlagMesh(resource) {
    {
      const slot = flags.clothOf(resource); if (!slot || flagMeshes.some((m) => m.resource === resource)) return;
      const source = hidden.find((mesh) => mesh.userData.hiddenResource === resource && [138, 268].includes(mesh.userData.batch?.texture));
      if (!source) return;
      const n = slot.width * slot.height, g = new T.BufferGeometry();
      const position = new T.BufferAttribute(new Float32Array(n * 3), 3); position.setUsage(T.DynamicDrawUsage);
      g.setAttribute('position', position);
      g.setAttribute('uv', new T.BufferAttribute(Float32Array.from(slot.uv), 2));
      const colour = new Float32Array(n * 4); for (let i = 0; i < n; i++) { colour[i * 4] = slot.colour[i * 4 + 1]; colour[i * 4 + 1] = slot.colour[i * 4 + 2]; colour[i * 4 + 2] = slot.colour[i * 4 + 3]; colour[i * 4 + 3] = slot.colour[i * 4]; }
      g.setAttribute('color', new T.BufferAttribute(colour, 4));
      g.setAttribute('ps2VertexAlpha', new T.BufferAttribute(new Float32Array(n).fill(1), 1));
      g.setIndex(new T.BufferAttribute(flagTriangleIndices(slot.width, slot.height), 1));
      const mesh = new T.Mesh(g, source.material); mesh.position.copy(source.position); mesh.frustumCulled = false; mesh.userData.flagResource = resource;
      group.add(mesh); flagMeshes.push({mesh, resource});
      flagStatic(resource, false);
    }
  }
  function removeFlagMesh(resource) {
    const k = flagMeshes.findIndex((m) => m.resource === resource); if (k < 0) return;
    const [{mesh}] = flagMeshes.splice(k, 1); mesh.removeFromParent(); mesh.geometry.dispose(); mesh.dispatchEvent({type: 'dispose'}); // release its render objects (terrain-overlays.js)
    flagStatic(resource, true);
  }
  function reset() {
    ticks = 0; contactCursor = 0; sectionCursor = 0; startCursor = 0; fired.clear(); cableLife?.reset(); avalanche?.reset();
    scroll = scrollData && params.get('uvscroll') !== '0' ? new UvScroll(scrollData) : null;
    live = liveData && params.get('livecomp') !== '0' ? new LiveCompAnimation(liveData) : null;
    if (live && coreStarts) live.chainStarts = false; // trigger / timer starts come from the core's stage VM
    if (sectionsNative) {
      // Race tick 0 = the ready savestate: pieces built before the race keep their phase.
      if (live) for (const entry of readyState.livecomp || []) {
        const inst = live.byResource.get(entry.resource), st = inst?.starts.find((x) => x.trigger === 'section'); if (!inst || !st) continue;
        const s = liveCompConstruct(inst, st.words, () => 0, live.fps);
        for (const key of Object.keys(entry.state)) s[key] = readyFloat(entry, key);
        s.dirty = true; s.advanced = false; live.active.set(inst.resource, s);
      }
      if (scroll) for (const entry of readyState.uvscroll || []) {
        const x = scroll.byResource.get(entry.resource); if (!x) continue;
        x.state = {...entry.state}; for (const key of Object.keys(entry.state)) if (typeof entry.state[key] === 'number') x.state[key] = readyFloat(entry, key);
      }
    }
    for (const mesh of liveMeshes) restMesh(mesh);
    if (!flagData || params.get('flags') === '0') return;
    flags = new FlagAnimation(flagData, coreFlags ? {random: flagRandom} : {}); flagWordCursor = 0;
    if (sectionsNative && readyState.flags) {
      const w = readyState.flags.wind; flags.wind = {wind: w.wind, base: w.base, delta: w.delta, timer: w.timer};
      if (readyState.flags.wind_bits) for (const key of Object.keys(flags.wind)) flags.wind[key] = new Float32Array(new Uint32Array([readyState.flags.wind_bits[key]]).buffer)[0];
      const random = flags.random; flags.random = () => (Math.random() * 0x100000000) >>> 0; // ready-state grids: their phases come from the savestate (no draws)
      for (const slot of readyState.flags.slots) {
        const [first, ...rest] = slot.instances, k = flags.activate(first); if (k < 0) continue;
        const cloth = flags.slots[k]; if (k !== slot.slot) throw Error('Flag slot order differs from the ready state');
        cloth.phase = Float32Array.from(slot.phase); cloth.uvOffset = slot.uvOffset.slice(); cloth.parity = slot.parity;
        for (const r of rest) flags.activate(r);
      }
      flags.random = random;
    } else flags.activateAll();
    for (const m of flagMeshes) { m.mesh.removeFromParent(); m.mesh.geometry.dispose(); m.mesh.dispatchEvent({type: 'dispose'}); }
    if (sectionsNative) for (const inst of flagData.instances) flagStatic(inst.resource, true);
    flagMeshes.length = 0;
    for (const inst of flags.instances) addFlagMesh(inst.resource);
  }
  // P (source cm) of a JS-animated LiveComp node for a halo: row 3 of the node matrix, into the caller's scratch array.
  const teeterScratch = new Map(), seenScratch = new Set(), activeScratch = new Set(), changedFlagSlots = new Set(); // per-frame sets, reused
  const haloNodePosition = (resource, node, out) => { const m = live?.matrices?.(resource); if (!(m && m[node])) return null; const r = m[node][3]; out[0] = r[0]; out[1] = r[1]; out[2] = r[2]; return out; };
  reset();
  return {
    // Core spline pieces [resource, phase, launch tick, distance, x, y, z] (QA).
    splines() { if (!core._set_piece_splines) return []; const q = core._set_piece_splines() >> 2, n = core.HEAPF32[q]; return Array.from({length: n}, (_, k) => Array.from(core.HEAPF32.subarray(q + 2 + 7 * k, q + 9 + 7 * k))); },
    debugMeshes(resource) { return liveMeshes.filter((m) => m.userData.liveComp[0] === resource).map((m) => ({auto: m.matrixAutoUpdate, matrix: Array.from(m.matrix.elements), world: Array.from(m.matrixWorld.elements), visible: m.visible, parent: !!m.parent})); },
    debugLive(resource) { return live ? {state: live.state(resource), deltas: live.nodeDeltas(resource)} : null; },
    get state() { return {ticks, flags: flagMeshes.length, scrollGroups: scrollGroups.size, liveComps: live ? live.active.size : 0, liveMeshes: liveMeshes.length, fired: [...fired], particles: particles?.state, halos: halos?.state, fogPuffs: fogPuffs?.state, avalanche: avalanche?.state, crowd: crowd?.state, coreStarts, cables: cables ? cables.meshes.map((m) => [m.userData.cableOwner, m.visible]) : null}; },
    particles,
    // Loading-screen warm-up (main.js warmupRender): one degenerate cloth-layout mesh per flag material, so the flag
    // meshes a section activation adds (addFlagMesh) find their node build and pipeline ready.
    warmProxies() {
      const out = [], seen = new Set();
      for (const source of hidden) {
        if (![138, 268].includes(source.userData.batch?.texture) || seen.has(source.material)) continue; seen.add(source.material);
        const g = new T.BufferGeometry();
        g.setAttribute('position', new T.BufferAttribute(new Float32Array(12), 3)); g.setAttribute('uv', new T.BufferAttribute(new Float32Array(8), 2));
        g.setAttribute('color', new T.BufferAttribute(new Float32Array(16), 4)); g.setAttribute('ps2VertexAlpha', new T.BufferAttribute(new Float32Array(4).fill(1), 1));
        g.setIndex(new T.BufferAttribute(flagTriangleIndices(2, 2), 1));
        const mesh = new T.Mesh(g, source.material); mesh.frustumCulled = false; out.push(mesh);
      }
      return out;
    },
    // Trigger contacts of other riders (computer riders run in their own core instances).
    fireContact(resource) { if (live && !coreStarts && contactOwners.has(resource) && !fired.has(resource)) { fired.add(resource); live.fire('contact', resource); } },
    update() {
      const info = core._set_piece_info ? new Float32Array(core.HEAPF32.buffer, core._set_piece_info(), 2) : null;
      if (!info) return;
      const target = info[1];
      if (target < ticks) reset();
      const steps = Math.min(target - ticks, 3600);
      changedFlagSlots.clear();
      // Fired slot-2 triggers of any rider (core set_piece_triggers: tick, owner*8+slot; other riders' cores
      // replay their triggers into this one, web/ai-racers.js), else the raw contact log (older cores).
      // The core logs only grow during a run: copy just the unread tail (from the cursor), not the whole log every frame
      // (a whole-log copy per frame grew to megabytes a second of garbage by the end of a run).
      const contactBase = contactCursor, sectionBase = sectionCursor, startBase = startCursor;
      const readLog = (fn, decode) => { const p = fn.call(core), n = new Uint32Array(core.HEAPU8.buffer, p, 1)[0], v = new Uint32Array(core.HEAPU8.buffer, p + 4 + 4 * contactBase, Math.max(0, 2 * n - contactBase)).slice(); if (decode) for (let i = 1; i < v.length; i += 2) v[i] = v[i] >>> 3; return v; };
      const log = core._set_piece_triggers ? readLog(core._set_piece_triggers, true) : core._set_piece_contacts ? readLog(core._set_piece_contacts, false) : new Uint32Array(0);
      // LiveComp starts of the core's trigger and timer programs: [tick, resource, 11 words, key8 word, key6 word, draws].
      const startLog = coreStarts ? (() => { const U = new Uint32Array(core.HEAPU8.buffer), p = core._stage_world_livecomps() >> 2, n = U[p]; return U.slice(p + 1 + startBase, p + 1 + Math.max(startBase, 16 * n)); })() : null;
      let sectionLog = null;
      if (sectionsNative) { const p = core._set_piece_sections(), n = new Uint32Array(core.HEAPU8.buffer, p, 1)[0]; sectionLog = new Uint32Array(core.HEAPU8.buffer, p + 8 + 4 * sectionBase, Math.max(0, 6 * n - sectionBase)).slice(); }
      for (let k = 0; k < steps; k++) {
        if (live) {
          if (ticks === 0 && !sectionsNative) for (const inst of live.instances) if ((inst.starts || []).some((st) => st.trigger === 'section')) live.fire('section', inst.resource);
          if (ticks === liveData.go_tick) live.fire('go');
          live.tick();
          if (startLog) {
            // starts of this tick (contact programs in the rider phase, timer programs in the entity pass): first advance next tick
            while (startCursor - startBase < startLog.length && startLog[startCursor - startBase] <= ticks) {
              const e = startLog.subarray(startCursor - startBase, startCursor - startBase + 16); startCursor += 16;
              const inst = live.byResource.get(e[1]); if (!inst) continue;
              const random = live.random, drawn = [e[13], e[14]]; let k = 0; live.random = () => drawn[k++] ?? 0;
              try { live.start(inst, Array.from(e.subarray(2, 13))); } finally { live.random = random; }
            }
          } else
          // contacts of this tick (rider phase, after the entity pass): slot-2 programs of trigger volumes
          while (contactCursor - contactBase < log.length && log[contactCursor - contactBase] <= ticks) {
            const resource = log[contactCursor - contactBase + 1]; contactCursor += 2;
            if (contactOwners.has(resource) && !fired.has(resource)) { fired.add(resource); live.fire('contact', resource); }
          }
        }
        // Section scan at the end of this tick (0x101B60 after every rider): slot-1 starts, leaves.
        while (sectionLog && sectionCursor - sectionBase < sectionLog.length && sectionLog[sectionCursor - sectionBase] <= ticks) {
          const [, resource, action, , draws, word] = sectionLog.subarray(sectionCursor - sectionBase, sectionCursor - sectionBase + 6); sectionCursor += 6;
          cableLife?.section(resource, action);
          if (action === 1) {
            if (live) { const random = live.random; if (draws) live.random = () => word; try { live.fire('section', resource); } finally { live.random = random; } }
            scroll?.activate(resource);
            if (flags && flags.activate(resource) >= 0) addFlagMesh(resource);
          } else if (action === 3 || action === 5) {
            live?.active.delete(resource);
            if (flags?.deactivate(resource)) removeFlagMesh(resource);
          }
        }
        if (flags) for (const slot of flags.tick(ticks + 1)) changedFlagSlots.add(slot);
        scroll?.tick(); ticks++; // 0x34B818 recomputes a grid when (tick + 1) % 2 == its parity (20/20 PS2 snapshots)
      }
      ticks = target;
      if (live && core._stage_world_section_clocks && pv('sectionClock')) syncSectionClocks(core, live); // builtins 28 / 54 on the section players
      // Point pickups in flight (MagnetModifier, core stage_world_magnets): the pickup's batches follow the magnet's
      // translation (source cm) away from the authored instance translation; a LiveComp pickup (collecta) animates on
      // the magnet matrix (its player's root row 3 moves, the node deltas carry the offset).
      if (core._stage_world_magnets) {
        const F = new Float32Array(core.HEAPU8.buffer), q = core._stage_world_magnets() >> 2, n = F[q];
        if (n || magnetOffsets.size) { const now = new Map(); // nothing to do (and nothing allocated) with no pickup in flight
        for (let k = 0, at = q + 1; k < n; k++, at += 10) { const d = [F[at + 1] - F[at + 7], F[at + 2] - F[at + 8], F[at + 3] - F[at + 9]]; if (d[0] || d[1] || d[2]) now.set(F[at], d); }
        for (const r of new Set([...magnetOffsets.keys(), ...now.keys()])) {
          const d = now.get(r) ?? [0, 0, 0]; if (!now.has(r)) magnetOffsets.delete(r); else magnetOffsets.set(r, d);
          for (const m of scriptMeshes.get(r) || []) { const o = m.userData.magnetRest ??= m.position.clone(); m.position.set(o.x + d[0] / 100, o.y + d[2] / 100, o.z - d[1] / 100); m.updateMatrix(); m.matrixWorldNeedsUpdate = true; }
          const inst = live?.byResource?.get(r);
          if (inst) { const base = inst.magnetBase ??= inst.matrixRows[3].slice(); inst.matrixRows[3] = [base[0] + d[0], base[1] + d[1], base[2] + d[2], base[3]]; const st = live.state(r); if (st) st.dirty = true; }
        }
        }
      }
      // Log teeters (core set_piece_teeters: [count, ticks, forces, max time, then resource + 16 per teeter]).
      const teeter = teeterScratch; teeter.clear();
      if (core._set_piece_teeters) { const q = core._set_piece_teeters() >> 2, n = core.HEAPF32[q]; for (let k = 0; k < n; k++) teeter.set(core.HEAPF32[q + 4 + 17 * k], core.HEAPF32.slice(q + 5 + 17 * k, q + 21 + 17 * k)); }
      for (const mesh of liveMeshes) {
        const lc = mesh.userData.liveComp, resource = lc[0], node = lc[1], delta = teeter.get(resource);
        if (!delta || node !== 1) continue;
        mesh.matrixAutoUpdate = false; mesh.userData.lcRest = false;
        const e = mesh.matrix.fromArray(delta).elements, o = mesh.userData.restPosition ??= mesh.position.clone();
        e[12] += o.x; e[13] += o.y; e[14] += o.z; mesh.matrixWorldNeedsUpdate = true;
      }
      if (live) for (const mesh of liveMeshes) {
        const lc = mesh.userData.liveComp, resource = lc[0], node = lc[1], deltas = live.nodeDeltas(resource);
        // pv liveCompObject: an owner the static collector skips but its LiveComp Object player (vtable 0x490B10, flags & 4) draws through
        // 0x356298 while the player lives (livecomp.json draw 'object', tools/export_livecomp.py): The Throne's summit flag pole.
        if (mesh.userData.hiddenResource !== undefined && live.byResource.get(resource)?.draw === 'object' && pv('liveCompObject')) {
          const on = !!deltas?.[node]; if (on && !mesh.parent) group.add(mesh); else if (!on && mesh.parent === group) mesh.removeFromParent(); }
        if (!deltas || !deltas[node]) {
          // a section leave destroyed the player (0x34FD90): back to the static draw
          if (sectionsNative && playing(mesh) && !teeter.has(resource)) restMesh(mesh);
          continue;
        }
        mesh.matrixAutoUpdate = false; mesh.userData.lcRest = false;
        const e = mesh.matrix.fromArray(deltas[node]).elements, o = mesh.userData.restPosition ??= mesh.position.clone();
        e[12] += o.x; e[13] += o.y; e[14] += o.z; mesh.matrixWorldNeedsUpdate = true;
      }
      attached?.apply(liveMeshes, live);
      if (core._stage_world_instances) { // stage setup (collectibles, 30C4A8) runs without the particle data too
        const U = new Uint32Array(core.HEAPU8.buffer);
        // Instance draw states (DeadNode / RestoreNode / Hide / breaking pieces hide the static draw).
        const ip = core._stage_world_instances() >> 2, count = U[ip], seen = seenScratch; seen.clear();
        for (let k = 0; k < count; k++) { const r = U[ip + 1 + 4 * k], drawn = U[ip + 2 + 4 * k] !== 0; seen.add(r);
          if (scriptState.get(r) !== drawn) { scriptState.set(r, drawn); for (const m of scriptMeshes.get(r) || []) { m.userData.scriptVisible = drawn; applyVisibility(m); } } }
        for (const r of scriptState.keys()) if (!seen.has(r)) { const drawn = scriptState.get(r); scriptState.delete(r); if (!drawn) for (const m of scriptMeshes.get(r) || []) { m.userData.scriptVisible = true; applyVisibility(m); } } // new race
        // Break pieces in flight: node deltas rest -> current on the per-node copies.
        const mp = core._stage_world_meshanims() >> 2, F = new Float32Array(core.HEAPU8.buffer), active = activeScratch; active.clear();
        for (let k = 0, at = mp + 1; k < F[mp]; k++) {
          const r = F[at], nodes = F[at + 1]; at += 2; active.add(r);
          for (const piece of pieceMeshes.get(r) || []) {
            if (piece.node >= nodes) continue; const mesh = piece.mesh;
            if (!mesh.parent) group.add(mesh);
            mesh.matrixAutoUpdate = false; const o = mesh.userData.restPosition ??= mesh.position.clone(), e = mesh.matrix.fromArray(F.subarray(at + 16 * piece.node, at + 16 * piece.node + 16)).elements;
            e[12] += o.x; e[13] += o.y; e[14] += o.z; mesh.matrixWorldNeedsUpdate = true;
          }
          at += 16 * nodes;
        }
        for (const r of pieceMeshes.keys()) if (!active.has(r)) for (const piece of pieceMeshes.get(r)) if (piece.mesh.parent) piece.mesh.removeFromParent();
        // Entity events: a destroyed LiveComp (builtin2 / builtin16 conversion / MeshAnim on its instance) stops its JS player.
        const lp = core._stage_world_entity_log() >> 2, events = U[lp];
        if (entityCursor > events) entityCursor = 0;
        for (; entityCursor < events; entityCursor++) { const at = lp + 1 + 3 * entityCursor; if (U[at + 2] === 1 || U[at + 2] === 2) live?.active.delete(U[at + 1]); }
      }
      // Texture chunks: static instances of chunks the original has not streamed in are not drawn (0x22A5A0).
      if (sectionsNative && core._section_chunks && chunkMeshes.length) {
        const p = core._section_chunks(), n = new Uint32Array(core.HEAPU8.buffer, p, 1)[0], v = new Uint32Array(core.HEAPU8.buffer, p + 4, 2 * n);
        for (let k = 0; k < n; k++) chunkResident.set(v[2 * k], v[2 * k + 1] === 3);
        for (const mesh of chunkMeshes) { const on = chunkResident.get(mesh.userData.chunk) ?? true; if (mesh.userData.chunkVisible !== on) { mesh.userData.chunkVisible = on; applyVisibility(mesh); } }
      }
      if (scroll) for (const id of representative.keys()) { const offset = worldUvScroll.get(id); if (offset) { const x = scroll.byResource?.get(representative.get(id)); if (x) offset.value.set(x.state.u, x.state.v); else { const uv = scroll.offsetOf(representative.get(id)); offset.value.set(uv[0], uv[1]); } } }
      if (cables) { // [count, then per modifier: resource, active, cars, distance, 16 words per car]
        cableActive.clear();
        if (core._set_piece_multi_bits) { const U = new Uint32Array(core.HEAPU8.buffer), p = core._set_piece_multi_bits() >> 2; for (let k = 0, at = p + 1; k < U[p]; k++) { cableActive.set(U[at], U[at + 1] !== 0); at += 4 + 16 * U[at + 2]; } }
        cables.update((owner) => params.get('cables') === 'all' || cableLife.lives(owner, (o) => cableActive.get(o), (chunk) => chunkResident.get(chunk))); // ?cables=all: every cable (QA)
      }
      avalanche?.update();
      particles?.update();
      crowd?.update(ticks);
      halos?.update(haloNodePosition); // the player's root already carries a magnet's offset
      if (flags) for (const entry of flagMeshes) {
        const {mesh, resource} = entry;
        // Cloth advances at 30 Hz; rendering can run faster or repeat a paused
        // tick. A newly attached mesh still needs its initial world positions.
        if (entry.uploaded && !changedFlagSlots.has(flags.byResource.get(resource)?.slot)) continue;
        const attribute = mesh.geometry.getAttribute('position');
        if (flags.writeWorld(resource, attribute.array)) { attribute.needsUpdate = true; entry.uploaded = true; }
      }
    },
  };
}
