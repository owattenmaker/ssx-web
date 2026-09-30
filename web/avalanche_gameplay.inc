// Avalanche / rock-slide playback in the core (docs/avalanche.md; engine/avalanche.hpp, bit-exact against the original,
// tests/avalanche_live.cpp). Included from web/stage_script_gameplay.inc (builtins 94 / 95) before the builtin dispatch.
//   init_avalanches(text, append): a location's tools/export_avalanches.py document (<LOC>/avalanches.json): an event course's
//     definitions (append 0), or a streamed location's added to them (1: the list gp+2496 keeps a location's own order, a later
//     location's first, as 0x2D92D0 inserts at the head).
//   builtin 94 (0x305C88 -> 0x2D97A8): the trigger; the new tumblers' +96 is the instance's translation (instance +0x40).
//   Per tick (0x2D8948, the PathArrow environment component: after entity group 1 and before the riders, as the capture records
//     show): every slot's 0x2D7EF8 at the end of advance_world_entities (web/roller_gameplay.inc), after the pieces' group-1
//     AvaSpline update, so a slot-2 trigger of tick N and a slot-5 one of the entity pass both see their first step before the
//     rider pass of N+1, as on the PS2. The trails emitters' births draw the presentation stream (docs/avalanche.md "Trails").
//   builtin 95 (0x305D90, the AvaSpline modifier 0x48F338): the pieces whose matrix follows their tumbler (0x2D9C00, read
//     by the draw: avalanche_pieces).
//   The pieces' collision (builtins 0 + 95 of the trigger program; docs/avalanche.md "Collision"): see "Pieces' collision" below.
#include "../engine/avalanche.hpp"
RIDER_LOCAL static ssx::OriginalAvalancheWorld avalancheWorld;
// The world is reset in place: a whole OriginalAvalancheWorld temporary (the 64 trails emitters are ~40 KB) overflowed the wasm
// stack. play: the slots, the tumblers and the loop; defs / emitters: those too (a new world: its emitters start constructed anew).
static void avalanche_world_reset(bool defs,bool emitters){
 for(auto& s:avalancheWorld.slots)s=ssx::OriginalAvalancheSlot{};for(auto& t:avalancheWorld.pool)t=ssx::OriginalTumbler{};
 avalancheWorld.activeCount=0;avalancheWorld.loopRefs=0;avalancheWorld.loopTransitions.clear();
 if(defs)avalancheWorld.defs.clear();if(emitters)for(auto& e:avalancheWorld.emitters)e=ssx::OriginalAvalancheEmitter{};
}
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
//   Each tick in entity group 1, before the riders and before that tick's tumbler step (the order the PS2 records show):
//     356198 -> 3556F8 -> 357820 -> 2D1CF0 -> 2D9C00 (+0x40 = the tumbler's matrix; unchanged without a tumbler), then
//     355028(group 1) -> 360DA0 -> 3568B0: bounds = +0x40 translation -/+ (r, r, r, 0) (VU vsub / vadd) and 3291E0 relocates
//     the instance in the octree (not modelled: the port keeps the octree order of the load, as for the chairlifts).
//   The tumbler's release (2D7DD8 -> entity vt+0x08(3) = 361038 -> 3553C0 -> 34FBF0): flags = (flags & 0xFFFF0300) |
//     (flags >> 16) | 2, i.e. the static route at the authored place again (0x40214145 -> 0x40214123). Type-2 pieces (EBA3's
//     rocks) are never released.
// 305D90 attaches only to an instance that has an entity (+0xC, vt+0x84): no avalanche piece has one at the countdown, so a
// piece follows its tumbler (the draw: moving_instances, avalanche_pieces) only after a builtin 0 of its own (ABC1 ava1bitB_1003
// gets builtin 95 from the snowfield timer but never a builtin 0: it stays at its authored place, and no release destroys it).
// A collidable piece's composed geometry goes to the instance's nodes and box too (the ray queries read those, as for
// place_chair), and the authored ones come back at the release / a new race. Only the instance flags of collidable pieces are
// written (the others' flags only steer the draw, web/avalanche-state.js). Only a core that loaded the definitions attaches
// (the human's: computer-rider contexts keep the static rocks, docs/avalanche.md "Open").
struct AvalanchePiece {
 uint32_t resource=0;bool collidable=false,spline=false;              // an Object entity; spline: its AvaSpline is attached
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
 if(!p.collidable)return;
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
 if(!p.collidable||!p.placed)return;
 auto* w=avalanche_entity_instance(p);if(!w)return;
 for(size_t n=0;n<w->nodes.size();n++){w->nodes[n].world=p.authoredNodes[n].first;w->nodes[n].inverse=p.authoredNodes[n].second;}
 w->low=p.authoredLow;w->high=p.authoredHigh;w->entity.reset();
}
static bool avalanche_group_instance(uint32_t resource){
 for(const auto& d:avalancheWorld.defs)for(const auto& g:d.groups)if(g.resource==resource)return true;return false;
}
// builtin 0 on a group instance without an entity.
static void avalanche_sync();
static void avalanche_object(uint32_t resource,uint32_t key1,uint32_t key2){
 avalanche_sync();
 if(!resource||avalanche_entity(resource)||!avalanche_group_instance(resource))return;
 auto* w=set_piece_instance_at(resource);if(!w)return;
 AvalanchePiece p;p.resource=resource;p.authoredMatrix=w->authoredMatrix;p.authoredLow=w->low;p.authoredHigh=w->high;
 p.collidable=w->type!=0&&!w->hierarchyLocal.empty()&&w->unsupported.empty();
 if(p.collidable)for(const auto& n:w->nodes)p.authoredNodes.emplace_back(n.world,n.inverse);
 avalancheEntities.push_back(std::move(p));
 if(!avalancheEntities.back().collidable)return;
 uint32_t f=stage_flags(resource)|0x100u;if(!key2&&(f&3u)==3u)f=(f&~2u)|4u;if(key1)f=(f&~0x40u)|0x20u;stage_set_flags(resource,f);
}
// builtin 95 on an Object piece: (re)attach its AvaSpline (a new one starts again from the instance matrix).
static void avalanche_spline(uint32_t resource){
 auto* p=avalanche_entity(resource);if(!p)return;
 const auto& a=p->authoredMatrix;for(unsigned i=0;i<4;i++)p->matrix[i]={a[i*4],a[i*4+1],a[i*4+2],a[i*4+3]};
 const uint32_t flags=ssx::originalSplineAttachBounds(p->low,p->high,p->radius,p->matrix[3],p->authoredLow,p->authoredHigh,stage_flags(resource));
 p->spline=true;if(p->collidable){stage_set_flags(resource,flags);avalanche_entity_place(*p);}
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
// 2D7DD8 -> the Object's destruction (34FBF0 flags) for the pieces released since `from` in avalancheReleased; a released piece
// without an entity is not destroyed (nothing to hide), so it leaves the list.
static void avalanche_entities_released(size_t from){
 size_t kept=from;
 for(size_t k=from;k<avalancheReleased.size();k++){
  const uint32_t r=avalancheReleased[k];auto it=std::find_if(avalancheEntities.begin(),avalancheEntities.end(),[r](const AvalanchePiece& p){return p.resource==r;});
  if(it==avalancheEntities.end())continue;
  avalancheReleased[kept++]=r;
  if(it->collidable){const uint32_t masked=stage_flags(r)&0xFFFF0300u;stage_set_flags(r,masked|uint32_t(int32_t(masked)>>16)|2u);}
  avalanche_entity_restore(*it);avalancheEntities.erase(it);
 }
 avalancheReleased.resize(kept);
}
static void avalanche_entities_clear(bool restore){if(restore)for(const auto& p:avalancheEntities)avalanche_entity_restore(p);avalancheEntities.clear();}
// A streamed location leaving the world (web/stage_world.inc browser_stage_track_reset): its pieces lose their entities.
void browser_avalanche_track_reset(uint32_t track){
 std::erase_if(avalancheEntities,[track](const AvalanchePiece& p){if((p.resource&255u)!=track)return false;avalanche_entity_restore(p);return true;});
}
// ---- The computer riders' definitions ------------------------------------------------------------------------------------------
// The PS2 has one world: an avalanche any rider sets off plays, and its rocks collide, for everyone. The computer riders are rider
// contexts of the human's core (web/rider_context.cpp) with their own copy of the world; their stage programs run builtins 94 / 0 /
// 95 too (their own contacts, and the others' replayed: web/shared_world.inc event 6), but only the human's host loads
// avalanches.json (web/avalanche-load.js). So the definitions a core loads (init_avalanches / avalanche_clear) are kept here, with a
// generation, and a context that never loaded its own takes the latest before a trigger, an Object or a new race. A context
// that loaded its own keeps them whatever its location name does (a streamed world renames it with every location it appends:
// core57 keyed the copies by that name and a streamed human lost its definitions at the next race start).
static std::vector<ssx::OriginalAvalancheDef> avalancheSharedDefs;static uint32_t avalancheSharedGeneration=0;
RIDER_LOCAL static uint32_t avalancheDefsGeneration=0;RIDER_LOCAL static bool avalancheOwnDefs=false;
static void avalanche_share(){avalancheSharedDefs=avalancheWorld.defs;++avalancheSharedGeneration;avalancheOwnDefs=true;avalancheDefsGeneration=avalancheSharedGeneration;}
static void avalanche_sync(){
 if(avalancheOwnDefs||avalancheDefsGeneration==avalancheSharedGeneration)return;
 avalanche_world_reset(true,true);avalancheReleased.clear();avalanche_entities_clear(false);
 avalancheWorld.defs=avalancheSharedDefs;avalancheDefsGeneration=avalancheSharedGeneration;
}
// The trigger's trails colour base: the human's Weather painter (environment block 0 +0x20: breathEnvironment, web/animation_bridge.cpp)
// flake R, G, B (properties 12..14 through 0x2EE7C8 / 0x2EE810 / 0x2EE858); the painter defaults (1, 1, 1) before the snow data.
static ssx::AvaQuad avalanche_flake(){
 if(!snowReady)return {1.f,1.f,1.f,0.f};const auto& c=breathEnvironment.properties.current;return {c[12],c[13],c[14],0.f};
}
// A new race: no slot, every tumbler free, no piece entity (the definitions stay with the world; web/stage_world.inc puts the
// countdown flags back first).
static void avalanche_reset(){avalanche_sync();avalanche_world_reset(false,false);avalancheReleased.clear();avalanche_entities_clear(true);}
static void avalanche_trigger(int32_t id){
 avalanche_sync();
 if(!ssx::originalAvalancheTrigger(avalancheWorld,id,&stage_visual_random(),avalanche_flake()))return;++avalancheTriggers;
 for(int32_t k=0;k<16;k++)if(avalancheWorld.slots[size_t(k)].avalanche>=0&&avalancheWorld.slots[size_t(k)].t==0.f)
  ssx::originalAvalancheSetStart(avalancheWorld,k,[](uint32_t resource){ // instance +0x40: the instance matrix's translation row
   if(const auto* w=set_piece_instance_at(resource))return ssx::AvaQuad{w->authoredMatrix[12],w->authoredMatrix[13],w->authoredMatrix[14],w->authoredMatrix[15]};
   return ssx::AvaQuad{0,0,0,1};});
}
// race_begin (web/roller_gameplay.inc advance_world_entities), before the riders: the pieces' AvaSpline and bounds from the
// tumblers as they are, then the slots' step. The PS2 records show that order: at the provider exit of a tick the AvaSpline
// +0x40 holds 2D9C00 of the tumblers of the previous record (avalanche/eba3-rock-hit watches 0x5B4700 against the slot table).
void browser_avalanche_tick(){
 avalanche_entities_tick();
 bool any=false;for(const auto& s:avalancheWorld.slots)any|=s.avalanche>=0;
 if(any){
  const size_t released=avalancheReleased.size();
  for(auto& slot:avalancheWorld.slots)ssx::originalAvalancheSlotTick(avalancheWorld,slot,avalancheReleased,&stage_visual_random()); // the trails' births draw the presentation stream
  ++avalancheTicks;avalanche_entities_released(released);
 }
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
   if(x.hasEmitter){const auto& e=g.at("emitter");if(e.size()!=54)throw std::runtime_error("Avalanche emitter block");for(unsigned k=0;k<54;k++)x.emitterWords[k]=e[k].get<uint32_t>();}
   d.groups.push_back(std::move(x));
  }
  for(const auto& s:a.value("sounds",nlohmann::json::array()))d.sounds.push_back({uint16_t(s.at("tick").get<int>()),uint16_t(s.at("tumbler").get<int>())});
  defs.push_back(std::move(d));
 }
 if(!append){avalanche_world_reset(true,true);avalanche_entities_clear(false);} // a new world: its instances are new
 avalancheWorld.defs.insert(avalancheWorld.defs.begin(),std::make_move_iterator(defs.begin()),std::make_move_iterator(defs.end()));
 avalanche_share();
 return int(defs.size());
}
EMSCRIPTEN_KEEPALIVE void avalanche_clear(){avalanche_world_reset(true,true);avalancheReleased.clear();avalanche_entities_clear(true);avalanche_share();}
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
// The trails (web/, the rendering-sweep agent): what 0x2D9130 -> 0x2D8EA8(slot) -> 0x371688(emitter, 7) draws. Per active slot in
// list order, per tumbler whose group has an emitter: [resource, pool index, the emitter image (130 words, +0x000..+0x207: the
// dynamic emitter of engine/set_piece_particles.hpp, vtable 0x4930D0; the ring pointers +0x1A0 / +0x1A4 / +0x204 read 0), n (the
// ring slots, capacity +0x178, 0 when unallocated), ring A (n x 4 words: position, w 0), ring B (n x 4: velocity, birth seed; 0 =
// no birth), the colours (n words: R, G, B, A bytes from the low byte)]. 0x371688 draws only with +0x174 (enabled) set and +0x1E0
// (live births) > 0; the ring is newest-first from +0x17C (the cursor, decremented after each birth).
EMSCRIPTEN_KEEPALIVE uint32_t* avalanche_trails(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,0u);
 for(const auto& slot:avalancheWorld.slots){
  if(slot.avalanche<0)continue;
  for(int32_t k=slot.list;k>=0;k=avalancheWorld.pool[size_t(k)].next){
   const auto& tu=avalancheWorld.pool[size_t(k)];if(tu.group<0)continue;
   const auto& g=avalancheWorld.defs[size_t(tu.avalanche)].groups[size_t(tu.group)];if(!g.hasEmitter)continue;
   const auto& em=avalancheWorld.emitters[size_t(k)];++out[0];out.push_back(g.resource);out.push_back(uint32_t(k));
   for(unsigned o=0;o<0x200;o+=4)out.push_back(o==0x1A0||o==0x1A4?0u:em.base.u(o));out.push_back(em.word200);out.push_back(0u);
   const size_t n=std::min({em.base.ringPosition.size(),em.base.ringVelocity.size(),em.colours.size()});out.push_back(uint32_t(n));
   for(size_t i=0;i<n;i++)for(float v:em.base.ringPosition[i])out.push_back(std::bit_cast<uint32_t>(v));
   for(size_t i=0;i<n;i++)for(float v:em.base.ringVelocity[i])out.push_back(std::bit_cast<uint32_t>(v));
   for(size_t i=0;i<n;i++){const auto& c=em.colours[i];out.push_back(uint32_t(c[0])|uint32_t(c[1])<<8|uint32_t(c[2])<<16|uint32_t(c[3])<<24);}
  }
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
// 2 a type-2 piece: stays at its last pose), AvaSpline follower (attached: builtin 95 on the builtin-0 entity), emitter, the
// 0x2D9C00 matrix (16, source rows: rotation x scale, translation), alpha, scale]; then the instances released since the last
// call whose entity the release destroyed (entity vt+0x08(3): hidden; a piece without an entity stays) [n, resources...];
// then the loop: [refcount (0x29DEF0, audio+0x6040), n changes since the last call, the refcount after each].
// The AvaSpline followers' draw deltas go through moving_instances() (browser_avalanche_moving).
EMSCRIPTEN_KEEPALIVE float* avalanche_pieces(){
 RIDER_LOCAL static std::vector<float> out;out.assign(1,0.f);
 for(const auto& slot:avalancheWorld.slots){
  if(slot.avalanche<0)continue;
  for(int32_t k=slot.list;k>=0;k=avalancheWorld.pool[size_t(k)].next){
   const auto& tu=avalancheWorld.pool[size_t(k)];if(tu.group<0)continue;
   const auto& g=avalancheWorld.defs[size_t(tu.avalanche)].groups[size_t(tu.group)];ssx::AvaMatrix m;ssx::originalAvalancheInstanceMatrix(avalancheWorld,g.resource,m);
   const auto* e=avalanche_entity(g.resource); // follower: its AvaSpline is attached (builtin 95 found the builtin-0 entity)
   ++out[0];out.insert(out.end(),{float(g.resource),g.type==2?2.f:1.f,e&&e->spline?1.f:0.f,g.hasEmitter?1.f:0.f});
   for(const auto& row:m)out.insert(out.end(),row.begin(),row.end());out.push_back(tu.alpha);out.push_back(tu.scale);
  }
 }
 out.push_back(float(avalancheReleased.size()));for(uint32_t r:avalancheReleased)out.push_back(float(r));avalancheReleased.clear();
 out.push_back(float(avalancheWorld.loopRefs));out.push_back(float(avalancheWorld.loopTransitions.size()));for(int32_t v:avalancheWorld.loopTransitions)out.push_back(float(v));avalancheWorld.loopTransitions.clear();
 return out.data();
}
}
// moving_instances() (web/roller_gameplay.inc): every AvaSpline follower a tumbler drives, key = its resource, the draw delta of its
// AvaSpline matrix +0x40 (the Object's draw matrix, vt+0xC4: 2D9C00 of this tick's entity group 1, or the instance matrix until
// then) against the authored instance matrix (the pieces' moving batches, baked with that matrix).
void browser_avalanche_moving(std::vector<float>& out,void(*append)(std::vector<float>&,float,const collision_transform::Matrix&,const collision_transform::Matrix&)){
 for(const auto& slot:avalancheWorld.slots){
  if(slot.avalanche<0)continue;
  for(int32_t k=slot.list;k>=0;k=avalancheWorld.pool[size_t(k)].next){
   const auto& tu=avalancheWorld.pool[size_t(k)];if(tu.group<0)continue;
   const auto& g=avalancheWorld.defs[size_t(tu.avalanche)].groups[size_t(tu.group)];
   const auto* e=avalanche_entity(g.resource);if(!e||!e->spline)continue; // 305D90 attaches only to an entity
   const auto* w=set_piece_instance_at(g.resource);if(!w)continue;
   const auto& m=e->matrix;
   collision_transform::Matrix mm;for(unsigned i=0;i<4;i++)for(unsigned c=0;c<4;c++)mm[i*4+c]=m[i][c];
   append(out,float(g.resource),mm,w->authoredMatrix);
  }
 }
}
#ifdef SSX_SNAPSHOT_REGISTRY
#include "world_snapshot.hpp"
// The rider-context snapshot's hook (web/world_snapshot.hpp; docs/replay.md §2a): the PS2's own avalanche save and restore
// (engine/avalanche.hpp): 0x2D9CB0 keeps per slot the playing avalanche's id and t; 0x2D9D68 (the one restore 0x26DBF0: the
// replay's restart, its seeks and the Transport) releases every playing slot (its hidden instances returned: avalancheReleased)
// and triggers each saved one again, brought to its t without the group speed factor, so a playing avalanche comes back as the
// PS2 brings it back, not as it was saved. Code-ported, not capture-verified (docs/replay.md §2a).
namespace {
struct AvalancheSaved{bool saved=false;std::array<uint32_t,32> words{};};
RIDER_LOCAL_LAZY std::array<AvalancheSaved,2> avalancheSnapshot;
void avalanche_snapshot_save(unsigned slot){auto& s=avalancheSnapshot[slot];s.saved=true;s.words=ssx::originalAvalancheSave(avalancheWorld);}
bool avalanche_snapshot_check(unsigned slot){return avalancheSnapshot[slot].saved;}
bool avalanche_snapshot_restore(unsigned slot){const auto& s=avalancheSnapshot[slot];if(!s.saved)return false;avalanche_sync();
 auto hidden=ssx::originalAvalancheRestore(avalancheWorld,s.words,[](uint32_t resource){ // instance +0x40, as the trigger (avalanche_trigger)
  if(const auto* w=set_piece_instance_at(resource))return ssx::AvaQuad{w->authoredMatrix[12],w->authoredMatrix[13],w->authoredMatrix[14],w->authoredMatrix[15]};
  return ssx::AvaQuad{0,0,0,1};},&stage_visual_random(),avalanche_flake());
 avalancheReleased.insert(avalancheReleased.end(),hidden.begin(),hidden.end());return true;}
uint64_t avalanche_snapshot_hash(){const auto w=ssx::originalAvalancheSave(avalancheWorld);return ssx_snapshot::fnv(w.data(),sizeof w,1469598103934665603ull);}
size_t avalanche_snapshot_bytes(){return sizeof avalancheSnapshot;}
struct AvalancheSnapshotHook{AvalancheSnapshotHook(){ssx_snapshot::hooks().push_back({"avalancheWorld (0x2D9CB0 / 0x2D9D68)",&avalanche_snapshot_save,&avalanche_snapshot_restore,&avalanche_snapshot_hash,&avalanche_snapshot_bytes,&avalanche_snapshot_check});}};
[[maybe_unused]] AvalancheSnapshotHook avalancheSnapshotHook;
}
#endif
