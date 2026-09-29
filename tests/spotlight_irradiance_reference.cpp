#include "ps2_runtime_macros.h"
#include "../engine/spotlight_irradiance.hpp"
#include <vector>
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
void sub_0038A6A8_0x38a6a8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0038A530_0x38a530(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00389520_0x389520(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00389308_0x389308(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(file)),{}),m(32*1024*1024);std::memcpy(m.data()+0xff000,elf.data(),elf.size());PS2Runtime rt;rt.registerFunction(0x38a530,sub_0038A530_0x38a530);rt.registerFunction(0x389520,sub_00389520_0x389520);rt.registerFunction(0x389308,sub_00389308_0x389308);auto put=[&](unsigned at,auto v){std::memcpy(m.data()+at,&v,sizeof(v));};std::mt19937 rng(0x38a6a8);auto random=[&](){return float(int(rng()%20001)-10000)/13.f;};
for(unsigned n=0;n<12000;++n){std::array<float,3> point{random(),random(),random()};ssx::OriginalSpotlight light;light.geometry={float(rng()%7001),{random()/770,random()/770,random()/770},{random(),random(),random()}};light.intensity=random()/100;light.outerCosine=-.8f;light.innerCosine=.6f;light.distanceMode=int8_t(n%6-1);light.angularMode=int8_t(n%12-3);for(auto&v:light.color)v=random()/770;if(n%7==0)light.geometry.position=point;ssx::OriginalIrradianceCoefficients bank;for(auto&row:bank)for(auto&v:row)v=random()/770;
put(0x20010,1u);put(0x20014,light.intensity);put(0x2001c,light.geometry.radius);put(0x20020,light.color);put(0x2002c,light.geometry.axis);put(0x20038,light.geometry.position);put(0x2005c,light.innerCosine);put(0x20060,light.outerCosine);put(0x20064,light.distanceMode);put(0x20065,light.angularMode);put(0x30000,bank);put(0x40000,point);
R5900Context c{};c.pc=0x38a6a8;SET_GPR_U32(&c,4,0x30000);SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,6,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);{ssx::terrain_original::Rounding round;sub_0038A6A8_0x38a6a8(m.data(),&c,&rt);}ssx::originalSpotlightIrradiance(bank,point,light);if(c.pc!=0x12345678||std::memcmp(&bank,m.data()+0x30000,160))throw std::runtime_error("Complete spotlight contribution mismatch");}
puts("12000 complete original kind1 local-light contributions match all40 irradiance words using real query and directional-projection callees.");}
