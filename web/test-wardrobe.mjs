// Equip Gear / outfits (web/wardrobe.js, tools/export_wardrobe.py; docs/characters.md "Equip Gear and outfits").
// 1. Rules: fresh-profile inventory (0x1513B8) equals the lodge port (shop.json runtime initial flags) for the ten riders;
//    UI equip 0x14AFB0 keeps the required slots, restores on refusal; commit/restore; '$' texture resolution.
// 2. Default outfits: the race assembly of a fresh profile (0x14D068 -> 0x11BBE8/0x11C138/0x11BE88) rebuilds every
//    character's live-verified package RIDER_<ID> (the default assembly of its countdown savestate): parts, bones,
//    skin, vertices, per-batch textures, bit-exact bind matrices (compile 0x30DBD0), bone slots, masks, bone_mask and
//    secondary motion enables of settings.json. Cheat skins assemble their whole bucket.
// 3. Changed outfits initialise in the core as the human (init_animation) and ride away with finite poses.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {fileURLToPath} from 'node:url';
import {Wardrobe,loadWardrobe,assembly,buildPackage,entries,resolveTexture,geometryMasks} from './wardrobe.js';
import {mergeCharacterSettings,humanSettings} from './character-roster.js';

const here=fileURLToPath(new URL('.',import.meta.url)),pub=here+'public';
const fetcher=async path=>{const file=pub+path;if(!fs.existsSync(file))return {ok:false,status:404};const data=fs.readFileSync(file);return {ok:true,json:async()=>JSON.parse(data),arrayBuffer:async()=>data.buffer.slice(data.byteOffset,data.byteOffset+data.byteLength),text:async()=>data.toString()};};
const json=path=>JSON.parse(fs.readFileSync(pub+path));
const roster=json('/assets/riders.json').filter(r=>r.kind!=='custom');
if(!fs.existsSync(pub+'/assets/WARDROBE/ZOE/wardrobe.json')){console.log('WARDROBE assets missing: run tools/export_wardrobe.py (skipped)');process.exit(0);}
const report=[];

// ---- 1. rules ------------------------------------------------------------------------------------------------
const shop=fs.existsSync(pub+'/assets/CAREER/shop.json')?json('/assets/CAREER/shop.json'):null;
for(const rider of roster.filter(r=>r.kind==='rider')){
 const w=await loadWardrobe(rider.id,fetcher),wd=new Wardrobe(w);
 if(shop){const want=shop.gear.runtime[rider.character].initial_flags;assert.deepEqual(wd.serialize(),Object.fromEntries(Object.entries(want).map(([k,v])=>[k,v])),`${rider.id}: 0x1513B8 init`);}
}
{
 const w=await loadWardrobe('zoe',fetcher),wd=new Wardrobe(w),E=new Map(entries(w).map(e=>[e.item,e]));
 const eq=()=>wd.raceEquipped().map(e=>e.item);
 // default: Blacked Out (55) + Rocked Out (515) -> zoe_Suit_B01_B01, race head 86 / hands 130 / PDA 4
 assert.equal(resolveTexture(E.get(55),wd.raceEquipped()),'zoe_suit_b01_b01');
 assert(eq().includes(86)&&eq().includes(130)&&eq().includes(4),'0x14D068 race models');
 // a top colour of another top model: Trouble (66, Skintight 63) replaces Bomber Jacket (52)
 assert(wd.set(66,true));wd.commit();
 assert(wd.equipped(63)&&!wd.equipped(52)&&!wd.equipped(55),'top model swap');
 const asm=assembly(w,wd.raceEquipped());
 assert(asm.parts.some(p=>p.resource==='zoe_topc.mnf')&&!asm.parts.some(p=>p.resource==='zoe_topb.mnf'));
 assert(asm.textures.includes('zoe_suit_c01_b01'),`suit texture combines top C01 and bottom B01: ${asm.textures}`);
 // unequipping the only top is refused by 0x1521F0's re-equip of the default (Junker) or kept equipped
 const before=wd.serialize();wd.set(66,false);assert(Object.values(wd.serialize()).length>0);
 assert(entries(w).filter(e=>e.parent===22||[46,52,63].includes(e.cls)).some(e=>wd.equipped(e.item)),'a top stays equipped (0x1521F0)');
 wd.inv.f=new Map(Object.entries(before).map(([k,v])=>[+k,v]));
 // restore/commit round trip
 wd.set(556,true);wd.commit();const committed=wd.serialize();wd.restore();assert.deepEqual(wd.serialize(),committed);
 report.push({rules:'ok'});
}

// Equip Gear lists of a fresh profile (0x19B180 equip mode: owned named leaves, folders holding one, by +0xA) ==
// the PS2 lists (local/reference/pcsx2/characters/gear-fresh-lists.json, captured screens + record walk; optional).
const listsFile=here+'../local/reference/pcsx2/characters/gear-fresh-lists.json';
if(fs.existsSync(listsFile))for(const [name,list] of Object.entries(JSON.parse(fs.readFileSync(listsFile)))){
 const wd=new Wardrobe(await loadWardrobe(name.toLowerCase(),fetcher));
 const tree=folder=>wd.menu(folder).map(e=>e.flags&0x20?{folder:e.name,id:e.item,items:tree(e.item)}:{item:e.name,id:e.item,eq:wd.equipped(e.item)});
 const strip=n=>n.map(x=>x.folder?{folder:x.folder,id:x.id,items:strip(x.items)}:{item:x.item,id:x.id,eq:x.eq});
 assert.deepEqual(tree(-1),strip(list),`${name}: Equip Gear lists`);
}

// ---- 2. default outfits == packages ------------------------------------------------------------------------
const initial=json('/assets/ANIMATIONS/initial.json');
for(const rider of roster){
 const w=await loadWardrobe(rider.id,fetcher),cheat=rider.kind==='cheat';
 const wd=new Wardrobe(w),equipped=cheat?entries(w):wd.raceEquipped();
 const asm=assembly(w,equipped,cheat),pkg=buildPackage(w,asm,{riderId:rider.id});
 const dir=`/assets/${rider.package}/`,rig=json(dir+'rider.json'),world=json(dir+'world.json');
 const settings=fs.existsSync(pub+dir+'settings.json')?json(dir+'settings.json'):null;
 const name=p=>(p.resource||`${/^(Bindings|BoardFlex)/.test(p.part)?'board':w.prefix}_${p.part}.mnf`).toLowerCase();
 assert.deepEqual(pkg.rig.parts.map(p=>p.resource.toLowerCase()).sort(),rig.parts.map(name).sort(),`${rider.id}: parts`);
 // bones: same set, same authored data; order = part order (file id)
 const key=b=>`${b.file}:${b.index}`,byKey=new Map(rig.bones.map((b,i)=>[key(b),i]));
 assert.equal(pkg.rig.bones.length,rig.bones.length,`${rider.id}: bone count`);
 const perm=pkg.rig.bones.map(b=>byKey.get(key(b)));assert(perm.every(i=>i!==undefined),`${rider.id}: bone set`);
 pkg.rig.bones.forEach((b,i)=>{const o=rig.bones[perm[i]];for(const f of ['name','translation','rotation','source_translation_bits','source_rotation_bits','animation_translation_channel','animation_rotation_channel','mirror_quaternion_map'])assert.deepEqual(b[f],o[f],`${rider.id} ${b.name} ${f}`);
  assert.equal(b.parent<0?-1:perm[b.parent],o.parent,`${rider.id} ${b.name} parent`);});
 // bind matrices bit-exact with the live banks, slots, slot count
 pkg.rig.bones.forEach((b,i)=>{assert.deepEqual(pkg.rig.source_bind_matrix_words[i],rig.source_bind_matrix_words[perm[i]],`${rider.id} ${b.name} bind`);assert.equal(pkg.rig.source_bone_slots[i],rig.source_bone_slots[perm[i]],`${rider.id} ${b.name} slot`);});
 assert.equal(pkg.rig.source_bone_slot_count,rig.source_bone_slot_count,`${rider.id}: slot count`);
 // masks / bone_mask / secondary enables against the character's settings (Zoe = initial.json)
 const s=settings?mergeCharacterSettings(initial,settings.settings):initial,id=settings?.identity??{upper_mask8c0:'0x8000fffe',upper_mask8c8:'0x8000fff8',upper_mask8d0:'0x870'};
 assert.equal(pkg.settings.bone_mask,s.original_animation.bone_mask,`${rider.id}: bone_mask`);
 assert.deepEqual(pkg.settings.secondary,s.original_animation.secondary_motion.enabled,`${rider.id}: secondary motion enables`);
 for(const m of ['upper_mask8c0','upper_mask8c8','upper_mask8d0'])assert.equal(BigInt(pkg.settings.identity[m]),BigInt(id[m]),`${rider.id}: ${m}`);
 // geometry: vertex/skin data per bone key, per-batch textures (as sets per part material)
 const verts=new Float32Array(fs.readFileSync(pub+dir+'vertices.bin').buffer.slice(0));
 assert.equal(pkg.vertices.length,verts.length,`${rider.id}: vertex count`);
 const rows=(v,rig,perm)=>Array.from({length:v.length/10},(_,i)=>Array.from(v.slice(i*10,i*10+10)).join(',')+'|'+JSON.stringify(rig.source_skin[i].map(([b,wt])=>[perm?perm[b]:b,wt])));
 assert.deepEqual(rows(pkg.vertices,pkg.rig,perm).sort(),rows(verts,rig).sort(),`${rider.id}: vertices + source skin`);
 const texOf=(wld,b)=>wld.textures[`9-${b.texture}`].resource.toLowerCase();
 const mine=new Set(pkg.world.batches.map(b=>texOf(pkg.world,b))),theirs=new Set(world.batches.map(b=>texOf(world,b)));
 assert.deepEqual([...mine].sort(),[...theirs].sort(),`${rider.id}: textures`);
 // every batch of the kept package binds the loaded texture whose SSH name is its material (0x11BE88/0x14B988)
 const rule=new Map();for(const t of asm.textures){const info=w.textures[t.replace(/\.gsh$/,'')];if(info&&!rule.has(info.name))rule.set(info.name,info.resource.toLowerCase());}
 world.batches.forEach((b,i)=>{assert(b.material,`${rider.id}: batch ${i} has no material (tools/export_characters.py --retexture)`);assert.equal(texOf(world,b),rule.get(b.material),`${rider.id}: batch ${i} ${b.material}`);});
 report.push({rider:rider.id,parts:pkg.rig.parts.length,bones:pkg.rig.bones.length,slots:pkg.rig.source_bone_slot_count,textures:[...mine].length,mask:pkg.settings.identity.upper_mask8c0});
}

// ---- 2b. PS2 Equip Gear states (tools/export_wardrobe.py --ground-truth; local reference, skipped when absent) --------
// Each derived state's own inventory rows assemble exactly its live rider: active LOD0 parts (header-matched),
// bone slot count, channel-1 masks, secondary enables, bit-exact bind bank rows, and only resident textures.
const truthFile=here+'../local/reference/pcsx2/characters/gear-ground-truth.json';
if(fs.existsSync(truthFile))for(const t of JSON.parse(fs.readFileSync(truthFile))){
 const id=roster.find(r=>t.cheat?r.character===t.cheat:r.kind==='rider'&&r.character===t.character).id,w=await loadWardrobe(id,fetcher);
 const wd=new Wardrobe(w,t.cheat?null:t.flags),asm=assembly(w,t.cheat?entries(w):wd.raceEquipped(),!!t.cheat),pkg=buildPackage(w,asm,{riderId:id});
 const visible=asm.parts.filter(p=>!w.hidden_slots.includes(p.slot));
 assert.deepEqual(visible.map(p=>p.slot),t.parts.map(p=>p.slot),`${t.state}: part slots`);
 visible.forEach((p,i)=>assert(t.parts[i].resources.includes(p.resource),`${t.state}: ${p.resource} not live ${t.parts[i].resources}`));
 assert.equal(pkg.rig.source_bone_slot_count,t.slot_count,`${t.state}: slot count`);
 assert.deepEqual(['upper_mask8c0','upper_mask8c8','upper_mask8d0'].map(k=>BigInt(pkg.settings.identity[k])),['m8c0','m8c8','m8d0'].map(k=>BigInt(t.masks[k])),`${t.state}: masks`);
 assert.deepEqual(pkg.settings.secondary,t.secondary,`${t.state}: secondary motion`);
 pkg.rig.source_bind_matrix_words.forEach((words,i)=>assert.deepEqual(words,t.bank[pkg.rig.source_bone_slots[i]],`${t.state}: bind ${pkg.rig.bones[i].name}`));
 for(const b of pkg.world.batches){const stem=pkg.world.textures['9-'+b.texture].resource.toLowerCase().replace('.gsh','');assert(t.resident.includes(stem),`${t.state}: ${stem} not resident`);}
 report.push({state:t.state,parts:visible.map(p=>w.parts[p.resource].name),slots:t.slot_count,mask:t.masks.m8c0});
}

// Equip Gear sequences of the PS2 runs (ps2_navigate scripts in local/reference/pcsx2/characters/scripts/gear), replayed
// through 0x14AFB0 + commit from a fresh profile, give exactly the live committed rows (flag 0x4) of each state.
const SEQUENCES={'zoe/gear/dangerous-trouble-zennish-countdown':['Dangerous','Trouble','Bare Back','Zennish','Wicked Spex'],
 'zoe/gear/bandito-stuff-flamingboard-countdown':['Bandito','Stuff','Flaming Board'],'zoe/gear/peacekeeper-then-tiara-element-countdown':['Peacekeeper','Tiara','Element'],
 'mac/gear/bedhead-infiltrator-supertweak-countdown':['Bedhead','Infiltrator','Supertweak'],'elise/gear/roughrider-bluezip-shadowlike-countdown':['Roughrider','Blue Zip Up','Shadow Like','Jacked In']};
if(fs.existsSync(truthFile))for(const t of JSON.parse(fs.readFileSync(truthFile))){
 const key=Object.keys(SEQUENCES).find(k=>t.state.includes(k));if(!key)continue;
 const id=key.split('/')[0],wd=new Wardrobe(await loadWardrobe(id,fetcher));
 for(const name of SEQUENCES[key]){const e=wd.list.find(x=>x.name===name&&(x.flags&4));assert(e&&wd.toggle(e.item),`${key}: ${name}`);}
 const mine=[...wd.inv.f].filter(([,v])=>v&4).map(([k])=>k).sort((a,b)=>a-b),live=Object.entries(t.flags).filter(([,v])=>v&4).map(([k])=>+k).sort((a,b)=>a-b);
 assert.deepEqual(mine,live,`${key}: committed rows`);
}

// ---- 2c. front-end preview assembly: the fresh outfit's FE set (0x10 rows: the NIS head/eyes/hands) rebuilds every
// RIDER_<ID>/fe package (tools/export_fe_preview.py; live preview slot of select.p2s for the ten riders): parts, bones,
// bit-exact bind rows and slots, morph targets, per-batch textures.
for(const rider of roster){
 const dir=`/assets/${rider.package}/fe/`;if(!fs.existsSync(pub+dir+'rider.json'))continue;
 const w=await loadWardrobe(rider.id,fetcher),cheat=rider.kind==='cheat',wd=new Wardrobe(w);
 const asm=assembly(w,cheat?entries(w):wd.feEquipped(),cheat),pkg=buildPackage(w,asm,{riderId:rider.id,fe:true});
 const rig=json(dir+'rider.json'),world=json(dir+'world.json');
 if(cheat){   // cheat skins: the FE agent's rule package (the original never draws them); report differences only
  const same=JSON.stringify(pkg.rig.parts.map(p=>p.resource.toLowerCase()))===JSON.stringify(rig.parts.map(p=>p.resource.toLowerCase()));
  report.push({fe:rider.id,rule:'bucket',same_parts_as_fe_package:same,note:'the original never draws a cheat skin preview'});continue;
 }
 assert.deepEqual(pkg.rig.parts.map(p=>p.resource.toLowerCase()),rig.parts.map(p=>p.resource.toLowerCase()),`${rider.id}: FE parts`);
 assert.deepEqual(pkg.rig.bones.map(b=>b.name),rig.bones.map(b=>b.name),`${rider.id}: FE bones`);
 assert.deepEqual(pkg.rig.source_bind_matrix_words,rig.source_bind_matrix_words,`${rider.id}: FE bind rows (live preview bank)`);
 assert.deepEqual(pkg.rig.source_bone_slots,rig.source_bone_slots,`${rider.id}: FE slots`);
 pkg.rig.parts.forEach((p,i)=>{const o=rig.parts[i];assert.deepEqual([p.file,p.vertex_count,p.index_count,p.board,p.morphs.map(m=>m.channel)],[o.file,o.vertex_count,o.index_count,o.board,o.morphs.map(m=>m.channel)],`${rider.id}: FE part ${o.part}`);});
 const mb=fs.readFileSync(pub+dir+'morphs.bin');assert.equal(pkg.morphs.byteLength,mb.byteLength,`${rider.id}: morph bytes`);
 assert(Buffer.from(pkg.morphs.buffer).equals(mb),`${rider.id}: morph targets`);
 const texOf=(wld,b)=>wld.textures[`9-${b.texture}`].resource.toLowerCase();
 world.batches.forEach((b,i)=>assert.equal(texOf(pkg.world,pkg.world.batches[i]),texOf(world,b),`${rider.id}: FE batch ${i}`));
 report.push({fe:rider.id,parts:pkg.rig.parts.length,morphs:pkg.rig.parts.reduce((n,p)=>n+p.morphs.length,0)});
}

// ---- 2d. Equip Gear preview (0x19BFE8): the board quaternion formula against the live slot +0xC60 of two PS2 states
{
 const T=await import('three'),{EquipGearScreen}=await import('./wardrobe.js');
 const screen=JSON.parse(fs.readFileSync(pub+'/assets/WARDROBE/equip-screen.json')).preview;
 const g=Object.create(EquipGearScreen.prototype);Object.defineProperty(g,'cfg',{value:screen});
 for(const [spin,live] of [[13,[0.21556781232357025,-0.7346371412277222,-0.3495430052280426,0.5400540232658386]],[319,[0.05310694873332977,0.4958774149417877,0.6449633240699768,-0.5790574550628662]]]){
  g.view={spin};const q=g.boardQuaternion(T),zup=[q.x,-q.z,q.y,q.w],sign=Math.sign(zup[3])===Math.sign(live[3])?1:-1;
  zup.forEach((v,i)=>assert(Math.abs(v*sign-live[i])<1e-6,`board quaternion spin ${spin}: ${zup} vs ${live}`));
 }
 // entering the Boards folder eases rider and board to the board view, and back (states 3 -> 2, 4 -> 1)
 g.view={mode:3,rider:screen.rider.map(Math.fround),board:screen.board.map(Math.fround),zoom:0,yaw:0,spin:0,frames:0};g.keys=new Set();
 let n=0;while(g.view.mode===3&&n<200){g.step();n++;}
 assert.equal(g.view.mode,2);g.step();assert.deepEqual(g.view.rider,screen.rider_boards);assert.deepEqual(g.view.board,screen.board_boards);g.view.mode=4;let m=0;while(g.view.mode===4&&m<200){g.step();m++;}
 assert.equal(g.view.mode,1);report.push({boards_ease_frames:[n,m]});
}

// ---- 2e. one outfit record per rider (the profile record): career save record, old single-event outfits migrate
{
 const {outfitState,saveOutfit,writeOutfit}=await import('./wardrobe.js');
 const store=new Map(),ls={getItem:k=>store.get(k)??null,setItem:(k,v)=>store.set(k,String(v)),removeItem:k=>store.delete(k)};
 const old=globalThis.localStorage;Object.defineProperty(globalThis,'localStorage',{value:ls,configurable:true});
 const realFetch=globalThis.fetch;globalThis.fetch=fetcher;
 try{
  const zoe=roster.find(r=>r.id==='zoe'),w=await loadWardrobe('zoe',fetcher),legacy=new Wardrobe(w);legacy.toggle(556);
  writeOutfit('zoe',legacy.serialize());                                         // a first-version single-event outfit
  const records={},career={rider:id=>records[id]??={},persisted:0,persist(){this.persisted++;}};
  const ui={careerUI:{career},careerMode:false};
  const cui={careerUI:{career},careerMode:true};
  const wd=await outfitState(cui,zoe);assert(wd.equipped(556)&&records.zoe.gearFlags&&!store.get('ssx3.outfit.v1'),'legacy outfit moves into the career record');
  // a record equipped by the old lodge screen without the commit bit
  records.mac={gearFlags:{...new Wardrobe(await loadWardrobe('mac',fetcher)).serialize(),142:0x12,151:0x22}};
  const mac=await outfitState(cui,roster.find(r=>r.id==='mac'));assert(mac.flags(142)&4&&!(mac.flags(151)&4),'old lodge rows committed');
  // Free play (Single Event / online, not career): every item owned, starting from the outfit the rider wears, saved
  // in its own store; the career record, its purchases and its outfit are untouched.
  const careerBefore=JSON.stringify(records.zoe.gearFlags);
  const free=await outfitState(ui,zoe);assert(free.allOwned&&free.equipped(556),'free play starts from the worn outfit');
  const locked=new Wardrobe(w).list.find(e=>!new Wardrobe(w,records.zoe.gearFlags).owned(e.item)&&e.name&&(e.flags&4));
  assert(locked&&free.owned(locked.item),'free play owns items the career has not bought');
  free.toggle(458);saveOutfit(ui,zoe,free);
  assert.equal(JSON.stringify(records.zoe.gearFlags),careerBefore,'free-play changes never touch the career record');
  assert((await outfitState(ui,zoe)).equipped(458),'free play keeps its outfit');
  assert(!(await outfitState(cui,zoe)).equipped(458),'the career keeps its own outfit');
 }finally{globalThis.fetch=realFetch;Object.defineProperty(globalThis,'localStorage',{value:old,configurable:true});}
 report.push({career_record:'ok',free_play_unlocked:'ok'});
}

// ---- 2f. Sam: the Sam PS2 build's bucket-30 lists (tools/sam_ps2/SamWardrobe.cs) and the package choice ---------
if(fs.existsSync(pub+'/assets/WARDROBE/SAM/wardrobe.json')){
 const w=await loadWardrobe('sam',fetcher),wd=new Wardrobe(w);
 const tree=folder=>wd.menu(folder).map(e=>e.flags&0x20?[e.name,tree(e.item)]:e.name);
 const tops=wd.menu(22).map(e=>e.name);assert.deepEqual(tops,['Midwest Unc','Sunday Unc','Earn Your Turns','Uphill Club','Lodge Legend'],'Sam build tops (gear-names.json)');
 assert(JSON.stringify(tree(-1)).includes('Unc Cut')&&JSON.stringify(tree(-1)).includes('Midwest Mileage'),'Sam build names');
 assert(wd.equipped(49)&&w.sam.default_package==='RIDER_SAM');
 // choice: the most specific outfit whose items are committed (a synthetic Packers mapping on 55 'Uphill Club')
 const w2={...w,sam:{...w.sam,outfits:[...w.sam.outfits.map(o=>o.id==='sunday_unc'?{...o,items:[55]}:o)]}},wd2=new Wardrobe(w2);
 const {samOutfitChoice}=await import('./wardrobe.js');
 assert.equal(samOutfitChoice(w2,wd2)[0]?.id,'rope_tow_regular');assert(wd2.toggle(55));assert.equal(samOutfitChoice(w2,wd2)[0]?.package,'RIDER_SAM_PACKERS');
 report.push({sam:{tops,outfits:w.sam.outfits.map(o=>[o.id,o.package,o.items,o.available]),packages:w.sam.packages.map(p=>[p.package,p.web])}});
}

// ---- 2g. online outfits: outfitKey -> remoteOutfitRider resolves the same race package on another client ---------
{
 const {outfitKey,remoteOutfitRider,prepareOutfit,wardrobeFile}=await import('./wardrobe.js');
 const store=new Map(),ls={getItem:k=>store.get(k)??null,setItem:(k,v)=>store.set(k,String(v)),removeItem:k=>store.delete(k)};
 const old=globalThis.localStorage;Object.defineProperty(globalThis,'localStorage',{value:ls,configurable:true});
 const realFetch=globalThis.fetch;globalThis.fetch=async(u,o)=>o?.method==='HEAD'?{ok:false,headers:{get:()=>''}}:fetcher(u);
 try{
  const records={},career={rider:id=>records[id]??={},persist(){}},ui={careerUI:{career}};
  const zoe=roster.find(r=>r.id==='zoe');
  assert.equal(await outfitKey(ui,zoe),null,'default outfit: no key');
  const wd=new Wardrobe(await loadWardrobe('zoe',fetcher));wd.toggle(556);wd.toggle(66);records.zoe={gearFlags:wd.serialize()};
  const key=await outfitKey(ui,zoe);assert(/^w1:[0-9,]+$/.test(key),key);
  const local=await prepareOutfit(ui,zoe),remote=await remoteOutfitRider(zoe,key,{fetcher});
  assert.equal(remote.root,local.root,'same package root on both clients');
  assert.deepEqual(wardrobeFile(remote.root+'rider.json'),wardrobeFile(local.root+'rider.json'));
  assert.deepEqual(remote.files,['world.json','rider.json','vertices.bin','indices.bin','colors.bin','animation-samples.json'].map(f=>remote.root+f));
  assert(remote.outfit_identity.upper_mask8c0&&remote.outfit_settings.original_animation.secondary_motion.enabled.length===3);
  for(const bad of [null,'w1:','w1:99999','x:1','w1:a,b'])assert.equal((await remoteOutfitRider(zoe,bad,{fetcher})).root,undefined,`bad key ${bad}`);
  const brodi=roster.find(r=>r.id==='brodi');assert.equal((await remoteOutfitRider(brodi,key,{fetcher})).package,'RIDER_BRODI','cheat skins keep their bucket');
  assert.equal((await remoteOutfitRider(roster.find(r=>r.id==='zoe'),'sam:RIDER_SAM_PACKERS',{fetcher})).package,'RIDER_ZOE');
  report.push({online:{key,root:remote.root}});
 }finally{globalThis.fetch=realFetch;Object.defineProperty(globalThis,'localStorage',{value:old,configurable:true});}
}

// ---- 3. changed outfits initialise in the core and ride the Snow Jam start --------------------------------
const createCore=(await import('./runtime/core.js')).default;
const metadataText=fs.readFileSync(pub+'/assets/ANIMATIONS/animation-packets.json','utf8'),packets=fs.readFileSync(pub+'/assets/ANIMATIONS/animation-packets.bin');
const mesh=fs.readFileSync(pub+'/assets/ARA1/collision.bin'),terrain=json('/assets/ARA1/terrain.json'),worldCollision=json('/assets/ARA1/world_collision.json');
// Zoe: Skintight 'Trouble' top (66), 'Dangerous' pigtails (556), 'Stuff' board (215 is sold; 204 'Alternative' reward) -> owned-at-start picks elsewhere
const outfits=[['zoe',[66,556]],['mac',null],['moby',null],['allegra',null],['elise',null],['kaori',null]];
for(const [id,items] of outfits){
 const rider=roster.find(r=>r.id===id),w=await loadWardrobe(id,fetcher),wd=new Wardrobe(w);
 // other riders: every owned-at-start leaf not equipped yet, applied in order (each 0x14AFB0 + commit)
 const pick=items??entries(w).filter(e=>(e.flags&4)&&!(e.flags&0x20)&&e.name&&wd.owned(e.item)&&!wd.equipped(e.item)).map(e=>e.item);
 const done=[];for(const item of pick)if(wd.byItem.has(item)&&wd.set(item,true)){wd.commit();done.push(item);}
 const asm=assembly(w,wd.raceEquipped()),pkg=buildPackage(w,asm,{riderId:id});
 const doc=fs.existsSync(pub+`/assets/${rider.package}/settings.json`)?json(`/assets/${rider.package}/settings.json`):null;
 const character={...(doc||{}),settings:mergeCharacterSettings(doc?.settings||{},{original_animation:{bone_mask:pkg.settings.bone_mask,secondary_motion:{enabled:pkg.settings.secondary}}}),identity:{...(doc?.identity||{}),...pkg.settings.identity}};
 const settings=humanSettings(initial,character),rig=pkg.rig;
 const c=await createCore(),put=b=>{const p=c._malloc(b.length);c.HEAPU8.set(b,p);return p;},str=x=>put(new TextEncoder().encode((typeof x==='string'?x:JSON.stringify(x))+'\0'));
 const explain=fn=>{try{return fn();}catch(e){throw e instanceof Error?e:new Error(`${id}: ${c.getExceptionMessage(e)}`);}};
 explain(()=>{
  {const p=put(mesh);c._init_world(p,mesh.length/4);c._free(p);}
  const m=str(metadataText),r=str(rig),s=str(settings),p=put(packets);c._init_animation(m,r,s,p,packets.length);c._init_race(s);for(const x of [m,r,s,p])c._free(x);
  c._animation_use_physics(1);
  {const t=str(terrain);c._init_terrain(t);c._free(t);}
  {const w2=str(worldCollision),h=str(terrain.source_sha256);c._init_world_collision(w2,h);c._free(w2);c._free(h);}
  {const t=str(terrain);c._init_body_terrain(t);c._free(t);}
 });
 const f=(ptr,n)=>new Float32Array(c.HEAPF32.buffer,ptr,n).slice(),masks=f(c._upper_request_info(),12);
 assert.equal(masks[6]*65536+masks[7],Number(BigInt(pkg.settings.identity.upper_mask8c0)),`${id}: channel-1 mask`);
 explain(()=>c._start_event());
 let released=-1,origin=null,far=0;
 explain(()=>{for(let tick=0;tick<600;tick++){
  const tuck=released>=0&&tick<released+200;c._start_input(0);c._ride_command(0,tuck?c._original_axis(1):0,0,0,0,0,0,0,0,1);c._race_begin();
  const r=f(c._step_rider(0,0,0,0),16),pose=f(c._animation_tick(r[7],0,0,r[9],r[8],0,0,0,0,0,r[15],0),rig.bones.length*7);
  assert(pose.every(Number.isFinite),`${id}: finite pose at ${tick}`);c._race_end();
  const start=f(c._start_info(),7);if(released<0&&!start[1]){released=tick;origin=r.slice(0,3);}if(origin)far=Math.max(far,Math.hypot(r[0]-origin[0],r[2]-origin[2]));
 }});
 assert(released>=180&&released<240,`${id}: released ${released}`);assert(far>10,`${id}: rode ${far}`);
 report.push({outfit:id,items:done,parts:asm.parts.filter(p=>!w.hidden_slots.includes(p.slot)).map(p=>w.parts[p.resource].name),textures:asm.textures,bones:rig.bones.length,secondary:pkg.settings.secondary,released,metres:+far.toFixed(1)});
}
console.log(JSON.stringify(report));
console.log('Wardrobe: rules, 30 default assemblies == packages (bit-exact binds/slots/masks), changed outfits initialise');
