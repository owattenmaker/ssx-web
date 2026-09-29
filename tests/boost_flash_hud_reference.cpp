#include "ps2_runtime_macros.h"
#include "../engine/boost_flash_hud.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <cstring>
void sub_00117FE0_0x117fe0(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
template<class T>void put(uint8_t*m,unsigned at,T value){std::memcpy(m+at,&value,sizeof value);}
static int calls,count;static float fraction;static bool removed,shown,hidden;static std::vector<int> events;
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> memory((std::istreambuf_iterator<char>(f)),{});if(memory.size()!=32*1024*1024)return 2;auto*m=memory.data();PS2Runtime rt;
 rt.registerFunction(0x1171a8,[](uint8_t*m,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x4057c||GPR_U32(c,5)!=9||GPR_U32(c,6))throw std::runtime_error("Unexpected pending widget call");events.push_back(9);calls++;count=GPR_S32(c,7);fraction=c->f[12];put(m,0x4057c,9);put(m,0x40580,-1.f);put(m,0x40584,-fraction);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x1179e0,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x20000||GPR_U32(c,5)!=9)throw std::runtime_error("Unexpected pending removal");events.push_back(-9);removed=true;c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x28b180,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back(1);SET_GPR_U32(c,2,0x50000);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x299638,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x50000||GPR_U32(c,5)!=0x30000)throw std::runtime_error("Unexpected show");shown=true;events.push_back(2);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x2997b8,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x50000||GPR_U32(c,5)!=0x30000)throw std::runtime_error("Unexpected hide");hidden=true;events.push_back(3);c->pc=GPR_U32(c,31);});
 put(m,0x201ac,0x30000u);put(m,0x201b0,0x40000u);std::mt19937 rng(0x118af8);std::uniform_real_distribution<float> value(-2,2),maximum(.1,20);
 for(unsigned n=0;n<20000;n++){
 ssx::OriginalBoostFlashHudInput input;input.tier=n%16;input.timer=n%4==0?0.f:float(n%2400)/60.f;input.uninitialized=n%3==0;input.value=value(rng);input.maximum=-maximum(rng);if(n%5==0)input.value=input.maximum;
 put(m,0x302f4,input.tier);put(m,0x302f0,input.timer);put(m,0x4057c,input.uninitialized?52:9);put(m,0x40580,input.maximum);put(m,0x40584,input.value);
 R5900Context c{};c.pc=0x118c2c;SET_GPR_U32(&c,17,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);calls=count=0;fraction=0;removed=shown=hidden=false;events.clear();
 {ssx::OriginalRounding round;sub_00117FE0_0x117fe0(m,&c,&rt);}
 auto result=ssx::originalBoostFlashHud(input);std::vector<int> expected=result.shown?std::vector<int>{9,1,2}:result.hidden?std::vector<int>{-9,1,3}:result.updated?std::vector<int>{9}:std::vector<int>{};
 if(c.pc!=0x12345678||events!=expected||removed!=result.hidden||shown!=result.shown||hidden!=result.hidden||count!=result.paletteTier||std::bit_cast<uint32_t>(fraction)!=std::bit_cast<uint32_t>(result.fraction)){printf("Flash mismatch %u actual%.9g expected%.9g\n",n,fraction,result.fraction);return 3;}

 }
 puts("20000 original Tricky timer HUD updates match palette tier, fraction bits and ordered lifecycle callbacks");
}
