#include "ps2_runtime_macros.h"
#include "../engine/boost_effect.hpp"
#include <cstring>
#include <random>
#include <fstream>
#include <cstdio>
void sub_002E66B8_0x2e66b8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};static int mode;
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t>m((std::istreambuf_iterator<char>(file)),{});if(m.size()!=32*1024*1024)return 2;
 PS2Runtime rt;rt.registerFunction(0x11fe98,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,mode);c->pc=GPR_U32(c,31);});
 auto put=[&](unsigned a,const auto&v){std::memcpy(m.data()+a,&v,sizeof(v));};auto get=[&](unsigned a){uint32_t v;std::memcpy(&v,m.data()+a,4);return v;};
 std::mt19937 rng(0x2e66b8);std::uniform_real_distribution<float> value(-2,2);put(0x10000,0x20000u);
 for(unsigned n=0;n<20000;n++){
  ssx::OriginalBoostEffectState s;s.trailLength=value(rng)*1000;s.alpha=value(rng);s.width=value(rng)*50;s.primaryCount=rng()%31;s.primaryDistance=value(rng);s.trailCount=rng()%20;s.wasAirborne=rng()%2;
  ssx::OriginalBoostEffectInput i;for(auto&v:i.velocity)v=value(rng)*2000;i.boost=value(rng);i.pickup=value(rng);i.motionMode=mode=rng()%6;i.tier=int(rng()%15)-1;i.forcePickup=n%13==0;i.forceHigh=n%17==0;i.forceMedium=n%19==0;
  put(0x10004,s.trailLength);put(0x10008,s.alpha);put(0x1000c,s.width);put(0x10014,s.primaryCount);put(0x10018,s.primaryDistance);put(0x103a0,s.trailCount);put(0x103a8,int(s.wasAirborne));put(0x201e0,i.velocity);put(0x201ec,0.f);put(0x202fc,i.boost);put(0x202e8,i.pickup);put(0x202f4,i.tier);
  put(0x4a30f0+0x1640,int(i.forcePickup));put(0x4a30f0+0x1644,int(i.forceHigh));put(0x4a30f0+0x1648,int(i.forceMedium));
  R5900Context c{};c.pc=0x2e66b8;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x10000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x90000);SET_GPR_U32(&c,31,0x12345678);
  ssx::OriginalRounding rounding;sub_002E66B8_0x2e66b8(m.data(),&c,&rt);auto expected=ssx::originalBoostEffectParameters(s,i);
  bool okay=c.pc==0x12345678&&GPR_U32((&c),6)==unsigned(expected.emitting)&&c.f[4]==expected.scrollStep&&c.f[5]==expected.speed&&get(0x10398)==unsigned(s.texture)&&get(0x103a0)==unsigned(s.trailCount)&&get(0x103a8)==unsigned(s.wasAirborne);
  okay&=get(0x10014)==uint32_t(s.primaryCount);
  for(auto pair:{std::pair{0x10004u,s.trailLength},{0x10008u,s.alpha},{0x1000cu,s.width},{0x10018u,s.primaryDistance}})okay&=get(pair.first)==std::bit_cast<uint32_t>(pair.second);
  if(!okay){printf("Boost FX parameters mismatch %u texture%d/%u\n",n,s.texture,get(0x10398));printf("mode%d boost%g pickup%g active%u/%d rate%.9g/%.9g speed%.9g/%.9g\n",i.motionMode,i.boost,i.pickup,GPR_U32((&c),6),expected.emitting,c.f[4],expected.scrollStep,c.f[5],expected.speed);for(auto pair:{std::pair{0x10004u,s.trailLength},{0x10008u,s.alpha},{0x1000cu,s.width},{0x10018u,s.primaryDistance}})printf("%x source%.9g native%.9g\n",pair.first,std::bit_cast<float>(get(pair.first)),pair.second);printf("air%u/%d count%u/%d\n",get(0x103a8),s.wasAirborne,get(0x103a0),s.trailCount);return 3;}
 }
 puts("20,000 original boost FX parameter updates match all fields and activation/scroll outputs");
}
