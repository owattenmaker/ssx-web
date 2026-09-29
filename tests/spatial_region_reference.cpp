#include "ps2_runtime_macros.h"
#include "../engine/spatial_region.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_00328360_0x328360(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(){
 std::vector<uint8_t> memory(32*1024*1024);PS2Runtime runtime;
 auto put=[&](unsigned at,auto value){std::memcpy(memory.data()+at,&value,sizeof(value));};put(0x4a30f0-0x2dfc,.20000000298023224f);
 std::mt19937 rng(0x328360);unsigned counts[3]{};
 for(unsigned n=0;n<20000;++n){
  ssx::OriginalSpatialRegion region;region.exponent=rng()%21;for(auto&c:region.cell)c=int(rng()%513)-256;
  const float scale=std::bit_cast<float>((uint32_t(region.exponent)+127u)<<23);std::array<float,3> low,high;
  for(unsigned i=0;i<3;++i){low[i]=(region.cell[i]+float(int(rng()%3001)-1000)/1000)*scale;high[i]=low[i]+float(rng()%1001)/1000*scale;}
  if(n%7==0){ssx::terrain_original::Rounding rounding;for(unsigned i=0;i<3;++i)low[i]=high[i]=ssx::terrain_original::mul(ssx::originalScalarAdd(float(region.cell[i]+1),.20000000298023224f),scale);}
  if(n%7==1){ssx::terrain_original::Rounding rounding;for(unsigned i=0;i<3;++i)low[i]=high[i]=ssx::terrain_original::mul(ssx::originalScalarSubtract(float(region.cell[i]),.20000000298023224f),scale);}
  put(0x20000,region.exponent);put(0x20004,region.cell);put(0x30000,low);put(0x30010,high);
  R5900Context c{};c.pc=0x328360;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x30000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,0x12345678);
  {ssx::terrain_original::Rounding rounding;sub_00328360_0x328360(memory.data(),&c,&runtime);}
  const int actual=ssx::originalSpatialRegionClassify(region,low,high);
  if(c.pc!=0x12345678||actual!=GPR_S32((&c),2)){printf("Region mismatch %u native%d original%d\n",n,actual,GPR_S32((&c),2));return 1;}
  ++counts[actual];
 }
 if(!counts[0]||!counts[1]||!counts[2])return 2;
 printf("20000 original spatial-region classifications match; codes0/1/2=%u/%u/%u, including padded boundary equality.\n",counts[0],counts[1],counts[2]);
}
