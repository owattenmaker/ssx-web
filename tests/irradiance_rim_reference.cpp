#include "ps2_runtime_macros.h"
#include "../engine/irradiance_rim.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
#include <fstream>
void sub_00389CB8_0x389cb8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00389840_0x389840(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00389620_0x389620(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(file)),{});float rimScale;std::memcpy(&rimScale,elf.data()+0x4a30f0-0x26e8-0xff000,4);
std::vector<uint8_t> m(32*1024*1024);std::memcpy(m.data()+0x4a30f0-0x26e8,&rimScale,4);PS2Runtime rt;rt.registerFunction(0x389840,sub_00389840_0x389840);rt.registerFunction(0x389620,sub_00389620_0x389620);std::mt19937 rng(0x38a4d4);auto random=[&](){return float(int(rng()%20001)-10000)/371.f;};
for(unsigned n=0;n<20000;++n){ssx::OriginalIrradianceCoefficients out,shape;for(auto&row:out)for(auto&v:row)v=random();for(auto&row:shape)for(auto&v:row)v=random();std::array<float,9> matrix;for(auto&v:matrix)v=random()/27.f;if(n%7==0)matrix={1,0,0,0,1,0,0,0,1};float scale=n<3?float(n):random();std::memcpy(m.data()+0x30000,&out,160);std::memcpy(m.data()+0x10000,&shape,160);std::memcpy(m.data()+0x101a0,&matrix,36);uint64_t done=0x12345678;std::memcpy(m.data()+0x10230,&done,8);
R5900Context c{};c.pc=0x38a4d4;c.f[24]=scale;SET_GPR_U32(&c,4,0x10000);SET_GPR_U32(&c,5,0x101a0);SET_GPR_U32(&c,6,3);SET_GPR_U32(&c,22,0x30000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);{ssx::terrain_original::Rounding round;sub_00389CB8_0x389cb8(m.data(),&c,&rt);}ssx::originalIrradianceRimCompose(out,shape,matrix,scale,rimScale);if(c.pc!=done||std::memcmp(&out,m.data()+0x30000,160))throw std::runtime_error("Original irradiance rim composition differs");}
puts("20000 original rim composition tails match all40 words using real transform/add callbacks and the executable's rim scale. View-direction/shape construction is outside this test.");}
