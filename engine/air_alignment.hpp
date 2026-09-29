#pragma once
#include "orientation_motion.hpp"
namespace ssx {
// Original physical orientation approach 0x121AA0. Source Z-up vectors, XYZW
// quaternion. A zero heading vector disables heading alignment. Returns the
// single normalization performed inside that function; its caller may rebuild
// once more. gain and maximumRate are the original f12/f13 caller arguments.
OriginalPhysicalOrientation originalAirAlignment(std::array<float,4> quaternion,
    std::array<float,3> normal,std::array<float,3> heading,float gain,float maximumRate);
}
namespace ssx {
struct OriginalAirAlignmentContext {
    std::array<float,3> normal{0,0,1},heading{},physicalForward{0,1,0};
    float predictedTime=0,elapsedTime=0,timeScale=1,adjustSpin=0;
    int trajectoryStatus=0,surfaceIndex=-1,surfaceFlags=0,surfaceProperty44=0;
    int controlState=5,airModeFlag=0;
};
// Exact orientation tail 0x139A64..0x139C68. Context must reflect the trajectory
// predictor's post-update result for this tick, not a frozen checkpoint.
OriginalPhysicalOrientation originalAirAlignmentStage(std::array<float,4> quaternion,
    const OriginalAirAlignmentContext& context);
}
