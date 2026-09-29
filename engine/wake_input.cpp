#include "wake_input.hpp"
#include <algorithm>
namespace ssx {
OriginalWakeTargets originalWakeTargets(const OriginalWakeFrameInput& in){
 using namespace terrain_original;Rounding rounding;OriginalWakeTargets out;out.normal=in.normal;
 auto scale=[](Vector v,float value){for(auto& x:v)x=mul(x,value);return v;};
 out.signedTurn=-in.turn1F0;out.turnAmount=mul(std::abs(out.signedTurn),originalScalarSubtract(1.f,std::abs(in.brake214)));
 out.direction=in.reverse320?scale(in.scaledAxis0,-1.f):in.scaledAxis0;out.side=cross(out.direction,in.normal);if(out.signedTurn<0)out.side=scale(out.side,-1.f);
 const float speed=terrain_original::sqrt(dot(in.velocity,in.velocity));out.speed=std::clamp(speed,0.f,1666.666748046875f);
 const float stance=in.reverse320?-1.f:1.f,side=out.signedTurn<0?-1.f:1.f;
 const float longitudinal=originalScalarAdd(mul(std::abs(in.lean208),118.f),-40.f);
 auto along=scale(scale(in.unitAxis0,longitudinal),stance),across=scale(scale(scale(in.unitAxis2,15.f),side),stance);
 for(unsigned k=0;k<3;k++)out.point[k]=add(add(in.boardPosition[k],along[k]),across[k]);
 const float squared=dot(in.velocity,in.velocity);auto direction=scale(in.velocity,squared==0?std::bit_cast<float>(0x7f7fffffu):div(1.f,terrain_original::sqrt(squared)));
 float weight=originalScalarAdd(mul(std::abs(dot(in.lateral,direction)),.800000011920929f),mul(out.turnAmount,.5f));weight=std::min(weight,1.f);
 const float low=std::clamp(originalScalarAdd(weight,weight),0.f,.25f);
 const float high=out.speed>555.5555419921875f?std::clamp(mul(originalScalarSubtract(weight,.25f),2.700000047683715f),0.f,.3499999940395355f):0.f;
 if(in.surfaceFlag84&&out.speed>1111.111083984375f)out.growth=std::clamp(mul(originalScalarSubtract(weight,.5400000214576721f),35.f),0.f,4.5f);
 out.amplitude=mul(mul(out.speed,originalScalarAdd(low,high)),in.surfaceScale4C);out.alpha=in.surfaceAlpha48;return out;
}
}
