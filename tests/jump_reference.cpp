// Development-only original SSX3 instruction oracle; never a product dependency.
#include "ps2_runtime_macros.h"
#include "../engine/jump_motion.hpp"
#include <fstream>
#include <cfenv>
#include <cstdio>
#include <bit>
#include <random>
void sub_00114298_0x114298(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C040_0x31c040(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BF60_0x31bf60(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00113F88_0x113f88(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001211F8_0x1211f8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x320000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=0x88000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x88000]={nullptr};
static unsigned mode,ticks;
int main(int argc,char** argv){
 PS2Runtime rt;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t> ram((std::istreambuf_iterator<char>(in)),{});
 if(ram.size()!=32*1024*1024)return 2;
 auto wr=[&](unsigned a,const auto& v){std::memcpy(ram.data()+a,&v,sizeof(v));};
 auto rd=[&](unsigned a){float v;std::memcpy(&v,ram.data()+a,4);return v;};
 auto no=[](uint8_t*,R5900Context* c,PS2Runtime*){c->pc=GPR_U32(c,31);};
 for(unsigned a:{0x2707E0u,0x309848u,0x10E098u,0x120378u})rt.registerFunction(a,no);
 rt.registerFunction(0x119E38,[](uint8_t*,R5900Context*c,PS2Runtime*){c->f[0]=0;c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x11FE98,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,mode);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x1298C8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,ticks);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x31C040,sub_0031C040_0x31c040);rt.registerFunction(0x31BF60,sub_0031BF60_0x31bf60);
 std::fesetround(FE_TOWARDZERO);
 constexpr unsigned rider=0x14701a0,stack=0x10000,done=0x12345678,owner=0x146f390;
 auto ctx=[&](){R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,31,done);SET_GPR_U32(&c,4,rider);return c;};
 for(float angle:{std::bit_cast<float>(0x3f5f66f4u),std::bit_cast<float>(0x3f9c61abu)}){auto c=ctx();c.f[12]=angle;sub_0031C040_0x31c040(ram.data(),&c,&rt);printf("cos %08x -> %08x %.9g\n",std::bit_cast<unsigned>(angle),std::bit_cast<unsigned>(c.f[0]),c.f[0]);}
 {auto c=ctx();c.f[12]=std::bit_cast<float>(0x3eb2b8c4u);sub_0031BF60_0x31bf60(ram.data(),&c,&rt);printf("sin20 -> %08x %.9g\n",std::bit_cast<unsigned>(c.f[0]),c.f[0]);}
 std::mt19937 rng(114298);std::uniform_real_distribution<float> dist(-1,1),charge(0,1),speed(100,4000);
 float worst=0;
 for(unsigned k=0;k<20000;k++){
  ssx::OriginalJumpState s;
  s.charge=k==0?rd(rider+0x220):(k%11==0?-1.f:charge(rng));s.speedLimit=k==0?rd(rider+0x2e4):speed(rng);s.ticksSinceGroundFocus=k==0?300:k%150;s.motionMode=mode=(k%17==0&&k)?4:0;s.riderState=k%19;s.flags=k%2?0x20:0;
  for(int i=0;i<3;i++){s.position[i]=rd(rider+0x110+4*i);s.velocity[i]=k==0?rd(rider+0x1e0+4*i):dist(rng)*speed(rng);s.normal[i]=k==0?rd(rider+0x370+4*i):dist(rng);s.forward[i]=k==0?rd(rider+0x3a0+4*i):dist(rng);s.takeoffNormal[i]=s.normal[i];}
  auto unit=[](auto v){float l=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);for(auto& x:v)x/=l;return v;};
  if(k){s.normal=unit(s.normal);s.forward=unit(s.forward);s.takeoffNormal=s.normal;}
  if(k&&k%7==0)s.takeoffNormal={0,0,1};
  ticks=s.ticksSinceGroundFocus;wr(owner+0x10,uint32_t(0));wr(rider+0x434,s.riderState);wr(rider+0x2d4,s.flags);wr(rider+0x2e4,s.speedLimit);
  auto vwrite=[&](unsigned at,auto v){for(int i=0;i<3;i++)wr(at+4*i,v[i]);wr(at+12,0.f);};
  vwrite(rider+0x110,s.position);vwrite(rider+0x1e0,s.velocity);vwrite(rider+0x370,s.normal);vwrite(rider+0x380,s.takeoffNormal);vwrite(rider+0x3a0,s.forward);vwrite(rider+0x1c0,s.boardUp);
  auto c=ctx();c.pc=0x114298;c.f[12]=s.charge;sub_00114298_0x114298(ram.data(),&c,&rt);
  if(c.pc!=done){printf("bad continuation %x\n",c.pc);return 3;}
  ssx::originalJumpTakeoff(s);
  for(int i=0;i<3;i++)if(std::bit_cast<unsigned>(s.cameraWallNormal[i])!=std::bit_cast<unsigned>(rd(rider+0x3c0+4*i))){printf("camera wall direction mismatch %u axis%d\n",k,i);return 8;}
  if(std::bit_cast<unsigned>(s.cameraLaunchValue)!=std::bit_cast<unsigned>(rd(rider+0x5a4))){printf("camera launch mismatch %u original%.9g native%.9g\n",k,rd(rider+0x5a4),s.cameraLaunchValue);return 7;}
  for(int i=0;i<3;i++){float pe=std::abs(s.position[i]-rd(rider+0x110+4*i));if(pe!=0){printf("position mismatch%u %d %.9g\n",k,i,pe);return 6;}float e=std::abs(s.velocity[i]-rd(rider+0x1e0+4*i));worst=std::max(worst,e);if(!std::isfinite(e)||e!=0.f){printf("mismatch case%u component%d original%.9g native%.9g error%g\n",k,i,rd(rider+0x1e0+4*i),s.velocity[i],e);return 4;}}
  if(k==0)printf("fixture30 launch %.9g %.9g %.9g\n",rd(rider+0x1e0),rd(rider+0x1e4),rd(rider+0x1e8));
 }
 printf("20000 original takeoff cases pass max error %.9g cm/s\n",worst);
 for(bool held:{true,false}){float native=held?0:1;wr(rider+0x220,native);wr(rider+0x214,0.f);for(int i=0;i<60;i++){
  auto c=ctx();c.f[12]=held?1:0;c.f[13]=0;sub_00113F88_0x113f88(ram.data(),&c,&rt);
  c=ctx();c.pc=0x1211f8;sub_001211F8_0x1211f8(ram.data(),&c,&rt);
  native=ssx::originalJumpChargeStep(native,held);if(native!=rd(rider+0x220)){printf("charge mismatch %d %d %.9g %.9g\n",held,i,native,rd(rider+0x220));return 5;}
 }}
 printf("120 original charge ticks match exactly\n");
}
