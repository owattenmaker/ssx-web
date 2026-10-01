// Presentation leftovers vs PS2 frames (docs/presentation.md), without a browser: the Select Peak help wrap, the keyboard
// dimming, world static-model clamping, the new-career plane's stage calls, the trick-boost rider light, the computer
// riders' boost FX meshes, the black sky clear and which world changes play the transport ride. Each item is checked with
// its web/pv-flags.js switch on (and, where it changes the shipped path, off).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { LuiScreen } from './lui-player.js';
import { helpOverride } from './fe-event-select.js';
import { sourceOf } from './test-source.mjs';

const ui = new URL('./public/assets/UI/', import.meta.url).pathname;
const read = (p) => JSON.parse(fs.readFileSync(ui + p, 'utf8'));

// ---- Select Peak / Mode / Event help line (local/ps2-capture/menus/single 04..07, fe-texture 57-select-peak) ----
{
  const menus = read('fe-menus.json'), glyphs = read('FEFONT-glyphs.json');
  const lines = [], host = { fonts: { FEFONT: glyphs }, text: (c, ln) => lines.push(ln) };
  const canvas = { save() {}, restore() {}, translate() {}, scale() {}, globalAlpha: 1 };
  const map = new LuiScreen(menus.screens.Map, {}, host), help = menus.screens.Map.elements.find((e) => e.label === 'HelpText');
  assert.ok(help, 'Map HelpText element');
  const wrap = (text) => { lines.length = 0; const o = helpOverride(text); map.text(canvas, o.text, 0, 0, { ...help.props, ...(o.props || {}) }); return [...lines]; };
  // the PS2's line breaks, read off its frames
  const ps2 = [
    ['Choose this peak and continue to select mode.'],
    ['LOCKED:  Get a Peak 3 pass in Conquer the', 'Mountain mode.'],
    ['Choose a mode and continue to select event.'],
    ['Snow Jam is an exciting BEGINNER track with', 'heavy forests and patches of fog.'],
    ['R&B is a world class BEGINNER course filled with', 'rails, pipes, paths, and hits.'],
    ['A battle against your rival.'],
  ];
  for (const want of ps2) assert.deepEqual(wrap(want.join(' ')), want, `help wraps like the PS2: ${want.join(' ')}`);
  console.log('help line: element box 375 / 50% -> every PS2 wrap');
}

// ---- Enter Cheat / Player Name keyboard: one dimming shape (local/ps2-capture/menus/fe-texture 40-enter-cheat-keyboard) ----
// Fullkeyboard has three full-screen shapes with a keyboard-shaped hole (back_com / back_exp / back_reg, alpha 153); they
// start hidden (flags bit 0x40) and cKeyboardPopup 0x1CD348 shows back_com only (compact mode, frame 45). All three
// drawn = 1 - 0.4^3 = 0.94 of the overlay colour over Options (the PS2 fits 0.591 = one shape).
{
  const { FeScreens } = await import('./fe-screens.js');
  const cs = read('character-select.json');
  globalThis.localStorage ??= { getItem: () => null, setItem() {}, removeItem() {} };
  const host = { screen: 'options', index: 5, ready: true, cb: {}, log: [], set(s) { this.screen = s; }, sync() {}, rider: { id: 'zoe', name: 'Zoe' }, riders: [],
    characterSelect: { base: { id: 'zoe', name: 'Zoe' }, unlocked: () => [], overlay: () => false, human() { return this.base; }, hidePreviewFor() {} }, careerUI: null, fonts: { FEFONT: read('FEFONT-glyphs.json') }, text() {} };
  const fe = new FeScreens(host); host.fe = fe; assert.ok(fe.init(cs, {}));
  const backs = { '07a1575d': 'back_com', '07a151f0': 'back_exp', '07a165b7': 'back_reg' };
  for (const kind of ['cheat', 'name']) {
    fe.openKeyboard(kind); const drawn = [];
    fe.kbLui.draw = (c, events, frame, override) => { for (const [name, label] of Object.entries(backs)) if (!override(fe.kbLui.byName.get(name))?.hidden) drawn.push(label); };
    fe.drawKeyboard({ save() {}, restore() {}, translate() {}, scale() {} }, fe.now());
    assert.deepEqual(drawn, ['back_com'], `${kind} keyboard: only the compact dimming shape`);
    const shape = fe.kbLui.byName.get('07a1575d'); assert.equal(shape.props[26], 153, 'back_com vertex alpha 153 (0.6)');
    fe.keyboard = null;
  }
  console.log('keyboard dim: back_com only (PS2 fit 0.591, port was 0.936)');
}

// ---- World static models: GS CLAMP_1 bits (material word+12 & 0x180000) exported per batch and sampled clamped ----
// Only CRA3 (polyrockshit meshes: textures 431 466 475 476 483), ABA1 (a loggy_1002 mesh on 102) and CHP2 (hidden
// volumes on 17) carry them; the sky materials are covered by test-sky-addressing.mjs.
{
  const assets = new URL('./public/assets/', import.meta.url).pathname;
  const want = { CRA3: [431, 466, 475, 476, 483], ABA1: [102], CHP2: [17] };
  for (const [pkg, loc] of [['CRA3', 'CRA3'], ['PEAK2/CRA3', 'CRA3'], ['ABA1', 'ABA1'], ['PEAK1/ABA1', 'ABA1'], ['CHP2', 'CHP2'], ['PEAK2/CHP2', 'CHP2']]) {
    const file = assets + pkg + '/world.json'; if (!fs.existsSync(file)) { console.log(`world wrap: ${pkg} not exported, skipped`); continue; }
    const d = JSON.parse(fs.readFileSync(file, 'utf8')), wrapped = d.batches.filter((b) => b.wrap);
    assert.deepEqual([...new Set(wrapped.map((b) => b.texture))].sort((a, b) => a - b), want[loc], `${pkg}: clamped static-model textures`);
    assert.ok(wrapped.every((b) => b.instance && b.wrap === 3), `${pkg}: clamp u and v, static models only`);
    if (loc === 'ABA1') assert.ok(d.batches.some((b) => b.texture === 102 && !b.wrap), `${pkg}: texture 102 also repeats on the other log meshes`);
  }
  const T = await import('three/webgpu'), { staticModelTexture } = await import('./world-material.js');
  const map = new T.Texture(); map.wrapS = map.wrapT = T.RepeatWrapping; const cache = new Map();
  const clamped = staticModelTexture(map, 3, cache);
  assert.notEqual(clamped, map); assert.equal(clamped.wrapS, T.ClampToEdgeWrapping); assert.equal(clamped.wrapT, T.ClampToEdgeWrapping);
  assert.equal(staticModelTexture(map, 3, cache), clamped, 'one clamped copy per texture'); assert.equal(map.wrapS, T.RepeatWrapping, 'the package texture keeps repeating');
  assert.equal(staticModelTexture(map, 0, cache), map); assert.equal(staticModelTexture(map, 1, cache).wrapT, T.RepeatWrapping);
  console.log('world wrap: CRA3 / ABA1 / CHP2 clamp batches, clamped texture copies');
}

// ---- New-career midway plane: the kind-7 stage calls drive the ABC1 plane's LiveComp (CUTSCENES/SETS/ABC1PLANE) ----
// PS2 local/ps2-capture/ctm-parity/runs/new-career s2400..3050: flies in from ~252 m, hovers at the locator, ramp down.
{
  const set = new URL('./public/assets/CUTSCENES/SETS/ABC1PLANE/world.json', import.meta.url).pathname;
  if (!fs.existsSync(set)) console.log('plane: SETS/ABC1PLANE not exported (python3 tools/export_cutscene_sets.py --plane), skipped');
  else {
    const meta = JSON.parse(fs.readFileSync(set, 'utf8')), { createStageSet, hideWorldCopy, NOSCRIPT } = await import('./cutscene-stage-sets.js');
    const T = await import('three/webgpu');
    const inst = meta.livecomp.instances[0];
    assert.equal(inst.name, 'mdl_ABC1_os609_full_version_inair2'); assert.equal(inst.resource, (1803 << 8) | 6);
    assert.deepEqual(inst.starts.map((s) => [s.symbol, s.cleanup]), [[0x0DE99225, 0x07D196B4], [0x0EE53794, 0x07D196B4]]);
    const meshes = [...new Set(meta.batches.map((b) => b.livecomp_node))].map((node) => { const m = new T.Mesh(); m.userData.liveComp = [inst.resource, node]; return m; });
    const stage = createStageSet(meta, meshes), root = meshes.find((m) => m.userData.liveComp[1] === 0);
    const locator = [-29634.0 / 100, -187319.578 / 100, -27407.998 / 100];   // anchor 37, PS2 cm (x, y, z) -> native (x, z, -y) / 100
    const rootAt = () => { stage.apply(); const p = new T.Vector3(...meta.world_copy.centre).applyMatrix4(root.matrix); return Math.hypot(p.x - locator[0], p.y - locator[1], p.z - locator[2]); };
    assert.ok(rootAt() > 240, 'at rest the plane is its static frame-0 copy, ~252 m out');
    assert.equal(stage.call(0x0D9F751E, 0x0D905E74, 0), false, 'the spray call (program 2) is not a plane player');
    assert.equal(stage.call(0x0DE99225, 0x07D196B4, 0), true, '#153 t0 starts frames 0..185');
    stage.sync(0); const far = rootAt(); stage.sync(185 * 2); const near = rootAt();
    assert.ok(far > 240 && near < 15, `#153 flies in: ${far.toFixed(1)} m -> ${near.toFixed(1)} m from the locator`);
    stage.sync(100); assert.ok(Math.abs(rootAt() - near) > 1, 'a seek back replays the player');
    stage.end(); assert.ok(rootAt() > 240, 'the cleanup (0x2807B0) puts it back to rest');
    stage.call(0x0EE53794, NOSCRIPT, 0); stage.sync(0); const hover0 = rootAt(); stage.sync(300); const hover1 = rootAt();
    assert.ok(hover0 < 15 && hover1 < 15, `#163 hovers at the locator (${hover0.toFixed(1)}, ${hover1.toFixed(1)} m)`);
    // the world's own copy (baked at frame 0) is hidden while the set stands in, restored after
    const scene = new T.Scene(), copy = new T.Mesh(new T.BufferGeometry()), other = new T.Mesh(new T.BufferGeometry());
    copy.geometry.boundingSphere = new T.Sphere(new T.Vector3(...meta.world_copy.centre), 4); other.geometry.boundingSphere = new T.Sphere(new T.Vector3(0, 0, 0), 4);
    scene.add(copy, other); const restore = hideWorldCopy(scene, meta, null);
    assert.equal(copy.visible, false); assert.equal(other.visible, true); restore(); assert.equal(copy.visible, true);
    console.log(`plane: #153 ${far.toFixed(0)} m -> ${near.toFixed(1)} m, #163 hover ${hover1.toFixed(1)} m, world copy hidden meanwhile`);
  }
}

// ---- Trick-boost rider light (RFX+0xD30: 1218D0 clears, 2EADD0 adds (0,0,1) x (2,2,2) while rider+0x2EC > 0) ----
// PS2 local/ps2-capture/presentation/boostfx bc-force (gp+0x1630 = 1) vs bc-normal, tick 2480, the human on the ground:
// rider pixels +19.7 / +14.7 / +16.9; the browser with this block +18.5 / +14.1 / +16.0 (docs/presentation.md).
{
  const { controllerLights, createControllerLights, TRICK_BOOST_LIGHT } = await import('./rider-controller-lights.js');
  const buf = new Float32Array(9), info = new Float32Array(8);
  assert.equal(controllerLights(info, buf), 0, 'no trick boost: no light');
  info[4] = 2.5; assert.equal(controllerLights(info, buf), 1); assert.deepEqual(Array.from(buf), [0, 0, 0, 0, 0, 1, 2, 2, 2], 'ambient 0, direction (0,0,1), colour (2,2,2)');
  assert.deepEqual(TRICK_BOOST_LIGHT.colour, [2, 2, 2]);
  const heap = new Float32Array(64); heap.set([1, 0, 0, 0, 0, 0, 1, 2, 2, 2], 4);
  const core = { HEAPF32: heap, _rider_controller_lights: () => 16 };   // the core's list at byte 16: [count, ambient, dir, colour]
  const lights = createControllerLights(core); assert.equal(lights.count(), 1); assert.equal(lights.pointer, 20, 'extra block = the list from +4');
  console.log('boost light: (0,0,1) x (2,2,2) from the core list (PS2 gain matched within 1.2 levels)');
}

// ---- Computer riders' streamers / aura: one rider's FX meshes filled from its own core view ----
{
  const T = await import('three/webgpu'), { createRiderFxMeshes } = await import('./boost-renderer.js');
  const mat = (tag) => Object.assign(new T.MeshBasicMaterial(), { name: tag });
  const materials = new Map([['psmr', mat('psmr')], [63, mat('strm')], [61, mat('prbn')]]), group = new T.Group();
  const fx = createRiderFxMeshes(group, materials, new T.Vector3(1, 2, 3));
  assert.equal(group.children.length, 6, 'four aura faces + two streamers');
  const heap = new Float32Array(8192), view = { HEAPF32: heap, _rider_fx_info: () => 0, _rider_fx_vertices: (strip) => 256 + strip * 1024 };
  heap.set([7, 61, 3, 0, 0, 0, 6, 0]);                              // serial 7, prbn, aura face 0: 3 vertices, streamer 4: 6
  heap.set([100, 200, 300, 0.5, 0.25, 1, 1, 1, 0.5], 256 / 4);
  fx.update(view);
  assert.equal(group.children[0].geometry.drawRange.count, 3); assert.equal(group.children[4].geometry.drawRange.count, 6);
  assert.equal(group.children[4].material.name, 'prbn', 'a trick boost: the purple prbn beams'); assert.equal(group.children[5].material.name, 'prbn');
  assert.deepEqual(Array.from(group.children[0].geometry.attributes.position.array.slice(0, 3)), [0, 1, -5], 'PS2 cm -> scene (x, z, -y) / 100 - origin');
  assert.ok(fx.populated());
  heap.set([8, 63, 0, 0, 0, 0, 0, 0]); fx.update(view); assert.equal(group.children[4].material.name, 'strm'); assert.equal(fx.populated(), false);
  console.log('computer rider FX: per-rider aura / streamer meshes from its core');
}

// ---- The clear behind the sky: black (382AF0 / renderer+6AE0, 0 in every in-race savestate) behind pv skyClear ----
{
  const main = sourceOf('main.js');
  assert.ok(main.includes('skyScene.background=new T.Color(0x000000)'), 'the sky scene clears to black');
  assert.ok(!main.includes('skyScene.background.copy'), 'the fog colour is never copied to the sky clear');
}

// ---- Heli / gondola ride over a world switch: which goWorld calls play it (web/ctm-transport.js) ----
{
  const { rideWanted } = await import('./ctm-transport.js');
  const peak1 = { course: { code: 'PEAK1', freeRide: { kind: 4 } }, cutscene: {} }, event = { course: { code: 'ARA1' }, cutscene: {} };
  const inFreeRide = { freeRide: { course: 17 } }, afterEvent = { afterEvent: true, active: { mode: 0, course: 0 } };
  assert.equal(rideWanted(peak1, inFreeRide, 15), true, 'Transport > Peak 2 from Peak 1: the heli ride');
  assert.equal(rideWanted(peak1, inFreeRide, 18), false, 'another station of the same peak: the in-world transport (main.js)');
  // pv mountainRide (free-ride.js freeRideHolds): the PS2 is one world, a Transport is world state 14 arg 1 (0x236250) inside it; the page's
  // MOUNTAIN world holds every course (free-ride.js transport plays the ride), and a peak world (a tier change) keeps its own stations.
  const mountain = { course: { code: 'MOUNTAIN', freeRide: { kind: 4 } }, cutscene: {} };
  assert.equal(rideWanted(mountain, inFreeRide, 15), false, 'mountainRide: Transport > Peak 2 inside the whole mountain');
  assert.equal(rideWanted(peak1, inFreeRide, 18), false, 'mountainRide: a peak world keeps its own stations');
  assert.equal(rideWanted({ course: { code: 'PEAK1', freeRide: { kind: 5 } }, cutscene: {} }, afterEvent, 18), true, 'mountainRide: after a peak run (kind 5) the world is loaded');
  assert.equal(rideWanted(event, afterEvent, 15), true, 'after Snow Jam, Peak 2: the heli ride');
  assert.equal(rideWanted(event, afterEvent, 17), true, 'after Snow Jam, a Peak 1 station: the gondola ride');
  assert.equal(rideWanted(event, afterEvent, 0), false, 'after Snow Jam, Snow Jam again: WS15 (white fade), no ride');
  assert.equal(rideWanted(peak1, {}, 15), false, 'a career start from the front end: the load screen');
  console.log('transport ride: peak change / post-event transport, not the same-location return');
}

// ---- The rival's '!' (2D5048): GS byte-space modulate and blend (encoded post-rider pass) ----
{
  const T = await import('three/webgpu'), { createRiderIcons, iconStep } = await import('./rival-beam.js');
  const scene = new T.Scene(), camera = new T.PerspectiveCamera(60, 4 / 3, 0.1, 1000); camera.position.set(0, 0, 10); camera.lookAt(0, 0, 0); camera.updateMatrixWorld();
  const icons = createRiderIcons({ T, scene, origin: new T.Vector3(), count: 1, texture: null });
  const mesh = scene.children[0].children[0];
  assert.ok(mesh.material.isNodeMaterial && mesh.material.fragmentNode, 'byte-space node material');
  assert.ok(mesh.layers.isEnabled(1), 'drawn in the encoded post-rider pass');
  for (let k = 0; k < 30; k++) icons.tick([3]);   // level 3 (the peak rival): orange, visibility ramps to 1
  icons.update(camera, [[0, 0, 0]]);
  const c = mesh.geometry.getAttribute('color');
  assert.deepEqual([c.getX(0), c.getY(0), c.getZ(0)], [1, 0.5, 0], 'vertex colour r, g, b as the GS sends them (x 255, texel 0x80 -> 255, 127, 0)');
  assert.ok(Math.abs(c.getW(0) - 0.8) < 1e-6, 'alpha 0.8 (x 128)');
  void iconStep;
  console.log("rival '!': byte-space orange 255,127,0 at 0.8 over the encoded frame");
}

// ---- New-career plane FX (pv planeFx): the engine loop and the snow spray of the kind-7 stage calls ----
// #153 / #163 (ABC1 track-6 globals, decoded through the stage VM): 0x0DE99225 / 0x0EE53794 = builtin 73 + 31 (script sound 201:
// TRANSPORT sound 1, 300 m, at the plane), cleanup 0x07D196B4 = builtin 73; 0x0D9F751E = builtin 25 (a Particle on
// ospreySpray_1001), cleanup 0x0D905E74 = builtin 69 mode 0.
{
  const { createPlaneFx } = await import('./cutscene-plane-fx.js');
  const calls = [], plays = [], stops = [];
  const core = { _stage_global_call: (...a) => { calls.push(['call', ...a]); return 1; }, _stage_cutscene_tick: () => calls.push(['tick']), _stage_cutscene_end: () => calls.push(['end']) };
  const sfx = { bankOf: () => ({ bnk: {} }), play: (o) => { plays.push(o); return { stop: (f) => stops.push(f) }; } };
  const fx = createPlaneFx({ core, audio: { sfx } });
  const matrices = [[[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [-54137, 21774, -186429, 1]]];
  const set = { meta: { location_index: 6, sound_loop: 201, instances: [{ position_cm: [-29634, 27408, -187319.6] }], livecomp: { instances: [{ resource: (1803 << 8) | 6 }] } },
    stage: { anim: { matrices: () => matrices } } };
  fx.call(set, 0x0DE99225, 0x07D196B4, true);
  assert.equal(plays.length, 1); assert.deepEqual([plays[0].slot, plays[0].sound, plays[0].bus, plays[0].volume, plays[0].vanish], [4, 1, 'UI', 127, 300], 'script sound 201: TRANSPORT sound 1, bus 5, 300 m');
  assert.deepEqual(plays[0].position, [-54137, 21774, -186429], 'at the plane (its LiveComp root, 297FA0 every frame)');
  matrices[0][3] = [-29700, 27400, -187300, 1]; fx.frame(set); assert.deepEqual(plays[0].position, [-29700, 27400, -187300], 'the voice follows the plane');
  fx.call(set, 0x0D9F751E, 0x0D905E74, false);
  assert.deepEqual(calls[0], ['call', 6, 0x0D9F751E, -29700, 27400, -187300], 'the spray: ABC1 track-6 globals in the core, the rider in the plane');
  fx.advance(2.5); assert.equal(calls.filter((c) => c[0] === 'tick').length, 2, 'the effects advance one entity pass per script tick');
  fx.stepEnd(set);
  assert.deepEqual(calls.find((c) => c[0] === 'call' && c[2] === 0x0D905E74)?.slice(0, 3), ['call', 6, 0x0D905E74], 'the step end runs the spray cleanup');
  assert.equal(stops.length, 1, 'and the plane cleanup (builtin 73) stops the engine');
  fx.end(); assert.ok(calls.some((c) => c[0] === 'end'), 'the list end hands the effects to the run');
  // The core (skipped before the core has the exports): the spray's Particle as the PS2 builds it (new-career fr.p2s holds two,
  // both alive after the run start: the owner matrix and every emitter word but the construct's visual-RNG draws 18..27 and
  // the flip phase 97 equal).
  const createCore = (await import(process.env.CORE ? new URL(process.env.CORE, `file://${process.cwd()}/`).href : './runtime/core.js')).default, c = await createCore();
  if (!c._stage_global_call) console.log('plane FX: the core has no stage_global_call (build web/build-core.sh), core part skipped');
  else {
    const root = new URL('./public/assets/', import.meta.url).pathname, txt = (p) => fs.readFileSync(root + p, 'utf8');
    const put = (t) => { const b = new TextEncoder().encode(t + '\0'), p = c._malloc(b.length); c.HEAPU8.set(b, p); return p; };
    const hash = put(JSON.parse(txt('ABC1/terrain.json')).source_sha256);
    c._init_terrain(put(txt('ABC1/terrain.json'))); c._init_world_collision(put(txt('ABC1/world_collision.json')), hash);
    assert.ok(c._init_stage_world(put(txt('ABC1/PARTICLES/particles.json')), put(txt('ABC1/LIVECOMP/livecomp.json')), put(txt('ABC1/STAGE/stage-world.json'))) > 0);
    const effects = () => { const U = new Uint32Array(c.HEAPU8.buffer), p = c._stage_world_effects() >> 2, out = []; let at = p + 1;
      for (let i = 0; i < U[p]; i++) { const r = U[at], kind = U[at + 1]; at += 4; if (kind !== 0) throw Error('only Particles here'); out.push({ r, matrix: Array.from(U.slice(at, at + 16)), emitter: Array.from(U.slice(at + 16, at + 116)) }); at += 116; } return out; };
    assert.equal(c._stage_global_call(6, 0x0D9F751E, -29634, 27408, -187319.6), 1, 'the spray function is in the ABC1 track-6 globals (a private VM before the first run)');
    assert.equal(c._stage_global_call(6, 0x0D9F751E, -29634, 27408, -187319.6 - 40000), 1);
    let spray = effects().filter((e) => e.r === 134662);
    assert.equal(spray.length, 1, 'a rider 400 m away: no second effect (0x2FEB24: 30000 cm)');
    assert.equal(c._stage_global_call(6, 0x0D9F751E, -29634, 27408, -187319.6), 1); spray = effects().filter((e) => e.r === 134662);
    assert.equal(spray.length, 2, '#153 t250 and #163 t0: two Particles on ospreySpray_1001 (PS2 fr.p2s)');
    const ps2 = { matrix: [0x3f800000, 0, 0, 0, 0, 0x3f800000, 0, 0, 0, 0, 0x3f800000, 0, 0xc6e72025, 0x46d75df7, 0xc838ff1e, 0x3f800000],
      emitter: { 0: 0xbf800000, 1: 0x19, 2: 1, 3: 1, 4: 0x3c, 6: 0x3fc00000, 7: 0x3ccccccd, 8: 0x437a0000, 10: 0xc3bb8000, 11: 0x3fc00000, 12: 0x442f0000, 16: 0x7f7fffff, 17: 0x3c9374bc, 23: 0x3fc00000,
        30: 0x43de38e1, 32: 0xc579ffff, 33: 0xc579ffff, 36: 0x4526aaaa, 41: 0x4526aaaa, 48: 0xc6e72025, 49: 0x46d75df7, 50: 0xc838ff1e, 51: 0x3f800000, 64: 0x42e66666, 65: 0x42e66666, 66: 0x43000000,
        67: 0x43000000, 76: 0x4108888a, 77: 0x4108888a, 79: 0xc2aaaaaa, 84: 0x3f800000, 85: 0x3f800000, 88: 0xc6f3136c, 89: 0x46cb6aaf, 90: 0xc838ff1e, 91: 0x3f800000, 92: 0xc6db2cdd, 93: 0x46e3513e,
        94: 0xc8385296, 95: 0x3f800000, 96: 1, 98: 0x41a00000, 99: 0x493160 } };
    assert.ok(c._stage_cutscene_tick() >= 2, 'the NIS tick advances both'); spray = effects().filter((e) => e.r === 134662);   // the snapshot is of updated emitters
    for (const e of spray) {
      assert.deepEqual(e.matrix, ps2.matrix, 'the owner matrix: ospreySpray_1001');
      for (let k = 0; k < 100; k++) if (![18, 19, 20, 21, 22, 24, 25, 26, 27, 97].includes(k)) assert.equal(e.emitter[k] >>> 0, ps2.emitter[k] ?? 0, `emitter word ${k} as on the PS2`);
    }
    c._stage_cutscene_end();
    assert.equal(c._stage_global_call(6, 0x0D905E74, NaN, NaN, NaN), 1, 'the cleanup (builtin 69 mode 0: a Particle stop is a no-op)');
    assert.equal(effects().filter((e) => e.r === 134662).length, 2, 'the spray goes on after the cleanup');
    console.log('plane FX: engine loop at the plane, spray = the PS2 Particle words (2 effects, kept emitting)');
  }
}

// ---- New-career plane camera (pv planeCam): #163 camera 5 is a Subject camera on anchor 43 = the live actor bound to 4 ----
// 0x27A0D8 ids 40..60 -> 279F18 -> 27B948: rider+0x110 (the NIS actor's root, 0x1237E4) and the heading / elevation of the
// physical forward rider+0x1B0, taken when the camera is cut to (27D850 -> 27D970). PS2 local/ps2-capture/presentation/nis
// (new-career.p2s re-run, docs/presentation.md 12): the camera block's Weather painter point (block 6 wrapper +8/+0xC = the
// eye after the camera update) at #163 t57 (camera 4) / t376 / t456 (camera 5).
{
  const cs = await import('./cutscenes.js'), root = new URL('./public/assets/CUTSCENES/', import.meta.url).pathname;
  if (!fs.existsSync(root + 'scripts/00000163.163.json')) console.log('plane camera: CUTSCENES not exported, skipped');
  else {
    const locators = JSON.parse(fs.readFileSync(root + 'locators.json', 'utf8')), script = JSON.parse(fs.readFileSync(root + 'scripts/00000163.163.json', 'utf8'));
    const objects = script.tracks.map((t) => t[0]), cam = (id) => objects.find((o) => o.ext?.camera_id === id), actor = objects.find((o) => o.kind === 5);
    const a37 = cs.anchorFor(37, { locations: locators.locations, location: 'ABC1' }), actors = [{ object: actor, frame: cs.objectFrame(a37, actor.ext.offset) }];
    const subject = (t) => { const r = cs.actorRootAt(actor, actors[0].frame, t); return { pos: r.pos, forward: [Math.cos(r.yaw + Math.PI / 2), Math.sin(r.yaw + Math.PI / 2), 0], up: [0, 0, 1], scale: 1 }; };
    const xy = (p, q) => Math.hypot(p[0] - q[0], p[1] - q[1]);
    const c4 = cs.cameraAt(cam(4), cs.objectFrame(a37, cam(4).ext.offset), 57);
    assert.ok(xy(c4.eye, [-30540.086, 28214.572]) < 0.01, `camera 4 (anchor 37) at t57 = the PS2 eye: ${c4.eye}`);
    assert.equal(cs.liveCutFrame(cam(4), actors, 328), null, 'a camera on a locator keeps its step-start frame');
    const live = cs.liveCutFrame(cam(5), actors, 328), old = cs.objectFrame(a37, cam(5).ext.offset);
    assert.ok(live && Math.abs(Math.hypot(live.T[0] - a37.pos[0], live.T[1] - a37.pos[1]) - 325) < 0.01, 'anchor 43 = the rider, 325 cm along the locator frame');
    for (const [t, ps2, tol] of [[376, [-29808.699, 27454.498], 1.5], [456, [-29879.826, 27464.574], 3]]) {   // tol: camera 5's shake channel (0.15) is not modelled
      const now = cs.cameraAt(cam(5), live, t, subject(t)), before = cs.cameraAt(cam(5), old, t, subject(t));
      assert.ok(xy(now.eye, ps2) < tol, `camera 5 at t${t} with planeCam: ${now.eye.map((v) => v.toFixed(2))} vs PS2 ${ps2}`);
      assert.ok(xy(before.eye, ps2) > 300, `without it (the actor's object frame) 3.25 m back: ${xy(before.eye, ps2).toFixed(1)} cm`);
    }
    // the runtime takes it at the cut only with the switch on (web/cutscenes.js cameraPose)
    const src = fs.readFileSync(new URL('./cutscenes.js', import.meta.url), 'utf8');
    assert.match(src, /seq\.liveCut !== cam\) \{ seq\.liveCut = cam; const f = liveCutFrame\(cam, seq\.actors, t\)/, 'cameraPose frames a live-actor camera at its cut');
    console.log('plane camera: #163 camera 5 at the door, PS2 eye within 1.5 / 3 cm at t376 / t456 (was 3.25 m back)');
  }
}

// ---- Terrain snow sparkle (pv sparkle; docs/presentation.md 14): 0x38D968 / VU1 program 4 at 0x1460 / LOD 0x22C410 ----
// PS2 local/ps2-capture/presentation/nis (new-career re-run, state a = #163 t66): the terrain object's lists hold 238598, 56326,
// 185606 with a level-8 edge (and unflagged 519942); 221446 / 553478 (mixed 4-6) and 15878 (all 6) not; the packets in RAM carry
// counts 321 / 307 / 279; VU1 memory holds 56326's sprites (alpha 63 / 75 / 42 / 79 / 82 for its sprites 198 / 200 / 202 / 221 / 226).
{
  const sp = await import('./terrain-sparkle.js'), file = new URL('./public/assets/PEAK1/ABC1/terrain-sparkle.bin', import.meta.url);
  if (!fs.existsSync(file)) console.log('sparkle: PEAK1/ABC1/terrain-sparkle.bin not exported (tools/export_terrain_sparkle.py), skipped');
  else {
    const read = (f) => { const b = fs.readFileSync(f); return sp.sparkleSet(b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength)); };
    const set = read(file);
    assert.equal(set.density, 1, 'a set starts at the Surface class default density 1.0');
    sp.setDensity(set, 4);   // renderer+0xC4 in state a (the ABC1 Surface region of the #163 camera)
    const index = (rid) => { for (let i = 0; i < set.n; i++) if (set.ids[i * sp.PATCH_WORDS + 54] === rid) return i; return -1; };
    const eye = [-30547.482, 28185.051, -188087.029].map(Math.fround);
    for (const [rid, count, level8] of [[238598, 321, true], [56326, 307, true], [185606, 279, true], [221446, null, false], [553478, null, false], [15878, null, false]]) {
      const i = index(rid); assert.ok(i >= 0, `patch ${rid} in the sparkle file`);
      if (count != null) assert.equal(set.counts[i], count, `patch ${rid}: the PS2 packet's count`);
      assert.equal(sp.hasLevel8Edge(set.corners, i * 12, eye), level8, `patch ${rid}: level-8 edge as in the PS2 lists`);
    }
    const f = (b) => new Float32Array(new Uint32Array([b]).buffer)[0];
    const i = index(56326), S = sp.sparkleSprites(set.words, set.counts[i], i * sp.PATCH_WORDS, true);
    const Tps2 = [[0xbe30fc4d, 0x3e584b6b, 0x3f7647eb], [0xbf3261c5, 0xbf376f38, 0x3d03a2d0], [0x3f323549, 0xbf2a3009, 0x3e8ac425]].map((r) => r.map(f));
    const s = sp.glintVector(Tps2, [0x31a646e2, 0x3e8afd53, 0x3f05a36c, 0x3f05a125].map(f));
    for (const [n, P, alpha] of [[198, [0xc6e1e623, 0x46d33299, 0xc837a2bc], 63], [200, [0xc6debedd, 0x46d8e94d, 0xc8368186], 75], [202, [0xc6dd262b, 0x46d14924, 0xc836b1a0], 42], [221, [0xc6df9525, 0x46d1e66d, 0xc8372601], 79], [226, [0xc6e0ab63, 0x46d32928, 0xc83759de], 82]]) {
      assert.deepEqual(Array.from(S.subarray(n * 6, n * 6 + 3)), P.map(f), `sprite ${n}: the VU's P(u, v) + 12 cm, bit for bit`);
      const dot = S[n * 6 + 3] * s[0] + S[n * 6 + 4] * s[1] + S[n * 6 + 5] * s[2];
      assert.ok(Math.abs(Math.trunc(Math.min(dot, 1) * 128) - alpha) <= 1, `sprite ${n}: alpha ${Math.trunc(Math.min(dot, 1) * 128)} vs the PS2's ${alpha}`);
    }
    const fast = sp.sparkleSprites(set.words, set.counts[i], i * sp.PATCH_WORDS);
    assert.ok(fast.every((v, k) => Math.abs(v - S[k]) < 0.05), 'the page evaluates P(u, v) in doubles: within 0.05 cm');
    // the twinkle rotation (terrain object +0x3E0) from the camera position: state a and PS2 weather/ebc3-wind-rail2
    for (const [e, T] of [[eye, [[-0.191, 0.208, 0.959], [-0.695, -0.719, 0.018], [0.694, -0.663, 0.282]]], [[-360808.539, 319309.95, 739866.481], [[-0.651, 0.622, 0.435], [-0.537, 0.027, -0.843], [-0.536, -0.782, 0.317]]]]) {
      const M = sp.twinkleRotation(e); assert.ok(M.every((r, a) => r.every((v, b) => Math.abs(v - T[a][b]) < 2e-3)), `twinkle rotation at ${e}`);
    }
    const src = sourceOf('main.js');
    assert.ok(src.includes('terrainSparkle=await createTerrainSparkle({T,origin})'), 'main.js builds it');
    console.log(`sparkle: PS2 counts / LOD lists / VU sprites (5 bit-exact) / twinkle rotation (2 states) match; ${set.n} patches in PEAK1/ABC1`);
    // Density renderer+0xC4 = world painter type 10 (Surface) current value: the counts scale with it; a count only ends the RNG chain.
    const lo = set.words.subarray(i * sp.PATCH_WORDS + 48, i * sp.PATCH_WORDS + 51), hi = set.words.subarray(i * sp.PATCH_WORDS + 51, i * sp.PATCH_WORDS + 54);
    assert.deepEqual([1, 1.8, 1.5, 4].map((d) => sp.sparkleCount(lo, hi, d)), [76, 138, 115, 307], 'patch 56326 counts at densities 1 / 1.8 / 1.5 / 4');
    const short = sp.sparkleSprites(set.words, 76, i * sp.PATCH_WORDS);
    assert.deepEqual(Array.from(short), Array.from(fast.subarray(0, 76 * 6)), 'a smaller count is a prefix of the chain');
    // PS2 renderer+0xC4 and the camera (outer camera +0x20): ABC1 race (setpieces-abc1 ticks 400 / 1200) 1.8, CRA3 (cra3-full 418 /
    // 2418) 1.5 (1.49998, blended), #163 state a 4.0; ARA1 / DBC2 / EBC3 / ERA5 1.0 (no Surface section).
    const courses = { ABC1: [[10093.025, -41885.477, 1.8], [28193.729, -32607.914, 1.8]], CRA3: [[-129214.375, -59956.914, 1.5], [-105596.281, -65784.281, 1.5]], 'PEAK1/ABC1': [[-30547.482, 28185.051, 4]] };
    const surfaced = Object.keys(courses).map((c) => [c, read(new URL(`./public/assets/${c}/terrain-sparkle.bin`, import.meta.url))]);
    if (surfaced.some(([, s]) => !s.surface)) console.log('sparkle: Surface painter blocks not exported yet (terrain-sparkle.bin v1), lookups skipped');
    else {
      for (const [c, s] of surfaced) for (const [x, y, d] of courses[c]) {
        const painter = sp.createSurfacePainter(); painter.step(s.surface, Math.fround(x), Math.fround(y));
        assert.equal(painter.current, Math.fround(d), `${c}: Surface density at (${x}, ${y})`);
      }
      assert.equal(read(new URL('./public/assets/ARA1/terrain-sparkle.bin', import.meta.url)).surface, null, 'ARA1 has no Surface section');
      // 2C0778 with a rate < 0: the first step jumps, then w = rate^2 per tick; leaving the tree resets to the defaults (1.0, +0 = 0)
      const t = surfaced[0][1].surface, painter = sp.createSurfacePainter(), [x, y] = courses.ABC1[0];
      painter.step(t, x, y); painter.current = 1; painter.step(t, x, y);
      assert.equal(painter.current, Math.fround(Math.fround(Math.fround(0.0016) * Math.fround(1.8)) + Math.fround(Math.fround(1 - Math.fround(0.0016)) * 1)), 'one blend tick (rate -0.04)');
      painter.step(t, 1e9, 1e9); assert.deepEqual([painter.current, painter.distance], [1, 0], 'outside the tree: class defaults');
      console.log('sparkle: Surface painter densities 1.8 / 1.5 / 4.0 (ABC1 race, CRA3, #163) as the PS2 renderer+0xC4; blend and reset');
    }
  }
}

// ---- Backcountry heli drops (pv bcHeli; docs/presentation.md 16): SETS/<LOC>HELI on anchor 29 (locator 5) ----
// The core runs the kind-7 calls in each location's stage globals (stage_global_call): 0x0E995385 = LiveComp frames 0..185,
// 0x0DD5F634 = 186..550, their cleanup 0x0AE69AB4 = 551..677 in loop mode, each with loop sound 201 on the heli.
{
  const { createStageSet } = await import('./cutscene-stage-sets.js');
  const setsRoot = new URL('./public/assets/CUTSCENES/SETS/', import.meta.url), f = (b) => new Float32Array(new Uint32Array([b]).buffer)[0];
  const heliSets = ['ABC1HELI', 'DBC2HELI', 'EBC3HELI'].filter((k) => fs.existsSync(new URL(`${k}/world.json`, setsRoot)));
  if (heliSets.length < 3) console.log('bcHeli: SETS/<LOC>HELI not exported (tools/export_cutscene_sets.py --helis), set checks skipped');
  else for (const k of heliSets) {
    const meta = JSON.parse(fs.readFileSync(new URL(`${k}/world.json`, setsRoot), 'utf8')), inst = meta.livecomp.instances[0];
    assert.equal(meta.anchor, 29, `${k}: anchor 29 (locator 5)`); assert.equal(meta.sound_loop, 201);
    assert.equal(inst.name, `mdl_${k.slice(0, 4)}_os609_full_version_inair`);
    const starts = inst.starts.map((st) => [st.symbol >>> 0, st.trigger, f(st.words[3]), f(st.words[4]), st.words[1]]);
    assert.deepEqual(starts, [[0x0E995385, 'stage', 0, 185, 0], [0x0DD5F634, 'stage', 186, 550, 0], [0x0AE69AB4, 'cleanup', 551, 677, 1]], `${k}: the calls' players`);
    const set = createStageSet(meta, []);
    assert.equal(set.call(0x0DD5F634, 0x0AE69AB4, 0), true); set.sync(100);
    assert.equal(Math.round(set.anim.state(inst.resource).high * 30), 550, '#137 t0: frames 186..550 (times in seconds at 30 fps)');
    set.end(); const st = set.anim.state(inst.resource);
    assert.ok(st && st.mode === 1 && Math.round(st.low * 30) === 551 && Math.round(st.high * 30) === 677, 'the step end: the cleanup starts the hover loop 551..677');
  }
  const plane = JSON.parse(fs.readFileSync(new URL('ABC1PLANE/world.json', setsRoot), 'utf8')), ps = createStageSet(plane, []);
  ps.call(0x0DE99225, 0x07D196B4, 0); ps.end();
  assert.equal(ps.anim.state(plane.livecomp.instances[0].resource), null, 'the plane: its cleanup still removes the player (SetNodeState 1)');
  const src = fs.readFileSync(new URL('./cutscenes.js', import.meta.url), 'utf8');
  assert.match(src, /: !seq\.loop && HELI_SETS\.has\(active\?\.location\) && seq\.objects\.some\(\(o\) => o\.ext\?\.anchor === 29\)/, 'setOf picks <LOC>HELI');
  const createCore = (await import(process.env.CORE ? new URL(process.env.CORE, `file://${process.cwd()}/`).href : './runtime/core.js')).default, c = await createCore();
  if (!c._stage_global_call || !c._stage_world_livecomps) console.log('bcHeli: the core has no stage_global_call, core part skipped');
  else {
    const root = new URL('./public/assets/', import.meta.url).pathname, txt = (p) => fs.readFileSync(root + p, 'utf8');
    const put = (t) => { const b = new TextEncoder().encode(t + '\0'), p = c._malloc(b.length); c.HEAPU8.set(b, p); return p; };
    const hash = put(JSON.parse(txt('DBC2/terrain.json')).source_sha256);
    c._init_terrain(put(txt('DBC2/terrain.json'))); c._init_world_collision(put(txt('DBC2/world_collision.json')), hash);
    assert.ok(c._init_stage_world(put(txt('DBC2/PARTICLES/particles.json')), put(txt('DBC2/LIVECOMP/livecomp.json')), put(txt('DBC2/STAGE/stage-world.json'))) > 0);
    const log = () => { const U = new Uint32Array(c.HEAPU8.buffer), p = c._stage_world_livecomps() >> 2, out = []; for (let i = 0; i < U[p]; i++) out.push(Array.from(U.slice(p + 1 + i * 16, p + 17 + i * 16))); return out; };
    for (const [sym, lo, hi, mode] of [[0x0E995385, 0, 185, 0], [0x0DD5F634, 186, 550, 0], [0x0AE69AB4, 551, 677, 1]]) {
      const before = log().length; assert.equal(c._stage_global_call(30, sym, NaN, NaN, NaN), 1, `DBC2 track 30 globals hold 0x${sym.toString(16)}`);
      const w = log().slice(before); assert.equal(w.length, 1); assert.deepEqual([w[0][1], f(w[0][5]), f(w[0][6]), w[0][3]], [580126, lo, hi, mode], `0x${sym.toString(16)}: the heli (resource 580126) frames ${lo}..${hi}, mode ${mode}`);
    }
  }
  // heliLight: VU1 program 3 lit instance (0x2170 x128 on xyz, w 128 on the constant row; 0x8B8 basis 1, x^2, y^2, z^2, xy, zx,
  // yz, x, y, z; clamp 0..255, FTOI0); the heli's bank = the location's Lighting reference 3 (PS2 RAM: DBC2 index 19, EBC3 27)
  const { litVertexColour } = await import('./cutscenes.js');
  const unit = (k, lane) => Array.from({ length: 10 }, (_, i) => [0, 1, 2, 3].map((l) => (i === k && l === lane ? 1 : 0)));
  assert.deepEqual(litVertexColour({ scale: 128, rows: unit(0, 0) }, [0, 0, 1]), [128, 0, 0, 128], 'the constant row x 128, alpha 128');
  assert.deepEqual(litVertexColour({ scale: 128, rows: unit(3, 1) }, [0.6, 0, 0.8]), [0, 81, 0, 128], 'row 3 = z^2 (0.64 x 128 = 81.9 -> 81)');
  assert.deepEqual(litVertexColour({ scale: 128, rows: unit(4, 2) }, [0.6, 0.8, 0]), [0, 0, 61, 128], 'row 4 = xy');
  assert.deepEqual(litVertexColour({ scale: 128, rows: unit(0, 0).map((r, i) => (i === 0 ? [3, 0, 0, 0] : r)) }, [0, 0, 1]), [255, 0, 0, 128], 'clamped to 255');
  for (const [k, bank, index] of [['ABC1HELI', 'AOBR1', 11], ['DBC2HELI', 'DOBR1', 19], ['EBC3HELI', 'EOBR1', 27]]) {
    const f2 = new URL(`${k}/world.json`, setsRoot); if (!fs.existsSync(f2)) continue;
    const L = JSON.parse(fs.readFileSync(f2, 'utf8')).lighting;
    if (!L) { console.log(`heliLight: ${k} has no lighting bank yet (tools/export_cutscene_sets.py --helis)`); continue; }
    assert.deepEqual([L.bank, L.index, L.scale, L.rows.length], [bank, index, 128, 10], `${k}: the object bank`);
  }
  const main = sourceOf('main.js');
  assert.ok(main.includes('cutscenes?.linger?.(isPaused()?0:Math.min(dt,0.25))'), 'main.js advances the heli hover (heliHover) while no cutscene plays');
  console.log('bcHeli: the three heli sets (players of the calls, the cleanup hover), DBC2 calls in the core = the set; heliLight VU semantics and banks');
}

// ---- NIS projection (pv nisProjection; docs/presentation.md 15): the letterbox block 0.75 / 0.63 of 2EAA28, slid by 2EA900 ----
// PS2 RAM (renderer 0x61BA60 +0x6B94 block, VU1 clip rows): #163 cam5a (fov 30 deg, block 1 / 0.125 / 0.75 / 0.75 / 0.63) P00 1.2990
// P11 1.4549; peak2-arr #137 (fov 37.73) 0.9693 / 1.0856; the objectives idle (script 73, fov 40, block 0 / 0 / 1 / 1 / 1)
// 1.1918 / 1.5890; bra2-arrival mid-slide (fov 43.19, top 0.1169 of 0.125) 0.8160 / 0.9285.
{
  const { nisTangents } = await import('./cutscenes.js');
  const deg = Math.PI / 180, P = (fov, mode, f) => nisTangents(Math.tan(fov * deg), mode, f).map((v) => 1 / v);
  for (const [name, fov, f, p00, p11] of [['#163 cam5a', 30, 1, 1.2990, 1.4549], ['#137 peak2-arr', 37.73, 1, 0.9693, 1.0856], ['script 73 idle', 40, 0, 1.1918, 1.5890], ['bra2-arrival slide', 43.19, 0.1169 / 0.125, 0.8160, 0.9285]]) {
    const [a, b] = P(fov, 0, f);
    assert.ok(Math.abs(a - p00) < 2e-3 && Math.abs(b - p11) < 2e-3, `${name}: P00 / P11 ${a.toFixed(4)} / ${b.toFixed(4)} vs the PS2's ${p00} / ${p11}`);
  }
  // 16:9 draws the 336-line band (P over the band = full-frame P11 / 0.75); Anamorphic keeps the 4:3 frame's numbers
  const [a1, b1] = P(30, 1, 1), [a2, b2] = P(30, 2, 1);
  assert.ok(Math.abs(a1 - 1.2990) < 2e-3 && Math.abs(b1 - 1.4549 / 0.75) < 2e-3 && Math.abs(a2 - 1.2990) < 2e-3 && Math.abs(b2 - 1.4549) < 2e-3, 'widescreen modes 16:9 / Anamorphic');
  const src = fs.readFileSync(new URL('./cutscenes.js', import.meta.url), 'utf8');
  assert.match(src, /if \(api\.qaFovAspect == null\) \{/, 'applyCamera uses it');
  console.log('NIS projection: P00 / P11 of 4 PS2 states (letterboxed, idle, mid-slide) and the widescreen bands');
}

// ---- The transport loop drawn over a world switch (pv acrossLoop): canvas alpha 1 ----
// cutscenes.acrossSwitch renders straight to the canvas; the sky dome's blended batches (alpha As^2 + Ad(1 - As)) and, without
// heliSky, the gondola windows over the transparent clear left alpha < 1, which three's output pass and the premultiplied canvas
// turn into sRGB(rgb / a) x a on screen (Chrome and WebKit alike; the 2D-canvas QA capture unpremultiplied it). Measured
// (docs/presentation.md 13): canvas alpha < 250 on 18 % of the frame before, none after; WebKit = Chrome within 1 level per region.
{
  const T = await import('three/webgpu'), { opaqueAlphaFill } = await import('./cutscenes.js');
  const fill = opaqueAlphaFill(T), m = fill.material;
  assert.deepEqual([m.blending, m.blendSrc, m.blendDst, m.blendSrcAlpha, m.blendDstAlpha, m.blendEquation], [T.CustomBlending, T.ZeroFactor, T.OneFactor, T.OneFactor, T.ZeroFactor, T.AddEquation], 'RGB kept (Zero / One), alpha set (One / Zero)');
  assert.ok(fill.renderOrder >= 1e9 && m.transparent && !m.depthTest && !m.depthWrite && !fill.frustumCulled, 'drawn last over everything');
  // blend arithmetic of the pass on a frame pixel: colour unchanged, alpha 1
  const blend = (src, dst) => [0, 1, 2].map((k) => src[k] * 0 + dst[k] * 1).concat([src[3] * 1 + dst[3] * 0]);
  assert.deepEqual(blend([0, 0, 0, 1], [0.3, 0.4, 0.6, 0.75]), [0.3, 0.4, 0.6, 1]);
  const src = fs.readFileSync(new URL('./cutscenes.js', import.meta.url), 'utf8');
  assert.match(src, /const fill = opaqueAlpha\(\); scene\.add\(fill\);\s*try \{ host\.render\(\); \}[^\n]*finally \{[^\n]*scene\.remove\(fill\); \}/, 'only the across-switch draw adds it');
  console.log('transport loop over a switch: canvas alpha 1 (acrossLoop)');
}

// ---- Weather leftovers (pv regionTick; docs/weather.md 10): the page painters take the record of the core's painter region
// gp+0x770 on the tick it changes (the camera block, PS2 weather/frd-regions D -> D_DRA4 at 2842: the camera Sun's target size
// 390 -> 410 at 2842, 390.1 / 390.3 / 390.4 at 2842 / 2844 / 2845; the browser 390.098 / 390.293 / 390.389) ----
{
  const { painterRegions } = await import('./painter-regions.js'), { createSunPainter } = await import('./sun-flare.js'), { createGlarePainter } = await import('./glare-pass.js');
  const { createScreenTint } = await import('./screen-tint.js');
  const root = new URL('./public/assets/', import.meta.url).pathname, doc = (p) => JSON.parse(fs.readFileSync(root + p, 'utf8'));
  // a fake core: fog_info (ticks at 7, the camera eye x/y at 8/9, ready at 10) and weather_region_info (gp+0x770)
  const heap = new ArrayBuffer(256), F = new Float32Array(heap), I = new Int32Array(heap);
  const core = { HEAPF32: F, HEAPU8: new Uint8Array(heap), _fog_info: () => 0, _weather_region_info: () => 128 };
  const at = (t, region, x = 115242.56, y = -117310.45) => { F[7] = t; F[8] = x; F[9] = y; F[10] = 1; I[32] = region; };
  painterRegions.reset();
  const sunD = doc('PEAK2/D/sun-painter.json'), sunDD = doc('PEAK2/D_DRA4/sun-painter.json');
  const base = doc('CRA3/sun-flare/sun-flare.json');
  // registry off (pv regionTick off): the painter keeps its package and the page's hook sets trees
  let sun = createSunPainter({ ...base, painter: sunD.painter });
  at(1, 27); sun.tick(core); at(2, 28); sun.tick(core); assert.equal(sun.state.current[8], 390, 'no records: the package tree');
  sun.setTree(sunDD.painter); at(3, 28); sun.tick(core); assert.ok(sun.state.current[8] > 390, 'no records: the hook tree is taken');
  // registry on
  painterRegions.set(27, { sun: sunD, tint: null, glare: null }); painterRegions.set(28, { sun: sunDD, tint: null, glare: null });
  sun = createSunPainter({ ...base, painter: sunD.painter });
  at(1, -1); sun.tick(core); assert.equal(sun.state.current[8], 390, 'before the first contact (gp+0x770 = -1): the package');
  at(2, 27); sun.tick(core); assert.equal(sun.state.current[8], 390, 'region 27 (D): its record');
  at(3, 27); sun.tick(core); assert.equal(sun.state.current[8], 390);
  at(4, 28); sun.tick(core); assert.ok(Math.abs(sun.state.current[8] - 390.098) < 1e-3, `region 28 on its tick: blends to D_DRA4's 410 (${sun.state.current[8]})`);
  sun.setTree(sunD.painter); at(5, 28); sun.tick(core); assert.ok(sun.state.current[8] > 390.098, 'the page hook is ignored while the records rule');
  at(6, 99); sun.tick(core); assert.deepEqual(sun.state.current, createSunPainter(base).state.current, 'a region without a loaded record: the class defaults (0x2C09D8)');
  // glare / ScreenTint follow the same way (a record without a section: the reset path)
  const glare = createGlarePainter(doc('CRA3/glare.json')); painterRegions.set(28, { sun: sunDD, tint: doc('PEAK2/D_DRA4/screen-tint.json'), glare: doc('PEAK2/D_DRA4/glare-painter.json') });
  at(10, 27); glare.tick(core); at(11, 28); glare.tick(core); assert.equal(glare.state.selected, -1, 'D_DRA4 has no glare section: reset');
  const tint = await createScreenTint(doc('CRA3/screen-tint.json'));
  at(20, 27); tint.step(core); at(21, 28); tint.step(core); assert.equal(tint.state.selected, 0, "D_DRA4's ScreenTint record on the region tick");
  painterRegions.reset();
  // the core's located Fog / Lighting records (skipped before the core has them)
  const createCore = (await import(process.env.CORE ? new URL(process.env.CORE, `file://${process.cwd()}/`).href : './runtime/core.js')).default, c = await createCore();
  if (!c._fog_location) console.log('region tick: the core has no fog_location (build web/build-core.sh), core part skipped');
  else {
    const put = (t) => { const b = new TextEncoder().encode(t + '\0'), p = c._malloc(b.length); c.HEAPU8.set(b, p); return p; };
    const text = (p) => fs.readFileSync(root + p, 'utf8');
    c._fog_keep_state(0); c._init_fog(put(text('CRA3/fog-tree.json')));
    assert.equal(c._fog_location(27, put(text('PEAK2/D/fog-tree.json'))), 1); assert.equal(c._fog_location(28, put(text('PEAK2/D_DRA4/fog-tree.json'))), 2);
    c._fog_keep_state(1); c._init_fog(put(text('PEAK2/D/fog-tree.json'))); c._fog_keep_state(0);
    assert.deepEqual(Array.from(new Int32Array(c.HEAPU8.buffer, c._fog_location_info(), 3)), [2, -2, 0], 'the page hook (keep state) is ignored while records exist; nothing applied before a step');
    c._fog_keep_state(0); c._init_fog(put(text('CRA3/fog-tree.json'))); assert.equal(new Int32Array(c.HEAPU8.buffer, c._fog_location_info(), 3)[0], 0, 'a new world drops the records');
    assert.equal(c._lighting_location(27, put(text('PEAK2/D/lighting.json'))), 1, 'lighting records are kept as text (parsed with the banks at the switch)');
    console.log('region tick: records fed; per-tick PS2 check: web/compare-ps2-capture.mjs weather/frd-regions with the located records (docs/presentation.md)');
  }
  console.log('region tick: Sun / glare / ScreenTint take gp+0x770\'s record on its tick (PS2 frd-regions 2842)');
}

console.log('test-presentation ok');
