// Oracle: original 2E2260 (BodySnow, emitter 9) against engine/snow_crash.hpp.
#include "ps2_runtime_macros.h"
#include "../engine/snow_crash.hpp"
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
#include <cfenv>
#include <bit>
void sub_002E2260_0x2e2260(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x380000,g_ps2RecompiledFunctionTableSlotCount=0xa0000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xa0000]={};
static std::vector<ssx::OriginalSnowEmission> births;
static void callback(uint8_t*m,R5900Context*c,PS2Runtime*){
 if(c->pc==0x3717c0){ssx::OriginalSnowEmission e;e.active=GPR_U32(c,8);e.emitter=(GPR_U32(c,4)-0xa0000)/0x210;e.stepSeconds=c->f[12];memcpy(e.positionCm.data(),m+GPR_U32(c,5),12);
  if(auto p=GPR_U32(c,6)){ssx::SnowVector v;memcpy(v.data(),m+p,12);e.velocityCmps=v;}if(auto p=GPR_U32(c,7)){ssx::SnowColour v;memcpy(v.data(),m+p,16);e.colour=v;}births.push_back(e);}
 else throw std::runtime_error("Unknown BodySnow oracle callback");
 c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){
 if(argc!=2)return 2;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>m((std::istreambuf_iterator<char>(in)),{});if(m.size()!=32*1024*1024)return 3;
 PS2Runtime rt;rt.registerFunction(0x3717c0,callback);
 auto put=[&](unsigned at,const auto&v){memcpy(m.data()+at,&v,sizeof(v));};auto u=[&](unsigned at){uint32_t v;memcpy(&v,m.data()+at,4);return v;};
 auto vector=[&](unsigned at,ssx::SnowVector v,float w=0){put(at,v);put(at+12,w);};
 constexpr unsigned fx=0x30000,table=0x31000,rider=0x40000,geometry=0x48000,profile=0x60000,matrices=0x200000,done=0x12345678;
 // gp constants (gp-3A4C/3A48/3A44) come from the real fixture memory at 49F6A4..49F6AC.
 if(u(0x49f6a4)!=0x42a6aaabu||u(0x49f6a8)!=0x3c888889u||u(0x49f6ac)!=0x3c888889u){puts("Fixture gp constants differ from the recovered values");return 4;}
 put(fx,rider);put(fx+0x28,0xa0000u);put(fx+0x2c,profile);put(fx+0x7c,table);put(rider+0x780,geometry);put(geometry+0x30,matrices);
 std::mt19937 gen(0x2e2260);std::uniform_real_distribution<float> unit(-1,1);unsigned active=0,decayed=0;
 for(unsigned i=0;i<20000;++i){std::fesetround(FE_TOWARDZERO);
  ssx::OriginalSnowTrailProfile p;p.velocityScale=i%9==0?.800000011920929f:unit(gen);ssx::OriginalSnowBodyState state;state.cursor=int(i%31==0?29:gen()%30);
  ssx::OriginalSnowImpactState impact;impact.buildup=i%13==0?0:(i%17==0?.6666666865348816f*.01666666753590107f:std::abs(unit(gen))*2.2f);
  ssx::OriginalSnowBodyContext c;c.speedCmps=i%11==0?83.33333587646484f:std::abs(unit(gen))*3000;c.motionMode=int(i%6);for(auto&v:c.velocityCmps)v=unit(gen)*3000;for(auto&x:c.colour)x=unit(gen)*2;
  ssx::OriginalSnowBodyBoneTable bones;for(auto&b:bones)b=int(gen()%32);
  for(unsigned b=0;b<32;++b){ssx::SnowVector origin{unit(gen)*100000,unit(gen)*100000,unit(gen)*100000};vector(matrices+b*64+0x30,origin,1);for(unsigned k=0;k<30;++k)if(bones[k]==int(b))c.boneOriginsCm[k]=origin;}
  unsigned primary=gen()%32;ssx::SnowVector primaryOrigin;memcpy(primaryOrigin.data(),m.data()+matrices+primary*64+0x30,12);c.primaryBoneOriginCm=primaryOrigin;
  put(rider+0x89c,primary);for(unsigned k=0;k<30;++k)put(table+k*4,uint32_t(bones[k]));
  put(fx+4,impact.buildup);put(fx+0x78,uint32_t(state.cursor));put(fx+0x90,c.colour);put(fx+0xb8,c.speedCmps);put(fx+0xd4,c.motionMode);vector(rider+0x1e0,c.velocityCmps,unit(gen));put(profile+0x900,p.velocityScale);
  R5900Context ctx{};ctx.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32((&ctx),4,fx);SET_GPR_U32((&ctx),28,0x4a30f0);SET_GPR_U32((&ctx),29,0x10000);SET_GPR_U32((&ctx),31,done);
  births.clear();sub_002E2260_0x2e2260(m.data(),&ctx,&rt);float before=impact.buildup;
  auto actual=ssx::originalSnowBodyEmission(p,state,impact,c);
  bool okay=ctx.pc==done&&births.size()==1;
  if(okay){auto&b=births[0];okay=actual.active==b.active&&actual.emitter==b.emitter&&actual.positionCm==b.positionCm&&actual.velocityCmps==b.velocityCmps&&actual.colour==b.colour&&actual.stepSeconds==b.stepSeconds;}
  okay=okay&&std::bit_cast<uint32_t>(impact.buildup)==u(fx+4)&&uint32_t(state.cursor)==u(fx+0x78);
  if(!okay){printf("BodySnow mismatch%u births%zu active%d/%d buildup%08x/%08x cursor%d/%u\n",i,births.size(),actual.active,births.empty()?-1:int(births[0].active),std::bit_cast<uint32_t>(impact.buildup),u(fx+4),state.cursor,u(fx+0x78));
   if(!births.empty())for(unsigned k=0;k<3;++k)printf(" pos%.9g/%.9g vel%.9g/%.9g\n",actual.positionCm[k],births[0].positionCm[k],actual.velocityCmps?actual.velocityCmps->at(k):0,births[0].velocityCmps?births[0].velocityCmps->at(k):0);return 5;}
  active+=actual.active;decayed+=impact.buildup!=before;
 }
 printf("20,000 full original2E2260 BodySnow requests exact: gates, cycling bone, velocity, colour/alpha, dt, cursor and buildup decay (%u active, %u decays)\n",active,decayed);
 return 0;
}
