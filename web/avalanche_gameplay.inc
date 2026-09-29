// Avalanche / rock-slide playback in the core (docs/avalanche.md; engine/avalanche.hpp, bit-exact against the original,
// tests/avalanche_live.cpp). Included from web/stage_script_gameplay.inc (builtins 94 / 95) before the builtin dispatch.
//   init_avalanches(text, append): a location's tools/export_avalanches.py document (<LOC>/avalanches.json): an event course's
//     definitions (append 0), or a streamed location's added to them (1: the list gp+2496 keeps a location's own order, a later
//     location's first, as 0x2D92D0 inserts at the head).
//   builtin 94 (0x305C88 -> 0x2D97A8): the trigger; the new tumblers' +96 is the instance's translation (instance +0x40).
//   Per tick (0x2D8948: the PathArrow environment component of group 3, after the entity pass): every slot's 0x2D7EF8,
//     at the end of advance_world_entities (web/roller_gameplay.inc), so a slot-2 trigger of tick N and a slot-5 one of the
//     entity pass both see their first step before the rider pass of N+1, as on the PS2.
//   builtin 95 (0x305D90, the AvaSpline modifier 0x48F338): the pieces whose matrix follows their tumbler (0x2D9C00, read
//     by the draw: avalanche_pieces).
//   The pieces' collision (builtins 0 + 95 of the trigger program; docs/avalanche.md "Collision"): see "Pieces' collision" below.
#include "../engine/avalanche.hpp"
RIDER_LOCAL static ssx::OriginalAvalancheWorld avalancheWorld;
RIDER_LOCAL static std::vector<uint32_t> avalancheReleased;   // instances released (entity vt+0x08(3)) since the last drain
RIDER_LOCAL static uint32_t avalancheTriggers=0,avalancheTicks=0;
// ---- Pieces' collision ------------------------------------------------------------------------------------------------------
// A collidable group instance (the type-3 rocks: authored flags 0x40210000, countdown seed 0x40214123 / 0x40214023, the static
// route 0x20 at the authored place) gets, from the trigger program:
//   builtin 0 (2FC0D0 -> 356DB0, key 0 the piece, keys 1 / 2 = 0): an Object entity (vtable 0x490E80) in entity group 1.
//     34FB00 -> 2D1BF0 -> 1032C0 flags |= 0x100; 356DB0 (key 2 = 0) (flags & 3) == 3 -> flags & ~2 | 4; key 1 -> & ~0x40 | 0x20.
//   builtin 95 (305D90 -> 355A78: gp+0x9D4, the record's bad-magic flag, is 0) -> 357750 AvaSpline (0x90 bytes): +0x40 = the
//     instance matrix (instance +0x10), +0x80 = the instance; attach 3554B0 -> 356780 -> 350570: bounds +0x10 / +0x20 = the
//     instance box +0x60 / +0x6C (w 1), radius +0x30 = the largest corner distance from the instance translation (sqrt.s);
//     flags & ~0x20 | 0x40: the entity route (EBA3 0x40214123 -> 0x40214145, the PS2 value).
//   Entity route: 334458 bounds vt+0x164 / +0x16C (3569D0 / 356A00 -> 352B88 -> modifier vt+0x64 0x361AD8 = +0x10 / +0x20);
//     334888 root vt+0xCC 356128: modifier vt+0x9C 0x360BC0 = 0, so vt+0x94 0x361B10 = +0x40; 1057B8 rigid predicate
//     355420 -> vt+0x44 0x360B60 = 1; 104E70 selected callback 356AE0 -> 353098 -> vt+0xB4 0x360BD8: nothing; 121818 355770
//     -> vt+0x54 0x360B70: nothing, gate vt+0x4C 0x360B68 = 1.
//   Each tick in entity group 1 (before the riders, after the previous tick's group 3 stepped the tumblers): 356198 ->
//     3556F8 -> 357820 -> 2D1CF0 -> 2D9C00 (+0x40 = the tumbler's matrix; unchanged without a tumbler), then 355028(group 1)
//     -> 360DA0 -> 3568B0: bounds = +0x40 translation -/+ (r, r, r, 0) (VU vsub / vadd) and 3291E0 relocates the instance in
//     the octree (not modelled: the port keeps the octree order of the load, as for the chairlifts).
//   The tumbler's release (2D7DD8 -> entity vt+0x08(3) = 361038 -> 3553C0 -> 34FBF0): flags = (flags & 0xFFFF0300) |
//     (flags >> 16) | 2, i.e. the static route at the authored place again (0x40214145 -> 0x40214123). Type-2 pieces (EBA3's
//     rocks) are never released.
// The composed geometry goes to the instance's nodes and box too (the ray queries read those, as for place_chair), and the
// authored ones come back at the release / a new race. Only a core that loaded the definitions attaches (the human's:
// computer-rider contexts keep the static rocks, docs/avalanche.md "Open").
struct AvalanchePiece {
 uint32_t resource=0;bool spline=false;                               // an Object entity; spline: its AvaSpline is attached
 ssx::SplineQuad low{},high{};float radius=0;ssx::AvaMatrix matrix{};  // AvaSpline +0x10 / +0x20, +0x30, +0x40
 collision_transform::Matrix authoredMatrix{};terrain_original::Vector authoredLow{},authoredHigh{}; // instance +0x10, +0x60 / +0x6C
 std::vector<std::pair<collision_transform::Matrix,collision_transform::Matrix>> authoredNodes;       // the static node matrices
 bool placed=false;ssx::AvaMatrix placedMatrix{};ssx::SplineQuad placedLow{},placedHigh{};
};
RIDER_LOCAL static std::vector<AvalanchePiece> avalancheEntities;
static AvalanchePiece* avalanche_entity(uint32_t resource){for(auto& p:avalancheEntities)if(p.resource==resource)return &p;return nullptr;}
bool browser_avalanche_entity(uint32_t resource){return avalanche_entity(resource)!=nullptr;} // web/roller_gameplay.inc, web/stage_world.inc
// The piece's instance, while it is still the one the piece was recorded on (a new world may reuse the resource id).
static WorldCollisionInstance* avalanche_entity_instance(const AvalanchePiece& p){
 auto* w=set_piece_instance_at(p.resource);
 return w&&w->nodes.size()==p.authoredNodes.size()&&std::memcmp(w->authoredMatrix.data(),p.authoredMatrix.data(),sizeof p.authoredMatrix)==0?w:nullptr;
}
static void avalanche_entity_place(AvalanchePiece& p){
 if(p.placed&&std::memcmp(&p.placedMatrix,&p.matrix,sizeof p.matrix)==0&&std::memcmp(&p.placedLow,&p.low,sizeof p.low)==0&&std::memcmp(&p.placedHigh,&p.high,sizeof p.high)==0)return;
 auto* w=avalanche_entity_instance(p);if(!w)return;
 collision_transform::Matrix base;for(unsigned i=0;i<4;i++)for(unsigned k=0;k<4;k++)base[i*4+k]=p.matrix[i][k];
 auto entity=std::make_shared<const WorldCollisionEntity>(composeWorldCollisionEntity(*w,base,{p.low[0],p.low[1],p.low[2]},{p.high[0],p.high[1],p.high[2]}));
 for(size_t n=0;n<w->nodes.size();n++){w->nodes[n].world=entity->world[n];w->nodes[n].inverse=entity->inverse[n];}
 w->low=entity->low;w->high=entity->high;w->entity=std::move(entity);
 p.placed=true;p.placedMatrix=p.matrix;p.placedLow=p.low;p.placedHigh=p.high;
}
// The authored static geometry back (the release, a new race, a location leaving the world).
static void avalanche_entity_restore(const AvalanchePiece& p){
 auto* w=avalanche_entity_instance(p);if(!w)return;
 for(size_t n=0;n<w->nodes.size();n++){w->nodes[n].world=p.authoredNodes[n].first;w->nodes[n].inverse=p.authoredNodes[n].second;}
 w->low=p.authoredLow;w->high=p.authoredHigh;w->entity.reset();
}
static bool avalanche_group_instance(uint32_t resource){
 for(const auto& d:avalancheWorld.defs)for(const auto& g:d.groups)if(g.resource==resource)return true;return false;
}
// builtin 0 on a collidable group instance without an entity.
static void avalanche_object(uint32_t resource,uint32_t key1,uint32_t key2){
 if(!resource||avalanche_entity(resource)||!avalanche_group_instance(resource))return;
 auto* w=set_piece_instance_at(resource);if(!w||w->type==0||w->hierarchyLocal.empty()||!w->unsupported.empty())return;
 AvalanchePiece p;p.resource=resource;p.authoredMatrix=w->authoredMatrix;p.authoredLow=w->low;p.authoredHigh=w->high;
 for(const auto& n:w->nodes)p.authoredNodes.emplace_back(n.world,n.inverse);
 avalancheEntities.push_back(std::move(p));
 uint32_t f=stage_flags(resource)|0x100u;if(!key2&&(f&3u)==3u)f=(f&~2u)|4u;if(key1)f=(f&~0x40u)|0x20u;stage_set_flags(resource,f);
}
// builtin 95 on an Object piece: (re)attach its AvaSpline (a new one starts again from the instance matrix).
static void avalanche_spline(uint32_t resource){
 auto* p=avalanche_entity(resource);if(!p)return;
 const auto& a=p->authoredMatrix;for(unsigned i=0;i<4;i++)p->matrix[i]={a[i*4],a[i*4+1],a[i*4+2],a[i*4+3]};
 const uint32_t flags=ssx::originalSplineAttachBounds(p->low,p->high,p->radius,p->matrix[3],p->authoredLow,p->authoredHigh,stage_flags(resource));
 p->spline=true;stage_set_flags(resource,flags);avalanche_entity_place(*p);
}
// Group 1: every attached AvaSpline follows its tumbler, then 3568B0.
static void avalanche_entities_tick(){
 for(auto& p:avalancheEntities){
  if(!p.spline)continue;
  ssx::AvaMatrix m;if(ssx::originalAvalancheInstanceMatrix(avalancheWorld,p.resource,m))p.matrix=m;
  {using namespace roller_math;OriginalRounding rounding;const ssx::SplineQuad extent{p.radius,p.radius,p.radius,0.f};p.low=vsub(p.matrix[3],extent);p.high=vadd(p.matrix[3],extent);}
  avalanche_entity_place(p);
 }
}
// 2D7DD8 -> the Object's destruction (34FBF0 flags), for the pieces released since `from` in avalancheReleased.
static void avalanche_entities_released(size_t from){
 for(size_t k=from;k<avalancheReleased.size();k++){
  const uint32_t r=avalancheReleased[k];auto it=std::find_if(avalancheEntities.begin(),avalancheEntities.end(),[r](const AvalanchePiece& p){return p.resource==r;});
  if(it==avalancheEntities.end())continue;
  const uint32_t masked=stage_flags(r)&0xFFFF0300u;stage_set_flags(r,masked|uint32_t(int32_t(masked)>>16)|2u);
  avalanche_entity_restore(*it);avalancheEntities.erase(it);
 }
}
static void avalanche_entities_clear(bool restore){if(restore)for(const auto& p:avalancheEntities)avalanche_entity_restore(p);avalancheEntities.clear();}
// A streamed location leaving the world (web/stage_world.inc browser_stage_track_reset): its pieces lose their entities.
void browser_avalanche_track_reset(uint32_t track){
 std::erase_if(avalancheEntities,[track](const AvalanchePiece& p){if((p.resource&255u)!=track)return false;avalanche_entity_restore(p);return true;});
}
// A new race: no slot, every tumbler free, no piece entity (the definitions stay with the world; web/stage_world.inc puts the
// countdown flags back first).
static void avalanche_reset(){auto defs=std::move(avalancheWorld.defs);avalancheWorld=ssx::OriginalAvalancheWorld{};avalancheWorld.defs=std::move(defs);avalancheReleased.clear();avalanche_entities_clear(true);}
static void avalanche_trigger(int32_t id){
 if(!ssx::originalAvalancheTrigger(avalancheWorld,id))return;++avalancheTriggers;
 for(int32_t k=0;k<16;k++)if(avalancheWorld.slots[size_t(k)].avalanche>=0&&avalancheWorld.slots[size_t(k)].t==0.f)
  ssx::originalAvalancheSetStart(avalancheWorld,k,[](uint32_t resource){ // instance +0x40: the instance matrix's translation row
   if(const auto* w=set_piece_instance_at(resource))return ssx::AvaQuad{w->authoredMatrix[12],w->authoredMatrix[13],w->authoredMatrix[14],w->authoredMatrix[15]};
   return ssx::AvaQuad{0,0,0,1};});
}
// race_begin (web/roller_gameplay.inc advance_world_entities): the previous tick's group 3 (the slots), then this tick's group 1
// part of the pieces (their AvaSpline and bounds), before the riders.
void browser_avalanche_tick(){
 bool any=false;for(const auto& s:avalancheWorld.slots)any|=s.avalanche>=0;
 if(any){
  const size_t released=avalancheReleased.size();
  for(auto& slot:avalancheWorld.slots)ssx::originalAvalancheSlotTick(avalancheWorld,slot,avalancheReleased);
  ++avalancheTicks;avalanche_entities_released(released);
 }
 avalanche_entities_tick();
}
extern "C" {
EMSCRIPTEN_KEEPALIVE int init_avalanches(const char* text,int append){
 const auto doc=nlohmann::json::parse(text);std::vector<ssx::OriginalAvalancheDef> defs;
 auto word=[](const nlohmann::json& v){return std::bit_cast<float>(v.get<uint32_t>());};
 for(const auto& a:doc.at("avalanches")){
  ssx::OriginalAvalancheDef d;d.id=a.at("id");
  for(const auto& g:a.at("groups")){
   ssx::OriginalAvalancheGroup x;x.resource=g.at("resource");x.type=uint16_t(g.at("type").get<int>());x.hasEmitter=!g.at("emitter").is_null();
   x.duration=word(g.at("duration"));x.fadeIn=word(g.at("fade_in"));x.fadeOut=word(g.at("fade_out"));x.speed=word(g.at("speed"));
   const auto& st=g.at("start");x.start={word(st[0]),word(st[1]),word(st[2]),1.f};x.avaSpline=g.value("ava_spline",false);
   const std::string hex=g.at("samples");x.samples.resize(hex.size()/2);for(size_t k=0;k<x.samples.size();k++)x.samples[k]=uint8_t(std::stoul(hex.substr(2*k,2),nullptr,16));
   if(x.samples.size()!=size_t(int32_t(x.duration))*10)throw std::runtime_error("Avalanche samples do not match the duration");
   d.groups.push_back(std::move(x));
  }
  for(const auto& s:a.value("sounds",nlohmann::json::array()))d.sounds.push_back({uint16_t(s.at("tick").get<int>()),uint16_t(s.at("tumbler").get<int>())});
  defs.push_back(std::move(d));
 }
 if(!append){avalancheWorld=ssx::OriginalAvalancheWorld{};avalanche_entities_clear(false);} // a new world: its instances are new
 avalancheWorld.defs.insert(avalancheWorld.defs.begin(),std::make_move_iterator(defs.begin()),std::make_move_iterator(defs.end()));
 return int(defs.size());
}
EMSCRIPTEN_KEEPALIVE void avalanche_clear(){avalancheWorld=ssx::OriginalAvalancheWorld{};avalancheReleased.clear();avalanche_entities_clear(true);}
// QA / tests: the pieces' Object entities [count, then per piece 28 words: resource, instance flags (+8), AvaSpline attached,
// radius (+0x30) bits, bounds +0x10 / +0x20 (8 bits words), matrix +0x40 (16 bits words)] (web/test-avalanche-collision.mjs).
EMSCRIPTEN_KEEPALIVE uint32_t* avalanche_entities(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,uint32_t(avalancheEntities.size()));
 for(const auto& p:avalancheEntities){
  out.insert(out.end(),{p.resource,stage_flags(p.resource),p.spline?1u:0u,std::bit_cast<uint32_t>(p.radius)});
  for(float v:p.low)out.push_back(std::bit_cast<uint32_t>(v));for(float v:p.high)out.push_back(std::bit_cast<uint32_t>(v));
  for(const auto& row:p.matrix)for(float v:row)out.push_back(std::bit_cast<uint32_t>(v));
 }
 return out.data();
}
// QA / tests: 94 from the host (a trigger volume the harness cannot reach).
EMSCRIPTEN_KEEPALIVE int avalanche_trigger_qa(int id){const uint32_t before=avalancheTriggers;avalanche_trigger(id);return avalancheTriggers!=before;}
// [definitions, triggers, ticks, active slots, then per slot 16 x (avalanche id or -1, t bits, sound cursor)]
EMSCRIPTEN_KEEPALIVE uint32_t* avalanche_info(){
 RIDER_LOCAL static std::vector<uint32_t> out;uint32_t active=0;for(const auto& s:avalancheWorld.slots)active+=s.avalanche>=0;
 out={uint32_t(avalancheWorld.defs.size()),avalancheTriggers,avalancheTicks,active};
 for(const auto& s:avalancheWorld.slots){out.push_back(s.avalanche>=0?uint32_t(avalancheWorld.defs[size_t(s.avalanche)].id):0xFFFFFFFFu);out.push_back(std::bit_cast<uint32_t>(s.t));out.push_back(uint32_t(s.soundCursor));}
 return out.data();
}
// The draw and the rumble loop (web/, the rendering-sweep agent): the pieces 0x2DA1C0 walks, i.e. every tumbler with a group (+736)
// in the active slots, slot by slot in list order: [count, then per piece 22 floats: resource, state (1 following its tumbler,
// 2 a type-2 piece: stays at its last pose), AvaSpline follower, emitter, the 0x2D9C00 matrix (16, source rows: rotation x scale,
// translation), alpha, scale]; then the instances released since the last call (entity vt+0x08(3): hidden) [n, resources...];
// then the loop: [refcount (0x29DEF0, audio+0x6040), n changes since the last call, the refcount after each].
// The AvaSpline followers' draw deltas go through moving_instances() (browser_avalanche_moving).
EMSCRIPTEN_KEEPALIVE float* avalanche_pieces(){
 RIDER_LOCAL static std::vector<float> out;out.assign(1,0.f);
 for(const auto& slot:avalancheWorld.slots){
  if(slot.avalanche<0)continue;
  for(int32_t k=slot.list;k>=0;k=avalancheWorld.pool[size_t(k)].next){
   const auto& tu=avalancheWorld.pool[size_t(k)];if(tu.group<0)continue;
   const auto& g=avalancheWorld.defs[size_t(tu.avalanche)].groups[size_t(tu.group)];ssx::AvaMatrix m;ssx::originalAvalancheInstanceMatrix(avalancheWorld,g.resource,m);
   ++out[0];out.insert(out.end(),{float(g.resource),g.type==2?2.f:1.f,g.avaSpline?1.f:0.f,g.hasEmitter?1.f:0.f});
   for(const auto& row:m)out.insert(out.end(),row.begin(),row.end());out.push_back(tu.alpha);out.push_back(tu.scale);
  }
 }
 out.push_back(float(avalancheReleased.size()));for(uint32_t r:avalancheReleased)out.push_back(float(r));avalancheReleased.clear();
 out.push_back(float(avalancheWorld.loopRefs));out.push_back(float(avalancheWorld.loopTransitions.size()));for(int32_t v:avalancheWorld.loopTransitions)out.push_back(float(v));avalancheWorld.loopTransitions.clear();
 return out.data();
}
}
// moving_instances() (web/roller_gameplay.inc): every AvaSpline follower a tumbler drives, key = its resource, the draw delta of its
// 0x2D9C00 matrix against the authored instance matrix (the pieces' moving batches, baked with that matrix).
void browser_avalanche_moving(std::vector<float>& out,void(*append)(std::vector<float>&,float,const collision_transform::Matrix&,const collision_transform::Matrix&)){
 for(const auto& slot:avalancheWorld.slots){
  if(slot.avalanche<0)continue;
  for(int32_t k=slot.list;k>=0;k=avalancheWorld.pool[size_t(k)].next){
   const auto& tu=avalancheWorld.pool[size_t(k)];if(tu.group<0)continue;
   const auto& g=avalancheWorld.defs[size_t(tu.avalanche)].groups[size_t(tu.group)];if(!g.avaSpline)continue;
   const auto* w=set_piece_instance_at(g.resource);if(!w)continue;
   ssx::AvaMatrix m;if(!ssx::originalAvalancheInstanceMatrix(avalancheWorld,g.resource,m))continue;
   collision_transform::Matrix mm;for(unsigned i=0;i<4;i++)for(unsigned c=0;c<4;c++)mm[i*4+c]=m[i][c];
   append(out,float(g.resource),mm,w->authoredMatrix);
  }
 }
}
