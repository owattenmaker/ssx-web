#include "ps2_runtime_macros.h"
#include "../engine/lighting_painter.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_002BCBD0_0x2bcbd0(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static unsigned hooks;
int main(){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;auto put=[&](uint32_t at,auto v){std::memcpy(m.data()+at,&v,sizeof(v));};put(0x20004,0x40000u);put(0x40218,0u);put(0x4021c,0x2bda58u);
rt.registerFunction(0x2bda58,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x20000)throw std::runtime_error("Lighting hook owner");++hooks;c->pc=GPR_U32(c,31);});
std::mt19937 rng(0x2bd5a8);auto value=[&](){return float(int(rng()%20001)-10000)/37.f;};
for(unsigned n=0;n<20000;++n){ssx::OriginalLightingPainterState state;std::array<ssx::OriginalLightingReference,4> refs;std::array<float,2> values;float rate=value(),weight=n<3?float(n):value()/100.f;put(0x20000,rate);
for(unsigned i=0;i<4;++i){state.references[i]={rng(),rng()};state.previousReferences[i]={rng(),rng()};refs[i]={rng(),rng()};put(0x20008+i*16,state.references[i]);put(0x20010+i*16,state.previousReferences[i]);put(0x30004+i*8,refs[i]);}
for(unsigned i=0;i<2;++i){state.values[i]=value();state.samples[i]=value();values[i]=value();put(0x20048+i*8,state.values[i]);put(0x2004c+i*8,state.samples[i]);put(0x30024+i*4,values[i]);}
R5900Context c{};c.pc=0x2bd5a8;c.f[12]=weight;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x30000);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);hooks=0;{ssx::terrain_original::Rounding round;sub_002BCBD0_0x2bcbd0(m.data(),&c,&rt);}ssx::originalLightingPainterBlend(state,refs,values,weight);
if(c.pc!=0x12345678||hooks!=2||std::memcmp(m.data()+0x20000,&rate,4))throw std::runtime_error("Lighting lifecycle mismatch");
for(unsigned i=0;i<4;++i)if(std::memcmp(m.data()+0x20008+i*16,&state.references[i],8)||std::memcmp(m.data()+0x20010+i*16,&state.previousReferences[i],8))throw std::runtime_error("Lighting references mismatch");
for(unsigned i=0;i<2;++i)if(std::memcmp(m.data()+0x20048+i*8,&state.values[i],4)||std::memcmp(m.data()+0x2004c+i*8,&state.samples[i],4))throw std::runtime_error("Lighting scalar mismatch");}
puts("20000 original Lighting painter blends match reference copies, preserved slots, scalar bits and both empty hooks.");}
