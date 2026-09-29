#pragma once
// Original SSX3 (PS2 USA, SLUS_207.72) SplineModifier (ELF name 0x48E8D0,
// vtable 0x48F250, 0xF0 bytes, ctor 0x359460, factory 0x355AD0, script
// builtin19 0x2FDED0) and the PositionModifier (vtable 0x48F5F0, 0x90 bytes,
// ctor 0x356F10 / factory 0x355918) that freezes a finished path.
//
// Snow Jam users (stage programs, see tools/export_spline_setpieces.py):
//   ravensplineanima_1000  program 54 (handler slot 1: section activation):
//                          builtin3 AnimObject (0x341AA0, LiveComp 0x490B10) + Spline(0x5408
//                          ravensplinea, loop, 45 km/h, roll -90 deg); drawn.
//   brocket_1000/1001      program 120 (triggerRockets_1100 slot 2 = rider contact):
//                          builtin0 Object + Spline(rocketPath_1000/1001, stop,
//                          150 km/h) + builtin26 DynamicParticle; not drawn
//                          (instance flags&3 == 2): only the particle trail shows.
//   spintwin_1000/1001     program 131 (spintwintrig_1000 slot 2): builtin3
//                          AnimObject + Spline(spintwinpath, stop, 160 km/h); not drawn.
//   chasingdragon_*        programs 136/137 (dragontrig_1000/1100 slot 2):
//                          builtin0 Object + Spline(dragonpath, stop, 90/120 km/h).
//
// Ported instruction by instruction (source Z-up centimetres, seconds):
//   0x359460 constructor        originalSplineConstruct (0x3451C0 bind, 0x359688,
//                               0x317830 -> 0x317810/0x317A08 shared RNG draw)
//   0x359688 set speed          originalSplineSpeed (km/h * 27.777779)
//   0x359698 update (vt+0x14)   originalSplineUpdate
//   0x359830 evaluate (vt+0x1C) originalSplineEvaluate (0x345248 path sample,
//                               0x31C228 atan, 0x31BE50 sincos, VU0 R(axis)*M)
//   0x361B90 matrix (vt+0x94)   originalSplineMatrix (lazy: evaluates when dirty)
//   0x359EB8 message (vt+0x2C)  originalSplineMessage (0x190 speed+start, 0x191 start, 0x192 reverse+start)
//   0x359CF8 contact (vt+0xB4)  originalSplineContactVelocity (surface velocity -> packet +0x20/+0x30)
//   0x361B48/0x361B50 (vt+0x34/+0x3C) finished flag read / clear
//   0x3554B0 attach (+0x356780/0x350570) originalSplineAttach (bounds, radius +0x4C)
//   0x3568B0 entity bounds      originalSplineEntityBounds (bounds part; 0x3291E0 relocation out of scope)
//   0x356F10/0x356FF0/0x361940  Position modifier (originalPositionFromMatrix / Evaluate / Matrix)
//   0x355F10 entity conversion  originalSplineFreeze (Position with the spline's current matrix)
//
// Modifier layout (0xF0): +0x10/+0x20 bounds, +0x30 end mode, +0x34 orientation
// mode, +0x38 roll (rad), +0x3C distance, +0x40 acceleration (cm/s^2), +0x44
// acceleration time (s), +0x48 speed (cm/s), +0x4C radius, +0x50 finished, +0x54
// running, +0x58 dirty, +0x60..+0x9C matrix rows (row-vector, row 3 = translation),
// +0xA0 path point, +0xB0 first derivative, +0xC0 second derivative * dt/ds,
// +0xD0/+0xD4 tangent x^2 / y^2, +0xD8 path (resource, cursor index, cursor
// segment pointer, length). +0x04..+0x0C, +0x5C, +0xE8, +0xEC are never written.
//
// End modes (0x359698): 1 (default, and 3/5+) loop by +-length; 2 ping-pong
// (speed negated, distance reflected: -d at the start, d-(d-(L-0.1)) at the end);
// 0 and 4 stop (distance 0 / L-0.1, speed negated, running 0, finished 1).
// Orientation modes (0x359830): 2/3 yaw from the unit first derivative, 1/3 pitch
// from the raw derivative; roll (+0x38) about the 0x4FF160 axis (0,0,1).
//
// Per game tick (world entity tick, before the rider; the same pass as the
// crashbag rollers): entity 0x356198 runs modifier vt+0x14 (originalSplineUpdate),
// then 0x352ED0: if the finished flag is set it is cleared (0x352F08) and the
// entity's vt+0x114(1) (0x34FD00) fires: authored handler slot 4 of the instance
// runs (0x2D19E8 -> 0x30A5C0) or, without one, the instance flags are restored
// ((f & 0xFFFF0300) | f>>16 | 2) and the entity is destroyed. Then 0x3568B0
// reads the matrix (lazy evaluation) for the entity bounds. For every Snow Jam
// stop-mode piece slot 4 is builtin16 (a particle burst): 0x2FAE38 converts the
// entity into the type-13 entity 0x48EE60 (0x3578A8 -> 0x355F10), which destroys
// the spline and attaches a PositionModifier holding the spline matrix at L-0.1.
//
// Float policy (as engine/roller_modifier.hpp): EE add.s/sub.s ->
// originalScalarAdd/Subtract, mul.s -> terrain_original::mul (chop), div.s/sqrt.s
// -> collision_scalar::divide/squareRoot (nearest; SQRT.S reads Ft), neg.s sign
// flip; VU lanes terrain_original add/sub/mul in ACC order; VRSQRT x>0?1/sqrt(x):0.
// Compile with -ffp-contract=off.
#include "original_spline_path.hpp"
#include "roller_modifier.hpp"
#include "original_random.hpp"
#include "collision_scalar.hpp"
#include <bit>
#include <cstring>
#include <stdexcept>

namespace ssx {

namespace spline_modifier_constants {
inline float constant(uint32_t bits){return std::bit_cast<float>(bits);}
inline const float kDegToRad=constant(0x3c8efa36u);        // gp-0x2A64
inline const float kKmhToCms=constant(0x41de38e4u);        // gp-0x2A60 27.777779
inline const float kEndInset=constant(0x3dcccccdu);        // gp-0x2A5C/-0x2A58/-0x2A54 0.1
inline const float kMinusHalfPi=constant(0xbfc90fdbu);     // gp-0x2A50, -0x2A40
inline const float kHalfPi=constant(0x3fc90fdbu);          // gp-0x2A4C, -0x2A3C
inline const float kPi=constant(0x40490fdbu);              // gp-0x2A48/-0x2A44, -0x2A38/-0x2A34
inline const float kVelocityEpsilon=constant(0x3727c5acu); // gp-0x2A30 1e-5
inline const float kAccelerationScale=100.f;               // 0x359460 lui 0x42C8
inline const float kClockStep=constant(0x3c888889u);       // clock +0x14 (0x2D1C70), 1/60
inline const SplineQuad kAxis{0.f,0.f,1.f,0.f};            // 0x4FF160 (runtime BSS, every savestate)
}

// builtin19 argument block (defaults at 0x4FB778, written by 0x2FDED0 on first use;
// field types at 0x4461D8: int int int int float float float float int float float;
// an int program literal given for a float field is converted with cvt.s.w).
struct OriginalSplineScriptArgs {
    int32_t instance=-1;        // field 0 (+0x00): -1 = current script instance (script context +0x290)
    uint32_t spline=0xFFFFFFFFu;// field 1 (+0x04): kind-8 spline resource; -1 = nothing is created
    int32_t endMode=1;          // field 2 (+0x08) -> +0x30
    int32_t orientation=3;      // field 3 (+0x0C) -> +0x34
    float speedKmh=20.f;        // field 4 (+0x10) -> +0x48 (* 27.777779); sign picks the start end
    float rollDegrees=0.f;      // field 5 (+0x14) -> +0x38 (* 0.017453)
    float startDistance=0.f;    // field 6 (+0x18) -> +0x3C when nonzero
    float startJitter=0.f;      // field 7 (+0x1C) start += rand(-1,1) * jitter (the draw always happens)
    int32_t running=1;          // field 8 (+0x20) -> +0x54 (!= 0)
    float acceleration=0.f;     // field 9 (+0x24) -> +0x40 (* 100)
    float accelerationTime=0.f; // field 10 (+0x28) -> +0x44
};

struct OriginalSplineModifier {
    // +0x00 vtable 0x48F250 (not modeled).
    std::array<uint32_t,3> word04{};   // +0x04..+0x0C never written (allocation contents)
    SplineQuad boundsMin{},boundsMax{};// +0x10/+0x20 (0x3554B0 attach, 0x3568B0)
    int32_t endMode=1;                 // +0x30
    int32_t orientation=3;             // +0x34
    float roll=0;                      // +0x38 radians
    float distance=0;                  // +0x3C cm along the path
    float acceleration=0;              // +0x40 cm/s^2
    float accelerationTime=0;          // +0x44 s remaining
    float speed=0;                     // +0x48 cm/s (signed)
    float radius=0;                    // +0x4C (0x350570 attach: largest authored-AABB corner distance)
    uint32_t finished=0;               // +0x50
    uint32_t running=0;                // +0x54
    uint32_t dirty=0;                  // +0x58
    uint32_t word5C=0;                 // +0x5C never written
    RollerMatrix matrix{};             // +0x60..+0x9F
    SplineQuad position{},tangent{},curvature{}; // +0xA0/+0xB0/+0xC0 (0x345248 outputs)
    float tangentXX=0,tangentYY=0;     // +0xD0/+0xD4
    OriginalSplinePath path;           // +0xD8 resource, +0xDC cursor index, +0xE0 cursor segment, +0xE4 length
    std::array<uint32_t,2> wordE8{};   // +0xE8/+0xEC never written
};

// 0x359688 (also message 0x190).
inline float originalSplineSpeed(float kmh){OriginalRounding rounding;return terrain_original::mul(kmh,spline_modifier_constants::kKmhToCms);}

// 0x317830(-1, 1): shared RNG 0x4FF030 (0x317A08) -> [1,2) mantissa trick -> -1 + 2*(u-1).
inline float originalSplineRandomSigned(OriginalRandomState& random){
    using terrain_original::mul;OriginalRounding rounding;
    const uint32_t v=random.next();
    const float unit=originalScalarSubtract(std::bit_cast<float>((v&0x7FFFFFu)|0x3F800000u),1.f);
    const float span=originalScalarSubtract(1.f,-1.f);
    return originalScalarAdd(-1.f,mul(span,unit));
}

// 0x359460. `record` is the bound kind-8 spline (args.spline). Draws exactly one
// value from the shared RNG. Unwritten words keep their previous contents.
inline void originalSplineConstruct(OriginalSplineModifier& m,const OriginalSplineScriptArgs& a,const OriginalRailRecord& record,OriginalRandomState& random){
    using namespace spline_modifier_constants;using terrain_original::mul;
    m.radius=0;
    m.endMode=a.endMode;m.orientation=a.orientation;
    {OriginalRounding rounding;m.roll=mul(a.rollDegrees,kDegToRad);m.acceleration=mul(a.acceleration,kAccelerationScale);}
    m.accelerationTime=a.accelerationTime;
    if(record.packedId!=a.spline)throw std::runtime_error("SplineModifier: bound record differs from the script spline");
    m.path=originalSplineBind(record);
    m.speed=originalSplineSpeed(a.speedKmh);
    m.distance=0.f<=a.speedKmh?0.f:m.path.length;
    m.finished=0;m.running=a.running!=0;
    if(a.startDistance!=0.f)m.distance=a.startDistance;
    const float r=originalSplineRandomSigned(random);
    {
        OriginalRounding rounding;
        const float d=originalScalarAdd(m.distance,mul(r,a.startJitter));
        m.distance=d;
        if(d<0.f||m.path.length<d)m.distance=0.f;
    }
    m.tangent={0,0,0,0};m.curvature={0,0,0,0};   // 0x4FF120 (runtime BSS zero quad)
    m.dirty=1;m.tangentXX=0;m.tangentYY=0;
}

// 0x359698 (dt = clock +0x14, read through 0x2D1C70).
inline void originalSplineUpdate(OriginalSplineModifier& m,float dt=spline_modifier_constants::kClockStep){
    using namespace spline_modifier_constants;using terrain_original::mul;OriginalRounding rounding;
    if(0.f<m.accelerationTime){
        m.accelerationTime=originalScalarSubtract(m.accelerationTime,dt);
        m.speed=originalScalarAdd(m.speed,mul(m.acceleration,dt));
    }
    if(!m.running||m.finished)return;
    const float speed=m.speed,length=m.path.length;
    const float d=originalScalarAdd(m.distance,mul(speed,dt));
    m.distance=d;
    if(d<0.f){
        if(m.endMode==0||m.endMode==4){m.distance=0.f;m.finished=1;m.speed=-speed;m.running=0;}
        else if(m.endMode==2){m.speed=-speed;m.distance=-d;}
        else m.distance=originalScalarAdd(d,length);
    }else if(length<=d){
        if(m.endMode==0||m.endMode==4){m.running=0;m.finished=1;m.speed=-speed;m.distance=originalScalarSubtract(length,kEndInset);}
        else if(m.endMode==2){
            m.speed=-speed;
            const float inset=originalScalarSubtract(length,kEndInset);
            m.distance=originalScalarSubtract(d,originalScalarSubtract(d,inset));
        }else m.distance=originalScalarSubtract(d,length);
    }
    m.dirty=1;
}

namespace spline_modifier_detail {
// vmul.xyzw; vadday.x; vmaddaz.x; vmaddw.x (vf6.x = 1); vrsqrt; vmulq.xyzw.
inline SplineQuad unit(const SplineQuad& t){
    using namespace terrain_original;
    float len2=add(mul(t[0],t[0]),mul(t[1],t[1]));
    len2=add(len2,mul(1.f,mul(t[2],t[2])));len2=add(len2,mul(1.f,mul(t[3],t[3])));
    return roller_math::vscale(t,roller_math::vuRsqrt(len2));
}
// 0x359830 yaw (f25 = 0 - angle) from the unit first derivative.
inline float yaw(float x,float y){
    using namespace spline_modifier_constants;
    float angle;
    if(x==0.f){
        if(y==0.f)angle=y;
        else angle=0.f<=y?kHalfPi:kMinusHalfPi;
    }else{
        angle=collision_scalar::atan(collision_scalar::divide(y,x));
        if(x<0.f)angle=0.f<y?originalScalarAdd(angle,kPi):originalScalarSubtract(angle,kPi);
    }
    return originalScalarSubtract(0.f,angle);
}
// 0x359830 pitch (f24 = 0 + angle) from the raw derivative.
inline float pitch(float horizontal,float z){
    using namespace spline_modifier_constants;
    float angle;
    if(horizontal==0.f){
        if(z==0.f)angle=z;
        else angle=0.f<=z?kHalfPi:kMinusHalfPi;
    }else{
        angle=collision_scalar::atan(collision_scalar::divide(z,horizontal));
        if(horizontal<0.f)angle=0.f<z?originalScalarAdd(angle,kPi):originalScalarSubtract(angle,kPi);
    }
    return originalScalarAdd(0.f,angle);
}
// 0x359B34 rotation about the unit axis (row-vector), row 3 = (0,0,0,1).
inline RollerMatrix axisRotation(float angle,const SplineQuad& axis){
    using terrain_original::mul;
    const auto sc=collision_scalar::sincos(angle);const float s=sc[0],c=sc[1];
    const float ax=axis[0],ay=axis[1],az=axis[2];
    OriginalRounding rounding;
    const float t=originalScalarSubtract(1.f,c);
    const float tz=mul(t,az),tx=mul(t,ax),ty=mul(t,ay),sz=mul(s,az),sx=mul(s,ax),sy=mul(s,ay);
    RollerMatrix r;
    r[0]={originalScalarAdd(mul(tx,ax),c),originalScalarSubtract(mul(ty,ax),sz),originalScalarAdd(mul(tz,ax),sy),0.f};
    r[1]={originalScalarAdd(mul(tx,ay),sz),originalScalarAdd(mul(ty,ay),c),originalScalarSubtract(mul(tz,ay),sx),0.f};
    r[2]={originalScalarSubtract(mul(tx,az),sy),originalScalarAdd(mul(ty,az),sx),originalScalarAdd(mul(tz,az),c),0.f};
    r[3]={0.f,0.f,0.f,1.f};
    return r;
}
}

// 0x359830: sample the path at +0x3C, Euler matrix (roll 0, pitch, yaw) with the
// path point as translation, then R(axis, +0x38) * M when the roll is nonzero.
inline void originalSplineEvaluate(OriginalSplineModifier& m){
    using namespace spline_modifier_constants;using terrain_original::mul;
    const auto sample=originalSplineEvaluate(m.path,m.distance);
    m.position=sample.position;m.tangent=sample.tangent;m.curvature=sample.curvature;
    OriginalRounding rounding;
    m.tangentXX=mul(m.tangent[0],m.tangent[0]);m.tangentYY=mul(m.tangent[1],m.tangent[1]);
    const SplineQuad n=spline_modifier_detail::unit(m.tangent);
    float yaw=0.f,pitch=0.f;
    if(m.orientation==2||m.orientation==3)yaw=spline_modifier_detail::yaw(n[0],n[1]);
    if(m.orientation==1||m.orientation==3){
        const float horizontal=collision_scalar::squareRoot(originalScalarAdd(m.tangentXX,m.tangentYY));
        pitch=spline_modifier_detail::pitch(horizontal,m.tangent[2]);
    }
    const auto r=collision_scalar::sincos(0.f),p=collision_scalar::sincos(pitch),w=collision_scalar::sincos(yaw);
    const float a=r[0],b=r[1],c=p[0],d=p[1],e=w[0],g=w[1];
    const float ac=mul(a,c),be=mul(b,e);
    RollerMatrix M;
    M[0]={mul(d,g),mul(-d,e),c,0.f};
    M[1]={originalScalarAdd(mul(ac,g),be),originalScalarSubtract(mul(b,g),mul(ac,e)),mul(-a,d),0.f};
    M[2]={originalScalarSubtract(mul(a,e),mul(mul(b,c),g)),originalScalarAdd(mul(be,c),mul(a,g)),mul(b,d),0.f};
    M[3]={m.position[0],m.position[1],m.position[2],1.f};
    if(m.roll!=0.f){
        const RollerMatrix R=spline_modifier_detail::axisRotation(m.roll,kAxis);
        RollerMatrix out;for(unsigned k=0;k<4;++k)out[k]=roller_math::transform(M,R[k]);
        M=out;
    }
    m.matrix=M;m.dirty=0;
}

// 0x361B90 (modifier vt+0x94, reached through entity vt+0xC4 0x356078): the
// entity/render/bounds matrix. Evaluates first when dirty.
inline const RollerMatrix& originalSplineMatrix(OriginalSplineModifier& m){
    if(m.dirty)originalSplineEvaluate(m);
    return m.matrix;
}

// 0x361B48 + 0x352F08: the entity update's end-of-path check (0x352ED0). Returns
// true (and clears +0x50) when the entity must fire vt+0x114(1) this tick.
inline bool originalSplineTakeFinished(OriginalSplineModifier& m){
    if(!m.finished)return false;
    m.finished=0;return true;
}

// 0x359EB8 (modifier vt+0x2C): 0x190 set speed (km/h) and start, 0x191 start, 0x192 reverse and start.
inline bool originalSplineMessage(OriginalSplineModifier& m,uint32_t id,float value){
    if(id==0x190){m.speed=originalSplineSpeed(value);m.finished=0;m.running=1;return true;}
    if(id==0x191){m.finished=0;m.running=1;return true;}
    if(id==0x192){m.speed=-m.speed;m.running=1;m.finished=0;return true;}
    return false;
}

// 0x359CF8 (modifier vt+0xB4, 104E70 selected-entity callback): only while running and
// not finished; packet +0x20 += linear + (point - path point) x angular, +0x30 += angular.
struct OriginalSplineContactVelocity {bool applied=false;SplineQuad linear{},angular{};};
inline OriginalSplineContactVelocity originalSplineContactVelocity(const OriginalSplineModifier& m,const SplineQuad& point){
    using namespace spline_modifier_constants;using terrain_original::mul;
    OriginalSplineContactVelocity out;
    if(!m.running||m.finished)return out;
    OriginalRounding rounding;
    float yawRate=0.f,pitchRate=0.f;   // f8 / f5
    const float tx=m.tangent[0];
    if(kVelocityEpsilon<std::fabs(tx)){
        const float cx=m.curvature[0],cy=m.curvature[1],cz=m.curvature[2];
        const float ratio=collision_scalar::divide(cy,tx);
        const float sum=originalScalarAdd(cx,cy),planar=originalScalarAdd(m.tangentXX,m.tangentYY);
        const float horizontal=collision_scalar::squareRoot(planar);
        const float inverse=collision_scalar::divide(1.f,horizontal);
        const float a=collision_scalar::divide(mul(m.tangent[1],cx),m.tangentXX);
        const float b=collision_scalar::divide(mul(m.tangent[2],sum),planar);
        const float c=collision_scalar::divide(-m.tangentXX,planar);
        yawRate=mul(c,originalScalarSubtract(ratio,a));
        pitchRate=mul(inverse,originalScalarSubtract(cz,b));
    }
    const SplineQuad linear=roller_math::vscale(spline_modifier_detail::unit(m.tangent),m.speed);
    const SplineQuad local{0.f,mul(pitchRate,m.speed),mul(yawRate,m.speed),0.f};
    const SplineQuad angular=roller_math::transform(m.matrix,local);
    const SplineQuad arm=roller_math::vsub(point,m.position);
    out.applied=true;out.angular=angular;out.linear=roller_math::vadd(linear,roller_math::cross(arm,angular));
    return out;
}
// Packet update of 0x359CF8: +0x20 = +0x20 + linear, +0x30 = +0x30 + angular (VU vadd).
inline void originalSplineApplyContactVelocity(const OriginalSplineContactVelocity& v,SplineQuad& packet20,SplineQuad& packet30){
    if(!v.applied)return;
    OriginalRounding rounding;packet20=roller_math::vadd(packet20,v.linear);packet30=roller_math::vadd(packet30,v.angular);
}

// 0x3554B0 -> 0x356780 -> 0x350570 attach (bounds = authored AABB, radius = largest
// corner distance from the instance translation, sqrt.s), instance flags &~0x20 | 0x40.
inline uint32_t originalSplineAttachBounds(SplineQuad& boundsMin,SplineQuad& boundsMax,float& radius,const SplineQuad& instanceTranslation,
        const std::array<float,3>& authoredLow,const std::array<float,3>& authoredHigh,uint32_t instanceFlags){
    using namespace roller_math;OriginalRounding rounding;
    SplineQuad low{authoredLow[0],authoredLow[1],authoredLow[2],1.f},high{authoredHigh[0],authoredHigh[1],authoredHigh[2],1.f};
    float largest=0.f;
    for(unsigned corner=0;corner<8;++corner){
        SplineQuad p{corner&1?low[0]:high[0],corner&2?low[1]:high[1],corner&4?low[2]:high[2],1.f};
        SplineQuad delta=vsub(p,instanceTranslation);
        float squared=dot4(delta,delta);
        if(largest<squared)largest=squared;
    }
    boundsMin=low;boundsMax=high;radius=originalScalarSqrt(largest);
    return (instanceFlags&~0x20u)|0x40u;
}
inline uint32_t originalSplineAttach(OriginalSplineModifier& m,const SplineQuad& instanceTranslation,
        const std::array<float,3>& authoredLow,const std::array<float,3>& authoredHigh,uint32_t instanceFlags){
    return originalSplineAttachBounds(m.boundsMin,m.boundsMax,m.radius,instanceTranslation,authoredLow,authoredHigh,instanceFlags);
}

// 0x3568B0 bounds part: matrix (lazy evaluation) translation -/+ radius.
struct OriginalSplineBounds {SplineQuad oldMin{},oldMax{},min{},max{};};
inline OriginalSplineBounds originalSplineEntityBounds(OriginalSplineModifier& m){
    const RollerMatrix& M=originalSplineMatrix(m);
    using namespace roller_math;OriginalRounding rounding;OriginalSplineBounds out;
    out.oldMin=m.boundsMin;out.oldMax=m.boundsMax;
    const SplineQuad extent{m.radius,m.radius,m.radius,0.f};
    out.min=vsub(M[3],extent);out.max=vadd(M[3],extent);
    m.boundsMin=out.min;m.boundsMax=out.max;
    return out;
}

// ---- PositionModifier (vtable 0x48F5F0, 0x90 bytes) -----------------------------
struct OriginalPositionModifier {
    std::array<uint32_t,3> word04{};   // +0x04..+0x0C never written
    SplineQuad boundsMin{},boundsMax{};// +0x10/+0x20
    SplineQuad translation{};          // +0x30 (ctor: matrix row 3)
    float radius=0;                    // +0x40 (vt+0x74 0x361930 / +0x7C 0x361938)
    uint32_t dirty=0;                  // +0x44
    std::array<uint32_t,2> word48{};   // +0x48/+0x4C never written
    RollerMatrix matrix{};             // +0x50..+0x8F (row 3 at +0x80)
};
// 0x356F10 (factory 0x355918 = allocate 0x90 + ctor + 0x3554B0 attach).
inline void originalPositionFromMatrix(OriginalPositionModifier& p,const RollerMatrix& matrix){
    p.translation=matrix[3];p.matrix=matrix;p.dirty=1;
}
// 0x356FF0 (vt+0x1C): +0x80 = +0x30, clean. The update 0x3618F8 is empty.
inline void originalPositionEvaluate(OriginalPositionModifier& p){p.matrix[3]=p.translation;p.dirty=0;}
// 0x361940 (vt+0x94).
inline const RollerMatrix& originalPositionMatrix(OriginalPositionModifier& p){if(p.dirty)originalPositionEvaluate(p);return p.matrix;}
inline uint32_t originalPositionAttach(OriginalPositionModifier& p,const SplineQuad& instanceTranslation,
        const std::array<float,3>& authoredLow,const std::array<float,3>& authoredHigh,uint32_t instanceFlags){
    return originalSplineAttachBounds(p.boundsMin,p.boundsMax,p.radius,instanceTranslation,authoredLow,authoredHigh,instanceFlags);
}
inline OriginalSplineBounds originalPositionEntityBounds(OriginalPositionModifier& p){
    const RollerMatrix& M=originalPositionMatrix(p);
    using namespace roller_math;OriginalRounding rounding;OriginalSplineBounds out;
    out.oldMin=p.boundsMin;out.oldMax=p.boundsMax;
    const SplineQuad extent{p.radius,p.radius,p.radius,0.f};
    out.min=vsub(M[3],extent);out.max=vadd(M[3],extent);
    p.boundsMin=out.min;p.boundsMax=out.max;
    return out;
}
// 0x355F10 conversion of the spline's entity (type-13 particle entity 0x3578A8 from
// builtin16 in the slot-4 end program): the old entity's matrix (0x356078 -> 0x361B90,
// evaluated if dirty) becomes a PositionModifier; the SplineModifier is destroyed.
inline OriginalPositionModifier originalSplineFreeze(OriginalSplineModifier& m){
    OriginalPositionModifier p;originalPositionFromMatrix(p,originalSplineMatrix(m));return p;
}

// ---- byte images (development oracles / diagnostics) ------------------------------
// `segmentBase` = runtime address of the bound record's first segment (the record's
// 144-byte segments are contiguous): +0xE0 = segmentBase + 0x90 * cursor.
inline std::array<uint8_t,0xF0> originalSplineModifierBytes(const OriginalSplineModifier& m,uint32_t segmentBase){
    std::array<uint8_t,0xF0> b{};
    auto put=[&](uint32_t o,const auto& v){std::memcpy(b.data()+o,&v,sizeof v);};
    put(0,uint32_t(0x48F250u));put(4,m.word04);put(0x10,m.boundsMin);put(0x20,m.boundsMax);
    put(0x30,m.endMode);put(0x34,m.orientation);put(0x38,m.roll);put(0x3C,m.distance);put(0x40,m.acceleration);put(0x44,m.accelerationTime);
    put(0x48,m.speed);put(0x4C,m.radius);put(0x50,m.finished);put(0x54,m.running);put(0x58,m.dirty);put(0x5C,m.word5C);
    put(0x60,m.matrix);put(0xA0,m.position);put(0xB0,m.tangent);put(0xC0,m.curvature);put(0xD0,m.tangentXX);put(0xD4,m.tangentYY);
    put(0xD8,m.path.resource);put(0xDC,m.path.index);put(0xE0,uint32_t(segmentBase+0x90u*uint32_t(m.path.cursor)));put(0xE4,m.path.length);put(0xE8,m.wordE8);
    return b;
}
inline OriginalSplineModifier originalSplineModifierFromBytes(const uint8_t* b,const OriginalRailRecord* record,uint32_t segmentBase){
    OriginalSplineModifier m;
    auto get=[&](uint32_t o,auto& v){std::memcpy(&v,b+o,sizeof v);};
    get(4,m.word04);get(0x10,m.boundsMin);get(0x20,m.boundsMax);get(0x30,m.endMode);get(0x34,m.orientation);get(0x38,m.roll);get(0x3C,m.distance);
    get(0x40,m.acceleration);get(0x44,m.accelerationTime);get(0x48,m.speed);get(0x4C,m.radius);get(0x50,m.finished);get(0x54,m.running);get(0x58,m.dirty);
    get(0x5C,m.word5C);get(0x60,m.matrix);get(0xA0,m.position);get(0xB0,m.tangent);get(0xC0,m.curvature);get(0xD0,m.tangentXX);get(0xD4,m.tangentYY);
    get(0xD8,m.path.resource);get(0xDC,m.path.index);uint32_t segment;get(0xE0,segment);get(0xE4,m.path.length);get(0xE8,m.wordE8);
    m.path.record=record;
    if(record){
        if(segment<segmentBase||(segment-segmentBase)%0x90u||(segment-segmentBase)/0x90u>=record->segments.size())throw std::runtime_error("SplineModifier cursor outside the record");
        m.path.cursor=(segment-segmentBase)/0x90u;
    }
    return m;
}
inline std::array<uint8_t,0x90> originalPositionModifierBytes(const OriginalPositionModifier& p){
    std::array<uint8_t,0x90> b{};
    auto put=[&](uint32_t o,const auto& v){std::memcpy(b.data()+o,&v,sizeof v);};
    put(0,uint32_t(0x48F5F0u));put(4,p.word04);put(0x10,p.boundsMin);put(0x20,p.boundsMax);put(0x30,p.translation);put(0x40,p.radius);put(0x44,p.dirty);
    put(0x48,p.word48);put(0x50,p.matrix);
    return b;
}
}
