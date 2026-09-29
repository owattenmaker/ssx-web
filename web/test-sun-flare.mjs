import fs from 'node:fs';
import assert from 'node:assert/strict';
import {PerspectiveCamera,Matrix4,Vector3} from 'three/webgpu';
import {createSunPainter,sunDirection,sunSprites,sunQueryRect,sunVisibility} from './sun-flare.js';
// Original sun glow / lens flare package (tools/export_sun_flare.py) against values
// read from the owned event-start savestate: Sun painter instance *(*(0x4FA370+6*0xF0+0x14))
// (vtable 484700) and the sun object at owner(0x1467070)+0x6070.
const pkg=JSON.parse(fs.readFileSync('public/assets/SUN_FLARE/sun-flare.json'));
const f=Math.fround;
assert.equal(pkg.painter.payloads.length,1,'ARA1 paints one Sun payload');
assert.deepEqual(pkg.painter.payloads[0],{rate:f(-.1),elevation:f(9.15),azimuth:160,colour:[1,1,1],glow_alpha:f(.95),texture:0,flare_alpha:f(.85),size:280});
// 2F43E0 flare table (quadrant, line position, size/300, ARGB) as held live at sun+0x30.
const live=[[1,1.2999999523162842,.03999999910593033,.4998680055141449,1,1,0],[3,.595413327217102,.03453768044710159,.12951700389385223,1,1,0],
 [0,.5,.20000000298023224,.22426900267601013,.9402310252189636,.5425059795379639,.0004030000127386302],[2,.20000000298023224,.025077050551772118,.4652079939842224,.32170501351356506,.9943439960479736,0],
 [0,0,.03999999910593033,.17792600393295288,1,1,0],[0,-.5141515731811523,.06998095661401749,.4411740005016327,.41388601064682007,.3211260139942169,.4398239850997925],
 [1,-.40145567059516907,.030118949711322784,.500997006893158,.24849599599838257,1,1],[3,-.6160849332809448,.03999999910593033,.27687299251556396,.6395620107650757,.13423900306224823,0],
 [1,-1,.0206892192363739,.8936439752578735,.5263469815254211,.5051450133323669,.8687300086021423]];
pkg.flares.forEach((e,i)=>assert.deepEqual([e.quadrant,e.position,e.size,...e.argb],live[i],`flare ${i}`));
for(const [name,w,h] of [['lens',256,256],['sun1',128,64],['sun2',128,64]]){
 const t=pkg.textures[name],bytes=fs.readFileSync('public/assets/SUN_FLARE/'+t.file);
 assert.equal(bytes.length,w*h*4);let a=0;for(let i=3;i<bytes.length;i+=4)a=Math.max(a,bytes[i]);assert.equal(a,128,`${name} keeps raw GS alpha`);
}
// Driver: the initial camera tick applies the payload (blend weight -1 -> 1).
const painter=createSunPainter(pkg),info=new Float32Array(16),core={HEAPF32:info,_fog_info:()=>0};
info[7]=1;info[8]=-131865.234375;info[9]=13858.5;info[10]=1;painter.tick(core);
assert.deepEqual(painter.values,{elevation:f(9.15),azimuth:160,r:1,g:1,b:1,glowAlpha:f(.95),texture:0,flareAlpha:f(.85),size:280});
// Event-start camera (owner+0x10 world->view rows, +0x50 fov/near/far) and live painter
// values project the sun to sun+0x1F0 = (412, 140) in the 512x448 viewport (37DD20 truncates).
const M=[[-0.16516868770122528,-0.0724114403128624,-0.983603298664093],[0.9862650632858276,-0.012126659974455833,-0.16472291946411133],[4.656612873077393e-10,0.9973011016845703,-0.07341983914375305]],t=[-35443.97265625,218711.4375,-143827.5];
const eye=[0,1,2].map(i=>-(t[0]*M[i][0]+t[1]*M[i][1]+t[2]*M[i][2])),web=v=>new Vector3(v[0],v[2],-v[1]);
const axis=j=>web([M[0][j],M[1][j],M[2][j]]);
const camera=new PerspectiveCamera(2*Math.atan(Math.tan(0.7539098262786865)*.75)*180/Math.PI,4/3,.3,29999.966796875/100);
camera.position.copy(web(eye).multiplyScalar(1/100));camera.quaternion.setFromRotationMatrix(new Matrix4().makeBasis(axis(0),axis(1),axis(2).negate()));camera.updateMatrixWorld(true);
const values={elevation:9.149983406066895,azimuth:159.99974060058594,r:1,g:1,b:1,glowAlpha:.9499989748001099,texture:0,flareAlpha:.8499990105628967,size:279.9994812011719};
const dir=sunDirection(values,pkg.constants).web,point=camera.position.clone().addScaledVector(new Vector3(...dir),camera.far-pkg.constants.far_inset_cm/100);
const ndc=point.clone().project(camera),screen=[(ndc.x+1)*256,(1-ndc.y)*224];
assert.deepEqual(screen.map(Math.trunc),[412,140],`sun screen ${screen}`);
// Query rect 2F5080: (x-8, y-8, 16, 16); clamp to the viewport when read.
assert.deepEqual(sunQueryRect([412,140],pkg.constants),{origin:[404,132],read:[404,132],size:16});
assert.deepEqual(sunQueryRect([505,4],pkg.constants),{origin:[497,-4],read:[496,0],size:16});
// 2EC478: fully on screen -> open/256; partly off -> max((open-128)/128,0).
assert.equal(sunVisibility(256,256),1);assert.equal(sunVisibility(256,64),.25);assert.equal(sunVisibility(192,192),.5);assert.equal(sunVisibility(128,128),0);
// 2F4A08/2F4690 sprites: half 280 (glow) and 300*s (flares), vertex bytes trunc(x*128).
const sprites=sunSprites(pkg,values,[412,140],1);
assert.equal(sprites[0].half,279.9994812011719);assert.deepEqual(sprites[0].rgb,[128,128,128]);assert.equal(sprites[0].a,121);assert.equal(sprites[0].texture,'sun1');
assert.deepEqual(sprites[1].centre,[256+156*1.2999999523162842,224-84*1.2999999523162842]);assert.equal(sprites[1].half,.03999999910593033*300);assert.deepEqual(sprites[1].uv,[.5,0,1,.5]);
assert.deepEqual(sprites[9].centre,[100,308]);assert.equal(sprites[9].a,Math.trunc(f(.8936439752578735*f(.8499990105628967))*128));
assert.equal(sunSprites(pkg,{...values,size:1},[0,0],1)[0].half,320,'default glow half size when +48 == 1');
console.log('sun flare: package, painter, projection, query and sprite checks pass');
