#include "ps2_runtime_macros.h"
#include "../engine/animation_cycle.hpp"
#include "../engine/animation_sequence.hpp"
#include <fstream>
#include <cfenv>
#include <cstdio>
#include <random>
void sub_00104358_0x104358(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00133308_0x133308(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313A20_0x313a20(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00103CC8_0x103cc8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00103E28_0x103e28(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003139A8_0x3139a8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003135B0_0x3135b0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313CF0_0x313cf0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313D28_0x313d28(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313D40_0x313d40(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313800_0x313800(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x320000,g_ps2RecompiledFunctionTableSlotCount=0x88000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x88000]={nullptr};
static std::array<uint32_t,5> ids{0x2500,0x2400,0x2300,0x2600,0x2700};static std::array<float,5> durations{2.4666667f,2.4666667f,3.3f,2.4666667f,2.4666667f};
int main(int argc,char**argv){PS2Runtime rt;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>ram((std::istreambuf_iterator<char>(f)),{});auto wr=[&](unsigned a,const auto&v){memcpy(ram.data()+a,&v,sizeof(v));};auto rf=[&](unsigned a){float v;memcpy(&v,ram.data()+a,4);return v;};auto ru=[&](unsigned a){uint32_t v;memcpy(&v,ram.data()+a,4);return v;};
 rt.registerFunction(0x313C50,[](uint8_t*m,R5900Context*c,PS2Runtime*){unsigned at=GPR_U32(c,4)+GPR_U32(c,5)*0x1c,id=GPR_U32(c,6);unsigned index=0;while(index<5&&ids[index]!=id)++index;if(index==5)throw std::runtime_error("Unknown fixture clip");memcpy(m+at+4,&id,4);memcpy(m+at+16,&durations[index],4);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x313CF0,sub_00313CF0_0x313cf0);rt.registerFunction(0x313D28,sub_00313D28_0x313d28);rt.registerFunction(0x313D40,sub_00313D40_0x313d40);rt.registerFunction(0x3135B0,sub_003135B0_0x3135b0);rt.registerFunction(0x313800,sub_00313800_0x313800);
 rt.registerFunction(0x3139A8,sub_003139A8_0x3139a8);
 for(unsigned pc:{0x313938,0x313868})rt.registerFunction(pc,[](uint8_t*,R5900Context*c,PS2Runtime*){c->pc=GPR_U32(c,31);});
 std::fesetround(FE_TOWARDZERO);std::mt19937 rng(0x103e28);std::uniform_real_distribution<float> v(-1,1);
 wr(0x4a30f0+0xd8c,uint32_t(0x80000));for(unsigned i=0;i<5;++i)wr(0x80000+0x1030+(i+5)*4,ids[i]);
 for(unsigned t=0;t<40000;++t){memset(ram.data()+0x60000,0,0xd0);ssx::OriginalAnimationCyclePair s;unsigned primary=t%5;s.clips={ids[primary],ids[(primary+1)%5]};s.durations={durations[primary],durations[(primary+1)%5]};s.times={s.durations[0]*(v(rng)+1)*.5f,0};s.rates[0]=v(rng);s.sequenceRate=(v(rng)+1)*1.5f;float amount=t<5?float(int(t)-2)*.5f:v(rng),timeScale=1+v(rng)*.5f;
  for(unsigned i=0;i<2;++i){unsigned at=0x60000+i*28;wr(at+4,s.clips[i]);wr(at+8,s.times[i]);wr(at+12,s.rates[i]);wr(at+16,s.durations[i]);wr(at+20,s.weights[i]);wr(at+24,uint32_t(1));}wr(0x60090,s.sequenceRate);
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),31,0x12345678);SET_GPR_U32((&c),6,0x60000);for(unsigned i=0;i<5;++i)SET_GPR_U32((&c),7+i,i+5);c.f[12]=timeScale;c.f[13]=amount;if(t<20000)sub_00103E28_0x103e28(ram.data(),&c,&rt);else sub_00103CC8_0x103cc8(ram.data(),&c,&rt);if(c.pc!=0x12345678)return 2;
  if(t<20000)ssx::originalFiveWayAnimationStep(s,ids,durations,amount,timeScale);else ssx::originalThreeWayAnimationStep(s,{ids[0],ids[1],ids[2]},{durations[0],durations[1],durations[2]},amount,timeScale);if(s.completed!=bool(ru(0x600c0))){printf("cycle completion mismatch%u\n",t);return 7;}for(unsigned i=0;i<2;++i){unsigned at=0x60000+i*28;if(s.clips[i]!=ru(at+4)||s.times[i]!=rf(at+8)||s.weights[i]!=rf(at+20)){printf("cycle mismatch%u/%u clip%x/%x time%.9g/%.9g weight%.9g/%.9g\n",t,i,s.clips[i],ru(at+4),s.times[i],rf(at+8),s.weights[i],rf(at+20));return 3;}}
 }
 for(unsigned i=0;i<20000;++i){ssx::OriginalAnimationSequence s;s.weight=(v(rng)+1)*.5f;s.targetWeight=i%2?1:0;s.fadeRemaining=i%5?std::abs(v(rng))*.4f:0;s.stopWhenFaded=i%3==0;float dt=.001f+std::abs(v(rng))*.03f,request=.01f+std::abs(v(rng))*.2f;wr(0x60094,s.weight);wr(0x60098,s.targetWeight);wr(0x6009c,s.fadeRemaining);wr(0x600a0,uint32_t(s.stopWhenFaded));
  R5900Context c{};SET_GPR_U32((&c),4,0x60000);SET_GPR_U32((&c),31,0x12345678);
  if(i%2){c.f[12]=request;sub_00313A20_0x313a20(ram.data(),&c,&rt);ssx::originalAnimationFadeOut(s,request);}
  c.f[12]=dt;sub_00313800_0x313800(ram.data(),&c,&rt);bool ended=ssx::originalAnimationFadeStep(s,dt);
  if(s.weight!=rf(0x60094)||s.targetWeight!=rf(0x60098)||s.fadeRemaining!=rf(0x6009c)||uint32_t(ended)!=GPR_U32((&c),2)){printf("fade mismatch%u\n",i);return 4;}
 }

 rt.registerFunction(0x312790,[](uint8_t*,R5900Context*c,PS2Runtime*){c->f[0]=c->f[31];c->pc=GPR_U32(c,31);});
 for(unsigned i=0;i<20000;++i){int id=std::array{268,287,305,245}[i%4],status=i%5;float predicted=std::abs(v(rng))*10,elapsed=predicted+v(rng),duration=.01f+std::abs(v(rng));
  wr(0x70058,uint32_t(0x71000));wr(0x71788,uint32_t(0x72000));wr(0x71784,uint32_t(0x73000));wr(0x720ac,uint32_t(status));wr(0x72098,predicted);wr(0x720a0,elapsed);wr(0x7301c,1.f);
  R5900Context c{};c.pc=0x134b80;SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),16,0x70000);SET_GPR_U32((&c),17,id);SET_GPR_U32((&c),2,0x71000);c.f[31]=duration;
  sub_00133308_0x133308(ram.data(),&c,&rt);auto result=ssx::originalAirLandingAnimation(id,status,predicted,elapsed,duration);
  if(c.pc!=0x12345678||result.semantic!=int(GPR_U32((&c),17))||result.rate!=rf(0x7301c)){printf("air animation anticipation mismatch%u %d/%d %.9g/%.9g\n",i,result.semantic,int(GPR_U32((&c),17)),result.rate,rf(0x7301c));return 5;}
 }
 rt.registerFunction(0x3145F8,[](uint8_t*,R5900Context*c,PS2Runtime*){c->pc=GPR_U32(c,31);});
 for(unsigned i=0;i<20000;++i){ssx::OriginalAnimationSequence s;s.slots.push_back({100,99.f,.1f+std::abs(v(rng))*3,7,1,false,true});s.weight=std::abs(v(rng));s.targetWeight=i%2?1:0;s.fadeRemaining=i%4?std::abs(v(rng)):0;s.stopWhenFaded=i%3==0;
  float spin=v(rng)*2,flip=v(rng)*2,timeScale=std::abs(v(rng))*2;wr(0x70060,uint32_t(0x71000));wr(0x712a4,spin);wr(0x712b0,flip);wr(0x60010,s.slots[0].duration);wr(0x60094,s.weight);wr(0x60098,s.targetWeight);wr(0x6009c,s.fadeRemaining);wr(0x600a0,uint32_t(s.stopWhenFaded));
  R5900Context c{};SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);SET_GPR_U32(&c,4,0x70000);SET_GPR_U32(&c,5,0x73000);SET_GPR_U32(&c,6,0x60000);c.f[12]=timeScale;
  sub_00104358_0x104358(ram.data(),&c,&rt);bool removed=ssx::originalAnimationPrewindStep(s,spin,flip,timeScale);
  if(c.pc!=0x12345678||s.slots[0].time!=rf(0x60008)||s.weight!=rf(0x60094)||s.fadeRemaining!=rf(0x6009c)||removed!=bool(GPR_U32((&c),2))){printf("prewind driver mismatch%u %.9g/%.9g\n",i,s.slots[0].time,rf(0x60008));return 6;}}
 for(unsigned i=0;i<20000;++i){memset(ram.data()+0x60000,0,0xd0);ssx::OriginalAnimationSlot slot;slot.enabled=true;slot.loop=i&1;slot.duration=.1f+std::abs(v(rng))*3;slot.time=v(rng)*slot.duration*2;slot.rate=v(rng)*2;float rate=std::abs(v(rng))*2,dt=.01f+std::abs(v(rng))*.1f;
  wr(0x60008,slot.time);wr(0x6000c,slot.rate);wr(0x60010,slot.duration);wr(0x6001c,uint32_t(slot.loop));wr(0x60090,rate);wr(0x600c0,1u);
  R5900Context c{};SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);SET_GPR_U32(&c,4,0x60000);SET_GPR_U32(&c,5,0);c.f[12]=dt;sub_003135B0_0x3135b0(ram.data(),&c,&rt);bool complete=ssx::originalAnimationSlotStep(slot,rate,dt);
  if(c.pc!=0x12345678||slot.time!=rf(0x60008)||complete!=bool(ru(0x600c0))){printf("slot completion mismatch%u %.9g/%.9g complete%d/%d\n",i,slot.time,rf(0x60008),complete,bool(ru(0x600c0)));return 8;}}
 puts("20,000 original slot clock/completion flags exact;40,000 cycle completion flags exact");
 puts("20,000 original prewind sample clocks/fades exact");
 puts("20,000 original airborne landing-animation choices/rates exact");
 puts("20,000 original sequence fade-in/out stages exact");
 puts("20,000 five-way +20,000 three-way original selections/weights/clocks bit-identical");}
