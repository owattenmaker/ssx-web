#pragma once
// Original SSX3 (PS2 USA, SLUS_207.72) MultiSplineModifier (ELF name 0x48E9E8,
// vtable 0x48F168, ctor 0x359F88, factory 0x355B30, builtin20 0x2FE0C0): the
// Snow Jam chairlift ("tramlores") cars. Stage programs 69/71 (handler rows
// 134/135, slot 1, run when the course section loads) attach one modifier to
// the LiveComp entity (0x490B10) of mdl_ARA1_tramlores_0 / _1 with
// MultiSpline(self, spline 0x608 / 0x708 = spline_ARA1_GondolaRail_0/1,
// count 3, 35 / -35 km/h, rotation 90 deg). The ctor clones the authored
// instance count times (0x35A3F0 -> 0x351170, 0x35A458 copies the authored
// 0xA0 bytes into clone 0 and registers clones 1.. with 0x351270 as static
// instances, flags &~0x40 | 0x20, &~0x100): one car per clone, evenly spaced
// along the closed path.
//
// Modifier layout (0x60 bytes):
//   +0x04 count (3)            +0x08 orientation mode (3: yaw and pitch)
//   +0x0C rotation about the 0x4FF160 axis (0,0,1), radians (deg * gp-0x2A2C)
//   +0x10 distance along the path (cm)   +0x14 speed cm/s (km/h * gp-0x2A28)
//   +0x18 radius (0x350570: largest authored-AABB corner distance)
//   +0x1C/+0x20..+0x2C script options (unused per tick)   +0x30 dirty
//   +0x34 authored flags & 0x100   +0x38 node matrix scratch  +0x3C car records
//   +0x40 authored instance        +0x44 clone list           +0x48 spline path
//   (0x3451C0: resource, cursor index, cursor segment, length)
// Car record (0x60 bytes each at +0x3C): +0x00 position, +0x10 first
// derivative, +0x20 second derivative * dt/ds (0x345048), +0x30/+0x40 bounds,
// +0x50/+0x54 tangent x^2, y^2.
//
// Per game tick, before the rider (verified against the capture records: at
// the provider exit +0x30 is 0 and the clones already sit at this tick's
// distance):
//   modifier vtable+0x14 0x35A560   distance += speed * clock dt, wrapped by the
//                                   path length; dirty = 1
//   LiveComp vtable+0x194 0x3568B0  entity matrix (vtable+0xC4 0x356078 ->
//                                   modifier+0x94 0x361C20: 0x35AC20 if dirty,
//                                   returns clone 0 +0x10); car-0 record bounds
//                                   = translation -/+ radius (modifier+0x74
//                                   0x361C10, +0x6C 0x361BF0), relocation;
//                                   then modifier+0xC4 0x35A918: records and
//                                   clone instance bounds (+0x60/+0x6C) of cars
//                                   1.. = clone translation -/+ radius.
//   0x35AC20 evaluate               car k at distance + k*length/count
//                                   (wrapped), 0x345248 -> record, yaw/pitch
//                                   from the first derivative (0x31C228),
//                                   Euler matrix (0x31BE50) with the path point
//                                   as translation into clone k +0x10, then
//                                   R(axis, +0x0C) * M (VU0).
// Collision: the authored instance takes the entity route (flags 0x210345):
// entity bounds = car-0 record bounds, hierarchy root = clone 0 matrix.
// Clones are static-route instances (0x210325 / 0x212325) whose hierarchy root
// is their own +0x10 matrix; clone 0 keeps the authored bounds (0x35A918 only
// relocates cars 1..). All carry the LiveComp entity (instance+0xC): 1057B8's
// rigid predicate (entity vtable+0x74 0x355420 -> modifier+0x44 0x360B60)
// returns 1, and 104E70's selected-entity callback (LiveComp vtable+0x154
// 0x34E698 -> 0x356AE0 -> 0x353098 -> modifier+0xB4 0x35B200) adds the car's
// surface velocity to the contact packet (+0x20) and its angular velocity (+0x30).
// Drawing: car 0 through the authored instance/entity, cars 1.. through the
// modifier draw (+0x5C 0x35B418), all with the clone matrices.
#include "original_spline_path.hpp"
#include "roller_modifier.hpp"
#include <vector>

namespace ssx {
struct OriginalMultiSplineCar {
    SplineQuad position{},tangent{},curvature{},boundsMin{},boundsMax{}; // record +0x00..+0x4C
    float tangentXX=0,tangentYY=0;                                       // record +0x50/+0x54
    uint32_t record58=0,record5C=0;                                      // record +0x58/+0x5C (untouched)
    RollerMatrix matrix{};                                               // clone +0x10 (row-vector, row 3 = translation)
    terrain_original::Vector instanceLow{},instanceHigh{};               // clone +0x60/+0x6C
};
struct OriginalMultiSplineModifier {
    int32_t count=0,mode=0;
    float angle=0,distance=0,speed=0,radius=0;
    uint32_t dirty=0;
    OriginalSplinePath path;
    std::vector<OriginalMultiSplineCar> cars;
};
namespace multi_spline_constants {
inline float constant(uint32_t bits){return std::bit_cast<float>(bits);}
inline const float kHalfPi=constant(0x3fc90fdbu),kMinusHalfPi=constant(0xbfc90fdbu),kPi=constant(0x40490fdbu); // gp-0x2A14..-0x2A24
inline const float kVelocityEpsilon=constant(0x3727c5acu);            // gp-0x2A10 1e-5
inline const float kKmhToCms=constant(0x41de38e4u);                   // gp-0x2A28 27.777779
inline const float kDegToRad=constant(0x3c8efa36u);                   // gp-0x2A2C
inline const SplineQuad kAxis{0.f,0.f,1.f,0.f};                       // 0x4FF160 (runtime data, all captured states)
}

// 0x35A560 (dt = clock +0x14, 1/60).
inline void originalMultiSplineUpdate(OriginalMultiSplineModifier& m,float dt){
    OriginalRounding rounding;
    float d=originalScalarAdd(m.distance,terrain_original::mul(m.speed,dt));
    if(d<0.f)d=originalScalarAdd(d,m.path.length);
    else if(m.path.length<=d)d=originalScalarSubtract(d,m.path.length);
    m.distance=d;m.dirty=1;
}
namespace multi_spline_detail {
// 0x31C228 heading/pitch construction of 0x35AC20 (the x == 0 branches keep the original constants).
inline float yawNegated(float x,float y){
    using namespace multi_spline_constants;
    if(x==0.f){
        if(y==0.f)return originalScalarSubtract(0.f,y);
        return originalScalarSubtract(0.f,0.f<=y?kHalfPi:kMinusHalfPi);
    }
    float a=collision_scalar::atan(collision_scalar::divide(y,x));
    if(x<0.f)a=0.f<y?originalScalarAdd(a,kPi):originalScalarSubtract(a,kPi);
    return originalScalarSubtract(0.f,a);
}
inline float pitch(float horizontal,float z){
    using namespace multi_spline_constants;
    if(horizontal==0.f){
        if(z==0.f)return originalScalarAdd(0.f,z);
        return originalScalarAdd(0.f,0.f<=z?kHalfPi:kMinusHalfPi);
    }
    float a=collision_scalar::atan(collision_scalar::divide(z,horizontal));
    if(horizontal<0.f)a=0.f<z?originalScalarAdd(a,kPi):originalScalarSubtract(a,kPi);
    return originalScalarAdd(0.f,a);
}
}
// 0x35AC20
inline void originalMultiSplineEvaluate(OriginalMultiSplineModifier& m){
    using namespace multi_spline_constants;using terrain_original::mul;
    const int32_t n=m.count;
    if(int32_t(m.cars.size())!=n)throw std::runtime_error("MultiSpline car count mismatch");
    std::vector<float> distance(std::max<int32_t>(n,1));
    {
        OriginalRounding rounding;
        const float spacing=collision_scalar::divide(m.path.length,float(n));
        distance[0]=m.distance;
        for(int32_t i=1;i<n;++i)distance[i]=originalScalarAdd(distance[i-1],spacing);
        for(int32_t i=0;i<n;++i){
            if(distance[i]<0.f)distance[i]=originalScalarAdd(distance[i],m.path.length);
            else if(m.path.length<=distance[i])distance[i]=originalScalarSubtract(distance[i],m.path.length);
        }
    }
    RollerMatrix rotation{};bool rotate=m.angle!=0.f;
    if(rotate){
        OriginalRounding rounding;
        const auto sc=collision_scalar::sincos(m.angle);const float s=sc[0],c=sc[1];
        const float ax=kAxis[0],ay=kAxis[1],az=kAxis[2];
        const float t=originalScalarSubtract(1.f,c);
        const float tz=mul(t,az),tx=mul(t,ax),ty=mul(t,ay),sz=mul(s,az),sx=mul(s,ax),sy=mul(s,ay);
        rotation[0]={originalScalarAdd(mul(tx,ax),c),originalScalarSubtract(mul(ty,ax),sz),originalScalarAdd(mul(tz,ax),sy),0.f};
        rotation[1]={originalScalarAdd(mul(tx,ay),sz),originalScalarAdd(mul(ty,ay),c),originalScalarSubtract(mul(tz,ay),sx),0.f};
        rotation[2]={originalScalarSubtract(mul(tx,az),sy),originalScalarAdd(mul(ty,az),sx),originalScalarAdd(mul(tz,az),c),0.f};
        rotation[3]={0.f,0.f,0.f,1.f};
    }
    for(int32_t i=0;i<n;++i){
        auto& car=m.cars[i];
        const auto sample=originalSplineEvaluate(m.path,distance[i]);
        car.position=sample.position;car.tangent=sample.tangent;car.curvature=sample.curvature;
        OriginalRounding rounding;
        const float x=car.tangent[0],y=car.tangent[1];
        car.tangentXX=mul(x,x);car.tangentYY=mul(y,y);
        float yaw=0.f,pitch=0.f;
        if(m.mode==2||m.mode==3)yaw=multi_spline_detail::yawNegated(x,y);
        if(m.mode==1||m.mode==3){
            const float horizontal=collision_scalar::squareRoot(originalScalarAdd(car.tangentXX,car.tangentYY));
            pitch=multi_spline_detail::pitch(horizontal,car.tangent[2]);
        }
        const auto r=collision_scalar::sincos(0.f),p=collision_scalar::sincos(pitch),w=collision_scalar::sincos(yaw);
        const float sr=r[0],cr=r[1],sp=p[0],cp=p[1],sy=w[0],cy=w[1];
        const float srsp=mul(sr,sp),crsy=mul(cr,sy);
        auto& M=car.matrix;
        M[0]={mul(cp,cy),mul(-cp,sy),sp,0.f};
        M[1]={originalScalarAdd(mul(srsp,cy),crsy),originalScalarSubtract(mul(cr,cy),mul(srsp,sy)),mul(-sr,cp),0.f};
        M[2]={originalScalarSubtract(mul(sr,sy),mul(mul(cr,sp),cy)),originalScalarAdd(mul(crsy,sp),mul(sr,cy)),mul(cr,cp),0.f};
        M[3]={car.position[0],car.position[1],car.position[2],1.f};
        if(rotate){RollerMatrix out;for(unsigned k=0;k<4;++k)out[k]=roller_math::transform(M,rotation[k]);M=out;}
    }
    m.dirty=0;
}
// Entity update part of 0x3568B0 (car-0 record bounds) and 0x35A918 (cars 1..).
inline void originalMultiSplineBounds(OriginalMultiSplineModifier& m){
    if(m.dirty)originalMultiSplineEvaluate(m);
    OriginalRounding rounding;const SplineQuad r{m.radius,m.radius,m.radius,0.f};
    for(size_t i=0;i<m.cars.size();++i){
        auto& car=m.cars[i];const SplineQuad t=car.matrix[3];
        car.boundsMin=roller_math::vsub(t,r);car.boundsMax=roller_math::vadd(t,r);
        if(i){for(unsigned k=0;k<3;++k){car.instanceLow[k]=car.boundsMin[k];car.instanceHigh[k]=car.boundsMax[k];}}
    }
}
// 0x35B200: surface velocity of car `index` at the contact point (added to packet +0x20; the
// angular velocity goes to packet +0x30).
struct OriginalMultiSplineContactVelocity {SplineQuad linear{},angular{};};
inline OriginalMultiSplineContactVelocity originalMultiSplineContactVelocity(const OriginalMultiSplineModifier& m,size_t index,const SplineQuad& point){
    using namespace multi_spline_constants;using terrain_original::mul;
    const auto& car=m.cars.at(index);OriginalRounding rounding;
    float yawRate=0.f,pitchRate=0.f;   // f8 / f5
    const float tx=car.tangent[0];
    if(kVelocityEpsilon<std::fabs(tx)){
        const float cx=car.curvature[0],cy=car.curvature[1],cz=car.curvature[2];
        const float ratio=collision_scalar::divide(cy,tx);
        const float sum=originalScalarAdd(cx,cy),planar=originalScalarAdd(car.tangentXX,car.tangentYY);
        const float horizontal=collision_scalar::squareRoot(planar);
        const float inverse=collision_scalar::divide(1.f,horizontal);
        const float a=collision_scalar::divide(mul(car.tangent[1],cx),car.tangentXX);
        const float b=collision_scalar::divide(mul(car.tangent[2],sum),planar);
        const float c=collision_scalar::divide(-car.tangentXX,planar);
        yawRate=mul(c,originalScalarSubtract(ratio,a));
        pitchRate=mul(inverse,originalScalarSubtract(cz,b));
    }
    // unit tangent (vmul, vadday.x, vmaddaz.x, vmaddw.x, vrsqrt, vmulq) * speed
    const SplineQuad& t=car.tangent;
    float len2=terrain_original::add(mul(t[0],t[0]),mul(t[1],t[1]));
    len2=terrain_original::add(len2,mul(1.f,mul(t[2],t[2])));len2=terrain_original::add(len2,mul(1.f,mul(t[3],t[3])));
    const float q=roller_math::vuRsqrt(len2);
    SplineQuad linear=roller_math::vscale(roller_math::vscale(t,q),m.speed);
    const SplineQuad local{0.f,mul(pitchRate,m.speed),mul(yawRate,m.speed),0.f};
    const SplineQuad angular=roller_math::transform(car.matrix,local);
    const SplineQuad arm=roller_math::vsub(point,car.position);
    OriginalMultiSplineContactVelocity out;out.angular=angular;out.linear=roller_math::vadd(linear,roller_math::cross(arm,angular));
    return out;
}
}
