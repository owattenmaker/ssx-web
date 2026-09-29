#pragma once
// Original PS2 SSX3 (SLUS_207.72) BodySnow producer (emitter 9, 2E2260) and the
// crash-path snow impact trigger wrapper (111AA0 -> 2E23E0). Header-only; see
// engine/SNOW_CRASH_RECOVERY.md for addresses, evidence and integration order.
#include "snow_emission.hpp"
#include "terrain_contact_math.hpp"
#include "original_float.hpp"
#include <algorithm>
#include <array>
#include <cfenv>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
namespace ssx {
struct OriginalSnowContextState; // snow_context.hpp; the trigger wrapper links against snow_context.cpp
bool originalSnowImpactTrigger(OriginalSnowContextState&,SnowVector position,SnowVector normal,float strength,int surface,bool kind,const OriginalSnowRiderInput&); //2E23E0
// 2DF4D0 binds these thirty skeleton bone names (strings at 4879D8.., 4A3B00,
// 4A3B08) through 310C48 into FX+7C, in this exact order. Fifteen names repeat.
inline constexpr std::array<std::string_view,30> originalSnowBodyBoneNames{
    "shinleft","footleft","shinright","footright","thighleft","thighright","hips","lowerspine","middlespine","upperspine",
    "neck","clavicleleft","handright","bicepleft","biceptwistleft","shinleft","footleft","shinright","footright","thighleft",
    "thighright","hips","lowerspine","middlespine","forearmleft","handleft","clavicleright","bicepright","biceptwistright","forearmright"};
using OriginalSnowBodyBoneTable=std::array<int,30>; // FX+7C contents (skeleton bone indices)
// 2DF4D0: each name resolves through the rig's bone table; 310C48 returns -1 for
// an unknown name and the original would then index an invalid world matrix.
inline OriginalSnowBodyBoneTable originalSnowBodyBoneTable(const std::function<int(std::string_view)>& boneIndexByName){
    OriginalSnowBodyBoneTable table{};
    for(unsigned i=0;i<30;++i){int index=boneIndexByName(originalSnowBodyBoneNames[i]);
        if(index<0)throw std::runtime_error("Original BodySnow bone is missing from the rig: "+std::string(originalSnowBodyBoneNames[i]));table[i]=index;}
    return table;
}
struct OriginalSnowBodyState {int cursor=0;}; // FX+78; 2DF3B0 resets it to 0
struct OriginalSnowBodyContext {
    std::array<SnowVector,30> boneOriginsCm{}; // geometry+30 world matrix row 3 for each FX+7C entry, source cm Z-up
    SnowVector primaryBoneOriginCm{};          // rider+89C bone world origin (inactive request position)
    SnowVector velocityCmps{};                 // rider+1E0
    float speedCmps=0;                         // FX+B8 (== OriginalSnowRiderCache::speedCmps)
    SnowColour colour{1,1,1,1};                // FX+90 (2R,2G,2B,1) from originalSnowEnvironmentColour
    int motionMode=0;                          // FX+D4 (owner+DE0 via 11FE98)
};
// 2E2260. `impact.buildup` is the shared FX+4 word that 2E1598 raises; this
// routine decays it. Draws nothing from the shared visual LCG. Profile is
// emitter 9's authored block (VelScale 0.8 = FX+2C+900); normal scales unused.
inline OriginalSnowEmission originalSnowBodyEmission(const OriginalSnowTrailProfile& profile,OriginalSnowBodyState& state,
        OriginalSnowImpactState& impact,const OriginalSnowBodyContext& c){
    OriginalRounding rounding;
    OriginalSnowEmission out;out.emitter=9;out.stepSeconds=.01666666753590107f; // gp-3A48 / gp-3A44, both 3C888889
    if(!(0.f<impact.buildup)||!(83.33333587646484f<c.speedCmps)){out.positionCm=c.primaryBoneOriginCm;return out;} // gp-3A4C = 42A6AAAB
    float scaled=terrain_original::mul(impact.buildup,1.5f);float alpha=0.f<=scaled?std::min(scaled,1.f):0.f;
    if(state.cursor<0||state.cursor>=30)throw std::runtime_error("Original BodySnow cursor is outside the thirty bound bones");
    out.active=true;out.positionCm=c.boneOriginsCm[unsigned(state.cursor)];
    SnowVector velocity=c.velocityCmps;for(auto& v:velocity)v=terrain_original::mul(v,profile.velocityScale);out.velocityCmps=velocity;
    out.colour=SnowColour{c.colour[0],c.colour[1],c.colour[2],alpha};
    state.cursor=state.cursor+1<30?state.cursor+1:0;
    if(c.motionMode!=2)impact.buildup=std::max(originalScalarSubtract(impact.buildup,.01666666753590107f),0.f);
    return out;
}
// 111AA0: every crash/landing/scenery impact report is 2E23E0 with kind=false on
// the owner's FX. Strength sign is irrelevant (2E23E0 takes the absolute value).
// Callers: 10EB30 (crash entry: event point/normal, closing speed, surface),
// 137860 (airborne crash landing: probe point/normal, normal speed, surface),
// 137D18 (sliding contact penetration: hit point/normal, |surface velocity| or
// |velocity| before correction, hit surface), 10E910 (ordinary landing award)
// and 1242B0 (cruise scenery probe). Returns whether the FX retained it.
inline bool originalSnowCrashImpactTrigger(OriginalSnowContextState& state,SnowVector pointCm,SnowVector normal,float strengthCmps,
        int surface,const OriginalSnowRiderInput& rider){
    return originalSnowImpactTrigger(state,pointCm,normal,strengthCmps,surface,false,rider);
}
}
