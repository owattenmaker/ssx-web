#include "ps2_runtime_macros.h"
#include "../engine/start_control.hpp"
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
#define DECL(name,address) void sub_##name##_0x##address(uint8_t*,R5900Context*,PS2Runtime*);
DECL(0012BF68,12bf68) DECL(0012C230,12c230) DECL(0012C408,12c408) DECL(0012C0C0,12c0c0) DECL(0012C130,12c130) DECL(0012C028,12c028) DECL(0012C078,12c078)
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
using Action=ssx::OriginalStartAction;static ssx::OriginalStartControlInput input;static int motion;static std::vector<Action> actions;
static void boundary(uint8_t*m,R5900Context*c,PS2Runtime*){
 switch(c->pc){
 case 0x114130:if(GPR_U32(c,5)||GPR_U32(c,6))throw std::runtime_error("Start requested boost");actions.push_back({Action::StopBoost});break;
 case 0x115ab0:c->f[0]=input.tilt;actions.push_back({Action::Tilt,0,input.tilt});break;
 case 0x113f88:if(c->f[13]!=0)throw std::runtime_error("Unexpected start brake");actions.push_back({Action::Crouch,0,c->f[12]});break;
 case 0x11fe98:SET_GPR_U32(c,2,motion);break;
 case 0x11fe78:{ssx::terrain_original::Vector v;std::memcpy(v.data(),m+0x201e0,12);actions.push_back({Action::Velocity,0,0,v});motion=int(GPR_U32(c,5));actions.push_back({Action::Motion,motion});break;}
 case 0x11fec8:actions.push_back({Action::Control,int(GPR_U32(c,5))});break;
 case 0x3128e8:if(c->f[12]!=-1||GPR_U32(c,6))throw std::runtime_error("Unexpected start animation arguments");actions.push_back({Action::Play,int(GPR_U32(c,5))});break;
 case 0x312aa0:SET_GPR_U32(c,2,input.semantic);break;
 case 0x312ab0:c->f[0]=input.mainTime;break;
 }
 c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>m((std::istreambuf_iterator<char>(f)),{});if(m.size()!=32*1024*1024)return 2;PS2Runtime rt;std::mt19937 rng(0x12bf68);
 for(unsigned pc:{0x114130u,0x115ab0u,0x113f88u,0x11fe98u,0x11fe78u,0x11fec8u,0x3128e8u,0x312aa0u,0x312ab0u})rt.registerFunction(pc,boundary);
#define REG(name,address) rt.registerFunction(0x##address,sub_##name##_0x##address);
 REG(0012C230,12c230) REG(0012C408,12c408) REG(0012C0C0,12c0c0) REG(0012C130,12c130) REG(0012C028,12c028) REG(0012C078,12c078)
 auto put=[&](unsigned a,const auto&v){std::memcpy(m.data()+a,&v,sizeof(v));};auto get=[&](unsigned a){uint32_t x;std::memcpy(&x,m.data()+a,4);return x;};auto value=[&](){return float(int(rng()%3000)-1000)*.001f;};
 const unsigned capturedOwner=get(0x14701a0+0x77c),world=get(get(0x4a28a8)+0x84),game=get(world+12),capturedTicks=get(game+8);
 if(get(game)!=4||get(game+12)!=0||get(capturedOwner+0xde4)!=6)throw std::runtime_error("Expected original countdown capture");
 ssx::OriginalStartControlState captured;std::memcpy(&captured,m.data()+capturedOwner+0x290,sizeof(captured));
 put(0x10014,0x20000u);put(0x4a28a8,0x40000u);put(0x40084,0x50000u);put(0x5000c,0x30000u);
 for(unsigned n=0;n<20000;n++){
  ssx::OriginalStartControlState s{int(rng()%6)-1,value(),value(),value(),value()};input={};input.command=rng();input.gamePhase=rng()%8;input.raceTicks=rng()%600;input.delaySeconds=rng()%4;input.motion=motion=rng()%5;input.semantic=rng()%4;input.mainTime=value();input.tilt=value();input.finishElapsed=value();for(auto&x:input.forward)x=value();
  put(0x10000,s);put(0x30000,input.gamePhase);put(0x3000c,input.raceTicks);put(0x20b30,input.delaySeconds);put(0x20470,input.finishElapsed);put(0x201b0,input.forward);put(0x201bc,0.f);put(0x60000,input.command);std::array<float,3> pose{3,4,5};put(0x202c8,pose);actions.clear();
  R5900Context c{};c.pc=0x12bf68;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x10000);SET_GPR_U32(&c,5,0x60000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x90000);SET_GPR_U32(&c,31,0x12345678);
  const auto before=s;ssx::OriginalRounding rounding;sub_0012BF68_0x12bf68(m.data(),&c,&rt);auto expected=ssx::originalStartControlStep(s,input);bool okay=c.pc==0x12345678&&!std::memcmp(m.data()+0x10000,&s,sizeof(s))&&actions.size()==expected.size();
  if(okay)for(unsigned k=0;k<expected.size();k++){const auto&a=actions[k],&b=expected[k];okay&=a.kind==b.kind&&a.value==b.value&&a.amount==b.amount&&a.vector==b.vector;if(b.kind==Action::Tilt)pose={b.amount,0,b.amount};}
  okay&=!std::memcmp(m.data()+0x202c8,pose.data(),12);
  if(!okay){printf("Start control mismatch %u phase%d/%u actions%zu/%zu pc%x\n",n,s.phase,get(0x10000),actions.size(),expected.size(),c.pc);printf("before phase%d time%g lo%g hi%g pose%g axis%x game%d tick%d delay%d\n",before.phase,before.steadyTime,before.low,before.high,before.pose,input.command,input.gamePhase,input.raceTicks,input.delaySeconds);for(unsigned k=0;k<5;k++)printf("field%u %08x/%08x\n",k,get(0x10000+k*4),reinterpret_cast<const uint32_t*>(&s)[k]);return 3;}
 }
 ssx::OriginalStartControlState fresh;ssx::originalStartControlEnter(fresh,false);ssx::OriginalStartControlInput neutral;
 for(unsigned tick=0;tick<capturedTicks;tick++)ssx::originalStartControlStep(fresh,neutral);
 if(std::memcmp(&fresh,&captured,sizeof(fresh)))throw std::runtime_error("Fresh start did not reproduce captured neutral countdown pose");
 printf("Fresh control6 entry plus%u neutral ticks reproduces the captured start state exactly\n",capturedTicks);
 puts("20,000 original control6 start updates match all phases, targets, release gates, velocity and ordered callbacks");
}
