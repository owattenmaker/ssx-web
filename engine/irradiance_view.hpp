#pragma once
#include "terrain_contact_math.hpp"
namespace ssx {
struct OriginalIrradianceViewDirection {std::array<float,4> eye{},direction{};float horizontalLength=0;};
//389D50..389E94 after the renderer supplies its view matrix. Reconstruct eye
//through the original transpose/negated-translation sequence, force W=1, then
//normalize the point-to-eye difference. This does not build the rim rotation.
inline OriginalIrradianceViewDirection originalIrradianceViewDirection(const std::array<float,16>& view,const std::array<float,4>& point){
 terrain_original::Rounding rounding;using namespace terrain_original;OriginalIrradianceViewDirection result;
 for(unsigned k=0;k<4;++k){float value=mul(view[k*4],mul(view[12],-1.f));for(unsigned j=1;j<4;++j)value=add(value,mul(view[k*4+j],mul(view[12+j],-1.f)));result.eye[k]=value;}
 result.eye[3]=1;
 float length2=0;
 for(unsigned k=0;k<4;++k){result.direction[k]=sub(point[k],result.eye[k]);float square=mul(result.direction[k],result.direction[k]);length2=k?add(length2,square):square;}
 const float inverse=length2>0?div(1.f,sqrt(length2)):0.f;
 for(auto&v:result.direction)v=mul(v,inverse);
 auto d=result.direction;d[2]=0;float sum=mul(d[0],d[0]);for(unsigned k=1;k<4;++k)sum=add(sum,mul(d[k],d[k]));result.horizontalLength=sqrt(sum);return result;
}
}
