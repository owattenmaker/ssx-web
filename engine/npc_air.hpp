#pragma once
#include "npc_input.hpp"
#include "air_control.hpp"
namespace ssx {
struct OriginalNpcGrabTiming {
    int semantic=438;
    float marker1=-1,marker2=-1; //312820 authored marker indices, not clip duration.
};
struct OriginalNpcGrabCatalog {
    std::array<OriginalNpcGrabTiming,15> normal,tweak;
    std::array<std::array<OriginalNpcGrabTiming,15>,2> uber; //tier<5, tier>=5.
    float grabStat=0; //149690 progression2/maximum10;120038 computes its rate.
    int eventId=0; //144BE0, distinct from eventVariant.
};
struct OriginalNpcAirContext {
    int trajectoryStatus=0;float predictedTime=0,elapsed=0;
    OriginalAirControlState angular;
    float trickStat=0,boostModifier=0,superTime=0;
    int mainAnimationClass=0,boostTierCounter=0;
    bool uberEnabled=false; //actor+B2C bit0, gameplay capability.
    const OriginalNpcGrabCatalog* grabs=nullptr;
};
std::array<float,2> originalNpcRotationTimes(const OriginalAirControlState&,bool flip,float trickStat,bool boosted);
void originalNpcStopRotations(OriginalNpcTrickPlan&,float remaining,const OriginalNpcAirContext&);
int originalNpcChooseGrab(OriginalNpcTrickPlan&,float remaining,float parameterDFC,const OriginalNpcAirContext&,const std::function<uint32_t()>&);
std::array<uint32_t,2> originalNpcPassiveAirCommand(std::array<uint32_t,2>,OriginalNpcDrivingState&,const OriginalNpcDrivingContext&,const OriginalNpcAirContext&,const std::function<uint32_t()>&); //10B590 control4
std::array<uint32_t,2> originalNpcAirCommand(std::array<uint32_t,2>,OriginalNpcDrivingState&,const OriginalNpcAirContext&,const std::function<uint32_t()>&);
}
