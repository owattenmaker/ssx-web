// Scripted dynamic instances (tools/export_scripted_instances.py) for the browser:
// crashbags run authored contact program 49 = builtin0() builtin15(): the first rider
// contact (105398 flag-bit1 copy -> 121818 -> 30A060 slot 2) constructs an Object entity
// (356DB0) and a RollerModifier rigid body (35DA70). The instance then takes the entity
// route (bit 0x40): its collision follows the roller matrix (+0x240) and bounds (+0x220).
// Later contacts re-kick the roller (121818 -> 355770 -> 361CD8 -> 35DDE8). Every game
// tick, before the rider, each roller runs 35E850 and its entity 3568B0 (race_begin).
#include "../engine/roller_modifier.hpp"
#include "../engine/roller_world_query.hpp"
#include "generated/event_instance_seed.hpp"
#include "set_piece_gameplay.inc"
extern const BrowserEventScriptedInstance* scripted_instance(uint32_t resource);
void browser_stage_entities_tick(); // web/stage_script_gameplay.inc
void shared_world_log_contact(const OriginalInstanceContactPacket&,const OriginalInstanceContactRecord&); // web/shared_world.inc
struct BrowserRoller {
 uint32_t resource=0;WorldCollisionInstance* instance=nullptr;
 ssx::OriginalRollerModifier modifier;
};
RIDER_LOCAL static std::vector<BrowserRoller> browserRollers; // creation order
RIDER_LOCAL static unsigned rollerCreated=0,rollerKicks=0,rollerTicks=0;
static WorldCollisionInstance* browser_world_instance(uint32_t resource){
 if(!browserBodies)return nullptr;
 for(auto& i:browserBodies->instances)if(i.resource==resource)return &i;return nullptr;
}
static BrowserRoller* browser_roller(uint32_t resource){for(auto& r:browserRollers)if(r.resource==resource)return &r;return nullptr;}
// Entity route geometry from the roller (334458 bounds 3569D0/356A00 -> +0x220/+0x230,
// 334888 base matrix 356128 -> +0x240).
static collision_transform::Matrix roller_matrix(const ssx::OriginalRollerModifier& m){
 collision_transform::Matrix out;const auto& rows=ssx::originalRollerMatrix(m);for(unsigned i=0;i<4;i++)for(unsigned k=0;k<4;k++)out[i*4+k]=rows[i][k];return out;
}
static void refresh_roller_entity(BrowserRoller& roller){
 const auto& m=roller.modifier;
 roller.instance->entity=std::make_shared<const WorldCollisionEntity>(composeWorldCollisionEntity(*roller.instance,roller_matrix(m),
  {m.boundsMin[0],m.boundsMin[1],m.boundsMin[2]},{m.boundsMax[0],m.boundsMax[1],m.boundsMax[2]}));
}
static ssx::OriginalRollerContactRecord roller_record(const OriginalInstanceContactRecord& r){
 ssx::OriginalRollerContactRecord out;out.point=r.point;out.direction=r.direction;out.normal=r.normal;out.closingSpeed=r.closingSpeed;return out;
}
// New race: restore the countdown runtime state (static route, no entity).
void browser_reset_rollers(){
 for(auto& r:browserRollers)if(r.instance){r.instance->entity.reset();if(const auto* s=scripted_instance(r.resource))r.instance->eventRuntimeFlags=s->runtimeFlags;}
 browserRollers.clear();rollerCreated=rollerKicks=rollerTicks=0;
 browser_attach_set_pieces(); // chairlifts back to race tick 0
}
// race_begin: the original entity update precedes the rider's provider/controllers.
void browser_avalanche_tick();void browser_avalanche_moving(std::vector<float>&,void(*)(std::vector<float>&,float,const collision_transform::Matrix&,const collision_transform::Matrix&)); // web/avalanche_gameplay.inc
void advance_world_entities(){
 if(!browserBodies)return;
 advance_set_pieces();
 browser_stage_entities_tick(); // 356198 entity guards, Debounce 342D88 (web/stage_script_gameplay.inc)
 for(auto& r:browserRollers){
  ssx::originalRollerUpdate(r.modifier,[&](ssx::OriginalSphereTreeCollider& collider,ssx::OriginalRollerTerrainCache& cache){return ssx::originalRollerWorldQuery(*browserBodies,collider,cache);});
  ssx::originalRollerEntityBounds(r.modifier);refresh_roller_entity(r);++rollerTicks;
 }
 browser_avalanche_tick(); // 0x2D8948 (entity group 3, after the entity pass): web/avalanche_gameplay.inc
}
// 121818 for a selected scripted instance (rider+A30 = packet+0x50). Returns true when handled.
bool browser_scripted_contact(const OriginalInstanceContactPacket& packet,const OriginalInstanceContactRecord& record){
 const auto* scripted=scripted_instance(packet.instance);
 if(!scripted)return false;
 if(scripted->contact==0)return true; // no slot-2 handler: 30A060 runs nothing
 if(scripted->contact!=1)throw std::runtime_error("Unported scripted contact handler");
 shared_world_log_contact(packet,record); // the same crashbag in every rider core (web/shared_world.inc)
 auto* instance=browser_world_instance(packet.instance);
 if(!instance)throw std::runtime_error("Scripted instance missing from the collision world");
 if(auto* roller=browser_roller(packet.instance)){ // entity: 355770 -> modifier vtable+0x54 (361CD8)
  ssx::originalRollerRecontact(roller->modifier,roller_record(record));++rollerKicks;return true;
 }
 // builtin0 (356DB0 via 34FB00/2D1BF0 -> 1032C0): flags |= 0x100 (0x200 is renderer
 // bookkeeping, not modeled); default args: (flags&3)==3 -> flags = flags&~2|4.
 uint32_t flags=instance->eventRuntimeFlags.value_or(instance->flags)|0x100u;
 if((flags&3u)==3u)flags=(flags&~2u)|4u;
 // packet+0x48: the hit node's prebuilt 32C508 collider (3A6CE8 in 334888): table corners
 // (+-1, w 0), centre = tree centre (w 1), scale 1, +0x98 = tree. The crashbag models have
 // a single sphere-tree node (model 0 of their collision resource).
 if(instance->nodes.size()!=1||instance->nodes[0].index!=packet.node||!instance->nodes[0].sphereTree)throw std::runtime_error("Roller hit node has no sphere tree");
 const auto& node=instance->nodes[0];
 const BrowserRollerTreeMass* treeMass=nullptr;for(const auto& t:browserRollerTreeMass)if(t.resource==node.sphereTreeResource&&t.model==0)treeMass=&t;
 // the streamed peak worlds: the course event's roller masses (web/world_bridge.cpp scripted_instance)
 if(!treeMass&&!browserEventCourseSeeded){static constexpr std::array<std::span<const BrowserRollerTreeMass>,17> lists{{browser_event_ARA1::rollers,browser_event_BRA2::rollers,browser_event_BHP1::rollers,browser_event_ABC1::rollers,browser_event_ASS1::rollers,browser_event_ABA1::rollers,browser_event_CRA3::rollers,browser_event_DRA4::rollers,browser_event_DSS2::rollers,browser_event_CBA2::rollers,browser_event_CHP2::rollers,browser_event_DBC2::rollers,browser_event_ERA5::rollers,browser_event_ESS3::rollers,browser_event_EBA3::rollers,browser_event_EHP3::rollers,browser_event_EBC3::rollers}};for(const auto& list:lists)for(const auto& t:list)if(t.resource==node.sphereTreeResource&&t.model==0)treeMass=&t;}
 if(!treeMass)throw std::runtime_error("Roller tree mass properties unavailable");
 ssx::OriginalRollerTreeMass mass;
 for(unsigned k=0;k<3;k++)mass.centerOfMass[k]=std::bit_cast<float>(treeMass->centerOfMass[k]);
 for(unsigned k=0;k<9;k++){mass.inertia[k]=std::bit_cast<float>(treeMass->inertia[k]);mass.inverseInertia[k]=std::bit_cast<float>(treeMass->inverseInertia[k]);}
 ssx::OriginalSphereTreeCollider hit;hit.scale=1;hit.tree=node.sphereTree.get();hit.treeResource=node.sphereTreeResource;
 static constexpr float table[4][3]={{-1,-1,-1},{-1,-1,1},{-1,1,-1},{-1,1,1}};
 for(unsigned i=0;i<4;i++){hit.corners[i]={table[i][0],table[i][1],table[i][2],0.f};hit.corners[7-i]={-table[i][0],-table[i][1],-table[i][2],0.f};}
 hit.center={node.sphereTree->centerCm[0],node.sphereTree->centerCm[1],node.sphereTree->centerCm[2],1.f};
 ssx::RollerMatrix instanceMatrix;for(unsigned i=0;i<4;i++)for(unsigned k=0;k<4;k++)instanceMatrix[i][k]=instance->authoredMatrix[i*4+k];
 // builtin15 default arguments (2FD250 block 0x4FB628): mass 1.0, parameter *(gp-0x3388).
 ssx::OriginalRollerScriptArgs args;args.mass=1.f;args.parameter=std::bit_cast<float>(0x3f333333u);
 BrowserRoller roller;roller.resource=packet.instance;roller.instance=instance;
 roller.modifier=ssx::originalRollerConstruct(instanceMatrix,hit,mass,roller_record(record),args,flags,packet.instance);
 // 3554B0 attach: 350570 bounds/radius from the authored AABB and translation, flags &~0x20|0x40.
 flags=ssx::originalRollerAttach(roller.modifier,instanceMatrix[3],instance->low,instance->high,flags);
 instance->eventRuntimeFlags=flags;
 browserRollers.push_back(std::move(roller));refresh_roller_entity(browserRollers.back());++rollerCreated;
 return true;
}
// 355420 (entity vtable+0x74) -> modifier vtable+0x44 (361CA8): 0, so 1057B8 takes the mass path.
bool browser_stage_livecomp_rigid(uint32_t resource); // web/stage_world.inc: a LiveComp-animated instance (0x355420: 1)
bool browser_stage_livecomp_selected(OriginalInstanceContactPacket& packet); // web/stage_world.inc: its 0x34E698 node velocity
bool browser_stage_frozen_magnet(uint32_t instance); // web/stage_world.inc: a collected pickup's Debounce (PositionModifier +0x44 0x360B60 = 1, +0xB4 0x360BD8 no-op)
static bool browser_entity_rigid(uint32_t resource){if(browser_roller(resource))return false;if(chairlift_of(resource))return true; /*MultiSpline +0x44 0x360B60*/if(rail_livecomp_rigid(resource))return true; /*LiveComp 0x355420: 1*/if(browser_stage_livecomp_rigid(resource))return true;if(browser_stage_frozen_magnet(resource))return true;if(spline_piece_of(resource))return true; /*Spline +0x44 0x360B60*/throw std::runtime_error("Unported entity rigid predicate");}
// 104E70 selected entity callback 356AE0 -> 353098 -> modifier vtable+0xB4 (360BD8): no-op.
bool browser_stage_magnet_selected(uint32_t instance); // web/stage_world.inc: Object 0x356AE0 -> magnet vt+0xB4 0x360BD8 (no-op)
static void browser_entity_selected(OriginalInstanceContactPacket& packet){if(chairlift_selected(packet))return;if(rail_livecomp_selected(packet))return;if(browser_stage_livecomp_selected(packet))return;if(browser_stage_magnet_selected(packet.instance))return;if(browser_stage_frozen_magnet(packet.instance))return;
 if(const auto* piece=spline_piece_of(packet.instance)){ /*Object 0x356AE0 -> Spline vt+0xB4 0x359CF8 (R&B train)*/
  const auto v=ssx::originalSplineContactVelocity(piece->spline,{packet.point[0],packet.point[1],packet.point[2],packet.point[3]});
  if(v.applied){OriginalRounding rounding;for(unsigned k=0;k<4;k++)packet.surfaceVelocity[k]=terrain_original::add(packet.surfaceVelocity[k],v.linear[k]);}
  return;}
 if(!browser_roller(packet.instance))throw std::runtime_error("Unported selected entity callback");}
// Draw delta per active roller in three.js space: C * M * A^-1 * C^-1 (C: source cm Z-up -> m Y-up).
// Draw delta D = A^-1 * M (row-vector source cm, p_new = p_baked * A^-1 * M) as three.js elements
// C * D * C^-1 (C: source cm Z-up -> m Y-up), appended as key + 16 floats.
static void append_moving_delta(std::vector<float>& out,float key,const collision_transform::Matrix& m,const collision_transform::Matrix& a){
 double ai[16];{ // rigid inverse of A (authored instance matrices are rigid)
  for(int i=0;i<3;i++)for(int j=0;j<3;j++)ai[i*4+j]=a[j*4+i];
  for(int j=0;j<3;j++){double t=0;for(int k=0;k<3;k++)t-=double(a[12+k])*ai[k*4+j];ai[12+j]=t;}
  ai[3]=ai[7]=ai[11]=0;ai[15]=1;
 }
 double d[16];for(int i=0;i<4;i++)for(int j=0;j<4;j++){double t=0;for(int k=0;k<4;k++)t+=ai[i*4+k]*double(m[k*4+j]);d[i*4+j]=t;}
 // three p3 = (x/100, z/100, -y/100) of source p; column-major elements e[col*4+row].
 const int axis[3]={0,2,1};const double sign[3]={1,1,-1};
 float e[16]={};
 for(int t=0;t<3;t++)for(int u=0;u<3;u++)e[u*4+t]=float(sign[t]*sign[u]*d[axis[u]*4+axis[t]]);
 for(int t=0;t<3;t++)e[12+t]=float(sign[t]*d[12+axis[t]]/100);
 e[15]=1;
 out.push_back(key);out.insert(out.end(),e,e+16);out[0]+=1;
}
// Draw delta per active roller and per chairlift car (key = resource + car * 2^20).
extern "C" EMSCRIPTEN_KEEPALIVE float* moving_instances(){
 RIDER_LOCAL static std::vector<float> out;out.assign(1,0.f);
 for(const auto& r:browserRollers)append_moving_delta(out,float(r.resource),roller_matrix(r.modifier),r.instance->authoredMatrix);
 chairlift_moving(out,append_moving_delta);
 browser_avalanche_moving(out,append_moving_delta); // AvaSpline followers (web/avalanche_gameplay.inc)
 return out.data();
}
// Diagnostics: count, created, kicks, ticks, then per roller (13): resource, timer +0x20, position, momentum P, quaternion, contact timer +0x28.
extern "C" EMSCRIPTEN_KEEPALIVE float* roller_info(){
 RIDER_LOCAL static std::vector<float> out;out={float(browserRollers.size()),float(rollerCreated),float(rollerKicks),float(rollerTicks)};
 for(const auto& r:browserRollers){const auto& m=r.modifier;out.push_back(float(r.resource));out.push_back(m.timer);for(int k=0;k<3;k++)out.push_back(m.body.position[k]);for(int k=0;k<3;k++)out.push_back(m.body.linearMomentum[k]);for(int k=0;k<4;k++)out.push_back(m.body.orientation[k]);out.push_back(m.contactTimer);}
 return out.data();
}
// Diagnostics: the original 0x2D0-byte image of roller k (creation order); pointer words
// (vtable, instance, collider tree) are placeholders.
extern "C" EMSCRIPTEN_KEEPALIVE uint8_t* roller_bytes(unsigned k){
 RIDER_LOCAL static std::array<uint8_t,0x2D0> out{};out={};if(k<browserRollers.size())out=ssx::originalRollerModifierBytes(browserRollers[k].modifier);return out.data();
}
