// Lodge economy and inventory rules ported from SLUS_207.72 (tables: tools/export_lodge_shop.py ->
// public/assets/CAREER/shop.json). Pure logic; screens are web/lodge-ui.js. R = per-character save record.
//
//   Gear (DATA/CHAR/BOLTPS2.DAT): inventory R+0x290 {item, flags 0x2 owned / 0x10 equipped / 0x4 / 0x20 default},
//     init 0x1513B8, equip 0x151C90 + rules 0x151EF0, buy 0x14B560, Buy Gear list 0x19B180.
//   Uber tricks (table 0x45AEB8, lists read by 0x14FF90): R+0xBE6+cat*6 {base, selected, u16 lock, u16 visible},
//     init 0x150558, buy 0x1854A0/0x184F40 -> 0x14FE08, select 0x14FD80.
//   Songs (MUSIC.INF, ADDTOFE=1): owned R+0xF70, playlist R+0xF78; 0x158558 / 0x1988D8: 6 free credits, then $5,000.

// ---- gear ------------------------------------------------------------------------------------------------
export class GearInventory {
 // table: shop.gear.runtime[character] = {entries:[[item,class,parent,order,flags,price,tier,name]], rules, defaults}
 constructor(table,flags=null){
  this.entries=table.entries.map(([item,cls,parent,order,flags,price,tier,name])=>({item,cls,parent,order,flags,price,tier,name}));
  this.by=new Map();for(const e of this.entries)if(!this.by.has(e.item))this.by.set(e.item,e);
  this.rules=table.rules.map(([on,item,cond,condState,target,desired])=>({on,item,cond,condState,target,desired}));
  this.defaults=table.defaults;this.f=new Map(this.entries.map(e=>[e.item,0]));
  if(flags)for(const [k,v] of Object.entries(flags))this.f.set(+k,v);else this.init();
 }
 flags(item){return this.f.get(item)??0;}
 owned(item){return !!(this.flags(item)&2);}
 equipped(item){return !!(this.flags(item)&0x10);}
 set(item,v){this.f.set(item,v);}
 // 0x151C90(R,item,on)
 equip(item,on){
  const e=this.by.get(item);if(!e)return 0;
  const was=this.flags(item)&0x10;this.set(item,on?this.flags(item)|0x10:this.flags(item)&~0x10);
  if(on){
   if(was)return 1;
   if(e.flags&1)for(const s of this.entries)if(s.cls===e.cls&&s.item!==item&&((this.flags(s.item)&0x10)||(s.flags&8)))if(!this.equip(s.item,0))return 0;
   return this.apply(item,1);
  }
  if(was||(e.flags&8))for(const k of this.entries)if(k.cls===item&&!this.equip(k.item,0))return 0;
  return was?this.apply(item,0):1;
 }
 // 0x151EF0: rules of (character, item, on); a rule may be conditioned on another item's equipped state.
 apply(item,on){
  for(const r of this.rules){
   if(r.item!==item||(r.on!==0)!==!!on||r.target===-1)continue;
   if(r.cond!==-1&&!!(this.flags(r.cond)&0x10)!==(r.condState!==0))continue;
   const desired=r.desired!==0,current=!!(this.flags(r.target)&0x10);
   if(current===desired){if(current)continue;const te=this.by.get(r.target);if(!te||!(te.flags&8))continue;}
   if(r.target!==item){if(!this.equip(r.target,desired))return 0;}
   else this.set(item,desired?this.flags(item)|0x10:this.flags(item)&~0x10);
  }
  return 1;
 }
 // 0x1513B8: default outfit, then price field 0 = owned, -1 = owned + equipped.
 init(){
  for(const item of this.defaults)this.equip(item,1);
  for(const [k,v] of this.f)if(v&0x10)this.f.set(k,v|0x26);
  let found=false;
  for(const e of this.entries){if(e.price===0)this.set(e.item,this.flags(e.item)|2);else if(e.price===-1){this.set(e.item,this.flags(e.item)|2);this.equip(e.item,1);found=true;}}
  if(found)for(const [k,v] of this.f)this.f.set(k,v&0x10?v|6:v&~4);
 }
 // 0x14B560: own the item and every entry bundled with it (price field == -item).
 buy(item){this.set(item,this.flags(item)|2);for(const e of this.entries)if(e.price===-item)this.set(e.item,this.flags(e.item)|2);}
 children(parent,depth=-1,intoFolders=false){ // 0x14D7E8
  const out=[];for(const e of this.entries)if(e.parent===parent){out.push(e);if(depth!==0&&(!(e.flags&0x20)||intoFolders))out.push(...this.children(e.item,depth-1,intoFolders));}return out;
 }
 // 0x19B180 buy mode: named unowned leaves sold in this lodge peak (tier == peak), folders holding one; by +0xA.
 buyList(folder,peak){
  const res=[];
  for(const e of this.children(folder)){
   if(!e.name)continue;
   if(e.flags&0x20){if(this.children(e.item,-1,true).some(c=>(c.flags&4)&&!(c.flags&0x20)&&!this.owned(c.item)&&c.tier===peak))res.push(e);}
   else if((e.flags&4)&&!this.owned(e.item)&&e.tier===peak)res.push(e);
  }
  return res.sort((a,b)=>a.order-b.order);
 }
 // Equip Gear list (same walker, owned leaves; INFERRED from the buy walker and PS2 Equip Gear frames).
 equipList(folder){
  const res=[];
  for(const e of this.children(folder)){
   if(!e.name)continue;
   if(e.flags&0x20){if(this.children(e.item,-1,true).some(c=>(c.flags&4)&&!(c.flags&0x20)&&this.owned(c.item)))res.push(e);}
   else if((e.flags&4)&&this.owned(e.item))res.push(e);
  }
  return res.sort((a,b)=>a.order-b.order);
 }
 price(item){const e=this.by.get(item);return e&&e.price>0?e.price*10:0;}
 pool(flag){return this.entries.filter(e=>(e.flags&flag)&&!this.owned(e.item));}   // award 17..31 pools (0x156C70/0x156EE0)
 serialize(){const o={};for(const [k,v] of this.f)if(v)o[k]=v;return o;}
}

// ---- uber tricks ------------------------------------------------------------------------------------------
export const UBER_ROWS=[['Mute',1],['Indy',3],['Stalefish',2],['Method',0],['Nose Grab',4],['Tail Grab',9]];  // Ubertrick Setup rows -> category
export function initialUber(shop,character){
 const hex=shop.uber_tricks.per_rider_defaults[character].save_bytes,b=hex.match(/../g).map(x=>parseInt(x,16)),out={};
 for(const c of shop.uber_tricks.categories){const o=c.category*6;out[c.category]={base:b[o],selected:b[o+1],lock:b[o+2]|b[o+3]<<8,visible:b[o+4]|b[o+5]<<8};}
 return out;
}
export function uberEntries(shop,category){return shop.uber_tricks.categories.find(c=>c.category===category)?.entries||[];}
// In the air the uber of grab slot c is row byte 0x530EC0 + bank*0x13EC + char*0x1FE + c*6 + (tier >= 5) of the table
// 0x45AEB8 list (0x1352A8 -> 0x150198 -> 0x14FEA8), read live: +0 the hidden base uber, +1 the Ubertrick Setup selection
// (written by 0x14FD80 from the lodge and the front end). The browser's grab profile holds the two rows per slot
// (original_grab_control.profile.uber[set][slot], set 1 = tier >= 5): {semantic = trick id A, upper_semantic = trick id B,
// score_id = the entry's name index, begin / hold points = the trick score table 0x530600 by score id}.
export function uberRow(entry,points){
 const [begin,hold]=points?.[entry.name_index]||[0,0];
 return {semantic:entry.trick_ids[0],upper_semantic:entry.trick_ids[1],score_id:entry.name_index,begin_points:begin,hold_points:hold};
}
// Set-1 rows [1, slot, row] for the categories whose selection differs from the rider's own default (the rider packages'
// settings already hold the defaults: Zoe's twelve rows in initial.json equal these rows exactly).
export function uberChoiceRows(shop,points,character,selection){
 const defaults=shop?.uber_tricks?.per_rider_defaults?.[character]?.rows||{},out=[];
 for(const [name,category] of UBER_ROWS){
  const chosen=selection?.[category],def=defaults[name]?.owned_default?.entry;
  if(chosen==null||chosen===def)continue;
  const entry=uberEntries(shop,category)[chosen];
  if(entry)out.push([1,category,uberRow(entry,points)]);
 }
 return out;
}

// ---- songs ------------------------------------------------------------------------------------------------
export const SONG_FREE_CREDITS=6,SONG_PRICE=5000;
