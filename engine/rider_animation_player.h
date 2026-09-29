#pragma once
#import "animation_asset.h"
#include "rider_pose_motion.hpp"
#include "animation_cycle.hpp"
#include "riding.hpp"
#include <memory>
namespace ssx {
class OriginalRiderAnimation : public std::enable_shared_from_this<OriginalRiderAnimation> {
    std::shared_ptr<const AnimationRigAsset> rig;
    std::vector<OriginalAnimationSequence> sequences;
    std::array<int,6> requestedSemantics{438,438,438,438,438,438};
    AnimationVector scale;
    AnimationTransform defaultRoot;bool defaultMirror=false;
    RiderPoseContact contact;
    RiderRootPresentation presentation;
    std::optional<OriginalCollisionFrame> reactionFrame;
    std::optional<AnimationVector> airPivot;
    unsigned pivotBone=0;
    int grabEndSemantic=287;
    uint32_t variantFlags=0;
    std::function<uint32_t()> variantRandom;
    uint64_t ticks=0,eventSerial=0,worldTicks=~uint64_t(0),activeBoneMask=~uint64_t(0);
    std::optional<std::vector<AnimationTransform>> sampledLocal,cachedWorld;
    std::vector<AnimationLayer> sampledLayers;
    OriginalGroundState posedGroundState;
    OriginalAirControlState posedAirControl;
    AnimationTransform posedPhysical;
    RiderRootPresentation posedPresentation;
    AnimationVector posedBoardDirection{};
    bool posedAirborne=false,posedMotion=false;
    bool supported=true,collisionEnabled=false;
    bool controllerDriven=false;
    float duration(uint32_t clip)const;
    // Animator +0x1C (setter 0x3158E0): 311F00 gives every new sequence this rate; rate < 0 plays with it (3128E8).
    float nextRate=1;
    bool enter(int semantic,float rate=-1,uint64_t mask=~uint64_t(0),bool force=false);
    void advance(const OriginalGroundState&,float prewindSpin,float prewindFlip);
    void completeSequences();
    void consumeEvent(const PrototypeRider&);
public:
    OriginalRiderAnimation(std::shared_ptr<const AnimationRigAsset>,std::vector<OriginalAnimationSequence>,AnimationVector,RiderPoseContact,bool enableCollision=false,RiderRootPresentation rootPresentation={},unsigned originalPivotBone=0,AnimationTransform originalDefaultRoot={},bool originalDefaultMirror=false,uint64_t originalActiveBoneMask=~uint64_t(0),std::array<int,6> originalRequestedSemantics={-1,-1,-1,-1,-1,-1});
    // Original121700/11EB60: update clocks, sample local transforms, then end events.
    bool prepareLocalPose(const PrototypeRider&,double seconds);
    // Original121728/11EB98: world FK/IK only; never advances the local clock.
    std::optional<std::vector<AnimationTransform>> finishWorldPose(const PrototypeRider&,double seconds);
    std::optional<BodyCollisionVolume> finishBodyPose(const PrototypeRider&,double seconds);
    std::optional<std::vector<AnimationTransform>> poseAt(const PrototypeRider&,double seconds);
    // Reuse the exact selected layers/clock with another compatible biped rig.
    // Geometry is reskinned; no independent clip selector or RNG is run.
    std::optional<std::vector<AnimationTransform>> poseForRig(const PrototypeRider&,const AnimationRigAsset&,AnimationVector scale)const;
    BodyPoseProvider bodyProvider();
    std::optional<AnimationTransform> preparedLocalTransform(size_t bone)const {if(!sampledLocal||bone>=sampledLocal->size())return std::nullopt;return (*sampledLocal)[bone];}
    std::optional<BodyAnimationState> mainAnimationState()const;
    int currentSemantic(unsigned channel)const{return requestedSemantics.at(channel);}
    int currentClass(unsigned channel)const;
    void fadeChannel(unsigned channel,float seconds);
    // Immediate3128E8 semantics; prepared local/world pose stays cached.
    bool playSemantic(int semantic,float rate=1,uint64_t mask=~uint64_t(0),bool force=false);
    void consumeRiderEvents(const PrototypeRider&);
    void setChannelRate(unsigned channel,float rate);
    void setGrabEndSemantic(int semantic){grabEndSemantic=semantic;}
    void bindVariantRandom(std::function<uint32_t()> random){variantRandom=std::move(random);}
    void setVariantFlags(uint32_t flags){variantFlags=flags;}
    void rotateSequenceRoots(AnimationQuaternion);
    void resetDefaultRoot(AnimationVector,AnimationQuaternion);
    void setDefaultMirror(bool mirror){defaultMirror=mirror;} // anim+0x18 stance switch
    void setStartPose(float pose){startPose=pose;}
    void setRailBalance(float balance){railBalance=balance;} // rider+238 drives the rail balance cycles
    void setHandplantBalance(float balance){handplantBalance=balance;} // rider+244 drives kind-9 handplant seeks
    // rider+268/+274/+280/+330 drive the board-press kinds 13/14/15 and completion kind 9.
    void setBoardPress(float depth268,float depth274,float pivot280,int press330){boardPress268=depth268;boardPress274=depth274;boardPress280=pivot280;boardPress330=press330;}
private:
    float railBalance=0,startPose=0,handplantBalance=0;
    float boardPress268=0,boardPress274=0,boardPress280=0;int boardPress330=0;
public:
    bool isSupported()const{return supported;}
    void setLegWeight(float weight){if(!std::isfinite(weight)||weight<0||weight>1)throw std::runtime_error("Invalid original leg weight");contact.legWeight=weight;}
    void setControllerDriven(bool enabled){controllerDriven=enabled;}
    float semanticDuration(int semantic)const;
    const std::optional<std::vector<AnimationTransform>>& currentWorldPose()const{return cachedWorld;}
    void setActiveBoneMask(uint64_t mask){activeBoneMask=mask;}
    AnimationVector modelScale()const{return scale;}
    const std::vector<OriginalAnimationSequence>& sequenceState()const{return sequences;}
    // Crash/ragdoll support for the10EB30 root bake and12CB68 clip reads.
    // 11FA10 presentation of a physical root using the supplied control values.
    AnimationTransform presentRoot(const AnimationTransform& physical,const RiderRootPresentation&)const;
    // 30ECD8 time-zero root sample of a semantic's clip with the current sequence root/mirror.
    std::optional<AnimationTransform> previewRoot(int semantic)const;
    // Current sampled local root bone multiplied by the authored geometry scale.
    std::optional<AnimationTransform> scaledLocalRoot()const;
    void offsetSequenceRoots(const AnimationTransform&); //311BF0
    std::optional<AnimationTransform> posedBone(unsigned bone)const{if(!cachedWorld||bone>=cachedWorld->size())return std::nullopt;return (*cachedWorld)[bone];}
    bool seekChannel(unsigned channel,float seconds); //312660 primary slot clock
    float channelProgress(unsigned channel)const; //312AB0
    float channelRate(unsigned channel)const;
    float channelDuration(unsigned channel)const;
    // A detached crash board replaces the posed board child bone until cleared.
    void setDetachedBoard(std::optional<AnimationTransform> board){detachedBoard=board;}
private:
    std::optional<AnimationTransform> detachedBoard;
public:
    unsigned boardRootBone()const{return contact.boardRoot;}
    unsigned boardChildBone()const{return contact.boardChild;}
};
// Consumes sequence/profile inputs only. Expected audit bone arrays are ignored.
std::shared_ptr<OriginalRiderAnimation> makeOriginalRiderAnimation(std::shared_ptr<const AnimationRigAsset>,NSDictionary* originalAnimation);
}
