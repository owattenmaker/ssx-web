#include "ps2_runtime_macros.h"
#include "../engine/boost_hud.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <cstring>
void sub_00117FE0_0x117fe0(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
template<class T>void put(uint8_t*m,unsigned at,T value){std::memcpy(m+at,&value,sizeof value);}
static std::vector<std::pair<int,float>> draws;
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> memory((std::istreambuf_iterator<char>(f)),{});if(memory.size()!=32*1024*1024)return 2;auto*m=memory.data();PS2Runtime rt;
 rt.registerFunction(0x1171a8,[](uint8_t*,R5900Context*c,PS2Runtime*){int kind=GPR_S32(c,5);if(GPR_U32(c,4)!=(kind==5?0x4030c:0x403a8)||GPR_U32(c,6)||GPR_U32(c,7))throw std::runtime_error("Unexpected HUD widget call");draws.push_back({kind,c->f[12]});c->pc=GPR_U32(c,31);});
 put(m,0x201ac,0x30000u);put(m,0x201b0,0x40000u);
 std::mt19937 rng(0x1188f8);std::uniform_real_distribution<float> meter(0,1),pending(-.1,2),value(-2,2),maximum(.1,20);
 for(unsigned n=0;n<20000;n++){
  ssx::OriginalBoostHudInput input;input.meter=meter(rng);input.pendingReward=pending(rng);input.previewValue=value(rng);input.storedValue=value(rng);input.previewMaximum=-maximum(rng);input.storedMaximum=-maximum(rng);input.previewUninitialized=n%3==0;input.storedUninitialized=n%5==0;input.drainMode=n%4;
  if(input.previewUninitialized)input.previewMaximum=0;if(input.storedUninitialized)input.storedMaximum=0;
  put(m,0x20014,input.pendingReward);put(m,0x302f8,input.meter);put(m,0x30304,input.drainMode);put(m,0x4030c,input.previewUninitialized?52:5);put(m,0x40310,input.previewMaximum);put(m,0x40314,input.previewValue);put(m,0x403a8,input.storedUninitialized?52:6);put(m,0x403ac,input.storedMaximum);put(m,0x403b0,input.storedValue);
  R5900Context c{};c.pc=0x1188f8;SET_GPR_U32(&c,17,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);draws.clear();{ssx::OriginalRounding round;sub_00117FE0_0x117fe0(m,&c,&rt);}
  auto result=ssx::originalBoostHudValues(input);
  if(c.pc!=0x12345678||draws.size()!=2||draws[0].first!=5||draws[1].first!=6||std::bit_cast<uint32_t>(draws[0].second)!=std::bit_cast<uint32_t>(result.preview)||std::bit_cast<uint32_t>(draws[1].second)!=std::bit_cast<uint32_t>(result.stored)){printf("HUD mismatch %u pc%x draws%zu actual %.9g %.9g\n",n,c.pc,draws.size(),result.preview,result.stored);return 3;}
 }
 puts("20000 original boost HUD updates match both ordered widget values exactly, including initialization, pending preview, disabled awards and smoothing");
}
