import * as T from 'three/webgpu';
import { attribute, texture, vec4, select } from 'three/tsl';
import { toFrame } from './frame-space.js';
import { snowBillboardScale } from './snow-billboard.js';
import { drawOrder, EFFECT, SUBMIT } from './ps2-draw-order.js';

// ARA1 start-gate spark fountains (mdl_ARA1_startfirePop_*). Data comes from
// tools/export_startfire.py: LUN program 118 builtin 0x10 profiles, texture 28
// spx2, GS ALPHA 0x48. Particle equations are the snow VU A00 equations
// (engine/snow_particles.cpp) with the Duration>=0 kernel of 0x3705E0/0x36CBF8.
const TICK = 1 / 60;
export function startfireKernel(p) {
  const d = p.Damp,
    half = p.LifeR * 0.5,
    maxLife = p.Life + half,
    minLife = p.Life - half,
    inv = 1 / d;
  const v = (k) => [p[k + 'X'], p[k + 'Y'], p[k + 'Z']],
    c = (k) => [p[k + 'R'], p[k + 'G'], p[k + 'B'], p[k + 'A']];
  const scale = (a, s) => a.map((x) => x * s),
    add = (a, b) => a.map((x, i) => x + b[i]),
    sub = (a, b) => a.map((x, i) => x - b[i]);
  const velocityRanges = ['R0V', 'R1V', 'R2V'].map((k) => scale(v(k), inv)),
    r0 = scale(c('R0'), 128),
    r1 = scale(c('R1'), 128),
    start = scale(c('StartCol'), 128),
    end = scale(c('EndCol'), 128);
  const lifeRange = (maxLife - minLife) * d;
  return {
    count: p.NumParticles,
    // 0x36CBF8 receives Duration (not lifetime) when Duration>=0: the burst's
    // particles are spread over the emission duration in damped time.
    ageStep: (p.Duration * d) / p.NumParticles,
    ageRate: d,
    remaining: p.Duration + p.Life + p.LifeR * 0.5,
    lifeBase: minLife * d - lifeRange,
    lifeRange,
    sizeBase: p.Size - p.SizeR * 1.5,
    sizeRange: p.SizeR,
    sizeDelta: p.SizeFinal - p.Size,
    force: scale(scale(v('Force'), inv), inv),
    positionBase: sub(v('Off'), scale(add(v('R0'), v('R1')), 1.5)),
    positionRanges: [v('R0'), v('R1')],
    velocityRanges,
    velocityBase: sub(scale(v('Vel'), inv), scale(add(add(velocityRanges[0], velocityRanges[1]), velocityRanges[2]), 1.5)),
    colourBase: sub(start, scale(add(r0, r1), 1.5)),
    colourRanges: [r0, r1],
    colourSlope: scale(sub(end, start), 1 / (maxLife * d))
  };
}
const lfsr = (s) => ((((s << 1) ^ ((s >>> 4) & 1) ^ ((s >>> 22) & 1)) & 0x7fffff) | 0x3f800000) >>> 0;
const bitsFloat = new DataView(new ArrayBuffer(4));
const asFloat = (s) => {
  bitsFloat.setUint32(0, s);
  return bitsFloat.getFloat32(0);
};
// Local-frame vectors are transformed by the instance matrix in 0x370058
// (row-major, translation in row 3; direction vectors use w=0).
const rotate = (m, v) => [
  v[0] * m[0] + v[1] * m[4] + v[2] * m[8],
  v[0] * m[1] + v[1] * m[5] + v[2] * m[9],
  v[0] * m[2] + v[1] * m[6] + v[2] * m[10]
];
export function startfireParticles(kernel, emitter, out = []) {
  const k = emitter.kernel;
  let state = emitter.seed;
  for (let i = 0; i < kernel.count; i++) {
    const r = [];
    for (let n = 0; n < 9; n++) {
      state = lfsr(state);
      r.push(asFloat(state));
    }
    const age = emitter.age - i * kernel.ageStep;
    if (age < 0) continue;
    const lifetime = kernel.lifeBase + kernel.lifeRange * r[8];
    if (!(age < lifetime)) continue;
    const velocity = [0, 1, 2].map(
      (a) => k.velocityBase[a] + k.velocityRanges[0][a] * r[2] + k.velocityRanges[1][a] * r[3] + k.velocityRanges[2][a] * r[4]
    );
    const t = Math.min(age, 2.700000047683716),
      poly = t * -0.7300000190734863 + t * t * 0.11299999803304672;
    const position = [0, 1, 2].map(
      (a) =>
        emitter.origin[a] +
        k.positionBase[a] +
        k.positionRanges[0][a] * r[0] +
        k.positionRanges[1][a] * r[1] +
        k.force[a] * age +
        (k.force[a] - velocity[a]) * poly
    );
    const size = Math.abs((kernel.sizeDelta * age) / lifetime + kernel.sizeBase + kernel.sizeRange * r[7]);
    const colour = [0, 1, 2, 3].map((a) => {
      const x = kernel.colourBase[a] + kernel.colourRanges[0][a] * r[5] + kernel.colourSlope[a] * age + kernel.colourRanges[1][a] * r[6];
      return Math.min(255, Math.trunc(Math.max(a < 3 ? Math.min(x, 128) : x, 0)));
    });
    out.push({ position, size, colour });
  }
  return out;
}
export function createStartfireSimulation(meta) {
  const kernel = startfireKernel(meta.profile),
    gateSeconds = meta.inferred.gate_unit_seconds,
    box = [meta.trigger.bounds_min_cm, meta.trigger.bounds_max_cm];
  let triggered = false,
    timer = 0,
    fired = 0,
    emitters = [];
  const localKernel = (m) => ({
    positionBase: rotate(m, kernel.positionBase),
    positionRanges: kernel.positionRanges.map((v) => rotate(m, v)),
    velocityBase: rotate(m, kernel.velocityBase),
    velocityRanges: kernel.velocityRanges.map((v) => rotate(m, v)),
    force: rotate(m, kernel.force)
  });
  const reset = () => {
    triggered = false;
    timer = 0;
    fired = 0;
    emitters = [];
  };
  return {
    kernel,
    reset,
    get triggered() {
      return triggered;
    },
    get emitters() {
      return emitters;
    },
    step(riderCm) {
      const inside = riderCm.every((x, a) => x >= box[0][a] && x <= box[1][a]);
      // Restart/reset uphill of the trigger re-arms the one-shot stage event.
      if (triggered && riderCm[0] > box[1][0] + 1000) reset();
      if (!triggered && inside) {
        triggered = true;
        timer = 0;
      }
      for (const e of emitters) {
        e.age += TICK * kernel.ageRate;
        e.remaining -= TICK;
      }
      emitters = emitters.filter((e) => e.remaining > 0);
      if (!triggered) return;
      timer += TICK;
      while (fired < meta.schedule.length && timer >= meta.schedule[fired].gate * gateSeconds - 1e-6) {
        for (const instance of meta.schedule[fired].instances) {
          const m = instance.matrix;
          // Per-emitter seeds come from 0x36CCB8 (3177F0 stream); a fixed per-instance seed stands in.
          emitters.push({
            rid: instance.rid,
            origin: [m[12], m[13], m[14]],
            kernel: localKernel(m),
            age: 0,
            remaining: kernel.remaining,
            seed: (0x3f800000 | ((instance.rid * 2654435761) >>> 9)) >>> 0
          });
        }
        fired++;
      }
    }
  };
}
export async function createStartfireRenderer(origin, encodedOutput) {
  const meta = await (await fetch('/assets/STARTFIRE/startfire.json')).json(),
    t = meta.texture;
  if (meta.version !== 1 || meta.profile.BlendMode !== 0 || meta.profile.TextureId !== t.id) throw Error('Unexpected startfire package');
  const bytes = new Uint8Array(await (await fetch('/assets/STARTFIRE/' + t.gs_alpha_file)).arrayBuffer());
  if (bytes.length !== t.width * t.height * 4) throw Error('Startfire texture extent');
  const map = new T.DataTexture(bytes, t.width, t.height, T.RGBAFormat);
  map.minFilter = map.magFilter = T.LinearFilter;
  map.colorSpace = T.NoColorSpace;
  map.needsUpdate = true;
  const simulation = createStartfireSimulation(meta),
    capacity = meta.schedule.reduce((n, s) => n + s.instances.length, 0) * simulation.kernel.count;
  const geometry = new T.PlaneGeometry(2, 2);
  for (let v = 0; v < geometry.attributes.uv.count; v++) geometry.attributes.uv.setY(v, 1 - geometry.attributes.uv.getY(v));
  geometry.setAttribute('sparkColour', new T.InstancedBufferAttribute(new Float32Array(capacity * 4), 4).setUsage(T.DynamicDrawUsage));
  const texel = texture(map),
    colour = attribute('sparkColour', 'vec4'),
    encodedColour = texel.rgb.mul(colour.rgb).clamp(0, 1);
  // GS ALPHA 0x48: Cs*As+Cd, MODULATE colour/alpha scale 128.
  const material = new T.MeshBasicNodeMaterial({
    transparent: true,
    depthWrite: false,
    depthTest: true,
    side: T.DoubleSide,
    forceSinglePass: true,
    fog: false,
    toneMapped: false,
    blending: T.AdditiveBlending
  });
  material.fragmentNode = vec4(
    select(encodedOutput, encodedColour, toFrame(encodedColour)),
    texel.a.mul(t.gs_alpha_scale).mul(colour.a).clamp(0, 1)
  );
  const mesh = new T.InstancedMesh(geometry, material, capacity);
  mesh.count = 0;
  mesh.frustumCulled = false;
  mesh.renderOrder = drawOrder(EFFECT.setPieceParticle(28), SUBMIT.setPiece);
  mesh.instanceMatrix.setUsage(T.DynamicDrawUsage);
  const group = new T.Group();
  group.add(mesh);
  group.userData.gameplayOnly = true;
  const view = (await (await fetch('/assets/SNOW_FX/snow-fx.json')).json()).view;
  let serial = null;
  const matrix = new T.Matrix4(),
    position = new T.Vector3(),
    scale = new T.Vector3(),
    viewPosition = new T.Vector3();
  return {
    group,
    simulation,
    update(core, camera) {
      if (!core._rider_state) return;
      // One step per original visual tick; the snow update serial advances once per tick.
      const now = new Float32Array(core.HEAPF32.buffer, core._snow_info(), 23)[20],
        rider = new Float32Array(core.HEAPF32.buffer, core._rider_state(), 3);
      const riderCm = [rider[0] * 100, -rider[2] * 100, rider[1] * 100];
      const steps = serial === null ? 0 : Math.min(Math.max(now - serial, 0), 30);
      serial = now;
      for (let i = 0; i < steps; i++) simulation.step(riderCm);
      let count = 0;
      const colours = geometry.attributes.sparkColour.array;
      camera.updateMatrixWorld();
      for (const e of simulation.emitters)
        for (const p of startfireParticles(simulation.kernel, e)) {
          position.set(p.position[0] / 100 - origin.x, p.position[2] / 100 - origin.y, -p.position[1] / 100 - origin.z);
          viewPosition.copy(position).applyMatrix4(camera.matrixWorldInverse);
          const extent = snowBillboardScale(
            p.size / 100,
            -viewPosition.z,
            camera.projectionMatrix.elements[0],
            camera.projectionMatrix.elements[5],
            view.source_viewport,
            view.max_projected_half_extent
          );
          scale.set(extent[0], extent[1], 1);
          matrix.compose(position, camera.quaternion, scale);
          mesh.setMatrixAt(count, matrix);
          for (let a = 0; a < 4; a++) colours[count * 4 + a] = p.colour[a] / 128;
          count++;
        }
      mesh.count = count;
      // an empty batch hidden (web/snow-renderer.js)
      mesh.visible = count > 0;
      if (count) {
        mesh.instanceMatrix.needsUpdate = true;
        geometry.attributes.sparkColour.needsUpdate = true;
      }
    }
  };
}
