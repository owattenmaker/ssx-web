#pragma once
#include <array>
#include <cstdint>
#include <functional>
namespace ssx {
struct OriginalGrabState {int state=0,index=-1;}; //owner234/238
struct OriginalGrabDefinition {int semantic=438,upperSemantic=438,scoreId=-1;int beginPoints=0,holdPoints=0;};
struct OriginalGrabProfile {
    float playbackRate=1;std::array<OriginalGrabDefinition,15> grabs;
    std::array<OriginalGrabDefinition,15> tweak;
    std::array<std::array<OriginalGrabDefinition,15>,2> uber;
    bool extendedDefinitions=false;
};
struct OriginalGrabContext {float superTime=0;int boostTier=0;bool uberEnabled=false;};
float originalGrabPlaybackRate(float grabStat); //120038
//120D90, called by timerFrame AFTER controllers and BEFORE1211F8 filters.
float originalGrabLegWeight(float previous,int mainAnimationClass);
struct OriginalGrabAnimationAccess {
    std::function<int()> mainClass;
    std::function<uint64_t()> mainFlags;
    std::function<void(int semantic,bool force)> play;
    std::function<void(unsigned channel,float)> setRate,fade;
    // Original150118->119708/1197D8->10E098 chain. The caller owns score state.
    std::function<void(int grabIndex,bool begin)> score;
    // Complete mapped150118/150178/1503F8 score requests; legacy ordinary
    // score(index,begin) remains valid when this callback is absent.
    std::function<void(int scoreId,bool begin)> mappedScore;
    std::function<void()> advancedStarted; //29A530 notification, no implicit award.
};
struct OriginalGrabResult {bool supported=true,active=false,advancedStarted=false;};
// Complete1352A8 states0..5. Extended transitions require their authored
// definitions and mappedScore callback. Context is sampled once on entry.
OriginalGrabResult originalGrabLifecycle(OriginalGrabState&,const OriginalGrabProfile&,
    int requestedIndex,bool tweakHeld,const OriginalGrabAnimationAccess&,const OriginalGrabContext& context={});
}
