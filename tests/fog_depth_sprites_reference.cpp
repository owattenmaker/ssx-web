#include "ps2_runtime_macros.h"
#include "../engine/fog_depth_sprites.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_0036AE20_0x36ae20(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x420000-0x100000)/4]={};
int main(){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;std::mt19937 rng(0x36b04c);size_t sprites=0;
auto put=[&](unsigned at,uint32_t value){std::memcpy(m.data()+at,&value,4);};
for(int n=0;n<10000;++n){int columns=int(rng()%130)-1;uint32_t height=rng()%1024+1,ox=24000+rng()%6000,oy=24000+rng()%6000;
R5900Context c{};c.pc=0x36b04c;SET_GPR_U32(&c,30,columns);SET_GPR_U32(&c,19,oy);SET_GPR_U32(&c,23,0x30000-0xc0);SET_GPR_U32(&c,29,0x10000);put(0x10000,height);put(0x10008,ox);put(0x1000c,oy);
sub_0036AE20_0x36ae20(m.data(),&c,&rt);auto expected=ssx::originalFogDepthSprites(columns,height,ox,oy);
if(c.pc!=0x36b0e8||GPR_U32((&c),23)!=0x30000+expected.size()*64||(!expected.empty()&&std::memcmp(expected.data(),m.data()+0x30000,expected.size()*64)))throw std::runtime_error("Original fog depth sprites differ");sprites+=expected.size();}
printf("10000 original fog depth strip layouts match %zu emitted sprites, including UV/XYZ register tags and empty/odd column counts. Pixel reinterpretation remains separate.\n",sprites);}
