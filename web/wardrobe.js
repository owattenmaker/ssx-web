// Equip Gear and outfits (docs/characters.md "Equip Gear and outfits"): the original wardrobe rules, the outfit
// state per rider, the rider package of an outfit (assembled in the browser from tools/export_wardrobe.py parts) and
// the Equip Gear screen (FE.LUI 12equ_char, cFEStateCharEquip) for Setup Character and the career lodge.
//
// Original code (SLUS_207.72):
//   inventory rows R+0x290 {item, flags}: 0x2 owned, 0x10 equipped, 0x4 committed, 0x20 default (R = 0x4A6CA8 +
//   profile*0x9B50 + char*0xF88). Init 0x1513B8, equip 0x151C90 + rules 0x151EF0 (web/lodge.js GearInventory),
//   UI equip 0x14AFB0: equip, then 0x1521F0 (every default-table folder keeps an equipped flag-0x10 entry, else its
//   default item is equipped) and 0x1520E8 (equipped flag-0x1000 entries re-apply their rules); a failure restores
//   the rows ("You must remove items before equipping this accessory."). Commit 0x14AEA8 (0x4 = 0x10), restore
//   0x14AF10 (0x10 = 0x4, front-end transition 0x1A1F1C).
//   Race 0x14D068 (0x22ED5C): 0x10 |= committed; every committed NIS model of table 2 equips its race model; item 4.
//   Parts 0x11BBE8/0x11C138: equipped entries' models at their part slot (+0x10 low byte), a cheat skin takes its
//   whole bucket; slots 5/6/8/9/11 are hidden in the race (0x11C61C). Textures 0x11BE88/0x14B988 ('$' wildcards
//   from another equipped entry of the same texture group), bound by SSH name = material name.
//   Secondary motion enables 0x11CF70..0x11D1B8 by part slot (tools/export_wardrobe.py SECONDARY).
import {GearInventory} from './lodge.js';
import {LuiScreen} from './lui-player.js';
import {speakFrontEnd} from './rider-speech.js';
import {afb,clipSample,channelValue,previewRoot} from './fe-preview.js';
import {FOCUS_LAG} from './lui-flash.js';
import {mul as vmul,vuAdd as vadd,vuSub as vsub,fromBits,bitsOf} from './ee-scalar-float.js';
import {readJSON,writeJSON,storage as saveStorage} from './save-store.js';
import {archiveBlob} from './texture-archive.js';

const ROOT='/assets/WARDROBE/';
// The gameplay clip table (PS2 ANM.BIG sampled, tools/export_animation_samples.py) is the same for every original rider: one
// shared file, /assets/ANIMATIONS/animation-samples.json (web/prepare-ui.py --part animations). Sam's packages keep their own
// (another bank).
export const SHARED_SAMPLES='/assets/ANIMATIONS/animation-samples.json';
export function riderSamplesUrl(root){return /^\/assets\/RIDER_/.test(root)&&!/^\/assets\/RIDER_SAM(_|\/)/.test(root)?SHARED_SAMPLES:root+'animation-samples.json';}
const OUTFIT_KEY='ssx3.outfit.v1',FREE_KEY='ssx3.outfit.free.v1';
const BUILD_ORDER=['suit','boot','head','bord','alph'];   // tools/export_characters.py build_package texture order
const cache=new Map(),virtual=new Map(),worn=new Map();
// true while `riderId` wears a non-default outfit (web/character-select.js then shows the race rider, not the FE package)

const storage=()=>{try{return globalThis.localStorage??null;}catch{return null;}};
// wardrobe.json (items, rules, parts, textures) and, when a package is built, parts.bin (geometry and skin).
export async function loadWardrobe(riderId,fetcher=globalThis.fetch,{parts=true}={}){
 const id=riderId.toUpperCase();
 if(!cache.has(id))cache.set(id,fetcher(`${ROOT}${id}/wardrobe.json`).then(r=>{if(!r.ok)throw Error(`${id}/wardrobe.json: ${r.status}`);return r.json();}).catch(e=>{cache.delete(id);throw e;}));
 const w=await cache.get(id);
 if(parts&&!w.bin)w.bin=await (w._bin??=fetcher(`${ROOT}${id}/parts.bin`).then(r=>{if(!r.ok)throw Error(`${id}/parts.bin: ${r.status}`);return r.arrayBuffer();}).catch(e=>{w._bin=null;throw e;}));
 return w;
}

// ---- the original rules ----------------------------------------------------------------------------------
export function gearTable(w){return {entries:w.entries.map(e=>e.slice(0,8)),rules:w.rules,defaults:w.defaults.map(d=>d[1])};}
const entryOf=e=>({item:e[0],cls:e[1],parent:e[2],order:e[3],flags:e[4],price:e[5],tier:e[6],name:e[7],group:e[8],slot:e[9],model:e[10],texture:e[11],icon:e[12],weight:e[13]??0});
export function entries(w){return w._entries??=w.entries.map(entryOf);}

export class Wardrobe {
  constructor(w, flags = null, { allOwned = false } = {}) {
    this.w = w;
    this.inv = new GearInventory(gearTable(w), flags);
    this.list = entries(w);
    this.byItem = new Map();
    for (const e of this.list) if (!this.byItem.has(e.item)) this.byItem.set(e.item, e);
    if (allOwned) {
      this.allOwned = true;
      for (const e of this.list) this.inv.set(e.item, this.flags(e.item) | 2);
    } /* free play: every item owned (0x2) */
  }
  flags(item) {
    return this.inv.flags(item);
  }
  equipped(item) {
    return this.inv.equipped(item);
  }
  owned(item) {
    return this.inv.owned(item);
  }
  // 0x14AFB0 (UI equip / unequip with validation): returns false when the original refuses (rows restored).
  set(item, on) {
    const backup = new Map(this.inv.f),
      fail = () => {
        this.inv.f = backup;
        return false;
      };
    this.inv.equip(item, on); // 0x151C90's result is ignored; the item's own flag is then forced
    if (this.byItem.has(item)) this.inv.set(item, on ? this.flags(item) | 0x10 : this.flags(item) & ~0x10);
    if (!this.required()) return fail();
    let applied = false;
    if (
      !this.fix((v) => {
        applied = v;
      })
    )
      return fail();
    if (applied && !this.required()) return fail();
    return true;
  }
  // 0x1521F0: every default-table row (class A, item B) needs an equipped class descendant of A with entry flag 0x10, else B is equipped.
  required() {
    let ok = true;
    for (const [folder, item] of this.w.defaults) {
      const under = this.classChildren(folder);
      if (under.some((e) => this.equipped(e.item) && e.flags & 0x10)) continue;
      if (!this.inv.equip(item, 1)) ok = false;
    }
    return ok;
  }
  // 0x14D608(db, char, A, out, depth -1, folders 1): the entries whose class (+6) is A, recursively through their own.
  classChildren(parent, seen = new Set()) {
    const out = [];
    if (seen.has(parent)) return out;
    seen.add(parent);
    for (const e of this.inv.entries)
      if (e.cls === parent) {
        out.push(e);
        out.push(...this.classChildren(e.item, seen));
      }
    return out;
  }
  // 0x1520E8: equipped entries with flag 0x1000 re-apply their equip rules.
  fix(report) {
    let any = false;
    for (const e of this.inv.entries)
      if (e.flags & 0x1000 && this.equipped(e.item)) {
        any = true;
        if (!this.inv.apply(e.item, 1)) {
          report(any);
          return false;
        }
      }
    report(any);
    return true;
  }
  commit() {
    for (const [k, v] of this.inv.f) this.inv.f.set(k, v & 0x10 ? v | 4 : v & ~4);
  } // 0x14AEA8
  restore() {
    for (const [k, v] of this.inv.f) this.inv.f.set(k, v & 4 ? v | 0x10 : v & ~0x10);
  } // 0x14AF10
  // 0x14D068: the race set from the committed rows.
  // working=true: the Equip Gear preview (rows as edited, before the 0x14AEA8 commit).
  raceEquipped(working = false) {
    const f = new Map([...this.inv.f].map(([k, v]) => [k, working ? (v & 0x10 ? v | 4 : v & ~4) : v & 4 ? v | 0x10 : v]));
    for (const [nis, race] of this.w.race_models) if ((f.get(nis) ?? 0) & 4 && race >= 0 && f.has(race)) f.set(race, f.get(race) | 0x10);
    if (f.has(this.w.pda_item)) f.set(this.w.pda_item, f.get(this.w.pda_item) | 0x10);
    return this.list.filter((e) => (f.get(e.item) ?? 0) & 0x10);
  }
  // the front-end preview set: the equipped rows as they are (the NIS head/eyes/hands, no race models, no PDA)
  feEquipped() {
    return this.list.filter((e) => this.flags(e.item) & 0x10);
  }
  serialize() {
    return this.inv.serialize();
  }
  // 0x14B478: the outfit's item weight (sum of +0xC over the equipped rows).
  weight() {
    let n = 0;
    for (const e of this.list) if (this.equipped(e.item)) n += e.weight;
    return n;
  }
  // 0x19B8F0: the weight change a toggle of `item` would make (0x14AFB0 simulated on a copy), null when refused.
  delta(item) {
    const backup = new Map(this.inv.f),
      before = this.weight(),
      on = !(this.flags(item) & 4);
    const ok = this.set(item, on),
      after = this.weight();
    this.inv.f = backup;
    return ok ? after - before : null;
  }
  // Equip Gear list 0x19B180 (equip mode): named owned leaves and folders holding one, by +0xA.
  menu(folder) {
    return this.inv.equipList(folder);
  }
  // Cross on an item (0x19BEE8): toggle by the committed bit, then commit (0x14AEA8).
  toggle(item) {
    const ok = this.set(item, !(this.flags(item) & 4));
    if (ok) this.commit();
    return ok;
  }
}

// 0x14B988: '$' wildcards take the same positions of another equipped entry's texture of the same group.
export function resolveTexture(e,equipped){
 if(e.group===-1||!e.texture)return null;const start=e.texture.indexOf('$');if(start<0)return e.texture;
 const out=[...e.texture];
 for(const o of equipped){
  if(o.group!==e.group||!o.texture||o.item===e.item)continue;
  for(let k=start;k<out.length&&out[k]!=='.';k++)if(out[k]==='$'&&k<o.texture.length&&o.texture[k]!=='$')out[k]=o.texture[k];
  if(!out.join('').slice(start).split('.')[0].includes('$'))break;
 }
 return out.join('');
}

// Parts (slot order) and resolved textures of an equipped set; `cheat` = the whole bucket with raw texture names.
export function assembly(w,equipped,cheat=false){
 const bySlot=new Map();   // 0x30D8B8: the first model added for a file id (part slot) makes the part
 for(const e of equipped)if(e.model&&e.slot!==255&&!bySlot.has(e.slot))bySlot.set(e.slot,e);
 const parts=[...bySlot.keys()].sort((a,b)=>a-b).map(slot=>({slot,resource:bySlot.get(slot).model,entry:bySlot.get(slot)}));
 const textures=[];
 for(const e of equipped){const t=cheat?e.texture:resolveTexture(e,equipped);if(t&&!textures.includes(t))textures.push(t);}
 return {parts,textures};
}
export function secondaryEnables(w,slots){return w.secondary_slots.map(list=>list.some(s=>slots.includes(s)));}

// ---- inverse bind matrices and bone masks (cAnimModel compile 0x30DBD0, VU0 round-toward-zero) -------------
// 0x30E560 local matrix from the authored record, 0x30E5F4 world = bank[parent] * local (vmulax/vmadday/vmaddaz/
// vmaddw per column), 0x30E748 in-place rigid inverse (transpose, row3 = R^T(-t) with w 0, row3.w = 1). Every
// multiply and add rounds toward zero (PCSX2 VU "Chop"); bit-exact with the live banks of all 30 characters.
// DaZ inputs and FtZ results (PCSX2 VU MXCSR): a denormal is a signed zero (e.g. 2*7.1e-20 * 7.1e-20 in a hips quaternion).
const TINY=2**-126,ftz=v=>v!==0&&Math.abs(v)<TINY?(v<0?-0:0):v;
const fmul=(a,b)=>ftz(vmul(ftz(a),ftz(b))),fadd=(a,b)=>ftz(vadd(ftz(a),ftz(b))),fsub=(a,b)=>ftz(vsub(ftz(a),ftz(b)));
const madd=(acc,a,b)=>fadd(acc,fmul(a,b));
function localMatrix(t,q){
 const [x,y,z,w]=q,X=fadd(x,x),Y=fadd(y,y),Z=fadd(z,z);
 const xx=fmul(X,x),yy=fmul(Y,y),zz=fmul(Z,z),xw=fmul(X,w),yw=fmul(Y,w),zw=fmul(Z,w);
 const yz=fadd(fmul(Y,z),0),zx=fadd(fmul(Z,x),0),xy=fadd(fmul(X,y),0);
 return [[fsub(fsub(1,yy),zz),fadd(fadd(0,xy),zw),fsub(fadd(0,zx),yw),0],[fsub(fadd(0,xy),zw),fsub(fsub(1,zz),xx),fadd(fadd(0,yz),xw),0],
  [fadd(fadd(0,zx),yw),fsub(fadd(0,yz),xw),fsub(fsub(1,xx),yy),0],t.slice()];
}
function mulMatrix(P,L){return L.map(v=>[0,1,2,3].map(c=>madd(madd(madd(fmul(P[0][c],v[0]),P[1][c],v[1]),P[2][c],v[2]),P[3][c],v[3])));}
function inverseMatrix(M){
 const R=[0,1,2].map(r=>[M[0][r],M[1][r],M[2][r],M[r][3]]),t=M[3],n=t.map(v=>fmul(v,-1));n[3]=0;
 const row=[0,1,2,3].map(c=>madd(madd(madd(fmul(R[0][c],n[0]),R[1][c],n[1]),R[2][c],n[2]),t[c],n[3]));row[3]=1;
 return [...R,row];
}
export function bindMatrixWords(bones){
 const world=[];
 return bones.map((b,i)=>{
  const L=localMatrix(b.source_translation_bits.map(fromBits),b.source_rotation_bits.map(fromBits));
  world[i]=b.parent<0?L:mulMatrix(world[b.parent],L);
  return inverseMatrix(world[i]).flat().map(v=>bitsOf(v)>>>0);
 });
}
// 0x11C298 masks via 0x310CE8 over compile's slots: bone names of file 0 plus the file-7 (hands) "morph" bit at
// slot_count + that part's morph index (running count of morph-bearing parts in slot order).
const LIST_8C0='lowerspine,middlespine,upperspine,neck,head,clavicleleft,bicepleft,biceptwistleft,forearmleft,handleft,clavicleright,bicepright,biceptwistright,forearmright,handright'.split(',');
const LIST_8C8=LIST_8C0.slice(2),LIST_8D0=['neck','head','clavicleleft','clavicleright'];
export function geometryMasks(w,asm){
 let count=0,morph=0;const table=new Map();
 for(const p of asm.parts){const part=w.parts[p.resource];table.set(p.slot,{base:count,part,morph:part.morph_count>0?morph++:-1});count+=part.bone_count;}
 const bits = (file, names) => {
   const t = table.get(file);
   if (!t) return 0n;
   let out = 0n;
   for (const n of names) {
     if (n === 'morph') {
       if (t.morph >= 0) out |= 1n << BigInt((count + t.morph) & 63);
       continue;
     }
     const k = t.part.bones?.findIndex((b) => b.name === n) ?? -1;
     if (k >= 0) out |= 1n << BigInt(t.base + k);
   }
   return out;
 };
 const morph7=bits(7,['morph']),hex=v=>'0x'+v.toString(16);
 // morph_bits: each morph part's slot bit (slot count + its morph index), as 30F2B0's morph slots
 const morph_bits=new Map([...table].filter(([,v])=>v.morph>=0).map(([k,v])=>[k,count+v.morph]));
 return {slot_count:count,morph_bits,base:new Map([...table].map(([k,v])=>[k,v.base])),upper_mask8c0:hex(bits(0,LIST_8C0)|morph7),upper_mask8c8:hex(bits(0,LIST_8C8)|morph7),upper_mask8d0:hex(bits(0,LIST_8D0))};
}

// ---- the rider package of an outfit (the tools/export_characters.py build_package + web_package layout) ------
// fe=true: the front-end preview assembly (every part drawn: the NIS head, eyes and hands with their morph targets,
// tools/export_fe_preview.py layout: parts with vertex/index ranges, board flag and morphs, plus morphs.bin).
export function buildPackage(w,asm,{riderId,fe=false,texturePath=stem=>`../textures/${stem}.png`}={}){
 const visible=asm.parts.filter(p=>fe||!w.hidden_slots.includes(p.slot)).map(p=>({...p,part:w.parts[p.resource]}));
 for(const p of visible)if(!p.part?.vertex_count)throw Error(`Missing wardrobe part ${p.resource}`);
 // bones: unique (file,index) in part order, parents resolved, Z-up -> Y-up, animation channels per file
 const bones=[],boneMap=new Map();
 for(const {part} of visible)for(const b of part.bones){const k=`${b.file}:${b.index}`;if(!boneMap.has(k)){boneMap.set(k,bones.length);bones.push(structuredClone(b));}}
 for(const b of bones){
  const k=`${b.parent_file}:${b.parent_index}`;
  if(b.parent_file===-1&&b.parent_index===-1)b.parent=-1;else if(boneMap.has(k))b.parent=boneMap.get(k);else throw Error(`Missing parent bone ${k}`);
  const [x,y,z]=b.translation;b.translation=[x,z,-y];const [qx,qy,qz,qw]=b.rotation;b.rotation=[qx,qz,-qy,qw];
 }
 const cursors=new Map();
 for(const b of [...bones].sort((a,c)=>a.file-c.file||a.index-c.index)){
  let cursor=cursors.get(b.file)??0;
  b.animation_translation_channel=b.dof_flags&1?cursor:-1;cursor+=b.dof_flags&1?3:0;
  b.animation_rotation_channel=b.dof_flags&2?cursor:-1;cursors.set(b.file,cursor+(b.dof_flags&2?3:0));
 }
 // textures: the resolved names; a material binds the loaded texture of its SSH name
 const byName=new Map();
 for(const t of asm.textures){const stem=t.replace(/\.gsh$/,'');const info=w.textures[stem];if(info&&!byName.has(info.name))byName.set(info.name,stem);}
 const chosen=new Map();
 for(const {resource,part} of visible)for(const b of part.batches){const stem=byName.get(b.material)??null;chosen.set(`${resource}|${b.material}`,stem);}
 const used=[...new Set([...chosen.values()].filter(Boolean))];
 const rank=stem=>Math.min(...[...chosen].filter(([,v])=>v===stem).map(([k])=>{const m=k.split('|')[1];const i=BUILD_ORDER.indexOf(m);return i<0?9:i;}));
 const resourceOf=stem=>w.textures[stem].resource;
 used.sort((a,b)=>rank(a)-rank(b)||(resourceOf(a)<resourceOf(b)?-1:resourceOf(a)>resourceOf(b)?1:0));
 const textures={},textureId=new Map();
 /* the rider's texture archives (WARDROBE/<ID>/textures.tex default outfits, gear.tex the other items; id = stem; tools/export_rider_textures.py), else the older PNG files */
 used.forEach((stem, i) => {
   const t = w.textures[stem];
   textures[`9-${i}`] = {
     width: t.width,
     height: t.height,
     ...((t.pack ?? w.texture_pack) ? { pack: t.pack ?? w.texture_pack, id: stem } : { path: texturePath(stem) }),
     source: 'gamecube',
     resource: t.resource,
     archive: t.archive,
     texel_domain: t.texel_domain,
     ps2_rgb: t.ps2_rgb,
     equipped_variant_verified: true
   };
   textureId.set(stem, i);
 });
 // geometry
 let vertexCount=0,indexCount=0;for(const {part} of visible){vertexCount+=part.vertex_count;indexCount+=part.index_count;}
 const vertices=new Float32Array(vertexCount*10),indices=new Uint32Array(indexCount),colors=new Float32Array(vertexCount*4).fill(1);
 const skin=[],sourceSkin=[],batches=[],parts=[],view=new DataView(w.bin),morphChunks=[];let vbase=0,ibase=0,morphBytes=0;
 // the race morph parts (web/board-flex.js): board-flex / hand-morphs / special-morphs files
 const morphParts={};
 for(const {resource,part} of visible){
  vertices.set(new Float32Array(w.bin,part.vertex_offset,part.vertex_count*10),vbase*10);
  const local=new Uint32Array(w.bin,part.index_offset,part.index_count);
  for(const b of part.batches){
   const stem=chosen.get(`${resource}|${b.material}`);
   batches.push({first_index:ibase+b.first,index_count:b.count,texture:stem!=null?textureId.get(stem):-1,lightmap:-1,instance:true,material:b.material,...(fe?{part:parts.length}:{})});
  }
  for(let i=0;i<part.index_count;i++)indices[ibase+i]=local[i]+vbase;
  for(let v=0;v<part.vertex_count;v++){
   const at=part.skin_offset+v*20,n=view.getUint8(at);const group=[];let total=0;
   for(let k=0;k<n;k++){const file=view.getUint8(at+4+k*4),bone=view.getUint8(at+5+k*4),weight=view.getInt16(at+6+k*4,true);group.push([file,bone,weight]);total+=weight;}
   skin.push(group.map(([file,bone,weight])=>[boneMap.get(`${file}:${bone}`),weight/total]));
   sourceSkin.push(group.map(([file,bone,weight])=>[boneMap.get(`${file}:${bone}`),weight]));
  }
  const entry={part:part.resource.replace(/^[^_]+_/,'').replace(/\.mnf$/i,''),resource:part.resource,model:part.name,morph_count:part.morph_count,source_sha256:part.source_sha256};
  if(fe){
   const morphs = (part.morphs || []).map((m) => {
     const chunk = new Float32Array(w.bin.slice(m.offset, m.offset + part.vertex_count * 12));
     const out = { channel: m.channel, offset: morphBytes, vertices: m.vertices };
     morphChunks.push(chunk);
     morphBytes += chunk.byteLength;
     return out;
   });
   Object.assign(entry,{file:part.slot,first_vertex:vbase,vertex_count:part.vertex_count,first_index:ibase,index_count:part.index_count,board:/^board_/i.test(part.resource),morphs});
  }
  // board flex (web/board-flex.js, tools/export_board_flex.py's board-flex.json / .bin): the race board's 8 morph targets
  if (!fe && part.morphs?.length && part.slot === 2) morphParts['board-flex'] = raceBoardFlex(w, part, vbase);
  if (!fe && part.morphs?.length && part.slot === 7) morphParts['hand-morphs'] = raceBoardFlex(w, part, vbase);
  if (!fe && part.morphs?.length && part.slot === 46) morphParts['special-morphs'] = raceBoardFlex(w, part, vbase);
  parts.push(entry);
  vbase+=part.vertex_count;ibase+=part.index_count;
 }
 if(batches.some(b=>b.texture<0))throw Error(`${riderId}: a material has no loaded texture`);
 // bone slots: the parts of every slot (hidden ones too) in slot order (geometry part +4 base)
 const geometry=geometryMasks(w,asm),count=geometry.slot_count;
 for (const part of Object.values(morphParts)) {
  const bit = geometry.morph_bits.get(part.meta.file);
  if (bit !== undefined) part.meta.slot_bit = bit;
 }
 const slots=bones.map(b=>geometry.base.get(b.file)+b.index);
 const min=[0,1,2].map(k=>Math.min(...Array.from({length:vertexCount},(_,i)=>vertices[i*10+k]))),max=[0,1,2].map(k=>Math.max(...Array.from({length:vertexCount},(_,i)=>vertices[i*10+k])));
 const hair=visible.filter(p=>p.part.bones.some(b=>b.name.startsWith('sec_'))).map(p=>p.part.resource);
 const rig = {
   bones,
   skin,
   source_skin: sourceSkin,
   source_skin_weight_units: 'integer-percent',
   parts,
   units: 'meters',
   up_axis: 'Y',
   hairstyle: { parts: hair, secondary_motion: 'settings original_animation.secondary_motion.enabled from the part slots (0x11CF70)' },
   character: riderId,
   resource_prefix: w.prefix,
   texture_archive: Object.values(textures)[0]?.archive,
   texture_selection_verified: false,
   source_bone_slots: slots,
   source_bone_slot_count: count,
   source_bind_matrix_words: bindMatrixWords(bones),
   source_bind_matrix_space: 'source-centimeters-Z-up',
   source_bind_provenance:
     'web/wardrobe.js bindMatrixWords: compile 0x30DBD0 from the authored bones (VU0 chop), bit-exact with the live banks',
   outfit: { parts: asm.parts.map((p) => p.resource), textures: asm.textures }
 };
 const world={version:1,location:`RIDER_${riderId.toUpperCase()}`,vertex_stride:40,vertex_count:vertexCount,index_count:indexCount,bounds:[min,max],batches,textures,
  lighting_verified:false,imported_patch_count:0,imported_instance_count:visible.length,missing_textures:[],source:`web/wardrobe.js outfit of ${riderId}`};
 const settings={bone_mask:2**bones.length-1,secondary:secondaryEnables(w,visible.map(p=>p.slot)),
  identity:{upper_mask8c0:geometry.upper_mask8c0,upper_mask8c8:geometry.upper_mask8c8,upper_mask8d0:geometry.upper_mask8d0}};
 if(fe){rig.fe=w.fe||null;world.location+='/fe';}
 const morphs=new Float32Array(morphBytes/4);let at=0;for(const c of morphChunks){morphs.set(c,at);at+=c.length;}
 return {world,rig,vertices,indices,colors,settings,morphs,morphParts};
}
// A race morph part's targets in board-flex.json / .bin form (the board, file 2; the race hands, file 7; Stretch's SpecialA, file 46): the dense deltas of the part's vertices (the FE package's layout),
// the mirror table = the MNF morph_ids (the geometry's part+0x40).
function raceBoardFlex(w, part, firstVertex) {
  const bytes = part.vertex_count * 12;
  const bin = new Float32Array(part.morphs.length * part.vertex_count * 3);
  const offsets = [];
  part.morphs.forEach((m, i) => {
    bin.set(new Float32Array(w.bin.slice(m.offset, m.offset + bytes)), i * part.vertex_count * 3);
    offsets.push(i * bytes);
  });
  const meta = {
    version: 1,
    resource: part.resource,
    file: part.slot,
    first_vertex: firstVertex,
    vertex_count: part.vertex_count,
    morph_count: part.morphs.length,
    mirror: part.morphs.map((m) => m.channel),
    offsets
  };
  return { meta, bin };
}

// ---- virtual package files (main.js load() consults wardrobeFile first) ----------------------------------
export function wardrobeFile(path,type='json'){
 const v=virtual.get(path);if(v===undefined)return undefined;
 return type==='json'?JSON.parse(v):v.slice(0);
}
function hash(text){let h=0x811c9dc5;for(let i=0;i<text.length;i++){h^=text.charCodeAt(i);h=Math.imul(h,0x01000193)>>>0;}return h.toString(16).padStart(8,'0');}

// ---- outfit state -----------------------------------------------------------------------------------------
// One record per rider, as the original's profile record (R = 0x4A6CA8 + profile*0x9B50 + char*0xF88; single event
// and career both use profile 0): the browser career save (web/career.js rider(id).gearFlags, localStorage
// ssx3.career.v2, web/career-save.js). Older browser saves load: a single-event outfit of the first version (localStorage ssx3.outfit.v1)
// moves into the record when the record still has the fresh outfit, and rows equipped by the old lodge screen
// without the commit bit are committed (0x14AEA8). Without the career data (no CAREER assets) the single-event
// store stays in ssx3.outfit.v1.
export function readOutfits(){try{return JSON.parse(storage()?.getItem(OUTFIT_KEY)||'{}')||{};}catch{return {};}}
export function writeOutfit(riderId, flags) {
  try {
    const all = readOutfits();
    if (flags) all[riderId] = flags;
    else delete all[riderId];
    if (Object.keys(all).length) storage()?.setItem(OUTFIT_KEY, JSON.stringify(all));
    else storage()?.removeItem(OUTFIT_KEY);
    return true;
  } catch {
    return false;
  }
}
function careerOf(ui){return ui?.careerUI?.career||null;}
// Free play (Single Event and online, i.e. not Conquer the Mountain): every rider's whole wardrobe is unlocked (all
// items owned) and the outfit is kept per rider in its own store (ssx3.outfit.free.v1), so free-play outfits never
// touch the career record, its purchases or its unlocks. The career keeps the original shared profile record.
const freePlay=ui=>!ui?.careerMode;
// atomic write / interrupted-write recovery: web/save-store.js (part of the browser save file, Export/Import)
function readFree(){return readJSON(FREE_KEY,v=>(v&&typeof v==='object'&&!Array.isArray(v)?v:null),saveStorage())||{};}
function writeFree(riderId,flags){const all=readFree();if(flags)all[riderId]=flags;else delete all[riderId];return writeJSON(FREE_KEY,all,saveStorage());}
function isDefault(wd){const fresh=new Wardrobe(wd.w);const a=wd.raceEquipped().map(e=>e.item).join(','),b=fresh.raceEquipped().map(e=>e.item).join(',');return a===b;}
function uncommitted(wd){for(const [,v] of wd.inv.f)if(!!(v&0x10)!==!!(v&4))return true;return false;}
export async function outfitState(ui,rider){
 // online races stream the skin palette of the rider's package (web/net): they keep the default outfit
 // online races show the worn outfit to the others too (web/net/mp-game.js outfitKey -> remoteOutfitRider)
 if(!rider||rider.kind==='cheat'||(rider.kind==='custom'&&rider.id!=='sam'))return null;   // Sam: the Sam build's wardrobe
 const w=await loadWardrobe(rider.id,globalThis.fetch,{parts:false}),career=careerOf(ui);
 // free play starts from the outfit the rider already wears: its free-play save, else the career record, else an old single-event save
 if(freePlay(ui)){const wd=new Wardrobe(w,readFree()[rider.id]||(career&&career.rider(rider.id).gearFlags)||readOutfits()[rider.id]||null,{allOwned:true});if(uncommitted(wd))wd.commit();return wd;}
 if(!career)return new Wardrobe(w,readOutfits()[rider.id]||null);
 const record=career.rider(rider.id),legacy=readOutfits()[rider.id];
 let wd=new Wardrobe(w,record.gearFlags||null),changed=false;
 if(legacy){const old=new Wardrobe(w,legacy);if(!record.gearFlags||isDefault(wd)){wd=old;changed=true;}writeOutfit(rider.id,null);}
 if(uncommitted(wd)){wd.commit();changed=true;}
 if(changed||!record.gearFlags){record.gearFlags=wd.serialize();if(career._gearId===rider.id)career._gear=null;if(changed)career.persist();}
 return wd;
}
// What outfitState reads for `rider`, as a string (synchronous, nothing created or written): a rider entry resolved with
// one stamp needs resolving again when the stamp changes (web/career-rider.js; the original assembles the race rider from
// the profile's committed rows at every world load, 0x14D068 from 0x22ED5C).
export function outfitStamp(ui,rider){
 if(!rider||rider.kind==='cheat'||(rider.kind==='custom'&&rider.id!=='sam'))return 'fixed';
 const career=careerOf(ui),record=career?.save?.riders?.[rider.id];
 if(freePlay(ui))return 'f:'+JSON.stringify(readFree()[rider.id]||record?.gearFlags||readOutfits()[rider.id]||null);
 return (career?'c:':'o:')+JSON.stringify(record?.gearFlags||null)+JSON.stringify(readOutfits()[rider.id]||null);
}
export function saveOutfit(ui,rider,wd){
 if(freePlay(ui)){writeFree(rider.id,wd.serialize());return;} // every committed row, front-end-only items too
 const career=careerOf(ui);
 if(career){career.rider(rider.id).gearFlags=wd.serialize();if(career._gearId===rider.id)career._gear=null;career.persist();}
 else writeOutfit(rider.id,isDefault(wd)?null:wd.serialize());
}

// The packages of a rider's current outfit: the race package (/assets/WARDROBE/<ID>/o<hash>/) and the front-end
// preview package (/f<hash>/, web/fe-preview.js), both served by wardrobeFile. The default outfit keeps the
// verified RIDER_<ID> and RIDER_<ID>/fe packages.
export async function prepareOutfit(ui,rider){
 const wd=await outfitState(ui,rider);if(!wd){if(rider)worn.delete(rider.id);return null;}
 if(wd.w.sam&&!wd.w.parts_mode)return samOutfit(rider,wd);   // Sam in parts mode (tools/sam_wardrobe.py) takes the riders' path
 if(isDefault(wd)){worn.delete(rider.id);return {wd,default:true};}
 const w=await loadWardrobe(rider.id);
 const {root,key}=await raceRoot(w,wd,rider);
 let feRoot=null;
 if(w.fe){
  const fasm=assembly(w,wd.feEquipped()),fkey=hash(JSON.stringify([fasm.parts.map(p=>p.resource),fasm.textures]));
  feRoot=`${ROOT}${rider.id.toUpperCase()}/f${fkey}/`;
  if(!virtual.has(feRoot+'world.json')){
   const pkg=buildPackage(w,fasm,{riderId:rider.id,fe:true});
   virtual.set(feRoot+'world.json',JSON.stringify(pkg.world));virtual.set(feRoot+'rider.json',JSON.stringify(pkg.rig));
   virtual.set(feRoot + 'vertices.bin', pkg.vertices.buffer);
   virtual.set(feRoot + 'indices.bin', pkg.indices.buffer);
   virtual.set(feRoot + 'colors.bin', pkg.colors.buffer);
   virtual.set(feRoot + 'morphs.bin', pkg.morphs.buffer);
  }
 }
 const state={wd,root,key,feRoot,settings:JSON.parse(virtual.get(root+'settings'))};worn.set(rider.id,state);
 return state;
}
// Sam (the port's own rider, config/characters/sam.json): the Sam PS2 build's items pick one of Sam's whole-outfit
// packages (tools/build_sam_web.py, owned by the Sam agent): the outfit whose `items` are all committed, the most
// specific first, when its package is in the browser assets; else the default RIDER_SAM.
const packageCache=new Map();
async function packageAvailable(pkg,file='world.json'){
 const key=pkg+'/'+file;
 if(!packageCache.has(key))packageCache.set(key,fetch(`/assets/${pkg}/${file}`,{method:'HEAD'}).then(r=>r.ok&&(r.headers.get('content-type')||'').includes('json')).catch(()=>false));
 return packageCache.get(key);
}
export function samOutfitChoice(w,wd){
 const committed=new Set(wd.list.filter(e=>wd.flags(e.item)&4).map(e=>e.item)),sam=w.sam;
 return sam.outfits.filter(o=>o.package&&o.items.length&&o.items.every(i=>committed.has(i))).sort((a,b)=>b.items.length-a.items.length||(a.default?1:0)-(b.default?1:0));
}
async function samOutfit(rider,wd){
 const sam=wd.w.sam;let pkg=sam.default_package,outfit=sam.outfits.find(o=>o.package===pkg)?.id;
 for(const o of samOutfitChoice(wd.w,wd))if(await packageAvailable(o.package)){pkg=o.package;outfit=o.id;break;}
 if(pkg===sam.default_package){worn.delete(rider.id);return {wd,default:true,package:pkg,outfit};}
 const fe=await packageAvailable(pkg,'fe/rider.json');
 const state={wd,package:pkg,outfit,key:pkg,feRoot:fe?`/assets/${pkg}/fe/`:null,settingsPackage:sam.settings_package};worn.set(rider.id,state);
 return state;
}
// The race package of a wardrobe state at /assets/WARDROBE/<ID>/o<hash>/ (served by wardrobeFile).
async function raceRoot(w,wd,rider,fetcher=globalThis.fetch){
 if(!w.bin)await loadWardrobe(rider.id,fetcher);
 const asm=assembly(w,wd.raceEquipped()),key=hash(JSON.stringify([asm.parts.map(p=>p.resource),asm.textures]));
 const root=`${ROOT}${rider.id.toUpperCase()}/o${key}/`;
 if(!virtual.has(root+'world.json')){
  const pkg=buildPackage(w,asm,{riderId:rider.id}),samples=await (await fetcher(riderSamplesUrl(`/assets/${rider.package}/`))).text();
  virtual.set(root+'world.json',JSON.stringify(pkg.world));virtual.set(root+'rider.json',JSON.stringify(pkg.rig));
  virtual.set(root+'vertices.bin',pkg.vertices.buffer);virtual.set(root+'indices.bin',pkg.indices.buffer);virtual.set(root+'colors.bin',pkg.colors.buffer);
  virtual.set(root+'animation-samples.json',samples);virtual.set(root+'settings',JSON.stringify(pkg.settings));
  for (const [stem, part] of Object.entries(pkg.morphParts)) {
   virtual.set(root + stem + '.json', JSON.stringify(part.meta));
   virtual.set(root + stem + '.bin', part.bin.buffer);
  }
 }
 return {root,key};
}

// ---- online races (web/net, docs/characters.md "Online races") ---------------------------------------------
// The client sends outfitKey(ui, rider) with its profile (null = the default outfit); every other client resolves it
// with remoteOutfitRider(entry, key) into the rider entry to load: {package, root?, outfit, outfit_settings,
// outfit_identity, settings_package?, files}. `files` are the package URLs (world.json, rider.json, vertices.bin,
// indices.bin, colors.bin, animation-samples.json; textures are world.json paths relative to `root`); a URL under
// /assets/WARDROBE is served by wardrobeFile(url, type) (main.js load() does this already), anything else by fetch.
//   wardrobe riders: 'w1:<committed item ids, ascending, comma-separated>' (the rows with flag 0x4, the only
//     input of the race assembly 0x14D068); Sam: 'sam:<package>'; cheat skins and the default outfit: null.
const PACKAGE_FILES=['world.json','rider.json','vertices.bin','indices.bin','colors.bin','animation-samples.json'];
export async function outfitKey(ui,rider){
 if(!rider||rider.kind==='cheat')return null;
 const wd=await outfitState({...ui,onlineMode:false},rider);if(!wd)return null;
 if(wd.w.sam&&!wd.w.parts_mode){const s=await samOutfit(rider,wd);return s.default?null:`sam:${s.package}`;}
 if(isDefault(wd))return null;
 return 'w1:'+wd.list.filter(e=>wd.flags(e.item)&4).map(e=>e.item).filter((v,i,a)=>a.indexOf(v)===i).sort((a,b)=>a-b).join(',');
}
export async function remoteOutfitRider(entry,key,{fetcher=globalThis.fetch}={}){
 const plain=e=>({...e,files:PACKAGE_FILES.map(f=>`${e.root||`/assets/${e.package}/`}${f}`)});
 try{
  if(!entry||!key||entry.kind==='cheat')return plain(entry);
  if(key.startsWith('sam:')){
   const pkg=key.slice(4),w=entry.id==='sam'?await loadWardrobe('sam',fetcher,{parts:false}):null;
   if(!w?.sam||!w.sam.outfits.some(o=>o.package===pkg)||!await packageAvailable(pkg))return plain(entry);
   return plain({...entry,package:pkg,outfit:key,settings_package:w.sam.settings_package});
  }
  if(!key.startsWith('w1:'))return plain(entry);
  const tokens=key.slice(3).split(',');if(!tokens.every(t=>/^\d+$/.test(t)))return plain(entry);
  const ids=tokens.map(Number),w=await loadWardrobe(entry.id,fetcher),known=new Set(w.entries.map(e=>e[0]));
  if(ids.some(i=>!known.has(i)))return plain(entry);
  const wd=new Wardrobe(w,Object.fromEntries(ids.map(i=>[i,0x16])));
  // a real record keeps every required slot filled (0x1521F0 changes nothing): anything else is not an outfit
  const before=JSON.stringify(wd.serialize());wd.required();if(JSON.stringify(wd.serialize())!==before||isDefault(wd))return plain(entry);
  const {root,key:outfit}=await raceRoot(w,wd,entry,fetcher),s=JSON.parse(virtual.get(root+'settings'));
  return plain({...entry,root,outfit,outfit_settings:{original_animation:{bone_mask:s.bone_mask,secondary_motion:{enabled:s.secondary}}},outfit_identity:s.identity});
 }catch(error){console.warn('Remote outfit unavailable, default package',error);return plain(entry);}
}

// The entry the FE preview (web/fe-preview.js previewRoot) loads for a rider: its outfit's FE package while one is worn.
export function outfitPreviewEntry(entry){const s=entry&&worn.get(entry.id);return s?.feRoot?{...entry,fe_root:s.feRoot}:entry;}

// The rider entry main.js selectRider loads: the default outfit keeps its verified RIDER_<ID> package; any other
// outfit gets its generated package plus its settings. A cheat skin keeps its bucket; its base rider's outfit is
// prepared for the front-end preview (the original shows the base rider there).
export async function outfitRider(ui,rider){
 // selectRider awaits this before it clears ui.ready itself: keep the front end from starting a second rider load
 // meanwhile (a concurrent load would leave the first model in the scene)
 const was=ui?.ready;if(ui)ui.ready=false;
 try{
  if(rider?.kind==='cheat'){const base=ui?.riders?.find(r=>r.id===(rider.base||'zoe'));if(base)await prepareOutfit(ui,base);return rider;}
  const s=await prepareOutfit(ui,rider);if(!s||s.default)return rider;
  if(s.package)return {...rider,package:s.package,outfit:s.key,settings_package:s.settingsPackage};   // Sam: another whole package, Sam's settings
  return {...rider,root:s.root,outfit:s.key,outfit_settings:{original_animation:{bone_mask:s.settings.bone_mask,secondary_motion:{enabled:s.settings.secondary}}},outfit_identity:s.settings.identity};
 }catch(error){console.warn('Outfit unavailable, default package',error);return rider;}
 finally{if(ui)ui.ready=was;}
}

// ---- Equip Gear screen (FE.LUI 12equ_char, cFEStateCharEquip) ---------------------------------------------
// Setup Character > Equip Gear (web/fe-screens.js calls openEquipGear) and the career lodge's Equip Gear
// (web/lodge-ui.js): Categories -> folders -> items (0x19B180 equip lists), Cross toggles an item (0x19BEE8 ->
// 0x14AFB0 + commit), refused when the rules fail or the outfit's item weight would pass the limit (0x199D30:
// *(gp-0x1818) = 3392); bars 1/2 = the current / possible weight, (w - 1500) / (3392 - 1500) (0x19BC90, 0x19BD48).
// The rider is the FE preview model of the outfit (web/fe-preview.js: the NIS head/eyes/hands with the FE idle's
// morphs; wardrobe buildPackage fe) placed by the Equip Gear update 0x19BFE8 (WARDROBE/equip-screen.json preview):
// camera eye (-110,394,0) -> (-35,0,0) cm, 25 deg; rider (-105,-100,-80) turned 80 deg (right stick: turn / zoom);
// the board at (265.75,-1073.5,-232) spinning 1 deg per frame; in the Boards folder (id 3) both ease (x0.2 per frame)
// to the board view (rider (-250,-650,-215), board (-100,45,15)) and back. While the model loads: "Loading...".
// equipLoading (PS2 local/ps2-capture/menus/eqg-k*, eqg2-k*: the lodge's Equip Gear, first and second entry, the same frames): the
// screen is up at once (from the lodge: at the flash's full white, as every lodge state) and the outfit's package is built behind
// "Loading...". Until phase 3 of the state (the intro's 0x42 label, frame 25, then FOCUS_LAG: CharEquip vt+0x30 = 0x1993A0) there is
// no rider, no 'equip btm left' group (dashes, bolt01, points; 19A238), no row highlight (the cursor, 186518) and no help line: 36
// frames after Cross, "Loading..." over the list. 0x1993A0 switches the preview on (19E538(slot, 1): +0xCC8 = drawn once the model
// is loaded, +0xCB4 / +0xCB8, else pending +0xCC4), so the rider, the board, the dashes, the highlight and the help show on frame 37;
// "Loading..." (update 0x199938: !+0xCC8, before that pass's phase 3) goes one frame later. The help also waits for the rider's gear
// data (19E238 -> +0xA60 -> 19A9B8 / 19A798): the outfit package here.
const DEG=Math.PI/180,SCREEN='equip-gear',ROWS=6,BACK_LAYER=7,EMPTY_BOX={page:'FE_1-11',sx:60,sy:25,sw:10,sh:10};
const HELP={category:0x059ad8f5,outfit:0x067655c4,remove:0x06fbfa43,free:0x065dc1f5,title:0x0b9b4c62,categories:'kT_FECategories'};
const loadImage=src=>new Promise((ok,fail)=>{const im=new Image();im.onload=()=>ok(im);im.onerror=fail;im.src=src;});

export class EquipGearScreen {
  constructor(ui) {
    this.ui = ui;
    this.data = null;
    this.images = {};
    this.icons = new Map();
    this.keys = new Set();
    this.rider = null;
    this.wd = null;
    this.folders = [];
    this.top = 0;
    this.enter = 0;
    this.notice = null;
    this.opts = {};
  }
  now() {
    return (performance.now() * 60) / 1000;
  }
  async load() {
    if (this.data) return true;
    const r = await fetch(ROOT + 'equip-screen.json');
    if (!r.ok) throw Error('WARDROBE/equip-screen.json');
    this.data = await r.json();
    await Promise.all(
      this.data.pages.map(async (p) => {
        this.images[p] = this.ui.characterSelect?.images?.[p] || (await loadImage(`/assets/UI/${p}.png`));
      })
    );
    const snow = this.ui.characterSelect?.data?.screens?.bg_snow_loop;
    const screen = snow
      ? {
          ...this.data.screen,
          elements: [...this.data.screen.elements, ...snow.elements.map((e) => ({ ...e, index: e.index + 1000 }))],
          animations: { ...this.data.screen.animations, ...snow.animations }
        }
      : this.data.screen;
    for (const p of new Set(snow?.elements.filter((e) => e.sprite).map((e) => e.sprite.page) || []))
      this.images[p] ??= this.ui.characterSelect?.images?.[p] || (await loadImage(`/assets/UI/${p}.png`));
    this.lui = new LuiScreen(screen, this.images, this.ui);
    this.snow = snow?.events || [];
    return true;
  }
  owns(screen) {
    return screen === SCREEN && !!this.lui;
  }
  t(key, fallback = '') {
    try {
      return this.ui.careerUI?.t?.(key, fallback) || fallback;
    } catch {
      return fallback;
    }
  }
  async open(rider, opts = {}) {
    this.opts = opts;
    this.rider = rider;
    this.base = rider?.kind === 'cheat' ? this.ui.riders.find((r) => r.id === (rider.base || 'zoe')) : rider;
    this.anim = null; // a cheat skin: the screen shows (and dresses) its base rider, as the original (Setup keeps the base rider's preview)
    this.folders = [];
    this.top = 0;
    this.enter = this.now();
    this.notice = null;
    await this.load();
    try {
      this.wd = await outfitState(this.ui, this.base);
    } catch (e) {
      console.warn('Wardrobe unavailable', e);
      this.wd = null;
    }
    this.changed = false;
    this.view = this.initialView();
    this.gearIn = false;
    this.shownAt = null;
    {
      // the outfit package behind "Loading..." (the PS2 loads the rider's gear with the screen up)
      const job = (this.preparing = { promise: null });
      job.promise = (this.wd ? prepareOutfit(this.ui, this.base) : Promise.resolve())
        .catch((e) => console.warn('Outfit package unavailable', e))
        .finally(() => {
          if (this.preparing === job) this.preparing = null;
        });
    }
    this.ui.set(SCREEN);
    this.ui.index = 0;
    this.pin = true;
    this.ui.sync(); // every list opens on its first entry (PS2)
    const lf = this.ui.careerUI?.lodgeFlash;
    if (lf?.active) this.enter = lf.introStart(this.now()); // opened by the lodge's state change (web/lui-flash.js)
  }
  get entries() {
    return this.wd ? this.wd.menu(this.folders.at(-1)?.item ?? -1) : [];
  }
  items() {
    return this.entries.map((e) => e.name);
  }
  layout(i) {
    const r = i - this.top;
    return r < 0 || r >= ROWS ? [0, -100, 1, 1] : [20, Math.round(((118 + 25 * r) * 448) / 480), 230, Math.round((20 * 448) / 480)];
  }
  disabled() {
    return !this.ui.ready;
  }
  scroll() {
    const i = this.ui.index;
    if (i < this.top) {
      this.top = i;
      this.arrow = { side: 'up', at: this.now() };
    }
    if (i >= this.top + ROWS) {
      this.top = i - ROWS + 1;
      this.arrow = { side: 'down', at: this.now() };
    }
  }
  choose(i) {
    if (this.ui.careerUI?.lodgeFlash?.active) return;
    const e = this.entries[i];
    if (!e || !this.wd) return;
    if (e.flags & 0x20) {
      this.folders.push({ item: e.item, name: e.name, index: i });
      this.top = 0;
      this.ui.index = 0;
      if (e.item === this.cfg.board_folder) this.view.mode = 3;
      this.ui.sync();
      return;
    } // 0x199D14
    const delta = this.wd.delta(e.item);
    if (delta == null) {
      this.notice = { text: this.t(HELP.remove, 'You must remove items before equipping this accessory.'), until: this.now() + 120 };
      return;
    }
    const weight = this.wd.weight(),
      limit = this.wd.w.item_limit?.limit ?? 3392;
    if (limit < weight + delta) {
      this.notice = {
        text: this.t(HELP.free, 'You must free up %d slots to equip that item.').replace('%d', String(weight + delta - limit)),
        until: this.now() + 120
      };
      return;
    }
    if (!this.wd.toggle(e.item)) return;
    saveOutfit(this.ui, this.base, this.wd);
    this.changed = true;
    speakFrontEnd(this.ui, 'customize', this.rider)?.catch?.(() => {}); // 0x199EA4 +0xC1C -> 1A0358 Customize
    // the preview wears it at once (after the entry's own build, equipLoading)
    (this.preparing?.promise ?? Promise.resolve())
      .then(() => prepareOutfit(this.ui, this.base))
      .catch((e) => console.warn(e))
      .finally(() => this.ui.sync());
  }
  back() {
    if (this.ui.careerUI?.lodgeFlash?.active) return;
    // the parent list reopens at its top (PS2); 0x199E30
    if (this.folders.length) {
      const f = this.folders.pop();
      if (f.item === this.cfg.board_folder) this.view.mode = 4;
      this.top = 0;
      this.ui.index = 0;
      this.ui.sync();
      return;
    }
    this.close();
  }
  // leaving: the race rider (a cheat skin keeps its bucket) is reloaded with the new outfit for the event
  close() {
    for (const m of this.preview?.model?.meshes || []) if (m.userData.board) m.visible = false;
    const done = this.opts.onExit;
    this.opts = {};
    if (this.changed) this.reload(this.rider);
    this.changed = false;
    this.keys.clear();
    if (done) done();
    else {
      this.ui.set('setup');
      this.ui.index = 1;
      this.ui.sync();
    }
  }
  key(e) {
    if (!this.owns(this.ui.screen)) return false;
    if (this.ui.careerUI?.lodgeFlash?.active) {
      e.preventDefault?.();
      return true;
    } // no input under the lodge's flash
    if (this.opts.onBuy && (e.code === 'ShiftLeft' || e.code === 'ShiftRight')) {
      const buy = this.opts.onBuy;
      this.opts = {};
      buy();
      return true;
    } // Square: Buy Gear (lodge)
    if (['ArrowUp', 'ArrowDown'].includes(e.code))
      setTimeout(() => {
        this.scroll();
        this.ui.sync();
      });
    if (['KeyJ', 'KeyL', 'KeyI', 'KeyK'].includes(e.code)) {
      this.keys.add(e.code);
      return true;
    } // right stick (Classic IJKL): turn / zoom
    return false;
  }
  // reload the race rider with the saved outfit (main.js selectRider -> outfitRider)
  reload(rider = this.base) {
    if (!rider || !this.ui.cb?.rider) return;
    Promise.resolve(this.ui.cb.rider(rider)).catch((e) => console.error(e));
  }
  get cfg() {
    return this.data?.preview || {};
  }
  // ---- 3D: the FE preview model (web/fe-preview.js) of the outfit, placed by the Equip Gear update 0x19BFE8 ----
  initialView() {
    const c = this.cfg;
    return {
      mode: 1,
      rider: (c.rider || [-105, -100, -80]).map(Math.fround),
      board: (c.board || [265.75, -1073.5, -232]).map(Math.fround),
      zoom: 0,
      yaw: 0,
      spin: 0,
      frames: 0
    };
  }
  get preview() {
    return this.ui.characterSelect?.preview3d || null;
  }
  entry() {
    return this.base ? outfitPreviewEntry(this.base) : null;
  }
  // true while the FE package is loading (PS2: "Loading...", no rider, list shown) or drawn
  loading() {
    const p = this.preview,
      e = this.entry();
    if (this.preparing) return !!(p && e);
    return !!(p && e && this.T && p.want(this.T, e) && !p.ready);
  }
  // the rider's gear data is in (PS2 +0xA60, set once per entry): the outfit package built and its FE model loaded (or
  // unavailable); the list's help line waits for it
  gearReady() {
    if (this.gearIn) return true;
    if (this.preparing) return false;
    return (this.gearIn = true);
  }
  // phase 3 of the state (0x1993A0: the intro's 0x42 label + FOCUS_LAG)
  settled() {
    this.settleAt ??= (this.data?.screen?.labels || []).find((l) => l.control?.some((c) => c.startsWith('42')))?.frame ?? 25;
    return this.now() - this.enter >= this.settleAt + FOCUS_LAG;
  }
  // "Loading..." (0x199938: !+0xCC8): until the frame after the rider first draws, and while a reload hides it
  loadingText() {
    const p = this.preview,
      e = this.entry(),
      root = previewRoot(e);
    if (!p || !e || (!this.preparing && root && p.failed.has(root))) return false;
    return this.shownAt == null || this.now() - this.shownAt < 1;
  }
  showPreview() {
    const p = this.preview,
      e = this.entry();
    if (!p || !e || !this.T) {
      p?.show?.(false);
      return false;
    }
    if (this.preparing) {
      p.show(false);
      this.shownAt = null;
      this.boards(p);
      return true;
    } // nothing drawn while the outfit builds
    const on = p.want(this.T, e),
      shown = on && p.ready && this.settled();
    p.show(shown);
    if (!shown) this.shownAt = null;
    else this.shownAt ??= this.now(); // from phase 3 (19E538(slot, 1))
    this.boards(p);
    return on;
  }
  boards(p) {
    for (const m of p?.model?.meshes || []) if (m.userData.board) m.visible = this.owns(this.ui.screen);
  }
  // one 0x19BFE8 frame: the stick, the state 1/2/3/4 targets and easing, the board spin (all cm, Z-up)
  step() {
    const c = this.cfg,
      v = this.view,
      stickX = (this.keys.has('KeyL') ? 1 : 0) - (this.keys.has('KeyJ') ? 1 : 0),
      stickY = (this.keys.has('KeyK') ? 1 : 0) - (this.keys.has('KeyI') ? 1 : 0);
    if (v.mode === 1 || v.mode === 4) {
      v.yaw = (((v.yaw + stickX * c.turn) % 360) + 360) % 360;
      v.zoom = Math.max(0, Math.min(1, v.zoom + stickY * c.zoom_rate));
    }
    const riderView = [c.rider[0], c.rider[1] + c.zoom[0] * v.zoom, c.rider[2] + c.zoom[1] * v.zoom * c.zoom[2]];
    // VU0 vsub / vmulx / vadd (round toward zero), then the squared length (vmul + vadday/vmaddaz/vmaddw)
    const ease = (toBoards) => {
      const k = Math.fround(v.mode === 3 ? c.ease : c.done),
        tr = toBoards ? c.rider_boards : riderView,
        tb = toBoards ? c.board_boards : c.board;
      const dr = v.rider.map((x, i) => fsub(Math.fround(tr[i]), x)),
        db = v.board.map((x, i) => fsub(Math.fround(tb[i]), x));
      v.rider = v.rider.map((x, i) => fadd(x, fmul(dr[i], k)));
      v.board = v.board.map((x, i) => fadd(x, fmul(db[i], k)));
      const len = (d) => fadd(fadd(fadd(fmul(d[1], d[1]), fmul(d[0], d[0])), fmul(d[2], d[2])), 0);
      return [len(dr), len(db)];
    };
    if (v.mode === 1) {
      v.rider = riderView.map(Math.fround);
      v.board = c.board.map(Math.fround);
    } else if (v.mode === 2) {
      v.rider = c.rider_boards.map(Math.fround);
      v.board = c.board_boards.map(Math.fround);
    } else if (v.mode === 3) {
      const [, db] = ease(true);
      if (db < Math.fround(c.ease)) v.mode = 2;
    } else if (v.mode === 4) {
      const [dr] = ease(false);
      if (dr < Math.fround(c.done)) v.mode = 1;
    }
    v.spin += c.board_spin;
    if (v.spin >= 360) v.spin -= 360;
  }
  // cm Z-up -> three (x, z, -y) / 100
  static three(T, p) {
    return new T.Vector3(p[0] / 100, p[2] / 100, -p[1] / 100);
  }
  riderTransform(T) {
    const v = this.view;
    return {
      position: EquipGearScreen.three(T, v.rider),
      quaternion: new T.Quaternion().setFromAxisAngle(new T.Vector3(0, 1, 0), (v.yaw + this.cfg.rider_yaw) * DEG)
    };
  }
  boardQuaternion(T) {
    const c = this.cfg,
      q = (axis, h) => new T.Quaternion(axis[0] * Math.sin(h), axis[1] * Math.sin(h), axis[2] * Math.sin(h), Math.cos(h));
    const z = q([0, 0, 1], c.board_half_angles[0])
      .multiply(q([0, 1, 0], c.board_half_angles[1]))
      .multiply(q([1, 0, 0], (this.view.spin + c.board_spin_offset) * c.deg * 0.5));
    return new T.Quaternion(z.x, z.z, -z.y, z.w); // Z-up -> Y-up, as the bones
  }
  place(T, model, camera) {
    if (!this.owns(this.ui.screen)) return false;
    this.T = T;
    const c = this.cfg;
    camera.clearViewOffset();
    camera.fov = (2 * Math.atan(Math.tan(c.fov_half_horizontal || 25 * DEG) / camera.aspect)) / DEG;
    camera.updateProjectionMatrix();
    camera.up.set(0, 1, 0);
    camera.position.copy(EquipGearScreen.three(T, c.eye));
    camera.lookAt(EquipGearScreen.three(T, c.target));
    const { position, quaternion } = this.riderTransform(T);
    const p = this.preview,
      e = this.entry();
    if (!this.preparing && p && e && p.want(T, e) && p.ready && model.parent && p.place(T, model.parent, 0)) {
      const g = p.model.group;
      g.position.copy(position);
      g.quaternion.copy(quaternion);
      g.updateMatrixWorld(true);
      // lighting: IRR record + rim at the hips with the Equip Gear camera's view matrix (0x19EE88)
      const fe = p.model.fe,
        saved = fe.view_matrix_bits;
      if (c.view_matrix_bits) fe.view_matrix_bits = c.view_matrix_bits;
      try {
        p.light(T, this.core);
      } finally {
        fe.view_matrix_bits = saved;
      }
    }
    model.position.copy(position);
    model.scale.setScalar(1 / (model.userData.riderScale || 1));
    model.quaternion.copy(quaternion); // race rider (fallback)
    this.model = model;
    return true;
  }
  // The FE idle loop (semantic 434, FE_GEAR_<X>_CYC of the base rider) with its morphs, then the board at +0xC50/+0xC60.
  pose(T, bones, rig, dt, { clips, samples, scale, core } = {}) {
    if (!this.owns(this.ui.screen) || !clips || !samples || !rig) return false;
    this.T = T;
    if (core) this.core = core;
    this.view.frames += dt * 60;
    while (this.view.frames >= 1) {
      this.view.frames -= 1;
      this.step();
    }
    const idle = this.base?.fe?.idle || 'FE_GEAR_MAC_CYC',
      clip = clips.find((c) => c.name === idle) || clips.find((c) => c.name === 'FE_GEAR_MAC_CYC');
    if (!clip) return false;
    if (this.preparing && this.preview && this.entry()) return true; // the outfit is still building
    const p = this.preview,
      e = this.entry(),
      usePreview = !!(p && e && p.want(T, e));
    if (usePreview && !p.ready) return true; // loading: nothing drawn yet
    const target = usePreview ? p.model.rig : rig;
    if (this.anim?.rig !== target) this.anim = { rig: target, t: 0 };
    this.anim.t += dt;
    const A = clipSample(clip, this.anim.t, true);
    const { position, quaternion } = this.riderTransform(T);
    const inverse = new T.Matrix4()
      .compose(position, quaternion, new T.Vector3().setScalar(usePreview ? 1 : 1 / (this.model?.userData.riderScale || 1)))
      .invert();
    const boardPosition = EquipGearScreen.three(T, this.view.board).applyMatrix4(inverse),
      boardQuaternion = quaternion.clone().invert().multiply(this.boardQuaternion(T));
    if (usePreview) {
      p.apply(T, A, null, 0, samples);
      const k = p.model.rig.bones.findIndex((b) => b.name === 'board_rootg');
      if (k >= 0) {
        p.model.bones[k].position.copy(boardPosition);
        p.model.bones[k].quaternion.copy(boardQuaternion);
      }
      this.boards(p);
      return true;
    }
    rig.bones.forEach((b, k) => {
      const bone = bones[k];
      if (!bone) return;
      bone.position.fromArray(b.translation).multiplyScalar(scale);
      bone.quaternion.fromArray(b.rotation).normalize();
      const stream = clip.streams?.[String(b.file)];
      if (!stream) return;
      const tc = b.animation_translation_channel,
        rc = b.animation_rotation_channel;
      if (tc >= 0)
        bone.position
          .set(
            channelValue(samples, A, stream, tc) / 100,
            channelValue(samples, A, stream, tc + 2) / 100,
            -channelValue(samples, A, stream, tc + 1) / 100
          )
          .multiplyScalar(scale);
      if (rc >= 0)
        bone.quaternion.copy(
          afb(T, channelValue(samples, A, stream, rc), channelValue(samples, A, stream, rc + 1), channelValue(samples, A, stream, rc + 2))
        );
    });
    const board = rig.bones.findIndex((b) => b.name === 'board_rootg');
    if (board >= 0 && bones[board]) {
      bones[board].position.copy(boardPosition);
      bones[board].quaternion.copy(boardQuaternion);
    }
    return true;
  }
  // ---- 2D ----
  icon(entry) {
    const name = entry?.icon;
    if (!name || !this.base) return null;
    const key = `${this.base.id}/${name}`;
    if (!this.icons.has(key)) {
      this.icons.set(key, null);
      const pack = this.wd?.w?.icon_pack; /* the rider's icon archive (WARDROBE/<ID>/icons.tex), else the older PNG files */
      (pack
        ? archiveBlob(pack, name).then((b) => {
            const u = URL.createObjectURL(b);
            return loadImage(u).finally(() => URL.revokeObjectURL(u));
          })
        : loadImage(`${ROOT}${this.base.id.toUpperCase()}/icons/${name}.png`)
      )
        .then((im) => this.icons.set(key, im))
        .catch(() => {});
    }
    return this.icons.get(key);
  }
  draw(c, b) {
    if (this.pin) {
      this.pin = false;
      this.ui.index = 0;
      this.top = 0;
    }
    const frame = this.now() - this.enter,
      list = this.entries,
      index = Math.min(this.ui.index, Math.max(0, list.length - 1)),
      row = index - this.top;
    const leafList = list.some((e) => !(e.flags & 0x20)),
      focused = list[index] && this.wd?.byItem.get(list[index].item);
    const events = [],
      settled = this.settled();
    for (const ev of this.data.screen.events) {
      if (ev.frame <= 25) {
        if (ev.frame <= Math.min(frame, 25)) events.push({ ev, start: ev.frame });
      } else if (ev.frame === 35 + 5 * Math.max(0, Math.min(ROWS - 1, row))) {
        if (settled) events.push({ ev, start: 0 });
      } // the cursor's label (equipLoading: from phase 3, 186518)
      else if (this.arrow && ev.frame === (this.arrow.side === 'up' ? 90 : 95) && frame - (this.arrow.at - this.enter) < 20)
        events.push({ ev, start: this.arrow.at - this.enter });
    }
    const snowFrame = frame % 600;
    for (const ev of this.snow) if (ev.frame <= snowFrame) events.push({ ev, start: frame - snowFrame + ev.frame });
    const weight = this.wd?.weight() ?? 0,
      limit = this.wd?.w.item_limit?.limit ?? 3392,
      base = this.wd?.w.item_limit?.base ?? 1500;
    const fill = (w) => Math.max(0, Math.min(1, (w - base) / (limit - base)));
    const delta = focused && !(focused.flags & 0x20) && this.wd ? this.wd.delta(focused.item) : null;
    const lodge = !!this.opts.onBuy,
      notice = this.notice && this.now() < this.notice.until ? this.notice.text : null;
    const helpText =
      frame < 16 || !settled || !this.gearReady()
        ? ''
        : notice ||
          (leafList
            ? this.t(HELP.outfit, '1   Your current outfit\\\\2   Your possible outfit')
            : list.length
              ? this.t(HELP.category, 'Choose a category to continue.')
              : '');
    const rowsText = ['00000031', '00000032', '00000033', '00000034', '00000035', '00000036'],
      boxes = ['069eb9e1', '069eb9e2', '069eb9e3', '069eb9e4', '069eb9e5', '069eb9e6'];
    const override = (e) => {
      const r = rowsText.indexOf(e.name);
      if (r >= 0) {
        const it = list[this.top + r];
        return it ? { text: it.name } : { hidden: true };
      }
      const k = boxes.indexOf(e.name);
      // FE_1-11 checked box / plain box
      if (k >= 0) {
        const it = list[this.top + k];
        if (!it || it.flags & 0x20) return { hidden: true };
        return this.wd?.equipped(it.item) ? null : { sprite: EMPTY_BOX };
      }
      switch (e.name) {
        case '0c37a134':
          return { hidden: true };
        case '0eb7b087':
          return { text: this.folders.at(-1)?.name || this.t(HELP.categories, 'Categories') };
        case '000006ec':
          return list.length ? null : { hidden: true };
        case '06963731':
        case '077605b3':
        case '0dc7b965':
          return { hidden: true };
        case '06651533':
          return leafList ? null : { hidden: true };
        case '06cc8862':
        case '0cc88692':
          return { hidden: true };
        case '00b67793':
          return { hidden: true };
        case '069acf75':
        case '007769a1':
          return lodge ? null : { hidden: true };
        case '0afd5af4':
          return this.loadingText() ? null : { hidden: true }; // "Loading..." while the preview model loads (equipLoading: loadingText)
        case '0b777dd4':
          return settled ? null : { hidden: true }; // 'equip btm left' (19A238 at phase 3; equipLoading)
      }
      return null;
    };
    b.fillStyle = '#75a9cb';
    b.fillRect(0, 0, 640, 448);
    b.save();
    b.scale(1, 448 / 480);
    c.save();
    c.scale(1, 448 / 480);
    this.lui.draw(b, events, frame, override, (layer) => layer <= BACK_LAYER);
    this.lui.draw(c, events, frame, override, (layer) => layer > BACK_LAYER);
    // the help lines ('\\' breaks, CMNAMER 0x067655C4) at the help element's place
    if (helpText) {
      const e = this.lui.byName.get('0c37a134'),
        p = this.lui.props(e, events, frame);
      helpText.split(/\\+|\n/).forEach((line, i) => this.lui.text(c, line.trim(), p[0] ?? 48, (p[1] ?? 400) + i * 15, p, 1));
    }
    // item icon (0x14B700 archive, 90x90 at (35,295)) and the weight bars 1 / 2
    const im = leafList && focused && !(focused.flags & 0x20) ? this.icon(focused) : null;
    if (im) c.drawImage(im, 35, 295, 90, 90);
    if (leafList) {
      const bar = (x, f) => {
        const h = 221 * f;
        c.fillStyle = 'rgb(220,97,0)';
        c.fillRect(x, 361 - h, 12, h);
      };
      bar(558, fill(weight));
      bar(577, fill(delta == null ? weight : weight + delta));
    }
    c.restore();
    b.restore();
    this.ui.careerUI?.lodgeFlash?.draw(c); // the lodge's flash over this screen when the lodge opened or leaves it
  }
}

// Setup Character > Equip Gear (web/fe-screens.js) and the lodge (web/lodge-ui.js): opens the screen for `rider`
// (a riders.json entry; a cheat skin edits its base rider's gear and keeps its fixed bucket outfit).
// opts: {onExit, onBuy} (the lodge returns to its menu and offers Square = Buy Gear).
export async function openEquipGear(ui,rider,opts={}){
 install(ui);
 await ui.equipGear.open(rider||ui.characterSelect?.human?.()||ui.rider,opts);
}
// the screen's own data (FE.LUI 12equ_char, the rider's gear lists) before the lodge opens it, so the state switch at
// the flash's full white does not wait for it (resident on the PS2). The outfit's parts load behind "Loading..." as before.
export function preloadEquipGear(ui,rider){
 install(ui);const base=rider?.kind==='cheat'?ui.riders?.find(r=>r.id===(rider.base||'zoe')):rider;
 const lists=base&&!(base.kind==='custom'&&base.id!=='sam')?loadWardrobe(base.id,globalThis.fetch,{parts:false}):null;
 return Promise.all([ui.equipGear.load(),lists]);
}
function install(ui){
 if(ui.equipGear)return;
 const gear=ui.equipGear=new EquipGearScreen(ui);
 // the screen joins OriginalUI's dispatch (items/choose/back/draw) without touching web/ui.js
 for(const name of ['items','choose','back']){const orig=ui[name].bind(ui);ui[name]=(...a)=>gear.owns(ui.screen)?gear[name](...a):orig(...a);}
 const draw = ui.draw.bind(ui);
 ui.draw = (s) => {
   if (!gear.owns(ui.screen)) return draw(s);
   ui.lastState = s;
   const c = ui.fg,
     b = ui.bg;
   c.clearRect(0, 0, 640, 448);
   b.clearRect(0, 0, 640, 448);
   if (ui.images['FE_1-20']) gear.draw(c, b);
 };
 addEventListener('keydown',e=>{if(gear.owns(ui.screen))gear.key(e);},true);
}
