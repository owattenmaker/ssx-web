// Stage-script calls of cutscene scripts on cutscene sets that carry a LiveComp record: the new-career midway plane
// (tools/export_cutscene_sets.py --plane -> CUTSCENES/SETS/ABC1PLANE; docs/presentation.md "New-career plane") and the
// backcountry helis (--helis -> SETS/<LOC>HELI, bcHeli; docs/presentation.md 16).
//
// PS2: a script's kind-7 object (0x280640) runs its channel-0 point events in the per-tick handler 0x2808E8: the call hash
// is looked up in the stage globals of the current course's location (0x309E50) and that function runs; its cleanup hash
// (unless NoScript 0x049A9AD4) is recorded once and 0x2807B0 runs every recorded cleanup when the step ends. For the plane,
// ABC1 global program 5: 0x0DE99225 (#153 t0) plays builtin 3 frames 0..185 once at 30 fps, 0x0EE53794 (#163 t0) frames
// 186..550, cleanup 0x07D196B4 = SetNodeState 1 (the animation removed: the plane is back at its static frame-0 pose,
// 252 m out). The world's own copy of the instance (ABC1 / PEAK1 packages, baked at frame 0) is the object the PS2
// animates, so it is hidden while the set stands in for it.
import { LiveCompAnimation } from './livecomp-animation.js';

export const NOSCRIPT = 0x049A9AD4;

// meta: the set's world.json; meshes: its meshes, each with userData.liveComp = [resource, node] (or none: static).
export function createStageSet(meta, meshes) {
  const anim = new LiveCompAnimation(meta.livecomp);
  const bySymbol = new Map();
  for (const inst of anim.instances) for (const st of inst.starts || []) if (st.symbol != null) bySymbol.set(st.symbol >>> 0, [inst, st]);
  const cleanups = new Set(), started = new Map();   // resource -> {inst, words, at (script tick of the call), ticks}
  const api = {
    anim,
    // A channel-0 event [call, cleanup] at script tick `at`. Returns whether the call started a player of this set.
    call(symbol, cleanup = NOSCRIPT, at = 0) {
      if (cleanup != null && (cleanup >>> 0) !== NOSCRIPT) cleanups.add(cleanup >>> 0);
      const hit = bySymbol.get(symbol >>> 0); if (!hit) return false;
      anim.start(hit[0], hit[1].words); started.set(hit[0].resource, { inst: hit[0], words: hit[1].words, at, ticks: 0 });
      return true;
    },
    // The entity pass once per 60 Hz script tick since the call: the pose is a function of the script time (a QA seek
    // replays the player from its start).
    sync(t) {
      for (const [res, p] of started) {
        const want = Math.max(0, Math.floor(t) - p.at);
        if (want < p.ticks) { anim.start(p.inst, p.words); p.ticks = 0; }
        const s = anim.state(res); if (!s) continue;
        for (; p.ticks < want; p.ticks++) anim.tick();
      }
    },
    // Per frame: each animated node's delta (rest -> animated, native metres) on its meshes; the rest pose otherwise.
    apply() {
      for (const mesh of meshes) {
        const lc = mesh.userData.liveComp; if (!lc) continue;
        const deltas = anim.nodeDeltas(lc[0]), d = deltas?.[lc[1]];
        mesh.matrixAutoUpdate = false;
        if (d) mesh.matrix.fromArray(d); else mesh.matrix.identity();
        mesh.matrixWorldNeedsUpdate = true;
      }
    },
    // 0x2807B0 at the step end: the recorded cleanups (SetNodeState 1 removes the player of the instance they stop).
    end() {
      for (const c of cleanups) {
        // a cleanup that starts a player (a backcountry heli's 0x0AE69AB4: frames 551..677 in loop mode, the hover after the
        // drop), timed from the next step's clock
        const hit = bySymbol.get(c);
        if (hit) { anim.start(hit[0], hit[1].words); started.set(hit[0].resource, { inst: hit[0], words: hit[1].words, at: 0, ticks: 0 }); continue; }
        for (const inst of anim.instances) if ((inst.starts || []).some((st) => (st.cleanup >>> 0) === c)) { anim.active.delete(inst.resource); started.delete(inst.resource); }
      }
      cleanups.clear(); api.apply();
    },
    reset() { anim.active.clear(); started.clear(); cleanups.clear(); api.apply(); },
  };
  return api;
}

// The world meshes of the set's instance (its static copy in the course / streamed location package): every drawn world
// mesh whose bounds lie within meta.world_copy {centre, radius} (native metres; world geometry keeps native coordinates,
// the mesh sits at -origin). Hidden while the set plays, restored after.
const activeCopies = new Set();   // the copies hidden now: {copy, hidden}
const inCopy = (o, copy) => { const s = o.geometry?.boundingSphere; if (!s) return false; const [cx, cy, cz] = copy.centre;
  return Math.hypot(s.center.x - cx, s.center.y - cy, s.center.z - cz) + s.radius <= copy.radius; };
export function hideWorldCopy(scene, meta, exclude) {
  const copy = meta?.world_copy; if (!copy || !scene) return () => {};
  const hidden = [], entry = {copy, hidden};
  scene.traverse((o) => {
    if (!o.isMesh || !o.visible || o === exclude || isInside(o, exclude)) return;
    if (!o.geometry?.boundingSphere || o.parent?.isSkinnedMesh) return;
    if (inCopy(o, copy)) { o.visible = false; hidden.push(o); }
  });
  activeCopies.add(entry);
  return () => { for (const o of hidden) o.visible = true; hidden.length = 0; activeCopies.delete(entry); };
}
// A world mesh put into the scene while a set stands in for its copy (web/set-pieces-renderer.js, heliWorld): hidden with the
// copy and shown again when the set leaves.
export function adoptWorldCopy(o) {
  for (const {copy, hidden} of activeCopies) if (inCopy(o, copy)) { o.visible = false; hidden.push(o); return true; }
  return false;
}
function isInside(o, group) { for (let p = o.parent; p; p = p.parent) if (p === group) return true; return false; }
