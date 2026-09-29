#include "ps2_runtime_macros.h"
#include "../engine/irradiance.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
#include <fstream>
void sub_00389308_0x389308(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){if(argc!=2)return 2;std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;std::ifstream constants(argv[1],std::ios::binary);constants.read(reinterpret_cast<char*>(m.data()+0x4a30f0-0x272c),44);if(!constants)return 3;std::mt19937 rng(0x389308);auto random=[&](){return float(int(rng()%20001)-10000)/371.f;};
for(unsigned n=0;n<20000;++n){ssx::OriginalIrradianceCoefficients out;for(auto&row:out)for(auto&v:row)v=random();std::array<float,3> direction,color;for(auto&v:direction)v=random()/27.f;for(auto&v:color)v=random();float weight=n<3?float(n):random();std::memcpy(m.data()+0x20000,&out,160);std::memcpy(m.data()+0x30000,&color,12);
R5900Context c{};c.pc=0x389308;c.f[12]=weight;c.f[13]=direction[0];c.f[14]=direction[1];c.f[15]=direction[2];SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x30000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);{ssx::terrain_original::Rounding round;sub_00389308_0x389308(m.data(),&c,&rt);}ssx::originalIrradianceDirectional(out,direction,color,weight);if(c.pc!=0x12345678||std::memcmp(&out,m.data()+0x20000,160))throw std::runtime_error("Original irradiance directional projection mismatch");}
puts("20000 original directional irradiance projections match all40 coefficient words, including preserved fourth lanes.");}
