#pragma once
// Original 0x106F78 (the hips-axis "rail snap" of 0x105398) including its entity side: when
// the winning rail comes from an entity-owned layer (a RailModifier, layer type 2: query
// out+0x50 = instance, +0x5C = node; the Snow Jam log teeters) the contact
//   1. adds the entity's surface velocity at the rail point to out+0x20/+0x30 through
//      entity vtable+0x154 (0x34E698, this = entity-0x14) right after the tangent flip, so the
//      impulse is max(0, e.(out+0x20 - v)) + 55.5556 (static spline layers: out+0x20 = 0);
//   2. after the velocity change, calls entity vtable+0x15C (AnimTeeter 0x342538, this =
//      entity-0x14) with the rail result and F = (v_before - v_after) * fps
//      (v_before = rider+0x1E0 after 0x106538, fps = float(*(gp-0x848)+0x10) = 60),
//      i.e. F = -60 * impulse * e: |F| >= 3333 > 100, so every snap onto a log rail torques it.
// Everything else is engine/rail_body_contact.hpp (verified, unchanged): this header adds
// originalRailSnapContact = originalRailBodyContact + the two entity hooks, plus the entity
// side for the AnimTeeter / LiveComp:
//   0x34E698 contact velocity  (0x356AE0 primary modifier (none on teeters), node < 0 -> none,
//                               0x34E798 table when +0x10 "evaluated", 0x34E600 node origin)
//   0x34E798 node velocity table entity+0x4C (32 bytes/node: linear, angular), per animated
//            node: 0x351660 channel time derivative x rate(vt+0x1A8) x fps (angular also x
//            pi/180, then rotated by a Y/Z matrix built with sincos of the NEGATED DEGREE
//            values (0x31BE50 fed degrees, an original quirk)), both by the node world
//            matrix, plus the parent's linear + (origin - parent origin) x parent angular;
//            clears +0x10.
//   0x351660 channel derivative ((a*(3t))*t + (b+b)*t) + c per masked component (linear =
//            components 0..2, angular = -(3..5), neg.s so unmasked ones are -0).
//   0x3424D0 AnimTeeter vt+0x1A8 rate: 0 when (time <= min and step < 0) or (max <= time
//            and step > 0), else step (+0x04). LiveComp vt+0x1A8 0x361158: done ? 0 : rate.
// Float policy as engine/rail_modifier.hpp (VU lanes terrain_original under OriginalRounding,
// EE mul.s chop, add.s/sub.s guard bit); compile with -ffp-contract=off.
// Oracle: tools/test_rail_snap_torque_native.py (tests/rail_snap_torque_reference.cpp).
#include "rail_body_contact.hpp"
#include "rail_modifier.hpp"
#include "livecomp_animation.hpp"

namespace ssx {

// ---------------------------------------------------------------------------------------
// Entity side: per-node velocity table (0x34E798) and contact velocity (0x34E698).
struct OriginalAnimNodeVelocity {RollerQuad linear{},angular{};};
struct OriginalAnimNodeVelocityTable {std::vector<OriginalAnimNodeVelocity> nodes;}; // entity+0x4C

// 0x3424D0 (AnimTeeter entity vtable+0x1A8, this = object).
inline float originalAnimTeeterRate(const OriginalAnimTeeter& e){
    if(e.time<=e.minimum&&e.step<0.f)return 0.f;
    if(e.maximum<=e.time&&0.f<e.step)return 0.f;
    return e.step;
}
// 0x361158 (LiveComp entity vtable+0x1A8, this = object).
inline float originalLiveCompRate(const OriginalLiveComp& s){return s.done?0.f:s.rate;}

namespace anim_velocity_original {
// 0x351660(channel, linear, angular, t): time derivative of every masked component (the
// sampler's segment lookup, 0x351A80 on a cache miss, updates the same cache).
inline void derivative(OriginalAnimChannel& c,float t,RollerQuad& linear,RollerQuad& angular){
    using terrain_original::mul;OriginalRounding rounding;
    std::array<float,16> d{};size_t active=0;
    const float three=mul(t,3.f);
    for(unsigned n=0;n<16;++n){
        if(!(c.track->mask>>n&1u)){d[n]=0.f;continue;}
        const auto& curve=c.track->curves.at(active);int32_t& cache=c.segment[active];++active;
        size_t k=size_t(cache);
        const bool inside=k<curve.size()&&curve[k].t0<=t&&t<curve[k].t1;
        if(!inside)k=anim_original::findSegment(curve,cache,t);
        const auto& s=curve.at(k);
        float a=mul(mul(s.a,three),t);float b=mul(originalScalarAdd(s.b,s.b),t);
        d[n]=originalScalarAdd(originalScalarAdd(a,b),s.c);
    }
    linear={d[0],d[1],d[2],0.f};
    angular={-d[3],-d[4],-d[5],0.f};
}
inline RollerQuad scaleLanes(const RollerQuad& v,float s){using terrain_original::mul;return {mul(v[0],s),mul(v[1],s),mul(v[2],s),mul(v[3],s)};}
}

// 0x34E798: fills `table` from the entity state (recomputing the node matrices first when
// dirty, as 0x34DC90 does) and clears `evaluated`. rate = vtable+0x1A8, fps = *(gp+0x2A74)+0x10.
inline void originalAnimNodeVelocities(OriginalAnimTeeter& e,float rate,OriginalAnimNodeVelocityTable& table,int32_t fps=60){
    using namespace roller_math;using terrain_original::mul;
    if(e.dirty){originalAnimEvaluateChannels(e);originalAnimComposeNodes(e);e.dirty=false;}
    const auto& nodes=e.model->nodes;table.nodes.assign(nodes.size(),{});
    const float degrees=std::bit_cast<float>(0x3c8efa36u); // gp-0x2AA4 pi/180
    size_t channel=0;
    for(size_t i=0;i<nodes.size();++i){
        table.nodes[i]={RollerQuad{0,0,0,0},RollerQuad{0,0,0,0}}; // 0x4FF120
        if(!nodes[i].track)continue;
        OriginalAnimChannel& c=e.channels.at(channel++);
        RollerQuad linear,angular;
        anim_velocity_original::derivative(c,e.sampleTime,linear,angular);
        const float ry=-c.value[4],rz=-c.value[5];
        float perSecond,angularScale;
        {OriginalRounding r;perSecond=mul(rate,float(fps));angularScale=mul(mul(rate,float(fps)),degrees);}
        auto [sy,cy]=collision_scalar::sincos(ry);auto [sz,cz]=collision_scalar::sincos(rz); // degrees fed as radians (original)
        OriginalRounding rounding;
        linear=anim_velocity_original::scaleLanes(linear,perSecond);
        angular=anim_velocity_original::scaleLanes(angular,angularScale);
        const RollerMatrix yz{RollerQuad{mul(cy,cz),mul(-cy,sz),sy,0.f},RollerQuad{sz,cz,0.f,0.f},RollerQuad{0,0,1,0},RollerQuad{0,0,0,1}};
        angular=transform(yz,angular);
        const RollerMatrix& world=e.matrices.at(i);
        linear=transform(world,linear);
        angular=transform(world,angular);
        if(nodes[i].parent>=0){
            const auto& parent=table.nodes.at(size_t(nodes[i].parent));
            const RollerQuad arm=vsub(world[3],e.matrices.at(size_t(nodes[i].parent))[3]);
            angular=vadd(angular,parent.angular);
            const RollerQuad carried=vadd(parent.linear,cross(arm,parent.angular));
            linear=vadd(linear,carried);
        }
        table.nodes[i].linear=linear;
        table.nodes[i].angular=vadd(table.nodes[i].angular,angular);
    }
    e.evaluated=0;
}

// 0x34E698's sum for one node: its velocity row (entity+0x4C) and its matrix origin.
inline void originalAnimContactVelocityAt(const RollerQuad& origin,const OriginalAnimNodeVelocity& v,const RollerQuad& point,RollerQuad& packet20,RollerQuad& packet30){
    using namespace roller_math;
    OriginalRounding rounding;
    const RollerQuad lever=vsub(point,origin);
    const RollerQuad linear=vadd(v.linear,cross(lever,v.angular));
    packet20=vadd(packet20,linear);packet30=vadd(packet30,v.angular);
}
// 0x34E698 for an entity without a primary modifier (AnimTeeter; LiveComps without a
// Spline/MultiSpline): packet +0x20 += node linear + (point - node origin) x node angular,
// +0x30 += node angular. The table is recomputed only when e.evaluated is set (0x34E348
// sets it whenever the channels are re-evaluated), exactly as the original caches entity+0x4C.
inline bool originalAnimContactVelocity(OriginalAnimTeeter& e,float rate,OriginalAnimNodeVelocityTable& table,int32_t node,
        const RollerQuad& point,RollerQuad& packet20,RollerQuad& packet30,int32_t fps=60){
    using namespace roller_math;
    if(node<0)return false;
    if(e.evaluated||table.nodes.size()!=e.model->nodes.size())originalAnimNodeVelocities(e,rate,table,fps);
    const RollerQuad origin=originalAnimNodeMatrix(e,node)[3]; // 0x34E600 -> vt+0xE8 0x3610E0
    originalAnimContactVelocityAt(origin,table.nodes.at(size_t(node)),point,packet20,packet30);
    return true;
}
inline bool originalAnimTeeterContactVelocity(OriginalAnimTeeter& e,OriginalAnimNodeVelocityTable& table,int32_t node,
        const RollerQuad& point,RollerQuad& packet20,RollerQuad& packet30,int32_t fps=60){
    return originalAnimContactVelocity(e,originalAnimTeeterRate(e),table,node,point,packet20,packet30,fps);
}

// ---------------------------------------------------------------------------------------
// 0x106F78 with the entity hooks. `hasEntity(hit)`: out+0x50 != 0 and instance+0xC != 0
// (only RailModifier layers fill +0x50). `contactVelocity(hit, out20, out30)`: vtable+0x154
// (out20/out30 start as the query's +0x20/+0x30, zero for every rail layer).
// `applyForce(hit, force)`: vtable+0x15C with the rail result (point +0x00, node +0x5C) and F.
struct OriginalRailSnapEntityHooks {
    std::function<bool(const OriginalRailQueryResult&)> hasEntity;
    std::function<void(const OriginalRailQueryResult&,RollerQuad&,RollerQuad&)> contactVelocity;
    std::function<void(const OriginalRailQueryResult&,const RollerQuad&)> applyForce;
};
struct OriginalRailSnapResult : OriginalRailBodyContactResult {
    bool entity=false;RollerQuad surfaceVelocity{},angularVelocity{},force{};bool forced=false;
};
inline OriginalRailSnapResult originalRailSnapContact(OriginalRailBodyContactRider& rider,const OriginalRailBodyContactHost& host,
        const OriginalRailSnapEntityHooks& entity,int32_t fps=60){
    using namespace terrain_original;using instance_contact_math::vuSqrt;using instance_contact_math::vuRsqrt;using instance_contact_math::vuReciprocal;
    OriginalRailSnapResult out;
    if(rider.motionMode==4)return out;
    auto need=[](const auto& f,const char* what)->const auto&{if(!f)throw std::runtime_error(std::string("Rail snap callback missing: ")+what);return f;};
    std::optional<OriginalRounding> rounding;rounding.emplace();
    const RailVector axis=originalRailBoardAxes(rider.hipsQuaternion).y;
    const RailVector P=rider.hipsPosition;
    rounding.reset();out.hit=need(host.query,"query")(P);rounding.emplace();
    out.stage=1;if(!out.hit.found)return out;
    auto vdot=[](const RailVector& a,const RailVector& b){return add(add(add(mul(a[0],b[0]),mul(a[1],b[1])),mul(a[2],b[2])),mul(0.f,0.f));};
    auto vscale=[](RailVector a,float s){for(auto& x:a)x=mul(x,s);return a;};
    auto vadd3=[](RailVector a,const RailVector& b){for(unsigned k=0;k<3;++k)a[k]=add(a[k],b[k]);return a;};
    auto vsub3=[](RailVector a,const RailVector& b){for(unsigned k=0;k<3;++k)a[k]=sub(a[k],b[k]);return a;};
    RailVector& v=rider.velocity;
    const float speed=vuSqrt(vdot(v,v));
    out.stage=2;
    if(std::bit_cast<float>(0x3a83126fu)<speed){
        const RailVector d=vsub3(out.hit.point,P);
        if(vdot(d,v)<mul(speed,std::bit_cast<float>(0xbe4ccccdu)))return out;
    }
    const RailVector d=vsub3(out.hit.point,P);
    float along=vdot(axis,d),clamped=-50.f;if(-50.f<=along)clamped=instance_contact_math::eeMin(along,50.f);
    const RailVector closest=vadd3(P,vscale(axis,clamped));
    const RailVector gap=vsub3(closest,out.hit.point);
    out.stage=3;if(50.f<vuSqrt(vdot(gap,gap)))return out;
    rounding.reset();const bool attach=need(host.attachable,"attachable")();rounding.emplace();
    out.stage=4;if(attach)return out;
    out.stage=5;out.contact=true;
    RailVector n=out.hit.tangent;
    float closing=vdot(v,n);
    if(closing<0){closing=-closing;n=vscale(n,-1.f);}
    // 0x107224: entity vtable+0x154 (after the flip, before the record copy).
    RollerQuad out20{0,0,0,0},out30{0,0,0,0};
    rounding.reset();
    out.entity=entity.hasEntity&&entity.hasEntity(out.hit);
    if(out.entity)need(entity.contactVelocity,"contactVelocity")(out.hit,out20,out30);
    rounding.emplace();
    out.surfaceVelocity=out20;out.angularVelocity=out30;
    out.record.point={out.hit.point[0],out.hit.point[1],out.hit.point[2],1};
    {const float r=vuRsqrt(vdot(v,v));out.record.direction={mul(v[0],r),mul(v[1],r),mul(v[2],r),0};}
    out.record.closingSpeed=closing;
    RailVector e=vsub3(P,out.hit.point);
    e=vsub3(e,vscale(n,vdot(e,n)));
    if(originalInstanceContactUsesGroundNormal(rider.motionMode,rider.ownerWord30))e=vsub3(e,vscale(rider.groundNormal,vdot(e,rider.groundNormal)));
    {const float length=vuSqrt(vdot(e,e));if(length<std::bit_cast<float>(0x3a83126fu))e=n;else e=vscale(e,vuReciprocal(length));}
    out.record.normal={e[0],e[1],e[2],0};
    const float separation=vdot(gap,e);
    if(separation<50.f){
        out.push=vscale(e,originalScalarSubtract(50.f,separation));out.pushed=true;
        rounding.reset();need(host.translate,"translate")(out.push);rounding.emplace();
    }
    const RailVector before=v;
    {
        const RailVector relative=vsub3(RailVector{out20[0],out20[1],out20[2]},v); // sp+0x30 (out+0x20) - v
        float d2=vdot(e,relative);
        d2=instance_contact_math::eeMax(d2,0.f);
        out.impulse=originalScalarAdd(d2,std::bit_cast<float>(0x425e38e4u));
        v=vadd3(v,vscale(e,out.impulse));
    }
    // 0x1074A8: F = (v_before - v_after) * float(fps), then entity vtable+0x15C.
    {
        const float scale=float(fps);
        out.force={mul(sub(before[0],v[0]),scale),mul(sub(before[1],v[1]),scale),mul(sub(before[2],v[2]),scale),0.f};
    }
    rounding.reset();
    if(out.entity){need(entity.applyForce,"applyForce")(out.hit,out.force);out.forced=true;}
    if(rider.motionMode==0){need(host.steer,"steer")(e);need(host.rebuild,"rebuild")();}
    need(host.notify,"notify")(out.record,out.hit.surface);
    return out;
}
}
