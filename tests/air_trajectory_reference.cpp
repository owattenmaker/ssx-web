#include "ps2_runtime_macros.h"
#include "../engine/air_trajectory.hpp"
#include <fstream>
#include <cfenv>
#include <cstdio>
#include <random>
#include <bit>
void sub_00113198_0x113198(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001135B8_0x1135b8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00113200_0x113200(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00113618_0x113618(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00113648_0x113648(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001139A0_0x1139a0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0032E100_0x32e100(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x340000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=0x90000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x90000]={nullptr};
using V=std::array<float,3>;
struct Request{V end,start;int mode;};static std::vector<Request> requests;static ssx::OriginalAirTrajectoryHit hit;
static void query(uint8_t*m,R5900Context*c,int mode){
 unsigned ray=GPR_U32(c,5),out=GPR_U32(c,6);Request r;r.mode=mode;
 for(int i=0;i<3;i++){float delta;std::memcpy(&r.end[i],m+ray+0x60+i*4,4);std::memcpy(&delta,m+ray+0x70+i*4,4);r.start[i]=r.end[i]+delta;}
 requests.push_back(r);
 auto wr=[&](unsigned a,auto x){std::memcpy(m+a,&x,sizeof(x));};wr(out,hit.position);wr(out+0x10,hit.normal);wr(out+0x4c,hit.surface);wr(out+0x54,uint32_t(hit.hasPatch?0xc0000:0));wr(out+0x6c,hit.patchU);wr(out+0x70,hit.patchV);wr(0xc000a,int16_t(hit.patchFlags));wr(0xc0150,hit.patchId);
 c->f[0]=hit.fraction;c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){PS2Runtime rt;std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> ram((std::istreambuf_iterator<char>(input)),{});if(ram.size()!=32*1024*1024)return 2;
 rt.registerFunction(0x113198,sub_00113198_0x113198);
 rt.registerFunction(0x113200,sub_00113200_0x113200);rt.registerFunction(0x113618,sub_00113618_0x113618);rt.registerFunction(0x1139A0,sub_001139A0_0x1139a0);rt.registerFunction(0x32E100,sub_0032E100_0x32e100);
 rt.registerFunction(0x336850,[](uint8_t*m,R5900Context*c,PS2Runtime*){query(m,c,0);});rt.registerFunction(0x3378C0,[](uint8_t*m,R5900Context*c,PS2Runtime*){query(m,c,2);});
 auto wr=[&](unsigned a,auto x){std::memcpy(ram.data()+a,&x,sizeof(x));};auto rd=[&](unsigned a){float x;std::memcpy(&x,ram.data()+a,4);return x;};auto ri=[&](unsigned a){int x;std::memcpy(&x,ram.data()+a,4);return x;};
 wr(0x4a30f0-0x848,uint32_t(0x90000));wr(0x90084,uint32_t(0x90100));wr(0x90120,uint32_t(0xa0000));
 std::fesetround(FE_TOWARDZERO);std::mt19937 rng(0x113648);std::uniform_real_distribution<float> v(-1,1);
 auto vec=[&](){return V{v(rng)*2000,v(rng)*2000,v(rng)*2000};};
 for(unsigned k=0;k<10000;k++){
  ssx::OriginalAirTrajectory s;s.hitPosition=vec();s.heading=vec();s.normal={0,0,1};s.apexPosition=vec();s.patchId=123;s.patchU=.2;s.patchV=.7;s.surface=3;s.patchFlags=73;
  s.prediction={vec(),vec()};s.integrated={vec(),vec()};s.predictedTime=k%9==0?61:float(k%200)*.05f;s.apexTime=.7;s.elapsed=float(k%100)*.05f;s.integratedTime=s.elapsed;s.speedLimit=1000+float(k%3000);s.status=k%4;
  ssx::OriginalAirState current{vec(),vec()};float seconds=std::bit_cast<float>(0x3c888889u)*(float(k%5)+1)*.5f;
  hit.fraction=k%3==0?-1:.35;hit.position=vec();hit.normal={0,0,k%2?1.f:-1.f};hit.surface=7;hit.hasPatch=k%2;hit.patchFlags=73;hit.patchId=321;hit.patchU=.35;hit.patchV=.25;
  std::fill(ram.begin()+0x30000,ram.begin()+0x300b0,0);
  auto vw=[&](unsigned a,V x,bool position=false){wr(a,x);wr(a+12,position?1.f:0.f);};
  vw(0x30000,s.hitPosition,true);vw(0x30010,s.heading);vw(0x30020,s.normal);vw(0x30040,s.apexPosition,true);wr(0x30030,s.patchId);wr(0x30034,s.patchU);wr(0x30038,s.patchV);
  vw(0x30050,s.prediction.position,true);vw(0x30060,s.prediction.velocity);vw(0x30070,s.integrated.position,true);vw(0x30080,s.integrated.velocity);wr(0x30090,s.surface);wr(0x30094,int16_t(s.patchFlags));wr(0x30098,s.predictedTime);wr(0x3009c,s.apexTime);wr(0x300a0,s.elapsed);wr(0x300a4,s.integratedTime);wr(0x300a8,s.speedLimit);wr(0x300ac,s.status);vw(0x40000,current.position,true);vw(0x50000,current.velocity);
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);SET_GPR_U32(&c,4,0x30000);SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,6,0x50000);c.f[12]=seconds;requests.clear();
  if(k%11==0){c.f[12]=s.speedLimit;sub_001135B8_0x1135b8(ram.data(),&c,&rt);}else sub_00113648_0x113648(ram.data(),&c,&rt);size_t requestIndex=0;
  auto nativeQuery=[&](V end,V start,int mode){if(requestIndex>=requests.size()){printf("extra query %u\n",k);exit(3);}auto&r=requests[requestIndex++];
   // Original ray materializes start as end+(start-end), so compare the same
   // VU operation order rather than falsely demanding cancellation-free start.
   for(int i=0;i<3;i++){float reconstructed=end[i]+(start[i]-end[i]);if(r.end[i]!=end[i]||r.start[i]!=reconstructed||r.mode!=mode){printf("query mismatch %u/%zu/%d %.9g %.9g vs %.9g %.9g\n",k,requestIndex,i,r.end[i],r.start[i],end[i],reconstructed);exit(4);}}
   return hit;};
  ssx::OriginalAirState result=current;if(k%11==0)s.begin(current,s.speedLimit);else result=s.step(seconds,current,nativeQuery);
  if(requestIndex!=requests.size()){printf("missing query%u\n",k);return 5;}
  auto check=[&](unsigned a,auto value){for(int i=0;i<int(value.size());i++)if(value[i]!=rd(a+4*i)){printf("state mismatch %u @%x[%d] expected%.9g native%.9g\n",k,a,i,rd(a+4*i),value[i]);exit(6);}};
  check(0x30000,s.hitPosition);check(0x30010,s.heading);check(0x30020,s.normal);check(0x30040,s.apexPosition);check(0x30050,s.prediction.position);check(0x30060,s.prediction.velocity);check(0x30070,s.integrated.position);check(0x30080,s.integrated.velocity);check(0x40000,result.position);check(0x50000,result.velocity);
  check(0x30098,std::array<float,5>{s.predictedTime,s.apexTime,s.elapsed,s.integratedTime,s.speedLimit});check(0x30034,std::array<float,2>{s.patchU,s.patchV});
  int16_t flags;std::memcpy(&flags,ram.data()+0x30094,2);if(s.status!=ri(0x300ac)||s.surface!=ri(0x30090)||s.patchId!=ri(0x30030)||s.patchFlags!=flags){printf("integer state mismatch%u\n",k);return 7;}
 }
 puts("10000 original trajectory predictor/update states and ordered query requests match exactly");
}
