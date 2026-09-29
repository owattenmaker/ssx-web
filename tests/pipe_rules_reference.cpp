// Development-only private original instruction oracle; no guest runtime in the app.
// 1210B0 (collision timer decay + reason-2 reset), 125228 (freestyle time limit) and 22E0E0 (streaming-table location
// index) against engine/collision_event.hpp, engine/race_session.hpp and engine/patch_state.hpp.
#include "ps2_runtime_macros.h"
#include "../engine/collision_event.hpp"
#include "../engine/race_session.hpp"
#include "../engine/patch_state.hpp"
#include <fstream>
#include <cstdio>
#include <random>
#include <cfenv>
#include <bit>
void sub_001210B0_0x1210b0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00125228_0x125228(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0022E0E0_0x22e0e0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_004139F8_0x4139f8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00413068_0x413068(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static unsigned motionResult=0,resetCalls=0,resetA1=0,resetA2=0,finishCalls=0,finishRider=0,gateFlag=0,otherCalls=0;
static void ret(R5900Context* c){c->pc=GPR_U32(c,31);}
int main(int argc,char**argv){
 PS2Runtime rt;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(file)),{});if(m.size()!=32*1024*1024)return 2;
 auto wr=[&](unsigned a,auto x){std::memcpy(m.data()+a,&x,sizeof(x));};
 auto rf=[&](unsigned a){float x;std::memcpy(&x,m.data()+a,4);return x;};
 auto ri=[&](unsigned a){int32_t x;std::memcpy(&x,m.data()+a,4);return x;};
 auto ctx=[](unsigned pc,unsigned a0){R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x20000);SET_GPR_U32(&c,31,0x12345678);SET_GPR_U32(&c,4,a0);return c;};
 rt.registerFunction(0x4139f8,sub_004139F8_0x4139f8);rt.registerFunction(0x413068,sub_00413068_0x413068);
 rt.registerFunction(0x11fe98,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,motionResult);ret(c);});
 rt.registerFunction(0x116120,[](uint8_t*,R5900Context*c,PS2Runtime*){++resetCalls;resetA1=GPR_U32(c,5);resetA2=GPR_U32(c,6);SET_GPR_U32(c,2,1);ret(c);});
 rt.registerFunction(0x1200d0,[](uint8_t*,R5900Context*c,PS2Runtime*){ret(c);});
 rt.registerFunction(0x120d90,[](uint8_t*,R5900Context*c,PS2Runtime*){ret(c);});
 rt.registerFunction(0x125108,[](uint8_t*,R5900Context*c,PS2Runtime*){++finishCalls;finishRider=GPR_U32(c,4);ret(c);});
 rt.registerFunction(0x28b180,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0x60000);ret(c);});
 rt.registerFunction(0x2a3c00,[](uint8_t*,R5900Context*c,PS2Runtime*){++otherCalls;ret(c);});
 rt.registerFunction(0x14dc80,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0);ret(c);});
 rt.registerFunction(0x14dd58,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0);ret(c);});
 rt.registerFunction(0x3f0000,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,gateFlag);ret(c);}); // rider sub-object vtable+0x44 (140BC0 reads +0x874)
 std::fesetround(FE_TOWARDZERO);std::mt19937 rng(0x1210b0);
 std::uniform_real_distribution<float> unit(-1,1),count(0,6),pred(0,60);
 const unsigned R=0x200000,RFX=0x210000,PRED=0x220000;
 // ---- 1210B0 ----
 const ssx::OriginalCollisionProfile profile;unsigned resets=0;
 for(unsigned k=0;k<60000;k++){
  ssx::OriginalCollisionHistory h;h.previousNormal={unit(rng),unit(rng),unit(rng)};h.directionChanges=count(rng)*(k%3?1.f:.8f);h.secondaryCounter=count(rng);h.peakImpactCmps=0;
  if(k%7==0)h.directionChanges=std::bit_cast<float>(std::bit_cast<uint32_t>(4.5f/profile.directionDecay)+int(k%5)-2);
  if(k%11==0)h.secondaryCounter=std::bit_cast<float>(std::bit_cast<uint32_t>(5.f/profile.secondaryDecay)+int(k%5)-2);
  const int motion=int(k%4);const bool crashAir=(k/4)%2;const int status=int((k/8)%5);float predicted=pred(rng),elapsed=pred(rng)*.2f;
  if(k%13==0)predicted=std::bit_cast<float>(std::bit_cast<uint32_t>(45.f+elapsed)+int(k%3)-1);
  for(int i=0;i<3;i++)wr(R+0x3e0+4*i,h.previousNormal[i]);wr(R+0x3ec,0.f);wr(R+0x3f0,h.directionChanges);wr(R+0x3f4,h.secondaryCounter);
  wr(R+0x77c,RFX);wr(RFX+0x30,crashAir?1:0);wr(R+0x788,PRED);wr(PRED+0xac,status);wr(PRED+0x98,predicted);wr(PRED+0xa0,elapsed);
  motionResult=unsigned(motion);resetCalls=0;
  auto c=ctx(0x1210b0,R);sub_001210B0_0x1210b0(m.data(),&c,&rt);
  if(c.pc!=0x12345678){std::printf("1210B0 continuation %x\n",c.pc);return 3;}
  auto e=h;const bool decayReset=ssx::originalCollisionHistoryDecay(profile,e);
  const bool expect=decayReset||ssx::originalCollisionTimerReset(e,motion,crashAir,status,predicted,elapsed);
  for(int i=0;i<3;i++)if(rf(R+0x3e0+4*i)!=e.previousNormal[i]){std::printf("1210B0 normal %u/%d\n",k,i);return 4;}
  if(rf(R+0x3f0)!=e.directionChanges||rf(R+0x3f4)!=e.secondaryCounter){std::printf("1210B0 counters %u\n",k);return 5;}
  if(unsigned(expect)!=resetCalls||(resetCalls&&(resetA1!=0||resetA2!=2))){std::printf("1210B0 reset %u expected %d calls %u (%u,%u) motion %d status %d\n",k,expect,resetCalls,resetA1,resetA2,motion,status);return 6;}
  resets+=resetCalls;
 }
 // ---- 125228 ----
 const unsigned P=0x230000,GMM=0x231000,Q=0x232000,RR=0x233000,VT=0x235000;unsigned timeouts=0;
 wr(0x4a30f0-0x848,P);wr(P+0xc0,GMM);wr(P+0x84,Q);wr(Q+0xc,RR);wr(Q+0x214,0); // race ticks = *(modeobj+0xC), modeobj = *(*(gp-0x848)+0x84)+0xC
 wr(R+0x6c0,VT);wr(VT+0x40,int16_t(-0x6c0));wr(VT+0x44,0x3f0000u);
 std::uniform_int_distribution<int> limits(1,20000),jitter(-90,90);
 for(unsigned k=0;k<40000;k++){
  const int32_t limit=k%5==0?60*int32_t(1+k%240):limits(rng);const int32_t ticks=std::max(0,(limit/60)*60+jitter(rng));
  const float finish=k%6==0?float(k%4):-1.f;const int8_t mode=int8_t(k%7==0?4:k%6);const int timed=(k/3)%4!=0;const unsigned bit9=(k%9==0)?0x200u:0u;gateFlag=(k%17)!=0;
  wr(R+0x470,finish);wr(R+0x480,0);wr(uint32_t(0x535c10),mode);wr(GMM+0x88,timed);wr(GMM+0x78,limit);wr(RR+0xc,ticks);wr(0x5308d0u,bit9|0x10000u);
  finishCalls=0;otherCalls=0;
  auto c=ctx(0x125228,R);sub_00125228_0x125228(m.data(),&c,&rt);
  if(c.pc!=0x12345678){std::printf("125228 continuation %x\n",c.pc);return 7;}
  const bool expect=gateFlag&&finish<0&&mode!=4&&timed&&!bit9&&ssx::originalRaceTimeLimitExpired(limit,ticks);
  if(unsigned(expect)!=finishCalls||(expect&&(finishRider!=R||ri(R+0x480)!=1||otherCalls!=1))||(!expect&&ri(R+0x480)!=0)){
   std::printf("125228 %u expected %d calls %u flag %u finish %g mode %d timed %d bit9 %u limit %d ticks %d\n",k,expect,finishCalls,gateFlag,finish,mode,timed,bit9,limit,ticks);return 8;}
  timeouts+=finishCalls;
 }
 // ---- 22E0E0 ----
 for(unsigned k=0;k<20000;k++){
  std::array<int32_t,ssx::kOriginalStreamingRows> rows;for(auto& v:rows)v=(rng()%4)?-1:int32_t(rng()%64);
  const int32_t track=int32_t(rng()%64);for(int i=0;i<ssx::kOriginalStreamingRows;i++)wr(0x442168+16*i+4,rows[i]);
  auto c=ctx(0x22e0e0,0);SET_GPR_U32(&c,5,uint32_t(track));sub_0022E0E0_0x22e0e0(m.data(),&c,&rt);R5900Context* cp=&c;
  if(c.pc!=0x12345678||int(GPR_U32(cp,2))!=ssx::originalStreamingLocationIndex(rows,track)){std::printf("22E0E0 %u: %d vs %d\n",k,int(GPR_U32(cp,2)),ssx::originalStreamingLocationIndex(rows,track));return 9;}
 }
 std::printf("60000 original 1210B0 collision-timer decays and reason-2 resets match exactly (%u resets)\n",resets);
 std::printf("40000 original 125228 time-limit checks match exactly (%u timeouts)\n",timeouts);
 std::puts("20000 original 22E0E0 streaming-table location lookups match exactly");
}
