#pragma once
// Original sequence-completion dispatch 0x103918 (the callback that 0x312490 runs
// for every channel sequence whose flag bit 63 is latched at +0xB0). The state's
// completion kind (state table +8) indexes the jump table 0x456990:
//
//   0 0x104CA0  remove the sequence (0x3145F8), requested semantic 438
//   1 0x104A40  0x312B18(...,287)
//   2 0x104A60  semantics 269..286 -> 0x312B18 with table 0x4569E0; a 438 entry
//               or any other semantic returns 438 and leaves the sequence alone
//   3 0x104B78  0x312B18(...,5)
//   4 0x104C18  0x312BD0(...,5)
//   5 0x104B48  0x312BD0(..., animator+0x64 != 0 ? 436 : 287)
//   6 0x104C38  0x312BD0(..., 69 -> 20, 70 -> 19, otherwise 18)   (rail entries 68..70, rail spins 49..54)
//   7 0x104B98  0x312B18(...,3)
//   8 0x104BB8  0x312B18(...,19)                                (RSFS_OUTOF_GRINDn)
//   9 0x104BD8  0x312BD0(..., *(animator+0x60)+0x330 == 1 ? 28 : 36)
//  10 0x104C80  0x312BD0(...,434)
//
// Replace (0x312B18): 0x3145F8 removes the finished sequence, 0x3128E8 plays the
// semantic (blend -1, flags 0), 0x314760 fetches the channel head and gets the old
// weight (+0x94) as weight and target (+0x98); a running old fade (+0x9C != 0)
// then runs 0x313A10(head, old target, old fade).
// Play (0x312BD0): 0x144670 moves bit 63 from +0xB0 to the raised word +0xB8 and
// 0x3128E8 plays the semantic (blend -1, flags 0): the finished sequence stays and
// fades out like any other channel sequence.
// Verified against the recompiled dispatch by tools/test_animation_completion_native.py.
#include <cstdint>
#include <stdexcept>
namespace ssx {
struct OriginalAnimationCompletion {
    enum class Action {Remove,Keep,Replace,Play} action=Action::Keep;
    int semantic=438; // played semantic (Replace/Play) or the requested-slot value (Remove/Keep)
};
inline OriginalAnimationCompletion originalAnimationCompletion(uint32_t kind,int semantic,uint32_t animatorWord64=0,int32_t boardPress330=0){
    using A=OriginalAnimationCompletion::Action;
    switch(kind){
    case 0:return {A::Remove,438};
    case 1:return {A::Replace,287};
    case 2:{
        constexpr int next[18]={289,290,291,292,293,294,295,296,438,289,290,292,292,438,289,290,291,292};
        const uint32_t index=uint32_t(semantic-269);
        if(index>=18||next[index]==438)return {A::Keep,438};
        return {A::Replace,next[index]};
    }
    case 3:return {A::Replace,5};
    case 4:return {A::Play,5};
    case 5:return {A::Play,animatorWord64?436:287};
    case 6:return {A::Play,semantic==69?20:(semantic==70?19:18)};
    case 7:return {A::Replace,3};
    case 8:return {A::Replace,19};
    case 9:return {A::Play,boardPress330==1?28:36};
    case 10:return {A::Play,434};
    default:throw std::runtime_error("Completion kind outside table 0x456990");
    }
}
}
