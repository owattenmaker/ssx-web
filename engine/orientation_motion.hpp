#pragma once
#include <array>
#include "ground_motion.hpp"
namespace ssx {
// Original 0x11DFE0 quaternion arithmetic. Source XYZ/W and source Z-up axis;
// pre-multiplies a world-axis delta. Deliberately does not normalize the result.
std::array<float,4> originalRotateOrientation(std::array<float,4> quaternion,
                                             std::array<float,3> axis,float radians);
}
namespace ssx {
float originalAsin(float value);
// Original cruise surface-alignment stage 0x13ED68..0x13EE9C. clearance is
// contact-result+0x88 (the caller's stack+0x148), in source centimeters.
std::array<float,4> originalGroundAlignment(std::array<float,4> quaternion,
    std::array<float,3> normal,std::array<float,3> boardUp,
    float clearance,float surfaceAlignmentRate,bool* rotated=nullptr);
}
namespace ssx {
struct OriginalHeadingState {
    std::array<float,3> relativeVelocity{},normal{0,0,1},forward{0,1,0},lateral{1,0,0},bodyForward{0,1,0};
    float turn=0,charge=0,dt=1.f/60,manualSpin=0;
    int controlState=0;
    bool reverseStance=false;
};
// Original 0x13E22C..0x13EB94: radians about the contact normal. Updates2DC.
float originalGroundHeading(const OriginalHeadingProfile&,OriginalHeadingState&);
}
namespace ssx {
struct OriginalPhysicalOrientation {
    std::array<float,4> quaternion;
    std::array<float,3> right,forward,up;
};
// Original0x11E098 normalization and matrix columns +1A0/+1B0/+1C0.
// Basis from the already-normalized stored quaternion; no extra normalization.
OriginalPhysicalOrientation originalOrientationBasis(std::array<float,4>);
OriginalPhysicalOrientation originalRebuildOrientation(std::array<float,4> quaternion);
}
