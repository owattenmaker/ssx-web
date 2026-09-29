#include "ps2_runtime_macros.h"
#include "../engine/npc_path.hpp"
#include <fstream>
#include <random>
#include <cfenv>
#include <cstdio>
void sub_0026AFB8_0x26afb8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026A9B0_0x26a9b0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010D1A0_0x10d1a0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010D870_0x10d870(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00112588_0x112588(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026A428_0x26a428(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026A638_0x26a638(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00112A50_0x112a50(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026AC48_0x26ac48(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026AB20_0x26ab20(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001125C0_0x1125c0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001125A8_0x1125a8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001125B8_0x1125b8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026AC88_0x26ac88(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010BB18_0x10bb18(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010B980_0x10b980(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010BBF8_0x10bbf8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026AA80_0x26aa80(uint8_t*,R5900Context*,PS2Runtime*);
static std::vector<int> occupiedPaths;static bool usePathOccupancy=false;
static bool computerControlled=true;static int occupants,randomCalls;static uint32_t randomWord;
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){
 PS2Runtime rt;rt.registerFunction(0x26aa80,sub_0026AA80_0x26aa80);rt.registerFunction(0x112a50,sub_00112A50_0x112a50);rt.registerFunction(0x1125a8,sub_001125A8_0x1125a8);rt.registerFunction(0x1125b8,sub_001125B8_0x1125b8);rt.registerFunction(0x26ac88,sub_0026AC88_0x26ac88);rt.registerFunction(0x31c228,sub_0031C228_0x31c228);rt.registerFunction(0x26afb8,sub_0026AFB8_0x26afb8);rt.registerFunction(0x26ac48,sub_0026AC48_0x26ac48);rt.registerFunction(0x26ab20,sub_0026AB20_0x26ab20);rt.registerFunction(0x10d410,sub_0010D1A0_0x10d1a0);rt.registerFunction(0x140bc8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,computerControlled);c->pc=GPR_U32(c,31);});rt.registerFunction(0x10d870,sub_0010D870_0x10d870);rt.registerFunction(0x112588,sub_00112588_0x112588);rt.registerFunction(0x26a428,sub_0026A428_0x26a428);rt.registerFunction(0x26a638,sub_0026A638_0x26a638);
 rt.registerFunction(0x10fc30,[](uint8_t*,R5900Context*c,PS2Runtime*){int count=occupants;if(usePathOccupancy){count=0;int index=int(GPR_U32(c,5)-0x30000)/64;for(int path:occupiedPaths)count+=path==index;}SET_GPR_U32(c,2,count);c->pc=GPR_U32(c,31);});rt.registerFunction(0x317810,[](uint8_t*,R5900Context*c,PS2Runtime*){++randomCalls;SET_GPR_U32(c,2,randomWord);c->pc=GPR_U32(c,31);});rt.registerFunction(0x26a9b0,sub_0026A9B0_0x26a9b0);
 std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(file)),{});if(m.size()!=32*1024*1024)return 2;
 auto write=[&](unsigned at,auto v){std::memcpy(m.data()+at,&v,sizeof(v));};auto read=[&](unsigned at){uint32_t v;std::memcpy(&v,m.data()+at,4);return v;};
 std::mt19937 rng(0x26afb8);std::uniform_real_distribution<float>value(-100000,100000);std::fesetround(FE_TOWARDZERO);
 for(unsigned k=0;k<20000;++k){
  unsigned count=1+rng()%129,maximum=1+rng()%8;bool require=k%2;std::vector<ssx::OriginalNpcPath> paths(count);std::array<float,3> point{value(rng),value(rng),value(rng)};
  for(unsigned i=0;i<count;++i){auto&p=paths[i];p.field3C=rng()%2;for(unsigned a=0;a<3;++a){float x=value(rng),y=value(rng);p.geometry.low[a]=std::min(x,y);p.geometry.high[a]=std::max(x,y);}write(0x30000+i*64+0x1c,p.geometry.low);write(0x30000+i*64+0x28,p.geometry.high);write(0x30000+i*64+0x3c,p.field3C);}
  write(0x20008,count);write(0x2000c,0x30000u);write(0x40000,point);write(0x4000c,1.f);
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,6,0x50000);SET_GPR_U32(&c,7,maximum);SET_GPR_U32(&c,8,require);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  sub_0026AFB8_0x26afb8(m.data(),&c,&rt);auto actual=ssx::originalNpcPathCandidates(paths,point,maximum,require);auto*cp=&c;
  if(c.pc!=0x12345678||GPR_U32(cp,2)!=actual.size()){printf("NPC candidate count mismatch%u\n",k);return 3;}
  for(unsigned i=0;i<actual.size();++i)if(read(0x50000+i*4)!=0x30000+actual[i]*64){printf("NPC candidate mismatch%u/%u\n",k,i);return 4;}
 }
 puts("20000 original NPC path candidate lists match exactly (order, capacity andfield3C filter)");
 unsigned accepted=0,draws=0;
 for(unsigned k=0;k<20000;++k){
  ssx::OriginalNpcPath p;p.flags38=rng()%1024;p.geometry.origin={value(rng),value(rng),value(rng)};
  p.geometry.segments={{1,0,0,3000},{0,1,.1f,4000},{-1,0,-.3f,5000}};
  ssx::OriginalNpcPathScoreContext context;context.position=p.geometry.origin;context.velocity={value(rng)*.02f,value(rng)*.02f,value(rng)*.02f};for(auto&x:context.position)x+=value(rng)*.02f;
  context.roleE00=int(k%5)-1;context.allowFlag0E04=k%3;context.randomizeE08=k%2;context.currentPathIndex=k%4?0:1;context.followedPathIndex=k%7? -1:0;
  std::vector<int> indices;occupants=k%4;for(int i=0;i<occupants;++i)indices.push_back(0);context.npcPathIndices=indices;
  write(0x4a30f0-0x848,0x60000u);write(0x60084,0x61000u);write(0x6100c,0x62000u);write(0x62028,0x70000u);write(0x70ab8,context.followedPathIndex==0?0x30000u:0x32000u);
  write(0x20018,0x40000u);write(0x20e00,context.roleE00);write(0x20e04,int(context.allowFlag0E04));write(0x20e08,int(context.randomizeE08));write(0x40110,context.position);write(0x4011c,1.f);write(0x401e0,context.velocity);write(0x401ec,0.f);write(0x400f0,int(context.followedPathIndex>=0));write(0x400f8,0u);write(0x40ab8,context.currentPathIndex==0?0x30000u:0x32000u);
  write(0x3000c,p.geometry.origin);write(0x30018,0x33000u);write(0x30008,unsigned(p.geometry.segments.size()));write(0x30038,p.flags38);for(unsigned i=0;i<p.geometry.segments.size();++i)write(0x33000+i*16,p.geometry.segments[i]);
  R5900Context c{};c.pc=0x10d410;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x30000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);randomWord=rng();randomCalls=0;
  sub_0010D1A0_0x10d1a0(m.data(),&c,&rt);unsigned nativeCalls=0;float actual=ssx::originalNpcPathScore(p,0,context,[&](){++nativeCalls;return randomWord;});
  if(c.pc!=0x12345678||actual!=c.f[0]||nativeCalls!=unsigned(randomCalls)){printf("NPC score mismatch%u flags%x role%d expected%.9g actual%.9g RNG%d/%u pc%x\n",k,p.flags38,context.roleE00,c.f[0],actual,randomCalls,nativeCalls,c.pc);return 5;}accepted+=actual>0;draws+=nativeCalls;
 }
 printf("20000 original NPC path scores match exactly (%u positive,%u RNG draws)\n",accepted,draws);

 usePathOccupancy=true;unsigned changed=0;
 for(unsigned k=0;k<2000;++k){
  unsigned count=1+rng()%10;std::vector<ssx::OriginalNpcPath> paths(count);ssx::OriginalNpcPathScoreContext context;context.position={value(rng),value(rng),value(rng)};context.velocity={500+float(rng()%1000),500+float(rng()%1000),-200.f};context.computerControlled=computerControlled=k<1000;context.roleE00=k%3;context.allowFlag0E04=k%2;context.randomizeE08=k%3==0;context.followedPathIndex=k%7?-1:int(rng()%count);
  ssx::OriginalNpcRouteState state;state.pathIndex=rng()%count;state.previousDistance=2;state.currentDistance=3;state.lateralDistance=4;state.closestPoint={1,2,3};state.lookaheadPoint={4,5,6};state.cache={{8,9,10},11,0};occupiedPaths={state.pathIndex,int(rng()%count),int(rng()%count),int(rng()%count)};context.npcPathIndices=occupiedPaths;
  for(unsigned i=0;i<count;++i){auto&p=paths[i];p.flags38=(rng()%1024)|0xe;p.geometry.origin=context.position;for(float&x:p.geometry.origin)x+=float(int(rng()%5000)-2500);p.geometry.low={-200000,-200000,-200000};p.geometry.high={200000,200000,200000};p.geometry.segments={{1,0,0,3000},{0,1,-.1f,4000}};unsigned at=0x30000+i*64;write(at+0xc,p.geometry.origin);write(at+0x1c,p.geometry.low);write(at+0x28,p.geometry.high);write(at+0x38,p.flags38);write(at+8,2u);write(at+0x18,0x33000u+i*32);write(0x33000+i*32,p.geometry.segments[0]);write(0x33010+i*32,p.geometry.segments[1]);}
  constexpr unsigned actor=0x40000,owner=actor-0xf50;
  write(0x4d33a8,count);write(0x4d33ac,0x30000u);write(0x4a30f0-0x848,0x60000u);write(0x60084,0x61000u);write(0x6100c,0x62000u);write(0x62028,0x70000u);write(0x70ab8,context.followedPathIndex<0?0u:0x30000+unsigned(context.followedPathIndex)*64);
  write(owner+0x18,actor);write(owner+0xe00,context.roleE00);write(owner+0xe04,int(context.allowFlag0E04));write(owner+0xe08,int(context.randomizeE08));write(actor+0x110,context.position);write(actor+0x11c,1.f);write(actor+0x1e0,context.velocity);write(actor+0x1ec,0.f);write(actor+0xf0,int(context.followedPathIndex>=0));write(actor+0xf8,0u);write(actor+0xab8,0x30000u+state.pathIndex*64);write(actor+0xabc,0x50000u);write(0x50000,state.cache.origin);write(0x50010,state.cache.distance);write(0x50014,state.cache.segment);write(actor+0x490,state.closestPoint);write(actor+0x4a0,state.lookaheadPoint);write(actor+0x4c0,state.previousDistance);write(actor+0x4c4,state.currentDistance);write(actor+0x4c8,state.lateralDistance);
  write(actor+0x6c0,0x48000u);write(0x48048,int16_t(0));write(0x4804c,0x140bc8u);write(0x480a0,int16_t(-0xf50));write(0x480a4,0x10d410u);
  R5900Context c{};c.pc=0x112a50;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,actor);SET_GPR_U32(&c,5,k%2);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);randomWord=rng();randomCalls=0;
  sub_00112A50_0x112a50(m.data(),&c,&rt);unsigned nativeCalls=0;changed+=ssx::originalNpcSelectPath(paths,state,context,k%2,[&](){++nativeCalls;return randomWord;});
  auto rf=[&](unsigned at){float v;std::memcpy(&v,m.data()+at,4);return v;};
  if(c.pc!=0x12345678||read(actor+0xab8)!=0x30000+state.pathIndex*64||nativeCalls!=unsigned(randomCalls)||rf(actor+0x4c0)!=state.previousDistance||rf(actor+0x4c4)!=state.currentDistance||rf(actor+0x4c8)!=state.lateralDistance||int(read(0x50014))!=state.cache.segment||rf(0x50010)!=state.cache.distance){printf("NPC selector mismatch%u path%d expected%d RNG%d/%u\n",k,state.pathIndex,int(read(actor+0xab8)-0x30000)/64,randomCalls,nativeCalls);return 6;}
  for(unsigned i=0;i<3;++i)if(rf(actor+0x490+i*4)!=state.closestPoint[i]||rf(actor+0x4a0+i*4)!=state.lookaheadPoint[i]||rf(0x50000+i*4)!=state.cache.origin[i]){printf("NPC selector position/cache mismatch%u/%u\n",k,i);return 7;}
  for(unsigned i=0;i<count;++i){paths[i].geometry.events={{20,0,1500,2500},{18,0,5000,5000}};write(0x30000+i*64,2u);write(0x30004+i*64,0x35000u+i*32);write(0x35000+i*32,paths[i].geometry.events[0]);write(0x35010+i*32,paths[i].geometry.events[1]);write(0x30034+i*64,0x49000u);}
  write(0x49010,int16_t(0));write(0x49014,0x26ac88u);context.position[0]+=float(int(k%5)-2)*400;context.position[1]+=float(int(k%7)-3)*400;write(actor+0x110,context.position);occupiedPaths[0]=state.pathIndex;
  float remaining=k%5?10000.f:-1.f;int32_t tick=k%2?int32_t(k):60;write(actor+0x4d4,remaining);write(0x62008,tick);
  c={};c.pc=0x1125c0;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,actor);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);randomCalls=0;nativeCalls=0;
  sub_001125C0_0x1125c0(m.data(),&c,&rt);ssx::originalNpcRouteProgress(paths,state,context,remaining,tick,[&](){++nativeCalls;return randomWord;});
  if(c.pc!=0x12345678||read(actor+0xab8)!=0x30000+state.pathIndex*64||nativeCalls!=unsigned(randomCalls)||rf(actor+0x4c0)!=state.previousDistance||rf(actor+0x4c4)!=state.currentDistance||rf(actor+0x4c8)!=state.lateralDistance||rf(actor+0x4cc)!=state.heading||int(read(0x50014))!=state.cache.segment||rf(0x50010)!=state.cache.distance){printf("NPC progress mismatch%u path%d expected%d RNG%d/%u heading%.9g/%.9g pc%x\n",k,state.pathIndex,int(read(actor+0xab8)-0x30000)/64,randomCalls,nativeCalls,rf(actor+0x4cc),state.heading,c.pc);return 8;}
  for(unsigned i=0;i<3;++i)if(rf(actor+0x490+i*4)!=state.closestPoint[i]||rf(actor+0x4a0+i*4)!=state.lookaheadPoint[i]||rf(actor+0x4b0+i*4)!=state.previousLookaheadPoint[i]||rf(0x50000+i*4)!=state.cache.origin[i]){printf("NPC progress point mismatch%u/%u\n",k,i);return 9;}

 }
 printf("2000 complete original human/NPC route selections AND progress updates match exactly (%u switches), including RNG call order andcache\n",changed);

 for(unsigned k=0;k<20000;++k){
  ssx::OriginalNpcPath path;path.geometry.origin={value(rng),value(rng),value(rng)};path.geometry.segments={{1,0,0,3000},{0,1,.1f,4000}};
  for(unsigned i=0;i<18;++i){float start=float(int(rng()%10000)-2000);path.geometry.events.push_back({uint32_t(14+rng()%7),uint32_t(rng()%2400),start,start+float(rng()%2000)});}
  float previous=float(int(rng()%10000)-2000),current=previous+float(int(rng()%2000)-100);
  write(0x20018,0x30000u);write(0x304c0,previous);write(0x304c4,current);write(0x30ab8,0x60000u);write(0x60000,unsigned(path.geometry.events.size()));write(0x60004,0x61000u);write(0x60008,2u);write(0x6000c,path.geometry.origin);write(0x60018,0x62000u);write(0x60034,0x63000u);write(0x63008,int16_t(0));write(0x6300c,0x26aa80u);for(unsigned i=0;i<18;++i)write(0x61000+i*16,path.geometry.events[i]);for(unsigned i=0;i<2;++i)write(0x62000+i*16,path.geometry.segments[i]);
  auto ctx=[&](){R5900Context c{};SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,6,0x40004);SET_GPR_U32(&c,7,0x40008);SET_GPR_U32(&c,8,0x4000c);SET_GPR_U32(&c,9,0x40010);SET_GPR_U32(&c,10,0x40014);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);return c;};auto rf=[&](unsigned at){float v;std::memcpy(&v,m.data()+at,4);return v;};
  auto c=ctx();sub_0010BB18_0x10bb18(m.data(),&c,&rt);auto speed=ssx::originalNpcSpeedZone(path,previous,current);auto*cp=&c;if(GPR_U32(cp,2)!=unsigned(speed.has_value())||(speed&&(speed->speedCmps!=rf(0x40000)||unsigned(speed->started)!=read(0x40004)))){printf("NPC speedzone mismatch%u\n",k);return 10;}
  c=ctx();sub_0010B980_0x10b980(m.data(),&c,&rt);auto jump=ssx::originalNpcJumpZone(path,previous,current);if(GPR_U32(cp,2)!=unsigned(jump.has_value())||(jump&&(jump->speedCmps!=rf(0x40000)||unsigned(jump->started)!=read(0x40004)))){printf("NPC jumpzone mismatch%u\n",k);return 11;}
  if(jump){for(unsigned i=0;i<4;++i)if(unsigned(jump->flags[i])!=read(0x40008+i*4))return 12;for(unsigned i=0;i<3;++i)if(jump->startPoint[i]!=rf(0x20e50+i*4)||jump->endPoint[i]!=rf(0x20e60+i*4))return 13;}
  c=ctx();sub_0010BBF8_0x10bbf8(m.data(),&c,&rt);auto toggle=ssx::originalNpcToggleZone(path,previous,current);if(GPR_U32(cp,2)!=unsigned(toggle.has_value())||read(0x40000)!=unsigned(toggle.value_or(false))){printf("NPC togglezone mismatch%u\n",k);return 14;}
 }
 puts("20000 original NPC speed/jump/toggle zone queries match exactly, including12-event capacity andauthoredfields");

}
