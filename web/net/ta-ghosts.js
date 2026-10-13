// The ghosts of a time-attack session on the page (pv ghostRiders; docs/big-multiplayer.md 2.5, 3, 4), owned by web/net/ta-game.js.
//   send     during an attempt, 20 Hz: the local rider's world pose as a ghost pose packet (web/server/ghost-packet.mjs kind 5)
//   receive  the server's bundles (kind 11): poses of the nearest players, lite entries and dots of the others
//   frame    every animation frame: each ghost DELAY_MS behind on the server clock, interpolated between its poses (never across a
//            serial change: a restart or a reset teleports), or its last pose moved to its newest lite / dot position; drawn nearest
//            first by web/ghost-riders.js (at most its cap)
// Display only: nothing here touches the local rider's simulation.
import { encodePose, decodeBundle, bonePositions, freeBones, POSE, LITE, DOT, FLAG_RIDING } from '../server/ghost-packet.mjs';
import { createGhostRiders } from '../ghost-riders.js';

export const SEND_MS = 50;
export const DELAY_MS = 300;
const KEEP = 8;

function nlerpQuats(a, b, f, out) {
  for (let i = 0; i < a.length; i += 4) {
    const sign = a[i] * b[i] + a[i + 1] * b[i + 1] + a[i + 2] * b[i + 2] + a[i + 3] * b[i + 3] < 0 ? -1 : 1;
    let n = 0;
    for (let k = 0; k < 4; k++) {
      out[i + k] = a[i + k] + (sign * b[i + k] - a[i + k]) * f;
      n += out[i + k] * out[i + k];
    }
    n = Math.sqrt(n) || 1;
    for (let k = 0; k < 4; k++) out[i + k] /= n;
  }
  return out;
}

// ui: the page's ui (ui.rider: the local rider's package); getCore(): the local rider's core; members(): Map index -> roster member
// (its pkg); serverNow(): the server clock; riding(): an attempt is on (send).
// rider(): the page's selected rider (web/net/mp-game.js: its package is the ghost's model).
export function createTaGhosts({ T, scene, load, loader, origin, getCore, camera = () => null, rider = () => null, client, ui, members, serverNow, riding,
  attempt = () => 0 }) {
  const renderer = createGhostRiders({ T, scene, load, loader, origin });
  const ghosts = new Map();
  const stats = { sent: 0, sentBytes: 0, bundles: 0, received: 0, drawn: 0, frameMs: 0 };
  let localRig = null;
  let localPkg = null;
  let qa = 0;
  let raf = 0;

  // ---- send ----
  async function rigOf(pkg) {
    return renderer.rig(pkg);
  }
  function capture() {
    const core = getCore();
    if (!core?._world_pose_bones || !riding()) return;
    const pkg = rider()?.package;
    if (pkg && pkg !== localPkg) {
      localPkg = pkg;
      localRig = null;
      rigOf(pkg).then((r) => {
        if (localPkg === pkg) localRig = r;
      });
    }
    if (!localRig) return;
    const f32 = (p, n) => new Float32Array(core.HEAPF32.buffer, p, n);
    const at = core._world_pose_bones();
    const bones = f32(at, 1)[0];
    if (!bones || bones !== localRig.bones.length) return;
    const pose = f32(at + 4, bones * 7).slice();
    const scale = Array.from(f32(core._rider_skin_scale(), 3));
    const pair = core._pair_view ? f32(core._pair_view(), 140) : null;
    const resets = (f32(core._reset_info(), 3)[2] + f32(core._rider_state(), 16)[13]) & 255;
    const bytes = encodePose({ flags: FLAG_RIDING, serial: ((attempt() & 255) << 8) | resets, time: serverNow(), pose, scale,
      velocity: pair ? [pair[87], pair[88], pair[89]] : [0, 0, 0], free: freeBones(localRig.bones), rigBones: localRig.bones });
    if (client.sendState(bytes)) {
      stats.sent++;
      stats.sentBytes += bytes.length;
    }
  }
  const sender = setInterval(() => {
    try {
      capture();
    } catch (e) {
      console.warn('ta: ghost capture failed', e);
    }
  }, SEND_MS);

  // ---- receive ----
  function ghostOf(index) {
    let g = ghosts.get(index);
    if (!g) {
      g = { index, pkg: null, rig: null, poses: [], lite: null, dot: null, positions: null, quats: null };
      ghosts.set(index, g);
    }
    const m = members().get(index);
    if (m && m.pkg !== g.pkg) {
      g.pkg = m.pkg;
      g.rig = null;
      rigOf(m.pkg).then((r) => {
        if (g.pkg === m.pkg) g.rig = r;
      });
    }
    return g;
  }
  function receive(frame) {
    const b = decodeBundle(frame);
    if (!b) return false;
    stats.bundles++;
    for (const e of b.entries) {
      stats.received++;
      const g = ghostOf(e.player);
      if (e.kind === POSE) {
        g.poses.push(e.value);
        if (g.poses.length > KEEP) g.poses.shift();
      } else if (e.kind === LITE) g.lite = e.value;
      else if (e.kind === DOT) g.dot = e.value;
    }
    return true;
  }

  // ---- frame ----
  // t: the server clock's low 32 bits (the packets carry those): differences as signed 32-bit integers, so a wrap is harmless
  function poseAt(g, t) {
    const s = g.poses;
    if (!s.length || !g.rig || s[s.length - 1].bones !== g.rig.bones.length) return null;
    const back = (x) => (t - x.time) | 0;
    let a = s[0];
    let b = s[0];
    for (let i = s.length - 1; i >= 0; i--) {
      if (back(s[i]) >= 0) {
        a = s[i];
        b = s[i + 1] ?? s[i];
        break;
      }
    }
    const f = b !== a && b.serial === a.serial ? Math.min(1, Math.max(0, back(a) / Math.max(1, (b.time - a.time) | 0))) : 0;
    g.quats = nlerpQuats(a.quats, b.quats, f, g.quats?.length === a.quats.length ? g.quats : new Float64Array(a.quats.length));
    const mix = { ...a, root: a.root.map((x, k) => x + (b.root[k] - x) * f), quats: g.quats };
    g.positions = bonePositions(mix, g.rig.bones, g.positions?.length === a.bones * 3 ? g.positions : new Float64Array(a.bones * 3));
    // a newer lite / dot than the poses (a mid or far ghost): the last pose moved there
    const newest = s[s.length - 1];
    const moved = g.lite && g.lite.serial === newest.serial ? g.lite.root : g.dot ? g.dot.root : null;
    if (moved && f === 0 && a === newest) {
      for (let i = 0; i < g.positions.length; i += 3) for (let k = 0; k < 3; k++) g.positions[i + k] += moved[k] - newest.root[k];
    }
    return { pkg: g.pkg, positions: g.positions, quats: g.quats, scale: a.scale, at: g.positions };
  }
  function frame() {
    raf = requestAnimationFrame(frame);
    const t0 = performance.now();
    const core = getCore();
    const here = core?._world_pose_bones ? new Float32Array(core.HEAPF32.buffer, core._world_pose_bones() + 4, 3) : [0, 0, 0];
    const list = [];
    if (qa) list.push(...qaGhosts(core));
    else {
      const t = (serverNow() - DELAY_MS) >>> 0;
      for (const g of ghosts.values()) {
        const p = poseAt(g, t);
        if (p) list.push(p);
      }
    }
    const d = (p) => (p.at[0] - here[0]) ** 2 + (p.at[1] - here[1]) ** 2 + (p.at[2] - here[2]) ** 2;
    list.sort((a, b) => d(a) - d(b));
    renderer.update(list);
    stats.drawn = renderer.stats.drawn;
    stats.frameMs = performance.now() - t0;
  }
  raf = requestAnimationFrame(frame);

  // ---- QA (the Safari budget, docs/big-multiplayer.md 4): n ghosts copying the local rider's pose around it ----
  const QA_PACKAGES = ['RIDER_ZOE', 'RIDER_PSYMON', 'RIDER_MAC', 'RIDER_MOBY', 'RIDER_ELISE', 'RIDER_KAORI', 'RIDER_GRIFF', 'RIDER_ALLEGRA'];
  const qaRigs = new Map();
  function qaGhosts(core) {
    if (!core?._world_pose_bones) return [];
    const at = core._world_pose_bones();
    const bones = new Float32Array(core.HEAPF32.buffer, at, 1)[0];
    if (!bones) return [];
    const pose = new Float32Array(core.HEAPF32.buffer, at + 4, bones * 7);
    const out = [];
    for (let i = 0; i < qa; i++) {
      const pkg = QA_PACKAGES[i % QA_PACKAGES.length];
      if (!qaRigs.has(pkg)) {
        qaRigs.set(pkg, null);
        rigOf(pkg).then((r) => qaRigs.set(pkg, r));
      }
      const rig = qaRigs.get(pkg);
      if (!rig) continue;
      // the local rider's bone rotations on this rig (bone counts differ between packages: the shared prefix, then the identity)
      const quats = new Float64Array(rig.bones.length * 4);
      for (let b = 0; b < rig.bones.length; b++) {
        if (b < bones) for (let k = 0; k < 4; k++) quats[b * 4 + k] = pose[b * 7 + 3 + k];
        else quats[b * 4 + 3] = 1;
      }
      const ring = 300 + 250 * Math.floor(i / 8);
      const angle = (i % 8) * (Math.PI / 4) + Math.floor(i / 8) * 0.4;
      const root = [pose[0] + ring * Math.cos(angle), pose[1] + ring * Math.sin(angle), pose[2]];
      const free = new Map();
      for (const b of freeBones(rig.bones)) if (b < bones) free.set(b, [0, 1, 2].map((k) => pose[b * 7 + k] - pose[k]));
      const positions = bonePositions({ bones: rig.bones.length, root, quats, scale: [1, 1, 1], free }, rig.bones);
      out.push({ pkg, positions, quats, scale: [1, 1, 1], at: positions });
    }
    return out;
  }

  return {
    stats,
    renderer,
    receive,
    // a player left: its ghost goes
    drop(index) {
      ghosts.delete(index);
    },
    clear() {
      ghosts.clear();
    },
    // QA: the sender's and the receivers' state
    inspect() {
      const core = getCore();
      const coreBones = core?._world_pose_bones ? new Float32Array(core.HEAPF32.buffer, core._world_pose_bones(), 1)[0] : null;
      return { localPkg, localRig: !!localRig, localBones: localRig?.bones.length ?? null, coreBones, riding: riding(), now: serverNow(),
        ghosts: [...ghosts.values()].map((g) => ({ index: g.index, pkg: g.pkg, rig: !!g.rig, rigBones: g.rig?.bones.length ?? null, poses: g.poses.length,
          bones: g.poses.at(-1)?.bones ?? null, time: g.poses.at(-1)?.time ?? null })) };
    },
    // QA: where the first drawn ghost of each package lands on screen (normalized device coordinates) with this frame's camera
    debug() {
      const cam = camera();
      const d = renderer.debug();
      if (!cam) return { ...d, ndc: null };
      const ndc = d.packages.map((p) => {
        const v = new T.Vector4(p.m0[12] * 0.01 - d.origin[0], p.m0[14] * 0.01 - d.origin[1], -p.m0[13] * 0.01 - d.origin[2], 1);
        v.applyMatrix4(cam.matrixWorldInverse).applyMatrix4(cam.projectionMatrix);
        return [v.x / v.w, v.y / v.w, v.z / v.w, v.w];
      });
      return { ...d, ndc, camera: cam.position.toArray() };
    },
    set qa(n) {
      qa = Math.max(0, n | 0);
    },
    get qa() {
      return qa;
    },
    dispose() {
      clearInterval(sender);
      cancelAnimationFrame(raf);
      renderer.dispose();
      ghosts.clear();
    }
  };
}
