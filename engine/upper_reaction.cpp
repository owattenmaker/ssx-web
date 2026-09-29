#include "upper_reaction.hpp"
#include "ground_motion.hpp"
#include "original_float.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {namespace {
// OriginalRounding also drives the software toward-zero policy in WebAssembly, where fesetround is inert.
struct Round {OriginalRounding rounding{FE_TOWARDZERO};};
float constant(uint32_t bits){return std::bit_cast<float>(bits);}
float heading(float y,float x){
    if(x==0)return y==0?y:y>=0?constant(0x3fc90fdb):constant(0xbfc90fdb);
    float result=originalAtan(originalScalarDivide(y,x));
    if(x<0)result=y>0?originalScalarAdd(result,constant(0x40490fdb)):originalScalarSubtract(result,constant(0x40490fdb));
    return result;
}
float relative(float bearing,float direction){
    float angle=originalScalarSubtract(bearing,direction);
    float scaled=originalScalarAdd(angle*constant(0x3e22f983),.5f);
    float turns=float(int32_t(scaled));if(scaled<turns)turns=originalScalarSubtract(turns,1.f);
    return originalScalarSubtract(angle,turns*constant(0x40c90fdb));
}
}
OriginalUpperReaction originalUpperReaction(float& idle,std::array<OriginalUpperPeer,6>& peers,
        const OriginalUpperReactionContext& c,const std::function<uint32_t()>& random){
    Round rounding;OriginalUpperReaction result;
    if(c.upperClass!=0){idle=0;return result;}
    idle=originalScalarAdd(idle,constant(0x3c888889));if(idle<1)return result;
    float direction=heading(c.physicalForward[1],c.physicalForward[0]);
    auto next=[&](){if(!random)throw std::runtime_error("Upper reaction requires shared original RNG");++result.randomDraws;return random();};
    if(next()%100<90)for(unsigned i=0;i<peers.size();++i){auto&peer=peers[i];
        if(!peer.enabled||!peer.attackEligible||std::bit_cast<int32_t>(c.clockTick)<std::bit_cast<int32_t>(peer.lastReactionTick+600u))continue;
        float angle=relative(peer.bearing,direction);
        if(peer.distanceCm<1000&&constant(0x3f860a93)<std::abs(angle)){
            peer.lastReactionTick=c.clockTick;result.peer=i;result.mask=c.reactionMask;
            result.semantic=constant(0x4016cbe5)<std::abs(angle)?319:((angle<0)==c.reverseStance?321:320);return result;
        }
    }
    // Original's second percentage test is always true, but the draw is real.
    (void)next();
    for(unsigned i=0;i<peers.size();++i){const auto&peer=peers[i];if(!peer.enabled)continue;
        float angle=relative(peer.bearing,direction);
        if(peer.distanceCm<1000&&constant(0x40060a93)<std::abs(angle)){
            result.peer=i;result.mask=c.lookbackMask;result.semantic=(angle<0)==c.reverseStance?317:316;return result;
        }
    }
    return result;
}
}
