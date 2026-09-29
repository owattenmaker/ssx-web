#include "air_alignment.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx { namespace {
using V=std::array<float,3>;using Q=std::array<float,4>;
constexpr float F(unsigned bits){return std::bit_cast<float>(bits);}
float A(float a,float b){return originalScalarAdd(a,b);}
float S(float a,float b){return originalScalarSubtract(a,b);}
using Round=OriginalRounding;
V cross(V a,V b){return {terrain_original::sub(terrain_original::mul(a[1],b[2]),terrain_original::mul(a[2],b[1])),terrain_original::sub(terrain_original::mul(a[2],b[0]),terrain_original::mul(a[0],b[2])),terrain_original::sub(terrain_original::mul(a[0],b[1]),terrain_original::mul(a[1],b[0]))};}
float dot(V a,V b){float x=terrain_original::mul(a[0],b[0]),y=terrain_original::mul(a[1],b[1]),z=terrain_original::mul(a[2],b[2]);return terrain_original::add((terrain_original::add(x,y)),z);}
Q mul(Q a,Q b){V c=cross({a[0],a[1],a[2]},{b[0],b[1],b[2]});Q out;
 for(int i=0;i<3;i++){float sum=terrain_original::add(terrain_original::mul(a[i],b[3]),terrain_original::mul(b[i],a[3]));out[i]=terrain_original::add(sum,c[i]);}
 float w=terrain_original::sub(terrain_original::mul(a[3],b[3]),terrain_original::mul(a[0],b[0]));w=terrain_original::sub(w,terrain_original::mul(a[1],b[1]));out[3]=terrain_original::sub(w,terrain_original::mul(a[2],b[2]));return out;}
Q delta(V axis,float angle){auto sc=originalSinCos(terrain_original::mul(angle,.5f));return {terrain_original::mul(axis[0],sc[0]),terrain_original::mul(axis[1],sc[0]),terrain_original::mul(axis[2],sc[0]),sc[1]};}
// Standalone 0x31BF60 sine has its own polynomial; it cannot be replaced by
// the shared sin/cos pair, which uses a different reduction and cosine path.
float sine(float x){float t=terrain_original::mul(x,F(0x3f22f983));t=x<0?S(t,.5f):A(t,.5f);int quadrant=int(t);
 x=S(x,terrain_original::mul(float(quadrant),F(0x3fc90fdb)));float square=terrain_original::mul(x,x),p;
 if(quadrant&1){p=terrain_original::mul(square,F(0x37d00d03));p=A(p,F(0xbab60b62));p=terrain_original::mul(p,square);p=A(p,F(0x3d2aaaab));p=terrain_original::mul(p,square);p=A(p,-.5f);p=terrain_original::mul(p,square);p=A(p,1.f);}
 else{p=terrain_original::mul(square,F(0x3638ef1f));p=A(p,F(0xb9500d03));p=terrain_original::mul(p,square);p=A(p,F(0x3c088889));p=terrain_original::mul(p,square);p=A(p,F(0xbe2aaaab));p=terrain_original::mul(p,square);p=terrain_original::mul(p,x);p=A(p,x);}
 return quadrant&2?-p:p;}
Q alignUp(Q q,V up,V normal){V axis=cross(up,normal);float length=terrain_original::sqrt(dot(axis,axis));if(length==0)return q;
 float inv=terrain_original::div(1.f,length);for(float& x:axis)x=terrain_original::mul(x,inv);float angle=originalAsin(std::min(length,F(0x3f7fff58)));
 if(dot(up,normal)<0)angle=S(F(0x40490fdb),angle);return mul(delta(axis,angle),q);}
}
OriginalPhysicalOrientation originalAirAlignment(Q q,V normal,V heading,float gain,float maximumRate){
 Round round;
 float a=A(terrain_original::mul(q[0],q[0]),terrain_original::mul(q[1],q[1]));float b=S(terrain_original::mul(q[2],q[1]),terrain_original::mul(q[3],q[0]));float c=A(terrain_original::mul(q[2],q[0]),terrain_original::mul(q[3],q[1]));
 V up={A(c,c),A(b,b),S(1.f,A(a,a))};Q target=alignUp(q,up,normal);
 if(heading!=V{0,0,0}){
  V vector={target[0],target[1],target[2]},c1=cross(vector,heading),c2=cross(vector,c1),local;
  for(int i=0;i<3;i++){float x=terrain_original::sub(heading[i],terrain_original::mul(c1[i],target[3]));x=terrain_original::sub(x,terrain_original::mul(c1[i],target[3]));x=terrain_original::add(x,c2[i]);local[i]=terrain_original::add(x,c2[i]);}
  float angle;
  if(local[1]==0){if(local[0]==0)angle=local[0];else angle=local[0]>=0?-F(0x3fc90fdb):F(0x3fc90fdb);}
  else{angle=originalAtan(originalScalarDivide(local[0],local[1]));if(local[1]<0)angle=local[0]>0?A(angle,F(0x40490fdb)):S(angle,F(0x40490fdb));angle=-angle;}
  target=mul(delta(normal,angle),target);
 }
 Q difference=mul(target,{-q[0],-q[1],-q[2],q[3]});float angle;float factor;
 if(difference[3]>0){float h=S(F(0x3fc90fdb),originalAsin(std::min(difference[3],1.f)));angle=A(h,h);factor=.5f;}
 else{float h=S(F(0x3fc90fdb),originalAsin(-std::max(difference[3],-1.f)));angle=A(h,h);factor=-.5f;}
 V axis={1,0,0};if(angle>=F(0x3727c5ac)){float inverse=terrain_original::div(1.f,sine(terrain_original::mul(angle,factor)));axis={terrain_original::mul(difference[0],inverse),terrain_original::mul(difference[1],inverse),terrain_original::mul(difference[2],inverse)};}
 float step=std::min(maximumRate,terrain_original::mul(angle,gain));step=terrain_original::mul(step,F(0x3c888889));
 return originalRebuildOrientation(mul(delta(axis,step),q));
}
}
namespace ssx {
OriginalPhysicalOrientation originalAirAlignmentStage(Q q,const OriginalAirAlignmentContext& c){
 Round round;
 if((c.trajectoryStatus==1||c.trajectoryStatus==3)&&c.surfaceIndex!=18&&c.surfaceProperty44==0&&c.surfaceFlags!=-1&&(c.surfaceFlags&8)){
  float remaining=S(c.predictedTime,c.elapsedTime);
  float gain=remaining>=F(0x3c888889)?originalScalarDivide(c.timeScale,remaining):terrain_original::mul(c.timeScale,F(0x426fffff));
  float cap=terrain_original::mul(c.timeScale,F(0x3fb2b8c4));V heading={0,0,0};
  if(!(c.controlState==5&&std::abs(c.adjustSpin)>F(0x3db2b8c4))&&(c.normal[2]<F(0x3e99999a)||c.airModeFlag!=0)){
   heading=c.heading;if(dot(heading,c.physicalForward)<0)for(float& x:heading)x=terrain_original::mul(x,-1.f);
  }
  q=originalAirAlignment(q,c.normal,heading,gain,cap).quaternion;
 }
 return originalRebuildOrientation(q);
}
}
