#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>
namespace ssx {
using AnimationVector=std::array<float,3>;
using AnimationQuaternion=std::array<float,4>;
struct AnimationTransform {AnimationVector position{};AnimationQuaternion rotation{0,0,0,1};};
// Original source coordinates: centimeters, Z-up. No renderer normalization.
AnimationQuaternion originalAnimationQuaternion(AnimationVector);
AnimationTransform originalAnimationCompose(const AnimationTransform& parent,const AnimationTransform& local,AnimationVector scale={1,1,1});
class AnimationPacket {
    struct Channel {unsigned type=0;std::array<float,4> parameters{};std::vector<uint16_t> samples;};
    std::vector<Channel> channels;
    unsigned frames_=0;
public:
    AnimationPacket(std::span<const uint8_t> data,unsigned frames,bool bigEndianSamples=true);
    std::vector<float> sample(float frame) const;
    unsigned frames()const{return frames_;}
};
}
namespace ssx {
struct AnimationBone {int parent=-1,part=0,translationChannel=-1,rotationChannel=-1;AnimationTransform bind;int mirrorSource=-1;std::array<uint8_t,4> mirrorQuaternion{0,1,2,3};AnimationVector mirrorTranslation{1,1,1};};
struct AnimationSegment {int part=0;AnimationPacket packet;};
struct AnimationClip {uint32_t id=0;float duration=0;std::vector<AnimationSegment> segments;std::vector<float> eventTimes;std::vector<float> sample(int part,float seconds)const;};
struct AnimationLayer {uint32_t clip=0;float time=0,weight=1;int priority=0;uint64_t mask=~uint64_t(0);AnimationTransform root;bool mirror=false;};
std::vector<AnimationTransform> originalAnimationLocalPose(const std::vector<AnimationBone>&,const std::vector<AnimationClip>&,const std::vector<AnimationLayer>&,uint64_t activeBoneMask=~uint64_t(0));
// Morph weights of one morph part (30F2B0's morph path, 0x30F7E0..0x30F97C and 0x3100A4): the part's stream `part`
// samples `count` channels; a layer covers the part's slot bit like a bone; a mirrored layer reads channel mirror[i].
std::vector<float> originalAnimationMorphWeights(const std::vector<AnimationClip>&,const std::vector<AnimationLayer>&,int part,unsigned slotBit,unsigned count,const std::vector<uint8_t>& mirror);
std::vector<AnimationTransform> originalAnimationWorldPose(const std::vector<AnimationBone>&,const std::vector<AnimationTransform>&,AnimationTransform root,AnimationVector scale,const std::vector<AnimationTransform>& roots={});
}
