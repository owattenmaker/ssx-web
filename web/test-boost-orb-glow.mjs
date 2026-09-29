import fs from 'node:fs';import assert from 'node:assert/strict';import {boostOrbGlow} from './boost-orb.js';
const profile=JSON.parse(fs.readFileSync(new URL('./public/assets/UI/boost-gauge.json',import.meta.url)));
const source=JSON.parse(fs.readFileSync(new URL('../local/browser-ui/hud/boost-orb-glow.json',import.meta.url)));
let pixels=0,alpha=0;
for(const c of source.cases){const a=boostOrbGlow(profile,c.phase,[0,5,10,11][c.palette]);if(c.phase<0){assert.equal(a,null);assert.equal(c.glow.length,0);continue;}assert.equal(c.glow.length,1);const d=c.glow[0];assert.deepEqual(a.uv,d.uv);assert.deepEqual(a.argb.slice(1),d.argb.slice(1));for(let i=0;i<2;i++)pixels=Math.max(pixels,Math.abs(a.position[i]-d.position[i]),Math.abs(a.size[i]-d.size[i]));alpha=Math.max(alpha,Math.abs(a.argb[0]-d.argb[0]));assert.equal(d.material[4]&65535,0x5f5);assert.equal((d.material[2]>>5)&31,10);}
assert.equal(source.cases.length,808);assert(pixels<.0001);assert(alpha<.000001);console.log({glowCases:808,maxPixelError:pixels,maxAlphaError:alpha});
