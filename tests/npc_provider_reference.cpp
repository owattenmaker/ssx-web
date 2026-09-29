#include "ps2_runtime_macros.h"
#include "../engine/npc_input.hpp"
#include "../engine/original_random.hpp"
#include <fstream>
#include <cfenv>
#include <cstdio>
#include <bit>
#include "npc_provider_registry.inc"
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){
 PS2Runtime rt;registerProviderOriginal(rt);std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(file)),{});if(m.size()!=32*1024*1024)return 2;
 auto u=[&](unsigned at){uint32_t v;std::memcpy(&v,m.data()+at,4);return v;};auto f=[&](unsigned at){return std::bit_cast<float>(u(at));};auto i=[&](unsigned at){return int32_t(u(at));};auto h=[&](unsigned at){int16_t v;std::memcpy(&v,m.data()+at,2);return v;};auto v=[&](unsigned at){return std::array<float,3>{f(at),f(at+4),f(at+8)};};
 auto context=[](uint32_t pc,uint32_t a0,uint32_t a1=0,uint32_t a2=0){R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,a0);SET_GPR_U32(&c,5,a1);SET_GPR_U32(&c,6,a2);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);return c;};
 auto run=[&](R5900Context& c){for(unsigned k=0;k<128;++k){auto fn=rt.lookupFunction(c.pc);if(!fn)throw std::runtime_error("Missing original NPC function "+std::to_string(c.pc));fn(m.data(),&c,&rt);if(c.pc==0x12345678)return;}throw std::runtime_error("NPC provider continuation limit");};
 unsigned game=u(u(u(0x4a30f0-0x848)+0x84)+0xc),base=u(0x4d33ac);std::array<unsigned,6> actors{};for(unsigned s=0;s<6;++s)actors[s]=u(game+0x28+s*4);
 if(argc>2){
  std::fesetround(FE_TOWARDZERO);ssx::OriginalRandomState expected;for(unsigned n=0;n<6;++n)expected.words[n]=u(0x4ff030+n*4);
  for(unsigned slot=0;slot<6;++slot){unsigned actor=actors[slot];if(i(u(actor+0x77c)+0xde4)!=0)continue;auto c=context(0x115d48,actor);run(c);unsigned draws=0;for(;draws<1000;++draws){bool same=true;for(unsigned n=0;n<6;++n)same &=expected.words[n]==u(0x4ff030+n*4);if(same)break;expected.next();}if(draws==1000)return 4;printf("upperreaction slot%u draws%u timer35C%.9g\n",slot,draws,f(actor+0x35c));}
  printf("upperreaction RNG");for(unsigned n=0;n<6;++n)printf(" %08x",u(0x4ff030+n*4));puts("");return 0;
 }
 std::vector<ssx::OriginalNpcPath> paths(u(0x4d33a8));for(unsigned k=0;k<paths.size();++k){unsigned p=base+k*64;auto&path=paths[k];path.geometry.origin=v(p+12);path.geometry.low=v(p+28);path.geometry.high=v(p+40);path.flags38=u(p+56);path.field3C=u(p+60);for(unsigned n=0;n<u(p+8);++n){unsigned at=u(p+24)+n*16;path.geometry.segments.push_back({f(at),f(at+4),f(at+8),f(at+12)});}for(unsigned n=0;n<u(p);++n){unsigned at=u(p+4)+n*16;path.geometry.events.push_back({u(at),u(at+4),f(at+8),f(at+12)});}}
 std::fesetround(FE_TOWARDZERO);
 for(unsigned slot=1;slot<6;++slot){unsigned actor=actors[slot],owner=u(actor+0x77c);ssx::OriginalNpcDrivingState state;
 state.desiredSpeedDF0=f(owner+0xdf0);state.parameterDF8=f(owner+0xdf8);state.parameterDFC=f(owner+0xdfc);state.boardTimerE40=i(owner+0xe40);state.targetPeerE70=i(owner+0xe70);state.offRouteTicksE74=i(owner+0xe74);state.oppositeHeadingTicksE78=i(owner+0xe78);state.defensiveTimerF30=f(owner+0xf30);state.defensiveDecisionF34=i(owner+0xf34);state.lastAttackTickF4=i(actor+0xf4);state.behaviorCounterF38=h(owner+0xf38);state.regionStartE50=v(owner+0xe50);state.regionEndE60=v(owner+0xe60);
 constexpr uint32_t behaviors[]={0x100680,0x1009e0,0x100b90,0x100f88};bool found=false;for(unsigned n=0;n<4;++n)if(u(owner+0xf48)==behaviors[n]){state.behavior=ssx::OriginalNpcBehavior(n);found=true;}if(!found)throw std::runtime_error("Unknown live behavior");
 auto&t=state.trick;t.indexE0C=i(owner+0xe0c);t.enabledE10=i(owner+0xe10);t.decisionE14=i(owner+0xe14);t.phaseE18=i(owner+0xe18);t.kindE1C=i(owner+0xe1c);t.fieldE20=i(owner+0xe20);t.remainingE34=f(owner+0xe34);t.spinE38=f(owner+0xe38);t.flipE3C=f(owner+0xe3c);
 ssx::OriginalNpcProviderContext input;auto&d=input.driving;d.position=v(actor+0x110);d.velocity=v(actor+0x1e0);d.boardUp=v(actor+0x1c0);d.physicalForward=v(actor+0x1b0);d.boostMeter=f(actor+0x2f8);d.superTime=f(actor+0x2f0);d.tick=i(game+8);d.motionMode=i(owner+0xde0);d.controlState=i(owner+0xde4);d.trajectoryElapsed=f(u(actor+0x788)+0xa0);d.selfSlot=slot;d.participantCount=u(game+0x78);if(u(actor+0xf0))d.designatedPeer=i(actor+0xf8);
 auto&r=d.route;r.pathIndex=(u(actor+0xab8)-base)/64;unsigned cache=u(actor+0xabc);r.cache={v(cache),f(cache+16),i(cache+20)};r.closestPoint=v(actor+0x490);r.lookaheadPoint=v(actor+0x4a0);r.previousLookaheadPoint=v(actor+0x4b0);r.previousDistance=f(actor+0x4c0);r.currentDistance=f(actor+0x4c4);r.lateralDistance=f(actor+0x4c8);r.heading=f(actor+0x4cc);
 std::vector<int> occupancy;for(unsigned s=0;s<6;++s){unsigned pair=actor+s*0x24;d.peers[s].enabled=u(pair);d.peers[s].planarDistanceCm=f(pair+8);d.peers[s].bearing=f(pair+12);d.peerHuman[s]=u(actors[s]+0x874);d.peerVelocities[s]=v(actors[s]+0x1e0);d.peerPathIndices[s]=(u(actors[s]+0xab8)-base)/64;if(!d.peerHuman[s])occupancy.push_back(d.peerPathIndices[s]);auto ac=context(0x311ae8,u(actors[s]+0x784),1);run(ac);auto*cp=&ac;d.peerUpperAnimationClass[s]=GPR_S32(cp,2);if(!input.pacing.referenceRemaining&&u(pair)&&u(pair+4))input.pacing.referenceRemaining=f(actors[s]+0x4d0);}
 input.pacing.remaining=f(actor+0x4d0);input.pacing.negativeThresholdDC=f(actor+0xdc);input.pacing.positiveThresholdE0=f(actor+0xe0);input.pacing.modeE4=i(actor+0xe4);input.pacing.eventVariant=int8_t(m[0x535c11]);input.currentTimeScale=f(actor+0x300);input.scoring.position=d.position;input.scoring.velocity=d.velocity;input.scoring.roleE00=h(owner+0xe00);input.scoring.allowFlag0E04=u(owner+0xe04);input.scoring.randomizeE08=u(owner+0xe08);input.scoring.currentPathIndex=r.pathIndex;input.scoring.followedPathIndex=d.designatedPeer?d.peerPathIndices[*d.designatedPeer]:-1;input.scoring.npcPathIndices=occupancy;
 std::array<int,6> relationships;for(unsigned s=0;s<6;++s){auto c=context(0x155b50,0,slot,s);run(c);auto*cp=&c;relationships[s]=GPR_S32(cp,2);}
 ssx::OriginalRandomState random;for(unsigned n=0;n<6;++n)random.words[n]=u(0x4ff030+n*4);unsigned draws=0;auto result=ssx::originalNpcControl(state,input,paths,[&](unsigned s){return relationships[s];},[&](){++draws;return random.next();});
 auto c=context(0x10a768,owner,0x50000);run(c);
 bool exact=result.words[0]==u(0x50000)&&result.words[1]==u(0x50004)&&std::bit_cast<uint32_t>(result.timeScale)==u(actor+0x300);
 auto eq=[&](unsigned at,auto value){return u(at)==std::bit_cast<uint32_t>(value);};
 exact &=eq(owner+0xdf0,state.desiredSpeedDF0)&&eq(owner+0xe40,state.boardTimerE40)&&eq(owner+0xe70,state.targetPeerE70)&&eq(owner+0xe74,state.offRouteTicksE74)&&eq(owner+0xe78,state.oppositeHeadingTicksE78)&&eq(owner+0xf30,state.defensiveTimerF30)&&eq(owner+0xf34,state.defensiveDecisionF34)&&eq(actor+0xf4,state.lastAttackTickF4)&&h(owner+0xf38)==state.behaviorCounterF38&&u(owner+0xf48)==behaviors[int(state.behavior)];
 exact &=eq(owner+0xe0c,t.indexE0C)&&eq(owner+0xe10,t.enabledE10)&&eq(owner+0xe14,t.decisionE14)&&eq(owner+0xe18,t.phaseE18)&&eq(owner+0xe1c,t.kindE1C)&&eq(owner+0xe20,t.fieldE20)&&eq(owner+0xe34,t.remainingE34)&&eq(owner+0xe38,t.spinE38)&&eq(owner+0xe3c,t.flipE3C);
 for(unsigned n=0;n<6;++n)exact &=u(0x4ff030+n*4)==random.words[n];
 printf("slot%u control%d tick%d words %08x %08x original %08x %08x scale %.9g/%.9g RNG draws%u exact%d\n",slot,d.controlState,d.tick,result.words[0],result.words[1],u(0x50000),u(0x50004),result.timeScale,f(actor+0x300),draws,exact);if(!exact)return 3;
 }
}
