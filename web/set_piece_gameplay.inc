// Snow Jam race-event set pieces driven by race-load entities (included by roller_gameplay.inc).
// Chairlifts: the LiveComp entity of mdl_ARA1_tramlores_0/_1 carries a MultiSplineModifier
// (engine/multi_spline_modifier.hpp; stage programs 69/71, slot 1 at section load). Each lift owns
// three clone instances (one chair each) evenly spaced on its closed spline (GondolaRail_0/1).
// State at race tick 0 comes from the snow-jam-ready savestate (tools/export_set_pieces.py ->
// generated/set_piece_seed.hpp); every game tick, before the rider (race_begin), the original
// 0x35A560 update and the LiveComp 0x3568B0 entity update (evaluation 0x35AC20, bounds) run.
// Collision: the authored instance takes the entity route (flags 0x210345, bounds = car-0
// record bounds, hierarchy on the car-0 matrix); clones are static-route instances (0x210325 /
// 0x212325) on their own matrix, clone 0 keeping the authored bounds. All carry the entity:
// the rigid predicate is 1 and the selected-contact callback adds the chair's surface velocity.
#include "../engine/multi_spline_modifier.hpp"
#include "../engine/spline_modifier.hpp"
#include <map>
#include "generated/set_piece_seed.hpp"
#if __has_include("generated/set_piece_seed_BRA2.hpp")
#include "generated/set_piece_seed_BRA2.hpp"
#define BROWSER_SET_PIECES_BRA2 1
#endif
#if __has_include("generated/set_piece_seed_BHP1.hpp")
#include "generated/set_piece_seed_BHP1.hpp"
#define BROWSER_SET_PIECES_BHP1 1
#endif
#if __has_include("generated/set_piece_seed_ASS1.hpp")
#include "generated/set_piece_seed_ASS1.hpp"
#define BROWSER_SET_PIECES_ASS1 1
#endif
#if __has_include("generated/set_piece_seed_ABA1.hpp")
#include "generated/set_piece_seed_ABA1.hpp"
#define BROWSER_SET_PIECES_ABA1 1
#endif
#if __has_include("generated/set_piece_seed_ABC1.hpp")
#include "generated/set_piece_seed_ABC1.hpp"
#define BROWSER_SET_PIECES_ABC1 1
#endif
// Peak 2 (docs/peak2.md)
#if __has_include("generated/set_piece_seed_CRA3.hpp")
#include "generated/set_piece_seed_CRA3.hpp"
#define BROWSER_SET_PIECES_CRA3 1
#endif
#if __has_include("generated/set_piece_seed_DRA4.hpp")
#include "generated/set_piece_seed_DRA4.hpp"
#define BROWSER_SET_PIECES_DRA4 1
#endif
#if __has_include("generated/set_piece_seed_DSS2.hpp")
#include "generated/set_piece_seed_DSS2.hpp"
#define BROWSER_SET_PIECES_DSS2 1
#endif
#if __has_include("generated/set_piece_seed_CBA2.hpp")
#include "generated/set_piece_seed_CBA2.hpp"
#define BROWSER_SET_PIECES_CBA2 1
#endif
#if __has_include("generated/set_piece_seed_CHP2.hpp")
#include "generated/set_piece_seed_CHP2.hpp"
#define BROWSER_SET_PIECES_CHP2 1
#endif
#if __has_include("generated/set_piece_seed_DBC2.hpp")
#include "generated/set_piece_seed_DBC2.hpp"
#define BROWSER_SET_PIECES_DBC2 1
#endif
// Peak 3 (docs/peak3.md)
#if __has_include("generated/set_piece_seed_ERA5.hpp")
#include "generated/set_piece_seed_ERA5.hpp"
#define BROWSER_SET_PIECES_ERA5 1
#endif
#if __has_include("generated/set_piece_seed_ESS3.hpp")
#include "generated/set_piece_seed_ESS3.hpp"
#define BROWSER_SET_PIECES_ESS3 1
#endif
#if __has_include("generated/set_piece_seed_EBA3.hpp")
#include "generated/set_piece_seed_EBA3.hpp"
#define BROWSER_SET_PIECES_EBA3 1
#endif
#if __has_include("generated/set_piece_seed_EHP3.hpp")
#include "generated/set_piece_seed_EHP3.hpp"
#define BROWSER_SET_PIECES_EHP3 1
#endif
#if __has_include("generated/set_piece_seed_EBC3.hpp")
#include "generated/set_piece_seed_EBC3.hpp"
#define BROWSER_SET_PIECES_EBC3 1
#endif
// Per-location seeds (tools/export_set_pieces.py [--location L]): MultiSpline modifiers (Snow Jam chairlifts,
// Metro-City bins, Junction traffic), looping SplineModifiers resident from the load (Junction blimp),
// triggered/section spline pieces and the slot-2 trigger owners. Nothing is set up for other locations.
struct SetPieceMultiSeed {uint32_t resource=0,runtimeFlags=0,count=0,mode=0,angle=0,distance=0,speed=0,radius=0,dirty=0,path=0,cursor=0,length=0,activation=0;
 int32_t constructedAfterTick=-1;float activationRemaining=0;std::vector<std::array<uint32_t,48>> cars;}; // car: resource, flags, record 24, matrix 16, low 3, high 3
struct SetPieceLoopSeed {uint32_t resource=0,runtimeFlags=0,path=0,cursor=0;std::array<uint32_t,60> bytes{};};
struct SetPieceTables {std::string location;std::vector<SetPieceMultiSeed> multi;std::vector<BrowserSplinePieceSeed> pieces;std::vector<SetPieceLoopSeed> loops;std::vector<uint32_t> owners;float sectionRemaining=-1;};
template<class Car> static std::array<uint32_t,48> set_piece_car(const Car& c){
 std::array<uint32_t,48> w{};w[0]=c.resource;w[1]=c.runtimeFlags;for(unsigned i=0;i<24;i++)w[2+i]=c.record[i];for(unsigned i=0;i<16;i++)w[26+i]=c.matrix[i];for(unsigned i=0;i<3;i++){w[42+i]=c.low[i];w[45+i]=c.high[i];}return w;
}
template<class NS> static void set_piece_location_tables(SetPieceTables& t){
 for(const auto& m:NS::multiSplines){SetPieceMultiSeed x{m.resource,m.authoredFlags,m.count,m.mode,m.angle,m.distance,m.speed,m.radius,m.dirty,m.path,m.cursor,m.length,m.activation,m.constructedAfterTick,m.activationRemaining,{}};
  for(uint32_t k=0;k<m.count;k++)x.cars.push_back(set_piece_car(NS::multiSplineCars[m.firstCar+k]));t.multi.push_back(std::move(x));}
 for(const auto& l:NS::splineModifiers){SetPieceLoopSeed x;x.resource=l.resource;x.runtimeFlags=l.authoredFlags;x.path=l.path;x.cursor=l.cursor;x.bytes=l.bytes;t.loops.push_back(x);}
 for(const auto& q:NS::splinePieces){BrowserSplinePieceSeed x{q.resource,q.owner,q.slot,q.program,q.guard,q.drawn,{q.args.instance,q.args.spline,q.args.endMode,q.args.orientation,q.args.speedKmh,q.args.rollDegrees,q.args.startDistance,q.args.startJitter,q.args.running,q.args.acceleration,q.args.accelerationTime},q.matrix,q.low,q.high};t.pieces.push_back(x);}
 t.owners.assign(NS::triggerOwners.begin(),NS::triggerOwners.end());
}
#ifdef BROWSER_SET_PIECES_BRA2
struct SetPiecesBRA2 {static constexpr const auto& multiSplines=browser_set_pieces_bra2::multiSplines;static constexpr const auto& multiSplineCars=browser_set_pieces_bra2::multiSplineCars;
 static constexpr const auto& splineModifiers=browser_set_pieces_bra2::splineModifiers;static constexpr const auto& splinePieces=browser_set_pieces_bra2::splinePieces;static constexpr const auto& triggerOwners=browser_set_pieces_bra2::triggerOwners;};
#endif
#ifdef BROWSER_SET_PIECES_ASS1
struct SetPiecesASS1 {static constexpr const auto& multiSplines=browser_set_pieces_ass1::multiSplines;static constexpr const auto& multiSplineCars=browser_set_pieces_ass1::multiSplineCars;
 static constexpr const auto& splineModifiers=browser_set_pieces_ass1::splineModifiers;static constexpr const auto& splinePieces=browser_set_pieces_ass1::splinePieces;static constexpr const auto& triggerOwners=browser_set_pieces_ass1::triggerOwners;};
#endif
#ifdef BROWSER_SET_PIECES_ABA1
struct SetPiecesABA1 {static constexpr const auto& multiSplines=browser_set_pieces_aba1::multiSplines;static constexpr const auto& multiSplineCars=browser_set_pieces_aba1::multiSplineCars;
 static constexpr const auto& splineModifiers=browser_set_pieces_aba1::splineModifiers;static constexpr const auto& splinePieces=browser_set_pieces_aba1::splinePieces;static constexpr const auto& triggerOwners=browser_set_pieces_aba1::triggerOwners;};
#endif
#ifdef BROWSER_SET_PIECES_ABC1
struct SetPiecesABC1 {static constexpr const auto& multiSplines=browser_set_pieces_abc1::multiSplines;static constexpr const auto& multiSplineCars=browser_set_pieces_abc1::multiSplineCars;
 static constexpr const auto& splineModifiers=browser_set_pieces_abc1::splineModifiers;static constexpr const auto& splinePieces=browser_set_pieces_abc1::splinePieces;static constexpr const auto& triggerOwners=browser_set_pieces_abc1::triggerOwners;};
#endif
#ifdef BROWSER_SET_PIECES_CRA3
struct SetPiecesCRA3 {static constexpr const auto& multiSplines=browser_set_pieces_cra3::multiSplines;static constexpr const auto& multiSplineCars=browser_set_pieces_cra3::multiSplineCars;
 static constexpr const auto& splineModifiers=browser_set_pieces_cra3::splineModifiers;static constexpr const auto& splinePieces=browser_set_pieces_cra3::splinePieces;static constexpr const auto& triggerOwners=browser_set_pieces_cra3::triggerOwners;};
#endif
#ifdef BROWSER_SET_PIECES_DRA4
struct SetPiecesDRA4 {static constexpr const auto& multiSplines=browser_set_pieces_dra4::multiSplines;static constexpr const auto& multiSplineCars=browser_set_pieces_dra4::multiSplineCars;
 static constexpr const auto& splineModifiers=browser_set_pieces_dra4::splineModifiers;static constexpr const auto& splinePieces=browser_set_pieces_dra4::splinePieces;static constexpr const auto& triggerOwners=browser_set_pieces_dra4::triggerOwners;};
#endif
#ifdef BROWSER_SET_PIECES_DSS2
struct SetPiecesDSS2 {static constexpr const auto& multiSplines=browser_set_pieces_dss2::multiSplines;static constexpr const auto& multiSplineCars=browser_set_pieces_dss2::multiSplineCars;
 static constexpr const auto& splineModifiers=browser_set_pieces_dss2::splineModifiers;static constexpr const auto& splinePieces=browser_set_pieces_dss2::splinePieces;static constexpr const auto& triggerOwners=browser_set_pieces_dss2::triggerOwners;};
#endif
#ifdef BROWSER_SET_PIECES_CBA2
struct SetPiecesCBA2 {static constexpr const auto& multiSplines=browser_set_pieces_cba2::multiSplines;static constexpr const auto& multiSplineCars=browser_set_pieces_cba2::multiSplineCars;
 static constexpr const auto& splineModifiers=browser_set_pieces_cba2::splineModifiers;static constexpr const auto& splinePieces=browser_set_pieces_cba2::splinePieces;static constexpr const auto& triggerOwners=browser_set_pieces_cba2::triggerOwners;};
#endif
#ifdef BROWSER_SET_PIECES_CHP2
struct SetPiecesCHP2 {static constexpr const auto& multiSplines=browser_set_pieces_chp2::multiSplines;static constexpr const auto& multiSplineCars=browser_set_pieces_chp2::multiSplineCars;
 static constexpr const auto& splineModifiers=browser_set_pieces_chp2::splineModifiers;static constexpr const auto& splinePieces=browser_set_pieces_chp2::splinePieces;static constexpr const auto& triggerOwners=browser_set_pieces_chp2::triggerOwners;};
#endif
#ifdef BROWSER_SET_PIECES_DBC2
struct SetPiecesDBC2 {static constexpr const auto& multiSplines=browser_set_pieces_dbc2::multiSplines;static constexpr const auto& multiSplineCars=browser_set_pieces_dbc2::multiSplineCars;
 static constexpr const auto& splineModifiers=browser_set_pieces_dbc2::splineModifiers;static constexpr const auto& splinePieces=browser_set_pieces_dbc2::splinePieces;static constexpr const auto& triggerOwners=browser_set_pieces_dbc2::triggerOwners;};
#endif
#ifdef BROWSER_SET_PIECES_ERA5
struct SetPiecesERA5 {static constexpr const auto& multiSplines=browser_set_pieces_era5::multiSplines;static constexpr const auto& multiSplineCars=browser_set_pieces_era5::multiSplineCars;
 static constexpr const auto& splineModifiers=browser_set_pieces_era5::splineModifiers;static constexpr const auto& splinePieces=browser_set_pieces_era5::splinePieces;static constexpr const auto& triggerOwners=browser_set_pieces_era5::triggerOwners;};
#endif
#ifdef BROWSER_SET_PIECES_ESS3
struct SetPiecesESS3 {static constexpr const auto& multiSplines=browser_set_pieces_ess3::multiSplines;static constexpr const auto& multiSplineCars=browser_set_pieces_ess3::multiSplineCars;
 static constexpr const auto& splineModifiers=browser_set_pieces_ess3::splineModifiers;static constexpr const auto& splinePieces=browser_set_pieces_ess3::splinePieces;static constexpr const auto& triggerOwners=browser_set_pieces_ess3::triggerOwners;};
#endif
#ifdef BROWSER_SET_PIECES_EBA3
struct SetPiecesEBA3 {static constexpr const auto& multiSplines=browser_set_pieces_eba3::multiSplines;static constexpr const auto& multiSplineCars=browser_set_pieces_eba3::multiSplineCars;
 static constexpr const auto& splineModifiers=browser_set_pieces_eba3::splineModifiers;static constexpr const auto& splinePieces=browser_set_pieces_eba3::splinePieces;static constexpr const auto& triggerOwners=browser_set_pieces_eba3::triggerOwners;};
#endif
#ifdef BROWSER_SET_PIECES_EHP3
struct SetPiecesEHP3 {static constexpr const auto& multiSplines=browser_set_pieces_ehp3::multiSplines;static constexpr const auto& multiSplineCars=browser_set_pieces_ehp3::multiSplineCars;
 static constexpr const auto& splineModifiers=browser_set_pieces_ehp3::splineModifiers;static constexpr const auto& splinePieces=browser_set_pieces_ehp3::splinePieces;static constexpr const auto& triggerOwners=browser_set_pieces_ehp3::triggerOwners;};
#endif
#ifdef BROWSER_SET_PIECES_EBC3
struct SetPiecesEBC3 {static constexpr const auto& multiSplines=browser_set_pieces_ebc3::multiSplines;static constexpr const auto& multiSplineCars=browser_set_pieces_ebc3::multiSplineCars;
 static constexpr const auto& splineModifiers=browser_set_pieces_ebc3::splineModifiers;static constexpr const auto& splinePieces=browser_set_pieces_ebc3::splinePieces;static constexpr const auto& triggerOwners=browser_set_pieces_ebc3::triggerOwners;};
#endif
#ifdef BROWSER_SET_PIECES_BHP1
struct SetPiecesBHP1 {static constexpr const auto& multiSplines=browser_set_pieces_bhp1::multiSplines;static constexpr const auto& multiSplineCars=browser_set_pieces_bhp1::multiSplineCars;
 static constexpr const auto& splineModifiers=browser_set_pieces_bhp1::splineModifiers;static constexpr const auto& splinePieces=browser_set_pieces_bhp1::splinePieces;static constexpr const auto& triggerOwners=browser_set_pieces_bhp1::triggerOwners;};
#endif
static const SetPieceTables& set_piece_tables(const std::string& location){
 static std::map<std::string,SetPieceTables> cache; // shared: tables built from the compiled seeds of a location (constant)
 auto found=cache.find(location);if(found!=cache.end())return found->second;
 SetPieceTables t;t.location=location;
 if(location=="ARA1"){
  for(const auto& c:browserChairliftSeeds){SetPieceMultiSeed x{c.resource,c.runtimeFlags,c.count,c.mode,c.angle,c.distance,c.speed,c.radius,c.dirty,c.path,c.cursor,c.length,0,-1,0,{}};
   for(const auto& car:c.cars)x.cars.push_back(set_piece_car(car));t.multi.push_back(std::move(x));}
  t.pieces.assign(browserSplinePieceSeeds.begin(),browserSplinePieceSeeds.end());
  t.owners.assign(browserSetPieceTriggerOwners.begin(),browserSetPieceTriggerOwners.end());t.sectionRemaining=browserRavenSectionRemaining;
 }
#ifdef BROWSER_SET_PIECES_BRA2
 if(location=="BRA2")set_piece_location_tables<SetPiecesBRA2>(t);
#endif
#ifdef BROWSER_SET_PIECES_BHP1
 if(location=="BHP1")set_piece_location_tables<SetPiecesBHP1>(t);
#endif
#ifdef BROWSER_SET_PIECES_ASS1
 if(location=="ASS1")set_piece_location_tables<SetPiecesASS1>(t);
#endif
#ifdef BROWSER_SET_PIECES_ABA1
 if(location=="ABA1")set_piece_location_tables<SetPiecesABA1>(t);
#endif
#ifdef BROWSER_SET_PIECES_ABC1
 if(location=="ABC1")set_piece_location_tables<SetPiecesABC1>(t);
#endif
#ifdef BROWSER_SET_PIECES_CRA3
 if(location=="CRA3")set_piece_location_tables<SetPiecesCRA3>(t);
#endif
#ifdef BROWSER_SET_PIECES_DRA4
 if(location=="DRA4")set_piece_location_tables<SetPiecesDRA4>(t);
#endif
#ifdef BROWSER_SET_PIECES_DSS2
 if(location=="DSS2")set_piece_location_tables<SetPiecesDSS2>(t);
#endif
#ifdef BROWSER_SET_PIECES_CBA2
 if(location=="CBA2")set_piece_location_tables<SetPiecesCBA2>(t);
#endif
#ifdef BROWSER_SET_PIECES_CHP2
 if(location=="CHP2")set_piece_location_tables<SetPiecesCHP2>(t);
#endif
#ifdef BROWSER_SET_PIECES_DBC2
 if(location=="DBC2")set_piece_location_tables<SetPiecesDBC2>(t);
#endif
#ifdef BROWSER_SET_PIECES_ERA5
 if(location=="ERA5")set_piece_location_tables<SetPiecesERA5>(t);
#endif
#ifdef BROWSER_SET_PIECES_ESS3
 if(location=="ESS3")set_piece_location_tables<SetPiecesESS3>(t);
#endif
#ifdef BROWSER_SET_PIECES_EBA3
 if(location=="EBA3")set_piece_location_tables<SetPiecesEBA3>(t);
#endif
#ifdef BROWSER_SET_PIECES_EHP3
 if(location=="EHP3")set_piece_location_tables<SetPiecesEHP3>(t);
#endif
#ifdef BROWSER_SET_PIECES_EBC3
 if(location=="EBC3")set_piece_location_tables<SetPiecesEBC3>(t);
#endif
 return cache.emplace(location,std::move(t)).first->second;
}
RIDER_LOCAL static std::string setPieceLocationName;              // location of the loaded collision world (set_piece_instance)
RIDER_LOCAL static const SetPieceTables* setPieceTables=nullptr;   // tables of the attached world
extern const ssx::OriginalRailRecord* browserRailRecord(uint32_t packedId);
extern uint32_t browser_shared_random_next(); // web/animation_bridge.cpp: the shared RNG 0x4FF030
extern "C" float* race_progress_info();       // web/race_bridge.cpp: [0] human remaining (rider+0x4D0)
RIDER_LOCAL extern bool browserHumanRider;                 // false in a computer rider's core (web/ai-racers.js)
// Spline set pieces (engine/spline_modifier.hpp; tools/export_spline_setpieces.py): the raven flyby
// (ravensplineanima_1000, program 54 slot 1 at section activation, loops forever, drawn) and the
// invisible carriers of the rocket / spintwin / dragon particle trails (programs 120/131/136/137,
// slot 2 of their trigger volumes, stop at the path end where builtin16 in slot 4 turns the entity into
// a type-13 emitter with a PositionModifier holding L - 0.1). Each construction draws one value from the
// shared RNG (0x317830, jitter 0). A piece created during tick T's rider physics first moves in T + 1.
RIDER_LOCAL static unsigned chairliftTicks=0; // race_begin calls since the race-tick-0 attach (set-piece tick index)
struct BrowserSplinePiece {
 const BrowserSplinePieceSeed* seed=nullptr;int phase=0;uint32_t launchTick=0; // 1 spline, 2 position (frozen at the path end)
 ssx::OriginalSplineModifier spline;ssx::OriginalPositionModifier position;
};
RIDER_LOCAL static std::vector<BrowserSplinePiece> browserSplinePieces;RIDER_LOCAL static std::vector<uint32_t> splineOwnersFired;RIDER_LOCAL static unsigned splineLaunches=0;
RIDER_LOCAL static bool setPieceCourse=false; // ARA1 event world attached (browser_attach_set_pieces): the seeds are Snow Jam's
// Fired course-script triggers (tick, owner * 8 + slot), from this core's rider or replayed from another
// rider's core (npc_gameplay.inc world_trigger_apply): web/set-pieces-renderer.js starts the LiveComps.
RIDER_LOCAL static std::vector<uint32_t> setPieceTriggerLog;
static void section_entity_created(uint32_t resource);static void section_reset(); // web/section_gameplay.inc
static void stage_world_spline_finished(uint32_t resource);static void stage_world_section_enter(uint32_t resource,int32_t program); // web/stage_world.inc
static void stage_world_section_leave(uint32_t resource);static void stage_world_section_leave_program(uint32_t resource,int32_t program);
static int32_t stage_slot_program(uint32_t resource,int slot);static bool stage_slot_calls(uint32_t resource,int slot,int builtin); // web/stage_script_gameplay.inc
static void section_multi_spline_destroyed(uint32_t resource); // web/section_gameplay.inc
void browser_release_section_lift(uint32_t resource); // below
RIDER_LOCAL static bool sectionsRunning;
// draw=false: the section pass (section_gameplay.inc) already took the builtin19 draw in program order.
static void attached_launch(uint32_t resource); // web/attached_setpieces.inc
static bool launch_spline_seed(const BrowserSplinePieceSeed& seed,bool draw);
static void launch_spline_pieces(uint32_t owner,uint32_t slot,bool draw=true){
 if(!setPieceCourse||!setPieceTables)return;
 if(std::find(splineOwnersFired.begin(),splineOwnersFired.end(),owner*8+slot)!=splineOwnersFired.end())return; /* builtin52 guard: launched once */
 bool any=slot==2&&std::find(setPieceTables->owners.begin(),setPieceTables->owners.end(),owner)!=setPieceTables->owners.end();
 for(const auto& seed:setPieceTables->pieces){
  if(seed.owner!=owner||seed.slot!=slot)continue;
  if(seed.guard==2)continue; /* random-gated program (Crow's Nest osprey): launched by the core VM's builtin19 (stage_launch_gated_spline) */
  if(launch_spline_seed(seed,draw))any=true;
 }
 if(any){splineOwnersFired.push_back(owner*8+slot);if(setPieceTriggerLog.size()<2*4096){setPieceTriggerLog.push_back(chairliftTicks?chairliftTicks-1:0);setPieceTriggerLog.push_back(owner*8+slot);}}
}
// builtin19 of a timer program (slot 5: Snow Jam EZrocketCore, program 142 on EZseqTimer_1000; Metro-City /
// Junction rocket cores): the seed of the program's current instance, launched in the entity pass (its first
// update is the next tick's). Guarded by the program's own builtin52 check (the entity exists after the launch).
static bool stage_launch_spline(uint32_t resource){
 if(!setPieceCourse||!setPieceTables)return false;
 // 2FAE38 on the instance's entity: a Spline still flying is the same type (vt+0x12C re-init, no construction, no draw); a finished
 // one (end mode 0: the Position entity frozen at the path end) is replaced by a new Spline, whose 0x359460 draws (PS2 Schizophrenia
 // timer 101655: 123159 / 111895 finish at 3441 / 3431 and are launched again at 3609 / 3615 and 3849 / 3855).
 for(auto it=browserSplinePieces.begin();it!=browserSplinePieces.end();++it)if(it->seed->resource==resource){if(it->phase!=2)return false;browserSplinePieces.erase(it);break;}
 for(const auto& seed:setPieceTables->pieces)if(seed.resource==resource&&seed.slot==5)return launch_spline_seed(seed,true);
 return false;
}
// builtin19 of a random-gated slot-2 program (seed guard 2: Crow's Nest ospreytrigger, program 42 flies the osprey when
// random(0, 100) >= 60): the seed of the target, launched when the VM reaches the call (after the program's draws).
static bool stage_launch_gated_spline(uint32_t resource){
 if(!setPieceCourse||!setPieceTables)return false;
 for(const auto& p:browserSplinePieces)if(p.seed->resource==resource)return false;
 for(const auto& seed:setPieceTables->pieces)if(seed.resource==resource&&seed.slot==2&&seed.guard==2){
  if(!launch_spline_seed(seed,true))return false;
  if(setPieceTriggerLog.size()<2*4096){setPieceTriggerLog.push_back(chairliftTicks?chairliftTicks-1:0);setPieceTriggerLog.push_back(seed.owner*8+2);}
  return true;}
 return false;
}
// Collidable spline pieces (R&B train: locomotive / flatbed / boxcar / tanker / trainboxes, collision types 1-2; every
// other course's pieces are collision type 0): 0x3554B0 attach -> flags &~0x20 | 0x40 (plus the Object entity's draw bits:
// countdown 0x40214023 -> 0x40214145 in setpieces-ass1/full), and the entity route follows the Spline matrix and entity
// bounds (334458 / 334888) like the chairlift cars. Rigid predicate: Spline vt+0x44 0x360B60 = 1; selected callback:
// Object 0x356AE0 -> 353098 -> Spline vt+0xB4 0x359CF8 (originalSplineContactVelocity).
static void place_chair(WorldCollisionInstance& instance,const ssx::RollerMatrix& root,const terrain_original::Vector& low,const terrain_original::Vector& high,bool entityRoute);
static WorldCollisionInstance* set_piece_instance_at(uint32_t resource);
static uint32_t stage_flags(uint32_t resource);static void stage_set_flags(uint32_t resource,uint32_t flags); // web/stage_world.inc (instance+8)
RIDER_LOCAL_LAZY static std::map<uint32_t,std::optional<uint32_t>> splinePieceCountdownFlags; // resource -> flags before the first launch (race restart)
static WorldCollisionInstance* spline_piece_body(const BrowserSplinePiece& p){
 auto* w=set_piece_instance_at(p.seed->resource);return w&&w->type!=0&&!w->hierarchyLocal.empty()?w:nullptr;
}
static void spline_piece_place(BrowserSplinePiece& p){
 auto* w=spline_piece_body(p);if(!w||!w->eventRuntimeFlags||(*w->eventRuntimeFlags&0x60u)!=0x40u)return; // a MeshAnim cleared the routes
 const auto& lo=p.phase==2?p.position.boundsMin:p.spline.boundsMin;const auto& hi=p.phase==2?p.position.boundsMax:p.spline.boundsMax;
 place_chair(*w,p.phase==2?ssx::originalPositionMatrix(p.position):ssx::originalSplineMatrix(p.spline),{lo[0],lo[1],lo[2]},{hi[0],hi[1],hi[2]},true);
}
static void spline_piece_collision_start(BrowserSplinePiece& p){
 auto* w=spline_piece_body(p);if(!w)return;
 if(!splinePieceCountdownFlags.count(w->resource))splinePieceCountdownFlags[w->resource]=w->eventRuntimeFlags;
 const uint32_t f=stage_flags(w->resource);stage_set_flags(w->resource,(f&~0x22u)|0x144u);w->entityPointer=true;spline_piece_place(p); // countdown 0x40214023 / 0x210023
}
static void spline_piece_collision_stop(uint32_t resource){
 auto it=splinePieceCountdownFlags.find(resource);if(it==splinePieceCountdownFlags.end())return;
 if(auto* w=set_piece_instance_at(resource)){w->eventRuntimeFlags=it->second;w->entity.reset();w->entityPointer=false;}
}
static const BrowserSplinePiece* spline_piece_of(uint32_t resource){for(const auto& p:browserSplinePieces)if(p.seed->resource==resource&&spline_piece_body(p))return &p;return nullptr;}
// A MeshAnim (builtin13, web/stage_world.inc) replaces the piece's Object entity and its Spline (R&B trainboxes).
static void spline_piece_replaced(uint32_t resource){
 std::erase_if(browserSplinePieces,[&](const BrowserSplinePiece& p){if(p.seed->resource!=resource)return false;if(auto* w=set_piece_instance_at(resource)){w->entity.reset();w->entityPointer=false;}return true;});
 browser_dynamic_rails_unbind(resource);
}
static bool launch_spline_seed(const BrowserSplinePieceSeed& seed,bool draw){
 {
  const auto* record=browserRailRecord(seed.args.spline);if(!record)return false;
  auto f=[](uint32_t b){return std::bit_cast<float>(b);};
  ssx::OriginalSplineScriptArgs a;a.instance=int32_t(seed.args.instance);a.spline=seed.args.spline;a.endMode=seed.args.endMode;a.orientation=seed.args.orientation;
  a.speedKmh=f(seed.args.speedKmh);a.rollDegrees=f(seed.args.rollDegrees);a.startDistance=f(seed.args.startDistance);a.startJitter=f(seed.args.startJitter);
  a.running=seed.args.running;a.acceleration=f(seed.args.acceleration);a.accelerationTime=f(seed.args.accelerationTime);
  if(a.startJitter!=0.f)throw std::runtime_error("Spline set piece with start jitter needs the shared RNG value");
  BrowserSplinePiece piece;piece.seed=&seed;piece.phase=1;piece.launchTick=chairliftTicks?chairliftTicks-1:0;
  attached_launch(seed.resource); /* the LiveComp of the same program (builtin3 before builtin19) */
  ssx::OriginalRandomState scratch;ssx::originalSplineConstruct(piece.spline,a,*record,scratch);if(draw)browser_shared_random_next(); /* the draw itself (value unused with jitter 0) */
  ssx::originalSplineAttach(piece.spline,{f(seed.matrix[12]),f(seed.matrix[13]),f(seed.matrix[14]),1.f},{f(seed.low[0]),f(seed.low[1]),f(seed.low[2])},{f(seed.high[0]),f(seed.high[1]),f(seed.high[2])},2u);
  browserSplinePieces.push_back(piece);++splineLaunches;section_entity_created(seed.resource);
  spline_piece_collision_start(browserSplinePieces.back());
 }
 return true;
}
// MultiSpline car evaluations shared by the rider contexts (docs/sim-performance.md "Chairlift evaluation"). 0x35AC20
// (engine/multi_spline_modifier.hpp originalMultiSplineEvaluate) is a pure function of the car count, mode, angle, distance, the
// path's length, cursor and index, and the spline record's segments, and writes the cars' evaluated words, the path cursor and
// index. Every rider context evaluates its copy of a lift at the same distance each tick; the last evaluation per lift is kept
// here with all of those inputs (the segments by content) and outputs, and a context whose inputs are bit-identical takes the
// outputs instead of evaluating again. Not per rider on purpose (web/check-rider-globals.mjs): only reused on equal inputs.
struct MultiSplineShared {bool valid=false;int32_t count=0,mode=0,index=0,indexAfter=0;uint32_t angle=0,distance=0,length=0,packedId=0;size_t cursor=0,cursorAfter=0;
 std::vector<ssx::OriginalRailSegment> segments;std::vector<ssx::OriginalMultiSplineCar> cars;};
static std::unordered_map<uint32_t,MultiSplineShared> multiSplineShared;
static void multi_spline_evaluate_shared(ssx::OriginalMultiSplineModifier& m,uint32_t key){
 if(!m.dirty)return;
 const ssx::OriginalRailRecord* record=m.path.record;if(!record){ssx::originalMultiSplineEvaluate(m);return;}
 const auto bits=[](float x){return std::bit_cast<uint32_t>(x);};
 auto& e=multiSplineShared[key];
 const bool segmentsSame=e.segments.size()==record->segments.size()&&(e.segments.empty()||std::memcmp(e.segments.data(),record->segments.data(),e.segments.size()*sizeof(ssx::OriginalRailSegment))==0);
 const bool same=e.valid&&segmentsSame&&e.count==m.count&&e.mode==m.mode&&e.angle==bits(m.angle)&&e.distance==bits(m.distance)&&e.length==bits(m.path.length)
  &&e.cursor==m.path.cursor&&e.index==m.path.index&&e.packedId==record->packedId&&e.cars.size()==m.cars.size();
 if(same){
  for(size_t i=0;i<m.cars.size();++i){auto& c=m.cars[i];const auto& x=e.cars[i];c.position=x.position;c.tangent=x.tangent;c.curvature=x.curvature;c.tangentXX=x.tangentXX;c.tangentYY=x.tangentYY;c.matrix=x.matrix;}
  m.path.cursor=e.cursorAfter;m.path.index=e.indexAfter;m.dirty=0;return;
 }
 e.valid=false;e.count=m.count;e.mode=m.mode;e.angle=bits(m.angle);e.distance=bits(m.distance);e.length=bits(m.path.length);e.cursor=m.path.cursor;e.index=m.path.index;e.packedId=record->packedId;
 if(!segmentsSame)e.segments=record->segments;
 ssx::originalMultiSplineEvaluate(m);
 if(e.cars.size()!=m.cars.size())e.cars=m.cars;else for(size_t i=0;i<m.cars.size();++i){auto& x=e.cars[i];const auto& c=m.cars[i];x.position=c.position;x.tangent=c.tangent;x.curvature=c.curvature;x.tangentXX=c.tangentXX;x.tangentYY=c.tangentYY;x.matrix=c.matrix;}e.cursorAfter=m.path.cursor;e.indexAfter=m.path.index;e.valid=true;
}
struct BrowserChairlift {
 uint32_t resource=0;std::vector<uint32_t> clones;const SetPieceMultiSeed* seed=nullptr;bool active=false;
 // MultiSplineModifier +0x34 (0x35A0E0: 1 when its instance is listed at the construction; section enter 0x35AAD0 +1, leave 0x35AAE0 -1).
 // An Object entity's update 0x356198 destroys it at <= 0 (vt+0xB8 0x356D48, 0x34FCE0 -> 2D1A30: its slot-3 program instead when it
 // has one); a LiveComp owner (builtin 3 + 20: BRA2 unique_cargobin_end_b, the Peak 2/3 trams) never reads it.
 int32_t refs=1;bool objectOwner=false;
 ssx::OriginalMultiSplineModifier modifier;
};
RIDER_LOCAL static std::vector<BrowserChairlift> browserChairlifts;
// Looping SplineModifiers resident from the load (Junction blimp): drawn only (their collision stays static).
struct BrowserLoopSpline {const SetPieceLoopSeed* seed=nullptr;bool ready=false;ssx::OriginalSplineModifier spline;};
RIDER_LOCAL static std::vector<BrowserLoopSpline> browserLoopSplines;
// A resident looping Spline whose own slot-1 program builds it again (a slot-1 seed of the same resource, R&B
// ravensplineanimb / program 66; tools/export_location_set_pieces.py): its entity is section-streamed like a launched
// piece (relocated by its bounds 0x3568B0, destroyed on a leave, relaunched on the next enter). Loops without one
// (the blimps, whose leave runs an empty slot-3 program) keep their static section cell.
static const BrowserSplinePieceSeed* loop_relaunch_seed(uint32_t resource){
 if(!setPieceTables)return nullptr;
 for(const auto& s:setPieceTables->pieces)if(s.resource==resource&&s.owner==resource&&s.slot==1)return &s;
 return nullptr;
}
#include "attached_setpieces.inc"
// Selected rider contacts (105398 store step -> 121818, rider+A30) per set-piece tick: the stage
// slot-2 handlers of trigger volumes (web/set-pieces-renderer.js, LiveComp/Spline starts) key on them.
RIDER_LOCAL static std::vector<uint32_t> setPieceContacts; // (tick, resource) pairs
void shared_world_log(std::initializer_list<uint32_t>); // web/shared_world.inc
// The contact log of the one world: a replay from another rider's core (web/shared_world.inc) only logs.
bool browser_set_piece_contact_log(uint32_t resource){
 const uint32_t tick=chairliftTicks?chairliftTicks-1:0;
 for(size_t i=setPieceContacts.size();i>=2&&setPieceContacts[i-2]==tick;i-=2)if(setPieceContacts[i-1]==resource)return false;
 if(setPieceContacts.size()<2*65536){setPieceContacts.push_back(tick);setPieceContacts.push_back(resource);}
 return true;
}
void browser_set_piece_contact(uint32_t resource){
 if(!browser_set_piece_contact_log(resource))return;
 shared_world_log({4u,resource});
 launch_spline_pieces(resource,2); /* 121818 -> 30A060 slot 2 */
 browser_rail_livecomp_fire(resource); /* program 144: the falling billboard (web/falling_billboard.inc) */
}
unsigned setPieceInstanceCalls=0; // web/world_bridge.cpp parse cache: whether a parse named the set-piece location
void browser_set_piece_location(const std::string& location){setPieceLocationName=location;}
bool set_piece_instance(const std::string& location,uint32_t resource,uint32_t authoredFlags,uint32_t& runtimeFlags){
 setPieceLocationName=location;++setPieceInstanceCalls;
 for(const auto& s:set_piece_tables(location).multi)if(s.resource==resource){
  if((authoredFlags>>16)!=(s.runtimeFlags>>16))throw std::runtime_error("MultiSpline set-piece instance identity mismatch");
  runtimeFlags=s.activation?(s.runtimeFlags&~0x40u)|0x20u:s.runtimeFlags; /* section pieces stay static until their section loads */
  return true;
 }
 // looping splines (blimp): keep the hierarchy/authored matrix for drawing; collision stays static
 for(const auto& l:set_piece_tables(location).loops)if(l.resource==resource){runtimeFlags=(l.runtimeFlags&~0x40u)|0x20u;return true;}
 return false;
}
// The first instance with `resource` (a linear scan of ~3500 instances, run for every chair and spline piece every tick
// in every rider context): a (resource, first index) table sorted by resource answers it. It is rebuilt when the list is
// another one (object, buffer, size, first/last resource); a list that only grew (clones pushed at the end) gets the
// new entries merged in. Every answer is checked against the list, with the scan as the fallback.
struct SetPieceInstanceIndex {const void* bodies=nullptr;const void* data=nullptr;size_t size=0;uint32_t first=0,last=0;std::vector<std::pair<uint32_t,uint32_t>> byResource;};
RIDER_LOCAL static SetPieceInstanceIndex setPieceInstanceIndex;
static WorldCollisionInstance* set_piece_instance_scan(uint32_t resource){
 for(auto& i:browserBodies->instances)if(i.resource==resource)return &i;return nullptr;
}
static WorldCollisionInstance* set_piece_instance_at(uint32_t resource){
 if(!browserBodies)return nullptr;auto& list=browserBodies->instances;auto& x=setPieceInstanceIndex;
 if(list.empty())return nullptr;
 const auto before=[](const std::pair<uint32_t,uint32_t>& a,const std::pair<uint32_t,uint32_t>& b){return a.first!=b.first?a.first<b.first:a.second<b.second;};
 if(x.bodies!=browserBodies.get()||x.data!=list.data()||x.size!=list.size()||x.first!=list.front().resource||x.last!=list.back().resource){
  const bool grew=x.bodies==browserBodies.get()&&x.size&&list.size()>x.size&&x.first==list.front().resource&&x.last==list[x.size-1].resource;
  if(grew)for(size_t i=x.size;i<list.size();i++){const std::pair<uint32_t,uint32_t> e{list[i].resource,uint32_t(i)};x.byResource.insert(std::upper_bound(x.byResource.begin(),x.byResource.end(),e,before),e);}
  else{x.byResource.clear();for(size_t i=0;i<list.size();i++)x.byResource.emplace_back(list[i].resource,uint32_t(i));std::sort(x.byResource.begin(),x.byResource.end(),before);}
  x.bodies=browserBodies.get();x.data=list.data();x.size=list.size();x.first=list.front().resource;x.last=list.back().resource;
 }
 auto it=std::lower_bound(x.byResource.begin(),x.byResource.end(),std::pair<uint32_t,uint32_t>{resource,0u},before);
 if(it==x.byResource.end()||it->first!=resource)return set_piece_instance_scan(resource);
 auto& w=list[it->second];return w.resource==resource?&w:set_piece_instance_scan(resource);
}
#include "falling_billboard_collision.inc"
static collision_transform::Matrix set_piece_matrix(const ssx::RollerMatrix& rows){
 collision_transform::Matrix out;for(unsigned i=0;i<4;i++)for(unsigned k=0;k<4;k++)out[i*4+k]=rows[i][k];return out;
}
// Entity/static geometry of one chair instance on a new root: node matrices (body and ray
// queries) and bounds. The authored instance also gets the entity (334458/334888 entity route).
// Chair entities shared by the rider contexts (docs/sim-performance.md "Chair entities"): each rider context moves its own copy
// of every lift through the same matrices, and composeWorldCollisionEntity is a pure function of the instance's hierarchy
// (local matrices, parents, scale, node indices), the root and the bounds. The last composition per resource is kept here with
// every one of those inputs; a context placing that chair with bit-identical inputs takes the same (immutable) entity instead
// of composing it again. Not per rider on purpose (web/check-rider-globals.mjs): an entry is only ever reused on equal inputs.
struct ChairEntityShared {collision_transform::Matrix base{};terrain_original::Vector low{},high{};float scale=0;
 std::vector<collision_transform::Matrix> local;std::vector<uint32_t> parent,nodes;std::shared_ptr<const WorldCollisionEntity> entity;};
static std::unordered_map<uint32_t,ChairEntityShared> chairEntityShared;
static bool chair_bits_equal(const void* a,const void* b,size_t n){return std::memcmp(a,b,n)==0;}
static std::shared_ptr<const WorldCollisionEntity> chair_entity(const WorldCollisionInstance& instance,const collision_transform::Matrix& base,const terrain_original::Vector& low,const terrain_original::Vector& high){
 auto& e=chairEntityShared[instance.resource];
 bool same=e.entity&&chair_bits_equal(e.base.data(),base.data(),sizeof base)&&chair_bits_equal(e.low.data(),low.data(),sizeof low)&&chair_bits_equal(e.high.data(),high.data(),sizeof high)
  &&chair_bits_equal(&e.scale,&instance.scale,sizeof(float))&&e.local.size()==instance.hierarchyLocal.size()&&e.parent==instance.hierarchyParent&&e.nodes.size()==instance.nodes.size()
  &&(e.local.empty()||chair_bits_equal(e.local.data(),instance.hierarchyLocal.data(),e.local.size()*sizeof(collision_transform::Matrix)));
 for(size_t n=0;same&&n<e.nodes.size();n++)same=e.nodes[n]==instance.nodes[n].index;
 if(same)return e.entity;
 auto entity=std::make_shared<const WorldCollisionEntity>(composeWorldCollisionEntity(instance,base,low,high));
 e.base=base;e.low=low;e.high=high;e.scale=instance.scale;e.local=instance.hierarchyLocal;e.parent=instance.hierarchyParent;
 e.nodes.resize(instance.nodes.size());for(size_t n=0;n<e.nodes.size();n++)e.nodes[n]=instance.nodes[n].index;e.entity=entity;
 return entity;
}
static void place_chair(WorldCollisionInstance& instance,const ssx::RollerMatrix& root,const terrain_original::Vector& low,const terrain_original::Vector& high,bool entityRoute){
 auto entity=chair_entity(instance,set_piece_matrix(root),low,high);
 for(size_t n=0;n<instance.nodes.size();n++){instance.nodes[n].world=entity->world[n];instance.nodes[n].inverse=entity->inverse[n];}
 instance.low=low;instance.high=high;
 if(entityRoute)instance.entity=std::move(entity);
}
static void refresh_chairlift(BrowserChairlift& lift){
 if(!lift.active)return;
 auto* authored=set_piece_instance_at(lift.resource);if(!authored)throw std::runtime_error("Chairlift instance missing");
 if(authored->hierarchyLocal.empty())return; /* collision type 0 (Junction traffic): drawing only */
 const auto& cars=lift.modifier.cars;
 place_chair(*authored,cars[0].matrix,{cars[0].boundsMin[0],cars[0].boundsMin[1],cars[0].boundsMin[2]},{cars[0].boundsMax[0],cars[0].boundsMax[1],cars[0].boundsMax[2]},true);
 for(size_t k=0;k<lift.clones.size();k++){auto* clone=set_piece_instance_at(lift.clones[k]);if(!clone)throw std::runtime_error("Chairlift clone missing");place_chair(*clone,cars[k].matrix,cars[k].instanceLow,cars[k].instanceHigh,false);}
}
// After every world load and on a new race: clone instances (once) and the race-tick-0 state.
static void activate_multi_spline(BrowserChairlift& lift){
 auto* authored=set_piece_instance_at(lift.resource);if(!authored)return;
 // Clone ids are reused by later modifiers (Metro-City bins): an older modifier holding one is unloaded.
 for(auto& other:browserChairlifts)if(&other!=&lift&&other.active)for(uint32_t c:lift.clones)if(std::find(other.clones.begin(),other.clones.end(),c)!=other.clones.end()){
  other.active=false;if(auto* a=set_piece_instance_at(other.resource)){a->entity.reset();a->entityPointer=false;a->eventRuntimeFlags=(other.seed->runtimeFlags&~0x40u)|0x20u;}break;}
 for(size_t k=0;k<lift.clones.size();k++)if(auto* clone=set_piece_instance_at(lift.clones[k])){clone->eventRuntimeFlags=lift.seed->cars[k][1];clone->entityPointer=true;}
 authored->eventRuntimeFlags=lift.seed->runtimeFlags;authored->entityPointer=true;lift.active=true;refresh_chairlift(lift);
}
// A section MultiSpline the human's progress activated, replayed in a computer rider's core (web/shared_world.inc).
void browser_activate_section_lift(uint32_t resource){for(auto& lift:browserChairlifts)if(lift.resource==resource&&!lift.active){activate_multi_spline(lift);return;}}
void browser_attach_set_pieces(){
 if(!browserBodies)return;
 for(const auto& [r,f]:splinePieceCountdownFlags)spline_piece_collision_stop(r);splinePieceCountdownFlags.clear(); // new race: the train back to its countdown state
 browserChairlifts.clear();browserLoopSplines.clear();chairliftTicks=0;setPieceContacts.clear();browserSplinePieces.clear();splineOwnersFired.clear();splineLaunches=0;setPieceTriggerLog.clear();setPieceCourse=false;
 browser_reset_teeters();
 setPieceTables=&set_piece_tables(setPieceLocationName);
 auto f=[](uint32_t b){return std::bit_cast<float>(b);};
 for(const auto& seed:setPieceTables->multi){
  auto* authored=set_piece_instance_at(seed.resource);
  if(!authored||!authored->eventRuntimeFlags)continue; // not the event world
  BrowserChairlift lift;lift.resource=seed.resource;lift.seed=&seed;
  auto& m=lift.modifier;m.count=int32_t(seed.count);m.mode=int32_t(seed.mode);m.angle=f(seed.angle);m.distance=f(seed.distance);m.speed=f(seed.speed);m.radius=f(seed.radius);m.dirty=seed.dirty;
  m.path.resource=seed.path;m.path.index=int32_t(seed.cursor);m.path.cursor=seed.cursor;m.path.length=f(seed.length);m.path.record=nullptr;
  for(const auto& c:seed.cars){
   ssx::OriginalMultiSplineCar car;
   for(unsigned i=0;i<4;i++){car.position[i]=f(c[2+i]);car.tangent[i]=f(c[6+i]);car.curvature[i]=f(c[10+i]);car.boundsMin[i]=f(c[14+i]);car.boundsMax[i]=f(c[18+i]);}
   car.tangentXX=f(c[22]);car.tangentYY=f(c[23]);car.record58=c[24];car.record5C=c[25];
   for(unsigned i=0;i<4;i++)for(unsigned j=0;j<4;j++)car.matrix[i][j]=f(c[26+i*4+j]);
   for(unsigned i=0;i<3;i++){car.instanceLow[i]=f(c[42+i]);car.instanceHigh[i]=f(c[45+i]);}
   m.cars.push_back(car);lift.clones.push_back(c[0]);
   // 0x351170/0x351270 clones: a copy of the authored instance with its own resource id and flags.
   auto* clone=set_piece_instance_at(c[0]);
   if(!clone){
    const size_t at=size_t(authored-browserBodies->instances.data());
    WorldCollisionInstance copy=browserBodies->instances[at];copy.resource=c[0];copy.entity.reset();copy.runtimeClone=true;
    browserBodies->instances.push_back(std::move(copy));clone=&browserBodies->instances.back();authored=set_piece_instance_at(seed.resource);
    clone->eventRuntimeFlags=0u; /* not collidable until its modifier is active */clone->entityPointer=false;
   }
  }
  browserChairlifts.push_back(std::move(lift));setPieceCourse=true;
 }
 for(auto& lift:browserChairlifts){ /* new race: every modifier back to its pre-activation state */
  for(uint32_t c:lift.clones)if(auto* clone=set_piece_instance_at(c)){clone->eventRuntimeFlags=0u;clone->entityPointer=false;}
  if(auto* a=set_piece_instance_at(lift.resource)){a->entity.reset();a->entityPointer=false;a->eventRuntimeFlags=(lift.seed->runtimeFlags&~0x40u)|0x20u;}
 }
 for(auto& lift:browserChairlifts)if(lift.seed->activation==0)activate_multi_spline(lift);
 for(const auto& seed:setPieceTables->loops){BrowserLoopSpline l;l.seed=&seed;browserLoopSplines.push_back(l);setPieceCourse=true;if(auto* a=set_piece_instance_at(seed.resource))a->entityPointer=false;}
 if(!setPieceTables->pieces.empty()||!setPieceTables->owners.empty())setPieceCourse=true;
 attached_reset();reset_rail_livecomp_collision();
 section_reset(); // section activation back to the race-tick-0 entity set
}
// race_begin, before the rider: 0x35A560 then the LiveComp 0x3568B0 (clock dt 1/60 = 0x3C888889).
static void advance_set_pieces(){
 browser_advance_teeters(); // log teeters 0x342358 (entity pass); falling billboard LiveComp 0x341D48
 refresh_rail_livecomp_collision(); // its collision nodes follow node 0 (0x334888)
 // 0x356198 -> 0x355748 -> 0x352D20 -> RailModifier 0x35C4E0 runs before the object's 0x3568B0 has refreshed the Spline
 // bounds (+0x10/+0x20): the R&B boxcar rails take the previous entity pass's bounds (web/rail_dynamic.inc).
 browser_dynamic_rails_tick();
 for(auto& piece:browserSplinePieces){
  if(piece.phase==1){
   ssx::originalSplineUpdate(piece.spline);
   if(ssx::originalSplineTakeFinished(piece.spline)){piece.position=ssx::originalSplineFreeze(piece.spline);piece.phase=2;ssx::originalPositionEntityBounds(piece.position);stage_world_spline_finished(piece.seed->resource);} /* slot 4 (builtin16 burst) after the entity pass: web/stage_world.inc */
   else ssx::originalSplineEntityBounds(piece.spline);
  }else if(piece.phase==2)ssx::originalPositionEntityBounds(piece.position);
  spline_piece_place(piece); // collidable pieces (R&B train) follow their modifier
 }
 attached_tick(); // LiveComps of the spline pieces / looping splines, ParentModifier updates
 // Raven flyby slot 1 (section activation, approximated by the human's course progress).
 // Only the human's core activates sections (the camera follows the human); computer riders' cores get it replayed.
 if(const float* progress=race_progress_info();!sectionsRunning&&setPieceCourse&&setPieceTables&&setPieceTables->sectionRemaining>0&&browserHumanRider&&progress&&progress[0]>0&&progress[0]<=setPieceTables->sectionRemaining)
  for(const auto& seed:setPieceTables->pieces)if(seed.slot==1)launch_spline_pieces(seed.owner,1);
 // Section-streamed MultiSplines (Metro-City bins): built when the human's course remaining reaches the
 // captured activation distance (end of the previous tick); the first update runs in this tick.
 if(const float* progress=race_progress_info();!sectionsRunning&&browserHumanRider&&progress&&progress[0]>0)
  for(auto& lift:browserChairlifts)if(!lift.active&&lift.seed->activation==1&&lift.modifier.distance==std::bit_cast<float>(lift.seed->distance)&&progress[0]<=lift.seed->activationRemaining){activate_multi_spline(lift);shared_world_log({5u,lift.resource});}
 for(auto& loop:browserLoopSplines){
  if(!loop.ready){const auto* record=browserRailRecord(loop.seed->path);if(!record)continue;loop.spline=ssx::originalSplineModifierFromBytes(reinterpret_cast<const uint8_t*>(loop.seed->bytes.data()),record,0);loop.ready=true;}
  ssx::originalSplineUpdate(loop.spline);ssx::originalSplineEntityBounds(loop.spline);
 }
 for(auto& lift:browserChairlifts){
  if(!lift.active)continue;
  // 0x356198: an Object-entity MultiSpline whose section count reached 0 (a leave scan at the end of the previous tick) runs its slot-3
  // program, or is destroyed (0x34FD90) before its modifier updates (PS2 setpieces-bra2 bin 707856: leave at 11341, gone at 11342).
  if(lift.objectOwner&&lift.refs<=0&&browserHumanRider){const int32_t slot3=stage_slot_program(lift.resource,3);
   if(slot3>=0)stage_world_section_leave_program(lift.resource,slot3);
   else{const uint32_t r=lift.resource;browser_release_section_lift(r);section_multi_spline_destroyed(r);shared_world_log({7u,r});continue;}}
  // The spline catalog (init_rails) may be absent in harnesses that load only the collision world:
  // the chairs then keep their race-tick-0 placement.
  if(!lift.modifier.path.record){lift.modifier.path.record=browserRailRecord(lift.modifier.path.resource);if(!lift.modifier.path.record)continue;}
  ssx::originalMultiSplineUpdate(lift.modifier,std::bit_cast<float>(0x3C888889u));multi_spline_evaluate_shared(lift.modifier,lift.resource);ssx::originalMultiSplineBounds(lift.modifier);refresh_chairlift(lift);
 }
 ++chairliftTicks;
}
static BrowserChairlift* chairlift_of(uint32_t resource,size_t* car=nullptr){
 for(auto& lift:browserChairlifts){
  if(!lift.active)continue;
  if(lift.resource==resource){if(car)*car=0;return &lift;} // the authored instance is car 0 (entity matrix = clone 0)
  for(size_t k=0;k<lift.clones.size();k++)if(lift.clones[k]==resource){if(car)*car=k;return &lift;}
 }
 return nullptr;
}
// 104E70 selected-entity callback: LiveComp 0x34E698 -> modifier+0xB4 0x35B200 (car lookup by the
// packet's instance among the clones; the authored instance is not in the clone list, so it adds nothing).
static bool chairlift_selected(OriginalInstanceContactPacket& packet){
 size_t car=0;auto* lift=chairlift_of(packet.instance,&car);if(!lift)return false;
 if(packet.instance==lift->resource)return true;
 auto v=ssx::originalMultiSplineContactVelocity(lift->modifier,car,{packet.point[0],packet.point[1],packet.point[2],packet.point[3]});
 OriginalRounding rounding;for(unsigned k=0;k<4;k++)packet.surfaceVelocity[k]=terrain_original::add(packet.surfaceVelocity[k],v.linear[k]);
 return true;
}
// Draw deltas: car k of a lift under key resource + k * 2^20 (web/moving-instances.js clones the
// authored draw batches for k > 0).
static void chairlift_moving(std::vector<float>& out,void(*append)(std::vector<float>&,float,const collision_transform::Matrix&,const collision_transform::Matrix&)){
 for(auto& piece:browserSplinePieces){
  if(!piece.seed->drawn||piece.phase!=1)continue;
  collision_transform::Matrix authored;for(unsigned i=0;i<16;i++)authored[i]=std::bit_cast<float>(piece.seed->matrix[i]);
  append(out,float(piece.seed->resource),set_piece_matrix(ssx::originalSplineMatrix(piece.spline)),authored);
 }
 for(auto& loop:browserLoopSplines){
  if(!loop.ready||!(loop.seed->runtimeFlags&1u))continue; /* drawn (flags bit0) */
  const auto* authored=set_piece_instance_at(loop.seed->resource);if(!authored)continue;
  append(out,float(loop.seed->resource),set_piece_matrix(ssx::originalSplineMatrix(loop.spline)),authored->authoredMatrix);
 }
 for(const auto& lift:browserChairlifts){
  if(!lift.active)continue;
  const auto* authored=set_piece_instance_at(lift.resource);if(!authored)continue;
  for(size_t k=0;k<lift.modifier.cars.size();k++)append(out,float(lift.resource+k*1048576u),set_piece_matrix(lift.modifier.cars[k].matrix),authored->authoredMatrix);
 }
}
// Diagnostics: count, ticks, then per lift: resource, distance, dirty, and per car translation (x,y,z).
extern "C" EMSCRIPTEN_KEEPALIVE float* set_piece_info(){
 RIDER_LOCAL static std::vector<float> out;out={float(browserChairlifts.size()),float(chairliftTicks)};
 for(const auto& l:browserChairlifts){out.push_back(float(l.resource));out.push_back(l.modifier.distance);out.push_back(float(l.modifier.dirty));
  for(const auto& c:l.modifier.cars)for(unsigned k=0;k<3;k++)out.push_back(c.matrix[3][k]);}
 return out.data();
}
// Raw bits for the capture comparer: per lift distance bits, then per car the 16 matrix words.
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* set_piece_bits(){
 RIDER_LOCAL static std::vector<uint32_t> out;out={uint32_t(browserChairlifts.size())};
 for(const auto& l:browserChairlifts){out.push_back(std::bit_cast<uint32_t>(l.modifier.distance));
  for(const auto& c:l.modifier.cars)for(unsigned i=0;i<4;i++)for(unsigned k=0;k<4;k++)out.push_back(std::bit_cast<uint32_t>(c.matrix[i][k]));}
 return out.data();
}
// Contact log: count, then (tick, resource) pairs since the attach.
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* set_piece_contacts(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,uint32_t(setPieceContacts.size()/2));out.insert(out.end(),setPieceContacts.begin(),setPieceContacts.end());return out.data();
}
// Log teeters: count, ticks, forces, max |time|, then per teeter: resource and the node-1 draw delta
// (three.js column-major, native space) of T = rest^-1 * current (static world -> current world).
extern "C" EMSCRIPTEN_KEEPALIVE float* set_piece_teeters(){
 RIDER_LOCAL static std::vector<float> out;auto info=browser_teeter_info();out.assign(info.begin(),info.end());
 for(const auto& [resource,t]:browser_teeter_transforms()){
  out.push_back(float(resource));
  const int axis[3]={0,2,1};const double sign[3]={1,1,-1};float e[16]={};
  for(int a=0;a<3;a++)for(int b=0;b<3;b++)e[b*4+a]=float(sign[a]*sign[b]*double(t[axis[b]][axis[a]]));
  for(int a=0;a<3;a++)e[12+a]=float(sign[a]*double(t[3][axis[a]])/100);
  e[15]=1;out.insert(out.end(),e,e+16);
 }
 return out.data();
}
// Spline pieces: count, launches, then per piece (7): resource, phase (1 spline, 2 frozen), launch tick, distance, translation.
extern "C" EMSCRIPTEN_KEEPALIVE float* set_piece_splines(){
 RIDER_LOCAL static std::vector<float> out;out={float(browserSplinePieces.size()),float(splineLaunches)};
 for(auto& p:browserSplinePieces){out.push_back(float(p.seed->resource));out.push_back(float(p.phase));out.push_back(float(p.launchTick));out.push_back(p.spline.distance);
  const auto& m=p.phase==2?ssx::originalPositionMatrix(p.position):ssx::originalSplineMatrix(p.spline);for(unsigned k=0;k<3;k++)out.push_back(m[3][k]);}
 return out.data();
}
// Fired trigger log: count, then (tick, owner * 8 + slot) pairs.
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* set_piece_triggers(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,uint32_t(setPieceTriggerLog.size()/2));out.insert(out.end(),setPieceTriggerLog.begin(),setPieceTriggerLog.end());return out.data();
}
// All MultiSpline modifiers of the attached location: count, then per modifier: resource, active, car
// count, distance bits, and 16 matrix words per car.
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* set_piece_multi_bits(){
 RIDER_LOCAL static std::vector<uint32_t> out;out={uint32_t(browserChairlifts.size())};
 for(const auto& l:browserChairlifts){out.push_back(l.resource);out.push_back(l.active);out.push_back(uint32_t(l.modifier.cars.size()));out.push_back(std::bit_cast<uint32_t>(l.modifier.distance));
  for(const auto& c:l.modifier.cars)for(unsigned i=0;i<4;i++)for(unsigned k=0;k<4;k++)out.push_back(std::bit_cast<uint32_t>(c.matrix[i][k]));}
 return out.data();
}
#include "section_gameplay.inc"
// Section enter (slot 1, end of tick E): core-side pieces of the program's instance. The JS players
// (LiveComp, flags, UV scroll) start from the section log.
static void section_start_piece(uint32_t resource,int32_t program){
 stage_world_section_enter(resource,program); // the program's particle/sound builtins (web/stage_world.inc)
 if(!setPieceTables)return;
 for(const auto& seed:setPieceTables->pieces)if(seed.owner==resource&&seed.slot==1){launch_spline_pieces(resource,1,false);break;} // raven flyby (program 54)
 for(auto& lift:browserChairlifts)if(lift.resource==resource&&!lift.active&&lift.seed->activation==1){activate_multi_spline(lift);lift.refs=1;lift.objectOwner=!stage_slot_calls(resource,1,3);shared_world_log({5u,resource});} // Metro-City bins (replayed in the computer riders' cores); the instance is listed: +0x34 = 1
}
// Section enter / leave of a MultiSpline's instance (0x30A3A0 / 0x30A460 -> vt+0xA8 / vt+0xB0 -> 0x35AAD0 / 0x35AAE0): the count only.
static void section_multi_spline_count(uint32_t resource,int delta){for(auto& lift:browserChairlifts)if(lift.resource==resource&&lift.active)lift.refs+=delta;}
// Section leave: 0x34FD90 destroys the entity (spline pieces); the Metro-City bins' MultiSpline is gone
// after its leave scan in the captures (11340, flags back to static).
void browser_release_section_lift(uint32_t resource){ // also shared-world event 7 (web/shared_world.inc)
 for(auto& lift:browserChairlifts)if(lift.resource==resource&&lift.active&&lift.seed->activation==1){
  lift.active=false;
  for(uint32_t c:lift.clones)if(auto* clone=set_piece_instance_at(c)){clone->eventRuntimeFlags=0u;clone->entityPointer=false;}
  if(auto* a=set_piece_instance_at(lift.resource)){a->entity.reset();a->entityPointer=false;a->eventRuntimeFlags=(lift.seed->runtimeFlags&~0x40u)|0x20u;}
 }
}
static void section_stop_piece(uint32_t resource,bool multiSplineRelease){
 stage_world_section_leave(resource); // 0x34FD90: the entity's effects / LiveComp go with it
 browser_dynamic_rails_unbind(resource);browser_location_teeter_section(resource,false); // its RailModifiers / the location teeter (web/rail_dynamic.inc)
 const BrowserSplinePieceSeed* moved=nullptr; // the destroyed entity carried a Spline / Position modifier
 for(size_t k=0;k<browserSplinePieces.size();)if(browserSplinePieces[k].seed->resource==resource){
  const auto* seed=browserSplinePieces[k].seed;moved=seed;browserSplinePieces.erase(browserSplinePieces.begin()+long(k));spline_piece_collision_stop(seed->resource);
  splineOwnersFired.erase(std::remove(splineOwnersFired.begin(),splineOwnersFired.end(),seed->owner*8+seed->slot),splineOwnersFired.end());
 }else ++k;
 if(const auto* seed=loop_relaunch_seed(resource))
  for(size_t k=0;k<browserLoopSplines.size();)if(browserLoopSplines[k].seed->resource==resource){browserLoopSplines.erase(browserLoopSplines.begin()+long(k));moved=seed;}else ++k;
 if(moved){ // entity dtor 0x3553C0 -> 0x3567E0: the instance goes back to the octree cell of its own bounds (+0x60/+0x6C)
  attached_destroy(resource);auto f=[](uint32_t b){return std::bit_cast<float>(b);};
  sectionActivation.relocate(resource,{f(moved->low[0]),f(moved->low[1]),f(moved->low[2])},{f(moved->high[0]),f(moved->high[1]),f(moved->high[2])});
 }
 if(multiSplineRelease)for(auto& lift:browserChairlifts)if(lift.resource==resource&&lift.active&&lift.seed->activation==1){browser_release_section_lift(resource);shared_world_log({7u,resource});}
}
