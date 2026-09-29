#pragma once
#include "orientation_motion.hpp"
#include <functional>
namespace ssx {
struct OriginalStanceRestoreState {
    int prewindStyle=0,motionMode=0;
    OriginalPhysicalOrientation physical{{0,0,0,1},{1,0,0},{0,1,0},{0,0,1}};
};
struct OriginalStanceRestoreResult {
    bool restored=false,physicalRotated=false,sequenceRootsRotated=false,defaultRootReset=false;
    float physicalAngle=0;
    std::array<float,4> sequenceRootRotation{0,0,0,1},defaultRootRotation{0,0,0,1};
    int animationSemantic=-1;
};
struct OriginalStanceRestoreCallbacks {
    // Called in source order, before prewindStyle is cleared. Existing root
    // transforms are composed with sequenceRootRotation; mirror flags stay put.
    std::function<void(const OriginalPhysicalOrientation&)> physicalChanged;
    std::function<void(std::array<float,4>)> rotateSequenceRoots;
    std::function<void(std::array<float,3>,std::array<float,4>)> resetDefaultRoot;
    std::function<void(int semantic,float blend,uint32_t flags)> requestAnimation;
};
// Complete115640. No-op116930 is accounted for after clearing328. The helper
// does not invent a control/motion transition or normalize unchanged poses.
OriginalStanceRestoreResult originalRestoreStance(OriginalStanceRestoreState&,
    const OriginalStanceRestoreCallbacks& callbacks={});
}
