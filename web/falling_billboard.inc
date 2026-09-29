// Snow Jam falling billboard (tools/export_falling_billboard.py -> generated/falling_billboard_seed.hpp):
// the bcvolume_1001 contact (slot 2, program 144) starts a once-mode LiveComp on fallingbb_1000 and binds
// the top-edge rails 0x5B08 (fallingbbl_1000) and 0x5A08 (bcrail_1005) to its node 0 (RailModifier 0x35B708:
// the static segments leave the query and 0x334680 answers them through 0x35C698 with T = rest^-1 * node 0).
// The drawn billboard is the LIVECOMP/livecomp.json player (web/set-pieces-renderer.js, same contact);
// the collision mesh keeps its static route (flags 0x210125). #include this at the END of web/rail_bridge.cpp
// (after web/rail_snap_teeter.inc: it uses `records`, `staticRecords`, `teeters`) and declare in rail_bridge.hpp:
//   void browser_rail_livecomps_reset();            // end of bind_teeters() and in the non-ARA1 branches
//   bool browser_rail_livecomp_fire(uint32_t owner); // web/set_piece_gameplay.inc browser_set_piece_contact()
//   void browser_rail_livecomps_advance();           // browser_advance_teeters() (entity pass, 0x341D48)
//   bool browser_rail_livecomp_snap_entity(const ssx::OriginalRailQueryResult&);
//   void browser_rail_livecomp_snap_velocity(const ssx::OriginalRailQueryResult&,ssx::RollerQuad&,ssx::RollerQuad&);
//   std::vector<float> browser_rail_livecomp_info(); // [count, fires, then per piece resource, active, time, done]
//   ssx::OriginalRailSnapEntityHooks browser_rail_snap_hooks(); // 0x106F78 hooks: teeters + billboard
//   bool browser_rail_livecomp_nodes(uint32_t resource,std::vector<ssx::RollerMatrix>&); // collision nodes (0x334888)
//   bool browser_rail_livecomp_entity(uint32_t resource);    // rigid predicate 0x355420 -> 1
//   bool browser_rail_livecomp_contact_velocity(uint32_t resource,int32_t node,const ssx::RollerQuad& point,ssx::RollerQuad&,ssx::RollerQuad&);
// and in teeterQuery(), after the teeter loop and before `if(modifierHit)`:
//   browser_rail_livecomps_query(point,mask,found,best,hit,modifierHit);
#include "../engine/rail_snap_torque.hpp"
#include "generated/falling_billboard_seed.hpp"
#include <memory>
namespace {
struct BrowserRailLiveComp {
 const BrowserRailLiveCompSeed* seed=nullptr;ssx::OriginalAnimModel model;ssx::RollerMatrix matrix{};float scale=1;bool active=false;
 ssx::OriginalLiveComp live;std::vector<std::pair<ssx::OriginalRailModifier,const ssx::OriginalRailRecord*>> rails;ssx::OriginalAnimNodeVelocityTable velocities;
};
RIDER_LOCAL std::vector<std::unique_ptr<BrowserRailLiveComp>> railLiveComps; // stable addresses (the LiveComp keeps a model pointer)
RIDER_LOCAL unsigned railLiveCompFires=0;
float railLiveCompBits(uint32_t b){return std::bit_cast<float>(b);}
ssx::RollerMatrix railLiveCompRows(const std::array<uint32_t,16>& w){ssx::RollerMatrix r;for(unsigned i=0;i<16;i++)r[i/4][i%4]=railLiveCompBits(w[i]);return r;}
}
void browser_rail_livecomps_reset(){
 railLiveComps.clear();railLiveCompFires=0;
 if(!teeterCourse)return; /* ARA1 only (the seed is Snow Jam's) */
 for(const auto& s:browserRailLiveCompSeeds){
  auto x=std::make_unique<BrowserRailLiveComp>();x->seed=&s;x->model.length=railLiveCompBits(s.length);
  for(uint32_t i=0;i<s.nodeCount;i++){
   const auto& n=browserRailLiveCompNodes[s.firstNode+i];ssx::OriginalAnimNode node;node.parent=n.parent;node.bind=railLiveCompRows(n.bind);
   if(n.animated){ssx::OriginalAnimTrack t;for(unsigned k=0;k<6;k++)t.base[k]=railLiveCompBits(n.base[k]);t.mask=n.mask;
    for(uint32_t c=0;c<n.curveCount;c++){const auto& cv=browserRailLiveCompCurves[n.firstCurve+c];std::vector<ssx::OriginalAnimSegment> segs;
     for(uint32_t k=0;k<cv.count;k++){const auto& g=browserRailLiveCompSegments[cv.first+k];segs.push_back({railLiveCompBits(g.a),railLiveCompBits(g.b),railLiveCompBits(g.c),railLiveCompBits(g.d),railLiveCompBits(g.t0),railLiveCompBits(g.t1)});}
     t.curves.push_back(std::move(segs));}
    node.track=std::move(t);}
   x->model.nodes.push_back(std::move(node));
  }
  x->matrix=railLiveCompRows(s.matrix);x->scale=railLiveCompBits(s.scale);
  railLiveComps.push_back(std::move(x));
 }
}
// Program 144 at the trigger's selected contact: builtin3 (0x341AA0) then builtin48 x2 (0x35B708, which
// removes the static segments: here the staticRecords copies stop answering through a zero query mask).
bool browser_rail_livecomp_fire(uint32_t owner){
 bool fired=false;
 for(auto& x:railLiveComps){
  if(x->active||x->seed->owner!=owner)continue;
  ssx::OriginalLiveCompArguments args;for(unsigned k=0;k<11;k++)args.words[k]=x->seed->words[k];
  x->live=ssx::originalLiveCompConstruct(x->model,x->matrix,x->scale,x->seed->flags,args,[]{return 0u;}); /* key6/key8 0: no RNG draw */
  const std::array<float,3> low{railLiveCompBits(x->seed->boundsMin[0]),railLiveCompBits(x->seed->boundsMin[1]),railLiveCompBits(x->seed->boundsMin[2])};
  const std::array<float,3> high{railLiveCompBits(x->seed->boundsMax[0]),railLiveCompBits(x->seed->boundsMax[1]),railLiveCompBits(x->seed->boundsMax[2])};
  x->rails.clear();
  for(uint32_t r=0;r<x->seed->railCount;r++){
   const auto& rail=browserRailLiveCompRails[x->seed->firstRail+r];const ssx::OriginalRailRecord* record=nullptr;
   for(const auto& e:records)if(e.packedId==rail.packedId)record=&e;
   if(!record)continue;
   x->rails.push_back({ssx::originalRailModifierConstruct(rail.packedId,rail.node,x->seed->resource,x->model,x->matrix,x->scale,low,high),record});
   for(auto& e:staticRecords)if(e.packedId==rail.packedId)e.flags=0; /* 0x3284B8: out of the static layers (pointers stay valid) */
  }
  x->active=true;fired=true;++railLiveCompFires;
 }
 return fired;
}
// Entity pass (before the rider): 0x341D48 (the once player; done after 80 ticks keeps the end pose).
void browser_rail_livecomps_advance(){for(auto& x:railLiveComps)if(x->active)ssx::originalLiveCompTick(x->live);}
// 0x334680 modifier layers of the bound rails (after the static layers, strictly closer replaces).
void browser_rail_livecomps_query(ssx::RailVector point,uint32_t mask,bool& found,float& best,ssx::OriginalRailModifierHit& hit,bool& modifierHit){
 const ssx::RailVector extent{300,300,300};
 for(auto& x:railLiveComps){
  if(!x->active)continue;
  const auto& nodes=ssx::originalLiveCompMatrices(x->live);
  for(auto& [m,record]:x->rails){
   bool overlaps=true;for(unsigned k=0;k<3;k++)overlaps&=m.boundsMin[k]<point[k]+extent[k]&&point[k]-extent[k]<m.boundsMax[k];
   if(!overlaps)continue;
   bool before=found;float previous=best;
   ssx::originalRailModifierQuery(m,*record,ssx::originalRailModifierTransform(m,nodes.at(size_t(m.node))),{point[0],point[1],point[2],1.f},mask,found,best,hit);
   if(found&&(!before||best!=previous))modifierHit=true;
  }
 }
}
namespace {
BrowserRailLiveComp* rail_livecomp_of(const ssx::OriginalRailQueryResult& hit,const ssx::OriginalRailModifier** modifier=nullptr){
 if(!hit.record)return nullptr;
 for(auto& x:railLiveComps)if(x->active)for(auto& [m,record]:x->rails)if(record==hit.record){if(modifier)*modifier=&m;return x.get();}
 return nullptr;
}
}
// 0x106F78 entity hooks (engine/rail_snap_torque.hpp): LiveComp vt+0x154 0x34E698 (rate vt+0x1A8 0x361158);
// vt+0x15C 0x356AB8 forwards only to a primary modifier (none): no force.
bool browser_rail_livecomp_snap_entity(const ssx::OriginalRailQueryResult& hit){return rail_livecomp_of(hit)!=nullptr;}
void browser_rail_livecomp_snap_velocity(const ssx::OriginalRailQueryResult& hit,ssx::RollerQuad& packet20,ssx::RollerQuad& packet30){
 const ssx::OriginalRailModifier* m=nullptr;auto* x=rail_livecomp_of(hit,&m);if(!x)return;
 ssx::originalAnimContactVelocity(x->live.anim,ssx::originalLiveCompRate(x->live),x->velocities,m->node,{hit.point[0],hit.point[1],hit.point[2],1.f},packet20,packet30);
}
std::vector<float> browser_rail_livecomp_info(){
 std::vector<float> out{float(railLiveComps.size()),float(railLiveCompFires)};
 for(auto& x:railLiveComps){out.push_back(float(x->seed->resource));out.push_back(x->active);out.push_back(x->live.anim.time);out.push_back(float(x->live.done));}
 return out;
}
// Combined 0x106F78 entity hooks (log teeters of web/rail_snap_teeter.inc + the billboard LiveComp) for
// web/instance_contact_gameplay.inc: auto result=originalRailSnapContact(rb,bh,browser_rail_snap_hooks());
ssx::OriginalRailSnapEntityHooks browser_rail_snap_hooks(){
 ssx::OriginalRailSnapEntityHooks h;
 h.hasEntity=[](const ssx::OriginalRailQueryResult& hit){return browser_teeter_snap_entity(hit)||browser_rail_livecomp_snap_entity(hit)||browser_dynamic_rail_snap_entity(hit);};
 h.contactVelocity=[](const ssx::OriginalRailQueryResult& hit,ssx::RollerQuad& a,ssx::RollerQuad& b){
  if(browser_teeter_snap_entity(hit))browser_teeter_snap_velocity(hit,a,b);else if(browser_dynamic_rail_snap_entity(hit))browser_dynamic_rail_snap_velocity(hit,a,b);else browser_rail_livecomp_snap_velocity(hit,a,b);}; // R&B run-time rails (web/rail_dynamic.inc)
 h.applyForce=[](const ssx::OriginalRailQueryResult& hit,const ssx::RollerQuad& force){browser_teeter_snap_force(hit,force);}; // LiveComp 0x356AB8: no-op
 return h;
}
// Collision: 0x334888 composes an instance with an entity whose vt+0xD4 is non-zero (LiveComp 0x361090) from
// the entity's node matrices (vt+0xE4 0x3560C0 -> vt+0xDC 0x361098), on either broad-phase route, so the
// billboard's collision mesh tips with node 0 (PS2 fall run: first contact at 1938 with a 24-degree tilted
// normal; the upright static mesh would be hit at 1922). The world-collision instance lives in
// web/world_bridge.cpp: web/set_piece_gameplay.inc copies these matrices into its nodes each entity pass.
bool browser_rail_livecomp_nodes(uint32_t resource,std::vector<ssx::RollerMatrix>& out){
 for(auto& x:railLiveComps)if(x->active&&x->seed->resource==resource){out=ssx::originalLiveCompMatrices(x->live);return true;}
 return false;
}
bool browser_rail_livecomp_entity(uint32_t resource){for(auto& x:railLiveComps)if(x->active&&x->seed->resource==resource)return true;return false;}
// 104E70 selected-entity callback on the billboard (LiveComp vt+0x154 0x34E698; 0x356AE0 has no primary modifier).
bool browser_rail_livecomp_contact_velocity(uint32_t resource,int32_t node,const ssx::RollerQuad& point,ssx::RollerQuad& packet20,ssx::RollerQuad& packet30){
 for(auto& x:railLiveComps)if(x->active&&x->seed->resource==resource)
  return ssx::originalAnimContactVelocity(x->live.anim,ssx::originalLiveCompRate(x->live),x->velocities,node,point,packet20,packet30);
 return false;
}
