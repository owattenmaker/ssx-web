#pragma once
// Original RollerModifier (ELF string "RollerModifier" 0x48E908, vtable 0x48F080,
// 0x2D0-byte object): the crashbag rigid body created by script builtin15
// (0x355DB8 -> ctor 0x35DA70) when the rider hits a crashbag, then ticked by
// 0x35E850 before the rider's controllers each game tick.
//
// Ported instruction by instruction (source Z-up centimeters, seconds):
//   0x35E850 update            originalRollerUpdate
//   0x35ED90 (+0x32C630)       originalRollerTranslate
//   0x35E770 + inline quaternion->matrix VU block (0x35E8D4/0x35EC40/0x35ECE8, 0x35DCB0)
//                              originalRollerWorldTransform / originalRollerQuaternionMatrix
//   0x35D340 integrate         originalRollerIntegrate
//   0x35E248 world inv inertia originalRollerWorldInverseInertia
//   0x35D288 velocities        originalRollerVelocities
//   0x32C648 collider corners  originalRollerColliderTransform (corner table 0x4FF640 = (+-1,+-1,+-1,0))
//   0x35EDC8 collision step    originalRollerCollision (world query 0x3303F0/0x2D1BE0/0x336850/0x3304E8 is a callback)
//   0x32C5A8 leaf radius       originalRollerLeafRadius
//   0x35D4A0 contact response  originalRollerContactResponse
//   0x35D908 impulse           originalRollerApplyImpulse
//   0x35DA70 constructor       originalRollerConstruct (0x35CFE8, 0x32C508/0x32C540, 0x327CC8, 0x31B748/0x31B7A8,
//                              0x35E770, 0x35E248, 0x35D288, 0x35DDE8)
//   0x31B7A8 matrix->quat      originalRollerMatrixQuaternion
//   0x35DDE8 kick (+0x35CFF0)  originalRollerKick; 0x361CD8 (vtable+0x54, rider re-contact) originalRollerRecontact
//   0x35CFF0 set angular vel.  originalRollerSetAngularVelocity
//   0x3568B0 entity bounds     originalRollerEntityBounds (bounds part only, see below)
//
// Float policy: VU lanes use terrain_original add/sub/mul (chop, one rounding per
// product and per sum, ACC chains in instruction order, dot products include the
// w-lane product); VU Q unit as recompiled: VDIV x!=0?1/x:0, VSQRT sqrt(max(0,x)),
// VRSQRT x>0?1/sqrt(x):0 (all chop). EE add.s/sub.s -> originalScalarAdd/Subtract
// (one guard bit), mul.s -> terrain_original::mul, div.s/sqrt.s ->
// originalScalarDivide/Sqrt (nearest), min.s b<a?b:a, neg.s sign flip.
// Compile with -ffp-contract=off.
//
// Update order per game tick (before the rider): 0x35E850(modifier), then the
// entity's 0x3568B0 (originalRollerEntityBounds + out-of-scope relocation).
// Construction: builtin15 0x355DB8 = allocate 0x2D0, 0x35DA70
// (originalRollerConstruct), 0x3554B0 attach (originalRollerAttach).
//
// Residual assumptions (documented, all hold for finite register contents):
// * EE div.s by an exact zero follows IEEE (x/0 = +-inf, 0/0 = NaN); the recompiled
//   oracle returns copysign(inf, x) and the PS2 +-FLT_MAX. Not reachable with the
//   script's mass 1.0 (divisors: mass, 1/inverseMass, det(inverse inertia), 0.5/root
//   and the impulse denominators inverseMass + positive-definite term).
// * `vsub.w vfN,vfN,vfN` on stale VU registers (vf6/vf8/vf10..12) yields +0; the
//   port writes +0 in those w lanes (a stale Inf/NaN would give NaN).
// * The world query callback reproduces 0x3303F0/0x336850/0x3304E8: it returns
//   0x336850's f0 depth (-1 without contact) and the packet +0x00 point / +0x10
//   normal (all four lanes are used: push, lever arm and dot products include w),
//   updates the +0x2C0 terrain cache and the collider scratch (+0x94, +0x9C..).
// * Radius +0x24 is not written by the constructor; the script sets it through
//   modifier vtable+0x78 (0x361D28, `swc1 f12,0x24(a0)`). originalRollerSetRadius.
// * 0x3568B0 then relocates the instance in the world spatial tree
//   (0x2D1BE0 world, 0x3291E0(world,0,instance,new bounds,old bounds)); that is
//   out of scope here: originalRollerEntityBounds returns old and new bounds.
//   0x3568B0 finally calls modifier vtable+0xC4 = 0x360BE8 (no-op).
#include "roller_collider.hpp"
#include "terrain_contact_math.hpp"
#include <cstring>
#include <functional>
#include <optional>
#include <stdexcept>

namespace ssx {
using RollerMatrix=std::array<RollerQuad,4>;

namespace roller_math {
// VU lane arithmetic (current chop rounding).
inline RollerQuad vadd(RollerQuad a,const RollerQuad& b){for(unsigned k=0;k<4;++k)a[k]=terrain_original::add(a[k],b[k]);return a;}
inline RollerQuad vsub(RollerQuad a,const RollerQuad& b){for(unsigned k=0;k<4;++k)a[k]=terrain_original::sub(a[k],b[k]);return a;}
// vmulx.xyzw vfd, vfs, vf3x with vf3.x = s (qmtc2 of a float) / vmulq.xyzw.
inline RollerQuad vscale(RollerQuad a,float s){for(auto& x:a)x=terrain_original::mul(x,s);return a;}
// vmul.xyzw; vadday.x; vmaddaz.x (vf6.x=1); vmaddw.x (vf6.x=1).
inline float dot4(const RollerQuad& a,const RollerQuad& b){
    using namespace terrain_original;
    float acc=add(mul(a[0],b[0]),mul(a[1],b[1]));
    acc=add(acc,mul(1.f,mul(a[2],b[2])));
    return add(acc,mul(1.f,mul(a[3],b[3])));
}
// vopmula.xyz ACC,a,b; vopmsub.xyz d,b,a; vsub.w d,d,d (+0).
inline RollerQuad cross(const RollerQuad& a,const RollerQuad& b){
    using namespace terrain_original;
    return {sub(mul(a[1],b[2]),mul(b[1],a[2])),sub(mul(a[2],b[0]),mul(b[2],a[0])),sub(mul(a[0],b[1]),mul(b[0],a[1])),0.f};
}
inline float vuDivideOne(float x){return x!=0?terrain_original::div(1.f,x):0.f;}     // VDIV Q,vf0w,x
inline float vuSqrt(float x){return terrain_original::sqrt(x>0?x:0.f);}               // VSQRT (sqrtf(max(0,x)))
inline float vuRsqrt(float x){return x>0?terrain_original::div(1.f,terrain_original::sqrt(x)):0.f;} // VRSQRT Q,vf0w,x
inline float eeMin(float a,float b){return b<a?b:a;}
// EE pattern (0x35D288/0x35D908/0x35D4A0/0x35CFF0/0x35E850/0x35EDC8):
// out.x = (M0*v.x + M3*v.y) + M6*v.z, out.y from M1/M4/M7, out.z from M2/M5/M8, out.w = v.w.
inline RollerQuad matrixTimes(const std::array<float,9>& m,const RollerQuad& v){
    using terrain_original::mul;
    auto row=[&](unsigned c){return originalScalarAdd(originalScalarAdd(mul(m[c],v[0]),mul(m[3+c],v[1])),mul(m[6+c],v[2]));};
    return {row(0),row(1),row(2),v[3]};
}
// 0x35E770 rotation block: t=q x v, u=q x t, v + t*w + t*w + u + u (xyz), w=+0.
inline RollerQuad rotate(const RollerQuad& q,const RollerQuad& v){
    using namespace terrain_original;
    RollerQuad t=cross(q,v),u=cross(q,t),out{0,0,0,0};
    for(unsigned k=0;k<3;++k){
        float acc=mul(v[k],1.f);
        acc=add(acc,mul(t[k],q[3]));acc=add(acc,mul(t[k],q[3]));acc=add(acc,mul(u[k],1.f));
        out[k]=add(acc,mul(u[k],1.f));
    }
    return out;
}
// Inline quaternion->matrix VU block (rows vf10/vf11/vf12 with w=+0, row 3 = t).
inline RollerMatrix quaternionMatrix(const RollerQuad& q,const RollerQuad& t){
    using namespace terrain_original;
    float d[3],s[3],p[3],c[3];
    for(unsigned k=0;k<3;++k)d[k]=add(q[k],q[k]);                 // vf5 = q+q
    for(unsigned k=0;k<3;++k){s[k]=mul(d[k],q[k]);p[k]=mul(d[k],q[3]);} // vf6 = 2q*q, vf7 = 2q*w
    c[0]=add(mul(d[1],q[2]),mul(0.f,0.f));                          // vf8 = opmula(vf5,q) + vf0*vf0
    c[1]=add(mul(d[2],q[0]),mul(0.f,0.f));
    c[2]=add(mul(d[0],q[1]),mul(0.f,0.f));
    RollerMatrix m;
    m[0]={sub(sub(1.f,s[1]),mul(1.f,s[2])),add(add(0.f,c[2]),mul(1.f,p[2])),sub(add(0.f,c[1]),mul(1.f,p[1])),0.f};
    m[1]={sub(add(0.f,c[2]),mul(1.f,p[2])),sub(sub(1.f,s[2]),mul(1.f,s[0])),add(add(0.f,c[0]),mul(1.f,p[0])),0.f};
    m[2]={add(add(0.f,c[1]),mul(1.f,p[1])),sub(add(0.f,c[0]),mul(1.f,p[0])),sub(sub(1.f,s[0]),mul(1.f,s[1])),0.f};
    m[3]=t;
    return m;
}
// vmulax/vmadday/vmaddaz/vmaddw.xyzw: ((M0*c.x + M1*c.y) + M2*c.z) + M3*c.w.
inline RollerQuad transform(const RollerMatrix& m,const RollerQuad& c){
    using namespace terrain_original;RollerQuad out;
    for(unsigned k=0;k<4;++k)out[k]=add(add(add(mul(m[0][k],c[0]),mul(m[1][k],c[1])),mul(m[2][k],c[2])),mul(m[3][k],c[3]));
    return out;
}
}

// RollerModifier+0x30 rigid body (0x35D288/0x35D340/0x35D4A0/0x35D908/0x35CFF0 view).
struct OriginalRollerRigidBody {
    RollerQuad position{};                  // +0x00 (modifier +0x30)
    RollerQuad orientation{};               // +0x10 (+0x40) quaternion x,y,z,w
    RollerQuad linearMomentum{};            // +0x20 (+0x50) P
    RollerQuad angularMomentum{};           // +0x30 (+0x60) L
    float inverseMass=0;                    // +0x40 (+0x70)
    std::array<uint32_t,3> stale44{};       // +0x44 (+0x74) never written (allocation contents)
    std::array<float,9> inverseInertia{};   // +0x50 (+0x80) world inverse inertia, row-major 3x3 (0x35E248)
    std::array<uint32_t,3> stale74{};       // +0x74 (+0xA4) never written
    RollerQuad velocity{};                  // +0x80 (+0xB0) P*inverseMass
    RollerQuad angularVelocity{};           // +0x90 (+0xC0) inverseInertia*L
};

// 0x2D0-byte RollerModifier. Offsets are the original ones.
struct OriginalRollerModifier {
    // +0x00 vtable 0x48F080 (base 0x4913F8 during construction); pointer, not modeled.
    float energy=0;                         // +0x04 0.95*e + 0.05*(v.P + w.L)/mass (0x35E850); kick: v.P + w.L
    float mass=1;                           // +0x08 script arg +4 (1.0)
    float scriptParameter=0;                // +0x0C script arg +8 (*(gp-0x3388) = 0.7), unused by the dynamics
    uint32_t word10=0;                      // +0x10 zeroed by the constructor
    float restitution=0;                    // +0x14 0.1 (gp-0x29FC)
    float friction=0;                       // +0x18 0.4 (gp-0x29F8)
    float restitutionSpeed=0;               // +0x1C 277.78 (gp-0x29F4): below this speed restitution is 0
    float timer=0;                          // +0x20 seconds (+1/60 per tick). >=10 or energy<10000: rest (momenta
                                            //   zeroed, timer=11); 11..16 frozen; >=16 sinks 10/tick. A contact
                                            //   while timer>5 sets 10. vtable+0x30 (0x361CB0): timer>=17
    float radius=0;                         // +0x24 entity bounds half-extent (vtable+0x78 = 0x361D28)
    float contactTimer=0;                   // +0x28 contact time (+1/60 per contact tick, *0.8333 per tick)
    uint32_t word2C=0;                      // +0x2C never written
    OriginalRollerRigidBody body;           // +0x30..+0xCF
    uint32_t instance=0;                    // +0xD0 instance pointer (packet +0x50); identity only
    std::array<uint32_t,3> staleD4{};       // +0xD4 never written
    OriginalSphereTreeCollider collider;    // +0xE0 (0x140-byte 0x32C508 object; +0x98 -> +0x1A0 header copy)
    uint32_t colliderWordBC=0;              // +0x19C (collider +0xBC) never written
    // +0x1A0 (collider +0xC0): 0x80-byte sphere-tree header copied by 0x327CC8 from the
    // hit node's collider (runtime sphere-tree header). Words: [0] resource id,
    // [2] compressed, [3] depth (leaf level index), [5..7] local centre, [8] levels
    // pointer, [10] decoded masks pointer (filled lazily by the query), [11..13]
    // centre of mass, [14..22] 3x3 inertia, [23..31] 3x3 inverse inertia (body frame).
    std::array<uint32_t,32> treeHeader{};
    RollerQuad boundsMin{},boundsMax{};     // +0x220/+0x230 (vtable+0x64 0x361D18, +0x68 0x361D00)
    RollerMatrix matrix{};                  // +0x240 world matrix rows (vtable+0x94/+0x9C 0x361D30/0x361D38)
    std::array<float,9> bodyInertia{};      // +0x280 header inverse inertia * 0.5 (body frame)
    std::array<uint32_t,3> stale2A4{};      // +0x2A4 never written
    RollerQuad centerOfMass{};              // +0x2B0 header centre of mass (w = +0)
    OriginalRollerTerrainCache cache;       // +0x2C0 0x336850 terrain cell cache
};

// 0x105398 sp+0x80 record (rider+0xA60): +0 point, +0x10 incoming direction,
// +0x20 normal, +0x30 closing speed. Same layout as OriginalInstanceContactRecord.
struct OriginalRollerContactRecord {
    RollerQuad point{},direction{},normal{};
    float closingSpeed=0;
};
// Script argument block of builtin15: +4 mass (1.0), +8 parameter (0.7).
struct OriginalRollerScriptArgs {float mass=1,parameter=0;};
// What 0x35DA70 reads through the 128-byte contact packet: +0x48 the hit node's
// sphere-tree collider (0x140 bytes; 0x327CC8 copies +0x00..+0x8F and +0xC0..+0x13F)
// and +0x50 the instance (+0x10 world matrix, +0x08 flags).
struct OriginalRollerSource {
    std::array<RollerQuad,8> corners{};
    RollerQuad center{};
    std::array<uint32_t,32> treeHeader{};
    const CollisionSphereTree* tree=nullptr;uint32_t treeResource=0;
    RollerMatrix instanceMatrix{};
    uint32_t instance=0;
};

using OriginalRollerWorldQuery=std::function<OriginalRollerWorldContact(OriginalSphereTreeCollider&,OriginalRollerTerrainCache&)>;

namespace roller_detail {
inline float bits(uint32_t word){return std::bit_cast<float>(word);}
// Original gp-relative constants (.sdata, bit patterns).
constexpr float constant(uint32_t word){return std::bit_cast<float>(word);}
inline constexpr float kTimerStep=constant(0x3c888889u);      // gp-0x29EC 1/60
inline constexpr float kEnergyBlend=constant(0x3d4ccccdu);    // gp-0x29E8 0.05
inline constexpr float kEnergyKeep=constant(0x3f733333u);     // gp-0x29E4 0.95
inline constexpr float kRestEnergy=constant(0x461c4000u);     // gp-0x29E0 10000
inline constexpr float kContactDecay=constant(0x3f555555u);   // gp-0x29DC 0.8333333
inline constexpr float kIntegrateStep=constant(0x3c888889u);  // gp-0x29D8 1/60
inline constexpr float kContactDamping=constant(0x3f79999au); // gp-0x29D4 0.975
inline constexpr float kContactStep=constant(0x3c888889u);    // gp-0x29D0 1/60
inline constexpr float kKickSpeed=constant(0x459c4000u);      // gp-0x29F0 5000
inline constexpr float kRestitution=constant(0x3dcccccdu);    // gp-0x29FC 0.1
inline constexpr float kFriction=constant(0x3ecccccdu);       // gp-0x29F8 0.4
inline constexpr float kRestitutionSpeed=constant(0x438ae38eu);// gp-0x29F4 277.77777
inline constexpr float kSlipSpeed=constant(0x3a83126fu);      // gp-0x2A00 0.001
// 0x4FF640 corner table rows 0..3 (rows 4..7 are their negations).
inline constexpr float kCorners[4][4]={{-1,-1,-1,0},{-1,-1,1,0},{-1,1,-1,0},{-1,1,1,0}};
}

// ---- rigid-body kernels ----------------------------------------------------

// 0x35D288: v = P*inverseMass (xyzw), w = inverseInertia*L (w lane = L.w).
inline void originalRollerVelocities(OriginalRollerRigidBody& b){
    OriginalRounding rounding;
    b.velocity=roller_math::vscale(b.linearMomentum,b.inverseMass);
    b.angularVelocity=roller_math::matrixTimes(b.inverseInertia,b.angularMomentum);
}

// 0x35D340(body, force a1, torque a2, f12 dt).
inline void originalRollerIntegrate(OriginalRollerRigidBody& b,const RollerQuad& force,const RollerQuad& torque,float dt){
    using namespace terrain_original;using namespace roller_math;OriginalRounding rounding;
    b.position=vadd(b.position,vscale(b.velocity,dt));
    const float half=0.5f;const auto& q=b.orientation;const auto& w=b.angularVelocity;
    float negativeHalf=sub(sub(1.f,1.f),mul(1.f,half));          // vsuba.w; vmsubx.w vf2
    float p[3],sw[3];for(unsigned k=0;k<3;++k){p[k]=mul(q[k],w[k]);sw[k]=mul(w[k],q[3]);}
    RollerQuad c=cross(w,q),derivative;
    derivative[3]=add(add(mul(negativeHalf,p[0]),mul(negativeHalf,p[1])),mul(negativeHalf,p[2]));
    for(unsigned k=0;k<3;++k)derivative[k]=add(mul(sw[k],half),mul(c[k],half));
    derivative=vscale(derivative,dt);
    for(unsigned k=0;k<4;++k)b.orientation[k]=originalScalarAdd(b.orientation[k],derivative[k]);
    b.orientation=vscale(b.orientation,vuRsqrt(dot4(b.orientation,b.orientation)));
    b.linearMomentum=vadd(b.linearMomentum,vscale(force,dt));
    b.angularMomentum=vadd(b.angularMomentum,vscale(torque,dt));
}

// 0x35E248: world inverse inertia = R^T * (I * R) with R the orientation matrix
// rows and I the body inverse inertia (+0x280), written to body +0x50.
inline void originalRollerWorldInverseInertia(OriginalRollerModifier& m){
    using terrain_original::mul;OriginalRounding rounding;
    auto rows=roller_math::quaternionMatrix(m.body.orientation,{0,0,0,0});
    auto R=[&](unsigned r,unsigned c){return rows[r][c];};
    auto I=[&](unsigned r,unsigned c){return m.bodyInertia[3*r+c];};
    float a[3][3];
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)
        a[r][c]=originalScalarAdd(originalScalarAdd(mul(R(0,c),I(r,0)),mul(R(1,c),I(r,1))),mul(R(2,c),I(r,2)));
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)
        m.body.inverseInertia[3*r+c]=originalScalarAdd(originalScalarAdd(mul(a[0][c],R(0,r)),mul(a[1][c],R(1,r))),mul(a[2][c],R(2,r)));
}

// 0x35E770(out, modifier): world centre = position - R(R(com)) (the offset is
// rotated twice exactly as the original does), orientation copied.
struct OriginalRollerPose {RollerQuad position{},orientation{};};
inline OriginalRollerPose originalRollerPose(const OriginalRollerModifier& m){
    using namespace roller_math;OriginalRounding rounding;
    const auto& q=m.body.orientation;
    RollerQuad once=rotate(q,m.centerOfMass);
    RollerQuad twice=rotate(q,vscale(once,-1.f));
    return {vadd(m.body.position,twice),q};
}
// 0x35E770 followed by the inline quaternion->matrix block writing +0x240.
inline void originalRollerWorldTransform(OriginalRollerModifier& m){
    auto pose=originalRollerPose(m);OriginalRounding rounding;
    m.matrix=roller_math::quaternionMatrix(pose.orientation,pose.position);
}

// 0x32C648(collider, matrix, f12 scale). Centre from the embedded header (+0x14).
inline void originalRollerColliderTransform(OriginalSphereTreeCollider& collider,const std::array<uint32_t,32>& header,
        const RollerMatrix& matrix,float scale){
    using namespace roller_math;OriginalRounding rounding;
    for(unsigned i=0;i<4;++i){
        RollerQuad corner{roller_detail::kCorners[i][0],roller_detail::kCorners[i][1],roller_detail::kCorners[i][2],roller_detail::kCorners[i][3]};
        auto world=transform(matrix,vscale(corner,scale));
        collider.corners[i]=world;collider.corners[7-i]=vscale(world,-1.f);
    }
    using terrain_original::mul;
    RollerQuad center{mul(roller_detail::bits(header[5]),scale),mul(roller_detail::bits(header[6]),scale),mul(roller_detail::bits(header[7]),scale),1.f};
    collider.scale=scale;collider.center=transform(matrix,center);
}

// 0x35ED90(modifier, delta): position += delta; collider centre += delta (0x32C630).
inline void originalRollerTranslate(OriginalRollerModifier& m,const RollerQuad& delta){
    OriginalRounding rounding;
    m.body.position=roller_math::vadd(m.body.position,delta);
    m.collider.center=roller_math::vadd(m.collider.center,delta);
}

// 0x32C5A8: leaf sphere radius = levels[header depth].radius * scale * 0.5.
inline float originalRollerLeafRadius(const OriginalRollerModifier& m){
    if(!m.collider.tree)throw std::runtime_error("Original roller collider has no sphere tree");
    OriginalRounding rounding;
    float radius=m.collider.tree->levels.at(m.treeHeader[3]).radiusCm;
    return terrain_original::mul(terrain_original::mul(radius,m.collider.scale),0.5f);
}

// 0x35D908(body, lever a1, impulse a2).
inline void originalRollerApplyImpulse(OriginalRollerRigidBody& b,const RollerQuad& lever,const RollerQuad& impulse){
    using namespace roller_math;OriginalRounding rounding;
    b.linearMomentum=vadd(b.linearMomentum,impulse);
    b.angularMomentum=vadd(b.angularMomentum,cross(lever,impulse));
    b.velocity=vscale(b.linearMomentum,b.inverseMass);
    b.angularVelocity=matrixTimes(b.inverseInertia,b.angularMomentum);
}

// 0x35D4A0(body, point a1, normal a2, f12 restitution, f13 friction):
// Coulomb friction impulse along the slip direction; only after a friction
// impulse is the normal speed recomputed and a separating contact skipped. With
// friction 0 or slip <= 0.001 the branches go straight to the normal impulse
// (0x35D7AC) with the first normal speed, whatever its sign.
struct OriginalRollerResponseResult {bool friction=false,normal=false;};
inline OriginalRollerResponseResult originalRollerContactResponse(OriginalRollerRigidBody& b,const RollerQuad& point,
        const RollerQuad& normal,float restitution,float friction){
    using namespace roller_math;using terrain_original::mul;OriginalRollerResponseResult out;
    OriginalRounding rounding;
    RollerQuad lever=vsub(point,b.position);
    RollerQuad r=vsub(point,b.position);
    RollerQuad velocity=vadd(b.velocity,cross(b.angularVelocity,r));
    float normalSpeed=dot4(velocity,normal);
    if(!(friction==0)){
        RollerQuad slip=vsub(velocity,vscale(normal,normalSpeed));
        float slipSpeed=vuSqrt(dot4(slip,slip));
        if(roller_detail::kSlipSpeed<slipSpeed){
            RollerQuad tangent=vscale(slip,vuDivideOne(slipSpeed));
            float magnitude=mul(-friction,slipSpeed);
            RollerQuad u=matrixTimes(b.inverseInertia,cross(lever,tangent));
            float denominator=originalScalarAdd(b.inverseMass,dot4(tangent,cross(u,lever)));
            magnitude=originalScalarDivide(magnitude,denominator);
            originalRollerApplyImpulse(b,lever,vscale(tangent,magnitude));
            out.friction=true;
            RollerQuad again=vsub(point,b.position);
            velocity=vadd(b.velocity,cross(b.angularVelocity,again));
            normalSpeed=dot4(velocity,normal);
            if(0<normalSpeed)return out;
        }
    }
    float magnitude=mul(-originalScalarAdd(restitution,1.f),normalSpeed);
    RollerQuad u=matrixTimes(b.inverseInertia,cross(lever,normal));
    float denominator=originalScalarAdd(b.inverseMass,dot4(normal,cross(u,lever)));
    magnitude=originalScalarDivide(magnitude,denominator);
    originalRollerApplyImpulse(b,lever,vscale(normal,magnitude));
    out.normal=true;return out;
}

// 0x35CFF0(body, angular velocity a1): w = a1, L = inverse(world inverse inertia)*w.
inline void originalRollerSetAngularVelocity(OriginalRollerRigidBody& b,const RollerQuad& angular){
    using terrain_original::mul;OriginalRounding rounding;
    b.angularVelocity=angular;
    const auto& m=b.inverseInertia;
    auto s=[](float a,float c){return originalScalarSubtract(a,c);};
    float c0=s(mul(m[4],m[8]),mul(m[5],m[7])),c1=s(mul(m[7],m[2]),mul(m[8],m[1])),c2=s(mul(m[1],m[5]),mul(m[2],m[4]));
    float det=originalScalarAdd(originalScalarAdd(mul(m[0],c0),mul(m[3],c1)),mul(m[6],c2));
    float c3=s(mul(m[6],m[5]),mul(m[8],m[3])),c4=s(mul(m[0],m[8]),mul(m[2],m[6])),c5=s(mul(m[3],m[2]),mul(m[5],m[0]));
    float c6=s(mul(m[3],m[7]),mul(m[4],m[6])),c7=s(mul(m[6],m[1]),mul(m[7],m[0])),c8=s(mul(m[0],m[4]),mul(m[1],m[3]));
    float inverse=originalScalarDivide(1.f,det);
    std::array<float,9> n{mul(c0,inverse),mul(c1,inverse),mul(c2,inverse),mul(c3,inverse),mul(c4,inverse),mul(c5,inverse),mul(c6,inverse),mul(c7,inverse),mul(c8,inverse)};
    b.angularMomentum=roller_math::matrixTimes(n,angular);
}

// 0x35DDE8(modifier, record a1, packet a2 unused): launch velocity and spin.
inline void originalRollerKick(OriginalRollerModifier& m,const OriginalRollerContactRecord& record){
    using namespace roller_math;std::optional<OriginalRounding> rounding;rounding.emplace();
    float lift=originalScalarAdd(originalScalarDivide(roller_detail::kKickSpeed,m.mass),100.f);
    if(600.f<lift)lift=600.f;
    m.contactTimer=0;
    RollerQuad half=vscale(vscale(record.direction,record.closingSpeed),vuDivideOne(2.f));
    RollerQuad velocity=vadd(half,RollerQuad{0,0,lift,0});
    m.body.linearMomentum=vscale(velocity,vuDivideOne(m.body.inverseMass));
    m.body.velocity=velocity;
    RollerQuad spin=vscale(record.normal,10.f);
    originalRollerSetAngularVelocity(m.body,spin);
    m.energy=originalScalarAdd(dot4(m.body.velocity,m.body.linearMomentum),dot4(m.body.angularVelocity,m.body.angularMomentum));
}
// 0x361CD8 (modifier vtable+0x54, rider re-contact through 0x355770): timer=0, kick.
inline void originalRollerRecontact(OriginalRollerModifier& m,const OriginalRollerContactRecord& record){
    m.timer=0;originalRollerKick(m,record);
}
inline void originalRollerSetRadius(OriginalRollerModifier& m,float radius){m.radius=radius;} // 0x361D28

// ---- collision step 0x35EDC8 ------------------------------------------------
struct OriginalRollerCollisionResult {
    bool contact=false,approaching=false;float depth=-1,penetration=0,speed=0;
    OriginalRollerResponseResult response;
};
inline OriginalRollerCollisionResult originalRollerCollision(OriginalRollerModifier& m,const OriginalRollerWorldQuery& query){
    using namespace roller_math;OriginalRollerCollisionResult out;
    if(!query)throw std::runtime_error("Original roller world query unavailable");
    auto contact=query(m.collider,m.cache);            // 0x3303F0, 0x2D1BE0, 0x336850(world,query,packet,+0x2C0)
    out.depth=contact.depth;
    float leaf=originalRollerLeafRadius(m);             // 0x32C5A8 on the modifier's own collider
    std::optional<OriginalRounding> rounding;rounding.emplace();
    float penetration=originalScalarSubtract(contact.depth,leaf);out.penetration=penetration;
    if(!(0.f<penetration))return out;                   // -> 0x3304E8(query,2)
    out.contact=true;
    RollerQuad push=vscale(contact.normal,originalScalarAdd(penetration,2.f));
    originalRollerTranslate(m,push);
    auto& b=m.body;
    b.angularMomentum=vscale(b.angularMomentum,roller_detail::kContactDamping);
    b.angularVelocity=matrixTimes(b.inverseInertia,b.angularMomentum);
    RollerQuad damped=vscale(b.velocity,roller_detail::kContactDamping);
    b.linearMomentum=vscale(damped,vuDivideOne(b.inverseMass));
    b.velocity=damped;
    RollerQuad lever=vsub(contact.point,b.position);
    RollerQuad velocity=vadd(b.velocity,cross(b.angularVelocity,lever));
    if(!(dot4(velocity,contact.normal)<0.f))return out;
    out.approaching=true;
    float speed=vuSqrt(dot4(b.velocity,b.velocity));out.speed=speed;
    float restitution=m.restitution;
    if(speed<m.restitutionSpeed)restitution=0.f;
    out.response=originalRollerContactResponse(b,contact.point,contact.normal,restitution,m.friction);
    m.contactTimer=originalScalarAdd(m.contactTimer,roller_detail::kContactStep);
    if(5.f<m.timer)m.timer=10.f;
    return out;
}

// ---- per-tick update 0x35E850 -----------------------------------------------
enum class OriginalRollerPath {Dynamics,Rest,Frozen,Sinking};
struct OriginalRollerUpdateResult {OriginalRollerPath path=OriginalRollerPath::Dynamics;OriginalRollerCollisionResult collision;};
inline OriginalRollerUpdateResult originalRollerUpdate(OriginalRollerModifier& m,const OriginalRollerWorldQuery& query){
    using namespace roller_math;using terrain_original::mul;OriginalRollerUpdateResult out;
    std::optional<OriginalRounding> rounding;rounding.emplace();
    m.timer=originalScalarAdd(m.timer,roller_detail::kTimerStep);
    if(11.f<=m.timer){
        if(!(16.f<=m.timer)){out.path=OriginalRollerPath::Frozen;return out;}
        out.path=OriginalRollerPath::Sinking;rounding.reset();
        originalRollerTranslate(m,{0,0,-10.f,0});
        originalRollerWorldTransform(m);
        originalRollerColliderTransform(m.collider,m.treeHeader,m.matrix,1.f);
        return out;
    }
    auto& b=m.body;
    float linear=dot4(b.velocity,b.linearMomentum),angular=dot4(b.angularVelocity,b.angularMomentum);
    float total=originalScalarAdd(linear,angular);
    float massScale=b.inverseMass==0.f?-1.f:originalScalarDivide(1.f,b.inverseMass);
    float blended=originalScalarDivide(mul(total,roller_detail::kEnergyBlend),massScale);
    m.energy=originalScalarAdd(mul(m.energy,roller_detail::kEnergyKeep),blended);
    if(m.energy<roller_detail::kRestEnergy||10.f<=m.timer){
        out.path=OriginalRollerPath::Rest;
        m.timer=11.f;
        b.linearMomentum={0,0,0,0};
        RollerQuad velocity=vscale({0,0,0,0},b.inverseMass);
        b.angularMomentum={0,0,0,0};
        b.angularVelocity=matrixTimes(b.inverseInertia,b.angularMomentum);
        b.velocity=velocity;
        return out;
    }
    m.contactTimer=mul(m.contactTimer,roller_detail::kContactDecay);
    float grip=0.f;{float scaled=mul(m.contactTimer,10.f);if(0.f<=scaled)grip=eeMin(scaled,2.f);}
    float damping=mul(grip,originalScalarAdd(originalScalarAdd(m.timer,m.timer),1.f));
    RollerQuad gravity{0,0,originalScalarDivide(-980.f,b.inverseMass),0};
    RollerQuad force=vsub(gravity,vscale(b.linearMomentum,damping));
    RollerQuad torque=vsub({0,0,0,0},vscale(b.angularMomentum,damping));
    rounding.reset();
    originalRollerIntegrate(b,force,torque,roller_detail::kIntegrateStep);
    originalRollerWorldInverseInertia(m);
    originalRollerVelocities(b);
    originalRollerWorldTransform(m);
    originalRollerColliderTransform(m.collider,m.treeHeader,m.matrix,1.f);
    out.collision=originalRollerCollision(m,query);
    originalRollerWorldTransform(m);
    return out;
}

// ---- construction 0x35DA70 ---------------------------------------------------
// 0x31B7A8: rotation part of a row-major world matrix -> quaternion (x,y,z,w).
inline RollerQuad originalRollerMatrixQuaternion(const RollerMatrix& m){
    using terrain_original::mul;OriginalRounding rounding;
    auto M=[&](unsigned r,unsigned c){return m[r][c];};
    float trace=originalScalarAdd(originalScalarAdd(M(0,0),M(1,1)),M(2,2));
    RollerQuad q;
    if(0.f<trace){
        float root=originalScalarSqrt(originalScalarAdd(trace,1.f));
        q[3]=mul(root,.5f);float f=originalScalarDivide(.5f,root);
        q[0]=mul(originalScalarSubtract(M(1,2),M(2,1)),f);
        q[1]=mul(originalScalarSubtract(M(2,0),M(0,2)),f);
        q[2]=mul(originalScalarSubtract(M(0,1),M(1,0)),f);
        return q;
    }
    unsigned i=0;if(M(0,0)<M(1,1))i=1;if(M(i,i)<M(2,2))i=2;
    unsigned j=(i+1)%3,k=(j+1)%3;
    float root=originalScalarSqrt(originalScalarAdd(originalScalarSubtract(originalScalarSubtract(M(i,i),M(j,j)),M(k,k)),1.f));
    q[i]=mul(root,.5f);
    float f=root==0.f?root:originalScalarDivide(.5f,root);
    q[3]=mul(originalScalarSubtract(M(j,k),M(k,j)),f);
    q[j]=mul(originalScalarAdd(M(i,j),M(j,i)),f);
    q[k]=mul(originalScalarAdd(M(i,k),M(k,i)),f);
    return q;
}
// Instance flag edits of 0x35DA70 (instance+8): static->entity route, bit1->bit2, bit0.
inline uint32_t originalRollerInstanceFlags(uint32_t flags){
    flags=(flags&~0x20u)|0x40u;flags=(flags&~2u)|4u;return flags|1u;
}
// m holds the allocation's previous contents (fields the constructor never writes
// keep them); returns the edited instance flags. The radius (+0x24) is set later
// by the script (originalRollerSetRadius).
inline uint32_t originalRollerConstruct(OriginalRollerModifier& m,const OriginalRollerScriptArgs& args,
        const OriginalRollerSource& source,uint32_t instanceFlags,const OriginalRollerContactRecord& record){
    using namespace roller_math;using terrain_original::mul;
    // 0x32C508/0x32C540: table corners, centre (0,0,0,1) (0x4FF130), scale 1, +0x98 -> +0xC0.
    m.collider.scale=1.f;m.timer=0;m.instance=source.instance;
    // 0x327CC8: corners, centre and the 0x80-byte header from the hit node's collider.
    m.collider.corners=source.corners;m.collider.center=source.center;m.treeHeader=source.treeHeader;
    m.collider.tree=source.tree;m.collider.treeResource=source.treeResource;
    m.mass=args.mass;m.scriptParameter=args.parameter;
    {
        OriginalRounding rounding;
        m.body.inverseMass=originalScalarDivide(1.f,m.mass);
        m.centerOfMass={roller_detail::bits(m.treeHeader[11]),roller_detail::bits(m.treeHeader[12]),roller_detail::bits(m.treeHeader[13]),0.f};
        for(unsigned i=0;i<9;++i)m.bodyInertia[i]=mul(roller_detail::bits(m.treeHeader[23+i]),.5f);
    }
    RollerQuad translation=source.instanceMatrix[3];
    RollerQuad q=originalRollerMatrixQuaternion(source.instanceMatrix);  // 0x31B748
    {
        OriginalRounding rounding;
        RollerQuad once=rotate(q,m.centerOfMass),twice=rotate(q,once);
        m.body.position=vadd(translation,twice);m.body.orientation=q;
    }
    originalRollerWorldTransform(m);           // 0x35E770 + matrix block
    originalRollerWorldInverseInertia(m);      // 0x35E248
    originalRollerVelocities(m.body);          // 0x35D288
    m.restitution=roller_detail::kRestitution;m.friction=roller_detail::kFriction;m.restitutionSpeed=roller_detail::kRestitutionSpeed;
    m.word10=0;
    uint32_t flags=originalRollerInstanceFlags(instanceFlags);
    originalRollerKick(m,record);              // 0x35DDE8
    return flags;
}

// Structured construction for callers without the raw 0x80-byte header: the mass
// properties of the kind-12 sphere-tree model (runtime tree +0x2C centre of mass,
// +0x38 inertia, +0x5C inverse inertia). Only header words the dynamics read are
// meaningful: [2] compressed, [3] depth, [5..7] centre, [11..13] centre of mass,
// [14..22]/[23..31] inertia/inverse inertia; the rest (ids, pointers) are 0.
struct OriginalRollerTreeMass {std::array<float,3> centerOfMass{};std::array<float,9> inertia{},inverseInertia{};};
inline std::array<uint32_t,32> originalRollerTreeHeader(const CollisionSphereTree& tree,const OriginalRollerTreeMass& mass){
    if(tree.levels.empty())throw std::runtime_error("Original roller sphere tree has no levels");
    std::array<uint32_t,32> h{};auto w=[](float f){return std::bit_cast<uint32_t>(f);};
    h[2]=tree.compressed?1u:0u;h[3]=uint32_t(tree.levels.size()-1);
    for(unsigned k=0;k<3;++k){h[5+k]=w(tree.centerCm[k]);h[11+k]=w(mass.centerOfMass[k]);}
    for(unsigned i=0;i<9;++i){h[14+i]=w(mass.inertia[i]);h[23+i]=w(mass.inverseInertia[i]);}
    return h;
}
// 0x35DA70 from structured inputs: instanceMatrix = instance+0x10 rows,
// hitCollider = packet+0x48 (0x32C508 object: corners, centre, tree), record =
// rider+0xA60, instanceFlags (instance+8) edited in place. Allocation words the
// constructor never writes are zero here; radius stays 0 until originalRollerSetRadius.
inline OriginalRollerModifier originalRollerConstruct(const RollerMatrix& instanceMatrix,const OriginalSphereTreeCollider& hitCollider,
        const OriginalRollerTreeMass& mass,const OriginalRollerContactRecord& record,const OriginalRollerScriptArgs& args,
        uint32_t& instanceFlags,uint32_t instance=0){
    if(!hitCollider.tree)throw std::runtime_error("Original roller hit collider has no sphere tree");
    OriginalRollerSource source;source.corners=hitCollider.corners;source.center=hitCollider.center;
    source.treeHeader=originalRollerTreeHeader(*hitCollider.tree,mass);source.tree=hitCollider.tree;source.treeResource=hitCollider.treeResource;
    source.instanceMatrix=instanceMatrix;source.instance=instance;
    OriginalRollerModifier m;instanceFlags=originalRollerConstruct(m,args,source,instanceFlags,record);
    return m;
}

// ---- attach 0x3554B0(entity, modifier) (builtin15 0x355DB8 after 0x35DA70) ---
// entity vtable+0x188 = 0x3567E0 first restores a previous modifier's bounds
// (none for a crashbag's first contact); the container gets the modifier; then
// entity vtable+0x180 = 0x356780 -> 0x350570(entity, &bounds, &radius):
//   bounds = instance+0x60..+0x68 / +0x6C..+0x74 authored AABB, w = 1 in both,
//   radius = sqrt.s(max over the 8 AABB corners (w=1) of dot4(corner - instance+0x40)),
// stored through modifier vtable+0x6C (0x361D00, +0x220/+0x230) and +0x78
// (0x361D28, +0x24). Finally instance flags &= ~0x20, |= 0x40.
inline uint32_t originalRollerAttach(OriginalRollerModifier& m,const RollerQuad& instanceTranslation,
        const std::array<float,3>& authoredLow,const std::array<float,3>& authoredHigh,uint32_t instanceFlags){
    using namespace roller_math;OriginalRounding rounding;
    RollerQuad low{authoredLow[0],authoredLow[1],authoredLow[2],1.f},high{authoredHigh[0],authoredHigh[1],authoredHigh[2],1.f};
    float largest=0.f;
    for(unsigned corner=0;corner<8;++corner){
        RollerQuad point{corner&1?low[0]:high[0],corner&2?low[1]:high[1],corner&4?low[2]:high[2],1.f};
        RollerQuad delta=vsub(point,instanceTranslation);
        float squared=dot4(delta,delta);
        if(largest<squared)largest=squared;
    }
    m.boundsMin=low;m.boundsMax=high;
    m.radius=originalScalarSqrt(largest);
    return (instanceFlags&~0x20u)|0x40u;
}

// ---- entity bounds 0x3568B0 (bounds part) ------------------------------------
struct OriginalRollerBounds {RollerQuad oldMin{},oldMax{},min{},max{};};
inline OriginalRollerBounds originalRollerEntityBounds(OriginalRollerModifier& m){
    using namespace roller_math;OriginalRounding rounding;OriginalRollerBounds out;
    out.oldMin=m.boundsMin;out.oldMax=m.boundsMax;             // 0x352B88 -> vtable+0x64 0x361D18
    RollerQuad extent{m.radius,m.radius,m.radius,0.f};         // 0x352BF8 -> vtable+0x74 0x361D20
    const RollerQuad& translation=m.matrix[3];                 // entity vtable+0xC4 0x356078 -> 0x361D30
    out.min=vsub(translation,extent);out.max=vadd(translation,extent);
    m.boundsMin=out.min;m.boundsMax=out.max;                   // 0x352BC0 -> vtable+0x6C 0x361D00
    return out;
}
inline const RollerMatrix& originalRollerMatrix(const OriginalRollerModifier& m){return m.matrix;}
inline const RollerQuad& originalRollerTranslation(const OriginalRollerModifier& m){return m.matrix[3];}

// ---- byte image (tests / live parity) ---------------------------------------
// Pointer words (+0x00 vtable, +0xD0 instance, +0x178 collider tree) are written
// as 0x48F080, `instance` and base+0x1A0.
inline std::array<uint8_t,0x2D0> originalRollerModifierBytes(const OriginalRollerModifier& m,uint32_t base=0){
    std::array<uint8_t,0x2D0> out{};
    auto put=[&](unsigned at,const auto& v){std::memcpy(out.data()+at,&v,sizeof(v));};
    put(0x00,uint32_t(0x48F080));put(0x04,m.energy);put(0x08,m.mass);put(0x0C,m.scriptParameter);put(0x10,m.word10);
    put(0x14,m.restitution);put(0x18,m.friction);put(0x1C,m.restitutionSpeed);put(0x20,m.timer);put(0x24,m.radius);
    put(0x28,m.contactTimer);put(0x2C,m.word2C);
    const auto& b=m.body;
    put(0x30,b.position);put(0x40,b.orientation);put(0x50,b.linearMomentum);put(0x60,b.angularMomentum);put(0x70,b.inverseMass);
    put(0x74,b.stale44);put(0x80,b.inverseInertia);put(0xA4,b.stale74);put(0xB0,b.velocity);put(0xC0,b.angularVelocity);
    put(0xD0,m.instance);put(0xD4,m.staleD4);
    const auto& c=m.collider;
    put(0xE0,c.corners);put(0x160,c.center);put(0x170,c.scale);put(0x174,c.depth);put(0x178,uint32_t(base+0x1A0));put(0x17C,c.order);
    put(0x19C,m.colliderWordBC);put(0x1A0,m.treeHeader);put(0x220,m.boundsMin);put(0x230,m.boundsMax);put(0x240,m.matrix);
    put(0x280,m.bodyInertia);put(0x2A4,m.stale2A4);put(0x2B0,m.centerOfMass);
    put(0x2C0,m.cache.patch);put(0x2C4,m.cache.cellU);put(0x2C6,m.cache.cellV);put(0x2C8,m.cache.half);put(0x2CC,m.cache.detailed);
    return out;
}
inline OriginalRollerModifier originalRollerModifierFromBytes(const uint8_t* in,const CollisionSphereTree* tree=nullptr,uint32_t treeResource=0){
    OriginalRollerModifier m;
    auto get=[&](unsigned at,auto& v){std::memcpy(&v,in+at,sizeof(v));};
    get(0x04,m.energy);get(0x08,m.mass);get(0x0C,m.scriptParameter);get(0x10,m.word10);
    get(0x14,m.restitution);get(0x18,m.friction);get(0x1C,m.restitutionSpeed);get(0x20,m.timer);get(0x24,m.radius);
    get(0x28,m.contactTimer);get(0x2C,m.word2C);
    auto& b=m.body;
    get(0x30,b.position);get(0x40,b.orientation);get(0x50,b.linearMomentum);get(0x60,b.angularMomentum);get(0x70,b.inverseMass);
    get(0x74,b.stale44);get(0x80,b.inverseInertia);get(0xA4,b.stale74);get(0xB0,b.velocity);get(0xC0,b.angularVelocity);
    get(0xD0,m.instance);get(0xD4,m.staleD4);
    auto& c=m.collider;
    get(0xE0,c.corners);get(0x160,c.center);get(0x170,c.scale);get(0x174,c.depth);get(0x17C,c.order);
    c.tree=tree;c.treeResource=treeResource;
    get(0x19C,m.colliderWordBC);get(0x1A0,m.treeHeader);get(0x220,m.boundsMin);get(0x230,m.boundsMax);get(0x240,m.matrix);
    get(0x280,m.bodyInertia);get(0x2A4,m.stale2A4);get(0x2B0,m.centerOfMass);
    get(0x2C0,m.cache.patch);get(0x2C4,m.cache.cellU);get(0x2C6,m.cache.cellV);get(0x2C8,m.cache.half);get(0x2CC,m.cache.detailed);
    return m;
}
}
