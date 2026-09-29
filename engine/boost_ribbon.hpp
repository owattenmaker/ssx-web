#pragma once
#include "boost_history.hpp"
namespace ssx {
struct OriginalBoostRibbonRow {float distance=0;std::array<float,3> a{},b{};};
struct OriginalBoostRibbonHistory {
 std::array<OriginalBoostRibbonRow,30> rows{};int cursor=0;
 BoostHistoryQuad previousPosition{},previousSide{1,0,0,1};
};
struct OriginalBoostRibbonInput {
 BoostHistoryQuad velocity{},position{},boardPosition{},boardX{1,0,0,0},boardZ{0,0,1,0};
 BoostHistoryQuad groundNormal{0,0,1,0},contactPoint{};
 bool switchStance=false,flag330=false;int semantic=438;
};
struct OriginalBoostRibbonSeed {bool advanced=false;std::array<BoostHistoryQuad,4> corners{};};
//2E6C08..2E71B4: history advance and board-relative seed rectangle. Subsequent
//velocity/ground-plane clipping writes the selected row's endpoints separately.
inline OriginalBoostRibbonSeed originalBoostRibbonSeed(OriginalBoostEffectState&s,
 OriginalBoostRibbonHistory&h,const OriginalBoostRibbonInput&i){
 OriginalRounding rounding;using namespace terrain_original;OriginalBoostRibbonSeed out;
 auto scale=[](BoostHistoryQuad v,float k){for(auto&x:v)x=terrain_original::mul(x,k);return v;};
 auto plus=[](BoostHistoryQuad a,BoostHistoryQuad b){for(unsigned k=0;k<4;k++)a[k]=terrain_original::add(a[k],b[k]);return a;};
 auto minus=[](BoostHistoryQuad a,BoostHistoryQuad b){for(unsigned k=0;k<4;k++)a[k]=terrain_original::sub(a[k],b[k]);return a;};
 auto length=[](BoostHistoryQuad v){float d=terrain_original::add(terrain_original::mul(v[0],v[0]),terrain_original::mul(v[1],v[1]));d=terrain_original::add(d,terrain_original::mul(v[2],v[2]));d=terrain_original::add(d,terrain_original::mul(v[3],v[3]));return terrain_original::sqrt(d);};
 if(!(s.trailLength>0||s.primaryDistance>0)||!(length(i.velocity)>0))return out;
 if(h.cursor<0||h.cursor>=30||s.primaryCount<0||s.primaryCount>30)throw std::runtime_error("Invalid boost ribbon history");
 const int next=(h.cursor+29)%30;
 if(s.primaryCount==30)s.primaryDistance=originalScalarSubtract(s.primaryDistance,h.rows[next].distance);else ++s.primaryCount;
 h.cursor=next;float traveled=s.primaryCount>=2?length(minus(i.position,h.previousPosition)):0;
 h.previousPosition=i.position;h.rows[h.cursor].distance=0;h.rows[(h.cursor+1)%30].distance=traveled;
 s.primaryDistance=originalScalarAdd(s.primaryDistance,traveled);
 const auto x=i.switchStance?scale(i.boardX,-1):i.boardX;
 const auto longitudinal=scale(x,75),vertical=scale(i.boardZ,17.5f);
 out.corners={minus(plus(i.boardPosition,longitudinal),vertical),plus(plus(i.boardPosition,longitudinal),vertical),minus(minus(i.boardPosition,longitudinal),vertical),plus(minus(i.boardPosition,longitudinal),vertical)};
 if(i.flag330){const bool front=(i.semantic>=23&&i.semantic<=28)||i.semantic==37||i.semantic==38;const auto offset=scale(scale(x,150),front?1.f:-1.f);
  if(front){out.corners[2]=plus(out.corners[2],offset);out.corners[3]=plus(out.corners[3],offset);}
  else {out.corners[0]=plus(out.corners[0],offset);out.corners[1]=plus(out.corners[1],offset);}
 }
 out.advanced=true;return out;
}
}

namespace ssx {
//2E794C..2E79F4: discard oldest distance intervals until reaching the requested
//length, retaining at least3 rows while trailLength>1cm, otherwise allowing0.
inline void originalBoostRibbonTrim(OriginalBoostEffectState&s,const OriginalBoostRibbonHistory&h){
 OriginalRounding rounding;
 if(h.cursor<0||h.cursor>=30||s.primaryCount<0||s.primaryCount>30)throw std::runtime_error("Invalid boost ribbon trim history");
 const int minimum=s.trailLength>1.f?3:0;
 while(s.primaryCount>minimum&&s.primaryDistance>s.trailLength){
  const int oldest=(h.cursor+s.primaryCount-1)%30;
  --s.primaryCount;s.primaryDistance=originalScalarSubtract(s.primaryDistance,h.rows[oldest].distance);
 }
}
}

namespace ssx {
//2E71BC..2E7948: orient the side plane continuously, select the two source
//corners (including the asymmetric0.5 preference), then project10cm off snow.
inline void originalBoostRibbonEndpoints(OriginalBoostRibbonHistory&h,const OriginalBoostRibbonInput&i,
 const OriginalBoostRibbonSeed&seed){
 if(!seed.advanced)return;OriginalRounding rounding;using namespace terrain_original;
 auto dot4=[](BoostHistoryQuad a,BoostHistoryQuad b){float x=terrain_original::add(terrain_original::mul(a[0],b[0]),terrain_original::mul(a[1],b[1]));x=terrain_original::add(x,terrain_original::mul(a[2],b[2]));return terrain_original::add(x,terrain_original::mul(a[3],b[3]));};
 auto scale=[](BoostHistoryQuad a,float k){for(auto&x:a)x=terrain_original::mul(x,k);return a;};
 auto minus=[](BoostHistoryQuad a,BoostHistoryQuad b){for(unsigned k=0;k<4;k++)a[k]=terrain_original::sub(a[k],b[k]);return a;};
 auto v=cross(Vector{i.velocity[0],i.velocity[1],i.velocity[2]},Vector{i.groundNormal[0],i.groundNormal[1],i.groundNormal[2]});
 BoostHistoryQuad side{v[0],v[1],v[2],0};if(dot4(side,h.previousSide)<0)side=scale(side,-1);h.previousSide=side;
 std::array<BoostHistoryQuad,4> relative;std::array<float,4>d;
 for(unsigned k=0;k<4;k++){relative[k]=minus(seed.corners[k],i.contactPoint);d[k]=dot4(relative[k],side);}
 const float extent=std::max({std::abs(d[0]),std::abs(d[1]),std::abs(d[2]),std::abs(d[3])});
 const float inverse=originalScalarDivide(1.f,extent);for(auto&x:d)x=mul(x,inverse);
 unsigned lo,hi;
 if(d[0]<d[1]&&d[0]<originalScalarAdd(d[2],.5f)&&d[0]<originalScalarAdd(d[3],.5f))lo=0;
 else if(d[1]<d[0]&&d[1]<originalScalarAdd(d[2],.5f)&&d[1]<originalScalarAdd(d[3],.5f))lo=1;
 else if(d[2]<d[0]&&d[2]<d[1]&&d[2]<d[3])lo=2;else lo=3;
 if(d[1]<=d[0]&&originalScalarSubtract(d[2],.5f)<=d[0]&&originalScalarSubtract(d[3],.5f)<=d[0])hi=0;
 else if(d[0]<=d[1]&&originalScalarSubtract(d[2],.5f)<=d[1]&&originalScalarSubtract(d[3],.5f)<=d[1])hi=1;
 else if(d[0]<=d[2]&&d[1]<=d[2]&&d[3]<=d[2])hi=2;else hi=3;
 auto project=[&](unsigned k){const float depth=originalScalarSubtract(dot4(i.groundNormal,relative[k]),10.f);auto p=minus(seed.corners[k],scale(i.groundNormal,depth));return Vector{p[0],p[1],p[2]};};
 h.rows[h.cursor].a=project(lo);h.rows[h.cursor].b=project(hi);
}
}

namespace ssx {
// Complete2E6C08 update; drawing is the separate2E7A10 routine.
inline bool originalBoostRibbonStep(OriginalBoostEffectState&s,OriginalBoostRibbonHistory&h,
 const OriginalBoostRibbonInput&i){
 const auto seed=originalBoostRibbonSeed(s,h,i);originalBoostRibbonEndpoints(h,i,seed);
 originalBoostRibbonTrim(s,h);return seed.advanced;
}
}
