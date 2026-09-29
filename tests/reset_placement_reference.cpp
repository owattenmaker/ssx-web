#include "ps2_runtime_macros.h"
#include "../engine/reset_placement.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <cstring>
#include <cfenv>
#define DECLARE(a,b) void sub_##a##_0x##b(uint8_t*,R5900Context*,PS2Runtime*);
DECLARE(0011D660,11d660) DECLARE(0011E098,11e098) DECLARE(0011DFE0,11dfe0)
DECLARE(0031BE50,31be50) DECLARE(0031C128,31c128) DECLARE(0031C228,31c228)
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
using V=std::array<float,3>;
static ssx::OriginalWorldSegmentHit supplied;static V actualEnd,actualStart;static float preferred;static int flags,queries;
template<class T>T load(uint8_t*m,unsigned p){T v;std::memcpy(&v,m+p,sizeof(v));return v;}
template<class T>void put(uint8_t*m,unsigned p,T v){std::memcpy(m+p,&v,sizeof(v));}
static void external(uint8_t*m,R5900Context*c,PS2Runtime*){
 if(c->pc==0x32e100){actualEnd=load<V>(m,GPR_U32(c,5));actualStart=load<V>(m,GPR_U32(c,6));preferred=c->f[12];flags=GPR_U32(c,7);}
 else{++queries;auto out=GPR_U32(c,6);put(m,out,supplied.position);put(m,out+12,1.f);put(m,out+16,supplied.normal);put(m,out+28,0.f);c->f[0]=supplied.fraction;}
 c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){
 if(argc!=2)return 1;PS2Runtime rt;
#define REGISTER(a,b) rt.registerFunction(0x##b,sub_##a##_0x##b);
 REGISTER(0011E098,11e098) REGISTER(0011DFE0,11dfe0) REGISTER(0031BE50,31be50) REGISTER(0031C128,31c128) REGISTER(0031C228,31c228)
 rt.registerFunction(0x32e100,external);rt.registerFunction(0x336850,external);
 std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> memory((std::istreambuf_iterator<char>(file)),{});if(memory.size()!=32*1024*1024)return 2;auto*m=memory.data();
 std::fesetround(FE_TOWARDZERO);std::mt19937 random(0x11d660);std::uniform_real_distribution<float> coordinate(-100000,100000),unit(-1,1);
 constexpr unsigned actor=0x40000,stack=0x10000,pointAt=0x50000,directionAt=0x50010;
 put(m,actor+0x864,0x60000u);put(m,actor+0x868,0x60100u);put(m,0x4a30f0-0x848,0x70000u);put(m,0x70084,0x71000u);put(m,0x71020,0x72000u);
 for(unsigned trial=0;trial<20000;trial++){
  V point{coordinate(random),coordinate(random),coordinate(random)},direction{unit(random),unit(random),unit(random)};
  if(trial%7==0)direction[0]=0;if(trial%11==0)direction[1]=0;
  supplied={};supplied.position={coordinate(random),coordinate(random),coordinate(random)};supplied.normal={unit(random)*.6f,unit(random)*.6f,1};
  float length=std::sqrt(supplied.normal[0]*supplied.normal[0]+supplied.normal[1]*supplied.normal[1]+1);for(auto& x:supplied.normal)x/=length;
  if(trial%9==0)supplied.normal={0,0,1};if(trial%13==0)supplied.normal={0,0,-1};
  supplied.fraction=trial%3?float(random()%1001)/1000.f:-1.f;
  float clearance=trial%4==0?-200.f:trial%4==1?0.f:trial%4==2?200.f:1000.f;
  put(m,pointAt,point);put(m,pointAt+12,1.f);put(m,directionAt,direction);put(m,directionAt+12,0.f);
  std::memset(m+0x60000,0x77,16);std::memset(m+0x60100,0x88,16);queries=0;
  R5900Context c{};c.pc=0x11d660;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);c.f[12]=clearance;
  SET_GPR_U32(&c,4,actor);SET_GPR_U32(&c,5,pointAt);SET_GPR_U32(&c,6,directionAt);SET_GPR_U32(&c,7,287);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,31,0x12345678);
  sub_0011D660_0x11d660(m,&c,&rt);int nativeQueries=0;
  auto actual=ssx::originalResetPlacement(point,direction,clearance,[&](V end,V start,float fraction){++nativeQueries;if(end!=actualEnd||start!=actualStart||fraction!=preferred||flags!=2)throw std::runtime_error("Reset probe arguments differ");return supplied;});
  if(c.pc!=0x12345678||nativeQueries!=queries||actual.queried!=(queries!=0)||actual.hit!=(queries&&supplied.fraction>=0))return 3;
  for(unsigned k=0;k<3;k++){
   if(actual.position[k]!=load<float>(m,actor+0x110+k*4)||actual.normal[k]!=load<float>(m,actor+0x370+k*4)||actual.forward[k]!=load<float>(m,actor+0x3a0+k*4)||actual.lateral[k]!=load<float>(m,actor+0x3b0+k*4)){
    printf("reset basis mismatch %u/%u pos %.9g/%.9g forward %.9g/%.9g lateral %.9g/%.9g\n",trial,k,actual.position[k],load<float>(m,actor+0x110+k*4),actual.forward[k],load<float>(m,actor+0x3a0+k*4),actual.lateral[k],load<float>(m,actor+0x3b0+k*4));return 4;
   }
  }
  for(unsigned k=0;k<4;k++)if(actual.physical.quaternion[k]!=load<float>(m,actor+0x120+k*4)){printf("reset quaternion mismatch %u/%u %.9g/%.9g\n",trial,k,actual.physical.quaternion[k],load<float>(m,actor+0x120+k*4));return 5;}
  for(unsigned p:{0x60000u,0x60100u})for(unsigned k=0;k<16;k++)if(m[p+k])return 6;
 }
 printf("20000 original reset placement/basis stages exact: probe endpoints/flags/preference, hit/miss, signed clearance, heading quadrants, slope quaternion and tangents; both source caches cleared\n");
}
