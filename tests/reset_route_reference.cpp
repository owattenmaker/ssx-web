#include "ps2_runtime_macros.h"
#include "../engine/reset_route.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <cstring>
#include <cfenv>
#define DECLARE(a,b) void sub_##a##_0x##b(uint8_t*,R5900Context*,PS2Runtime*);
DECLARE(0026A8B8,26a8b8) DECLARE(00112D58,112d58) DECLARE(0026AF98,26af98) DECLARE(0026AFB8,26afb8) DECLARE(0026A9B0,26a9b0)
DECLARE(0026A428,26a428) DECLARE(0026A638,26a638) DECLARE(0026AC48,26ac48) DECLARE(0026AB20,26ab20) DECLARE(00269F18,269f18)
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static bool allowEnd=false;
int main(int argc,char**argv){
 if(argc!=2)return 1;PS2Runtime rt;
#define REGISTER(a,b) rt.registerFunction(0x##b,sub_##a##_0x##b);
 REGISTER(0026AF98,26af98) REGISTER(0026AFB8,26afb8) REGISTER(0026A9B0,26a9b0) REGISTER(0026A428,26a428) REGISTER(0026A638,26a638) REGISTER(0026AC48,26ac48) REGISTER(0026AB20,26ab20) REGISTER(00269F18,269f18)
 rt.registerFunction(0x33fff0,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,allowEnd);c->pc=GPR_U32(c,31);});
 std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> memory((std::istreambuf_iterator<char>(file)),{});if(memory.size()!=32*1024*1024)return 2;
 auto write=[&](unsigned at,auto value){std::memcpy(memory.data()+at,&value,sizeof(value));};
 auto word=[&](unsigned at){uint32_t value;std::memcpy(&value,memory.data()+at,4);return value;};
 auto scalar=[&](unsigned at){float value;std::memcpy(&value,memory.data()+at,4);return value;};
 std::fesetround(FE_TOWARDZERO);
 std::mt19937 random(0x112d58);std::uniform_real_distribution<float> coordinate(-100000,100000);
 unsigned changed=0,refreshed=0;
 for(unsigned trial=0;trial<10000;trial++){
  const unsigned count=1+random()%16;std::vector<ssx::OriginalNpcPath> paths(count);
  std::array<float,3> position{coordinate(random),coordinate(random),coordinate(random)};
  for(unsigned i=0;i<count;i++){
   auto& p=paths[i];p.field3C=random()%3?1:0;p.geometry.origin=position;for(float& v:p.geometry.origin)v+=float(int(random()%12000)-6000);
   p.geometry.low=p.geometry.origin;p.geometry.high=p.geometry.origin;for(float& v:p.geometry.low)v-=500;for(float& v:p.geometry.high)v+=4000;
   p.geometry.segments={{1,0,0,3000},{0,1,-.1f,4000}};
   if(trial%3)p.geometry.events={{16,0,0,1200},{12,0,2000,2400},{14,0,3000,3800}};
   unsigned at=0x30000+i*64;write(at,uint32_t(p.geometry.events.size()));write(at+4,0x35000u+i*64);for(unsigned j=0;j<p.geometry.events.size();j++)write(0x35000+i*64+j*16,p.geometry.events[j]);write(at+8,2u);write(at+0xc,p.geometry.origin);write(at+0x18,0x33000u+i*32);write(at+0x1c,p.geometry.low);write(at+0x28,p.geometry.high);write(at+0x3c,p.field3C);
   write(0x33000+i*32,p.geometry.segments[0]);write(0x33010+i*32,p.geometry.segments[1]);
  }
  ssx::OriginalNpcRouteState state;state.pathIndex=trial%17?int(random()%count):-1;state.cache={{8,9,10},11,0};state.closestPoint={1,2,3};state.lookaheadPoint={4,5,6};state.previousDistance=3;state.currentDistance=4;state.lateralDistance=5;
  constexpr unsigned actor=0x40000,cache=0x50000;
  write(0x4d33a8,count);write(0x4d33ac,0x30000u);write(actor+0x110,position);write(actor+0x11c,1.f);
  write(actor+0xab8,state.pathIndex<0?0u:0x30000u+state.pathIndex*64);write(actor+0xabc,cache);
  write(cache,state.cache.origin);write(cache+16,state.cache.distance);write(cache+20,state.cache.segment);
  write(actor+0x490,state.closestPoint);write(actor+0x4a0,state.lookaheadPoint);write(actor+0x4c0,state.previousDistance);write(actor+0x4c4,state.currentDistance);write(actor+0x4c8,state.lateralDistance);
  write(actor+0x6c0,0x48000u);write(0x48040,int16_t(0));write(0x48044,0x33fff0u);allowEnd=trial%2;
  R5900Context c{};c.pc=0x112d58;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,actor);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  sub_00112D58_0x112d58(memory.data(),&c,&rt);
  auto effects=ssx::originalResetRoute(paths,state,position,allowEnd);changed+=effects.changed;refreshed+=effects.refreshed;
  const unsigned expectedPointer=state.pathIndex<0?0:0x30000+state.pathIndex*64;
  if(c.pc!=0x12345678||word(actor+0xab8)!=expectedPointer||scalar(actor+0x4c0)!=state.previousDistance||scalar(actor+0x4c4)!=state.currentDistance||scalar(actor+0x4c8)!=state.lateralDistance||int(word(cache+20))!=state.cache.segment||scalar(cache+16)!=state.cache.distance){printf("reset route mismatch %u path %d/%d distance %.9g/%.9g\n",trial,state.pathIndex,int(word(actor+0xab8)-0x30000)/64,state.currentDistance,scalar(actor+0x4c4));return 3;}
  for(unsigned k=0;k<3;k++)if(scalar(actor+0x490+k*4)!=state.closestPoint[k]||scalar(actor+0x4a0+k*4)!=state.lookaheadPoint[k]||scalar(cache+k*4)!=state.cache.origin[k]){printf("reset point/cache mismatch %u/%u point %.9g/%.9g look %.9g/%.9g\n",trial,k,state.closestPoint[k],scalar(actor+0x490+k*4),state.lookaheadPoint[k],scalar(actor+0x4a0+k*4));return 4;}
 }
 ssx::OriginalRacePath samplePath;samplePath.origin={-100,500,200};samplePath.segments={{1,0,0,3000},{0,1,-.1f,4000}};
 samplePath.events={{16,0,0,1200},{12,0,2000,2400},{14,0,3000,3800},{16,0,1900,2600}};
 write(0x30000,uint32_t(samplePath.events.size()));write(0x30004,0x35000u);write(0x30008,2u);write(0x3000c,samplePath.origin);write(0x30018,0x33000u);
 for(unsigned i=0;i<2;i++)write(0x33000+i*16,samplePath.segments[i]);for(unsigned i=0;i<samplePath.events.size();i++)write(0x35000+i*16,samplePath.events[i]);
 for(float input:{-50.f,0.f,1.f,1199.999f,1200.f,1200.001f,1899.f,1900.f,1999.999f,2000.f,2000.001f,2399.f,2400.f,2599.f,2600.f,2999.f,3000.f,3000.001f,3799.f,3800.f,3800.001f,6999.f,7000.f,8000.f}){
  write(0x60000,input);R5900Context c{};c.pc=0x269f18;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x60010);SET_GPR_U32(&c,5,0x30000);SET_GPR_U32(&c,6,0x60000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  sub_00269F18_0x269f18(memory.data(),&c,&rt);float distance=input;auto point=ssx::originalResetPathSample(samplePath,distance);
  if(c.pc!=0x12345678||distance!=scalar(0x60000)){printf("reset event-distance mismatch %.9g\n",input);return 5;}
  for(unsigned k=0;k<3;k++)if(point[k]!=scalar(0x60010+k*4)){printf("reset event-point mismatch %.9g/%u\n",input,k);return 6;}
  R5900Context d{};d.pc=0x26a8b8;d.f[12]=input;SET_GPR_U32(&d,4,0x60030);SET_GPR_U32(&d,5,0x30000);SET_GPR_U32(&d,31,0x12345678);sub_0026A8B8_0x26a8b8(memory.data(),&d,&rt);auto direction=ssx::originalResetPathDirection(samplePath,input);if(d.pc!=0x12345678)return 7;for(unsigned k=0;k<3;k++)if(direction[k]!=scalar(0x60030+k*4))return 8;
 }
 printf("24 reset event-boundary samples and path directions match exact point and by-reference distance, including overlapping12/14/16 events\n");
 printf("10000 original reset-route selections/refreshes exact: %u switches, %u refreshes; includes missing route, unchanged winner, path-end permission and field3C filtering\n",changed,refreshed);
}
