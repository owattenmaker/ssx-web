#include "ps2_runtime_macros.h"
#include "../engine/animation_events.hpp"
#include "../engine/original_float.hpp"
#include <fstream>
#include <cstring>
#include <random>
#include <cstdio>
#include <cfenv>
#include <bit>
void sub_003135B0_0x3135b0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313868_0x313868(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313938_0x313938(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003139A8_0x3139a8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x340000,g_ps2RecompiledFunctionTableSlotCount=0x90000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x90000]={};

int main(int argc,char**argv){
 if(argc!=2)return 2;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(in)),{});if(m.size()!=32*1024*1024)return 3;
 PS2Runtime rt;rt.registerFunction(0x313868,sub_00313868_0x313868);rt.registerFunction(0x313938,sub_00313938_0x313938);rt.registerFunction(0x3139a8,sub_003139A8_0x3139a8);
 auto put=[&](unsigned at,const auto&v){memcpy(m.data()+at,&v,sizeof(v));};auto get=[&](unsigned at){uint64_t v;memcpy(&v,m.data()+at,8);return v;};
 std::mt19937 gen(0x3135b0);std::uniform_real_distribution<float> unit(-1,1);
 constexpr unsigned seq=0x30000,nodes=0x40000,done=0x12345678;
 for(unsigned i=0;i<30000;++i){std::fesetround(FE_TOWARDZERO);ssx::OriginalAnimationSlot slot;slot.enabled=true;slot.loop=i&1;slot.duration=.1f+std::abs(unit(gen))*3;slot.time=unit(gen)*slot.duration*2;slot.rate=unit(gen)*10;float rate=unit(gen)*10,dt=.01f+std::abs(unit(gen));
  ssx::OriginalAnimationEventFlags flags;flags.latched=gen();flags.raised=gen();flags.completed=i&1;flags.seekPending=i%3==0;
  std::vector<ssx::OriginalAnimationEventMarker> events;
  for(unsigned n=0;n<9;++n){float time=n==0?0:n==1?slot.time:n==2?slot.duration:std::abs(unit(gen))*slot.duration;events.push_back({time,n==8?63:n,n==8});}
  put(seq+8,slot.time);put(seq+12,slot.rate);put(seq+16,slot.duration);put(seq+28,uint32_t(slot.loop));put(seq+0x90,rate);put(seq+0xac,nodes);put(seq+0xb0,flags.latched);put(seq+0xb8,flags.raised);put(seq+0xc0,uint32_t(flags.completed));put(seq+0xc4,uint32_t(flags.seekPending));
  for(unsigned n=0;n<events.size();++n){unsigned at=nodes+n*16;put(at,events[n].bit);put(at+4,uint32_t(events[n].end));put(at+8,events[n].time);put(at+12,n+1<events.size()?at+16:0u);}
  R5900Context c{};c.pc=0x3135b0;SET_GPR_U32((&c),4,seq);SET_GPR_U32((&c),5,0);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),31,done);c.f[12]=dt;sub_003135B0_0x3135b0(m.data(),&c,&rt);
  ssx::originalAnimationPrimaryStep(slot,rate,dt,flags,events);uint32_t clock;memcpy(&clock,m.data()+seq+8,4);
  if(c.pc!=done||flags.latched!=get(seq+0xb0)||flags.raised!=get(seq+0xb8)||uint32_t(flags.completed)!=uint32_t(get(seq+0xc0))||flags.seekPending!=bool(get(seq+0xc4)&0xffffffffu)||std::bit_cast<uint32_t>(slot.time)!=clock){printf("Event mismatch%u loop%d flags%llx/%llx raised%llx/%llx time%x/%x\n",i,slot.loop,(unsigned long long)flags.latched,(unsigned long long)get(seq+0xb0),(unsigned long long)flags.raised,(unsigned long long)get(seq+0xb8),std::bit_cast<uint32_t>(slot.time),clock);return 4;}
 }
 puts("30,000 full original3135B0 primary clocks/timed events/end events/seek flags exact, including forward/reverse and multiple wraps");
}
