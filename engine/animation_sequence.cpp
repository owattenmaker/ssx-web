#include "animation_sequence.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <algorithm>
#include <cfenv>
#include <stdexcept>
#include <bit>
#include <cmath>
#pragma STDC FENV_ACCESS ON
namespace ssx {namespace {
using Round=OriginalRounding;
}
OriginalAirAnimationChoice originalAirLandingAnimation(int semantic,int status,float predictedTime,float elapsed,float duration){
    Round round;OriginalAirAnimationChoice result{semantic,1};
    if(status!=1&&status!=3)return result;
    float remaining=originalScalarSubtract(predictedTime,elapsed);
    if(semantic==287&&remaining<=duration){result.semantic=305;if(remaining>0.0010000000474974513f)result.rate=std::min(originalScalarDivide(duration,remaining),2.f);}
    else if(semantic==305&&duration<remaining)result.semantic=287;
    return result;
}
bool originalAnimationStartStep(OriginalAnimationSequence&s,float pose,float timeScale){
    Round round;if(s.slots.empty())throw std::runtime_error("Original start sequence has no slot");
    s.slots[0].time=terrain_original::mul(s.slots[0].duration,pose);s.seekPending=true;
    return originalAnimationFadeStep(s,terrain_original::mul(timeScale,.01666666753590107f));
}
bool originalAnimationPrewindStep(OriginalAnimationSequence&s,float spin,float flip,float timeScale){
    Round round;if(s.slots.empty())throw std::runtime_error("Original prewind sequence has no slot");
    float magnitude=std::max(std::abs(spin),std::abs(flip));s.slots[0].time=terrain_original::mul(magnitude,s.slots[0].duration);s.seekPending=true;
    return originalAnimationFadeStep(s,terrain_original::mul(timeScale,0.01666666753590106964111328125f));
}
uint32_t originalAirAdjustSecondaryLeaf(int semantic){
    constexpr std::array<uint32_t,8> leaves={165,171,168,172,166,170,167,169};
    return semantic>=297&&semantic<=304?leaves[semantic-297]:165;
}
bool originalAnimationAirAdjustStep(OriginalAnimationSequence&s,uint32_t clip,float duration,float adjustFlip,float adjustSpin,float timeScale){
    Round round;if(s.slots.empty())throw std::runtime_error("Original air-adjust primary slot missing");
    if(s.slots.size()<2)s.slots.push_back({});
    auto&secondary=s.slots[1];secondary.clip=clip;secondary.duration=duration;
    if(!secondary.enabled){secondary.time=0;secondary.rate=1;secondary.weight=1;secondary.enabled=true;secondary.loop=false;}
    secondary.loop=true;
    float magnitude=std::clamp(std::max(std::abs(adjustFlip),std::abs(adjustSpin)),0.f,1.f);
    float weight=originalScalarAdd(terrain_original::mul(magnitude,.75f),.25f);
    s.slots[0].weight=originalScalarSubtract(1.f,weight);secondary.weight=weight;
    s.slots[0].time=terrain_original::mul(magnitude,s.slots[0].duration);s.seekPending=true;
    float dt=terrain_original::mul(timeScale,std::bit_cast<float>(0x3c888889u));
    (void)originalAnimationSlotStep(secondary,s.rate,dt);
    return originalAnimationFadeStep(s,dt);
}
void originalAnimationFadeOut(OriginalAnimationSequence&s,float duration){
    if(s.targetWeight!=0||s.fadeRemaining==0||duration<s.fadeRemaining)s.fadeRemaining=duration;
    s.targetWeight=0;s.stopWhenFaded=true;
}
bool originalAnimationFadeStep(OriginalAnimationSequence&s,float dt){
    Round round;if(s.fadeRemaining==0)return false;
    if(s.fadeRemaining<=dt){s.weight=s.targetWeight;s.fadeRemaining=0;return s.stopWhenFaded;}
    float ratio=originalScalarDivide(dt,s.fadeRemaining);s.fadeRemaining=originalScalarSubtract(s.fadeRemaining,dt);float delta=originalScalarSubtract(s.targetWeight,s.weight);delta=terrain_original::mul(ratio,delta);s.weight=originalScalarAdd(s.weight,delta);return false;
}
bool originalAnimationSlotStep(OriginalAnimationSlot&s,float sequenceRate,float dt){
    Round round;if(!s.enabled)return false;if(s.duration<=0)throw std::runtime_error("Invalid original slot duration");float delta=terrain_original::mul(s.rate,sequenceRate);delta=terrain_original::mul(delta,dt);s.time=originalScalarAdd(s.time,delta);
    bool completed=s.loop?(s.time<0||s.time>=s.duration):(s.time<=0||s.time>=s.duration);
    if(s.loop&&(s.time<0||s.time>=s.duration)){float quotient=originalScalarDivide(s.time,s.duration);float count=float(int(quotient));if(quotient<count)count=originalScalarSubtract(count,1.f);float span=terrain_original::mul(s.duration,count);s.time=originalScalarSubtract(s.time,span);}
    else if(!s.loop)s.time=std::clamp(s.time,0.f,s.duration);
    return completed;
}
std::vector<AnimationLayer> originalAnimationLayers(const std::vector<OriginalAnimationSequence>&sequences){
    Round round;std::vector<AnimationLayer>result;
    for(const auto&s:sequences)for(const auto&slot:s.slots){if(!slot.enabled)continue;float weight=terrain_original::mul(s.weight,slot.weight);if(slot.time<s.startFadeIn)weight=std::min(weight,originalScalarDivide(slot.time,s.startFadeIn));float remaining=originalScalarSubtract(slot.duration,slot.time);if(remaining<s.endFadeOut)weight=std::min(weight,originalScalarDivide(remaining,s.endFadeOut));result.push_back({slot.clip,slot.time,weight,s.priority,s.mask,s.root,s.mirror});}
    return result;
}
}
