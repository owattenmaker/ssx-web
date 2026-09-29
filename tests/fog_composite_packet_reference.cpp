#include "ps2_runtime_macros.h"
#include "../engine/fog_composite.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_0036B158_0x36b158(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;std::mt19937 rng(0x36b300);auto read=[&](unsigned at){uint64_t x;std::memcpy(&x,m.data()+at,8);return x;};
for(int n=0;n<1000;++n){R5900Context c{};c.pc=0x36b300;SET_GPR_U32(&c,22,0x30000);SET_GPR_U32(&c,16,rng());SET_GPR_U32(&c,18,rng());sub_0036B158_0x36b158(m.data(),&c,&rt);
if(c.pc!=0x36b360||read(0x30030)!=ssx::originalFogTestRegister||read(0x30038)!=0x47||read(0x30058)!=0x42||read(0x30050)!=ssx::originalFogAlphaRegister||read(0x30068)!=0x3f||read(0x30060)!=0)throw std::runtime_error("Fog blend packet differs");
c.pc=0x36b374;SET_GPR_U32(&c,2,512+n%3*64);SET_GPR_U32(&c,19,rng()%1024);SET_GPR_U32(&c,20,rng()%16384);sub_0036B158_0x36b158(m.data(),&c,&rt);
auto tex=read(0x30090);if(c.pc!=0x36b3e8||read(0x30098)!=6||((tex>>20)&63)!=27||((tex>>34)&1)!=1||((tex>>35)&3)!=1||read(0x300a0)!=ssx::originalFogPrimitive||read(0x300a8)!=0||read(0x300b0)!=0x3f80000080808080ull||read(0x300b8)!=1)throw std::runtime_error("Fog texture/sprite packet differs");
c={};c.pc=0x36b3f0;SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,22,0x30000);SET_GPR_U32(&c,21,1+n%8);SET_GPR_U32(&c,30,0);SET_GPR_U32(&c,17,0);uint32_t width=512,height=448;std::memcpy(m.data()+0x10010,&width,4);std::memcpy(m.data()+0x10020,&height,4);sub_0036B158_0x36b158(m.data(),&c,&rt);
if(c.pc!=0x36b4a4)throw std::runtime_error("Fog sprite stop differs");for(unsigned i=0;i<unsigned(1+n%8);++i)if((read(0x30010+i*64)>>32)!=ssx::originalFogSpriteDepth||(read(0x30030+i*64)>>32)!=ssx::originalFogSpriteDepth)throw std::runtime_error("Fog sprite depth differs");}
for(uint32_t depth:{0u,65534u,65535u,65536u,16777215u})if(ssx::originalFogDepthPasses(depth)!=(depth<65535))throw std::runtime_error("Fog depth boundary differs");
for(unsigned fog=0;fog<256;++fog)for(unsigned scene=0;scene<256;++scene){std::array<uint8_t,3> pixel{uint8_t(scene),uint8_t(scene),uint8_t(scene)};uint32_t rgb=fog|(fog<<8)|(fog<<16);if(ssx::originalFogCompositeRgb(pixel,rgb)!=std::array<uint8_t,3>{uint8_t(fog),uint8_t(fog),uint8_t(fog)}||ssx::originalFogCompositeRgb(pixel,rgb|0x80000000u)!=pixel)throw std::runtime_error("Fog transmittance endpoint mismatch");}
puts("1000 original fog packet setup cases confirm ALPHA=1, PABE=0, DECAL/RGBA T8H, sprite flags, unity RGBAQ, strict TEST=0x70000 and sprite Z=0xFFFF. Byte blend/depth boundary checks pass. Full render/composite output is not emulated.");}
