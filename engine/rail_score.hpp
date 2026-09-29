#pragma once
#include "terrain_contact_math.hpp"
#include <optional>
namespace ssx {
struct OriginalRailScoreState {
 float accumulated14=0,inverted1C=0,distance24=-1,spin34=0,multiplier1C4=1;
 int32_t style20=0,bonusPoints84=0,threshold98=0;
};
struct OriginalRailScoreProfile {
 float dt=std::bit_cast<float>(0x3c888889u),distanceReward=std::bit_cast<float>(0x3851b717u),invertedReward=std::bit_cast<float>(0x3951b717u),spinReward=std::bit_cast<float>(0x3c826135u);
 std::array<std::array<float,2>,12> thresholds{{{10000,1000},{12000,3000},{14000,5000},{16000,7000},{18000,9000},{20000,12000},{22000,16000},{24000,20000},{26000,30000},{28000,40000},{30000,50000},{-1,0}}};
};
struct OriginalRailScoreBonus {
 int eventType=29,points=0,distanceCm=0;float displaySeconds=1.5f;
 std::array<int,5> clearEvents{28,29,30,31,32};
};
//119918 ->119898. This updates pending scoring; its immediate meter award is0.
inline float originalRailRotationScore(OriginalRailScoreState& state,const OriginalRailScoreProfile& profile,int style,float spin){
 using namespace terrain_original;Rounding rounding;state.style20=style;
 const float removed=mul(std::abs(state.spin34),profile.spinReward),added=mul(std::abs(spin),profile.spinReward);state.spin34=spin;
 state.accumulated14=std::max(0.f,originalScalarAdd(originalScalarSubtract(state.accumulated14,removed),added));return 0;
}
//117D74..117DFC: distance accrual and one119210 threshold event per tick.
// Input speed and dt follow117C28's original vector/scalar operation ordering.
inline std::optional<OriginalRailScoreBonus> originalRailScoreTick(OriginalRailScoreState& state,const OriginalRailScoreProfile& profile,terrain_original::Vector velocity,float upZ,float timeScale){
 using namespace terrain_original;Rounding rounding;if(state.distance24<0)return {};
 const float speed=sqrt(add(dot(velocity,velocity),0.f)),dt=mul(timeScale,profile.dt),distance=mul(speed,dt);
 state.distance24=originalScalarAdd(state.distance24,distance);state.accumulated14=originalScalarAdd(state.accumulated14,mul(distance,profile.distanceReward));
 if(upZ<0)state.inverted1C=originalScalarAdd(state.inverted1C,mul(distance,profile.invertedReward));
 if(state.threshold98<0||size_t(state.threshold98)>=profile.thresholds.size())throw std::runtime_error("Rail score threshold outside authored table");
 const auto threshold=profile.thresholds[state.threshold98];if(threshold[0]<0||state.distance24<threshold[0])return {};
 const int32_t points=int32_t(originalScalarAdd(mul(threshold[1],state.multiplier1C4),.5f));
 ++state.threshold98;state.bonusPoints84=std::bit_cast<int32_t>(uint32_t(state.bonusPoints84)+uint32_t(points));
 return OriginalRailScoreBonus{29,points,int(threshold[0]),1.5f};
}
}
