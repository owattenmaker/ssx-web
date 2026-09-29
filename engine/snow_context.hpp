#pragma once
#include "snow_emission.hpp"
#include "animation_motion.hpp"
namespace ssx {
// Authored0xB0-byte surface's visual fields. No physics/input heuristics.
struct OriginalSnowSurface {
 int id=0;bool cloudActive=false,trailActive=false,impactActive=false,largeImpactActive=false;
 float cloudMaxHeightCm=0,impactMultiplier=0,rockChance=0,smallChunkChance=0,largeChunkChance=0;
 bool chunksUseWakeVelocity=false;float minWakeVelocityScale=0,maxWakeVelocityScale=0;
};
struct OriginalSnowContextInput {
 OriginalSnowRiderInput rider;
 OriginalSnowBoardFrame board; // geometry+30 unit axes; source Z-up centimeters
 SnowVector groundNormal{},lateral{};
 SnowColour environmentARGB{}; // source4FA398+environmentIndex*F0
 float secondaryBrake274=0;
 bool suppressed=false; // rider88C->A0
 int visibilityMode=0; // rider898; controls2E2550 emitter gates
 std::optional<SnowVector> wakeVelocity; // newest retained owner3B0 wake-row tip, not a fixture
 // Null means the original wake ring has no retained row result. A caller
 // without a native wake cache must report that coverage gap separately.
};
struct OriginalSnowContextState {
 float previousSpeedCmps=0; // FX74
 int visibilityMode=0,impactSurface=0; // FX128,110
 OriginalSnowImpactState impact;
 std::array<bool,10> emitterEnabled{}; // emitterBase+174 +index*210
};
struct OriginalSnowContext {
 OriginalSnowRiderCache rider;
 OriginalSnowTrailContext trail;
 OriginalSnowCloudContext cloud;
 OriginalSnowChunkContext chunks;
 OriginalSnowImpactContext impact;
 bool impactTriggered=false;
};
//11F328/geometry+30 representation: use native generated board quaternion and
//position. Original geometry scale is deliberately absent from this matrix.
OriginalSnowBoardFrame originalSnowUnitBoardFrame(const AnimationTransform& board);
SnowColour originalSnowEnvironmentColour(SnowColour argb); //2DFD6C..DA8
bool originalSnowImpactTrigger(OriginalSnowContextState&,SnowVector position,SnowVector normal,
                              float strength,int surface,bool kind,const OriginalSnowRiderInput&); //2E23E0
void originalSnowVisibility(OriginalSnowContextState&,int mode,bool tracking); //2E2550
OriginalSnowContext originalSnowContextStep(OriginalSnowContextState&,const OriginalSnowContextInput&,
                                          const OriginalSnowSurface&);
}
