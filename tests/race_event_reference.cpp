#include "ps2_runtime_macros.h"
#include "../engine/race_event.hpp"
#include <fstream>
#include <cstdio>
#include <random>
#include <cfenv>
#include "../engine/original_float.hpp"
void sub_00113B10_0x113b10(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00113B48_0x113b48(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00113C20_0x113c20(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0012A250_0x12a250(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026A638_0x26a638(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00270AB0_0x270ab0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026AA80_0x26aa80(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001127F0_0x1127f0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00112588_0x112588(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026A428_0x26a428(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026AB20_0x26ab20(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026A9B0_0x26a9b0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026B178_0x26b178(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00112338_0x112338(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00112FB0_0x112fb0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026AC48_0x26ac48(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026AC88_0x26ac88(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00269F18_0x269f18(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){PS2Runtime rt;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(file)),{});if(m.size()!=32*1024*1024)return 2;
 auto wr=[&](unsigned a,auto x){std::memcpy(m.data()+a,&x,sizeof(x));};auto ri=[&](unsigned a){int x;std::memcpy(&x,m.data()+a,4);return x;};auto rf=[&](unsigned a){float x;std::memcpy(&x,m.data()+a,4);return x;};
 auto ctx=[](unsigned pc){R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);SET_GPR_U32(&c,4,0x30000);return c;};
 rt.registerFunction(0x113b10,sub_00113B10_0x113b10);rt.registerFunction(0x113b48,sub_00113B48_0x113b48);rt.registerFunction(0x12a250,sub_0012A250_0x12a250);
 for(unsigned pc:{0x113cb8u,0x113cc0u,0x113ce0u,0x113ce8u,0x113cf0u,0x113d10u,0x113d18u,0x113d28u,0x113d30u,0x113d38u,0x113d48u,0x113d80u,0x113d88u,0x113db0u,0x113e18u,0x113e40u,0x113e48u,0x113e50u,0x113e70u,0x113e78u})rt.registerFunction(pc,sub_00113C20_0x113c20);
 for(unsigned pc:{0x144068u,0x12b090u,0x231250u})rt.registerFunction(pc,[](uint8_t*,R5900Context*c,PS2Runtime*){c->pc=GPR_U32(c,31);});wr(0x4a30f0-0x204,0u);
 std::fesetround(FE_TOWARDZERO);std::mt19937 rng(0x113d48);std::uniform_real_distribution<float> f(-1000,1000);
 constexpr unsigned offsets[]={0,0xac,0xb0,0xb4,0xbc,0xc0,0xc4,0xc8};
 for(unsigned k=0;k<20000;k++){
  std::memcpy(m.data()+0x30000,m.data()+0x5bc500,0xd0);ssx::OriginalRaceClock s;s.phase=ssx::RacePhase(1+k%7);s.previous=s.phase;s.countdownTicks=int(k%185)-2;s.raceTicks=int(k);s.totalTicks=int(k);s.raceEnabled=k%2;s.preRaceLocal=9;
  auto c=ctx(0x113b48);SET_GPR_U32(&c,5,unsigned(s.phase));sub_00113B48_0x113b48(m.data(),&c,&rt);
  wr(0x30004,int(s.previous));wr(0x30008,s.totalTicks);wr(0x3000c,s.raceTicks);wr(0x30014,s.raceEnabled);wr(0x3001c,s.countdownTicks);wr(0x300b8,s.preRaceLocal);wr(0x3009c,0u);
  std::vector<float> humans(k%7);wr(0x3007c,int(humans.size()));for(unsigned i=0;i<humans.size();i++){humans[i]=k%3==0?1.f:f(rng);wr(0x30040+i*4,0x80000+i*0x1000);wr(0x80018+i*0x1000,0x90000+i*0x1000);wr(0x90470+i*0x1000,humans[i]);}
  if(k%3){auto phase=ssx::RacePhase(1+(k/3)%7);ssx::originalRaceSelect(s,phase);c=ctx(0x113b10);SET_GPR_U32(&c,5,unsigned(phase));sub_00113B10_0x113b10(m.data(),&c,&rt);}
  c=ctx(0x113c20);sub_00113C20_0x113c20(m.data(),&c,&rt);wr(0x30008,int(uint32_t(ri(0x30008))+1));auto effects=ssx::originalRaceClockStep(s,humans);
  for(auto [at,x]:std::initializer_list<std::pair<unsigned,int>>{{0,int(s.phase)},{4,int(s.previous)},{8,s.totalTicks},{12,s.raceTicks},{0x14,s.raceEnabled},{0x1c,s.countdownTicks},{0xb8,s.preRaceLocal}})if(ri(0x30000+at)!=x){printf("clock %u at%x expected%d actual%d\n",k,at,ri(0x30000+at),x);return 3;}
 }
 puts("20000 original race clock/state transition ticks match exactly");
 for(unsigned k=0;k<20000;k++){
  ssx::OriginalRacePath p;p.origin={f(rng),f(rng),f(rng)};for(unsigned i=0;i<1+k%20;i++){std::array<float,4> v={f(rng),f(rng),f(rng),std::abs(f(rng))};float norm=std::sqrt((v[0]*v[0]+v[1]*v[1])+v[2]*v[2]);for(int j=0;j<3;j++)v[j]/=norm;p.segments.push_back(v);}std::array<float,3> position={f(rng),f(rng),f(rng)};
  ssx::OriginalRacePathCache cache;if(k%2){cache.segment=int(k%p.segments.size());cache.origin=p.origin;cache.distance=float(k%30);}
  auto vw=[&](unsigned a,auto v,float w=0){wr(a,v);wr(a+12,w);};vw(0x3000c,p.origin);wr(0x30008,int(p.segments.size()));wr(0x30018,0x40000u);for(unsigned i=0;i<p.segments.size();i++)wr(0x40000+i*16,p.segments[i]);vw(0x50000,position,1);vw(0x60000,cache.origin,1);wr(0x60010,cache.distance);wr(0x60014,cache.segment);
  auto c=ctx(0x26a638);SET_GPR_U32(&c,5,0x50000);SET_GPR_U32(&c,6,0x70000);SET_GPR_U32(&c,7,0x70010);SET_GPR_U32(&c,8,0x60000);SET_GPR_U32(&c,9,k%3?1:0);sub_0026A638_0x26a638(m.data(),&c,&rt);auto out=ssx::originalRacePathProject(p,position,cache,k%3);
  if(c.f[0]!=out.distance||rf(0x70010)!=out.lateralDistance||ri(0x60014)!=cache.segment||rf(0x60010)!=cache.distance){printf("projection%u expected %.9g %.9g actual %.9g %.9g\n",k,c.f[0],rf(0x70010),out.distance,out.lateralDistance);return 4;}
  for(int i=0;i<3;i++)if(rf(0x70000+i*4)!=out.point[i]||rf(0x60000+i*4)!=cache.origin[i])return 5;
 }
 puts("20000 original cached course path projections match exactly");
 static int humanIndex;rt.registerFunction(0x270730,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_S32(c,2,humanIndex);c->pc=GPR_U32(c,31);});
 for(unsigned k=0;k<20000;k++){
  ssx::OriginalRaceCheckpoints s;s.count=k%35;s.mode=k%7==0?10:0;s.field610=k%17==0;for(auto& x:s.humanMasks)x=rng();s.pendingHumanMask=k%255;humanIndex=int(k%5)-1;int checkpoint=int(k%40)-1;
  wr(0x30000,s.mode);wr(0x30010,s.count);wr(0x3060c,s.field60C);wr(0x30610,s.field610);wr(0x3061c,s.field61C);wr(0x30620,s.field620);wr(0x303b4,0x50000u);wr(0x5001f,s.pendingHumanMask);wr(0x305fc,s.humanMasks);
  auto c=ctx(0x270ab0);SET_GPR_U32(&c,5,0);SET_GPR_S32(&c,6,checkpoint);sub_00270AB0_0x270ab0(m.data(),&c,&rt);ssx::originalRaceRecordCheckpoint(s,humanIndex,checkpoint);
  if(m[0x5001f]!=s.pendingHumanMask)return 6;for(unsigned j=0;j<4;j++)if(uint32_t(ri(0x305fc+j*4))!=s.humanMasks[j])return 7;
 }
 puts("20000 original checkpoint-mask updates match exactly");
 for(unsigned k=0;k<20000;k++){
  ssx::OriginalRacePath path;path.remainingAtOrigin=f(rng);for(unsigned j=0;j<k%30;j++){float a=f(rng),b=f(rng);if(a>b)std::swap(a,b);path.events.push_back({rng(),rng(),a,b});}float previous=f(rng),current=f(rng);if(k%3==0)current=previous;
  float low=ssx::originalScalarSubtract(path.remainingAtOrigin,previous),high=ssx::originalScalarSubtract(path.remainingAtOrigin,current);wr(0x30000,int(path.events.size()));wr(0x30004,0x40000u);for(unsigned j=0;j<path.events.size();j++)wr(0x40000+j*16,path.events[j]);
  auto c=ctx(0x26aa80);SET_GPR_U32(&c,5,0x50000);SET_GPR_U32(&c,6,12);c.f[12]=low;c.f[13]=high;sub_0026AA80_0x26aa80(m.data(),&c,&rt);auto events=ssx::originalRacePathEvents(path,previous,current);auto* cp=&c;if(GPR_U32(cp,2)!=events.size())return 8;for(unsigned j=0;j<events.size();j++)if(std::memcmp(m.data()+0x50000+j*16,&events[j],16))return 9;
 }
 puts("20000 original inclusive course-event interval queries match exactly");

 rt.registerFunction(0x112588,sub_00112588_0x112588);rt.registerFunction(0x26a428,sub_0026A428_0x26a428);rt.registerFunction(0x26ab20,sub_0026AB20_0x26ab20);rt.registerFunction(0x26a9b0,sub_0026A9B0_0x26a9b0);rt.registerFunction(0x26b178,sub_0026B178_0x26b178);
 unsigned pathsBase=unsigned(ri(0x4d33b4));std::vector<ssx::OriginalRacePath> paths;
 for(int i=0;i<ri(0x4d33b0);i++){
  unsigned p=pathsBase+i*60;ssx::OriginalRacePath path;for(int j=0;j<3;j++){path.origin[j]=rf(p+12+j*4);path.low[j]=rf(p+0x1c+j*4);path.high[j]=rf(p+0x28+j*4);}path.remainingAtOrigin=rf(p+0x38);
  unsigned segments=unsigned(ri(p+0x18));for(int j=0;j<ri(p+8);j++){std::array<float,4> segment;for(int z=0;z<4;z++)segment[z]=rf(segments+j*16+z*4);path.segments.push_back(segment);}for(int j=0;j<ri(p);j++){ssx::OriginalRacePathEvent e;std::memcpy(&e,m.data()+ri(p+4)+j*16,16);path.events.push_back(e);}paths.push_back(path);
 }
 for(unsigned k=0;k<10000;k++){
  ssx::OriginalRaceProgress state;state.pathIndex=k%paths.size();state.remaining=paths[state.pathIndex].remainingAtOrigin-f(rng)*20;state.cache.segment=k%7;
  auto position=paths[(k/7)%paths.size()].origin;for(float& v:position)v+=f(rng)*20;std::array<float,3> velocity={f(rng),f(rng),f(rng)};if(k%13==0)velocity={0,0,0};
  unsigned rider=0x14701a0,cache=unsigned(ri(rider+0xac0));wr(rider+0x110,position);wr(rider+0x1e0,velocity);wr(rider+0xab4,pathsBase+state.pathIndex*60);wr(rider+0x4d0,state.remaining);wr(cache+0x14,state.cache.segment);
  auto c=ctx(0x1127f0);SET_GPR_U32(&c,4,rider);SET_GPR_U32(&c,5,k%2);sub_001127F0_0x1127f0(m.data(),&c,&rt);ssx::originalRaceSelectPath(paths,state,position,velocity,k%2);
  if(unsigned(ri(rider+0xab4))!=pathsBase+state.pathIndex*60||ri(cache+0x14)!=state.cache.segment){printf("select path%u original%d native%d cache%d/%d\n",k,(ri(rider+0xab4)-int(pathsBase))/60,state.pathIndex,ri(cache+0x14),state.cache.segment);return 10;}
 }
 puts("10000 complete original Snow Jam course-path selections match exactly");

 rt.registerFunction(0x1127f0,sub_001127F0_0x1127f0);rt.registerFunction(0x112fb0,sub_00112FB0_0x112fb0);rt.registerFunction(0x26a638,sub_0026A638_0x26a638);rt.registerFunction(0x26ac48,sub_0026AC48_0x26ac48);rt.registerFunction(0x26ac88,sub_0026AC88_0x26ac88);rt.registerFunction(0x26a090,sub_00269F18_0x269f18);rt.registerFunction(0x26a0b8,sub_00269F18_0x269f18);rt.registerFunction(0x26aa80,sub_0026AA80_0x26aa80);
 rt.registerFunction(0x3e6574,[](uint8_t*m,R5900Context*c,PS2Runtime*){std::memmove(m+GPR_U32(c,4),m+GPR_U32(c,5),GPR_U32(c,6));c->pc=GPR_U32(c,31);});rt.registerFunction(0x10e5d8,[](uint8_t*,R5900Context*c,PS2Runtime*){c->pc=GPR_U32(c,31);});
 for(unsigned k=0;k<2000;k++){
  ssx::OriginalRaceProgress state;state.pathIndex=k%paths.size();state.remaining=paths[state.pathIndex].remainingAtOrigin-f(rng)*20;state.bestRemaining=state.remaining-std::abs(f(rng));state.cache.segment=-1;
  auto position=paths[(k/7)%paths.size()].origin;for(float& v:position)v+=f(rng)*20;std::array<float,3> velocity={f(rng),f(rng),f(rng)};
  unsigned rider=0x14701a0,cache=unsigned(ri(rider+0xac0));wr(rider+0x110,position);wr(rider+0x1e0,velocity);wr(rider+0xab4,pathsBase+state.pathIndex*60);wr(rider+0x4d0,state.remaining);wr(rider+0x4d4,state.bestRemaining);wr(cache+0x14,state.cache.segment);wr(rider+0x59c,0);wr(0x5bc508,int(k));
  auto c=ctx(0x112338);SET_GPR_U32(&c,4,rider);sub_00112338_0x112338(m.data(),&c,&rt);auto events=ssx::originalRaceProgressStep(paths,state,position,velocity,int(k));
  if(c.pc!=0x12345678){printf("progress continuation%x\n",c.pc);return 11;}
  if(unsigned(ri(rider+0xab4))!=pathsBase+state.pathIndex*60||rf(rider+0x4d0)!=state.remaining||rf(rider+0x4d4)!=state.bestRemaining||ri(cache+0x14)!=state.cache.segment||rf(cache+0x10)!=state.cache.distance){printf("clockptr%x tick%d; pos%.9g,%.9g,%.9g\n",ri(ri(ri(0x4a30f0-0x848)+0x84)+0xc),ri(ri(ri(ri(0x4a30f0-0x848)+0x84)+0xc)+8),position[0],position[1],position[2]);printf("progress%u index%d/%d remaining %.9g/%.9g best%.9g/%.9g cache%d/%d\n",k,(ri(rider+0xab4)-int(pathsBase))/60,state.pathIndex,rf(rider+0x4d0),state.remaining,rf(rider+0x4d4),state.bestRemaining,ri(cache+0x14),state.cache.segment);return 12;}
  if(size_t(ri(rider+0x59c))!=events.size())return 13;for(unsigned i=0;i<events.size();i++)if(std::memcmp(m.data()+rider+0x4dc+i*16,&events[i],16))return 14;
 }
 puts("2000 complete original Snow Jam progress/event stages match exactly");

}
