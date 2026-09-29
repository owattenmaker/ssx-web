#include "ps2_runtime_macros.h"
#include "../engine/irradiance.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_00389730_0x389730(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003897A0_0x3897a0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00389810_0x389810(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;std::mt19937 rng(0x389730);auto random=[&](){return std::bit_cast<float>((rng()&0x807fffffu)|((uint32_t(rng()%40)+107u)<<23));};
for(unsigned op=0;op<3;++op)for(unsigned n=0;n<12000;++n){ssx::OriginalIrradianceCoefficients a,b;for(auto&row:a)for(auto&v:row)v=random();for(auto&row:b)for(auto&v:row)v=random();float weight=n<3?float(n):random();std::memcpy(m.data()+0x20000,&a,160);std::memcpy(m.data()+0x30000,&b,160);uint32_t target=op==2||n%2?0x20000:0x40000;
R5900Context c{};c.pc=op==0?0x389730:op==1?0x3897a0:0x389810;c.f[12]=weight;SET_GPR_U32(&c,4,target);SET_GPR_U32(&c,5,0x20000);SET_GPR_U32(&c,6,0x30000);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
{ssx::terrain_original::Rounding round;if(op==0)sub_00389730_0x389730(m.data(),&c,&rt);else if(op==1)sub_003897A0_0x3897a0(m.data(),&c,&rt);else sub_00389810_0x389810(m.data(),&c,&rt);}
auto expected=op==0?ssx::originalIrradianceSum(a,b):ssx::originalIrradianceScale(a,weight);
if(c.pc!=0x12345678||GPR_U32((&c),2)!=target||std::memcmp(m.data()+target,&expected,160))throw std::runtime_error("Original irradiance VU bank operation mismatch");}
puts("12000 cases each: original irradiance VU sum, copy-scale and in-place scale match all40 words and returned pointer, including aliased destinations.");}
