// Remote racers' rider effects (board track, snow spray / wake / breath / kicker / body snow, boost, board sparks,
// fist sparkle): the original draws every rider's RFX container (docs/terrain-render-fidelity.md "rider+0x77C"),
// so each remote racer gets a puppet core here (web/fx_puppet.inc) that replays the FX inputs its own client recorded
// and streamed (web/net/rider-packet.js FX records), and a set of the human's effect renderers reading that core.
// The effects are stepped once per streamed FX pass, DELAY ticks behind like the drawn pose, and moved by the same
// dead-reckoning correction as the body (web/net/remote-riders.js), so the track leaves the drawn board.
//
// Renderer sets are pooled for the page: snow-composite.js keeps every registered encoded effect for the page, so the
// sets are created once and reused by later races.
import createCore from '../runtime/core.js';
import { createOpponentFx } from '../opponent-fx.js';
import { createBoostRenderer } from '../boost-renderer.js';
import { FX_FIELDS } from './rider-packet.js';

const text = (core, s) => { const b = new TextEncoder().encode(s + '\0'), p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
const bytesIn = (core, b) => { const p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };

// One puppet core: the remote rider's package + settings (its FX profiles, trail bones, snow emitters) and this
// course's environment (the track / wake colour and Lighting painter).
export async function createFxPuppet({ riderText, settingsText, packetsJson, packetsBin, environment }) {
  const core = await createCore(), temp = [];
  const t = (s) => { const p = text(core, s); temp.push(p); return p; }, b = (x) => { const p = bytesIn(core, x); temp.push(p); return p; };
  try {
    core._init_animation(t(packetsJson), t(riderText), t(settingsText), b(packetsBin), packetsBin.length);
    if (environment) core._init_environment(t(JSON.stringify(environment.meta)), b(environment.bytes), environment.bytes.length);
    core._reset_animation(); core._fx_puppet_reset(1);
  } catch (e) { throw e instanceof Error ? e : new Error(core.getExceptionMessage ? core.getExceptionMessage(e).join(': ') : String(e)); }
  finally { for (const p of temp) core._free(p); }
  const record = core._malloc(FX_FIELDS * 4), pose = core._malloc(64 * 7 * 4);
  let steps = 0;
  return {
    core,
    get steps() { return steps; },
    reset(full = true) { core._fx_puppet_reset(full ? 1 : 0); },
    // One streamed FX pass: its record, the rider's world pose at that tick and the reset the sender made before
    // it (1: a reset placement 111890, 2: a rescue / new run -- as the sender's core, before that tick's FX pass).
    step(values, poseFloats, reset = 0) {
      if (reset) core._fx_puppet_reset(reset === 2 ? 1 : 0);
      core.HEAPF32.set(values, record >> 2); core.HEAPF32.set(poseFloats, pose >> 2);
      core._fx_puppet_step(record, pose, poseFloats.length / 7); steps++;
    },
  };
}

// The effect renderers of the remote riders, pooled for the page: the computer riders' module (web/opponent-fx.js:
// one set of the human's track / wake / snow renderers per rider, snow in the encoded composite) plus a boost
// renderer per rider (the original draws every rider's boost, RFX+0x610). A set is in the scene only while a remote
// racer uses it (main.js shows every gameplayOnly object while racing).
let renderPool = null;
async function rendererPool({ scene, origin, core }) {
  renderPool ??= (async () => {
    const saved = globalThis.ssxEffects ? { ...globalThis.ssxEffects } : null; // keep the human's QA handles
    const fx = await createOpponentFx({ scene, origin, count: 5, sampleCore: core });
    const boosts = await Promise.all(fx.entries.map(() => createBoostRenderer(origin)));
    if (saved) globalThis.ssxEffects = saved; else delete globalThis.ssxEffects;
    return fx.entries.map((e, i) => {
      const objects = [e.trail.group, e.wake.mesh, e.snow.group, boosts[i].group];
      for (const o of objects) scene.remove(o);
      return {
        attach() { for (const o of objects) { if (!o.parent) scene.add(o); o.visible = true; } },
        detach() { for (const o of objects) scene.remove(o); },
        // offset: source-cm correction of the drawn rider (remote-riders.js) -> scene metres (x, z up, -y).
        update(puppet, camera, offset) {
          const debugCore = globalThis.ssxEffects?.core;
          e.trail.update(puppet); e.wake.update(puppet); e.snow.update(puppet, camera); boosts[i].update(puppet);
          if (globalThis.ssxEffects) globalThis.ssxEffects.core = debugCore;
          for (const o of objects) o.position.set(offset[0] / 100, offset[2] / 100, -offset[1] / 100);
        },
      };
    });
  })();
  return renderPool;
}

// Per race: puppets for the remote slots, renderers from the pool; call step(remote, tick) each frame.
export async function createRemoteFx({ T, scene, origin, load, entries, environment, settingsFor }) {
  const get = async (path) => { const r = await fetch(path); if (!r.ok) throw new Error(`${path}: ${r.status}`); return r.text(); };
  const [packetsJson, packetsBin] = await Promise.all([get('/assets/ANIMATIONS/animation-packets.json'), load('/assets/ANIMATIONS/animation-packets.bin', 'buffer')]);
  const packets = new Uint8Array(packetsBin), racers = new Map();
  for (const [i, e] of entries.entries()) {
    // The rider's own package (an outfit's generated one is served by main.js load() from wardrobe.js).
    const [riderText, settingsText] = await Promise.all([e.root ? load(`${e.root}rider.json`).then((j) => JSON.stringify(j)) : get(`/assets/${e.pkg}/rider.json`), settingsFor(e)]);
    const puppet = await createFxPuppet({ riderText, settingsText, packetsJson, packetsBin: packets, environment });
    const pool = await rendererPool({ scene, origin, core: puppet.core });
    racers.set(e.slot, { puppet, renderers: pool[i], next: -1, pending: [] });
    await new Promise((r) => setTimeout(r, 0)); // keep the loading screen drawing
  }
  const stats = { steps: 0, late: 0 };
  return {
    stats,
    // A decoded state packet (remote-riders.js receive): queue its FX records with their ticks and the pose to use.
    receive(s) {
      const r = racers.get(s.slot); if (!r || !s.fx) return;
      for (const x of s.fx) r.pending.push({ tick: s.tick - x.back, values: x.values, reset: x.reset });
      if (r.pending.length > 240) r.pending.splice(0, r.pending.length - 240);
    },
    // Step every puppet up to the drawn tick and draw its effects. poseAt(slot, tick) -> world pose floats.
    frame(remote, drawnTick, camera) {
      for (const [slot, r] of racers) {
        const racer = remote.racers.get(slot);
        if (!racer?.latest || racer.hidden) { r.renderers.detach(); continue; }
        while (r.pending.length && r.pending[0].tick <= drawnTick) {
          const x = r.pending.shift(); if (x.tick <= r.next) continue;
          const pose = remote.poseAt(slot, x.tick); if (!pose) continue;
          r.puppet.step(x.values, pose, x.reset); r.next = x.tick; stats.steps++;
        }
        r.renderers.attach();
        r.renderers.update(r.puppet.core, camera, racer.offset);
      }
    },
    dispose() { for (const r of racers.values()) r.renderers.detach(); racers.clear(); },
  };
}
