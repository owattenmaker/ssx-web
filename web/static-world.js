// staticWorld (docs/sim-performance.md "Free-roam steady state"): the static world batches of a streamed location, as three.js
// objects that cost less per render, with the same pixels.
//
// three.js (r186) spends most of a phone frame on per-object work: every render of the scene recomposes the matrix of every
// object whose matrixAutoUpdate is on (and multiplies every descendant of such an object), then visits every visible mesh for its
// frustum test. A streamed location is thousands of batch meshes at a fixed position (-origin) that never move. Here:
//   - freeze: each plain static batch mesh keeps the matrix composed once (matrixAutoUpdate off), and so do the location group and
//     its static ancestors (the page never moves them); matrixWorld is the same product as before, computed once;
//   - cells: the plain static batches move into cell groups of a 128 m grid (by bounding-sphere centre). Before each render of the
//     scene (scene.onBeforeRender, with the render's own camera) a cell whose box holds none of its members' bounding spheres inside
//     the camera frustum is hidden, so three skips its members; a cell never hides a member three would draw: the box contains every
//     member's world sphere (plus 1 cm), and a sphere inside a box that lies outside one frustum plane lies outside that plane too.
//     A render whose camera does not see layer 0 (the encoded snow composite, layer 1) hides every cell (their members are layer 0).
//     A cell with frustumCulled off (the race warm-up turns culling off on every object) stays visible. The cells skip their
//     members' matrix update unless forced (the members are frozen).
// Draw order is unchanged: three sorts by group order (0 for every group here), render order, projected bounding-sphere depth and
// object id, none of which a cell changes. Meshes that anything moves or shows per frame by other means (LiveComp players,
// MeshAnim pieces, magnets and script / pickup / moving / event-dead instances, flag cloth) stay where they are.
// staticRefresh: three refreshes every node-material render object FULL on every draw (NodeMaterialObserver: a
// material with any node property): node, geometry and every binding update. A frozen static batch of a cell drawn with a shared
// world material (web/world-material.js sharedWorldMaterials) without a UV-scroll uniform changes nothing per object after its first
// draw: its object uniforms (world matrix, alpha test) are fixed, its textures are bound at the first (FULL) draw, and the only
// per-frame values it reads (camera; the rider-shadow receiver's rows and count, moved to the shared render group under the switch,
// web/rider-shadow.js) sit in shared groups. So those render objects take three's own static path: the shared refresh (RenderObjectRefreshType.SHARED) once
// per render and material observer, else none (until 2026-09-27 the first one per observer took FULL: a wrong constant, same pixels). Event
// terrain (terrain refinement edits its index) is not marked. staticRefreshEnabled: QA A/B in one page.
import { Group, Box3, Sphere, Frustum, Matrix4, NodeMaterial, RenderObjectRefreshType } from 'three/webgpu';
const { SHARED = 1, NONE = 0, FULL = 2 } = RenderObjectRefreshType ?? {}; // three's own static path: the shared refresh once per render, else none
// staticRefresh.wide: the static path for every render object drawn with a shared world material without a UV-scroll uniform
// (marked or not: blended batches, event terrain, LiveComp / MeshAnim / script-shown / flag batches), each one taking a full refresh whenever
// something the static path skips has changed since its last full refresh (unchanged below). What it skips, per render object: the object
// group (world matrix; material opacity and alpha test: the only object uniforms of these graphs besides the UV-scroll offset), its texture
// bindings (a texture whose version moved re-uploads only through a full refresh: crowd-2d.js swaps world texture 9-161's image every few
// frames, the stadium crowd; this, not blending, is why marked transparent batches drew differently), and the geometry upload (terrain
// refinement edits the coarse terrain index; flag cloth writes its positions).
// enabled / wide can be switched off in a page (QA comparisons)
export const staticRefresh = { enabled: true, wide: true, installed: false };
const wideStatic = (m) => { const t = m?.userData?.ps2Textures; return t !== undefined && t.shape?.scroll === undefined; };
const version = (a) => (a === undefined || a === null ? 0 : a.isInterleavedBufferAttribute ? a.data.version : a.version);
function unchanged(ro) { // records what the next check compares against
  const m = ro.material, t = m.userData.ps2Textures, g = ro.geometry, e = ro.object.matrixWorld.elements;
  // versions only grow: their sum moves whenever one does
  const index = g.index, pos = g.attributes.position, col = g.attributes.color, map = t.map, atlas = t.atlas;
  const v = m.version + (map ? map.version : 0) + (atlas ? atlas.version : 0) + version(index) + version(pos) + version(col);
  let s = ro.ssxStatic; if (s === undefined) s = ro.ssxStatic = { v: -1, opacity: NaN, alphaTest: NaN, e: new Float64Array(16), g: null, index: null, pos: null, col: null, map: null, atlas: null };
  // identities too: a swapped geometry / attribute / texture of the same version is a change
  let same = s.v === v && s.opacity === m.opacity && s.alphaTest === m.alphaTest && s.g === g && s.index === index && s.pos === pos && s.col === col && s.map === map && s.atlas === atlas;
  const se = s.e; for (let i = 0; i < 16; i++) if (se[i] !== e[i]) { se[i] = e[i]; same = false; }
  s.v = v; s.opacity = m.opacity; s.alphaTest = m.alphaTest; s.g = g; s.index = index; s.pos = pos; s.col = col; s.map = map; s.atlas = atlas;
  return same;
}
if (staticRefresh.enabled) {
  const proto = NodeMaterial?.prototype, setup = proto?.setupObserver;
  if (typeof setup !== 'function') console.warn('staticRefresh: NodeMaterial.setupObserver missing (three changed); switch inactive');
  else {
    proto.setupObserver = function (builder) {
      const observer = setup.call(this, builder), full = observer?.needsRefresh;
      if (typeof full !== 'function' || typeof observer.firstInitialization !== 'function') return observer;
      observer.needsRefresh = function (renderObject, nodeFrame) {
        if (staticRefresh.enabled && (staticRefresh.wide ? wideStatic(renderObject.material) : renderObject.object?.userData?.staticRefresh === true) &&
          !this.hasAnimation && !this.needsVelocity?.(nodeFrame.renderer) && !this.hasDynamicInstancing?.(renderObject.object)) {
          const first = this.firstInitialization(renderObject), same = !staticRefresh.wide || unchanged(renderObject);
          if (first) return full.call(this, renderObject, nodeFrame);
          if (!same) return FULL;
          const { renderId } = nodeFrame; if (this.renderId !== renderId) { this.renderId = renderId; return SHARED; }
          return NONE;
        }
        return full.call(this, renderObject, nodeFrame);
      };
      return observer;
    };
    staticRefresh.installed = true;
  }
}
// A member whose every material is an opaque shared world material without a UV-scroll uniform (the scroll offsets are per-object uniforms).
// Transparent batches keep the full refresh here: with them marked, the Snow Jam stadium drew differently until a full refresh (the crowd
// texture's frames, see staticRefreshWide, which covers them with its checks); opaque batches were identical everywhere.
const refreshable = (o) => !o.userData.terrainBase && [].concat(o.material).every((m) => { const t = m?.userData?.ps2Textures; return !!t && t.shape?.scroll === undefined && !m.transparent; });

const CELL = 128, MARGIN = 0.01; // metres (three space)
const organized = new Map();     // organized group -> its cells (hidden by the scene hook); a group left detached is dropped
const hooked = new WeakSet();
const frustum = new Frustum(), projScreen = new Matrix4(), sphere = new Sphere();
export const staticWorldStats = { cells: 0, members: 0, culled: 0, renders: 0 };

class StaticCell extends Group {
  constructor(box) { super(); this.name = 'static-cell'; this.box = box; this.matrixAutoUpdate = false; this.userData.staticWorld = true; }
  // Members are frozen: their world matrices change only when an ancestor's does (force).
  updateMatrixWorld(force) { if (force || this.matrixWorldNeedsUpdate) super.updateMatrixWorld(force); }
}

// A batch mesh nothing else moves, shows or re-parents per frame.
function plainStatic(o) {
  const u = o.userData;
  return o.isMesh && !o.isSkinnedMesh && !o.isInstancedMesh && o.frustumCulled && o.layers.mask === 1 && !!o.geometry?.boundingSphere &&
    u.liveComp === undefined && u.scriptResource === undefined && u.pickupResource === undefined && u.movingResource === undefined &&
    u.eventDeadResource === undefined && u.hiddenResource === undefined && u.flagResource === undefined && u.meshanimResource === undefined;
}

function cull(camera) {
  staticWorldStats.renders++;
  const layer0 = (camera.layers.mask & 1) !== 0;
  if (layer0 && !camera.isArrayCamera) {
    projScreen.multiplyMatrices(camera.projectionMatrix, camera.matrixWorldInverse);
    frustum.setFromProjectionMatrix(projScreen, camera.coordinateSystem, camera.reversedDepth);
  }
  let culled = 0;
  for (const [group, list] of organized) {
    if (!group.parent) { releaseStaticWorld(group); continue; } // released with its course / location (removed from the scene)
    if (!group.visible) continue; // the location is not drawn: three never reaches its cells
    for (const c of list) { const on = !c.frustumCulled || (layer0 && (camera.isArrayCamera || frustum.intersectsBox(c.box))); c.visible = on; if (!on) culled++; }
  }
  staticWorldStats.culled = culled;
}

function hookScene(scene) {
  if (!scene?.isScene || hooked.has(scene)) return;
  hooked.add(scene);
  const prior = scene.onBeforeRender;
  scene.onBeforeRender = function (renderer, s, camera, target) { if (organized.size) cull(camera); return prior.call(this, renderer, s, camera, target); };
}

// Freeze and cell the static batches of a world group (a streamed location, or an event course's world; called once it is in the
// scene, after everything that indexes its meshes by parent has run). Returns the cells.
export function organizeStaticWorld(group) {
  if (group.userData.staticCells) return group.userData.staticCells;
  let root = group; while (root.parent) root = root.parent;
  // the group and its ancestors up to the scene: identity transforms the page never moves
  for (let p = group; p && !p.isScene; p = p.parent)
    if (
      p.matrixAutoUpdate &&
      p.position.lengthSq() === 0 &&
      p.quaternion.w === 1 &&
      p.scale.x === 1 &&
      p.scale.y === 1 &&
      p.scale.z === 1
    ) {
      p.updateMatrix();
      p.matrixAutoUpdate = false;
    }
  group.updateWorldMatrix(true, false);
  const byKey = new Map();
  for (const o of [...group.children]) {
    if (!plainStatic(o)) continue;
    if (o.matrixAutoUpdate) { o.updateMatrix(); o.matrixAutoUpdate = false; } // main.js asset() already freezes world batches (2026-09-27)
    o.updateMatrixWorld(true);
    if (staticRefresh.installed && refreshable(o)) o.userData.staticRefresh = true;
    sphere.copy(o.geometry.boundingSphere).applyMatrix4(o.matrixWorld);
    const c = sphere.center, key = `${Math.floor(c.x / CELL)},${Math.floor(c.y / CELL)},${Math.floor(c.z / CELL)}`;
    let e = byKey.get(key); if (!e) byKey.set(key, (e = { box: new Box3(), members: [] }));
    e.box.expandByPoint(c.clone().addScalar(-sphere.radius - MARGIN)); e.box.expandByPoint(c.clone().addScalar(sphere.radius + MARGIN));
    e.members.push(o);
  }
  const out = [];
  for (const { box, members } of byKey.values()) {
    const cell = new StaticCell(box);
    // keep the members' order among the group's children (traversal order only; the draw order is the sort's)
    group.add(cell); for (const m of members) cell.add(m);
    cell.updateMatrixWorld(true); out.push(cell);
    staticWorldStats.members += members.length;
  }
  organized.set(group, out); staticWorldStats.cells += out.length;
  group.userData.staticCells = out; group.userData.staticWorld = true;
  hookScene(root);
  return out;
}

// The location is released (or detached): its cells leave the hook's list.
export function releaseStaticWorld(group) {
  const list = organized.get(group); if (!list) return;
  organized.delete(group); for (const c of list) c.visible = true;
  staticWorldStats.cells -= list.length; staticWorldStats.members -= list.reduce((n, c) => n + c.children.length, 0);
}
