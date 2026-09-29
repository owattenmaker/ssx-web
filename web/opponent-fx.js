// Board trail, wake, snow spray, air streamers and the trick boost's aura for the computer riders. Every computer
// rider's core runs the full rider pipeline, so it already builds the same original trail ribbon (386E78 order), wake
// rows, snow particles, streamers (2EF6D0) and aura (2EADD0) as the human's core; this draws them with one set of the
// human's renderers per rider (web/board-trail.js, web/wake-renderer.js, web/snow-renderer.js, boost-renderer.js
// createRiderFxMeshes). The PS2's rider manager runs the FX pass for every rider: its computer riders show the white
// `strm` streamers under a jump and, holding a trick boost, the purple `prbn` beams and the `psmr` board aura
// (local/ps2-capture/presentation/boostfx, docs/presentation.md). Snow spray and the rider FX join the encoded
// post-rider composite like the human's.
import { Group } from 'three/webgpu';
import { uniform } from 'three/tsl';
import { createBoardTrail } from './board-trail.js';
import { createWakeRenderer } from './wake-renderer.js';
import { createSnowRenderer } from './snow-renderer.js';
import { createRiderFxMaterials, createRiderFxMeshes } from './boost-renderer.js';
import { registerEncodedEffect } from './snow-composite.js';
import { pv } from './pv-flags.js';

export async function createOpponentFx({ scene, origin, count, sampleCore }) {
  // One aura / streamer material set for all computer riders (its own encoded-output switch, set with the others').
  const encodedOutput = uniform(false);
  const fxMaterials = !pv('aiFx') ? null : await createRiderFxMaterials(encodedOutput).catch((e) => { console.warn('Computer rider streamers unavailable', e); return null; });
  const entries = await Promise.all(Array.from({ length: count }, async () => {
    const trail = await createBoardTrail(origin), wake = await createWakeRenderer(origin);
    const snow = await createSnowRenderer(origin, sampleCore, { startfire: false }); // start-gate fire belongs to the course, drawn once
    scene.add(trail.group); scene.add(wake.mesh); scene.add(snow.group);
    registerEncodedEffect({ object: snow.group, setEncodedOutput: (v) => snow.setEncodedOutput(v),
      populated(camera) { let populated = false; snow.group.traverseVisible((o) => { if (o.isInstancedMesh && o.count > 0 && o.material.visible && o.layers.test(camera.layers)) populated = true; }); return populated; } });
    let fx = null, fxGroup = null;
    if (fxMaterials) {
      fxGroup = new Group(); fxGroup.userData.gameplayOnly = true; fx = createRiderFxMeshes(fxGroup, fxMaterials, origin); scene.add(fxGroup);
      registerEncodedEffect({ object: fxGroup, setEncodedOutput: (v) => { encodedOutput.value = !!v; }, populated: () => fxGroup.visible && fx.populated() });
    }
    return { trail, wake, snow, fx, fxGroup };
  }));
  const setVisible = (e, v) => { e.trail.group.visible = v; e.wake.mesh.visible = v; e.snow.group.visible = v; if (e.fxGroup) e.fxGroup.visible = v; };
  return {
    entries,
    // cores[i]: the computer rider in slot i+1 (null hides it); active: computer riders race this run.
    update(cores, camera, active) {
      const debugCore = globalThis.ssxEffects?.core; // board-trail.js records the last core for QA; keep the human's
      entries.forEach((e, i) => {
        const core = active ? cores[i] : null;
        setVisible(e, !!core); if (!core) { e.fx?.clear(); return; }
        e.trail.update(core); e.wake.update(core); e.snow.update(core, camera); e.fx?.update(core);
      });
      if (globalThis.ssxEffects) globalThis.ssxEffects.core = debugCore;
    },
  };
}
