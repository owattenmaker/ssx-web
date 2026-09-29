#pragma once
#include "terrain_contact_math.hpp"
#include <array>
namespace ssx {
//36A6EC..36AA20 after projected near/far bins and packed RGB are prepared.
//Returns the original 256-entry swizzled CLUT. Unknown equation values leave
//entries below nearBin untouched, matching the source's branch behavior.
inline void originalFogDepthTable(std::array<uint32_t,256>& table,int nearBin,int farBin,
                                 int equation,float density,uint32_t rgb){
 terrain_original::Rounding rounding;
 if(nearBin<0||nearBin>255||farBin<0||farBin>255)throw std::runtime_error("Fog depth bin out of range");
 const int span=std::max(1,nearBin-farBin+1);float inverse=originalScalarDivide(1.f,float(span));
 auto slot=[](unsigned i){return (i&~24u)|((i&8u)<<1)|((i&16u)>>1);};
 for(int i=0;i<nearBin;++i){
  unsigned alpha;
  if(equation==0)alpha=unsigned(std::clamp(((std::max(farBin,i)-farBin)<<7)/span,0,128));
  else if(equation==1||equation==2){
   float x=terrain_original::mul(density,std::bit_cast<float>(0x3dcccccdu));
   x=terrain_original::mul(x,float(nearBin-std::max(farBin,i)+1));x=terrain_original::mul(x,inverse);
   if(equation==2)x=terrain_original::mul(x,x);x=-x;
   float p=terrain_original::mul(x,std::bit_cast<float>(0x39500d03u));
   for(auto coefficient:{0x3ab60b62u,0x3c088889u,0x3d2aaaabu,0x3e2aaaabu,0x3f000000u,0x3f800000u})p=terrain_original::mul(originalScalarAdd(p,std::bit_cast<float>(coefficient)),x);
   p=originalScalarAdd(p,1.f);alpha=unsigned(int32_t(terrain_original::mul(std::clamp(p,0.f,1.f),128.f)));
  }else continue;
  table[slot(i)]=rgb|(alpha<<24);
 }
 for(int i=nearBin;i<256;++i)table[slot(i)]=rgb|0x80000000u;
}
}
namespace ssx {
struct OriginalFogDepthParameters {int nearBin=0,farBin=0;uint32_t rgb=0;};
//36A478..36A618: project (0,0,distance,1), divide by W, convert to signed
//integer depth and keep the clamped high byte. Matrix uses four column vectors.
inline OriginalFogDepthParameters originalFogDepthParameters(const std::array<float,16>& projection,
 float nearCm,float farCm,const std::array<float,3>& color){
 terrain_original::Rounding rounding;
 auto integer=[](float x)->int32_t{if(!std::isfinite(x)||double(x)<-2147483648.||double(x)>=2147483648.)throw std::runtime_error("Fog projection outside supported conversion range");return int32_t(x);};
 auto bin=[&](float depth){
  std::array<float,4> projected{};
  for(unsigned k=0;k<4;++k){float v=terrain_original::mul(projection[k],0.f);v=terrain_original::add(v,terrain_original::mul(projection[4+k],0.f));v=terrain_original::add(v,terrain_original::mul(projection[8+k],depth));projected[k]=terrain_original::add(v,terrain_original::mul(projection[12+k],1.f));}
  float reciprocal=originalScalarDivide(1.f,projected[3]);float z=terrain_original::mul(projected[2],reciprocal);
  return std::clamp(integer(z)>>8,0,255);
 };
 OriginalFogDepthParameters result;result.nearBin=bin(nearCm);result.farBin=bin(farCm);
 auto component=[&](unsigned i){return uint32_t(integer(terrain_original::mul(color[i],255.f)));};
 result.rgb=component(0)|(component(1)<<8)|(component(2)<<16);return result;
}
}
namespace ssx {
struct OriginalFogDepthProjection {float slope=0,offset=0;};
//376CF0..376D20 choose depth range;376EA4..376F70 build matrix Z terms.
//View-space Z projects to slope + offset/Z, with W=Z.
inline OriginalFogDepthProjection originalFogDepthProjection(float nearCm,float farCm,float viewportDepthOffset,uint32_t depthFormat){
 terrain_original::Rounding rounding;
 const float range=depthFormat==0x31?16777215.f:65535.f;
 float delta=originalScalarSubtract(farCm,nearCm);
 float slope=originalScalarDivide(terrain_original::mul(-range,nearCm),delta);
 float offset=originalScalarAdd(terrain_original::mul(-farCm,slope),viewportDepthOffset);
 return {slope,offset};
}
}
