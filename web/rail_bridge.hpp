#pragma once
#include "../engine/rail_motion.hpp"
#include "../engine/rail_modifier.hpp"
ssx::OriginalRailQueryResult browserRailQuery(ssx::RailVector point);
ssx::OriginalRailQueryResult browserHandplantQuery(ssx::RailVector point);
// Snow Jam log teeters (AnimTeeter + RailModifier, rail_bridge.cpp).
void browser_advance_teeters();void browser_reset_teeters();
void browser_teeter_attach_force(const ssx::OriginalRailQueryResult&,const ssx::RailVector&,const ssx::RailVector&);
void browser_teeter_motion_force(const ssx::OriginalRailQueryResult&,const ssx::RailVector&); // 0x13AF28 per-tick entity push
std::vector<std::pair<uint32_t,ssx::RollerMatrix>> browser_teeter_transforms();std::array<float,4> browser_teeter_info();
#include "../engine/rail_snap_torque.hpp"
// 0x106F78 entity hooks (web/rail_snap_teeter.inc) and the Snow Jam falling billboard (web/falling_billboard.inc).
bool browser_teeter_snap_entity(const ssx::OriginalRailQueryResult&);
void browser_teeter_snap_velocity(const ssx::OriginalRailQueryResult&,ssx::RollerQuad&,ssx::RollerQuad&);
void browser_teeter_snap_force(const ssx::OriginalRailQueryResult&,const ssx::RollerQuad&);
void browser_rail_livecomps_reset();bool browser_rail_livecomp_fire(uint32_t owner);void browser_rail_livecomps_advance();
void browser_rail_livecomps_query(ssx::RailVector point,uint32_t mask,bool& found,float& best,ssx::OriginalRailModifierHit& hit,bool& modifierHit);
bool browser_rail_livecomp_snap_entity(const ssx::OriginalRailQueryResult&);
void browser_rail_livecomp_snap_velocity(const ssx::OriginalRailQueryResult&,ssx::RollerQuad&,ssx::RollerQuad&);
std::vector<float> browser_rail_livecomp_info();ssx::OriginalRailSnapEntityHooks browser_rail_snap_hooks();
bool browser_rail_livecomp_nodes(uint32_t resource,std::vector<ssx::RollerMatrix>& out);bool browser_rail_livecomp_entity(uint32_t resource);
bool browser_rail_livecomp_contact_velocity(uint32_t resource,int32_t node,const ssx::RollerQuad& point,ssx::RollerQuad& packet20,ssx::RollerQuad& packet30);
// Run-time rails of the other locations (web/rail_dynamic.inc): builtin48 RailModifiers on Spline pieces / stage-world
// LiveComps, the resident extra location's log teeter (section driven), QA exports rail_dynamic_bits / rail_location_teeters.
bool browser_dynamic_rail_bind(uint32_t owner,uint32_t packedId,int32_t node,const ssx::OriginalAnimModel* model,const ssx::RollerMatrix& instanceMatrix,float scale,
  const std::array<float,3>& low,const std::array<float,3>& high);
void browser_dynamic_rails_unbind(uint32_t owner);void browser_rail_group(uint32_t id,bool on);void browser_dynamic_rails_tick();void browser_location_teeter_section(uint32_t resource,bool alive);
bool browser_dynamic_rail_snap_entity(const ssx::OriginalRailQueryResult&);
void browser_dynamic_rail_snap_velocity(const ssx::OriginalRailQueryResult&,ssx::RollerQuad&,ssx::RollerQuad&);
