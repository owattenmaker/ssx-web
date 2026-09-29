#pragma once
#import <Foundation/Foundation.h>
#include "animation_motion.hpp"
#include "animation_sequence.hpp"
#include "animation_variant.hpp"
#include <string>
#include <map>
namespace ssx {
struct AnimationRigAsset {std::string character;std::map<int,AnimationStateDefinition> stateDefinitions;std::map<int,std::vector<OriginalAnimationVariant>> animationVariants;std::map<uint32_t,uint32_t> variantClips;std::map<int,std::array<uint32_t,5>> cycleMaps;std::map<int,std::array<uint32_t,3>> threeWayMaps;std::vector<AnimationBone> bones;std::vector<AnimationClip> clips;};
AnimationRigAsset loadOriginalAnimationRig(NSString* folder);
AnimationRigAsset loadOriginalAnimationBindings(NSString* folder);
std::vector<AnimationLayer> readOriginalAnimationLayers(NSArray* layers);
}
