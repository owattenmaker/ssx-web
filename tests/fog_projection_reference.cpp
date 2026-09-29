#include "ps2_runtime_macros.h"
#include "../engine/fog_depth_table.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
#include <fstream>
void sub_0036A428_0x36a428(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){if(argc!=2)return 2;std::array<float,21> captured;std::ifstream input(argv[1],std::ios::binary);input.read(reinterpret_cast<char*>(captured.data()),sizeof(captured));if(!input)return 3;std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;auto put=[&](uint32_t at,auto v){std::memcpy(m.data()+at,&v,sizeof(v));};std::mt19937 rng(0x36a428);
for(unsigned n=0;n<20000;++n){unsigned view=n%4;uint32_t context=0x20000+0x6c64+view*32,matrixAt=0x20000+0x5930+view*64;std::array<float,16> matrix{};for(auto&x:matrix)x=float(int(rng()%20001)-10000)/100.f;matrix[11]=float(rng()%100)/10000.f;matrix[15]=1.f;matrix[10]=float(int(rng()%601)-300)/100.f;matrix[14]=float(int(rng()%160001)-80000);std::memcpy(m.data()+matrixAt,matrix.data(),64);
float nearCm=float(rng()%50000+1),farCm=float(rng()%50000+1);std::array<float,3> color;for(auto&x:color)x=float(rng()%10001)/10000.f;if(n==0){std::copy_n(captured.begin(),16,matrix.begin());nearCm=captured[16];farCm=captured[17];std::copy_n(captured.begin()+18,3,color.begin());std::memcpy(m.data()+matrixAt,matrix.data(),64);}
put(context,nearCm);put(context+4,farCm);for(unsigned i=0;i<3;++i)put(context+20+i*4,color[i]);
R5900Context c{};c.pc=0x36a428;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,6,view);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
{ssx::terrain_original::Rounding round;sub_0036A428_0x36a428(m.data(),&c,&rt);}auto result=ssx::originalFogDepthParameters(matrix,nearCm,farCm,color);
if(c.pc!=0x36a61c||GPR_U32((&c),18)!=uint32_t(result.nearBin)||GPR_U32((&c),17)!=uint32_t(result.farBin)||GPR_U32((&c),21)!=result.rgb){printf("Fog projection mismatch case%u\n",n);return 1;}}
puts("20000 original fog projection/color preparations match near/far bins and packed RGB across4 view slots, including the captured countdown projection. Cache and compositing remain outside this test.");}
