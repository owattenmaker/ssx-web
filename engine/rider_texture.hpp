#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
namespace ssx {
// Rider37A94C selects TEX0.TFX=3 (HIGHLIGHT2), with TCC=1 in the audited
// descriptors. Inputs are byte colors after texture sampling/interpolation.
// The fourth lighting lane adds a rim highlight to RGB; texture alpha survives.
inline std::array<uint8_t,4> originalRiderHighlight2(
    const std::array<uint8_t,4>& texture,const std::array<uint8_t,4>& lighting){
    std::array<uint8_t,4> result{};
    for(unsigned i=0;i<3;++i)result[i]=uint8_t(std::min(255,((int(texture[i])*lighting[i])>>7)+lighting[3]));
    result[3]=texture[3];return result;
}
}
