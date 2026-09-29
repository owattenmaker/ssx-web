// Rider contact/impact FX against real PS2 state (docs/terrain-render-fidelity.md "Impact and contact
// effects"). The fx-* captures (local/ps2-capture/runs/fx, tools/ps2_capture.py --watch windows) record
// the sparks component RFX+0x470 (0x146F800, 0xA4), its 0x150-byte kernel (0x5DD200), the visual LCG,
// the fist sparkle RFX+0xC70 (0x1470000, 0xA4) and the snow FX RFX+0xB40 (0x146FED0, 0x130) every tick.
// Checks, per capture tick:
//  - fx-rail (rail-balance-lr pads): spark/chunk gates, rider+0x438, emit point and every kernel word
//    (36D428 age/seeds, 36D318 colours, 36D1F0 velocities, 36CEF8 box) bit-exact through tick 900
//    (the capture's bone gate; 901 is the known ground-attach pose difference);
//  - fx-attack (attack-mix pads): fist sparkle active on exactly the PS2 ticks, sprites around the same hand;
//  - all three (and fx-race, the event-race pads with two crashes): snow impact strength FX+0xE0 bit-exact
//    (landing |v| 10E910, crash 111AA0 triggers, rail surface 10 zeroing impacts).
import fs from 'node:fs';import assert from 'node:assert/strict';import os from 'node:os';import {execFileSync} from 'node:child_process';
import {sparkSpriteAlphas} from './impact-fx-renderer.js';

// VU program 4 fade: accepted sprites step alpha by 1/8 within a slot, rejected ones do not.
{const s=new Float32Array(9*5);const slots=[3,3,3,4,4];slots.forEach((slot,j)=>{s[j*9+7]=160;s[j*9+8]=slot;});
 const a=sparkSpriteAlphas(s,5,j=>j!==1);assert.deepEqual(Array.from(a),[160,-1,140,160,140]);}

const runs=new URL('../local/ps2-capture/runs/fx/',import.meta.url).pathname;
const captures=[{name:'fx-rail',args:[]},{name:'fx-attack',args:[]},{name:'fx-race',args:['--event']}];
if(!captures.every(c=>fs.existsSync(runs+c.name+'.bin'))){console.log('Impact FX: fade unit check passed; private fx captures absent, capture checks skipped.');process.exit(0);}
const W=10752,K=W+0xA4,C=K+0x150+4,S=C+0xA4,R=16384;
const results=[];
for(const c of captures){
 const dump=`${os.tmpdir()}/impact-fx-${process.pid}-${c.name}.json`;
 execFileSync(process.execPath,[new URL('compare-ps2-capture.mjs',import.meta.url).pathname,runs+c.name+'.bin','--pad','--zoe','--sync-rng',...c.args],{env:{...process.env,FX_DUMP:dump},stdio:['ignore','ignore','inherit']});
 const web=JSON.parse(fs.readFileSync(dump,'utf8'));fs.unlinkSync(dump);
 const raw=fs.readFileSync(runs+c.name+'.bin'),dv=new DataView(raw.buffer,raw.byteOffset,raw.byteLength),ps2=new Map();
 for(let at=0;at+R<=raw.length;at+=R)ps2.set(dv.getUint32(at+4,true),at);
 let ticks=0,strength=0,surface=0,gates=0,kernelTicks=0,fist=0,fistBoth=0;
 for(const row of web){const at=ps2.get(row.tick);if(at===undefined)continue;ticks++;
  const u=o=>dv.getUint32(at+o,true),f=o=>dv.getFloat32(at+o,true);
  // FX+0xE0 retained snow impact strength (snow_info[22]).
  assert.equal(Math.fround(row.snow[22]),f(S+0xE0),`${c.name} ${row.tick}: snow impact strength`);strength++;
  assert.equal(row.info[10],dv.getInt32(at+32+0x438-0x100,true),`${c.name} ${row.tick}: rider+0x438`);surface++;
  assert.deepEqual([row.info[6],row.info[8]],[u(W+0x70),u(W+0x78)],`${c.name} ${row.tick}: spark/chunk gates`);gates++;
  if(u(W+0x70))for(let k=0;k<3;k++)assert.equal(Math.fround(row.info[14+k]),f(W+0x10+4*k),`${c.name} ${row.tick}: emit point`);
  if(c.name==='fx-rail'&&row.tick>=673&&row.tick<=900){
   for(let w=0;w<84;w++){if([0x24,0x28,0x2C,0x140,0x144].includes(w*4))continue;assert.equal(row.kernel[w],u(K+4*w),`fx-rail ${row.tick}: kernel +0x${(w*4).toString(16)}`);}kernelTicks++;}
  const active=u(C+0xA0);assert.equal(row.info[9],active,`${c.name} ${row.tick}: fist sparkle active`);if(active)fist++;
  if(active&&row.info[9]){const s=f(32+0x350-0x100);const mean=a=>[0,1,2].map(k=>a.reduce((t,p)=>t+p[k],0)/4);
   const webMean=mean([0,1,2,3].map(i=>row.fist.slice(i*8,i*8+3))),ps2Mean=mean([0,1,2,3].map(i=>[0,1,2].map(k=>f(C+0x10+16*i+4*k))));
   const d=Math.hypot(...webMean.map((x,k)=>x-ps2Mean[k]));assert(d<=6*s+0.01,`${c.name} ${row.tick}: fist sparkle ${d.toFixed(2)} cm from the PS2 sprites`);fistBoth++;}
 }
 results.push(`${c.name}: ${ticks} ticks (impact strength/surface/gates exact${kernelTicks?`, kernel exact ${kernelTicks}`:''}${fist?`, fist ${fistBoth}/${fist}`:''})`);
}
console.log('Impact FX vs PS2:',results.join('; '));
