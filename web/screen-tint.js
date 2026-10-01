import {Vector3} from 'three/webgpu';
import {uniform,clamp,floor,select} from 'three/tsl';
import {followRegion,followWorldLoad} from './painter-regions.js';

// Original tWPIGD_ScreenTint (factory 7, constructor 2BC890, vtable 484B70).
// State: current/sample pairs scale R,G,B at +8/+10/+18, add R,G,B at +20/+28/+30.
// Driver 2C0778 (engine/painter_driver.hpp) runs once per camera tick with the
// same source X/Y the Fog driver receives (2ED490). 2F00A0 copies the current
// values to gp+14F4..1508; 390458 stores them in render context+6CA4.
// Draw 3904A0/3905E8, after the fog composite (363490: 36AC00 then 3904A0):
//  skipped when scale==(1,1,1) and add==(0,0,0);
//  pass 1: framebuffer re-read as PSMCT32 texture, TEX1 nearest, TFX0 MODULATE,
//          no blend, vertex RGB = trunc(scale*127.5) -> C = (C*s)>>7;
//  pass 2 (only if some trunc(add*127.5) > 0): untextured sprite, ALPHA 0x48
//          with As=0x80 -> C = C + a. GS colour clamp applies after each pass.
const f=Math.fround;
function payloadIndex(tint,x,y){
 if(tint.absent||!tint.nodes?.length)return -1; // a location without a ScreenTint section (streamed world): the identity defaults
 let px=f(f(x-tint.origin[0])*tint.scale),py=f(f(y-tint.origin[1])*tint.scale);
 let leaf=null;
 if(px>-1&&px<32768&&py>-1&&py<32768){
  let a=((Math.trunc(px)<<1)&0xffff)>>>0,b=((Math.trunc(py)<<1)&0xffff)>>>0,index=tint.root;
  for(let steps=0;steps<=tint.nodes.length;steps++){
   const node=tint.nodes[index];if(!node)throw Error('ScreenTint node outside package');
   if(!(node[0]&1)){leaf=node;break;}
   index=node[((a>>15)<<1)|(b>>15)]>>1;a=(a<<1)&0xfffc;b=(b<<1)&0xfffc;
  }
  if(!leaf)throw Error('Cyclic ScreenTint tree');
 }
 const value=leaf?((leaf[2]|(leaf[3]<<16))>>>0):tint.outside_words[1];
 return value===0xffffffff?-1:value;
}
export async function createScreenTint(source='/assets/ARA1/screen-tint.json'){
 let tint=source;if(typeof source==='string'){const response=await fetch(source);if(!response.ok)throw Error('Missing ScreenTint package');tint=await response.json();}
 if(tint.version!==1||typeof tint.location!=='string'||!tint.payloads?.length)throw Error('Unsupported ScreenTint package');
 const payloads=tint.payloads.map(p=>[...p.scale,...p.add].map(f)),defaults=[1,1,1,0,0,0];
 const scaleBytes=uniform(new Vector3(127,127,127)),addBytes=uniform(new Vector3()),enabled=uniform(0);
 // Lightning 0x390C60 / its render 0x390F20 (web/weather.inc weather_lightning): a full-screen sprite while the flash
 // intensity I != 0, vertex RGB trunc(I*colour*255) and A trunc(I*255) (8-bit GIF words), blend mode 5 = GS ALPHA 0x44
 // ((Cs - Cd)*As>>7 + Cd), untextured, after the tint.
 const flashRGB=uniform(new Vector3()),flashA=uniform(0),flashOn=uniform(0);let painterResets=-1;
 let current=defaults.slice(),distance=-99999,lastX=0,lastY=0,lastTicks=-1,selected=-1;
 // regionTick (web/painter-regions.js): the record of the core's painter region gp+0x770, switched on the camera block's tick;
 // missing: that record is not loaded (0x2C09D8: the class defaults, +0 = 0).
 const region={track:-1,located:false};let missing=false;
 const reset=()=>{current=defaults.slice();distance=0;};
 const blend=(values,weight)=>{const w=f(weight*weight),c=f(1-w);current=current.map((v,i)=>f(f(w*values[i])+f(c*v)));};
 function step(x,y){
  if(missing){lastX=x;lastY=y;current=defaults.slice();distance=0;selected=-1;return;}
  const initial=distance===-99999;
  if(!initial){const dx=f(lastX-x),dy=f(lastY-y);distance=f(distance+f(Math.sqrt(f(f(dx*dx)+f(dy*dy)))));}
  lastX=x;lastY=y;selected=payloadIndex(tint,x,y);
  if(selected<0){reset();return;}
  const values=payloads[selected],rate=tint.payloads[selected].rate;
  if(values.every((v,i)=>v===current[i]))distance=0;
  if(initial){blend(values,-1);distance=0;return;}
  if(rate>=0&&rate<=distance){blend(values,1);distance=0;}else blend(values,-rate);
 }
 function useTree(next) {
   if (next?.version !== 1 || !next.payloads) throw Error('Unsupported ScreenTint package');
   tint = next;
   payloads.length = 0;
   payloads.push(...(next.payloads.length ? next.payloads : []).map((p) => [...p.scale, ...p.add].map(f)));
 }
 function publish(){
  const s=current.slice(0,3).map(v=>Math.trunc(f(v*127.5))),a=current.slice(3).map(v=>Math.trunc(f(v*127.5)));
  scaleBytes.value.set(...s);addBytes.value.set(...a.map(v=>Math.max(0,v)));
  enabled.value=current.every((v,i)=>v===defaults[i])?0:(a.some(v=>v>0)?2:1);
 }
 return {
   // Once per simulated camera tick, after _step_camera_head ran the Fog driver.
   step(core) {
     const info = new Float32Array(core.HEAPF32.buffer, core._fog_info(), 11),
       ticks = info[7];
     // 0x2C03E8 (a rider's reset placement, web/weather.inc): every painter wrapper restarts (+0 = -99999: the next step jumps).
     if (core._weather_info) {
       const w = new Float32Array(core.HEAPF32.buffer, core._weather_info(), 47);
       if (painterResets >= 0 && w[46] !== painterResets && distance !== -99999) distance = -99999;
       painterResets = w[46];
     }
     if (core._weather_lightning) {
       const L = new Float32Array(core.HEAPF32.buffer, core._weather_lightning(), 10),
         I = L[1];
       if (I !== 0) {
         const c = [L[2], L[3], L[4]].map((v) => Math.trunc(f(f(I * v) * 255)) & 255);
         flashRGB.value.set(...c);
         flashA.value = Math.trunc(f(I * 255)) & 255;
         flashOn.value = 1;
       } else flashOn.value = 0;
     }
     if (!info[10]) return;
     if (ticks < lastTicks) {
       current = defaults.slice();
       distance = -99999;
       selected = -1;
       publish();
     }
     if (followWorldLoad(region, core)) {
       current = defaults.slice();
       distance = 0;
       selected = -1;
       publish();
     } // the class defaults, +0 = 0
     if (ticks > 0 && ticks !== lastTicks) {
       const r = followRegion(region, core, 'tint');
       if (r) {
         missing = !r.record;
         if (r.record) useTree(r.doc ?? { version: 1, absent: true, payloads: [] });
       }
       (step(info[8], info[9]), publish());
     }
     lastTicks = ticks;
   },
   // Encoded 0..255 RGB after the fog composite -> tinted encoded bytes.
   apply(bytes) {
     const scaled = clamp(floor(bytes.mul(scaleBytes).div(128)), 0, 255);
     const tinted = select(enabled.greaterThan(0), select(enabled.greaterThan(1), clamp(scaled.add(addBytes), 0, 255), scaled), bytes);
     return select(flashOn.greaterThan(0), clamp(floor(flashRGB.sub(tinted).mul(flashA).div(128)).add(tinted), 0, 255), tinted);
   },
   // A streamed location's own ScreenTint record (web/free-ride.js): the region follows the rider; the blend state is kept.
   setTree(next) {
     if (region.located) return;
     useTree(next);
   },
   // Restore a captured original instance (+0 distance, +8..+30 current values).
   seed({ current: values, distance: travelled, x, y, ticks }) {
     current = values.map(f);
     distance = f(travelled);
     lastX = f(x);
     lastY = f(y);
     lastTicks = ticks;
     publish();
   },
   get state() {
     return {
       current: current.slice(),
       selected,
       distance,
       scaleBytes: scaleBytes.value.toArray(),
       addBytes: addBytes.value.toArray(),
       enabled: enabled.value,
       flash: flashOn.value ? [...flashRGB.value.toArray(), flashA.value] : null
     };
   }
 };
}
