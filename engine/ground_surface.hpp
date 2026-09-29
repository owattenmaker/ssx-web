#pragma once
#include "ground_motion.hpp"

namespace ssx {
// Material fields from one race surface record; character statistics, shared
// control curves and the frame's cached speed limit are not material data.
struct OriginalGroundMaterial {
    GroundSurface surface;
    float depthTarget1=0,depthTarget3=0,maxTurnAngle=0,airHeight=0;
    float autoBoostSpeed=0,autoBoostFactor=0,alignmentRate=0,terminalVelocity=0;
    std::array<float,4> heading{};
};
inline void applyGroundMaterial(OriginalGroundProfile& profile,const OriginalGroundMaterial& material) {
    profile.surface=material.surface;
    profile.depthTarget1=material.depthTarget1;profile.depthTarget3=material.depthTarget3;
    profile.maxTurnAngle=material.maxTurnAngle;profile.airHeight=material.airHeight;
    profile.autoBoostSpeed=material.autoBoostSpeed;profile.autoBoostFactor=material.autoBoostFactor;
    profile.alignmentRate=material.alignmentRate;profile.surfaceTerminalVelocity=material.terminalVelocity;
    profile.headingProfile.surface28=material.heading[0];profile.headingProfile.surface2C=material.heading[1];
    profile.headingProfile.surface30=material.heading[2];profile.headingProfile.surface34=material.heading[3];
}
// Original 13D64C..13D65C and no-contact 13D420.
inline int originalGroundSurfaceId(bool hit,int authoredSurface) {return !hit||authoredSurface==-1?0:authoredSurface;}
}
