#pragma once
#include "terrain_contact_math.hpp"
#include "original_float.hpp"
#include <limits>
namespace ssx {
struct OriginalRailContactState {
 terrain_original::Vector position{},velocity{},groundNormal{},forward{},previousPosition{};
 float stationarySeconds=0;
};
struct OriginalRailContactResult {
 bool accepted=false,notify=false;terrain_original::Vector translation{},normal{},incomingDirection{};
 float impulse=0;
};
//13C140 after the physical query. The callback receives the exact106538
//translation for companion volume/bounds fields, then models11E098 and updates
//forward from the rebuilt orientation. Do not recover the delta by subtracting
//large rounded world positions.
template<class Rebuild>
OriginalRailContactResult originalRailContactResponse(OriginalRailContactState&s,float depth,
        terrain_original::Vector normal,Rebuild&&rebuild){
 using namespace terrain_original;OriginalRounding rounding;OriginalRailContactResult out;
 auto length=[](Vector v){return terrain_original::sqrt(add(dot(v,v),0.f));};
 auto scale=[](Vector v,float f){for(auto&x:v)x=mul(x,f);return v;};
 auto unit=[&](Vector v){float d=length(v);return scale(v,d>0?div(1.f,d):std::numeric_limits<float>::max());};
 if(depth<0)return out;
 float alignment=add(dot(s.groundNormal,normal),0.f);if(alignment<-.9998999834060669f)return out;
 if(alignment<0)normal=unit(difference(normal,scale(s.groundNormal,alignment)));
 out.accepted=true;out.normal=normal;out.translation=scale(scale(normal,1.100000023841858f),depth);
 for(unsigned k=0;k<3;k++)s.position[k]=add(s.position[k],out.translation[k]);
 rebuild(s,out.translation);
 float moved=length(difference(s.previousPosition,s.position));s.previousPosition=s.position;
 if(moved>2.f)s.stationarySeconds=0;
 else {
  s.stationarySeconds=originalScalarAdd(s.stationarySeconds,.01666666753590107f);
  if(s.stationarySeconds>.5f){float along=add(dot(s.forward,normal),0.f);
   auto direction=along<-.9990000128746033f?s.forward:unit(difference(s.forward,scale(normal,along)));
   s.velocity=scale(direction,1111.111083984375f);
  }
 }
 float closing=add(dot(scale(normal,-1),s.velocity),0.f);if(closing<0)return out;
 out.incomingDirection=unit(s.velocity);out.impulse=std::max(mul(closing,.5f),27.77777862548828f);
 float amount=originalScalarAdd(closing,out.impulse);auto change=scale(normal,amount);
 for(unsigned k=0;k<3;k++)s.velocity[k]=add(s.velocity[k],change[k]);
 out.notify=true;return out;
}
}
