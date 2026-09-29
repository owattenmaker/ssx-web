#include "stance_restore.hpp"
#include <bit>
#include <cfenv>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {
OriginalStanceRestoreResult originalRestoreStance(OriginalStanceRestoreState& state,const OriginalStanceRestoreCallbacks& callbacks){
    OriginalStanceRestoreResult result;
    if(state.prewindStyle==0)return result;
    struct Round{int old=std::fegetround();Round(){if(std::fesetround(FE_TOWARDZERO))throw std::runtime_error("Original stance restore rounding");}~Round(){std::fesetround(old);}}round;
    const int style=state.prewindStyle;result.restored=true;
    auto resetRoot=[&](){
        auto sc=originalSinCos(-0.f);
        result.defaultRootRotation={sc[0]*0.f,sc[0]*0.f,sc[0]*1.f,sc[1]};
        result.defaultRootReset=true;
        if(callbacks.resetDefaultRoot)callbacks.resetDefaultRoot({0,0,0},result.defaultRootRotation);
    };
    if(state.motionMode==1){
        if(style==3||style==4)resetRoot();
        result.animationSemantic=style==3?282:style==4?277:268;
    }else if(state.motionMode==0){
        if(style==3||style==4){
            result.physicalAngle=std::bit_cast<float>(style==3?0x3fc90fdbu:0xbfc90fdbu);
            state.physical=originalRebuildOrientation(originalRotateOrientation(state.physical.quaternion,state.physical.up,result.physicalAngle));
            result.physicalRotated=true;
            if(callbacks.physicalChanged)callbacks.physicalChanged(state.physical);
            auto sc=originalSinCos(-result.physicalAngle*.5f);
            result.sequenceRootRotation={sc[0]*0.f,sc[0]*0.f,sc[0]*1.f,sc[1]};
            result.sequenceRootsRotated=true;
            if(callbacks.rotateSequenceRoots)callbacks.rotateSequenceRoots(result.sequenceRootRotation);
            resetRoot();
        }
        result.animationSemantic=5;
    }
    if(result.animationSemantic>=0&&callbacks.requestAnimation)callbacks.requestAnimation(result.animationSemantic,-1.f,0);
    state.prewindStyle=0;
    return result;
}
}
