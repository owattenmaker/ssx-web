// Development-only private original instruction oracle; no guest runtime in app.
#include "ps2_runtime_macros.h"
#include "../engine/air_alignment.hpp"
#include <fstream>
#include <cfenv>
#include <cstdio>
#include <random>
#include <bit>
void sub_00139A20_0x139a20(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00121AA0_0x121aa0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BB30_0x31bb30(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BE50_0x31be50(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BF60_0x31bf60(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C128_0x31c128(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011E098_0x11e098(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x320000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=0x88000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x88000]={nullptr};
int main(int argc,char**argv){PS2Runtime rt;std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> ram((std::istreambuf_iterator<char>(input)),{});if(ram.size()!=32*1024*1024)return 2;
#define REGISTER(addr) rt.registerFunction(0x##addr,sub_00##addr##_0x##addr)
 rt.registerFunction(0x121AA0,sub_00121AA0_0x121aa0);rt.registerFunction(0x31BB30,sub_0031BB30_0x31bb30);
 rt.registerFunction(0x31BE50,sub_0031BE50_0x31be50);rt.registerFunction(0x31BF60,sub_0031BF60_0x31bf60);
 rt.registerFunction(0x31C128,sub_0031C128_0x31c128);rt.registerFunction(0x31C228,sub_0031C228_0x31c228);rt.registerFunction(0x11E098,sub_0011E098_0x11e098);
 static unsigned controlState;rt.registerFunction(0x11FEE8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,controlState);c->pc=GPR_U32(c,31);});
 auto wr=[&](unsigned a,auto x){std::memcpy(ram.data()+a,&x,sizeof(x));};auto rd=[&](unsigned a){float x;std::memcpy(&x,ram.data()+a,4);return x;};
 std::fesetround(FE_TOWARDZERO);std::mt19937 rng(0x121aa0);std::uniform_real_distribution<float> v(-1,1);
 auto unit=[](auto x){float sum=0;for(float f:x)sum+=f*f;float inv=1.f/std::sqrt(sum);for(float& f:x)f*=inv;return x;};
 for(unsigned k=0;k<30000;k++){
  std::array<float,4> q=unit(std::array<float,4>{v(rng),v(rng),v(rng),v(rng)});
  std::array<float,3> normal=unit(std::array<float,3>{v(rng),v(rng),v(rng)}),heading=unit(std::array<float,3>{v(rng),v(rng),v(rng)});
  if(k%3==0)heading={0,0,0};if(k%10==0){q={0,0,0,1};normal={0,0,1};heading={float(int(k%3)-1),0,0};}
  float gain=float(k%120)*.25f,cap=float(k%100)*.05f;
  wr(0x30120,q);wr(0x40000,normal);wr(0x4000c,0.f);wr(0x50000,heading);wr(0x5000c,0.f);
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);SET_GPR_U32(&c,4,0x30000);SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,6,0x50000);c.f[12]=gain;c.f[13]=cap;
  sub_00121AA0_0x121aa0(ram.data(),&c,&rt);auto out=ssx::originalAirAlignment(q,normal,heading,gain,cap);
  if(c.pc!=0x12345678){printf("continuation %x\n",c.pc);return 3;}
  for(int i=0;i<4;i++)if(out.quaternion[i]!=rd(0x30120+i*4)){printf("alignment mismatch %u/%d expected%.9g native%.9g heading%.9g %.9g %.9g\n",k,i,rd(0x30120+i*4),out.quaternion[i],heading[0],heading[1],heading[2]);return 4;}
  std::array<std::array<float,3>,3> columns={out.right,out.forward,out.up};for(int j=0;j<3;j++)for(int i=0;i<3;i++)if(columns[j][i]!=rd(0x301a0+j*16+i*4)){printf("matrix mismatch %u/%d/%d\n",k,j,i);return 5;}
 }
 for(unsigned k=0;k<20000;k++){
  auto q=unit(std::array<float,4>{v(rng),v(rng),v(rng),v(rng)});ssx::OriginalAirAlignmentContext context;
  context.normal=unit(std::array<float,3>{v(rng),v(rng),v(rng)});context.heading=unit(std::array<float,3>{v(rng),v(rng),v(rng)});context.physicalForward=unit(std::array<float,3>{v(rng),v(rng),v(rng)});
  context.predictedTime=v(rng);context.elapsedTime=v(rng);context.timeScale=.2f+std::abs(v(rng));context.adjustSpin=k%2?v(rng):0;
  context.trajectoryStatus=k%5;context.surfaceIndex=k%19;context.surfaceFlags=k%7==0?-1:k%3==0?0:8;context.surfaceProperty44=k%4==0?1:0;context.controlState=controlState=k%2?5:0;context.airModeFlag=k%3;
  auto vector=[&](unsigned at,auto a){wr(at,a);wr(at+12,0.f);};
  wr(0x30120,q);vector(0x301b0,context.physicalForward);wr(0x30300,context.timeScale);wr(0x30788,uint32_t(0x60000));wr(0x3077c,uint32_t(0x80000));wr(0x80258,context.adjustSpin);
  vector(0x60020,context.normal);vector(0x60010,context.heading);wr(0x60098,context.predictedTime);wr(0x600a0,context.elapsedTime);wr(0x600ac,context.trajectoryStatus);wr(0x60090,context.surfaceIndex);wr(0x60094,int16_t(context.surfaceFlags));
  wr(0x4a30f0-0x848,uint32_t(0x90000));wr(0x90084,uint32_t(0x90100));wr(0x90144,uint32_t(0xa0000));wr(0xa0000+context.surfaceIndex*0xb0+0x44,context.surfaceProperty44);
  wr(0x70000,context.airModeFlag);wr(0x70004,uint32_t(0x30000));wr(0x10020,uint64_t(0x12345678));
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,16,0x70000);c.pc=0x139a64;
  sub_00139A20_0x139a20(ram.data(),&c,&rt);auto out=ssx::originalAirAlignmentStage(q,context);
  if(c.pc!=0x12345678){printf("stage continuation %x\n",c.pc);return 6;}
  for(int i=0;i<4;i++)if(out.quaternion[i]!=rd(0x30120+i*4)){printf("stage mismatch %u/%d expected%.9g native%.9g\n",k,i,rd(0x30120+i*4),out.quaternion[i]);return 7;}
 }
 puts("20000 full original trajectory-gated physical air orientation tails match exactly");
 puts("30000 complete original physical air alignment stages and rebuilt axes match exactly");
}
