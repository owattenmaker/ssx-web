#pragma once
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <array>
namespace ssx {
// Environment property object with vtable484058:19 current/target float pairs
// at+08..+9C. The scalar consumed by breath2EE6F0 is pair5 (current+30).
struct OriginalEnvironmentProperties {
    std::array<float,19> current{},target{};
};
//2BD698, vtable slot210. The default class's before/after hook2BDA90 is a no-op.
// Weight comes from the caller; source squares it and does not clamp it.
inline void originalEnvironmentPropertiesBlend(OriginalEnvironmentProperties& s,
        const std::array<float,19>& incoming,float weight){
    OriginalRounding rounding;
    float fraction=terrain_original::mul(weight,weight);
    float remaining=originalScalarSubtract(1.f,fraction);
    for(unsigned i=0;i<19;i++){
        s.current[i]=originalScalarAdd(terrain_original::mul(fraction,incoming[i]),terrain_original::mul(remaining,s.current[i]));
        s.target[i]=incoming[i];
    }
}
}
