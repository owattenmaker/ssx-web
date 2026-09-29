#include "ps2_runtime_macros.h"
#include "../engine/environment_irradiance.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_002ED490_0x2ed490(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00389730_0x389730(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003897A0_0x3897a0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00389810_0x389810(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static float brightness,gain;
int main(){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;auto put=[&](unsigned at,auto value){std::memcpy(m.data()+at,&value,sizeof(value));};put(0x4a30f0-0x3960,.10000000149011612f);
for(unsigned address:{0x2ee318,0x2ee340,0x2ee368})rt.registerFunction(address,[](uint8_t*,R5900Context*c,PS2Runtime*){auto address=c->pc;SET_GPR_U32(c,2,address==0x2ee318?0x20000:address==0x2ee340?0x30000:0x40000);c->pc=GPR_U32(c,31);});
rt.registerFunction(0x2edf00,[](uint8_t*,R5900Context*c,PS2Runtime*){c->f[0]=brightness;c->pc=GPR_U32(c,31);});rt.registerFunction(0x2eeff0,[](uint8_t*,R5900Context*c,PS2Runtime*){c->f[0]=gain;c->pc=GPR_U32(c,31);});rt.registerFunction(0x389730,sub_00389730_0x389730);rt.registerFunction(0x3897a0,sub_003897A0_0x3897a0);rt.registerFunction(0x389810,sub_00389810_0x389810);
std::mt19937 rng(0x2ed92c);auto random=[&](){return float(int(rng()%20001)-10000)/371.f;};
for(unsigned n=0;n<20000;++n){unsigned index=n%6;ssx::OriginalIrradianceCoefficients previous,bright,dark,alternate;for(auto* p:{&previous,&bright,&dark,&alternate})for(auto& row:*p)for(auto& v:row)v=random();float selector=n%3==0?.1f:n%3==1?.10001f:-1.f;brightness=random()/27.f;gain=random()/20.f;float incoming=n<3?float(n):random()/27.f;put(0x20000,bright);put(0x30000,dark);put(0x40000,alternate);put(0x4fa3c0+index*0xf0,previous);put(0x4fa394+index*0xf0,selector);put(0x4a30f0+0xa80,incoming);put(0x102a0,uint64_t(0x12345678));
R5900Context c{};c.pc=0x2ed92c;SET_GPR_U32(&c,19,index);SET_GPR_U32(&c,21,0x500000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);{ssx::terrain_original::Rounding round;sub_002ED490_0x2ed490(m.data(),&c,&rt);}auto result=ssx::originalEnvironmentIrradiance(previous,bright,dark,alternate,selector,brightness,gain,incoming);if(c.pc!=0x12345678||std::memcmp(&result,m.data()+0x4fa3c0+index*0xf0,160))throw std::runtime_error("Original environment irradiance differs");}
puts("20000 original environment irradiance mixes match all40 words across6 rider slots and both branches; bank/brightness/gain getters are controlled boundaries.");}
