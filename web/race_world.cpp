#include "rider_local.hpp"
// Shared race world of the six-rider event (one instance per race, used by web/ai-racers.js): the
// original manager refresh 0x10F560, run by 0x128AF0 before the riders' control phase:
//   every sixth game tick (game-info +8 % 6 == 0):
//     0x10F998 ranking (rank mode game-info +0x74, only when 0x12A180: every roster actor +0x880 == 7):
//       mode 1 (race) key = +0x878 ? 0xFFFF : -cvt.w.s(+0x4D0 remaining + cvt.s.w(+0xEC * 20)),
//       mode 2 (score) key = +0x878 ? 0xFFFF0001 : *(*(+0x790)+0x198) (freestyle: R&B slope style ranks the human and its
//       computer opponent by run score, game-info +0x74 = 2; race_world_score feeds the totals, docs/slopestyle-bigair.md);
//       shell sort 0x3E6328 (gaps 0x451218 = 1,4,13,..; larger keys first), then +0xEC = place.
//     0x10F5CC..0x10F81C for owner a < active (count +0x78 minus tail +0x84, at least 2):
//       +0xDC = -50000, +0xE0 = 50000 (pacing bounds read by 0x10DEF0);
//       for b > a with an enabled record a->b: planar distance/bearings (originalPairProximity)
//       into both records, record a->b +0x1C = (0x155B50 relationship(a,b) >= 2).
//     0x10F878 designated peer: +0xF0 = 0, +0xF8 = -1; for b != a with relationship(a,b) == 4:
//       +0xF0 = 1, +0xF8 = b; +0xFC = the last relationship evaluated.
// Rider-pair contact timestamps (+0x10/+0x14/+0x18) belong to the pair dispatcher 0x107888.
#include "../engine/rider_pair_system.hpp"
#include "../engine/original_float.hpp"
#include <emscripten/emscripten.h>
#include <array>
#include <cstdint>
#include <stdexcept>
#include "audio_events.hpp" // audio observers (web/audio_events.inc)
using namespace ssx;
namespace {
struct WorldRecord {OriginalPairRecord pair;bool human=false;int32_t flag1C=0,reaction20=0;};
struct RaceWorld {
 unsigned count=0,tail=0;int32_t rankMode=1;
 std::array<std::array<WorldRecord,6>,6> records{};
 std::array<int32_t,6> rank{},designatedValid{},designatedSlot{},lastRelationship{};
 std::array<float,6> pacingNegative{},pacingPositive{};
 std::array<std::array<int32_t,6>,6> relationship{};
 std::array<bool,6> active{},finished{};std::array<float,6> remaining{};std::array<int32_t,6> score{};std::array<terrain_original::Vector,6> position{};
};
RIDER_LOCAL RaceWorld world;RIDER_LOCAL float worldOut[6*6*10+6*6];
// Rider pairs 0x107888: attributes per slot (0x11FF98 weight inputs, attack stat) from npc-riders.json.
RIDER_LOCAL std::array<int,6> pairWeight{};RIDER_LOCAL std::array<float,6> pairStat{},pairAttackStat{};RIDER_LOCAL bool pairKnockdownCheat=false;
int32_t eeTruncate(float value){ // cvt.w.s: truncation toward zero, saturating
 if(!(value==value))return 0x7fffffff;if(value>=2147483648.f)return 0x7fffffff;if(value<=-2147483648.f)return int32_t(0x80000000u);return int32_t(value);
}
void rank(){
 const unsigned n=world.count;std::array<int32_t,6> keys{},order{};
 for(unsigned s=0;s<n;++s){order[s]=int32_t(s);
  if(world.rankMode==1){
   if(world.finished[s])keys[s]=0xFFFF;
   else{OriginalRounding rounding;keys[s]=-eeTruncate(originalScalarAdd(world.remaining[s],float(world.rank[s]*20)));}
  }else if(world.rankMode==2)keys[s]=world.finished[s]?int32_t(0xFFFF0001u):world.score[s]; // 10FA90..10FAAC
  else throw std::runtime_error("Original ranking mode other than race is not recovered");
 }
 // 0x3E6328: Knuth-gap shell sort, descending keys, index array moved with the keys.
 static constexpr int32_t gaps[]={1,4,13,40,121,364};unsigned g=0;while(g+1<std::size(gaps)&&gaps[g+1]<int32_t(n))++g;
 for(;;){const int32_t gap=gaps[g];
  for(int32_t i=gap;i<int32_t(n);++i){int32_t key=keys[i],index=order[i],j=i-gap;
   while(j>=0&&keys[j]<key){keys[j+gap]=keys[j];order[j+gap]=order[j];j-=gap;}
   keys[j+gap]=key;order[j+gap]=index;}
  if(gap<2)break;--g;
 }
 for(unsigned place=0;place<n;++place){const int32_t r=order[place];
  // 10FB0C..10FB8C: a rider whose place changed -> 299E28(audio, rider, behind, ahead, old place, new place) (audio observer).
  if(world.rank[r]!=int32_t(place))audio_event(AE_OVERTAKE,float(r),float(world.rank[r]*8+int32_t(place)),place+1<n?float(order[place+1]):-1.f,place>0?float(order[place-1]):-1.f);
  world.rank[r]=int32_t(place);}
}
}
extern "C" {
// init: [count, tail, rankMode, then per owner a (6): 6 x (enabled, human, distance, bearing, +10, +14, +18, +1C, +20), rank, then relationships 6x6]
EMSCRIPTEN_KEEPALIVE void race_world_reset(const float* init){
 world={};world.count=unsigned(init[0]);world.tail=unsigned(init[1]);world.rankMode=int32_t(init[2]);
 if(world.count<1||world.count>6||world.tail>world.count)throw std::runtime_error("Race world roster");
 const float* p=init+3;
 for(unsigned a=0;a<6;++a){for(unsigned b=0;b<6;++b){auto& r=world.records[a][b];r.pair.enabled=p[0]!=0;r.human=p[1]!=0;r.pair.planarDistanceCm=p[2];r.pair.bearing=p[3];r.pair.lastContactTick=int32_t(p[4]);r.pair.lastCheckedTick=int32_t(p[5]);r.pair.lastAttackTick=int32_t(p[6]);r.flag1C=int32_t(p[7]);r.reaction20=int32_t(p[8]);p+=9;}world.rank[a]=int32_t(*p++);}
 for(unsigned a=0;a<6;++a)for(unsigned b=0;b<6;++b)world.relationship[a][b]=int32_t(*p++);
 world.pacingNegative.fill(-50000);world.pacingPositive.fill(50000);world.designatedSlot.fill(-1);
}
// 0x10F3B8 (10F398, from 1297C8(C, 1): the rider manager's restart 129768, which 230180 calls through C vt+0xCC+0x28 with 3):
// every record of the roster made again in place (0x10F420..0x10F4F4): +0 = a != b and b loaded (11D640; every rider here),
// +8 = 1e10 ([gp-0x7CB0]), +0xC .. +0x20 = 0. The +4 kind word, the ranks and the relationship levels stay (web/ai-racers.js
// resume, the WS15 return; the game tick restarts at 0 with it). Not modelled: +0x4D8 = 113128(rider), +0xE8 = -1.
EMSCRIPTEN_KEEPALIVE void race_world_pair_restart(){
 for(unsigned a=0;a<6;++a)for(unsigned b=0;b<6;++b){auto& r=world.records[a][b];r.pair=OriginalPairRecord{};r.pair.enabled=a!=b&&a<world.count&&b<world.count;r.flag1C=r.reaction20=0;}
}
// 128A48(C, mode) (0x128A48..0x128AB4): game info +0x74 = mode; mode 0 sets every rider's +0xEC to 0, another mode to its list index.
// World state 1's phase 1 calls it with 0 after a WS15 return (0x234570; PS2 c0a-ret3: ranks and mode 0 from the return's tick 3).
EMSCRIPTEN_KEEPALIVE void race_world_rank_mode(int mode){world.rankMode=mode;for(unsigned s=0;s<6;++s)world.rank[s]=mode==0?0:int32_t(s);}
// Rank mode 2: each slot's score object total *(+0x790)+0x198 at the tick start (web/ai-racers.js beginTick).
EMSCRIPTEN_KEEPALIVE void race_world_score(unsigned slot,int32_t total){if(slot<6)world.score[slot]=total;}
// riders: per slot [x, y, z (source cm), +0x4D0 remaining, +0x878, +0x880 == 7]
EMSCRIPTEN_KEEPALIVE int race_world_frame(int32_t tick,const float* riders){
 for(unsigned s=0;s<6;++s){const float* r=riders+6*s;world.position[s]={r[0],r[1],r[2]};world.remaining[s]=r[3];world.finished[s]=r[4]!=0;world.active[s]=r[5]!=0;}
 if(tick%6!=0)return 0;
 bool all=true;for(unsigned s=0;s<world.count;++s)all&=world.active[s];
 if(world.rankMode!=0&&all)rank();
 const int active=int(world.count)-int(world.tail);
 if(active>=2)for(int a=0;a<active;++a){
  world.pacingNegative[a]=-50000.f;world.pacingPositive[a]=50000.f;
  for(int b=a+1;b<active;++b){auto& ab=world.records[a][b];if(!ab.pair.enabled)continue;auto& ba=world.records[b][a];
   const auto result=originalPairProximity(world.position[a],world.position[b]);
   ab.pair.planarDistanceCm=ba.pair.planarDistanceCm=result.distanceCm;ab.pair.bearing=result.bearingAToB;ba.pair.bearing=result.bearingBToA;
   ab.flag1C=world.relationship[a][b]>=2;
  }
 }
 for(unsigned a=0;a<world.count;++a){world.designatedValid[a]=0;world.designatedSlot[a]=-1;
  for(unsigned b=0;b<world.count;++b){if(a==b)continue;const int32_t rel=world.relationship[a][b];
   if(rel==4){world.designatedValid[a]=1;world.designatedSlot[a]=int32_t(b);}world.lastRelationship[a]=rel;}}
 return 1;
}
// Records a->b (6x6x10: enabled, human, distance, bearing, +10, +14, +18, +1C, +20, 0) then
// per rider (6x6: rank +EC, designated +F0, +F8, +DC, +E0, 0).
EMSCRIPTEN_KEEPALIVE float* race_world_state(){
 float* o=worldOut;
 for(unsigned a=0;a<6;++a)for(unsigned b=0;b<6;++b){const auto& r=world.records[a][b];
  *o++=r.pair.enabled;*o++=r.human;*o++=r.pair.planarDistanceCm;*o++=r.pair.bearing;*o++=float(r.pair.lastContactTick);*o++=float(r.pair.lastCheckedTick);*o++=float(r.pair.lastAttackTick);*o++=float(r.flag1C);*o++=float(r.reaction20);*o++=0;}
 for(unsigned a=0;a<6;++a){*o++=float(world.rank[a]);*o++=float(world.designatedValid[a]);*o++=float(world.designatedSlot[a]);*o++=world.pacingNegative[a];*o++=world.pacingPositive[a];*o++=0;}
 return worldOut;
}
// ---- 0x107888 rider pairs over the six cores (callbacks through web/ai-racers.js Module.pairHost) ----
EM_JS(void,js_pair_view,(int slot,uint32_t* out),{Module.pairHost.view(slot,out);});
EM_JS(void,js_pair_translate,(int slot,float x,float y,float z),{Module.pairHost.translate(slot,x,y,z);});
EM_JS(void,js_pair_velocity,(int slot,float x,float y,float z,int reseed),{Module.pairHost.velocity(slot,x,y,z,reseed);});
EM_JS(void,js_pair_react,(int slot,int kind,int animation,int attack,const float* event,int other),{Module.pairHost.react(slot,kind,animation,attack,event,other);}); // other: 0x155BF0 relationship event (web/lineup.js)
EM_JS(uint32_t,js_pair_random,(),{return Module.pairHost.random()>>>0;});
// Online races (web/net/pair-net.js): an attack hit on a remote rider is reported to its own client.
EM_JS(void,js_pair_attack,(int victim,int attacker,float x,float y,float z,float amount),{if(Module.pairHost.attack)Module.pairHost.attack(victim,attacker,x,y,z,amount);});
// Online races: slots whose remote rider has no live state (not yet received, disconnected) take no part in pairs.
RIDER_LOCAL uint32_t pairDisabledMask=0;
// In-race relationship levels (0x155BF0 changed a record; web/lineup.js): 6x6 relationship(own, peer) for 10F560.
EMSCRIPTEN_KEEPALIVE void race_world_set_relationships(const float* m){for(unsigned a=0;a<6;++a)for(unsigned b=0;b<6;++b)world.relationship[a][b]=int32_t(m[a*6+b]);}
// A slot's setup record turned into a player setup (149A88(., slot): character 4, +0x10 |= 2; web/animation_bridge.cpp
// rider_setup_player_reset for the rider's own words), the pair inputs 107888 resolves from it: 0x11FF98's weight is CHARDB[4].+0x40
// (65: 0x530970 + id x 0x88 + 0x40) and the collision / attack stat getter 0x148F50 returns 0.5 (1477E8 set: 0x148F98).
EMSCRIPTEN_KEEPALIVE void race_world_player_setup(unsigned slot){if(slot<6){pairWeight[slot]=65;pairStat[slot]=pairAttackStat[slot]=0.5f;}}
// weights: [weight attribute, collision stat, attack stat] x 6, then knockdown cheat.
EMSCRIPTEN_KEEPALIVE void race_world_pair_setup(const float* p){for(unsigned s=0;s<6;++s){pairWeight[s]=int(p[3*s]);pairStat[s]=p[3*s+1];pairAttackStat[s]=p[3*s+2];}pairKnockdownCheat=p[18]!=0;}
// One slot's 0x107888 pass (after the shared pose). Returns the dispatch counts.
RIDER_LOCAL static std::array<std::array<uint32_t,140>,6> pairRaw{};RIDER_LOCAL static std::array<BodyCollisionVolume,6> pairBodies;
static OriginalPairCallbacks pair_callbacks(){
 OriginalPairCallbacks cb;
 cb.liveView=[](unsigned s){
  auto& raw=pairRaw;auto& bodies=pairBodies;
  auto f=[&](unsigned s,unsigned i){return std::bit_cast<float>(raw[s][i]);};
  auto v3=[&](unsigned s,unsigned i){return terrain_original::Vector{f(s,i),f(s,i+1),f(s,i+2)};};
  js_pair_view(int(s),raw[s].data());OriginalPairActorView v;v.slot=s;v.kind880=7;v.disabled=world.finished[s]||((pairDisabledMask>>s)&1);
  auto& b=bodies[s];b=BodyCollisionVolume{};if(raw[s][0]){b.count=raw[s][1];b.activeMask=raw[s][2];b.broadCenterCm=v3(s,3);b.broadRadiusCm=f(s,6);
   for(unsigned i=0;i<b.count&&i<20;++i){b.spheres[i].centerCm=v3(s,7+4*i);b.spheres[i].radiusCm=f(s,10+4*i);}
   if(raw[s][110])b.reactionFrame=OriginalCollisionFrame{v3(s,111),v3(s,114),v3(s,117),v3(s,120)};v.body=&b;}
  v.impulse.velocityCmps=v3(s,87);v.impulse.groundNormal=v3(s,90);v.impulse.physicalUp=v3(s,93);
  v.impulse.motionMode=int(raw[s][96]);v.impulse.controlState=int(raw[s][97]);v.impulse.ragdollSubmode=int(raw[s][98]);
  if(raw[s][110])v.reaction.presentation={v3(s,111),v3(s,114),v3(s,117),v3(s,120)};v.reaction.physical={v3(s,123),v3(s,126),v3(s,129),v3(s,132)};
  v.reaction.reverseStance=raw[s][135]!=0;v.reaction.manualSpin=f(s,136);
  v.attack.positionCm=v3(s,99);v.attack.animationClass1=int(raw[s][102]);v.attack.marker0=raw[s][103]!=0;v.attack.marker1=raw[s][104]!=0;
  v.attack.facing340=v3(s,105);v.attack.strength350=f(s,108);v.attack.resolvedAttackStat=pairAttackStat[s];
  v.weightAttribute=pairWeight[s];v.resolvedCollisionStat=pairStat[s];v.boost=f(s,109);return v;};
 cb.translate=[](unsigned s,terrain_original::Vector d){js_pair_translate(int(s),d[0],d[1],d[2]);};
 cb.setVelocity=[](unsigned s,terrain_original::Vector v,bool reseed){js_pair_velocity(int(s),v[0],v[1],v[2],reseed);};
 cb.react=[](unsigned target,unsigned other,const OriginalPairReactionRequest& r,const CollisionRandom&){
  if(r.kind==OriginalPairReactionRequest::Kind::None)return;
  audio_event(AE_PAIR,float((r.kind==OriginalPairReactionRequest::Kind::Soft?0:1)+(r.attack?2:0)),0,float(target),float(other)); /*107E70 -> 10E228 / 10E2E8 / 10E3A8 / 10E468 on the target*/
 float e[10];for(unsigned k=0;k<3;++k){e[k]=r.event.pointCm[k];e[3+k]=r.event.normal[k];e[6+k]=r.event.incomingDirection[k];}e[9]=r.event.closingSpeedCmps;
  js_pair_react(int(target),r.kind==OriginalPairReactionRequest::Kind::Soft?1:2,r.animation,r.attack,e,int(other));};
 cb.randomWord=[](){return js_pair_random();};
 cb.attackHit=[](unsigned victim,unsigned attacker,terrain_original::Vector d,float amount){js_pair_attack(int(victim),int(attacker),d[0],d[1],d[2],amount);};
 return cb;
}
EMSCRIPTEN_KEEPALIVE float* race_world_pairs(int32_t tick,int slot){
 RIDER_LOCAL static float out[5];
 OriginalRiderPairSystem system(world.count,pair_callbacks(),world.tail);
 OriginalRiderPairSystem::Records records{};for(unsigned a=0;a<6;++a)for(unsigned b=0;b<6;++b)records[a][b]=world.records[a][b].pair;system.seedRecords(records);
 const auto counts=system.resolveActor(unsigned(slot),tick,pairKnockdownCheat);
 for(unsigned a=0;a<6;++a)for(unsigned b=0;b<6;++b){auto& r=world.records[a][b].pair;const auto& n=system.records()[a][b];r.lastContactTick=n.lastContactTick;r.lastCheckedTick=n.lastCheckedTick;r.lastAttackTick=n.lastAttackTick;}
 out[0]=counts.checks;out[1]=counts.separations;out[2]=counts.impulses;out[3]=counts.attacks;out[4]=counts.reactions;return out;
}
// Online races: bit i set = slot i has no live remote state (web/net/pair-net.js).
EMSCRIPTEN_KEEPALIVE void race_world_pair_disable(uint32_t mask){pairDisabledMask=mask;}
// Online races: another client reported that `attacker`'s attack hit `target` (this client's rider): the
// 107E70 response of 107888's attack branch (impulse + typed reaction request) through the same callbacks.
EMSCRIPTEN_KEEPALIVE float* race_world_pair_respond(int target,int attacker,float x,float y,float z,float amount){
 RIDER_LOCAL static float out[5];
 OriginalRiderPairSystem system(world.count,pair_callbacks(),world.tail);
 OriginalRiderPairSystem::Records records{};for(unsigned a=0;a<6;++a)for(unsigned b=0;b<6;++b)records[a][b]=world.records[a][b].pair;system.seedRecords(records);
 const auto counts=system.respondToAttack(unsigned(target),unsigned(attacker),{x,y,z},amount,pairKnockdownCheat);
 out[0]=counts.checks;out[1]=counts.separations;out[2]=counts.impulses;out[3]=counts.attacks;out[4]=counts.reactions;return out;
}
}
#ifdef SSX_SNAPSHOT_REGISTRY // the rider-context snapshot's registry (web/generate-snapshot-registry.mjs, docs/replay.md §2a)
#include "generated/snapshot/race_world.inc"
#endif
