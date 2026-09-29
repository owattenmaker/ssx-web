#pragma once
namespace ssx {
enum class OriginalCrashAnimationSelection {GroundContinuation,AirContinuation,GroundGetUp,AirGetUp,SpecialLanding,ResetClip};
struct OriginalCrashAnimationRequest {int semantic=0;bool play=false,updatePlaybackRate=false,rebakeRoot=false;};
// Original12DCB0/12DD98/12DE80/12DF48/12E468 and reset12E010.
// All play requests use group0 and the original default blend argument−1.
inline OriginalCrashAnimationRequest originalCrashSelectAnimation(OriginalCrashAnimationSelection kind,int current,bool detached) {
    OriginalCrashAnimationRequest result;result.semantic=current;
    switch(kind) {
        case OriginalCrashAnimationSelection::GroundContinuation:
            if(detached)result.semantic=372;
            else if(current>=373&&current<=382)result.semantic=current-11;
            else if(current>=384&&current<=387)result.semantic=current-16;
            break;
        case OriginalCrashAnimationSelection::AirContinuation:
            if(detached)result.semantic=383;
            else if(current>=362&&current<=371)result.semantic=current+11;
            else if(current>=384&&current<=387)result.semantic=current-5;
            break;
        case OriginalCrashAnimationSelection::GroundGetUp:
            if(current>=362&&current<=371)result.semantic=current+27;
            else if(current>=384&&current<=387)result.semantic=current+11;
            break;
        case OriginalCrashAnimationSelection::AirGetUp:
            if(current>=373&&current<=382)result.semantic=current+26;
            break;
        case OriginalCrashAnimationSelection::SpecialLanding:
            if(current>=379&&current<=383)result.semantic=current+5;
            break;
        case OriginalCrashAnimationSelection::ResetClip:
            result.semantic=detached?409:410;result.rebakeRoot=true;break;
    }
    result.play=result.rebakeRoot||result.semantic!=current;
    result.updatePlaybackRate=result.play&&(kind==OriginalCrashAnimationSelection::GroundContinuation||kind==OriginalCrashAnimationSelection::AirContinuation);
    return result;
}
}
