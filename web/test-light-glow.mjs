import fs from 'node:fs';
import assert from 'node:assert/strict';
import {PerspectiveCamera,Matrix4,Vector3} from 'three/webgpu';
import {ps2Mul,glowPixel,glowDepthFraction,glowRecord,glowVisibility,glowColour,glowQuad} from './light-glow.js';
// Original light glow halos (tools/export_light_glow.py) against the live glow list of the
// owned event-start savestate: owner(0x1467070)+0x60 count, +0x68 records (0x30 each),
// record+0x20 -> type-8 world entity (kind-7 record: +0C flags, +10 RGB, +1C position).
const pkg=JSON.parse(fs.readFileSync('public/assets/LIGHT_GLOW/light-glow.json'));
const c=pkg.constants,f=Math.fround;
assert.equal(pkg.lights.length,133,'the Snow Jam event octree holds 128 ARA1 + 5 ARA1_B glow sources');
assert.deepEqual([16,32,64].map(k=>pkg.lights.filter(l=>l.flags===k).length),[82,51,0]);
assert.deepEqual(c.classes,{16:{texture:'shal',half_size:180,pull_cm:100},32:{texture:'mhal',half_size:100,pull_cm:80},64:{texture:'shal',half_size:350,pull_cm:200}});
assert.equal(c.z_scale,5.960465188081798e-08);assert.equal(c.far_threshold,f(.005));assert.equal(c.rotation_scale,f(Math.PI/2));
for(const name of ['shal','mhal']){
 const t=pkg.textures[name],bytes=fs.readFileSync('public/assets/LIGHT_GLOW/'+t.file);
 assert.equal(bytes.length,64*64*4);let a=0;for(let i=3;i<bytes.length;i+=4)a=Math.max(a,bytes[i]);assert.equal(a,128,`${name} keeps raw GS alpha`);
}
// Live records (screen x/y = count origin + w>>1 / h>>1) and their sources.
const live=[
 {rect:[141,130,12,6],read:[139,129],angle:-0.6688156127929688,flags:16,colour:[0.6809999942779541,0.8351302146911621,1],position:[-133131.5,12937.6083984375,-228313.625]},
 {rect:[395,128,13,6],read:[393,127],angle:0.8897088766098022,flags:16,colour:[0.6809999942779541,0.8351302146911621,1],position:[-133365.921875,14536.169921875,-228313.625]},
 {rect:[114,245,4,2],read:[108,242],angle:-0.859029233455658,flags:32,colour:[0.1599999964237213,0.42527997493743896,0.800000011920929],position:[-135715.765625,10757.482421875,-229370.34375]},
 {rect:[200,209,4,2],read:[194,206],angle:-0.33133983612060547,flags:32,colour:[0.1599999964237213,0.42527997493743896,0.800000011920929],position:[-135595.390625,12375.8671875,-228818.4375]}];
for(const r of live)assert.ok(pkg.lights.some(l=>l.flags===r.flags&&l.position.every((v,i)=>v===r.position[i])&&l.colour.every((v,i)=>v===r.colour[i])),'live source is an authored kind-7 record');
// Event-start camera (owner+0x10 world->view rows, +0x50 fov/near/far) as in test-sun-flare.
const M=[[-0.16516868770122528,-0.0724114403128624,-0.983603298664093],[0.9862650632858276,-0.012126659974455833,-0.16472291946411133],[4.656612873077393e-10,0.9973011016845703,-0.07341983914375305]],t=[-35443.97265625,218711.4375,-143827.5];
const eye=[0,1,2].map(i=>-(t[0]*M[i][0]+t[1]*M[i][1]+t[2]*M[i][2])),web=v=>new Vector3(v[0],v[2],-v[1]);
const axis=j=>web([M[0][j],M[1][j],M[2][j]]);
const camera=new PerspectiveCamera(2*Math.atan(Math.tan(0.7539098262786865)*.75)*180/Math.PI,4/3,.3,29999.966796875/100);
camera.position.copy(web(eye).multiplyScalar(1/100));camera.quaternion.setFromRotationMatrix(new Matrix4().makeBasis(axis(0),axis(1),axis(2).negate()));camera.updateMatrixWorld(true);
for(const r of live){
 const p=web(r.position).multiplyScalar(1/100),ndc=p.clone().project(camera),depth=-p.clone().applyMatrix4(camera.matrixWorldInverse).z;
 const screen=[glowPixel((ndc.x+1)*256),glowPixel((1-ndc.y)*224)];
 const rec=glowRecord(screen,glowDepthFraction(depth*100,30,29999.966796875),c);
 assert.equal(rec.far,false);assert.deepEqual(rec.count,r.rect,`count rect ${screen}`);assert.deepEqual(rec.read,r.read);assert.equal(rec.angle,r.angle,'2E2B00 rotation');
}
// Far threshold (zf <= 0.005 -> far, no query) and the size clamps.
assert.deepEqual(glowRecord([100,100],.005,c),{far:true,angle:0});
assert.deepEqual(glowRecord([4,2],.5,c).count,[-4,-2,16,8]);assert.deepEqual(glowRecord([4,2],.5,c).read,[0,0]);
assert.deepEqual(glowRecord([510,446],.00501,c),{far:false,count:[508,445,4,2],read:[496,440],angle:ps2Mul(f(f(f(510*2)/512)-1),c.rotation_scale)});
assert.deepEqual(glowRecord([300,100],.02,c,[0,56,512,336]).read,[292,96]);
// 2EC478 visibility and 2E2868 colour / 3781A0 corners.
assert.equal(glowVisibility(128,128,128),1);assert.equal(glowVisibility(72,36,72),.5);assert.equal(glowVisibility(4,3,8),0);assert.equal(glowVisibility(6,5,8),.25);
assert.deepEqual(glowColour(live[0].colour),[59,72,87]);assert.deepEqual(glowColour(live[2].colour),[22,59,111]);
assert.deepEqual(glowQuad([100,50],[20,-3],[3,-20]),[[77,73,0,0],[117,67,1,0],[83,33,0,1],[123,27,1,1]]);
console.log('light glow: package, live records, projection, query rects and sprite checks pass');
