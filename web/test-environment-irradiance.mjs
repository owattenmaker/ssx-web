import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const core=await createCore();
process.on('uncaughtException',e=>{if(e instanceof Error)console.error(e);else console.error(core.getExceptionMessage(e));process.exit(1);});
const riderPackage='RIDER_ZOE';
const root=new URL('public/assets/',import.meta.url),read=p=>fs.readFileSync(new URL(p,root)),json=p=>JSON.parse(read(p));
const put=bytes=>{const p=core._malloc(bytes.length);core.HEAPU8.set(bytes,p);return p;};
const str=path=>put(Buffer.concat([read(path),Buffer.from([0])]));
const meta=str('ANIMATIONS/animation-packets.json'),rigPtr=str(riderPackage+'/rider.json'),cfg=str('ANIMATIONS/initial.json'),packets=read('ANIMATIONS/animation-packets.bin'),packetsPtr=put(packets);
core._init_animation(meta,rigPtr,cfg,packetsPtr,packets.length);core._init_race(cfg);core._animation_use_physics(1);
const mesh=read('ARA1/collision.bin'),mp=put(mesh);core._init_world(mp,mesh.length/4);
const terrain=str('ARA1/terrain.json'),world=str('ARA1/world_collision.json'),hash=put(Buffer.from(json('ARA1/terrain.json').source_sha256+'\0'));
core._init_terrain(terrain);core._init_world_collision(world,hash);core._init_body_terrain(terrain);const rails=str('ARA1/rails.json');core._init_rails(rails,hash);core._free(rails);
for(const p of [meta,rigPtr,cfg,packetsPtr,mp,terrain,world,hash])core._free(p);
const poseCount=json(riderPackage+'/rider.json').bones.length*7,start=json('ARA1/start.json');
const fogPackage=str('ARA1/fog-tree.json');core._init_fog(fogPackage);core._free(fogPackage);


// Integration check: actual terrain ratio -> brightness -> persistent bank, once per frame.
const sample=(p,n)=>Array.from(new Float32Array(core.HEAPF32.buffer,p,n));
const environment=json('ARA1/environment.json'),bytes=read('ARA1/environment.bin'),ptr=put(bytes),metaPtr=str('ARA1/environment.json');
core._init_environment(metaPtr,ptr,bytes.length);core._free(metaPtr);core._free(ptr);
const lighting=environment.irradiance;assert(lighting,'Missing authored irradiance package');
const scratch=new ArrayBuffer(4),word=new Uint32Array(scratch),real=new Float32Array(scratch);
const bits=x=>{real[0]=x;return word[0];},fromBits=x=>{word[0]=x;return real[0];};
const reduce=x=>fromBits(bits(x)-1);
const correct=(rounded,residual)=>(rounded>0&&residual<0)||(rounded<0&&residual>0)?reduce(rounded):rounded;
const mul=(a,b)=>{const exact=a*b;return correct(Math.fround(exact),exact-Math.fround(exact));};
const add=(a,b)=>{const sum=a+b,r=Math.fround(sum),error=Math.abs(a)>=Math.abs(b)?b-(sum-a):a-(sum-b);return correct(r,(sum-r)+error);};
const sub=(a,b)=>{let x=bits(a),y=bits(b);const shift=((x>>>23)&255)-((y>>>23)&255);if(shift>=25)y&=0x80000000;else if(shift<=-25)x&=0x80000000;else if(shift>0)y&=0xffffffff<<(shift-1);else if(shift<0)x&=0xffffffff<<(-shift-1);return add(fromBits(x),-fromBits(y));};
let previous=Array(40).fill(0),changed=0,successfulUpdates=0,gapFrames=0;const brightnesses=new Set();
core._reset_animation();core._reset_race();core._reset_rider(...start.position,start.heading);
for(let frame=0;frame<600;frame++){
 const jump=+(frame>=1&&frame<30);core._race_begin();const state=sample(core._step_rider(0,jump,0,0),16);
 core._animation_tick(state[7],0,0,state[9],state[8],jump,0,0,0,0,state[15],0);core._race_end();
 const info=sample(core._environment_irradiance_info(),10),actual=sample(core._environment_irradiance(),40);
 assert.equal(info[0],1);
 if(info[1]){gapFrames++;assert.equal(info[2],successfulUpdates);assert.deepEqual(actual,previous,'Unavailable terrain sample must preserve the last bank');continue;}
 successfulUpdates++;assert.equal(info[2],successfulUpdates,'Bank must update exactly once per successful environment sample');
 assert(actual.every(Number.isFinite));assert(info[3]>=0&&info[3]<=1);brightnesses.add(info[3]);
 assert.equal(info[4],lighting.gain);assert.equal(info[5],lighting.incoming);
 const weight=mul(info[3],info[4]),opposite=sub(1,weight),oldWeight=sub(1,info[5]);
 const bright=lighting.bright.flat(),dark=lighting.dark.flat();
 const expected=previous.map((v,i)=>add(mul(add(mul(bright[i],weight),mul(dark[i],opposite)),info[5]),mul(v,oldWeight)));
 assert.deepEqual(actual,expected,`Wrong authored bank blend at frame${frame}`);
 changed+=+actual.some((v,i)=>v!==previous[i]);previous=actual;
}
assert(changed>1);assert(brightnesses.size>1,'Terrain ratio must vary the lighting weight');
console.log('Live normal-rider environment bank:',{frames:600,successfulUpdates,gapFrames,changed,brightnessValues:brightnesses.size});
