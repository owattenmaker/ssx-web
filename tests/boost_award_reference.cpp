#include "ps2_runtime_macros.h"
#include "../engine/boost_award.hpp"
#include <bit>
#include <cfenv>
#include <fstream>
#include <random>
#include <cstdio>
void sub_0010E098_0x10e098(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x410000,g_ps2RecompiledFunctionTableSlotCount=0xc4000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc4000]={nullptr};
static std::vector<unsigned> events;
static float callbackMeter,callbackDelta;
int main(int argc,char**argv){
 if(argc!=2)return 1;
 std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> memory((std::istreambuf_iterator<char>(file)),{});if(memory.size()!=32*1024*1024)return 2;
 PS2Runtime runtime;
 for(auto address:{0x14dc80,0x14dd58,0x28b180})runtime.registerFunction(address,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0x90000);c->pc=GPR_U32(c,31);});
 runtime.registerFunction(0x149690,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back(1);c->pc=GPR_U32(c,31);});
 runtime.registerFunction(0x29ab08,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back(2);callbackMeter=c->f[12];callbackDelta=c->f[13];c->pc=GPR_U32(c,31);});
 runtime.registerFunction(0x10e028,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back(GPR_U32(c,5));c->pc=GPR_U32(c,31);});
 auto write=[&](unsigned at,auto value){std::memcpy(memory.data()+at,&value,sizeof(value));};
 auto bits=[&](unsigned at){unsigned value;std::memcpy(&value,memory.data()+at,4);return value;};
 constexpr unsigned rider=0x14701a0;std::mt19937 rng(0x10e098);std::uniform_real_distribution<float> value(-2,2);std::fesetround(FE_TOWARDZERO);
 for(unsigned n=0;n<30000;n++){
  ssx::OriginalBoostState s;s.meter=value(rng);s.tier=int(n%15)-3;s.drainEnabled=n%5;s.superTime=n%3?value(rng)*15:0;
  float delta=n%7?value(rng):0; if(n%11==0){s.meter=.75f;delta=.25f;} if(n%13==0){s.meter=.25f;delta=-.25f;}
  ssx::OriginalBoostAwardContext a{uint32_t(n%8),bool(n%2)};uint32_t category=1u<<(n%4);
  write(rider+0x2f8,s.meter);write(rider+0x2f4,s.tier);write(rider+0x304,s.drainEnabled);write(rider+0x2f0,s.superTime);write(rider+0xb28,a.rewardMask);write(rider+0xb2c,uint32_t(a.enableTricky));
  events.clear();R5900Context c{};c.pc=0x10e098;SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);SET_GPR_U32(&c,4,rider);SET_GPR_U32(&c,5,category);c.f[12]=delta;
  sub_0010E098_0x10e098(memory.data(),&c,&runtime);if(c.pc!=0x12345678)return 3;
  auto effect=ssx::originalBoostAward(s,a,delta,category);std::vector<unsigned> actual;
  if(effect.positiveRewardEvent)actual.push_back(1);if(effect.meterChangeEvent)actual.push_back(2);if(effect.fullMeterNotification)actual.push_back(5);
  if(bits(rider+0x2f8)!=std::bit_cast<unsigned>(s.meter)||bits(rider+0x2f4)!=uint32_t(s.tier)||bits(rider+0x2f0)!=std::bit_cast<unsigned>(s.superTime)||actual!=events){printf("Award mismatch %u meter %08x/%08x tier%d/%d timer%08x/%08x\n",n,bits(rider+0x2f8),std::bit_cast<unsigned>(s.meter),int(bits(rider+0x2f4)),s.tier,bits(rider+0x2f0),std::bit_cast<unsigned>(s.superTime));return 4;}
  if(effect.meterChangeEvent&&(std::bit_cast<unsigned>(callbackMeter)!=std::bit_cast<unsigned>(effect.previousMeter)||std::bit_cast<unsigned>(callbackDelta)!=std::bit_cast<unsigned>(effect.delta)))return 5;
 }
 puts("30000 original10E098 award cases match: meter bits, eligibility masks, tier/timer transitions, callback arguments and order.");
}
