#pragma once
#include "original_camera.hpp"
namespace ssx {
struct OriginalIrradianceAngles {float pitch=0,yaw=0;};
//389E98..389F9C, using the source atan/asin helpers. The horizontal magnitude
//comes from the preceding VU stage; yaw recomputes its denominator with EE SQRT.
inline OriginalIrradianceAngles originalIrradianceAngles(const std::array<float,4>& direction,float horizontal){
 terrain_original::Rounding rounding;OriginalIrradianceAngles result;
 result.pitch=-original_camera::atanEE(originalScalarDivide(direction[2],horizontal));
 if(horizontal>0){auto flat=direction;flat[2]=0;const std::array<float,4> axis{1,0,0,0};
  auto dot=[](const auto&a,const auto&b){float sum=terrain_original::mul(a[0],b[0]);for(unsigned i=1;i<4;++i)sum=terrain_original::add(sum,terrain_original::mul(a[i],b[i]));return sum;};
  float denominator=originalScalarSqrt(terrain_original::mul(dot(flat,flat),dot(axis,axis)));
  if(denominator>=std::bit_cast<float>(0x3a83126fu)){float cosine=originalScalarDivide(dot(flat,axis),denominator);result.yaw=originalScalarSubtract(std::bit_cast<float>(0x3fc90fdbu),original_camera::asinEE(std::clamp(cosine,-1.f,1.f)));}
  if(flat[1]<0)result.yaw=originalScalarSubtract(std::bit_cast<float>(0x40c90fdbu),result.yaw);
 }
 return result;
}
}
