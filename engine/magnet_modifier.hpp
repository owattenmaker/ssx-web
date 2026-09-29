#pragma once
// Original SSX3 (PS2 USA, SLUS_207.72) MagnetModifier (ELF name 0x48E918,
// vtable 0x48F420, 0xB0 bytes): the point pickup that flies to the rider.
//
// Script entry: builtin90 (0x305478, builtin table 0x441F38 entry 90). Keyed
// arguments against the defaults block 0x4FBD58 (written on first use, gp+0x2750)
// and the field types at 0x4466A0 (1 int, 2 float, 2 float):
//   key 0 instance (-1 = the running script's instance, context gp+0xCE8 +0x290)
//   key 1 radius   (default 400.0) -> +0x40: entity bounds half extent = contact box
//   key 2 speed    (default 30.0 km/h) -> +0x48 = key2 * 27.777779 cm/s (gp-0x2A84)
// An argument whose type differs from the field type is stored raw, except that a
// non-float argument for a float field is converted with cvt.s.w.
// The magnet is created only if instance+0xC holds an entity whose vt+0x84 is
// nonzero (Object 0x490E80: 0x360DD0 = 1); the entity matrix vt+0xC4 (0x356078:
// modifier vt+0x94, or instance+0x10 without a main modifier) is copied into it.
// Factory 0x3559F8: allocate 0xB0, ctor 0x3572E0, attach 0x3554B0.
//
// The Junction (BHP1): the 14 mdl_BHP1_pointa_*/pointc_* pickups, stage programs 8/10
// in handler slot 1 (section load): builtin99, builtin29, builtin0 (Object entity),
// builtin21 (UVScroll 0.05), builtin97 (HaloModifier 0x491220 through 0x355EB0/0x346120
// into the container +0x10 list: a glow sprite, key7 460 = size, key8 9 = spin in
// degrees per tick; draw only, it does not touch the magnet or the contact), then
// builtin90(key1 400, key2 10): radius 400 cm, speed 277.777771 cm/s. Slot 2
// (program 9) awards 2000 points (builtin27 type 6), builtin69(1) deletes the type-3
// container entries (the halo, 0x353278), builtin1 (Debounce 0x342C08, see below).
//
// Ported instruction by instruction (source Z-up centimetres, seconds):
//   0x3572E0 ctor                       originalMagnetConstruct
//   0x3554B0 -> 0x356780 -> 0x350570    originalMagnetAttach (bounds = authored AABB, w 1.0;
//                                       the radius setter vt+0x7C 0x361A50 is empty, so
//                                       +0x40 keeps key1; flags &~0x20 | 0x40)
//   0x3573F8 update (vt+0x14)           originalMagnetUpdate
//   0x357528 evaluate (vt+0x1C)         originalMagnetEvaluate (+0x90 = +0x10, clean)
//   0x361A60 matrix (vt+0x94)           originalMagnetMatrix (lazy)
//   0x3568B0 entity bounds part         originalMagnetEntityBounds (radius vt+0x74 0x361A48 = +0x40;
//                                       0x3291E0 spatial relocation out of scope)
//   0x357660 contact gate (vt+0x4C)     originalMagnetGate
//   0x357538 collision (vt+0xAC)        originalMagnetAnswersQuery / originalMagnetContactPacket
//   0x355F10 Debounce conversion        originalMagnetFreeze (PositionModifier at the magnet matrix)
// Other slots: vt+0x54 (entity contact 0x355770 "refresh") 0x360B70 empty, vt+0x34
// finished 0x360B50 = 0, vt+0x44 0x360B60 = 1 (rigid), vt+0x8C 0x361A10 = dirty != 0,
// vt+0x9C 0x360BC0 = 0 (no base-matrix override: the hierarchy root is vt+0x94),
// vt+0xA4 0x361A20 = 1 (334888 override: the magnet answers collision itself),
// vt+0xB4 0x360BD8 empty (no surface velocity), vt+0xC4 0x360BE8 empty, vt+0xD4
// 0x361A58 = 6 (type), vt+0xDC 0x3576D0 / ctor 0x357358 = save/load of +0x10..+0x5F
// and +0x60..+0x9F.
//
// Modifier layout (0xB0): +0x10 position (quad, w 1 from the matrix), +0x20/+0x30
// bounds, +0x40 radius, +0x44 target rider index (-1 none), +0x48 speed (cm/s), +0x4C
// time since acquisition (s), +0x50 dirty, +0x54 reached, +0x60..+0x9F matrix rows
// (row-vector, row 3 at +0x90 = translation), +0xA0 instance pointer.
// +0x04..+0x0C, +0x58/+0x5C, +0xA4..+0xAC are never written.
//
// Behaviour. Nothing moves until a HUMAN rider touches the contact box. Contacts
// come from the instance-contact phase (105398 -> 104E70 -> 334458 with filter 1 ->
// 334888): for an entity-route instance 334888 asks entity vt+0x134 (0x356A28 ->
// modifier vt+0xA4 = 1) and hands the query to vt+0x13C (0x356A70 -> 0x357538), which
// only answers filter 1 (t0 & 1) and runs the body query's box test (query vtable
// 0x48E590 +0x3C = 0x32FAC0 -> 0x32B2B8, `originalBodyBoxContact`) against the
// modifier bounds, i.e. position -/+ (radius, radius, radius): a 800 cm cube around
// the pickup, not the pickup's own trigger node. The selected contact runs 121818 ->
// entity vt+0x144 0x355770: modifier vt+0x54 (empty), then the gate
// vt+0x4C(modifier, rider index 0x2D1B30 -> rider+0x86C):
//   target < 0: rider interface 0x2D1B08 (rider+0x6C0), vt+0x44 = 0x140BC0 reads
//               rider+0x874 (nonzero = human); a human becomes the target. Returns 0.
//   target == index: returns +0x54 (reached).  Any other rider: 0.
// Only a nonzero gate with entity+0x20 <= 0 runs the slot-2 program (entity+0x20 =
// clock rate / 2 = 30 ticks, 34FE00 -> 30A060). So the first contact only acquires;
// the award comes with the first contact after the magnet reached the rider.
// Every game tick (entity pass, before the rider): 0x356198 runs the container
// (0x352C70: modifier vt+0x14 first, then UVScroll, TexFlip, the +0x10 list), then
// 0x3568B0 rebuilds the bounds from the (lazily committed) matrix. The update:
//   time += dt (clock +0x14 via 0x2D1C70, 1/60)
//   d = P(target) - position   (P = 0x2D1B58: rider interface vt+0x2C 0x1408F0 = rider+0x110)
//   |d| < 50: reached = 1 (no move); else step = d * (speed / |d| * time), and if
//   |d| < |step| the step is d (reached = 1); position += step; dirty = 1.
// The step grows with the accumulated time (speed * time per tick, i.e. an
// acceleration of speed/dt per second): with speed 277.78 the pickup covers
// 4.63 * n(n+1)/2 cm in n ticks. Once reached it keeps snapping to the rider.
// After the award, builtin1 (Debounce, 0x2FC7D0 -> 0x342C08, 0x34 bytes, vtable
// 0x4906F0) replaces the Object entity through 0x355F10: the halo list moves over, the
// magnet matrix (committed) becomes a PositionModifier (0x355918) and the Object entity
// with its container (and the MagnetModifier) is destroyed; key3 = 0 clears the
// instance flags' low four bits (no draw, no collision route). On expiry (key1 default
// 1 s) the entity becomes the empty type-19 node 0x491680 (instance flags 0x210105: no
// 0x20/0x40 route, not drawn). A collected pickup is never answered again in the run
// (PS2 setpieces-bhp1 full: pointa_1002 / pointa_1000 collected before 1219 / 2418 stay
// in that state through 4420; no savestate magnet is ever re-created).
//
// Float policy (as engine/roller_modifier.hpp / spline_modifier.hpp): VU lanes
// terrain_original add/sub/mul with chop, ACC chains in instruction order including
// the w lane, VSQRT sqrt(max(0,x)), VDIV x!=0?1/x:0; EE add.s originalScalarAdd, mul.s
// terrain_original::mul, div.s originalScalarDivide (nearest). -ffp-contract=off.
#include "spline_modifier.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>

namespace ssx {

using MagnetQuad=std::array<float,4>;

namespace magnet_modifier_constants {
inline float constant(uint32_t bits){return std::bit_cast<float>(bits);}
inline const float kKmhToCms=constant(0x41de38e4u);   // gp-0x2A84 27.777779
inline const float kReachDistance=50.f;               // 0x3573F8 lui 0x4248
inline const float kClockStep=constant(0x3c888889u);  // clock +0x14 (0x2D1C70), 1/60
inline const float kDefaultRadius=400.f;              // 0x305478 lui 0x43C8
inline const float kDefaultSpeedKmh=30.f;             // 0x305478 lui 0x41F0
inline constexpr uint32_t kVtable=0x48F420u;
inline constexpr uint32_t kType=6u;                   // vt+0xD4 0x361A58
}

// builtin90 argument block (sp+0x10 of 0x305478).
struct OriginalMagnetScriptArgs {
    int32_t instance=-1;                                          // key 0
    float radius=magnet_modifier_constants::kDefaultRadius;       // key 1
    float speedKmh=magnet_modifier_constants::kDefaultSpeedKmh;   // key 2
};
// One stage-VM keyed argument (16 bytes on the VM stack: +0 key, +4 value word, +0xC type).
struct OriginalScriptKeyedArg {uint32_t key=0;uint32_t value=0;uint32_t type=0;};
// 0x305478 decode: field types {1,2,2} (0x4466A0). Keys > 2 only touch unused stack
// words in the original and are ignored here.
inline OriginalMagnetScriptArgs originalMagnetDecodeArgs(std::span<const OriginalScriptKeyedArg> args){
    std::array<uint32_t,3> field{0xFFFFFFFFu,std::bit_cast<uint32_t>(magnet_modifier_constants::kDefaultRadius),
                                 std::bit_cast<uint32_t>(magnet_modifier_constants::kDefaultSpeedKmh)};
    static constexpr uint32_t kTypes[3]={1,2,2};
    for(const auto& a:args){
        if(a.key>2)continue;
        if(a.type==kTypes[a.key]||kTypes[a.key]!=2)field[a.key]=a.value;
        else{OriginalRounding rounding;volatile int32_t v=int32_t(a.value);field[a.key]=std::bit_cast<uint32_t>(float(v));} /* cvt.s.w */
    }
    return {int32_t(field[0]),std::bit_cast<float>(field[1]),std::bit_cast<float>(field[2])};
}

struct OriginalMagnetModifier {
    // +0x00 vtable 0x48F420 (not modeled).
    std::array<uint32_t,3> word04{};   // +0x04..+0x0C never written (allocation contents)
    MagnetQuad position{};             // +0x10
    MagnetQuad boundsMin{},boundsMax{};// +0x20/+0x30 (attach: authored AABB; 0x3568B0: position -/+ radius)
    float radius=0;                    // +0x40 key1
    int32_t target=-1;                 // +0x44 rider index (rider+0x86C), -1 = none
    float speed=0;                     // +0x48 cm/s
    float time=0;                      // +0x4C seconds since the first update after acquisition
    uint32_t dirty=0;                  // +0x50
    uint32_t reached=0;                // +0x54
    std::array<uint32_t,2> word58{};   // +0x58/+0x5C never written
    RollerMatrix matrix{};             // +0x60..+0x9F (row 3 at +0x90)
    uint32_t instance=0;               // +0xA0 instance pointer (identity only)
    std::array<uint32_t,3> wordA4{};   // +0xA4..+0xAC never written
};

// 0x3572E0 (a1 = instance, a2 = entity matrix, f12 = radius, f13 = speed km/h).
// Unwritten words keep their previous contents.
inline void originalMagnetConstruct(OriginalMagnetModifier& m,const RollerMatrix& entityMatrix,float radius,float speedKmh,uint32_t instance){
    {OriginalRounding rounding;m.speed=terrain_original::mul(speedKmh,magnet_modifier_constants::kKmhToCms);}
    m.instance=instance;m.radius=radius;m.target=-1;m.time=0.f;m.reached=0;
    m.matrix=entityMatrix;m.dirty=1;m.position=entityMatrix[3];
}

// 0x3554B0 attach tail for the magnet: 0x356780 -> 0x350570 bounds = instance +0x60..+0x74
// (authored/current instance AABB) with w = 1.0, set through vt+0x6C; the radius
// (0x350570 corner distance) goes to vt+0x7C, which is empty for the magnet. Returns the
// new instance flags (entity route).
inline uint32_t originalMagnetAttach(OriginalMagnetModifier& m,const std::array<float,3>& instanceLow,const std::array<float,3>& instanceHigh,uint32_t instanceFlags){
    m.boundsMin={instanceLow[0],instanceLow[1],instanceLow[2],1.f};
    m.boundsMax={instanceHigh[0],instanceHigh[1],instanceHigh[2],1.f};
    return (instanceFlags&~0x20u)|0x40u;
}

// builtin90 end to end for an instance that has an Object entity (vt+0x84 != 0):
// construct from the entity matrix and attach.
inline OriginalMagnetModifier originalMagnetCreate(const OriginalMagnetScriptArgs& a,const RollerMatrix& entityMatrix,uint32_t instance,
        const std::array<float,3>& instanceLow,const std::array<float,3>& instanceHigh,uint32_t& instanceFlags){
    OriginalMagnetModifier m;
    originalMagnetConstruct(m,entityMatrix,a.radius,a.speedKmh,instance);
    instanceFlags=originalMagnetAttach(m,instanceLow,instanceHigh,instanceFlags);
    return m;
}

// 0x3573F8 (modifier vt+0x14). `targetPosition` = rider+0x110 of the target rider
// (all four lanes; w is 1.0 in every rider), read when the update runs (entity pass,
// before that tick's rider physics). Ignored while target < 0.
inline void originalMagnetUpdate(OriginalMagnetModifier& m,const MagnetQuad& targetPosition,float dt=magnet_modifier_constants::kClockStep){
    using namespace roller_math;using terrain_original::mul;
    if(m.target<0)return;
    OriginalRounding rounding;
    m.time=originalScalarAdd(m.time,dt);
    const MagnetQuad d=vsub(targetPosition,m.position);
    const float distance=vuSqrt(dot4(d,d));
    if(distance<magnet_modifier_constants::kReachDistance){m.reached=1;m.dirty=1;return;}
    const float scale=mul(originalScalarDivide(m.speed,distance),m.time);
    MagnetQuad step=vscale(d,scale);
    const float length=vuSqrt(dot4(step,step));
    if(distance<length){m.reached=1;step=d;}
    m.position=vadd(m.position,step);
    m.dirty=1;
}

// 0x357528 (vt+0x1C): +0x90 = +0x10 (all four lanes), clean.
inline void originalMagnetEvaluate(OriginalMagnetModifier& m){m.matrix[3]=m.position;m.dirty=0;}
// 0x361A60 (vt+0x94, reached through entity vt+0xC4 0x356078): render/collision/bounds matrix.
inline const RollerMatrix& originalMagnetMatrix(OriginalMagnetModifier& m){if(m.dirty)originalMagnetEvaluate(m);return m.matrix;}

// 0x3568B0 bounds part: matrix translation -/+ (radius, radius, radius, 0). Returns the
// old and new bounds (0x3291E0(world, 0, instance, new, old) relocation is the host's).
inline OriginalSplineBounds originalMagnetEntityBounds(OriginalMagnetModifier& m){
    const RollerMatrix& M=originalMagnetMatrix(m);
    using namespace roller_math;OriginalRounding rounding;OriginalSplineBounds out;
    out.oldMin=m.boundsMin;out.oldMax=m.boundsMax;
    const MagnetQuad extent{m.radius,m.radius,m.radius,0.f};
    out.min=vsub(M[3],extent);out.max=vadd(M[3],extent);
    m.boundsMin=out.min;m.boundsMax=out.max;
    return out;
}

// One entity-pass tick of a magnet (0x356198 container part + 0x3568B0).
inline OriginalSplineBounds originalMagnetTick(OriginalMagnetModifier& m,const MagnetQuad& targetPosition,float dt=magnet_modifier_constants::kClockStep){
    originalMagnetUpdate(m,targetPosition,dt);
    return originalMagnetEntityBounds(m);
}

// 0x357660 (vt+0x4C), called by entity contact 0x355770 with the contacting rider's
// index (0x2D1B30 -> rider+0x86C). `riderIsHuman` = rider+0x874 != 0 (0x140BC0); it is
// only read while there is no target. A nonzero result lets the slot-2 program run.
inline uint32_t originalMagnetGate(OriginalMagnetModifier& m,int32_t riderIndex,bool riderIsHuman){
    if(m.target<0){if(riderIsHuman)m.target=riderIndex;return 0;}
    return m.target==riderIndex?m.reached:0u;
}

// 0x357538 (vt+0xAC via 334888's override): answers only filter 1 (t0 & 1; the
// 104E70 instance-contact query of 105398), with the body query's box test against
// [boundsMin, boundsMax] (0x32FAC0 -> 0x32B2B8; engine/body_collision.hpp
// originalBodyBoxContact).
inline bool originalMagnetAnswersQuery(uint32_t filter){return (filter&1u)!=0;}
// Packet of one box hit (0x32FAC0 + 0x357538). `point` = 0x32B2B8 point (+0x00),
// `translation` = its push vector (+0x10, w lane as 0x32B2B8 wrote it): depth +0x40 =
// VSQRT(|t|^2), normal = t * VDIV(1/depth). The fixed words: +0x20/+0x30 = 0x4FF120 (zero),
// +0x44 = 1, +0x48 = 0 (no collider), +0x4C = -1 (surface), +0x50 = instance, +0x54/+0x58 = 0,
// +0x5C = 0 (node 0), +0x60/+0x64 = -1, +0x68/+0x6C/+0x70 = 0.
struct OriginalMagnetContactPacket {
    MagnetQuad point{},normal{},velocity{},angular{};
    float depth=0;
    uint32_t word44=1,word48=0;int32_t surface=-1;uint32_t instance=0;uint32_t word54=0,word58=0,node=0;
    int32_t word60=-1,word64=-1;uint32_t word68=0,word6C=0,word70=0;
};
inline OriginalMagnetContactPacket originalMagnetContactPacket(const OriginalMagnetModifier& m,const MagnetQuad& point,const MagnetQuad& translation){
    using namespace roller_math;OriginalRounding rounding;OriginalMagnetContactPacket p;
    p.point=point;p.depth=vuSqrt(dot4(translation,translation));
    p.normal=vscale(translation,vuDivideOne(p.depth));p.instance=m.instance;
    return p;
}

// 0x355F10 (Debounce / any entity conversion of a collected pickup): the old entity's
// matrix (committed if dirty) becomes a PositionModifier; the magnet is destroyed.
inline OriginalPositionModifier originalMagnetFreeze(OriginalMagnetModifier& m){
    OriginalPositionModifier p;originalPositionFromMatrix(p,originalMagnetMatrix(m));return p;
}

// ---- byte images (development oracles / diagnostics) ------------------------------
inline std::array<uint8_t,0xB0> originalMagnetModifierBytes(const OriginalMagnetModifier& m){
    std::array<uint8_t,0xB0> b{};
    auto put=[&](uint32_t o,const auto& v){std::memcpy(b.data()+o,&v,sizeof v);};
    put(0,magnet_modifier_constants::kVtable);put(4,m.word04);put(0x10,m.position);put(0x20,m.boundsMin);put(0x30,m.boundsMax);
    put(0x40,m.radius);put(0x44,m.target);put(0x48,m.speed);put(0x4C,m.time);put(0x50,m.dirty);put(0x54,m.reached);put(0x58,m.word58);
    put(0x60,m.matrix);put(0xA0,m.instance);put(0xA4,m.wordA4);
    return b;
}
inline OriginalMagnetModifier originalMagnetModifierFromBytes(const uint8_t* b){
    OriginalMagnetModifier m;
    auto get=[&](uint32_t o,auto& v){std::memcpy(&v,b+o,sizeof v);};
    get(4,m.word04);get(0x10,m.position);get(0x20,m.boundsMin);get(0x30,m.boundsMax);get(0x40,m.radius);get(0x44,m.target);
    get(0x48,m.speed);get(0x4C,m.time);get(0x50,m.dirty);get(0x54,m.reached);get(0x58,m.word58);get(0x60,m.matrix);get(0xA0,m.instance);get(0xA4,m.wordA4);
    return m;
}
}
