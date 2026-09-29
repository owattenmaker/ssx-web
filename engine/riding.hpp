#pragma once
#include "collision.hpp"
#include "input.hpp"
#include "air_motion.hpp"
#include "air_control.hpp"
#include "air_entry.hpp"
#include "landing_motion.hpp"
#include "landing_contact.hpp"
#include "air_alignment.hpp"
#include "air_trajectory.hpp"
#include "air_trajectory_world.hpp"
#include "jump_motion.hpp"
#include "race_session.hpp"
#include "boost_control.hpp"
#include "ground_motion.hpp"
#include "ground_surface.hpp"
#include "ground_pose_motion.hpp"
#include "ground_animation_control.hpp"
#include "soft_collision_control.hpp"
#include "passive_air_control.hpp"
#include "orientation_motion.hpp"
#include "world_body_collision.hpp"
#include "rider_pose_motion.hpp"
#include "collision_event.hpp"
#include "crash_entry.hpp"
#include "crash_recovery.hpp"
#include "crash_animation.hpp"
#include "crash_collision.hpp"
#include "crash_world.hpp"
#include "crash_detached.hpp"
#include "rail_motion.hpp"
#include "stance_restore.hpp"
#include <span>
#include <functional>
#include <optional>

namespace ssx {
class PrototypeRider;
enum class RiderFramePhase {Idle,Started,Controls,Timers,Filters,Motion,LocalPose,WorldPose,MotionContacts,GeometryCommit,Events};
inline constexpr std::array riderFramePhases={RiderFramePhase::Controls,RiderFramePhase::Timers,RiderFramePhase::Filters,RiderFramePhase::Motion,RiderFramePhase::LocalPose,RiderFramePhase::WorldPose,RiderFramePhase::MotionContacts,RiderFramePhase::GeometryCommit,RiderFramePhase::Events};
struct RiderFrameTiming {uint32_t tick=0;float finishElapsed=-1;};
struct RiderPoseStages {
    std::function<bool(const PrototypeRider&,double)> prepareLocal;
    std::function<std::optional<BodyCollisionVolume>(const PrototypeRider&,double)> finishWorld;
    std::function<std::optional<BodyAnimationState>()> mainAnimation;
};
struct RiderControllerCallbacks {
    std::function<void(PrototypeRider&)> normalUpperReaction,normalGain,restoreStance;
    std::function<void(PrototypeRider&)> airGain;
    std::function<bool(PrototypeRider&,const RiderInput&)> airGrab;
    std::function<uint32_t()> nextRandom;
    std::function<BodyAnimationState(int,float)> requestAnimation;
    std::function<int()> upperAnimationClass;
    std::function<void(unsigned,float)> animationRate,animationFade;
    std::function<bool(bool,bool)> upperAction;
    std::function<bool(bool)> handplantAction;
    std::function<bool()> railAction;
    std::function<void(const PrototypeRider&)> frameComplete;
    std::function<void(const PrototypeRider&)> timerUpdate;
    std::function<void(PrototypeRider&,const OriginalAirControlFrame&)> airAnimation;
    // Crash/ragdoll animation access:10EB30 root bake,12CB68 clip reads and
    //12D4E8/12D160 continuation playback. Missing hooks fail explicitly.
    std::function<AnimationTransform(const AnimationTransform&,const RiderRootPresentation&)> presentedRoot;
    std::function<std::optional<AnimationTransform>(int)> previewClipRoot;
    std::function<std::optional<AnimationTransform>()> currentScaledLocalRoot;
    std::function<void(const AnimationTransform&)> offsetAnimationRoots;
    std::function<void(float)> rotateAnimationRoot;
    std::function<void()> resetAnimationRootBasis;
    std::function<BodyAnimationState(int)> playCrashAnimation;
    std::function<std::optional<OriginalCrashClipState>(unsigned,unsigned)> crashClip;
    std::function<void(float)> seekMainAnimation;
    std::function<void(std::optional<AnimationTransform>)> detachedBoard;
    std::function<void(int)> crashObserver;
    // Rail (control7 / motion4) animation access.
    std::function<std::optional<AnimationTransform>(unsigned)> posedBoneTransform;
    std::function<void(bool)> setAnimationMirror;
    std::function<void(float)> setAnimationRootHalfAngle;
    std::function<void(int,float)> railScoreEvent;
    std::function<void(std::array<float,4>)> rotateAnimationRootQuaternion; //311B48 with a quaternion
    std::function<void(std::array<float,3>,std::array<float,4>)> resetAnimationRoot;
    std::function<void(float)> railBalance; // rider+238 current value for the kind5 balance cycles
};
using RiderInteraction=std::function<void(PrototypeRider&)>;
using BodyPoseProvider=std::function<std::optional<BodyCollisionVolume>(const PrototypeRider&,double)>;
// The source-backed riding path coexists with an unseeded inspection fallback.
// Landing, unsupported control states and other remaining gameplay branches
// still require original-state recovery; named kernel parity is not full-game parity.
struct ObstacleContact {
    RayHit hit;
    Vec3 origin,velocityBefore,velocityAfter;
    double probeHeight=0,simulationTime=0;
};
class PrototypeRider {
    RiderControllerCallbacks controllerCallbacks;
    bool detailedBodyQueries=true;
    bool originalBoostSeeded=false;
    OriginalBoostProfile boostProfile;OriginalBoostState boostState;OriginalBoostEffects boostEffects;
    std::optional<OriginalRaceSession> raceSession;
    double accumulator=0;
    bool previousJump=false,sourceAirValid=false,originalJumpSeeded=false;
    double inputAccumulator=0;
    OriginalJumpState jumpState;
    bool originalGroundSeeded=false,originalPoseControlsSeeded=false,originalGroundProfileSeeded=false;
    OriginalGroundProfile groundProfile;
    std::optional<std::array<OriginalGroundMaterial,19>> groundSurfaceCatalog;
    void selectGroundMaterial(int surface) {
        if(groundSurfaceCatalog&&surface>=0&&surface<int(groundSurfaceCatalog->size())) {
            applyGroundMaterial(groundProfile,(*groundSurfaceCatalog)[surface]);groundSurfaceProfileMissing=false;
        } else if(groundProfile.surface.id!=surface) {
            groundProfile.surface.id=surface;groundSurfaceProfileMissing=true;
        }
    }
    OriginalGroundState groundState;
    OriginalGroundDiagnostics groundDiagnostics;
    double airAccumulator=0;
    OriginalAirState sourceAir;
    bool originalAirControlSeeded=false,airPresentationValid=false;
    int sourceControlState=0;
    OriginalAirControlProfile airControlProfile;
    bool originalAirEntrySeeded=false;
    OriginalAirPrewindState airPrewind;
    OriginalPassiveAirState passiveAirState;
    bool originalLandingSeeded=false;
    OriginalLandingProfile landingProfile;
    OriginalLandingRuntime landingRuntime;
    OriginalAnimationEvent animationEvent;
    OriginalAirControlState airControlState;
    OriginalAirPresentation airPresentationCache;
    bool originalAirTrajectorySeeded=false,airTrajectoryQueryFailed=false;
    OriginalAirTrajectory airTrajectory,airAnimationTrajectory;
    OriginalAirAlignmentContext airAlignmentContext;
    OriginalAirTrajectoryQuery airTrajectoryQuery;
    std::function<int(int)> airSurfaceProperty;
    std::array<float,4> airPhysicalQuaternion={0,0,0,1};
    std::array<float,3> airPivot={0,0,0};
    // Original105D98 history and the control8/motion2 crash owners.
    OriginalCollisionProfile collisionProfile;
    OriginalCollisionHistory collisionHistory;
    OriginalCrashControlState crashControl;
    OriginalCrashMotionState crashMotion;
    OriginalCrashActorState crashActor;
    bool crashActive=false;
    float crashPenalty=0;
    // Authored grind rails and the control7 / motion4 owners.
    std::span<const OriginalRailRecord> railRecords;
    OriginalRailMotionState railMotion;OriginalRailControlState railControl;
    bool railActive=false,railAttachedThisTick=false,railStanceRestored=false;
    GroundControlValue railSteer22C,railBalance238,railTolerance25C; // rider+22C/238/25C triplets
    unsigned boardRootBone=22;
    int riderCategoryB20=1;      // Human racer category in the captured fixtures.
    float resetPermission470=-1; // No forced fast-recovery permission.
    static Vec3 tangent(Vec3 vector,Vec3 normal) { return vector-normal*dot(vector,normal); }
    struct FrameContext {
        RiderInput input;
        const CollisionWorld* ground=nullptr;const CollisionWorld* obstacles=nullptr;
        const WorldBodyCollision* bodyWorld=nullptr;const BodyPoseProvider* poseProvider=nullptr;
        const RiderPoseStages* poseStages=nullptr;const RiderInteraction* interaction=nullptr;std::optional<RiderFrameTiming> externalTiming;
        Vec3 startPosition{},next{};
        bool enteredAirControl=false,pressed=false,boostModifierForControl=false;
        bool airMotionThisTick=false,passiveTakeoff=false,localPoseReady=false;
        RiderFramePhase phase=RiderFramePhase::Idle;
    } frame;
    bool pendingSharedFinish=false;
    static constexpr double dt=1.0/60,clearance=.025;
    void beginFrameState(){
        if(originalGroundSeeded&&originalCollisionHistoryDecay(collisionProfile,collisionHistory)&&!crashActive&&!resetRequested){resetRequested=true;resetReason=2;}
        if(frame.externalTiming){
            if(raceSession)throw std::runtime_error("Cannot combine per-rider and shared race clocks");
            landingRuntime.tick=frame.externalTiming->tick;
            if(originalGroundSeeded||originalPoseControlsSeeded)groundState.modeTiming=frame.externalTiming->finishElapsed;
        }
        if(raceSession){
            raceSession->beginTick();
            if(originalGroundSeeded)groundState.modeTiming=raceSession->state.finish.elapsed;
        }
        if(originalLandingSeeded){
            if(raceSession)landingRuntime.tick=uint32_t(raceSession->state.clock.totalTicks);
            jumpState.ticksSinceGroundFocus=landingRuntime.tick-landingRuntime.groundFocusTick;
        }
        if(originalGroundSeeded&&groundProfile.speedLimitTable[0]>0) {
            // Frame-begin11B3F8 runs before control smoothing/motion.
            groundProfile.speedLimit=originalGroundSpeedLimit(groundProfile,groundState,grounded?0:1);
            jumpState.speedLimit=groundProfile.speedLimit;
        }
    }
    void controlFrame(){
        const auto& input=frame.input;auto& enteredAirControl=frame.enteredAirControl;auto& pressed=frame.pressed;
        enteredAirControl=false;
        if(sourceControlState==8){crashControlFrame();airAnimationTrajectory=airTrajectory;return;}
        if(sourceControlState==7){railControlFrame();airAnimationTrajectory=airTrajectory;return;}
        railAttachedThisTick=false;
        //0x131620/0x12E9B8/0x133308: cruise, crouch and air controllers try 0x106848 first.
        if(!railRecords.empty()&&originalGroundSeeded&&(sourceControlState==0||sourceControlState==2||sourceControlState==5)&&tryRailAttach()){airAnimationTrajectory=airTrajectory;return;}
        if(!grounded&&sourceControlState==0&&hasOriginalPoseControls()){
            // Unseeded inspection/translation fixtures do not own the original
            // animation/controller graph. Keep their provisional flight path.
            //131CC0 enters passive-air control4 on the tick after mode0 leaves.
            sourceControlState=groundState.controlState=4;airAlignmentContext.controlState=4;
            airAngularSupported=false;airPresentationValid=false;
            originalPassiveAirBegin(passiveAirState,groundState,airPrewind);
            frame.boostModifierForControl=originalBoostSeeded?boostState.modifier>0:airControlProfile.boostModifier;
            airAnimationTrajectory=airTrajectory;boostEffects={};return; //131700 returns after gain, before update4.
        }
        const int boostControlAtStart=sourceControlState;
        frame.boostModifierForControl=originalBoostSeeded?boostState.modifier>0:airControlProfile.boostModifier;
        if(sourceControlState==4){
            OriginalPassiveAirAccess access;
            //116120 forced reset (control9/motion3) is represented by an explicit
            // respawn request that the application honours; not a silent no-op.
            access.recover=[&](bool requested){if(requested){resetRequested=true;resetReason=1;}return requested;};
            access.upper=[&](bool left,bool right){
                if(controllerCallbacks.upperAction)return controllerCallbacks.upperAction(left,right);
                //1163B0(false,false) can still restore/fade active classes3/13.
                // Only skip it when known animation metadata proves a no-op.
                if(!left&&!right&&controllerCallbacks.upperAnimationClass){
                    int cls=controllerCallbacks.upperAnimationClass();
                    if(cls>=0&&cls!=3&&cls!=13)return false;
                }
                throw std::runtime_error("Passive air upper-action callback missing");
            };
            access.handplant=controllerCallbacks.handplantAction;access.rail=[&](){return tryRailAttach();};
            access.stopBoost=[&](){boostEffects={};if(originalBoostSeeded){boostState.amount=groundState.boost;boostState.window=groundState.boostWindow;boostState.tier=groundState.boostTierCounter;boostEffects=originalBoostControl(boostState,boostProfile,false,false);groundState.boost=boostState.amount;}};
            access.mainAnimation=[&](){auto current=currentMainAnimation();if(!current)throw std::runtime_error("Passive air animation unavailable");return OriginalPassiveAirAnimation{current->semantic,current->animationClass};};
            access.requestAnimation=[&](int semantic,float,uint32_t){requestControllerAnimation(semantic);};
            access.requestControl=[&](int control){
                if(control!=5)throw std::runtime_error("Unsupported passive-air transition");
                int upper=controllerCallbacks.upperAnimationClass?controllerCallbacks.upperAnimationClass():0;
                auto exit=originalPassiveAirLeave(true,upper);
                if(exit.fade){if(!controllerCallbacks.animationFade||!controllerCallbacks.animationRate)throw std::runtime_error("Passive exit requires animation callbacks");controllerCallbacks.animationFade(exit.fadeChannel,exit.fadeSeconds);controllerCallbacks.animationRate(exit.rateChannel,exit.upperRate);}
                sourceControlState=groundState.controlState=5;airAlignmentContext.controlState=5;
                airControlState=originalAirControlBegin(airPrewind.spin.current,airPrewind.flip.current);originalAirControlSeeded=airAngularSupported=true;airControlTicks=0;
                if(controllerCallbacks.airGain)controllerCallbacks.airGain(*this);
                if(airPrewind.spin.current!=0||airPrewind.flip.current!=0){if(!controllerCallbacks.animationRate)throw std::runtime_error("Air entry needs animation-rate callback");controllerCallbacks.animationRate(2,originalAirReleaseAnimationRate(airControlProfile.trickStat,airControlState.spinRate,airControlState.flipRate));}
            };
            originalPassiveAirStep(passiveAirState,groundState,{input.turn,input.crouch,input.passiveInputCode,input.recoverPressed,input.handplant,input.passiveUpper14,input.passiveUpper15},access);
            airAnimationTrajectory=airTrajectory;return;
        }
        if(sourceControlState==3){
            if(!hasOriginalPoseControls())throw std::runtime_error("Soft controller requires original pose controls");
            auto animation=currentMainAnimation();
            if(!animation)throw std::runtime_error("Soft controller requires current animation completion state");
            auto result=originalSoftControlStep(groundProfile,groundState,{grounded?0:1,animation->completed,false},
                {input.turn,input.boardPress,input.boostPressed,input.boostHeld,input.recoverPressed});
            if(result.stop==OriginalSoftControlResult::Stop::Recovery){resetRequested=true;resetReason=1;previousJump=input.jumpHeld;airAnimationTrajectory=airTrajectory;return;}
            if(result.stop!=OriginalSoftControlResult::Stop::None)throw std::runtime_error("Soft controller external finish/obstruction transition is not connected");
            if(result.requestBalance){auto view=railRiderView();originalRailSteerTarget(view,result.balance);railSteer22C=view.steer22C;}
            if(result.resetRail){
                //1326C8 re-enters the rail cycle after a soft impact on the rail.
                if(!railActive)throw std::runtime_error("Soft controller rail reset without an active rail");
                playCrashAnimation(originalRailEntrySemantic(groundState.prewindStyle,false));
                sourceControlState=groundState.controlState=7;airAlignmentContext.controlState=7;
                auto view=railRiderView();originalRailControlBegin(railControl,view);applyRailRider(view);
                previousJump=input.jumpHeld;airAnimationTrajectory=airTrajectory;return;
            }
            boostEffects={};
            if(originalBoostSeeded&&result.requestBoost){
                boostState.amount=groundState.boost;boostState.window=groundState.boostWindow;boostState.tier=groundState.boostTierCounter;
                boostEffects=originalBoostControl(boostState,boostProfile,result.boostHeld,result.boostPressed);groundState.boost=boostState.amount;
            }
            if(result.restoreStance){
                //115640 returns immediately when328 iszero. Nonzero styles
                // need its physical/animation root restoration, not a reset guess.
                if(controllerCallbacks.restoreStance)controllerCallbacks.restoreStance(*this);else restoreStanceNow();
            }
            if(result.nextControl>=0){
                sourceControlState=groundState.controlState=result.nextControl;airAlignmentContext.controlState=result.nextControl;
                if(result.normalGainFocus){airPrewind.jumpGate=0;if(controllerCallbacks.normalGain)controllerCallbacks.normalGain(*this);}
            }
            previousJump=input.jumpHeld;airAnimationTrajectory=airTrajectory;
            return; // Recovery enters0 now; its normal update runs next tick.
        }
        frame.pressed=false;
        if(grounded&&sourceControlState==0){
            pressed=originalAirEntrySeeded?originalCrouchRequest(airPrewind.jumpGate,input.jumpHeld,input.jumpPressed)
                :(input.jumpPressed||(!previousJump&&input.jumpHeld));
        }
        // Control2 releases when held is absent even after a pressed-only pulse.
        bool released=(sourceControlState==2&&!input.jumpHeld)||(grounded&&!originalGroundSeeded&&previousJump&&!input.jumpHeld);
        previousJump=input.jumpHeld;
        if(released) {
            if(grounded) {
            auto source=OriginalAirState::fromNative(position,velocity);
            jumpState.position=source.position;jumpState.velocity=source.velocity;
            jumpState.charge=float(jumpCharge);jumpState.motionMode=0;
            if(!originalJumpSeeded) {
                if(originalGroundSeeded) {
                    jumpState.normal=groundState.normal;jumpState.takeoffNormal=groundState.previousNormal;
                    jumpState.forward=groundState.forward;jumpState.speedLimit=groundProfile.speedLimit;
                } else {
                    Vec3 facing=unit(tangent({std::sin(heading),0,std::cos(heading)},normal));
                    jumpState.normal={float(normal.x),float(-normal.z),float(normal.y)};
                    jumpState.takeoffNormal=jumpState.normal;
                    jumpState.forward={float(facing.x),float(-facing.z),float(facing.y)};
                }
            }
            originalJumpTakeoff(jumpState);
            sourceAir={jumpState.position,jumpState.velocity};
            position=sourceAir.nativePosition();velocity=sourceAir.nativeVelocity();
            sourceAirValid=true;airAccumulator=0;grounded=false;sourceControlState=5;
            if(originalGroundSeeded){
                uint32_t leave=landingRuntime.lastGroundLeaveTick;
                originalLandingGroundLeave(groundState,landingRuntime.tick,leave);
                if(originalLandingSeeded)landingRuntime.lastGroundLeaveTick=leave;
                // Original ground mode0 -> air mode1 entry 0x1399E0 initializes
                // the predictor with its fixed 3333.3335cm/s air limit.
                airTrajectory.begin(sourceAir);originalAirTrajectorySeeded=true;airTrajectoryQueryFailed=false;
                airPhysicalQuaternion=groundState.quaternion;airAlignmentContext={};
                airAlignmentContext.timeScale=groundState.timeScale;
                airAlignmentContext.physicalForward=groundState.physicalForward;
                airAlignmentContext.controlState=5;airAlignmentContext.airModeFlag=0;
                airPhysicalOrientationPending=true;
                originalAirReleaseGroundTargets(groundProfile,groundState,sourceAir.velocity);
            }
            } else {
                //12EAE4..12EB1C skips114298 and motion re-entry when already
                //airborne. Release still enters control5; no second impulse.
                sourceControlState=5;
                if(originalGroundSeeded)originalAirReleaseGroundTargets(groundProfile,groundState,sourceAir.velocity);
            }
            if(originalAirEntrySeeded&&airPrewindSupported){
                airControlState=originalAirControlRelease(airPrewind);
                int releaseSemantic=originalAirReleaseAnimation(airPrewind.spin.current,airPrewind.flip.current,groundState.reverseStance,groundState.prewindStyle);
                ++animationEvent.serial;animationEvent.semantic=releaseSemantic;animationEvent.airExit=animationEvent.reverseTurn=false;
                animationEvent.rate=(airPrewind.spin.current!=0||airPrewind.flip.current!=0)?originalAirReleaseAnimationRate(airControlProfile.trickStat,airControlState.spinRate,airControlState.flipRate):1.f;
                groundState.animationIndex=releaseSemantic;groundState.animationClass=9;groundState.prewindStyle=0;
                originalAirControlSeeded=airAngularSupported=true;airControlTicks=0;enteredAirControl=true;
                if(controllerCallbacks.airGain)controllerCallbacks.airGain(*this);
                auto physical=OriginalAirState::fromNative(position,velocity);
                airPresentationCache=originalAirPresentationCurrent(airControlState,{physical.position,airPhysicalQuaternion},airPivot);
                airPresentationValid=true;
            }
        }
        boostEffects={};
        if(originalBoostSeeded){
            if(originalGroundSeeded||originalPoseControlsSeeded){boostState.amount=groundState.boost;boostState.window=groundState.boostWindow;boostState.tier=groundState.boostTierCounter;}
            if(boostControlAtStart==0&&!pressed&&sourceControlState==0)boostEffects=originalBoostControl(boostState,boostProfile,input.boostHeld,input.boostPressed);
            else if(boostControlAtStart==2&&!released&&sourceControlState==2)boostEffects=originalBoostControl(boostState,boostProfile,input.boostHeld,false);
            else if(boostControlAtStart==5&&!enteredAirControl)boostEffects=originalBoostControl(boostState,boostProfile,false,false);
            if(originalGroundSeeded||originalPoseControlsSeeded)groundState.boost=boostState.amount;
        }
        // Cruise control0->crouch control2 returns before requesting charge on
        // the first press (0x131794/0x13179C); held ticks begin next frame.
        if(originalGroundSeeded&&(grounded||sourceControlState==2)) {
            if(pressed){
                sourceControlState=groundState.controlState=2;
                //12E980 gain: request neutral animation turn before12EE30.
                groundState.animationTurn.target=0;groundState.animationTurn.rate=0.03333333507180214f;
            }
            // Normal controller1319C0/131B6C: animation turn follows the old
            // physical turn with its own slower triplet, before1211F8 runs.
            if(groundState.controlState==0&&groundState.animationClass==7){groundState.animationTurn.target=groundState.brake.current==0?groundState.turn.current:0.f;groundState.animationTurn.rate=0.03333333507180214f;}
            // Original target/rate functions consume effective controls. The
            // first Cross transition preserves the existing crouch request.
            if(!pressed)groundTurnTarget(groundState.turn,input.turn,groundState.velocity,groundProfile.surface.id);
            if(!pressed) groundCrouchBrakeTargets(groundState.crouch,groundState.brake,
                input.jumpHeld?1.f:input.crouch,input.brake,
                float(dot(velocity,{groundState.forward[0],groundState.forward[2],-groundState.forward[1]})*100),groundState.turn.current);
            if(!pressed&&groundState.controlState==0&&controllerCallbacks.normalUpperReaction)controllerCallbacks.normalUpperReaction(*this);
            if(originalAirEntrySeeded&&!pressed&&groundState.controlState==2){
                OriginalAirPrewindContext context{groundState.prewindStyle,groundState.animationClass,groundState.animationIndex,groundState.manualSpin,
                    originalAirReverseTurnRequired(groundState.brake.current,groundState.velocity,groundState.forward)};
                if(!originalAirPrewindTargets(airPrewind,input.spin,input.flip,context))airPrewindSupported=false;
            }
            if(!pressed&&groundState.controlState==0&&groundState.animationIndex!=22){
                OriginalReverseTurnResult reverse;
                if(groundState.animationIndex!=21&&groundState.manualSpin==0)reverse=originalReverseTurn(groundState,groundState.balance280);
                if(reverse.reversed){
                    groundState.animationIndex=21;groundState.animationClass=7;
                    ++animationEvent.serial;animationEvent.semantic=21;animationEvent.rate=1;animationEvent.airExit=false;animationEvent.reverseTurn=true;animationEvent.reverseRootQuaternion=reverse.animationRootQuaternion;
                }
                if(groundState.animationIndex==21){groundState.animationTurn.target=groundState.brake.target=0;groundState.animationTurn.rate=groundState.brake.rate=0.03333333507180214f;}
            }
            originalSelectGroundAnimation(groundProfile,groundState,input,airPrewind.spin.current,airPrewind.flip.current,controllerCallbacks.nextRandom);
            if(controllerCallbacks.requestAnimation&&groundState.animationSelectionSupported){auto current=currentMainAnimation();if(!current||current->semantic!=groundState.animationIndex)requestControllerAnimation(groundState.animationIndex);}
        }
        airAnimationTrajectory=airTrajectory;
        if(originalBoostSeeded)airControlProfile.boostModifier=frame.boostModifierForControl;
            if(sourceControlState==5&&originalAirControlSeeded&&airAngularSupported&&!enteredAirControl){
                airControlProfile.grabLifecycleResolved=bool(controllerCallbacks.airGrab);airControlProfile.grabActive=false;
                if(controllerCallbacks.airGrab)airControlProfile.grabActive=controllerCallbacks.airGrab(*this,input);
                if(auto animation=currentMainAnimation())airControlProfile.landingAnimation=animation->semantic==0x120;
                if((input.grabMask&&!airControlProfile.grabLifecycleResolved)||input.handplant||airControlState.mode==2){
                    // A grab/landing animation state machine must own these
                    // interruptions. Preserve flight translation and flag the gap.
                    airAngularSupported=false;airPresentationValid=false;
                }else {OriginalAirControlFrame values;originalAirControlStep(airControlState,input,airControlProfile,&values);++airControlTicks;if(controllerCallbacks.airAnimation)controllerCallbacks.airAnimation(*this,values);}
            }
    }
    void timerFrame(){
        if(originalBoostSeeded){
            float timeScale=(originalGroundSeeded||originalPoseControlsSeeded)?groundState.timeScale:airAlignmentContext.timeScale;
            auto tickEffects=originalBoostTick(boostState,boostProfile,timeScale,currentMotionMode(),sourceControlState);
            boostEffects.timerExpired=tickEffects.timerExpired;
            airControlProfile.boostModifier=frame.boostModifierForControl;
            if(originalGroundSeeded||originalPoseControlsSeeded){groundState.boost=boostState.amount;groundState.boostWindow=boostState.window;groundState.boostTierCounter=boostState.tier;}
        }
        if(controllerCallbacks.timerUpdate)controllerCallbacks.timerUpdate(*this);
    }
    void filterFrame(){
        const auto& input=frame.input;bool pressed=frame.pressed;
        if(originalGroundSeeded||originalPoseControlsSeeded){
            //1211F8 approaches retained target/rate triplets in all motion modes.
            groundControlApproach(railSteer22C);groundControlApproach(railBalance238);groundControlApproach(railTolerance25C);
            groundControlApproach(groundState.turn);groundControlApproach(groundState.brake);groundControlApproach(groundState.crouch);groundControlApproach(groundState.presentationLift);groundControlApproach(groundState.animationTurn);groundControlApproach(groundState.extraLean);groundControlApproach(groundState.boardAlignment);groundControlApproach(groundState.presentationRoll);groundControlApproach(groundState.balance280);groundControlApproach(groundState.adjustment28C);groundControlApproach(groundState.adjustment298);
            jumpCharge=groundState.crouch.current;
        } else if(!(grounded&&pressed))jumpCharge=originalJumpChargeStep(float(jumpCharge),grounded&&input.jumpHeld);
        if(originalAirEntrySeeded)originalAirPrewindApproach(airPrewind);
        ++jumpState.ticksSinceGroundFocus;
    }
    void motionFrame(){
        const auto& ground=*frame.ground;const auto& obstacles=*frame.obstacles;auto* bodyWorld=frame.bodyWorld;auto& next=frame.next;
        const auto& input=frame.input;auto& passiveTakeoff=frame.passiveTakeoff;
        if(crashActive){crashMotionFrame();position=next;return;}
        if(railActive){railMotionFrame();position=next;return;}
        if(!grounded||!originalGroundSeeded)heading-=double(input.turn)*dt*(grounded?1.45:.8);
        Vec3 forward={std::sin(heading),0,std::cos(heading)};
        Vec3 recoveredNext=position;
        if(grounded&&originalGroundSeeded) {
            sourceAirValid=false;airAccumulator=0;
            auto physical=OriginalAirState::fromNative(position,velocity);
            groundState.position=physical.position;groundState.velocity=physical.velocity;
            groundDiagnostics=originalGroundIntegrate(groundProfile,groundState);originalGroundPresentationTarget(groundProfile,groundState,groundDiagnostics);++groundTicks;
            if(groundProfile.headingProfile.crouchTurnCurve[3].x>groundProfile.headingProfile.crouchTurnCurve[0].x) {
                OriginalHeadingState orientation;
                orientation.relativeVelocity=groundDiagnostics.relativeVelocity;orientation.normal=groundState.normal;
                orientation.forward=groundState.forward;orientation.lateral=groundState.lateral;orientation.bodyForward=groundState.physicalForward;
                orientation.turn=groundState.turn.current;orientation.charge=groundState.crouch.current;orientation.dt=groundDiagnostics.stepTime;
                orientation.manualSpin=groundState.manualSpin;orientation.controlState=groundState.controlState;orientation.reverseStance=groundState.reverseStance;
                float angle=originalGroundHeading(groundProfile.headingProfile,orientation);groundState.manualSpin=orientation.manualSpin;
                if(angle!=0) {
                    auto q=originalRotateOrientation(groundState.quaternion,groundState.normal,angle);
                    auto rebuilt=originalRebuildOrientation(q);groundState.quaternion=rebuilt.quaternion;
                    groundState.physicalForward=rebuilt.forward;groundState.boardUp=rebuilt.up;
                }
            }
            physical={groundState.position,groundState.velocity};recoveredNext=physical.nativePosition();velocity=physical.nativeVelocity();
        } else if (grounded) {
            sourceAirValid=false;airAccumulator=0;
            forward=unit(tangent(forward,normal));
            velocity=tangent(velocity,normal);
            Vec3 gravity=tangent({0,-9.81,0},normal);
            double speed=std::sqrt(dot(velocity,velocity));
            // Carve toward board heading, keeping gravity able to produce a slide.
            double along=dot(velocity,forward);
            Vec3 sideways=velocity-forward*along;
            velocity=velocity-sideways*(1-std::exp(-dt*5));
            velocity=velocity+(gravity+forward*(input.crouch*4+(input.boostHeld?10:0)))*dt;
            double nextSpeed=std::sqrt(dot(velocity,velocity));
            double drag=(.15+speed*.035+input.brake*11)*dt;
            velocity=nextSpeed>drag?velocity*((nextSpeed-drag)/nextSpeed):Vec3{};
        }
        frame.airMotionThisTick=!grounded;
        if (!grounded) {
            if (!sourceAirValid) {sourceAir=OriginalAirState::fromNative(position,velocity);sourceAirValid=true;airAccumulator=0;}
            airAccumulator+=dt;
            if (airAccumulator+1e-12<1.0/60) throw std::runtime_error("Incomplete air step in fixed60Hz frame");
            airAccumulator-=1.0/60;
            lastBodyVolume.reset();lastBodyQuery={};lastBodyResponse={};
            bodyCollisionMissingWorld=!bodyWorld;bodyCollisionMissingPose=true;
            groundState.normal=groundState.presentationUp;groundState.surfaceVelocity={}; //139A70..8C, retained pre-motion180.
            if(originalAirTrajectorySeeded&&!airTrajectoryQueryFailed&&airSurfaceProperty&&(airTrajectoryQuery||bodyWorld)){
                try {
                    auto query=airTrajectoryQuery;
                    if(!query)query=[&](auto end,auto start,int mode){return queryOriginalAirTrajectoryWorld(ground,bodyWorld,end,start,mode);};
                    auto candidate=airTrajectory;
                    auto nextAir=candidate.stepLogic(airAlignmentContext.timeScale,sourceAir,query);
                    auto context=airAlignmentContext;
                    context.normal=candidate.normal;context.heading=candidate.heading;
                    context.predictedTime=candidate.predictedTime;context.elapsedTime=candidate.elapsed;
                    context.trajectoryStatus=candidate.status;context.surfaceIndex=candidate.surface;
                    context.surfaceFlags=candidate.patchFlags;
                    if(candidate.status==1||candidate.status==3)context.surfaceProperty44=airSurfaceProperty(candidate.surface);
                    if(originalAirControlSeeded)context.adjustSpin=airControlState.adjustSpin;
                    auto orientation=originalAirAlignmentStage(airPhysicalQuaternion,context);
                    context.physicalForward=orientation.forward;
                    airTrajectory=std::move(candidate);sourceAir=nextAir;airAlignmentContext=context;
                    airPhysicalQuaternion=orientation.quaternion;airPhysicalOrientationPending=false;
                }catch(const OriginalAirTrajectoryUnavailable&){
                    airPhysicalOrientationPending=true;airTrajectoryQueryFailed=true;airSpeedCapped=sourceAir.step();
                }
            }else airSpeedCapped=sourceAir.step();
            ++airborneTicks;
            next=sourceAir.nativePosition();velocity=sourceAir.nativeVelocity();
            if(originalAirControlSeeded&&airAngularSupported){
                airPresentationCache=originalAirPresentation(airControlState,{sourceAir.position,airPhysicalQuaternion},airPivot);
                airPresentationValid=true;
            }
        } else next=originalGroundSeeded?recoveredNext:position+velocity*dt;
        double speed=std::sqrt(dot(velocity,velocity));
        if (speed>55) velocity=velocity*(55/speed);
        Vec3 travel=next-frame.startPosition;
        double distance=std::sqrt(dot(travel,travel));
        obstacleContact=false;
        if (!originalGroundSeeded && distance>1e-9) {
            // Torso/board swept rays plus radius margin. Not a complete capsule;
            // original collision classifications, rails and complex contacts remain.
            for (double height:{.2,.65,1.1}) {
                auto hit=obstacles.raycast(frame.startPosition+Vec3{0,height,0},travel,distance+.22);
                if (hit.hit && hit.normal.y<.45 && dot(velocity,hit.normal)<0) {
                    double safe=std::max(0.0,hit.distance-.22);
                    next=frame.startPosition+unit(travel)*std::min(safe,distance);
                    auto before=velocity;
                    velocity=tangent(velocity,hit.normal)*.4;
                    obstacleContacts.push_back({hit,position+Vec3{0,height,0},before,velocity,height,elapsed});
                    obstacleContact=true;break;
                }
            }
        }
        if(grounded&&originalGroundSeeded) {
            Vec3 previousNormal={groundState.normal[0],groundState.normal[2],-groundState.normal[1]};
            Vec3 previousLateral={groundState.lateral[0],groundState.lateral[2],-groundState.lateral[1]};
            groundState.previousNormal=groundState.normal; // Original rider+0x380 receives old+0x370 before querying.
            auto previousSurfaceVelocity=groundState.surfaceVelocity;
            auto hit=ground.sourceGroundContact(next,previousNormal,previousLateral,groundState.turn.current,groundProfile.bodyScale,&terrainContactCache);
            int selectedSurface=ssx::originalGroundSurfaceId(hit.hit,hit.surface);
            lastGroundContact=hit;
            auto physical=OriginalAirState::fromNative(next,velocity);groundState.position=physical.position;groundState.velocity=physical.velocity;
            passiveTakeoff=!hit.hit;
            if(hit.hit) {
                auto contact=OriginalAirState::fromNative(hit.position,{0,0,0});
                std::array<float,3> n={float(hit.normal.x),float(-hit.normal.z),float(hit.normal.y)};
                originalGroundContact(groundState,contact.position,n,groundState.surfaceVelocity);
                groundState.distance=hit.contactSignedDistanceCm;groundState.contactClearance=hit.contactLateralDistanceCm;
                passiveTakeoff=groundProfile.airHeight>0&&groundState.flags308&&groundState.distance>groundProfile.airHeight;
            } else {
                // Original no-contact branch13D328..13D424 resets the frame to
                // world-up but retains signed distance and previous normal380.
                float distanceBefore=groundState.distance;
                originalGroundContact(groundState,groundState.position,{0,0,1},{0,0,0});
                groundState.distance=distanceBefore;groundState.contactClearance=0;
            }
            originalGroundVelocityContact(groundState,selectedSurface,previousSurfaceVelocity);
            bool rotated=false;auto q=groundState.quaternion;
            if(!passiveTakeoff)q=originalGroundAlignment(q,groundState.normal,groundState.boardUp,
                groundState.contactClearance,groundProfile.alignmentRate,&rotated);
            if(rotated)q=originalRebuildOrientation(q).quaternion; // 11DFE0 calls11E098.
            auto rebuilt=originalRebuildOrientation(q); // Final unconditional11E098 in cruise.
            groundState.quaternion=rebuilt.quaternion;groundState.physicalForward=rebuilt.forward;groundState.boardUp=rebuilt.up;
            // 13D818 retains its old surface pointer through forces, passive
            // height (+10) and alignment (+38); 13D1B8 has already changed +438.
            // Publish the new material now for second phase/next-frame forces,
            // preserving the speed limit cached at this frame's beginning.
            selectGroundMaterial(selectedSurface);
            {
                terrain_original::Rounding rounding;
                auto relative=terrain_original::difference(groundState.velocity,groundState.surfaceVelocity);
                originalGroundVisualTargets(groundProfile,groundState,groundDiagnostics.stepTime,relative);
            }
            // Cruise tail13F068 updates the procedural board oscillator before
            // second-phase body pose/collision. Ground surfaces currently static.
            if(!passiveTakeoff){std::array<float,3> relative;for(unsigned k=0;k<3;++k)relative[k]=groundState.velocity[k]-groundState.surfaceVelocity[k];originalGroundBoardLift(groundState,relative);}
            physical={groundState.position,groundState.velocity};next=physical.nativePosition();velocity=physical.nativeVelocity();
        }
        position=next;
    }
    void localPoseFrame(){
        if(controllerCallbacks.railBalance)controllerCallbacks.railBalance(railBalance238.current);
        frame.localPoseReady=frame.poseStages&&frame.poseStages->prepareLocal&&frame.poseStages->prepareLocal(*this,elapsed+dt);
    }
    void worldPoseFrame(){
        bodyCollisionMissingWorld=!frame.bodyWorld;bodyCollisionMissingPose=true;
        lastBodyQuery={};lastBodyResponse={};lastBodyVolume.reset();
        auto* provider=frame.poseProvider;
        if(frame.poseStages) {
            if(frame.localPoseReady&&frame.poseStages->finishWorld)lastBodyVolume=frame.poseStages->finishWorld(*this,elapsed+dt);
        } else if(provider)lastBodyVolume=(*provider)(*this,elapsed+dt);
        bodyCollisionMissingPose=!lastBodyVolume.has_value();
        if(lastBodyVolume){
            if(lastBodyVolume->reactionFrame)groundState.presentationUp=lastBodyVolume->reactionFrame->up;
            if(lastBodyVolume->mainAnimation){groundState.animationIndex=lastBodyVolume->mainAnimation->semantic;groundState.animationClass=lastBodyVolume->mainAnimation->animationClass;}
            if(frame.airMotionThisTick&&lastBodyVolume->airPivotCm&&originalAirControlSeeded&&airAngularSupported){
                airPivot=*lastBodyVolume->airPivotCm;airPresentationCache=originalAirPresentationCurrent(airControlState,{sourceAir.position,airPhysicalQuaternion},airPivot);
            }
        }
    }
    void contactFrame(){
        const auto& ground=*frame.ground;auto* bodyWorld=frame.bodyWorld;auto& next=frame.next;
        bool airMotionThisTick=frame.airMotionThisTick;bool passiveTakeoff=frame.passiveTakeoff;OriginalAirState physical;
        if(crashActive){crashContactFrame();return;}
        if(railActive){railContactFrame();return;}
        if(!frame.airMotionThisTick&&originalGroundSeeded) {
            if(passiveTakeoff) {
                // Original second phase13F194..13F1A0 invokes negative-charge
                // takeoff; airborne integration starts on the following tick.
                jumpState.position=groundState.position;jumpState.velocity=groundState.velocity;jumpState.charge=-1;
                jumpState.motionMode=0;jumpState.normal=groundState.normal;jumpState.takeoffNormal=groundState.previousNormal;
                jumpState.forward=groundState.forward;jumpState.speedLimit=groundProfile.speedLimit;
                originalJumpTakeoff(jumpState);
                groundState.position=jumpState.position;groundState.velocity=jumpState.velocity;
                // Motion remains ground mode through the final body query;
                // original13F2B0 switches to airborne only after that phase.
            }
            if(bodyWorld&&lastBodyVolume) {
                const auto& body=lastBodyVolume;
                lastBodyQuery=bodyWorld->query(*body,groundState.normal,nullptr,detailedBodyQueries,2,&bodyContactCache);
                    if(lastBodyQuery.best.hit) {
                        const auto& hit=lastBodyQuery.best;
                        lastBodyResponse=originalObstacleResponse(hit.penetrationCm,groundState.normal,hit.normal,groundState.velocity,hit.surfaceVelocityCmps);
                        if(lastBodyResponse.accepted) {
                            terrain_original::Rounding rounding;
                            auto before=velocity;
                            for(unsigned k=0;k<3;++k) {
                                groundState.position[k]=terrain_original::add(groundState.position[k],lastBodyResponse.translationCm[k]);
                                lastBodyVolume->broadCenterCm[k]=terrain_original::add(lastBodyVolume->broadCenterCm[k],lastBodyResponse.translationCm[k]);
                                for(unsigned i=0;i<lastBodyVolume->count;++i)lastBodyVolume->spheres[i].centerCm[k]=terrain_original::add(lastBodyVolume->spheres[i].centerCm[k],lastBodyResponse.translationCm[k]);
                            }
                            groundState.velocity=lastBodyResponse.velocityCmps;
                            OriginalAirState after{groundState.position,groundState.velocity};
                            auto contact=OriginalAirState{hit.pointCm,{}}.nativePosition();
                            RayHit reported;reported.hit=true;reported.position=contact;reported.normal={hit.normal[0],hit.normal[2],-hit.normal[1]};reported.distance=hit.penetrationCm/100;reported.resource=hit.instance;
                            obstacleContacts.push_back({reported,position,before,after.nativeVelocity(),-1,elapsed});obstacleContact=true;
                            if(lastBodyResponse.bounced) {
                                auto contactOrientation=originalObstacleOrientation(groundState.quaternion,groundState.boardUp,groundState.physicalForward,lastBodyResponse.normal);
                                auto rebuiltAfterImpact=originalRebuildOrientation(contactOrientation); // Original13F7D0 always calls11E098.
                                groundState.quaternion=rebuiltAfterImpact.quaternion;groundState.physicalForward=rebuiltAfterImpact.forward;groundState.boardUp=rebuiltAfterImpact.up;
                                obstacleOrientationPending=false;
                                dispatchCollisionEvent(hit,lastBodyResponse,before,false);
                            }
                        }
                    }
            }
            if(crashActive){OriginalAirState current{crashActor.position,crashActor.velocity};next=current.nativePosition();velocity=current.nativeVelocity();position=next;return;}
            if(frame.interaction){
                OriginalAirState current{groundState.position,groundState.velocity};position=current.nativePosition();velocity=current.nativeVelocity();
                (*frame.interaction)(*this);
            }
            originalGroundBoardNormal(groundState); // Original late13F354, after body query.
            originalGroundClampSpeed(groundState,groundProfile.speedLimit); // Second phase13F358..13F3A8.
            physical={groundState.position,groundState.velocity};next=physical.nativePosition();velocity=physical.nativeVelocity();
            if(passiveTakeoff){
                uint32_t leave=landingRuntime.lastGroundLeaveTick;
                originalLandingGroundLeave(groundState,landingRuntime.tick,leave);
                if(originalLandingSeeded)landingRuntime.lastGroundLeaveTick=leave;
                sourceAir=physical;sourceAirValid=true;airAccumulator=0;grounded=false;
                airTrajectory.begin(sourceAir);originalAirTrajectorySeeded=true;airTrajectoryQueryFailed=false;
                airPhysicalQuaternion=groundState.quaternion;airAlignmentContext={};airAlignmentContext.timeScale=groundState.timeScale;
                airAlignmentContext.physicalForward=groundState.physicalForward;airAlignmentContext.controlState=5;airAlignmentContext.airModeFlag=0;
                airPhysicalOrientationPending=true;
            }
            heading=std::atan2(double(groundState.physicalForward[0]),-double(groundState.physicalForward[1]));
            normal={groundState.normal[0],groundState.normal[2],-groundState.normal[1]};
        } else if (grounded) {
            auto hit=ground.raycast(next+Vec3{0,.65,0},{0,-1,0},1.3);
            if (hit.hit && hit.normal.y>.2) {
                next=hit.position+Vec3{0,clearance,0};
                normal=hit.normal;velocity=tangent(velocity,normal);
            } else grounded=false;
        } else if(originalLandingSeeded) {
            //139C88 uses the posed board probe and retained source864 cache;
            // its fraction gate is independent of actor travel this tick.
            lastLandingContact={};landingTransitionPending=false;
            if(lastBodyVolume&&lastBodyVolume->landingCenterCm&&lastBodyVolume->reactionFrame){
                lastLandingContact=originalLandingContact(ground,bodyWorld,
                    {*lastBodyVolume->landingCenterCm,lastBodyVolume->reactionFrame->up},&terrainContactCache);
                if(!lastLandingContact.complete)landingTransitionPending=true;
                else {
                    OriginalLandingState touchdown;touchdown.rider=groundState;
                    touchdown.rider.position=sourceAir.position;touchdown.rider.velocity=sourceAir.velocity;
                    auto physical=originalRebuildOrientation(airPhysicalQuaternion);
                    touchdown.rider.quaternion=airPhysicalQuaternion;touchdown.physicalRight=physical.right;
                    touchdown.rider.physicalForward=physical.forward;touchdown.rider.boardUp=physical.up;
                    auto impact=originalLandingImpact(touchdown,lastLandingContact);
                    if(impact.contact){
                        const auto& hit=lastLandingContact;
                        lastLandingNormalSpeedCmps=impact.relativeNormalSpeed;
                        if(!originalGroundProfileSeeded||hit.surface<0||hit.surface>=int(landingProfile.materials.size()))
                            throw std::runtime_error("Original touchdown has no retained ground/material profile");
                        bool exitAir=sourceControlState==5;
                        if(exitAir){
                            if(!originalAirControlSeeded||!airAngularSupported)throw std::runtime_error("Original touchdown requires supported air-control exit");
                            OriginalAirExitState exit;exit.physical={sourceAir.position,airPhysicalQuaternion};
                            exit.adjustment28C=groundState.adjustment28C;exit.adjustment298=groundState.adjustment298;
                            originalAirControlExit(airControlState,airPrewind,exit,airPivot);
                            touchdown.rider.adjustment28C=exit.adjustment28C;touchdown.rider.adjustment298=exit.adjustment298;
                            touchdown.rider.position=exit.physical.position;touchdown.rider.quaternion=exit.physical.quaternion;
                            touchdown.physicalRight=exit.right;touchdown.rider.physicalForward=exit.forward;touchdown.rider.boardUp=exit.up;
                            touchdown.rider.manualSpin=airControlState.spinRate;
                        }
                        const auto& material=landingProfile.materials[hit.surface];
                        if(!originalLandingResolveContact(touchdown,hit,material,impact.relativeNormalSpeed))
                            throw std::runtime_error("Original touchdown entered unimplemented recovery-surface state");
                        int animationClass=landingRuntime.animationClass;uint64_t animationFlags=landingRuntime.animationFlags;
                        if(lastBodyVolume->mainAnimation){animationClass=lastBodyVolume->mainAnimation->animationClass;animationFlags=lastBodyVolume->mainAnimation->flags;}
                        else throw std::runtime_error("Original touchdown has no current animation classification");
                        OriginalLandingClassification classification{animationClass,animationFlags,landingRuntime.manualState330,landingProfile.landingStat,0};
                        auto choice=originalLandingClassify(touchdown,classification);
                        if(choice.consumedRandom){
                            //13A2xx draws317810 only on this branch; the choice is re-evaluated with the drawn word.
                            if(!controllerCallbacks.nextRandom)throw std::runtime_error("Original landing crash selection requires the shared RNG");
                            classification.randomWord=controllerCallbacks.nextRandom();choice=originalLandingClassify(touchdown,classification);
                        }
                        if(choice.crashAnimation!=0x1b6){
                            //13A4C8..:10EB30 receives the crash clip and landing contact instead of13C7A8 ground entry.
                            groundState=touchdown.rider;groundState.controlState=sourceControlState;selectGroundMaterial(hit.surface);
                            lastLandingTick=landingRuntime.tick;landingScoreEventPending=false;
                            OriginalCollisionEvent event;event.pointCm=hit.position;event.normal=hit.normal;event.closingSpeedCmps=-impact.relativeNormalSpeed;event.surface=hit.surface;
                            if(airSurfaceProperty)event.surfaceProperty44=airSurfaceProperty(hit.surface);
                            auto direction=touchdown.rider.velocity;float length=std::sqrt(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]);
                            if(length>0)for(auto& x:direction)x/=length;event.incomingDirection=direction;
                            enterHardCrash(choice.crashAnimation,event,0);
                            OriginalAirState current{crashActor.position,crashActor.velocity};next=current.nativePosition();velocity=current.nativeVelocity();position=next;
                            return;
                        }
                        if(sourceControlState==7){
                            // Touchdown before 132770 handed the rail controller to 4/5: leave
                            // control7 (132048 balance reset) and continue as a passive-air landing.
                            auto view=railRiderView();originalRailControlLeave(view);railBalance238=view.balance;
                            restoreStanceNow();
                            sourceControlState=groundState.controlState=4;airAlignmentContext.controlState=4;
                        }
                        if(landingRuntime.manualState330!=0||groundState.prewindStyle!=0||(sourceControlState!=5&&sourceControlState!=4&&sourceControlState!=2))
                            throw std::runtime_error("Original touchdown control/manual continuation is not yet recovered (control "+std::to_string(sourceControlState)+", style "+std::to_string(groundState.prewindStyle)+", manual330 "+std::to_string(landingRuntime.manualState330)+")");
                        originalLandingGroundEnter(touchdown.rider,material,landingProfile.bodyScale,landingRuntime.tick,landingRuntime.lastGroundLeaveTick);
                        //13A594..13A5D0 keeps held-jump control2 on touchdown.
                        //Only a hard impact requests a landing clip; a small
                        //ledge preserves the current crouch/prewind animation.
                        bool heldLanding=sourceControlState==2;
                        OriginalReverseTurnResult reverse;
                        if(!heldLanding)reverse=originalReverseTurn(touchdown.rider,touchdown.rider.balance280);
                        bool playLanding=!heldLanding||impact.relativeNormalSpeed<-1388.888916015625f;
                        int semantic=playLanding?(reverse.reversed?originalReverseLandingAnimation(impact.relativeNormalSpeed,touchdown.rider.manualSpin):originalLandingAnimation(impact.relativeNormalSpeed,touchdown.rider.manualSpin)):-1;
                        groundState=touchdown.rider;groundState.controlState=sourceControlState=heldLanding?2:0;
                        if(playLanding){groundState.animationIndex=semantic;groundState.animationClass=10;}
                        if(!heldLanding){airPrewind.jumpGate=0;if(controllerCallbacks.normalGain)controllerCallbacks.normalGain(*this);}
                        selectGroundMaterial(hit.surface);originalGroundSeeded=true;grounded=true;
                        landingRuntime.groundFocusTick=landingRuntime.tick;jumpState.ticksSinceGroundFocus=0;
                        airTrajectory.predictedTime=0;airPhysicalQuaternion=groundState.quaternion;
                        airControlState.spinRate=groundState.manualSpin;
                        sourceAir={groundState.position,groundState.velocity};next=sourceAir.nativePosition();velocity=sourceAir.nativeVelocity();
                        normal={groundState.normal[0],groundState.normal[2],-groundState.normal[1]};
                        heading=std::atan2(double(groundState.physicalForward[0]),-double(groundState.physicalForward[1]));
                        ++animationEvent.serial;animationEvent.semantic=semantic;animationEvent.rate=1;animationEvent.airExit=exitAir;animationEvent.reverseTurn=reverse.reversed;animationEvent.reverseRootQuaternion=reverse.animationRootQuaternion;
                        landingScoreEventPending=true;lastLandingTick=landingRuntime.tick;
                        //10E910's award callbacks remain a distinct score event;
                        // no boost-meter award is invented by motion recovery.
                    }
                }
            }else landingTransitionPending=true;
        } else {
            // Continuous segment against terrain prevents tunneling on fast falls.
            Vec3 movement=next-frame.startPosition;
            double length=std::sqrt(dot(movement,movement));
            auto hit=ground.raycast(frame.startPosition,movement,length+clearance);
            if (hit.hit && hit.normal.y>.2 && dot(velocity,hit.normal)<0) {
                next=hit.position+Vec3{0,clearance,0};normal=hit.normal;
                velocity=tangent(velocity,normal);grounded=true;jumpCharge=0;jumpState.ticksSinceGroundFocus=0;
            }
        }
        if(airMotionThisTick&&originalLandingSeeded&&bodyWorld&&lastBodyVolume&&lastBodyVolume->reactionFrame){
            //13AA48 is the air second phase even on a touchdown tick. Its
            // source AA0 spheres and presentation180 remain the cached pose.
            lastBodyQuery=bodyWorld->query(*lastBodyVolume,lastBodyVolume->reactionFrame->up,nullptr,detailedBodyQueries,2,&bodyContactCache);
            if(lastBodyQuery.best.hit){
                const auto& hit=lastBodyQuery.best;
                lastBodyResponse=originalAirObstacleResponse(hit.penetrationCm,hit.normal,sourceAir.velocity);
                if(lastBodyResponse.accepted){
                    terrain_original::Rounding rounding;auto before=velocity;
                    for(unsigned k=0;k<3;++k){
                        sourceAir.position[k]=terrain_original::add(sourceAir.position[k],lastBodyResponse.translationCm[k]);
                        lastBodyVolume->broadCenterCm[k]=terrain_original::add(lastBodyVolume->broadCenterCm[k],lastBodyResponse.translationCm[k]);
                        for(unsigned i=0;i<lastBodyVolume->count;++i)lastBodyVolume->spheres[i].centerCm[k]=terrain_original::add(lastBodyVolume->spheres[i].centerCm[k],lastBodyResponse.translationCm[k]);
                    }
                    sourceAir.velocity=lastBodyResponse.velocityCmps;next=sourceAir.nativePosition();velocity=sourceAir.nativeVelocity();
                    RayHit reported;reported.hit=true;reported.position=OriginalAirState{hit.pointCm,{}}.nativePosition();
                    reported.normal={hit.normal[0],hit.normal[2],-hit.normal[1]};reported.distance=hit.penetrationCm/100;reported.resource=hit.instance;
                    obstacleContacts.push_back({reported,position,before,velocity,-1,elapsed});obstacleContact=true;
                    if(lastBodyResponse.bounced){
                        auto rebuilt=originalRebuildOrientation(airPhysicalQuaternion);airPhysicalQuaternion=rebuilt.quaternion;
                        if(grounded){groundState.quaternion=rebuilt.quaternion;groundState.physicalForward=rebuilt.forward;groundState.boardUp=rebuilt.up;}
                        obstacleOrientationPending=false;
                        dispatchCollisionEvent(hit,lastBodyResponse,before,true);
                        if(crashActive){OriginalAirState current{crashActor.position,crashActor.velocity};next=current.nativePosition();velocity=current.nativeVelocity();position=next;return;}
                    }
                    if(grounded){groundState.position=sourceAir.position;groundState.velocity=sourceAir.velocity;}
                }
            }
        }
        position=next;
        if(airMotionThisTick&&frame.interaction){(*frame.interaction)(*this);next=position;}
        if(originalAirControlSeeded){
            if(grounded){airAngularSupported=false;airPresentationValid=false;}
            else if(airAngularSupported){
                auto physical=OriginalAirState::fromNative(position,velocity);
                airPresentationCache=originalAirPresentationCurrent(airControlState,{physical.position,airPhysicalQuaternion},airPivot);
                airPresentationValid=true;
            }
        }
        if (grounded) {sourceAirValid=false;airAccumulator=0;originalJumpSeeded=false;}
        else if (obstacleContact) sourceAir=OriginalAirState::fromNative(position,velocity);
    }
    void eventFrame(){
        if(controllerCallbacks.frameComplete)controllerCallbacks.frameComplete(*this);
        if(raceSession){
            auto source=OriginalAirState::fromNative(position,velocity);
            raceSession->endTick(source.position,source.velocity);
        }
        if(!frame.externalTiming&&originalLandingSeeded){
            landingRuntime.tick=raceSession?uint32_t(raceSession->state.clock.totalTicks):landingRuntime.tick+1;
            jumpState.ticksSinceGroundFocus=landingRuntime.tick-landingRuntime.groundFocusTick;
        }
        if(frame.externalTiming)pendingSharedFinish=true;else elapsed+=dt;
    }

    //115640: restore a sideways/backward stance (rider+328) after rails and soft impacts.
    void restoreStanceNow(){
        if(groundState.prewindStyle==0)return;
        OriginalStanceRestoreState state;state.prewindStyle=groundState.prewindStyle;state.motionMode=currentMotionMode();
        state.physical=originalOrientationBasis(grounded?groundState.quaternion:airPhysicalQuaternion);
        OriginalStanceRestoreCallbacks cb;
        cb.physicalChanged=[&](const OriginalPhysicalOrientation& o){groundState.quaternion=airPhysicalQuaternion=o.quaternion;groundState.physicalForward=o.forward;groundState.boardUp=o.up;};
        cb.rotateSequenceRoots=[&](std::array<float,4> q){if(!controllerCallbacks.rotateAnimationRootQuaternion)throw std::runtime_error("Stance restoration requires animation root rotation");controllerCallbacks.rotateAnimationRootQuaternion(q);};
        cb.resetDefaultRoot=[&](std::array<float,3> p,std::array<float,4> q){if(!controllerCallbacks.resetAnimationRoot)throw std::runtime_error("Stance restoration requires the animation root");controllerCallbacks.resetAnimationRoot(p,q);};
        cb.requestAnimation=[&](int semantic,float,uint32_t){playCrashAnimation(semantic);};
        originalRestoreStance(state,cb);groundState.prewindStyle=state.prewindStyle;
    }
    // ---- Original grind rails (control7 / motion4) integration ----
    float boostLevel2FC()const{
        // rider+2FC boost level; represented from the114130 tier (0/.25/.625/1).
        if(!originalBoostSeeded)return 0;switch(boostState.tier){case 0:return 0;case 1:return .25f;case 2:return .625f;default:return 1;}
    }
    OriginalRailRider railRiderView(){
        OriginalRailRider r;
        bool air=!grounded&&!railActive;
        r.position=air&&sourceAirValid?sourceAir.position:groundState.position;r.velocity=air&&sourceAirValid?sourceAir.velocity:groundState.velocity;
        if(!air||!sourceAirValid){auto physical=OriginalAirState::fromNative(position,velocity);r.position=physical.position;r.velocity=physical.velocity;}
        r.quaternion=air?airPhysicalQuaternion:groundState.quaternion;
        auto basis=originalOrientationBasis(r.quaternion);r.right=basis.right;r.forward=basis.forward;r.up=basis.up;
        r.contactNormal=groundState.normal;r.surfaceForward=groundState.forward;r.lateral=groundState.lateral;r.surfaceVelocity=groundState.surfaceVelocity;r.balance280=groundState.balance280;
        r.contactPoint=OriginalAirState::fromNative(lastGroundContact.position,{}).position;
        r.speedLimit=groundProfile.speedLimit;r.timeScale=groundState.timeScale;r.boost=boostLevel2FC();
        r.turn=groundState.turn;r.animationTurn=groundState.animationTurn;r.extraLean=groundState.extraLean;r.brake=groundState.brake;r.crouch=groundState.crouch;
        r.steer22C=railSteer22C;r.balance=railBalance238;r.presentationRoll=groundState.presentationRoll;r.tolerance25C=railTolerance25C;
        r.manualSpin2DC=groundState.manualSpin;r.style=groundState.prewindStyle;r.reverseStance=groundState.reverseStance;
        r.state324=groundState.state320Equals324?groundState.reverseStance:!groundState.reverseStance;
        r.flag330=landingRuntime.manualState330;r.surfaceId=groundProfile.surface.id;r.controlState=sourceControlState;r.motionMode=currentMotionMode();
        if(controllerCallbacks.posedBoneTransform){if(auto bone=controllerCallbacks.posedBoneTransform(boardRootBone)){r.bonePosition=bone->position;r.boneQuaternion=bone->rotation;}}
        return r;
    }
    void applyRailRider(const OriginalRailRider& r){
        groundState.position=r.position;groundState.velocity=r.velocity;groundState.quaternion=airPhysicalQuaternion=r.quaternion;
        auto basis=originalRebuildOrientation(r.quaternion);groundState.physicalForward=basis.forward;groundState.boardUp=basis.up;
        groundState.normal=r.contactNormal;groundState.surfaceVelocity=r.surfaceVelocity;groundState.forward=r.surfaceForward;groundState.lateral=r.lateral;groundState.balance280=r.balance280; //0x115168 negates +0x3A0/+0x3B0/+0x280
        groundState.turn=r.turn;groundState.animationTurn=r.animationTurn;groundState.extraLean=r.extraLean;groundState.brake=r.brake;groundState.crouch=r.crouch;groundState.presentationRoll=r.presentationRoll;
        railSteer22C=r.steer22C;railBalance238=r.balance;railTolerance25C=r.tolerance25C;
        groundState.manualSpin=r.manualSpin2DC;groundState.prewindStyle=r.style;groundState.reverseStance=bool(r.reverseStance);
        groundState.state320Equals324=(bool(r.reverseStance)==bool(r.state324));
        if(r.surfaceId!=groundProfile.surface.id)selectGroundMaterial(r.surfaceId);
        OriginalAirState current{r.position,r.velocity};position=current.nativePosition();velocity=current.nativeVelocity();frame.next=position;
        sourceAir=current;sourceAirValid=true;
        normal={r.contactNormal[0],r.contactNormal[2],-r.contactNormal[1]};
        heading=std::atan2(double(basis.forward[0]),-double(basis.forward[1]));
    }
    OriginalRailAccess railAccess(){
        OriginalRailAccess a;
        a.query=[&](RailVector point){return originalRailWorldQuery(railRecords,point);};
        a.channel2Class=[&](){auto current=currentMainAnimation();return current?current->animationClass:0;};
        a.channel2SequenceFlag=[&](unsigned bit){auto current=currentMainAnimation();return current&&(current->flags&(uint64_t(1)<<bit));};
        a.currentSemantic=[&](){auto current=currentMainAnimation();return current?current->semantic:438;};
        a.playAnimation=[&](int semantic,float,int){playCrashAnimation(semantic);};
        a.rotateAnimation=[&](float radians){if(!controllerCallbacks.rotateAnimationRoot)throw std::runtime_error("Rail stance alignment requires animation root rotation");controllerCallbacks.rotateAnimationRoot(radians);};
        a.setAnimationRoot=[&](float half){if(!controllerCallbacks.setAnimationRootHalfAngle)throw std::runtime_error("Rail stance alignment requires the animation root");controllerCallbacks.setAnimationRootHalfAngle(half);};
        a.setAnimationSwitch=[&](int mirror){if(controllerCallbacks.setAnimationMirror)controllerCallbacks.setAnimationMirror(mirror!=0);};
        a.requestControl=[&](int control){sourceControlState=groundState.controlState=control;airAlignmentContext.controlState=control;};
        a.requestMotion=[&](int motion){
            if(motion==4){railActive=true;grounded=true;airAngularSupported=false;airPresentationValid=false;}
            else if(motion==1){railActive=false;leaveRailToAir();}
            else throw std::runtime_error("Rail requested unsupported motion "+std::to_string(motion));
        };
        a.railEntryScore=[&](bool,int style,int){if(controllerCallbacks.railScoreEvent)controllerCallbacks.railScoreEvent(1,float(style));return 0.f;}; //119D40 scoring is recorded, not computed.
        a.awardScore=[&](float value){if(controllerCallbacks.railScoreEvent)controllerCallbacks.railScoreEvent(2,value);};
        a.airborneRailEvent=[&](int style,int,float speed,const OriginalRailQueryResult&){if(controllerCallbacks.railScoreEvent)controllerCallbacks.railScoreEvent(3,float(style)+speed*0);};
        a.recordRailSurface=[](int){};
        a.balanceStat=[&](){return landingProfile.landingStat;}; //149208 stat family; all Zoe stats share the level-1 value.
        a.railSpinScore=[&](int style,float spin){if(controllerCallbacks.railScoreEvent)controllerCallbacks.railScoreEvent(4,spin);(void)style;return 0.f;};
        a.recovery=[&](bool requested){if(requested){resetRequested=true;resetReason=1;}return requested;};
        a.upperAction=[&](bool a14,bool a13){return controllerCallbacks.upperAction?controllerCallbacks.upperAction(a14,a13):false;};
        a.boost=[&](bool held,bool pressed){boostEffects={};if(originalBoostSeeded){boostState.amount=groundState.boost;boostState.window=groundState.boostWindow;boostState.tier=groundState.boostTierCounter;boostEffects=originalBoostControl(boostState,boostProfile,held,pressed);groundState.boost=boostState.amount;groundState.boostWindow=boostState.window;groundState.boostTierCounter=boostState.tier;}return boostEffects.started;};
        a.upperReactions=[&](){if(controllerCallbacks.normalUpperReaction)controllerCallbacks.normalUpperReaction(*this);};
        a.restoreStance=[&](){restoreStanceNow();railStanceRestored=true;};
        a.leaveEffects=[&](){}; //13BFA8 audio/score/effect block: observers only.
        return a;
    }
    bool tryRailAttach(){
        if(railRecords.empty()||railActive||crashActive||!originalGroundSeeded)return false;
        auto view=railRiderView();if(view.bonePosition==RailVector{})return false;
        auto access=railAccess();
        //0x106848's requestMotion(4) switches immediately (0x1112B8): 0x13AD20 bakes the 11FA10
        //presented root and zeroes the motion state before 0x115358 and 0x13ADC0 run.
        auto baseMotion=access.requestMotion;
        access.requestMotion=[&](int motion){
            baseMotion(motion);if(motion!=4)return;
            if(controllerCallbacks.presentedRoot){
                RiderRootPresentation values;values.turn=view.turn.current;values.brake=view.brake.current;values.extraLean=view.extraLean.current;values.roll=view.presentationRoll.current;values.liftCm=groundState.presentationLift.current;values.lateral=view.lateral;values.controlState=13;
                auto presented=controllerCallbacks.presentedRoot({view.position,view.quaternion},values);view.position=presented.position;view.quaternion=presented.rotation;
            }
            originalRailMotionBegin(railMotion,view);
        };
        auto result=originalRailAttach(view,railMotion,access);
        if(!result.attached)return false;
        originalRailControlBegin(railControl,view);
        applyRailRider(view);
        railAttachedThisTick=true;airPrewind.jumpGate=0;originalJumpSeeded=false;sourceAirValid=false;
        if(result.entrySemantic>=0)playCrashAnimation(result.entrySemantic);
        return true;
    }
    void leaveRailToAir(){
        OriginalAirState current{groundState.position,groundState.velocity};sourceAir=current;sourceAirValid=true;airAccumulator=0;grounded=false;
        airTrajectory.begin(sourceAir);originalAirTrajectorySeeded=true;airTrajectoryQueryFailed=false;
        airPhysicalQuaternion=groundState.quaternion;airAlignmentContext={};airAlignmentContext.timeScale=groundState.timeScale;
        airAlignmentContext.physicalForward=groundState.physicalForward;airAlignmentContext.controlState=sourceControlState;airAlignmentContext.airModeFlag=0;airPhysicalOrientationPending=true;
        uint32_t leave=landingRuntime.lastGroundLeaveTick;originalLandingGroundLeave(groundState,landingRuntime.tick,leave);if(originalLandingSeeded)landingRuntime.lastGroundLeaveTick=leave;
    }
    void railMotionFrame(){
        auto view=railRiderView();auto access=railAccess();
        auto result=originalRailMotionStep(railMotion,view,access);
        applyRailRider(view);
        //13AF28 leaves +20 set on a miss and on the detach return; 13BFA8 then requests motion1.
        if(railMotion.lostRail){
            //13BFA8: the lost rail leaves motion4 for motion1 with the current velocity.
            auto leaving=railRiderView();auto effects=originalRailMotionLeave(railMotion,leaving,access);
            if(!effects.requestAirMotion){railActive=false;leaveRailToAir();}
            railSteer22C=leaving.steer22C;groundState.velocity=leaving.velocity;OriginalAirState current{groundState.position,groundState.velocity};sourceAir=current;velocity=current.nativeVelocity();
        }
        ++groundTicks;frame.airMotionThisTick=false;
    }
    void railContactFrame(){
        // Motion4 has no cruise terrain contact; the rail query already placed
        // the rider. Body collision against scenery is not part of13AF28.
        position=frame.next;
        if(frame.interaction)(*frame.interaction)(*this);
    }
    void railControlFrame(){
        const auto& input=frame.input;
        OriginalRailCommand command;command.recovery=input.recoverPressed;command.upper13=input.passiveUpper14;command.upper14=input.passiveUpper15;
        command.boostPressed15=input.boostPressed;command.boostHeld16=input.boostHeld;command.turn=input.turn;
        command.rotate=input.spin; // Rail rotation uses the spin axis; transfers stay unmapped (unrecovered branch).
        command.transfer=0;
        auto view=railRiderView();auto access=railAccess();
        railStanceRestored=false;
        auto result=originalRailControlStep(railControl,view,railMotion,command,access);
        applyRailRider(view);
        if(railStanceRestored){groundState.prewindStyle=0;railStanceRestored=false;} // 115640 already cleared 328; the stale view must not restore it.
        if(result.stop==OriginalRailControlResult::Stop::Airborne){
            //132770 handed control to4/5 via requestControl; seed those controllers.
            originalRailControlLeave(view);railBalance238=view.balance;
            if(sourceControlState==4){originalPassiveAirBegin(passiveAirState,groundState,airPrewind);airAngularSupported=airPresentationValid=false;}
            else {airControlState=originalAirControlBegin(0,0);originalAirControlSeeded=airAngularSupported=true;airControlTicks=0;airPresentationValid=false;if(controllerCallbacks.airGain)controllerCallbacks.airGain(*this);}
        }
        previousJump=input.jumpHeld;
    }
    // ---- Original hard crash (control8 / motion2) integration ----
    void playCrashAnimation(int semantic){
        if(!controllerCallbacks.playCrashAnimation)throw std::runtime_error("Crash animation playback requires the original animation player");
        auto state=controllerCallbacks.playCrashAnimation(semantic);
        groundState.animationIndex=state.semantic;groundState.animationClass=state.animationClass;groundState.animationSelectionSupported=true;
    }
    OriginalCrashClipState crashClipState(){
        if(!controllerCallbacks.crashClip)throw std::runtime_error("Crash recovery requires original clip state access");
        auto clip=controllerCallbacks.crashClip(0,23);
        if(!clip)throw std::runtime_error("Crash recovery requires posed primary/secondary bones");
        clip->controllerScale1C=1;return *clip;
    }
    void beginCrashPredictor(float speedLimit=3333.33349609375f){
        sourceAir={crashActor.position,crashActor.velocity};sourceAirValid=true;airAccumulator=0;
        airTrajectory.begin(sourceAir,speedLimit);originalAirTrajectorySeeded=true;airTrajectoryQueryFailed=false;
    }
    void publishCrashActor(){
        OriginalAirState current{crashActor.position,crashActor.velocity};
        position=current.nativePosition();velocity=current.nativeVelocity();frame.next=position;
        sourceAir=current;sourceAirValid=true;
        groundState.position=crashActor.position;groundState.velocity=crashActor.velocity;
        groundState.quaternion=airPhysicalQuaternion=crashActor.quaternion;
        auto basis=originalOrientationBasis(crashActor.quaternion);groundState.physicalForward=basis.forward;groundState.boardUp=basis.up;
        groundState.normal=groundState.previousNormal=crashActor.groundNormal;groundState.surfaceVelocity=crashActor.surfaceVelocity;groundState.distance=crashActor.contactDistance;
        normal={crashActor.groundNormal[0],crashActor.groundNormal[2],-crashActor.groundNormal[1]};
        heading=std::atan2(double(basis.forward[0]),-double(basis.forward[1]));
        grounded=crashMotion.submode==0;
        if(controllerCallbacks.detachedBoard)controllerCallbacks.detachedBoard(crashActor.detached?std::optional<AnimationTransform>{{crashActor.detachedPosition,crashActor.detachedQuaternion}}:std::nullopt);
    }
    void updateCrashPlaybackRate(float* baseOut=nullptr){
        auto clip=crashClipState();
        auto rate=originalCrashPlaybackRate(crashMotion.angularVelocity,clip.duration10,clip.animationClass,crashControl.recovery70,resetPermission470);
        if(controllerCallbacks.animationRate)controllerCallbacks.animationRate(2,rate.applied);
        if(baseOut)*baseOut=rate.base;
    }
    void rebakeCrashRoot(int semantic){
        if(!controllerCallbacks.previewClipRoot||!controllerCallbacks.currentScaledLocalRoot||!controllerCallbacks.offsetAnimationRoots)throw std::runtime_error("Crash root bake requires original animation root access");
        auto preview=controllerCallbacks.previewClipRoot(semantic);auto current=controllerCallbacks.currentScaledLocalRoot();
        if(!preview||!current)throw std::runtime_error("Crash root bake could not sample the original clip roots");
        AnimationTransform physical{crashActor.position,crashActor.quaternion};
        auto bake=originalCrashRootBake(physical,*current,*preview);
        crashActor.position=bake.physical.position;crashActor.quaternion=bake.physical.rotation;
        controllerCallbacks.offsetAnimationRoots(bake.animationRootOffset);
    }
    void selectCrashAnimation(OriginalCrashAnimationSelection kind){
        auto clip=crashClipState();
        auto request=originalCrashSelectAnimation(kind,clip.semantic,crashActor.detached);
        if(request.rebakeRoot)rebakeCrashRoot(request.semantic);
        if(request.play)playCrashAnimation(request.semantic);
        if(request.updatePlaybackRate)updateCrashPlaybackRate();
    }
    void notifyCrash(int observer){if(controllerCallbacks.crashObserver)controllerCallbacks.crashObserver(observer);}
    void refundCrashBoost(bool quick){
        // Quick recoveries return the119B08 penalty that entry removed.
        if(!quick||!originalBoostSeeded||crashPenalty==0)return;
        boostState.amount=std::clamp(boostState.amount-crashPenalty,0.f,1.f);groundState.boost=boostState.amount;crashPenalty=0;
    }
    void leaveCrashToGround(){
        crashActive=false;
        sourceControlState=groundState.controlState=0;airAlignmentContext.controlState=0;
        int surface=std::clamp(crashActor.surface,0,int(landingProfile.materials.size())-1);
        selectGroundMaterial(surface);
        if(originalLandingSeeded){
            //11FE78 motion0 entry is13C7A8, shared with touchdown.
            originalLandingGroundEnter(groundState,landingProfile.materials[surface],landingProfile.bodyScale,landingRuntime.tick,landingRuntime.lastGroundLeaveTick);
            crashActor.velocity=groundState.velocity;landingRuntime.groundFocusTick=landingRuntime.tick;jumpState.ticksSinceGroundFocus=0;
        }
        publishCrashActor();grounded=true;
        airPrewind.jumpGate=0;if(controllerCallbacks.normalGain)controllerCallbacks.normalGain(*this);
        airTrajectory.predictedTime=0;airControlState.spinRate=groundState.manualSpin;originalJumpSeeded=false;
    }
    void leaveCrashToAir(){
        crashActive=false;
        sourceControlState=groundState.controlState=5;airAlignmentContext={};airAlignmentContext.timeScale=groundState.timeScale;
        airAlignmentContext.controlState=5;airAlignmentContext.airModeFlag=0;
        publishCrashActor();grounded=false;
        airAlignmentContext.physicalForward=groundState.physicalForward;airPhysicalOrientationPending=true;
        airTrajectory.begin(sourceAir);originalAirTrajectorySeeded=true;airTrajectoryQueryFailed=false;
        airControlState=originalAirControlBegin(0,0);originalAirControlSeeded=airAngularSupported=true;airControlTicks=0;airPresentationValid=false;
        if(controllerCallbacks.airGain)controllerCallbacks.airGain(*this);
    }
    OriginalCrashRecoveryCallbacks crashRecoveryCallbacks(){
        OriginalCrashRecoveryCallbacks cb;
        cb.reportImpact=[&](float speed){lastCrashImpactCmps=speed;}; //28B180/296xxx observers only
        cb.setRecoveryPresentation=[&](float value){crashRecoveryPresentation=value;};
        cb.stopCrashEffect=[&](){notifyCrash(int(OriginalCrashObserver::StopCrash));};
        cb.playAnimation=[&](int semantic){playCrashAnimation(semantic);};
        cb.enterControl=[&](int control){if(control!=0&&control!=5)throw std::runtime_error("Crash get-up requested unsupported control "+std::to_string(control));crashControl.phase=control==0?-1:-2;};
        cb.enterMotion=[&](int motion){if(motion==0)leaveCrashToGround();else if(motion==1)leaveCrashToAir();else throw std::runtime_error("Crash get-up requested unsupported motion "+std::to_string(motion));};
        cb.setAirScoringStance=[](bool){};
        cb.refundCrashBoost=[&](bool quick){refundCrashBoost(quick);};
        cb.requestReset=[&](int reason){resetRequested=true;resetReason=reason;};
        return cb;
    }
    OriginalCrashContinuationCallbacks crashContinuationCallbacks(){
        OriginalCrashContinuationCallbacks cb;
        cb.currentClip=[&](){return crashClipState();};
        cb.updatePlaybackRate=[&](){float base=0;updateCrashPlaybackRate(&base);return base;};
        cb.playGroundContinuation=[&](){selectCrashAnimation(OriginalCrashAnimationSelection::GroundContinuation);};
        cb.playAirContinuation=[&](){selectCrashAnimation(OriginalCrashAnimationSelection::AirContinuation);};
        cb.playGroundGetUp=[&](){selectCrashAnimation(OriginalCrashAnimationSelection::GroundGetUp);};
        cb.playAirGetUp=[&](){selectCrashAnimation(OriginalCrashAnimationSelection::AirGetUp);};
        cb.playSpecialLanding=[&](){selectCrashAnimation(OriginalCrashAnimationSelection::SpecialLanding);};
        cb.rebakeResetClip=[&](){selectCrashAnimation(OriginalCrashAnimationSelection::ResetClip);};
        cb.seekClipSeconds=[&](float seconds){if(!controllerCallbacks.seekMainAnimation)throw std::runtime_error("Crash continuation requires clip seeking");controllerCallbacks.seekMainAnimation(seconds);};
        cb.reportImpact=[&](float speed){lastCrashImpactCmps=speed;}; //28B180/296xxx observers only
        cb.setRecoveryPresentation=[&](float value){crashRecoveryPresentation=value;};
        cb.cameraShake=[](float){};
        cb.notify=[&](OriginalCrashObserver observer){notifyCrash(int(observer));};
        cb.refundCrashBoost=[&](bool quick){refundCrashBoost(quick);};
        cb.requestReset=[&](int reason){resetRequested=true;resetReason=reason;};
        return cb;
    }
    void crashControlFrame(){
        const auto& input=frame.input;
        //12CB68: recovery meter from the held command bit, then the phase machine.
        uint32_t command=input.jumpHeld?0x2000u:0u;
        originalCrashRecoveryMeter(crashControl,command,riderCategoryB20);
        OriginalCrashRecoveryInputs inputs;inputs.resetPermission470=resetPermission470;
        if(crashControl.phase==0){
            auto clip=crashClipState();
            auto effects=originalCrashInitialControlStep(crashControl,crashMotion,crashActor,clip);
            if(effects.beginPredictor)beginCrashPredictor();
            if(effects.playContinuation)playCrashAnimation(effects.continuationSemantic);
            if(effects.updatePlaybackRate)updateCrashPlaybackRate();
            if(effects.groundContinuationObserverRequired)notifyCrash(int(OriginalCrashObserver::GroundMoving));
        }else if(crashControl.phase==1)originalCrashAirRecoveryStep(crashControl,crashMotion,crashActor,inputs,crashContinuationCallbacks());
        else if(crashControl.phase==2)originalCrashGroundRecoveryStep(crashControl,crashMotion,crashActor,inputs,crashContinuationCallbacks());
        else if(crashControl.phase==3){
            auto clip=crashClipState();
            originalCrashGetUpStep(crashControl,crashMotion.submode,clip.progress,clip.complete,!groundState.state320Equals324,crashRecoveryCallbacks());
        }else if(crashControl.phase==4){
            auto clip=crashClipState();
            originalCrashResetClipStep(crashControl,resetPermission470,clip.complete,crashRecoveryCallbacks());
        }else throw std::runtime_error("Unknown crash recovery phase");
        if(crashActive&&crashActor.detached&&controllerCallbacks.detachedBoard)controllerCallbacks.detachedBoard(std::optional<AnimationTransform>{{crashActor.detachedPosition,crashActor.detachedQuaternion}});
    }
    void crashMotionFrame(){
        const auto& ground=*frame.ground;auto* bodyWorld=frame.bodyWorld;
        if(!bodyWorld)throw std::runtime_error("Crash motion requires the authored world collision package");
        if(!airSurfaceProperty)throw std::runtime_error("Crash motion requires the original surface property catalog");
        terrain_original::Rounding rounding;
        crashActor.timeScale=groundState.timeScale;
        originalCrashDetachedStep(crashMotion,crashActor);
        if(crashMotion.submode==0){
            if(!groundSurfaceCatalog)throw std::runtime_error("Crash sliding requires the original ground surface catalog");
            int oldSurface=std::clamp(crashActor.surface,0,int(groundSurfaceCatalog->size())-1);
            const auto& material=(*groundSurfaceCatalog)[oldSurface];
            int animationClass=lastBodyVolume&&lastBodyVolume->mainAnimation?lastBodyVolume->mainAnimation->animationClass:groundState.animationClass;
            auto physicalForward=originalOrientationBasis(crashActor.quaternion).forward;
            auto alignment=originalCrashSlidingForces(material.surface,crashMotion,crashActor,crashActor.surfaceVelocity,animationClass,physicalForward);
            crashActor.quaternion=originalAirAlignment(crashActor.quaternion,alignment.normal,alignment.heading,alignment.gain,alignment.maximumRate).quaternion;
            crashActor.quaternion=originalRebuildOrientation(crashActor.quaternion).quaternion;
            OriginalCrashWorldQueries queries(ground,*bodyWorld,airSurfaceProperty);
            auto hit=queries.motionHit(queries.terrainContact({crashActor.position,crashActor.groundNormal},&terrainContactCache));
            // Surface+18 retained depth of the material selected before this query.
            float oldDepth=originalLandingSeeded?landingProfile.materials[std::clamp(oldSurface,0,int(landingProfile.materials.size())-1)].depth3:groundProfile.depthTarget3;
            auto effects=originalCrashSlidingContact(crashMotion,crashActor,hit,landingProfile.bodyScale>0?landingProfile.bodyScale:groundProfile.bodyScale,oldDepth,alignment.relativeVelocityBeforeForces);
            if(effects.impact){lastCrashImpactCmps=effects.impactSpeed;lastCrashTick=landingRuntime.tick;lastCrashImpactPointCm=hit.point;lastCrashImpactNormal=hit.normal;lastCrashImpactSurface=hit.surface;}
            if(effects.requestReset){resetRequested=true;resetReason=1;}
            if(effects.beginPredictor)beginCrashPredictor(effects.predictorSpeedLimit);
        }else{
            auto query=airTrajectoryQuery;
            if(!query)query=[&](auto end,auto start,int mode){return queryOriginalAirTrajectoryWorld(ground,bodyWorld,end,start,mode);};
            if(!originalAirTrajectorySeeded)beginCrashPredictor();
            originalCrashAirFirstPhase(crashMotion,crashActor,airTrajectory,query);
        }
        ++(crashMotion.submode==0?groundTicks:airborneTicks);
        publishCrashActor();frame.airMotionThisTick=crashMotion.submode==1;
    }
    void crashContactFrame(){
        const auto& ground=*frame.ground;auto* bodyWorld=frame.bodyWorld;
        if(!bodyWorld)throw std::runtime_error("Crash contacts require the authored world collision package");
        if(!lastBodyVolume)throw std::runtime_error("Crash contacts require the posed crash body volume");
        terrain_original::Rounding rounding;
        OriginalCrashWorldQueries queries(ground,*bodyWorld,airSurfaceProperty);
        float bodyScale=landingProfile.bodyScale>0?landingProfile.bodyScale:groundProfile.bodyScale;
        auto translateBody=[&](const terrain_original::Vector& delta){
            for(unsigned k=0;k<3;++k){lastBodyVolume->broadCenterCm[k]=terrain_original::add(lastBodyVolume->broadCenterCm[k],delta[k]);
                for(unsigned i=0;i<lastBodyVolume->count;++i)lastBodyVolume->spheres[i].centerCm[k]=terrain_original::add(lastBodyVolume->spheres[i].centerCm[k],delta[k]);}
        };
        auto reportRagdollImpact=[&](const WorldBodyHit& hit,const ObstacleResponse& response,terrain_original::Vector velocityBefore){
            RayHit reported;reported.hit=true;reported.position=OriginalAirState{hit.pointCm,{}}.nativePosition();
            reported.normal={hit.normal[0],hit.normal[2],-hit.normal[1]};reported.distance=hit.penetrationCm/100;reported.resource=hit.instance;
            obstacleContacts.push_back({reported,position,OriginalAirState{{},velocityBefore}.nativeVelocity(),OriginalAirState{{},response.velocityCmps}.nativeVelocity(),-1,elapsed});obstacleContact=true;
            //105D98 in motion2: mark another crash collision and reseed the airborne predictor.
            OriginalCollisionEvent event;event.pointCm=hit.pointCm;event.normal=response.normal;event.closingSpeedCmps=response.closingSpeedCmps;event.surface=hit.surface;
            if(airSurfaceProperty&&hit.surface>=0)event.surfaceProperty44=airSurfaceProperty(hit.surface);
            OriginalCollisionContext context;context.motionMode=2;context.controlState=sourceControlState;context.ragdollSubmode=crashMotion.submode;context.velocityCmps=crashActor.velocity;
            auto reaction=originalCollisionReaction(collisionProfile,collisionHistory,context,event,controllerCallbacks.nextRandom);
            if(reaction.kind==OriginalCollisionReactionKind::SurfaceReset){resetRequested=true;resetReason=1;}
            else if(reaction.kind==OriginalCollisionReactionKind::RagdollImpact){
                crashControl.impactPending54=1;crashControl.impactVelocity60=velocityBefore;
                if(reaction.resetPredictor&&originalAirTrajectorySeeded)airTrajectory.reseed({crashActor.position,crashActor.velocity});
            }
        };
        lastBodyQuery={};lastBodyResponse={};
        if(crashMotion.submode==0){
            lastBodyQuery=queries.slidingBody(*lastBodyVolume,crashActor.groundNormal,&bodyContactCache);
            if(lastBodyQuery.best.hit){
                const auto& hit=lastBodyQuery.best;auto before=crashActor.velocity;
                lastBodyResponse=originalCrashSlidingBodyResponse(hit.penetrationCm,crashActor.groundNormal,hit.normal,crashActor.velocity,hit.surfaceVelocityCmps);
                if(lastBodyResponse.accepted){
                    for(unsigned k=0;k<3;++k)crashActor.position[k]=terrain_original::add(crashActor.position[k],lastBodyResponse.translationCm[k]);
                    translateBody(lastBodyResponse.translationCm);crashActor.velocity=lastBodyResponse.velocityCmps;
                    if(lastBodyResponse.bounced)reportRagdollImpact(hit,lastBodyResponse,before);
                }
            }
            if(crashActor.detached){
                auto clip=crashClipState();
                auto board=originalCrashDetachedBody({crashActor.detachedPosition,crashActor.detachedQuaternion},bodyScale);
                auto detachedQuery=queries.detachedBody(board);
                if(detachedQuery.best.hit)originalCrashDetachedBodyResponse(crashMotion,crashActor,detachedQuery.best.penetrationCm,detachedQuery.best.normal,false,60,controllerCallbacks.nextRandom);
                (void)clip;
            }
            originalCrashSlidingFinish(crashMotion,crashActor);
            if(crashMotion.submode==1&&!originalAirTrajectorySeeded)beginCrashPredictor();
        }else{
            lastBodyQuery=queries.airborneBody(*lastBodyVolume,true,&bodyContactCache);
            if(lastBodyQuery.best.hit){
                const auto& hit=lastBodyQuery.best;auto before=crashActor.velocity;
                auto terrainHit=queries.motionHit(queries.terrainContact({crashActor.position,hit.normal},&terrainContactCache));
                auto effects=originalCrashAirBodyResponse(crashMotion,crashActor,hit.penetrationCm,hit.normal,terrainHit,60);
                if(effects.moved)translateBody(effects.translation);
                if(effects.landed){crashControl.impactPending54=1;crashControl.impactVelocity60=effects.impactVelocity;lastCrashImpactCmps=-effects.impactSpeed;lastCrashTick=landingRuntime.tick;lastCrashImpactPointCm=terrainHit.point;lastCrashImpactNormal=terrainHit.normal;lastCrashImpactSurface=terrainHit.surface;}
                else if(effects.moved){ObstacleResponse response;response.accepted=response.moved=response.bounced=true;response.normal=hit.normal;response.velocityCmps=crashActor.velocity;
                    float closing=0;for(unsigned k=0;k<3;++k)closing=terrain_original::add(closing,terrain_original::mul(-hit.normal[k],before[k]));response.closingSpeedCmps=closing;
                    reportRagdollImpact(hit,response,before);}
                if(effects.beginPredictor)beginCrashPredictor(effects.predictorSpeedLimit);
            }
            if(crashActor.detached){
                auto board=originalCrashDetachedBody({crashActor.detachedPosition,crashActor.detachedQuaternion},bodyScale);
                auto detachedQuery=queries.detachedBody(board);
                if(detachedQuery.best.hit)originalCrashDetachedBodyResponse(crashMotion,crashActor,detachedQuery.best.penetrationCm,detachedQuery.best.normal,false,60,controllerCallbacks.nextRandom);
            }
        }
        if(frame.interaction){publishCrashActor();(*frame.interaction)(*this);crashActor.position=OriginalAirState::fromNative(position,velocity).position;crashActor.velocity=OriginalAirState::fromNative(position,velocity).velocity;}
        publishCrashActor();
    }
    // Original105D98 dispatch after a cruise/air body response has bounced.
    void dispatchCollisionEvent(const WorldBodyHit& hit,const ObstacleResponse& response,Vec3 nativeVelocityBefore,bool airborne){
        if(!originalGroundSeeded)return;
        OriginalCollisionEvent event;event.pointCm=hit.pointCm;event.normal=response.normal;event.closingSpeedCmps=response.closingSpeedCmps;event.surface=hit.surface;
        if(airSurfaceProperty&&hit.surface>=0)event.surfaceProperty44=airSurfaceProperty(hit.surface);
        auto incoming=OriginalAirState::fromNative(position,nativeVelocityBefore).velocity;float length=std::sqrt(incoming[0]*incoming[0]+incoming[1]*incoming[1]+incoming[2]*incoming[2]);
        if(length>0)for(auto& x:incoming)x/=length;event.incomingDirection=incoming;
        OriginalCollisionContext context;
        if(lastBodyVolume&&lastBodyVolume->reactionFrame)context.presentation=*lastBodyVolume->reactionFrame;
        context.physical=currentPhysicalFrame();context.velocityCmps=response.velocityCmps;
        context.motionMode=airborne?1:0;context.controlState=sourceControlState;
        context.animationClass=lastBodyVolume&&lastBodyVolume->mainAnimation?lastBodyVolume->mainAnimation->animationClass:groundState.animationClass;
        context.reverseStance=groundState.reverseStance;context.manualSpin=groundState.manualSpin;
        auto reaction=originalCollisionReaction(collisionProfile,collisionHistory,context,event,controllerCallbacks.nextRandom);
        if(reaction.resetPredictor&&!grounded&&originalAirTrajectorySeeded)airTrajectory.reseed(sourceAir);
        switch(reaction.kind){
            case OriginalCollisionReactionKind::Ignored:break;
            case OriginalCollisionReactionKind::SurfaceReset:resetRequested=true;resetReason=1;break;
            case OriginalCollisionReactionKind::Soft:
                if(reaction.cancelControlOne)throw std::runtime_error("Soft collision from control1 is not connected");
                if(!grounded){obstacleEventPending=true;break;} // Airborne soft reactions are gated out by108388's motion check.
                enterSoftCollision(reaction.animation,reaction.manualSpin,false,reaction.strongSoftImpact);break;
            case OriginalCollisionReactionKind::Crash:enterHardCrash(reaction.animation,event,0);break;
            case OriginalCollisionReactionKind::RagdollImpact:break;
        }
    }
    void enterHardCrash(int semantic,const OriginalCollisionEvent& event,int impactType){
        if(crashActive)throw std::runtime_error("Hard crash entry while already crashing");
        if(!controllerCallbacks.playCrashAnimation||!controllerCallbacks.presentedRoot)throw std::runtime_error("Hard crash requires the original animation player hooks");
        OriginalHardCrashEntryState state;bool airborne=!grounded;
        if(airborne){
            if(sourceControlState==5&&originalAirControlSeeded&&airAngularSupported){
                //10EB30 from control5 bakes the air spin presentation first (134CB0).
                OriginalAirExitState exit;exit.physical={sourceAir.position,airPhysicalQuaternion};exit.adjustment28C=groundState.adjustment28C;exit.adjustment298=groundState.adjustment298;
                originalAirControlExit(airControlState,airPrewind,exit,airPivot);
                groundState.adjustment28C=exit.adjustment28C;groundState.adjustment298=exit.adjustment298;groundState.manualSpin=airControlState.spinRate;
                state.physical={exit.physical.position,exit.physical.quaternion};state.physicalUp=exit.up;
                if(controllerCallbacks.animationFade)controllerCallbacks.animationFade(1,0.33000001311302185f);
            }else{state.physical={sourceAir.position,airPhysicalQuaternion};state.physicalUp=originalOrientationBasis(airPhysicalQuaternion).up;}
        }else{state.physical={groundState.position,groundState.quaternion};state.physicalUp=groundState.boardUp;}
        state.prewindStyle328=groundState.prewindStyle;
        OriginalHardCrashEntryCallbacks cb;
        cb.reportPeakImpact=[&](float value){collisionHistory.peakImpactCmps=std::max(collisionHistory.peakImpactCmps,value);};
        cb.recordCrash=[&](bool){
            //119B08 counts the crash and returns -.25 while117948 reports boost to lose.
            crashPenalty=(originalBoostSeeded&&boostState.amount>0)?-.25f:0.f;return crashPenalty;};
        cb.changeBoostMeter=[&](float penalty){if(originalBoostSeeded&&penalty!=0){boostState.amount=std::clamp(boostState.amount+penalty,0.f,1.f);groundState.boost=boostState.amount;}};
        cb.notify=[&](OriginalHardCrashObserver observer,float){notifyCrash(100+int(observer));};
        cb.reportImpact=[&](const OriginalCollisionEvent& e,int){lastCrashImpactCmps=e.closingSpeedCmps;lastCrashTick=landingRuntime.tick;lastCrashImpactPointCm=e.pointCm;lastCrashImpactNormal=e.normal;lastCrashImpactSurface=e.surface;};
        cb.rotateAnimationRoot=[&](float angle){if(!controllerCallbacks.rotateAnimationRoot)throw std::runtime_error("Crash prewind compensation requires animation root rotation");controllerCallbacks.rotateAnimationRoot(angle);};
        cb.resetAnimationRootBasis=[&](const AnimationTransform&){if(controllerCallbacks.resetAnimationRootBasis)controllerCallbacks.resetAnimationRootBasis();};
        cb.presentedRoot=[&](const OriginalHardCrashEntryState& s){
            RiderRootPresentation values;values.turn=groundState.turn.current;values.brake=groundState.brake.current;values.extraLean=groundState.extraLean.current;values.roll=groundState.presentationRoll.current;values.liftCm=groundState.presentationLift.current;values.lateral=groundState.lateral;values.controlState=13;
            return controllerCallbacks.presentedRoot(s.physical,values);};
        cb.previewCrashRoot=[&](int clip){auto root=controllerCallbacks.previewClipRoot?controllerCallbacks.previewClipRoot(clip):std::nullopt;if(!root)throw std::runtime_error("Crash entry could not preview clip "+std::to_string(clip));return *root;};
        cb.currentScaledLocalRoot=[&](){auto root=controllerCallbacks.currentScaledLocalRoot?controllerCallbacks.currentScaledLocalRoot():std::nullopt;if(!root)throw std::runtime_error("Crash entry requires the sampled local root");return *root;};
        cb.offsetAnimationRoots=[&](const AnimationTransform& offset){if(!controllerCallbacks.offsetAnimationRoots)throw std::runtime_error("Crash entry requires animation root offsets");controllerCallbacks.offsetAnimationRoots(offset);};
        cb.playAnimation=[&](int clip){playCrashAnimation(clip);};
        cb.enterControl=[&](int control,OriginalHardCrashEntryState& s){
            sourceControlState=groundState.controlState=control;airAlignmentContext.controlState=control;
            if(control==13)return;
            crashActor={};crashActor.position=s.physical.position;crashActor.quaternion=s.physical.rotation;
            crashActor.velocity=airborne?sourceAir.velocity:groundState.velocity;
            crashActor.groundNormal=airborne?std::array<float,3>{0,0,1}:groundState.normal;
            crashActor.surfaceVelocity=groundState.surfaceVelocity;crashActor.surface=groundProfile.surface.id;crashActor.timeScale=groundState.timeScale;
            crashActor.contactDistance=groundState.distance;
            auto clip=crashClipState();
            auto effects=originalCrashControlBegin(crashControl,crashMotion,crashActor,clip.primary,clip.secondary,{},clip.animationClass);
            if(effects.clearBoostWindows&&originalBoostSeeded){boostState.window=0;groundState.boostWindow=0;}
        };
        cb.enterMotion=[&](int motion,OriginalHardCrashEntryState&){
            if(motion!=2)throw std::runtime_error("Hard crash requested unsupported motion mode");
            auto entry=originalCrashMotionBegin(crashMotion,crashActor,airborne?1:0);
            crashActive=true;airAngularSupported=false;airPresentationValid=false;airPrewind.jumpGate=0;
            for(auto* value:{&groundState.turn,&groundState.brake,&groundState.crouch,&groundState.presentationLift,&groundState.extraLean,&groundState.presentationRoll,&groundState.animationTurn,&groundState.boardAlignment})value->current=value->target=0;
            groundState.prewindStyle=0;groundState.manualSpin=0;
            if(entry.beginPredictor)beginCrashPredictor(entry.predictorSpeedLimit);
            publishCrashActor();
        };
        originalHardCrashEnter(state,semantic,false,impactType,event,cb);
    }
    void step(const RiderInput& input,const CollisionWorld& ground,const CollisionWorld& obstacles,
              const WorldBodyCollision* bodyWorld,const BodyPoseProvider* poseProvider,const RiderPoseStages* stages){
        inputAccumulator+=1.0/120;if(inputAccumulator+1e-12<dt)return;inputAccumulator-=dt;
        beginFrame(input,ground,obstacles,bodyWorld,poseProvider,stages);
        for(auto phase:riderFramePhases)runFramePhase(phase);
    }

public:
    void setAirAnimationAdjustments(GroundControlValue a,GroundControlValue b){groundState.adjustment28C=a;groundState.adjustment298=b;}
    void synchronizeAnimationState(const BodyAnimationState& state){groundState.animationIndex=state.semantic;groundState.animationClass=state.animationClass;groundState.animationSelectionSupported=true;}

    void requestControllerAnimation(int semantic,float rate=1){
        if(controllerCallbacks.requestAnimation){auto state=controllerCallbacks.requestAnimation(semantic,rate);groundState.animationIndex=state.semantic;groundState.animationClass=state.animationClass;groundState.animationSelectionSupported=true;}
        else{++animationEvent.serial;animationEvent.semantic=semantic;animationEvent.rate=rate;animationEvent.airExit=animationEvent.reverseTurn=false;groundState.animationIndex=semantic;groundState.animationClass=semantic==287?2:(semantic>=268&&semantic<=286?9:7);groundState.animationSelectionSupported=true;}
    }

    void setControllerCallbacks(RiderControllerCallbacks callbacks){if(frame.phase!=RiderFramePhase::Idle)throw std::runtime_error("Cannot change controller hooks mid-frame");controllerCallbacks=std::move(callbacks);}
    std::optional<BodyAnimationState> currentMainAnimation()const {
        if(frame.phase!=RiderFramePhase::Idle&&frame.poseStages&&frame.poseStages->mainAnimation)return frame.poseStages->mainAnimation();
        return lastBodyVolume?lastBodyVolume->mainAnimation:std::nullopt;
    }
    void enterSoftCollision(int semantic,float manualSpin,bool attack,bool strong){
        if(frame.phase!=RiderFramePhase::WorldPose&&frame.phase!=RiderFramePhase::MotionContacts)throw std::runtime_error("Soft collision entry outside motion contact phase");
        if(!grounded||(sourceControlState!=0&&sourceControlState!=2)||semantic<55||semantic>60||!std::isfinite(manualSpin))throw std::runtime_error("Unsupported soft collision entry lifecycle");
        groundState.manualSpin=manualSpin;groundState.animationIndex=semantic;groundState.animationClass=6;groundState.animationSelectionSupported=true;
        sourceControlState=groundState.controlState=3;airAlignmentContext.controlState=3;
        softCollisionScorePending=attack;strongSoftCollision=strong;
    }

    // Shared world callers run each phase over the entire selected roster.
    void beginFrame(const RiderInput& input,const CollisionWorld& ground,const CollisionWorld& obstacles,
                    const WorldBodyCollision* bodyWorld=nullptr,const BodyPoseProvider* provider=nullptr,
                    const RiderPoseStages* stages=nullptr,std::optional<RiderFrameTiming> timing={},const RiderInteraction* interaction=nullptr){
        if(frame.phase!=RiderFramePhase::Idle||pendingSharedFinish)throw std::runtime_error("Previous rider frame is incomplete");
        if(timing)obstacleContacts.clear(); // Shared ticks have no outer advance() to clear transient contacts.
        frame={};frame.input=input;frame.ground=&ground;frame.obstacles=&obstacles;frame.bodyWorld=bodyWorld;
        frame.poseProvider=provider;frame.poseStages=stages;frame.interaction=interaction;frame.externalTiming=timing;frame.startPosition=position;frame.next=position;
        beginFrameState();frame.phase=RiderFramePhase::Started;
    }
    void setControllerTimeScale(float scale){
        if(frame.phase!=RiderFramePhase::Started||!std::isfinite(scale)||scale<0)throw std::runtime_error("Invalid controller time-scale update");
        groundState.timeScale=scale;airAlignmentContext.timeScale=scale;
    }
    void setRouteHeading(float angle){if(frame.phase!=RiderFramePhase::GeometryCommit||!std::isfinite(angle))throw std::runtime_error("AI route heading outside post-motion progress phase");groundState.headingOffset=angle;}
    void setFrameInput(const RiderInput& input){if(frame.phase!=RiderFramePhase::Started)throw std::runtime_error("Control input supplied outside controller phase");frame.input=input;}
    void runFramePhase(RiderFramePhase phase){
        if(int(phase)!=int(frame.phase)+1)throw std::runtime_error("Rider phases must follow original order");
        switch(phase){
            case RiderFramePhase::Controls:controlFrame();break;
            case RiderFramePhase::Timers:timerFrame();break;
            case RiderFramePhase::Filters:filterFrame();break;
            case RiderFramePhase::Motion:motionFrame();break;
            case RiderFramePhase::LocalPose:localPoseFrame();break;
            case RiderFramePhase::WorldPose:worldPoseFrame();break;
            case RiderFramePhase::MotionContacts:contactFrame();break;
            case RiderFramePhase::GeometryCommit:break; // Source1217F8 commits skin geometry after all motion contacts.
            case RiderFramePhase::Events:eventFrame();break;
            default:throw std::runtime_error("Invalid rider phase");
        }
        frame.phase=phase==RiderFramePhase::Events?RiderFramePhase::Idle:phase;
    }
    RiderFramePhase framePhase()const{return frame.phase;}
    void completeSharedTick(uint32_t tick){
        if(frame.phase!=RiderFramePhase::Idle||!pendingSharedFinish)throw std::runtime_error("Shared clock commit outside completed rider events");
        if(originalLandingSeeded){landingRuntime.tick=tick;jumpState.ticksSinceGroundFocus=tick-landingRuntime.groundFocusTick;}
        elapsed+=dt;pendingSharedFinish=false;frame.externalTiming.reset();
    }

    void applyWorldContact(std::array<float,3> sourcePosition,std::array<float,3> sourceVelocity,bool reseedPrediction){
        if(frame.phase!=RiderFramePhase::WorldPose&&frame.phase!=RiderFramePhase::MotionContacts)
            throw std::runtime_error("Pair mutation outside original contact phase");
        OriginalAirState current{sourcePosition,sourceVelocity};position=current.nativePosition();velocity=current.nativeVelocity();frame.next=position;
        if(originalGroundSeeded||originalPoseControlsSeeded){groundState.position=sourcePosition;groundState.velocity=sourceVelocity;}
        if(sourceAirValid||frame.airMotionThisTick)sourceAir=current;
        if(reseedPrediction&&originalAirTrajectorySeeded)airTrajectory.reseed(current);
    }
    void setDetailedBodyQueries(bool detailed){if(frame.phase!=RiderFramePhase::Idle)throw std::runtime_error("Cannot change query policy during a frame");detailedBodyQueries=detailed;}
    void translateWorldContact(std::array<float,3> delta){
        terrain_original::Rounding rounding;
        auto source=OriginalAirState::fromNative(position,velocity);
        for(unsigned k=0;k<3;++k)source.position[k]=terrain_original::add(source.position[k],delta[k]);
        applyWorldContact(source.position,source.velocity,false);
        //106538/329B40 translate the cached collision body, not posed bones or
        // presentation frame. Preserve the unquantized source delta here.
        if(lastBodyVolume)for(unsigned k=0;k<3;++k){
            lastBodyVolume->broadCenterCm[k]=terrain_original::add(lastBodyVolume->broadCenterCm[k],delta[k]);
            for(unsigned i=0;i<lastBodyVolume->count;++i)lastBodyVolume->spheres[i].centerCm[k]=terrain_original::add(lastBodyVolume->spheres[i].centerCm[k],delta[k]);
        }
    }
    void setWorldContactVelocity(std::array<float,3> value,bool reseedPrediction){
        auto source=OriginalAirState::fromNative(position,velocity);applyWorldContact(source.position,value,reseedPrediction);
    }
    void useSharedRaceClock(){if(frame.phase!=RiderFramePhase::Idle)throw std::runtime_error("Cannot change race ownership mid-frame");raceSession.reset();accumulator=inputAccumulator=0;}

    std::vector<ObstacleContact> obstacleContacts;
    WorldBodyQuery lastBodyQuery;
    ObstacleResponse lastBodyResponse;
    std::optional<BodyCollisionVolume> lastBodyVolume;
    bool bodyCollisionMissingWorld=true,bodyCollisionMissingPose=true,obstacleOrientationPending=false,obstacleEventPending=false;
    bool groundSurfaceProfileMissing=false;
    Vec3 position,velocity,normal={0,1,0};
    double heading=0,jumpCharge=0,elapsed=0;
    uint64_t airborneTicks=0,groundTicks=0;
    RayHit lastGroundContact;
    OriginalWorldSegmentHit lastLandingContact;
    bool landingTransitionPending=false,landingScoreEventPending=false;
    bool softCollisionScorePending=false,strongSoftCollision=false;
    uint32_t lastLandingTick=0;
    float lastLandingNormalSpeedCmps=0;
    terrain_original::ContactCache terrainContactCache,bodyContactCache;
    bool airSpeedCapped=false;
    bool airAngularSupported=false,airPhysicalOrientationPending=true,airPrewindSupported=false;
    uint64_t airControlTicks=0;
    bool grounded=false,obstacleContact=false;
    // Explicit116120-style reset requests (recover input, hazard surfaces,
    // crash reset clips) that the owning session honours by respawning.
    bool resetRequested=false;int resetReason=0;
    float crashRecoveryPresentation=0,lastCrashImpactCmps=0;uint32_t lastCrashTick=0;
    // 111AA0 snow impact arguments from 10EB30 / 137860 / 137D18 (not ragdoll scenery hits).
    terrain_original::Vector lastCrashImpactPointCm{},lastCrashImpactNormal{0,0,1};int lastCrashImpactSurface=0;
    int currentMotionMode()const{return crashActive?2:(railActive?4:(grounded?0:1));}
    bool isCrashing()const{return crashActive;}
    bool isGrinding()const{return railActive;}
    const OriginalRailMotionState& originalRailMotionState()const{return railMotion;}
    void bindOriginalRails(std::span<const OriginalRailRecord> records){railRecords=records;}
    const OriginalCrashControlState& originalCrashControlState()const{return crashControl;}
    const OriginalCrashMotionState& originalCrashMotionState()const{return crashMotion;}
    const OriginalCrashActorState& originalCrashActorState()const{return crashActor;}
    const OriginalCollisionHistory& originalCollisionHistoryState()const{return collisionHistory;}
    void reset(Vec3 point,Vec3 up,double yaw) {
        collisionHistory={};crashControl={};crashMotion={};crashActor={};crashActive=false;crashPenalty=0;resetRequested=false;resetReason=0;crashRecoveryPresentation=lastCrashImpactCmps=0;lastCrashTick=0;
        railMotion={};railControl={};railActive=railAttachedThisTick=false;railSteer22C={};railBalance238={};railTolerance25C={};
        frame={};pendingSharedFinish=false;detailedBodyQueries=true;
        raceSession.reset();originalBoostSeeded=false;boostState={};boostProfile={};boostEffects={};
        position=point+Vec3{0,.025,0}; velocity={};normal=unit(up);heading=yaw;
        grounded=true;jumpCharge=elapsed=accumulator=0;previousJump=false;obstacleContact=false;obstacleContacts.clear();
        sourceAirValid=false;airAccumulator=inputAccumulator=0;airborneTicks=0;airSpeedCapped=false;
        originalAirControlSeeded=airPresentationValid=airAngularSupported=false;airPhysicalOrientationPending=true;airControlTicks=0;
        originalAirEntrySeeded=airPrewindSupported=false;airPrewind={};sourceControlState=0;
        originalLandingSeeded=false;landingProfile={};landingRuntime={};animationEvent={};lastLandingContact={};landingTransitionPending=landingScoreEventPending=false;lastLandingTick=0;
        softCollisionScorePending=strongSoftCollision=false;
        originalAirTrajectorySeeded=false;airTrajectoryQueryFailed=false;airTrajectory={};airAlignmentContext={};
        originalJumpSeeded=false;jumpState={};originalGroundSeeded=false;originalPoseControlsSeeded=false;originalGroundProfileSeeded=false;groundTicks=0;lastGroundContact={};terrainContactCache={};bodyContactCache={};
        groundSurfaceCatalog.reset();groundSurfaceProfileMissing=false;
        lastBodyQuery={};lastBodyResponse={};lastBodyVolume.reset();bodyCollisionMissingWorld=bodyCollisionMissingPose=true;obstacleOrientationPending=obstacleEventPending=false;
        // The inspector's unseeded scene still lacks recovered rider stats.
        // Preserve its existing provisional 55m/s limit until roster initialization.
        jumpState.speedLimit=5500;
    }
    void seedOriginalBoost(const OriginalBoostProfile& profile,const OriginalBoostState& state){boostProfile=profile;boostState=state;originalBoostSeeded=true;}
    bool hasOriginalBoost()const{return originalBoostSeeded;}
    const OriginalBoostState& originalBoostState()const{return boostState;}
    const OriginalBoostEffects& originalBoostEffects()const{return boostEffects;}
    void seedOriginalRaceSession(const OriginalRaceEventAsset& asset){raceSession.emplace(asset);}
    bool hasOriginalRaceSession()const{return raceSession.has_value();}
    const OriginalRaceSessionSnapshot& originalRaceState()const{return raceSession->state;}
    void seedOriginalGround(const OriginalGroundProfile& profile,const OriginalGroundState& state) {
        groundProfile=profile;groundState=state;sourceControlState=state.controlState;originalGroundSeeded=originalGroundProfileSeeded=true;
        groundSurfaceCatalog.reset();groundSurfaceProfileMissing=false;
        OriginalAirState physical{state.position,state.velocity};position=physical.nativePosition();velocity=physical.nativeVelocity();
        normal={state.normal[0],state.normal[2],-state.normal[1]};jumpCharge=state.crouch.current;
        heading=std::atan2(double(state.physicalForward[0]),-double(state.physicalForward[1]));
    }
    void seedOriginalGroundProfile(const OriginalGroundProfile& profile){groundProfile=profile;originalGroundProfileSeeded=true;}
    const OriginalAnimationEvent& originalAnimationEvent()const{return animationEvent;}
    void seedOriginalPoseControls(const OriginalGroundState& state){groundState=state;sourceControlState=state.controlState;originalPoseControlsSeeded=true;}
    bool hasOriginalPoseControls()const{return originalGroundSeeded||originalPoseControlsSeeded;}
    void seedOriginalGroundCache(const terrain_original::ContactCache& cache) {terrainContactCache=cache;}
    void seedOriginalBodyCache(const terrain_original::ContactCache& cache) {bodyContactCache=cache;}
    bool hasOriginalGround() const {return originalGroundSeeded;}
    const OriginalGroundProfile& originalGroundProfile() const {return groundProfile;}
    int currentGroundSurfaceId() const {return groundProfile.surface.id;}
    void bindOriginalGroundSurfaceCatalog(std::array<OriginalGroundMaterial,19> catalog) {
        for(unsigned i=0;i<catalog.size();++i)if(catalog[i].surface.id!=int(i))throw std::runtime_error("Original ground surface catalog order");
        groundSurfaceCatalog=std::move(catalog);selectGroundMaterial(groundProfile.surface.id);
    }
    const OriginalGroundState& originalGroundState() const {return groundState;}
    const OriginalGroundDiagnostics& originalGroundDiagnostics() const {return groundDiagnostics;}
    void seedOriginalLanding(const OriginalLandingProfile& profile,const OriginalLandingRuntime& runtime){landingProfile=profile;landingRuntime=runtime;originalLandingSeeded=true;}
    bool hasOriginalLanding()const{return originalLandingSeeded;}
    const OriginalLandingRuntime& originalLandingRuntime()const{return landingRuntime;}
    void seedOriginalAirEntry(const OriginalAirControlProfile& profile,const OriginalAirPrewindState& prewind,const std::array<float,3>& pivot){
        airControlProfile=profile;airPrewind=prewind;airPivot=pivot;originalAirEntrySeeded=airPrewindSupported=true;
    }
    bool hasOriginalAirEntry()const{return originalAirEntrySeeded;}
    const OriginalAirPrewindState& originalAirPrewindState()const{return airPrewind;}
    void seedOriginalAirControl(const OriginalAirControlProfile& profile,const OriginalAirControlState& state,
                                const OriginalAirPresentation& physical,const std::array<float,3>& pivot,
                                bool noGrabContext=true){
        airControlProfile=profile;airControlState=state;airPhysicalQuaternion=physical.quaternion;airPivot=pivot;
        originalAirControlSeeded=true;sourceControlState=groundState.controlState=5;airAngularSupported=noGrabContext&&state.mode!=2&&!grounded;
        airPhysicalOrientationPending=true;airControlTicks=0;
        auto source=OriginalAirState::fromNative(position,velocity);
        airPresentationCache=originalAirPresentationCurrent(state,{source.position,airPhysicalQuaternion},pivot);
        airPresentationValid=airAngularSupported;
    }
    void seedOriginalAirTrajectory(const OriginalAirTrajectory& state,const OriginalAirAlignmentContext& context){
        airTrajectory=airAnimationTrajectory=state;airAlignmentContext=context;sourceControlState=groundState.controlState=context.controlState;originalAirTrajectorySeeded=true;airTrajectoryQueryFailed=false;airPhysicalOrientationPending=true;
    }
    void bindOriginalAirTrajectoryQuery(OriginalAirTrajectoryQuery query,std::function<int(int)> surfaceProperty){
        airTrajectoryQuery=std::move(query);airSurfaceProperty=std::move(surfaceProperty);
    }
    void bindOriginalAirSurfaceProperties(std::vector<int> values){
        airSurfaceProperty=[values=std::move(values)](int index){
            if(index<0||size_t(index)>=values.size())throw OriginalAirTrajectoryUnavailable("Missing original air surface property");
            return values[index];
        };
    }
    bool hasOriginalAirTrajectory() const {return originalAirTrajectorySeeded;}
    const OriginalAirTrajectory& originalAirTrajectoryState() const {return airTrajectory;}
    const OriginalAirTrajectory& originalAirAnimationTrajectoryState()const{return airAnimationTrajectory;}
    const OriginalAirAlignmentContext& originalAirAlignmentState() const {return airAlignmentContext;}
    const std::array<float,4>& originalAirPhysicalQuaternion() const {return airPhysicalQuaternion;}
    OriginalAirPresentation originalAirPhysicalPose()const{return {sourceAirValid?sourceAir.position:OriginalAirState::fromNative(position,velocity).position,airPhysicalQuaternion};}
    int currentControlState()const{return sourceControlState;}
    OriginalCollisionFrame currentPhysicalFrame()const{
        auto basis=originalOrientationBasis(grounded?groundState.quaternion:airPhysicalQuaternion);
        return {basis.right,basis.forward,basis.up,OriginalAirState::fromNative(position,velocity).position};
    }

    bool hasOriginalAirControl() const {return originalAirControlSeeded;}
    const OriginalAirControlState& originalAirControlState() const {return airControlState;}
    const OriginalAirControlProfile& originalAirControlProfile() const {return airControlProfile;}
    bool hasOriginalAirPresentation() const {return airPresentationValid&&!grounded;}
    const OriginalAirPresentation& originalAirPresentationPose() const {return airPresentationCache;}
    void seedOriginalJump(const OriginalJumpState& state,bool previousHeld) {
        jumpState=state;originalJumpSeeded=true;jumpCharge=state.charge;previousJump=previousHeld;
    }
    void cancelInput() { previousJump=false;jumpCharge=0; }
    void advance(double seconds,const RiderInput& input,const CollisionWorld& ground,const CollisionWorld& obstacles,
                 const WorldBodyCollision* bodyWorld=nullptr,const BodyPoseProvider* poseProvider=nullptr,const RiderPoseStages* stages=nullptr) {
        obstacleContacts.clear();
        if (!std::isfinite(seconds)||seconds<=0) return;
        accumulator+=std::min(seconds,.25);
        while (accumulator+1e-12>=1.0/120) {
            step(input,ground,obstacles,bodyWorld,poseProvider,stages);accumulator-=1.0/120;
        }
    }
    double speed() const { return std::sqrt(dot(velocity,velocity)); }
};
}
