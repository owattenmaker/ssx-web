#include "ps2_runtime_macros.h"
#include "../engine/local_light_query.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_0038A530_0x38a530(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;std::mt19937 rng(0x38a530);auto random=[&](){return float(int(rng()%20001)-10000)/13.f;};unsigned accepted=0;
for(unsigned n=0;n<20000;++n){std::array<float,3> point{random(),random(),random()};ssx::OriginalLocalLight light{random(),{random()/1000,random()/1000,random()/1000},{random(),random(),random()}};if(n%5==0)light.position=point;if(n%5==1){point={0,0,0};light.position={light.radius,0,0};}
ssx::OriginalLocalLightQuery out{{random(),random(),random(),random()},random(),random(),random()};std::memcpy(m.data()+0x20000,&point,12);std::memcpy(m.data()+0x3001c,&light.radius,4);std::memcpy(m.data()+0x3002c,&light.axis,12);std::memcpy(m.data()+0x30038,&light.position,12);std::memcpy(m.data()+0x40000,&out,sizeof(out));
R5900Context c{};c.pc=0x38a530;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,6,0x30000);SET_GPR_U32(&c,7,0x40010);SET_GPR_U32(&c,8,0x40014);SET_GPR_U32(&c,9,0x40018);SET_GPR_U32(&c,31,0x12345678);{ssx::terrain_original::Rounding round;sub_0038A530_0x38a530(m.data(),&c,&rt);}bool hit=ssx::originalLocalLightQuery(point,light,out);accepted+=hit;
if(c.pc!=0x12345678||GPR_U32((&c),2)!=unsigned(hit)||std::memcmp(m.data()+0x40000,&out,sizeof(out)))throw std::runtime_error("Original local-light query differs");}
printf("20000 original local-light queries match all output words and rejection preservation; %u accepted, including coincident and exact-radius cases.\n",accepted);}
