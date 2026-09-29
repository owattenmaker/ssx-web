#pragma once
#include "rider_animation_player.h"
namespace ssx {
inline RiderPoseStages originalPoseStages(std::shared_ptr<OriginalRiderAnimation> player){
    if(!player)return {};
    return {[player](const PrototypeRider& rider,double seconds){return player->prepareLocalPose(rider,seconds);},
            [player](const PrototypeRider& rider,double seconds){return player->finishBodyPose(rider,seconds);},
            [player](){return player->mainAnimationState();}};
}
}
