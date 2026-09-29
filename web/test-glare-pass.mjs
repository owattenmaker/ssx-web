import fs from 'node:fs';
import assert from 'node:assert/strict';
import {createGlarePainter,glareBytes,glarePlan,glareReference,copySprites,compositeSprites,GLARE_DEFAULTS} from './glare-pass.js';
// Original framebuffer glare pass 36C790 + world painter type 6 (see glare-pass.js).
// 1. Packages (tools/export_glare.py): ARA1 has no type-6 section, BRA2/BHP1 do.
// 2. Painter blend 2BD068 against live savestate memory: the Metro-City glide-620 and
//    The Junction glide painters (*(*(0x4FA370+8)), vtable 484DA8) are 9 / 14 driver steps
//    ahead of the render context copy (+6CD4, 36C740) held in the same savestate.
// 3. Pass parameters and the full GS draw list against the original 36C790 packets built by
//    tools/test_glare_pass_native.py on the Metro-City glide-620 RAM (case 0 = live state,
//    39 synthetic painter/debug states); the live case is also embedded below.
const f=Math.fround;
const ara1=JSON.parse(fs.readFileSync('public/assets/ARA1/glare.json')),bra2=JSON.parse(fs.readFileSync('public/assets/BRA2/glare.json'));
assert.equal(ara1.painter,null,'Snow Jam authors no type-6 painter');
assert.deepEqual(bra2.defaults,GLARE_DEFAULTS);
assert.equal(bra2.painter.payloads.length,5);
assert.deepEqual(bra2.painter.payloads[1],{rate:f(-.1),cutoff:f(1.6),post_cutoff_scale:1,copy_intensity:1,frame_source_intensity:1.5,frame_blend_intensity:f(.9),blend_texture2:0,blend_texture3:1});
// Snow Jam: the painter never leaves its reset state, so 36C790 returns at once.
{const p=createGlarePainter(ara1);p.step(-131865.234375,13858.5);assert.deepEqual(p.values,GLARE_DEFAULTS);assert.equal(glareBytes(p.values).enabled,false);assert.deepEqual(glarePlan(glareBytes(p.values)),[]);}
// 2BD068 from live memory: context (one copy behind) -> n steps of weight -rate toward the sample.
function blendSteps(pkg,payloadIndex,context,n){
 // Reproduce 2C0778's non-initial step with a stationary camera inside the payload's leaf.
 const tree=pkg.painter,leaf=tree.nodes.findIndex(node=>!(node[0]&1)&&(node[2]|(node[3]<<16))===payloadIndex);assert.ok(leaf>=0);
 const p=createGlarePainter({...pkg,painter:{...tree,root:leaf,scale:1,origin:[0,0]}});
 p.step(0,0);p.seed(context);for(let i=0;i<n;i++)p.step(0,0);return p.values;
}
const metroContext=[1.2406212091445923,1,1,1.2005192041397095,0.9598925709724426,0,0.4010432958602905];
assert.ok(metroContext.every(v=>f(v)===v));
const metroPainter=blendSteps(bra2,1,metroContext,9),metroLive=[1.271700143814087,1,1,1.2264188528060913,0.9547122120857239,0,0.4528425633907318];
metroPainter.forEach((v,i)=>assert.ok(v===metroLive[i],`Metro-City painter value ${i}: ${v} vs live ${metroLive[i]}`));
const bhp1=JSON.parse(fs.readFileSync('public/assets/BHP1/glare.json'));
const junction=blendSteps(bhp1,0,[1.1931724548339844,1,1,1,1,0,1],14);
assert.ok(junction[0]===1.1851611137390137,`Junction cutoff ${junction[0]}`);
// 36C790 parameters of the live Metro-City frame (context above; gp+12F8/12FC = 0, jitter 2, 2^8).
const live=glareBytes(metroContext);
assert.deepEqual(live,{enabled:true,alphas:[0,0,0,51],cut:158,cutScale:127,copy:31,source:153,frameBlend:122,jitter:32,sizes:[256,128,64,32]});
// Embedded original draws of that frame (decoded from the 36C790 packets).
const plan=glarePlan(live);
assert.deepEqual(plan.map(d=>[d.op,d.frame,d.tex,d.alpha,d.rgba[0],d.sprites.length]),[
 ['copy',[224,4],[0,8],null,153,8],['cutoff',[224,4],null,[1,0,2,2,127],158,8],
 ['copy',[256,2],[7168,4],null,31,4],['copy',[256,2],[7168,4],[0,2,2,1,128],31,4],['copy',[256,2],[7168,4],[0,2,2,1,128],31,4],['copy',[256,2],[7168,4],[0,2,2,1,128],31,4],
 ['copy',[224,1],[8192,2],null,31,2],['copy',[224,1],[8192,2],[0,2,2,1,128],31,2],['copy',[224,1],[8192,2],[0,2,2,1,128],31,2],['copy',[224,1],[8192,2],[0,2,2,1,128],31,2],
 ['copy',[256,1],[7168,1],null,31,1],['copy',[256,1],[7168,1],[0,2,2,1,128],31,1],['copy',[256,1],[7168,1],[0,2,2,1,128],31,1],['copy',[256,1],[7168,1],[0,2,2,1,128],31,1],
 ['composite',[0,8],[8192,1],[1,2,2,0,122],51,16]]);
assert.deepEqual(plan[0].sprites[0],[8,8,0,0,1032,7176,512,4096]);
assert.deepEqual(plan[3].sprites[0],[72,72,0,0,1080,4104,512,2048]);
assert.deepEqual(plan.at(-1).sprites[15],[488,0,7680,0,520,512,8192,7168]);
assert.deepEqual(copySprites([0,0,256,256],128,128,32,4)[2].sprites[0],[72,8,0,0,1080,4040,512,2048]);
assert.deepEqual(compositeSprites(256,256)[1],[264,0,512,0,520,4096,1024,7168]);
// Decoded original packets (development oracle output) for all captured cases.
const oraclePath='../local/browser-validation/glare-pass-packets.json';
let oracleCases=0;
if(fs.existsSync(oraclePath)){
 const bits=(v,lo,n)=>Number((v>>BigInt(lo))&((1n<<BigInt(n))-1n));
 const decode=words=>{
  const q=[];for(let k=0;k<words.length;k+=2)q.push([BigInt('0x'+words[k]),BigInt('0x'+words[k+1])]);
  const st={},out=[];let i=0,cur=null,uv=null;
  while(i<q.length){const qwc=bits(q[i][0],0,16);i++;const end=i+qwc;
   while(i<end){const nloop=bits(q[i][0],0,15);i++;
    for(let l=0;l<nloop;l++){const [v,rr]=q[i++],r=Number(rr&0xffn);
     if(r===0){st.prim={tme:bits(v,4,1),abe:bits(v,6,1)};cur=null;}
     else if(r===1)st.rgba=[0,8,16,24].map(s=>bits(v,s,8));
     else if(r===3)uv=[bits(v,0,14),bits(v,16,14)];
     else if(r===5){const xy=[bits(v,0,16),bits(v,16,16)];
      if(!cur){cur={state:JSON.parse(JSON.stringify(st)),sprites:[],pending:null};out.push(cur);}
      if(!cur.pending)cur.pending=[uv,xy];else{const [u0,p0]=cur.pending;cur.sprites.push(st.prim.tme?[...u0,...p0,...uv,...xy]:[...p0,...xy]);cur.pending=null;}}
     else if(r===0x4c)st.frame=[bits(v,0,9),bits(v,16,6)];
     else if(r===0x42)st.alpha=[bits(v,0,2),bits(v,2,2),bits(v,4,2),bits(v,6,2),bits(v,32,8)];
     else if(r===6)st.tex=[bits(v,0,14),bits(v,14,6)];
     else if(r===8)st.clamp=[bits(v,0,2),bits(v,2,2),bits(v,4,10),bits(v,14,10),bits(v,24,10),bits(v,34,10)];
     else if(r===0x40)st.scissor=[bits(v,0,11),bits(v,16,11),bits(v,32,11),bits(v,48,11)];
    }}}
  return out.map(d=>({frame:d.state.frame,scissor:d.state.scissor,tex:d.state.prim.tme?d.state.tex:null,clamp:d.state.prim.tme?d.state.clamp:null,alpha:d.state.prim.abe?d.state.alpha:null,rgba:d.state.rgba,sprites:d.sprites}));
 };
 const oracle=JSON.parse(fs.readFileSync(oraclePath));
 for(const [k,c] of oracle.cases.entries()){
  const [bt0,bt1,jitter,capture,enable]=c.debug;
  const bytes=glareBytes(c.params,{blend_texture0:bt0,blend_texture1:bt1,jitter,capture_log2:capture,enable,post_cutoff_scale:c.gp12e8});
  // One PRIM (ABE=1) covers samples 1..3 of a copy: merge draws that share their GS state.
  const expected=[];for(const {frame,scissor,tex,clamp,alpha,rgba,sprites} of glarePlan(bytes)){const d={frame,scissor,tex,clamp,alpha,rgba,sprites:[...sprites]},last=expected.at(-1),key=x=>JSON.stringify([x.frame,x.scissor,x.tex,x.clamp,x.alpha,x.rgba]);
   if(last&&alpha&&key(last)===key(d))last.sprites.push(...d.sprites);else expected.push(d);}
  assert.deepEqual(decode(c.main),expected,`original 36C790 packet case ${k}`);
  oracleCases++;
 }
}
// Byte model: a frame with one saturated block and a dark rest; the dark rest is only scaled
// by FrameBlend (122/128) while the glare spreads around the bright block.
const frame=new Uint8Array(512*448*4);for(let y=0;y<448;y++)for(let x=0;x<512;x++){const bright=x>=200&&x<264&&y>=160&&y<224,i=(y*512+x)*4;frame.set(bright?[255,255,255,255]:[40,60,90,255],i);}
const result=glareReference(frame,live);
assert.equal(result.levels.length,4);assert.deepEqual([result.levels[0].length,result.levels[3].length],[256*256*4,32*32*4]);
const px=(x,y)=>Array.from(result.frame.subarray((y*512+x)*4,(y*512+x)*4+3));
assert.deepEqual(px(5,5),[38,57,85],'dark frame: Cd*122>>7 plus zero glare');
assert.ok(px(232,192)[0]===255&&px(180,192)[0]>(40*122>>7),'glare spreads beyond the bright block');
console.log(`glare pass: packages, painter blend vs live memory (9/14 steps), parameters, draw list${oracleCases?` vs ${oracleCases} original packet cases`:' (oracle packets not present)'} and byte model pass`);
