#include "ps2_runtime_macros.h"
#include "../engine/boost_letter_hud.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <cstring>
void sub_00117FE0_0x117fe0(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
template<class T>void put(uint8_t*m,unsigned at,T value){std::memcpy(m+at,&value,sizeof value);}
static int calls,count;static float fraction;static bool removed;
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> memory((std::istreambuf_iterator<char>(f)),{});if(memory.size()!=32*1024*1024)return 2;auto*m=memory.data();PS2Runtime rt;
 rt.registerFunction(0x1171a8,[](uint8_t*m,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x404e0||GPR_U32(c,5)!=8||GPR_U32(c,6)){fprintf(stderr,"Widget a0=%x type=%u a2=%u pc=%x\n",GPR_U32(c,4),GPR_U32(c,5),GPR_U32(c,6),c->pc);throw std::runtime_error("Unexpected letter widget call");};calls++;count=GPR_S32(c,7);fraction=c->f[12];put(m,0x404e0,8);put(m,0x404e4,-1.f);put(m,0x404e8,-fraction);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x1179e0,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x20000||GPR_U32(c,5)!=8)throw std::runtime_error("Unexpected letter removal");removed=true;c->pc=GPR_U32(c,31);});
 put(m,0x201ac,0x30000u);put(m,0x201b0,0x40000u);std::mt19937 rng(0x118a28);std::uniform_real_distribution<float> value(-2,2),maximum(.1,20);
 for(unsigned n=0;n<20000;n++){
 ssx::OriginalBoostLetterHudInput input;input.tier=n%16;input.uninitialized=n%3==0;input.value=value(rng);input.maximum=-maximum(rng);
 put(m,0x302f4,input.tier);put(m,0x404e0,input.uninitialized?52:8);put(m,0x404e4,input.maximum);put(m,0x404e8,input.value);
 R5900Context c{};c.pc=0x118a28;SET_GPR_U32(&c,17,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);calls=0;removed=false;count=0;fraction=0;
 {ssx::OriginalRounding round;sub_00117FE0_0x117fe0(m,&c,&rt);}
 const auto result=ssx::originalBoostLetterHud(input);
 if(c.pc!=0x12345678||removed!=result.removed||calls!=(input.tier==0?0:input.uninitialized?2:1)||count!=result.count||std::bit_cast<uint32_t>(fraction)!=std::bit_cast<uint32_t>(result.fraction)){printf("Letter mismatch %u tier%d calls%d fraction%.9g expected%.9g\n",n,input.tier,calls,fraction,result.fraction);return 3;}
 }
 puts("20000 original letter HUD updates match: removal, initialization, count cap and smoothed fraction");
}
