#include "ps2_runtime_macros.h"
#include "../engine/wake_physics.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <cstring>
#include <cfenv>
void sub_002DD0B8_0x2dd0b8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002DE058_0x2de058(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002DDD30_0x2ddd30(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002D1928_0x2d1928(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002D18B0_0x2d18b0(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
using V=ssx::terrain_original::Vector;
template<class T>T get(uint8_t*m,unsigned p){T v;std::memcpy(&v,m+p,sizeof(v));return v;}
template<class T>void put(uint8_t*m,unsigned p,T v){std::memcpy(m+p,&v,sizeof(v));}
static int mode;
static void advance(uint8_t*m,R5900Context*c,PS2Runtime* runtime){
 // Execute the original cursor prefix in a scratch CPU context. Its later
 // colour/UV initialization is outside this physics-cache comparison.
 auto scratch=*c;scratch.pc=0x2de058;sub_002DE058_0x2de058(m,&scratch,runtime);if(scratch.pc!=0x12345678)throw std::runtime_error("Wake cursor prefix escaped");c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){
 if(argc!=2)return 1;PS2Runtime runtime;runtime.registerFunction(0x2de058,advance);runtime.registerFunction(0x2ddd30,sub_002DDD30_0x2ddd30);runtime.registerFunction(0x2d1928,sub_002D1928_0x2d1928);runtime.registerFunction(0x2d18b0,sub_002D18B0_0x2d18b0);runtime.registerFunction(0x11fe98,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,mode);c->pc=GPR_U32(c,31);});
 std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> memory((std::istreambuf_iterator<char>(file)),{});if(memory.size()!=32*1024*1024)return 2;auto*m=memory.data();
 constexpr unsigned owner=0x20000,actor=0x40000,geometry=0x50000,unitBoard=0x60000,scaledBoard=0x61000,surface=0x70000,records=0x90000;
 const auto noise=get<ssx::OriginalWakeNoiseTable>(m,0x445ab0);std::fesetround(FE_TOWARDZERO);unsigned populated=0;
 for(int scenario=0;scenario<20;scenario++){
  ssx::OriginalWakePhysics state;state.reset(scenario%2);state.noise=noise;std::memset(m+owner,0,0xc0);std::memset(m+records,0,32*0x70);
  put(m,owner,actor);put(m,owner+0xc,records);put(m,owner+0xb4,state.profile.columns);put(m,owner+0xb8,state.profile.capacity);put(m,owner+0xbc,state.profile.lifetime);put(m,owner+0x24,state.profile.verticalIncrement);
  put(m,actor+0x780,geometry);put(m,actor+0x8a4,0u);put(m,geometry+0x30,unitBoard);put(m,geometry+0x34,scaledBoard);put(m,actor+0x438,0u);put(m,0x4a30f0-0x848,0xa0000u);put(m,0xa0084,0xa1000u);put(m,0xa1044,surface);
  for(unsigned b=0;b<5;b++){put(m,owner+0x10+b*4,0xb0000u+b*0x2000);std::memset(m+0xb0000+b*0x2000,0,32*48);}
  for(int tick=0;tick<500;tick++){
   ssx::OriginalWakeFrameInput in;in.normal={0,0,1};in.lateral={0,1,0};in.velocity=tick%17?V{1400,800,-20}:V{};in.unitAxis0=in.scaledAxis0={1,0,0};in.unitAxis2={0,1,0};in.boardPosition={float(tick*30-10000),float(scenario*100),1500};in.turn1F0=(tick/23)%2?.9f:-.9f;in.lean208=.1f;in.brake214=tick%29<5?.8f:0;in.reverse320=scenario%3==0;in.surfaceAlpha48=1;in.surfaceScale4C=.5f+float(scenario%3)*.2f;in.surfaceFlag84=scenario%2;float roll=tick%37==0?.2f:0;mode=tick%60>=50?1:0;
   auto vec=[&](unsigned at,V v,float w=0){put(m,at,v);put(m,at+12,w);};vec(actor+0x370,in.normal);vec(actor+0x3b0,in.lateral);vec(actor+0x1e0,in.velocity);vec(unitBoard,in.unitAxis0);vec(unitBoard+0x20,in.unitAxis2);vec(unitBoard+0x30,in.boardPosition,1);vec(scaledBoard,in.scaledAxis0);
   put(m,actor+0x1f0,in.turn1F0);put(m,actor+0x208,in.lean208);put(m,actor+0x214,in.brake214);put(m,actor+0x274,roll);put(m,actor+0x320,int(in.reverse320));put(m,surface+0x48,in.surfaceAlpha48);put(m,surface+0x4c,in.surfaceScale4C);put(m,surface+0x84,int(in.surfaceFlag84));
   R5900Context c{};c.pc=0x2dd0b8;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,owner);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_002DD0B8_0x2dd0b8(m,&c,&runtime);state.step(in,mode,roll);
   if(c.pc!=0x12345678||state.cursor.count!=get<int>(m,owner+4)||state.cursor.head!=get<int>(m,owner+8)||state.cursor.phase!=get<float>(m,owner+0x38)||int(state.control.active3C)!=get<int>(m,owner+0x3c)||state.control.amplitude90!=get<float>(m,owner+0x90)||state.control.alpha94!=get<float>(m,owner+0x94)){printf("wake frame state mismatch %d/%d\n",scenario,tick);return 3;}
   for(int row=0;row<state.cursor.capacity;row++){
    unsigned at=records+row*0x70;const auto& r=state.rows[row];if(r.age64!=get<float>(m,at+0x64)||r.value60!=get<float>(m,at+0x60))return 4;
    for(int band=0;band<state.profile.columns;band++)for(unsigned k=0;k<3;k++)if(r.velocity[band][k]!=get<float>(m,at+band*16+k*4)||r.positions[band][k]!=get<float>(m,0xb0000+band*0x2000+row*48+32+k*4)){printf("wake frame row mismatch %d/%d row%d band%d\n",scenario,tick,row,band);return 5;}
   }
   if(state.tip())populated++;
  }
 }
 printf("10000 composed original wake physics frames exact, with %u populated-tip frames; colour/UV preparation and rendering excluded\n",populated);
}
