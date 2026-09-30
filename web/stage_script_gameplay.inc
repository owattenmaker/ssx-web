#include "streamed_world.hpp" // browser_streamed_world: PEAK1..PEAK3
// Course stage scripts: the trigger programs a selected rider contact runs (docs/stage-scripts.md).
// Included by web/animation_bridge.cpp after score_gameplay.inc.
//
// Original chain (every loaded location track has its own stage; tools/export_stage_scripts.py ->
// generated/stage_scripts_seed.hpp):
// * 105398's store step copies the selected contact packet to rider+9E0 (instance at +0x50 = rider+A30)
//   and the contact record to rider+A60 when the hit node has flag bit 1 and instance+8 lacks 0x2000
//   (engine/instance_contact.hpp; web/instance_contact_gameplay.inc host.store -> stage_trigger_store).
// * 128AC0's per-rider pass runs 121818 after the motion: instance+0xC entity ? entity vtable+0x140
//   (Object 0x490E80 / LiveComp 0x490B10 / emitter 0x48EE60: 355770; flag cloth, type-16 node, DeadNode,
//   Debounce 0x4906F0 and RestoreNode 0x491680: no-ops) : 30A060. 355770: with a modifier, modifier
//   vt+0x54 (refresh) and the predicate vt+0x4C(2D1B30(rider)) (0 rejects); entity+0x20 > 0 rejects;
//   entity+0x20 = clockRate/2, then 34FE00 -> 2D19B8 -> 30A060. 120E30 clears rider+A30 at the end.
//   The browser runs this from race_end (web/race_bridge.cpp), before 125228 and the 117C28 score tick.
// * 30A060(context *(gp+0xCE8), rider+A60, rider+9E0, rider+6C0): current player = the rider's index
//   (+0x86C); a human rider (+0x874) with +0x480 set (DNF: time-out / Give Up) fires nothing unless the game mode
//   (*(gp-0x848)+0x84 +0x214) is 4 (free ride; events are 3). Handler slot 2 of the instance (3A6B78),
//   run through 309C88 (the LUN VM), then entity vt+0x148 355858: entity+0x20 = int(clockRate * 0.5).
// * 356198 (entity update, entity pass before the rider) decrements entity+0x20 while positive.
// Builtins with gameplay effects are ported here; builtins whose effects are owned by other browser
// systems (LiveComp/Spline/MultiSpline/UVScroll/flags/particles: set_piece_gameplay.inc and the JS
// renderers; crashbag RollerModifier: roller_gameplay.inc) are no-ops that only count calls.
#include "generated/stage_scripts_seed.hpp"
#include "../engine/stage_script_vm.hpp"
#include <span>
struct StageCourseTables {
 std::span<const BrowserStageInfo> stages;std::span<const BrowserStageProgram> programs;std::span<const uint32_t> words;
 std::span<const int16_t> globals;std::span<const BrowserStageHandler> handlers;std::span<const std::array<uint32_t,2>> entities;
 int32_t collectibleAward=500;
};
template<class NS> static StageCourseTables stage_course_tables(){return {NS::stages,NS::programs,NS::words,NS::globals,NS::handlers,NS::entities,NS::collectible_award};}
struct StageCourseARA1{static constexpr const auto& stages=browser_stage_ara1::stages;static constexpr const auto& programs=browser_stage_ara1::programs;static constexpr const auto& words=browser_stage_ara1::words;static constexpr const auto& globals=browser_stage_ara1::globals;static constexpr const auto& handlers=browser_stage_ara1::handlers;static constexpr const auto& entities=browser_stage_ara1::entities;static constexpr int32_t collectible_award=browser_stage_ara1::collectible_award;};
struct StageCourseBRA2{static constexpr const auto& stages=browser_stage_bra2::stages;static constexpr const auto& programs=browser_stage_bra2::programs;static constexpr const auto& words=browser_stage_bra2::words;static constexpr const auto& globals=browser_stage_bra2::globals;static constexpr const auto& handlers=browser_stage_bra2::handlers;static constexpr const auto& entities=browser_stage_bra2::entities;static constexpr int32_t collectible_award=browser_stage_bra2::collectible_award;};
struct StageCourseBHP1{static constexpr const auto& stages=browser_stage_bhp1::stages;static constexpr const auto& programs=browser_stage_bhp1::programs;static constexpr const auto& words=browser_stage_bhp1::words;static constexpr const auto& globals=browser_stage_bhp1::globals;static constexpr const auto& handlers=browser_stage_bhp1::handlers;static constexpr const auto& entities=browser_stage_bhp1::entities;static constexpr int32_t collectible_award=browser_stage_bhp1::collectible_award;};
struct StageCourseASS1{static constexpr const auto& stages=browser_stage_ass1::stages;static constexpr const auto& programs=browser_stage_ass1::programs;static constexpr const auto& words=browser_stage_ass1::words;static constexpr const auto& globals=browser_stage_ass1::globals;static constexpr const auto& handlers=browser_stage_ass1::handlers;static constexpr const auto& entities=browser_stage_ass1::entities;static constexpr int32_t collectible_award=browser_stage_ass1::collectible_award;};
struct StageCourseABA1{static constexpr const auto& stages=browser_stage_aba1::stages;static constexpr const auto& programs=browser_stage_aba1::programs;static constexpr const auto& words=browser_stage_aba1::words;static constexpr const auto& globals=browser_stage_aba1::globals;static constexpr const auto& handlers=browser_stage_aba1::handlers;static constexpr const auto& entities=browser_stage_aba1::entities;static constexpr int32_t collectible_award=browser_stage_aba1::collectible_award;};
struct StageCourseABC1{static constexpr const auto& stages=browser_stage_abc1::stages;static constexpr const auto& programs=browser_stage_abc1::programs;static constexpr const auto& words=browser_stage_abc1::words;static constexpr const auto& globals=browser_stage_abc1::globals;static constexpr const auto& handlers=browser_stage_abc1::handlers;static constexpr const auto& entities=browser_stage_abc1::entities;static constexpr int32_t collectible_award=browser_stage_abc1::collectible_award;};
// Courses beyond Peak 1 (tools/export_stage_scripts.py courses(): BROWSER_STAGE_HAVE_<CODE> once exported).
#define BROWSER_STAGE_COURSE(Name,ns) struct Name{static constexpr const auto& stages=ns::stages;static constexpr const auto& programs=ns::programs;static constexpr const auto& words=ns::words;static constexpr const auto& globals=ns::globals;static constexpr const auto& handlers=ns::handlers;static constexpr const auto& entities=ns::entities;static constexpr int32_t collectible_award=ns::collectible_award;};
// Peak 2 (docs/peak2.md)
#ifdef BROWSER_STAGE_HAVE_CRA3
BROWSER_STAGE_COURSE(StageCourseCRA3,browser_stage_cra3)
#endif
#ifdef BROWSER_STAGE_HAVE_DRA4
BROWSER_STAGE_COURSE(StageCourseDRA4,browser_stage_dra4)
#endif
#ifdef BROWSER_STAGE_HAVE_DSS2
BROWSER_STAGE_COURSE(StageCourseDSS2,browser_stage_dss2)
#endif
#ifdef BROWSER_STAGE_HAVE_CBA2
BROWSER_STAGE_COURSE(StageCourseCBA2,browser_stage_cba2)
#endif
#ifdef BROWSER_STAGE_HAVE_CHP2
BROWSER_STAGE_COURSE(StageCourseCHP2,browser_stage_chp2)
#endif
#ifdef BROWSER_STAGE_HAVE_DBC2
BROWSER_STAGE_COURSE(StageCourseDBC2,browser_stage_dbc2)
#endif
// Peak 3 (docs/peak3.md)
#ifdef BROWSER_STAGE_HAVE_ERA5
BROWSER_STAGE_COURSE(StageCourseERA5,browser_stage_era5)
#endif
#ifdef BROWSER_STAGE_HAVE_ESS3
BROWSER_STAGE_COURSE(StageCourseESS3,browser_stage_ess3)
#endif
#ifdef BROWSER_STAGE_HAVE_EBA3
BROWSER_STAGE_COURSE(StageCourseEBA3,browser_stage_eba3)
#endif
#ifdef BROWSER_STAGE_HAVE_EHP3
BROWSER_STAGE_COURSE(StageCourseEHP3,browser_stage_ehp3)
#endif
#ifdef BROWSER_STAGE_HAVE_EBC3
BROWSER_STAGE_COURSE(StageCourseEBC3,browser_stage_ebc3)
#endif
#if __has_include("generated/peak_stage_seed.hpp")
#include "generated/peak_stage_seed.hpp" // tools/export_peak_stage.py
struct StageCoursePEAK1{static constexpr const auto& stages=browser_stage_peak1::stages;static constexpr const auto& programs=browser_stage_peak1::programs;static constexpr const auto& words=browser_stage_peak1::words;static constexpr const auto& globals=browser_stage_peak1::globals;static constexpr const auto& handlers=browser_stage_peak1::handlers;static constexpr const auto& entities=browser_stage_peak1::entities;static constexpr int32_t collectible_award=browser_stage_peak1::collectible_award;};
#endif
#if __has_include("generated/peak2_stage_seed.hpp")
#include "generated/peak2_stage_seed.hpp" // tools/export_peak_stage.py --peak 2 (docs/peak2.md)
BROWSER_STAGE_COURSE(StageCoursePEAK2,browser_stage_peak2)
#endif
#if __has_include("generated/peak3_stage_seed.hpp")
#include "generated/peak3_stage_seed.hpp" // tools/export_peak_stage.py --peak 3 (docs/peak3.md)
BROWSER_STAGE_COURSE(StageCoursePEAK3,browser_stage_peak3)
#endif
#if __has_include("generated/mountain_stage_seed.hpp")
#include "generated/mountain_stage_seed.hpp" // tools/export_peak_stage.py --mountain: the whole mountain (docs/peak3.md section 6)
BROWSER_STAGE_COURSE(StageCourseMOUNTAIN,browser_stage_mountain)
#endif
RIDER_LOCAL int browserEventKind=-1; // 0x535C10 when a host sets it (rival challenges 5/6: event_kind below, docs/backcountry.md; free ride 4, peak runs 5/6: web/peak_world.inc); -1 = the event course's
extern "C" EMSCRIPTEN_KEEPALIVE void event_kind(int kind){browserEventKind=kind;} // 0x535C10 of the loaded event (-1: the course default)
static std::optional<StageCourseTables> stage_tables_for(const std::string& location){
 if(location=="ARA1")return stage_course_tables<StageCourseARA1>();
 if(location=="BRA2")return stage_course_tables<StageCourseBRA2>();
 if(location=="BHP1")return stage_course_tables<StageCourseBHP1>();
 if(location=="ASS1")return stage_course_tables<StageCourseASS1>(); // R&B slopestyle
 if(location=="ABA1")return stage_course_tables<StageCourseABA1>(); // Crow's Nest big air
 if(location=="ABC1")return stage_course_tables<StageCourseABC1>(); // Happiness backcountry (docs/backcountry.md)
#ifdef BROWSER_STAGE_HAVE_CRA3
 if(location=="CRA3")return stage_course_tables<StageCourseCRA3>(); // Ruthless Ridge race (docs/peak2.md)
#endif
#ifdef BROWSER_STAGE_HAVE_DRA4
 if(location=="DRA4")return stage_course_tables<StageCourseDRA4>(); // Intimidator race
#endif
#ifdef BROWSER_STAGE_HAVE_DSS2
 if(location=="DSS2")return stage_course_tables<StageCourseDSS2>(); // Style Mile slopestyle
#endif
#ifdef BROWSER_STAGE_HAVE_CBA2
 if(location=="CBA2")return stage_course_tables<StageCourseCBA2>(); // Launch Time big air
#endif
#ifdef BROWSER_STAGE_HAVE_CHP2
 if(location=="CHP2")return stage_course_tables<StageCourseCHP2>(); // Schizophrenia super pipe
#endif
#ifdef BROWSER_STAGE_HAVE_DBC2
 if(location=="DBC2")return stage_course_tables<StageCourseDBC2>(); // Ruthless backcountry
#endif
#ifdef BROWSER_STAGE_HAVE_ERA5
 if(location=="ERA5")return stage_course_tables<StageCourseERA5>(); // Gravitude race
#endif
#ifdef BROWSER_STAGE_HAVE_ESS3
 if(location=="ESS3")return stage_course_tables<StageCourseESS3>(); // Kick Doubt slopestyle
#endif
#ifdef BROWSER_STAGE_HAVE_EBA3
 if(location=="EBA3")return stage_course_tables<StageCourseEBA3>(); // Much-2-Much big air
#endif
#ifdef BROWSER_STAGE_HAVE_EHP3
 if(location=="EHP3")return stage_course_tables<StageCourseEHP3>(); // Perpendiculous super pipe
#endif
#ifdef BROWSER_STAGE_HAVE_EBC3
 if(location=="EBC3")return stage_course_tables<StageCourseEBC3>(); // The Throne backcountry
#endif
#if __has_include("generated/peak_stage_seed.hpp")
 if(location=="PEAK1")return stage_course_tables<StageCoursePEAK1>(); // streamed Peak 1 world (web/peak_world.inc)
#endif
#if __has_include("generated/peak2_stage_seed.hpp")
 if(location=="PEAK2")return stage_course_tables<StageCoursePEAK2>(); // streamed Peak 2 world (docs/peak2.md)
#endif
#if __has_include("generated/peak3_stage_seed.hpp")
 if(location=="PEAK3")return stage_course_tables<StageCoursePEAK3>(); // streamed Peak 3 world (docs/peak3.md)
#endif
#if __has_include("generated/mountain_stage_seed.hpp")
 if(browser_mountain_world(location))return stage_course_tables<StageCourseMOUNTAIN>(); // the whole mountain (docs/peak3.md section 6)
#endif
 return std::nullopt;
}
// 0x535C10 of an event course's own Single Event (the kinds the Peak 1 countdown anchors hold, keyed by the location
// code's event letters 2..3, which name the event on every peak: RA race 0, SS slope style 1, BA big air 2, HP half pipe
// 3, BC backcountry = Rival Time 5; Peak 2 anchors: docs/peak2.md); anything else (PEAK1, hubs) 0.
static int32_t stage_course_event_kind(const std::string& l){
 if(l.size()!=4)return 0;const std::string k=l.substr(1,2);
 return k=="SS"?1:k=="BA"?2:k=="HP"?3:k=="BC"?5:0;}
// Contact class of instance+0xC at 121818: -1 no entity (30A060 directly), 0 no-op entity, 1 the 355770 gate.
enum StageEntity:int {StageNoEntity=-1,StageNoopEntity=0,StageGateEntity=1};
struct StageInstance {
 uint32_t resource=0;const BrowserStageHandler* handler=nullptr;int entity=StageNoEntity;bool dead=false; // dead: DeadNode (type 6) since the race start
 int32_t guard=0;                       // entity+0x20 (355770 / 355858 / 356198)
 bool debounce=false;int32_t debounceTicks=0,debounceRestore=1; // Debounce component 0x4906F0 (+0x2C ticks, +0x30 restore)
 int entityBeforeDebounce=StageNoEntity;bool restored=false;   // RestoreNode (type 19) after restore 3
 bool rejects=false;                    // modifier predicate vt+0x4C returns 0 (RollerModifier 361CF8)
 uint32_t flags=0;bool flagsKnown=false; // instance+8 low bits changed by builtins (draw visibility)
 uint32_t fired=0;
};
RIDER_LOCAL static std::optional<StageCourseTables> stageTables;RIDER_LOCAL static std::string stageTablesLocation;
RIDER_LOCAL static std::vector<StageInstance> stageInstances; // sorted by resource (handler order)
struct StagePendingContact {uint32_t resource=0;OriginalInstanceContactPacket packet;OriginalInstanceContactRecord record;};
RIDER_LOCAL static std::optional<StagePendingContact> stagePending; // rider+A30/9E0/A60
RIDER_LOCAL static std::optional<OriginalInstanceContactRecord> stagePendingRecord; // the record of the contact being dispatched (context +0x294)
RIDER_LOCAL static uint32_t stageDispatches=0,stagePrograms=0,stageUnsupported=0,stageLastResource=0,stageLastProgram=0;
RIDER_LOCAL static std::array<uint32_t,128> stageBuiltinCalls{};
RIDER_LOCAL static std::vector<uint32_t> stageFiredLog; // (tick, resource, program) triples: diagnostics / computer-rider sharing
// LUN VM (engine/stage_script_vm.hpp, 2227D0) with every stage program, and the per-track globals table
// *(ctx+0x3CC+4*track) that 309C88 passes to handler programs (ctx+0x2A4/+0x2A8 are 0 in a race).
RIDER_LOCAL static std::optional<ssx::OriginalScriptVM> stageVm;
RIDER_LOCAL static std::vector<uint32_t> stageProgramHandles;                 // by global program index
RIDER_LOCAL static std::vector<std::pair<uint32_t,ssx::OriginalScriptValue>> stageTrackGlobals; // track -> table value
// Collectible set ctx+0x2C0 (one 0x10C slot): 30B928 (builtin 37) opens it, 30C4A8 (builtin 38) appends,
// 30B9A0 (builtin 39) awards a listed contact. Built by a stage global program (ARA1 track 8 program 26).
struct StageCollection {int32_t id=-1,expected=0;std::vector<uint32_t> resources;};
RIDER_LOCAL static StageCollection stageCollection;
RIDER_LOCAL static std::vector<uint32_t> stageCollected;                      // 153B00 marks (index) this race
// 0x535C11 game path (0 Conquer the Mountain, 1 Single Event, 2 Multiplayer) and the career collect row of the course
// (character block C+4+12*course: bits = list index), set by the host before each race start (set_stage_collect_state).
RIDER_LOCAL static uint32_t stageCollectPath=1;RIDER_LOCAL static uint32_t stageCollectMask[2]={0,0};
// Streamed Peak 1 world: the slot belongs to the stage (track) that opened it; each location's career row by track (web/stage-collect.js
// set_stage_collect_row), the stage setup runs at the location's read completion and the slot is freed at its unload start.
RIDER_LOCAL static int32_t stageCollectionTrack=-1,stageRunningTrack=-1;RIDER_LOCAL_LAZY static std::map<uint32_t,std::array<uint32_t,2>> stageCollectRows;
RIDER_LOCAL static std::vector<uint32_t> stageCollectEvents;                  // (index, amount) per human collect, drained by stage_collect_events()
void section_collectible_state(uint32_t resource,bool dead);void section_mark_dead(uint32_t resource); // web/section_gameplay.inc
RIDER_LOCAL static bool stage_current_human=false;                            // player object of ctx+0 (rider+0x874)
RIDER_LOCAL static uint32_t stageCollectibleAwards=0;
static StageInstance* stage_instance(uint32_t resource){
 auto it=std::lower_bound(stageInstances.begin(),stageInstances.end(),resource,[](const StageInstance& s,uint32_t r){return s.resource<r;});
 return it!=stageInstances.end()&&it->resource==resource?&*it:nullptr;
}
static const BrowserStageInfo* stage_of(uint32_t resource){
 if(!stageTables)return nullptr;for(const auto& s:stageTables->stages)if(s.track==(resource&255u))return &s;return nullptr;
}
static void stage_vm_reset();static void stage_world_reset();static void stage_note_countdown_flags(uint32_t resource); // web/stage_world.inc
static void avalanche_reset(); // web/avalanche_gameplay.inc
void browser_mission_tick();void browser_mission_rider_snapshot();void browser_mission_contact(uint32_t);void browser_mission_track_teardown(uint32_t);void browser_mission_reset(); // web/mission_gameplay.inc (Big Challenges)
RIDER_LOCAL extern bool sharedWorldReplay; // web/shared_world.inc
// World selection and new race: countdown entities (the countdown audit) and no pending contact.
void browser_reset_stage_scripts(){
 if(stageTablesLocation!=setPieceLocationName){stageTablesLocation=setPieceLocationName;stageTables=stage_tables_for(setPieceLocationName);}
 browser_mission_reset();stage_world_reset();avalanche_reset();stageInstances.clear();stagePending.reset();stageVm.reset();stageProgramHandles.clear();stageTrackGlobals.clear();stageCollection={};stageCollected.clear();stageCollectEvents.clear();stageCollectibleAwards=0;stageDispatches=stagePrograms=stageUnsupported=stageLastResource=stageLastProgram=0;stageBuiltinCalls.fill(0);stageFiredLog.clear();
 if(!stageTables)return;
 for(const auto& h:stageTables->handlers){StageInstance s;s.resource=h.resource;s.handler=&h;stageInstances.push_back(s);}
 for(const auto& e:stageTables->entities)if(auto* s=stage_instance(e[0]))s->entity=e[1]?StageGateEntity:StageNoopEntity;
 stage_vm_reset();
}
// Another browser system created an entity on the instance (section streaming, slot-1 builtins).
void browser_stage_instance_entity(uint32_t resource,bool gate){if(auto* s=stage_instance(resource))if(s->entity==StageNoEntity)s->entity=gate?StageGateEntity:StageNoopEntity;}
// 105398 store step (rider+9E0/A30/A60).
void stage_trigger_store(const OriginalInstanceContactPacket& packet,const OriginalInstanceContactRecord& record){stagePending=StagePendingContact{packet.instance,packet,record};}

// ---- LUN program execution --------------------------------------------------------------------
// Builtin argument entries as the VM passes them (16 bytes: key, value word0, word1, type).
struct StageArg {int32_t key=0;uint32_t word0=0,word1=0;int32_t type=0;};
// The builtins' shared keyed-argument parse (e.g. 2FF850 loop): same type -> copy; an expected non-float
// -> copy the raw word; an expected float given another type -> cvt.s.w of the word.
template<size_t N> static void stage_parse(std::array<uint32_t,N>& fields,const std::array<int32_t,N>& types,const StageArg* args,int count){
 for(int i=0;i<count;i++){const auto& a=args[i];if(a.key<0||size_t(a.key)>=N)throw std::runtime_error("Stage builtin key outside its argument block");
  if(a.type==types[a.key]||types[a.key]!=2)fields[a.key]=a.word0;else fields[a.key]=std::bit_cast<uint32_t>(float(int32_t(a.word0)));}
}
RIDER_LOCAL static uint32_t stage_current_instance=0; // context +0x290
// Builtin 34 (0x123210) on this core's rider: set by the rider code (nullptr: counted as unsupported).
RIDER_LOCAL void (*browserStageTeleport)(const std::array<float,16>& matrix)=nullptr;RIDER_LOCAL static uint32_t stageTeleportRequests=0;
RIDER_LOCAL static bool stageTeleportEnabled=false;static void stage_teleport_rider(const std::array<float,16>& matrix); // web/stage_teleport.inc
RIDER_LOCAL static bool stage_current_human_rider=false; // slot-2 program of this core's own rider contact (not a replay)
void request_stage_camera_shake(int type,float scale,float fade); // web/core.cpp (builtin 91)
static void stage_effect(int type,float amount); // 10F1C0
static void stage_debounce(StageInstance&,float duration,int32_t restore,int32_t mode);
static void stage_collectible_award(uint32_t resource,int32_t id); // 30B9A0
static StageArg stage_int(int32_t v){return {0,uint32_t(v),0,1};}
RIDER_LOCAL extern bool browserObjectRoutes; // web/world_bridge.cpp (pv finishFences)
StageArg mission_builtin_call(int,const StageArg*,int); // web/mission_gameplay.inc (Big Challenges)
#include "stage_world.inc" // LiveComp timers, particle effects, sound requests (the entities the programs build)
#include "avalanche_gameplay.inc" // builtins 94 / 95: the recorded avalanches (docs/avalanche.md)
#ifdef SSX_SNAPSHOT_REGISTRY
#include "world_snapshot.hpp"
// The rider-context snapshot's hook (web/world_snapshot.hpp; docs/replay.md §2a): the stage VM's run-time state (the PS2's
// WScriptMan gp+0xCE8, 0x30BB10 / 0x30BD20), its loaded programs kept (web/snapshot-policy.mjs).
namespace {
struct StageVmRuntime{bool present=false;ssx::OriginalScriptVM::RuntimeSnapshot vm;};
RIDER_LOCAL_LAZY std::array<StageVmRuntime,2> stageVmSnapshot;
void stage_vm_snapshot_save(unsigned slot){auto& s=stageVmSnapshot[slot];s.present=stageVm.has_value();if(stageVm)stageVm->saveRuntime(s.vm);}
bool stage_vm_snapshot_check(unsigned slot){const auto& s=stageVmSnapshot[slot];if(s.present!=stageVm.has_value())return false;return !stageVm||stageVm->programCount()==s.vm.programs;}
bool stage_vm_snapshot_restore(unsigned slot){const auto& s=stageVmSnapshot[slot];if(s.present!=stageVm.has_value())return false;return !stageVm||stageVm->restoreRuntime(s.vm);}
uint64_t stage_vm_snapshot_hash(){if(!stageVm)return 0;ssx::OriginalScriptVM::RuntimeSnapshot now;stageVm->saveRuntime(now);uint64_t h=ssx_snapshot::mix(1469598103934665603ull,now.programs);h=ssx_snapshot::mix(h,uint64_t(now.depth));for(const auto& t:now.tables)h=ssx_snapshot::hash_value(t,h);return h;}
size_t stage_vm_snapshot_bytes(){size_t n=0;for(const auto& s:stageVmSnapshot)n+=s.vm.tables.size()*sizeof(ssx::OriginalScriptTable);return n;}
struct StageVmSnapshotHook{StageVmSnapshotHook(){ssx_snapshot::hooks().push_back({"stageVm (run-time state)",&stage_vm_snapshot_save,&stage_vm_snapshot_restore,&stage_vm_snapshot_hash,&stage_vm_snapshot_bytes,&stage_vm_snapshot_check});}};
[[maybe_unused]] StageVmSnapshotHook stageVmSnapshotHook;
}
#endif
static bool stage_launch_spline(uint32_t resource); // web/set_piece_gameplay.inc: builtin19 of a timer program (slot 5)
// Builtin dispatch (table 0x441F38). Returns the result value (nil unless noted).
static StageArg stage_builtin(int index,const StageArg* args,int count){
 if(index>=0&&size_t(index)<stageBuiltinCalls.size())++stageBuiltinCalls[index];
 StageArg nil{};
 switch(index){
 case 44:{ // 302968: context +0x290 = the instance (key 0; 0 when it does not resolve)
  std::array<uint32_t,1> f{0xffffffffu};stage_parse(f,std::array<int32_t,1>{1},args,count); /*types gp+0xBF8*/
  stage_current_instance=stage_instance(f[0])||stageWorldMatrices.count(f[0])||set_piece_instance_at(f[0])?f[0]:0u;return nil;}
 case 16:case 25:case 26:stage_particle_builtin(index,args,count);return nil; // MakeParticleData / AddParticleData / AddDynamicParticleData
 case 69:stage_particle_stop(args,count);return nil;                            // 302490 stop / delete effects
 case 105:stage_multiparticle_create(args,count);return nil;                   // 306A90 MultiParticle group (roadflare flame)
 case 106:stage_multiparticle_member(args,count);return nil;                   // 306CB0 MultiParticle member add / remove
 case 43:{ // 302778 -> 303DF8: int(table 0x4465F8[n] (n for 0..6, 0 for 7) == (s8)0x535C10 event kind), 0 for n >= 8. The
  // mode fences / challenge reset planes / startmode colliders kill themselves (builtin2 DeadNode) in slot 1 unless free ride.
  std::array<uint32_t,1> f{0xffffffffu};stage_parse(f,std::array<int32_t,1>{1},args,count);
  const uint32_t n=f[0];if(n>=8)return stage_int(0);
  const int32_t kind=browserEventKind>=0?browserEventKind:stage_course_event_kind(setPieceLocationName); // 0x535C10 of the events: Rival Time (?BC 5, Rival Points 6 via event_kind), Race (?RA 0), Slopestyle (?SS 1), Big Air (?BA 2), Half Pipe (?HP 3); countdown anchors of every peak
  return stage_int((n==7?0:int32_t(n))==kind?1:0);}
 case 34:{ // 300770: the contacting player (ctx+0) -> rider vt+0x54 = 0x123210(rider, destination matrix): BRA2 water-tower /
  // phone-booth beams. The destination is instance+0x10 (stage-world.json teleports; their UVScroll is not a primary
  // modifier). The rider side (slot offset, motion/control exits, camera cuts, 11D660 placement RNORM_FWD_CYC, speed kept
  // along the new forward) is browserStageTeleport (docs/stage-scripts.md "Teleport"); a replayed contact never moves the
  // other core's rider.
  std::array<uint32_t,1> f{0xffffffffu};stage_parse(f,std::array<int32_t,1>{1},args,count); /*types gp+0xB50*/
  const uint32_t target=stage_target(f[0]);if(!target||sharedWorldReplay||!stage_current_human_rider)return nil;
  std::array<float,16> m;if(auto it=stageTeleports.find(target);it!=stageTeleports.end())for(unsigned i=0;i<16;i++)m[i]=std::bit_cast<float>(it->second[i]);
  else{const auto e=stage_entity_matrix(target);for(unsigned i=0;i<16;i++)m[i]=e[i/4][i%4];}
  ++stageTeleportRequests;if(stageTeleportEnabled)stage_teleport_rider(m);else if(browserStageTeleport)browserStageTeleport(m);else ++stageUnsupported; // web/stage_teleport.inc (pv boothTeleport)
  return nil;}
 case 7:stage_boost_builtin(args,count);return nil;                            // 2FBEC8 one-way volume (Boost, entity type 8)
 case 87:{ // 304E38: an entity with vt+0x84 (Object / LiveComp / Debounce) gets its contact guard entity+0x20 = int(clockRate * seconds) (0x355858)
  std::array<uint32_t,2> f{0xffffffffu,std::bit_cast<uint32_t>(1.f)};stage_parse(f,std::array<int32_t,2>{1,2},args,count); /*defaults 0x4FBD20, types 0x4A3D80*/
  auto* st=stage_instance(stage_target(f[0]));
  if(st&&(st->entity==StageGateEntity||st->debounce)&&!st->dead){terrain_original::Rounding rounding;st->guard=int32_t(terrain_original::mul(60.f,std::bit_cast<float>(f[1])));}
  return nil;}
 case 12:stage_flag_builtin(args,count);return nil;                           // 2FC9C8 flag entity (manager slot / grid build draws)
 case 88:stage_crowd_builtin(args,count);return nil;                          // 303BA0 CrowdMan2d (crowd texture, camera-flash areas)
 case 97:stage_halo_builtin(args,count);return nil;                           // 3057C0 HaloModifier (pickup glow sprite)
 case 90:stage_magnet_builtin(args,count);return nil;                         // 305478 MagnetModifier (point pickup flies to the rider)
 case 55:return stage_timer_gate(args,count);                                  // 3019C8 LiveComp time crossed key1/30 s
 case 54:return stage_entity_time(args,count);                                 // 301680 entity time (LiveComp: time x 30)
 case 28:stage_livecomp_property(args,count);return nil;                      // 2FF9A8 entity vt+0x120 (LiveComp 0x341FE8 properties)
 case 94:{ // 305C88: key0 avalanche id (default 0) -> 0x2D97A8 when the record's magic was good (gp+2516 = 0)
  std::array<uint32_t,1> f{0u};stage_parse(f,std::array<int32_t,1>{1},args,count);avalanche_trigger(int32_t(f[0]));return nil;}
 case 95:{ // 305D90: key0 instance (-1: ctx+0x290; types gp+0xCB0) with an entity accepting modifiers (vt+0x84) -> 355A78 AvaSpline
  std::array<uint32_t,1> f{0xffffffffu};stage_parse(f,std::array<int32_t,1>{1},args,count);avalanche_spline(stage_target(f[0]));return nil;}
 case 91:{ // 3050F0: key0 (0 or skip), key1 use an instance (1), key2 instance (-1: ctx+0x290), key3 distance factor (1). With an
  // instance: every rider with a camera (rider+0x87C) gets 15E360(camera, gp+0xAE8 = type 0, 1.0, |rider+0x110 - P| x key3), P the
  // translation of its entity matrix (vt+0xC4) or of instance+0x10; without one only the current player, at fade 0. Only this
  // core's human has a camera. The request is applied before the camera update (web/core.cpp request_stage_camera_shake).
  std::array<uint32_t,4> f{0u,1u,0xffffffffu,std::bit_cast<uint32_t>(1.f)};stage_parse(f,std::array<int32_t,4>{1,1,1,2},args,count); /*defaults 0x4FBD40, types 0x446690*/
  if(f[0]!=0||!browserHumanRider||sharedWorldReplay)return nil;
  const uint32_t instance=f[1]==0?0u:int32_t(f[2])==-1?stage_current_instance:stage_target(f[2]);
  if(!instance){if(stage_current_human_rider)request_stage_camera_shake(0,1.f,0.f);return nil;}
  const auto m=stage_entity_matrix(instance);terrain_original::Rounding rounding;using namespace terrain_original; // VU vsub, vmul, vadday / vmaddaz / vmaddw (w 1 - 1), vsqrt
  const float dx=sub(physicsState.position[0],m[3][0]),dy=sub(physicsState.position[1],m[3][1]),dz=sub(physicsState.position[2],m[3][2]);
  const float distance=terrain_original::sqrt(add(add(add(mul(dx,dx),mul(dy,dy)),mul(dz,dz)),0.f));
  request_stage_camera_shake(0,1.f,mul(distance,std::bit_cast<float>(f[3])));return nil;}
 case 74: // 303F80: the current player (ctx+0) -> rider+0x3FC = 1 (DBC2 tunnelvolume_1000..1002); only this core's own rider
  if(stage_current_human_rider)stageRiderTunnel=true;return nil;
 case 92:{ // 305660: the current player (ctx+0) human (vt+0x44) -> 0x390EC8(0): a strike at distance 0 (flash, thunder at once), gated
  // by gp+0x151C as the ScreenTint update (web/stage_world.inc stageLightningFinishAge). DRA4 treefalltriggers, EBA3 speakertower.
  if(stage_current_human&&!sharedWorldReplay&&stageLightningFinishAge<3)weather_lightning_strike(0.f);return nil;}
 case 35:{ // 300B20: key0 rail id (-1: nothing), key1 1 on / 0 off -> 0x358B28 (web/rail_dynamic.inc browser_rail_group)
  std::array<uint32_t,2> f{0xffffffffu,1u};stage_parse(f,std::array<int32_t,2>{1,1},args,count); /*defaults 0x4FBB48, types gp+0xB60*/
  if(int32_t(f[0])!=-1&&(f[1]==0||f[1]==1))browser_rail_group(f[0],f[1]==1);return nil;}
 case 30:stage_sound_builtin(args,count);return nil;                           // 2FFF00 sound request
 case 31:stage_sound_loop_builtin(args,count,1);return nil;                    // 3000A8 looping sound (audio only)
 case 73:stage_sound_loop_builtin(args,count,2);return nil;                    // 300260 stop sound (audio only)
 case 19:{ // 2FDED0 Spline: slot-2 pieces are launched from the contact log (set_piece_gameplay.inc); a timer program's (slot 5) here
  std::array<uint32_t,1> f{0xffffffffu};if(count)for(int i=0;i<count;i++)if(args[i].key==0)f[0]=args[i].word0;
  if(stage_current_slot==5)stage_launch_spline(stage_target(f[0]));
  else if(stage_current_slot==2)stage_launch_gated_spline(stage_target(f[0])); // random-gated trigger programs (seed guard 2)
  return nil;}
 case 3:stage_livecomp_builtin(args,count);return nil;                         // 2FBCB8 LiveComp player (0x341AA0)
 case 48:stage_rail_builtin(args,count);return nil;                            // 2FF1C8 RailModifier on a Spline piece / LiveComp (R&B; web/rail_dynamic.inc)
 case 6:{ // 2FB498 AnimTeeter: a location teeter's slot-1 program at its section enter rebuilds it with its rails (R&B A_ASS1 log)
  std::array<uint32_t,1> f{0xffffffffu};for(int i=0;i<count;i++)if(args[i].key==0)f[0]=args[i].word0;
  if(stage_section_run)browser_location_teeter_section(stage_target(f[0]),true);return nil;}
 case 2:stage_node_builtin(args,count);return nil;                             // 2FC420 DeadNode / authored flags / RestoreNode
 case 29:stage_hide_builtin(args,count,true);return nil;                       // 2FFB50 Hide (type 16)
 case 58:stage_hide_builtin(args,count,false);return nil;                      // 2FFD58 Unhide
 case 68:{ // 302210: location streaming / station request for the human (key0 map id 0x43D950+0x5C, key1 action, key2 entry; web/peak_world.inc)
  std::array<uint32_t,3> f{0xffffffffu,0xffffffffu,0xffffffffu};stage_parse(f,std::array<int32_t,3>{1,1,1},args,count); /*defaults 0x4FBC20, types 0x4465C8*/
  extern void browser_peak_builtin68(int32_t,int32_t,int32_t);if(stage_current_human)browser_peak_builtin68(int32_t(f[0]),int32_t(f[1]),int32_t(f[2]));
  if(stage_current_human&&int32_t(f[1])==2){void browser_mission_load_trigger();browser_mission_load_trigger();} // 22D088 -> world state 10: the missions are rebuilt at its exit (web/mission_gameplay.inc)
  return nil;}
 case 67:{ // 302048 -> 22D6C8(streamer, 0x445E40[key0]) (web/peak_world.inc)
  std::array<uint32_t,2> f{0xffffffffu,0xffffffffu};stage_parse(f,std::array<int32_t,2>{1,1},args,count);
  extern void browser_peak_builtin67(int32_t,int32_t);if(stage_current_human)browser_peak_builtin67(int32_t(f[0]),int32_t(f[1]));
  // 0x3021A4..0x3021B8: key1 0 and 0x445E40[key0] != 1 -> 30B7F8 (every mission reset: a running Big Challenge ends) before 22D6C8
  if(stage_current_human&&int32_t(f[1])==0&&!(f[0]<8u&&(std::array<int32_t,8>{2,3,4,5,1,1,1,0})[f[0]]==1)){void browser_mission_gate();browser_mission_gate();}
  return nil;}
 case 13:stage_meshanim_builtin(args,count);return nil;                        // 2FCFF0 MeshAnim break pieces (0x351B40)
 case 27:{ // 2FF850: player (-1 current), effect type, amount -> 10F1C0(rider, type, amount)
  std::array<uint32_t,3> f{0xffffffffu,0u,0u};stage_parse(f,std::array<int32_t,3>{1,1,2},args,count); /*types 0x446530*/
  stage_effect(int32_t(f[1]),std::bit_cast<float>(f[2]));return nil;}
 case 1:{ // 2FC7D0: target (-1 current instance), duration (s), restore/state, mode -> Debounce 342C08
  std::array<uint32_t,4> f{0xffffffffu,std::bit_cast<uint32_t>(1.f),1u,0xffffffffu};stage_parse(f,std::array<int32_t,4>{1,2,1,1},args,count); /*types 0x445FB0*/
  if(auto* s=stage_instance(int32_t(f[0])==-1?stage_current_instance:f[0]))stage_debounce(*s,std::bit_cast<float>(f[1]),int32_t(f[2]),int32_t(f[3]));
  return nil;}
 case 0:{ // 2FC0D0 Object entity (356DB0): instance+0xC (key 0, -1 current) gets an entity
  std::array<uint32_t,1> f{0xffffffffu};for(int i=0;i<count;i++)if(args[i].key==0)f[0]=args[i].word0;
  if(browser_streamed_world(setPieceLocationName)&&stage_entity_type(stage_target(f[0]))==16)stage_destroy_entity(stage_target(f[0])); // 2FAE38 replaces a Hide node (type 16) by the Object (Big Challenge markers: hidden, then shown by their status program)
  if(auto* s=stage_instance(stage_target(f[0])))if(s->entity==StageNoEntity)s->entity=StageGateEntity;
  // 356DB0 with key 1 != 0: instance+8 = (flags & ~0x40) | 0x20, the static collision route (pv finishFences, web/world_bridge.cpp:
  // the flags-0 barriers before the ARA1 / ERA5 finishes, "if 43(4) or 43(5) or 43(6): builtin0(key1 1)"; PS2 apr-part4 0x122).
  {uint32_t k1=0,k2=0;for(int i=0;i<count;i++){if(args[i].key==1)k1=args[i].word0;if(args[i].key==2)k2=args[i].word0;}const uint32_t t=stage_target(f[0]);
   if(browserObjectRoutes&&k1&&t&&stage_entity_type(t)!=6)stage_set_flags(t,(stage_flags(t)&~0x40u)|0x20u);
   avalanche_object(t,k1,k2);} // an avalanche piece's Object (web/avalanche_gameplay.inc, "Pieces' collision")
  return nil;}
 case 15:{ // 2FD250 -> 355DB8 RollerModifier (web/roller_gameplay.inc builds it): its 355770 predicate 361CF8 is 0
  if(auto* s=stage_instance(stage_current_instance))s->rejects=true;
  return nil;}
 case 37:{ // 300E28: collection id (types gp+0xB70), expected count -> 30B928: opens the one slot when it is free (-1)
  std::array<uint32_t,2> f{0xffffffffu,1u};stage_parse(f,std::array<int32_t,2>{3,1},args,count);
  if(int32_t(f[0])!=-1&&stageCollection.id!=int32_t(f[0])&&stageCollection.id==-1){stageCollection={};stageCollection.id=int32_t(f[0]);stageCollection.expected=int32_t(f[1]);stageCollectionTrack=stageRunningTrack;} // 30C390 reset
  return nil;}
 case 38:{ // 300F50: collection id, up to 20 resources (types 0x446570) -> 30BAC8 -> 30C4A8 appends every resource != -1.
  // 30C4A8 also turns a listed instance into a dead node when 0x535C11 != 0 (a single event, not Conquer the
  // Mountain) or career stats 1538E8 already hold it; the countdown audit carries that (draw none, DeadNode).
  std::array<uint32_t,21> f;f.fill(0xffffffffu);std::array<int32_t,21> t;t.fill(1);t[0]=3;stage_parse(f,t,args,count);
  if(int32_t(f[0])!=-1&&stageCollection.id==int32_t(f[0]))for(unsigned i=1;i<21;i++)if(f[i]!=0xffffffffu&&stageCollection.resources.size()<64){
   const uint32_t index=uint32_t(stageCollection.resources.size()),r=f[i];stageCollection.resources.push_back(r);
   // 30C4A8: a single event / multiplayer (0x535C11 != 0) or a career bit already set (1538E8) -> DeadNode; a career race
   // keeps the instance without entity at its authored flags (0x34FC1C: static draw + route) until its section builds it.
   const uint32_t* mask=stageCollectMask;if(browser_streamed_world(setPieceLocationName)){auto row=stageCollectRows.find(uint32_t(stageCollectionTrack));mask=row==stageCollectRows.end()?nullptr:row->second.data();}
   const bool dead=stageCollectPath!=0||(mask&&((mask[index>>5]>>(index&31))&1u));
   if(dead)stage_make_node(r,6);
   else{stageNodeState.erase(r);if(auto* st=stage_instance(r)){st->entity=StageNoEntity;st->dead=false;}if(auto* w=set_piece_instance_at(r))stage_set_flags(r,ssx::originalPickupRestoredFlags(w->flags));}
   section_collectible_state(r,dead);
  }
  return nil;}
 case 39:{ // 301120: int 1 when the current player (ctx+0) is human (vt+0x4C = !+0x874), after 30B9A0(collection id, resource)
  std::array<uint32_t,2> f{0xffffffffu,0xffffffffu};stage_parse(f,std::array<int32_t,2>{3,1},args,count); /*defaults 0x4FBBD0, types gp+0xB78*/
  if(!stage_current_human)return stage_int(0);
  stage_collectible_award(int32_t(f[1])==-1?stage_current_instance:f[1],int32_t(f[0]));
  return stage_int(1);}
 case 52:{ // 303130: int 1 when the instance (-1 current) has an entity at +0xC
  std::array<uint32_t,1> f{0xffffffffu};stage_parse(f,std::array<int32_t,1>{1},args,count); /*defaults 0x4FBC50, types gp+0xC28*/
  const auto* s=stage_instance(int32_t(f[0])==-1?stage_current_instance:f[0]);
  return stage_int(s&&s->entity!=StageNoEntity?1:0);}
 case 61:return stage_int(stage_current_instance?int32_t(stage_current_instance):-1); // 303430: current instance +0x78 (resource), -1 without one
 case 77:{ // 303598: float lo + (hi - lo) * unit(317810), the shared gameplay RNG 0x4FF030 (defaults 0, 1)
  std::array<uint32_t,2> f{0u,std::bit_cast<uint32_t>(1.f)};stage_parse(f,std::array<int32_t,2>{2,2},args,count);
  const float lo=std::bit_cast<float>(f[0]),hi=std::bit_cast<float>(f[1]);
  terrain_original::Rounding rounding;const uint32_t word=stage_section_run?0x3FC00000u:rng.next(); /*slot-1 draws: the section model took them*/
  const float r=originalScalarSubtract(std::bit_cast<float>((word&0x7FFFFFu)|0x3F800000u),1.f); /*317830*/
  return {0,std::bit_cast<uint32_t>(originalScalarAdd(lo,terrain_original::mul(originalScalarSubtract(hi,lo),r))),0,2};}
 case 110:{extern int32_t browser_peak_builtin110();return stage_int(browser_peak_builtin110());} // 307020 -> 1E3570: the station "?" homes in until the first FAQ was shown (web/peak_world.inc)
 case 108:{ // 302870 -> 303E60: game mode byte 0x535C12 == table 0x446618[n] (the stations' peak-race / peak-jam fences; web/peak_world.inc)
  std::array<uint32_t,1> f{0xffffffffu};stage_parse(f,std::array<int32_t,1>{1},args,count); /*default gp+0x2668 = -1*/
  extern int32_t browser_peak_builtin108(uint32_t);const int32_t v=browser_peak_builtin108(f[0]);return v<0?nil:stage_int(v);}
 case 100:{extern void browser_peak_builtin100();if(stage_current_human)browser_peak_builtin100();return nil;} // 306300 -> 1E3510: the "?" contact opens the Message Center on FAQ 1 (web/peak_world.inc)
 case 40:case 47:case 50:case 51:case 59:case 63:case 64:case 65:case 75:case 76:case 78:case 79:case 81:case 83:case 86:case 98:case 107:return mission_builtin_call(index,args,count); // WScript missions / Big Challenges (web/mission_gameplay.inc)
 default:return nil; // effects owned by other systems (see the header) or not modeled
 }
}
// A program function 0 through the LUN VM (309C88: 2224B8 with the track's globals, the return released).
static ssx::OriginalScriptValue stage_builtin_value(int index,const ssx::OriginalScriptArg* args,int count,bool setup){
 if(setup&&index!=37&&index!=38&&index!=105)return {}; // stage setup only builds the collectible set and the MultiParticle groups here; the rest is owned elsewhere
 std::vector<StageArg> a(size_t(std::max(count,0)));
 for(int i=0;i<count;i++)a[size_t(i)]={args[i].key,args[i].value.word0,args[i].value.word1,args[i].value.type};
 const StageArg r=stage_builtin(index,a.data(),count);return {r.word0,r.word1,r.type};
}
static const ssx::OriginalScriptValue& stage_track_globals(uint32_t track){
 for(auto& [t,v]:stageTrackGlobals)if(t==track)return v;
 throw std::runtime_error("Stage track without a globals table");
}
static void stage_run_program(uint32_t globalProgram,uint32_t track,bool setup=false){
 if(!stageVm)throw std::runtime_error("Stage VM not initialised");
 const int32_t previousTrack=stageRunningTrack;stageRunningTrack=int32_t(track);struct Restore{int32_t v;~Restore(){stageRunningTrack=v;}} restore{previousTrack};
 auto result=stageVm->run(stageProgramHandles.at(globalProgram),0,stage_track_globals(track),
  [setup](int index,const ssx::OriginalScriptArg* args,int count){return stage_builtin_value(index,args,count,setup);});
 stageVm->release(result);
}
// Stage load: every program into the VM, a globals table per track, then each stage's global handler
// programs (they store the track callbacks and build the collectible set, builtins 37/38).
static void stage_vm_reset(){
 stageVm.emplace();stageProgramHandles.clear();stageTrackGlobals.clear();stageCollection={};stageCollectionTrack=-1;
 if(!stageTables)return;
 for(const auto& p:stageTables->programs)stageProgramHandles.push_back(stageVm->addProgram(ssx::originalScriptProgramFromWords(stageTables->words.data()+p.first,p.count)));
 for(const auto& st:stageTables->stages)stageTrackGlobals.emplace_back(st.track,ssx::OriginalScriptVM::tableValue(stageVm->newTable(64)));
 for(const auto& st:stageTables->stages)for(uint32_t g=0;g<st.globalCount;g++){
  const int program=stageTables->globals[st.firstGlobal+g];if(program>=0)stage_run_program(st.firstProgram+uint32_t(program),st.track,true);}
 // Streamed Peak 1 world: the collectible slot goes to the first RESIDENT stage in load order (the setup of a location that is not
 // in the world does not count); later locations take it at their read completion (browser_stage_track_setup).
 if(browser_streamed_world(setPieceLocationName)&&stageCollectionTrack>=0&&!ssx::worldTrackOctree()[size_t(stageCollectionTrack)]){
  stageCollection={};stageCollectionTrack=-1;
  for(const auto& st:stageTables->stages)if(ssx::worldTrackOctree()[size_t(st.track)])for(uint32_t g=0;g<st.globalCount;g++){
   const int program=stageTables->globals[st.firstGlobal+g];if(program>=0)stage_run_program(st.firstProgram+uint32_t(program),st.track,true);}
 }
}
// web/peak_world.inc: a location's read completion builds its stage (3ACA10 + 230430: the global handler programs; 30B928 opens
// the collectible slot only when it is free) and its unload start tears it down (230360 -> 308FE0: the slot it owns is freed).
void browser_stage_track_setup(uint32_t track){
 if(!stageVm||!stageTables)return;
 for(const auto& st:stageTables->stages)if(st.track==track)for(uint32_t g=0;g<st.globalCount;g++){
  const int program=stageTables->globals[st.firstGlobal+g];if(program>=0)stage_run_program(st.firstProgram+uint32_t(program),st.track,true);}
}
void browser_set_piece_track_teardown(uint32_t); // web/set_piece_gameplay.inc (pv peakSplines)
void browser_stage_track_teardown(uint32_t track){if(stageCollectionTrack==int32_t(track)){stageCollection={};stageCollectionTrack=-1;}browser_mission_track_teardown(track);browser_set_piece_track_teardown(track);}
extern "C" EMSCRIPTEN_KEEPALIVE void set_stage_collect_row(int track,uint32_t lo,uint32_t hi){if(track>=0)stageCollectRows[uint32_t(track)]={lo,hi};}
extern "C" EMSCRIPTEN_KEEPALIVE int stage_collect_track(){return stageCollectionTrack;}
// The host set the collect path / rows after the run's stage reset (free ride has no event start, web/free-ride.js afterReset):
// rebuild the collectible slot like the reset did (the first resident stage in load order), with the new path and rows.
extern "C" EMSCRIPTEN_KEEPALIVE void peak_collect_refresh(){
 if(!stageVm||!stageTables||!browser_streamed_world(setPieceLocationName))return;stageCollection={};stageCollectionTrack=-1;
 for(const auto& st:stageTables->stages)if(ssx::worldTrackOctree()[size_t(st.track)])for(uint32_t g=0;g<st.globalCount;g++){
  const int program=stageTables->globals[st.firstGlobal+g];if(program>=0)stage_run_program(st.firstProgram+uint32_t(program),st.track,true);}
}
// 30A060 for the rider's selected instance.
static void stage_run_handler(StageInstance& s,int slot){
 const auto* stage=stage_of(s.resource);if(!stage||!s.handler)return;
 const int program=s.handler->slots[slot];if(program<0)return; // 3A6B78 returned -1
 stage_current_instance=s.resource;++stagePrograms;stageLastResource=s.resource;stageLastProgram=uint32_t(program);++s.fired;
 if(slot!=5&&stageFiredLog.size()<3*4096){stageFiredLog.push_back(motionTick); /*slot-5 timer programs run every tick: stage_world_slots()*/stageFiredLog.push_back(s.resource);stageFiredLog.push_back(uint32_t(program));}
 stage_run_program(stage->firstProgram+uint32_t(program),stage->track);
 stage_current_instance=0;
}
// Entity callbacks of the stage world (web/stage_world.inc): slot 5 (LiveComp tick 0x34EBA0 -> 0x30A688) and slot 4
// (finished: LiveComp 0x341FC8 / spline end 0x34FD00 -> 0x30A598/0x30A5C0).
static void stage_run_slot(uint32_t resource,int slot){
 auto* s=stage_instance(resource);if(!s||!s->handler||s->handler->slots[slot]<0)return;
 if(stageWorldSlotLog.size()<4*8192)stageWorldSlotLog.insert(stageWorldSlotLog.end(),{stage_world_tick(),resource,uint32_t(slot),uint32_t(s->handler->slots[slot])});
 const int previous=stage_current_slot;stage_current_slot=slot;
 try{stage_run_handler(*s,slot);}catch(...){stage_current_slot=previous;throw;}
 stage_current_slot=previous;
}
// Section enter (0x30A3A0 -> 0x30A298, end of tick E): the slot-1 program of a newly listed instance. The section
// model (web/section_gameplay.inc) already took its gameplay-RNG draws and the JS players start its LiveComps, flags
// and UV scroll from the section log; here the program's other builtins run (particles, sounds).
static void stage_world_section_program(uint32_t resource,int32_t program,int slot){
 // The streamed Peak 1 world runs the slot-1/3 programs without a stage world package (their DeadNode / Hide builtins
 // are what matters there: challenge reset planes, mode fences); its section ids carry the track in bits 16+.
 if((!stageWorldLoaded&&!browser_streamed_world(setPieceLocationName))||!stageVm||program<0)return;
 const auto* stage=stage_of(resource);if(!stage)return;program&=0xFFFF;
 stage_section_run=true;stage_current_slot=slot;stage_current_instance=resource;stageLastProgram=uint32_t(program);
 try{stage_run_program(stage->firstProgram+uint32_t(program),stage->track);}catch(...){stage_section_run=false;stage_current_slot=-1;stage_current_instance=0;throw;}
 stage_section_run=false;stage_current_slot=-1;stage_current_instance=0;
}
// 0x30A3A0 slot 1 (section enter) / 0x30A460 slot 3 (section leave of an instance without a live entity, or whose
// instance has a slot-3 handler: roadflare MultiParticle removal, crowd / camera-flash unregistration, BHP1 vehicle
// glows back to their authored flags).
static void stage_world_section_enter(uint32_t resource,int32_t program){stage_world_section_program(resource,program,1);}
// The handler program of a resource's slot (-1 none), and whether it calls a builtin (the instruction walk: 0x21 calls builtin b2; the
// opcodes with one inline word, 0x14..0x17, 0x1D, 0x24..0x27, take two).
static int32_t stage_slot_program(uint32_t resource,int slot){const auto* s=stage_instance(resource);return s&&s->handler?s->handler->slots[size_t(slot)]:-1;}
static bool stage_slot_calls(uint32_t resource,int slot,int builtin){
 const int32_t program=stage_slot_program(resource,slot);const auto* stage=stage_of(resource);if(program<0||!stage||!stageTables)return false;
 const auto& pr=stageTables->programs[stage->firstProgram+uint32_t(program)];
 const auto p=ssx::originalScriptProgramFromWords(stageTables->words.data()+pr.first,pr.count);
 for(size_t pc=0;pc<p.code.size();){const uint32_t w=p.code[pc],op=w&255u;if(op==0x21u&&int((w>>16)&255u)==builtin)return true;pc+=(op>=0x14u&&op<=0x17u)||op==0x1Du||(op>=0x24u&&op<=0x27u)?2:1;}
 return false;
}
static void stage_world_section_leave_program(uint32_t resource,int32_t program){stage_world_section_program(resource,program,3);}
static void stage_world_spline_finished(uint32_t resource){if(stageWorldLoaded)stageSlot4Pending.push_back(resource);}
extern "C" float* race_result_info();
RIDER_LOCAL extern bool sharedWorldReplay;void shared_world_log(std::initializer_list<uint32_t>); // web/shared_world.inc
extern "C" int race_timed_out(); // web/race_bridge.cpp: rider+0x480
static bool stage_rider_may_fire(){
 // 30A060 0x30A0CC..0x30A0F0: a human rider whose +0x480 is set (DNF: the 125228 time-out or a Give Up 1253D0) fires nothing unless
 // the game mode (*(gp-0x848)+0x84 +0x214) is 4 (free ride; events are 3). A normal finish (125108: +0x470 = 0) leaves +0x480 at 0,
 // so a finished rider still fires (PS2 Schizophrenia: trigger 24087 re-fires at 3590 after the 3492 finish and relaunches its splines).
 return !(browserHumanRider&&race_timed_out()!=0&&resetInputs.eventModeActive);
}
// 121818 for a selected instance: entity gate, then 30A060. `rider` false replays another rider's
// contact (web/shared_world.inc event 6): the shared instance state changes, this core's rider does not.
static void stage_contact(uint32_t resource,bool rider,bool human){
 auto* s=stage_instance(resource);if(!s){if(rider&&human&&stage_rider_may_fire())browser_mission_contact(resource);return;} // no handler row: 30A060 posts the contact event (3097E0) and finds no slot-2 program
 ++stageDispatches;
 if(s->entity==StageNoopEntity)return;
 if(s->entity==StageGateEntity){ // 355770
  if(s->rejects)return;              // modifier vt+0x54 refresh (the roller re-kick runs in roller_gameplay.inc), predicate 0
  if(rider&&!stage_magnet_gate(resource,human))return; // MagnetModifier vt+0x4C 0x357660: acquire, then wait until it reached the rider
  if(s->guard>0)return;
  s->guard=30; // clockRate (60) / 2
 }
 if(rider&&!stage_rider_may_fire())return;
 if(rider&&human)browser_mission_contact(resource); // 30A060 -> 3097E0: WScript event kind 0 (web/mission_gameplay.inc)
 if(rider&&s->handler&&s->handler->slots[2]>=0)shared_world_log({6u,resource,human?1u:0u});
 stage_current_human=human;stage_current_human_rider=rider;stage_current_slot=2;if(rider&&stagePendingRecord)stage_contact_record=*stagePendingRecord;
 stage_run_handler(*s,2);stage_current_slot=-1;stage_current_human=false;stage_current_human_rider=false;stage_contact_record.reset();
 if(s->entity==StageGateEntity)s->guard=int32_t(60.f*.5f); // 355858
}
// 121818 (from race_end), then 120E30 clears rider+A30.
void browser_stage_triggers(){if(!stagePending)return;const auto pending=*stagePending;stagePending.reset();stagePendingRecord=pending.record;stage_contact(pending.resource,true,browserHumanRider);stagePendingRecord.reset();}
void browser_stage_replay(uint32_t resource,bool human){stage_contact(resource,false,human);}
// Entity pass (race_begin): 356198 guards and the Debounce countdown 342D88 / completion 342E98.
void browser_stage_entities_tick(){
 browser_mission_rider_snapshot(); // rider +0x5B8 as the WScript tick sees it (before the entity pass clears it for the rider pass)
 stage_magnet_tick();         // MagnetModifier containers (0x3573F8 + 0x3568B0)
 stage_world_tick_entities(); // LiveComp timers (slots 5/4), particle effects (web/stage_world.inc)
 stage_halo_tick();           // HaloModifier spin (entity effect lists)
 for(auto& s:stageInstances){
  if(s.guard>0)--s.guard;
  if(s.debounce){
   if(s.debounceTicks>0&&--s.debounceTicks==0){
    s.debounce=false;
    if(!(s.handler&&s.handler->slots[4]>=0))stage_world_destroy(s.resource); // 342E98 destroys the Debounce entity: the effects it carries (0x355F10 moved them there, builtin25 bursts added after it) go with it
    if(s.handler&&s.handler->slots[4]>=0)stage_run_handler(s,4); // 34FCC0 -> 2D19E8: authored slot 4 replaces the completion
    else if(s.debounceRestore==0||s.debounceRestore==3){s.restored=s.debounceRestore==3;s.dead=s.debounceRestore==0;s.entity=StageNoopEntity;s.flags=ssx::originalPickupRestorePendingFlags(s.flags); // 0: DeadNode 0x3506D8 (permanent), 3: RestoreNode 0x350F60 (back at the section leave); same flags
     stageFlags[s.resource]=s.flags;if(s.dead){stageNodeState[s.resource]=6;section_mark_dead(s.resource);} // a collected pickup never comes back (0x34FB00: DeadNodes are not listed)
     if(auto* w=set_piece_instance_at(s.resource)){stage_note_countdown_flags(s.resource);/* a race restart puts them back */w->eventRuntimeFlags=ssx::originalPickupRestorePendingFlags(w->eventRuntimeFlags.value_or(w->flags));} // RestoreNode 350F60: no 0x20/0x40, the route skips it
     // restore 3: 342E98's RestoreNode 0x350F60 is the entity at instance+0xC (type 19), listed for sections: its leave (0x34FD90 -> the
     // 0x34FBF0 destructor) puts the authored flags back, so the trigger fires again (c0a-ws13: dragontrig_1000 by Griff at 11944)
     if(s.restored){stageNodeState[s.resource]=19;if(!stage_section_run)section_entity_created(s.resource);shared_world_node_changed(s.resource);}}
    else {s.entity=s.entityBeforeDebounce;s.flags=ssx::originalPickupRestoredFlags(s.flags);stageFlags[s.resource]=s.flags;} // 34FBF0 base destruction
   }
  }
 }
 browser_mission_tick();     // WScriptMan 309270 (web/mission_gameplay.inc)
 stage_multiparticle_tick(); // 0x2306A8 order: entity passes, WScriptMan, CrowdMan2d/3d, MultiParticleMan 0x357BF8
}
static void stage_debounce(StageInstance& s,float duration,int32_t restore,int32_t mode){
 // 342C08: ticks = int(duration * clockRate), restore at +0x30; mode 0 clears the low four instance flag bits,
 // 1 sets bit 0 then (&~2)|4, otherwise (flags & 3) == 3 -> (&~2)|4. Without an entity the Debounce itself
 // becomes instance+0xC (contact callback 3614F8: no-op).
 terrain_original::Rounding rounding;
 if(!s.flagsKnown){if(auto* w=set_piece_instance_at(s.resource))s.flags=w->eventRuntimeFlags.value_or(ssx::originalPickupRestoredFlags(w->flags));s.flagsKnown=true;} /*countdown runtime flags: 34FC1C restore of the authored flags*/
 if(mode==0)s.flags&=0xfffffff0u;else if(mode==1)s.flags=((s.flags|1u)&0xfffffffdu)|4u;else if((s.flags&3u)==3u)s.flags=(s.flags&0xfffffffdu)|4u;
 stageFlags[s.resource]=s.flags; // draw state for the renderer (stage_world_instances); the collision route bits are unchanged
 if(s.dead)return; // 2FAE38 refuses a DeadNode (type 6): no Debounce
 if(!s.debounce){s.entityBeforeDebounce=s.entity;s.entity=StageNoopEntity;stage_magnet_freeze(s.resource);} // 0x355F10: the Debounce replaces any entity (its contact 3614F8 is a no-op)
 s.debounce=true;s.debounceTicks=int32_t(terrain_original::mul(duration,60.f));s.debounceRestore=restore;
}
// 30B9A0 for the human player: when the open collection matches, 30C3E0 marks a listed resource in the career
// stats (153B00), then (listed or not) the pickup sound and 10F338 -> 119EF8(score, 3, 151178(peak)) and 1599A0.
// 151178(144C98()): the award of the current course's peak (0x535C08's course table +0x54): 500 / 1000 / 2000. A world holding one
// peak carries it in its tables; the whole mountain (collectible_award -1) reads the current course (web/peak_world.inc).
// web/crash_runtime.hpp: a crash in a streamed peak world leaves the dynamic entities without callbacks out of its queries.
bool browser_crash_partial_world(){return browser_streamed_world(setPieceLocationName);}
extern "C" int peak_world_course();
static int32_t stage_collectible_amount(){
 if(stageTables->collectibleAward>=0)return stageTables->collectibleAward;
 static constexpr int8_t coursePeak[22]={0,0,1,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,0,1,1,2}; // course table 0x43D950 +0x54
 const int32_t c=peak_world_course();return c>=0&&c<22?(coursePeak[c]==0?500:coursePeak[c]==1?1000:2000):500;
}
static void stage_collectible_award(uint32_t resource,int32_t id){
 if(stageCollection.id!=id)return;
 uint32_t listed=0xffffffffu; // the list index, or none: 30B9A0 pays an unlisted resource too (cash only, web/stage-collect.js pv unlistedPickup)
 for(size_t i=0;i<stageCollection.resources.size();i++)if(stageCollection.resources[i]==resource){stageCollected.push_back(uint32_t(i));listed=uint32_t(i);
  // 30C3E0 -> 153B00 sets the career stats bit at once, and the next setup of this stage (30C4A8 -> 1538E8) makes the instance
  // a DeadNode. The core's copy of that career row (web/stage-collect.js) takes the bit too: a streamed location that unloads
  // and reads again (browser_stage_track_reset, then browser_stage_track_setup) keeps its collected ones dead this session.
  if(i<64){uint32_t* mask=stageCollectMask;if(browser_streamed_world(setPieceLocationName)&&stageCollectionTrack>=0)mask=stageCollectRows[uint32_t(stageCollectionTrack)].data();mask[i>>5]|=1u<<(i&31u);}
  break;}
 if(sharedWorldReplay)return; // the award belongs to the replayed rider's core
 if(stageCollectEvents.size()<2*64)stageCollectEvents.insert(stageCollectEvents.end(),{listed,uint32_t(stage_collectible_amount())}); // career save (153B00 for a listed one, then 151178 -> 10F338 -> 119EF8 -> 150A90 cash unconditionally)
 ++stageCollectibleAwards;audio_event(AE_PICKUP,5,1); /*30B9A0 -> 29CED8(5)*/push_score();originalScoreCareerAward(scoreObject,3,stage_collectible_amount(),score_environment(),audioScoreHooks);pull_score();
}
// Host input (web/main.js, before every race start / restart): path = 0x535C11 (0 career, 1 single event, 2 multiplayer)
// and the course's career collect mask (lo = list indexes 0..31, hi = 32..63). Default: single event.
extern "C" EMSCRIPTEN_KEEPALIVE void set_stage_collect_state(uint32_t path,uint32_t lo,uint32_t hi){stageCollectPath=path;stageCollectMask[0]=lo;stageCollectMask[1]=hi;}
// The collectible slot ctx+0x2C0 as 30C4A8 filled it: [id, count, resources in list order]. The career bit of a collectible is its list
// index (30C4A8 stores the resource at +0xC + 4 * count, then 1538E8(profile, 0, index = count, id); 0x30C510..0x30C548), so a savestate's
// collected set (its DeadNode collectibles in a CTM race) maps to the mask through this list (web/compare-ai-capture.mjs --in-world-ai).
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* stage_collection_list(){RIDER_LOCAL static std::vector<uint32_t> out;out.assign({uint32_t(stageCollection.id),uint32_t(stageCollection.resources.size())});out.insert(out.end(),stageCollection.resources.begin(),stageCollection.resources.end());return out.data();}
int browser_stage_collect_path(){return int(stageCollectPath);} // 0x535C11 (web/peak_world.inc: the crossing's game-tick restart)
// 11A228 / 119608 / 117718: 0x535C10 (event kind 4 = free ride) and 0x535C11 (0 = Conquer the Mountain) -> tricks pay cash.
static bool score_points_to_career(){return browserEventKind==4&&stageCollectPath==0;}
// Collected this race since the last call: [count, then (list index, award amount)] (the career save marks the bit and
// adds the cash: 30C3E0 -> 153B00, 119EF8 -> 150A90); index 0xFFFFFFFF: an unlisted resource, cash only.
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* stage_collect_events(){RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,uint32_t(stageCollectEvents.size()/2));out.insert(out.end(),stageCollectEvents.begin(),stageCollectEvents.end());stageCollectEvents.clear();return out.data();}
// 10F1C0 effect table 0x456AF0 for the current player (the human rider in this core).
static void score_stage_points(int32_t points){push_score();originalScorePickup(scoreObject,points,score_environment(),audioScoreHooks);pull_score();}
static void stage_effect(int type,float amount){
 if(sharedWorldReplay)return; // another rider's pickup/reset: the effect belongs to that rider
 switch(type){
 case 1:case 2:award_boost_pickup(type,amount);break;       // rider vt+0x6C 10E770 (+0x2E8) / vt+0x74 10E7D0 (+0x2EC)
 case 5:if(browserResetBegin)browserResetBegin(4);break;    // 116120(rider,0,4): "Wrong Way!" and control 9
 case 6:{score_stage_points(int32_t(amount));award_boost(0.f,0x10);physicsState.boost=boostState.amount;audio_event(AE_PICKUP,3,1);break;} /*10E8F8: 29CED8(3, 1.0)*/ // 10E8B8: 119608 then 10E098(rider,0x10,0)
 case 3:{ // rider vt+0x78 10E830: only in the air or on a rail (11FE98 == 1 or 4): 119448 multiplier icon (R&B x2/x3/x5/x10),
  // 10E098(rider,4,result), 29CED8(audio,2,rider,value)
  const int m=npc_motion_mode();if(m==1||m==4){push_score();const float r=originalScoreMultiplierPickup(scoreObject,amount);pull_score();award_boost(r,4);physicsState.boost=boostState.amount;audio_event(AE_PICKUP,2,amount);}break;}
 case 0:case 7:break;
 default:++stageUnsupported;break;                          // 4 (10E098(rider,8)): no authored use
 }
}
extern "C" {
// Diagnostics: [dispatches, programs run, unsupported, last resource, last program, instances, fired count] then (tick, resource, program) triples.
// QA: calls per builtin index (0..127) since the last stage reset (table 0x441F38 order).
EMSCRIPTEN_KEEPALIVE uint32_t* stage_builtin_counts(){return stageBuiltinCalls.data();}
EMSCRIPTEN_KEEPALIVE uint32_t* stage_script_info(){
 RIDER_LOCAL static std::vector<uint32_t> out;out={stageDispatches,stagePrograms,stageUnsupported,stageLastResource,stageLastProgram,uint32_t(stageInstances.size()),uint32_t(stageFiredLog.size()/3)};
 out.insert(out.end(),stageFiredLog.begin(),stageFiredLog.end());return out.data();
}
// 1 while the instance draws (instance+8 low bits not cleared by a Debounce / RestoreNode).
EMSCRIPTEN_KEEPALIVE int stage_instance_visible(uint32_t resource){const auto* s=stage_instance(resource);return !s||!s->flagsKnown||(s->flags&0xfu)!=0;}
}
#include "mission_gameplay.inc" // Big Challenges: the WScript mission system (docs/peak-mountain.md)
