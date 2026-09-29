// pv cables: the chairlift / gondola cables and the Metro-City mill lines (docs/visual-parity.md section 37).
//
// The PS2 draws them with every MultiSplineModifier whose builtin-20 key 10 is set (default 1 at 0x4FB7D0; only the Junction
// traffic and the Crow's Nest box vehicles pass 0):
//   entity draw 0x356298 (entity vt+0x24, every frame from 0x3550A0 while the entity lives and its instance's texture chunk is
//   resident) -> modifier container 0x352DD0 -> MultiSpline draw 0x35B418 (vt 0x48F168 +0x5C): the cars 1.., then
//   `if (mod+0x1C) 0x345430(mod+0x48 path, mod+0x20 colour)`;
//   0x345430: for every segment of the path (the kind-8 rail record the chairs ride, rails.json) whose AABB (+0x6C/+0x78) passes
//   the frustum test 0x345638 (renderer +0x2EC 0x37DE88), renderer +0x25C = 0x381F10 with the colour and 10 points;
//   0x381F10: A+D RGBAQ = colour (a, r, g, b) x 128 (default (1, 0, 0, 0): black, A 128), GIF tag PRIM 0x2 (LINESTRIP: flat, no
//   texture, no fog, no alpha blend, no AA) NLOOP 10 XYZF2, the segment's cubic rows (t^3, t^2, t, 1), MSCAL 0x77E;
//   VU1 program 3 at 0x3BF0: vertex k at t = k / 9 (t += Q = 1 / (count - 1)), through the guard-band clip matrix (renderer
//   +0x57C0: x 0.25 P00, y 0.21875 P11, z / w = the camera's near..far), and ADC (no line into it) when the vertex or the one
//   before it is outside the clip volume; vertex 0 always (tools/test_cable_vu_native.py: 400 cases, ADC exact).
// Here: one mesh per cable, each sub-line a screen-space quad one PS2 pixel thick (the 512 x 448 frame; the GS line covers one
// pixel per step of its major axis, so the quad is offset +-0.5 pixel along the minor axis), at least one device pixel. A sub-line
// with an end outside the guard band (|x| > 4, |y| > 2048 / 448 in NDC) or the near..far range is not drawn, as the ADC. Depth
// tested and written in the world pass like the world models it follows, so the fog composite fogs it by depth. The segment AABB
// cull of 0x345638 draws nothing the GPU would not clip (every sample point lies in its segment's box: test-set-piece-cables.mjs).
import * as T from 'three/webgpu';
import {attribute, vec4, vec2, float, select, max, positionGeometry, modelViewMatrix, cameraProjectionMatrix, cameraNear, cameraFar, viewportSize} from 'three/tsl';

// MultiSpline owners with a cable, per location: owner = the authored instance resource ((rid << 8) | track, the stage handler row's
// instance), rail = the builtin-20 key 1 path (rails.json packed_id). tools/export_location_set_pieces.py script_calls(loc, 20) on
// all 17 race locations and the PEAK1/2/3 section programs: no other builtin-20 call leaves key 10 at its default; none passes a
// colour (keys 6..9), so every cable is black; the hubs and TRANSP have none.
export const CABLES = Object.freeze({
  ARA1: [{owner: 407560, rail: 1544, program: 69}, {owner: 711688, rail: 1800, program: 71}],           // tramlores_0/1 on GondolaRail_0/1
  BRA2: [{owner: 707856, rail: 16, program: 47}, {owner: 252432, rail: 528, program: 48}, {owner: 820240, rail: 272, program: 49}], // movingbina / cargobin_end(_b): mill_a, mill_end, mill_end_b
  ABA1: [{owner: 57093, rail: 16645, program: 38}],   // tramlores_1000
  CBA2: [{owner: 152086, rail: 11542, program: 38}],  // tramlores_1000
  CHP2: [{owner: 226327, rail: 791, program: 81}],    // tramlores_1000
  ESS3: [{owner: 175664, rail: 42032, program: 50}],  // tramlores_1000
  EBA3: [{owner: 91433, rail: 297, program: 50}, {owner: 183081, rail: 41, program: 52}], // tramwindya / tramwindyb
  EHP3: [{owner: 38444, rail: 24108, program: 40}],   // tramlores_1000
});
export const CABLE_OWNERS = new Set(Object.values(CABLES).flatMap((l) => l.map((c) => c.owner)));
// A cable location's package root in the streamed worlds (PEAK1..3 and MOUNTAIN share /assets/PEAK<n>/<LOC>/, peak.json).
const PEAK_OF = {ARA1: 1, BRA2: 1, ABA1: 1, CBA2: 2, CHP2: 2, ESS3: 3, EBA3: 3, EHP3: 3};
export const streamedCableRoot = (code) => (PEAK_OF[code] ? `/assets/PEAK${PEAK_OF[code]}/${code}/` : null);
export const POINTS = 10;          // 0x345430 passes 10 (a3) to 0x381F10: NLOOP and the VU loop count
const GUARD_X = 2048 / 512, GUARD_Y = 2048 / 448; // guard-band clip matrix scales 0.25 / 0.21875 (0x376C58 -> +0x57C0)

// The sample points of one segment (source cm, Z up): the VU's t sequence (Q = 1 / (count - 1) added in single precision).
export function segmentPoints(coefficients, count = POINTS, out = []) {
  const [a, b, c, d] = coefficients, step = Math.fround(1 / Math.fround(count - 1));
  let t = 0;
  for (let k = 0; k < count; k++) {
    out.push([0, 1, 2].map((i) => ((a[i] * t + b[i]) * t + c[i]) * t + d[i]));
    t = Math.fround(t + step);
  }
  return out;
}

let sharedMaterial = null;
function cableMaterial() {
  if (sharedMaterial) return sharedMaterial;
  const own = positionGeometry, other = attribute('cableOther', 'vec3'), corner = attribute('cableCorner', 'vec2');
  const viewOwn = modelViewMatrix.mul(vec4(own, 1)), viewOther = modelViewMatrix.mul(vec4(other, 1));
  const clipOwn = cameraProjectionMatrix.mul(viewOwn), clipOther = cameraProjectionMatrix.mul(viewOther);
  // the VU's CLIP on the projected vertex: outside the guard band or the near..far range (w = view depth)
  const outside = (clip, view) => clip.x.abs().greaterThan(clip.w.mul(GUARD_X)).or(clip.y.abs().greaterThan(clip.w.mul(GUARD_Y)))
    .or(view.z.negate().lessThan(cameraNear)).or(view.z.negate().greaterThan(cameraFar));
  const dropped = outside(clipOwn, viewOwn).or(outside(clipOther, viewOther));
  // the GS line: one pixel per step of the major axis (PS2 pixels: NDC x 256, y 224); never thinner than one device pixel
  const d = clipOther.xy.div(clipOther.w).sub(clipOwn.xy.div(clipOwn.w)).mul(vec2(256, 224));
  const half = vec2(max(float(1 / 512), viewportSize.x.reciprocal()), max(float(1 / 448), viewportSize.y.reciprocal()));
  const offset = select(d.x.abs().greaterThanEqual(d.y.abs()), vec2(0, corner.x.mul(half.y)), vec2(corner.x.mul(half.x), 0));
  const placed = vec4(clipOwn.xy.add(offset.mul(clipOwn.w)), clipOwn.zw);
  const material = new T.MeshBasicNodeMaterial({depthTest: true, depthWrite: true, side: T.DoubleSide, fog: false, toneMapped: false});
  material.vertexNode = select(dropped, vec4(2, 2, 2, 1), placed); // a dropped sub-line: every corner outside the clip volume
  material.colorNode = vec4(0, 0, 0, 1);   // RGBAQ (0, 0, 0, 128): colour (1, 0, 0, 0) as (a, r, g, b); PRIM ABE 0: opaque
  material.name = 'SetPieceCable';
  sharedMaterial = material;
  return material;
}

// One cable mesh from a rails.json rail: per segment POINTS - 1 quads (own end in position, the other end, corner (side, end)).
export function cableGeometry(rail, origin) {
  const quads = rail.segments.length * (POINTS - 1), pos = new Float32Array(quads * 12), oth = new Float32Array(quads * 12), cor = new Float32Array(quads * 8);
  const index = new Uint32Array(quads * 6); let q = 0;
  const scene = (p) => [p[0] / 100 - origin[0], p[2] / 100 - origin[1], -p[1] / 100 - origin[2]];
  for (const s of rail.segments) {
    const p = segmentPoints(s.source.coefficients).map(scene);
    for (let k = 1; k < POINTS; k++, q++) {
      const a = p[k - 1], b = p[k];
      for (let v = 0; v < 4; v++) {
        const end = v & 1, side = v < 2 ? 1 : -1, mine = end ? b : a, theirs = end ? a : b;
        pos.set(mine, q * 12 + v * 3); oth.set(theirs, q * 12 + v * 3); cor.set([side, end], q * 8 + v * 2);
      }
      index.set([q * 4, q * 4 + 1, q * 4 + 2, q * 4 + 1, q * 4 + 3, q * 4 + 2], q * 6);
    }
  }
  const g = new T.BufferGeometry();
  g.setAttribute('position', new T.BufferAttribute(pos, 3)); g.setAttribute('cableOther', new T.BufferAttribute(oth, 3)); g.setAttribute('cableCorner', new T.BufferAttribute(cor, 2));
  g.setIndex(new T.BufferAttribute(index, 1)); g.computeBoundingSphere();
  return g;
}

// A hidden one-quad cable (zero size) so the loading warm-up (main.js warmupRender shows every mesh) builds the cable pipeline
// before a streamed location with cables attaches (web/peak-set-pieces.js).
export function cableProxy() {
  const g = new T.BufferGeometry(), z = () => new Float32Array(12);
  g.setAttribute('position', new T.BufferAttribute(z(), 3)); g.setAttribute('cableOther', new T.BufferAttribute(z(), 3)); g.setAttribute('cableCorner', new T.BufferAttribute(new Float32Array(8), 2));
  g.setIndex(new T.BufferAttribute(new Uint32Array([0, 1, 2, 1, 3, 2]), 1));
  const mesh = new T.Mesh(g, cableMaterial()); mesh.visible = false; mesh.frustumCulled = false; mesh.name = 'cable warm-up';
  return mesh;
}

// Lifetime of a location's cable owners (the MultiSpline entity 0x35B418 draws from). sectionsDoc = SECTIONS/sections.json
// (tools/export_sections.py): an owner whose slot-1 program builds the modifier at section activation lives from its section log
// start (action 1) to its leave (3 / 5); the ones built by the tick-0 scan (initial.active_after_tick0_scan) live from race tick 0.
// chunk = the owner instance's texture chunk (0x356298 draws nothing while it is not resident).
export class CableLife {
  constructor(code, sectionsDoc) {
    this.info = new Map(); this.alive = new Map();
    const atStart = new Set(sectionsDoc?.initial?.active_after_tick0_scan || []);
    for (const c of CABLES[code] || []) this.info.set(c.owner, {chunk: undefined, activation: false, atStart: atStart.has(c.owner)});
    for (const x of sectionsDoc?.instances || []) { const i = this.info.get(x.resource); if (i) { i.chunk = Array.isArray(x.chunk) ? x.chunk[1] : undefined; i.activation = !!x.activation; } }
    this.reset();
  }
  reset() { this.alive.clear(); for (const [owner, i] of this.info) if (!i.activation || i.atStart) this.alive.set(owner, true); }
  // Section log actions: 1 = the slot-1 program built the MultiSpline, 3 = the entity destroyed (0x34FD90). 5 (leave) and 4 (enter) of a
  // MultiSpline entity only count its listed instances at modifier +0x34 (0x30A460 / 0x30A3A0 -> 0x35AAE0 / 0x35AAD0; +0x34 is never
  // read): the modifier, its cars and its cable run on.
  section(resource, action) { if (!this.info.has(resource)) return; if (action === 1) this.alive.set(resource, true); else if (action === 3) this.alive.set(resource, false); }
  // modifierActive(owner): the core's own MultiSpline state when it runs one for this owner (undefined otherwise); resident(chunk)
  lives(owner, modifierActive, resident) {
    const i = this.info.get(owner), m = modifierActive?.(owner);
    return (m ?? this.alive.get(owner) ?? !i?.activation) && (i?.chunk === undefined || resident?.(i.chunk) !== false);
  }
}

// A location's cables. load(url) -> JSON; root = the location's package root (rails.json); origin = the scene origin (metres).
// alive(owner) says whether the owner's MultiSpline entity lives (and its chunk is resident); unknown owners count as alive.
export async function createLocationCables({code, root, load, origin}) {
  const list = CABLES[code]; if (!list) return null;
  const rails = await load(root + 'rails.json').catch(() => null); if (!rails) return null;
  const byId = new Map(rails.rails.map((r) => [r.packed_id, r]));
  const group = new T.Group(); group.name = `cables ${code}`; group.userData.cables = true;
  const meshes = [];
  for (const c of list) {
    const rail = byId.get(c.rail); if (!rail) continue;
    const mesh = new T.Mesh(cableGeometry(rail, origin), cableMaterial());
    mesh.name = `cable ${code} ${rail.name ?? c.rail}`; mesh.userData.cableOwner = c.owner; mesh.userData.cableRail = c.rail;
    mesh.matrixAutoUpdate = false; group.add(mesh); meshes.push(mesh);
  }
  if (!meshes.length) return null;
  return {
    group, meshes,
    update(alive) { for (const m of meshes) m.visible = alive ? alive(m.userData.cableOwner) !== false : true; },
    dispose() { for (const m of meshes) m.geometry.dispose(); group.removeFromParent(); },
  };
}
