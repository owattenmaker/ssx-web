#pragma once
#include "original_float.hpp"
#include <algorithm>
namespace ssx {
//12F230 configures white in/out effects, durations(.5,0,.5), progress binding.
//2E4760 maps that binding to elapsed time;2E48AC..2E492C selects/clamps opacity.
inline float originalResetFadeAlpha(float progress){
 struct Round{int old=std::fegetround();Round(){std::fesetround(FE_TOWARDZERO);}~Round(){std::fesetround(old);}} round;
 const float alpha=progress<.5f?originalScalarDivide(progress,.5f):originalScalarSubtract(1.f,originalScalarDivide(originalScalarSubtract(progress,.5f),.5f));
 return alpha>=0?std::min(alpha,1.f):0.f;
}
}
