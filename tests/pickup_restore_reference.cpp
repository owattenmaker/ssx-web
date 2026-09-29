#include "ps2_runtime_macros.h"
#include "../engine/pickup_instance_flags.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_00350F60_0x350f60(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int,char**){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;
rt.registerFunction(0x34fb00,[](uint8_t*m,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x20000||GPR_U32(c,5)!=1||GPR_U32(c,6)!=19||GPR_U32(c,7)!=0x30000)throw std::runtime_error("Unexpected RestoreNode base ctor");uint32_t instance=0x30000;std::memcpy(m+0x20018,&instance,4);c->pc=GPR_U32(c,31);});
std::mt19937 rng(0x350f60);for(int i=0;i<20000;i++){uint32_t before=rng();std::memcpy(m.data()+0x30008,&before,4);R5900Context c{};c.pc=0x350f60;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x30000);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_00350F60_0x350f60(m.data(),&c,&rt);uint32_t after,vtable;std::memcpy(&after,m.data()+0x30008,4);std::memcpy(&vtable,m.data()+0x2000c,4);if(c.pc!=0x12345678||GPR_U32((&c),2)!=0x20000||after!=ssx::originalPickupRestorePendingFlags(before)||vtable!=0x491680)return 1;}
puts("20000 original RestoreNode flag transitions match");}
