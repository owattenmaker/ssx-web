// Streamed Peak 1 world (docs/peak-mountain.md), included at the end of web/world_bridge.cpp.
//
// Free ride and the peak runs stream whole locations in and out of the world (ELF location table 0x43E250,
// streaming table 0x442168). The browser fetches each location's package once (web/peak-world.js, in slices
// on idle frames) and APPENDS it to the loaded world through the ordinary init functions (browserWorldAppend);
// whether the world queries see it is the per-track residency flag (engine/world_residency.hpp), which follows
// the original streaming rows exactly. The rider's location id (22E0E0, rider+0x434) reads the same rows.
#include "../engine/world_residency.hpp"
#include "../engine/collision.hpp"
void browser_unset_track_location(const std::string& name);void browser_rails_reserve(size_t count);size_t browser_world_release_track(uint32_t);size_t browser_rails_release_track(uint32_t);
extern "C" {
// Start a streamed world: nothing is resident until the streaming state says so. Reserves the instance and
// rail storage so pointers held by set pieces and an attached rail survive later appends.
EMSCRIPTEN_KEEPALIVE void peak_world_begin(){ssx::worldTrackResidency().fill(0);ssx::worldTrackOctree().fill(0);browser_clear_track_locations();browserWorldAppend=false;}
EMSCRIPTEN_KEEPALIVE void peak_world_reserve(int instanceCapacity,int railCapacity){
 if(browserBodies&&instanceCapacity>0)browserBodies->instances.reserve(size_t(instanceCapacity));
 if(railCapacity>0)browser_rails_reserve(size_t(railCapacity));
}
// The next init_* calls append (1) or replace (0).
EMSCRIPTEN_KEEPALIVE void peak_world_append(int on){browserWorldAppend=on!=0;}
// A location's staged body-terrain slices join the traversal order (one index sort + linear merge).
extern std::unique_ptr<ssx::CollisionWorld> cameraTerrain;
EMSCRIPTEN_KEEPALIVE void peak_world_commit(){if(browserBodies)browserBodies->commitTerrainTraversal();if(cameraTerrain)cameraTerrain->finishAppend();}
// A location entered (resident=1) or left (0) the world: queries see its track, and its streaming row holds the track
// for 22E0E0. Returns the number of resident tracks.
EMSCRIPTEN_KEEPALIVE int peak_world_set_resident(int track,const char* location,int resident){
 if(track<0||track>255)throw std::runtime_error("Track slot out of range");
 ssx::worldTrackResidency()[size_t(track)]=resident?1:0;
 if(resident)browser_set_track_location(track,location);else browser_unset_track_location(location);
 int n=0;for(auto v:ssx::worldTrackResidency())n+=v!=0;return n;
}
// Back to a single race-event package: every track resident (the default of engine/world_residency.hpp).
EMSCRIPTEN_KEEPALIVE void peak_world_end(){ssx::worldTrackResidency().fill(1);ssx::worldTrackOctree().fill(1);browserWorldAppend=false;}
EMSCRIPTEN_KEEPALIVE int peak_world_resident(int track){return track>=0&&track<256?ssx::worldTrackResidency()[size_t(track)]:0;}
}

// ---- the original location streaming (docs/peak-mountain.md "Streaming") ---------------------------------------------
// Rows mirror the streaming table 0x442168 (one per ELF location id, +8 state); the residency rows 0x442488 come from the
// manifest (web/public/assets/PEAK1/peak.json). Row states (22D3D8 / 22D478 / 22D598 / 22D8D8):
//   0 absent, 3 load wanted, 4 load wanted + activate, 6 reading, 8 reading + activate, 1 resident inactive (in the
//   octree, patch flag 0x40 clear: neither collidable nor drawn), 2 resident active (collidable 0x41, drawn: track 6),
//   5 unload wanted, 7 unloading (track 6 -> 3: no longer drawn, still collidable until its chunk slot is evicted).
// 22CEA8 (the connectors' Unload trigger, builtin 68 action 0) releases every other location (1/2/6 -> 5, 3 -> 0; 4/8
// kept) and wants the row (0/7 -> 3, 5 -> 1); 22D088 (the Load trigger, action 2; the initial load) wants it with
// activation (0/3/7 -> 4, 6 -> 8, 1/5 -> 2 at once, in the trigger tick). Sky and TRANSP rows are wanted, never released.
// One streamer pass runs at the start of every game tick (peak_world_tick() after race_end is the next tick's pass):
// unloads first (5 -> 7; 7 -> 0 at the eviction, 7 passes later), any row still in 5/7 blocks load starts; then 3 -> 6 /
// 4 -> 8 and, one pass later, one disc read at a time by slot priority (sky 0.92 > TRANSP 0.91 > locations 0.90, then
// the lowest chunk index), each taking its measured time; at completion the resolver runs (12A340 delivers the path bank
// of a location with id < 22) and the next pass moves 6 -> 1 / 8 -> 2 and starts the next read. A sky reaching 1 is
// activated at once (22DE58) and replaces the dome when the switch is allowed (+0x1A4: skybox trigger, action 5).
// 0x535C08 (the current course: HUD counter, finish rule) changes on the first pass after 22CEA8/22D088 (22DF50).
// PS2 measurements: S/peak1/streaming.md (frA1/frA2 A -> ARA1, frB1..3 ARA1 -> B, fr-tuck ABC1 -> A).
#include "json.hpp"
#include "../engine/npc_path.hpp"
void browser_section_rescan();void browser_section_drop_track(uint32_t);void browser_stage_track_reset(uint32_t);void browser_rollers_track_teardown(uint32_t); // web/roller_gameplay.inc (the crashbags' Object entities at the unload start)
 // web/section_gameplay.inc, web/stage_world.inc
void browser_stage_seed_node(uint32_t,int,uint32_t);void browser_stage_track_setup(uint32_t);void browser_stage_track_teardown(uint32_t);void browser_section_seed_entity(uint32_t,const std::string&);
void browser_lighting_default_course(int);bool browser_race_replace_paths(std::vector<ssx::OriginalRacePath>,int);bool browser_reset_replace_paths(std::vector<ssx::OriginalNpcPath>,int);
void browser_game_tick_restart(uint32_t);int browser_stage_collect_path(); // web/animation_bridge.cpp, web/stage_script_gameplay.inc
std::vector<ssx::OriginalNpcPath> browser_parse_reset_paths(const nlohmann::json&);extern "C" void race_finish_eligible(int);
namespace peak_stream {
struct Row {int32_t track=-1,state=0,chunk=0,kind=0,readTicks=0,evict=0,order=0,duration=0;bool data=false,reading=false,released=false,evictFresh=false;std::string name;float priority=.9f;};
struct Residency {int32_t sky=-1,transp=-1;std::vector<int32_t> locations;};
RIDER_LOCAL static std::array<Row,50> rows;RIDER_LOCAL_LAZY static std::map<int32_t,Residency> residency;RIDER_LOCAL_LAZY static std::map<int32_t,int32_t> mapIds; // table +0x5C -> course
RIDER_LOCAL static int32_t currentCourse=-1,pendingCourse=-1,requests=0,passes=0,orderCounter=0;
RIDER_LOCAL static int32_t loaderRow=-1,loaderDone=-1;      // the one disc read in flight (WO+0x390/+0x394) and its completion pass
RIDER_LOCAL static int32_t skyActive=-1,skyPending=-1;RIDER_LOCAL bool skySwitchAllowed=false; // dome, +0x1AC, +0x1A4
RIDER_LOCAL static bool crossingPending=false;              // 22D640/22D668 phase: 22DF50 on the next pass
// The Unload's game-tick restart (web/animation_bridge.cpp browser_game_tick_restart): 22DF50 requests world state 11 and the
// background state 10; 235080 waits for the destination's NIS script read (278B98 / 278D58), then sub-states 1, 2, 3 (one
// tick each); sub-state 3 calls 128A10 -> 1297C8 in a Conquer the Mountain time challenge (0x535C10 5, 0x535C11 0; 1289F0 in
// event type 0). PS2 (apr-full / p2r-full, Unload record -> the record reading 0): 12 into a hub or station, 13 into
// Snow Jam / Ruthless / Intimidator, 9 into Metro-City, 24 into Gravitude (and once into Yellow Station: the read waited for
// another); the disc read's time is not modelled, so these measured delays stand in for it. The captures drive the restart
// themselves (web/peak-capture.mjs: the record's 1298C8).
RIDER_LOCAL static bool crossingUnload=false;RIDER_LOCAL static int32_t tickRestartIn=0;
static int32_t tick_restart_delay(int32_t course){return course>=17?12:course==1?9:course==4?24:13;}
RIDER_LOCAL static bool loadedFlag=false;                   // streamer +0x1C0
RIDER_LOCAL static std::vector<int32_t> events; // {kind, a, b, c}: 1 builtin 68 (course, action, mode), 3 builtin 67 (entry, flag), 4 sky (location id), 5 course (0x535C08)
RIDER_LOCAL static bool active=false;RIDER_LOCAL static int32_t eventKind=4,gameMode=12;RIDER_LOCAL_LAZY static std::map<int32_t,nlohmann::json> pathBanks; // location id -> paths.json
RIDER_LOCAL static int32_t bankLocation=-1,bankVariant=-1;
RIDER_LOCAL static int32_t faqShown=1; // 147580(P, 0): the career's first FAQ was shown (1 outside a career: no host call)
static const int32_t kEvictionPasses=7; // rows 5 -> 7 on the first pass, chunk slots evicted 7 passes later (frA2: +1 .. +8)
// The eviction itself (3A8668 updatePages frees the page 5 renderer frames after 3A84C8 made it unwanted: 3A8528 -> 3A8E20 -> 3AB498
// -> 3284B8 unlinks the location's patches, instances and rail segments from the octree) runs in the world update of T+7, before
// that tick's gameplay; the row reads 0 one pass later (T+8, 3A8230 then 22D8D8 case 7). PS2 release/throne-evict: EBC3 5 -> 7 in
// record 14422, the octree removal counter *(root+0xA0) 6545 -> 11500 in record 14428, the row 0 in 14429.
static const int32_t kReleasePass=1; // evict countdown value at the removal pass (T+7)
// 10E5D8: a course finish event counts in kinds 0..3, in the peak runs (kinds 5/6, GMM+0x98 = 0) and in free ride only on
// the courses 1 and 5..13.
static void update_finish_rule(){const int32_t c=currentCourse;race_finish_eligible(eventKind<4||c==1||(c>=5&&c<=13));}
// 12A340: the delivered AIP of a location with id < 22 replaces the path bank; hubs take variant 1 in a time challenge,
// 2 in a points challenge (1 in mode 11), 0 otherwise; then 112180 re-attaches the human.
static void deliver_paths(int32_t id){
 if(id>=22)return;auto it=pathBanks.find(id);if(it==pathBanks.end())return;
 int32_t variant=0;if(id>=17&&id<=21){if(eventKind==5)variant=1;else if(eventKind==6)variant=gameMode==11?1:2;}
 const auto& variants=it->second.at("variants");if(!variants.contains(std::to_string(variant)))return;
 const auto& bank=variants.at(std::to_string(variant));
 std::vector<ssx::OriginalRacePath> race;
 for(const auto& p:bank.at("race_paths")){ssx::OriginalRacePath path;path.origin=p.at("origin").get<std::array<float,3>>();path.low=p.at("low").get<std::array<float,3>>();path.high=p.at("high").get<std::array<float,3>>();path.remainingAtOrigin=p.at("remaining_at_origin");path.segments=p.at("segments").get<std::vector<std::array<float,4>>>();
  for(const auto& e:p.at("events"))path.events.push_back({e.at("type").get<uint32_t>(),e.at("value").get<uint32_t>(),e.at("start").get<float>(),e.at("end").get<float>()});race.push_back(std::move(path));}
 // 112180: 26B5E0(bank, 1, rider+0x86C = 0). The runtime region kinds are the exported (disc) kinds + 1 (PS2 fr/sj-01, race18001),
 // so this is exported kind 0 index 0 (the grid slot); 26B5E0 returns the bank's first row when no row matches.
 const nlohmann::json* region=nullptr;for(const auto& r:bank.at("regions"))if(r.at("kind")==0&&r.at("index")==0){region=&r;break;}
 if(!region&&!bank.at("regions").empty())region=&bank.at("regions").at(0);
 bankLocation=id;bankVariant=variant;if(!region)return;
 browser_race_replace_paths(std::move(race),region->at("race_path").get<int>());
 browser_reset_replace_paths(browser_parse_reset_paths(bank.at("reset_paths")),region->at("reset_path").get<int>());
}
static void drop_paths(){bankLocation=-1;bankVariant=-1;browser_race_replace_paths({},-1);browser_reset_replace_paths({},-1);}
// Query gating by track: collidable in states 2, 5, 7 (until the eviction at T+7); drawn in state 2 only (JS reads the rows).
static void apply(int32_t id){auto& r=rows[size_t(id)];if(r.track>=0&&r.kind==0)ssx::worldTrackResidency()[size_t(r.track)]=(r.state==2||r.state==5||(r.state==7&&!r.released))?1:0;}
static void release(int32_t id){auto& r=rows[size_t(id)];if(r.released)return;r.released=true;apply(id);}
// Section activation (0x101B60, web/section_gameplay.inc): a location's instances are in the octree from its read
// completion (octree_insert) until the unload start (row -> 7: 230360 -> 103308 drops them from the list, the stage
// teardown ends their entities); 3A9858 (row 6 -> 1 / 8 -> 2) calls 230338, a rescan. A location that leaves state 7 was
// evicted (3AB498): its instances come back from the disc without entities.
static void octree_insert(int32_t id){auto& r=rows[size_t(id)];if(r.track>=0&&r.kind==0)ssx::worldTrackOctree()[size_t(r.track)]=1;}
static void set_state(int32_t id,int32_t state){auto& r=rows[size_t(id)];if(r.state==state)return;const int32_t from=r.state;r.state=state;r.order=++orderCounter;r.released=false;if(state==7){r.evict=kEvictionPasses;r.evictFresh=true;}apply(id); // 22E298: to the queue tail
 if(r.track<0||r.kind!=0)return;
 if((from==6||from==8)&&(state==1||state==2)){octree_insert(id);browser_section_rescan();}
 else if(state==7){ssx::worldTrackOctree()[size_t(r.track)]=0;browser_section_drop_track(uint32_t(r.track));browser_stage_track_teardown(uint32_t(r.track));browser_rollers_track_teardown(uint32_t(r.track));}
 else if(from==7)browser_stage_track_reset(uint32_t(r.track));
}
static bool wanted_by(const Residency& set,int32_t id){return id==set.sky||id==set.transp||std::find(set.locations.begin(),set.locations.end(),id)!=set.locations.end();}
// 22CEA8 (activate=false) / 22D088 (activate=true)
static void request(int32_t course,bool activate){
 auto it=residency.find(course);if(it==residency.end())return;const auto& set=it->second;
 pendingCourse=course;crossingPending=true;crossingUnload=!activate;++requests;loadedFlag=activate; // +0x1C0: 1 after 22D088, 0 after 22CEA8
 if(activate)browser_lighting_default_course(course); // 22D088 -> 22E180: the peak's default rider irradiance bank (web/environment_bridge.cpp)
 for(int32_t id=0;id<50;++id){auto& r=rows[size_t(id)];if(r.name.empty()||wanted_by(set,id)||r.kind!=0)continue; // 22D598 (never skies / TRANSP)
  if(r.state==1||r.state==2||r.state==6)set_state(id,5);else if(r.state==3)set_state(id,0);}
 std::vector<int32_t> want(set.locations);if(set.sky>=0)want.push_back(set.sky);if(set.transp>=0)want.push_back(set.transp);
 for(int32_t id:want){const int32_t s=rows[size_t(id)].state;
  if(activate){if(s==0||s==3||s==7)set_state(id,4);else if(s==6)set_state(id,8);else if(s==1||s==5)set_state(id,2);} // 22D478
  else{if(s==0||s==7)set_state(id,3);else if(s==5)set_state(id,1);}}                                                // 22D3D8
 if(set.sky>=0&&activate==false)skySwitchAllowed=false; // 22CEA8 clears +0x1A4 when the destination row has a sky
}
static void sky_switch(){
 if(skyPending<0||!skySwitchAllowed)return; // 22DE98: the other skies unload, the dome switches (353D98 + 353CF0)
 for(int32_t id=0;id<50;++id){auto& r=rows[size_t(id)];if(r.kind==2&&id!=skyPending&&(r.state==1||r.state==2))set_state(id,5);}
 skyActive=skyPending;skyPending=-1;skySwitchAllowed=false;events.insert(events.end(),{4,skyActive,0,0});
}
// One streamer pass (22D8D8) + the world's track/loader steps (3A9258, 3A8668, 3A7098).
RIDER_LOCAL static bool manual=false; // capture comparisons drive the rows from the PS2 records (peak_world_row)
static void pass(){
 if(manual)return;
 ++passes;
 if(tickRestartIn>0&&--tickRestartIn==0)browser_game_tick_restart(0); // 1297C8 before this tick's rider pass
 if(crossingPending){crossingPending=false;if(pendingCourse!=currentCourse){currentCourse=pendingCourse;update_finish_rule();events.insert(events.end(),{5,currentCourse,0,0});} // 22DF50
  if(crossingUnload&&browser_stage_collect_path()==0&&(eventKind==5||eventKind==0))tickRestartIn=tick_restart_delay(pendingCourse);crossingUnload=false;}
 bool blocked=false;
 for(int32_t id=0;id<50;++id){auto& r=rows[size_t(id)];
  if(r.state==5){set_state(id,7);r.evictFresh=false;blocked=true;}
  else if(r.state==7){if(--r.evict<=0)set_state(id,0); /* the AIP bank stays: 3AB498 kind 14 -> 12A490 -> 26ADA0 is jr ra (12A340 replaces it at the next delivery) */
   else{if(r.evict==kReleasePass)release(id);blocked=true;}}}
 if(loaderRow>=0&&passes>loaderDone){auto& r=rows[size_t(loaderRow)];r.reading=false;set_state(loaderRow,r.state==8?2:1);if(r.kind==2&&r.state==1){set_state(loaderRow,2);}
  if(r.kind==2){skyPending=loaderRow;} loaderRow=-1;}
 if(!blocked){
  for(int32_t id=0;id<50;++id){auto& r=rows[size_t(id)];if(r.state==3){set_state(id,6);r.reading=true;r.readTicks=-1;}else if(r.state==4){set_state(id,8);r.reading=true;r.readTicks=-1;}}
 }
 if(loaderRow<0){ // start the best waiting read (requested on an earlier pass)
  int32_t best=-1;
  for(int32_t id=0;id<50;++id){auto& r=rows[size_t(id)];if(!r.reading||(r.state!=6&&r.state!=8))continue;if(r.readTicks==-1){r.readTicks=-2;continue;} // tracks 1 -> 2 next pass
   if(best<0||r.priority>rows[size_t(best)].priority||(r.priority==rows[size_t(best)].priority&&r.chunk<rows[size_t(best)].chunk))best=id;}
  if(best>=0){auto& r=rows[size_t(best)];loaderRow=best;loaderDone=passes+r.duration;}
 }
 if(loaderRow>=0&&passes==loaderDone){octree_insert(loaderRow);deliver_paths(loaderRow);if(rows[size_t(loaderRow)].kind==0&&rows[size_t(loaderRow)].track>=0)browser_stage_track_setup(uint32_t(rows[size_t(loaderRow)].track));} // 3A7238 -> resolver: kind 3 -> 328C20, kind 14 -> 12A340 at the completion
 sky_switch();
}
}
RIDER_LOCAL extern void (*browserRaceAfterReset)();
static void peak_after_race_reset(){using namespace peak_stream;tickRestartIn=0;crossingUnload=false;if(!active)return;update_finish_rule();if(bankLocation>=0)deliver_paths(bankLocation);}
void browser_peak_builtin68(int32_t mapId,int32_t action,int32_t entry){
 using namespace peak_stream;if(!active)return;
 static const int32_t modes[8]={2,3,4,5,1,1,1,0}; // 0x445E40
 const int32_t mode=entry>=0&&entry<8?modes[entry]:0;
 auto c=mapIds.find(mapId);const int32_t course=c==mapIds.end()?-1:c->second;
 events.insert(events.end(),{1,course,action,mode});
 if(action==0&&course>=0)request(course,false);        // 22CEA8
 else if(action==2&&course>=0)request(course,true);    // 22D088
 else if(action==5)skySwitchAllowed=true;              // +0x1A4 (the skybox trigger)
}
void browser_peak_builtin67(int32_t entry,int32_t flag){if(!peak_stream::active)return;peak_stream::events.insert(peak_stream::events.end(),{3,entry,flag,0});}
// Stage builtin 110 (307020 -> 1E3570), called by the slot-1 program of Green Base Station's "?" (mdl_A_questionmark_1000,
// A program 8): 1 in Conquer the Mountain (0x535C11 == 0) outside the Peak 1 Race / Jam (0x535C12 != 6, 9) while the
// career has not shown its first FAQ (147580(P, 0) == 0); the program then adds the MagnetModifier (builtin 90) so the "?"
// flies to the rider. Builtin 100 (306300 -> 1E3510, the "?" contact, A program 9): gp-0x1024 = 0 (world state 4 then
// opens the Message Center on the Progression/Rewards FAQ 1 at 0x2309A4) and 1475C0(P, 0, 1) marks it shown. PS2 new
// career (docs/audio-logic.md 9.11): the DJ's Text_Message, then the "?" homes in at Green Base Station and the Message
// Center shows "How do I open up other peaks?". Event 6 tells the host (web/free-ride.js 'faq').
int32_t browser_peak_builtin110(){using namespace peak_stream;return active&&gameMode!=6&&gameMode!=9&&!faqShown?1:0;}
// Stage builtin 108 (302870 -> 303E60): n < 15 (unsigned) and table 0x446618 {0..13, 1}[n] == (s8)0x535C12, the game mode
// byte (12 free ride, 6-8 peak races, 9-11 peak jams, 13 mission). Only the stations' slot-1 programs call it (every peak's
// hubs: the peak-race fences / challenge reset planes run "if 43(5)==0 and 108(11)==0: DeadNode", the peak-jam ones "if
// 43(6)==0 or 108(11)!=0: DeadNode"; no event course program does). Without it the race fences stayed solid in free ride:
// Green Station's collision-only mdl_A_fenceCollision_r_* walled off the lodge (PS2 state-green-cutscene: flags 0x200204,
// DeadNode). -1 (nil, the old answer) unless the host turned it on (pv stationFences, web/free-ride.js) or outside a world.
RIDER_LOCAL static bool builtin108On=false;
int32_t browser_peak_builtin108(uint32_t n){using namespace peak_stream;if(!builtin108On||!active)return -1;
 static constexpr int32_t table[15]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,1};return n<15u&&table[n]==int32_t(int8_t(gameMode))?1:0;}
void browser_peak_builtin100(){using namespace peak_stream;if(!active)return;faqShown=1;events.insert(events.end(),{6,0,0,0});}
extern "C" {
// peak.json subset {locations:[{code,id,track,kind,chunk,read_ticks}], location_ids, residency:[{course,map_id,sky,locations}]}
EMSCRIPTEN_KEEPALIVE void peak_world_manifest(const char* text){
 using namespace peak_stream;const auto p=nlohmann::json::parse(text);rows={};residency.clear();mapIds.clear();events.clear();
 loaderRow=loaderDone=-1;skyActive=skyPending=-1;skySwitchAllowed=false;crossingPending=false;currentCourse=pendingCourse=-1;passes=0;
 std::map<std::string,int32_t> ids;
 for(const auto& l:p.at("streaming")){const int32_t id=l.at("id");auto& r=rows.at(size_t(id));r.name=l.at("code");r.track=l.value("track",-1);r.kind=l.at("class");
  r.priority=r.kind==2?.92f:r.kind==1?.91f:.9f;r.chunk=l.at("chunk");r.duration=std::max(1,l.at("read_ticks").get<int32_t>());ids[r.name]=id;
  if(r.kind==0&&r.track>=0)browser_set_track_location(r.track,r.name); // 22CD40 sets +4 by name: 22E0E0 answers for every track
 }
 for(const auto& r:p.at("residency")){Residency s;for(const auto& c:r.at("locations"))s.locations.push_back(ids.at(c.get<std::string>()));
  if(!r.at("sky").is_null())s.sky=ids.at(r.at("sky").get<std::string>());s.transp=ids.count("TRANSP")?ids.at("TRANSP"):-1;residency[r.at("course")]=s;
  mapIds[r.at("map_id").get<int32_t>()]=r.at("course");}
 for(int32_t id=0;id<50;++id)apply(id);
 active=true;browserRaceAfterReset=peak_after_race_reset;
}
// The initial world load 22F6B0: 22D088(course, 0) and loop until every row is resident (under the loading screen): all
// wanted rows end active, their reads (and path banks) in loader order.
EMSCRIPTEN_KEEPALIVE void peak_world_start(int course){
 using namespace peak_stream;request(course,true);skySwitchAllowed=true;
 for(int guard=0;guard<100000;++guard){pass();bool busy=loaderRow>=0;for(const auto& r:rows)busy|=r.state==3||r.state==4||r.state==6||r.state==8||r.state==5||r.state==7;if(!busy)break;}
}
EMSCRIPTEN_KEEPALIVE void peak_world_data_ready(int track,int ready){for(auto& r:peak_stream::rows)if(r.track==track)r.data=ready!=0;}
// One streamer pass: call once per game tick after race_end (it is the next tick's pass).
EMSCRIPTEN_KEEPALIVE void peak_world_tick(){if(peak_stream::active)peak_stream::pass();}
EMSCRIPTEN_KEEPALIVE int peak_world_course(){return peak_stream::currentCourse;}
// +0x1D0 == 0 (a row still in 4/8 after a Load trigger): 2306B8 skips the gameplay part of the frame until it is in.
EMSCRIPTEN_KEEPALIVE int peak_world_blocking(){for(const auto& r:peak_stream::rows)if(r.state==4||r.state==8)return 1;return 0;}
// [count, {location id, track, state, class}...] for every named row
EMSCRIPTEN_KEEPALIVE int32_t* peak_world_rows(){
 RIDER_LOCAL static std::vector<int32_t> out;out.assign(1,0);
 for(int32_t id=0;id<50;++id){const auto& r=peak_stream::rows[size_t(id)];if(r.name.empty())continue;out.insert(out.end(),{id,r.track,r.state,r.kind});++out[0];}
 return out.data();
}
// Free-ride runtime flags of a location's DeadNodes / type-16 nodes (PS2 free-ride audit, tools/export_peak_instances.py):
// {runtime:[[resource, flags]...]} -> instance+8, which routes the collision queries past them (like the event seeds).
EMSCRIPTEN_KEEPALIVE int peak_world_instance_flags(const char* text){
 if(!browserBodies)return 0;const auto p=nlohmann::json::parse(text);int n=0;
 for(const auto& row:p.at("runtime")){const uint32_t resource=row.at(0),flags=row.at(1);for(auto& i:browserBodies->instances)if(i.resource==resource&&!i.runtimeClone){i.eventRuntimeFlags=flags;++n;break;}}
 return n;
}
// Capture comparisons (web/peak-capture.mjs): the PS2 rows of each record replace the browser's loader timing.
EMSCRIPTEN_KEEPALIVE void peak_world_manual(int on){peak_stream::manual=on!=0;}
EMSCRIPTEN_KEEPALIVE void peak_world_row(int id,int state){if(id>=0&&id<50)peak_stream::set_state(id,state);}
// Capture comparisons, once per record after its rows: the eviction countdown of the rows in 7 (the record shows 7 until T+8, the
// octree removal is at T+7: pass() above).
EMSCRIPTEN_KEEPALIVE void peak_world_rows_tick(){using namespace peak_stream;for(int32_t id=0;id<50;++id){auto& r=rows[size_t(id)];if(r.state!=7)continue;if(r.evictFresh){r.evictFresh=false;continue;}if(--r.evict==kReleasePass)release(id);}}
EMSCRIPTEN_KEEPALIVE void peak_world_deliver(int id){if(id>=0&&id<50){peak_stream::octree_insert(id);peak_stream::deliver_paths(id);}}
// Mid-run seed of the stage/section state from a PS2 savestate (tools/export_peak_seed.py seed-state.json "nodes":
// [resource, node type 6/16/19, instance flags]; "entities": [resource, section entity class]).
EMSCRIPTEN_KEEPALIVE int peak_world_seed(const char* text){
 const auto p=nlohmann::json::parse(text);int n=0;
 for(const auto& e:p.at("entities")){browser_section_seed_entity(e.at(0).get<uint32_t>(),e.at(1).get<std::string>());++n;}
 for(const auto& x:p.at("nodes")){browser_stage_seed_node(x.at(0).get<uint32_t>(),x.at(1).get<int>(),x.at(2).get<uint32_t>());++n;}
 return n;
}
// Capture seeds: the octree holds the locations whose rows are resident (1/2) or unload-wanted (5) in the first record.
EMSCRIPTEN_KEEPALIVE void peak_world_sync_octree(){using namespace peak_stream;for(const auto& r:rows)if(r.track>=0&&r.kind==0)ssx::worldTrackOctree()[size_t(r.track)]=r.state==1||r.state==2||r.state==5;}
EMSCRIPTEN_KEEPALIVE void peak_world_drop_paths(){peak_stream::drop_paths();}
// pv peakRelease (web/free-ride.js, one rider context): the host frees an evicted location's collision data (its terrain patches from
// both terrain systems, its instances' collision nodes, its grind rails: web/world_bridge.cpp browser_world_release_track, web/rail_bridge.cpp
// browser_rails_release_track) when it drops the location (the PS2 returns the Section Allocator's blocks at T+8, 3A8230); its next want
// feeds it again (web/peak-world.js releaseCore), into the same instance / rail slots. Refused (-1) while the track is collidable or in
// the section octree. Returns the items freed.
EMSCRIPTEN_KEEPALIVE int peak_world_free_track(int track){
 if(track<0||track>255||ssx::worldTrackResidency()[size_t(track)]||ssx::worldTrackOctree()[size_t(track)])return -1;
 return int(browser_world_release_track(uint32_t(track))+browser_rails_release_track(uint32_t(track)));
}
EMSCRIPTEN_KEEPALIVE void peak_world_set_course(int course){peak_stream::currentCourse=course;peak_stream::update_finish_rule();}
// 22D390: every row of the current course (+0x1BC) resident (22D278: 22E078 = state 1 or 2, also its sky and TRANSP) and
// +0x1C0 set (the Load trigger ran). The HUD draws the collectible counter only then (21F9B0).
EMSCRIPTEN_KEEPALIVE int peak_world_course_ready(){
 using namespace peak_stream;if(!active||!loadedFlag)return 0;auto it=residency.find(pendingCourse);if(it==residency.end())return 0;
 auto ok=[](int32_t id){return id<0||rows[size_t(id)].state==1||rows[size_t(id)].state==2;};
 for(int32_t id:it->second.locations)if(!ok(id))return 0;return ok(it->second.sky)&&ok(it->second.transp)?1:0;
}
// QA (screenshots at a capture spot): 22CEA8 (activate 0) / 22D088 (1) for a course, as its trigger would.
EMSCRIPTEN_KEEPALIVE void peak_world_request(int course,int activate){peak_stream::request(course,activate!=0);}
EMSCRIPTEN_KEEPALIVE int peak_world_sky(){return peak_stream::skyActive;}
// Transport (world state 14 arg 1, docs/peak-mountain.md "Transport"): phase 0 = 22CEA8(streamer, dest, 7) (the crossing starts,
// screen 11 with the transport-arrival flag +0x1C8); phase 1 = 22D088(streamer, dest, 7) once every wanted row is resident
// (236960 -> 22D278), which activates them in the same pass. The arrival shows the destination's sky: the dome switch is allowed
// as after the initial load (+0x1A4 = 1; the skybox trigger volumes are not ridden through on a transport).
// pv transportFade (web/free-ride.js): phase 2 = the Load at the release (0x236AC8: 22D088 without the dome), phase 3 = the dome allowed
// once the list's head is no longer a Transport step (world state 10 update 0x235808..0x235838: +0x1A4 = 1, R+30).
EMSCRIPTEN_KEEPALIVE void peak_world_transport(int course,int phase){if(phase==3){peak_stream::skySwitchAllowed=true;return;}peak_stream::request(course,phase!=0);if(phase==1)peak_stream::skySwitchAllowed=true;}
// 22D278: every wanted row of the request (locations, sky, TRANSP) resident (state 1 or 2).
EMSCRIPTEN_KEEPALIVE int peak_world_request_resident(){
 using namespace peak_stream;auto it=residency.find(pendingCourse);if(it==residency.end())return 0;
 auto ok=[](int32_t id){return id<0||rows[size_t(id)].state==1||rows[size_t(id)].state==2;};
 for(int32_t id:it->second.locations)if(!ok(id))return 0;return ok(it->second.sky)&&ok(it->second.transp)?1:0;
}
// Stage requests since the last call: [count, {kind, a, b, c}...] (1 builtin 68 course/action/mode, 3 builtin 67 entry/flag,
// 4 sky switched to location a, 5 current course 0x535C08 = a).
EMSCRIPTEN_KEEPALIVE int32_t* peak_world_events(){RIDER_LOCAL static std::vector<int32_t> out;out.assign(1,int32_t(peak_stream::events.size()/4));out.insert(out.end(),peak_stream::events.begin(),peak_stream::events.end());peak_stream::events.clear();return out.data();}
// 0x535C10 event kind seen by the stage builtins (43): 4 free ride, 5 time challenge, 6 points challenge; -1 restores the course's.
RIDER_LOCAL extern int browserEventKind;
EMSCRIPTEN_KEEPALIVE void peak_world_event_kind(int kind){browserEventKind=kind;peak_stream::eventKind=kind;}
EMSCRIPTEN_KEEPALIVE void peak_world_game_mode(int mode){peak_stream::gameMode=mode;}
EMSCRIPTEN_KEEPALIVE void peak_world_builtin108(int on){builtin108On=on!=0;} // stage builtin 108 (above); off = nil as before
// The career's first-FAQ flag (147580(P, 0)) before the world starts: 0 = not shown yet (a new career), 1 = shown / no career.
EMSCRIPTEN_KEEPALIVE void peak_world_faq_shown(int shown){peak_stream::faqShown=shown?1:0;}
// A location's paths.json (tools/export_peak_world.py paths_packages), delivered when its read completes.
EMSCRIPTEN_KEEPALIVE void peak_world_paths(int locationId,const char* text){peak_stream::pathBanks[locationId]=nlohmann::json::parse(text);}
// After the host placed the rider for a new run: 112180 re-attaches it to the current bank at its new position.
EMSCRIPTEN_KEEPALIVE void peak_world_reattach(){peak_after_race_reset();}
EMSCRIPTEN_KEEPALIVE int* peak_world_bank(){RIDER_LOCAL static int v[2];v[0]=peak_stream::bankLocation;v[1]=peak_stream::bankVariant;return v;}
EMSCRIPTEN_KEEPALIVE void peak_world_stop(){browserEventKind=-1;peak_stream::eventKind=4;peak_stream::tickRestartIn=0;peak_stream::crossingUnload=false;browserRaceAfterReset=nullptr;race_finish_eligible(1);peak_stream::active=false;peak_stream::rows={};ssx::worldTrackResidency().fill(1);browserWorldAppend=false;}
// pv eventInWorld (docs/ctm-events-in-world.md stages 2 / 3): a Conquer the Mountain event run inside the streamed world takes the event
// course's start seeds (web/generated/event_start_seed.hpp, keyed by browserEventLocation: the grid ground state, the human participant,
// the start delay, the progress origin, the start camera, the backcountry rolling start) and keeps the streamed world's own node, skip,
// scripted-instance and roller lists (the section activation and the streamed lists own those; the event package's are a Single Event
// countdown's). "" returns to the streamed world's (no event start). Returns 1 when the code is an event course.
EMSCRIPTEN_KEEPALIVE int event_course_seed(const char* code){
 const auto dead=browserEventDeadNodes;const auto skip=browserCourseSkipNodes;const auto scripted=browserEventScriptedInstances;const auto rollers=browserRollerTreeMass;const bool seeded=browserEventCourseSeeded;
 browser_select_event_course(code?std::string_view(code):std::string_view());
 const bool ok=browserEventCourseSeeded;
 browserEventDeadNodes=dead;browserCourseSkipNodes=skip;browserEventScriptedInstances=scripted;browserRollerTreeMass=rollers;browserEventCourseSeeded=seeded;
 return ok?1:0;}
}
