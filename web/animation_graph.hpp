#pragma once
#include "../engine/animation_motion.hpp"
#include "../engine/animation_sequence.hpp"
#include "../engine/animation_variant.hpp"
#include "../engine/animation_cycle.hpp"
#include "../engine/rail_animation.hpp"
#include "../engine/animation_completion.hpp"
#include "../engine/board_press_animation.hpp"
#include "../engine/animation_events.hpp"
#include "../engine/ground_motion.hpp"
#include "../engine/ground_animation_control.hpp"
#include "../engine/grab_lifecycle.hpp"
#include "../engine/original_random.hpp"
#include "../engine/original_float.hpp"
#include <map>
#include <memory>
#include <optional>
#include <algorithm>
#include <bit>
#include <stdexcept>
namespace ssx {
struct BrowserRig {
 std::map<int,AnimationStateDefinition> stateDefinitions;
 std::map<int,std::vector<OriginalAnimationVariant>> animationVariants;
 std::map<uint32_t,uint32_t> variantClips;
 std::map<int,std::array<uint32_t,5>> cycleMaps;
 std::map<int,std::array<uint32_t,3>> threeWayMaps;
 std::vector<AnimationBone> bones;std::vector<AnimationClip> clips;
};
class BrowserAnimationGraph {
public:
 std::shared_ptr<BrowserRig> rig;
 std::vector<OriginalAnimationSequence> sequences;
 std::array<int,6> requestedSemantics{438,438,438,438,438,438};
 uint32_t variantFlags=0;std::function<uint32_t()> variantRandom;
 std::optional<std::vector<AnimationTransform>> sampledLocal;AnimationVector scale{1,1,1};uint64_t activeBoneMask=~uint64_t(0);
 std::optional<AnimationTransform> previewRoot(int)const;std::optional<AnimationTransform> scaledLocalRoot()const;void offsetSequenceRoots(const AnimationTransform&);
 bool seekChannel(unsigned,float);float channelProgress(unsigned)const;float channelRate(unsigned)const;float channelDuration(unsigned)const;
 AnimationTransform defaultRoot;bool defaultMirror=false,supported=true;
 int grabEndSemantic=287;float railBalance=0,startPose=0,handplantBalance=0;
 float boardPress268=0,boardPress274=0,boardPress280=0;int boardPress330=0; //rider+268/+274/+280/+330 (board-press kinds 13/14/15, completion 9)
 float duration(uint32_t)const;float semanticDuration(int)const;
 float nextRate=1; //animator +0x1C (setter 0x3158E0): 311F00 gives every new sequence this rate, rate < 0 plays with it (3128E8)
 bool enter(int,float rate=-1,uint64_t mask=~uint64_t(0),bool force=false);
 void advance(const OriginalGroundState&,float,float);
 void completeSequences();
 int currentClass(int channel){auto i=rig->stateDefinitions.find(requestedSemantics.at(channel));return i==rig->stateDefinitions.end()?0:i->second.animationClass;}
 //311B20/314760 returns only the first playback sequence for rate writes.
 void setRate(unsigned channel,float rate){for(auto&s:sequences)if(s.channel==int(channel)){s.rate=rate;break;}}
 //311E88 clears the requested slot and removes completion63 from every
 // playback sequence on the channel; their poses remain during the fade.
 void fade(unsigned channel,float seconds){requestedSemantics.at(channel)=438;for(auto&s:sequences)if(s.channel==int(channel)){s.completionEnabled=false;s.flags&=~(uint64_t(1)<<63);originalAnimationFadeOut(s,seconds);}}
 uint64_t flags(int channel){for(auto&s:sequences)if(s.channel==channel)return s.flags;return 0;}
};
}
