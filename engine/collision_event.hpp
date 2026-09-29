#pragma once
#include "collision_frame.hpp"
#include "terrain_contact_math.hpp"
#include <functional>

namespace ssx {
struct OriginalCollisionEvent {
    terrain_original::Vector pointCm{},incomingDirection{},normal{};
    float closingSpeedCmps=0;
    int surface=-1,surfaceProperty44=0;
};
struct OriginalCollisionHistory {
    terrain_original::Vector previousNormal{};
    float directionChanges=0,secondaryCounter=0,peakImpactCmps=0;
};
struct OriginalCollisionContext {
    OriginalCollisionFrame presentation,physical;
    terrain_original::Vector velocityCmps{};
    int motionMode=0,controlState=0,animationClass=0,ragdollSubmode=0;
    bool reverseStance=false;
    float manualSpin=0;
};
struct OriginalCollisionProfile {
    float upwardNormal=.800000011920929f;
    float airForward=416.5320129394531f,airSide=833.337646484375f;
    float groundScaleMin=.20000000298023224f,groundScaleMax=1.399999976158142f;
    float groundForward=1388.880859375f,groundSide=2083.227294921875f;
    float fastSpeed=2222.22216796875f;
    float softFront=.7071067690849304f,softBack=-.7071067690849304f;
    float softImpact=1111.111083984375f,softSpeed=1388.888916015625f;
    float spinLimit=6.2831854820251465f,spinNegative=-3.1415927410125732f,spinPositive=3.1415927410125732f,spinStep=3.1415927410125732f;
    float normalDecay=.9800000190734863f,directionDecay=.9559999704360962f,secondaryDecay=.9783333539962769f;
};
enum class OriginalCollisionReactionKind {Ignored,SurfaceReset,RagdollImpact,Soft,Crash};
struct OriginalCollisionReaction {
    OriginalCollisionReactionKind kind=OriginalCollisionReactionKind::Ignored;
    int animation=-1,nextControlState=-1;
    float manualSpin=0;
    bool resetPredictor=false,ragdollCollisionFlag=false,cancelControlOne=false,strongSoftImpact=false;
    unsigned randomDraws=0;
};
using CollisionRandom=std::function<uint32_t()>;

// Original1210B0 runs before the control/motion phases. The constants are per
// original fixed tick, not exponential rates rescaled by elapsed wall time.
inline bool originalCollisionHistoryDecay(const OriginalCollisionProfile& p,OriginalCollisionHistory& history) {
    using namespace terrain_original;Rounding rounding;
    for(auto& x:history.previousNormal)x=mul(x,p.normalDecay);
    history.directionChanges=mul(history.directionChanges,p.directionDecay);
    history.secondaryCounter=mul(history.secondaryCounter,p.secondaryDecay);
    return history.directionChanges>4.5f; // Original recovery reason2.
}
// Original 1210B0 after the decay: the reason-2 reset request 116120(rider,0,2). motion = 11FE98; crashAir = the crash
// submode RFX+0x30 == 1 in motion 2. In the air a live prediction (+0xAC status 1 or 3) with more than 45 s left
// (+0x98 - +0xA0, scalar sub.s) or an air bounce counter +0x3F4 above 5 also resets.
inline bool originalCollisionTimerReset(const OriginalCollisionHistory& history,int motion,bool crashAir,int predictorStatus,float predictedTime,float elapsed) {
    if(4.5f<history.directionChanges)return true;
    if(!(motion==1||(motion==2&&crashAir)))return false;
    if(predictorStatus==1||predictorStatus==3){terrain_original::Rounding rounding;if(45.f<originalScalarSubtract(predictedTime,elapsed))return true;}
    return 5.f<history.secondaryCounter;
}

// Standalone108388 entry: pair collisions bypass the105D98 scenery classifier.
// The caller performs requested control1 cancellation before applying this
// selection, and dispatches the resulting animation before entering control3.
inline OriginalCollisionReaction originalSoftCollisionReaction(const OriginalCollisionProfile& p,
        const OriginalCollisionContext& c,const OriginalCollisionEvent& event,const CollisionRandom& random) {
    using namespace terrain_original;Rounding rounding;OriginalCollisionReaction result;result.manualSpin=c.manualSpin;int animation=-1;
    auto next=[&](){if(!random)throw std::runtime_error("Original collision random source unavailable");++result.randomDraws;return random();};
    // Original108388: eligible grounded modes/states select small impact clips,
    // with the stronger side reaction changing manual spin by an original RNG draw.
    if(c.motionMode!=0&&c.motionMode!=4)return result;
    if(c.controlState!=0&&c.controlState!=1&&c.controlState!=2&&c.controlState!=7)return result;
    result.cancelControlOne=c.controlState==1;
    float physicalForward=dot(c.physical.forward,event.normal);
    if(p.softFront<physicalForward)animation=55;
    else if(physicalForward<p.softBack)animation=56;
    else {
        float side=dot(c.physical.right,event.normal);if(c.reverseStance)side=-side;
        float speed=terrain_original::sqrt(dot(c.velocityCmps,c.velocityCmps));
        bool strong=event.closingSpeedCmps<p.softImpact&&p.softSpeed<speed&&c.motionMode!=4;
        if(strong) {
            float spin=c.manualSpin;
            if(std::abs(spin)<p.spinLimit) {
                if(spin<0)spin=originalScalarAdd(spin,mul(float(next()%3),p.spinNegative));
                else if(0<spin)spin=originalScalarAdd(spin,mul(float(next()%3),p.spinPositive));
                else spin=originalScalarAdd(spin,mul(float(int(next()%5)-2),p.spinStep));
            }
            result.manualSpin=spin;result.strongSoftImpact=true;animation=side<0?58:60;
        } else animation=side<0?57:59;
    }
    result.kind=OriginalCollisionReactionKind::Soft;result.animation=animation;result.nextControlState=3;return result;
}

// Original105D98 plus108388 reaction selection. External animation/controller
// dispatch is described by the result, rather than being silently discarded.
inline OriginalCollisionReaction originalCollisionReaction(const OriginalCollisionProfile& p,
        OriginalCollisionHistory& history,const OriginalCollisionContext& c,const OriginalCollisionEvent& event,
        const CollisionRandom& random) {
    using namespace terrain_original;Rounding rounding;OriginalCollisionReaction result;result.manualSpin=c.manualSpin;
    if(c.controlState==9)return result;
    float correlation=std::max(dot(event.normal,history.previousNormal),0.f);
    history.directionChanges=originalScalarAdd(history.directionChanges,originalScalarSubtract(1.f,correlation));
    history.previousNormal=event.normal;
    if(event.surface!=-1&&event.surfaceProperty44!=0){result.kind=OriginalCollisionReactionKind::SurfaceReset;result.nextControlState=9;return result;}
    history.peakImpactCmps=std::max(event.closingSpeedCmps,history.peakImpactCmps);
    if(c.motionMode==2){result.kind=OriginalCollisionReactionKind::RagdollImpact;result.resetPredictor=c.ragdollSubmode==1;result.ragdollCollisionFlag=true;return result;}
    result.resetPredictor=c.motionMode==1;
    float right=dot(c.presentation.right,event.normal),forward=dot(c.presentation.forward,event.normal),up=dot(c.presentation.up,event.normal);
    float contactHeight=dot(difference(event.pointCm,c.presentation.origin),c.presentation.up);
    if(c.controlState==11)return result;
    auto next=[&](){if(!random)throw std::runtime_error("Original collision random source unavailable");++result.randomDraws;return random();};
    int animation=438;
    auto largestForward=[&](){return std::abs(right)<std::abs(forward)&&std::abs(up)<std::abs(forward);};
    auto largestUp=[&](){return std::abs(right)<std::abs(up)&&std::abs(forward)<std::abs(up);};
    auto frontal=[&](bool airborne) {
        if(forward<0) {
            float speed=terrain_original::sqrt(dot(c.velocityCmps,c.velocityCmps));
            if(contactHeight<60&&p.fastSpeed<speed)return airborne?342:332;
            unsigned choice=next()%(p.fastSpeed<speed?6:3);
            constexpr int groundChoices[]={334,335,337,333,336,336};
            constexpr int airChoices[]={347,343,344,346,345,345};
            return airborne?airChoices[choice]:groundChoices[choice];
        }
        return (next()&1)?(airborne?341:331):(airborne?340:330);
    };
    if(c.motionMode==1&&p.upwardNormal<event.normal[2]) {
        if(c.animationClass==20||c.animationClass==21)animation=361;
        else if(largestForward())animation=forward<0?354:355;
        else if(largestUp()) {
            if(up<0)animation=350;
            else if(c.animationClass==18||c.animationClass==19)animation=351;
        } else animation=right<0?357:356;
    } else if(c.motionMode==1) {
        if(largestForward()){if(p.airForward<event.closingSpeedCmps)animation=frontal(true);}
        else if(largestUp()) {
            if(up<0)animation=350;
            else if(c.animationClass>=18&&c.animationClass<=21)animation=351;
        } else if(p.airSide<event.closingSpeedCmps)animation=right<0?339:338;
        if(animation!=438&&(c.animationClass==20||c.animationClass==21))animation=361;
    } else {
        float scale=originalScalarSubtract(originalScalarAdd(c.physical.up[2],1.f),std::abs(event.normal[2]));
        scale=std::clamp(scale,p.groundScaleMin,p.groundScaleMax);
        if(largestForward()){if(mul(scale,p.groundForward)<event.closingSpeedCmps)animation=frontal(false);}
        else if(!largestUp()&&mul(scale,p.groundSide)<event.closingSpeedCmps)animation=right<0?329:328;
    }
    if(animation!=438){result.kind=OriginalCollisionReactionKind::Crash;result.animation=animation;return result;}
    auto soft=originalSoftCollisionReaction(p,c,event,random);
    soft.resetPredictor=result.resetPredictor;
    return soft;
}
}
