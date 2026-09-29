#include "ps2_runtime_macros.h"
#include "../engine/irradiance_angles.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
#include <fstream>
void sub_00389CB8_0x389cb8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C128_0x31c128(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(input)),{}),m(32*1024*1024);std::memcpy(m.data()+0xff000,elf.data(),elf.size());std::array<float,4> axis{1,0,0,0};std::memcpy(m.data()+0x4ff140,&axis,16);PS2Runtime rt;rt.registerFunction(0x31c228,sub_0031C228_0x31c228);rt.registerFunction(0x31c128,sub_0031C128_0x31c128);std::mt19937 rng(0x389e98);
for(unsigned n=0;n<20000;++n){std::array<float,4> direction{float(int(rng()%20001)-10000)/10000.f,float(int(rng()%20001)-10000)/10000.f,float(int(rng()%20001)-10000)/10000.f,0};if(n<2)direction={0,0,n?1.f:-1.f,0};if(n%13==0&&n>1){direction[0]*=.00001f;direction[1]*=.00001f;}
float horizontal;{ssx::terrain_original::Rounding round;horizontal=ssx::terrain_original::sqrt(ssx::terrain_original::add(ssx::terrain_original::mul(direction[0],direction[0]),ssx::terrain_original::mul(direction[1],direction[1])));}
auto flat=direction;flat[2]=0;std::memcpy(m.data()+0x100f0,&flat,16);R5900Context c{};c.pc=0x389e98;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);c.f[12]=direction[2];c.f[20]=horizontal;c.f[21]=-1;c.f[22]=1;SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);{ssx::terrain_original::Rounding round;sub_00389CB8_0x389cb8(m.data(),&c,&rt);}auto result=ssx::originalIrradianceAngles(direction,horizontal);if(c.pc!=0x389f9c||std::memcmp(&result.pitch,&c.f[23],4)||std::memcmp(&result.yaw,&c.f[21],4)){printf("Rim angle mismatch %u (%g,%g) vs (%g,%g)\n",n,result.pitch,result.yaw,c.f[23],c.f[21]);return 1;}}
puts("20000 original rim angle pairs match bits with real atan/asin callees, including vertical, tiny-horizontal and both yaw half-planes.");}
