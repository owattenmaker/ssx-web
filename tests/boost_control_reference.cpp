#include "ps2_runtime_macros.h"
#include "../engine/boost_control.hpp"
#include <bit>
#include <cfenv>
#include <fstream>
#include <random>
#include <cstdio>
void sub_001200D0_0x1200d0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00114130_0x114130(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002F6AC8_0x2f6ac8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x410000,g_ps2RecompiledFunctionTableSlotCount=0xc4000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc4000]={nullptr};
static std::vector<unsigned> events;static unsigned startPressed=0;static int motionMode=0,controlMode=0;
int main(int argc,char**argv){
 if(argc!=2)return 1;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> memory((std::istreambuf_iterator<char>(file)),{});if(memory.size()!=32*1024*1024)return 2;
 PS2Runtime runtime;auto write=[&](unsigned at,auto value){std::memcpy(memory.data()+at,&value,sizeof(value));};auto bits=[&](unsigned at){unsigned value;std::memcpy(&value,memory.data()+at,4);return value;};
 runtime.registerFunction(0x2f6ac8,sub_002F6AC8_0x2f6ac8);
 runtime.registerFunction(0x28b180,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0x90000);c->pc=GPR_U32(c,31);});
 runtime.registerFunction(0x298d90,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back(1);startPressed=GPR_U32(c,6);c->pc=GPR_U32(c,31);});
 runtime.registerFunction(0x299368,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back(2);c->pc=GPR_U32(c,31);});
 runtime.registerFunction(0x2992d8,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back(3);c->pc=GPR_U32(c,31);});
 runtime.registerFunction(0x11fe98,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_S32(c,2,motionMode);c->pc=GPR_U32(c,31);});
 runtime.registerFunction(0x11fee8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_S32(c,2,controlMode);c->pc=GPR_U32(c,31);});
 runtime.registerFunction(0x140bc8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0);c->pc=GPR_U32(c,31);});
 runtime.registerFunction(0x10e028,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back(GPR_U32(c,5));c->pc=GPR_U32(c,31);});
 constexpr unsigned rider=0x14701a0,owner=0x146f390,gp=0x4a30f0;std::mt19937 rng(0x114130);std::uniform_real_distribution<float> values(-.2f,1.2f);std::fesetround(FE_TOWARDZERO);
 unsigned starts=0,stops=0,denials=0;
 for(unsigned n=0;n<30000;n++){
  ssx::OriginalBoostProfile p{values(rng),values(rng),.00150679936632514f};
  ssx::OriginalBoostState state{values(rng),n%3?0.f:values(rng),n%3?0.f:values(rng),int(rng()%14)-2,int(rng()%2),uint8_t(rng())};
  if(n%10==0)state.meter=p.fullThreshold;if(n%10==1)state.meter=p.mediumThreshold;if(n%10==2)state.meter=0;
  bool held=n&1,pressed=n&2;auto expected=state;
  write(rider+0x77c,owner);write(rider+0x2f8,state.meter);write(rider+0x2fc,state.amount);write(rider+0x2e8,state.window);write(rider+0x2f4,state.tier);write(rider+0x304,state.drainEnabled);write(owner+0xd27,state.feedbackFlags);
  write(gp-0x7bfc,p.fullThreshold);write(gp-0x7bf8,p.mediumThreshold);write(gp-0x7bf4,p.drainPerTick);events.clear();startPressed=0;
  R5900Context context{};SET_GPR_U32(&context,28,gp);SET_GPR_U32(&context,29,0x10000);SET_GPR_U32(&context,31,0x12345678);SET_GPR_U32(&context,4,rider);SET_GPR_U32(&context,5,held);SET_GPR_U32(&context,6,pressed);context.pc=0x114130;
  sub_00114130_0x114130(memory.data(),&context,&runtime);if(context.pc!=0x12345678){printf("Unexpected boost continuation %x\n",context.pc);return 3;}
  auto result=ssx::originalBoostControl(expected,p,held,pressed);std::vector<unsigned> nativeEvents;if(result.started)nativeEvents.push_back(1);if(result.denied)nativeEvents.push_back(2);if(result.stopped)nativeEvents.push_back(3);
  if(bits(rider+0x2f8)!=std::bit_cast<unsigned>(expected.meter)||bits(rider+0x2fc)!=std::bit_cast<unsigned>(expected.amount)||memory[owner+0xd27]!=expected.feedbackFlags||events!=nativeEvents||(result.started&&startPressed!=unsigned(pressed))){printf("Boost mismatch %u meter %08x/%08x amount %08x/%08x flags %u/%u events %zu/%zu\n",n,bits(rider+0x2f8),std::bit_cast<unsigned>(expected.meter),bits(rider+0x2fc),std::bit_cast<unsigned>(expected.amount),memory[owner+0xd27],expected.feedbackFlags,events.size(),nativeEvents.size());return 4;}
  starts+=result.started;stops+=result.stopped;denials+=result.denied;
 }
 for(unsigned n=0;n<30000;n++){
  ssx::OriginalBoostProfile p{};p.tickSeconds=1.f/60;p.modifierThreshold=1.f/60;p.superTimerFloor=1.f/60;p.normalDecay=.005f;p.fastDecay=.02398611046373844f;
  float timeScale=n%6?.125f*float(n%15):0.f;
  ssx::OriginalBoostState state{};state.window=values(rng);state.modifier=values(rng);state.superTime=n%4?.05f*float(n%100):values(rng);state.meter=values(rng);state.tier=int(n%13);state.drainEnabled=int(n%5);
  if(n%8==0)state.superTime=p.superTimerFloor;if(n%8==1)state.superTime=timeScale*p.tickSeconds;
  motionMode=n%4;controlMode=n%14;auto expected=state;
  write(rider+0x2e8,state.window);write(rider+0x2ec,state.modifier);write(rider+0x2f0,state.superTime);write(rider+0x2f8,state.meter);write(rider+0x2f4,state.tier);write(rider+0x304,state.drainEnabled);write(rider+0x300,timeScale);
  write(gp-0x7958,p.tickSeconds);write(gp-0x7954,p.modifierThreshold);write(gp-0x7950,p.superTimerFloor);write(gp-0x794c,p.normalDecay);write(gp-0x7948,p.fastDecay);events.clear();
  R5900Context context{};SET_GPR_U32(&context,28,gp);SET_GPR_U32(&context,29,0x10000);SET_GPR_U32(&context,31,0x12345678);SET_GPR_U32(&context,4,rider);context.pc=0x1200d0;
  sub_001200D0_0x1200d0(memory.data(),&context,&runtime);if(context.pc!=0x12345678){printf("Unexpected boost tick continuation %x\n",context.pc);return 5;}
  auto effect=ssx::originalBoostTick(expected,p,timeScale,motionMode,controlMode);
  if(bits(rider+0x2e8)!=std::bit_cast<unsigned>(expected.window)||bits(rider+0x2ec)!=std::bit_cast<unsigned>(expected.modifier)||bits(rider+0x2f0)!=std::bit_cast<unsigned>(expected.superTime)||bits(rider+0x2f8)!=std::bit_cast<unsigned>(expected.meter)||bits(rider+0x2f4)!=unsigned(expected.tier)||events!=(effect.timerExpired?std::vector<unsigned>{6}:std::vector<unsigned>{})){printf("Boost tick mismatch %u mode %d ctrl %d super %08x/%08x meter %08x/%08x\n",n,motionMode,controlMode,bits(rider+0x2f0),std::bit_cast<unsigned>(expected.superTime),bits(rider+0x2f8),std::bit_cast<unsigned>(expected.meter));return 6;}
 }
 printf("30,000 original boost meter/timer ticks exact across all four motion modes and control0..13\n");
 printf("30,000 original boost controller cases exact: %u starts, %u stops, %u denials; meter, amount, feedback flags and callback order\n",starts,stops,denials);
}
