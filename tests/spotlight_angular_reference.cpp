#include "ps2_runtime_macros.h"
#include "../engine/spotlight_angular.hpp"
#include <vector>
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
void sub_0038A6A8_0x38a6a8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(file)),{}),m(32*1024*1024);std::memcpy(m.data()+0xff000,elf.data(),elf.size());PS2Runtime rt;std::mt19937 rng(0x38a7fc);constexpr std::array<int8_t,11> modes{-128,-7,-1,0,1,2,3,4,5,12,127};unsigned innerCases=0;
for(unsigned n=0;n<22000;++n){float cosine=float(int(rng()%20001)-10000)/10000.f,inner=float(int(rng()%20001)-10000)/10000.f,outer=inner-.3f,radial=float(int(rng()%20001)-10000)/1000.f;int8_t mode=modes[n%modes.size()];if(n%17==0)cosine=inner;if(n%19==0)inner=cosine=0;innerCases+=inner<=cosine;std::memcpy(m.data()+0x20065,&mode,1);
R5900Context c{};c.pc=0x38a7fc;c.f[0]=radial;c.f[2]=1;c.f[3]=inner;c.f[7]=cosine;c.f[8]=outer;SET_GPR_U32(&c,16,0x20000);SET_GPR_U32(&c,29,0x10000);{ssx::terrain_original::Rounding round;sub_0038A6A8_0x38a6a8(m.data(),&c,&rt);}float expected=ssx::originalSpotlightAngular(radial,cosine,outer,inner,mode);if(c.pc!=0x38aa74||std::memcmp(&expected,&c.f[5],4)){printf("Angular mismatch %u mode%d cos%g inner%g native%g source%g\n",n,int(mode),cosine,inner,expected,c.f[5]);return 1;}}
printf("22000 original spotlight angular weights match bits (%u inner-cone cases), including signed exponents, zero/equality branches and clamp.\n",innerCases);}
