// Stage-script entity world of the race event (included by web/stage_script_gameplay.inc before the
// builtin dispatcher). The LUN programs of every handler slot run through the VM there; this file owns the
// entities they create that no other browser system models:
// * LiveComp players built by trigger/timer programs (builtin3 0x2FBCB8 -> 0x341AA0 with the gameplay-RNG
//   key8/key6 draws): the 0x341D48 tick in the entity pass, handler slot 5 (0x34EBA0 -> 0x30A688, the
//   builtin55 0x3019C8 crossing gate 0x34EBE0) and slot 4 when a once-mode player finishes (0x341FC8 ->
//   0x34FCC0). Their node matrices (0x361098) carry DynamicParticle trails (snow crumbs). The JS players
//   (web/livecomp-animation.js) draw them from the start log stage_world_livecomps().
// * Particle effects (engine/set_piece_particles.hpp): builtin16 0x2FD420 MakeParticleData (type-13 emitter
//   entity 0x48EE60, or the conversion 0x2FAE38 of an existing entity: effects moved and stopped, matrix
//   frozen in a PositionModifier), builtin25 0x2FE840 AddParticleData (a Particle pushed onto the existing
//   entity 0x355CB0, or a new type-13 entity; race mode: only with a rider within 30000 cm), builtin26
//   0x2FEE98 AddDynamicParticleData (0x355D10 DynamicParticle on a carrier node), builtin69 0x302490 (mode 0
//   stops every effect of the entity). Updated in the world object pass (0x354F98 group 1: entities newest
//   first; per entity 0x356198: modifier, LiveComp tick incl. slot 5, then the effect list 0x352D20 in list
//   order) from race_begin, before the rider. The visual RNG 0x4FF018 (0x3177F0) is the core's snow/camera
//   stream (browser_camera_random), seeded from the ready savestate otherwise.
// * Sound requests of builtin30 0x2FFF00 are logged (stage_world_sounds) for the audio system.
// Data: /assets/<LOC>/PARTICLES/particles.json (tools/export_set_piece_particles.py: instances, carrier node
// chains, race-tick-0 effects, visual RNG) and the location's LIVECOMP/livecomp.json (models), passed by
// web/set-pieces-renderer.js to init_stage_world(). Without them nothing here runs (node harnesses that do not
// load the data see the pre-2026-09-23 behaviour).
#include "../engine/set_piece_particles.hpp"
#include "../engine/livecomp_animation.hpp"
#include "../engine/magnet_modifier.hpp"
#include "../engine/one_way_volume.hpp"
#include "../engine/rider_trigger_contacts.hpp"
#include "../engine/flag_cloth.hpp"
#include "json.hpp"
#include <memory>
namespace stage_world {
using PMat=ssx::set_piece_particles::Mat4;
struct Model {uint32_t resource=0;std::string name;ssx::OriginalAnimModel model;ssx::RollerMatrix matrix{};float scale=1;uint32_t flags=0;};
struct Effect {bool dynamic=false;ssx::OriginalParticleEffect burst;ssx::OriginalDynamicParticleEffect trail;uint32_t program=0;};
enum Type:int {External=0,LiveComp=1,Boost=8,Emitter=13,Breaking=11};
// MeshAnim (type 11, vtable 0x48F6D8): the break pieces of an instance fly apart (0x351B40 / 0x352230 / 0x352500).
struct MeshAnim {int32_t endMode=0;float life=0,fade=-1,gravity=.1f,spinMax=0;std::vector<std::array<float,4>> q,pos,axis,vel;std::vector<float> spin;bool finished=false;};
struct MeshModel {uint32_t resource=0;PMat matrix{};float scale=1;std::vector<int32_t> parent;std::vector<PMat> local;};
struct Entity {
 uint32_t resource=0;uint64_t serial=0;int type=External;bool alive=true;MeshAnim mesh;
 bool frozen=false;PMat matrix{};              // type 13: vt+0xC4 (instance+0x10, or the frozen matrix of the replaced entity)
 Model* model=nullptr;ssx::OriginalLiveComp live;uint32_t startTick=0; // type 1
 bool sectionPlayer=false; // type 1 started by a section scan: the JS player draws it, the core runs its clock and slots 4 / 5
 ssx::OriginalAnimNodeVelocityTable velocities; // type 1: entity+0x4C (0x34E798, the 104E70 selected-contact velocity)
 ssx::OriginalBoostVolume boost;uint32_t pushes=0;   // type 8 (one-way volume)
 std::vector<Effect> effects;                  // [0] = list head (0x345700 pushes to the front)
};
struct Carrier {float scale=1;std::map<int32_t,std::vector<PMat>> chains;};
struct Initial {uint32_t resource=0;bool dynamic=false;uint32_t dead=0;PMat matrix{};std::vector<uint32_t> emitter,block;};
// MultiParticle group (builtin105 0x306A90 -> 0x357A78 / 0x357DD8, 0x50 bytes, vtable 0x48EA00): one static emitter
// (0x370018 + 0x3705E0 at the identity 0x4FF1A0) updated once per world tick by MultiParticleMan 0x357BF8 -> 0x358120
// (0x370788, dt gp-0x2A80) and drawn by 0x358260 at every registered instance: kernel +0xB0 (emitter +0xC0) = instance
// translation (instance+0x40) + the constructed position base (+0x40), bounds +0x20/+0x30 moved alike (0x370888).
struct MultiGroup {bool used=false;uint32_t capacity=0;std::vector<uint32_t> members;ssx::OriginalStaticEmitter emitter;ssx::set_piece_particles::Vec4 positionBase{};};
}
RIDER_LOCAL_LAZY static std::map<uint32_t,std::unique_ptr<stage_world::Model>> stageWorldModels;
RIDER_LOCAL_LAZY static std::map<uint32_t,stage_world::PMat> stageWorldMatrices;  // authored instance+0x10
RIDER_LOCAL_LAZY static std::map<uint32_t,float> stageWorldScales;                 // instance+0x84
RIDER_LOCAL_LAZY static std::map<uint32_t,stage_world::Carrier> stageWorldCarriers;
RIDER_LOCAL static std::vector<stage_world::Initial> stageWorldInitial;
RIDER_LOCAL static std::array<stage_world::MultiGroup,4> stageMultiGroups,stageMultiInitial; // MultiParticleMan *(gp+0xF38) slots (live / race tick 0)
RIDER_LOCAL static bool stageMultiExported=false;
// HaloModifier (builtin97 0x3057C0, ctor 0x346120, vtable 0x491220; effect type 3): the glow sprite of pickups.
struct StageHalo {uint32_t resource=0;int32_t texture=0,node=-1,spin=0;float r=1,g=1,b=1,a=1,size=100,angle=0;};
RIDER_LOCAL static std::vector<StageHalo> stageHalos; // newest first (0x345700 prepends)
RIDER_LOCAL static std::vector<StageHalo> stageHaloInitial; // particles.json "halos": the load's halos (ready savestate)
RIDER_LOCAL_LAZY static std::map<uint32_t,ssx::OriginalMagnetModifier> stageMagnets;   // builtin90 MagnetModifiers (BHP1 pointa/pointc) by instance
struct StageMagnetInitial {uint32_t resource=0,flags=0;std::vector<uint32_t> words;};
RIDER_LOCAL static std::vector<StageMagnetInitial> stageMagnetInitial; // particles.json "magnets": the load's magnets (ready savestate)
struct StageBoostInitial {uint32_t resource=0;std::array<uint32_t,9> words{};};
RIDER_LOCAL static std::vector<StageBoostInitial> stageBoostInitial; // particles.json "boosts": one-way volumes the load built (R&B, Crow's Nest)
RIDER_LOCAL_LAZY static std::map<uint32_t,stage_world::PMat> stageFrozenMagnetMatrix; // their frozen magnet matrix: the Debounce's PositionModifier (entity vt+0xC4)
RIDER_LOCAL static std::vector<uint32_t> stageFrozenMagnets;                       // collected pickups whose Debounce holds the frozen magnet matrix (entity collision)              // particles.json "multi": the groups of the ready savestate
RIDER_LOCAL_LAZY static std::map<uint32_t,stage_world::MeshModel> stageMeshModels;   // builtin13 targets (stage-world.json meshanim)
RIDER_LOCAL_LAZY static std::map<uint32_t,std::pair<std::array<uint32_t,11>,uint32_t>> stageSectionLiveComps; // section-started LiveComps (JS players): builtin3 words, start tick
RIDER_LOCAL_LAZY static std::map<uint32_t,uint32_t> stageFlags;                      // instance+8 of instances the programs changed (else the countdown value)
RIDER_LOCAL_LAZY static std::map<uint32_t,int> stageNodeState;                       // 6 DeadNode, 16 Hide, 19 RestoreNode (their empty draw / contact)
RIDER_LOCAL static std::vector<uint32_t> stageEntityLog;                        // (tick, resource, event): 1 entity destroyed (JS players stop), 2 MeshAnim start, 3 MeshAnim end
RIDER_LOCAL static std::optional<OriginalInstanceContactRecord> stage_contact_record; // *(context +0x294): the contacting rider's record (rider+A60) during a slot-2 program
RIDER_LOCAL_LAZY static std::map<uint32_t,std::array<uint32_t,16>> stageTeleports;   // builtin34 destinations (instance matrix bits)
RIDER_LOCAL static ssx::OriginalRandomState stageWorldVisualTick0,stageWorldVisualOwn;
RIDER_LOCAL static std::string stageWorldLocation;RIDER_LOCAL static bool stageWorldLoaded=false;
RIDER_LOCAL static std::vector<stage_world::Entity> stageEntities;RIDER_LOCAL static uint64_t stageEntitySerial=0;
RIDER_LOCAL static std::vector<uint32_t> stageLiveCompLog;   // per start: tick, resource, 11 words, key8 word, key6 word, flags (bit0 key8 drawn, bit1 key6 drawn)
RIDER_LOCAL static std::vector<uint32_t> stageSoundLog;      // per request: tick, program, current instance, 3 raw words (keys 0..2), key mask
RIDER_LOCAL static std::vector<uint32_t> stageWorldSlotLog;  // per slot 4/5 program run: tick, resource, slot, program
RIDER_LOCAL static std::vector<uint32_t> stageSlot4Pending;  // spline pieces that finished in this entity pass (0x352ED0 -> vt+0x114)
RIDER_LOCAL static unsigned stageParticleCreates=0,stageParticleRefused=0,stageLiveCompStarts=0;
RIDER_LOCAL static int stage_current_slot=-1;                // handler slot of the running program (-1: global/setup)
RIDER_LOCAL static bool stage_section_run=false;             // slot-1 program of the section pass: gameplay-RNG users are owned by the section model
static ssx::OriginalRandomState& stage_visual_random(){if(auto* r=browser_camera_random())return *r;return stageWorldVisualOwn;}
static uint32_t stage_world_tick(){return chairliftTicks?chairliftTicks-1:0;}
static stage_world::Entity* stage_entity(uint32_t resource){for(auto it=stageEntities.rbegin();it!=stageEntities.rend();++it)if(it->alive&&it->resource==resource)return &*it;return nullptr;}
static stage_world::PMat stage_instance_matrix(uint32_t resource){
 auto it=stageWorldMatrices.find(resource);if(it!=stageWorldMatrices.end())return it->second;
 if(auto mm=stageMeshModels.find(resource);mm!=stageMeshModels.end())return mm->second.matrix; // instance+0x10 of builtin13 targets (stage-world.json)
 if(auto* w=set_piece_instance_at(resource)){stage_world::PMat m;for(unsigned i=0;i<16;i++)m[i/4][i%4]=w->authoredMatrix[i];return m;}
 return ssx::set_piece_particles::kIdentity;
}
// Entity matrix vt+0xC4 of an instance: its primary modifier's matrix (spline piece: Spline, or the
// PositionModifier after a stop-mode path end; resident looping spline), else instance+0x10.
static stage_world::PMat stage_entity_matrix(uint32_t resource){
 if(auto it=stageMagnets.find(resource);it!=stageMagnets.end()){const auto& M=ssx::originalMagnetMatrix(it->second);stage_world::PMat m;for(unsigned r=0;r<4;r++)for(unsigned c=0;c<4;c++)m[r][c]=M[r][c];return m;} // main modifier vt+0x94
 // A collected magnet pickup (0x355F10): while its Debounce lives, vt+0xC4 is the frozen magnet matrix, so the award's
 // builtin25 burst is born where the pickup reached the rider (PS2 pipe-finish 2863..2881), not at the authored instance.
 if(auto it=stageFrozenMagnetMatrix.find(resource);it!=stageFrozenMagnetMatrix.end())if(const auto* st=stage_instance(resource);st&&st->debounce)return it->second;
 if(auto* e=stage_entity(resource);e&&e->type==stage_world::Emitter)return e->matrix;
 if(const auto* root=attached_root(resource))return *root;
 return stage_instance_matrix(resource);
}
// Kind of the entity at instance+0xC as the particle builtins see it (0 none, 1 LiveComp, 13 emitter, 17 other).
static int stage_entity_kind(uint32_t resource){
 if(auto* e=stage_entity(resource))return e->type==stage_world::External?17:e->type;
 for(auto& p:browserSplinePieces)if(p.seed->resource==resource)return attached_livecomp(resource)?1:17;
 for(auto& l:browserLoopSplines)if(l.seed->resource==resource)return 17;
 if(const auto* s=stage_instance(resource);s&&s->entity!=StageNoEntity)return 17;
 return 0;
}
// Carrier node world matrix 0x34FED8: animated LiveComp nodes (vt+0xEC) or the static node chain (model
// node table *(instance+0x80)+8) times the entity matrix.
static bool stage_node_matrix(uint32_t resource,int32_t node,stage_world::PMat& out){
 if(node<0)return false;
 if(auto* e=stage_entity(resource);e&&e->type==stage_world::LiveComp){const auto& m=ssx::originalLiveCompMatrices(e->live);if(size_t(node)>=m.size())return false;out=m[size_t(node)];return true;}
 if(auto* e=stage_entity(resource);!e||e->type!=stage_world::Emitter)
  if(auto* x=attached_livecomp(resource);x&&x->active)if(const auto* nodes=attached_nodes(*x);nodes&&size_t(node)<nodes->size()){out=(*nodes)[size_t(node)];return true;}
 const auto c=stageWorldCarriers.find(resource);
 static const std::vector<stage_world::PMat> none;
 const auto& chain=c!=stageWorldCarriers.end()&&c->second.chains.count(node)?c->second.chains.at(node):none;
 if(chain.empty()&&node>0)return false;
 out=ssx::originalParticleNodeMatrix(chain,c!=stageWorldCarriers.end()?c->second.scale:1.f,stage_entity_matrix(resource));return true;
}
static void stage_run_slot(uint32_t resource,int slot); // web/stage_script_gameplay.inc (after stage_run_handler)
static stage_world::Entity& stage_new_entity(uint32_t resource,int type){
 if(auto* old=stage_entity(resource))old->alive=false;
 stage_world::Entity e;e.resource=resource;e.serial=++stageEntitySerial;e.type=type;stageEntities.push_back(std::move(e));return stageEntities.back();
}
static void stage_world_destroy(uint32_t resource){for(auto& e:stageEntities)if(e.resource==resource)e.alive=false;}
// Collision of a LiveComp-animated instance: 0x334888 composes an instance whose entity answers vt+0xD4 (every LiveComp:
// 0x361090 = 1) from the entity's node matrices (vt+0xE4 0x3560C0 -> 0x361098), on either broad-phase route; the static
// route keeps the authored inline box +0x60/+0x6C (EBC3 mdl_EBC3_fallingpatha_1000: program 19 on the fallingpathatrigger
// contact starts its 4.43 s fall, and from then on its three triangle nodes follow the animation: PS2 allpeak/apj-start
// 2661, the rider slides under where the path stood; web/falling_billboard_collision.inc does the same for Snow Jam's
// billboard). The authored nodes come back when the entity goes (a section leave, a location eviction, a new race).
RIDER_LOCAL_LAZY static std::map<uint32_t,std::vector<std::pair<collision_transform::Matrix,collision_transform::Matrix>>> stageLiveCompRest;
static void stage_livecomp_collision(){
 for(auto it=stageLiveCompRest.begin();it!=stageLiveCompRest.end();){
  if(const auto* e=stage_entity(it->first);e&&e->type==stage_world::LiveComp&&e->model){++it;continue;}
  if(auto* w=set_piece_instance_at(it->first)){for(size_t k=0;k<w->nodes.size()&&k<it->second.size();k++){w->nodes[k].world=it->second[k].first;w->nodes[k].inverse=it->second[k].second;}w->entityPointer=false;}
  it=stageLiveCompRest.erase(it);
 }
 for(auto& e:stageEntities){
  if(!e.alive||e.type!=stage_world::LiveComp||!e.model)continue;
  auto* w=set_piece_instance_at(e.resource);if(!w||w->nodes.empty()||w->type==0)continue;
  const auto& m=ssx::originalLiveCompMatrices(e.live);
  if(!stageLiveCompRest.count(e.resource)){std::vector<std::pair<collision_transform::Matrix,collision_transform::Matrix>> rest;for(const auto& n:w->nodes)rest.push_back({n.world,n.inverse});stageLiveCompRest.emplace(e.resource,std::move(rest));}
  for(auto& n:w->nodes){if(n.index>=m.size())continue;collision_transform::Matrix x;for(unsigned i=0;i<16;i++)x[i]=m[n.index][i/4][i%4];n.world=x;n.inverse=collision_transform::inverseRigid(x);}
  w->entityPointer=true;
 }
}
// 1057B8 rigid predicate (entity+0x74 0x355420): 1 for a LiveComp without a primary modifier.
bool browser_stage_livecomp_rigid(uint32_t resource){const auto* e=stage_entity(resource);return e&&e->type==stage_world::LiveComp&&e->model&&stageLiveCompRest.count(resource);}
// 104E70 selected-entity callback (LiveComp vt+0x154 0x34E698): the node's velocity at the contact point into packet +0x20.
bool browser_stage_livecomp_selected(OriginalInstanceContactPacket& packet){
 auto* e=stage_entity(packet.instance);if(!e||e->type!=stage_world::LiveComp||!e->model||!stageLiveCompRest.count(packet.instance))return false;
 ssx::RollerQuad angular{0,0,0,0}; // packet +0x30 is not carried by OriginalInstanceContactPacket
 ssx::originalAnimContactVelocity(e->live.anim,ssx::originalLiveCompRate(e->live),e->velocities,int32_t(packet.node),packet.point,packet.surfaceVelocity,angular);
 return true;
}

// ---- particle builtins -----------------------------------------------------------------------------
static bool stage_rider_near(const stage_world::PMat& m){
 // 0x2FEB24..0x2FEE00: race mode ((gp-0x84C)+0x550 == 2) creates the effect only when a rider (both player lists)
 // is within 30000 cm (gp-0x337C..-0x3370) of the entity translation. The other riders' positions are the shared
 // race-world buffer; in practice every caller is next to a rider.
 terrain_original::Rounding rounding;
 auto near=[&](float x,float y,float z){const float dx=terrain_original::sub(m[3][0],x),dy=terrain_original::sub(m[3][1],y),dz=terrain_original::sub(m[3][2],z);
  return terrain_original::sqrt(terrain_original::add(terrain_original::add(terrain_original::mul(dx,dx),terrain_original::mul(dy,dy)),terrain_original::mul(dz,dz)))<30000.f;};
 // Only the human rider's position lives in this core; the callers (pickup contacts, section enters within the
 // 15000 cm scan box, timers started by a nearby trigger) always have the human or the contacting rider close.
 return sharedWorldReplay||near(physicsState.position[0],physicsState.position[1],physicsState.position[2]);
}
static uint32_t stage_target(uint32_t word){return int32_t(word)==-1?stage_current_instance:word;}
// builtin16 / builtin25 / builtin26 parameter blocks (keys in export_snow_assets FIELDS order).
static ssx::OriginalParticleParams stage_particle_block(int builtin,const StageArg* args,int count){
 auto p=ssx::originalParticleDefaults(builtin==26?26:16);p.builtin=builtin==26?26:16;
 for(int i=0;i<count;i++)ssx::originalParticleApplyArgument(p,unsigned(args[i].key),args[i].type,args[i].word0);
 return p;
}
static void stage_particle_builtin(int builtin,const StageArg* args,int count){
 if(!stageWorldLoaded)return;
 using namespace ssx::particle_param;auto p=stage_particle_block(builtin,args,count);
 const uint32_t target=stage_target(p.u(Instance));
 if(!target||!stage_instance(target)&&!stageWorldMatrices.count(target)&&!set_piece_instance_at(target)){++stageParticleRefused;return;}
 auto& visual=stage_visual_random();
 const int kind=stage_entity_kind(target);
 if(builtin==26){ // 0x2FEE98: always clamps; needs an entity that accepts modifiers (vt+0x84); node < model node count
  ssx::originalParticleClampLifeRange(p);
  if(!kind){++stageParticleRefused;return;}
  stage_world::PMat node;if(!stage_node_matrix(target,p.i(Node),node)){++stageParticleRefused;return;}
  auto* e=stage_entity(target);if(!e)e=&stage_new_entity(target,kind==1&&!attached_livecomp(target)?stage_world::LiveComp:stage_world::External);
  stage_world::Effect x;x.dynamic=true;x.program=stageLastProgram;x.trail=ssx::originalDynamicParticleConstruct(p,node,visual);
  e->effects.insert(e->effects.begin(),std::move(x));++stageParticleCreates;return;
 }
 if(builtin==25){ // 0x2FE840: always clamps
  ssx::originalParticleClampLifeRange(p);
  if(kind){ // 355CB0 onto the existing entity (vt+0x84 accepts), owner matrix = entity vt+0xC4
   const auto m=stage_entity_matrix(target);if(!stage_rider_near(m))return;
   auto* e=stage_entity(target);if(!e){e=&stage_new_entity(target,stage_world::External);}
   stage_world::Effect x;x.program=stageLastProgram;x.burst=ssx::originalParticleEffectConstruct(p,m,visual);e->effects.insert(e->effects.begin(),std::move(x));++stageParticleCreates;return;
  }
  const auto m=stage_instance_matrix(target);if(!stage_rider_near(m))return;
  auto& e=stage_new_entity(target,stage_world::Emitter);e.matrix=m;
  stage_world::Effect x;x.program=stageLastProgram;x.burst=ssx::originalParticleEffectConstruct(p,m,visual);e.effects.insert(e.effects.begin(),std::move(x));
  ++stageParticleCreates;if(!stage_section_run)section_entity_created(target);return;
 }
 // builtin16 0x2FD420
 if(!kind){ // new type-13 entity 0x3578A8 (clamp LifeR <= Life), owner matrix instance+0x10
  ssx::originalParticleClampLifeRange(p);
  auto& e=stage_new_entity(target,stage_world::Emitter);e.matrix=stage_instance_matrix(target);
  stage_world::Effect x;x.program=stageLastProgram;x.burst=ssx::originalParticleEffectConstruct(p,e.matrix,visual);e.effects.insert(e.effects.begin(),std::move(x));
  ++stageParticleCreates;if(!stage_section_run)section_entity_created(target);return;
 }
 if(kind==13){++stageParticleRefused;return;} // 0x2FAE38: already type 13 -> vt+0x12C 0x3609D8 (no-op); the block is freed
 // 0x2FAE38 -> 0x3578A8(obj,1,inst,block,old) -> 0x355F10: the old modifier/effect lists move to the new type-13
 // entity (every moved effect gets vt+0x1C: a DynamicParticle stops), a matrix other than instance+0x10 is frozen
 // in a PositionModifier; no LifeR clamp. The new entity is inserted at the head of the world list.
 const auto matrix=stage_entity_matrix(target);std::vector<stage_world::Effect> moved;
 if(auto* old=stage_entity(target)){moved=std::move(old->effects);old->alive=false;}
 for(auto& m:moved)if(m.dynamic)ssx::originalDynamicParticleStop(m.trail);
 auto& e=stage_new_entity(target,stage_world::Emitter);e.matrix=matrix;e.frozen=attached_root(target)!=nullptr;e.effects=std::move(moved);
 stage_world::Effect x;x.program=stageLastProgram;x.burst=ssx::originalParticleEffectConstruct(p,e.matrix,visual);e.effects.insert(e.effects.begin(),std::move(x));
 ++stageParticleCreates;
}
// builtin69 0x302490 (instance -1 current, mode): 0 stops every effect (vt+0x1C: DynamicParticle 0x346060; Particle
// no-op), 1 deletes the effects whose vt+0x14 == 3 (neither particle class: Particle 1, DynamicParticle 2).
// builtin105 0x306A90: parameter block (defaults 0x4FBF68 = builtin16's, +0xD8 group 0, +0xDC capacity 0x80; keys 0..53
// particle fields, 54 group, 55 capacity) -> 0x357A78: a free group slot gets a MultiParticle (9 visual draws, no LifeR
// clamp); an existing group ignores the call. Stage load (global programs: ARA1 program 20, BRA2 226, the roadflare
// flame).
static void stage_multiparticle_create(const StageArg* args,int count){
 auto p=ssx::originalParticleDefaults(16);p.setI(0xD8,0);int32_t group=0,capacity=0x80;
 for(int i=0;i<count;i++){const auto& a=args[i];
  if(a.key==54)group=int32_t(a.word0);else if(a.key==55)capacity=int32_t(a.word0);
  else if(a.key>=0&&a.key<54)ssx::originalParticleApplyArgument(p,unsigned(a.key),a.type,a.word0);}
 // The ready savestate holds the groups the load built (particles.json "multi", seeded by stage_world_reset); only
 // data without that export builds them here, with a copy of the tick-0 visual RNG (the load's draws are not replayed
 // on the shared generator).
 if(!stageWorldLoaded||stageMultiExported||group<0||group>3||stageMultiGroups[size_t(group)].used)return;
 auto& g=stageMultiGroups[size_t(group)];g.used=true;g.capacity=uint32_t(std::max(capacity,0));g.members.clear();
 auto visual=stageWorldVisualTick0;g.emitter=ssx::originalStaticEmitterConstruct(p,ssx::set_piece_particles::kIdentity,visual);
 const auto pb=g.emitter.v(unsigned(ssx::static_emitter::Kernel)+unsigned(ssx::particle_kernel::PosBase));g.positionBase={pb[0],pb[1],pb[2],0.f};
 stageMultiInitial[size_t(group)]=g;
}
// builtin106 0x306CB0 (group 0, instance -1 current, mode): mode 0 appends (0x3581B8, ignored when full, no duplicate
// check), else removes the first entry of the instance (0x3581F0: the last entry moves into its place). Roadflare slot 1 /
// slot 3: the race-start scan registers the load's roadflares a second time, and each leave removes one entry.
static void stage_multiparticle_member(const StageArg* args,int count){
 std::array<uint32_t,3> f{0u,0xffffffffu,0u};stage_parse(f,std::array<int32_t,3>{1,1,1},args,count); /*types 0x446970*/
 const int32_t group=int32_t(f[0]);const uint32_t target=stage_target(f[1]);
 if(group<0||group>3||!stageMultiGroups[size_t(group)].used)return;auto& g=stageMultiGroups[size_t(group)];
 if(f[2]==0){if(g.members.size()<g.capacity)g.members.push_back(target);return;}
 for(size_t i=0;i<g.members.size();i++)if(g.members[i]==target){g.members[i]=g.members.back();g.members.pop_back();return;}
}
static void stage_multiparticle_tick(){ // 0x357BF8: groups 0..3
 for(auto& g:stageMultiGroups)if(g.used)ssx::originalStaticEmitterUpdate(g.emitter,std::bit_cast<float>(0x3C888889u));
}
static void stage_particle_stop(const StageArg* args,int count){
 std::array<uint32_t,2> f{0xffffffffu,0u};stage_parse(f,std::array<int32_t,2>{1,1},args,count);
 if(f[1]==1){const uint32_t t=stage_target(f[0]);std::erase_if(stageHalos,[&](const StageHalo& h){return h.resource==t;});return;} // 0x353278: vt+0x14 == 3 (halos)
 auto* e=stage_entity(stage_target(f[0]));if(!e||f[1]!=0)return;
 for(auto& x:e->effects)if(x.dynamic)ssx::originalDynamicParticleStop(x.trail);
}

// ---- instance flags and node states (builtins 2 / 29 / 58 / 13) --------------------------------------------
// instance+8: bit0|bit1 static draw ((f&3)==3, 0x22A5A0), bit2 entity draw, 0x20 static / 0x40 entity collision route
// (333EF8). Changes reach the collision world (eventRuntimeFlags) and the renderer (stage_world_instances).
RIDER_LOCAL_LAZY static std::map<uint32_t,uint32_t> stageScriptCountdownFlags; // stage-world.json "script": countdown runtime flags (instance+8) of every instance the programs change
// pv loadFlags (web/free-ride.js stage_load_flags; docs/peak-mountain.md "Course limits"): an instance nothing has changed yet
// holds the load's runtime flags, the authored high half copied down with bit 1 (34FC1C: authored 0x200000 -> 0x200022,
// 0x210000 -> 0x210023; every untouched instance of the 794 PS2 free-ride savestates), not the authored word. With the
// authored word, builtin 7 (one-way volume, flags | 0x100) left 0x200100: no static route, so the connectors' one-way
// volumes never reached the rider list and never pushed back (PS2: 0x200322, entity type 8). Off: the authored word as before.
RIDER_LOCAL static bool stageLoadFlags=false;
extern "C" EMSCRIPTEN_KEEPALIVE void stage_load_flags(int on){stageLoadFlags=on!=0;}
static uint32_t stage_flags(uint32_t resource){
 if(auto it=stageFlags.find(resource);it!=stageFlags.end())return it->second;
 if(const auto* st=stage_instance(resource);st&&st->flagsKnown)return st->flags;
 if(auto* w=set_piece_instance_at(resource);w&&!w->eventRuntimeFlags)if(auto c=stageScriptCountdownFlags.find(resource);c!=stageScriptCountdownFlags.end())return c->second; // e.g. R&B treerailhevbb 0x210023 (authored 0x210000)
 if(auto* w=set_piece_instance_at(resource))return w->eventRuntimeFlags.value_or(stageLoadFlags?ssx::originalPickupRestoredFlags(w->flags):w->flags);
 return 0;
}
RIDER_LOCAL_LAZY static std::map<uint32_t,std::optional<uint32_t>> stageFlagsCountdown; // collision-world eventRuntimeFlags before the first change (race restart)
// The collision world's flags of an instance before a run first changes them, put back by the next race reset (stage_world_reset).
static void stage_note_countdown_flags(uint32_t resource){if(!stageFlagsCountdown.count(resource)){auto* w=set_piece_instance_at(resource);stageFlagsCountdown[resource]=w?w->eventRuntimeFlags:std::nullopt;}}
void shared_world_node_changed(uint32_t resource); // web/shared_world.inc (kind 9, pv eventInWorldAi: off unless shared_world_nodes)
static void stage_set_flags(uint32_t resource,uint32_t flags){
 stage_note_countdown_flags(resource);
 stageFlags[resource]=flags;
 if(auto* w=set_piece_instance_at(resource))w->eventRuntimeFlags=flags;
 if(auto* st=stage_instance(resource)){st->flags=flags;st->flagsKnown=true;}
 shared_world_node_changed(resource);
}
static void stage_log_entity(uint32_t resource,uint32_t event){if(stageEntityLog.size()<3*16384)stageEntityLog.insert(stageEntityLog.end(),{stage_world_tick(),resource,event});}
// Base destructor 0x34FBF0 (vt+0xC mode 3): the instance flags return to the authored high half (0x34FC1C) with bit 1
// forced, instance+0xC cleared.
static void stage_destroy_entity(uint32_t resource){
 bool any=false;for(auto& e:stageEntities)if(e.alive&&e.resource==resource){e.alive=false;any=true;}
 browser_dynamic_rails_unbind(resource); // 0x35BD70: the entity's RailModifiers go with it (R&B animloga crane rail, web/rail_dynamic.inc)
 std::erase_if(stageHalos,[&](const StageHalo& h){return h.resource==resource;}); // the container goes with the entity
 stageNodeState.erase(resource);
 if(auto* st=stage_instance(resource))st->entity=StageNoEntity;
 stage_set_flags(resource,ssx::originalPickupRestoredFlags(stage_flags(resource)));
 if(any)stage_log_entity(resource,1);
}
static int stage_node_state(uint32_t resource){auto it=stageNodeState.find(resource);return it==stageNodeState.end()?0:it->second;}
// DeadNode 0x3506D8 (type 6) / Hide 0x350E90 (type 16) / RestoreNode 0x350F60 (type 19): flags &~0x60 &~2 |4, empty
// draw and contact. DeadNode and Hide are not listed for sections (0x34FB00); a RestoreNode is and its leave restores.
static void stage_make_node(uint32_t resource,int type){
 stageNodeState[resource]=type;
 if(auto* st=stage_instance(resource)){st->entity=StageNoopEntity;st->dead=type==6;st->restored=type==19;}
 stage_set_flags(resource,ssx::originalPickupRestorePendingFlags(stage_flags(resource)));
 if(type==19&&!stage_section_run)section_entity_created(resource);
}
// Entity kind at instance+0xC for 0x2FAE38's refusal test (types 6/9/22 refuse; 16/19 are node states).
static int stage_entity_type(uint32_t resource){if(int n=stage_node_state(resource))return n;return stage_entity_kind(resource);}
// builtin2 0x2FC420 (instance -1 current, mode): an existing entity other than DeadNode is destroyed (mode 3), then
// 0x2FC2C0: mode 0 DeadNode, 1 authored flags back, 3 RestoreNode.
static void stage_node_builtin(const StageArg* args,int count){
 std::array<uint32_t,2> f{0xffffffffu,0u};stage_parse(f,std::array<int32_t,2>{1,1},args,count);
 const uint32_t target=stage_target(f[0]);const int32_t mode=int32_t(f[1]);if(!target)return;
 const int type=stage_entity_type(target);
 if(type==6)return;                                   // 0x2FAE38 refuses a DeadNode
 if(mode!=0&&mode!=1&&mode!=3)return;
 if(mode==3&&type==19)return;
 if(type)stage_destroy_entity(target);
 if(mode==1)stage_set_flags(target,ssx::originalPickupRestoredFlags(stage_flags(target)));
 else stage_make_node(target,mode==0?6:19);
}
// builtin29 0x2FFB50 Hide (type 16) unless already 16 / 6; builtin58 0x2FFD58 Unhide destroys a Hide node.
static void stage_hide_builtin(const StageArg* args,int count,bool hide){
 std::array<uint32_t,1> f{0xffffffffu};stage_parse(f,std::array<int32_t,1>{1},args,count);
 const uint32_t target=stage_target(f[0]);if(!target)return;const int type=stage_entity_type(target);
 if(hide){if(type==16||type==6)return;if(type)stage_destroy_entity(target);stage_make_node(target,16);}
 else if(type==16)stage_destroy_entity(target);
}
// Node matrices of a section-started LiveComp (the collectible's spin + bob) at this tick. The section start built it in the
// original (0x341AA0 with the section's drawn word, on the entity matrix vt+0xC4: the magnet's) and every entity pass since
// ticked it (0x341D48); the browser draws it with a JS player (web/livecomp-animation.js), so the same player is rebuilt
// here from its start (builtin3 words, section log tick and word) and advanced to now. No draw, no shared state.
static bool stage_section_livecomp_nodes(uint32_t resource,const stage_world::Model& model,const stage_world::PMat& root,std::vector<stage_world::PMat>& out){
 const auto rec=stageSectionLiveComps.find(resource);if(rec==stageSectionLiveComps.end())return false;
 const uint32_t start=rec->second.second,now=stage_world_tick();if(now<start||now-start>216000u)return false;
 uint32_t draws=0,word=0;bool started=false; // the instance's latest section event must be that slot-1 start (a leave ends the player)
 for(size_t i=sectionLog.size();i>=6;i-=6){const uint32_t* e=&sectionLog[i-6];if(e[1]!=resource)continue;
  started=e[2]==uint32_t(ssx::sections::Action::Slot1)&&e[0]==start;draws=e[4];word=e[5];break;}
 if(!started)return false;
 ssx::OriginalLiveCompArguments a;a.words=rec->second.first;ssx::RollerMatrix m;for(unsigned r=0;r<4;r++)for(unsigned c=0;c<4;c++)m[r][c]=root[r][c];
 auto live=ssx::originalLiveCompConstruct(model.model,m,model.scale,model.flags,a,[&]{return draws?word:0u;}); // as the JS player: the drawn word
 for(uint32_t t=start;t<now;t++)ssx::originalLiveCompTick(live,true);
 const auto& nodes=ssx::originalLiveCompMatrices(live);out.assign(nodes.begin(),nodes.end());return true;
}
// builtin13 0x2FCFF0 MeshAnim (defaults 0x4FB5E8, key types 0x446060): key0 instance, 1 end mode, 2 gravity scale,
// 3 rider velocity share, 4 life s, 5..7 random speed ranges, 8..10 base velocity (cm/s), 11 max spin deg/s, 12 fade,
// 13 pose source instance, 14 its node.
static void stage_meshanim_builtin(const StageArg* args,int count){
 auto F=[](float x){return std::bit_cast<uint32_t>(x);};
 std::array<uint32_t,16> k{0xffffffffu,0u,F(std::bit_cast<float>(0x3dcccccdu)),F(1.f),F(2.f),F(600.f),F(600.f),F(300.f),F(0.f),F(0.f),F(0.f),F(360.f),F(-1.f),0xffffffffu,0xffffffffu,0u};
 stage_parse(k,std::array<int32_t,16>{1,1,2,2,2,2,2,2,2,2,2,2,2,1,1,0},args,count);
 const uint32_t target=stage_target(k[0]);if(!target||!stageWorldLoaded)return;
 const auto model=stageMeshModels.find(target);if(model==stageMeshModels.end())return;
 const int type=stage_entity_type(target);
 if(type==6||type==11)return;                        // DeadNode refuses; a running MeshAnim is not restarted (vt+0x12C)
 auto R=[&](unsigned i){return std::bit_cast<float>(k[i]);};
 terrain_original::Rounding rounding;using terrain_original::mul;using terrain_original::add;
 const float dt=std::bit_cast<float>(0x3C888889u),tps=60.f;
 // Pose base: the pose source's entity matrix / node (key13/key14, e.g. the collected pickup's LiveComp node 0), else
 // the old entity's matrix, else instance+0x10.
 const auto& m=model->second;const size_t n=m.local.size();
 stage_world::PMat base=stage_entity_matrix(target);bool fromNodes=false;std::vector<stage_world::PMat> srcNodes;
 const uint32_t source=k[13]!=0xffffffffu?k[13]:0u;
 if(source&&stage_entity_kind(source)){
  base=stage_entity_matrix(source);
  if(auto* e=stage_entity(source);e&&e->type==stage_world::LiveComp){const auto& nm=ssx::originalLiveCompMatrices(e->live);
   if(int32_t(k[14])>=0){if(size_t(k[14])<nm.size())base=nm[k[14]];}else{srcNodes.assign(nm.begin(),nm.end());fromNodes=srcNodes.size()>=n;}}
  else if(int32_t(k[14])>=0)if(auto mit=stageWorldModels.find(source);mit!=stageWorldModels.end()){
   // A section-started (JS-animated) LiveComp, e.g. the collectible: the pose its player has now (spin and bob phase,
   // stage_section_livecomp_nodes), else its node chain at the bind pose on the entity matrix (the magnet's).
   std::vector<stage_world::PMat> posed;
   if(stage_section_livecomp_nodes(source,*mit->second,base,posed)){if(size_t(k[14])<posed.size())base=posed[k[14]];}
   else{
   const auto& nodes=mit->second->model.nodes;std::vector<stage_world::PMat> w(nodes.size());
   for(size_t i=0;i<nodes.size();i++){stage_world::PMat l;for(unsigned r=0;r<4;r++)for(unsigned c=0;c<4;c++)l[r][c]=nodes[i].bind[r][c];
    for(unsigned c=0;c<3;c++)l[3][c]=mul(l[3][c],mit->second->scale);
    w[i]=ssx::set_piece_particles::mProduct(l,nodes[i].parent>=0&&size_t(nodes[i].parent)<i?w[size_t(nodes[i].parent)]:base);}
   if(size_t(k[14])<w.size())base=w[k[14]];
   }
  }
 }
 std::vector<stage_world::PMat> world(n);
 for(size_t i=0;i<n;i++){
  if(fromNodes){world[i]=srcNodes[i];continue;}
  stage_world::PMat local=m.local[i];for(unsigned c=0;c<3;c++)local[3][c]=mul(local[3][c],m.scale);
  world[i]=ssx::set_piece_particles::mProduct(local,m.parent[i]>=0&&size_t(m.parent[i])<i?world[size_t(m.parent[i])]:base);
 }
 if(type)stage_destroy_entity(target);               // the old entity (its dtor restores the flags first)
 spline_piece_replaced(target);                      // a moving piece's Object + Spline go too (R&B trainboxes on the train)
 auto& e=stage_new_entity(target,stage_world::Breaking);auto& a=e.mesh;
 a.endMode=int32_t(k[1]);a.life=R(4);a.fade=R(12);a.gravity=R(2);a.spinMax=mul(mul(R(11),std::bit_cast<float>(0x3c8efa36u)),dt);
 std::array<float,4> baseVel{R(8),R(9),R(10),0};
 if(stage_contact_record){ // + normalize(record direction) * |closing speed| * key3
  const auto& d=stage_contact_record->direction;const float len=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
  if(len>0)for(unsigned c=0;c<3;c++)baseVel[c]=add(baseVel[c],mul(mul(d[c]/len,std::fabs(stage_contact_record->closingSpeed)),R(3)));
 }
 a.q.resize(n);a.pos.resize(n);a.axis.resize(n);a.vel.resize(n);a.spin.resize(n);
 for(size_t i=0;i<n;i++){ // 0x31B748: translation row and rotation quaternion (0x31B7A8) of the node world matrix
  const auto& w=world[i];a.pos[i]={w[3][0],w[3][1],w[3][2],1.f};
  const float tr=w[0][0]+w[1][1]+w[2][2];std::array<float,4> q;
  if(tr>0){const float s=std::sqrt(tr+1.f)*2;q={(w[1][2]-w[2][1])/s,(w[2][0]-w[0][2])/s,(w[0][1]-w[1][0])/s,.25f*s};}
  else if(w[0][0]>w[1][1]&&w[0][0]>w[2][2]){const float s=std::sqrt(1.f+w[0][0]-w[1][1]-w[2][2])*2;q={.25f*s,(w[0][1]+w[1][0])/s,(w[2][0]+w[0][2])/s,(w[1][2]-w[2][1])/s};}
  else if(w[1][1]>w[2][2]){const float s=std::sqrt(1.f+w[1][1]-w[0][0]-w[2][2])*2;q={(w[0][1]+w[1][0])/s,.25f*s,(w[1][2]+w[2][1])/s,(w[2][0]-w[0][2])/s};}
  else{const float s=std::sqrt(1.f+w[2][2]-w[0][0]-w[1][1])*2;q={(w[2][0]+w[0][2])/s,(w[1][2]+w[2][1])/s,.25f*s,(w[0][1]-w[1][0])/s};}
  a.q[i]=q;
 }
 // instance flags: entity draw on, no collision while the pieces fly (static and entity routes cleared)
 stage_set_flags(target,((stage_flags(target)&~2u)|4u)&~0x60u);
 // 0x352230: 9 visual draws per node: [N x (spin, axis x3)] then [N x (|vx|, sign, |vy|, sign, |vz|)]
 auto& visual=stage_visual_random();auto unit=[&](uint32_t w){return std::bit_cast<float>((w&0x7FFFFFu)|0x3F800000u)-1.f;};
 for(size_t i=0;i<n;i++){
  a.spin[i]=mul(a.spinMax,add(mul(unit(visual.next()),.5f),.5f));
  std::array<float,3> ax{2*unit(visual.next())-1,2*unit(visual.next())-1,2*unit(visual.next())-1};
  const float l=std::sqrt(ax[0]*ax[0]+ax[1]*ax[1]+ax[2]*ax[2]);a.axis[i]={l>0?ax[0]/l:0.f,l>0?ax[1]/l:0.f,l>0?ax[2]/l:1.f,0};
 }
 for(size_t i=0;i<n;i++){
  float vx=R(5)*unit(visual.next())/tps;if(visual.next()&1)vx=-vx;
  float vy=R(6)*unit(visual.next())/tps;if(visual.next()&1)vy=-vy;
  const float vz=R(7)*unit(visual.next())/tps;
  a.vel[i]={add(mul(baseVel[0],dt),vx),add(mul(baseVel[1],dt),vy),add(mul(baseVel[2],dt),vz),0};
 }
 if(!stage_section_run)section_entity_created(target);
 stage_log_entity(target,2);
}
static void stage_meshanim_finish(stage_world::Entity& e){ // 0x352708: slot 4 if any, else by end mode
 auto* st=stage_instance(e.resource);
 if(st&&st->handler&&st->handler->slots[4]>=0){stage_run_slot(e.resource,4);return;}
 if(e.mesh.endMode==0){const uint32_t r=e.resource;stage_destroy_entity(r);stage_make_node(r,6);}
 else if(e.mesh.endMode==2)stage_destroy_entity(e.resource);
 // mode 1: an idle, undrawn entity until the section leave (collision stays cleared)
}
// 0x352500 per tick (EE sub.s with the operand alignment mask (PS2 capture: exact) / mul.s, VU0 ops in the original
// order): life -= frame dt (+0x14 of the timer at
// gp-0x2A74); per node q = normalize(q (x) (axis sin(spin/2), cos(spin/2))) (VOPMULA/VOPMSUB cross, VRSQRT), then
// position += velocity and velocity.z += gravity * -980 * dt.
static void stage_meshanim_update(stage_world::Entity& e){
 auto& a=e.mesh;const float dt=std::bit_cast<float>(0x3C888889u);
 terrain_original::Rounding rounding;using terrain_original::mul;using terrain_original::add;using terrain_original::sub;
 if(a.life>0){a.life=ssx::set_piece_particles::eeSub(a.life,dt);if(a.life<=0){a.finished=true;stage_log_entity(e.resource,3);stage_meshanim_finish(e);return;}}
 if(!(a.life>0))return;
 for(size_t i=0;i<a.q.size();i++){
  const auto sc=collision_scalar::sincos(mul(a.spin[i],.5f));const float s=sc[0],c=sc[1];
  const auto& q=a.q[i];const std::array<float,4> r{mul(s,a.axis[i][0]),mul(s,a.axis[i][1]),mul(s,a.axis[i][2]),c};
  const std::array<float,3> cross{sub(mul(q[1],r[2]),mul(r[1],q[2])),sub(mul(q[2],r[0]),mul(r[2],q[0])),sub(mul(q[0],r[1]),mul(r[0],q[1]))};
  std::array<float,4> o;
  for(unsigned k=0;k<3;k++)o[k]=add(add(mul(q[k],r[3]),mul(r[k],q[3])),cross[k]);
  o[3]=sub(sub(sub(mul(q[3],r[3]),mul(q[0],r[0])),mul(q[1],r[1])),mul(q[2],r[2]));
  const float len2=add(add(add(mul(o[0],o[0]),mul(o[1],o[1])),mul(o[2],o[2])),mul(o[3],o[3]));
  const float inv=len2>0?terrain_original::div(1.f,terrain_original::sqrt(len2)):std::bit_cast<float>(0x7F7FFFFFu);
  for(float& x:o)x=mul(x,inv);a.q[i]=o;
 }
 const float g=mul(mul(a.gravity,-980.f),dt);
 for(size_t i=0;i<a.q.size();i++){for(unsigned k=0;k<4;k++)a.pos[i][k]=add(a.pos[i][k],a.vel[i][k]);a.vel[i][2]=add(a.vel[i][2],g);}
}

// ---- RailModifier / AnimTeeter of the other locations (builtin48 0x2FF1C8, builtin6 0x2FB498; web/rail_dynamic.inc) ----
// builtin48 (defaults 0x4FBA28: instance -1 current, rail -1, node -1; key types 0x446470 {1,1,1}): only when the instance's
// entity answers vt+0x84 (Object 0x360DD0 / LiveComp: a Spline piece or a stage-world LiveComp here), 0x355E38 -> 0x35B708.
RIDER_LOCAL_LAZY static std::map<uint64_t,ssx::OriginalAnimNodeVelocityTable> stageRailVelocityTables; // entity+0x4C of the stage LiveComps with rails
static void stage_rail_builtin(const StageArg* args,int count){
 std::array<uint32_t,3> f{0xffffffffu,0xffffffffu,0xffffffffu};stage_parse(f,std::array<int32_t,3>{1,1,1},args,count);
 const uint32_t target=stage_target(f[0]);if(!target||int32_t(f[1])==-1)return;
 auto* w=set_piece_instance_at(target);if(!w)return;
 const std::array<float,3> low{w->low[0],w->low[1],w->low[2]},high{w->high[0],w->high[1],w->high[2]};
 if(auto* e=stage_entity(target);e&&e->type==stage_world::LiveComp&&e->model){ // a LiveComp built by the program (the crane)
  browser_dynamic_rail_bind(target,f[1],int32_t(f[2]),&e->model->model,e->model->matrix,e->model->scale,low,high);return;}
 for(const auto& p:browserSplinePieces)if(p.seed->resource==target){ // an Object + Spline piece (the train's boxcar)
  ssx::RollerMatrix m;for(unsigned i=0;i<16;i++)m[i/4][i%4]=w->authoredMatrix[i];
  if(int32_t(f[2])<0)browser_dynamic_rail_bind(target,f[1],-1,nullptr,m,w->scale,low,high);
  return;}
}
// Entity matrix / node world (0x34FED8) and entity bounds (vt+0x164/+0x16C: a primary modifier's) of a rail owner.
bool browser_dynamic_rail_entity(uint32_t owner,int32_t node,ssx::RollerMatrix* matrix,ssx::RollerQuad* boundsMin,ssx::RollerQuad* boundsMax){
 for(auto& p:browserSplinePieces)if(p.seed->resource==owner){
  if(matrix){if(node>=0)return false;*matrix=p.phase==2?ssx::originalPositionMatrix(p.position):ssx::originalSplineMatrix(p.spline);}
  if(boundsMin||boundsMax){const auto& lo=p.phase==2?p.position.boundsMin:p.spline.boundsMin;const auto& hi=p.phase==2?p.position.boundsMax:p.spline.boundsMax;
   if(boundsMin)*boundsMin={lo[0],lo[1],lo[2],lo[3]};if(boundsMax)*boundsMax={hi[0],hi[1],hi[2],hi[3]};}
  return true;
 }
 if(auto* e=stage_entity(owner);e&&e->type==stage_world::LiveComp){
  if(boundsMin||boundsMax)return false; // no primary modifier: 0x3569D0 returns 0
  if(!matrix||node<0)return false;
  const auto& nodes=ssx::originalLiveCompMatrices(e->live);if(size_t(node)>=nodes.size())return false;*matrix=nodes[size_t(node)];return true;
 }
 return false;
}
// Rail snap 0x106F78 entity vt+0x154: Object 0x356AE0 -> Spline vt+0xB4 0x359CF8; LiveComp 0x34E698 (node velocity).
bool browser_dynamic_rail_contact_velocity(uint32_t owner,int32_t node,const ssx::RollerQuad& point,ssx::RollerQuad& packet20,ssx::RollerQuad& packet30){
 for(auto& p:browserSplinePieces)if(p.seed->resource==owner){
  if(p.phase!=1)return false;
  const auto v=ssx::originalSplineContactVelocity(p.spline,point);ssx::originalSplineApplyContactVelocity(v,packet20,packet30);return v.applied;
 }
 if(auto* e=stage_entity(owner);e&&e->type==stage_world::LiveComp)
  return ssx::originalAnimContactVelocity(e->live.anim,ssx::originalLiveCompRate(e->live),stageRailVelocityTables[e->serial],node,point,packet20,packet30);
 return false;
}
// ---- MagnetModifier (builtin90 0x305478, engine/magnet_modifier.hpp) --------------------------------------------
// Created on an instance whose entity answers vt+0x84 (the builtin0 Object entity) from the entity matrix (instance+0x10
// without a main modifier); the instance takes the entity route (flags &~0x20 | 0x40) and the collision query is answered
// by the magnet's box (world_body_collision.hpp answerBox, filter 1 only). The first human contact acquires the rider;
// the slot-2 program runs on a contact after the pickup reached it (0x355770 gate before the contact guard).
static void stage_magnet_box(uint32_t resource,const ssx::OriginalMagnetModifier& m){
 if(auto* w=set_piece_instance_at(resource))w->answerBox=std::array<terrain_original::Vector,2>{terrain_original::Vector{m.boundsMin[0],m.boundsMin[1],m.boundsMin[2]},terrain_original::Vector{m.boundsMax[0],m.boundsMax[1],m.boundsMax[2]}};
}
static void stage_magnet_builtin(const StageArg* args,int count){
 std::vector<ssx::OriginalScriptKeyedArg> a;for(int i=0;i<count;i++)a.push_back({uint32_t(args[i].key),args[i].word0,uint32_t(args[i].type)});
 const auto k=ssx::originalMagnetDecodeArgs(a);const uint32_t target=stage_target(uint32_t(k.instance));
 auto* st=stage_instance(target);auto* w=set_piece_instance_at(target);
 if(!st||st->entity!=StageGateEntity||!w)return; // 0x3559F8: entity vt+0x84 (Object 0x360DD0 = 1) or nothing
 const auto pm=stage_instance_matrix(target);ssx::RollerMatrix em;for(unsigned r=0;r<4;r++)for(unsigned c=0;c<4;c++)em[r][c]=pm[r][c];
 uint32_t flags=stage_flags(target);
 auto m=ssx::originalMagnetCreate(k,em,target,{w->low[0],w->low[1],w->low[2]},{w->high[0],w->high[1],w->high[2]},flags);
 stage_set_flags(target,flags);stageMagnets[target]=m;stage_magnet_box(target,m);
}
// Entity pass (0x356198 container: modifier vt+0x14, then 0x3568B0 bounds): the target is the human rider (rider+0x110).
static void stage_magnet_tick(){
 for(auto& [r,m]:stageMagnets){
  if(m.target>=0)ssx::originalMagnetTick(m,{physicsState.position[0],physicsState.position[1],physicsState.position[2],1.f});
  else ssx::originalMagnetEntityBounds(m);
  stage_magnet_box(r,m);
 }
}
bool browser_stage_magnet_selected(uint32_t instance){return stageMagnets.count(instance)!=0;}
bool browser_stage_frozen_magnet(uint32_t instance){return std::find(stageFrozenMagnets.begin(),stageFrozenMagnets.end(),instance)!=stageFrozenMagnets.end();}
// 0x355770 gate for a contact of the local rider (index 0 human, else a computer rider's slot).
static bool stage_magnet_gate(uint32_t resource,bool human){
 auto it=stageMagnets.find(resource);if(it==stageMagnets.end())return true;
 return ssx::originalMagnetGate(it->second,human?0:1,human)!=0;
}
// 0x355F10 (the award's Debounce replaces the Object entity): the magnet matrix becomes the Debounce's PositionModifier;
// the pickup's model collision follows the entity matrix (entity route) until the Debounce ends.
static void stage_magnet_freeze(uint32_t resource){
 auto it=stageMagnets.find(resource);if(it==stageMagnets.end())return;
 const auto& M=ssx::originalMagnetMatrix(it->second);auto* w=set_piece_instance_at(resource);
 if(w){
  w->answerBox.reset();collision_transform::Matrix base;for(unsigned r=0;r<4;r++)for(unsigned c=0;c<4;c++)base[r*4+c]=M[r][c];
  try{
   terrain_original::Rounding rounding;terrain_original::Vector low,high;
   for(unsigned c=0;c<3;c++){const float d=terrain_original::sub(M[3][c],w->authoredMatrix[12+c]);low[c]=terrain_original::add(w->low[c],d);high[c]=terrain_original::add(w->high[c],d);}
   w->entity=std::make_shared<const ssx::WorldCollisionEntity>(ssx::composeWorldCollisionEntity(*w,base,low,high));
  }catch(const std::exception&){w->entity.reset();}
 }
 {stage_world::PMat f;for(unsigned r=0;r<4;r++)for(unsigned c=0;c<4;c++)f[r][c]=M[r][c];stageFrozenMagnetMatrix[resource]=f;}
 stageFrozenMagnets.push_back(resource);stageMagnets.erase(it);
}
// Renderer / QA: [count, then per magnet 10 floats: resource, translation (source cm) of the entity matrix, target rider
// index (-1 none), reached, time since acquisition (s), authored instance translation (instance+0x40)].
extern "C" EMSCRIPTEN_KEEPALIVE float* stage_world_magnets(){
 RIDER_LOCAL static std::vector<float> out;out.assign(1,0.f);
 for(auto& [r,m]:stageMagnets){const auto& M=ssx::originalMagnetMatrix(m);const auto rest=stage_instance_matrix(r);out.insert(out.end(),{float(r),M[3][0],M[3][1],M[3][2],float(m.target),float(m.reached),m.time,rest[3][0],rest[3][1],rest[3][2]});out[0]+=1;}
 return out.data();
}

// ---- one-way volumes (builtin7 0x2FBEC8, engine/one_way_volume.hpp) -----------------------------------------------
// The hub connectors' onewayvolume_* (slot 1: mode 2, 100 km/h, gain 0.9, armed after 1 s): a human rider in the volume
// (rider+0x5B8 of the previous tick) moving against its +Y is pushed back along +Y. Flags |= 0x100 (renderer list; the
// static route stays), no draw / contact. 2FAE38: DeadNode / type 9 / 0x16 or an existing Boost -> nothing, another
// entity is destroyed first.
RIDER_LOCAL extern std::array<uint32_t,64> browserRiderTriggers; // web/instance_contact_gameplay.inc
// rider+0x3FC: set by stage builtin 74 (0x303F80) for the contacting rider, cleared by 120F20 at the start of the rider pass, read by
// 2ED490 at 1218D0 after the stage triggers (web/race_bridge.cpp race_end -> browser_environment_selector_step).
RIDER_LOCAL static bool stageRiderTunnel=false;
bool browser_rider_tunnel(){return stageRiderTunnel;}
static void stage_boost_builtin(const StageArg* args,int count){
 std::vector<ssx::OriginalBoostKey> k;for(int i=0;i<count;i++)k.push_back({args[i].key,args[i].word0,args[i].type});
 const auto a=ssx::originalBoostArgs(k.data(),count);const uint32_t target=stage_target(uint32_t(a.instance));
 if(!target||!set_piece_instance_at(target))return;
 const int type=stage_entity_type(target);if(type==6||type==16||type==8)return; // node states refuse; a Boost re-applies (no-op)
 if(auto* e=stage_entity(target);e&&e->type==stage_world::Boost)return;
 if(type)stage_destroy_entity(target);
 const auto m=stage_instance_matrix(target);std::array<ssx::BoostQuad,4> rows;for(unsigned r=0;r<4;r++)rows[r]={m[r][0],m[r][1],m[r][2],m[r][3]};
 auto& e=stage_new_entity(target,stage_world::Boost);e.boost=ssx::originalBoostConstruct(a,rows);
 if(auto* st=stage_instance(target))if(st->entity==StageNoEntity)st->entity=StageNoopEntity; // contact 0x3609F0 / selected 0x360A00: no-ops
 stage_set_flags(target,stage_flags(target)|0x100u);
 if(!stage_section_run)section_entity_created(target);
}
// Capture seeds (web/peak-capture.mjs PEAK_SEED_BOOSTS): a one-way volume a PS2 savestate already holds (entity type 8, words
// +0x20..+0x40 = direction xyzw, speed, gain, duration, countdown, mode; instance+8 flags), which a mid-run seed's section state
// would otherwise leave as a plain entity that never pushes.
extern "C" EMSCRIPTEN_KEEPALIVE int stage_seed_boost(uint32_t resource,const float* w,uint32_t flags){
 if(!set_piece_instance_at(resource))return 0;
 if(auto* e=stage_entity(resource);e&&e->type==stage_world::Boost)return 1;
 auto& e=stage_new_entity(resource,stage_world::Boost);
 e.boost.direction={w[0],w[1],w[2],w[3]};e.boost.speed=w[4];e.boost.gain=w[5];
 e.boost.duration=std::bit_cast<int32_t>(w[6]);e.boost.countdown=std::bit_cast<int32_t>(w[7]);e.boost.mode=std::bit_cast<int32_t>(w[8]);
 if(auto* st=stage_instance(resource))if(st->entity==StageNoEntity)st->entity=StageNoopEntity;
 stage_set_flags(resource,flags);return 1;
}
// 0x1250A8 on this core's rider (and the air predictor restart 0x1135B8 when 11FE98 == 1).
static void stage_boost_push(const ssx::BoostQuad& dv){
 const int motion=npc_motion_mode();
 if(crash.active){const auto v=ssx::originalBoostAddVelocity({crash.actor.velocity[0],crash.actor.velocity[1],crash.actor.velocity[2],0.f},dv);crash.actor.velocity={v[0],v[1],v[2]};physicsState.velocity=crash.actor.velocity;}
 else{const auto v=ssx::originalBoostAddVelocity({physicsState.velocity[0],physicsState.velocity[1],physicsState.velocity[2],0.f},dv);physicsState.velocity={v[0],v[1],v[2]};
  if(animationPost.pending&&groundUnclampedVelocity)groundUnclampedVelocity=physicsState.velocity;}
 if(motion==1)begin_prediction({physicsState.position,physicsState.velocity});
 commit_rider_physics();
}
// 0x3415D0 (entity pass): mode 2 only for the human; the rider list is the one the previous tick's 105398 left.
static void stage_boost_update(stage_world::Entity& e){
 const auto step=ssx::originalBoostBegin(e.boost);
 if(step.runLoop&&!sharedWorldReplay&&(e.boost.mode!=2||browserHumanRider)&&ssx::originalRiderTouchesInstance(browserRiderTriggers,e.resource)){
  const auto dv=ssx::originalBoostPush(e.boost,{physicsState.velocity[0],physicsState.velocity[1],physicsState.velocity[2],0.f});
  if(dv){stage_boost_push(*dv);++e.pushes;}
 }
 // Mode 1's end (0x34FD00: nobody touched it after the countdown) never occurs on the disc (all calls are mode 2).
}

// QA: [count, then per Boost 10 words: resource, direction x y z w, speed, gain, duration, countdown, mode] (Boost +0x20..+0x40).
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* stage_world_boosts(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,0u);auto B=[](float x){return std::bit_cast<uint32_t>(x);};
 for(auto& e:stageEntities)if(e.alive&&e.type==stage_world::Boost){const auto& b=e.boost;++out[0];
  out.insert(out.end(),{e.resource,B(b.direction[0]),B(b.direction[1]),B(b.direction[2]),B(b.direction[3]),B(b.speed),B(b.gain),uint32_t(b.duration),uint32_t(b.countdown),uint32_t(b.mode)});}
 return out.data();
}
RIDER_LOCAL static std::vector<uint32_t> stageVisualLog; // QA: (tick, kind) 1 flag wind, 2 splash, 3 lightning, 4 crowd expiry, 5 flag grid build (0x4FF018 draws); 6 snowfall wind expiry (4 LCG draws)
static void stage_visual_log(uint32_t kind); // QA log (world visual pass)
// ---- CrowdMan2d (builtin88 0x303BA0; object *(gp-0x6F0)) --------------------------------------------------------
// Mode 0 (0x229820): the instance's crowd material slots take the shared animated record (the renderer animates world
// texture 9-161, web/crowd-2d.js); 1: nothing; 2 (0x229788): the first free of slots 0..63 gets the instance (no duplicate
// check: the race-start scan registers the load's areas a second time, as the PS2 at setpieces/full.tick418); 3
// (0x2297D8): the first slot holding it is freed. Camera flashes 0x229530 run in the world visual pass below.
struct StageCrowdFlash {uint32_t resource=0xffffffffu;float c1=0,c2=0;int32_t life=0;};
RIDER_LOCAL static std::array<uint32_t,128> stageCrowdSlots{},stageCrowdSlotsTick0{};
RIDER_LOCAL static std::array<int32_t,128> stageCrowdCountdown{},stageCrowdCountdownTick0{};
RIDER_LOCAL static std::array<StageCrowdFlash,40> stageCrowdRing{};RIDER_LOCAL static uint32_t stageCrowdCursor=0,stageCrowdCursorTick0=0;RIDER_LOCAL static int32_t stageCrowdCheer=0;
RIDER_LOCAL static bool stageCrowdLoaded=false;
RIDER_LOCAL static std::vector<uint32_t> stageCrowdAnimated; // mode-0 instances (their static crowd texture now animates)
static void stage_crowd_builtin(const StageArg* args,int count){
 std::array<uint32_t,4> k{0xffffffffu,0xffffffffu,0u,0u};stage_parse(k,std::array<int32_t,4>{1,1,1,1},args,count); /*defaults 0x4FBC80, types 0x4465D8*/
 const uint32_t target=stage_target(k[0]);if(!target)return;
 if(k[1]==0){if(std::find(stageCrowdAnimated.begin(),stageCrowdAnimated.end(),target)==stageCrowdAnimated.end())stageCrowdAnimated.push_back(target);}
 else if(k[1]==2){for(unsigned i=0;i<64;i++)if(stageCrowdSlots[i]==0xffffffffu){stageCrowdSlots[i]=target;break;}}
 else if(k[1]==3){for(auto& s:stageCrowdSlots)if(s==target){s=0xffffffffu;break;}}
}
// 0x2294C8 flash part: ring lives down, then 0x229530 per slot in index order: countdown -= 10*|cheer| + 4; on expiry c1,
// c2 (2*unit-1) and the re-arm 300 + r % 300 (divu), 3 visual draws.
static void stage_crowd_tick(ssx::OriginalRandomState& visual){
 for(auto& f:stageCrowdRing)if(f.life>0)--f.life;
 const int32_t d=10*std::abs(stageCrowdCheer)+4;terrain_original::Rounding rounding;
 auto unit=[&](uint32_t w){return originalScalarSubtract(std::bit_cast<float>((w&0x7FFFFFu)|0x3F800000u),1.f);};
 for(unsigned i=0;i<128;i++){
  if(stageCrowdSlots[i]==0xffffffffu)continue;
  stageCrowdCountdown[i]-=d;if(stageCrowdCountdown[i]>0)continue;
  const float c1=originalScalarSubtract(terrain_original::mul(2.f,unit(visual.next())),1.f),c2=originalScalarSubtract(terrain_original::mul(2.f,unit(visual.next())),1.f);
  stageCrowdRing[stageCrowdCursor]={stageCrowdSlots[i],c1,c2,3};stage_visual_log(4);stageCrowdCursor=(stageCrowdCursor+1)%40;
  stageCrowdCountdown[i]=int32_t(visual.next()%300u+300u);
 }
}
extern "C" EMSCRIPTEN_KEEPALIVE void set_stage_crowd_cheer(int32_t level){stageCrowdCheer=level;} // audio crowd record +0x18C (0x2A77C8)
// Renderer: [128 slots, animated-crowd count, resources..., then 40 ring entries (resource, c1, c2 bits, life)].
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* stage_world_crowd(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(stageCrowdSlots.begin(),stageCrowdSlots.end());
 out.push_back(uint32_t(stageCrowdAnimated.size()));out.insert(out.end(),stageCrowdAnimated.begin(),stageCrowdAnimated.end());
 for(const auto& f:stageCrowdRing){out.push_back(f.resource);out.push_back(std::bit_cast<uint32_t>(f.c1));out.push_back(std::bit_cast<uint32_t>(f.c2));out.push_back(uint32_t(f.life));}
 return out.data();
}

// ---- flag manager bookkeeping (0x34C548 / 0x34B168 / 0x34C668; the cloth itself animates in web/flag-animation.js) -----
// Slots keyed by cloth (flags.json: model + parameter block, the 0x34C548 memcmp key); a new slot builds its grid with
// four visual draws (0x34B228) inside the section pass; the manager's wind draws once per second (group 2). The drawn
// words are logged in order for the JS cloth (stage_world_flag_words), which consumes them in the same order.
struct StageFlagSlot {int32_t cloth=-1;std::vector<uint32_t> instances;};
RIDER_LOCAL static std::array<StageFlagSlot,15> stageFlagSlots,stageFlagSlotsTick0;RIDER_LOCAL_LAZY static std::map<uint32_t,int32_t> stageFlagCloth;
RIDER_LOCAL static ssx::OriginalFlagWind stageFlagWind,stageFlagWindTick0;RIDER_LOCAL static int stageFlagMode=1;RIDER_LOCAL static bool stageFlagsLoaded=false;
RIDER_LOCAL static std::vector<float> stageSnowWind,stageSnowWindTick0;RIDER_LOCAL static uint32_t stageSnowWindDraws=0; // snowfall wind layers (stage_snowfall_wind_tick)
RIDER_LOCAL static std::vector<uint32_t> stageFlagWords,stageFlagWordModes; // the drawn words and, per word, the wind mode of a wind draw (0: a grid build word)
// The wind mode 0x2D1BA0 reads EVERY update: course table 0x43D950 row [0x535C08] +0x54, plus 1 (2: amplitude 0.3, 3: 0.45, any
// other value 0.15; courses 0..21: Peak 1 1, Peak 2 2, Peak 3 3; row 32 holds 2). A course event's flags.json has its course's;
// a streamed world (init_streamed_flags: the free-ride flag manager, built at the world load with the 0x34C428 wind) follows
// the current course 0x535C08 (web/peak_world.inc peak_world_course, 22DF50 at a crossing).
RIDER_LOCAL static bool stageFlagStreamed=false;
static int stage_flag_course_mode(int course){
 static constexpr std::array<uint8_t,48> modes{1,1,2,2,3,1,2,3,1,2,3,1,2,3,1,2,3,1,1,2,2,3,1,1,1,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
 return course>=0&&course<48?modes[size_t(course)]:1;
}
extern "C" int peak_world_course(); // web/peak_world.inc (0x535C08)
static void stage_flag_register(uint32_t resource,ssx::OriginalRandomState& visual){
 auto it=stageFlagCloth.find(resource);if(it==stageFlagCloth.end())return;
 for(auto& s:stageFlagSlots)if(s.cloth==it->second&&!s.instances.empty()){if(std::find(s.instances.begin(),s.instances.end(),resource)==s.instances.end())s.instances.push_back(resource);return;}
 for(auto& s:stageFlagSlots)if(s.instances.empty()){s.cloth=it->second;s.instances.assign(1,resource);for(int k=0;k<4;k++){stageFlagWords.push_back(visual.next());stageFlagWordModes.push_back(0);}stage_visual_log(5);return;}
}
static void stage_flag_unregister(uint32_t resource){
 for(auto& s:stageFlagSlots)for(size_t k=0;k<s.instances.size();k++)if(s.instances[k]==resource){s.instances[k]=s.instances.back();s.instances.pop_back();if(s.instances.empty())s.cloth=-1;return;}
}
// builtin12 0x2FC9C8 (key 0 = the flag instance, -1 current): the flag entity registers with the manager.
static void stage_flag_builtin(const StageArg* args,int count){
 std::array<uint32_t,1> f{0xffffffffu};for(int i=0;i<count;i++)if(args[i].key==0)f[0]=args[i].word0;
 const uint32_t target=stage_target(f[0]);if(!target||!stageFlagsLoaded)return;
 stage_flag_register(target,stage_visual_random());
}
// Event-start visual seed (ready-state.json, tools/export_section_ready_state.py): the six 0x4FF018 words, the visual LCG
// gp+0xA0C and the human's RiderBreath 2E1120 state at the start of game tick 0. start_event applies it after its reset, so
// the countdown's breath births (1 LCG draw per breathing tick, 1 visual word per birth), trail, snow and world-pass draws
// follow the PS2 stream from the grid (docs/visual-rng-order.md).
RIDER_LOCAL static struct {bool set=false,breath=false;std::array<uint32_t,6> words{};uint32_t lcg=0;OriginalBreathState b{};} stageEventVisualSeed;
static bool stage_parse_breath(const nlohmann::json& d,OriginalBreathState& out){ // {"breath":{...}} or the human row of {"riders":[...]}
 const nlohmann::json* b=nullptr;if(d.contains("breath"))b=&d["breath"];else if(d.contains("riders"))for(const auto& r:d["riders"])if(r.value("human",false)){b=&r.at("breath");break;}
 if(!b)return false;out={b->at("accumulator").get<float>(),b->at("phase").get<int>(),b->at("effort").get<float>(),b->at("clock").get<float>(),b->at("duration").get<float>()};return true;
}
void stage_event_visual_seed_apply(){
 if(!stageEventVisualSeed.set)return;snowParticleRandom.words=stageEventVisualSeed.words;trailVisualRandom=stageEventVisualSeed.lcg;if(stageEventVisualSeed.breath)breathState=stageEventVisualSeed.b;
}
extern "C" EMSCRIPTEN_KEEPALIVE int init_stage_flags(const char* flagsText,const char* readyText){
 stageFlagCloth.clear();stageFlagSlotsTick0={};stageFlagsLoaded=false;stageFlagStreamed=false;
 if(!flagsText||!*flagsText)return 0;
 const auto flags=nlohmann::json::parse(flagsText);
 for(const auto& j:flags.at("instances"))stageFlagCloth[j.at("resource").get<uint32_t>()]=j.at("cloth").get<int32_t>();
 stageFlagMode=flags.at("wind").value("mode",1);
 const auto& w=flags.at("wind").at("initial");stageFlagWindTick0={w.at("wind").get<float>(),w.at("base").get<float>(),w.at("delta").get<float>(),w.at("timer").get<float>()};
 if(readyText&&*readyText){const auto ready=nlohmann::json::parse(readyText);
  if(ready.contains("flags")){const auto& rf=ready["flags"];
   if(rf.contains("wind_bits")){const auto& b=rf["wind_bits"];auto F=[&](const char* k){return std::bit_cast<float>(b.at(k).get<uint32_t>());};stageFlagWindTick0={F("wind"),F("base"),F("delta"),F("timer")};}
   for(const auto& sl:rf.at("slots")){const int k=sl.at("slot").get<int>();if(k<0||k>=15)continue;stageFlagSlotsTick0[size_t(k)].cloth=sl.at("cloth").get<int32_t>();stageFlagSlotsTick0[size_t(k)].instances=sl.at("instances").get<std::vector<uint32_t>>();}}
  stageSnowWindTick0.clear();if(ready.contains("snowfall_wind"))for(const auto& t:ready["snowfall_wind"].at("bits"))stageSnowWindTick0.push_back(std::bit_cast<float>(t.get<uint32_t>()));
  stageEventVisualSeed={};if(ready.contains("visual_rng_4ff018")&&ready.contains("visual_lcg_4a3afc")){stageEventVisualSeed.set=true;stageEventVisualSeed.words=ready["visual_rng_4ff018"].get<std::array<uint32_t,6>>();stageEventVisualSeed.lcg=ready["visual_lcg_4a3afc"].get<uint32_t>();stageEventVisualSeed.breath=stage_parse_breath(ready,stageEventVisualSeed.b);}}
 stageFlagsLoaded=true;stageFlagSlots=stageFlagSlotsTick0;stageFlagWind=stageFlagWindTick0;stageFlagWords.clear();stageFlagWordModes.clear();stageSnowWind=stageSnowWindTick0;
 return int(stageFlagCloth.size());
}
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* stage_world_visual_log(){RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,uint32_t(stageVisualLog.size()/2));out.insert(out.end(),stageVisualLog.begin(),stageVisualLog.end());return out.data();}
// Mid-run seed from a PS2 savestate (tools/export_world_visual_state.py): crowd timers and slots, flag manager wind and
// slots, the section list; the six visual words are the caller's (compare-ps2-capture --seed-visual-rng).
void section_seed_state(const std::vector<std::pair<uint32_t,bool>>& listed,int32_t lastScanAgo,const std::array<float,3>& last,bool parity);
extern "C" EMSCRIPTEN_KEEPALIVE int set_world_visual_state(const char* text){
 const auto d=nlohmann::json::parse(text);int applied=0;
 if(d.contains("crowd")&&!d["crowd"].is_null()){const auto& c=d["crowd"];const auto sl=c.at("slots").get<std::vector<uint32_t>>();const auto cd=c.at("countdowns").get<std::vector<int32_t>>();
  for(size_t i=0;i<128&&i<sl.size();i++)stageCrowdSlots[i]=sl[i];for(size_t i=0;i<128&&i<cd.size();i++)stageCrowdCountdown[i]=cd[i];stageCrowdCursor=c.value("cursor",0u)%40u;stageCrowdRing={};stageCrowdLoaded=true;applied|=1;}
 if(d.contains("flags")&&stageFlagsLoaded){const auto& f=d["flags"];
  if(f.contains("wind_bits")){const auto& b=f["wind_bits"];auto F=[&](const char* k){return std::bit_cast<float>(b.at(k).get<uint32_t>());};stageFlagWind={F("wind"),F("base"),F("delta"),F("timer")};}
  stageFlagSlots={};for(const auto& sl:f.at("slots")){const int k=sl.at("slot").get<int>();if(k<0||k>=15)continue;stageFlagSlots[size_t(k)].cloth=sl.at("cloth").get<int32_t>();stageFlagSlots[size_t(k)].instances=sl.at("instances").get<std::vector<uint32_t>>();}
  applied|=2;}
 if(d.contains("visual_lcg_4a3afc")){trailVisualRandom=d["visual_lcg_4a3afc"].get<uint32_t>();applied|=8;} // gp+0xA0C (web/animation_bridge.cpp)
 if(d.contains("snowfall_wind")){stageSnowWind.clear();for(const auto& t:d["snowfall_wind"].at("bits"))stageSnowWind.push_back(std::bit_cast<float>(t.get<uint32_t>()));applied|=32;}
 if(OriginalBreathState b;stage_parse_breath(d,b)){breathState=b;applied|=16;} // the human's RiderBreath (or {"breath"} for this core's rider)
 if(d.contains("weather")&&d["weather"].is_object()){weather_set_state(d["weather"]);applied|=64;} // web/weather.inc (tools/export_weather.py state)
 if(d.contains("sections")){const auto& s=d["sections"];std::vector<std::pair<uint32_t,bool>> listed;for(const auto& x:s.at("listed"))listed.emplace_back(x.at("resource").get<uint32_t>(),x.at("parity").get<int>()!=0);
  const int32_t last=s.at("last_scan").get<int32_t>(),tick=s.at("game_tick").get<int32_t>();const auto lb=s.at("last_bits").get<std::vector<uint32_t>>();
  section_seed_state(listed,last<0?-1:tick-last,{std::bit_cast<float>(lb.at(0)),std::bit_cast<float>(lb.at(1)),std::bit_cast<float>(lb.at(2))},s.at("parity").get<uint32_t>()!=0);applied|=4;}
 return applied;
}
// [count, then the visual words the flag manager drew this race, in order (4 per grid build, 1 per wind second)].
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* stage_world_flag_words(){RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,uint32_t(stageFlagWords.size()));out.insert(out.end(),stageFlagWords.begin(),stageFlagWords.end());out.insert(out.end(),stageFlagWordModes.begin(),stageFlagWordModes.end());return out.data();} // [count, words..., then per word its wind mode (0: grid build)]
// A streamed world's flag manager (web/peak-set-pieces.js, the merged SETPIECES/flags.json): no ready state, the 0x34C428 wind
// (0, base 0.5, delta 0.25, timer 0), no slots; its grid builds (section pass builtin12) and wind draw from the world's one
// visual stream like a course event's (docs/weather.md 10, docs/visual-rng-order.md), the mode following the current course.
// QA / PS2 captures (compare-ps2-capture.mjs --weather "flags"): the manager's wind +0x10 wind, +0x14 base, +0x18 delta, +0x1C timer, then the mode.
extern "C" EMSCRIPTEN_KEEPALIVE float* stage_flag_wind(){RIDER_LOCAL static std::array<float,5> v{};v={stageFlagWind.wind,stageFlagWind.base,stageFlagWind.delta,stageFlagWind.timer,float(stageFlagStreamed?stage_flag_course_mode(peak_world_course()):stageFlagMode)};return v.data();}
extern "C" EMSCRIPTEN_KEEPALIVE int init_streamed_flags(const char* flagsText){const int n=init_stage_flags(flagsText,nullptr);stageFlagStreamed=stageFlagsLoaded;return n;}

// ---- world visual pass: after every rider and the section pass, before the cameras (0x2306B8 steps 5..8) -----------
// Group 2: flag wind 0x34C668, camera splash 0x2F39E0 (one unconditional draw 0x2F3BE8: no drop or crystal spawns
// below snowfall 1.5 / impact snowfall 1.0, every course here); group 3: ScreenTint lightning 0x390C60 (one draw
// against chance^2 while the game flow is countdown/race, chance 0 on these courses: no strike); CrowdMan2d 0x229530.
extern "C" float* race_result_info(); // web/race_bridge.cpp
static void stage_visual_log(uint32_t kind){if(stageVisualLog.size()<2*200000)stageVisualLog.insert(stageVisualLog.end(),{stage_world_tick(),kind});}
RIDER_LOCAL static int32_t stageLightningFinishAge=-1;RIDER_LOCAL static uint32_t stageSplashDraws=0,stageLightningDraws=0;
// Snowfall 0x2E5DA0 (group 2, first; every course has one, vtable 0x487FD8): its wind layers (the non-null slots at
// +0x10..+0x3C, six on ARA1/BRA2/BHP1) each run 0x2E4F50: timer +0x90 -= 1/fps (60); below 0, four draws of the visual
// LCG gp+0xA0C (a new gust from the sine table 0x504FB8; the fourth sets +0x90 = +0x94 = (r - 1) * 4 + 1 s). The
// flakes need snowfall > 1.5 (not on these courses), so only the timers and the LCG draws are ported.
static void stage_snowfall_wind_tick(){
 terrain_original::Rounding rounding;const float step=originalScalarDivide(1.f,60.f); // 0x2E5030 div.s 1 / float(fps)
 auto next=[]{trailVisualRandom=((trailVisualRandom*0x18fcdu+0xe9507cu)&0x7fffffu)|0x3f800000u;return std::bit_cast<float>(trailVisualRandom);};
 for(auto& t:stageSnowWind){t=originalScalarSubtract(t,step);if(!(t<0.f))continue;
  next();next();next();t=originalScalarAdd(terrain_original::mul(originalScalarSubtract(next(),1.f),4.f),1.f);stageSnowWindDraws+=4;stage_visual_log(6);}
}
static void stage_world_visual_pass(){
 auto& visual=stage_visual_random();
 // The snowfall object 0x2E5DA0 (web/weather.inc: the camera's Weather record, 0x2F4330 into the splash, all six layers);
 // without a weather.json only its wind-layer timers (the LCG draws).
 if(!weather_group2_snowfall(trailVisualRandom))stage_snowfall_wind_tick();
 if(stageFlagsLoaded){const int mode=stageFlagStreamed?stage_flag_course_mode(peak_world_course()):stageFlagMode; // 0x2D1BA0, this update
  ssx::originalFlagWindTick(stageFlagWind,mode,60,[&]{const uint32_t w=visual.next();stageFlagWords.push_back(w);stageFlagWordModes.push_back(uint32_t(mode));stage_visual_log(1);return w;});}
 // Camera splash 0x2F39E0 (web/weather.inc: drops / ice crystals, spawns at snowfall > 1.5); else its one unconditional draw.
 if(!weather_group2_splash(trailVisualRandom,visual))visual.next();
 ++stageSplashDraws;stage_visual_log(2); // 0x2F3BE8
 // 0x151C = game-flow word S in {0, 1}, refreshed by 0x2F00A0 after the components: off from the second update after
 // the one after the finish update (PS2 setpieces/full trace: finish in update 12296, last lightning draw in update 12298).
 const bool finished=race_result_info()[0]!=0.f;if(finished&&stageLightningFinishAge<0)stageLightningFinishAge=0;else if(stageLightningFinishAge>=0)++stageLightningFinishAge;
 if(stageLightningFinishAge<3){const uint32_t d=weather_lightning_update(visual);stageLightningDraws+=d;if(d)stage_visual_log(3);} // 0x390C60: 0x390CA0 (+ 0x390CE0 on a strike; none while a flash runs)
 weather_group3_refresh(); // 0x2F00A0: the chance the next update reads (camera block property 10)
 if(stageCrowdLoaded)stage_crowd_tick(visual);
}

// ---- HaloModifier (builtin97) ----------------------------------------------------------------------------------
// Needs an entity whose vt+0x84 is 1 (Object / LiveComp / Debounce; not Hide or DeadNode); no conversion, no RNG.
static void stage_halo_builtin(const StageArg* args,int count){
 auto F=[](float x){return std::bit_cast<uint32_t>(x);};
 std::array<uint32_t,9> k{0xffffffffu,0u,0xffffffffu,F(1.f),F(1.f),F(1.f),F(1.f),F(100.f),0u};
 stage_parse(k,std::array<int32_t,9>{1,1,1,2,2,2,2,2,1},args,count); /*defaults 0x4FBD68, types 0x4466B0*/
 const uint32_t target=stage_target(k[0]);if(!target)return;
 const auto* st=stage_instance(target);const int node=stage_node_state(target);
 const bool entity=(st&&st->entity!=StageNoEntity)||stage_entity(target);
 if(!entity||node==6||node==16||(st&&st->dead))return;
 StageHalo h;h.resource=target;h.texture=int32_t(k[1]);h.node=int32_t(k[2]);
 h.a=std::bit_cast<float>(k[3]);h.r=std::bit_cast<float>(k[4]);h.g=std::bit_cast<float>(k[5]);h.b=std::bit_cast<float>(k[6]);h.size=std::bit_cast<float>(k[7]);h.spin=int32_t(k[8]);
 stageHalos.insert(stageHalos.begin(),h);
}
// 0x346258 (entity effect list, after the container and vt+0x7C): angle += spin; |angle| >= 360 -> 0 (not a wrap).
static void stage_halo_tick(){
 terrain_original::Rounding rounding;
 for(auto& h:stageHalos){h.angle=originalScalarAdd(h.angle,float(h.spin));if(360.f<=h.angle)h.angle=0.f;else if(h.angle<=-360.f)h.angle=0.f;}
}
// Renderer (0x3462A0 -> 0x2D1D10): [count, then per halo 12 floats: resource, texture key (FX 37 + key), node, R, G, B, A,
// half size cm, angle degrees, P (source cm) x, y, z; node >= 0 on a JS-animated LiveComp: P = NaN (the renderer takes the
// player's node world translation)].
extern "C" EMSCRIPTEN_KEEPALIVE float* stage_world_halos(){
 RIDER_LOCAL static std::vector<float> out;out.assign(1,0.f);
 for(const auto& h:stageHalos){
  std::array<float,3> P{NAN,NAN,NAN};
  if(h.node<0){ // entity vt+0xC4: the magnet's matrix, else the instance matrix
   if(auto it=stageMagnets.find(h.resource);it!=stageMagnets.end()){const auto& M=ssx::originalMagnetMatrix(it->second);P={M[3][0],M[3][1],M[3][2]};}
   else{const auto m=stage_entity_matrix(h.resource);P={m[3][0],m[3][1],m[3][2]};}
  }else if(auto* e=stage_entity(h.resource);e&&e->type==stage_world::LiveComp){const auto& nm=ssx::originalLiveCompMatrices(e->live);if(size_t(h.node)<nm.size())P={nm[h.node][3][0],nm[h.node][3][1],nm[h.node][3][2]};}
  out.insert(out.end(),{float(h.resource),float(h.texture),float(h.node),h.r,h.g,h.b,h.a,h.size,h.angle,P[0],P[1],P[2]});out[0]+=1;
 }
 return out.data();
}

// ---- LiveComp players (builtin3) ----------------------------------------------------------------------
static bool stage_section_timer_livecomp(uint32_t target,const std::array<uint32_t,11>& f){
 const auto* s=stage_instance(target);if(!s||!s->handler||(s->handler->slots[4]<0&&s->handler->slots[5]<0))return false;
 if(f[8]!=0||std::bit_cast<float>(f[6])!=0.f||!stageWorldModels.count(target)||attached_livecomp(target)||stage_entity(target))return false;
 return true;
}
static void stage_livecomp_builtin(const StageArg* args,int count){
 auto a=ssx::OriginalLiveCompArguments::defaults();std::array<uint32_t,11> f=a.words;
 stage_parse(f,std::array<int32_t,11>{1,1,1,2,2,2,2,2,1,1,1},args,count); /*key types 0x445F48*/
 const uint32_t target=stage_target(f[0]);
 if(auto* s=stage_instance(target))if(s->entity==StageNoEntity)s->entity=StageGateEntity; // instance+0xC = the LiveComp
 if(stageWorldLoaded&&stage_section_run&&stageWorldModels.count(target))stageSectionLiveComps[target]={f,stage_world_tick()}; // its pose for a MeshAnim (stage_section_livecomp_nodes)
 // A section-started LiveComp whose instance has a slot-4 / slot-5 program (Gravitude's crash billboards 332333 / 669741) is a
 // core entity too: the PS2's entity pass ticks it and runs its slot 5 every tick (0x34EBA0), and builtins 28 / 54 / 55 act on
 // it. The JS player still draws it (nothing is logged for it). Only a construct without draws (key8 0, key6 0) is built here.
 if(stageWorldLoaded&&stage_section_run&&stage_section_timer_livecomp(target,f)){
  auto& e=stage_new_entity(target,stage_world::LiveComp);e.model=stageWorldModels.at(target).get();e.startTick=stage_world_tick();e.sectionPlayer=true;
  a.words=f;e.live=ssx::originalLiveCompConstruct(e.model->model,e.model->matrix,e.model->scale,e.model->flags,a,[]()->uint32_t{throw std::runtime_error("Section LiveComp construct would draw");});
  stage_livecomp_collision();return;}
 if(!stageWorldLoaded||stage_section_run)return; // section starts: the section model draws, the JS players animate
 if(auto* x=attached_livecomp(target);x&&!x->active)return; // a spline LiveComp is built (and draws) with its spline piece: attached_launch at builtin19
 const int kind=stage_entity_kind(target);
 if(kind==1||kind==13)return; // 0x2FAE38: an existing LiveComp -> vt+0x12C 0x3609D8 (no-op); type 13 not converted here
 auto model=stageWorldModels.find(target);
 a.words=f;uint32_t words[2]={0,0};int drawn=0;
 auto random=[&]{const uint32_t w=browser_shared_random_next();if(drawn<2)words[drawn]=w;++drawn;return w;};
 if(model==stageWorldModels.end()){ // no model data: the draws still happen (0x341AA0 key8 then key6)
  if(f[8])random();if(std::bit_cast<float>(f[6])!=0.f)random();return;}
 // 0x341AA0's entity takes instance+0xC: a Hide (16) or RestoreNode (19) node there is replaced, so a later builtin29 Hide destroys
 // this LiveComp and the next builtin3 builds a fresh one (PS2 Schizophrenia timer 101655: program 98 hides it at 3544, the trigger's
 // re-contact at 3590 restarts it and its slot 5 relaunches the splines at 3609 / 3615).
 if(const int n=stage_node_state(target);n==16||n==19){stageNodeState.erase(target);shared_world_node_changed(target);}
 auto& e=stage_new_entity(target,stage_world::LiveComp);e.model=model->second.get();e.startTick=stage_world_tick();
 e.live=ssx::originalLiveCompConstruct(e.model->model,e.model->matrix,e.model->scale,e.model->flags,a,random);
 ++stageLiveCompStarts;if(!stage_section_run)section_entity_created(target);
 stage_livecomp_collision(); // instance+0xC is the LiveComp from here on
 if(stageLiveCompLog.size()<16*4096){stageLiveCompLog.push_back(stage_world_tick());stageLiveCompLog.push_back(target);stageLiveCompLog.insert(stageLiveCompLog.end(),f.begin(),f.end());
  stageLiveCompLog.push_back(words[0]);stageLiveCompLog.push_back(words[1]);stageLiveCompLog.push_back(uint32_t(drawn));}
}
// builtin55 0x3019C8 (instance -1 current, key1 time in 1/30 s): int(entity vt+0x104(time/30)): the LiveComp
// crossing 0x34EBE0; other entities 0x3609C8 (0); no entity / instance 0.
static StageArg stage_timer_gate(const StageArg* args,int count){
 std::array<uint32_t,2> f{0xffffffffu,std::bit_cast<uint32_t>(-1.f)};stage_parse(f,std::array<int32_t,2>{1,2},args,count);
 auto* e=stage_entity(stage_target(f[0]));if(!e||e->type!=stage_world::LiveComp)return stage_int(0);
 terrain_original::Rounding rounding;const float x=terrain_original::mul(std::bit_cast<float>(f[1]),std::bit_cast<float>(0x3d088889u)); /* gp-0x3368 1/30 */
 const float p=e->live.anim.previous,n=e->live.anim.unclamped;const float big=std::bit_cast<float>(0x501502f9u);
 if(p==big)return stage_int(0);
 return stage_int(p<n?(p<=x&&x<n):(n<=x&&x<p));
}
// builtin54 0x301680 (key0 instance, -1 current; types gp+0xB98): float entity vt+0xF0. A LiveComp answers its time x 30
// (0x361068: base+0x1C), the other entities 0 (0x361080); an instance without entity -1.
static StageArg stage_entity_time(const StageArg* args,int count){
 std::array<uint32_t,1> f{0xffffffffu};stage_parse(f,std::array<int32_t,1>{1},args,count);
 const uint32_t target=stage_target(f[0]);float v=-1.f;
 if(auto* e=target?stage_entity(target):nullptr;e&&e->alive){if(e->type==stage_world::LiveComp){terrain_original::Rounding rounding;v=terrain_original::mul(e->live.anim.time,30.f);}else v=0.f;}
 else if(stage_entity_kind(target)!=0)v=0.f;
 return {0,std::bit_cast<uint32_t>(v),0,2};
}
// builtin28 0x2FF9A8 (key0 instance -1 current, key1 property, key2 value; types 0x446540 {1, 1, 2}, defaults -1, -1, 0):
// entity vt+0x120. The LiveComp's is 0x341FE8 (seconds = value / 30, fps [[gp+0x2A74]+0x10] = 60): 0x64 delay = int(value x fps),
// 0x65 time = s when low <= s <= high, 0x66 rate = s / fps, 0x67 high = clamp(s, 0, length) (time down to it), 0x68 low = the same
// clamp (time up to it), 0x69 once mode; other properties go to the base (0x356B08, not modelled). Gravitude's billboard trigger
// (program 83) plays its billboard on to 149 / 30 s once, and the billboard's slot 5 (program 85) then breaks the ice pieces.
static void stage_livecomp_property(const StageArg* args,int count){
 std::array<uint32_t,3> f{0xffffffffu,0xffffffffu,0u};stage_parse(f,std::array<int32_t,3>{1,1,2},args,count);
 const uint32_t target=stage_target(f[0]);auto* e=target?stage_entity(target):nullptr;if(!e||!e->alive||e->type!=stage_world::LiveComp)return;
 terrain_original::Rounding rounding;using terrain_original::mul;auto& L=e->live;const float value=std::bit_cast<float>(f[2]),third=std::bit_cast<float>(0x3d088889u),fps=60.f;
 const float length=e->model?e->model->model.length:L.high;
 auto clampRange=[&](float s){if(length<s)s=length;if(s<0.f)s=0.f;return s;};
 switch(int32_t(f[1])){
  case 0x64:L.delay=int32_t(mul(value,fps));break;
  case 0x65:{const float s=mul(value,third);if(L.low<=s&&s<=L.high)L.anim.time=s;break;}
  case 0x66:L.rate=collision_scalar::divide(mul(value,third),fps);break;
  case 0x67:{const float s=clampRange(mul(value,third));L.high=s;if(s<L.anim.time)L.anim.time=s;break;}
  case 0x68:{const float s=clampRange(mul(value,third));L.low=s;if(L.anim.time<s)L.anim.time=s;break;}
  case 0x69:L.mode=0;break;
  default:break;
 }
}
// builtin30 0x2FFF00: sound request (the audio system plays it; web/game-audio.js).
static void stage_sound_builtin(const StageArg* args,int count){
 std::array<uint32_t,3> w{0xffffffffu,0xffffffffu,0xffffffffu};uint32_t mask=0;
 for(int i=0;i<count;i++)if(args[i].key>=0&&args[i].key<3){w[size_t(args[i].key)]=args[i].word0;mask|=1u<<args[i].key;}
 if(stageSoundLog.size()<7*4096)stageSoundLog.insert(stageSoundLog.end(),{stage_world_tick(),stageLastProgram,stage_current_instance,w[0],w[1],w[2],mask});
 audio_stage_sound(0,w,stage_current_instance); // 2974A0 (web/audio_events.inc)
}
// Builtins 31 (3000A8 -> 297950 loop) and 73 (300260 -> 297EB8 stop): the same keyed words, sound side only.
static void stage_sound_loop_builtin(const StageArg* args,int count,int kind){
 std::array<uint32_t,3> w{0xffffffffu,0xffffffffu,0xffffffffu};
 for(int i=0;i<count;i++)if(args[i].key>=0&&args[i].key<3)w[size_t(args[i].key)]=args[i].word0;
 audio_stage_sound(kind,w,stage_current_instance);
}

// ---- world pass ---------------------------------------------------------------------------------------
// Entity pass (race_begin, after the set-piece modifiers of this tick): newest entity first; per entity the
// LiveComp tick with its slot-5 program, then the effect list; a finished once-mode player runs slot 4.
static void stage_world_tick_entities(){
 if(!stageWorldLoaded)return;
 std::vector<uint64_t> order;for(auto& e:stageEntities)if(e.alive)order.push_back(e.serial);
 auto& visual=stage_visual_random();
 for(auto it=order.rbegin();it!=order.rend();++it){
  auto find=[&]{for(auto& e:stageEntities)if(e.serial==*it)return &e;return (stage_world::Entity*)nullptr;};
  auto* e=find();if(!e||!e->alive)continue;
  bool done=false;const uint32_t resource=e->resource;(void)done;
  if(e->type==stage_world::LiveComp){
   // 0x356198: vt+0x7C = 0x341D48 advances, runs slot 5 (0x34EBA0) and, on the tick the once-mode player finishes,
   // slot 4 (vt+0x110 0x341FC8 -> 0x34FCC0) and returns 0: the entity's effect list (0x355748) is not updated in that
   // tick (PS2 snapshots: every snow crumb trail skips exactly one birth when its player ends).
   const auto r=ssx::originalLiveCompTick(e->live,true);done=r==ssx::OriginalLiveCompTick::Done;
   if(r==ssx::OriginalLiveCompTick::Updated||done)stage_run_slot(resource,5);
   if(done){stage_run_slot(resource,4);continue;}
   e=find();if(!e||!e->alive)continue;
  }
  if(e->type==stage_world::Breaking){stage_meshanim_update(*e);e=find();if(!e||!e->alive)continue;}
  if(e->type==stage_world::Boost){stage_boost_update(*e);continue;}
  for(size_t k=0;k<e->effects.size();k++){
   auto& x=e->effects[k];
   if(!x.dynamic){ssx::originalParticleEffectUpdate(x.burst);continue;}
   if(x.trail.dead)continue;
   stage_world::PMat node;if(!stage_node_matrix(resource,x.trail.node,node))node=stage_entity_matrix(resource);
   ssx::originalDynamicParticleUpdate(x.trail,node,visual);
  }
 }
 const auto pending=std::move(stageSlot4Pending);stageSlot4Pending.clear();
 for(uint32_t r:pending)stage_run_slot(r,4); // spline pieces finished this tick: 0x352ED0 -> vt+0x114 0x34FD00
 stage_livecomp_collision(); // the LiveComp-animated collision nodes of this tick (0x334888 reads them at query time)
 browserRiderTriggers.fill(0xffffffffu); // 120F20 at the start of the rider pass
 stageRiderTunnel=false; // 120F20 clears rider+0x3FC too
}
// Streamed world (web/peak_world.inc): a location evicted from the world (3AB498 -> 3284B8, its instance memory freed)
// comes back from the disc without entities at its authored flags, so its instances' stage state is dropped.
void browser_stage_track_reset(uint32_t track){
 for(auto it=stageFlagsCountdown.begin();it!=stageFlagsCountdown.end();){if((it->first&255u)!=track){++it;continue;}if(auto* w=set_piece_instance_at(it->first))w->eventRuntimeFlags=it->second;it=stageFlagsCountdown.erase(it);}
 std::erase_if(stageFlags,[track](const auto& p){return (p.first&255u)==track;});std::erase_if(stageNodeState,[track](const auto& p){return (p.first&255u)==track;});std::erase_if(stageSectionLiveComps,[track](const auto& p){return (p.first&255u)==track;});
 for(auto& e:stageEntities)if((e.resource&255u)==track)e.alive=false;
 for(auto& s:stageInstances)if((s.resource&255u)==track){const auto* h=s.handler;const uint32_t r=s.resource;s=StageInstance{};s.resource=r;s.handler=h;}
 void browser_avalanche_track_reset(uint32_t);browser_avalanche_track_reset(track);
}
// Mid-run seed of a streamed world (web/peak-capture.mjs from tools/export_peak_seed.py): the node entities (6 DeadNode,
// 16 Hide, 19 RestoreNode) and instance flags a PS2 savestate holds.
void browser_stage_seed_node(uint32_t resource,int type,uint32_t flags){
 stageNodeState[resource]=type;if(!stageFlagsCountdown.count(resource)){auto* w=set_piece_instance_at(resource);stageFlagsCountdown[resource]=w?w->eventRuntimeFlags:std::nullopt;}
 stageFlags[resource]=flags;if(auto* w=set_piece_instance_at(resource))w->eventRuntimeFlags=flags;
 if(auto* st=stage_instance(resource)){st->entity=StageNoopEntity;st->dead=type==6;st->restored=type==19;st->flags=flags;st->flagsKnown=true;}
}
static std::vector<stage_world::Entity> stage_cutscene_kept();static void stage_cutscene_restore(std::vector<stage_world::Entity>&); // web/presentation_core.inc
static void stage_world_reset(){
 auto keptCutscene=stage_cutscene_kept(); // the new-career spray (a cutscene call's Particle) lives on through the run start (PS2 new-career s3276..3330)
 for(const auto& [r,f]:stageFlagsCountdown)if(auto* w=set_piece_instance_at(r))w->eventRuntimeFlags=f; // race restart: the countdown flags
 stageFlagsCountdown.clear();stageFlags.clear();stageNodeState.clear();stageSectionLiveComps.clear();stageEntityLog.clear();stage_contact_record.reset();
 stageEntities.clear();stageEntitySerial=0;stage_cutscene_restore(keptCutscene);stageLiveCompLog.clear();stageSoundLog.clear();stageWorldSlotLog.clear();stageSlot4Pending.clear();
 stage_livecomp_collision(); // a new race: every LiveComp-animated instance back at its authored nodes
 stageParticleCreates=stageParticleRefused=stageLiveCompStarts=0;
 stageWorldVisualOwn=stageWorldVisualTick0;
 const bool stageHere=stageWorldLoaded&&browser_same_world(stageWorldLocation,setPieceLocationName);
 stageMultiGroups=stageHere?stageMultiInitial:decltype(stageMultiInitial){};
 for(auto& [r,m]:stageMagnets)if(auto* w=set_piece_instance_at(r))w->answerBox.reset();
 for(uint32_t r:stageFrozenMagnets)if(auto* w=set_piece_instance_at(r))w->entity.reset();
 stageMagnets.clear();stageFrozenMagnets.clear();stageFrozenMagnetMatrix.clear();stageHalos.clear();
 stageCrowdSlots=stageCrowdLoaded&&stageHere?stageCrowdSlotsTick0:[]{std::array<uint32_t,128> a;a.fill(0xffffffffu);return a;}();
 stageCrowdCountdown=stageCrowdCountdownTick0;stageCrowdRing={};stageCrowdCursor=stageCrowdCursorTick0;stageCrowdAnimated.clear();
 stageFlagSlots=stageFlagSlotsTick0;stageFlagWind=stageFlagWindTick0;stageFlagWords.clear();stageFlagWordModes.clear();stageSnowWind=stageSnowWindTick0;stageSnowWindDraws=0;
 stageLightningFinishAge=-1;stageSplashDraws=stageLightningDraws=0;stageVisualLog.clear();
 if(stageHere)stageHalos=stageHaloInitial;
 if(stageHere)for(const auto& i:stageMagnetInitial){ // race tick 0: the magnets the load built (BHP1 pointa / pointc slot 1)
  if(i.words.size()*4<0xB0)continue;auto m=ssx::originalMagnetModifierFromBytes(reinterpret_cast<const uint8_t*>(i.words.data()));
  stageMagnets[i.resource]=m;stage_set_flags(i.resource,i.flags);stage_magnet_box(i.resource,m); // entity route (flags | 0x40)
 }
 if(!stageWorldLoaded||!browser_same_world(stageWorldLocation,setPieceLocationName))return;
 for(const auto& b:stageBoostInitial){ // race tick 0: one-way volumes (builtin7 Boost) the load built, their ready-savestate words
  if(!set_piece_instance_at(b.resource))continue;auto F=[](uint32_t w){return std::bit_cast<float>(w);};
  auto& e=stage_new_entity(b.resource,stage_world::Boost);
  e.boost.direction={F(b.words[0]),F(b.words[1]),F(b.words[2]),F(b.words[3])};e.boost.speed=F(b.words[4]);e.boost.gain=F(b.words[5]);
  e.boost.duration=int32_t(b.words[6]);e.boost.countdown=int32_t(b.words[7]);e.boost.mode=int32_t(b.words[8]);
  if(auto* st=stage_instance(b.resource))if(st->entity==StageNoEntity)st->entity=StageNoopEntity;
 }
 for(const auto& i:stageWorldInitial){ // race tick 0 (ready savestate): emitters built during the load
  auto* e=stage_entity(i.resource);if(!e){e=&stage_new_entity(i.resource,stage_world::Emitter);e->matrix=i.matrix;}
  stage_world::Effect x;
  if(!i.dynamic){x.burst.matrix=i.matrix;for(size_t o=0;o<i.emitter.size()&&o<0x190/4;o++)x.burst.emitter.setU(unsigned(o*4),i.emitter[o]);x.burst.dead=i.dead;
   for(size_t o=0;o<i.block.size()&&o<0xE0/4;o++)x.burst.params.setU(unsigned(o*4),i.block[o]);}
  e->effects.push_back(std::move(x)); // memory order of the export = list order
 }
}
// A section leave destroyed the instance's entity (0x34FD90): its effects and player go with it.
static void stage_world_section_leave(uint32_t resource){
 stage_flag_unregister(resource); // 0x34C600 -> 0x34B168 (flag entity dtor)
 std::erase_if(stageHalos,[&](const StageHalo& h){return h.resource==resource;}); // 0x34FD90: the entity and its container
 if(auto it=stageMagnets.find(resource);it!=stageMagnets.end()){ // pickup Object / collectible LiveComp: flags back (high half | 2), no entity
  if(auto* w=set_piece_instance_at(resource))w->answerBox.reset();stageMagnets.erase(it);
  if(auto* st=stage_instance(resource))st->entity=StageNoEntity;stage_set_flags(resource,ssx::originalPickupRestoredFlags(stage_flags(resource)));
 }
 auto* e=stage_entity(resource);
 if(e&&e->type==stage_world::Breaking){ // 0x352780: mode 0 -> DeadNode, else authored flags back and destroyed
  if(e->mesh.endMode==0){stage_destroy_entity(resource);stage_make_node(resource,6);}
  else stage_destroy_entity(resource);
  return;
 }
 if(stage_node_state(resource)==19){stage_destroy_entity(resource);return;} // RestoreNode: the object comes back
 if(e)stage_destroy_entity(resource);
}

// ---- data ---------------------------------------------------------------------------------------------
static stage_world::PMat stage_rows(const nlohmann::json& a){stage_world::PMat m;for(unsigned i=0;i<16;i++)m[i/4][i%4]=a.at(i).get<float>();return m;}
static stage_world::PMat stage_row_bits(const nlohmann::json& a){stage_world::PMat m;for(unsigned i=0;i<16;i++)m[i/4][i%4]=std::bit_cast<float>(a.at(i).get<uint32_t>());return m;}
// One element of each section of the three documents (particles.json, livecomp.json, stage-world.json): shared by init_stage_world
// (the whole documents in one call) and stage_world_part (pv sliceLoad, web/load-slices.js: the same elements in the same order,
// fed in parts so no load-screen call takes long; every container is a std::map by resource or a vector filled in document order).
static void stage_meshanim(const nlohmann::json& j){
 stage_world::MeshModel m;m.resource=j.at("resource").get<uint32_t>();m.matrix=stage_row_bits(j.at("matrix_bits"));m.scale=std::bit_cast<float>(j.at("scale_bits").get<uint32_t>());
 for(const auto& n:j.at("nodes")){m.parent.push_back(n.at("parent").get<int32_t>());m.local.push_back(stage_row_bits(n.at("local_bits")));}
 stageMeshModels[m.resource]=std::move(m);
}
static void stage_teleport(const nlohmann::json& j){stageTeleports[j.at("resource").get<uint32_t>()]=j.at("matrix_bits").get<std::array<uint32_t,16>>();}
static void stage_script_flags(const nlohmann::json& j){if(j.contains("runtime_flags")&&j["runtime_flags"].is_number_unsigned())stageScriptCountdownFlags[j.at("resource").get<uint32_t>()]=j["runtime_flags"].get<uint32_t>();}
static void stage_particle_instance(const std::string& key,const nlohmann::json& j){
 const uint32_t r=uint32_t(std::stoul(key));
 stageWorldMatrices[r]=j.contains("matrix_bits")?stage_row_bits(j["matrix_bits"]):stage_rows(j.at("matrix"));
 if(j.contains("scale_bits"))stageWorldScales[r]=std::bit_cast<float>(j["scale_bits"].get<uint32_t>());
}
static void stage_carrier(const std::string& key,const nlohmann::json& j){
 stage_world::Carrier c;c.scale=std::bit_cast<float>(j.at("scale_bits").get<uint32_t>());
 for(auto n=j.at("chains").begin();n!=j.at("chains").end();++n){auto& chain=c.chains[std::stoi(n.key())];for(const auto& m:n.value())chain.push_back(stage_row_bits(m));}
 stageWorldCarriers[uint32_t(std::stoul(key))]=std::move(c);
}
static void stage_initial_effect(const nlohmann::json& j){
 if(j.at("kind").get<std::string>()!="Particle")return; // no DynamicParticle is alive at race tick 0 on the three courses
 stage_world::Initial i;i.resource=j.at("resource").get<uint32_t>();i.matrix=stage_row_bits(j.at("matrix"));i.dead=j.at("dead").get<uint32_t>();
 i.emitter=j.at("emitter").get<std::vector<uint32_t>>();if(j.contains("block"))i.block=j["block"].get<std::vector<uint32_t>>();stageWorldInitial.push_back(std::move(i));
}
static void stage_crowd(const nlohmann::json& c){
 const auto sl=c.at("slots").get<std::vector<uint32_t>>();const auto cd=c.at("countdowns").get<std::vector<int32_t>>();
 for(size_t i=0;i<128&&i<sl.size();i++)stageCrowdSlotsTick0[i]=sl[i];for(size_t i=0;i<128&&i<cd.size();i++)stageCrowdCountdownTick0[i]=cd[i];
 stageCrowdCursorTick0=c.value("cursor",0u)%40u;stageCrowdLoaded=true;
}
static void stage_boost(const nlohmann::json& j){StageBoostInitial b;b.resource=j.at("resource").get<uint32_t>();const auto w=j.at("words").get<std::vector<uint32_t>>();for(size_t k=0;k<9&&k<w.size();k++)b.words[k]=w[k];stageBoostInitial.push_back(b);}
static void stage_halo(const nlohmann::json& j){
 StageHalo h;h.resource=j.at("resource").get<uint32_t>();h.texture=j.at("texture").get<int32_t>();h.node=j.at("node").get<int32_t>();h.spin=j.at("spin").get<int32_t>();
 h.r=j.at("r").get<float>();h.g=j.at("g").get<float>();h.b=j.at("b").get<float>();h.a=j.at("a").get<float>();h.size=j.at("size").get<float>();h.angle=std::bit_cast<float>(j.at("angle_bits").get<uint32_t>());
 stageHaloInitial.push_back(h);
}
static void stage_magnet(const nlohmann::json& j){stageMagnetInitial.push_back({j.at("resource").get<uint32_t>(),j.at("instance_flags").get<uint32_t>(),j.at("bytes").get<std::vector<uint32_t>>()});}
static void stage_multi(const nlohmann::json& j){
 const int g=j.at("group").get<int>();if(g<0||g>3)return;auto& m=stageMultiInitial[size_t(g)];m.used=true;m.capacity=j.at("capacity").get<uint32_t>();
 m.members=j.at("members").get<std::vector<uint32_t>>();const auto w=j.at("emitter").get<std::vector<uint32_t>>();
 for(size_t o=0;o<w.size()&&o<0x190/4;o++)m.emitter.setU(unsigned(o*4),w[o]);
 const auto pb=j.at("position_base").get<std::vector<uint32_t>>();for(unsigned k=0;k<4&&k<pb.size();k++)m.positionBase[k]=std::bit_cast<float>(pb[k]);
}
static void stage_livecomp(const nlohmann::json& j){
 auto m=std::make_unique<stage_world::Model>();m->resource=j.at("resource").get<uint32_t>();m->name=j.value("name","");
 m->model.length=j.at("length").get<float>();m->scale=j.value("scale",1.f);m->flags=0;
 for(unsigned i=0;i<16;i++)m->matrix[i/4][i%4]=j.at("matrix").at(i).get<float>();
 for(const auto& n:j.at("nodes")){
  ssx::OriginalAnimNode node;node.parent=n.at("parent").get<int32_t>();for(unsigned i=0;i<16;i++)node.bind[i/4][i%4]=n.at("bind").at(i).get<float>();
  if(n.contains("track")&&!n["track"].is_null()){const auto& t=n["track"];ssx::OriginalAnimTrack tr;for(unsigned k=0;k<6;k++)tr.base[k]=t.at("base").at(k).get<float>();tr.mask=t.at("mask").get<uint32_t>();
   for(const auto& c:t.at("curves")){std::vector<ssx::OriginalAnimSegment> s;for(const auto& g:c)s.push_back({g[0].get<float>(),g[1].get<float>(),g[2].get<float>(),g[3].get<float>(),g[4].get<float>(),g[5].get<float>()});tr.curves.push_back(std::move(s));}
   node.track=std::move(tr);}
  m->model.nodes.push_back(std::move(node));
 }
 if(!stageWorldMatrices.count(m->resource))stageWorldMatrices[m->resource]=m->matrix; // after every particles instance (the documents' order)
 stageWorldModels[m->resource]=std::move(m);
}
// The state init_stage_world clears before it reads (stage: a stage-world document is given; its script flags are cleared only then).
static void stage_world_load_clear(bool stage){
 stageMeshModels.clear();stageTeleports.clear();if(stage)stageScriptCountdownFlags.clear();
 stageWorldModels.clear();stageWorldMatrices.clear();stageWorldScales.clear();stageWorldCarriers.clear();stageWorldInitial.clear();stageWorldLoaded=false;
 stageCrowdLoaded=false;stageCrowdSlotsTick0.fill(0xffffffffu);stageCrowdCountdownTick0.fill(0);stageCrowdCursorTick0=0;
 stageMagnetInitial.clear();stageHaloInitial.clear();stageBoostInitial.clear();stageMultiInitial={};stageMultiExported=false;
}
static int stage_world_load_end(){stageWorldLoaded=true;stage_world_reset();return int(stageWorldModels.size()+stageWorldMatrices.size());}
extern "C" EMSCRIPTEN_KEEPALIVE int init_stage_world(const char* particlesText,const char* liveCompText,const char* stageText){
 const bool stage=stageText&&*stageText;
 stage_world_load_clear(stage);
 if(stage){
  const auto doc=nlohmann::json::parse(stageText);
  for(const auto& j:doc.at("meshanim"))stage_meshanim(j);
  for(const auto& j:doc.at("teleports"))stage_teleport(j);
  if(doc.contains("script"))for(const auto& j:doc["script"])stage_script_flags(j);
 }
 const auto particles=nlohmann::json::parse(particlesText);
 stageWorldLocation=particles.at("location").get<std::string>();
 for(auto it=particles.at("instances").begin();it!=particles.at("instances").end();++it)stage_particle_instance(it.key(),it.value());
 if(particles.contains("carriers"))for(auto it=particles["carriers"].begin();it!=particles["carriers"].end();++it)stage_carrier(it.key(),it.value());
 const auto& ini=particles.at("initial");
 stageWorldVisualTick0.words=ini.at("visual_random_0x4FF018").get<std::array<uint32_t,6>>();
 for(const auto& j:ini.at("effects"))stage_initial_effect(j);
 if(particles.contains("crowd")&&!particles["crowd"].is_null())stage_crowd(particles["crowd"]);
 if(particles.contains("boosts"))for(const auto& j:particles["boosts"])stage_boost(j);
 if(particles.contains("halos"))for(const auto& j:particles["halos"])stage_halo(j);
 if(particles.contains("magnets"))for(const auto& j:particles["magnets"])stage_magnet(j);
 stageMultiExported=particles.contains("multi");
 if(stageMultiExported)for(const auto& j:particles["multi"])stage_multi(j);
 if(liveCompText&&*liveCompText){
  const auto live=nlohmann::json::parse(liveCompText);
  for(const auto& j:live.at("instances"))stage_livecomp(j);
 }
 return stage_world_load_end();
}
// pv sliceLoad (web/load-slices.js): init_stage_world in parts, in the documents' order. kind 0 begin (text "1" when a stage-world
// document follows: its script flags are cleared), 1 a stage-world part {meshanim?, teleports?, script?}, 2 a particles part {location?,
// instances?, carriers?, initial? {visual_random_0x4FF018, effects}, crowd?, boosts?, halos?, magnets?, multi?}, 3 a livecomp part
// {instances}, 4 end (the count init_stage_world returns). The parts of a document come whole and in order: every particles part before
// the first livecomp part.
extern "C" EMSCRIPTEN_KEEPALIVE int stage_world_part(int kind,const char* text){
 if(kind==0){stage_world_load_clear(text&&text[0]=='1');return 0;}
 if(kind==4)return stage_world_load_end();
 const auto doc=nlohmann::json::parse(text);
 if(kind==1){
  if(doc.contains("meshanim"))for(const auto& j:doc["meshanim"])stage_meshanim(j);
  if(doc.contains("teleports"))for(const auto& j:doc["teleports"])stage_teleport(j);
  if(doc.contains("script"))for(const auto& j:doc["script"])stage_script_flags(j);
 }else if(kind==2){
  if(doc.contains("location"))stageWorldLocation=doc["location"].get<std::string>();
  if(doc.contains("instances"))for(auto it=doc["instances"].begin();it!=doc["instances"].end();++it)stage_particle_instance(it.key(),it.value());
  if(doc.contains("carriers"))for(auto it=doc["carriers"].begin();it!=doc["carriers"].end();++it)stage_carrier(it.key(),it.value());
  if(doc.contains("initial")){const auto& ini=doc["initial"];if(ini.contains("visual_random_0x4FF018"))stageWorldVisualTick0.words=ini["visual_random_0x4FF018"].get<std::array<uint32_t,6>>();if(ini.contains("effects"))for(const auto& j:ini["effects"])stage_initial_effect(j);}
  if(doc.contains("crowd")&&!doc["crowd"].is_null())stage_crowd(doc["crowd"]);
  if(doc.contains("boosts"))for(const auto& j:doc["boosts"])stage_boost(j);
  if(doc.contains("halos"))for(const auto& j:doc["halos"])stage_halo(j);
  if(doc.contains("magnets"))for(const auto& j:doc["magnets"])stage_magnet(j);
  if(doc.contains("multi")){stageMultiExported=true;for(const auto& j:doc["multi"])stage_multi(j);}
 }else if(kind==3){
  if(doc.contains("instances"))for(const auto& j:doc["instances"])stage_livecomp(j);
 }else throw std::runtime_error("Unknown stage world part");
 return 0;
}
// Tests (web/test-ctm-stream.mjs): FNV-1a over everything a load puts in (init_stage_world == stage_world_part, bit for bit).
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t stage_world_load_hash(){
 uint32_t h=0x811c9dc5u;auto mix=[&](const void* p,size_t n){const auto* b=static_cast<const uint8_t*>(p);for(size_t i=0;i<n;++i){h^=b[i];h*=0x01000193u;}};
 auto u=[&](uint32_t v){mix(&v,4);};auto f=[&](float v){mix(&v,4);};auto mat=[&](const auto& m){mix(m.data(),sizeof(m));};
 u(uint32_t(stageMeshModels.size()));for(const auto& [r,m]:stageMeshModels){u(r);u(m.resource);mat(m.matrix);f(m.scale);for(auto p:m.parent)u(uint32_t(p));for(const auto& l:m.local)mat(l);}
 u(uint32_t(stageTeleports.size()));for(const auto& [r,t]:stageTeleports){u(r);mix(t.data(),sizeof(t));}
 u(uint32_t(stageScriptCountdownFlags.size()));for(const auto& [r,v]:stageScriptCountdownFlags){u(r);u(v);}
 u(uint32_t(stageWorldModels.size()));for(const auto& [r,m]:stageWorldModels){u(r);u(m->resource);mix(m->name.data(),m->name.size());f(m->model.length);f(m->scale);u(m->flags);mat(m->matrix);
  for(const auto& n:m->model.nodes){u(uint32_t(n.parent));mat(n.bind);u(n.track?1u:0u);if(n.track){mix(n.track->base.data(),sizeof(n.track->base));u(n.track->mask);for(const auto& c:n.track->curves){u(uint32_t(c.size()));for(const auto& g:c){f(g.a);f(g.b);f(g.c);f(g.d);f(g.t0);f(g.t1);}}}}}
 u(uint32_t(stageWorldMatrices.size()));for(const auto& [r,m]:stageWorldMatrices){u(r);mat(m);}
 u(uint32_t(stageWorldScales.size()));for(const auto& [r,v]:stageWorldScales){u(r);f(v);}
 u(uint32_t(stageWorldCarriers.size()));for(const auto& [r,c]:stageWorldCarriers){u(r);f(c.scale);for(const auto& [k,chain]:c.chains){u(uint32_t(k));for(const auto& m:chain)mat(m);}}
 u(uint32_t(stageWorldInitial.size()));for(const auto& i:stageWorldInitial){u(i.resource);u(i.dynamic);u(i.dead);mat(i.matrix);for(auto w:i.emitter)u(w);u(0xffffffffu);for(auto w:i.block)u(w);}
 mix(stageWorldLocation.data(),stageWorldLocation.size());mix(stageWorldVisualTick0.words.data(),sizeof(stageWorldVisualTick0.words));
 u(stageCrowdLoaded);mix(stageCrowdSlotsTick0.data(),sizeof(stageCrowdSlotsTick0));mix(stageCrowdCountdownTick0.data(),sizeof(stageCrowdCountdownTick0));u(stageCrowdCursorTick0);
 u(uint32_t(stageMagnetInitial.size()));for(const auto& m:stageMagnetInitial){u(m.resource);u(m.flags);for(auto w:m.words)u(w);}
 u(uint32_t(stageHaloInitial.size()));for(const auto& x:stageHaloInitial){u(x.resource);u(uint32_t(x.texture));u(uint32_t(x.node));u(uint32_t(x.spin));f(x.r);f(x.g);f(x.b);f(x.a);f(x.size);f(x.angle);}
 u(uint32_t(stageBoostInitial.size()));for(const auto& b:stageBoostInitial){u(b.resource);mix(b.words.data(),sizeof(b.words));}
 u(stageMultiExported);for(const auto& m:stageMultiInitial){u(m.used);u(m.capacity);for(auto w:m.members)u(w);for(unsigned o=0;o<0x190;o+=4)u(m.emitter.u(o));mix(m.positionBase.data(),sizeof(m.positionBase));}
 u(stageWorldLoaded);return h;
}
// ---- exports ----------------------------------------------------------------------------------------------
// Drawable effects of this tick for web/set-piece-particles.js (sprites evaluated there, set-piece-particle-sprites.js):
// [count, then per effect: kind (0 Particle burst 0x380518, 1 DynamicParticle trail 0x3807A0), texture (id + trunc(flip
// phase)), blend mode, resource, 84 kernel words, then for a trail: capacity, cursor, capacity*4 ring A words
// (positions), capacity*4 ring B words (velocities, w = seed)].
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* stage_world_particles(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,0u);
 auto kernel=[&](const auto& img,unsigned base){for(unsigned o=0;o<0x150;o+=4)out.push_back(img.u(base+o));};
 for(auto& e:stageEntities){
  if(!e.alive)continue;
  for(auto& x:e.effects){
   if(!x.dynamic){
    const auto& m=x.burst.emitter;if(!ssx::originalParticleEffectDrawn(x.burst))continue;
    out.insert(out.end(),{0u,uint32_t(m.i(ssx::static_emitter::Texture)+ssx::set_piece_particles::eeTruncate(m.f(ssx::static_emitter::FlipPhase))),m.u(ssx::static_emitter::Blend),e.resource});
    kernel(m,ssx::static_emitter::Kernel);++out[0];continue;
   }
   const auto& m=x.trail.emitter;using namespace ssx::dynamic_emitter;
   if(x.trail.dead||m.i(ActiveCount)<=0||m.i(Capacity)<=0)continue;
   out.insert(out.end(),{1u,uint32_t(m.i(Texture)+ssx::set_piece_particles::eeTruncate(m.f(FlipPhase))),m.u(Blend),e.resource});
   kernel(m,Kernel);out.push_back(m.u(Capacity));out.push_back(m.u(Cursor));
   for(const auto& v:m.ringPosition)for(float f:v)out.push_back(std::bit_cast<uint32_t>(f));
   for(const auto& v:m.ringVelocity)for(float f:v)out.push_back(std::bit_cast<uint32_t>(f));
   ++out[0];
  }
 }
 // MultiParticle groups (0x358260): the group's emitter at every member, kernel position base = member translation +
 // the constructed base (VU vadd), in member order.
 for(auto& g:stageMultiGroups){
  if(!g.used||!g.emitter.u(ssx::static_emitter::Updated))continue;
  for(uint32_t member:g.members){
   const auto m=stage_instance_matrix(member);auto e=g.emitter;
   {terrain_original::Rounding rounding;e.setV(unsigned(ssx::static_emitter::Kernel)+unsigned(ssx::particle_kernel::PosBase),ssx::set_piece_particles::vAdd({m[3][0],m[3][1],m[3][2],m[3][3]},g.positionBase));}
   out.insert(out.end(),{0u,uint32_t(e.i(ssx::static_emitter::Texture)+ssx::set_piece_particles::eeTruncate(e.f(ssx::static_emitter::FlipPhase))),e.u(ssx::static_emitter::Blend),member});
   kernel(e,ssx::static_emitter::Kernel);++out[0];
  }
 }
 return out.data();
}
// Every effect's object words (PS2 comparisons, web/test-stage-world.mjs): [count, then per effect: resource, kind
// (0 Particle, 1 DynamicParticle), dead, entity type, then Particle: 16 owner-matrix words + 100 emitter words (+0x50);
// DynamicParticle: node, stopped, 128 emitter words (+0x60), capacity, capacity*4 ring A, capacity*4 ring B].
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* stage_world_effects(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,0u);
 for(auto& e:stageEntities){
  if(!e.alive)continue;
  for(auto& x:e.effects){
   ++out[0];out.insert(out.end(),{e.resource,x.dynamic?1u:0u,x.dynamic?x.trail.dead:x.burst.dead,uint32_t(e.type)});
   if(!x.dynamic){for(const auto& r:x.burst.matrix)for(float f:r)out.push_back(std::bit_cast<uint32_t>(f));out.insert(out.end(),x.burst.emitter.w.begin(),x.burst.emitter.w.end());continue;}
   out.push_back(uint32_t(x.trail.node));out.push_back(x.trail.stopped);out.insert(out.end(),x.trail.emitter.w.begin(),x.trail.emitter.w.end());
   out.push_back(uint32_t(x.trail.emitter.ringPosition.size()));
   for(const auto& v:x.trail.emitter.ringPosition)for(float f:v)out.push_back(std::bit_cast<uint32_t>(f));
   for(const auto& v:x.trail.emitter.ringVelocity)for(float f:v)out.push_back(std::bit_cast<uint32_t>(f));
  }
 }
 return out.data();
}
// Diagnostics: [loaded, entities alive, effects, particle creates, refused, LiveComp starts, slot runs] then per
// alive entity (resource, type, effect count, LiveComp time bits).
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* stage_world_info(){
 RIDER_LOCAL static std::vector<uint32_t> out;uint32_t alive=0,effects=0;for(auto& e:stageEntities)if(e.alive){++alive;effects+=uint32_t(e.effects.size());}
 out={stageWorldLoaded,alive,effects,stageParticleCreates,stageParticleRefused,stageLiveCompStarts,uint32_t(stageWorldSlotLog.size()/4)};
 for(auto& e:stageEntities)if(e.alive){out.push_back(e.resource);out.push_back(uint32_t(e.type));out.push_back(uint32_t(e.effects.size()));out.push_back(std::bit_cast<uint32_t>(e.live.anim.time));}
 return out.data();
}
// LiveComp starts by trigger/timer programs: [count, then per start 16 words: tick, resource, 11 builtin3 words,
// key8 word, key6 word, draws].
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* stage_world_livecomps(){RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,uint32_t(stageLiveCompLog.size()/16));out.insert(out.end(),stageLiveCompLog.begin(),stageLiveCompLog.end());return out.data();}
// pv sectionClock (web/livecomp-animation.js syncSectionClocks): the clocks of the section LiveComps that are core entities too
// (sectionPlayer: a slot-4 / slot-5 program; builtins 28 / 54 act on them): [count, then per entity 10 words: resource, mode, enabled,
// done, delay, rate, low, high, time, sampleTime (floats as bits)]. The JS player draws them and takes these each frame.
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* stage_world_section_clocks(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,0u);
 for(const auto& e:stageEntities)if(e.alive&&e.sectionPlayer&&e.type==stage_world::LiveComp){const auto& L=e.live;
  out.insert(out.end(),{e.resource,uint32_t(int32_t(L.mode)),uint32_t(L.enabled),uint32_t(L.done),uint32_t(L.delay),std::bit_cast<uint32_t>(L.rate),std::bit_cast<uint32_t>(L.low),
   std::bit_cast<uint32_t>(L.high),std::bit_cast<uint32_t>(L.anim.time),std::bit_cast<uint32_t>(L.anim.sampleTime)});++out[0];}
 return out.data();
}
// Sound requests (builtin30): [count, then per request 7 words: tick, program, current instance, keys 0..2, key mask].
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* stage_world_sounds(){RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,uint32_t(stageSoundLog.size()/7));out.insert(out.end(),stageSoundLog.begin(),stageSoundLog.end());return out.data();}
// Instances whose runtime state the stage programs changed: [count, then per instance: resource, drawn (static draw
// (f&3)==3 or an entity that draws), node state (0, 6 DeadNode, 16 Hide, 19 RestoreNode, 11 MeshAnim), flags].
bool browser_avalanche_entity(uint32_t resource);void browser_avalanche_track_reset(uint32_t track); // web/avalanche_gameplay.inc (a piece's Object entity)
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* stage_world_instances(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,0u);
 for(const auto& [r,f]:stageFlags){
  const int node=stage_node_state(r);auto* e=stage_entity(r);const bool breaking=e&&e->type==stage_world::Breaking;
  const auto* st=stage_instance(r); // entity draw 0x356298 (flags & 4): a LiveComp, the builtin0 Object entity (pickups, magnets) or a Debounce
  const bool entityDraws=(e&&e->type==stage_world::LiveComp)||(st&&(st->entity==StageGateEntity||st->debounce))||stageMagnets.count(r)||browser_avalanche_entity(r);
  const bool drawn=!node&&!breaking&&((f&3u)==3u||((f&4u)&&entityDraws));
  out.insert(out.end(),{r,drawn?1u:0u,uint32_t(breaking?11:node),f});++out[0];
 }
 return out.data();
}
// Flying break pieces: [count, then per MeshAnim: resource, node count, then per node the native (three.js column-major)
// delta rest -> current: the renderer applies it to the node's authored draw batch (like set_piece_attached)].
extern "C" EMSCRIPTEN_KEEPALIVE float* stage_world_meshanims(){
 RIDER_LOCAL static std::vector<float> out;std::vector<float> body;unsigned count=0;
 for(auto& e:stageEntities){
  if(!e.alive||e.type!=stage_world::Breaking||e.mesh.life<=0)continue; // 0x352810: not drawn once finished
  const auto it=stageMeshModels.find(e.resource);if(it==stageMeshModels.end())continue;const auto& m=it->second;
  std::vector<stage_world::PMat> rest(m.local.size());
  for(size_t i=0;i<rest.size();i++){stage_world::PMat l=m.local[i];for(unsigned c=0;c<3;c++)l[3][c]*=m.scale;rest[i]=ssx::set_piece_particles::mProduct(l,m.parent[i]>=0&&size_t(m.parent[i])<i?rest[size_t(m.parent[i])]:m.matrix);}
  std::vector<float> nodes;
  for(size_t i=0;i<e.mesh.q.size()&&i<rest.size();i++){
   const auto& q=e.mesh.q[i];const float x=q[0],y=q[1],z=q[2],w=q[3];
   ssx::RollerMatrix cur{{{1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y),0},{2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x),0},{2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y),0},{e.mesh.pos[i][0],e.mesh.pos[i][1],e.mesh.pos[i][2],1}}};
   std::vector<float> d;attached_delta(d,0,0,rest[i],cur);nodes.insert(nodes.end(),d.begin()+2,d.end());
  }
  body.push_back(float(e.resource));body.push_back(float(nodes.size()/16));body.insert(body.end(),nodes.begin(),nodes.end());++count;
 }
 out.assign(1,float(count));out.insert(out.end(),body.begin(),body.end());return out.data();
}
// MeshAnim states (PS2 comparisons): [count, then per MeshAnim: resource, flags, life, fade, spin max, end mode, gravity,
// node count, then per node q(4) pos(4) vel(4) axis(4) spin (17 floats)] (floats as bits).
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* stage_world_meshanim_state(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,0u);auto B=[](float x){return std::bit_cast<uint32_t>(x);};
 for(auto& e:stageEntities){
  if(!e.alive||e.type!=stage_world::Breaking)continue;const auto& a=e.mesh;++out[0];
  out.insert(out.end(),{e.resource,stage_flags(e.resource),B(a.life),B(a.fade),B(a.spinMax),uint32_t(a.endMode),B(a.gravity),uint32_t(a.q.size())});
  for(size_t i=0;i<a.q.size();i++){for(float v:a.q[i])out.push_back(B(v));for(float v:a.pos[i])out.push_back(B(v));for(float v:a.vel[i])out.push_back(B(v));for(float v:a.axis[i])out.push_back(B(v));out.push_back(B(a.spin[i]));}
 }
 return out.data();
}
// MultiParticle groups (PS2 comparisons): [count, then per used group: group, capacity, member count, members...,
// 100 emitter words (0x190)].
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* stage_world_multi(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,0u);
 for(unsigned g=0;g<4;g++){const auto& m=stageMultiGroups[g];if(!m.used)continue;++out[0];
  out.insert(out.end(),{g,m.capacity,uint32_t(m.members.size())});out.insert(out.end(),m.members.begin(),m.members.end());
  for(unsigned o=0;o<0x190;o+=4)out.push_back(m.emitter.u(o));}
 return out.data();
}
// Entity events for the JS players: [count, then (tick, resource, event: 1 destroyed, 2 MeshAnim start, 3 MeshAnim end)].
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* stage_world_entity_log(){RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,uint32_t(stageEntityLog.size()/3));out.insert(out.end(),stageEntityLog.begin(),stageEntityLog.end());return out.data();}
// Slot 4/5 program runs: [count, then (tick, resource, slot, program)].
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* stage_world_slots(){RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,uint32_t(stageWorldSlotLog.size()/4));out.insert(out.end(),stageWorldSlotLog.begin(),stageWorldSlotLog.end());return out.data();}
