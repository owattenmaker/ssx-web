#include "../engine/software_float.hpp"
#include "../engine/terrain_contact_math.hpp"
extern "C" unsigned rounding_scope_test(){
 using ssx::terrain_original::div;
 const auto bits=[](float value){return std::bit_cast<uint32_t>(value);};
 if(bits(div(1,3))!=0x3eaaaaab)return 1;
 {
  ssx::OriginalRounding chop;
  if(bits(div(1,3))!=0x3eaaaaaa)return 2;
  if(bits(ssx::originalScalarDivide(1,3))!=0x3eaaaaab)return 3;
  if(bits(div(1,3))!=0x3eaaaaaa)return 4;
  {ssx::OriginalRounding nearest(FE_TONEAREST);if(bits(div(1,3))!=0x3eaaaaab)return 5;}
  if(bits(div(1,3))!=0x3eaaaaaa)return 6;
 }
 return bits(div(1,3))==0x3eaaaaab?0:7;
}
extern "C" void evaluate(float* values,unsigned count){
 for(unsigned i=0;i<count;i++){
  auto* p=values+i*7;const float a=p[0],b=p[1];
  p[2]=ssx::software_float::add(a,b);p[3]=ssx::software_float::sub(a,b);
  p[4]=ssx::software_float::mul(a,b);p[5]=ssx::software_float::div(a,b);p[6]=ssx::software_float::sqrt(a);
 }
}
