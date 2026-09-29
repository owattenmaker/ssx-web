#include "ps2_runtime_macros.h"
#include "../engine/snow_context.hpp"
#include "../engine/terrain_contact_math.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <random>
#define FN(n) void sub_##n(uint8_t*,R5900Context*,PS2Runtime*);
FN(002DF920_0x2df920) FN(002E23E0_0x2e23e0) FN(002E2550_0x2e2550)
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x320000,g_ps2RecompiledFunctionTableSlotCount=0x88000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x88000]={};
using namespace ssx;template<class T>void wr(uint8_t*m,uint32_t p,const T&v){std::memcpy(m+p,&v,sizeof(v));}template<class T>T rd(uint8_t*m,uint32_t p){T v;std::memcpy(&v,m+p,sizeof(v));return v;}
static int control=0;static void external(uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,c->pc==0x11fee8?control:0);c->pc=GPR_U32(c,31);}
int main(int argc,char**argv){if(argc!=2)return 1;PS2Runtime runtime;std::vector<uint8_t> memory(32*1024*1024);auto*m=memory.data();std::ifstream f(argv[1],std::ios::binary);f.read((char*)m,memory.size());if(!f)return 2;
 runtime.registerFunction(0x11fee8,external);runtime.registerFunction(0x31fff0,external);runtime.registerFunction(0x2e23e0,sub_002E23E0_0x2e23e0);runtime.registerFunction(0x2e2550,sub_002E2550_0x2e2550);
 constexpr uint32_t actor=0x10000,fx=0x20000,geometry=0x30000,matrices=0x40000,emitters=0x50000,stack=0x70000,point=0x80000,normal=0x80010,table=0x90000;
 wr(m,fx,actor);wr(m,fx+0x28,emitters);wr(m,actor+0x780,geometry);wr(m,geometry+0x30,matrices);wr(m,actor+0x8a4,0);wr(m,actor+0x6c0,0xa0000u);wr(m,0xa0038,int16_t(0));wr(m,0xa003c,0x31fff0u);wr(m,rd<uint32_t>(m,rd<uint32_t>(m,0x4a30f0-0x848)+0x84)+0x44,table);
 auto vec=[&](uint32_t p,SnowVector v,float w=0){wr(m,p,v);wr(m,p+12,w);};
 std::mt19937 rng(0x43545853);std::uniform_real_distribution<float> unit(-1,1),positive(0,1);auto v=[&](float s){return SnowVector{unit(rng)*s,unit(rng)*s,unit(rng)*s};};
 for(int n=0;n<40000;++n){terrain_original::Rounding rounding;OriginalSnowContextState state;state.previousSpeedCmps=positive(rng)*6000;state.visibilityMode=rng()%4;state.impactSurface=rng()%19;state.impact.strength=positive(rng)*1000;state.impact.positionCm=v(300);state.impact.normal=v(1);state.impact.kind=rng()%2;state.impact.wideScatter=rng()%2;for(auto& e:state.emitterEnabled)e=rng()%2;
  OriginalSnowContextInput input;auto&i=input.rider;i.velocityCmps=v(3000);i.motionMode=rng()%5;i.controlState=control=rng()%14;i.turn=unit(rng);i.brake=unit(rng);i.trackingInhibited=rng()%2;i.trackingAD0=rng()%2;i.trackingAFC=rng()%2;i.trackingB00=rng()%2;input.board={v(1),v(1),v(1),v(300)};input.groundNormal=v(1);input.environmentARGB={unit(rng),unit(rng),unit(rng),unit(rng)};input.visibilityMode=rng()%4;
  OriginalSnowSurface surface;surface.id=rng()%19;auto cache=originalSnowRiderCache(i);
  wr(m,fx+0x74,state.previousSpeedCmps);wr(m,fx+0x128,state.visibilityMode);wr(m,fx+0x110,state.impactSurface);wr(m,fx+0xe0,state.impact.strength);vec(fx+0xf0,state.impact.positionCm,1);vec(fx+0x100,state.impact.normal);wr(m,fx+0x114,int(state.impact.kind));wr(m,fx+0x124,int(state.impact.wideScatter));for(int e=0;e<10;++e)wr(m,emitters+0x174+e*0x210,int(state.emitterEnabled[e]));
  wr(m,actor+0xac4,int(i.trackingInhibited));wr(m,actor+0xad0,int(i.trackingAD0));wr(m,actor+0xafc,int(i.trackingAFC));wr(m,actor+0xb00,int(i.trackingB00));wr(m,actor+0x898,input.visibilityMode);wr(m,actor+0x438,surface.id);vec(actor+0x370,input.groundNormal);wr(m,0x4fa398,input.environmentARGB);vec(matrices,input.board.right);vec(matrices+16,input.board.forward);vec(matrices+32,input.board.up);vec(matrices+48,input.board.originCm,1);wr(m,fx+0xb8,cache.speedCmps);wr(m,fx+0xb0,int(cache.groundEmission));
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,31,0x12345678);
  OriginalSnowContext result;
  if(n<20000){c.pc=0x2dfb60;SET_GPR_U32(&c,16,fx);sub_002DF920_0x2df920(m,&c,&runtime);result=originalSnowContextStep(state,input,surface);
   if(rd<SnowColour>(m,fx+0x90)!=result.trail.colour||rd<SnowVector>(m,fx+0x30)!=result.trail.board.forward||rd<SnowVector>(m,fx+0x40)!=result.trail.board.up||rd<SnowVector>(m,fx+0x50)!=result.trail.board.right||rd<SnowVector>(m,fx+0xa0)!=result.trail.board.originCm||rd<float>(m,fx+0x74)!=state.previousSpeedCmps||rd<int>(m,fx+0x128)!=state.visibilityMode||rd<uint32_t>(m,fx+0xd0)!=table+surface.id*0xb0)throw std::runtime_error("Original snow postprefix context mismatch");
   for(int e=0;e<10;++e)if(bool(rd<int>(m,emitters+0x174+e*0x210))!=state.emitterEnabled[e]){std::cerr<<"visibility "<<state.visibilityMode<<" wide "<<state.impact.wideScatter<<" emitter "<<e<<"\n";throw std::runtime_error("Original snow emitter visibility mismatch");}
  }else{auto p=v(300),normalValue=v(1);float strength=unit(rng)*1500;bool kind=rng()%2;vec(point,p,1);vec(normal,normalValue);SET_GPR_U32(&c,4,fx);SET_GPR_U32(&c,5,point);SET_GPR_U32(&c,6,normal);SET_GPR_U32(&c,7,surface.id);SET_GPR_U32(&c,8,kind);c.f[12]=strength;c.pc=0x2e23e0;sub_002E23E0_0x2e23e0(m,&c,&runtime);originalSnowImpactTrigger(state,p,normalValue,strength,surface.id,kind,i);}
  if(c.pc!=0x12345678||rd<float>(m,fx+0xe0)!=state.impact.strength||rd<SnowVector>(m,fx+0xf0)!=state.impact.positionCm||rd<SnowVector>(m,fx+0x100)!=state.impact.normal||bool(rd<int>(m,fx+0x114))!=state.impact.kind||rd<int>(m,fx+0x110)!=state.impactSurface){std::cerr<<"case "<<n<<" strength "<<rd<float>(m,fx+0xe0)<<'/'<<state.impact.strength<<"\n";throw std::runtime_error("Original snow impact trigger mismatch");}
 }
 std::cout<<"20000 original postprefix snow contexts and20000 complete impact triggers match colour, board/cache, visibility and pending impact state exactly\n";
}
