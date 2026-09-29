#include "ps2_runtime_macros.h"
#include "../engine/irradiance.hpp"
#include <vector>
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
void sub_00389590_0x389590(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> bank((std::istreambuf_iterator<char>(file)),{});if(bank.size()<4)return 3;uint32_t count;std::memcpy(&count,bank.data(),4);if(count!=45||bank.size()!=4+count*168)return 4;
std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;std::mt19937 rng(0x389590);auto random=[&](){return float(int(rng()%20001)-10000)/371.f;};
for(unsigned n=0;n<20000;++n){ssx::OriginalIrradianceCoefficients source,destination;std::memcpy(&source,bank.data()+4+count*8+(n%count)*160,160);if(n%2)for(auto&row:source)row[3]=random();for(auto&row:destination)for(auto&x:row)x=random();std::array<float,4> color;for(auto&x:color)x=random();float weight=n<3?float(n):random();std::memcpy(m.data()+0x20000,&destination,160);std::memcpy(m.data()+0x30000,&source,160);std::memcpy(m.data()+0x40000,&color,16);
R5900Context c{};c.pc=0x389590;c.f[12]=weight;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x30000);SET_GPR_U32(&c,6,0x40000);SET_GPR_U32(&c,31,0x12345678);
{ssx::terrain_original::Rounding round;sub_00389590_0x389590(m.data(),&c,&rt);}ssx::originalIrradianceAccumulate(destination,source,color,weight);
if(c.pc!=0x12345678||std::memcmp(&destination,m.data()+0x20000,160))throw std::runtime_error("Original irradiance accumulation mismatch");}
puts("20000 original irradiance accumulations match all40 float words, cycling all45 owned coefficient records with varied signed weights/ARGB modulation and synthetic fourth-lane coverage.");}
