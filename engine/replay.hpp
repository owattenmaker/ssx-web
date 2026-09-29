#pragma once
#include "original_command.hpp"
#include "riding.hpp"
#include <array>
#include <optional>
#include <string>

namespace ssx {
// Active-high PS2 mask: bits0..15 have the same positions as the movie's
// two active-low button bytes. Analog bytes retain their exact source values.
struct ReplayPad {
    uint16_t buttons=0;
    std::array<uint8_t,2> left={127,127},right={127,127};
    std::array<uint8_t,18> movieBytes() const {
        std::array<uint8_t,18> result{};
        result[0]=uint8_t(~buttons);result[1]=uint8_t(~(buttons>>8));
        result[2]=right[0];result[3]=right[1];result[4]=left[0];result[5]=left[1];
        constexpr int pressure[16]={-1,-1,-1,-1,8,6,9,7,16,17,14,15,10,11,12,13};
        for (unsigned i=0;i<16;++i) if ((buttons&(1u<<i))&&pressure[i]>=0) result[pressure[i]]=255;
        return result;
    }
};
inline uint16_t replayButton(const std::string& name) {
    constexpr const char* names[]={"Select","L3","R3","Start","Up","Right","Down","Left",
                                  "L2","R2","L1","R1","Triangle","Circle","Cross","Square"};
    for (unsigned i=0;i<16;++i) if (name==names[i]) return uint16_t(1u<<i);
    throw std::runtime_error("Unknown PS2 button: "+name);
}
inline InputSample replayInput(const ReplayPad& pad) {
    return originalReplayInput(pad.movieBytes());
}
struct ReplayEvent {
    int start=0,end=0;
    uint16_t buttons=0;
    std::optional<std::array<uint8_t,2>> left,right;
};
struct AcceptedReplayEvent {
    int start=0,end=0,controlState=0;
    uint32_t word0=0,word1=0;
    RiderInput input;
};
class InputReplay {
public:
    int frames;
    std::vector<ReplayEvent> events;
    std::vector<AcceptedReplayEvent> accepted;
    bool initialJumpHeld=false;
    bool decodeAcceptedAtRuntime=false;
    int expectedInitialControlState=-1;
    InputReplay(int count,std::vector<ReplayEvent> input):frames(count),events(std::move(input)) {
        if (frames<1||frames>36000) throw std::runtime_error("Replay duration must be 1..36000 frames");
        for (const auto& event:events)
            if (event.start<0||event.start>=event.end||event.end>frames) throw std::runtime_error("Input range outside replay");
    }
    ReplayPad at(int frame) const {
        if (frame<0||frame>=frames) throw std::runtime_error("Replay frame outside range");
        ReplayPad pad;
        for (const auto& event:events) if (event.start<=frame&&frame<event.end) {
            pad.buttons|=event.buttons;
            if (event.left) pad.left=*event.left;
            if (event.right) pad.right=*event.right;
        }
        return pad;
    }
    const AcceptedReplayEvent* acceptedAt(int frame) const {
        if(accepted.empty())return nullptr;
        auto found=std::upper_bound(accepted.begin(),accepted.end(),frame,[](int f,const AcceptedReplayEvent& e){return f<e.end;});
        if(found==accepted.end()||frame<found->start)throw std::runtime_error("Accepted input has a coverage gap");
        return &*found;
    }
};
struct ReplayRecord {
    int frame=0,inputFrame=-1,riderControlState=-1;
    ReplayPad pad;
    RiderInput input;
    std::vector<ObstacleContact> obstacleContacts;
    Vec3 position,velocity,normal;
    double heading=0,speed=0,jumpCharge=0,simulationTime=0;
    uint64_t airborneTicks=0;bool airSpeedCapped=false;
    bool grounded=false,jumpReleased=false,paused=false,obstacleContact=false;
    std::optional<OriginalBoostState> originalBoost;OriginalBoostEffects boostEffects;
    std::optional<OriginalRaceSessionSnapshot> originalRace;
    std::optional<OriginalGroundState> originalGround;
    OriginalGroundDiagnostics groundDiagnostics;
    uint64_t groundTicks=0;
    int groundSurfaceId=0;bool groundSurfaceProfileMissing=false;
    RayHit groundContact;
    std::optional<AcceptedReplayEvent> accepted;
    bool bodyMissingWorld=true,bodyMissingPose=true,bodyOrientationPending=false,bodyEventPending=false;
    std::optional<OriginalLandingRuntime> originalLanding;OriginalWorldSegmentHit landingContact;
    bool landingPending=false,landingScorePending=false;uint32_t lastLandingTick=0;
    std::optional<OriginalAirPrewindState> originalAirPrewind;
    std::optional<OriginalAirControlState> originalAirControl;
    std::optional<OriginalAirTrajectory> originalAirTrajectory;
    std::optional<OriginalAirAlignmentContext> originalAirAlignment;
    std::optional<OriginalAirPresentation> originalAirPresentation;
    std::array<float,4> airPhysicalQuaternion{0,0,0,1};
    bool airAngularSupported=false,airPhysicalOrientationPending=true;
    uint64_t airControlTicks=0;
    WorldBodyQuery bodyQuery;
    std::optional<BodyCollisionVolume> bodyVolume;
    int motionMode=0,crashPhase=-1,crashSubmode=-1;float crashRecovery=0;bool resetRequested=false;
};
inline std::vector<ReplayRecord> runReplay(const InputReplay& replay,PrototypeRider& rider,
                                         const CollisionWorld& terrain,const CollisionWorld& obstacles,
                                         const WorldBodyCollision* bodyWorld=nullptr,const BodyPoseProvider* poseProvider=nullptr,const RiderPoseStages* stages=nullptr) {
    if(replay.decodeAcceptedAtRuntime&&rider.currentControlState()!=replay.expectedInitialControlState)
        throw std::runtime_error("Raw replay initial controller differs from original baseline");
    std::vector<ReplayRecord> records;
    records.reserve(size_t(replay.frames)+1);
    auto record=[&](int frame,int inputFrame,const ReplayPad& pad,const RiderInput& input,bool released,bool paused,const AcceptedReplayEvent* accepted=nullptr){
        records.emplace_back();auto& row=records.back();
        row.frame=frame;row.inputFrame=inputFrame;row.pad=pad;row.input=input;row.riderControlState=rider.currentControlState();
        if(!paused)row.obstacleContacts=rider.obstacleContacts;
        row.position=rider.position;row.velocity=rider.velocity;row.normal=rider.normal;
        row.heading=rider.heading;row.speed=rider.speed();row.jumpCharge=rider.jumpCharge;row.simulationTime=rider.elapsed;
        row.airborneTicks=rider.airborneTicks;row.airSpeedCapped=rider.airSpeedCapped;row.grounded=rider.grounded;
        row.jumpReleased=released;row.paused=paused;row.obstacleContact=rider.obstacleContact;
        row.bodyMissingWorld=rider.bodyCollisionMissingWorld;row.bodyMissingPose=rider.bodyCollisionMissingPose;
        row.bodyOrientationPending=rider.obstacleOrientationPending;row.bodyEventPending=rider.obstacleEventPending;
        row.bodyQuery=rider.lastBodyQuery;row.bodyVolume=rider.lastBodyVolume;
        row.airAngularSupported=rider.airAngularSupported;row.airPhysicalOrientationPending=rider.airPhysicalOrientationPending;
        row.airControlTicks=rider.airControlTicks;row.airPhysicalQuaternion=rider.originalAirPhysicalQuaternion();
        row.motionMode=rider.currentMotionMode();row.resetRequested=rider.resetRequested;
        if(rider.isCrashing()){row.crashPhase=rider.originalCrashControlState().phase;row.crashSubmode=rider.originalCrashMotionState().submode;row.crashRecovery=rider.originalCrashControlState().recovery70;}
        if(rider.hasOriginalLanding()){row.originalLanding=rider.originalLandingRuntime();row.landingContact=rider.lastLandingContact;row.landingPending=rider.landingTransitionPending;row.landingScorePending=rider.landingScoreEventPending;row.lastLandingTick=rider.lastLandingTick;}
        if(rider.hasOriginalAirEntry())row.originalAirPrewind=rider.originalAirPrewindState();
        if(rider.hasOriginalAirControl())row.originalAirControl=rider.originalAirControlState();
        if(rider.hasOriginalAirTrajectory()) {
            row.originalAirTrajectory=rider.originalAirTrajectoryState();row.originalAirAlignment=rider.originalAirAlignmentState();
        }
        if(rider.hasOriginalAirPresentation())row.originalAirPresentation=rider.originalAirPresentationPose();
        if(rider.hasOriginalBoost()){row.originalBoost=rider.originalBoostState();row.boostEffects=rider.originalBoostEffects();}
        if(rider.hasOriginalRaceSession())row.originalRace=rider.originalRaceState();
        if(rider.hasOriginalGround()) {
            auto& row=records.back();row.originalGround=rider.originalGroundState();row.groundDiagnostics=rider.originalGroundDiagnostics();
            row.groundTicks=rider.groundTicks;row.groundContact=rider.lastGroundContact;
            row.groundSurfaceId=rider.currentGroundSurfaceId();row.groundSurfaceProfileMissing=rider.groundSurfaceProfileMissing;
        }
        if(accepted)row.accepted=*accepted;
    };
    record(0,-1,{}, {},false,false);
    InputMapper mapper;bool previousJump=replay.initialJumpHeld,paused=false;
    for (int frame=0;frame<replay.frames;++frame) {
        auto pad=replay.at(frame);RiderInput input;std::optional<AcceptedReplayEvent> acceptedInput;
        if(auto* accepted=replay.acceptedAt(frame)){
            acceptedInput=*accepted;
            if(replay.decodeAcceptedAtRuntime){
                acceptedInput->controlState=rider.currentControlState();
                try{acceptedInput->input=originalDecodeCommand(acceptedInput->controlState,acceptedInput->word0,acceptedInput->word1);}
                catch(const std::exception& e){throw std::runtime_error("Original raw replay frame "+std::to_string(frame)+": "+e.what());}
            }
            input=acceptedInput->input;
        }
        else input=mapper.update(replayInput(pad));
        bool released=previousJump&&!input.jumpHeld;previousJump=input.jumpHeld;
        if (input.pausePressed) paused=!paused;
        if (!paused) rider.advance(1.0/60,input,terrain,obstacles,bodyWorld,poseProvider,stages);
        record(frame+1,frame,pad,input,released,paused,acceptedInput?&*acceptedInput:nullptr);
    }
    return records;
}
}
