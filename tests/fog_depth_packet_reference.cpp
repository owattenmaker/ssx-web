#include "ps2_runtime_macros.h"
#include "../engine/fog_composite.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_0036AE20_0x36ae20(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;std::mt19937 rng(0x36af10);auto read=[&](unsigned at){uint64_t x;std::memcpy(&x,m.data()+at,8);return x;};
for(int n=0;n<1000;++n){using P=ssx::OriginalFogDepthPass;R5900Context c{};c.pc=0x36af10;SET_GPR_U32(&c,23,0x30000);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,16,rng()%512);SET_GPR_U32(&c,22,rng()%512);SET_GPR_U32(&c,17,224);SET_GPR_U32(&c,18,512);SET_GPR_U32(&c,2,8+n%4);sub_0036AE20_0x36ae20(m.data(),&c,&rt);
auto frame=read(0x30010);if(c.pc!=0x36afdc||read(0x30018)!=0x4c||((frame>>24)&63)!=P::frameFormat||uint32_t(frame>>32)!=P::frameMask||read(0x30078)!=0x3b||read(0x30070)!=P::textureAlpha||read(0x30068)!=0x3f||read(0x30060)!=0)throw std::runtime_error("Fog depth preparation state differs");
c.pc=0x36aff0;SET_GPR_U32(&c,2,512+n%3*64);SET_GPR_U32(&c,21,rng()%512);sub_0036AE20_0x36ae20(m.data(),&c,&rt);auto tex=read(0x30090);
if(c.pc!=0x36b04c||read(0x30098)!=6||((tex>>20)&63)!=P::textureFormat||((tex>>34)&1)!=1||((tex>>35)&3)!=1||read(0x300a0)!=P::primitive||read(0x300a8)!=0||read(0x300b0)!=0x3f80000080808080ull||read(0x300b8)!=1)throw std::runtime_error("Fog depth texture/sprite differs");}
puts("1000 original depth preparation packet cases match: PSMCT16 mask3FFF, PSMZ16 DECAL/RGBA, TEXA and unblended sprites. Pixel memory/UV mapping is outside this test.");}
