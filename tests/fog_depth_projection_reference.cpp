#include "ps2_runtime_macros.h"
#include "../engine/fog_depth_table.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_00376938_0x376938(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;std::mt19937 rng(0x376ea4);auto put=[&](uint32_t at,float x){std::memcpy(m.data()+at,&x,4);};
for(unsigned n=0;n<20000;++n){uint32_t format=n%2?0x31:0x32;float nearCm=float(rng()%30000+1)/10.f,farCm=nearCm+float(rng()%100000+1),depthOffset=float(int(rng()%20001)-10000);if(n%7==0)std::swap(nearCm,farCm);
R5900Context c{};c.pc=0x376e98;c.f[20]=format==0x31?16777215.f:65535.f;c.f[23]=farCm;c.f[25]=nearCm;SET_GPR_U32(&c,16,0x30000);SET_GPR_U32(&c,18,0x20000);SET_GPR_U32(&c,29,0x10000);put(0x2001c,depthOffset);
{ssx::terrain_original::Rounding round;sub_00376938_0x376938(m.data(),&c,&rt);}auto p=ssx::originalFogDepthProjection(nearCm,farCm,depthOffset,format);
if(c.pc!=0x376f74||std::memcmp(m.data()+0x36b18,&p.slope,4)||std::memcmp(m.data()+0x36b28,&p.offset,4))throw std::runtime_error("Original fog depth coefficients differ");}
puts("20000 original reverse-depth coefficient constructions match slope/offset bits for16/24-bit ranges, reversed planes and viewport offsets; range selection is source-audited separately.");}
