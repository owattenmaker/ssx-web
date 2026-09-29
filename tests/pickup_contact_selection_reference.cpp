#include "ps2_runtime_macros.h"
#include "../engine/pickup_contact_selection.hpp"
#include <cstring>
#include <random>
#include <cstdio>
void sub_00104E70_0x104e70(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static std::vector<std::array<uint8_t,128>> packets;
static float radius;
static void query(uint8_t*m,R5900Context*c,PS2Runtime*){
 if(GPR_U32(c,7)!=64||GPR_U32(c,8)!=0)throw std::runtime_error("Unexpected contact query capacity/filter");
 for(unsigned i=0;i<packets.size();i++)std::memcpy(m+GPR_U32(c,6)+128*i,packets[i].data(),128);
 SET_GPR_U32(c,2,packets.size());c->pc=GPR_U32(c,31);
}
int main(){std::vector<uint8_t>m(32*1024*1024);PS2Runtime rt;rt.registerFunction(0x334458,query);rt.registerFunction(0x123400,[](uint8_t*,R5900Context*c,PS2Runtime*){c->f[0]=radius;c->pc=GPR_U32(c,31);});
 auto put=[&](unsigned a,const auto&v){std::memcpy(m.data()+a,&v,sizeof(v));};auto get=[&](unsigned a){uint32_t v;std::memcpy(&v,m.data()+a,4);return v;};
 constexpr unsigned rider=0x100000,shape=0x200000,output=0x300000;
 put(0x4a30f0-0x7f38,uint32_t(981668463));put(rider+0xaa0,0x700000u);put(shape+0x50,0x800000u);put(0x80001c,0x123400u);
 std::mt19937 rng(0x104e70);std::uniform_real_distribution<float> random(-100,100);
 for(unsigned n=0;n<20000;n++){
  unsigned count=1+rng()%64;packets.assign(count,{});std::vector<ssx::OriginalPickupContactCandidate> candidates(count);
  std::array<float,3> center={random(rng),random(rng),random(rng)};put(0x700010,center);put(0x70001c,1.f);radius=random(rng);
  for(unsigned i=0;i<count;i++){
   auto&x=candidates[i];x.instance=rng()%17;x.priority=rng()%2;x.distance=n%3==0?20.f:random(rng);
   for(unsigned k=0;k<3;k++){x.point[k]=random(rng);x.normal[k]=random(rng);}
   auto store=[&](unsigned a,const auto&v){std::memcpy(packets[i].data()+a,&v,sizeof(v));};store(0,x.point);store(12,1.f);store(16,x.normal);store(0x40,x.distance);store(0x50,0x400000u+i*256);store(0x7c,i);
   put(0x400078+i*256,x.instance);put(0x400088+i*256,0x600000u+i*256);put(0x600010+i*256,x.priority?1.f:0.f);put(0x600018+i*256,uint16_t(1));
  }
  R5900Context c{};c.pc=0x104e70;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,rider);SET_GPR_U32(&c,5,shape);SET_GPR_U32(&c,6,output);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x90000);SET_GPR_U32(&c,31,0x12345678);
  ssx::OriginalRounding rounding;sub_00104E70_0x104e70(m.data(),&c,&rt);auto expected=ssx::originalPickupContactSelection(candidates,center,radius);
  if(c.pc!=0x12345678||get(output+0x7c)!=unsigned(expected.selected)){printf("Selection mismatch %u expected%d actual%u\n",n,expected.selected,get(output+0x7c));return 1;}
  for(unsigned i=0;i<expected.instances.size();i++)if(get(rider+0x5b8+4*i)!=expected.instances[i])return 2;
  if(get(rider+0x5b8+4*expected.instances.size())!=0xffffffffu)return 3;
 }
 puts("20,000 original104E70 instance selections and unique contact lists match");
}
