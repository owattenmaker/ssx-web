#include "ps2_runtime_macros.h"
#include "../engine/original_camera.hpp"
#include <vector>
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
void sub_0015EE00_0x15ee00(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C128_0x31c128(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){
 if(argc!=2)return 2;
 std::ifstream file(argv[1],std::ios::binary);
 std::vector<uint8_t> elf((std::istreambuf_iterator<char>(file)),{}),m(32*1024*1024);
 std::memcpy(m.data()+0xff000,elf.data(),elf.size());
 auto put=[&](unsigned at,auto v){std::memcpy(m.data()+at,&v,sizeof(v));};
 PS2Runtime rt;rt.registerFunction(0x31c128,sub_0031C128_0x31c128);
 put(0x20004,0x21000u);put(0x21788,0x22000u);
 std::mt19937 rng(0x15f780);
 for(unsigned n=0;n<20000;++n){
  ssx::OriginalCameraInput in;
  for(unsigned k=0;k<3;++k){in.trajectoryHeading[k]=float(int(rng()%20001)-10000);in.trajectoryNormal[k]=float(int(rng()%20001)-10000)/10000;}
  if(n%7==0)in.trajectoryHeading={};
  if(n%7==1)in.trajectoryNormal={};
  if(n%7==2)for(unsigned k=0;k<3;++k)in.trajectoryHeading[k]=in.trajectoryNormal[k];
  if(n%7==3)for(unsigned k=0;k<3;++k)in.trajectoryHeading[k]=-in.trajectoryNormal[k];
  if(n%7==4)for(unsigned k=0;k<3;++k)in.trajectoryHeading[k]*=1e-8f;
  put(0x22010,in.trajectoryHeading);put(0x22020,in.trajectoryNormal);
  R5900Context c{};c.pc=0x15f780;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);
  SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  float result;
  {ssx::original_camera::Rounding rounding;sub_0015EE00_0x15ee00(m.data(),&c,&rt);result=ssx::original_camera::landingAngle(in);}
  if(c.pc!=0x12345678||std::memcmp(&result,&c.f[0],4)){
   printf("Landing angle mismatch case%u original%g native%g\n",n,c.f[0],result);return 1;
  }
 }
 puts("20000 complete original landing-angle getters match bits with the real asin callee, including zero, tiny, parallel and antiparallel vectors.");
}
