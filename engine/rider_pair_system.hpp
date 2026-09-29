#pragma once
#include "rider_pair_collision.hpp"
#include <functional>
#include <optional>

namespace ssx {
// A fresh view is requested after callbacks: an earlier participant's contact
// can change this actor's velocity, control state, pose mode, or boost weight.
struct OriginalPairActorView {
    unsigned slot=0;
    int kind880=0;
    bool disabled=false;
    const BodyCollisionVolume* body=nullptr;
    OriginalPairImpulseState impulse;
    OriginalCollisionContext reaction;
    OriginalPairAttackState attack;
    int weightAttribute=0;
    float resolvedCollisionStat=0,boost=0;
};
struct OriginalPairCallbacks {
    std::function<OriginalPairActorView(unsigned)> liveView;
    // 0x106538: move physical position, +9D0, AABB and cached AA0 centers.
    // Do not regenerate geometry bones or move the cached presentation frame.
    std::function<void(unsigned,terrain_original::Vector)> translate;
    // Update live velocity and perform 0x1135B8 reseed when the bool is true.
    std::function<void(unsigned,terrain_original::Vector,bool)> setVelocity;
    // Execute the typed soft/crash request, including its score/attack event.
    // Must report unsupported lifecycle explicitly, rather than discarding it.
    std::function<void(unsigned,unsigned,const OriginalPairReactionRequest&,const CollisionRandom&)> react;
    CollisionRandom randomWord; // The world's one RNG; never a per-rider copy.
    // Optional (online races, web/net/pair-net.js): an owner's attack hit on another actor, reported before its
    // response (victim, attacker, direction, impulse); the victim's own client applies respondToAttack.
    std::function<void(unsigned,unsigned,terrain_original::Vector,float)> attackHit;
};
struct OriginalPairDispatchCounts {
    unsigned checks=0,separations=0,impulses=0,attacks=0,reactions=0;
};
// The AA0 translation portion of 0x106538 -> 0x3098B0 -> 0x329B40.
// Metadata produced from geometry intentionally remains at its posed values.
inline void translateOriginalPairBody(BodyCollisionVolume& body,terrain_original::Vector displacement) {
    using namespace terrain_original;Rounding rounding;
    if(body.count>body.spheres.size())throw std::runtime_error("Original pair body count exceeds storage");
    for(unsigned k=0;k<3;++k)body.broadCenterCm[k]=add(body.broadCenterCm[k],displacement[k]);
    for(unsigned n=0;n<body.count;++n)for(unsigned k=0;k<3;++k)body.spheres[n].centerCm[k]=add(body.spheres[n].centerCm[k],displacement[k]);
}
class OriginalRiderPairSystem {
public:
    static constexpr unsigned capacity=6;
    using Records=std::array<std::array<OriginalPairRecord,capacity>,capacity>;
private:
    unsigned participantCount=0,excludedTail=0;
    Records pairRecords{};
    OriginalPairCallbacks callbacks;
    std::optional<int32_t> proximityTick;
    bool resolving=false;
    OriginalPairActorView view(unsigned slot)const {
        if(slot>=participantCount)throw std::runtime_error("Original pair record names absent participant");
        auto result=callbacks.liveView(slot);
        if(result.slot!=slot)throw std::runtime_error("Original pair live view slot mismatch");
        // These duplicated helper inputs refer to the same original fields.
        result.attack.velocityCmps=result.impulse.velocityCmps;
        result.reaction.velocityCmps=result.impulse.velocityCmps;
        result.reaction.motionMode=result.impulse.motionMode;
        result.reaction.controlState=result.impulse.controlState;
        result.reaction.ragdollSubmode=result.impulse.ragdollSubmode;
        return result;
    }
    void respond(unsigned target,unsigned other,terrain_original::Vector direction,float amount,bool attack,
                 bool knockdownCheat,OriginalPairDispatchCounts& counts) {
        auto before=view(target);auto impulse=originalPairImpulse(before.impulse,direction,amount);
        if(impulse.ignored)return;
        callbacks.setVelocity(target,impulse.velocityCmps,impulse.resetPredictor);
        auto current=view(target);
        auto request=originalPairReactionRequest(impulse,current.reaction,direction,current.attack.positionCm,
                                                pairRecords[target][other].planarDistanceCm,attack,knockdownCheat,callbacks.randomWord);
        if(request.kind!=OriginalPairReactionRequest::Kind::None) {
            ++counts.reactions;
            callbacks.react(target,other,request,callbacks.randomWord);
        }
    }
public:
    OriginalRiderPairSystem(unsigned count,OriginalPairCallbacks operations,unsigned excludedTailCount=0)
        :participantCount(count),excludedTail(excludedTailCount),callbacks(std::move(operations)) {
        if(count==0||count>capacity||excludedTail>count)throw std::runtime_error("Invalid original six-slot pair roster");
        if(!callbacks.liveView||!callbacks.translate||!callbacks.setVelocity||!callbacks.react)
            throw std::runtime_error("Original pair callbacks are incomplete");
        // 0x10F3B8 creates records from actual selected roster and actor kind.
        for(unsigned a=0;a<count;++a){auto owner=view(a);for(unsigned b=0;b<capacity;++b)pairRecords[a][b].enabled=owner.kind880==7&&b<count&&a!=b;}
    }
    unsigned size()const{return participantCount;}
    const Records& records()const{return pairRecords;}
    void seedRecords(Records source) {
        if(resolving)throw std::runtime_error("Cannot reseed pair records during dispatch");
        for(unsigned a=0;a<capacity;++a)for(unsigned b=0;b<capacity;++b)
            if(source[a][b].enabled&&(a>=participantCount||b>=participantCount||a==b))throw std::runtime_error("Invalid seeded pair eligibility");
        pairRecords=std::move(source);proximityTick.reset();
    }
    // Online races: another client's 107888 found its attack hit this actor (attackHit); apply the same
    // 107E70 response here (impulse, then the typed reaction request with the world RNG).
    OriginalPairDispatchCounts respondToAttack(unsigned target,unsigned attacker,terrain_original::Vector direction,float amount,bool knockdownCheat=false) {
        if(resolving)throw std::runtime_error("Recursive original pair dispatch");
        struct Guard {bool& value;Guard(bool& v):value(v){value=true;}~Guard(){value=false;}} guard(resolving);
        terrain_original::Rounding rounding;OriginalPairDispatchCounts counts;
        if(target>=participantCount||attacker>=participantCount||target==attacker)throw std::runtime_error("Invalid attack response slots");
        respond(target,attacker,direction,amount,true,knockdownCheat,counts);++counts.attacks;
        return counts;
    }
    // Called from 0x10F560's shared pre-control phase. The ancillary proximity
    // ranking/AI observers remain world-owned; these are all pair physics fields.
    bool refreshProximity(int32_t tick) {
        if(resolving)throw std::runtime_error("Proximity refresh during pair dispatch");
        if(tick%6!=0||proximityTick==tick)return false;
        proximityTick=tick;unsigned active=participantCount-excludedTail;
        for(unsigned a=0;a<active;++a)for(unsigned b=a+1;b<active;++b)if(pairRecords[a][b].enabled) {
            auto owner=view(a),other=view(b);auto result=originalPairProximity(owner.attack.positionCm,other.attack.positionCm);
            pairRecords[a][b].planarDistanceCm=pairRecords[b][a].planarDistanceCm=result.distanceCm;
            pairRecords[a][b].bearing=result.bearingAToB;pairRecords[b][a].bearing=result.bearingBToA;
        }
        return true;
    }
    // Attach inside the initiating actor's second motion phase at 0x107888,
    // after all actors have posed, not after all actors finish their contacts.
    OriginalPairDispatchCounts resolveActor(unsigned slot,int32_t tick,bool knockdownCheat=false) {
        if(resolving)throw std::runtime_error("Recursive original pair dispatch");
        struct Guard {bool& value;Guard(bool& v):value(v){value=true;}~Guard(){value=false;}} guard(resolving);
        using namespace terrain_original;Rounding rounding;OriginalPairDispatchCounts counts;
        auto initial=view(slot);float ownWeight=originalRiderCollisionWeight(initial.weightAttribute,initial.resolvedCollisionStat,initial.boost);
        for(unsigned otherSlot=0;otherSlot<capacity;++otherSlot) {
            auto& record=pairRecords[slot][otherSlot];if(!record.enabled)continue;
            auto other=view(otherSlot);if(other.disabled)continue;
            auto& reciprocal=pairRecords[otherSlot][slot];
            if(originalPairShouldCheck(record,reciprocal,false,tick)) {
                auto owner=view(slot);
                if(!owner.body||!other.body)throw std::runtime_error("Native pair body pose is unavailable");
                record.lastCheckedTick=reciprocal.lastCheckedTick=tick;++counts.checks;
                auto contact=originalBodyPairContact(*owner.body,*other.body);
                if(contact.hit) {
                    float otherWeight=originalRiderCollisionWeight(other.weightAttribute,other.resolvedCollisionStat,other.boost);
                    auto separation=originalPairSeparation(contact,owner.impulse,other.impulse,ownWeight,otherWeight);
                    callbacks.translate(slot,separation.translationA);callbacks.translate(otherSlot,separation.translationB);++counts.separations;
                    if(originalPairImpulseDue(record,tick)) {
                        respond(slot,otherSlot,separation.direction,separation.impulseA,false,knockdownCheat,counts);
                        respond(otherSlot,slot,separation.direction,separation.impulseB,false,knockdownCheat,counts);
                        counts.impulses+=2;record.lastContactTick=reciprocal.lastContactTick=tick;
                    }
                }
            }
            auto owner=view(slot);other=view(otherSlot);
            // 0x107888 caches the owner's attack marker window before its loop.
            owner.attack.animationClass1=initial.attack.animationClass1;
            owner.attack.marker0=initial.attack.marker0;owner.attack.marker1=initial.attack.marker1;
            auto attack=originalPairAttack(owner.attack,other.attack,reciprocal,tick);
            if(attack.hit) {
                if(callbacks.attackHit)callbacks.attackHit(otherSlot,slot,attack.direction,attack.impulseCmps);
                respond(otherSlot,slot,attack.direction,attack.impulseCmps,true,knockdownCheat,counts);
                ++counts.attacks;reciprocal.lastAttackTick=tick;
            }
        }
        return counts;
    }
};
}
