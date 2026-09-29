#pragma once
#include "terrain_contact_math.hpp"
namespace ssx {
struct OriginalLocalLight {float radius=0;std::array<float,3> axis{},position{};};
struct OriginalLocalLightQuery {std::array<float,4> direction{};float distance=0,inverseDistance=0,axisCosine=0;};
//38A530: sphere acceptance and direction/axis preparation. Rejection preserves
//output storage. At coincident positions the source returns distance/inverse1.
inline bool originalLocalLightQuery(const std::array<float,3>& point,const OriginalLocalLight& light,OriginalLocalLightQuery& out){
 terrain_original::Rounding rounding;using terrain_original::mul;
 std::array<float,3> d;for(unsigned i=0;i<3;++i)d[i]=originalScalarSubtract(light.position[i],point[i]);
 float length2=originalScalarAdd(originalScalarAdd(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2]));
 if(mul(light.radius,light.radius)<length2)return false;
 float distance=1,inverse=1;if(length2!=0){distance=originalScalarSqrt(length2);inverse=originalScalarDivide(1.f,distance);}
 for(auto&v:d)v=mul(v,inverse);
 float cosine=originalScalarAdd(originalScalarAdd(mul(d[0],light.axis[0]),mul(d[1],light.axis[1])),mul(d[2],light.axis[2]));
 out={{d[0],d[1],d[2],1},distance,inverse,-cosine};return true;
}
}
