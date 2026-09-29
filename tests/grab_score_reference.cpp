#include "ps2_runtime_macros.h"
#include "../engine/grab_score.hpp"
#include <fstream>
#include <cfenv>
#include <cstdio>
#include <random>
#include <bit>
#include "grab_score_registry.inc"
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static std::vector<int> cleared;static std::optional<ssx::OriginalGrabScoreBonus> bonus;
int main(int argc,char**argv){
 setbuf(stdout,nullptr);PS2Runtime rt;registerScoreOriginal(rt);std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(file)),{});if(m.size()!=32*1024*1024)return 2;
 auto u=[&](unsigned a){uint32_t v;std::memcpy(&v,m.data()+a,4);return v;};auto f=[&](unsigned a){return std::bit_cast<float>(u(a));};auto w=[&](unsigned a,auto v){std::memcpy(m.data()+a,&v,sizeof(v));};
 rt.registerFunction(0x3e6448,[](uint8_t*m,R5900Context*c,PS2Runtime*){std::memset(m+GPR_U32(c,4),GPR_U32(c,5),GPR_U32(c,6));c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x1179e0,[](uint8_t*,R5900Context*c,PS2Runtime*){cleared.push_back(GPR_S32(c,5));c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x117b88,[](uint8_t*,R5900Context*c,PS2Runtime*){if(bonus)throw std::runtime_error("Duplicate original grab bonus event");bonus=ssx::OriginalGrabScoreBonus{GPR_S32(c,5),GPR_S32(c,6),GPR_S32(c,7),c->f[12]};c->pc=GPR_U32(c,31);});
 auto context=[](uint32_t pc,uint32_t a0,uint32_t a1=0,uint32_t a2=0){R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,a0);SET_GPR_U32(&c,5,a1);SET_GPR_U32(&c,6,a2);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);return c;};
 auto run=[&](R5900Context& c){for(unsigned n=0;n<32;++n){auto fn=rt.lookupFunction(c.pc);if(!fn)throw std::runtime_error("Missing original score PC "+std::to_string(c.pc));fn(m.data(),&c,&rt);if(c.pc==0x12345678)return;}throw std::runtime_error("Score continuation limit");};
 ssx::OriginalGrabScoreProfile profile;for(unsigned n=0;n<4;++n)profile.holdThresholds[n]={f(0x459e20+n*8),f(0x459e24+n*8)};
 for(unsigned index=0;index<15;++index){auto c=context(0x150118,0,0,index);run(c);auto*cp=&c;unsigned id=GPR_U32(cp,2);profile.normal[index]={int32_t(id),int32_t(u(0x530600+id*8)),int32_t(u(0x530604+id*8))};}
 std::mt19937 rng(0x119708);std::uniform_real_distribution<float> value(-3,12);std::fesetround(FE_TOWARDZERO);unsigned bonuses=0;
 for(unsigned k=0;k<20000;++k){
  ssx::OriginalGrabScoreState s;s.accumulated14=value(rng);s.holdIncrement3C=value(rng);s.holdSeconds40=k%5==0?-1.f:value(rng);s.totalSeconds44=value(rng);s.longestSeconds48=value(rng);s.normalCount4C=int32_t(rng());s.tweakCount50=int32_t(rng());s.uberCount54=int32_t(rng());s.superUberCount58=int32_t(rng());s.activeUber5C=k%3;for(auto&id:s.history60)id=rng()%5;s.bonusPoints84=int32_t(rng());s.holdThresholdIndex8C=k%4;s.comboTimeoutA4=value(rng);s.multiplier1C4=value(rng);
  auto writeState=[&](){const std::pair<unsigned,float> fs[]={{0x14,s.accumulated14},{0x3c,s.holdIncrement3C},{0x40,s.holdSeconds40},{0x44,s.totalSeconds44},{0x48,s.longestSeconds48},{0xa4,s.comboTimeoutA4},{0x1c4,s.multiplier1C4}};for(auto [off,x]:fs)w(0x20000+off,x);const std::pair<unsigned,int32_t> is[]={{0x4c,s.normalCount4C},{0x50,s.tweakCount50},{0x54,s.uberCount54},{0x58,s.superUberCount58},{0x5c,s.activeUber5C},{0x84,s.bonusPoints84},{0x8c,s.holdThresholdIndex8C}};for(auto[off,x]:is)w(0x20000+off,x);for(unsigned n=0;n<3;++n)w(0x20060+n*4,s.history60[n]);w(0x201ac,0x30000u);};
  auto equal=[&](){bool okay=true;const std::pair<unsigned,float> fs[]={{0x14,s.accumulated14},{0x3c,s.holdIncrement3C},{0x40,s.holdSeconds40},{0x44,s.totalSeconds44},{0x48,s.longestSeconds48},{0xa4,s.comboTimeoutA4},{0x1c4,s.multiplier1C4}};for(auto [off,x]:fs)if(u(0x20000+off)!=std::bit_cast<uint32_t>(x)){printf("Score float mismatchcase%u off%x original%08x native%08x\n",k,off,u(0x20000+off),std::bit_cast<uint32_t>(x));okay=false;}const std::pair<unsigned,int32_t> is[]={{0x4c,s.normalCount4C},{0x50,s.tweakCount50},{0x54,s.uberCount54},{0x58,s.superUberCount58},{0x5c,s.activeUber5C},{0x84,s.bonusPoints84},{0x8c,s.holdThresholdIndex8C}};for(auto[off,x]:is)if(u(0x20000+off)!=uint32_t(x))okay=false;for(unsigned n=0;n<3;++n)if(u(0x20060+n*4)!=uint32_t(s.history60[n])){printf("History mismatch%u/%u\n",k,n);okay=false;}return okay;};
  ssx::OriginalGrabScoreRule rule=k%3?profile.normal[k%15]:ssx::OriginalGrabScoreRule{int32_t(k%60),int32_t(rng()%12000)-1000,int32_t(rng()%9000)-1000};w(0x530600+rule.scoreId*8,rule.beginPoints);w(0x530604+rule.scoreId*8,rule.holdPoints);writeState();auto c=context(0x119708,0x20000,rule.scoreId);run(c);float result=ssx::originalGrabScoreBegin(s,rule);if(std::bit_cast<uint32_t>(c.f[0])||std::bit_cast<uint32_t>(result)||!equal()){puts("Grab begin mismatch");return 3;}
  auto actorBefore=std::vector<uint8_t>(m.begin()+0x30000,m.begin()+0x30b40);auto b=context(0x10e098,0x30000,2);b.f[12]=c.f[0];run(b);if(!std::equal(actorBefore.begin(),actorBefore.end(),m.begin()+0x30000)){puts("Grab begin unexpectedly changes boost actor");return 4;}
  s.holdSeconds40=value(rng);writeState();int tier=int(k%13)-2;w(0x302f4,tier);c=context(0x1197d8,0x20000,rule.scoreId);run(c);result=ssx::originalGrabScoreEnd(s,rule.scoreId,tier);if(std::bit_cast<uint32_t>(c.f[0])||std::bit_cast<uint32_t>(result)||!equal()){puts("Grab end mismatch");return 5;}
  actorBefore.assign(m.begin()+0x30000,m.begin()+0x30b40);b=context(0x10e098,0x30000,2);b.f[12]=c.f[0];run(b);if(!std::equal(actorBefore.begin(),actorBefore.end(),m.begin()+0x30000)){puts("Grab end unexpectedly changes boost actor");return 6;}
  s.holdSeconds40=k%4?value(rng):-1;writeState();float timeScale=value(rng);cleared.clear();bonus.reset();c=context(0x117d24,0x20000);SET_GPR_U32(&c,16,0x20000);c.f[20]=timeScale*std::bit_cast<float>(0x3c888889u);c.f[21]=0;sub_00117C28_0x117c28(m.data(),&c,&rt);if(c.pc!=0x12345678){printf("Held tick continuation%x\n",c.pc);return 7;}
  auto effect=ssx::originalGrabScoreTick(s,profile,timeScale);if(!equal()||bool(effect)!=bool(bonus)){puts("Grab tick mismatch");return 8;}
  if(effect){++bonuses;if(cleared!=std::vector<int>({28,29,30,31,32})||effect->eventType!=bonus->eventType||effect->points!=bonus->points||effect->thresholdSeconds!=bonus->thresholdSeconds||effect->displaySeconds!=bonus->displaySeconds){puts("Grab bonus event mismatch");return 9;}}else if(!cleared.empty())return 10;
  c=context(0x117948,0x20000);run(c);auto*cp=&c;if(GPR_S32(cp,2)!=ssx::originalCurrentTrickPoints(s)){puts("Pending trick points mismatch");return 11;}
  c=context(0x117838,0x20000);run(c);ssx::originalResetGrabScore(s);if(!equal()){puts("Original trick reset mismatch");return 12;}
 }
 printf("20000 original grab begins,ends,held ticks and point getters match exactly (%u authored bonus events); both immediate10E098 calls leave the complete rider object unchanged\n",bonuses);
}
