#include "ps2_runtime_macros.h"
#include "../engine/generated/irradiance_transform.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
#include <fstream>
void sub_00389840_0x389840(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> bank((std::istreambuf_iterator<char>(file)),{});uint32_t count;if(bank.size()<4)return 3;std::memcpy(&count,bank.data(),4);if(count!=45||bank.size()!=4+count*168)return 4;
std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;std::mt19937 rng(0x389840);auto random=[&](){return float(int(rng()%20001)-10000)/371.f;};
for(unsigned n=0;n<20000;++n){ssx::OriginalIrradianceCoefficients out;std::memcpy(&out,bank.data()+4+count*8+n%count*160,160);if(n%2)for(auto&row:out)for(auto&v:row)v=random();std::array<float,9> matrix;for(auto&v:matrix)v=random()/27.f;if(n%7==0)matrix={1,0,0,0,1,0,0,0,1};unsigned first=rng()%4,last=rng()%4;std::memcpy(m.data()+0x20000,&out,160);std::memcpy(m.data()+0x30000,&matrix,36);
R5900Context c{};for(auto&value:c.f)value=random();c.pc=0x389840;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x30000);SET_GPR_U32(&c,6,first);SET_GPR_U32(&c,7,last);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);{ssx::terrain_original::Rounding round;sub_00389840_0x389840(m.data(),&c,&rt);}ssx::originalIrradianceTransform(out,matrix,first,last);if(c.pc!=0x12345678||std::memcmp(&out,m.data()+0x20000,160))throw std::runtime_error("Original irradiance transform mismatch");}
puts("20000 original irradiance matrix transforms match all40 words across real/synthetic coefficients, channel ranges, empty ranges and identity/general matrices.");}
