// Segment-wise beam search for the TAS (docs/tas.md "Search"). Node only: the race is tools/tas/race.mjs (the page's race, exact),
// one per worker thread (tools/tas/search-worker.mjs; --workers, at most 3 on the shared machine; run the whole thing with nice).
//
//   nice -n 10 node tools/tas/search.mjs --start START.json --guide GUIDE.json --out DIR [--prefix PAD.tas [--prefix-ticks N]]
//        [--until TICK] [--beam 3] [--cands 16] [--seg 20] [--horizon 180] [--seed 1] [--workers 3] [--weights JSON]
//
// Every segment of --seg ticks, each beam node tries --cands macros (tools/tas/policy.mjs), each followed by --horizon ticks of
// its own continuation (the macro without a new jump or air program); all candidates are valued at the same tick. The --beam best
// distinct ones are re-run and saved. The pad the policy produced is what is kept: DIR/best.tas (tools/tas/pad-format.mjs) and
// DIR/log.jsonl (one row per segment). Booth teleports (Metro City's stage builtin 34, core stage_teleport_info) end a candidate.
//
// Value (metres of course progress at the valuation tick, tools/tas/search-worker.mjs): progress + speed x 1 s + meter x 45 + Uber
// tier x 30 (towards Super Uber: 60 s with the meter locked at 1, about +5 m/s) + Super time x 5 per second - 40 while crashing;
// a finish: 1e7 - race ticks.
import fs from 'node:fs';
import path from 'node:path';
import { Worker } from 'node:worker_threads';
import { fileURLToPath } from 'node:url';
import { parse, serialize } from './pad-format.mjs';
import { DEFAULT_MACRO } from './policy.mjs';
import { candidates, seedRandom } from './macros.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
const args = process.argv.slice(2);
const opt = (k, d) => {
  const i = args.indexOf('--' + k);
  return i < 0 ? d : args[i + 1];
};
const START = opt('start');
const GUIDE = opt('guide');
const OUT = opt('out');
if (!START || !GUIDE || !OUT) throw new Error('usage: search.mjs --start START.json --guide GUIDE.json --out DIR');
const BEAM = +opt('beam', 3);
const CANDS = +opt('cands', 16);
const SEG = +opt('seg', 20);
const HORIZON = +opt('horizon', 180);
const UNTIL = +opt('until', 9000);
const WORKERS = Math.min(3, +opt('workers', 3));
const WEIGHTS = { speed: 1, meter: 45, tier: 30, superTime: 5, crash: 40, ...JSON.parse(opt('weights', '{}')) };
seedRandom(+opt('seed', 1));
fs.mkdirSync(OUT, { recursive: true });
const logPath = path.join(OUT, 'log.jsonl');

// ---- workers ----
const start = JSON.parse(fs.readFileSync(START, 'utf8'));
const guide = JSON.parse(fs.readFileSync(GUIDE, 'utf8'));
const prefix = opt('prefix') ? fs.readFileSync(opt('prefix'), 'utf8') : null;
const prefixTicks = opt('prefix-ticks') ? +opt('prefix-ticks') : undefined;
const workers = [];
let nextId = 1;
const pending = new Map();
function call(w, msg) {
  const id = nextId++;
  return new Promise((resolve) => {
    pending.set(id, resolve);
    w.worker.postMessage({ ...msg, id });
  });
}
for (let k = 0; k < WORKERS; k++) {
  const worker = new Worker(path.join(here, 'search-worker.mjs'));
  const w = { worker, has: new Set() };
  w.readyP = new Promise((r) => (w.ready = r));
  worker.on('message', (m) => {
    if (m.type === 'ready') {
      w.ready(m);
      return;
    }
    const resolve = pending.get(m.id);
    pending.delete(m.id);
    resolve(m);
  });
  worker.on('error', (e) => {
    console.error('worker error', e);
    process.exit(1);
  });
  worker.postMessage({ type: 'init', start, guide, prefix, prefixTicks, seg: SEG, horizon: HORIZON, weights: WEIGHTS });
  workers.push(w);
}
const ready = await Promise.all(workers.map((w) => w.readyP));
if (new Set(ready.map((r) => r.base)).size !== 1) throw new Error('the workers start from different memory');
const prefixFrames = prefix ? parse(prefix).frames.slice(0, ready[0].frames) : [];

// A node for a worker: its save only the first time that worker sees it.
let nodeKey = 1;
function nodeFor(w, n) {
  const send = !w.has.has(n.key);
  w.has.add(n.key);
  return { key: n.key, mem: n.mem, save: send ? n.save : undefined };
}

let beam = [{ key: nodeKey++, save: ready[0].save, mem: ready[0].mem, frames: prefixFrames, macro: { ...DEFAULT_MACRO }, macros: [], info: ready[0].info, v: 0 }];
let best = null;
const t0 = Date.now();
let simTicks = 0;
while (beam.length && beam[0].frames.length < UNTIL && !best) {
  // every (node, macro) pair, spread over the workers; one message per (worker, node)
  const jobs = [];
  for (const node of beam) for (const m of candidates(node.macro, node.info.grounded, node.info.tricky, CANDS)) jobs.push({ node, m });
  const per = Math.ceil(jobs.length / workers.length);
  const results = await Promise.all(workers.map(async (w, k) => {
    const mine = jobs.slice(k * per, (k + 1) * per);
    const out = [];
    const byNode = new Map();
    for (const j of mine) {
      if (!byNode.has(j.node)) byNode.set(j.node, []);
      byNode.get(j.node).push(j);
    }
    for (const [node, list] of byNode) {
      const r = await call(w, { type: 'eval', node: nodeFor(w, node), macros: list.map((j) => j.m) });
      simTicks += r.ticks;
      r.values.forEach((v, i) => out.push({ ...list[i], v }));
    }
    return out;
  }));
  const scored = results.flat().sort((a, b) => b.v - a.v);
  const chosen = [];
  const seen = new Set();
  for (const s of scored) {
    if (chosen.length >= BEAM) break;
    const key = s.node.key + JSON.stringify(s.m);
    if (seen.has(key)) continue;
    seen.add(key);
    chosen.push(s);
  }
  // re-run the chosen ones for their segment and keep their saves
  const next = await Promise.all(chosen.map(async (s, k) => {
    const w = workers[k % workers.length];
    const r = await call(w, { type: 'commit', node: nodeFor(w, s.node), macro: s.m });
    const macros = s.node.macros.concat([{ tick: s.node.frames.length, macro: s.m }]);
    return { key: nodeKey++, save: r.save, mem: r.mem, frames: s.node.frames.concat(r.frames), macro: s.m, macros, info: r.info, v: s.v, finish: r.finish };
  }));
  // the workers forget the old beam
  const old = beam.map((n) => n.key);
  for (const w of workers) {
    w.worker.postMessage({ type: 'forget', keys: old });
    for (const k of old) w.has.delete(k);
  }
  const done = next.find((n) => n.finish && !n.finish.dnf);
  if (done) best = done;
  beam = next.filter((n) => !n.finish);
  const lead = best ?? beam[0];
  if (!lead) break;
  const row = { tick: lead.frames.length, v: +lead.v.toFixed(1), remaining: Math.round(lead.info.remaining), speed: +lead.info.speed.toFixed(2),
    meter: +lead.info.meter.toFixed(3), tier: lead.info.tier, superTime: +lead.info.superTime.toFixed(1), macro: lead.macro,
    secs: Math.round((Date.now() - t0) / 1000), ticksPerSec: Math.round(simTicks / ((Date.now() - t0) / 1000)) };
  fs.appendFileSync(logPath, JSON.stringify(row) + '\n');
  if (row.tick % 200 === 0) console.log(JSON.stringify({ ...row, macro: undefined }));
  if (row.tick % 200 === 0 || best) {
    const head = [`search ${JSON.stringify({ start: START, guide: GUIDE, BEAM, CANDS, SEG, HORIZON, WEIGHTS })}`, `tick ${row.tick} remaining ${row.remaining}`];
    fs.writeFileSync(path.join(OUT, 'best.tas'), serialize(lead.frames, head));
    fs.writeFileSync(path.join(OUT, 'best.macros.json'), JSON.stringify(lead.macros));
  }
}
if (best) {
  console.log(`finish: race ticks ${best.finish.ticks} at run tick ${best.finish.tick}`);
  fs.writeFileSync(path.join(OUT, 'best.tas'), serialize(best.frames, [`search finish race ticks ${best.finish.ticks}`]));
  fs.writeFileSync(path.join(OUT, 'finish.json'), JSON.stringify(best.finish));
  fs.writeFileSync(path.join(OUT, 'best.macros.json'), JSON.stringify(best.macros));
}
for (const w of workers) await w.worker.terminate();
