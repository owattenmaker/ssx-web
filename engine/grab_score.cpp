#include "grab_score.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <bit>
#include <algorithm>
#include <cfenv>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {namespace {
constexpr float F(uint32_t bits){return std::bit_cast<float>(bits);}
using Round=OriginalRounding;
int32_t plus(int32_t a,int32_t b){return std::bit_cast<int32_t>(uint32_t(a)+uint32_t(b));}
void appendHistory(std::array<int32_t,3>& list,int32_t id){
    if(list[0]==0){list[0]=id;return;}
    for(unsigned i=0;i<2;++i){if(list[i+1]==0){if(list[i]!=id)list[i+1]=id;return;}}
}
}
void originalResetGrabScore(OriginalGrabScoreState& s){
 const float timeout=s.comboTimeoutA4,multiplier=s.multiplier1C4;
 s={};s.comboTimeoutA4=timeout;s.multiplier1C4=multiplier;
}
float originalGrabScoreBegin(OriginalGrabScoreState& s,const OriginalGrabScoreRule& rule){
    Round round;float base=terrain_original::mul(float(rule.beginPoints),F(0x38d1b717)),rate=terrain_original::mul(float(rule.holdPoints),F(0x35dfb23b));
    s.accumulated14=originalScalarAdd(s.accumulated14,base);s.holdIncrement3C=rate;
    if(s.holdSeconds40<0)s.holdSeconds40=0;
    if(rule.scoreId>=35)s.activeUber5C=1;
    s.comboTimeoutA4=-1;return 0;
}
float originalGrabScoreEnd(OriginalGrabScoreState& s,int32_t id,int32_t tier){
    Round round;appendHistory(s.history60,id);
    if(id>=35){s.uberCount54=plus(s.uberCount54,1);if(tier>=6)s.superUberCount58=plus(s.superUberCount58,1);}
    else if(id>=19)s.tweakCount50=plus(s.tweakCount50,1);else s.normalCount4C=plus(s.normalCount4C,1);
    s.totalSeconds44=originalScalarAdd(s.totalSeconds44,s.holdSeconds40);
    s.longestSeconds48=std::max(s.holdSeconds40,s.longestSeconds48);s.activeUber5C=0;s.holdSeconds40=-1;return 0;
}
std::optional<OriginalGrabScoreBonus> originalGrabScoreTick(OriginalGrabScoreState& s,const OriginalGrabScoreProfile& profile,float timeScale){
    Round round;if(s.holdSeconds40<0)return std::nullopt;
    s.holdSeconds40=originalScalarAdd(s.holdSeconds40,terrain_original::mul(timeScale,F(0x3c888889)));s.accumulated14=originalScalarAdd(s.accumulated14,s.holdIncrement3C);
    if(s.holdThresholdIndex8C<0||size_t(s.holdThresholdIndex8C)>=profile.holdThresholds.size())throw std::runtime_error("Original grab hold threshold index outside authored table");
    auto threshold=profile.holdThresholds[s.holdThresholdIndex8C];if(threshold.seconds<0||s.holdSeconds40<threshold.seconds)return std::nullopt;
    float points=originalScalarAdd(terrain_original::mul(threshold.points,s.multiplier1C4),.5f);int32_t count=int32_t(points);
    s.holdThresholdIndex8C=plus(s.holdThresholdIndex8C,1);s.bonusPoints84=plus(s.bonusPoints84,count);
    return OriginalGrabScoreBonus{32,count,int(threshold.seconds),1.5f,{28,29,30,31,32}};
}
int32_t originalCurrentTrickPoints(const OriginalGrabScoreState& s){
    Round round;float points=terrain_original::mul(s.multiplier1C4,s.accumulated14);points=terrain_original::mul(points,10000.f);points=originalScalarAdd(points,5.f);
    int32_t rounded=int32_t(points);return plus(rounded,-(rounded%10));
}
}
