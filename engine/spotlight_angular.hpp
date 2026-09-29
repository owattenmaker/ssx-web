#pragma once
#include "terrain_contact_math.hpp"
namespace ssx {
//Source specializes exponents0..4; higher/negative signed modes use squaring.
inline float originalSpotlightPower(float base,int8_t mode){
 using terrain_original::mul;
 switch(mode){case 0:return 1;case 1:return base;case 2:return mul(base,base);case 3:return mul(mul(base,base),base);case 4:return mul(mul(mul(base,base),base),base);}
 if(base==0)return 0;
 int exponent=mode;if(exponent<0){base=originalScalarDivide(1.f,base);exponent=-exponent;}
 float result=1;while(exponent){if(exponent&1)result=mul(result,base);exponent>>=1;base=mul(base,base);}return result;
}
//38A7FC..38AA74. Caller has already accepted the outer cone and formed the
//radial intensity. Between cones the powered inner value gets a linear ramp.
inline float originalSpotlightAngular(float radial,float cosine,float outer,float inner,int8_t mode){
 terrain_original::Rounding rounding;using terrain_original::mul;float value;
 if(inner<=cosine)value=mul(radial,originalSpotlightPower(cosine,mode));
 else {value=mul(radial,originalSpotlightPower(inner,mode));float ramp=originalScalarDivide(originalScalarSubtract(cosine,outer),originalScalarSubtract(inner,outer));value=mul(value,ramp);}
 return 0.f<=value?std::min(value,5.f):0.f;
}
}
