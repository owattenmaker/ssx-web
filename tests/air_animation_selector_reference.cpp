#include "ps2_runtime_macros.h"
#include "../engine/air_animation_selector.hpp"
#include <fstream>
#include <cfenv>
#include <cstdio>
#include <random>
#include <bit>
void sub_00133308_0x133308(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static ssx::OriginalAirAnimationContext input;static std::array<float,439> durations;static std::vector<int> durationQueries;static int requested;static float requestedRate;
int main(int argc,char**argv){
 setbuf(stdout,nullptr);PS2Runtime rt;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(file)),{});if(m.size()!=32*1024*1024)return 2;
 auto u=[&](unsigned at){uint32_t x;std::memcpy(&x,m.data()+at,4);return x;};auto f=[&](unsigned at){return std::bit_cast<float>(u(at));};auto w=[&](unsigned at,auto x){std::memcpy(m.data()+at,&x,sizeof(x));};
 rt.registerFunction(0x31c228,sub_0031C228_0x31c228);
 rt.registerFunction(0x312aa0,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,input.mainSemantic);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x311ae8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,input.mainClass);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x312ae8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,input.mainCompleted);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x312790,[](uint8_t*,R5900Context*c,PS2Runtime*){unsigned semantic=GPR_U32(c,5);durationQueries.push_back(semantic);c->f[0]=durations.at(semantic);c->pc=GPR_U32(c,31);});
 for(unsigned pc:{0x14dc80u,0x14dd58u})rt.registerFunction(pc,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0x80000);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x1495a8,[](uint8_t*,R5900Context*c,PS2Runtime*){c->f[0]=input.trickStat;c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x3128e8,[](uint8_t*m,R5900Context*c,PS2Runtime*){if(requested!=-1||GPR_U32(c,6)||c->f[12]!=-1.f)throw std::runtime_error("Invalid air play request");requested=GPR_U32(c,5);memcpy(&requestedRate,m+GPR_U32(c,4)+0x1c,4);c->pc=GPR_U32(c,31);});
 w(0x50058,0x30000u);w(0x30784,0x60000u);w(0x30788,0x70000u);
 std::mt19937 rng(0x13437c);std::uniform_real_distribution<float> value(-1,1);std::fesetround(FE_TOWARDZERO);std::array<unsigned,439> outcomes{};unsigned queryCount=0,plays=0;
 for(unsigned k=0;k<60000;++k){
  ssx::OriginalAirControlState air;air.phase=k%4;air.targetSpin=value(rng)*12;air.targetFlip=value(rng)*18;air.progressSpin=value(rng)*12;air.progressFlip=value(rng)*18;air.adjustSpin=k%3==0?value(rng)*1.7f:0;air.adjustFlip=k%5==0?value(rng)*1.7f:0;air.maxSpin=std::abs(value(rng))*5;air.maxFlip=std::abs(value(rng))*5;air.extended=k%2;air.spinRate=value(rng)*6;air.flipRate=value(rng)*6;
  if(k%7==0)air.targetSpin=air.targetFlip=0;if(k%11==0){air.progressSpin=air.targetSpin;air.progressFlip=air.targetFlip;}
  ssx::OriginalAirControlFrame frame{int(rng()%4),value(rng),value(rng)};
  if(k%13==0)frame.effectiveSpin=0;if(k%17==0)frame.effectiveFlip=0;
  ssx::OriginalAirAnimationState state;state.adjustment28C={value(rng),value(rng),value(rng)};state.adjustment298={value(rng),value(rng),value(rng)};state.nextRate=value(rng)+1.5f;
  input.mainSemantic=k%3?268+int(rng()%46):std::array{287,288,305,75,22}[k%5];input.mainClass=int(u(0x446990+input.mainSemantic*28));input.mainCompleted=k%2;input.reverseStance=k%2;input.grabActive=k%19==0;input.trickStat=value(rng)+1;input.boostModifier=k%3==0?1:0;input.trajectoryStatus=k%5;input.predictedTime=value(rng)+1;input.elapsed=value(rng)+1;
  for(unsigned n=0;n<durations.size();++n)durations[n]=float((n+k)%50)*.05f;
  w(0x5000c,air.phase);w(0x50010,air.targetFlip);w(0x50014,air.targetSpin);w(0x50018,air.progressFlip);w(0x5001c,air.progressSpin);w(0x50028,air.adjustSpin);w(0x5002c,air.adjustFlip);w(0x50038,air.maxSpin);w(0x5003c,air.maxFlip);w(0x50044,air.extended);w(0x302dc,air.spinRate);w(0x302e0,air.flipRate);w(0x302ec,input.boostModifier);w(0x30320,int32_t(input.reverseStance));w(0x6001c,state.nextRate);w(0x70098,input.predictedTime);w(0x700a0,input.elapsed);w(0x700ac,input.trajectoryStatus);
  for(auto[at,c]:{std::pair{0x3028c,state.adjustment28C},std::pair{0x30298,state.adjustment298}}){w(at,c.current);w(at+4,c.rate);w(at+8,c.target);}w(0x10000,frame.effectiveFlip);w(0x10004,frame.effectiveSpin);
  requested=-1;requestedRate=0;durationQueries.clear();R5900Context c{};c.pc=0x13437c;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,16,0x50000);SET_GPR_U32(&c,18,input.grabActive);SET_GPR_U32(&c,19,frame.phaseBefore);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_00133308_0x133308(m.data(),&c,&rt);if(c.pc!=0x12345678){printf("Air selector continuation%x\n",c.pc);return 3;}
  auto queries=durationQueries;durationQueries.clear();ssx::OriginalAirAnimationAccess access;access.duration=[&](int semantic){durationQueries.push_back(semantic);return durations.at(semantic);};int nativeRequested=-1;float nativeRate=0,lastRate=state.nextRate;access.play=[&](int semantic,float rate){nativeRequested=semantic;nativeRate=rate;};access.setNextRate=[&](float rate){lastRate=rate;};
  auto result=ssx::originalSelectAirAnimation(air,frame,state,input,access);int selected=requested<0?input.mainSemantic:requested;++outcomes.at(selected);queryCount+=queries.size();plays+=requested>=0;
  if(nativeRequested!=requested||result.semantic!=selected||std::bit_cast<uint32_t>(nativeRate)!=std::bit_cast<uint32_t>(requestedRate)||queries!=durationQueries||u(0x6001c)!=std::bit_cast<uint32_t>(state.nextRate)||state.nextRate!=lastRate){printf("Air selector mismatch%u before%d after%d adjust%.9g/%.9g old%d/class%d expected%d/%.9g actual%d/%.9g queries%zu/%zu\n",k,frame.phaseBefore,air.phase,air.adjustSpin,air.adjustFlip,input.mainSemantic,input.mainClass,requested,requestedRate,nativeRequested,nativeRate,queries.size(),durationQueries.size());return 4;}
  for(auto[at,control]:{std::pair{0x3028c,state.adjustment28C},std::pair{0x30298,state.adjustment298}})for(auto[d,x]:{std::pair{0,control.current},std::pair{4,control.rate},std::pair{8,control.target}})if(u(at+d)!=std::bit_cast<uint32_t>(x)){printf("Air selector pose target mismatch%u at%x original%.9g native%.9g\n",k,at+d,f(at+d),x);return 5;}
 }
 printf("60000 complete original air animation selectors match semantic/playrate/pose controls/duration-query ordering (%u plays,%u semantic-duration queries)\n",plays,queryCount);for(unsigned n=0;n<outcomes.size();++n)if(outcomes[n])printf("semantic%u cases%u\n",n,outcomes[n]);
}
