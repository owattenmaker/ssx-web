#pragma once
#include "local_light_query.hpp"
#include <optional>
namespace ssx {
struct OriginalSpotlightDistance {float radius=0,intensity=0,outerCosine=0;int8_t mode=0;};
struct OriginalSpotlightDistanceResult {float intensity=0,fade=0;};
//38A710..38A7FC after geometric query. Angular attenuation follows separately.
inline std::optional<OriginalSpotlightDistanceResult> originalSpotlightDistance(const OriginalSpotlightDistance& light,const OriginalLocalLightQuery& query){
 terrain_original::Rounding rounding;using terrain_original::mul;
 if(!(light.outerCosine<=query.axisCosine))return {};
 float fade=1;
 if(!(light.radius<5000.f)&&!(query.distance<=3750.f))fade=originalScalarSubtract(1.f,mul(originalScalarSubtract(query.distance,3750.f),.0007999999797903001f));
 float reciprocal=mul(query.inverseDistance,100.f),factor=1;
 if(reciprocal<1){if(light.mode==1)factor=reciprocal;else if(light.mode==2)factor=mul(reciprocal,reciprocal);else if(light.mode==3)factor=mul(mul(reciprocal,reciprocal),reciprocal);}
 return OriginalSpotlightDistanceResult{mul(factor,light.intensity),fade};
}
}
