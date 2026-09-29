#pragma once
#include "terrain_contact_math.hpp"
namespace ssx {
using OriginalLightingReference=std::array<uint32_t,2>;
struct OriginalLightingPainterState {
 std::array<OriginalLightingReference,4> references{},previousReferences{};
 std::array<float,2> values{},samples{};
};
//2BD5A8..2BD698. Lighting's218 hook resolves to the empty2BDA58.
//Eight-byte references copy directly; previous-reference slots and rate persist.
inline void originalLightingPainterBlend(OriginalLightingPainterState& state,
 const std::array<OriginalLightingReference,4>& references,const std::array<float,2>& values,float weight){
 terrain_original::Rounding rounding;const float squared=terrain_original::mul(weight,weight),complement=originalScalarSubtract(1.f,squared);
 state.references=references;
 for(unsigned i=0;i<2;++i){state.values[i]=originalScalarAdd(terrain_original::mul(squared,values[i]),terrain_original::mul(complement,state.values[i]));state.samples[i]=values[i];}
}
}
