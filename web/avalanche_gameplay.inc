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
//     by the draw: avalanche_pieces). Their collision (type-3 rocks with flags 0x40210000, entity route 0x40 after the
//     trigger's builtin 0) is not modelled yet: the port keeps those instances unsupported, as before.
#include "../engine/avalanche.hpp"
RIDER_LOCAL static ssx::OriginalAvalancheWorld avalancheWorld;
RIDER_LOCAL static std::vector<uint32_t> avalancheReleased;   // instances released (entity vt+0x08(3)) since the last drain
RIDER_LOCAL static uint32_t avalancheTriggers=0,avalancheTicks=0;
// A new race: no slot, every tumbler free (the definitions stay with the world).
static void avalanche_reset(){auto defs=std::move(avalancheWorld.defs);avalancheWorld=ssx::OriginalAvalancheWorld{};avalancheWorld.defs=std::move(defs);avalancheReleased.clear();}
static void avalanche_trigger(int32_t id){
 if(!ssx::originalAvalancheTrigger(avalancheWorld,id))return;++avalancheTriggers;
 for(int32_t k=0;k<16;k++)if(avalancheWorld.slots[size_t(k)].avalanche>=0&&avalancheWorld.slots[size_t(k)].t==0.f)
  ssx::originalAvalancheSetStart(avalancheWorld,k,[](uint32_t resource){ // instance +0x40: the instance matrix's translation row
   if(const auto* w=set_piece_instance_at(resource))return ssx::AvaQuad{w->authoredMatrix[12],w->authoredMatrix[13],w->authoredMatrix[14],w->authoredMatrix[15]};
   return ssx::AvaQuad{0,0,0,1};});
}
void browser_avalanche_tick(){
 bool any=false;for(const auto& s:avalancheWorld.slots)any|=s.avalanche>=0;if(!any)return;
 for(auto& slot:avalancheWorld.slots)ssx::originalAvalancheSlotTick(avalancheWorld,slot,avalancheReleased);
 ++avalancheTicks;
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
 if(!append)avalancheWorld=ssx::OriginalAvalancheWorld{};
 avalancheWorld.defs.insert(avalancheWorld.defs.begin(),std::make_move_iterator(defs.begin()),std::make_move_iterator(defs.end()));
 return int(defs.size());
}
EMSCRIPTEN_KEEPALIVE void avalanche_clear(){avalancheWorld=ssx::OriginalAvalancheWorld{};avalancheReleased.clear();}
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
