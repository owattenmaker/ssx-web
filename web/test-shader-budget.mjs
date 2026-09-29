// WGSL private/function memory budget (web/wgsl-budget.js, docs/mobile.md). WebKit rejects a pipeline whose shader
// declares more than 8192 bytes of private (or function) variables; Chrome and Firefox accept it, so the break only
// showed on iPhones (field reports 2026-09-26: the fog composite and glare pipelines on Metro City, Ruthless, Peak 1).
//   node test-shader-budget.mjs            analyzer checks, then headless Chrome loads courses and checks every shader
//   node test-shader-budget.mjs --no-browser   analyzer checks only
// The browser part serves web/ with its own Vite server, hooks GPUDevice.createShaderModule / create*Pipeline*, loads
// Metro City (BRA2, ridden into the glare), Ruthless (DBC2, weather) and Peak 1 (PEAK1 free ride) at the phone quality
// tier (Ruthless and Peak 1 with pv=sparkle: the terrain sparkle's pipeline must compile there, whatever the switch's default),
// and asserts every compiled module stays under BUDGET (75% of the limit, so growth is caught before phones
// break) and that no pipeline failed. Skipped when no Chrome is installed (CHROME=/path overrides the search).
import assert from 'node:assert/strict';
import { wgslMemory, wgslTypeLayout, WGSL_PRIVATE_LIMIT, WGSL_FUNCTION_LIMIT } from './wgsl-budget.js';
import { startBrowser, startServer, sleep } from './headless-chrome.mjs';
const BUDGET = Math.floor(WGSL_PRIVATE_LIMIT * 0.75);

// ---- analyzer ----
assert.deepEqual(wgslTypeLayout('vec3<f32>'), { size: 12, align: 16 });
assert.deepEqual(wgslTypeLayout('mat4x4f'), { size: 64, align: 16 });
assert.deepEqual(wgslTypeLayout('mat3x3<f32>'), { size: 48, align: 16 });
assert.deepEqual(wgslTypeLayout('array<vec3<f32>, 4>'), { size: 64, align: 16 });
{
  const code = `struct S { a : f32, @location(0) b : vec3<f32> };\nvar<private> s : S;\nvar<private> m : array<mat4x4<f32>, 128>;\n` +
    `var<uniform> u : vec4<f32>;\nfn helper( x : f32 ) -> f32 { var t : vec4<f32>; var k : array<f32, 4>; return x; }\n` +
    `@fragment fn main() -> @location(0) vec4<f32> { var q : vec2<f32>; let z = 1.0; return vec4<f32>(z); }`;
  const m = wgslMemory(code);
  assert.equal(m.privateBytes, 32 + 128 * 64, 'struct S (32) + 128 mat4 (8192)');
  assert.equal(m.functionBytes, 32, 'helper: vec4 + array<f32,4>');
  assert.ok(m.privateBytes > WGSL_PRIVATE_LIMIT && m.largest[0].name === 'm');
  // three.js style: one var<private> per TSL temporary, vec3 rounded to 16.
  const three = Array.from({ length: 600 }, (_, i) => `var<private> nodeVar${i} : ${i % 2 ? 'vec3<f32>' : 'vec4<f32>'};`).join('\n');
  assert.equal(wgslMemory(three).privateBytes, 600 * 16);
}
console.log(`WGSL budget analyzer OK (limit ${WGSL_PRIVATE_LIMIT} B private / ${WGSL_FUNCTION_LIMIT} B function, test budget ${BUDGET} B)`);
if (process.argv.includes('--no-browser')) process.exit(0);

// ---- headless Chrome over a private dev server (web/headless-chrome.mjs) ----
const HOOK = `(() => {
  if (!globalThis.GPUDevice) return;
  const W = globalThis.__wgslBudget = { modules: [], pipelines: [], errors: [] };
  const P = GPUDevice.prototype, createModule = P.createShaderModule;
  P.createShaderModule = function (d) { const m = createModule.call(this, d); m.__budgetId = W.modules.length; W.modules.push({ label: d.label || '', code: d.code }); return m; };
  for (const n of ['createRenderPipeline', 'createRenderPipelineAsync', 'createComputePipeline', 'createComputePipelineAsync']) {
    const f = P[n]; P[n] = function (d) { W.pipelines.push({ label: d.label || '', modules: [d.vertex?.module, d.fragment?.module, d.compute?.module].filter(Boolean).map((m) => m.__budgetId) }); return f.call(this, d); };
  }
  const error = console.error.bind(console);
  console.error = (...a) => { const s = a.map(String).join(' '); if (/pipeline creation failed|GPUValidationError|private address space/i.test(s)) W.errors.push(s.slice(0, 400)); error(...a); };
})();`;

const browser = await startBrowser({ init: HOOK });
if (!browser) { console.log('Shader budget browser check SKIPPED: no Chrome found (set CHROME=/path)'); process.exit(0); }
const server = await startServer(), origin = server.origin;
const CASES = [
  // Metro City (the iPhone reports): ridden 900 ticks into the start-area glare, so the glare's final pipeline compiles too.
  { name: 'BRA2 Metro City', url: '/?qa=1&course=BRA2&quality=low', qaTicks: 900 },
  { name: 'DBC2 Ruthless (weather, terrain sparkle)', url: '/?course=DBC2&rider=moby&autostart=1&quality=low&pv=sparkle', pipeline: 'terrainSparkle' },
  { name: 'PEAK1 free ride (terrain sparkle)', url: '/?course=PEAK1&autostart=1&quality=low&pv=sparkle', pipeline: 'terrainSparkle' },
];
const failures = [], rows = [];
try {
  if (!(await browser.hasWebGPU(origin))) { console.log('Shader budget browser check SKIPPED: headless Chrome has no WebGPU adapter here'); }
  else for (const c of CASES) {
    const t0 = Date.now();
    await browser.send('Page.navigate', { url: origin + c.url });
    await sleep(1000);
    if (c.qaTicks) {
      await browser.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
      await browser.evaluate('(()=>{ssxQA.start();ssxQA.hud(false);return 1})()');
      await sleep(3000); // the rider's animation core (the camera step runs the glare/tint painters only with a pose)
      for (let t = 0; t < c.qaTicks; t += 100) { await browser.evaluate('(()=>{ssxQA.advance(100);return 1})()'); await sleep(200); }
      await sleep(2000);
      if (!(await browser.evaluate('!!globalThis.ssxEffects?.glare?.state?.frame?.enabled'))) failures.push(`${c.name}: the glare did not run at tick ${c.qaTicks}; its final pipeline went unchecked`);
    } else await browser.waitFor(`document.getElementById('stage')?.dataset.screen === 'game' || !!document.body.dataset.loadError`, 300000);
    const loadError = await browser.evaluate('document.body.dataset.loadError || ""');
    if (loadError) { failures.push(`${c.name}: load failed: ${loadError}`); continue; }
    await sleep(6000); // frames draw: the lazily compiled pipelines (glare, snow composite, effects) build
    const data = JSON.parse(await browser.evaluate('JSON.stringify(globalThis.__wgslBudget || null)'));
    if (!data) { failures.push(`${c.name}: the WebGPU hook did not run`); continue; }
    const measured = data.modules.map((m, i) => ({ i, label: m.label, length: m.code.length, ...wgslMemory(m.code), pipelines: data.pipelines.filter((p) => p.modules.includes(i)).map((p) => p.label) }));
    const worst = [...measured].sort((a, b) => b.privateBytes - a.privateBytes);
    rows.push(`${c.name}: ${data.modules.length} modules / ${data.pipelines.length} pipelines in ${((Date.now() - t0) / 1000).toFixed(0)} s; largest private ${worst[0]?.privateBytes ?? 0} B (${worst[0]?.pipelines[0] ?? '-'}), largest function ${Math.max(0, ...measured.map((m) => m.functionBytes))} B`);
    for (const m of measured) {
      const who = `${c.name}: module ${m.i} ${m.label} (${m.pipelines.slice(0, 2).join(', ') || 'no pipeline'}, ${m.length} chars, ${m.vars} private vars)`;
      if (m.privateBytes > BUDGET) failures.push(`${who}: private ${m.privateBytes} B > budget ${BUDGET} B (WebKit limit ${WGSL_PRIVATE_LIMIT}); largest ${m.largest.map((v) => `${v.name}:${v.type}`).join(' ')}`);
      if (m.functionBytes > BUDGET) failures.push(`${who}: function-scope ${m.functionBytes} B > budget ${BUDGET} B (WebKit limit ${WGSL_FUNCTION_LIMIT})`);
    }
    for (const e of data.errors) failures.push(`${c.name}: ${e}`);
    if (c.pipeline) {   // a switched-on effect whose pipeline must have compiled (so its shader was measured)
      const own = measured.filter((m) => m.pipelines.some((l) => l.includes(`_${c.pipeline}_`)));
      if (!own.length) failures.push(`${c.name}: no ${c.pipeline} pipeline compiled`);
      else rows.push(`  ${c.pipeline}: ${own.map((m) => `${m.label} ${m.privateBytes} B private / ${m.functionBytes} B function`).join(', ')}`);
    }
    if (!data.modules.length) failures.push(`${c.name}: no shader compiled`);
  }
} finally { await browser.close(); await server.close(); }
for (const r of rows) console.log(r);
if (failures.length) { console.error(`Shader budget FAILED (${failures.length}):\n` + failures.slice(0, 20).join('\n')); process.exit(1); }
console.log(`Shader budget OK: every compiled WGSL module <= ${BUDGET} B private and function memory, no pipeline errors`);
process.exit(0);
