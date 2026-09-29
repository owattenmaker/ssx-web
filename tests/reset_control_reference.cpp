#include "ps2_runtime_macros.h"
#include "../engine/reset_control.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <cstring>
#include <cfenv>
void sub_0012F398_0x12f398(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0012F588_0x12f588(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
using V=ssx::terrain_original::Vector;
struct Call{int kind=0,argument=0;float scalar=0;V vector{};bool operator==(const Call&)const=default;};
static std::vector<Call> actual,expected;static V direction;static float score;static bool eventMode;
template<class T>T load(uint8_t*m,unsigned p){T v;std::memcpy(&v,m+p,sizeof(v));return v;}
template<class T>void put(uint8_t*m,unsigned p,T v){std::memcpy(m+p,&v,sizeof(v));}
static void external(uint8_t*m,R5900Context*c,PS2Runtime*){
 switch(c->pc){
 case 0x112d58:actual.push_back({1});break;
 case 0x26a8b8:actual.push_back({2});put(m,GPR_U32(c,4),direction);put(m,GPR_U32(c,4)+12,0.f);break;
 case 0x14dc80:SET_GPR_U32(c,2,0x50000);break;
 case 0x14dd58:SET_GPR_U32(c,2,eventMode);break;
 case 0x11d660:actual.push_back({3,int(GPR_U32(c,7)),c->f[12],load<V>(m,GPR_U32(c,6))});break;
 case 0x11df18:actual.push_back({4,int(GPR_U32(c,5))});break;
 case 0x119368:actual.push_back({5,int(GPR_U32(c,5))});c->f[0]=score;break;
 case 0x10e098:actual.push_back({6,int(GPR_U32(c,5)),c->f[12]});break;
 case 0x230698:SET_GPR_U32(c,2,0x60000);break;
 case 0x2e4578:actual.push_back({7});break;
 case 0x119e38:actual.push_back({8,int(GPR_U32(c,5))});break;
 case 0x311b20:actual.push_back({9,int(GPR_U32(c,5))});SET_GPR_U32(c,2,0x70000);break;
 case 0x11fec8:actual.push_back({10,int(GPR_U32(c,5))});break;
 case 0x11fe78:actual.push_back({11,int(GPR_U32(c,5))});break;
 }
 c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){
 if(argc!=2)return 1;PS2Runtime rt;for(unsigned pc:{0x112d58,0x26a8b8,0x14dc80,0x14dd58,0x11d660,0x11df18,0x119368,0x10e098,0x230698,0x2e4578,0x119e38,0x311b20,0x11fec8,0x11fe78})rt.registerFunction(pc,external);rt.registerFunction(0x12f588,sub_0012F588_0x12f588);
 std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> memory((std::istreambuf_iterator<char>(file)),{});if(memory.size()!=32*1024*1024)return 2;auto*m=memory.data();
 constexpr unsigned control=0x20000,actor=0x40000;put(m,control+8,actor);put(m,0x4a30f0-0x848,0x50000u);put(m,0x50084,0x60000u);
 std::fesetround(FE_TOWARDZERO);std::mt19937 random(0x12f398);std::uniform_real_distribution<float> progress(-.5f,1.5f),value(-1,1);unsigned placements=0,completions=0;
 for(unsigned trial=0;trial<20000;trial++){
  ssx::OriginalResetControlState state{bool(trial%2),progress(random)};if(trial%5==0)state.progress=.5f;if(trial%7==0)state.progress=1;
  ssx::OriginalResetControlInputs input;input.timeScale=trial%4?float(random()%400)/100.f:0;input.eventModeActive=eventMode=trial%3;input.eventVariant=trial%4;input.deviceEnabled=trial%2;input.deviceIndex=int(trial%4)-1;
  direction={value(random),value(random),value(random)};score=value(random);put(m,control,int(state.reasonNonzero));put(m,control+4,state.progress);put(m,actor+0x300,input.timeScale);put(m,actor+0x87c,int(input.deviceEnabled));put(m,actor+0x870,input.deviceIndex);put(m,0x535c12,int8_t(input.eventVariant));put(m,0x70090,.37f);actual.clear();expected.clear();
  R5900Context c{};c.pc=0x12f398;SET_GPR_U32(&c,4,control);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_0012F398_0x12f398(m,&c,&rt);
  ssx::OriginalResetControlCallbacks cb;cb.refreshRoute=[](){expected.push_back({1});};cb.routeDirection=[](){expected.push_back({2});return direction;};
  cb.place=[&](V v,float height,int semantic){++placements;expected.push_back({3,semantic,height,v});};cb.postPlacement=[](){expected.push_back({4,1});};
  cb.resetScore=[](bool penalize){expected.push_back({5,int(penalize)});return score;};cb.awardBoost=[](float delta){expected.push_back({6,1,delta});};cb.finishDeviceFade=[](){expected.push_back({7});};cb.clearScoringStance=[](){expected.push_back({8,0});};
  cb.setAnimationRate=[&](float rate){++completions;expected.push_back({9,2});if(rate!=1||load<float>(m,0x70090)!=1)throw std::runtime_error("Reset animation rate differs");};cb.enterControl=[](int state){expected.push_back({10,state});};cb.enterMotion=[](int mode){expected.push_back({11,mode});};
  ssx::originalResetControlStep(state,input,cb);
  if(c.pc!=0x12345678||state.progress!=load<float>(m,control+4)||actual!=expected){printf("reset control mismatch %u progress %.9g/%.9g calls %zu/%zu\n",trial,state.progress,load<float>(m,control+4),actual.size(),expected.size());return 3;}
 }
 ssx::OriginalResetControlState state;ssx::OriginalResetControlInputs input;eventMode=false;
 put(m,control,0);put(m,control+4,0.f);put(m,actor+0x300,1.f);put(m,actor+0x87c,0);put(m,actor+0x870,-1);put(m,0x535c12,int8_t(0));
 int currentTick=0,placedAt=0,finishedAt=0;
 ssx::OriginalResetControlCallbacks cb;cb.refreshRoute=[](){};cb.routeDirection=[](){return direction;};cb.place=[&](V,float,int){placedAt=currentTick;};cb.postPlacement=[](){};cb.resetScore=[](bool){return score;};cb.awardBoost=[](float){};cb.finishDeviceFade=[](){};cb.clearScoringStance=[](){};cb.setAnimationRate=[](float){};cb.enterControl=[](int){};cb.enterMotion=[&](int){finishedAt=currentTick;};
 for(currentTick=1;currentTick<=100&&!finishedAt;currentTick++){
  actual.clear();R5900Context c{};c.pc=0x12f398;SET_GPR_U32(&c,4,control);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_0012F398_0x12f398(m,&c,&rt);
  ssx::originalResetControlStep(state,input,cb);bool originalPlaced=false,originalFinished=false;for(auto call:actual){originalPlaced|=call.kind==3;originalFinished|=call.kind==11;}
  if(c.pc!=0x12345678||state.progress!=load<float>(m,control+4)||originalPlaced!=(placedAt==currentTick)||originalFinished!=(finishedAt==currentTick))return 4;
 }
 if(!placedAt||!finishedAt)return 5;
 printf("Original reset timeline from zero at scale1: placement tick%d, passive-air entry tick%d, every intermediate progress value exact\n",placedAt,finishedAt);
 printf("20000 original reset-control updates exact: %u placement crossings, %u completions; progress, clearances, score/device callbacks and ordered control4/motion1 reentry\n",placements,completions);
}
