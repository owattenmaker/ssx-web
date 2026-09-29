#include "ps2_runtime_macros.h"
#include "../engine/rail_motion.hpp"
#include "json.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
void sub_00335128_0x335128(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){
 if(argc!=3)return 2;
 std::ifstream elfFile(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(elfFile)),{}),ram(32*1024*1024);std::memcpy(ram.data()+0xff000,elf.data(),elf.size());
 std::ifstream catalog(argv[2]);auto data=nlohmann::json::parse(catalog);
 auto put=[&](unsigned at,const auto& value){std::memcpy(ram.data()+at,&value,sizeof(value));};
 auto check=[&](unsigned at,const auto& value){return std::memcmp(ram.data()+at,&value,sizeof(value))==0;};
 PS2Runtime runtime;unsigned cases=0,mismatches=0;ssx::rail_original::Rounding rounding;
 for(const auto& rail:data.at("rails"))for(const auto& part:rail.at("segments")){
  ssx::OriginalRailRecord record;record.surface=rail.at("header_words").at("surface_id");
  ssx::OriginalRailSegment segment;segment.coefficients=part.at("source").at("coefficients").get<std::array<ssx::RailVector,4>>();segment.boundsMin=part.at("source").at("bounds_min").get<ssx::RailVector>();segment.boundsMax=part.at("source").at("bounds_max").get<ssx::RailVector>();
  put(0x20068,0x21000u);put(0x21028,record.surface);put(0x2006c,segment.boundsMin);put(0x20078,segment.boundsMax);
  for(unsigned i=0;i<4;++i){put(0x20010+i*16,segment.coefficients[i]);put(0x2001c+i*16,i==3?1.f:0.f);}
  for(unsigned sample=1;sample<20;++sample)for(int direction:{-1,1}){
   float t=float(sample)/20,t2=ssx::rail_original::mul(t,t);auto q=ssx::originalRailCurvePoint(segment,{ssx::rail_original::mul(t,t2),t2,t,1});
   auto tangent=ssx::rail_original::normalizeRsqrt(ssx::originalRailCurvePoint(segment,{3*t2,2*t,1,0}));for(unsigned k=0;k<3;++k)q[k]-=direction*tangent[k]*10;
   auto minimum=ssx::rail_original::vsub(q,{300,300,300}),maximum=ssx::rail_original::vadd(q,{300,300,300});
   put(0x22000,minimum);put(0x22010,maximum);put(0x23000,q);put(0x2300c,1.f);put(0x24000,0u);put(0x24010,0.f);
   R5900Context c{};c.pc=0x335128;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);
   SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x22000);SET_GPR_U32(&c,6,0x23000);SET_GPR_U32(&c,7,0x24000);SET_GPR_U32(&c,8,0x24010);SET_GPR_U32(&c,9,0x25000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
   sub_00335128_0x335128(ram.data(),&c,&runtime);if(c.pc!=0x12345678)throw std::runtime_error("Incomplete original spline query");
   ssx::OriginalRailQueryResult hit;ssx::originalRailSegmentQuery(record,segment,minimum,maximum,q,hit);unsigned found=hit.found;
   bool same=check(0x24000,found)&&(!found||(check(0x24010,hit.distance)&&check(0x25000,hit.point)&&check(0x25010,hit.tangent)&&check(0x25068,hit.t)&&check(0x2504c,hit.surface)));
   if(!same){if(mismatches++<5){float originalT;std::memcpy(&originalT,ram.data()+0x25068,4);std::cerr<<"Rail query mismatch "<<rail.at("name")<<" segment "<<part.at("index")<<" sample "<<sample<<" t "<<originalT<<'/'<<hit.t<<'\n';}}
   ++cases;
  }
 }
 std::cout<<cases<<" original335128 segment queries: "<<mismatches<<" mismatches (found, distance, point, tangent, parameter, surface).\n";
 return mismatches?1:0;
}
