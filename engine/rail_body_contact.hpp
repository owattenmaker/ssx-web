#pragma once
// Original 0x106F78: the hips-axis rail contact that 0x105398 runs before its
// instance query (return value ignored). Skipped in motion 4. The hips bone
// (*(rider+0x780)+0x2C, index rider+0x89C = "hips") casts its local Y axis
// against the rider+0x860 spline layers through 0x334680(list, hips, out, 1,
// 300) (engine/rail_motion.hpp originalRailWorldQuery). A rail point within
// 50 cm of the +-50 cm hips segment that is not behind a moving rider, and that
// 0x108A48 would not attach to, pushes the rider out to 50 cm (0x106538), adds
// max(0, n.(hit+0x20 - v)) + 55.5556 cm/s along the contact normal, steers
// (0x1065B0 with the hit normal replaced, then 0x11E098) in motion 0 and
// notifies 0x105D98(rider, record, surface). The contact normal is the hips
// offset from the rail with its component along the rail tangent removed and,
// when 0x1231A8 holds, the component along rider+0x370. The record closing
// speed is |v . tangent|.
// Arithmetic: VU vector ops (terrain_original under OriginalRounding, 4-lane
// dots with zero w lanes), EE add.s/sub.s with the guard-bit policy, VSQRT/
// VRSQRT/VDIV as in engine/instance_contact.hpp.
#include "instance_contact.hpp"
#include "rail_motion.hpp"

namespace ssx {
struct OriginalRailBodyContactRider {
    int motionMode=0,ownerWord30=0;                 // owner+0xDE0, owner+0x30
    RailVector velocity{};                          // +0x1E0 (updated)
    RailVector groundNormal{0,0,1};                 // +0x370
    RailVector hipsPosition{};                      // hips bone +0x00
    std::array<float,4> hipsQuaternion{0,0,0,1};    // hips bone +0x10
};
struct OriginalRailBodyContactHost {
    std::function<OriginalRailQueryResult(RailVector)> query;          // 0x334680(rider+0x860,hips,out,1,300)
    std::function<bool()> attachable;                                  // 0x108A48(rider,tmp) != 0
    std::function<void(const RailVector&)> translate;                  // 0x106538
    std::function<void(const RailVector& normal)> steer;               // 0x1065B0 (motion 0)
    std::function<void()> rebuild;                                     // 0x11E098 (motion 0)
    std::function<void(const OriginalInstanceContactRecord&,int)> notify; // 0x105D98(rider,record,out+0x4C)
};
struct OriginalRailBodyContactResult {
    int stage=0;       // 0 motion4, 1 no rail, 2 behind, 3 beyond 50 cm, 4 attachable, 5 contact
    bool contact=false;
    OriginalRailQueryResult hit;
    OriginalInstanceContactRecord record;
    RailVector push{};bool pushed=false;
    float impulse=0;
};
inline OriginalRailBodyContactResult originalRailBodyContact(OriginalRailBodyContactRider& rider,const OriginalRailBodyContactHost& host) {
    using namespace terrain_original;using instance_contact_math::vuSqrt;using instance_contact_math::vuRsqrt;using instance_contact_math::vuReciprocal;
    OriginalRailBodyContactResult out;
    if(rider.motionMode==4)return out;
    auto need=[](const auto& f,const char* what)->const auto&{if(!f)throw std::runtime_error(std::string("Rail body contact callback missing: ")+what);return f;};
    std::optional<OriginalRounding> rounding;rounding.emplace();
    const RailVector axis=originalRailBoardAxes(rider.hipsQuaternion).y; // same EE sequence as 0x106FD4..0x107024
    const RailVector P=rider.hipsPosition;
    rounding.reset();out.hit=need(host.query,"query")(P);rounding.emplace();
    out.stage=1;if(!out.hit.found)return out;
    auto vdot=[](const RailVector& a,const RailVector& b){return add(add(add(mul(a[0],b[0]),mul(a[1],b[1])),mul(a[2],b[2])),mul(0.f,0.f));};
    auto vscale=[](RailVector a,float s){for(auto& x:a)x=mul(x,s);return a;};
    auto vadd=[](RailVector a,const RailVector& b){for(unsigned k=0;k<3;++k)a[k]=add(a[k],b[k]);return a;};
    auto vsub=[](RailVector a,const RailVector& b){for(unsigned k=0;k<3;++k)a[k]=sub(a[k],b[k]);return a;};
    RailVector& v=rider.velocity;
    const float speed=vuSqrt(vdot(v,v));
    out.stage=2;
    if(std::bit_cast<float>(0x3a83126fu)<speed){
        const RailVector d=vsub(out.hit.point,P);
        if(vdot(d,v)<mul(speed,std::bit_cast<float>(0xbe4ccccdu)))return out;
    }
    const RailVector d=vsub(out.hit.point,P);
    float along=vdot(axis,d),clamped=-50.f;if(-50.f<=along)clamped=instance_contact_math::eeMin(along,50.f);
    const RailVector closest=vadd(P,vscale(axis,clamped));
    const RailVector gap=vsub(closest,out.hit.point); // sp+0x90
    out.stage=3;if(50.f<vuSqrt(vdot(gap,gap)))return out;
    rounding.reset();const bool attach=need(host.attachable,"attachable")();rounding.emplace();
    out.stage=4;if(attach)return out;
    out.stage=5;out.contact=true;
    RailVector n=out.hit.tangent; // out+0x10
    float closing=vdot(v,n);
    if(closing<0){closing=-closing;n=vscale(n,-1.f);}
    out.record.point={out.hit.point[0],out.hit.point[1],out.hit.point[2],1};
    {const float r=vuRsqrt(vdot(v,v));out.record.direction={mul(v[0],r),mul(v[1],r),mul(v[2],r),0};}
    out.record.closingSpeed=closing;
    RailVector e=vsub(P,out.hit.point);
    e=vsub(e,vscale(n,vdot(e,n)));
    if(originalInstanceContactUsesGroundNormal(rider.motionMode,rider.ownerWord30))e=vsub(e,vscale(rider.groundNormal,vdot(e,rider.groundNormal)));
    {const float length=vuSqrt(vdot(e,e));if(length<std::bit_cast<float>(0x3a83126fu))e=n;else e=vscale(e,vuReciprocal(length));}
    out.record.normal={e[0],e[1],e[2],0};
    const float separation=vdot(gap,e);
    if(separation<50.f){
        out.push=vscale(e,originalScalarSubtract(50.f,separation));out.pushed=true;
        rounding.reset();need(host.translate,"translate")(out.push);rounding.emplace();
    }
    {
        const RailVector relative=vsub(RailVector{0,0,0},v); // hit+0x20 = 0x4FF120 for spline layers
        float d2=vdot(e,relative);d2=instance_contact_math::eeMax(d2,0.f);
        out.impulse=originalScalarAdd(d2,std::bit_cast<float>(0x425e38e4u));
        v=vadd(v,vscale(e,out.impulse));
    }
    rounding.reset();
    if(rider.motionMode==0){need(host.steer,"steer")(e);need(host.rebuild,"rebuild")();}
    need(host.notify,"notify")(out.record,out.hit.surface);
    return out;
}
}
