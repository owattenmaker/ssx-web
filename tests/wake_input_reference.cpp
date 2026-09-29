#include "ps2_runtime_macros.h"
#include "../engine/wake_input.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <cstring>
#include <cfenv>
void sub_002DD0B8_0x2dd0b8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
template<class T>T get(uint8_t*m,unsigned p){T v;std::memcpy(&v,m+p,sizeof(v));return v;}
template<class T>void put(uint8_t*m,unsigned p,T v){std::memcpy(m+p,&v,sizeof(v));}
int main(int argc,char**argv){
 if(argc!=2)return 1;PS2Runtime runtime;runtime.registerFunction(0x11fe98,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0);c->pc=GPR_U32(c,31);});
 std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> memory((std::istreambuf_iterator<char>(file)),{});if(memory.size()!=32*1024*1024)return 2;auto*m=memory.data();
 constexpr unsigned owner=0x20000,actor=0x40000,geometry=0x50000,unitBoard=0x60000,scaledBoard=0x61000,surface=0x70000,stack=0x10000;
 put(m,owner,actor);put(m,actor+0x780,geometry);put(m,actor+0x8a4,0u);put(m,geometry+0x30,unitBoard);put(m,geometry+0x34,scaledBoard);
 std::fesetround(FE_TOWARDZERO);std::mt19937 random(0x2dd2a4);std::uniform_real_distribution<float> value(-1,1),unit(0,1);
 for(unsigned trial=0;trial<20000;trial++){
  ssx::OriginalWakeFrameInput in;in.turn1F0=value(random);in.lean208=unit(random);in.brake214=value(random)*1.2f;in.reverse320=trial%2;in.surfaceFlag84=trial%3;in.surfaceAlpha48=unit(random);in.surfaceScale4C=unit(random)*2;
  for(unsigned k=0;k<3;k++){in.normal[k]=value(random);in.lateral[k]=value(random);in.velocity[k]=trial%7?value(random)*3000:0;in.scaledAxis0[k]=value(random)*1.5f;in.unitAxis0[k]=value(random);in.unitAxis2[k]=value(random);in.boardPosition[k]=value(random)*100000;}
  auto vector=[&](unsigned at,auto v,float w=0){put(m,at,v);put(m,at+12,w);};
  vector(actor+0x370,in.normal);vector(actor+0x3b0,in.lateral);vector(actor+0x1e0,in.velocity);vector(unitBoard,in.unitAxis0);vector(unitBoard+0x20,in.unitAxis2);vector(unitBoard+0x30,in.boardPosition,1);vector(scaledBoard,in.scaledAxis0);
  put(m,actor+0x1f0,in.turn1F0);put(m,actor+0x208,in.lean208);put(m,actor+0x214,in.brake214);put(m,actor+0x320,int(in.reverse320));put(m,surface+0x48,in.surfaceAlpha48);put(m,surface+0x4c,in.surfaceScale4C);put(m,surface+0x84,int(in.surfaceFlag84));
  R5900Context c{};c.pc=0x2dd2a4;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,18,owner);SET_GPR_U32(&c,19,surface);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,31,0x12345678);sub_002DD0B8_0x2dd0b8(m,&c,&runtime);
  auto out=ssx::originalWakeTargets(in);
  if(c.pc!=0x12345678||out.amplitude!=get<float>(m,stack+0x30)||out.alpha!=get<float>(m,stack+0x34)||out.growth!=c.f[20]||out.signedTurn!=c.f[21]||out.turnAmount!=c.f[22]){printf("wake target scalar mismatch %u amp %.9g/%.9g growth %.9g/%.9g\n",trial,out.amplitude,get<float>(m,stack+0x30),out.growth,c.f[20]);return 3;}
  for(unsigned k=0;k<3;k++)if(out.normal[k]!=get<float>(m,stack+k*4)||out.side[k]!=get<float>(m,stack+0x10+k*4)||out.direction[k]!=get<float>(m,stack+0x20+k*4)||out.point[k]!=get<float>(m,stack+0x50+k*4)){printf("wake target vector mismatch %u/%u\n",trial,k);return 4;}
 }
 puts("20000 original wake target stages exact: scaled/unit board axes, stance, steering/extra-lean/brake, speed limits and surface-dependent amplitude/growth");
}
