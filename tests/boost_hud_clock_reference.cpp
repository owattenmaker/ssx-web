#include "ps2_runtime_macros.h"
#include "../engine/boost_hud_clock.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <cstring>
void sub_001E9A30_0x1e9a30(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
template<class T>void put(uint8_t*m,unsigned p,T v){std::memcpy(m+p,&v,sizeof v);}
template<class T>T get(uint8_t*m,unsigned p){T v;std::memcpy(&v,m+p,sizeof v);return v;}
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});if(bytes.size()!=32*1024*1024)return 2;auto*m=bytes.data();PS2Runtime rt;std::mt19937 rng(0x1ebdf4);std::uniform_real_distribution<float> fraction(0,1),phase(-1,4);
for(int mode=0;mode<2;mode++)for(int i=0;i<20000;i++){
float old=phase(rng),value=fraction(rng);if(i<5)old=float(i)-1;R5900Context c{};c.pc=mode?0x1ebed4:0x1ebdf4;SET_GPR_U32(&c,16,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);c.f[0]=value;c.f[2]=1;c.f[21]=0;unsigned at=0x20000+(mode?0x68:0x64);put(m,at,old);
{ssx::OriginalRounding round;sub_001E9A30_0x1e9a30(m,&c,&rt);}
float expected=mode?ssx::originalBoostPendingLetterClock(old):ssx::originalBoostFlashClock(old,value),actual=get<float>(m,at);
if(c.pc!=0x12345678||std::bit_cast<unsigned>(expected)!=std::bit_cast<unsigned>(actual)){printf("Clock mismatch mode%d case%d actual%.9g expected%.9g pc%x\n",mode,i,actual,expected,c.pc);return 3;}
}
puts("40000 original HUD flash and pending-letter clock cases match exactly");
}
