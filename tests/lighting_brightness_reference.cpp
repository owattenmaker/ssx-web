#include "ps2_runtime_macros.h"
#include "../engine/lighting_brightness.hpp"
#include <vector>
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
void sub_002EDF00_0x2edf00(uint8_t*,R5900Context*,PS2Runtime*);
#define DECL(N,L) void sub_##N##_0x##L(uint8_t*,R5900Context*,PS2Runtime*);
DECL(0040DA10,40da10) DECL(0040D758,40d758) DECL(0040E550,40e550) DECL(0040E048,40e048) DECL(0040F9D0,40f9d0) DECL(0040F8C0,40f8c0)
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){if(argc!=2&&argc!=4)return 2;std::ofstream inputs,expected;if(argc==4){inputs.open(argv[2],std::ios::binary);expected.open(argv[3],std::ios::binary);if(!inputs||!expected)return 3;}std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(file)),{}),m(32*1024*1024);std::memcpy(m.data()+0xff000,elf.data(),elf.size());PS2Runtime rt;
#define REG(N,L) rt.registerFunction(0x##L,sub_##N##_0x##L);
REG(0040DA10,40da10) REG(0040D758,40d758) REG(0040E550,40e550) REG(0040E048,40e048) REG(0040F9D0,40f9d0) REG(0040F8C0,40f8c0)
std::mt19937 rng(0x2edf00);unsigned below=0,above=0,middle=0;
for(unsigned n=0;n<20000;++n){std::array<float,4> color;for(auto&x:color)x=float(int(rng()%16001)-3000)/10000.f;if(n%2==0)color[1]=color[2]=color[3]=float(n%5001)/10000.f;if(n<4)color[1]=color[2]=color[3]=n%2?.45f:.1f;std::memcpy(m.data()+0x20000,&color,16);R5900Context c{};c.pc=0x2edf00;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);{ssx::terrain_original::Rounding round;sub_002EDF00_0x2edf00(m.data(),&c,&rt);}float actual=ssx::originalLightingBrightness(color);if(c.pc!=0x12345678||std::memcmp(&actual,&c.f[0],4))throw std::runtime_error("Original lighting brightness mismatch");if(inputs.is_open()){inputs.write(reinterpret_cast<const char*>(color.data()),16);expected.write(reinterpret_cast<const char*>(&c.f[0]),4);}if(actual==0)below++;else if(actual==1)above++;else middle++;}
printf("20000 complete original brightness evaluations match: %u lower, %u upper, %u nonlinear results; original wrappers and leaf kernels execute.\n",below,above,middle);}
