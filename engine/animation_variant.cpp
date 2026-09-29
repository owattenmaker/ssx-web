#include "animation_variant.hpp"
#include <bit>
#include <stdexcept>
namespace ssx {
uint32_t originalAnimationVariant(std::span<const OriginalAnimationVariant> choices,
        uint32_t required,const std::function<uint32_t()>& random){
    if(choices.empty()||choices.size()>32767)throw std::runtime_error("Invalid authored animation variant count");
    if(choices.size()==1)return choices.front().leaf;
    uint32_t sum=0;for(const auto&choice:choices)if((choice.allowedFlags&required)==required)sum+=choice.weight;
    if(!random)throw std::runtime_error("Animation variant requires the shared original RNG");
    uint32_t draw=random();if(sum==0)throw std::runtime_error("No weighted original animation variant is eligible");
    uint32_t remainder=draw%sum;
    for(const auto&choice:choices)if((choice.allowedFlags&required)==required){
        remainder-=choice.weight;
        // Original311810 uses signed <=0 AFTER subtraction. The inclusive
        // boundary is intentional, including a zero-weight first choice at0.
        if(std::bit_cast<int32_t>(remainder)<=0)return choice.leaf;
    }
    return choices.back().leaf; // Original311828 fallback, before leaf load.
}
}
