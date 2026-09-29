#include "ps2_runtime_macros.h"
#include "../engine/pickup_instance_flags.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_0034FBF0_0x34fbf0(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static std::vector<int> calls;
int main(int,char**){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;uint32_t instance=0x30000;std::memcpy(m.data()+0x20018,&instance,4);
rt.registerFunction(0x34fc80,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x20000||GPR_U32(c,5)!=0x30000)throw std::runtime_error("Unexpected node detach");calls.push_back(1);c->pc=GPR_U32(c,31);});
rt.registerFunction(0x354920,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x20000||GPR_U32(c,5)!=3)throw std::runtime_error("Unexpected base destruction");calls.push_back(2);c->pc=GPR_U32(c,31);});
std::mt19937 rng(0x34fbf0);for(int i=0;i<20000;i++){uint32_t before=i?uint32_t(rng()):0x210004;std::memcpy(m.data()+0x30008,&before,4);R5900Context c{};c.pc=0x34fbf0;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,3);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);calls.clear();sub_0034FBF0_0x34fbf0(m.data(),&c,&rt);uint32_t after;std::memcpy(&after,m.data()+0x30008,4);if(c.pc!=0x12345678||calls!=std::vector<int>{1,2}||after!=ssx::originalPickupRestoredFlags(before))return 1;}
puts("20000 original instance flag restorations match, including authored high-half sign extension");}
