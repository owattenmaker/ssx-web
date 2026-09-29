// The PS2 rider draw state (pv riderDrawState, web/rider-material.js riderDrawState; docs/xbox-textures.md section 9): 37A610 -> 363C20 ->
// 3626D8 give 'alph' / 'ea*' materials (flag 0x8000, bind 0x386920) GS ALPHA 0x44 with ATST GREATER 92 (AFAIL FB_ONLY: colour always, Z
// only above 92) and every other material ALPHA 0x2A with ATST ALWAYS. Checked here: the material split and settings; the frames are
// checked against PS2 VRAM-alpha experiments (docs).
import assert from 'node:assert/strict';
import { MeshBasicNodeMaterial } from 'three/webgpu';
import { vec4, float } from 'three/tsl';
import { riderDrawState, blendedRiderMaterial, RIDER_AREF } from './rider-material.js';

assert.equal(RIDER_AREF, 92);
for (const [name, blended] of [['alph', true], ['eatf', true], ['eatv', true], ['eata', true], ['suit', false], ['head', false], ['boot', false], ['bord', false], ['extu', false], ['pdas', false], [undefined, false]])
  assert.equal(blendedRiderMaterial(name), blended, String(name));
const make = () => { const m = new MeshBasicNodeMaterial({ alphaTest: 0.35 }); m.outputNode = vec4(float(0.5), float(0.5), float(0.5), float(0.6)); m.userData.sourceTexture = { big: true }; return m; };
// opaque: no alpha test, alpha written as 1 (the canvas is composited)
{ const m = make(), out = riderDrawState(m, 'suit'); assert.equal(out, m); assert.equal(m.alphaTest, 0); assert.equal(m.transparent, false); assert.notEqual(m.outputNode, null); }
// blended: two passes of the same geometry, the first above 92 (doubled texel alpha > 184) with Z writes, the second the rest without
{ const m = make(), out = riderDrawState(m, 'alph');
  assert.ok(Array.isArray(out) && out.length === 2 && out[0] === m);
  const [high, low] = out;
  assert.equal(high.alphaTest, 185 / 255); assert.equal(high.transparent, true); assert.equal(high.depthWrite, true);
  assert.equal(low.alphaTest, 0); assert.equal(low.transparent, true); assert.equal(low.depthWrite, false);
  assert.notEqual(low.outputNode, high.outputNode, 'the low pass keeps only alpha <= 184');
  assert.equal(low.userData.riderDrawPass, 'low'); assert.equal(high.userData.sourceTexture.big, true, 'userData kept by reference, not JSON-copied'); }
console.log('rider draw state: alph / ea* blended in two passes (Z above alpha 92), others opaque with alpha 1');
