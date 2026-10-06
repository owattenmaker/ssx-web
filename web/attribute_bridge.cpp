#include "rider_local.hpp"
// Career attributes -> the human rider's original stat fields (see web/rider_attributes.hpp).
#include "rider_attributes.hpp"
#include "../engine/ground_motion.hpp"
#include "../engine/landing_motion.hpp"
#include <emscripten/emscripten.h>
using namespace ssx;
RIDER_LOCAL extern OriginalGroundProfile physicsProfile;RIDER_LOCAL extern std::array<OriginalGroundProfile,19> physicsMaterials;RIDER_LOCAL extern OriginalLandingProfile landingProfile;
static void applyGround(OriginalGroundProfile& p){
    p.topSpeedStat=browserAttributeStat(0);   // 0x1494C0 (0x11B3F8 recomputes the speed limit every frame from it)
    p.speedStat=browserAttributeStat(1);      // 0x1493D8
    p.edgeStat=browserAttributeStat(3);       // 0x148D80 / 0x148E68
}
// The selected human's body scale (geometry+0x140; ground profile bodyScale of its live actor): Zoe's is the compiled
// seed; another rider sets it through settings.original_event_start (web/animation_bridge.cpp). 0 = the seed's.
RIDER_LOCAL float browserHumanBodyScale=0;
// The compiled seed's scales, kept while a human scale replaces them: a later Zoe (0) in the same rider context (the page
// reuses the human core across rider picks) gets them back. Computer-rider cores never set a human scale (npc_seed_rider
// writes theirs), so this leaves them alone.
RIDER_LOCAL static bool humanScaleApplied=false;RIDER_LOCAL static float seedLandingScale=0;RIDER_LOCAL static std::array<float,19> seedMaterialScale{};
// Called after every place that re-seeds the ground profile (reset, event start); attributes: no-op until set.
void browser_apply_ground_attributes(){
    if(browserHumanBodyScale>0){
        if(!humanScaleApplied){humanScaleApplied=true;seedLandingScale=landingProfile.bodyScale;for(size_t k=0;k<physicsMaterials.size();++k)seedMaterialScale[k]=physicsMaterials[k].bodyScale;}
        physicsProfile.bodyScale=browserHumanBodyScale;for(auto& m:physicsMaterials)m.bodyScale=browserHumanBodyScale;
        // The ground entry 13C7A8 scales the material depths into the motion owner +4/+8 by the rider's own scale
        // (geometry+0x140), as do the crash sliding contact and detached body: Griff lands on R&B at 0.7 x 29.57 = 20.70 cm,
        // not Zoe's 0.85 (held-out capture ass1-griff-87e9ff58, docs/slopestyle-bigair.md).
        landingProfile.bodyScale=browserHumanBodyScale;
    }else if(humanScaleApplied){humanScaleApplied=false;landingProfile.bodyScale=seedLandingScale;for(size_t k=0;k<physicsMaterials.size();++k)physicsMaterials[k].bodyScale=seedMaterialScale[k];}
    if(!browserAttributesSet){
        // The seeds' stats are mode-1 quotients: recompute them (a no-op in mode 1; matcher batch 3, docs/ps2-float.md).
        browser_reseed_ground_stats();
        return;
    }
    applyGround(physicsProfile);for(auto& m:physicsMaterials)applyGround(m);
    landingProfile.landingStat=browserAttributeStat(6); // 0x149120 landing; 0x149208 rail balance reads the same field
}
// The ground and landing stats of the current profiles, recomputed by div.s in the current arithmetic (browserStatFromSeed).
RIDER_LOCAL extern float browserSettingsLandingStat; // web/animation_bridge.cpp (the rider's settings)
void browser_reseed_ground_stats(){
    auto reseed=[](OriginalGroundProfile& p){
        p.topSpeedStat=browserStatFromSeed(p.topSpeedStat);
        p.speedStat=browserStatFromSeed(p.speedStat);
        p.edgeStat=browserStatFromSeed(p.edgeStat);
    };
    reseed(physicsProfile);
    for(auto& m:physicsMaterials)reseed(m);
    // 149120: the rider's own landing stat (its settings) over the compiled seed's 1/11
    landingProfile.landingStat=browserStatFromSeed(browserSettingsLandingStat>0?browserSettingsLandingStat:landingProfile.landingStat);
}
extern "C" {
// raw: the seven attribute progress bytes in original order (speed, accel, tricks, edging, spin, toughness,
// stability; 5..55 = level 1.0..11.0), override: +0xB34 level override (0 = none).
EMSCRIPTEN_KEEPALIVE void set_rider_attributes(const int32_t* raw,int32_t override){
    for(int k=0;k<7;++k)browserAttributeRaw[k]=raw[k];browserAttributeOverride=override;browserAttributesSet=true;
    browser_apply_ground_attributes();browser_apply_animation_attributes();
}
EMSCRIPTEN_KEEPALIVE float* rider_attribute_stats(){
    RIDER_LOCAL static float out[12];for(int k=0;k<7;++k)out[k]=browserAttributeStat(k);
    out[7]=physicsProfile.topSpeedStat;out[8]=physicsProfile.speedStat;out[9]=physicsProfile.edgeStat;out[10]=landingProfile.landingStat;out[11]=browserAttributesSet;
    return out;
}
}
#ifdef SSX_SNAPSHOT_REGISTRY // the rider-context snapshot's registry (web/generate-snapshot-registry.mjs, docs/replay.md §2a)
#include "generated/snapshot/attribute_bridge.inc"
#endif
