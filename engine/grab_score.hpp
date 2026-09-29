#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
namespace ssx {
struct OriginalGrabScoreRule {int32_t scoreId=0,beginPoints=0,holdPoints=0;};
struct OriginalGrabScoreThreshold {float seconds=-1,points=0;};
struct OriginalGrabScoreProfile {
    std::array<OriginalGrabScoreRule,15> normal;
    std::array<OriginalGrabScoreThreshold,4> holdThresholds;
};
struct OriginalGrabScoreState {
    float accumulated14=0,holdIncrement3C=0,holdSeconds40=-1,totalSeconds44=0,longestSeconds48=0;
    int32_t normalCount4C=0,tweakCount50=0,uberCount54=0,superUberCount58=0,activeUber5C=0;
    std::array<int32_t,3> history60{};
    int32_t bonusPoints84=0,holdThresholdIndex8C=0;
    float comboTimeoutA4=-1,multiplier1C4=1;
};
struct OriginalGrabScoreBonus {
    int eventType=32,points=0,thresholdSeconds=0;
    float displaySeconds=1.5f;
    std::array<int,5> clearEventTypes{28,29,30,31,32};
};
//117838 subset for the fields represented here: retain persistent combo
// timeout A4 and rider multiplier1C4 across an ordinary trick reset.
void originalResetGrabScore(OriginalGrabScoreState&);
// Complete119708 and1197D8; returned float is their original10E098 argument.
// Both always return +0, so these begin/end calls do not directly award boost.
float originalGrabScoreBegin(OriginalGrabScoreState&,const OriginalGrabScoreRule&);
float originalGrabScoreEnd(OriginalGrabScoreState&,int32_t scoreId,int32_t boostTier);
//117D24..117D70 plus119210: caller runs this once in121818 after AI progress.
// Hold time uses scaled dt; hold score increment is once-per-tick, unscaled.
std::optional<OriginalGrabScoreBonus> originalGrabScoreTick(OriginalGrabScoreState&,
    const OriginalGrabScoreProfile&,float timeScale);
// Complete117948 pending trick points, rounded to the original multiple of10.
int32_t originalCurrentTrickPoints(const OriginalGrabScoreState&);
}
