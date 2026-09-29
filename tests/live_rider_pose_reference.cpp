#include "ps2_runtime_macros.h"
#include <fstream>
#include <cfenv>
#include <cstdio>
#define FN(n) void sub_##n(uint8_t*,R5900Context*,PS2Runtime*);
FN(0011EB98_0x11eb98) FN(0011FA10_0x11fa10) FN(00310200_0x310200) FN(0011F3D8_0x11f3d8) FN(0031BE50_0x31be50) FN(0031C128_0x31c128)
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x320000,g_ps2RecompiledFunctionTableSlotCount=0x88000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x88000]={nullptr};
int main(int argc,char**argv){PS2Runtime rt;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t>ram((std::istreambuf_iterator<char>(file)),{});auto before=ram;auto u=[&](unsigned a){uint32_t x;memcpy(&x,ram.data()+a,4);return x;};auto f=[&](const auto&m,unsigned a){float x;memcpy(&x,m.data()+a,4);return x;};
#define REG(a,n) rt.registerFunction(a,sub_##n);
 REG(0x11FA10,0011FA10_0x11fa10) REG(0x310200,00310200_0x310200) REG(0x11F3D8,0011F3D8_0x11f3d8) REG(0x31BE50,0031BE50_0x31be50) REG(0x31C128,0031C128_0x31c128)
 rt.registerFunction(0x11FEE8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0);c->pc=GPR_U32(c,31);});rt.registerFunction(0x106828,[](uint8_t*,R5900Context*c,PS2Runtime*){c->pc=GPR_U32(c,31);});
 std::fesetround(FE_TOWARDZERO);R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);SET_GPR_U32(&c,4,0x14701a0);sub_0011EB98_0x11eb98(ram.data(),&c,&rt);printf("pc%x\n",c.pc);auto once=ram;unsigned geo=u(0x14701a0+0x780),at=u(geo+0x2c);for(unsigned i=0;i<29;++i){float p=0,q=0;for(unsigned k=0;k<3;++k)p=std::max(p,std::abs(f(ram,at+i*32+k*4)-f(before,at+i*32+k*4)));for(unsigned k=0;k<4;++k)q=std::max(q,std::abs(f(ram,at+i*32+16+k*4)-f(before,at+i*32+16+k*4)));printf("%u p%.9g q%.9g\n",i,p,q);}
 unsigned other=0;for(unsigned i=0;i<ram.size();++i)if(ram[i]!=before[i]&&!(i>=0xf000&&i<0x11000)&&!(i>=at&&i<at+29*32)&&!(i>=0x1470300&&i<0x1470340)){if(other<40)printf("otherwrite%x\n",i);++other;}printf("non-output changed bytes %u\n",other);
 c={};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);SET_GPR_U32(&c,4,0x14701a0);sub_0011EB98_0x11eb98(ram.data(),&c,&rt);printf("second pose identical %d\n",memcmp(ram.data()+at,once.data()+at,29*32)==0);
 }
