#include "ps2_runtime_macros.h"
#include "../engine/terrain_contact_math.hpp"
#include <fstream>
#include <cstdio>
#include <cstring>
#include <vector>
void sub_001E9A30_0x1e9a30(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
template<class T>void put(uint8_t*m,unsigned p,T v){std::memcpy(m+p,&v,sizeof v);}
template<class T>T get(uint8_t*m,unsigned p){T v;std::memcpy(&v,m+p,sizeof v);return v;}
struct Draw{int letter,mode,order;float sx,sy;};static std::vector<Draw> draws;
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});if(bytes.size()!=32*1024*1024)return 2;auto*m=bytes.data();PS2Runtime rt;
rt.registerFunction(0x21ed48,[](uint8_t*m,R5900Context*c,PS2Runtime*){auto scale=GPR_U32(c,9);draws.push_back({GPR_S32(c,8),GPR_S32(c,11),get<int>(m,GPR_U32(c,29)),get<float>(m,scale),get<float>(m,scale+4)});c->pc=GPR_U32(c,31);});
put(m,0x2042c,0x30000u);puts("count,fraction,letter,mode,order,scale_x,scale_y");
for(int count=1;count<=9;count++)for(int step=0;step<=180;step++){
float fraction=float(step)/180;R5900Context c{};c.pc=0x1ed790;SET_GPR_U32(&c,22,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);c.f[22]=fraction;
put(m,0x10354,0);put(m,0x10348,0x200000);put(m,0x1035c,count);put(m,0x10030,1.f);put(m,0x10034,1.f);put(m,0x10324,0x4768b0);put(m,0x10338,0x31000);put(m,0x10334,0);draws.clear();
{ssx::OriginalRounding round;sub_001E9A30_0x1e9a30(m,&c,&rt);}if(c.pc!=0x12345678||draws.empty())return 3;
for(auto d:draws)printf("%d,%.9g,%d,%d,%d,%.9g,%.9g\n",count,fraction,d.letter,d.mode,d.order,d.sx,d.sy);
}
}
