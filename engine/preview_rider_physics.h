#pragma once
#include "rider_animation_player.h"
#include "rider_pose_stages.h"
#include "original_random.hpp"
// The preview's wardrobe changes are cosmetic. Its recovered physics start
// must keep the matching original rig for board/contact queries.
inline std::shared_ptr<ssx::OriginalRiderAnimation> makePreviewCollisionAnimation(
    NSString* assetRoot,std::shared_ptr<const ssx::AnimationRigAsset> visibleRig,NSDictionary* animation){
    if(!animation)return {};
    NSString* character=animation[@"character"];
    if(![@[@"zoe",@"psymon",@"allegra",@"moby",@"griff",@"luther"] containsObject:character])
        throw std::runtime_error("Unknown physics-start animation character");
    auto rig=visibleRig;
    if(!rig||rig->character!=character.UTF8String){
        NSString* folder=[assetRoot stringByAppendingPathComponent:[@"RIDER_" stringByAppendingString:character.uppercaseString]];
        rig=std::make_shared<const ssx::AnimationRigAsset>(ssx::loadOriginalAnimationRig(folder));
    }
    auto player=ssx::makeOriginalRiderAnimation(rig,animation);
    if(NSArray* words=animation[@"random_state"]){
        if(words.count!=6)throw std::runtime_error("Invalid preview animation random seed");
        auto random=std::make_shared<ssx::OriginalRandomState>();
        for(unsigned i=0;i<6;++i){if(![words[i] isKindOfClass:NSNumber.class]||[words[i] doubleValue]<0||[words[i] doubleValue]>UINT32_MAX)throw std::runtime_error("Invalid preview random word");random->words[i]=[words[i] unsignedIntValue];}
        player->bindVariantRandom([random](){return random->next();});
    }
    return player;
}
inline ssx::RiderPoseStages previewCollisionStages(std::shared_ptr<ssx::OriginalRiderAnimation> player){
    auto stages=ssx::originalPoseStages(player);if(!player)return stages;
    stages.prepareLocal=[player](const ssx::PrototypeRider& rider,double seconds){
        if(!player->prepareLocalPose(rider,seconds))throw std::runtime_error("This pose is not implemented yet; landing collision paused. Press R to restart.");return true;};
    stages.finishWorld=[player](const ssx::PrototypeRider& rider,double seconds){auto body=player->finishBodyPose(rider,seconds);
        if(!body)throw std::runtime_error("Landing collision pose unavailable; press R to restart.");return body;};
    return stages;
}
