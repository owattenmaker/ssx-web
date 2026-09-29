// Collision side of the Snow Jam falling billboard (web/falling_billboard.inc runs its LiveComp in
// web/rail_bridge.cpp). #include this in web/set_piece_gameplay.inc right after set_piece_instance_at()
// (it uses set_piece_instance_at and the rail_bridge.hpp declarations).
// Once program 144 has built the LiveComp, 0x334888 composes fallingbb_1000 from the entity's node
// matrices (vt+0xD4 0x361090 = 1 -> vt+0xE4 0x3560C0 -> 0x361098) while the broad phase keeps the static
// route and the authored bounds (instance flags 0x210023 -> 0x210125: 0x34D9B0 (&~2)|4, 1032C0 0x100);
// the rigid predicate 0x355420 answers 1 (no primary modifier) and the selected-contact callback 0x34E698
// adds the node's surface velocity at the contact point to packet +0x20.
#include "generated/falling_billboard_seed.hpp"
RIDER_LOCAL static std::vector<std::pair<uint32_t,std::vector<std::pair<collision_transform::Matrix,collision_transform::Matrix>>>> railLiveCompRest; // resource, static node world/inverse
RIDER_LOCAL static std::vector<std::pair<uint32_t,uint32_t>> railLiveCompFlags; // resource, countdown runtime flags
// Entity pass, after browser_advance_teeters() (which ticks the LiveComp 0x341D48).
static void refresh_rail_livecomp_collision(){
 for(const auto& s:browserRailLiveCompSeeds){
  std::vector<ssx::RollerMatrix> nodes;if(!browser_rail_livecomp_nodes(s.resource,nodes))continue;
  auto* instance=set_piece_instance_at(s.resource);if(!instance)continue;
  if(std::none_of(railLiveCompRest.begin(),railLiveCompRest.end(),[&](const auto& r){return r.first==s.resource;})){
   std::vector<std::pair<collision_transform::Matrix,collision_transform::Matrix>> rest;for(const auto& n:instance->nodes)rest.push_back({n.world,n.inverse});
   railLiveCompRest.push_back({s.resource,std::move(rest)});railLiveCompFlags.push_back({s.resource,instance->eventRuntimeFlags.value_or(s.flags)});
   instance->eventRuntimeFlags=(instance->eventRuntimeFlags.value_or(s.flags)&~2u)|4u|0x100u;
  }
  for(auto& n:instance->nodes){collision_transform::Matrix w;const auto& m=nodes.at(n.index);for(unsigned i=0;i<16;i++)w[i]=m[i/4][i%4];n.world=w;n.inverse=collision_transform::inverseRigid(w);}
  instance->entityPointer=true;
 }
}
// New race (browser_attach_set_pieces): the static nodes, flags and no entity again.
static void reset_rail_livecomp_collision(){
 for(auto& [resource,rest]:railLiveCompRest)if(auto* instance=set_piece_instance_at(resource)){
  for(size_t k=0;k<instance->nodes.size()&&k<rest.size();k++){instance->nodes[k].world=rest[k].first;instance->nodes[k].inverse=rest[k].second;}
  instance->entityPointer=false;
  for(auto& [r,f]:railLiveCompFlags)if(r==resource)instance->eventRuntimeFlags=f;
 }
 railLiveCompRest.clear();railLiveCompFlags.clear();
}
// 1057B8 rigid predicate (entity+0x74 0x355420): 1 without a primary modifier.
static bool rail_livecomp_rigid(uint32_t resource){return browser_rail_livecomp_entity(resource);}
// 104E70 selected-entity callback (vt+0x154 0x34E698): node velocity at the contact point into packet +0x20.
static bool rail_livecomp_selected(OriginalInstanceContactPacket& packet){
 if(!browser_rail_livecomp_entity(packet.instance))return false;
 ssx::RollerQuad angular{0,0,0,0}; // packet +0x30 is not carried by OriginalInstanceContactPacket
 browser_rail_livecomp_contact_velocity(packet.instance,int32_t(packet.node),packet.point,packet.surfaceVelocity,angular);
 return true;
}
