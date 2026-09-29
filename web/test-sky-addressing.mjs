// Area sky texture addressing (web/world-material.js createOriginalSkyMaterials / originalModelWrap).
// The PS2 draws every ASKY..ESKY texture with GS CLAMP_1 = clamp/clamp: the sky's render-state push 353B10 sets
// [renderer+E84] word0 |= 0xC and the model material code 37F2BC..37F354 sets the same bits from material word +0C
// & 0x180000 (flags bits 3/4), which 363C20 hands to 3625C0 (mode 1 -> CLAMP 4 = WMT, 2 -> 1 = WMS, 3 -> 5 = both).
// The port used to sample them with Repeat (the package default in main.js asset()), so the bilinear filter mixed each
// edge with the opposite edge of the same texture: a half-transparent line along the bottom of the mountain ring (its
// bottom row is opaque, its top row transparent), a dark line at the top of the band below it, and vertical seams
// between the 45-degree ring segments (reported by players on every course, 2026-09-26).
// Checks, for the five sky packages (SKY = ASKY, BRA2/sky = BSKY, CRA3/sky, DRA4/sky, ERA5/sky):
//  - the flag decode table and that every sky material carries both clamp bits;
//  - starting from main.js's default (RepeatWrapping), building the sky materials leaves every sky texture ClampToEdge;
//  - a texel-level pixel check with the resulting sampler state: the bilinear sample exactly at each texture edge
//    (u or v = 0 / 1, where the ring and bands end) equals that edge's own texels, as the PS2's CLAMP gives, and the
//    ring's bottom edge stays opaque. Under Repeat the same samples miss by up to half the edge-to-edge difference.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import zlib from 'node:zlib';
import {Texture,RepeatWrapping,ClampToEdgeWrapping} from 'three/webgpu';
import {createOriginalSkyMaterials,originalModelWrap,originalModelBlend} from './world-material.js';

const PUBLIC=new URL('./public/',import.meta.url).pathname;
const SKIES={ASKY:'assets/SKY/',BSKY:'assets/BRA2/sky/',CSKY:'assets/CRA3/sky/',DSKY:'assets/DRA4/sky/',ESKY:'assets/ERA5/sky/'};

// ---- flag decode (37F2BC..37F354 / 3625C0) ----
assert.deepEqual(originalModelWrap(0),{wrapS:RepeatWrapping,wrapT:RepeatWrapping});
assert.deepEqual(originalModelWrap(8),{wrapS:ClampToEdgeWrapping,wrapT:RepeatWrapping},'0x80000 -> WMS clamp');
assert.deepEqual(originalModelWrap(16),{wrapS:RepeatWrapping,wrapT:ClampToEdgeWrapping},'0x100000 -> WMT clamp');
assert.deepEqual(originalModelWrap(24),{wrapS:ClampToEdgeWrapping,wrapT:ClampToEdgeWrapping});

// ---- texture archive entry -> RGBA (indexed PNG with PLTE/tRNS, or 8-bit RGB/RGBA) ----
const archives=new Map();
function archive(url){
 const file=PUBLIC+url.replace(/^\//,'');if(archives.has(file))return archives.get(file);
 const b=fs.readFileSync(file);assert.equal(b.toString('latin1',0,8),'SSXTEX01');
 const n=b.readUInt32LE(8),index=JSON.parse(b.toString('utf8',12,12+n)),start=12+n+((16-((12+n)%16))%16);
 const a={byId:new Map(index.entries.map(e=>[e.id,e])),png:e=>b.subarray(start+e.offset,start+e.offset+e.size)};archives.set(file,a);return a;
}
function decodePng(b){
 let at=8,w,h,depth,ct,plte=Buffer.alloc(0),trns=Buffer.alloc(0);const idat=[];
 while(at<b.length){const n=b.readUInt32BE(at),t=b.toString('latin1',at+4,at+8),d=b.subarray(at+8,at+8+n);at+=12+n;
  if(t==='IHDR'){w=d.readUInt32BE(0);h=d.readUInt32BE(4);depth=d[8];ct=d[9];}else if(t==='PLTE')plte=d;else if(t==='tRNS')trns=d;else if(t==='IDAT')idat.push(d);}
 const ch={2:3,6:4,3:1}[ct],bpp=Math.max(1,(ch*depth)>>3),stride=Math.ceil(w*ch*depth/8),raw=zlib.inflateSync(Buffer.concat(idat)),out=Buffer.alloc(w*h*4);let prev=Buffer.alloc(stride);
 for(let y=0;y<h;y++){
  const f=raw[y*(stride+1)],line=Buffer.from(raw.subarray(y*(stride+1)+1,(y+1)*(stride+1)));
  for(let i=0;i<stride&&f;i++){const a=i>=bpp?line[i-bpp]:0,u=prev[i],c=i>=bpp?prev[i-bpp]:0;let p=0;
   if(f===1)p=a;else if(f===2)p=u;else if(f===3)p=(a+u)>>1;else{const q=a+u-c,pa=Math.abs(q-a),pb=Math.abs(q-u),pc=Math.abs(q-c);p=pa<=pb&&pa<=pc?a:pb<=pc?u:c;}
   line[i]=(line[i]+p)&255;}
  for(let x=0;x<w;x++){const o=(y*w+x)*4;
   if(ct===6)line.copy(out,o,x*4,x*4+4);else if(ct===2){line.copy(out,o,x*3,x*3+3);out[o+3]=255;}
   else{const bit=x*depth,j=(line[bit>>3]>>(8-depth-(bit&7)))&((1<<depth)-1);plte.copy(out,o,j*3,j*3+3);out[o+3]=j<trns.length?trns[j]:255;}}
  prev=line;
 }
 return {w,h,rgba:out};
}
const texels=(root,t)=>{assert(t.pack!==undefined,'sky textures come from a texture archive');const a=archive(t.pack.startsWith('/')?t.pack:'/'+root+t.pack),e=a.byId.get(t.id);assert(e,`texture ${t.id} in ${t.pack}`);return decodePng(a.png(e));};

// Bilinear sample (texel centres at +0.5, as WebGPU/GL and the GS) at texture coordinate (u, v) with a wrap mode per axis.
const address=(i,n,wrap)=>wrap===ClampToEdgeWrapping?Math.min(n-1,Math.max(0,i)):((i%n)+n)%n;
function sample(img,u,v,wrapS,wrapT){
 const x=u*img.w-.5,y=v*img.h-.5,x0=Math.floor(x),y0=Math.floor(y),fx=x-x0,fy=y-y0,out=[0,0,0,0];
 for(const [dx,wx] of [[0,1-fx],[1,fx]])for(const [dy,wy] of [[0,1-fy],[1,fy]]){
  const o=(address(y0+dy,img.h,wrapT)*img.w+address(x0+dx,img.w,wrapS))*4;for(let c=0;c<4;c++)out[c]+=img.rgba[o+c]*wx*wy;
 }
 return out;
}
const texel=(img,x,y)=>Array.from(img.rgba.subarray((y*img.w+x)*4,(y*img.w+x)*4+4));
const maxDiff=(a,b)=>Math.max(...a.map((v,i)=>Math.abs(v-b[i])));

const rows=[];let edgeSamples=0;
for(const [sky,root] of Object.entries(SKIES)){
 const pkg=JSON.parse(fs.readFileSync(PUBLIC+root+'world.json','utf8'));
 assert.equal(pkg.sky_location,sky);assert.equal(pkg.batches.length,9,`${sky}: dome, band cap, four ring and three band textures`);
 for(const b of pkg.batches)assert.equal(b.material_flags&24,24,`${sky} ${b.texture}: material flags ${b.material_flags} carry both clamp bits (CLAMP_1 = 5)`);
 // main.js asset(): every package texture starts as RepeatWrapping, then the sky materials are built per batch.
 const textures={};for(const k of Object.keys(pkg.textures)){const t=new Texture();t.wrapS=t.wrapT=RepeatWrapping;textures[k]=t;}
 const materials=createOriginalSkyMaterials({textures});for(const b of pkg.batches)materials.material(b);
 for(const [k,t] of Object.entries(textures))assert.deepEqual([t.wrapS,t.wrapT],[ClampToEdgeWrapping,ClampToEdgeWrapping],`${sky} ${k}: clamp/clamp as the PS2's CLAMP_1`);
 let worstRepeat=0,ringBottomAlpha=255;
 for(const b of pkg.batches){
  const img=texels(root,pkg.textures['9-'+b.texture]),tex=textures['9-'+b.texture],{w,h}=img;
  // the four edges, at every texel centre along them: u = 0 / 1 across columns, v = 0 / 1 across rows
  const edges=[];for(let x=0;x<w;x++){const u=(x+.5)/w;edges.push([u,0,x,0],[u,1,x,h-1]);}for(let y=0;y<h;y++){const v=(y+.5)/h;edges.push([0,v,0,y],[1,v,w-1,y]);}
  for(const [u,v,x,y] of edges){
   const got=sample(img,u,v,tex.wrapS,tex.wrapT),want=texel(img,x,y);
   assert(maxDiff(got,want)<1e-6,`${sky} ${b.texture}: edge sample at (${u.toFixed(3)}, ${v.toFixed(3)}) ${got.map(Math.round)} != edge texel ${want} (the line/seam)`);
   worstRepeat=Math.max(worstRepeat,maxDiff(sample(img,u,v,RepeatWrapping,RepeatWrapping),want));edgeSamples++;
   if(originalModelBlend(b.material_flags)===2&&v===1)ringBottomAlpha=Math.min(ringBottomAlpha,got[3]);
  }
 }
 // The ring (blend class 2, alpha) ends opaque at its bottom edge (a few texels at 230..236 = 0.90..0.93, as authored).
 assert(ringBottomAlpha>=228,`${sky}: ring bottom edge alpha ${ringBottomAlpha}`);
 // Guard the check itself: Repeat really does break these edges (the reported line), so this test would catch a revert.
 assert(worstRepeat>=100,`${sky}: Repeat edge error ${worstRepeat} (expected the ring's transparent top row to bleed into its bottom edge)`);
 rows.push(`${sky} ring bottom alpha >= ${Math.round(ringBottomAlpha)}, Repeat would miss by ${Math.round(worstRepeat)}`);
}
console.log(`Sky texture addressing OK: 5 skies x 9 textures clamp/clamp, ${edgeSamples} edge samples equal their edge texels\n  ${rows.join('\n  ')}`);
