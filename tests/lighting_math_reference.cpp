#include "ps2_runtime_macros.h"
#include "../engine/generated/lighting_math.hpp"
#include <vector>
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
void sub_0040E550_0x40e550(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0040E048_0x40e048(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(file)),{}),m(32*1024*1024);std::memcpy(m.data()+0xff000,elf.data(),elf.size());PS2Runtime rt;std::mt19937 rng(0x2edf00);
for(unsigned kind=0;kind<2;++kind)for(unsigned n=0;n<20000;++n){float input=kind?-float(rng()%300001)/10000.f:std::bit_cast<float>(((100u+rng()%28)<<23)|(rng()&0x7fffff));if(n==0)input=kind?0.f:1.f;R5900Context c{};c.pc=kind?0x40e048:0x40e550;c.f[12]=input;SET_GPR_U32(&c,31,0x12345678);{ssx::terrain_original::Rounding round;if(kind)sub_0040E048_0x40e048(m.data(),&c,&rt);else sub_0040E550_0x40e550(m.data(),&c,&rt);}float actual=kind?ssx::originalLightingExp(input):ssx::originalLightingLog(input);if(c.pc!=0x12345678||std::memcmp(&actual,&c.f[0],4)){printf("Math mismatch kind%u case%u input%g expected%g actual%g\n",kind,n,input,c.f[0],actual);return 1;}}
puts("20000 original log and20000 exp kernel results match bits in the lighting domain; libc errno/error wrappers are outside this test.");}
