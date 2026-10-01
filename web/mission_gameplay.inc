// Big Challenges: the original WScript mission system (docs/peak-mountain.md "Big Challenges").
// Included at the end of web/stage_script_gameplay.inc (same translation unit as the stage VM, the builtins and the rider).
//
// Original (SLUS_207.72): stage-script context C = *(gp+0xCE8), fields ported here:
//   C+0      current player (the human, 0)
//   C+4..+0x3C  challenge HUD words, zeroed by every WScript tick (309270) and set again by the running mission's programs:
//            +4 goal flag / +8 / +0xC "Goal: %d" (builtin 98 -> 30B4C0), +0x10 count flag / +0x14 / +0x18 "Count: %d / %d"
//            (builtin 75 -> 30B4D8), +0x1C timer flag / +0x20 ticks / +0x24 (builtin 76 -> 30B4F0), +0x28 height flag /
//            +0x2C instance / +0x30 player (builtin 81 -> 30B508), +0x34 flag / +0x38..+0x3F (30B520).
//   C+0x40   event count, C+0x44.. 12-byte events {0x70000 | kind, word, word} (3097E0 kind 0 contact from 30A060, 309848 1,
//            3098B0 2, 309918 3 (8 data bytes: trick), 309990 4, 3099F8 5), dispatched and cleared by the next WScript tick.
//   C+0x1C4  offer queue: 16-entry ring {read, write, entries {id, type, player}} (30C7C0 post by 30A868 ops 8/9/11,
//            30C870 peek by the game update 2306A8 @230890, 30C820 pop by the prompt overlay's init 1F7548).
//   C+0x290 current instance, +0x29C event data, +0x2A0 active challenge, +0x2A4 current mission, +0x2A8 current task,
//   C+0x2AC pending op, +0x2B0 its challenge, +0x2B4 inactive / +0x2B8 active / +0x2BC pending mission lists (next +0x18).
// Missions: WScriptMission (vtable 0x489A30, 0x78 bytes: 307128 over the WScriptProcess base 307598), built per mission
// record of every loaded location stage by 308C60 (free ride, one human) and torn down by 308F38 / 308FE0; tasks are
// WScriptTask (vtable 0x489AE0, 0x44 bytes: 308228 / 308278). tools/export_peak_missions.py -> generated/peak_mission_seed.hpp.
// Challenge status: character block +0x118 + 4*index (table 0x43EE10 row): bit0 locked follow-on, bit1 new, bit2 failed,
// bit3 completed, bit4 available (151600 initial: bit0 = flags byte +0x1A, bit1 = 1, bit4 = flags byte +0x1B).
#if __has_include("generated/peak_mission_seed.hpp")
#include "generated/peak_mission_seed.hpp"
#define BROWSER_MISSION_SEED 1
#endif
static constexpr uint32_t kMissionNone=0xffffffffu;
struct MissionTask{int32_t state=0;uint32_t id=0;std::array<uint32_t,13> cb{};}; // +0 state, +4 id, +8 start, +0xC tick, +0x10 end, +0x14.. event kinds 0..9
struct MissionObject{
 bool alive=false;uint32_t id=0,track=0;int32_t state=0,count=0,index=0; // +0x20, +0x24 (stage), +0 (0 idle, 1 starting, 2 running, 3 stopped), +4 task count, +8 task index
 std::array<uint32_t,3> proc{};std::array<uint32_t,10> ev{};std::array<uint32_t,4> cb{};uint32_t misc=0; // +0x28..+0x30, +0x34..+0x58, +0x68..+0x74, +0x60
 std::vector<MissionTask> tasks;                                                          // +0x14
 bool hasTable=false;ssx::OriginalScriptValue table{};                                     // +0x1C
};
struct MissionEvent{uint32_t word0=0,a=0,b=0;};
struct MissionOfferEntry{uint32_t id=0;int32_t type=0,player=0;};
RIDER_LOCAL static std::vector<MissionObject> missionPool;          // object storage (index = handle)
RIDER_LOCAL static std::vector<int32_t> missionLists[3];            // 0 C+0x2B4 inactive, 1 C+0x2B8 active, 2 C+0x2BC pending; front first
RIDER_LOCAL static std::array<int32_t,16> missionHud{};             // C+0 .. C+0x3C (word index = offset / 4)
RIDER_LOCAL static std::vector<MissionEvent> missionEvents;         // C+0x40 / +0x44 (capacity 0x180 / 12 = 32)
RIDER_LOCAL static std::array<MissionOfferEntry,16> missionOffers{};RIDER_LOCAL static int32_t missionOfferRead=0,missionOfferWrite=0; // C+0x1C4
RIDER_LOCAL static int32_t missionActive=-1;                        // C+0x2A0 (challenge id as int; -1 none)
RIDER_LOCAL static int32_t missionCurrent=-1,missionCurrentTask=-1; // C+0x2A4 (mission handle), C+0x2A8 (task: mission handle << 8 | task)
RIDER_LOCAL static int32_t missionPendingOp=0,missionPendingId=-1;  // C+0x2AC / C+0x2B0
RIDER_LOCAL static const uint8_t* missionEventData=nullptr;         // C+0x29C (kind 3 events)
RIDER_LOCAL static std::array<uint32_t,88> missionStatus{};         // character block +0x118 + 4*index
RIDER_LOCAL static bool missionStatusSet=false,missionStatusDirty=false;
RIDER_LOCAL static std::vector<int32_t> missionUiEvents;            // {kind, a, b}: see mission_ui_events()
RIDER_LOCAL static int32_t missionRebuildIn=-1;              // ticks until the 308C60 rebuild (-1 none)
RIDER_LOCAL static uint32_t missionTicks=0,missionProgramsRun=0,missionUnsupported=0;
RIDER_LOCAL static bool missionSpeedBuiltin=false; // builtin 59 (3032C0) answered; web/big-challenges.js sets it from pv bcSpeed
RIDER_LOCAL static int32_t missionOfferGate=0;                      // *(*(W+0x28)): 30A868 posts offers only while 0 or >= 10
// pv bcDecline (web/big-challenges.js -> mission_lifecycle): bit 0 the 1235F8 reset of a declined / quit challenge, bit 1 builtin 67's
// 30B7F8 (an event gate ends a running challenge), bit 2 world state 10 enter's 309030 -> 308988 (the context reset at a Load trigger).
RIDER_LOCAL static int32_t missionLifecycle=0;RIDER_LOCAL static int32_t missionContextResetIn=-1;
static int32_t mission_find_in(int list,uint32_t id);

// ---- challenge table 0x43EE10 and the status word (153C88 .. 1541D0) ----------------------------------------------------------
static int mission_challenge_index(uint32_t id){ // 154240
#ifdef BROWSER_MISSION_SEED
 for(size_t i=0;i<browser_missions_peak1::challenges.size();i++)if(browser_missions_peak1::challenges[i].id==id)return int(i);
#endif
 return -1;
}
static uint32_t mission_initial_status(int index){ // 151600
#ifdef BROWSER_MISSION_SEED
 const uint32_t flags=browser_missions_peak1::challenges[size_t(index)].flags;
 return ((flags>>16)&1u)|2u|(((flags>>24)&1u)<<4);
#else
 (void)index;return 2u;
#endif
}
static void mission_status_defaults(){if(missionStatusSet)return;for(int i=0;i<88;i++)missionStatus[size_t(i)]=mission_initial_status(i);missionStatusSet=true;}
static bool mission_bit(uint32_t id,int bit){mission_status_defaults();const int i=mission_challenge_index(id);return i>=0&&((missionStatus[size_t(i)]>>bit)&1u);} // 153E28/153DD0/153E80/153D78/153ED8 (index -1 reads outside the table: unreachable for authored ids)
static void mission_set_bit(uint32_t id,int bit,bool on){ // 154010/154080/1540F0/154160/1541D0
 mission_status_defaults();const int i=mission_challenge_index(id);if(i<0)return;auto& w=missionStatus[size_t(i)];const uint32_t v=(w&~(1u<<bit))|(uint32_t(on)<<bit);
 if(v!=w){w=v;missionStatusDirty=true;missionUiEvents.insert(missionUiEvents.end(),{5,i,int32_t(v)});}
}
static uint32_t mission_next(uint32_t id){ // 153FD8
#ifdef BROWSER_MISSION_SEED
 const int i=mission_challenge_index(id);if(i>=0)return browser_missions_peak1::challenges[size_t(i)].next;
#endif
 (void)id;return kMissionNone;
}
static int32_t mission_level(uint32_t id){ // 153FA0: (s16) flags (the challenge's peak)
#ifdef BROWSER_MISSION_SEED
 const int i=mission_challenge_index(id);if(i>=0)return int16_t(browser_missions_peak1::challenges[size_t(i)].flags&0xffffu);
#endif
 (void)id;return 1;
}
static int32_t mission_cash(int32_t level){return level==2?4000:level==3?6000:2000;} // 1511B0

// ---- lists (30B260 push front, 30B2D0 remove, 30B208 pop, 30B320 find by id, 30B428 contains) ---------------------------------
static MissionObject* mission_obj(int32_t h){return h>=0&&size_t(h)<missionPool.size()&&missionPool[size_t(h)].alive?&missionPool[size_t(h)]:nullptr;}
static void mission_push(int list,int32_t h){missionLists[list].insert(missionLists[list].begin(),h);}
static bool mission_remove(int list,int32_t h){auto& l=missionLists[list];auto it=std::find(l.begin(),l.end(),h);if(it==l.end())return false;l.erase(it);return true;}
static bool mission_contains(int list,int32_t h){const auto& l=missionLists[list];return std::find(l.begin(),l.end(),h)!=l.end();}
static int32_t mission_find_in(int list,uint32_t id){for(int32_t h:missionLists[list])if(auto* m=mission_obj(h);m&&m->id==id)return h;return -1;} // 30B320
static int32_t mission_find(int list,uint32_t id){return id==kMissionNone?-1:mission_find_in(list,id);} // 30B388 (vt+0x18 30D480: every mission is one)

// ---- running programs (30AC98 mission / 30ADA8 task: globals = the mission's table, or a temporary 8-bucket one) ------------------
static ssx::OriginalScriptValue stage_builtin_value(int,const ssx::OriginalScriptArg*,int,bool);
static void mission_run_ref(uint32_t ref,int32_t h,int32_t task){
 if(ref==kMissionNone||!stageVm||!stageTables)return;
 const auto* stage=stage_of(ref);if(!stage)return;
 const uint32_t global=stage->firstProgram+(ref>>8);if(global>=stageProgramHandles.size())throw std::runtime_error("Mission callback outside the stage programs");
 auto* m=mission_obj(h);
 const int32_t save2A4=missionCurrent,save2A8=missionCurrentTask;const uint32_t save290=stage_current_instance;
 if(task>=0)missionCurrentTask=(h<<8)|task;else missionCurrent=h;
 ssx::OriginalScriptValue globals;bool temporary=false;
 if(m&&m->hasTable)globals=m->table;else{globals=ssx::OriginalScriptVM::tableValue(stageVm->newTable(8));temporary=true;} // 317D70 + 224DA0(8)
 struct Restore{int32_t a,b;uint32_t c;~Restore(){missionCurrent=a;missionCurrentTask=b;stage_current_instance=c;}} restore{save2A4,save2A8,save290};
 ++missionProgramsRun;const int prevSlot=stage_current_slot;stage_current_slot=-1;
 auto result=stageVm->run(stageProgramHandles[global],0,globals,[](int index,const ssx::OriginalScriptArg* args,int count){return stage_builtin_value(index,args,count,false);});
 stage_current_slot=prevSlot;stageVm->release(result);if(temporary)stageVm->release(globals); // 224DF0(table, 3) / 225B90(ret, 2)
}
static void mission_run(int32_t h,uint32_t ref){mission_run_ref(ref,h,-1);}          // 30AC98
static void mission_run_task(int32_t h,int32_t t,uint32_t ref){mission_run_ref(ref,h,t);} // 30ADA8

// ---- tasks (WScriptTask vtable 0x489AE0) ------------------------------------------------------------------------------------------
static void mission_task_start(int32_t h,int32_t t){auto& k=mission_obj(h)->tasks[size_t(t)];if(k.state==1)return;k.state=1;mission_run_task(h,t,k.cb[0]);} // 3086B0
static void mission_task_tick(int32_t h,int32_t t){ // 308328 -> vt+0x28 3086F0
 auto& k=mission_obj(h)->tasks[size_t(t)];if(k.state==1)k.state=2;else if(k.state!=2)return;mission_run_task(h,t,k.cb[1]);
}
static void mission_task_end(int32_t h,int32_t t){auto& k=mission_obj(h)->tasks[size_t(t)];if(k.state==3)return;k.state=3;mission_run_task(h,t,k.cb[2]);} // 308720
static void mission_task_event(int32_t h,int32_t t,const MissionEvent& e){ // 3083A0: kinds 0..9 -> callbacks +0x14..+0x38
 auto& k=mission_obj(h)->tasks[size_t(t)];if(k.state!=2)return;const uint32_t kind=e.word0&0xffffu;if(kind>=10)return;
 if(kind==3)missionEventData=reinterpret_cast<const uint8_t*>(&e.a);
 mission_run_task(h,t,k.cb[3+kind]);missionEventData=nullptr;
}

// ---- missions (WScriptProcess 0x489B70 / WScriptMission 0x489A30) -------------------------------------------------------------
static void mission_release_table(MissionObject& m){if(m.hasTable){stageVm->release(m.table);m.hasTable=false;m.table={};}}
static void mission_to_inactive(int32_t h){if(mission_remove(1,h))mission_push(0,h);} // 309AA0
static void mission_quiet_stop(int32_t h){auto* m=mission_obj(h);if(!m)return;m->state=3;mission_to_inactive(h);mission_release_table(*m);} // 307D78
static void mission_stop(int32_t h){auto* m=mission_obj(h);m->state=3;mission_run(h,m->proc[2]);mission_quiet_stop(h);}  // vt+0x30 307D30
static void mission_process_start(int32_t h){ // vt+0x20 base 307C40
 auto* m=mission_obj(h);mission_release_table(*m);m->table=ssx::OriginalScriptVM::tableValue(stageVm->newTable(8));m->hasTable=true;
 m->state=1;m->index=0;mission_run(h,m->proc[0]);
 mission_remove(0,h);mission_push(1,h); // 309A60: 30B2D0 from the inactive list, 30B260 onto the active one
 m=mission_obj(h);if(m&&m->index<m->count)mission_task_start(h,m->index);
}
static void mission_rider_challenge_start(); // 10F2B8 -> 11A0E0: popup 0x1A
static void mission_start(int32_t h){mission_rider_challenge_start();mission_process_start(h);} // vt+0x20 3071F0
static void mission_restart(int32_t h){auto* m=mission_obj(h);if(m->state==2)mission_stop(h);mission_start(h);} // vt+0x88 3072B0
static void mission_complete(int32_t h); // vt+0x90 307308
static void mission_success(int32_t h){ // vt+0x10 307240
 auto* m=mission_obj(h);if(mission_find_in(1,m->id)<0)return; // 30B898
 mission_stop(h);mission_complete(h);missionUiEvents.insert(missionUiEvents.end(),{3,int32_t(m->id),0}); // 29D8E0 (music event 39)
}
static void mission_next_task(int32_t h){ // op 2 / builtin 47: 307E10
 auto* m=mission_obj(h);if(!m||m->state==3)return;
 mission_task_end(h,m->index);m=mission_obj(h);
 if(m->index+1<m->count){++m->index;mission_task_start(h,m->index);}else mission_success(h);
}
static void mission_fail(int32_t h){ // vt+0x98 3074C0
 auto* m=mission_obj(h);if(!mission_bit(m->id,2))mission_set_bit(m->id,2,true);
 if(m->cb[2]!=kMissionNone)mission_run(h,m->cb[2]);else mission_run(h,m->cb[3]);
 m=mission_obj(h);if(m&&m->state==2)mission_quiet_stop(h);
}
static void mission_visual(int32_t h){auto* m=mission_obj(h);mission_run(h,m->cb[3]);} // vt+0xA0 307568
static void mission_refresh_visuals(); // 308DB8
static void mission_stats_award(int32_t cash){ // 10F2D8 -> 11A110: popup 0x1B + 119EF8(score, 4, cash)
 push_score();originalHudPostPoints(scoreObject,0x1B,cash,0,1.5f);originalScoreCareerAward(scoreObject,4,cash,score_environment(),audioScoreHooks);pull_score();
 missionUiEvents.insert(missionUiEvents.end(),{6,cash,0});
}
static void mission_to_inactive_clear(int32_t h){mission_to_inactive(h);missionActive=-1;} // 309B00
static void mission_complete(int32_t h){ // 307308
 auto* m=mission_obj(h);const uint32_t id=m->id;int32_t cash=0;
 if(!mission_bit(id,3)){mission_set_bit(id,3,true);cash=mission_cash(mission_level(id));}
 mission_stats_award(cash);missionUiEvents.insert(missionUiEvents.end(),{2,int32_t(id),cash});
 const uint32_t next=mission_next(id);
 if(next!=kMissionNone){
  mission_set_bit(id,4,false);if(m->cb[1]!=kMissionNone)mission_run(h,m->cb[1]);
  if(mission_bit(next,0)){mission_set_bit(next,0,false);mission_set_bit(next,1,true);mission_set_bit(next,4,true);}
 }else if(m->cb[1]!=kMissionNone)mission_run(h,m->cb[1]);
 mission_refresh_visuals();mission_to_inactive_clear(h);
}

// ---- 30A728 / 30A868: the challenge interface of builtin 40 and the host ---------------------------------------------------------
static bool mission_mode(){return browserEventKind==4&&browser_streamed_world(setPieceLocationName);} // 0x535C10 == 4 and one human (W+0xC +0x7C)
static bool mission_running_challenge(){ // 30B8C0: an active mission with misc (+0x60) 2
 for(int32_t h:missionLists[1])if(auto* m=mission_obj(h);m&&m->misc==2)return true;return false;
}
static int32_t mission_current(){return mission_obj(missionCurrent)?missionCurrent:-1;} // 30AEB8
static void mission_op_list(uint32_t id,int op){ // 30A728 (id -1: the current mission C+0x2A4)
 const int32_t cur=missionCurrent;
 switch(op){
  case 0:{const int32_t h=id==kMissionNone?cur:mission_find_in(1,id);if(mission_obj(h)){const int32_t s=missionCurrent;missionCurrent=h;mission_stop(h);missionCurrent=s;}break;} // 309798: vt+0x30
  case 1:{const int32_t h=id==kMissionNone?cur:mission_find_in(0,id);if(mission_obj(h)){const int32_t s=missionCurrent;missionCurrent=h;mission_start(h);missionCurrent=s;}break;} // 309750: vt+0x20
  case 2:{const int32_t h=id==kMissionNone?cur:mission_find_in(1,id);if(mission_obj(h))mission_next_task(h);break;} // 307E10
  case 18:{const int32_t h=id==kMissionNone?cur:mission_find_in(1,id);if(mission_obj(h))mission_quiet_stop(h);break;} // 307D78
  default:break;
 }
}
static void mission_offer_post(uint32_t id,int32_t type,int32_t player){ // 30C7C0: 16-entry ring, no overflow check
 missionOffers[size_t(missionOfferWrite)]={id,type,player};missionOfferWrite=(missionOfferWrite+1)&15;
}
static int32_t mission_op(uint32_t id,int op,int32_t arg){ // 30A868
 const bool query=op==15||op==19;
 if(!query&&!mission_mode())return 0;
 (void)arg; // arg != -1 sets C+0 (the current player) for the call: one human, always player 0
 if(op<19){
  if(op==0||op==1||op==2||op==18)mission_op_list(id,op);
  else if(op==8||op==9||op==11){if(missionOfferGate==0||missionOfferGate>=10)mission_offer_post(id,op,0);}
 }
 int32_t result=0;const int32_t cur=mission_current();
 auto idOr=[&](uint32_t v){return v==kMissionNone&&mission_obj(cur)?mission_obj(cur)->id:v;};
 switch(op){
  case 3:{int32_t h=mission_find(1,id);if(h<0)h=mission_find(0,id);if(id==kMissionNone)h=cur;if(!mission_obj(h))break; // vt+0x88 restart + 29D6E0 music
   const int32_t s=missionCurrent;missionCurrent=h;mission_restart(h);missionCurrent=s;missionUiEvents.insert(missionUiEvents.end(),{1,int32_t(id),1});break;}
  case 4:{const int32_t h=id==kMissionNone?cur:mission_find(1,id);if(mission_obj(h))mission_success(h);break;} // vt+0x10
  case 5:{const int32_t h=id==kMissionNone?cur:mission_find(1,id);if(!mission_obj(h))break; // vt+0x98 fail, 29DBB0(audio, 0)
   const int32_t s=missionCurrent;missionCurrent=h;mission_fail(h);missionCurrent=s;missionUiEvents.insert(missionUiEvents.end(),{4,int32_t(mission_obj(h)?mission_obj(h)->id:id),0});break;}
  case 17:{int32_t h=mission_find(1,id);if(h<0)h=mission_find(0,id);if(id==kMissionNone)h=cur;if(mission_obj(h))mission_visual(h);break;} // vt+0xA0
  case 13:{const uint32_t v=idOr(id);if(v!=kMissionNone)result=mission_bit(v,0);break;}
  case 12:{const uint32_t v=idOr(id);if(v!=kMissionNone)result=mission_bit(v,1);break;}
  case 14:{const uint32_t v=idOr(id);if(v!=kMissionNone)result=mission_bit(v,2);break;}
  case 15:{const uint32_t v=idOr(id);if(v!=kMissionNone)result=mission_bit(v,3);break;}
  case 16:{const uint32_t v=idOr(id);if(v==kMissionNone||missionPendingOp==1||mission_running_challenge())break;result=mission_bit(v,4);break;}
  case 19:{const uint32_t v=idOr(id);if(v!=kMissionNone)result=mission_next(v)==kMissionNone;break;}
  default:break;
 }
 return result;
}

// ---- pending ops (309118) and the prompt / pause hooks (30B540 .. 30B7F8) --------------------------------------------------------
static void mission_pending(){ // 309118
 const int op=missionPendingOp;const uint32_t id=uint32_t(missionPendingId);
 switch(op){
  case 1:mission_refresh_visuals();mission_op(id,1,0);missionUiEvents.insert(missionUiEvents.end(),{1,missionActive,0});break; // 308DB8, start, 29D6E0(audio, C+0x2A0), 26F7B8
  case 2:mission_op(id,3,0);break;
  case 3:mission_op(id,18,0);mission_refresh_visuals();missionUiEvents.insert(missionUiEvents.end(),{4,int32_t(id),1});break; // 29DBB0(audio, 1)
  case 4:mission_op(id,5,0);mission_refresh_visuals();missionUiEvents.insert(missionUiEvents.end(),{4,int32_t(id),1});break;
  case 5:mission_op(id,1,1);mission_refresh_visuals();break; // 26CC48 (restore the saved restart point: not modelled)
  case 6:mission_op(id,18,0);mission_refresh_visuals();break;
  default:return;
 }
 missionPendingOp=0;missionPendingId=-1;
}
static void mission_rider_decline(); // 1235F8
static void mission_accept_op(){missionPendingOp=1;missionPendingId=missionActive;}   // 30B540 (26CDF8 / 26F228: restart point, stats)
static void mission_restart_op(){missionPendingOp=2;missionPendingId=missionActive;}  // 30B6F8
static void mission_decline_op(int op){mission_rider_decline();missionPendingId=missionActive;missionActive=-1;missionPendingOp=op;} // 30B658 (3) / 30B758 (4)
static void mission_reset_all(){ // 30B7F8
 if(missionActive!=-1){const int32_t h=mission_find_in(1,uint32_t(missionActive));if(h>=0)mission_quiet_stop(h);}
 missionActive=-1;missionPendingOp=0;missionPendingId=-1;
 while(!missionLists[1].empty()){const int32_t h=missionLists[1].front();missionLists[1].erase(missionLists[1].begin());if(auto* m=mission_obj(h))m->state=0;mission_push(0,h);}
 missionOfferRead=missionOfferWrite=0;missionOffers={}; // 30C790
}

// ---- building (308C60 / 308F38 / 308FE0) and the visual refresh (308DB8) --------------------------------------------------------
static bool mission_track_live(uint32_t track){return track<ssx::worldTrackOctree().size()&&ssx::worldTrackOctree()[track];}
static void mission_clear(){ // 308F38: every mission destroyed (dtor 3071C8 -> 307738 releases the table)
 for(auto& m:missionPool)if(m.alive&&stageVm)mission_release_table(m);
 missionPool.clear();for(auto& l:missionLists)l.clear();missionCurrent=missionCurrentTask=-1;
}
static void mission_refresh_visuals(){ // 308DB8: each loaded stage's mission records except the active challenge: callback +0x4C, temporary table
#ifdef BROWSER_MISSION_SEED
 if(!stageVm||!stageTables)return;
 for(const auto& st:stageTables->stages){if(!mission_track_live(st.track))continue;
  for(const auto& r:browser_missions_peak1::missions)if(r.track==st.track&&int32_t(r.id)!=missionActive)mission_run_ref(r.c[3],-1,-1);}
#endif
 missionCurrentTask=-1;stage_current_instance=0;missionCurrent=-1;
}
static void mission_build_track(uint32_t track){ // 308C60 for one loaded stage: 309568 per mission record (front of the inactive list)
#ifdef BROWSER_MISSION_SEED
 for(const auto& r:browser_missions_peak1::missions){if(r.track!=track)continue;
  MissionObject m;m.alive=true;m.id=r.id;m.track=r.track;m.count=int32_t(r.stepCount);m.index=0;m.state=0;m.proc=r.a;m.ev=r.b;m.cb=r.c;m.misc=r.misc; // 307598 / 307128
  for(uint32_t k=0;k<r.stepCount;k++){const auto& s=browser_missions_peak1::steps[r.firstStep+k];MissionTask t;t.id=s.id;t.cb=s.slots;m.tasks.push_back(t);}
  missionPool.push_back(std::move(m));mission_push(0,int32_t(missionPool.size()-1)); // 30B2C0
 }
#else
 (void)track;
#endif
}
static bool mission_track_built(uint32_t track){for(const auto& m:missionPool)if(m.alive&&m.track==track)return true;return false;}
static bool mission_track_has_records(uint32_t track){
#ifdef BROWSER_MISSION_SEED
 for(const auto& r:browser_missions_peak1::missions)if(r.track==track)return true;
#endif
 (void)track;return false;
}
static void mission_build(){ // 308F38 + 308C60 (+ 308DB8): every loaded stage's missions from scratch
 mission_clear();
 if(!mission_mode()||!stageTables)return;
 for(const auto& st:stageTables->stages)if(mission_track_live(st.track))mission_build_track(st.track);
 mission_refresh_visuals();
}
void browser_mission_track_teardown(uint32_t track){ // 230360 -> 308FE0 -> 308F70: the unloading stage's missions leave every list
 for(size_t h=0;h<missionPool.size();h++){auto& m=missionPool[h];if(!m.alive||m.track!=track)continue;
  for(auto& l:missionLists)mission_remove(int(&l-missionLists),int32_t(h));if(stageVm)mission_release_table(m);m.alive=false;}
}
// World start / stage VM reset (web/stage_script_gameplay.inc stage_vm_reset): no missions until the first WScript tick builds them.
void browser_mission_reset(){missionPool.clear();for(auto& l:missionLists)l.clear();missionHud={};missionHud[3]=-1;missionHud[9]=-1;missionHud[11]=-1;missionEvents.clear();missionOffers={};missionOfferRead=missionOfferWrite=0;
 missionActive=-1;missionCurrent=missionCurrentTask=-1;missionPendingOp=0;missionPendingId=-1;missionRebuildIn=-1;missionContextResetIn=-1;missionTicks=0;missionUiEvents.clear();}

// 230180's 308C60 (0x23027C), after its bucket-1 purge 355118(S, 1) (0x2301B8) destroyed the Hide nodes (350E90 -> 34FB00 bucket 1,
// type 16): 308C60 builds the missions only in mode 4 (0x535C10 == 4: the port's first WScript tick, missionTicks 0) and ends with
// 308DB8 (0x308D84) whatever the mode: each loaded stage's mission records' +0x4C programs run again, the Big Challenge markers'
// Hide (2FFB50) among them. PS2 c0a-ws13: mdl_ARA1_BigCGate_1001 / flg_ARA1_BigCFlag_1001 are type-16 nodes (+8 0x2102x5) at the
// semi's tick 0. In mode 4 the next tick's mission_build runs 308DB8 (one run, as the PS2's).
void browser_mission_world_reset(){if(!mission_mode())mission_refresh_visuals();}

// ---- events (3097E0 .. 3099F8) and the WScript tick (309270) ------------------------------------------------------------------
static void mission_post_event(uint32_t kind,uint32_t a=0,uint32_t b=0){if(missionEvents.size()<32)missionEvents.push_back({0x70000u|kind,a,b});}
// 30A060 -> 3097E0: every rider contact that reaches 30A060 posts kind 0 with the instance's resource (+0x78).
void browser_mission_contact(uint32_t resource){if(mission_mode())mission_post_event(0,resource);}
// 118FF8 -> 309918: a trick the human's score object names (the HUD text of 11A228 / 11A168) posts kind 3 with the trick's
// eight-byte record (11A8C8), which builtin 78 (30AF08) reads field by field while the event is dispatched (C+0x29C).
void browser_mission_trick(const std::array<uint32_t,2>& record){if(mission_mode())mission_post_event(3,record[0],record[1]);}
// 11DF18(rider, 1) -> 3099F8: the reset controller's placement (12F398 @0x12F498) posts kind 5 without data (the Peak 3 Combat
// and New Line challenges listen for it). 20D1D8 / 235FD0 pass their own flag word to 11DF18 (not posted here).
void browser_mission_placed(){if(mission_mode())mission_post_event(5);}
// builtin 68 action 2 (22D088, the Load trigger) for the human: state 10, whose exit rebuilds the missions two ticks later.
void browser_mission_load_trigger(){if(mission_mode()){missionRebuildIn=1;if(missionLifecycle&4)missionContextResetIn=1;}}
// World state 10 enter (0x2356A8 -> 309030, the tick after the Load trigger): 30B1B8 destroys the three mission lists, 308988 resets
// the context (C+0 .. +0x3C HUD words with +0xC / +0x24 / +0x2C = -1, +0x40 events, the +0x1C4 offer ring 30C760, +0x2A0 active and
// +0x2B0 pending id -1, +0x2AC pending op 0), then 308BE0 builds the stages' plain WScript processes (3094C8, not modelled: none of
// the port's). The missions come back at the exit (235868: 308F38 + 308C60, missionRebuildIn). Before, a challenge running across a
// Load trigger lost its missions at the exit but kept C+0x2A0, its HUD words and any queued offer.
static void mission_context_reset(){ // 309030 (missions only) + 308988
 mission_clear();missionHud={};missionHud[3]=-1;missionHud[9]=-1;missionHud[11]=-1;missionEvents.clear();missionOffers={};missionOfferRead=missionOfferWrite=0;
 missionActive=-1;missionPendingOp=0;missionPendingId=-1;
}
void browser_mission_gate(){if(mission_mode()&&(missionLifecycle&2))mission_reset_all();} // builtin 67 (0x3021B8): 30B7F8 before 22D6C8 for a gate whose 0x445E40 entry is not 1
void browser_mission_tick(){ // 309270 (game update 2306A8 @230CF0: after the entity passes, before CrowdMan / MultiParticleMan)
 if(!mission_mode()){if(!missionPool.empty())mission_clear();return;}
 ++missionTicks;
 // 308C60 runs at the world start and at every world state-10 exit (235868: two ticks after a Load trigger, builtin 68 action 2):
 // the missions of every loaded stage are rebuilt (a running challenge ends with them). A stage that finished loading since
 // (a Transport arrival, the first tick) gets its missions without touching the others.
 if(missionTicks==1)missionRebuildIn=0;
 if(missionContextResetIn>0)--missionContextResetIn;else if(missionContextResetIn==0){missionContextResetIn=-1;mission_context_reset();}
 if(missionRebuildIn>0)--missionRebuildIn;else if(missionRebuildIn==0){missionRebuildIn=-1;mission_build();}
 else if(stageTables){bool added=false;for(const auto& st:stageTables->stages)if(mission_track_live(st.track)&&mission_track_has_records(st.track)&&!mission_track_built(st.track)){mission_build_track(st.track);added=true;}
  if(added)mission_refresh_visuals();}
 mission_pending();
 missionHud[1]=missionHud[4]=missionHud[7]=missionHud[10]=missionHud[13]=0; // C+4, +0x10, +0x1C, +0x28, +0x34
 while(!missionLists[2].empty()){const int32_t h=missionLists[2].front();missionLists[2].erase(missionLists[2].begin());mission_push(0,h);const int32_t s=missionCurrent;missionCurrent=h;mission_start(h);missionCurrent=s;} // 30B208 / 30B260 / 309750
 const auto events=missionEvents;missionEvents.clear();
 for(const auto& e:events){
  if((e.word0&0xffffu)==0)stage_current_instance=stage_instance(e.a)||stageWorldMatrices.count(e.a)||set_piece_instance_at(e.a)?e.a:0u; // C+0x290 = instance of the resource
  const auto active=missionLists[1];
  for(size_t i=0;i<active.size();i++){const int32_t h=active[i];auto* m=mission_obj(h);if(!m||!mission_contains(1,h))continue;missionCurrent=h;
   if(e.word0&0x20000u){const uint32_t kind=e.word0&0xffffu;if(kind<10){if(kind==3)missionEventData=reinterpret_cast<const uint8_t*>(&e.a);mission_run(h,m->ev[kind]);missionEventData=nullptr;}} // 3078C0
   m=mission_obj(h);if(m&&(e.word0&0x10000u)&&m->index<m->count)mission_task_event(h,m->index,e); // 307A10
  }
  stage_current_instance=0;missionCurrent=-1;
 }
 const auto active=missionLists[1];
 for(int32_t h:active){auto* m=mission_obj(h);if(!m)continue;missionCurrent=h; // 3077F8
  if(m->state==1)m->state=2;else if(m->state!=2)continue;
  if(m->index<m->count)mission_task_tick(h,m->index);
  m=mission_obj(h);if(m&&m->state==2)mission_run(h,m->proc[1]); // vt+0x28 307D00
 }
 missionCurrent=-1;
}

// ---- rider side ------------------------------------------------------------------------------------------------------------------
static void mission_rider_challenge_start(){push_score();originalHudPostPoints(scoreObject,0x1A,0,0,1.f);pull_score();} // 10F2B8 -> 11A0E0
RIDER_LOCAL extern int browserCrashResetReason; // web/core.cpp: -2 = the 1235F8 entry at the next tick (animation_bridge.cpp begin_decline_reset)
static void mission_rider_decline(){if(missionLifecycle&1)browserCrashResetReason=-2;} // 1235F8(rider): called from the prompt between ticks; the reset control steps from the next tick
// 1234D0 (rider vt+0x60, builtin 64): the challenge start point. 270970 (stats), P = M row 3, D = M * (1, 0, 0, 0), 119368
// score reset, motion 3 + control 13, placement 11D660(P, D, semantic 5, clearance 0), owner +0x290 = 2, control 6 (the start
// grid controller: 12BE20 entry, released by 12C028 in the race phase), 125038 fade stop, 111890 FX reset, and for devices 0 / 1
// two camera cuts (11D660's tail and 1234D0's end: director vt+0x24 = 15CCF0, set-target on every node with the placed rider):
// the camera is behind the rider at the start (PS2 ctm-parity/runs/pc75 s130).
void browser_camera_set_targets(int count); // web/core.cpp
static void set_presented_frame(const OriginalCollisionFrame&); // web/animation_bridge.cpp: +0x170 / +0x180 from a board root frame
static void mission_rider_teleport(const std::array<float,16>& mat){
 if(!cameraTerrain||!browserBodies)return;
 const std::array<float,3> P{mat[12],mat[13],mat[14]};std::array<float,3> D{mat[0],mat[1],mat[2]};
 score_reset(true);
 if(browserResetActive)clear_reset();rail_reset_leave();detach_rail_for_crash();crash.active=false;browserCrashActive=false;
 clear_reset_contacts();reset_body_queries();
 auto value=originalResetPlacement(P,D,0.f,[](auto end,auto start,float preferred){return queryOriginalWorldSegment(*cameraTerrain,browserBodies.get(),end,start,0,preferred,true);});
 reset_trail();reset_snow(false);reset_impact_fx(false);reset_boost_fx();weather_rider_fx_reset();apply_reset_placement(value,physicsState.reverseStance);crash.actor.detached=false;painterPlacement=physicsState.position;painterPlacementTrail=browserTrailContact;painterPlacementPending=true; /*+0x460 = the placed position (web/animation_bridge.cpp update_trail)*/
 heldAirMode=passiveMode=false;prewind={};air={};landingAirExitBaked=false;browserSoftActive=browserSoftFrame=false;grab={};
 gs=physicsState;legWeight=1;poseContact=initialPoseContact;currentPivot={};completedMain=false;completedMainSemantic=-1;cachedCrashWorld.clear();idleSeconds=0;upperRequest358=0;
 // 1234D0 -> 11D660(rider, P, D, semantic 5, clearance 0) before 11FEC8(rider, 6): the placement plays 5 (RNORM_FWD_CYC) with one
 // full-rate step and caches the placed pose (11EB60/11EB98 -> rider+0x2C) and its board frame (+0x170 / +0x180), as the reset
 // placement and the stage teleport do (reset_place_at): the next contacts and 120378 read it, and a palette exists at once.
 {const bool stance=physicsState.reverseStance;graph.sequences.clear();graph.sampledLocal.reset();graph.requestedSemantics.fill(438);graph.nextRate=1;graph.defaultMirror=stance;graph.defaultRoot={};
  const auto sc5=originalSinCos((stance?-3.1415927410125732f:-0.f)*.5f);graph.defaultRoot.rotation={0.f*sc5[0],0.f*sc5[0],sc5[0],sc5[1]};
  if(!graph.enter(5,-1,~uint64_t(0),true))throw std::runtime_error("Missing original reset animation");{auto step=gs;step.timeScale=1;graph.advance(step,prewind.spin.current,prewind.flip.current);graph.completeSequences();}
  const auto local=originalAnimationLocalPose(graph.rig->bones,graph.rig->clips,originalAnimationLayers(graph.sequences));RiderRootPresentation rs;rs.turn=gs.turn.current;rs.brake=gs.brake.current;rs.extraLean=physicsState.extraLean.current;rs.roll=physicsState.presentationRoll.current;rs.liftCm=physicsState.presentationLift.current;rs.lateral=physicsState.lateral;rs.controlState=physicsState.controlState;
  AnimationTransform boardRoot;const auto bodyRoot=originalRiderRootPresentation({physicsState.position,physicsState.quaternion},local.at(22).position,graph.scale,rs,&boardRoot);
  cachedCrashWorld=originalAnimationWorldPose(graph.rig->bones,local,bodyRoot,graph.scale,{bodyRoot,boardRoot});clear_skin_matrices();
  set_presented_frame(originalRiderCollisionFrame(boardRoot));}
 // control 6: the start controller entry (12BE20, web/start_gameplay.inc start_event without the clock / route). 0x12357C writes
 // 2 into the start controller's phase (+0x77C +0x290) before 11FEC8(rider, 6): the MANUAL start - the rider waits in the start
 // pose (semantic 1) until the stick is pushed forward, then pushes off (PS2 ctm-parity/runs/pc75, dizzy: 0 MPH for 40 s with a
 // neutral pad; pc75-up: pushing Up starts the ride), not the race start that launches when the clock allows it.
 startControl={};startControl.phase=2;const auto entry=originalStartControlEnter(startControl,physicsState.reverseStance);startTilt=physicsState.presentationLift.current;
 graph.sequences.clear();graph.sampledLocal.reset();graph.requestedSemantics.fill(438);graph.defaultMirror=entry.mirror;graph.defaultRoot={};
 const auto sc=originalSinCos(entry.rootHalfAngle);graph.defaultRoot.rotation={0,0,sc[0],sc[1]};
 if(!graph.enter(entry.semantic,-1,~uint64_t(0),true))throw std::runtime_error("Missing original grid animation");
 graph.startPose=startControl.pose;gs=physicsState;physicsState.animationIndex=entry.semantic;physicsState.animationClass=graph.currentClass(2);
 previousGround=true;previousHeld=false;physicsState.controlState=gs.controlState=6;browserStarting=browserStartFrame=browserStartFrozen=true;
 ++resetPlacements;if(browserHumanRider)browser_camera_set_targets(2);
}
RIDER_LOCAL static std::array<uint32_t,64> missionRiderTriggers=[]{std::array<uint32_t,64> a;a.fill(0xffffffffu);return a;}();
// rider +0x5B8 at the WScript tick: the previous rider pass's contacts (120F20 clears the list at the start of the rider pass, after
// 309270; the browser clears it in the entity pass, so browser_stage_entities_tick copies it first).
void browser_mission_rider_snapshot(){missionRiderTriggers=browserRiderTriggers;}
static bool mission_rider_in(uint32_t resource){for(uint32_t r:missionRiderTriggers){if(r==0xffffffffu)break;if(r==resource)return true;}return false;} // 108C28 (rider +0x5B8, 64 entries)

// ---- builtins ----------------------------------------------------------------------------------------------------------------
static StageArg mission_instance_matrix_arg(uint32_t resource,std::array<float,16>& m){ // instance +0x10 (or the entity's vt+0xC0 matrix)
 const auto e=stage_entity_matrix(resource);for(unsigned i=0;i<16;i++)m[i]=e[i/4][i%4];return {};
}
static uint32_t mission_resolve(uint32_t key){return key==0xffffffffu?stage_current_instance:(stage_instance(key)||stageWorldMatrices.count(key)||set_piece_instance_at(key)?key:0u);} // resource -> instance (C+0x290 for -1)
static int32_t mission_score_word(unsigned offset){push_score();const int32_t v=scoreObject.i(offset);pull_score();return v;}
static int32_t mission_rider_stat(int32_t n){ // 122EE8 (jump table 0x457E30)
 const int mm=npc_motion_mode(),cs=browser_control_state();
 switch(n){
  case 0:return mm==2;                         // 11FE98 == 2
  case 1:return cs==9;                         // 11FEE8 == 9 (reset)
  case 2:return mm==1||(mm==2&&crash.control.phase==1); // motion 1, or 2 with owner +0x30 == 1 (not modelled beyond the phase)
  case 3:return mm==0;
  case 4:return mm==4;                         // rail
  case 8:return mission_score_word(0x198);
  case 9:return board_press_style()==1;        // rider +0x330
  case 10:return board_press_style()==2;
  case 11:return boostState.amount!=0.f;       // +0x2FC != 0
  case 12:case 13:case 14:return 0;
  case 15:return mission_score_word(0x138);
  case 16:return mission_score_word(0x13C);
  case 17:return mission_score_word(0x19C);
  default:++missionUnsupported;return 0;       // 5..7 air-control queries (135EB0/135EF0/135F30), 18 animation query (311AE8)
 }
}
// 30AF08: the trick record's fields (jump table 0x4899D0); 12 has no field (0).
static int32_t mission_trick_field(const uint8_t* data,uint32_t field){
 if(!data||field>=16)return 0;uint32_t w0,w1;std::memcpy(&w0,data,4);std::memcpy(&w1,data+4,4);
 switch(field){
  case 0:return int32_t(w0&7u);case 1:return int32_t((w0>>3)&15u);case 2:return int32_t((w0>>7)&7u);case 3:return int32_t((w0>>10)&3u);
  case 4:return int32_t((w0>>12)&15u);case 5:return int32_t((w0>>16)&7u);case 6:return int32_t((w0>>19)&7u);case 7:return int32_t((w0>>22)&63u);
  case 8:return int32_t(w0>>28);case 9:return int32_t(w1&7u);case 10:return int32_t((w1>>3)&127u);case 11:return int32_t((w1>>10)&1u);
  case 13:return int32_t((w1>>18)&3u);case 14:return int32_t((w1>>24)&7u);case 15:return int32_t((w1>>20)&15u);default:return 0;
 }
}
// 3045B8: fields 0..15 -> the 64-bit record (field 12 at bits 43..49, which 30AF08 never reads back).
static std::array<uint32_t,2> mission_trick_pack(const std::array<uint32_t,16>& f){
 static constexpr std::array<std::pair<int,uint64_t>,16> kField{{{0,7},{3,15},{7,7},{10,3},{12,15},{16,7},{19,7},{22,63},{28,15},{32,7},{35,127},{42,1},{43,127},{50,3},{56,7},{52,15}}};
 uint64_t r=0;for(size_t i=0;i<16;i++){const auto [shift,mask]=kField[i];r=(r&~(mask<<shift))|((uint64_t(f[i])&mask)<<shift);}
 r&=0x07FFFFFFFFFFFFFFull;return {uint32_t(r),uint32_t(r>>32)};
}
// Builtin 59 (3032C0): |v| of the xyzw velocity (w 0) by VU vmul.xyzw, vadday.x, vmaddaz.x, vmaddw.x, vsqrt, then mul.s by G+0x14
// (0x3C888889 = 1/60, the frame step). PS2 ctm-parity/grinder/c-grind: all 212 increments of Grinder's distance on a rail = cvt.w.s
// of it (web/test-big-challenges.mjs).
static float mission_speed59(const std::array<float,3>& u){
 terrain_original::Rounding rounding;using terrain_original::add;using terrain_original::mul;
 return mul(terrain_original::sqrt(add(add(add(mul(u[0],u[0]),mul(u[1],u[1])),mul(1.f,mul(u[2],u[2]))),mul(1.f,mul(0.f,0.f)))),std::bit_cast<float>(0x3C888889u));
}
static StageArg mission_builtin(int index,const StageArg* args,int count){
 StageArg nil{};
 switch(index){
 case 40:{ // 3012F0: challenge id (symbol), op, player -> int 30A868 (nil for id -1)
  std::array<uint32_t,3> f{0xffffffffu,0xffffffffu,0u};stage_parse(f,std::array<int32_t,3>{3,1,1},args,count); /*defaults 0x4FBBE0, types gp+0xB80*/
  if(f[0]==0xffffffffu)return nil;return stage_int(mission_op(f[0],int32_t(f[1]),int32_t(f[2])));}
 case 47:{ // 302DF0 -> 30A728(C, id, 2): next task (id -1: the current mission)
  std::array<uint32_t,2> f{0xffffffffu,0u};stage_parse(f,std::array<int32_t,2>{3,0},args,count);mission_op_list(f[0],2);return nil;}
 case 50:case 79:{ // 302EF8 / 3037B0: rider stat 122EE8(rider, n), key0 through the jump tables 0x4897B0 / 0x489800 (same order)
  static constexpr std::array<int32_t,19> kStat{3,2,4,0,1,5,6,7,17,8,9,10,11,12,13,14,15,16,18};
  std::array<uint32_t,2> f{0u,0u};stage_parse(f,std::array<int32_t,2>{1,0},args,count);return stage_int(f[0]<19?mission_rider_stat(kStat[f[0]]):0);}
 case 63:return stage_int(mission_rider_stat(4)?railMotion.railId:-1); // 3034F8: on a rail (122EE8 4) -> owner +0xD4 = the rail motion +0x24 (owner +0xB0): the rail's packed id (rid << 8 | track; PS2 fr-idle 0x607 = ABC1_A rail 6), else -1
 case 64:{ // 300948: current player's rider vt+0x60 1234D0 to the instance matrix (key0, -1 current)
  std::array<uint32_t,2> f{0xffffffffu,0xffffffffu};stage_parse(f,std::array<int32_t,2>{1,0},args,count);
  const uint32_t target=mission_resolve(f[0]);if(!target||sharedWorldReplay)return nil;
  std::array<float,16> m;mission_instance_matrix_arg(target,m);mission_rider_teleport(m);return nil;}
 case 65:{ // 301D78: int(rider vt+0x18 108C28: the instance is in the rider's contact list +0x5B8)
  std::array<uint32_t,2> f{0xffffffffu,0xffffffffu};stage_parse(f,std::array<int32_t,2>{1,0},args,count);
  const uint32_t target=mission_resolve(f[0]);return stage_int(target&&mission_rider_in(target)?1:0);}
 case 75:{std::array<uint32_t,2> f{0u,0u};stage_parse(f,std::array<int32_t,2>{1,1},args,count);missionHud[4]=1;missionHud[5]=int32_t(f[0]);missionHud[6]=int32_t(f[1]);return nil;} // 304100 -> 30B4D8
 case 76:{std::array<uint32_t,2> f{0u,0xffffffffu};stage_parse(f,std::array<int32_t,2>{1,1},args,count);missionHud[7]=1;missionHud[8]=int32_t(f[0]);missionHud[9]=int32_t(f[1]);return nil;} // 304210 -> 30B4F0
 case 98:{std::array<uint32_t,2> f{0u,0u};stage_parse(f,std::array<int32_t,2>{1,1},args,count);missionHud[1]=1;missionHud[2]=int32_t(f[0]);missionHud[3]=int32_t(f[1]);return nil;} // 303FF0 -> 30B4C0
 case 81:{ // 304320 -> 30B508(C, player, instance): height reference
  std::array<uint32_t,2> f{0xffffffffu,0u};stage_parse(f,std::array<int32_t,2>{1,0},args,count);
  const uint32_t target=mission_resolve(f[0]);if(target){missionHud[10]=1;missionHud[12]=0;missionHud[11]=int32_t(target);}return nil;}
 case 86:{ // 304C50: float rider z - instance +0x48 (0 without the instance)
  std::array<uint32_t,2> f{0xffffffffu,0u};stage_parse(f,std::array<int32_t,2>{1,0},args,count);
  const uint32_t target=mission_resolve(f[0]);float v=0;
  if(target){terrain_original::Rounding rounding;std::array<float,16> m;mission_instance_matrix_arg(target,m);v=originalScalarSubtract(physicsState.position[2],m[14]);}
  return {0,std::bit_cast<uint32_t>(v),0,2};}
 case 107:{ // 306E68 -> 30BFC0(C, previous, gate, next): 1 through the gate (and 2974A0 sound 0x5C), -1 missed, 0 not yet
  std::array<uint32_t,3> f{0xffffffffu,0xffffffffu,0xffffffffu};stage_parse(f,std::array<int32_t,3>{1,1,1},args,count); /*types 0x446980*/
  auto inst=[](uint32_t key)->uint32_t{return key==0xffffffffu?0u:mission_resolve(key);};
  const uint32_t prev=inst(f[0]),gate=inst(f[1]),next=inst(f[2]);
  if(gate&&mission_rider_in(gate)){audio_stage_sound(0,{gate,0x5Cu,0xffffffffu},gate);return stage_int(1);}
  // instance +0x40 (matrix row 3, w = 1) and rider +0x110; VU length of the xyzw difference (vsqrt of x2+y2+z2+w2).
  auto at=[](uint32_t r){std::array<float,16> m;mission_instance_matrix_arg(r,m);return std::array<float,4>{m[12],m[13],m[14],1.f};};
  auto len=[](const std::array<float,4>& a,const std::array<float,4>& b){terrain_original::Rounding rounding;float s=0;
   for(int k=0;k<4;k++){const float d=originalScalarSubtract(a[k],b[k]);s=originalScalarAdd(s,terrain_original::mul(d,d));}return std::sqrt(s);};
  const std::array<float,4> rider{physicsState.position[0],physicsState.position[1],physicsState.position[2],1.f},zero{};
  const auto g=gate?at(gate):zero,p=prev?at(prev):zero,n=next?at(next):zero;
  terrain_original::Rounding rounding;
  if(prev){if(len(rider,p)<=terrain_original::mul(len(g,p),1.1f))return stage_int(0);}          // gp-0x3364 = 1.1
  else if(len(rider,g)<=terrain_original::mul(len(n,g),1.5f))return stage_int(0);
  if(next)return stage_int(len(rider,g)<=terrain_original::mul(len(n,g),1.1f)?0:-1);            // gp-0x3360 = 1.1
  return stage_int(len(rider,g)<=terrain_original::mul(len(g,p),1.5f)?0:-1);
 }
 case 78:{ // 3036B0 -> 30AF08(C, field): a field of the dispatched trick event's record (C+0x29C; 0 without one or for field >= 16)
  std::array<uint32_t,1> f{0xffffffffu};stage_parse(f,std::array<int32_t,1>{1},args,count); /*defaults gp+0x26D0 (-1), types gp+0xC38*/
  return stage_int(mission_trick_field(missionEventData,f[0]));}
 case 83:{ // 3045B8: sixteen int fields (keys 0..15, default 0) packed into a trick record, the inverse of 30AF08 (bits 59..63
  // clear), then 30B520: C+0x34 = 1 and C+0x38..+0x3F = the record: the called trick the challenge HUD names (1EB350 0x6000)
  std::array<uint32_t,16> f{};std::array<int32_t,16> t;t.fill(1);stage_parse(f,t,args,count); /*defaults 0x4FBCC8, types 0x446650*/
  const auto r=mission_trick_pack(f);missionHud[13]=1;missionHud[14]=int32_t(r[0]);missionHud[15]=int32_t(r[1]);return nil;}
 case 59:{ // 3032C0: float |v| * G+0x14 of the context's rider (ctx+0 -> race riders[]): its +0x6C0 part's vt+0x14 (140910) is
  // rider+0x1E0, the velocity (w 0); VU vmul.xyzw, vadday.x, vmaddaz.x, vmaddw.x, vsqrt, then mul.s by G+0x14 (0x3C888889 = 1/60,
  // the frame step): the distance of this tick in cm. Kick Doubt's Grinder / Pepper Grinder / Meat Grinder add it on a rail.
  if(!missionSpeedBuiltin){++missionUnsupported;return nil;} // pv bcSpeed (web/big-challenges.js): nil before (the port's old answer)
  return {0,std::bit_cast<uint32_t>(mission_speed59(physicsState.velocity)),0,2};}
 case 51:case 110:++missionUnsupported;return nil; // 302BD8 (unused), 307020 1E3570 query (web/peak_world.inc owns 110)
 default:return nil;
 }
}

// Dispatch from web/stage_script_gameplay.inc stage_builtin (builtins 40 47 50 51 63 64 65 75 76 78 79 81 83 86 98 107 110).
StageArg mission_builtin_call(int index,const StageArg* args,int count){return mission_builtin(index,args,count);}
extern "C" {
// The called trick of the running challenge (C+0x34 flag, C+0x38 record: builtin 83) named by 116950, "" without one.
EMSCRIPTEN_KEEPALIVE const char* mission_called_trick(){
 RIDER_LOCAL static std::string text;text.clear();
 if(missionHud[13])try{text=score_trick_name({uint32_t(missionHud[14]),uint32_t(missionHud[15])});}catch(const std::exception&){text.clear();} // no name tables (tests) or a record 116950 cannot name
 return text.c_str();
}
// Tests: post a trick event (kind 3) with a record, as 118FF8 does for a named trick.
EMSCRIPTEN_KEEPALIVE void mission_test_trick(uint32_t lo,uint32_t hi){browser_mission_trick({lo,hi});}
}

// ---- host interface (web/big-challenges.js) ---------------------------------------------------------------------------------
extern "C" {
// 88 status words (character block +0x118 + 4*index) from the career save; unset rows keep the new-character defaults (151600).
EMSCRIPTEN_KEEPALIVE void mission_set_status(int index,uint32_t word){mission_status_defaults();if(index>=0&&index<88)missionStatus[size_t(index)]=word;}
EMSCRIPTEN_KEEPALIVE uint32_t* mission_status(){mission_status_defaults();missionStatusDirty=false;return missionStatus.data();}
EMSCRIPTEN_KEEPALIVE void mission_offer_gate(int value){missionOfferGate=value;}
// The game update's queue read (2306A8 @230890): {type, id, player} of the queue front, or type 0. Type 9 runs 30A868(id, 5)
// (the challenge fails) and is popped; type 8 stays until the prompt opens (mission_offer_pop).
EMSCRIPTEN_KEEPALIVE int32_t* mission_offer_peek(){
 RIDER_LOCAL static int32_t v[3];v[0]=v[1]=v[2]=0;if(missionOfferRead==missionOfferWrite)return v;
 const auto e=missionOffers[size_t(missionOfferRead)];v[0]=e.type;v[1]=int32_t(e.id);v[2]=e.player;
 if(e.type==9){mission_op(e.id,5,e.player);missionOfferRead=(missionOfferRead+1)&15;}
 else if(e.type!=8)missionOfferRead=(missionOfferRead+1)&15;
 return v;
}
// Prompt overlay 0x1D init (1F7548): pops the entry; its id (or C+0x2A0 without one) is the prompt's challenge.
EMSCRIPTEN_KEEPALIVE int32_t mission_offer_pop(){if(missionOfferRead==missionOfferWrite)return missionActive;const auto e=missionOffers[size_t(missionOfferRead)];missionOfferRead=(missionOfferRead+1)&15;audio_event(AE_PICKUP,4,1); /*230890: 29CED8(audio, 4)*/return int32_t(e.id);}
// Prompt notify 1F75A0 / fail overlay 1F7800 / pause overlay 2 (20D944): 1 Yes (a running challenge restarts, 30B6F8; otherwise
// C+0x2A0 = id and 30B540), 2 No (30B658), 3 Quit Challenge (30B758), 4 retry after a fail (30B6F8 with C+0x2A0 = id).
EMSCRIPTEN_KEEPALIVE void mission_prompt(int action,int32_t id){
 switch(action){
  case 1:if(mission_running_challenge())mission_restart_op();else{missionActive=id;mission_accept_op();}missionUiEvents.insert(missionUiEvents.end(),{7,id,0});break; // 29D6D0
  case 2:missionActive=id;mission_decline_op(3);break;
  case 3:mission_decline_op(4);break;
  case 4:missionActive=id;mission_restart_op();break;
  default:break;
 }
}
EMSCRIPTEN_KEEPALIVE void mission_speed_builtin(int on){missionSpeedBuiltin=on!=0;} // pv bcSpeed
EMSCRIPTEN_KEEPALIVE void mission_lifecycle(int bits){missionLifecycle=bits;} // pv bcDecline (missionLifecycle)
// Tests: builtin 59's value for a velocity (cm/s), as float bits.
EMSCRIPTEN_KEEPALIVE uint32_t mission_test_speed(float x,float y,float z){return std::bit_cast<uint32_t>(mission_speed59({x,y,z}));}
EMSCRIPTEN_KEEPALIVE int mission_running(){return mission_running_challenge()?1:0;} // 30B8C0
// [active id (C+0x2A0), running (30B8C0), mission state, task index, task count, C+0 .. C+0x3C (16 words), height (cm, float bits)]
EMSCRIPTEN_KEEPALIVE int32_t* mission_hud(){
 RIDER_LOCAL static int32_t v[24];std::fill(std::begin(v),std::end(v),0);v[0]=missionActive;v[1]=mission_running_challenge();
 if(missionActive!=-1){const int32_t h=mission_find_in(1,uint32_t(missionActive));if(auto* m=mission_obj(h)){v[2]=m->state;v[3]=m->index;v[4]=m->count;}}
 for(int i=0;i<16;i++)v[5+i]=missionHud[size_t(i)];
 if(missionHud[10]){std::array<float,16> m;mission_instance_matrix_arg(uint32_t(missionHud[11]),m);terrain_original::Rounding rounding;v[21]=int32_t(std::bit_cast<uint32_t>(originalScalarSubtract(physicsState.position[2],m[14])));}
 return v;
}
// UI / audio / career events since the last call: [count, then {kind, a, b}]: 1 challenge start (29D6E0: id, 1 = restart),
// 2 complete (id, cash), 3 success music (29D8E0), 4 challenge stop (29DBB0: id, 0 fail / 1 quit or decline), 5 status word
// (index, word), 6 cash award (amount: 119EF8(score, 4)), 7 prompt accepted (29D6D0).
// World state 15 (236058: MCOMM Session "Session this area?", a Transport to the current location, the post-event Transport
// back to the same course), around the host's placement (web/free-ride.js placeRegion -> place_rider_region = 11DE60(rider,
// k + 1, 2)): phase 0 = 30B7F8 before it (every mission back to inactive, the active challenge stopped, the offers cleared),
// phase 1 = 11DF18(rider, 1) after it, whose 3099F8 posts WScript event kind 5 as the reset controller's placement does
// (12F398). The other 11DF18(rider, 1) caller, 20D1D8 action 4 ("Quit the event?", overlay 0x1B = MCOMM item 9), is in no
// MCOMM list (0x478140..0x4781A8) and cannot be reached; a location entry (11D390) calls 11DF18(rider, 0).
EMSCRIPTEN_KEEPALIVE void mission_world_session(int phase){if(phase==0)mission_reset_all();else browser_mission_placed();}
EMSCRIPTEN_KEEPALIVE int32_t* mission_ui_events(){RIDER_LOCAL static std::vector<int32_t> out;out.assign(1,int32_t(missionUiEvents.size()/3));out.insert(out.end(),missionUiEvents.begin(),missionUiEvents.end());missionUiEvents.clear();return out.data();}
// Diagnostics / tests: [missions, inactive, active, pending, ticks, programs run, unsupported builtins, pending op, events queued,
// offers queued] then per mission {id, state, index, count, list}.
EMSCRIPTEN_KEEPALIVE int32_t* mission_info(){
 RIDER_LOCAL static std::vector<int32_t> out;out={int32_t(missionPool.size()),int32_t(missionLists[0].size()),int32_t(missionLists[1].size()),int32_t(missionLists[2].size()),int32_t(missionTicks),int32_t(missionProgramsRun),int32_t(missionUnsupported),missionPendingOp,int32_t(missionEvents.size()),(missionOfferWrite-missionOfferRead)&15};
 for(size_t h=0;h<missionPool.size();h++){const auto& m=missionPool[h];if(!m.alive)continue;int list=-1;for(int l=0;l<3;l++)if(mission_contains(l,int32_t(h)))list=l;out.insert(out.end(),{int32_t(m.id),m.state,m.index,m.count,list});}
 return out.data();
}
// Tests (web/test-big-challenges.mjs): the mission system without the rider / world. setup: location PEAK1, event kind, the
// stage VM of every Peak 1 stage, the octree flag of the listed SDB tracks; rider: position (source cm) + contact list
// (rider +0x5B8); contact: 121818 -> 30A060 for the human; tick: the entity pass with the WScript tick (race_begin).
EMSCRIPTEN_KEEPALIVE void mission_test_setup(int kind,const int32_t* tracks,int n){
 browser_set_piece_location("PEAK1");browserEventKind=kind;ssx::worldTrackOctree().fill(0);ssx::worldTrackResidency().fill(0);
 for(int i=0;i<n;i++)if(tracks[i]>=0&&size_t(tracks[i])<ssx::worldTrackOctree().size())ssx::worldTrackOctree()[size_t(tracks[i])]=ssx::worldTrackResidency()[size_t(tracks[i])]=1;
 browser_reset_stage_scripts();
}
EMSCRIPTEN_KEEPALIVE void mission_test_rider(float x,float y,float z,const uint32_t* triggers,int n){
 physicsState.position={x,y,z};browserRiderTriggers.fill(0xffffffffu);for(int i=0;i<n&&i<63;i++)browserRiderTriggers[size_t(i)]=triggers[i];
}
EMSCRIPTEN_KEEPALIVE void mission_test_contact(uint32_t resource){stage_contact(resource,true,true);}
EMSCRIPTEN_KEEPALIVE void mission_test_load_trigger(){browser_mission_load_trigger();} // builtin 68 action 2 for the human
EMSCRIPTEN_KEEPALIVE void mission_test_gate(){browser_mission_gate();} // builtin 67's 30B7F8 (key1 0, a 0x445E40 entry other than 1)
EMSCRIPTEN_KEEPALIVE void mission_test_tick(){browser_stage_entities_tick();}
// Tests: the PS2 derived state bc-sd-lastgate pokes the running challenge (task index, task states, a table value).
EMSCRIPTEN_KEEPALIVE void mission_test_set(int task,uint32_t symbol,int32_t value){
 if(missionActive==-1||!stageVm)return;auto* m=mission_obj(mission_find_in(1,uint32_t(missionActive)));if(!m)return;
 if(task>=0&&task<m->count){m->index=task;for(int i=0;i<m->count;i++)m->tasks[size_t(i)].state=i<task?3:i==task?2:0;}
 if(symbol&&m->hasTable)stageVm->set(m->table,{symbol,0,3},{uint32_t(value),0,1});
}
// Value of a symbol in the running challenge's table (tests: the Speed Demon count 0x07A6C5B1 / timer 0x08B03C93).
EMSCRIPTEN_KEEPALIVE int32_t mission_table_value(uint32_t symbol){
 if(missionActive==-1||!stageVm)return 0;const int32_t h=mission_find_in(1,uint32_t(missionActive));auto* m=mission_obj(h);if(!m||!m->hasTable)return 0;
 const auto v=stageVm->get(m->table,{symbol,0,3});return int32_t(v.word0);
}
}
