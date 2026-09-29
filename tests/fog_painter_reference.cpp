#include "ps2_runtime_macros.h"
#include "../engine/fog_painter.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_002BCBD0_0x2bcbd0(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static int hooks;
int main(){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;
auto put=[&](uint32_t at,auto x){std::memcpy(m.data()+at,&x,sizeof(x));};
put(0x20004,uint32_t(0x40000));put(0x40218,uint32_t(0));put(0x4021c,uint32_t(0x2bda48));
rt.registerFunction(0x2bda48,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x20000)throw std::runtime_error("Wrong Fog hook owner");++hooks;c->pc=GPR_U32(c,31);});
std::mt19937 rng(0x2bcf38);auto random=[&](){return float(int(rng()%200001)-100000)/37.f;};
for(int n=0;n<20000;++n){ssx::OriginalFogPainterState s;std::array<float,6> v;
 for(unsigned i=0;i<6;++i){s.current[i]=random();s.sample[i]=random();v[i]=random();put(0x20008+i*8,s.current[i]);put(0x2000c+i*8,s.sample[i]);put(0x30004+i*4,v[i]);}
 float rate=random(),payloadRate=random(),weight=n<3?float(n):random()/1300.f;put(0x20000,rate);put(0x30000,payloadRate);
 R5900Context c{};c.pc=0x2bcf38;c.f[12]=weight;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x30000);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);hooks=0;
 {ssx::terrain_original::Rounding round;sub_002BCBD0_0x2bcbd0(m.data(),&c,&rt);}
 ssx::originalFogPainterBlend(s,v,weight);
 if(c.pc!=0x12345678||hooks!=2||std::memcmp(m.data()+0x20000,&rate,4))throw std::runtime_error("Fog dispatch/rate mismatch");
 for(unsigned i=0;i<6;++i)if(std::memcmp(m.data()+0x20008+i*8,&s.current[i],4)||std::memcmp(m.data()+0x2000c+i*8,&s.sample[i],4))throw std::runtime_error("Fog blend float mismatch");
}
for(int n=0;n<20000;++n){
 ssx::OriginalFogPainterState s;std::array<float,6> v;std::array<float,5> defaults;
 for(unsigned i=0;i<6;++i){s.current[i]=random();s.sample[i]=random();v[i]=s.current[i];put(0x20008+i*8,s.current[i]);put(0x2000c+i*8,s.sample[i]);}
 if(n%7<6)v[n%7]=random();for(unsigned i=0;i<6;++i)put(0x30004+i*4,v[i]);
 R5900Context c{};c.pc=0x2bdb38;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x30000);SET_GPR_U32(&c,31,0x12345678);
 sub_002BCBD0_0x2bcbd0(m.data(),&c,&rt);
 if(c.pc!=0x12345678||GPR_U32((&c),2)!=ssx::originalFogPainterMatches(s,v))throw std::runtime_error("Fog equality mismatch");
 float distance=random();put(0x20000,distance);
 for(unsigned i=0;i<5;++i){defaults[i]=random();put(0x4a30f0-0x42a4+i*4,defaults[i]);}
 c={};c.pc=0x2be108;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,0x12345678);sub_002BCBD0_0x2bcbd0(m.data(),&c,&rt);
 ssx::originalFogPainterDefaults(s,distance,defaults);
 if(c.pc!=0x12345678||std::memcmp(m.data()+0x20000,&distance,4))throw std::runtime_error("Fog reset distance mismatch");
 for(unsigned i=0;i<6;++i)if(std::memcmp(m.data()+0x20008+i*8,&s.current[i],4)||std::memcmp(m.data()+0x2000c+i*8,&s.sample[i],4))throw std::runtime_error("Fog reset field mismatch");
}
puts("20000 Fog equality checks and default resets match, including cleared density and preserved last-sample slots.");
puts("20000 original Fog weighted blends match all12 floats; rate unchanged; both empty class hooks observed.");
}
