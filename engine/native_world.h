#pragma once
#include "rider_world.hpp"
#include "rider_animation_player.h"
#include "npc_input.hpp"
#include "npc_air.hpp"
#include "upper_reaction.hpp"
#include "grab_lifecycle.hpp"
namespace ssx {
struct NativeGrabScoreRequest {uint32_t tick=0;unsigned rider=0;int index=-1;bool begin=false;};
// Owns native actors, their animation players and recovered AI state. The
// course collision assets must outlive this object. No snapshot is read here.
class NativeRaceWorld {
    struct Participant {
        std::unique_ptr<PrototypeRider> rider;
        std::shared_ptr<const AnimationRigAsset> rig;
        std::shared_ptr<OriginalRiderAnimation> animation;
        OriginalNpcDrivingState driving;
        OriginalNpcProviderContext context;
        OriginalPairActorView pair;
        OriginalNpcAirContext air;
        OriginalNpcGrabCatalog grabs;
        OriginalGrabState grabState;
        OriginalGrabProfile grabProfile;
        float upperIdleSeconds=0;
        OriginalUpperReactionContext upperContext;
        std::array<OriginalUpperPeer,6> upperPeers{};
        int followedSlot=-1;
        bool human=false;
        std::array<uint32_t,2> commands{};
    };
    std::vector<Participant> participants;
    std::vector<OriginalNpcPath> paths;
    std::array<std::array<int,6>,6> relationships{};
    std::vector<int> occupancy;
    std::array<int,0x1b6> animationClasses{};
    std::unique_ptr<RiderWorld> simulation;
    WorldControls humanControls;
    int8_t eventVariant=0;
    std::vector<NativeGrabScoreRequest> grabScoreRequests;
    void refreshContext(size_t);
    RiderInput controls(size_t);
    void progress(size_t);
    void upperReaction(size_t);
    bool grabControl(size_t,const RiderInput&);
public:
    NativeRaceWorld(NSDictionary* source,NSString* assetRoot,const CollisionWorld&,const CollisionWorld&,
        const WorldBodyCollision*,Vec3 spawn,Vec3 normal,WorldControls humanInput);
    NativeRaceWorld(const NativeRaceWorld&)=delete;
    NativeRaceWorld& operator=(const NativeRaceWorld&)=delete;
    RiderWorld& world(){return *simulation;}
    const std::array<uint32_t,2>& commands(size_t slot)const{return participants.at(slot).commands;}
    const OriginalNpcProviderContext& npcContext(size_t slot)const{return participants.at(slot).context;}
    const OriginalNpcDrivingState& npcState(size_t slot)const{return participants.at(slot).driving;}
    std::shared_ptr<OriginalRiderAnimation> animator(size_t slot)const{return participants.at(slot).animation;}
    const std::vector<NativeGrabScoreRequest>& pendingGrabScores()const{return grabScoreRequests;}
};
}
