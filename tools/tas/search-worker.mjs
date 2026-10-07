// A search worker (tools/tas/search.mjs --workers N): its own race (tools/tas/race.mjs, built from the same start state, so its
// start memory is the same bytes as every other worker's), evaluating macros from branch points the main thread sends.
//   {type: 'init', start, guide, prefix, horizon, seg, weights}       -> {type: 'ready', base}  (base: a hash of the start memory)
//   {type: 'eval', id, node: {key, save?, mem}, macros}               -> {type: 'values', id, values}
//   {type: 'commit', id, node: {key, save?, mem}, macro}              -> {type: 'committed', id, save, mem, frames, finish, info}
// A node's save is sent once per worker and kept by its key until {type: 'forget', keys}.
import { parentPort } from 'node:worker_threads';
import { createTasRace } from './race.mjs';
import { parse, toChannels } from './pad-format.mjs';
import { makeGuide } from './guide.mjs';
import { DEFAULT_MACRO, cloneMemory, newPolicyMemory, policyFrame } from './policy.mjs';

let race = null;
let guide = null;
let cfg = null;
let TOTAL = 0;
const saves = new Map();
const f32 = (p, n) => new Float32Array(race.core.HEAPF32.buffer, p, n);
const boostInfo = () => f32(race.core._boost_info(), 6);
const teleports = () => (race.core._stage_teleport_info ? f32(race.core._stage_teleport_info(), 1)[0] : 0);
const crashing = () => f32(race.core._crash_info(), 1)[0] !== 0;

// The value of a state in metres of course progress (tools/tas/search.mjs explains the weights); a finish beats everything.
function value(finish) {
  if (finish) return 1e7 - finish.ticks;
  const W = cfg.weights;
  const p = race.progress();
  const s = race.state();
  const b = boostInfo();
  const tier = Math.min(b[3], 10);
  let v = (TOTAL - p[0]) / 100;
  v += W.speed * s[7];
  v += W.meter * b[0];
  v += W.tier * tier;
  if (tier >= 10) v += W.superTime * b[5];
  if (crashing()) v -= W.crash;
  return v;
}

function step(mem, macro, frames) {
  const f = policyFrame(guide, macro, mem, race.state(), boostInfo());
  if (frames) frames.push(f);
  return race.tick(toChannels(f)).finish;
}

const continuation = (m) => ({ ...m, jump: false, pre: null, program: null, cancel: false });

function load(node) {
  if (node.save) saves.set(node.key, node.save);
  const s = saves.get(node.key);
  if (!s) throw new Error(`worker has no save for node ${node.key}`);
  race.restore(s);
}

function info() {
  const b = boostInfo();
  const s = race.state();
  return { grounded: !!s[8], tricky: b[5] > 0, remaining: race.progress()[0], speed: s[7], meter: b[0], tier: b[3], superTime: b[5] };
}

parentPort.on('message', async (msg) => {
  if (msg.type === 'init') {
    cfg = msg;
    guide = makeGuide(msg.guide.points);
    race = await createTasRace({ start: msg.start });
    TOTAL = race.progress()[0];
    const mem = newPolicyMemory();
    const frames = msg.prefix ? parse(msg.prefix).frames.slice(0, msg.prefixTicks ?? Infinity) : [];
    for (const f of frames) {
      policyFrame(guide, DEFAULT_MACRO, mem, race.state(), boostInfo());
      race.tick(toChannels(f));
    }
    const save = race.save();
    let h = 0x811c9dc5;
    for (const [at, c] of save.chunks) {
      h = Math.imul(h ^ at, 16777619) >>> 0;
      for (let i = 0; i < c.length; i += 64) h = Math.imul(h ^ c[i], 16777619) >>> 0;
    }
    parentPort.postMessage({ type: 'ready', base: h, save, mem, frames: frames.length, info: info() });
    return;
  }
  if (msg.type === 'forget') {
    for (const k of msg.keys) saves.delete(k);
    return;
  }
  if (msg.type === 'eval') {
    const values = [];
    for (const m of msg.macros) {
      load(msg.node);
      const tele = teleports();
      const mem = cloneMemory(msg.node.mem);
      let fin = null;
      let ok = true;
      for (let k = 0; k < cfg.seg && !fin; k++) fin = step(mem, m, null);
      const c = continuation(m);
      for (let k = 0; !fin && ok && k < cfg.horizon; k++) {
        fin = step(mem, c, null);
        if (k % 10 === 0 && teleports() !== tele) ok = false;
      }
      if (teleports() !== tele) ok = false;
      values.push(ok ? value(fin) : -1e9);
    }
    parentPort.postMessage({ type: 'values', id: msg.id, values, ticks: msg.macros.length * (cfg.seg + cfg.horizon) });
    return;
  }
  if (msg.type === 'commit') {
    load(msg.node);
    const mem = cloneMemory(msg.node.mem);
    const frames = [];
    let fin = null;
    for (let k = 0; k < cfg.seg && !fin; k++) fin = step(mem, msg.macro, frames);
    parentPort.postMessage({ type: 'committed', id: msg.id, save: race.save(), mem, frames, finish: fin, info: info() });
  }
});
