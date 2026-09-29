#include "ps2_runtime_macros.h"
#include "../engine/wake_control.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <cstring>
#include <cfenv>
void sub_002DD0B8_0x2dd0b8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
using V=ssx::terrain_original::Vector;
template<class T>T get(uint8_t*m,unsigned p){T v;std::memcpy(&v,m+p,sizeof(v));return v;}
template<class T>void put(uint8_t*m,unsigned p,T v){std::memcpy(m+p,&v,sizeof(v));}
static ssx::OriginalWakeControlState readState(uint8_t*m,unsigned owner){ssx::OriginalWakeControlState s;s.normal60=get<V>(m,owner+0x60);s.side70=get<V>(m,owner+0x70);s.direction80=get<V>(m,owner+0x80);s.point50=get<V>(m,owner+0x50);s.amplitude90=get<float>(m,owner+0x90);s.alpha94=get<float>(m,owner+0x94);s.active3C=get<int>(m,owner+0x3c);s.counter40=get<int>(m,owner+0x40);s.positiveSideB0=get<int>(m,owner+0xb0);return s;}
struct Call{int kind=0;V point{};float value=0;ssx::OriginalWakeControlState state;bool operator==(const Call&)const=default;};
static int mode;static std::vector<Call> actual,expected;
static void external(uint8_t*m,R5900Context*c,PS2Runtime*){
 if(c->pc==0x11fe98)SET_GPR_U32(c,2,mode);
 else{Call call;call.kind=c->pc==0x2de058?0:1;call.state=readState(m,GPR_U32(c,4));if(call.kind){call.point=get<V>(m,GPR_U32(c,5));call.value=c->f[12];}actual.push_back(call);}
 c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){
 if(argc!=2)return 1;PS2Runtime runtime;for(unsigned pc:{0x11fe98,0x2de058,0x2ddd30})runtime.registerFunction(pc,external);
 std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> memory((std::istreambuf_iterator<char>(file)),{});if(memory.size()!=32*1024*1024)return 2;auto*m=memory.data();
 constexpr unsigned owner=0x20000,actor=0x40000,stack=0x10000;put(m,owner,actor);
 std::fesetround(FE_TOWARDZERO);std::mt19937 random(0x2dd6f4);std::uniform_real_distribution<float> unit(0,1),value(-1,1);unsigned births=0,advances=0;
 for(unsigned trial=0;trial<30000;trial++){
  ssx::OriginalWakeControlState state;ssx::OriginalWakeTargets target;float roll=value(random)*.3f;mode=trial%4;
  state.active3C=(trial/4)%2;state.positiveSideB0=trial%3;state.counter40=random()%50;state.amplitude90=unit(random)*1200;state.alpha94=unit(random);
  target.amplitude=unit(random)*1200;target.alpha=unit(random);target.growth=unit(random)*4.5f;target.signedTurn=trial%9?value(random):0;target.turnAmount=unit(random);
  for(unsigned k=0;k<3;k++){state.normal60[k]=value(random);state.side70[k]=value(random);state.direction80[k]=value(random);state.point50[k]=value(random)*10000;target.normal[k]=value(random);target.side[k]=value(random);target.direction[k]=value(random);target.point[k]=state.point50[k]+value(random)*100;}
  auto vec=[&](unsigned at,V v,float w=0){put(m,at,v);put(m,at+12,w);};
  vec(owner+0x60,state.normal60);vec(owner+0x70,state.side70);vec(owner+0x80,state.direction80);vec(owner+0x50,state.point50,1);put(m,owner+0x90,state.amplitude90);put(m,owner+0x94,state.alpha94);put(m,owner+0x3c,int(state.active3C));put(m,owner+0xb0,int(state.positiveSideB0));put(m,owner+0x40,state.counter40);put(m,actor+0x274,roll);
  vec(stack,target.normal);vec(stack+0x10,target.side);vec(stack+0x20,target.direction);vec(stack+0x50,target.point,1);put(m,stack+0x30,target.amplitude);put(m,stack+0x34,target.alpha);put(m,stack+0x100,uint64_t(0x12345678));actual.clear();expected.clear();
  R5900Context c{};c.pc=0x2dd6f4;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);c.f[20]=target.growth;c.f[21]=target.signedTurn;c.f[22]=target.turnAmount;SET_GPR_U32(&c,2,mode);SET_GPR_U32(&c,16,1);SET_GPR_U32(&c,18,owner);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,stack);sub_002DD0B8_0x2dd0b8(m,&c,&runtime);
  ssx::OriginalWakeControlCallbacks cb;cb.advanceRow=[&](){++advances;expected.push_back({0,{},0,state});};cb.createRow=[&](V p,float v){++births;expected.push_back({1,p,v,state});};ssx::originalWakeControl(state,target,mode,roll,cb);
  if(c.pc!=0x12345678||state!=readState(m,owner)||expected!=actual){printf("wake control mismatch %u mode%d active%d calls%zu/%zu amp%.9g/%.9g alpha%.9g/%.9g\n",trial,mode,state.active3C,expected.size(),actual.size(),state.amplitude90,get<float>(m,owner+0x90),state.alpha94,get<float>(m,owner+0x94));return 3;}
 }
 printf("30000 original wake-control stages exact: eligibility, side transitions, filters and ordered row callbacks (%u advances, %u births)\n",advances,births);
}
