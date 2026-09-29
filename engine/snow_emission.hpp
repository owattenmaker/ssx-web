#pragma once
#include <array>
#include <cstdint>
#include <optional>
namespace ssx {
using SnowVector=std::array<float,3>;
using SnowColour=std::array<float,4>;
// Separate effects LCG atgp+A0C. Never substitute the gameplay6-word RNG.
struct OriginalSnowRandom {uint32_t word=0;float next();};
struct OriginalSnowBoardFrame {SnowVector right{},forward{},up{},originCm{};};
struct OriginalSnowRiderInput {
    SnowVector velocityCmps{};
    float turn=0,brake=0;
    int manualState=0,animationSemantic=0,motionMode=0,controlState=0;
    bool reverse=false,trackingInhibited=false,trackingAD0=false,trackingAFC=false,trackingB00=false;
};
struct OriginalSnowRiderCache {float turnAmount=0,speedCmps=0,edgeBias=0;SnowVector direction{};bool groundEmission=false;};
OriginalSnowRiderCache originalSnowRiderCache(const OriginalSnowRiderInput&); //2DF960..2DFB5C
struct OriginalSnowTrailProfile {float velocityScale=.4000000059604645f,normalScale=0,normalSpeedScale=0;};
struct OriginalSnowTrailContext {
    OriginalSnowBoardFrame board;
    SnowVector velocityCmps{},groundNormal{};
    SnowColour colour{1,1,1,1};
    float absoluteSpeedCmps=0,edgeBias=0; //FX+BC,+12C
    bool groundEmission=false,surfaceActive=false,suppressed=false; //FX+B0,surface+58,rider88C->A0
};
// Arguments delivered to original3717C0. Null velocity/colour means retain the
// emitter's previous defaults, including an inactive birth slot.
struct OriginalSnowEmission {
    SnowVector positionCm{};
    std::optional<SnowVector> velocityCmps;
    std::optional<SnowColour> colour;
    bool active=false;
    float stepSeconds=.01666666753590107f;
    unsigned emitter=0;
};
void originalSnowBoardJitter(SnowVector&,const OriginalSnowBoardFrame&,float edgeBias,OriginalSnowRandom&); //2DE398
OriginalSnowEmission originalSnowTrailEmission(const OriginalSnowTrailProfile&,const OriginalSnowTrailContext&,OriginalSnowRandom&); //2E0EE8
struct OriginalSnowImpactState {
    float strength=0,buildup=0,alpha=0; //FX+E0,+4,+120
    SnowVector positionCm{},normal{}; //FX+F0,+100
    bool kind=false,wideScatter=false; //FX+114,+124
};
struct OriginalSnowImpactContext {
    SnowVector boardOriginCm{},velocityCmps{},groundNormal{};
    SnowColour colour{1,1,1,1};
    bool smallActive=false,largeActive=false,trackingInhibited=false;
    float decay=0;
    int motionMode=0;
};
struct OriginalSnowImpactEmission {std::array<OriginalSnowEmission,2> births;bool requestSecondaryImpact=false;};
OriginalSnowImpactEmission originalSnowImpactEmission(OriginalSnowImpactState&,const OriginalSnowImpactContext&,OriginalSnowRandom&); //2E1598
struct OriginalSnowCloudContext {
    OriginalSnowTrailContext trail;
    SnowVector lateral{},direction{};
    float turn=0,turnAmount=0,maxHeightCm=45;
    bool reverse=false;
};
OriginalSnowEmission originalSnowCloudEmission(const OriginalSnowTrailProfile&,const OriginalSnowCloudContext&,OriginalSnowRandom&); //2E1A80
struct OriginalSnowChunkProfile : OriginalSnowTrailProfile {float sideScale=0;};
struct OriginalSnowChunkContext {
    OriginalSnowCloudContext cloud;
    float brake=0,secondaryBrake274=0;
    std::array<float,2> chanceScales{1,1}; //surface70(small),74(large)
    OriginalSnowImpactState impact;
    int motionMode=0;
    bool largeImpactActive=false;
    std::optional<SnowVector> wakeVelocity;
    float minWakeScale=1.1f,maxWakeScale=1.1f;
};
std::array<OriginalSnowEmission,2> originalSnowChunkEmission(const std::array<OriginalSnowChunkProfile,2>& largeThenSmall,
    const OriginalSnowChunkContext&,OriginalSnowRandom&); //2E02B8, returns small2 thenlarge1
}

namespace ssx {
struct OriginalBreathState {float accumulator=0;int phase=0;float effort=0,clock=0,duration=0;};
struct OriginalBreathContext {
    std::array<std::array<float,4>,4> headMatrix{}; // source column-major head bone matrix
    SnowVector velocityCmps{},groundNormal{};
    float speedCmps=0,environmentValue=0;
    int animationClass=0;
};
OriginalSnowEmission originalBreathEmission(OriginalBreathState&,const OriginalSnowTrailProfile&,const OriginalBreathContext&,OriginalSnowRandom&); //2E1120
}

namespace ssx {OriginalSnowEmission originalRockSprayEmission(const OriginalSnowChunkProfile&,const OriginalSnowCloudContext&,float brake,float surfaceChance,OriginalSnowRandom&);}

namespace ssx {OriginalSnowEmission originalSnowKickerEmission(float& buildup,const OriginalSnowTrailProfile&,const OriginalSnowTrailContext&,float surfaceCapacity,OriginalSnowRandom&);}
