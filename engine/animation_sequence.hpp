#pragma once
#include "animation_motion.hpp"
#include <vector>
namespace ssx {
struct AnimationStateDefinition {int semantic=0,animationClass=0,kind=0,channel=2;uint32_t completionKind=0;float blendSeconds=0,firstFadeIn=0,endFadeOut=0;uint32_t initialClip=0,followupClip=0;};
//3128E8 passes *(0x48D808+4*channel) to 311F00 as the new sequence+84 priority.
inline int originalChannelPriority(int channel){constexpr int table[6]={3,2,1,0,0,0};return channel>=0&&channel<6?table[channel]:0;}
struct OriginalAnimationSlot {uint32_t clip=0;float time=0,duration=0,rate=1,weight=1;bool loop=false,enabled=false;};
struct OriginalAnimationSequence {
    int semantic=0,channel=0,priority=0;uint64_t mask=~uint64_t(0);
    std::vector<OriginalAnimationSlot> slots;
    float rate=1,weight=1,targetWeight=1,fadeRemaining=0,startFadeIn=0,endFadeOut=0;
    bool stopWhenFaded=false,completionEnabled=true,completed=false;uint64_t flags=0;AnimationTransform root;bool mirror=false;
    uint64_t raisedFlags=0;bool seekPending=true;
    size_t completionBatchToken=0; // Host identity during312490 callback dispatch; not a source field.
};
//312490 first collects flagged nodes by channel/list order, then invokes callbacks.
// Newly entered sequences have token0 and cannot enter the current batch.
template<class Callback> void originalAnimationCompletionBatch(std::vector<OriginalAnimationSequence>& sequences,Callback callback){
    std::vector<size_t> pending;
    for(size_t i=0;i<sequences.size();++i)sequences[i].completionBatchToken=i+1;
    for(int channel=0;channel<6;++channel)for(const auto& sequence:sequences)
        if(sequence.channel==channel&&(sequence.flags&(uint64_t(1)<<63)))pending.push_back(sequence.completionBatchToken);
    for(size_t token:pending){
        size_t index=0;while(index<sequences.size()&&sequences[index].completionBatchToken!=token)++index;
        if(index<sequences.size())callback(index);
    }
}
struct OriginalAirAnimationChoice {int semantic;float rate=1;};
OriginalAirAnimationChoice originalAirLandingAnimation(int semantic,int trajectoryStatus,float predictedTime,float elapsed,float outDuration); //134B80..134C3C
//1045D8 kind8: seek start stance by owner+2A0, advancing fades only.
bool originalAnimationStartStep(OriginalAnimationSequence&,float pose,float timeScale);
// Original104358 driverkind7: sampled prewind extent, never elapsed clip time.
bool originalAnimationPrewindStep(OriginalAnimationSequence&,float spin,float flip,float timeScale);
//1043F8 kind11: seek primary pose by filtered28C/298 magnitude and blend
// a secondary looping cycle. Returntrue requests removal after sequence fade.
uint32_t originalAirAdjustSecondaryLeaf(int semantic);
bool originalAnimationAirAdjustStep(OriginalAnimationSequence&,uint32_t secondaryClip,
    float secondaryDuration,float adjustment28C,float adjustment298,float timeScale);
void originalAnimationFadeOut(OriginalAnimationSequence&,float duration); //313A20
bool originalAnimationFadeStep(OriginalAnimationSequence&,float dt); //313800
// Returns original sequence+C0 boundary status, not the original function return.
bool originalAnimationSlotStep(OriginalAnimationSlot&,float sequenceRate,float dt); //3135B0 clock only
std::vector<AnimationLayer> originalAnimationLayers(const std::vector<OriginalAnimationSequence>&);
}
