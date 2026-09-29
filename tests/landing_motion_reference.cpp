#include "ps2_runtime_macros.h"
#include "../engine/landing_motion.hpp"
#include "../engine/ground_pose_motion.hpp"
#include <fstream>
#include <cstdio>
#include <random>
#include <cfenv>
void sub_0013D818_0x13d818(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00139C88_0x139c88(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00106538_0x106538(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00113998_0x113998(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0013C7A8_0x13c7a8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0013F410_0x13f410(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0013A8F8_0x13a8f8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0013A968_0x13a968(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int landingOracleStage=0;static bool recovery,randomConsumed;static int animationClass,animationRequest,animationIndex,controlState;static float landingStat;static uint32_t randomWord,tick;
int main(int argc,char**argv){
 PS2Runtime rt;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(file)),{});if(m.size()!=32*1024*1024)return 2;
 auto wr=[&](unsigned a,auto x){std::memcpy(m.data()+a,&x,sizeof(x));};auto rd=[&](unsigned a){float x;std::memcpy(&x,m.data()+a,4);return x;};auto ri=[&](unsigned a){uint32_t x;std::memcpy(&x,m.data()+a,4);return x;};
 auto ctx=[&](unsigned pc){R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);SET_GPR_U32(&c,16,0x20000);SET_GPR_U32(&c,4,0x20000);wr(0x100f0,uint64_t(0x12345678));return c;};
 rt.registerFunction(0x312aa0,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_S32(c,2,animationIndex);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x11fee8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_S32(c,2,controlState);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x113998,sub_00113998_0x113998);rt.registerFunction(0x106538,sub_00106538_0x106538);
 for(unsigned pc:{0x329b40u,0x14dc80u,0x14dd58u})rt.registerFunction(pc,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x116120,[](uint8_t*,R5900Context*c,PS2Runtime*){recovery=true;c->pc=GPR_U32(c,31);});rt.registerFunction(0x311ae8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_S32(c,2,animationClass);c->pc=GPR_U32(c,31);});rt.registerFunction(0x311b20,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0x60000);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x1446a0,[](uint8_t*m,R5900Context*c,PS2Runtime*){uint64_t flags;std::memcpy(&flags,m+GPR_U32(c,4),8);SET_GPR_U32(c,2,(flags>>(GPR_U32(c,5)&63))&1);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x149120,[](uint8_t*,R5900Context*c,PS2Runtime*){c->f[0]=landingStat;c->pc=GPR_U32(c,31);});rt.registerFunction(0x317810,[](uint8_t*,R5900Context*c,PS2Runtime*){randomConsumed=true;SET_GPR_U32(c,2,randomWord);c->pc=GPR_U32(c,31);});rt.registerFunction(0x1298c8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,tick);c->pc=GPR_U32(c,31);});rt.registerFunction(0x3128e8,[](uint8_t*,R5900Context*c,PS2Runtime*){animationRequest=GPR_U32(c,5);c->pc=GPR_U32(c,31);});
 wr(0x4a30f0-0x848,0xa0000u);wr(0xa0084,0xa1000u);wr(0xa1044,0x90000u);wr(0x20004,0x30000u);wr(0x20018,0x30000u);wr(0x30788,0x70000u);wr(0x30780,0x80000u);
 std::fesetround(FE_TOWARDZERO);std::mt19937 rng(0x139c88);std::uniform_real_distribution<float> f(-1,1);using V=std::array<float,3>;
 auto unit=[&](){V v={f(rng),f(rng),f(rng)};float norm=std::sqrt((v[0]*v[0]+v[1]*v[1])+v[2]*v[2]);for(float& x:v)x/=norm;return v;};auto vw=[&](unsigned a,V v){wr(a,v);wr(a+12,0.f);};
 auto check=[&](unsigned at,V v,unsigned k,const char* name){for(int i=0;i<3;i++)if(rd(at+i*4)!=v[i]){printf("%s %u/%d expected%.9g native%.9g\n",name,k,i,rd(at+i*4),v[i]);exit(3);}};
 for(unsigned k=0;k<20000;k++){
  ssx::OriginalLandingState s;s.rider.position={f(rng)*1e5f,f(rng)*1e5f,f(rng)*1e5f};s.rider.velocity={f(rng)*5000,f(rng)*5000,f(rng)*5000};s.rider.physicalForward=unit();s.patchId=123;s.patchU=.2;s.patchV=.3;s.trajectoryPredictionTime=3;
  ssx::OriginalWorldSegmentHit h;h.position=s.rider.position;h.normal=unit();for(int i=0;i<3;i++){h.position[i]+=f(rng)*50;h.surfaceVelocityCmps[i]=f(rng)*200;}h.surface=0;h.hasPatch=k%2;h.patchFlags=k%11?73:2;h.patchId=421;h.patchU=.55;h.patchV=.66;
  ssx::OriginalLandingMaterial mat{10,std::abs(f(rng))*20,std::abs(f(rng))*2,std::abs(f(rng))*100,k%17==0?1u:0u};float impact=-std::abs(f(rng))*2500;
  vw(0x30110,s.rider.position);vw(0x301e0,s.rider.velocity);vw(0x301b0,s.rider.physicalForward);wr(0x30430,s.patchId);wr(0x30aac,s.patchU);wr(0x30ab0,s.patchV);wr(0x70098,s.trajectoryPredictionTime);vw(0x10000,h.position);vw(0x10010,h.normal);vw(0x10020,h.surfaceVelocityCmps);wr(0x1004c,h.surface);wr(0x10050,0u);wr(0x10054,h.hasPatch?0x50000u:0u);wr(0x1006c,h.patchU);wr(0x10070,h.patchV);wr(0x5000a,int16_t(h.patchFlags));wr(0x50150,h.patchId);wr(0x90018,mat.depth3);wr(0x9003c,mat.normalImpulseFactor);wr(0x90040,mat.maximumNormalSpeed);wr(0x90044,mat.recovery);
  auto c=ctx(0x139d78);c.f[21]=impact;landingOracleStage=1;recovery=false;sub_00139C88_0x139c88(m.data(),&c,&rt);bool accepted=ssx::originalLandingResolveContact(s,h,mat,impact);
  if(c.pc!=0x12345678||accepted==recovery){printf("contact branch%u pc%x\n",k,c.pc);return 4;}if(rd(0x70098)!=s.trajectoryPredictionTime)return 5;
  if(accepted){check(0x30110,s.rider.position,k,"position");check(0x301e0,s.rider.velocity,k,"velocity");check(0x30370,s.rider.normal,k,"normal");check(0x30380,s.rider.previousNormal,k,"oldnormal");check(0x303d0,s.rider.surfaceVelocity,k,"surface velocity");check(0x303a0,s.rider.forward,k,"forward");check(0x303b0,s.rider.lateral,k,"lateral");check(0x30460,s.groundPoint,k,"groundpoint");if(rd(0x30454)!=s.rider.distance||rd(0x30770)!=s.impactNormalSpeed||ri(0x30430)!=s.patchId||rd(0x30aac)!=s.patchU||rd(0x30ab0)!=s.patchV)return 6;}
 }
 puts("20000 original landing contact-response stages match exactly");
 for(unsigned k=0;k<20000;k++){
  ssx::OriginalLandingState s;s.rider.normal=unit();s.rider.forward=unit();s.rider.physicalForward=unit();s.rider.boardUp=unit();s.physicalRight=unit();s.rider.velocity={f(rng)*4000,f(rng)*4000,f(rng)*4000};
  ssx::OriginalLandingClassification options;options.animationClass=animationClass=k%24;options.animationFlags=rng()%16;options.manualState330=k%3;options.landingStat=landingStat=std::abs(f(rng));options.randomWord=randomWord=rng();
  vw(0x30370,s.rider.normal);vw(0x303a0,s.rider.forward);vw(0x301b0,s.rider.physicalForward);vw(0x301c0,s.rider.boardUp);vw(0x301a0,s.physicalRight);vw(0x301e0,s.rider.velocity);wr(0x30330,options.manualState330);wr(0x600b0,options.animationFlags);
  auto c=ctx(0x13a14c);SET_GPR_U32(&c,17,0x1b6);landingOracleStage=2;randomConsumed=false;sub_00139C88_0x139c88(m.data(),&c,&rt);auto result=ssx::originalLandingClassify(s,options);auto* cp=&c;
  if(c.pc!=0x12345678||int(GPR_U32(cp,17))!=result.crashAnimation||randomConsumed!=result.consumedRandom){printf("classify%u expected%x native%x\n",k,GPR_U32(cp,17),result.crashAnimation);return 7;}
 }
 puts("20000 original landing crash classifications match exactly");
 for(unsigned k=0;k<20000;k++){
  ssx::OriginalGroundState s;s.velocity={f(rng)*4000,f(rng)*4000,f(rng)*4000};s.normal=unit();ssx::OriginalLandingMaterial mat;mat.depth1=std::abs(f(rng))*50;mat.depth3=mat.depth1+1;float scale=.5f+std::abs(f(rng));uint32_t leave=k%13?uint32_t(k):0xffffffffu;tick=k+unsigned(k%100);
  wr(0x20014,leave);wr(0x80140,scale);wr(0x90014,mat.depth1);wr(0x90018,mat.depth3);wr(0x30438,0);vw(0x301e0,s.velocity);vw(0x30370,s.normal);
  auto c=ctx(0x13c7a8);sub_0013C7A8_0x13c7a8(m.data(),&c,&rt);ssx::originalLandingGroundEnter(s,mat,scale,tick,leave);check(0x301e0,s.velocity,k,"ground entry velocity");check(0x30390,s.boardNormal,k,"ground entry board normal");if(rd(0x20004)!=s.depth1||rd(0x20008)!=s.depth3||rd(0x20000)!=s.boardBouncePhase)return 8;
  wr(0x20018,0x30000u);wr(0x20014,leave);c=ctx(0x13f410);sub_0013F410_0x13f410(m.data(),&c,&rt);ssx::originalLandingGroundLeave(s,tick,leave);if(ri(0x20014)!=leave||rd(0x302c0)!=s.boardAlignment.rate||rd(0x302c4)!=s.boardAlignment.target||rd(0x3020c)!=s.extraLean.rate||rd(0x30210)!=s.extraLean.target||rd(0x302cc)!=s.presentationLift.rate||rd(0x302d0)!=s.presentationLift.target)return 9;
  // Ground enter overwrites owner+4 depth; restore the separate air-mode link.
  wr(0x20004,0x30000u);float impact=-std::abs(f(rng))*3000,spin=f(rng)*10;wr(0x302dc,spin);c=ctx(0x13a968);c.f[12]=impact;animationRequest=-1;sub_0013A968_0x13a968(m.data(),&c,&rt);int clip=ssx::originalLandingAnimation(impact,spin);if(clip!=animationRequest||spin!=rd(0x302dc))return 10;
  spin=f(rng)*10;wr(0x302dc,spin);c=ctx(0x13a8f8);c.f[12]=impact;animationRequest=-1;sub_0013A8F8_0x13a8f8(m.data(),&c,&rt);clip=ssx::originalReverseLandingAnimation(impact,spin);if(clip!=animationRequest||spin!=rd(0x302dc))return 11;
 }
 puts("20000 original ground focus/leave and landing animation choices match exactly");
 for(unsigned k=0;k<20000;k++){
  ssx::OriginalGroundProfile p;ssx::OriginalGroundState s;p.surface.id=k%19;s.animationClass=animationClass=k%24;s.animationIndex=animationIndex=k%30;s.controlState=controlState=k%14;
  s.extraLean={f(rng),f(rng),f(rng)};s.boardAlignment={f(rng),f(rng),f(rng)};
  float dt=(.5f+std::abs(f(rng)))/60;V velocity={f(rng)*3000,f(rng)*3000,f(rng)*3000};
  p.extraLeanCurve={{{0,f(rng)},{30,f(rng)},{60,f(rng)},{120,f(rng)}}};
  wr(0x4a30f0-0x1fc8,0x91000u);wr(0x91000,p.extraLeanCurve);wr(0x20018,0x30000u);wr(0x30438,p.surface.id);wr(0x30208,s.extraLean);wr(0x302bc,s.boardAlignment);vw(0x10000,velocity);
  auto c=ctx(0x13eea0);SET_GPR_U32(&c,17,0x20000);c.f[25]=dt;sub_0013D818_0x13d818(m.data(),&c,&rt);ssx::originalGroundVisualTargets(p,s,dt,velocity);
  if(c.pc!=0x12345678||rd(0x30208)!=s.extraLean.current||rd(0x3020c)!=s.extraLean.rate||rd(0x30210)!=s.extraLean.target||rd(0x302bc)!=s.boardAlignment.current||rd(0x302c0)!=s.boardAlignment.rate||rd(0x302c4)!=s.boardAlignment.target){printf("ground visual targets mismatch%u class%d id%d control%d surface%d expected%.9g %.9g %.9g %.9g actual%.9g %.9g %.9g %.9g\n",k,animationClass,animationIndex,controlState,p.surface.id,rd(0x3020c),rd(0x30210),rd(0x302c0),rd(0x302c4),s.extraLean.rate,s.extraLean.target,s.boardAlignment.rate,s.boardAlignment.target);return 12;}
 }
 puts("20000 original ground board/lean target stages match exactly");

}
