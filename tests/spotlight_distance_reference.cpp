#include "ps2_runtime_macros.h"
#include "../engine/spotlight_distance.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_0038A6A8_0x38a6a8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;auto put=[&](unsigned at,auto v){std::memcpy(m.data()+at,&v,sizeof(v));};put(0x4a30f0-0x26e4,5000.f);put(0x4a30f0-0x26e0,3750.f);put(0x4a30f0-0x26dc,.0007999999797903001f);std::mt19937 rng(0x38a710);unsigned accepted=0;
for(unsigned n=0;n<20000;++n){ssx::OriginalSpotlightDistance light{float(rng()%10001),float(int(rng()%20001)-10000)/1000.f,float(int(rng()%20001)-10000)/10000.f,int8_t(n%9-3)};ssx::OriginalLocalLightQuery query;query.distance=float(rng()%100000+1)/10.f;query.inverseDistance=1.f/query.distance;query.axisCosine=n%5==0?light.outerCosine:float(int(rng()%20001)-10000)/10000.f;if(n%11==0){light.radius=5000;query.distance=3750;query.inverseDistance=1.f/3750.f;}
put(0x2001c,light.radius);put(0x20014,light.intensity);put(0x20060,light.outerCosine);put(0x20064,light.mode);put(0x10020,query.distance);put(0x10024,query.inverseDistance);R5900Context c{};c.pc=0x38a710;c.f[1]=query.axisCosine;SET_GPR_U32(&c,16,0x20000);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,28,0x4a30f0);{ssx::terrain_original::Rounding round;sub_0038A6A8_0x38a6a8(m.data(),&c,&rt);}auto out=ssx::originalSpotlightDistance(light,query);if(!out){if(c.pc!=0x38abe4)throw std::runtime_error("Spotlight rejection differs");continue;}accepted++;if(c.pc!=0x38a7fc||std::memcmp(&out->intensity,&c.f[0],4)||std::memcmp(&out->fade,&c.f[2],4))throw std::runtime_error("Spotlight distance attenuation differs");}
printf("20000 original spotlight distance stages match; %u accepted, covering outer-cone equality, radius/fade boundaries and distance modes. Angular attenuation remains separate.\n",accepted);}
