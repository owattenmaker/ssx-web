#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
namespace ssx {
//36B300..36B3E4: GS ALPHA_1=1, textured sprite, DECAL/RGBA T8H.
inline constexpr uint64_t originalFogAlphaRegister=1;
inline constexpr uint32_t originalFogPrimitive=0x156;
//36B330 TEST_1=0x70000 enables ZTE with strict GREATER;36B440..464
//gives the overlay sprites Z=0xFFFF. Near scene pixels must bypass the pass.
inline constexpr uint64_t originalFogTestRegister=0x70000;
inline constexpr uint32_t originalFogSpriteDepth=0xffff;
inline constexpr bool originalFogDepthPasses(uint32_t sceneDepth){return originalFogSpriteDepth>sceneDepth;}
//The CLUT's alpha is scene transmittance in GS units (128 = full scene).
//This is encoded-byte GS blending; callers must not feed linear-light colors.
inline std::array<uint8_t,3> originalFogCompositeRgb(std::array<uint8_t,3> scene,uint32_t palette){
 const int alpha=palette>>24;std::array<uint8_t,3> result;
 for(unsigned k=0;k<3;++k){int fog=(palette>>(k*8))&255;result[k]=uint8_t(std::clamp((((int(scene[k])-fog)*alpha)>>7)+fog,0,255));}
 return result;
}
}
namespace ssx {
//36AF10..36B048 depth preparation. This descriptor does not model the GS
//memory reinterpretation or tiled UV mapping; those require separate recovery.
struct OriginalFogDepthPass {
 static constexpr uint32_t frameFormat=2; //PSMCT16
 static constexpr uint32_t frameMask=0x3fff;
 static constexpr uint32_t textureFormat=50; //PSMZ16
 static constexpr uint32_t primitive=0x116; //textured FST sprite, no alpha blend
 static constexpr uint64_t textureAlpha=0x8000000000ull; //TEXA TA0=0, TA1=128
};
}
namespace ssx {
//Result of the recovered strip layout plus GS Z16/C16/T8H address mapping.
//Per-pixel index wraps to8 bits; do not reuse the clamped near/far-bin function.
inline constexpr uint8_t originalFogPaletteIndex(uint32_t depth){return uint8_t(depth>>8);}
}
