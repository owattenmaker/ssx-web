// Page side of web/bench-sim-browser.mjs (docs/sim-performance.md "Simulation core, round 3"). QA only: the game never imports it.
// web/bench-sim.mjs's six-rider Snow Jam tick (phone-like reads) for several core builds in one page, in alternating 50-tick
// chunks, so machine load hits them alike. Query: ?specs=K[:fast][@A],K...&ticks=2000&chunk=50, where K / A index the core
// directories / ai-racers.js files the driver serves as /__bench/core/K/ and /__bench/ai/A.js.
const q = new URL(location.href).searchParams;
const TICKS = +(q.get('ticks') || 2000), CHUNK = +(q.get('chunk') || 50), READ_FX = 4, SOLO = q.get('solo') === '1'; // solo: the human alone (ai=0, free roam)
const specs = (q.get('specs') || '').split(',').filter(Boolean);
const A = (p) => '/assets/' + p;
const text = async (p) => (await fetch(A(p))).text(), bin = async (p) => new Uint8Array(await (await fetch(A(p))).arrayBuffer());
async function race(spec) {
  const [coreSpec, ai] = spec.split('@'); const [name, mode] = coreSpec.split(':');
  const createCore = (await import(`/__bench/core/${name}/core.js`)).default;
  const { createAiRacers } = await import(ai ? `/__bench/ai/${ai}.js` : '/ai-racers.js');
  const human = await createCore({ locateFile: (f) => `/__bench/core/${name}/${f}` });
  const course = 'ARA1', enc = new TextEncoder();
  const put = (bytes) => { const p = human._malloc(bytes.length); human.HEAPU8.set(bytes, p); return p; }, str = (s) => put(enc.encode(s + '\0'));
  const resources = {
    packetsJson: await text('ANIMATIONS/animation-packets.json'),
    packetsBin: await bin('ANIMATIONS/animation-packets.bin'),
    initialText: await text('ANIMATIONS/initial.json'),
    collision: await bin(`${course}/collision.bin`),
    terrainText: await text(`${course}/terrain.json`),
    worldCollisionText: await text(`${course}/world_collision.json`),
    railsText: await text(`${course}/rails.json`),
    riderText: {}
  };
  resources.terrainHash = JSON.parse(resources.terrainText).source_sha256;
  const doc = JSON.parse(await text(`${course}/npc-riders.json`)); for (const r of doc.riders) resources.riderText[r.package] = await text(`${r.package}/rider.json`);
  human._init_animation(str(resources.packetsJson), str(await text('RIDER_ZOE/rider.json')), str(resources.initialText), put(resources.packetsBin), resources.packetsBin.length);
  human._init_race(str(resources.initialText)); human._animation_use_physics(1); human._init_world(put(resources.collision), resources.collision.length / 4);
  const hash = str(resources.terrainHash);
  human._init_terrain(str(resources.terrainText));
  human._init_world_collision(str(resources.worldCollisionText), hash);
  human._init_body_terrain(str(resources.terrainText));
  human._init_rails(str(resources.railsText), hash);
  const racers = SOLO ? { npcs: [], start() {}, beginTick() {}, endTick() { human._fx_pass?.(-1); } } : await createAiRacers({ human, resources, document: doc, sharedVisual: true });
  const padPtr = human._malloc(96), f32 = (ptr, n) => new Float32Array(human.HEAPF32.buffer, ptr, n);
  human._reset_pad_history(); human._start_event(); racers.start();
  const cores = [human, ...racers.npcs.map((n) => n.core)];
  if (mode === 'fast') for (const c of cores) c._set_presentation_fast?.(1);
  let t = 0; const pad = new Float32Array(24); pad[22] = 1;
  return () => {
    racers.beginTick(); human.HEAPF32.set(pad, padPtr >> 2);
    const o = f32(human._pad_tick(padPtr), 24).slice(); human._race_begin();
    const s = f32(human._step_rider(o[0], o[6], o[2] ? 1 : 0, o[7]), 16).slice();
    human._animation_tick(s[7], o[10], o[11], s[9], s[8], o[6], o[8], o[7], o[7], 0, s[15], o[13]);
    const pose = f32(human._pose_physical(), 12).slice(); human._race_end(); racers.endTick(); human._step_camera_head(pose[9], pose[10], pose[11]);
    if (t % READ_FX >= READ_FX - 2) for (const c of cores) if (c._rider_skin_palette_count()) c._rider_skin_palette();
    if (t % READ_FX === READ_FX - 1) for (const c of cores) { c._snow_info(); c._trail_info(); c._wake_info(); }
    t++;
  };
}
window.__bench = { status: 'loading' };
(async () => {
  try {
    const ticks = []; for (const s of specs) ticks.push(await race(s));
    window.__bench.status = 'warmup'; for (const tick of ticks) for (let i = 0; i < 300; i++) tick();
    const total = specs.map(() => 0), chunks = specs.map(() => []);
    for (let done = 0, round = 0; done < TICKS; done += CHUNK, round++) {
      for (let j = 0; j < ticks.length; j++) {
        const k = (j + round) % ticks.length,
          t0 = performance.now();
        for (let i = 0; i < CHUNK; i++) ticks[k]();
        const ms = performance.now() - t0;
        total[k] += ms;
        chunks[k].push(ms / CHUNK);
      }
      await new Promise((r) => setTimeout(r, 0));
      window.__bench.progress = done;
    }
    const median = (a) => [...a].sort((x, y) => x - y)[a.length >> 1];
    window.__bench = {
      status: 'done',
      results: specs.map((s, k) => ({
        spec: s,
        msPerTick: total[k] / TICKS,
        medianChunk: median(chunks[k]),
        ratio: k ? median(chunks[k].map((v, i) => v / chunks[0][i])) : 1
      }))
    };
  } catch (e) { window.__bench = { status: 'error', error: String(e && e.stack || e) }; }
})();
