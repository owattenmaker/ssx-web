#include "ps2_runtime_macros.h"
#include "../engine/boost_ribbon.hpp"
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
void sub_002E6C08_0x2e6c08(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};static int semantic;
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>m((std::istreambuf_iterator<char>(f)),{});if(m.size()!=32*1024*1024)return 2;PS2Runtime rt;std::mt19937 rng(0x2e6c08);
 auto put=[&](unsigned a,const auto&v){std::memcpy(m.data()+a,&v,sizeof(v));};auto word=[&](unsigned a){uint32_t x;std::memcpy(&x,m.data()+a,4);return x;};
 auto value=[&](){return float(int(rng()%800000)-400000)*.01f;};auto quad=[&](float w){ssx::BoostHistoryQuad v;for(auto&x:v)x=value();v[3]=w;return v;};
 rt.registerFunction(0x140000,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0x60000);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x312aa0,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,semantic);c->pc=GPR_U32(c,31);});
 put(0x10000,0x20000u);put(0x206c0,0x30000u);put(0x30028,uint16_t(0));put(0x3002c,0x140000u);put(0x20780,0x40000u);put(0x40030,0x50000u);put(0x208a4,0u);
 static_assert(sizeof(ssx::OriginalBoostRibbonRow)==28);
 unsigned advances=0;
 for(unsigned n=0;n<20000;n++){
  ssx::OriginalBoostEffectState state;state.trailLength=value();state.primaryDistance=value();state.primaryCount=rng()%31;
  ssx::OriginalBoostRibbonHistory h;h.cursor=rng()%30;h.previousPosition=quad(1);
  for(auto&row:h.rows){row.distance=value();for(auto&x:row.a)x=value();for(auto&x:row.b)x=value();}
  ssx::OriginalBoostRibbonInput input;input.velocity=n%9?quad(0):ssx::BoostHistoryQuad{};input.position=quad(1);input.boardPosition=quad(1);input.boardX=quad(0);input.boardZ=quad(0);input.switchStance=n%2;input.flag330=n%3;input.semantic=semantic=rng()%50;
  put(0x10004,state.trailLength);put(0x10014,state.primaryCount);put(0x10018,state.primaryDistance);put(0x10020,h.previousPosition);put(0x10030,h.cursor);put(0x10050,h.rows);
  put(0x201e0,input.velocity);put(0x20320,int(input.switchStance));put(0x20330,int(input.flag330));put(0x50000,input.boardX);put(0x50020,input.boardZ);put(0x50030,input.boardPosition);put(0x60000,input.position);
  R5900Context c{};c.pc=0x2e6c08;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x10000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x90000);SET_GPR_U32(&c,31,0x12345678);
  ssx::OriginalRounding rounding;sub_002E6C08_0x2e6c08(m.data(),&c,&rt);const auto seed=ssx::originalBoostRibbonSeed(state,h,input);
  bool okay=c.pc==(seed.advanced?0x12345678u:0x87654321u)&&word(0x10030)==uint32_t(h.cursor)&&word(0x10014)==uint32_t(state.primaryCount)&&word(0x10018)==std::bit_cast<uint32_t>(state.primaryDistance)&&!std::memcmp(m.data()+0x10020,h.previousPosition.data(),16)&&!std::memcmp(m.data()+0x10050,h.rows.data(),sizeof(h.rows));
  if(seed.advanced){++advances;okay&=!std::memcmp(m.data()+0x90000-0x150+0x40,seed.corners.data(),sizeof(seed.corners));}
  if(!okay){printf("Boost ribbon seed mismatch %u pc%x active%d count%d/%d cursor%u/%d length%.9g/%.9g\n",n,c.pc,seed.advanced,int(word(0x10014)),state.primaryCount,word(0x10030),h.cursor,std::bit_cast<float>(word(0x10018)),state.primaryDistance);return 3;}
 }
 printf("20,000 original boost ribbon seeds match history, distance and four anchors; %u advanced\n",advances);
}
