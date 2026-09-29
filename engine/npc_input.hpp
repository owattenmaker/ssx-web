#pragma once
#include <array>
#include <cstdint>
namespace ssx {
// Original10C0A8/10C140 command-writer leaves, used by the NPC behavior
// dispatcher. These are not a replacement for its path/state/decision driver.
uint32_t originalNpcCrouchWord(uint32_t commandWord0,std::array<float,3> velocityCmps,float fieldDF8);
uint32_t originalNpcBoostWord(uint32_t commandWord0,std::array<float,3> velocityCmps);
}
namespace ssx {
//100610: correction to the retained route lookahead, all source centimeters.
std::array<float,3> originalNpcSteeringTarget(std::array<float,3> position,
    std::array<float,3> closestRoutePoint,std::array<float,3> previousLookaheadPoint);
//100348: returns the original steering request before command-bit quantization.
// Uses raw velocity, not board-forward; the original NPC virtual methods return
// actor1E0 and110. Board-up1C0 is replaced byworld-up when its Z<=0.
float originalNpcSteering(std::array<float,3> position,std::array<float,3> velocity,
    std::array<float,3> boardUp,std::array<float,3> target);
}
#include "npc_path.hpp"
namespace ssx {
struct OriginalNpcPrewindInput {
    std::array<float,3> position{},velocity{},boardUp{0,0,1},closestRoutePoint{};
    float previousDistance=0,currentDistance=0,desiredSpeedDF0=0;
    bool regionModeE1C=false;
    std::array<float,3> regionStartE50{},regionEndE60{};
    float plannedSpinE38=0,plannedFlipE3C=0;
};
// Complete10AA70 controller2 command producer. No state/RNG mutation; it uses
// current AI-path events/region plan and preserves unrelated existing bits.
std::array<uint32_t,2> originalNpcPrewindCommand(std::array<uint32_t,2>,const OriginalNpcPath&,
    const OriginalNpcPrewindInput&);
}
#include <functional>
namespace ssx {
struct OriginalNpcTrickPlan {
    int32_t indexE0C=-1,enabledE10=0,decisionE14=0,phaseE18=0,kindE1C=0,fieldE20=0;
    int32_t fieldE24=0,fieldE28=0;
    float releaseE2C=0,startE30=0,remainingE34=0,spinE38=0,flipE3C=0;
    std::array<int32_t,15> normalCounts{},uberCounts{},tweakCounts{};
};
// Original10BFA8 and10C1D0. The probability comparison and optional two sign
// draws consume the shared game RNG in source order; no per-rider RNG is used.
void originalNpcPrepareTrick(OriginalNpcTrickPlan&,bool spin,bool flip,int32_t fieldE20,
    int32_t kindE1C,float parameterDFC,const std::function<uint32_t()>& nextRandom);
}
#include <optional>
namespace ssx {
struct OriginalNpcPacingContext {
    std::optional<float> referenceRemaining;
    float remaining=0,negativeThresholdDC=0,positiveThresholdE0=0;
    int32_t modeE4=0;int8_t eventVariant=0;
};
float originalNpcTimeScaleTarget(const OriginalNpcPacingContext&); //10DEF0.
float originalNpcTimeScaleApproach(float current,float target); //120090.
}
#include "rider_pair_collision.hpp"
namespace ssx {
//10BD10 keeps an eligible peer target or rescans the six retained proximity
// records on the original12-tick cadence. designatedResult changes only when
// a newly selected record wins, matching the original output parameter.
bool originalNpcSelectPeer(int32_t& targetE70,int32_t& designatedResult,float lateralDistance,
    std::array<float,3> physicalForward,const std::array<OriginalPairRecord,6>&,
    std::optional<int> designatedPeer,int32_t tick);
}
namespace ssx {
enum class OriginalNpcBehavior {Cruise100680,Jump1009E0,Designated100B90,Peer100F88};
struct OriginalNpcDrivingState {
    float desiredSpeedDF0=0,parameterDF8=0,parameterDFC=0;
    int32_t boardTimerE40=0,targetPeerE70=-1;
    int32_t offRouteTicksE74=0,oppositeHeadingTicksE78=0;
    int32_t longFlightTicks484=0;
    float defensiveTimerF30=0;int32_t defensiveDecisionF34=0,lastAttackTickF4=0;
    int16_t behaviorCounterF38=0;
    OriginalNpcBehavior behavior=OriginalNpcBehavior::Cruise100680;
    std::array<float,3> regionStartE50{},regionEndE60{};
    OriginalNpcTrickPlan trick;
};
struct OriginalNpcDrivingContext {
    std::array<float,3> position{},velocity{},boardUp{0,0,1},physicalForward{0,1,0};
    OriginalNpcRouteState route;
    float boostMeter=0,superTime=0;int32_t tick=0;
    int motionMode=0,controlState=0;float trajectoryElapsed=0;
    float physicalRightZ=0;int prewindStyle=0,mainAnimationClass=0;
    unsigned selfSlot=0,participantCount=6;std::array<int,6> peerUpperAnimationClass{};
    std::array<OriginalPairRecord,6> peers;
    std::array<bool,6> peerHuman{};
    std::array<std::array<float,3>,6> peerVelocities{},peerPositions{};
    std::array<int,6> peerPathIndices{-1,-1,-1,-1,-1,-1};
    std::optional<int> designatedPeer;
};
// Complete default ground behavior100680. State transitions select the next
// behavior for a later invocation; they do not redispatch within this call.
std::array<uint32_t,2> originalNpcCruiseBehavior(std::array<uint32_t,2>,OriginalNpcDrivingState&,
    const OriginalNpcDrivingContext&,const OriginalNpcPath&);
}
namespace ssx {
//1009E0 jump-zone approach. Calls100680 immediately if the zone is gone;
// otherwise steers, prepares the authored spin/flip plan and requests Cross.
std::array<uint32_t,2> originalNpcJumpBehavior(std::array<uint32_t,2>,OriginalNpcDrivingState&,
    const OriginalNpcDrivingContext&,const OriginalNpcPath&,const std::function<uint32_t()>& nextRandom);
}

namespace ssx {
//100F88 peer-aware driving. May invoke the fully recovered NPC route selector
// immediately; publishes that change through context.route before returning.
std::array<uint32_t,2> originalNpcPeerBehavior(std::array<uint32_t,2>,OriginalNpcDrivingState&,
    OriginalNpcDrivingContext&,std::span<const OriginalNpcPath>,OriginalNpcPathScoreContext,
    const std::function<uint32_t()>& nextRandom);
}

namespace ssx {
//10D1A0: wrong-way/off-route/fall recovery request. This returns bit12's
// request; it does not invent the separate reset/recovery lifecycle.
bool originalNpcRecovery(OriginalNpcDrivingState&,const OriginalNpcDrivingContext&);
}

namespace ssx {
//10DBF0 supplies left/right attack requests; relationship corresponds to the
// original155B50 save/roster query and is evaluated only for eligible peers.
std::optional<bool> originalNpcAttackRequest(OriginalNpcDrivingState&,const OriginalNpcDrivingContext&,
    const std::function<int(unsigned)>& relationship);
//10DA10's nearby-human attack response, including its unscaled2-second cache.
int32_t originalNpcDefensiveRequest(OriginalNpcDrivingState&,const OriginalNpcDrivingContext&,
    const std::function<uint32_t()>& nextRandom);
}

namespace ssx {
struct OriginalNpcAirContext;
struct OriginalNpcProviderContext {
    OriginalNpcDrivingContext driving;
    OriginalNpcPacingContext pacing;
    OriginalNpcPathScoreContext scoring;
    float currentTimeScale=1;
    const OriginalNpcAirContext* air=nullptr;
};
struct OriginalNpcProviderResult {std::array<uint32_t,2> words{};float timeScale=1;};
// Runtime10A768 facade. Mutates only its NPC state/context and shared RNG via
// the callback. Publish result.timeScale to the rider before control dispatch;
// copy back context.driving.route if the behavior selects a different path.
// Current coverage: original controls0/1/2/3/4/5/6/8/9/10/11/12/13 and all four
// ground behaviors. Control7 remains explicit until its undefined-output
// branch and native rail lifecycle are resolved.
OriginalNpcProviderResult originalNpcControl(OriginalNpcDrivingState&,OriginalNpcProviderContext&,
    std::span<const OriginalNpcPath>,const std::function<int(unsigned)>& relationship,
    const std::function<uint32_t()>& nextRandom);
}

namespace ssx {
//10B790 control6: start-grid leaning, then cadence-driven random balance.
std::array<uint32_t,2> originalNpcStartCommand(std::array<uint32_t,2>,OriginalNpcDrivingState&,int32_t tick,unsigned slot,const std::function<uint32_t()>&);
}

namespace ssx {
std::array<uint32_t,2> originalNpcDesignatedBehavior(std::array<uint32_t,2>,OriginalNpcDrivingState&,const OriginalNpcDrivingContext&,const OriginalNpcPath&); //100B90
}

namespace ssx {
//10AED8 control7 (rail): 10B980 jump zone (launch + 10BFA8 trick plan when started), otherwise
// balance from 2*physical-right Z, RailSpin against the planned spin sign (not in sideways styles
// 3/4 or main class 14), and the 10BB18 speed-zone boost request. On a jump-zone miss the original
// copies an unwritten stack word to E20; every captured rail sequence shows 0 (docs/ai-racers.md).
std::array<uint32_t,2> originalNpcRailCommand(std::array<uint32_t,2>,OriginalNpcDrivingState&,const OriginalNpcDrivingContext&,const OriginalNpcPath&,const std::function<uint32_t()>&);
}

namespace ssx {
std::array<uint32_t,2> originalNpcManualCommand(std::array<uint32_t,2>,const OriginalNpcDrivingContext&,const OriginalNpcPath&); //10AD78 control1
}
