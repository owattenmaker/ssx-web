#pragma once
// Original instance-contact phase 0x105398 and its callees 0x104E70 (selection,
// normal aggregation, selected-entity callback), 0x1231A8 (ground-normal
// predicate) and 0x1057B8 (response + 0x105D98 notification).
// Source Z-up centimeters. VU instructions use the terrain_original helpers
// (current chop rounding); EE add.s/sub.s keep the one-guard-bit policy and
// div.s/sqrt.s round to nearest. VU dot products include the w-lane product
// (vmul, vadday.x, vmaddaz.x, vmaddw.x). The VU Q-unit follows the recompiled
// semantics verified by the oracle: VSQRT sqrt(max(0,x)), VDIV x!=0?1/x:0,
// VRSQRT x>0?1/sqrt(x):0.
#include "pickup_contact_selection.hpp"
#include "collision_scalar.hpp"
#include <functional>
#include <optional>
#include <span>

namespace ssx {
using ContactQuad=std::array<float,4>;
namespace instance_contact_math {
// The VU0 horizontal dot (vmul, vadday.x, then vmaddaz.x / vmaddw.x with the 1.0 vector as fs, e.g. 0x1055C8..0x1055D4): the z and w
// products go through 1.0 x (docs/ps2-float.md "The VU0 forms").
inline float dot4(const ContactQuad& a,const ContactQuad& b){
    using namespace terrain_original;
    float v=add(mul(a[0],b[0]),mul(a[1],b[1]));v=add(v,mul(1.f,mul(a[2],b[2])));return add(v,mul(1.f,mul(a[3],b[3])));
}
inline ContactQuad scale4(ContactQuad a,float s){for(auto& x:a)x=terrain_original::mul(x,s);return a;}
inline ContactQuad add4(ContactQuad a,const ContactQuad& b){for(unsigned k=0;k<4;++k)a[k]=terrain_original::add(a[k],b[k]);return a;}
inline ContactQuad sub4(ContactQuad a,const ContactQuad& b){for(unsigned k=0;k<4;++k)a[k]=terrain_original::sub(a[k],b[k]);return a;}
inline float vuSqrt(float x){return x>0?terrain_original::sqrt(x):0.f;}
inline float vuReciprocal(float x){return x!=0?terrain_original::div(1.f,x):0.f;}
inline float vuRsqrt(float x){return x>0?terrain_original::div(1.f,terrain_original::sqrt(x)):0.f;}
inline float eeMin(float a,float b){return b<a?b:a;}  // min.s as recompiled (std::min)
inline float eeMax(float a,float b){return a<b?b:a;}  // max.s as recompiled (std::max)
}

// 12-byte descriptor node entry at *(instance+0x88)+16+12*node.
struct OriginalInstanceContactNode {
    float value=0;        // +0 (nonzero with flag bit0: priority; response mass; 0 = soft 108388)
    float auxiliary=0;    // +4 (response restitution, clamped to [0,1])
    uint16_t flags=0;     // +8 bit0 physical response, bit1 contact copy to rider+9E0
    int16_t surface=-1;   // +A
};
// Modeled fields of the 128-byte 334888 contact packet.
struct OriginalInstanceContactPacket {
    ContactQuad point{0,0,0,1},normal{},surfaceVelocity{}; // +0x00,+0x10,+0x20
    float depth=0;                                          // +0x40
    int32_t surface=-1;                                     // +0x4C (0x105D98 kind)
    uint32_t instance=0;                                    // identity instance+0x78 (+0x50 points at the instance)
    unsigned node=0;                                        // +0x5C
    // Instance data reached through +0x50, not packet bytes.
    OriginalInstanceContactNode descriptor{};
    uint32_t instanceFlags=0;                               // instance+8
    bool entity=false;                                      // instance+0xC != 0
};
// sp+0x80 record passed to 0x1057B8/0x105D98 and copied to rider+0xA60.
// Lanes y/z/w of +0x30 are stale stack words in the original and not modeled.
// 0x105398 never writes sp+0xB4..0xBF, and 0x105718 copies them to rider+0xA94..0xA9F as they were. The EE oracle at
// event-race-ai tick 1599 (docs/ps2-float.md) shows sp+0xBC, rider+0xA9C, as the last overlap flag of the 0x33B748
// spatial-tree cell test of the same pass (its sp+0x1C, 0x33BAD8 / 0x33BB0C). That flag is float-dependent: keep these
// words out of comparisons.
struct OriginalInstanceContactRecord {
    ContactQuad point{},direction{},normal{};
    float closingSpeed=0;
};
inline bool originalInstanceContactPriority(const OriginalInstanceContactNode& node){return (node.flags&1)&&node.value!=0;}

// 0x104E70 after 334458: ranking/unique list (pickup_contact_selection.hpp),
// normal aggregation of priority contacts (only with two or more contacts),
// the selected entity callback (instance+0xC vtable+0x154, 34E698) and the
// returned +0x40 depth (-1 without contacts).
struct OriginalInstanceContactQuery {
    int selected=-1;std::vector<uint32_t> instances;OriginalInstanceContactPacket contact;float result=-1;
};
inline OriginalInstanceContactQuery originalInstanceContactQuery(std::span<const OriginalInstanceContactPacket> contacts,
        std::array<float,3> center,float queryRadius,const std::function<void(OriginalInstanceContactPacket&)>& entityCallback={}) {
    using namespace instance_contact_math;OriginalRounding rounding;OriginalInstanceContactQuery out;
    if(contacts.empty())return out;
    std::vector<OriginalPickupContactCandidate> candidates;candidates.reserve(contacts.size());
    for(const auto& c:contacts)candidates.push_back({c.instance,{c.point[0],c.point[1],c.point[2]},{c.normal[0],c.normal[1],c.normal[2]},c.depth,originalInstanceContactPriority(c.descriptor)});
    auto selection=originalPickupContactSelection(candidates,center,queryRadius);
    out.selected=selection.selected;out.instances=std::move(selection.instances);out.contact=contacts[out.selected];
    if(contacts.size()>=2) {
        ContactQuad sum{0,0,0,0}; // 0x4FF120
        for(const auto& c:contacts)if(originalInstanceContactPriority(c.descriptor))sum=add4(sum,c.normal);
        float length=vuSqrt(dot4(sum,sum));
        if(.0010000000474974513f<length)out.contact.normal=scale4(sum,vuReciprocal(length));
    }
    if(out.contact.entity) {
        if(!entityCallback)throw std::runtime_error("Original selected contact entity callback unavailable");
        entityCallback(out.contact);
    }
    out.result=out.contact.depth;return out;
}

// 0x1231A8: project against rider+0x370 in motion 0, or motion 2 with owner+0x30 == 0.
inline bool originalInstanceContactUsesGroundNormal(int motionMode,int ownerWord30){
    return motionMode==0||(motionMode==2&&ownerWord30==0);
}

struct OriginalInstanceContactRider {
    int controlState=0,motionMode=0,ownerWord30=0;  // owner+0xDE4, +0xDE0, +0x30
    ContactQuad groundNormal{};                      // rider+0x370
    ContactQuad railDirection{};                     // owner+0xB0 (motion 4)
    ContactQuad velocity{};                          // rider+0x1E0
};
struct OriginalInstanceContactPhaseHost {
    std::function<void()> boundaryContacts;                                   // 0x106F78
    std::function<float(OriginalInstanceContactPacket&)> query;               // 32F650(mode1)+0x104E70
    std::function<void(const OriginalInstanceContactPacket&,const OriginalInstanceContactRecord&)> store; // rider+9E0/+A60
    std::function<void(const OriginalInstanceContactPacket&,const OriginalInstanceContactRecord&,float)> respond; // 0x1057B8
    std::function<uint32_t(const OriginalInstanceContactPacket&)> instanceFlags; // re-read after 1057B8 (optional)
    std::function<void(const OriginalInstanceContactPacket&)> audio;          // 0x296088
    std::function<void()> finish;                                             // 0x16D320, 32F708(query,2)
    std::function<ContactQuad()> velocity;                                    // rider+0x1E0 after 106F78 (optional)
};
struct OriginalInstanceContactPhaseResult {
    bool ran=false,contact=false,projected=false,stored=false,responded=false,audio=false;
    float depth=-1;OriginalInstanceContactPacket packet;OriginalInstanceContactRecord record;
};
// 0x105398. callerDirection is a1 (nullptr for 0).
inline OriginalInstanceContactPhaseResult originalInstanceContactPhase(const OriginalInstanceContactRider& rider,
        const ContactQuad* callerDirection,const OriginalInstanceContactPhaseHost& host) {
    using namespace instance_contact_math;OriginalInstanceContactPhaseResult r;
    if(rider.controlState==9)return r;
    r.ran=true;if(host.boundaryContacts)host.boundaryContacts();
    // 104E70 reads rider+0x1E0 after 106F78, whose notification (105D98) may have changed it (fuzz r8-0165 2607: a rail-body
    // contact first, then the panel's record took the old velocity, 1278.5 cm/s closing instead of 295.0, and crashed).
    ContactQuad velocity=host.velocity?host.velocity():rider.velocity;
    if(!host.query)throw std::runtime_error("Original instance contact query unavailable");
    OriginalInstanceContactPacket& hit=r.packet;r.depth=host.query(hit);
    {
        OriginalRounding rounding;
        if(!(0<=r.depth)){if(host.finish)host.finish();return r;}
        r.contact=true;
        const ContactQuad* axis=originalInstanceContactUsesGroundNormal(rider.motionMode,rider.ownerWord30)?&rider.groundNormal:callerDirection;
        if(axis) {
            float along=dot4(hit.normal,*axis);auto tangent=sub4(hit.normal,scale4(*axis,along));
            float length=vuSqrt(dot4(tangent,tangent));
            hit.normal=length<.0010000000474974513f?*axis:scale4(tangent,vuReciprocal(length));r.projected=true;
        } else if(rider.motionMode==4) {
            float along=dot4(hit.normal,rider.railDirection);
            hit.normal=0<along?rider.railDirection:scale4(rider.railDirection,-1.f);r.projected=true;
        }
        float closing=0;
        {auto negative=scale4(hit.normal,-1.f);float d=dot4(negative,sub4(velocity,hit.surfaceVelocity));if(0<=d)closing=d;}
        r.record.point=hit.point;r.record.direction=scale4(velocity,vuRsqrt(dot4(velocity,velocity)));
        r.record.normal=hit.normal;r.record.closingSpeed=closing;
    }
    if((hit.descriptor.flags&2)&&!(hit.instanceFlags&0x2000)){r.stored=true;if(host.store)host.store(hit,r.record);}
    if(hit.descriptor.flags&1) {
        if(!host.respond)throw std::runtime_error("Original instance contact response unavailable");
        r.responded=true;host.respond(hit,r.record,r.depth);
    }
    uint32_t flags=host.instanceFlags?host.instanceFlags(hit):hit.instanceFlags;
    if(!(flags&0x2000)){r.audio=true;if(host.audio)host.audio(hit);}
    if(host.finish)host.finish();
    return r;
}

// 0x1057B8 mutable rider fields.
struct OriginalInstanceResponseRider {
    int controlState=0,motionMode=0,ownerWord30=0;
    ContactQuad velocity{};       // +0x1E0
    ContactQuad presentationUp{}; // +0x180
    float secondaryCounter=0;     // +0x3F4 (0x105D98 history)
    int surface=0;                // +0x438
    bool stanceDiffers=false;     // +0x320 != +0x324
    float speedLimit=0;           // +0x2E4
};
struct OriginalInstanceResponseHost {
    std::function<void(const OriginalInstanceContactRecord&)> softCollision;        // 0x108388(rider,record,1)
    std::function<bool()> entityRigid;                                              // entity vtable+0x74 (only with entity)
    std::function<void(const ContactQuad&)> translate;                              // 0x106538
    std::function<void(const OriginalInstanceContactPacket&)> steer;                // 0x1065B0
    std::function<void()> rebuild;                                                  // 0x11E098
    std::function<int()> channelClass;                                              // 0x311AE8(anim,2)
    std::function<void(float)> surfaceLanding;                                      // rider+6C0 vtable+0x8C(f12)
    std::function<int()> requestedSemantic;                                         // 0x312AA0(anim,2)
    std::function<float()> channelTime;                                             // 0x312AB0(anim,2)
    std::function<void(float,const OriginalInstanceContactPacket&)> landingAward;   // 0x10E910(rider,0,0,0,hit,f12)
    std::function<void(bool)> scoreBoundary;                                        // 0x119E38(rider+790,stanceDiffers)
    std::function<void(int)> requestControl;                                        // 0x11FEC8
    std::function<void(int)> play;                                                  // 0x3128E8(anim,semantic,1,-1)
    std::function<void()> beginPredictor;                                           // 0x1135B8(rider+788,pos,vel,rider+2E4)
    std::function<void(const OriginalInstanceContactRecord&,int)> notify;           // 0x105D98(rider,record,surface)
};
enum class OriginalInstanceResponsePath {Ignored,Soft,SurfaceLanding,Bounce};
struct OriginalInstanceResponseResult {
    OriginalInstanceResponsePath path=OriginalInstanceResponsePath::Ignored;
    bool pushed=false;float closingSpeed=0,impulse=0,upAlignment=0;
};
template<class F> static void originalInstanceRequire(const F& f,const char* name){if(!f)throw std::runtime_error(name);}
inline OriginalInstanceResponseResult originalInstanceContactResponse(OriginalInstanceResponseRider& rider,
        const OriginalInstanceContactPacket& hit,const OriginalInstanceContactRecord& record,float depth,
        const OriginalInstanceResponseHost& host) {
    using namespace instance_contact_math;using terrain_original::mul;OriginalInstanceResponseResult out;
    if(rider.controlState==9)return out;
    std::optional<OriginalRounding> rounding;rounding.emplace();
    float restitution=0;{float a=hit.descriptor.auxiliary;if(0<=a)restitution=eeMin(a,1.f);}
    float mass=hit.descriptor.value;
    if(mass==0) {
        originalInstanceRequire(host.softCollision,"Original soft instance collision unavailable");
        out.path=OriginalInstanceResponsePath::Soft;rounding.reset();host.softCollision(record);return out;
    }
    float closing=0;{float d=dot4(hit.normal,sub4(hit.surfaceVelocity,rider.velocity));if(0<=d)closing=d;}
    bool rigid=true;
    if(hit.entity){originalInstanceRequire(host.entityRigid,"Original entity contact callback unavailable");rounding.reset();rigid=host.entityRigid();rounding.emplace();}
    float impulse;
    if(rigid) {
        auto push=scale4(hit.normal,mul(depth,1.100000023841858f));
        originalInstanceRequire(host.translate,"Original contact translation unavailable");
        rounding.reset();host.translate(push);rounding.emplace();out.pushed=true;
        if(closing<0)closing=0;
        float scaled=mul(restitution,closing),extra=55.55555725097656f<=scaled?eeMin(scaled,1388.888916015625f):55.55555725097656f;
        impulse=originalScalarAdd(closing,extra);
        rounding.reset();
        if(rider.motionMode==0){originalInstanceRequire(host.steer,"Original contact steering unavailable");host.steer(hit);}
        originalInstanceRequire(host.rebuild,"Original orientation rebuild unavailable");host.rebuild();
        rounding.emplace();
    } else {
        float inverseMass=collision_scalar::divide(100.f,mass);
        float loss=originalScalarSubtract(1.f,restitution);
        inverseMass=originalScalarAdd(inverseMass,1.f);
        float share=collision_scalar::divide(1.f,inverseMass);
        float half=mul(closing,share);loss=mul(loss,share);
        float root=originalScalarSubtract(mul(half,half),loss);root=eeMax(root,0.f);root=originalScalarSqrt(root);
        impulse=originalScalarSubtract(root,half);
    }
    out.closingSpeed=closing;
    float up=dot4(rider.presentationUp,hit.normal);out.upAlignment=up;
    if(rider.motionMode==1||(rider.motionMode==2&&rider.ownerWord30==1)) {
        float speed=vuSqrt(dot4(rider.velocity,rider.velocity));
        if(speed<833.3333740234375f) {
            float z=hit.normal[2],floor=.20000000298023224f,gain=originalScalarSubtract(2.5f,up);
            gain=floor<=z?mul(gain,z):mul(gain,floor);
            rider.secondaryCounter=originalScalarAdd(rider.secondaryCounter,gain);
        }
    }
    rounding.reset();
    originalInstanceRequire(host.channelClass,"Original animation class unavailable");
    int animationClass=host.channelClass();
    rounding.emplace();
    bool landing=rider.motionMode==1&&(animationClass==9||animationClass==2||animationClass==11||animationClass==1)&&.30000001192092896f<up;
    if(landing) {
        out.path=OriginalInstanceResponsePath::SurfaceLanding;
        auto relative=sub4(rider.velocity,hit.surfaceVelocity);
        float excess=originalScalarSubtract(impulse,closing),normalSpeed=277.77777099609375f;
        auto approach=scale4(hit.normal,closing);
        if(normalSpeed<excess)normalSpeed=excess;
        auto tangent=add4(relative,approach);
        float tangentSpeed=vuSqrt(dot4(tangent,tangent));
        float factor=mul(originalScalarAdd(tangentSpeed,555.5555419921875f),.5f);
        factor=tangentSpeed<=27.77777862548828f?collision_scalar::divide(factor,27.77777862548828f):collision_scalar::divide(factor,tangentSpeed);
        tangent=scale4(tangent,factor);
        rider.velocity=add4(tangent,scale4(hit.normal,normalSpeed));
        rounding.reset();
        for(auto f:{host.requestedSemantic?1:0,host.surfaceLanding?1:0,host.beginPredictor?1:0})if(!f)throw std::runtime_error("Original surface-landing callbacks unavailable");
        host.surfaceLanding(closing);
        bool award=true;
        if(host.requestedSemantic()==0x10c){originalInstanceRequire(host.channelTime,"Original channel time unavailable");award=.20000000298023224f<host.channelTime();}
        if(award) {
            for(auto f:{host.landingAward?1:0,host.scoreBoundary?1:0,host.requestControl?1:0,host.play?1:0})if(!f)throw std::runtime_error("Original surface-landing callbacks unavailable");
            rider.surface=0;host.landingAward(tangentSpeed,hit);host.scoreBoundary(rider.stanceDiffers);
            host.requestControl(13);host.requestControl(5);host.play(0x10c);
        }
        host.beginPredictor();
        out.impulse=impulse;return out;
    }
    out.path=OriginalInstanceResponsePath::Bounce;out.impulse=impulse;
    rider.velocity=add4(rider.velocity,scale4(hit.normal,impulse));
    rounding.reset();
    originalInstanceRequire(host.notify,"Original collision notification unavailable");
    host.notify(record,hit.surface);
    return out;
}
}
