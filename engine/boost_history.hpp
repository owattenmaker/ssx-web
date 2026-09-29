#pragma once
#include "boost_effect.hpp"
namespace ssx {
using BoostHistoryQuad=std::array<float,4>;
struct OriginalBoostHistoryPair {BoostHistoryQuad left{},right{};};
//2E6698..2E66A8: human/device-bound riders retain20 pairs, others18.
inline int originalBoostHistoryCapacity(int deviceIndex){return deviceIndex<0?18:20;}
struct OriginalBoostSideHistory {
 std::array<OriginalBoostHistoryPair,20> samples{};
 int cursor=0,capacity=20;float scroll=0;
};
//2E68A8..2E6BEC. Inputs are the original physical right column and three
//bone matrix translations. Their rendering-space interpretation is caller-owned.
inline void originalBoostSideHistory(OriginalBoostEffectState&state,OriginalBoostSideHistory&h,
 const OriginalBoostEffectParameters&p,BoostHistoryQuad right,BoostHistoryQuad head,
 BoostHistoryQuad boneA,BoostHistoryQuad boneB,bool enabled){
 OriginalRounding rounding;using namespace terrain_original;
 if(!enabled)return;
 auto scale=[](BoostHistoryQuad v,float amount){for(auto&x:v)x=terrain_original::mul(x,amount);return v;};
 auto subtract=[](BoostHistoryQuad a,BoostHistoryQuad b){for(unsigned k=0;k<4;k++)a[k]=terrain_original::sub(a[k],b[k]);return a;};
 auto dot4=[](BoostHistoryQuad a,BoostHistoryQuad b){float v=terrain_original::add(terrain_original::mul(a[0],b[0]),terrain_original::mul(a[1],b[1]));v=terrain_original::add(v,terrain_original::mul(a[2],b[2]));return terrain_original::add(v,terrain_original::mul(a[3],b[3]));};
 if(h.cursor<0||h.cursor>=20)throw std::runtime_error("Invalid boost history cursor");
 if(p.emitting){
  const int previous=h.cursor;h.cursor=(previous+1)%20;if(state.trailCount<h.capacity)++state.trailCount;
  h.samples[h.cursor]=h.samples[previous];
  const auto spread=scale(right,1.399999976158142f);
  for(auto&pair:h.samples){pair.left=subtract(pair.left,spread);for(unsigned k=0;k<4;k++)pair.right[k]=add(pair.right[k],spread[k]);}
 }else {const int count=state.trailCount;state.trailCount=count>1?count-1:count>-2?0:count+1;}
 if(state.trailCount<=0)return;
 const auto offset=scale(head,.699999988079071f);
 if(dot4(boneA,right)<dot4(boneB,right))h.samples[h.cursor]={subtract(boneA,offset),subtract(boneB,offset)};
 else h.samples[h.cursor]={subtract(boneB,offset),subtract(boneA,offset)};
 float step=mul(mul(p.speed,.01666666753590107f),p.scrollStep);
 h.scroll=originalScalarSubtract(h.scroll,step);
 if(h.scroll<0)h.scroll=originalScalarAdd(h.scroll,1.f);
 if(h.scroll>1)h.scroll=originalScalarSubtract(h.scroll,1.f);
}
}
