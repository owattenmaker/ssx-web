#include "animation_motion.hpp"
#include <cassert>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdio>
using namespace ssx;
static void float24(std::vector<uint8_t>&b,float f){uint32_t x=std::bit_cast<uint32_t>(f);assert(!(x&255));for(unsigned i=1;i<4;++i)b.push_back(uint8_t(x>>(i*8)));}
static AnimationPacket constant(std::array<float,3> value){std::vector<uint8_t> data{3,0x11,0x10};for(float v:value)float24(data,v);return {data,2};}
int main(){
    std::vector<uint8_t> packet{1,0x70};float24(packet,1);float24(packet,.5);packet.insert(packet.end(),{0,100,200});AnimationPacket half(packet,5);
    std::fesetround(FE_UPWARD);
    for(unsigned i=0;i<5;++i){assert(half.sample(float(i))[0]==float(1+25*i));assert(std::fegetround()==FE_UPWARD);}
    assert(half.sample(-100)[0]==1&&half.sample(100)[0]==101);
    bool rejected=false;try{AnimationPacket truncated(std::span(packet.data(),packet.size()-1),5);}catch(...){rejected=true;}assert(rejected);
    AnimationBone body;body.translationChannel=0;body.part=0;AnimationBone hair=body;hair.part=19;
    std::vector<AnimationClip>clips;
    clips.push_back({1,1,{{0,constant({1,2,3})}}});clips.push_back({2,1,{{0,constant({4,5,6})}}});clips.push_back({3,1,{{19,constant({7,8,9})}}});
    AnimationClip overlap{4,2.f/30,{{0,constant({1,2,3})},{0,constant({4,5,6})}}};
    assert(overlap.sample(0,0)[0]==1&&overlap.sample(0,1.f/30)[0]==4&&overlap.sample(0,2.f/30)[0]==4);
    auto pose=originalAnimationLocalPose({body,hair},clips,{{1,0,1,2,1},{2,0,1,1,~uint64_t(0)},{3,0,1,0,~uint64_t(0)}});
    assert((pose[0].position==AnimationVector{1,2,3}));assert((pose[1].position==AnimationVector{7,8,9}));
    AnimationTransform root{{1,2,3},{0,0,0,1}},child{{4,5,6},{0,0,0,1}};
    auto composed=originalAnimationCompose(root,child,{2,3,4});assert((composed.position==AnimationVector{9,17,27}));assert(std::fegetround()==FE_UPWARD);
    std::fesetround(FE_TONEAREST);puts("Original animation packet boundaries, layer masks/parts and scoped rounding passed");
}
