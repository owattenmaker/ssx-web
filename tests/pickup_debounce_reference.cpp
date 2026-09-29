#include "ps2_runtime_macros.h"
#include "../engine/pickup_debounce.hpp"
#include <vector>
#include <cstring>
#include <cstdio>
void sub_00342D10_0x342d10(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static bool called;
int main(int,char**){std::vector<uint8_t> m(32*1024*1024);auto put=[&](unsigned at,uint32_t v){std::memcpy(m.data()+at,&v,4);};PS2Runtime rt;put(0x2000c,0x30000);put(0x30114,0x123400);
rt.registerFunction(0x123400,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x20000||GPR_U32(c,5)!=1)throw std::runtime_error("Wrong debounce finish request");called=true;c->pc=GPR_U32(c,31);});
for(int n=-2;n<20000;n++){int32_t ticks=n,expected=ticks;put(0x2002c,uint32_t(ticks));R5900Context c{};c.pc=0x342d88;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);called=false;sub_00342D10_0x342d10(m.data(),&c,&rt);std::memcpy(&ticks,m.data()+0x2002c,4);bool done=ssx::originalPickupDebounceTick(expected);if(c.pc!=0x12345678||ticks!=expected||called!=done||GPR_U32((&c),2)!=unsigned(!done))return 1;}
puts("20002 original debounce countdown cases and completion requests match");}
