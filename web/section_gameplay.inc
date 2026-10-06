#include "streamed_world.hpp"
// Course "sections" of the race event (included by set_piece_gameplay.inc; engine/section_streaming.hpp,
// tools/export_sections.py -> /assets/<LOC>/SECTIONS/sections.json, docs/set-pieces.md "Section activation").
// The original activates stage pieces by proximity, not by load volumes: the manager 0x101B60 (end of the
// rider manager 0x128AF0, after every rider's passes of the tick) rescans every 20 ticks (or after 2000 cm
// of travel) the instances whose octree cell overlaps the human's rider+0x110 -/+ 15000 cm. Newly listed
// instances without an entity run their stage slot-1 program (their gameplay-RNG draws are the last draws
// of the tick, in resource order); unlisted ones lose their entity (0x34FD90) or run slot 3. Entities built
// in tick E first update in the entity pass of tick E + 1.
// The browser runs it in the human's core only (the scan follows the human), from browser_section_pass()
// after every rider's race_end (web/ai-racers.js endTick, main.js without computer riders). Core-side
// pieces (the raven spline, Metro-City bins, set-piece emitters) start/stop here; the JS players (LiveComp,
// flags, UV scroll) read the event log set_piece_sections() and apply tick E's events after that tick.
#include "../engine/section_streaming.hpp"
#include "../engine/world_residency.hpp"
#include "json.hpp"
RIDER_LOCAL_LAZY static ssx::sections::Activation sectionTemplate,sectionActivation;
// Texture chunks (draw eligibility only): W+0x3F0 states, stepped once per tick with the outer camera of the
// previous tick (0x3A8290; eye = outer+0x20, range = 1.5 x outer+0x08).
RIDER_LOCAL_LAZY static ssx::sections::ChunkStreaming chunkTemplate,chunkStreaming;RIDER_LOCAL static ssx::sections::Vec3 sectionViewerEye{};RIDER_LOCAL static float sectionViewerFar=0;RIDER_LOCAL static bool sectionViewerSet=false;
void browser_section_viewer(float x,float y,float z,float far){sectionViewerEye={x,y,z};sectionViewerFar=far;sectionViewerSet=true;}
RIDER_LOCAL static bool sectionsLoaded=false; // sectionsRunning: set_piece_gameplay.inc
RIDER_LOCAL static std::string sectionLocation;
// Event log for JS: (tick, resource, action, program, draws, word) per event; action = sections::Action.
RIDER_LOCAL static std::vector<uint32_t> sectionLog;RIDER_LOCAL static unsigned sectionScans=0;
static void section_start_piece(uint32_t resource,int32_t program);
static void section_stop_piece(uint32_t resource,bool multiSplineRelease);
static void section_multi_spline_count(uint32_t resource,int delta);
// 0x101C80: the scan reads the game tick 1298C8 (rider manager +8). A peak run's location crossing restarts it (1297C8 via
// 128A10, world state 10's background rider load: browser_game_tick_restart); the set-piece tick runs on, so the scan's
// tick is the set-piece tick minus the base of the last restart. The scan period (tick - A+0xD0 >= 20, signed) then waits
// for the distance or a -1 rescan, as on the PS2.
RIDER_LOCAL static int32_t sectionTickBase=0;
static void section_reset(){auto points=std::move(sectionActivation.points);sectionActivation=sectionTemplate;sectionActivation.points=std::move(points); /*a NIS director's point outlives a race reset (it is the director's)*/chunkStreaming=chunkTemplate;sectionLog.clear();sectionScans=0;sectionTickBase=0;sectionsRunning=sectionsLoaded&&browser_same_world(sectionLocation,setPieceLocationName);}
// Called before the race_begin of the restart tick (chairliftTicks - 1 then is this tick): the scans of that tick read value.
void browser_section_tick_restart(uint32_t value){sectionTickBase=int32_t(chairliftTicks)-int32_t(value);}
extern "C" EMSCRIPTEN_KEEPALIVE int init_sections(const char* text){
 const auto doc=nlohmann::json::parse(text);
 ssx::sections::loadSections(doc,sectionTemplate,&chunkTemplate);
 sectionLocation=doc.at("location").get<std::string>();sectionsLoaded=true;
 if(browser_streamed_world(sectionLocation))sectionTemplate.present=ssx::worldTrackOctree().data(); // streamed world: resident tracks only (web/peak_world.inc)
 section_reset(); // normally before the first race (the renderer loads it after the world)
 return int(sectionTemplate.instances.size());
}
// New race (browser_attach_set_pieces): back to the race-tick-0 entity set (the ready savestate; the load
// leaves the list empty with A+0xD0 = -1, so the first scan runs at the end of tick 0).
// A piece was built outside a scan (trigger / timer programs, 0x34FB00 -> 0x1032C0).
static void section_entity_created(uint32_t resource){
 if(!sectionsRunning)return;
 // Instances outside the activation octree get their cell from their bounds (0x328F28) when the entity is built.
 ssx::sections::Cell cell;const ssx::sections::Cell* at=nullptr;
 if(!sectionActivation.find(resource))if(auto* w=set_piece_instance_at(resource)){cell=ssx::sections::cellFromBounds({w->low[0],w->low[1],w->low[2]},{w->high[0],w->high[1],w->high[2]});at=&cell;}
 sectionActivation.entityCreated(resource,ssx::sections::EntityKind::Plain,at);
}
static void section_entity_destroyed(uint32_t resource){if(sectionsRunning)sectionActivation.entityDestroyed(resource);}
// Stage setup 30C4A8 (builtin38 collectibles, web/stage_script_gameplay.inc): a career race starts an uncollected one
// without entity, its slot-1 program building the LiveComp (+ magnet, halo; one gameplay-RNG draw); a single event or
// a collected one is a DeadNode (never listed).
void section_collectible_state(uint32_t resource,bool dead){
 if(!sectionsRunning)return;auto* i=sectionActivation.find(resource);if(!i)return;
 i->entity=dead?ssx::sections::EntityKind::Dead:ssx::sections::EntityKind::None;
 if(!dead&&i->slot1>=0)if(auto it=sectionActivation.programs.find(i->slot1);it!=sectionActivation.programs.end())it->second.creates=ssx::sections::EntityKind::Plain;
}
// Mid-run seed (web/stage_world.inc set_world_visual_state): the 0x101B60 object of a PS2 savestate. lastScanAgo = game
// tick - A+0xD0 (-1: rescan), listed = A+0xDC (resource, instance parity bit 0x200).
void section_seed_state(const std::vector<std::pair<uint32_t,bool>>& listed,int32_t lastScanAgo,const std::array<float,3>& last,bool parity){
 if(!sectionsRunning)return;auto& a=sectionActivation;a.reset();
 const int32_t now=int32_t(chairliftTicks)-sectionTickBase; // the next scan's tick (chairliftTicks - 1 after the coming race_begin)
 if(lastScanAgo>=0){a.rescan=false;a.lastScan=now-lastScanAgo;}
 a.last={last[0],last[1],last[2]};a.parity=parity;
 for(const auto& [r,p]:listed)for(uint32_t k=0;k<a.instances.size();k++)if(a.instances[k].resource==r){a.instances[k].listed=true;a.instances[k].parity=p;a.list.push_back(k);break;}
 std::sort(a.list.begin(),a.list.end());
}
// Streamed world (web/peak_world.inc): 230338 (row -> 1/2) sets A+0xD0 = -1; 230360 -> 103308 (row 5 -> 7) drops the
// location's instances from the list.
void browser_section_rescan(){if(sectionsRunning)sectionActivation.rescan=true;}
// 129768 (the rider manager's restart, C vt+0xCC+0x28(3) at WS2's exit: a CTM round card's Continue, 0x236D28) -> 0x103358: the
// activation list emptied (flags unlisted, entities kept) and a rescan at the end of the next tick. PS2 c0a-full-ai: A+0xD8 132 -> 0 with
// A+0xD0 = -1 at the countdown's tick 0, 147 listed at tick 1, the slot-1 programs of the unbuilt pieces run again (pv eventInWorld, main.js).
extern "C" EMSCRIPTEN_KEEPALIVE void section_restart(){if(sectionsRunning)sectionActivation.reset();}
// 230180 (the CTM world reset: WS13 Next heat / results Restart, WS14 to the same course, WS15, cGame_restart 2302A8): the set-piece
// buckets 1 and 8 purged (8: the DeadNodes, destructor 0x360A28; PS2 r6-dtor), the missions, the stage world and its programs, the
// rollers and pickups back to their load state (the port's reset_race world part, browser_reset_pickups); the activation list is kept
// (no 0x103358: PS2 c0d-ws15 75 entries through WS15). pv eventReturnInWorld (main.js). Unconfirmed item by item against 230180's
// callees (26CA90, 309030, 2D9B40, 22E7C8, 3A6800, 26F228, 2789E0, 15DB58, 229498, 343BC0, 357B38, 309F18, 308C60, 28B180).
void browser_reset_pickups();void browser_mission_world_reset(); // web/mission_gameplay.inc: 308C60 -> 308DB8 (the markers' Hide again)
extern "C" EMSCRIPTEN_KEEPALIVE void ctm_world_reset(){browser_reset_pickups();browser_mission_world_reset();}
// A NIS director's camera point in the activation (engine/section_streaming.hpp Activation::points): on = 1 adds it (0x281370 ->
// 0x1033B0, when the script starts) or, while it is in, sets where it is (0x281100: the outer camera's +0x20 = the rendered eye as the
// previous tick's camera update left it, copied in the director's update before this tick's 0x101B60); on = 0 removes it (0x281400 ->
// 0x1033F8, the script's end). Both force a scan. PS2 cm, the rider's frame (physicsState.position). Returns the point count.
extern "C" EMSCRIPTEN_KEEPALIVE int section_point(int on,float x,float y,float z){
 if(!on)sectionActivation.removePoint();
 else if(sectionActivation.points.empty())sectionActivation.addPoint({x,y,z});
 else sectionActivation.points.back().pos={x,y,z};
 return int(sectionActivation.points.size());
}
void browser_section_drop_track(uint32_t track){if(sectionsRunning)sectionActivation.dropTrack(track);}
// Mid-run seed (web/peak-capture.mjs): the live entity class of an instance in a PS2 savestate (export_sections entity_kind).
void browser_section_seed_entity(uint32_t resource,const std::string& kind){if(!sectionsRunning)return;if(auto* i=sectionActivation.find(resource))i->entity=ssx::sections::entityKindFromName(kind);}
void section_mark_dead(uint32_t resource){if(!sectionsRunning)return;if(auto* i=sectionActivation.find(resource))i->entity=ssx::sections::EntityKind::Dead;}
// 0x101B60 at the end of tick T (T = set-piece tick of the last race_begin).
RIDER_LOCAL extern bool sharedWorldReplay; // web/shared_world.inc
static int section_scan();static void stage_world_visual_pass(); // web/stage_world.inc
static uint64_t stage_world_serial();static void stage_world_tick_newer(uint64_t serial); // web/stage_world.inc
// 0x101B60 (end of the rider manager), then the post-rider world passes that draw from the visual stream.
extern "C" EMSCRIPTEN_KEEPALIVE int section_pass(){
 const uint64_t serial=stage_world_serial();
 const int events=section_scan();
 if(browserHumanRider&&chairliftTicks&&!sharedWorldReplay)stage_world_visual_pass();
 // A PS2 record falls after the human's 11B3F8, so one record interval runs the computer riders, this 0x101B60 and then the next
 // race_begin's entity pass; the port's tick runs its entity pass first. An entity this scan built (a slot-1 program) is updated in
 // that entity pass on the PS2, before the next record: update it here (PS2 hl2/carve-s1-era5 snapshots 545 / 1150 / 1193: the crash
 // billboard 332333 built by the 540 scan has 5 steps at 545; the port had 4, and its program 85 drew one tick late).
 stage_world_tick_newer(serial);
 return events;
}
static int section_scan(){
 if(!sectionsRunning||!browserHumanRider||!chairliftTicks)return 0;
 const int32_t tick=int32_t(chairliftTicks-1),gameTick=tick-sectionTickBase; // gameTick: 1298C8 (sectionTickBase: a crossing's restart)
 // Relocate the moving entity-routed pieces (0x3291E0) with this tick's entity bounds.
 for(const auto& lift:browserChairlifts)if(lift.active&&!lift.modifier.cars.empty()){const auto& c=lift.modifier.cars[0];
  sectionActivation.relocate(lift.resource,{c.boundsMin[0],c.boundsMin[1],c.boundsMin[2]},{c.boundsMax[0],c.boundsMax[1],c.boundsMax[2]});}
 for(const auto& piece:browserSplinePieces){ // Spline / Position entity bounds (translation -/+ radius)
  const auto& lo=piece.phase==2?piece.position.boundsMin:piece.spline.boundsMin;const auto& hi=piece.phase==2?piece.position.boundsMax:piece.spline.boundsMax;
  sectionActivation.relocate(piece.seed->resource,{lo[0],lo[1],lo[2]},{hi[0],hi[1],hi[2]});}
 for(const auto& loop:browserLoopSplines)if(loop.ready&&loop_relaunch_seed(loop.seed->resource)){ // resident Spline entity, same bounds
  const auto& lo=loop.spline.boundsMin;const auto& hi=loop.spline.boundsMax;
  sectionActivation.relocate(loop.seed->resource,{lo[0],lo[1],lo[2]},{hi[0],hi[1],hi[2]});}
 if(sectionViewerSet){terrain_original::Rounding rounding;chunkStreaming.frame(tick,tick,sectionViewerEye,terrain_original::mul(1.5f,sectionViewerFar));}
 std::vector<ssx::sections::Event> events;
 const ssx::sections::Vec3 position{physicsState.position[0],physicsState.position[1],physicsState.position[2]};
 if(!sectionActivation.update(gameTick,position,events))return 0;
 ++sectionScans;
 for(const auto& e:events){
  using ssx::sections::Action;
  uint32_t word=0;
  for(unsigned k=0;k<e.rngDraws;k++)word=browser_shared_random_next(); // 0x317810 (builtin3 key 8, builtin19)
  if(e.action==Action::Slot1&&!e.guarded)section_start_piece(e.resource,e.program); // guarded: returned at its builtin52 head (the blimps' rebuild programs)
  else if(e.action==Action::Destroy)section_stop_piece(e.resource,false);
  else if(e.action==Action::MultiSplineRelease||e.action==Action::MultiSplineAcquire)section_multi_spline_count(e.resource,e.action==Action::MultiSplineAcquire?1:-1); // not a destroy: the entity update decides (set_piece_gameplay.inc)
  else if(e.action==Action::Slot3)stage_world_section_leave_program(e.resource,e.program);
  if(e.action!=Action::None&&sectionLog.size()<6*65536){sectionLog.insert(sectionLog.end(),{uint32_t(tick),e.resource,uint32_t(e.action),uint32_t(e.program),uint32_t(e.rngDraws),word});}
 }
 return int(events.size());
}
// Section log: count, scans, then per event (tick, resource, action, program, draws, drawn word).
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* set_piece_sections(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign({uint32_t(sectionLog.size()/6),sectionScans});out.insert(out.end(),sectionLog.begin(),sectionLog.end());return out.data();
}
// Listed flag per resource (QA): count, then (resource, listed, entity kind) for the given resources.
extern "C" EMSCRIPTEN_KEEPALIVE int section_listed(uint32_t resource){auto* i=sectionActivation.find(resource);return i?(i->listed?2:1)|(int(i->entity)<<4):0;}
// Texture chunk states (0 free, 1 requested, 2 loading, 3 resident, 4 releasing): count, then (chunk, state) pairs.
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* section_chunks(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,uint32_t(chunkStreaming.chunks.size()));
 for(const auto& [id,c]:chunkStreaming.chunks){out.push_back(uint32_t(id));out.push_back(sectionsRunning?c.state:3u);}
 return out.data();
}
// An Object-entity MultiSpline destroyed by its entity update (web/set_piece_gameplay.inc): no entity, so the next enter runs slot 1.
static void section_multi_spline_destroyed(uint32_t resource){sectionActivation.entityDestroyed(resource);}
