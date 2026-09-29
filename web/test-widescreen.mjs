import assert from 'node:assert/strict';import {PerspectiveCamera} from 'three';import {WIDESCREEN_MODES,originalProjection,widescreenView} from './widescreen.js';
import {defaultWidescreen} from './widescreen.js';
// DEFAULT_3 Mid half-horizontal fov (~0.7539) recovered from the live 4:3 GS x scale ctx+0x6AF0 = 4362.43 (= 16*256/tan fov).
const fov=Math.atan(16*256/4362.43017578125),near=(a,b,e=0.01)=>assert(Math.abs(a-b)<e,`${a} != ${b}`);
// PS2 GS scales (px at 512x448) and 3D viewport per mode; mode 0 matches ctx+0x6AF0/0x6B04 = 4362.43/-5089.50 (x16).
const expected={0:[272.65,318.09,[0,0,512,448]],1:[204.49,238.57,[0,56,512,336]],2:[204.49,318.09,[0,0,512,448]]};
for(const {mode} of WIDESCREEN_MODES){
 const p=originalProjection(mode,fov),[sx,sy,vp]=expected[mode];near(p.scaleX,sx);near(p.scaleY,sy);assert.deepEqual(p.viewport,vp);
 // Browser camera reproduces the PS2 normalized projection of the 3D viewport (x over half width, y over half height).
 const v=widescreenView(mode,fov),camera=new PerspectiveCamera(v.fovY,v.cameraAspect,.1,100),e=camera.projectionMatrix.elements;
 near(e[0],p.scaleX/(p.viewport[2]/2),1e-6);near(e[5],p.scaleY/(p.viewport[3]/2),1e-6);
 // Rendered 3D band keeps the PS2's square-pixel shape on the stage.
 near(v.stageAspect/v.band[1],v.cameraAspect,1e-12);
}
// 16:9 letterbox zoomed on a 16:9 TV and Anamorphic stretched on one show the same view: vertical angle unchanged, horizontal 4/3 wider.
const [off,letterbox,anamorphic]=[0,1,2].map(m=>widescreenView(m,fov));
assert.equal(off.fovY,letterbox.fovY);assert.equal(off.fovY,anamorphic.fovY);assert.equal(letterbox.cameraAspect,anamorphic.cameraAspect);near(letterbox.cameraAspect,16/9,1e-12);
assert.deepEqual([off.stageAspect,letterbox.stageAspect,anamorphic.stageAspect],[4/3,4/3,16/9]);assert.deepEqual(letterbox.band,[.125,.75]);
console.log('Widescreen projection (0x377950/0x376C58):',Object.fromEntries(WIDESCREEN_MODES.map(m=>[m.label,(p=>[+p.scaleX.toFixed(2),+p.scaleY.toFixed(2),p.viewport])(originalProjection(m.mode,fov))])));

// Unchosen default: Anamorphic on 16:9/16:10 landscape displays, Off on 4:3 and portrait screens.
assert.equal(defaultWidescreen(1920,1080),2);assert.equal(defaultWidescreen(1440,900),2);assert.equal(defaultWidescreen(1024,768),0);assert.equal(defaultWidescreen(1080,1920),0);assert.equal(defaultWidescreen(0,0),0);
console.log('Widescreen default: Anamorphic on widescreen displays, Off otherwise.');
