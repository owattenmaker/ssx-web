#include "ground_pose_motion.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cfenv>
#pragma STDC FENV_ACCESS ON
namespace ssx {namespace {
using Round=OriginalRounding;
constexpr float f(uint32_t bits){return std::bit_cast<float>(bits);}
}
float originalCosine(float x){
    Round round;float scaled=terrain_original::mul(x,f(0x3f22f983u));scaled=x<0?originalScalarSubtract(scaled,.5f):originalScalarAdd(scaled,.5f);int quadrant=int(scaled);
    float offset=terrain_original::mul(float(quadrant),f(0x3fc90fdbu));x=originalScalarSubtract(x,offset);float square=terrain_original::mul(x,x),value;
    if(quadrant&1){value=terrain_original::mul(square,f(0x3638ef1fu));value=originalScalarAdd(value,f(0xb9500d03u));value=terrain_original::mul(value,square);value=originalScalarAdd(value,f(0x3c088889u));value=terrain_original::mul(value,square);value=originalScalarAdd(value,f(0xbe2aaaabu));value=terrain_original::mul(value,square);value=terrain_original::mul(value,x);value=originalScalarAdd(value,x);}
    else{value=terrain_original::mul(square,f(0x37d00d03u));value=originalScalarAdd(value,f(0xbab60b62u));value=terrain_original::mul(value,square);value=originalScalarAdd(value,f(0x3d2aaaabu));value=terrain_original::mul(value,square);value=originalScalarAdd(value,-.5f);value=terrain_original::mul(value,square);value=originalScalarAdd(value,1.f);}
    return ((quadrant+1)&2)?-value:value;
}
void originalGroundBoardLift(OriginalGroundState&s,const std::array<float,3>&v){
    // 0x13F07C..0x13F084: vadda x^2+y^2, vmadda vf0w 1.0 x z^2, vmadd 1.0 x w, then vsqrt (the speed)
    Round round;float x=terrain_original::mul(v[0],v[0]),y=terrain_original::mul(v[1],v[1]),z=terrain_original::mul(v[2],v[2]);float speedSquared=terrain_original::add(x,y);speedSquared=terrain_original::add(speedSquared,terrain_original::mul(1.f,z));speedSquared=terrain_original::add(speedSquared,0.f);float speed=terrain_original::sqrt(speedSquared);
    float factor=terrain_original::mul(speed,f(0x39bcbe62u));factor=std::min(factor,1.f);float amplitude=originalScalarAdd(std::abs(s.turn.current),std::abs(s.brake.current));amplitude=originalScalarAdd(amplitude,.1f);amplitude=terrain_original::mul(factor,amplitude);
    float delta=terrain_original::mul(amplitude,f(0x4029999au));s.boardBouncePhase=originalScalarAdd(s.boardBouncePhase,delta);if(s.boardBouncePhase>=f(0x40c90fdcu))s.boardBouncePhase=originalScalarSubtract(s.boardBouncePhase,f(0x40c90fdcu));
    float lift=originalCosine(s.boardBouncePhase);lift=terrain_original::mul(lift,7.5f);s.boardLift=terrain_original::mul(lift,amplitude);
}
}

namespace ssx {
void originalGroundPresentationTarget(const OriginalGroundProfile&p,OriginalGroundState&s,const OriginalGroundDiagnostics&d){
    Round round;float x=terrain_original::mul(s.position[0],d.boardNormalForPose[0]),y=terrain_original::mul(s.position[1],d.boardNormalForPose[1]),z=terrain_original::mul(s.position[2],d.boardNormalForPose[2]);float projection=terrain_original::add(x,y);projection=terrain_original::add(projection,terrain_original::mul(1.f,z)); /*0x13E15C vmadda 1.0 x z*/projection=terrain_original::add(projection,0.f);
    float difference=originalScalarSubtract(d.poseReferenceHeight,projection);difference=terrain_original::mul(difference,10.f);
    float candidate=originalScalarAdd(s.presentationLift.current,difference);float decay=terrain_original::mul(d.stepTime,20.f);decay=originalScalarSubtract(1.f,decay);candidate=terrain_original::mul(candidate,decay);
    float low=originalScalarSubtract(1.f,s.crouch.current);low=terrain_original::mul(low,(-20.f));float high=terrain_original::mul(s.crouch.current,10.f);
    candidate=low<=candidate?std::min(candidate,high):low;
    if(s.animationClass==10||s.animationClass==4||s.animationClass==5||s.animationIndex==22)candidate=0;
    s.presentationLift.target=terrain_original::mul(candidate,p.bodyScale);s.presentationLift.rate=terrain_original::mul(d.stepTime,100.f);
}
}

namespace ssx {
void originalGroundBoardNormal(OriginalGroundState&s){
    Round round;std::array<float,3> value;
    for(unsigned i=0;i<3;++i){float term=terrain_original::mul(s.normal[i],.5f);value[i]=terrain_original::add(s.boardNormal[i],term);}
    // 0x13F32C..0x13F344: the horizontal VU dot (vadda x+y, vmadda vf0w 1.0 x z^2, vmadd 1.0 x w), then vrsqrt / vmul
    float x=terrain_original::mul(value[0],value[0]),y=terrain_original::mul(value[1],value[1]),z=terrain_original::mul(value[2],value[2]);float sum=terrain_original::add(x,y);sum=terrain_original::add(sum,terrain_original::mul(1.f,z));sum=terrain_original::add(sum,0.f);float inverse=terrain_original::div(1.f,terrain_original::sqrt(sum));
    for(unsigned i=0;i<3;++i)s.boardNormal[i]=terrain_original::mul(value[i],inverse);
}
}

namespace ssx {
void originalGroundVisualTargets(const OriginalGroundProfile& profile,OriginalGroundState& s,float dt,const std::array<float,3>& v){
 Round round;bool lean=s.controlState!=1&&(profile.surface.id==2||profile.surface.id==3);
 float rate=terrain_original::mul(dt,3.f);
 if(lean){
  float a=terrain_original::mul(v[0],v[0]),b=terrain_original::mul(v[1],v[1]),c=terrain_original::mul(v[2],v[2]);float norm=terrain_original::add(a,b);norm=terrain_original::add(norm,c);norm=terrain_original::add(norm,0.f);float speed=terrain_original::sqrt(norm);speed=terrain_original::mul(speed,f(0x3d1374bc));
  const auto& curve=profile.extraLeanCurve;unsigned i=0;float target;
  if(curve[1].x<speed){i=1;if(curve[2].x<speed){i=2;if(curve[3].x<speed)i=3;}}
  if(speed<curve[0].x)target=curve[0].y;
  else if(i==3)target=curve[3].y;
  else{float distance=originalScalarSubtract(speed,curve[i].x);float span=originalScalarSubtract(curve[i+1].x,curve[i].x);float rise=originalScalarSubtract(curve[i+1].y,curve[i].y);rise=terrain_original::mul(rise,distance);rise=originalScalarDivide(rise,span);target=originalScalarAdd(curve[i].y,rise);}
  s.extraLean.target=target;s.extraLean.rate=terrain_original::mul(std::abs(originalScalarSubtract(s.extraLean.current,target)),rate);
 }else{s.extraLean.target=0;s.extraLean.rate=rate;}
 bool align=!lean&&s.animationClass!=10&&s.animationClass!=5&&s.animationIndex!=22&&s.animationIndex!=2;
 s.boardAlignment.target=align?1.f:0.f;s.boardAlignment.rate=rate;
}
}
