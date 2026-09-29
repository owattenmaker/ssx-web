#include "ps2_runtime_macros.h"
#include "../engine/reset_fade.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <cstring>
#include <cfenv>
void sub_002E4678_0x2e4678(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){
 if(argc!=2)return 1;PS2Runtime rt;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> memory((std::istreambuf_iterator<char>(file)),{});if(memory.size()!=32*1024*1024)return 2;
 auto put=[&](unsigned at,auto value){std::memcpy(memory.data()+at,&value,sizeof(value));};
 constexpr unsigned fade=0x20000,in=0x30000,out=0x30020;put(fade+0x50,out);put(fade+0x54,.5f);put(fade+0x58,0.f);put(fade+0x5c,.5f);
 std::fesetround(FE_TOWARDZERO);std::mt19937 random(0x2e48ac);std::uniform_real_distribution<float> value(-.5f,1.5f);
 for(unsigned trial=0;trial<20000;trial++){
  const float progress=trial<5?float(trial)*.25f:value(random);
  R5900Context c{};c.pc=0x2e48ac;c.f[20]=progress;c.f[1]=.5f;SET_GPR_U32(&c,17,fade);SET_GPR_U32(&c,2,in);SET_GPR_U32(&c,31,0x12345678);
  sub_002E4678_0x2e4678(memory.data(),&c,&rt);const float alpha=ssx::originalResetFadeAlpha(progress);
  if(c.pc!=0x12345678||alpha!=c.f[22]||GPR_U32((&c),18)!=(progress<.5f?in:out)){printf("reset fade mismatch %u %.9g %.9g/%.9g\n",trial,progress,alpha,c.f[22]);return 3;}
 }
 puts("20000 original reset-fade opacity/phase selections exact, including endpoints, midpoint and clamping");
}
