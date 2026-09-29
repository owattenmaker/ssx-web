#include "ps2_runtime_macros.h"
#include "../engine/snow_flipbook.hpp"
#include <random>
#include <vector>
#include <cstring>
#include <cstdio>
void sub_00370788_0x370788(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(){std::vector<uint8_t> memory(32*1024*1024);PS2Runtime runtime;std::mt19937 rng(0x370788);auto put=[&](unsigned at,auto value){std::memcpy(memory.data()+at,&value,sizeof(value));};
 for(unsigned target:{0x36d428,0x36d3e8})runtime.registerFunction(target,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x20010)throw std::runtime_error("Unexpected particle kernel receiver");c->pc=GPR_U32(c,31);});
 put(0x4a30f0+0x2a74,uint32_t(0x30000));put(0x30010,int32_t(60));
 for(unsigned i=0;i<20000;++i){const int count=1+rng()%16;const float phase=float(rng()%10000)/10000.f*count,rate=float(rng()%10000)/100.f,elapsed=i%4?1.f/60.f:float(rng()%100)/100.f,lifetime=i%3==0?0.f:i%3==1?-1.f:.01f;
  put(0x20000,lifetime);put(0x20180,count);put(0x20184,phase);put(0x20188,rate);R5900Context c{};c.pc=0x370788;c.f[12]=elapsed;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  {ssx::terrain_original::Rounding rounding;sub_00370788_0x370788(memory.data(),&c,&runtime);}
  const float actual=ssx::originalSnowFlipbookStep(phase,count,rate,elapsed,lifetime!=0);
  if(c.pc!=0x12345678||std::memcmp(&actual,memory.data()+0x20184,4)){printf("Flipbook mismatch%u\n",i);return 1;}
 }
 puts("20000 original370788 emitter phase updates match exactly; dead/infinite/expiring emitters, varied rates, overshoot and frame counts. Particle kernel callbacks are controlled boundaries.");
}
