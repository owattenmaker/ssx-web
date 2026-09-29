#include "ps2_runtime_macros.h"
#include "../engine/fog_depth_table.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_0036A428_0x36a428(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;auto put=[&](uint32_t at,auto v){std::memcpy(m.data()+at,&v,sizeof(v));};
std::array<uint32_t,6> coeff={0x3dcccccdu,0x39500d03u,0x3ab60b62u,0x3c088889u,0x3d2aaaabu,0x3e2aaaabu};for(unsigned i=0;i<6;++i){put(0x4a30f0-0x289c+i*4,coeff[i]);put(0x4a30f0-0x2884+i*4,coeff[i]);}
std::mt19937 rng(0x36a6ec);
for(unsigned n=0;n<5000;++n){int nearBin=rng()%256,farBin=rng()%256,equation=n%5-1;float density=float(int(rng()%3001)-500)/100.f;uint32_t rgb=rng()&0xffffff;std::array<uint32_t,256> table;for(auto&v:table)v=rng();std::memcpy(m.data()+0x30000,table.data(),1024);put(0x20008,equation);put(0x2000c,density);
R5900Context c{};c.pc=0x36a6ec;SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,16,1);SET_GPR_U32(&c,17,farBin);SET_GPR_U32(&c,18,nearBin);SET_GPR_U32(&c,19,std::max(1,nearBin-farBin+1));SET_GPR_U32(&c,20,0x20000);SET_GPR_U32(&c,21,rgb);SET_GPR_U32(&c,2,0x30000);c.f[20]=ssx::originalScalarDivide(1.f,float(std::max(1,nearBin-farBin+1)));
{ssx::terrain_original::Rounding round;sub_0036A428_0x36a428(m.data(),&c,&rt);}if(c.pc!=0x36aa24)throw std::runtime_error("Fog table original incomplete");ssx::originalFogDepthTable(table,nearBin,farBin,equation,density,rgb);
if(std::memcmp(table.data(),m.data()+0x30000,1024)){printf("Fog palette mismatch case%u bins%d/%d equation%d density%f\n",n,nearBin,farBin,equation,density);return 1;}}
puts("5000 original fog palette fills match all256 packed/swizzled entries; projection, cache and upload are outside this test.");}
