#pragma once
#include "boost_ribbon.hpp"
#include <vector>
namespace ssx {
// Original48-byte submitted vertex. Pad is not written by2E7A10.
struct OriginalBoostDrawVertex {float u=0,v=0,q=1,pad=0;std::array<uint32_t,4> colour{255,255,255,0};BoostHistoryQuad position{};};
struct OriginalBoostDraw {std::vector<OriginalBoostDrawVertex> left,right,main;};
inline bool originalBoostDrawEnabled(const OriginalBoostEffectState&s,bool enabled){return s.trailCount!=0&&s.trailLength>=3.f&&s.alpha>=.05000000074505806f&&enabled;}
//2E7C3C..2E81B8. Material push/pop and backend submission are caller-owned.
inline OriginalBoostDraw originalBoostDrawVertices(const OriginalBoostEffectState&s,
 const OriginalBoostSideHistory&side,const OriginalBoostRibbonHistory&main,
 BoostHistoryQuad head,BoostHistoryQuad physicalRight){
 OriginalRounding rounding;using namespace terrain_original;OriginalBoostDraw out;
 if(s.trailCount<0||s.trailCount>20||s.primaryCount<0||s.primaryCount>30||side.cursor<0||side.cursor>=20||main.cursor<0||main.cursor>=30)throw std::runtime_error("Invalid boost draw history");
 auto scale=[](BoostHistoryQuad v,float k){for(auto&x:v)x=terrain_original::mul(x,k);return v;};
 auto plus=[](BoostHistoryQuad a,BoostHistoryQuad b){for(unsigned k=0;k<4;k++)a[k]=terrain_original::add(a[k],b[k]);return a;};
 auto minus=[](BoostHistoryQuad a,BoostHistoryQuad b){for(unsigned k=0;k<4;k++)a[k]=terrain_original::sub(a[k],b[k]);return a;};
 auto vertex=[](BoostHistoryQuad p,float u,float v,float alpha){OriginalBoostDrawVertex out;out.position=p;out.u=u;out.v=v;out.colour[3]=uint32_t(terrain_original::mul(alpha,128.f));return out;};
 if(s.trailCount>=2){
  const auto origin=scale(head,.699999988079071f),half=scale(physicalRight,mul(s.width,.5f));
  const float inverseSquare=originalScalarDivide(1.f,float((s.trailCount-1)*(s.trailCount-1))),step=originalScalarDivide(1.f,float(s.trailCount));float ordinal=0,v=side.scroll;
  for(int j=0;j<s.trailCount;j++){
   float a=mul(s.alpha,originalScalarSubtract(1.f,mul(mul(ordinal,ordinal),inverseSquare)));if(a<0)a=0;
   const auto& row=side.samples[(side.cursor-j+20)%20];const auto left=plus(row.left,origin),right=plus(row.right,origin);
   out.left.push_back(vertex(minus(left,half),0,v,a));out.left.push_back(vertex(plus(left,half),1,v,a));
   out.right.push_back(vertex(minus(right,half),0,v,a));out.right.push_back(vertex(plus(right,half),1,v,a));
   ordinal=originalScalarAdd(ordinal,1.f);v=originalScalarAdd(v,step);
  }
 }
 if(s.primaryCount>=2){
  const float limit=std::min(s.trailLength,s.primaryDistance);float distance=0;
  for(int j=0;j<s.primaryCount;j++){
   // Source reads distance rows linearly, but position rows through the ring.
   distance=originalScalarAdd(distance,main.rows[j].distance);float fraction=originalScalarDivide(distance,limit);
   const float v=originalScalarAdd(side.scroll,fraction),raw=mul(s.alpha,originalScalarSubtract(1.f,fraction));float a=0<=raw?std::min(raw,1.f):0.f;a=0<=a?std::min(a,1.f):0.f;
   const auto& row=main.rows[(main.cursor+j)%30];out.main.push_back(vertex({row.a[0],row.a[1],row.a[2],1},0,v,a));out.main.push_back(vertex({row.b[0],row.b[1],row.b[2],1},1,v,a));
  }
 }
 return out;
}
}
