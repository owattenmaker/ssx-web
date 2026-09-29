#pragma once
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <array>
namespace ssx {
//tWPIGD_Fog: paired current/last-sample values at+8..+34.
//Order: density, near cm, far cm, red, green, blue. Rate at+0 is separate.
struct OriginalFogPainterState {std::array<float,6> current{},sample{};};
//2BCF38..2BD064, vtable210. The actual Fog class's218 hook is empty.
//The source squares the weight without clamping and leaves rate unchanged.
inline void originalFogPainterBlend(OriginalFogPainterState& state,const std::array<float,6>& values,float weight){
 terrain_original::Rounding rounding;
 const float squared=terrain_original::mul(weight,weight),complement=originalScalarSubtract(1.f,squared);
 for(unsigned i=0;i<6;++i){state.current[i]=originalScalarAdd(terrain_original::mul(squared,values[i]),terrain_original::mul(complement,state.current[i]));state.sample[i]=values[i];}
}
}
namespace ssx {
//2BDB38: equality of all six current values, not the stored sample slots.
inline bool originalFogPainterMatches(const OriginalFogPainterState& state,const std::array<float,6>& values){return state.current==values;}
//2BE108: clears density/distance, retains last-sample slots. Globals supply near/far/RGB.
inline void originalFogPainterDefaults(OriginalFogPainterState& state,float& distance,const std::array<float,5>& defaults){
 for(unsigned i=0;i<5;++i)state.current[i+1]=defaults[i];state.current[0]=0;distance=0;
}
}
