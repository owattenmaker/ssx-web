// Log-teeter side of the hips/rail snap 0x106F78 (engine/rail_snap_torque.hpp). #include this at the
// END of web/rail_bridge.cpp (it uses that file's anonymous-namespace `teeters` / `teeterForces`), and
// declare the three functions in web/rail_bridge.hpp:
//   bool browser_teeter_snap_entity(const ssx::OriginalRailQueryResult&);
//   void browser_teeter_snap_velocity(const ssx::OriginalRailQueryResult&,ssx::RollerQuad&,ssx::RollerQuad&);
//   void browser_teeter_snap_force(const ssx::OriginalRailQueryResult&,const ssx::RollerQuad&);
// A 0x334680 result on a bound teeter rail is a RailModifier (type-2 layer) hit: out+0x50 = the log
// instance (it has the AnimTeeter entity), out+0x5C = node 1. 0x106F78 then calls the entity's
// vtable+0x154 (0x34E698: the node's surface velocity at the rail point -> out+0x20/+0x30, used by the
// impulse) and vtable+0x15C (0x342538 with F = (v_before - v_after) * 60).
#include "../engine/rail_snap_torque.hpp"
#include <map>
namespace {
RIDER_LOCAL_LAZY std::map<const ssx::OriginalAnimTeeter*,ssx::OriginalAnimNodeVelocityTable> teeterVelocityTables; // entity+0x4C per log
struct TeeterRail {BrowserTeeter* teeter=nullptr;const ssx::OriginalRailModifier* modifier=nullptr;};
TeeterRail teeter_rail_of(const ssx::OriginalRailQueryResult& hit){
 if(!hit.record)return {};
 for(auto& t:teeters)for(auto& [m,record]:t.rails)if(record==hit.record)return {&t,&m};
 return {};
}
}
bool browser_teeter_snap_entity(const ssx::OriginalRailQueryResult& hit){return teeter_rail_of(hit).teeter!=nullptr;}
void browser_teeter_snap_velocity(const ssx::OriginalRailQueryResult& hit,ssx::RollerQuad& packet20,ssx::RollerQuad& packet30){
 auto r=teeter_rail_of(hit);if(!r.teeter)return;
 if(teeterVelocityTables.size()>2*teeters.size()+8)teeterVelocityTables.clear(); /* entities rebuilt by bind_teeters */
 auto& table=teeterVelocityTables[&r.teeter->entity];
 ssx::originalAnimTeeterContactVelocity(r.teeter->entity,table,r.modifier->node,{hit.point[0],hit.point[1],hit.point[2],1.f},packet20,packet30);
}
void browser_teeter_snap_force(const ssx::OriginalRailQueryResult& hit,const ssx::RollerQuad& force){
 auto r=teeter_rail_of(hit);if(!r.teeter)return;
 if(ssx::originalAnimTeeterApplyForce(r.teeter->entity,r.modifier->node,{hit.point[0],hit.point[1],hit.point[2],1.f},force))++teeterForces;
}
