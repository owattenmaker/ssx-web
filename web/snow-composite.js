import { RenderTarget, UnsignedByteType, MeshBasicNodeMaterial, QuadMesh, AlwaysDepth, DataTexture, RGBAFormat } from 'three/webgpu';
import { texture, screenUV, vec4, passTexture, uniform, select } from 'three/tsl';
import { fromFrame } from './frame-space.js';
// The composite's scene render right after the world pass keeps the world pass's matrices (nothing moves between the two);
// `matrixPass.once` switches it off in a page (QA).
export const matrixPass = { once: true };

// Post-rider GS effects blended in encoded 0..255 space, like the original GS.
// Each entry: {object, setEncodedOutput(bool), populated()}. Wake (650), boost
// strips (660..662) and snow (700..709) keep their source draw order because
// the encoded pass renders them together, sorted by renderOrder.
const encodedEffects = new Set();
const useEncodedLayer = (object) =>
  object.traverse((o) => {
    if (o.isMesh) o.layers.enable(1);
  });
export function registerEncodedEffect(effect) {
  if (!effect?.object || typeof effect.setEncodedOutput !== 'function' || typeof effect.populated !== 'function')
    throw Error('Invalid encoded effect');
  useEncodedLayer(effect.object);
  encodedEffects.add(effect);
  return effect;
}
// Course change (main.js unloadCourse): drop the effects whose objects left the scene with the old course.
export function pruneEncodedEffects(keep) {
  for (const e of [...encodedEffects]) if (!keep(e)) encodedEffects.delete(e);
  return encodedEffects.size;
}
const snowEffect = (snow) => ({
  object: snow.group,
  setEncodedOutput: (v) => snow.setEncodedOutput(v),
  populated(camera) {
    let populated = false;
    snow.group.traverseVisible((o) => {
      if (o.isInstancedMesh && o.count > 0 && o.material.visible && o.layers.test(camera.layers)) populated = true;
    });
    return populated;
  }
});
const effectsFor = (snow) => [...(snow ? [snowEffect(snow)] : []), ...encodedEffects];

// Blend the effects into one encoded destination, retaining opaque scene depth.
// seed: optional encoded 0..1 destination (fog-renderer passes the fogged world: the
// original draws these effects at priority 7, after the 36AC00 fog composite).
export function createEncodedSnowComposite(renderer, sourceTarget, scene, camera, snow, seed = null) {
  const target = new RenderTarget(1, 1, { type: UnsignedByteType, depthBuffer: true });
  const material = new MeshBasicNodeMaterial({ depthTest: true, depthWrite: true, depthFunc: AlwaysDepth, toneMapped: false, fog: false });
  material.fragmentNode = vec4(seed ?? fromFrame(texture(sourceTarget.texture, screenUV).rgb).clamp(0, 1), 1);
  if (!sourceTarget.depthTexture) throw Error('Encoded snow requires scene depth');
  material.depthNode = texture(sourceTarget.depthTexture, screenUV).r;
  const quad = new QuadMesh(material);
  if (snow) useEncodedLayer(snow.group);
  const seed0 = material.fragmentNode;
  return {
    target,
    // PS2 softness (web/fog-renderer.js setSoftness): the destination seeded with another fogged world, or back to the first (null)
    useSeed(seed = null) {
      material.fragmentNode = seed ? vec4(seed, 1) : seed0;
      material.needsUpdate = true;
    },
    render(visible = true) {
      target.setSize(sourceTarget.width, sourceTarget.height);
      const effects = effectsFor(snow),
        priorTarget = renderer.getRenderTarget(),
        priorClear = renderer.autoClear,
        priorMask = camera.layers.mask,
        background = scene.background,
        backgroundNode = scene.backgroundNode,
        autoWorld = scene.matrixWorldAutoUpdate;
      try {
        renderer.setRenderTarget(target);
        renderer.autoClear = true;
        quad.render(renderer);
        if (visible) {
          renderer.autoClear = false;
          camera.layers.set(1);
          scene.background = null;
          scene.backgroundNode = null;
          for (const e of effects) e.setEncodedOutput(true);
          if (matrixPass.once) scene.matrixWorldAutoUpdate = false;
          renderer.render(scene, camera);
        }
      } finally {
        scene.matrixWorldAutoUpdate = autoWorld;
        for (const e of effects) e.setEncodedOutput(false);
        scene.background = background;
        scene.backgroundNode = backgroundNode;
        camera.layers.mask = priorMask;
        renderer.autoClear = priorClear;
        renderer.setRenderTarget(priorTarget);
      }
    },
    dispose() {
      target.dispose();
      material.dispose();
    }
  };
}
export function attachEncodedSnowComposite(renderer, worldPass, scene, camera, snow, seed = null) {
  const composite = createEncodedSnowComposite(renderer, worldPass.renderTarget, scene, camera, snow, seed),
    update = worldPass.updateBefore.bind(worldPass);
  // A real initialized fallback avoids binding an unwritten render target on
  // frames before the first particle. The target is only sized/drawn when used.
  const empty = new DataTexture(new Uint8Array([0, 0, 0, 255]), 1, 1, RGBAFormat);
  empty.needsUpdate = true;
  const enabled = uniform(false),
    colour = passTexture(worldPass, empty),
    stats = { renders: 0, skipped: 0 };
  worldPass.updateBefore = (frame) => {
    const effects = effectsFor(snow),
      visible = effects.map((e) => e.object.visible);
    let populated = false;
    effects.forEach((e, i) => {
      if (visible[i] && e.populated(camera)) populated = true;
      e.object.visible = false;
    });
    try {
      update(frame);
    } finally {
      effects.forEach((e, i) => {
        e.object.visible = visible[i];
      });
    }
    enabled.value = populated;
    if (enabled.value) {
      composite.render(true);
      colour.value = composite.target.texture;
      stats.renders++;
    } else {
      colour.value = empty;
      stats.skipped++;
    }
  };
  const ordinary = worldPass.getTextureNode().sample(screenUV);
  return {
    encodedColour: select(enabled, colour.sample(screenUV).rgb, seed ?? fromFrame(ordinary.rgb)),
    stats,
    // another seed's encoded colour (only built when asked: the default graph is untouched) and the composite switched to it
    seeded(other) {
      return select(enabled, colour.sample(screenUV).rgb, other);
    },
    useSeed: (other) => composite.useSeed(other),
    dispose() {
      composite.dispose();
      empty.dispose();
    }
  };
}
