#include "../engine/software_float.hpp"
#include <array>
#include <cfenv>
#include <cstdio>
#include <fstream>
#include <random>
#pragma STDC FENV_ACCESS ON
static std::array<float,5> software(float a,float b){
 using namespace ssx::software_float;return {add(a,b),sub(a,b),mul(a,b),div(a,b),ssx::software_float::sqrt(a)};
}
int main(int argc,char**argv){
 if(argc!=2)return 1;std::ofstream corpus(argv[1],std::ios::binary);std::mt19937 random(0x524f554e);unsigned cases=0;
 auto check=[&](float a,float b){
  std::fesetround(FE_TONEAREST);auto actual=software(a,b);
  std::fesetround(FE_TOWARDZERO);volatile float x=a,y=b;
  std::array<float,5> expected{x+y,x-y,x*y,x/y,std::sqrt(x)};
  for(unsigned i=0;i<5;i++)if(!(std::isnan(actual[i])&&std::isnan(expected[i]))&&std::bit_cast<uint32_t>(actual[i])!=std::bit_cast<uint32_t>(expected[i])){
   std::printf("case %u op %u input %08x %08x actual %08x expected %08x\n",cases,i,std::bit_cast<uint32_t>(a),std::bit_cast<uint32_t>(b),std::bit_cast<uint32_t>(actual[i]),std::bit_cast<uint32_t>(expected[i]));return false;
  }
  corpus.write((char*)&a,4);corpus.write((char*)&b,4);corpus.write((char*)expected.data(),20);++cases;return true;
 };
 constexpr std::array<uint32_t,18> edges={0,0x80000000,1,0x80000001,0x7fffff,0x807fffff,0x800000,0x80800000,0x3f800000,0xbf800000,0x3f7fffff,0x3f800001,0x7f7fffff,0xff7fffff,0x00800001,0x3f000000,0x33800000,0xb3800000};
 for(auto a:edges)for(auto b:edges)if(!check(std::bit_cast<float>(a),std::bit_cast<float>(b)))return 2;
 for(unsigned n=0;n<250000;n++){
  auto finite=[&](){uint32_t bits;do{bits=random();}while((bits&0x7f800000u)==0x7f800000u);return std::bit_cast<float>(bits);};
  float a=finite(),b=finite();if(!check(a,b))return 3;
 }
 std::fesetround(FE_TONEAREST);std::printf("%u operand pairs: software add/sub/mul/div/sqrt match native toward-zero bits\n",cases);
 return !corpus.good();
}
