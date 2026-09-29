#include "ps2_runtime_macros.h"
#include "../engine/painter_driver.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_002C0408_0x2c0408(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static std::vector<int> calls;static std::vector<float> weights;static bool matching,present;static float px,py;
static void check(bool b){if(!b)throw std::runtime_error("Painter driver source mismatch");}
int main(){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;auto put=[&](uint32_t at,auto v){std::memcpy(m.data()+at,&v,sizeof(v));};
constexpr uint32_t gp=0x4a30f0;
put(gp-0x848,0x60000u);put(0x60084,0x61000u);put(0x61010,0x62000u);put(0x62008,0x63000u);put(0x63004,0x64000u);put(0x64008,0x65000u);put(0x65004,1u);put(gp+0x770,0u);put(gp-0x4284,-99999.f);put(gp-0x4280,-99999.f);
put(0x20000,0x21000u);put(0x20010,0x500u);put(0x21004,0x40000u);put(0x30004,0x31000u);
for(auto [slot,target]:{std::pair{0x210,0x100200},std::pair{0x220,0x100100},std::pair{0x228,0x100300}}){put(0x40000+slot,0u);put(0x40004+slot,uint32_t(target));}
rt.registerFunction(0x2c0a10,[](uint8_t*,R5900Context*c,PS2Runtime*){check(GPR_U32(c,4)==0x20000&&GPR_U32(c,5)==0x66000&&c->f[12]==px&&c->f[13]==py);calls.push_back(1);SET_GPR_U32(c,2,present?0x30000:0);c->pc=GPR_U32(c,31);});
rt.registerFunction(0x100100,[](uint8_t*,R5900Context*c,PS2Runtime*){check(GPR_U32(c,4)==0x21000&&GPR_U32(c,5)==0x31000);calls.push_back(2);SET_GPR_U32(c,2,matching);c->pc=GPR_U32(c,31);});
rt.registerFunction(0x100200,[](uint8_t*m,R5900Context*c,PS2Runtime*){check(GPR_U32(c,4)==0x21000&&GPR_U32(c,5)==0x31000);calls.push_back(3);weights.push_back(c->f[12]);float d;std::memcpy(&d,m+0x21000,4);d=ssx::originalScalarAdd(d,1.f);std::memcpy(m+0x21000,&d,4);c->pc=GPR_U32(c,31);});
rt.registerFunction(0x100300,[](uint8_t*m,R5900Context*c,PS2Runtime*){check(GPR_U32(c,4)==0x21000);calls.push_back(4);uint32_t zero=0;std::memcpy(m+0x21000,&zero,4);c->pc=GPR_U32(c,31);});
std::mt19937 rng(0x2c0778);auto random=[&](){return float(int(rng()%200001)-100000)/37.f;};
for(int n=0;n<20000;++n){using A=ssx::OriginalPainterAvailability;auto availability=n%5==0?A::MissingRegion:n%5==1?A::MissingSection:A::Ready;ssx::OriginalPainterDriverState s{n%7==0?-99999.f:random(),random(),random()};px=random();py=random();float rate=n%3==0?0.f:random()/1000.f,weight=n%4==0?random()/1000.f:-99999.f;matching=n%3==0;present=n%4!=1;uint32_t type=n%6==0?6:5;
put(0x64000,availability==A::MissingRegion?0u:3u);put(0x65008,availability==A::MissingSection?0u:0x66000u);put(0x20008,s.lastX);put(0x2000c,s.lastY);put(0x21000,s.distance);put(0x30000,type);put(0x31000,rate);
R5900Context c{};c.pc=0x2c0778;c.f[12]=px;c.f[13]=py;c.f[14]=weight;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,28,gp);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);calls.clear();weights.clear();
{ssx::terrain_original::Rounding round;sub_002C0408_0x2c0408(m.data(),&c,&rt);}check(c.pc==0x12345678);
std::vector<int> expected;std::vector<float> expectedWeights;ssx::OriginalPainterDriverAccess access;
access.sample=[&](float x,float y)->std::optional<ssx::OriginalPainterSample>{check(x==px&&y==py);expected.push_back(1);if(!present)return {};return ssx::OriginalPainterSample{type,rate};};access.matches=[&](){expected.push_back(2);return matching;};access.blend=[&](float w){expected.push_back(3);expectedWeights.push_back(w);s.distance=ssx::originalScalarAdd(s.distance,1.f);};access.reset=[&](){expected.push_back(4);s.distance=0;};
ssx::originalPainterDriverStep(s,px,py,5,availability,weight,access);
if(calls!=expected||weights!=expectedWeights||std::memcmp(m.data()+0x21000,&s.distance,4)||std::memcmp(m.data()+0x20008,&s.lastX,4)||std::memcmp(m.data()+0x2000c,&s.lastY,4)){printf("case %d availability%d\n",n,int(availability));return 1;}
}
puts("20000 original painter transition-driver cases match state bits, sample coordinates, weights and ordered callbacks; region data/virtual methods are controlled boundaries.");}
