#include "ps2_runtime_macros.h"
#include "../engine/pickup_reward.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_0010E5D8_0x10e5d8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static std::vector<int> calls;static int mode;
int main(int,char**){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;
rt.registerFunction(0x28b180,[](uint8_t*,R5900Context*c,PS2Runtime*){calls.push_back(0);SET_GPR_U32(c,2,0x30000);c->pc=GPR_U32(c,31);});
rt.registerFunction(0x29ced8,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x30000||GPR_U32(c,5)!=unsigned(mode)||GPR_U32(c,6)!=0x20000||c->f[12]!=1)throw std::runtime_error("Wrong pickup feedback");calls.push_back(1);c->pc=GPR_U32(c,31);});
rt.registerFunction(0x2a3b18,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x30000||GPR_U32(c,5)!=0x20000||GPR_U32(c,6)!=unsigned(mode+1))throw std::runtime_error("Wrong pickup FX");calls.push_back(2);c->pc=GPR_U32(c,31);});
std::mt19937 rng(0x10e770);std::uniform_real_distribution<float> random(-100,100);
for(mode=0;mode<2;mode++)for(unsigned i=0;i<20000;i++){float before=random(rng),amount=i%2?5:random(rng);unsigned at=0x202e8+mode*4;std::memcpy(m.data()+at,&before,4);R5900Context c{};c.pc=mode?0x10e7d0:0x10e770;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);c.f[12]=amount;calls.clear();{ssx::OriginalRounding round;sub_0010E5D8_0x10e5d8(m.data(),&c,&rt);}float actual;std::memcpy(&actual,m.data()+at,4);auto expected=ssx::originalPickupCounterAward(before,amount);if(c.pc!=0x12345678||std::bit_cast<unsigned>(actual)!=std::bit_cast<unsigned>(expected)||calls!=std::vector<int>{0,1,0,2})return 1;}
puts("40000 original pickup counter awards and ordered feedback/FX requests match");}
