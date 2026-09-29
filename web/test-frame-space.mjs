// The frame's colour space (pv encodedBlend, web/frame-space.js; docs/visual-parity.md section 38).
// 1. Every material, composite and texture choice goes through web/frame-space.js: no other shipped module converts with three's sRGB
//    transfer functions or sets SRGBColorSpace (a new material that does would write linear light into an encoded frame, or the reverse).
// 2. The helper in both states: linear (EOTF / OETF, sRGB textures, three's colour management on) and encoded (identity, NoColorSpace,
//    colour management off, the canvas without output conversion).
// 3. Real browsers (headless Chrome and the macOS WebKit, WebGPU and WebGL2): a world-pass blend drawn through the helper and read back as
//    the fog composite reads it. Opaque bytes come back exact in both states; blends match the GS's 0x44 on bytes within 1 when encoded.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
const web = path.dirname(fileURLToPath(import.meta.url));

// ---- 1. sources ----
const offenders = [];
for (const f of fs.readdirSync(web).filter((f) => f.endsWith('.js') && !f.startsWith('test-') && !/-test\.js$/.test(f) && f !== 'frame-space.js')) {
  const s = fs.readFileSync(path.join(web, f), 'utf8');
  for (const m of s.matchAll(/\bsRGBTransfer(?:EOTF|OETF)\b|(?<![A-Za-z])SRGBColorSpace\b/g)) offenders.push(`${f}:${s.slice(0, m.index).split('\n').length} ${m[0]}`);
}
assert.deepEqual(offenders, [], 'colour-space conversions outside web/frame-space.js (use toFrame / fromFrame / linearToFrame / frameTextureSpace)');
console.log('sources: every frame colour conversion goes through web/frame-space.js');

// ---- 2. the helper in both states (a fresh process each: the choice is made once per page) ----
const probe = (search) => {
  const code = `globalThis.location={search:${JSON.stringify(search)},href:'http://x/'+${JSON.stringify(search)}};
    const f=await import('./frame-space.js'),tsl=await import('three/tsl'),T=await import('three/webgpu');
    const r=new T.Color(0x7a9cb9),out={};const renderer={outputColorSpace:null};f.configureFrameSpace(renderer);
    Object.assign(out,{encoded:f.encodedFrame,cm:T.ColorManagement.enabled,toFrame:f.toFrame===tsl.sRGBTransferEOTF?'eotf':f.toFrame(7)===7?'same':'?',
      fromFrame:f.fromFrame===tsl.sRGBTransferOETF?'oetf':f.fromFrame(7)===7?'same':'?',linearToFrame:f.linearToFrame===tsl.sRGBTransferOETF?'oetf':f.linearToFrame(7)===7?'same':'?',
      texture:f.frameTextureSpace,output:renderer.outputColorSpace,hex:r.getHex(),r:+r.r.toFixed(4)});console.log(JSON.stringify(out));`;
  const p = spawnSync(process.execPath, ['--input-type=module', '-e', code], { cwd: web, encoding: 'utf8' });
  assert.equal(p.status, 0, p.stderr); return JSON.parse(p.stdout.trim().split('\n').pop());
};
const linear = probe('?pv=-encodedBlend'), encoded = probe('?pv=encodedBlend');   // each state explicitly, whatever the default
assert.deepEqual(linear, { encoded: false, cm: true, toFrame: 'eotf', fromFrame: 'oetf', linearToFrame: 'same', texture: 'srgb', output: 'srgb', hex: 0x7a9cb9, r: 0.1946 });
assert.deepEqual(encoded, { encoded: true, cm: false, toFrame: 'same', fromFrame: 'same', linearToFrame: 'oetf', texture: '', output: 'srgb-linear', hex: 0x7a9cb9, r: 0.4784 });
console.log('frame-space.js: linear by default; ?pv=encodedBlend: identity conversions, NoColorSpace textures, Colors kept encoded, no output conversion');

if (process.argv.includes('--no-browser')) process.exit(0);

// ---- 3. real browsers ----
const { startServer, startBrowser } = await import('./headless-chrome.mjs');
const { startWebKit } = await import('./webkit-driver.mjs');
const server = await startServer();
const pages = [['pv=-encodedBlend&', 'webgpu'], ['pv=-encodedBlend&', 'webgl'], ['pv=encodedBlend&', 'webgpu'], ['pv=encodedBlend&', 'webgl']];   // each state explicitly
const check = (name, q, r) => {
  const enc = q[0] === 'pv=encodedBlend&', label = `${name} ${r?.backend ?? q[1]} ${enc ? 'encoded' : 'linear'}`;
  if (r?.webgpu === false) { console.log(`${label}: no WebGPU adapter, skipped`); return; }
  assert.ok(r && !r.error && r.blends === 16384, `${label}: ${JSON.stringify(r)?.slice(0, 400)}`);
  assert.equal(r.encoded, enc, label); assert.equal(r.opaqueExact, true, `${label}: opaque bytes changed`);
  if (r.encoded) assert.ok(r.max <= 1 && r.mean < 0.5, `${label}: encoded blend off the GS by ${r.max} (mean ${r.mean})`);
  else assert.ok(r.max >= 60 && r.mean > 8, `${label}: the linear-light blend is expected to differ from the GS (mean ${r.mean}, max ${r.max})`);
  console.log(`${label}: opaque exact; blend vs GS 0x44 mean ${r.mean}, max ${r.max}, within 1: ${(r.within1 * 100).toFixed(1)} %`);
};
try {
  const chrome = await startBrowser({ width: 400, height: 400 });
  if (!chrome) console.log('headless Chrome: not installed, skipped');
  else try {
    for (const q of pages) { await chrome.goto(`${server.origin}/frame-space-gpu-test.html?${q[0]}backend=${q[1]}&mute=1`); await chrome.waitFor('!!window.__frameSpace', 60000); check('Chrome', q, await chrome.evaluate('window.__frameSpace')); }
  } finally { await chrome.close(); }
  const webkit = await startWebKit({ width: 400, height: 400, offscreen: true });
  if (!webkit) console.log('WebKit (webkit-driver): unavailable, skipped');
  else try {
    for (const q of pages) { await webkit.goto(`${server.origin}/frame-space-gpu-test.html?${q[0]}backend=${q[1]}&mute=1`); await webkit.waitFor('!!window.__frameSpace', 60000); check('WebKit', q, await webkit.eval('window.__frameSpace')); }
  } finally { await webkit.close(); }
} finally { await server.close(); }
