#pragma once
// Original Snow Jam log teeters: the AnimTeeter entity (ELF string "AnimTeeter" 0x489678,
// 0x80-byte object, entity vtable 0x4908F8 at +0x50) and the RailModifier (ELF string
// "RailModifier" 0x48E9B8, 0x90-byte object, vtable 0x4911D0 at +0x08) that binds an authored
// rail (SSB kind-8 record) to an animated model node. Created by the authored slot-1
// (section activation) programs 65..68 of handler rows 130..133:
//     builtin6 (0x2FB498 -> ctor 0x3421A0): AnimTeeter(gain 1000, restitution 0, max speed 120,
//              stiffness 0.5, rest 0; damping 0.6, flip mask 0, range = model animation range)
//     builtin48 (0x2FF1C8 -> 0x355E38 -> ctor 0x35B708): RailModifier(rail, node 1), linked into
//              the entity's ModifierBlock rail list (container+0x1C, 0x3556A8/0x35B670)
// (tools/export_rail_teeters.py asserts the program words; web/generated/rail_teeter_seed.hpp).
//
// What moves: nothing by itself. The teeter parameter +0x30 ("time" of the model's authored
// node animation, 0..1 for the logs = a rotation about node 1's local Y of 0..40.68 deg
// (logbreakteeter) / 0..62.15 deg (logteetera)) is a damped spring driven only by torques that
// the rider applies through entity vtable+0x15C (0x342538):
//     0x106848 rail attach (any controller/air): force = v_before - v_after (cm/s), the attach
//              velocity change (tangent projection, v.z *= 0.1), when the attached rail result
//              carries this instance (+0x50) and node (+0x5C);
//     0x106F78 rail snap in the 0x105398 instance-contact phase: force = (v_before - v_after) * fps;
//     0x137D18 wipeout sliding on the instance's mesh (only static-route instances; the
//              logbreakteeters have neither 0x20 nor 0x40 and are never body-collidable).
// Grinding (motion 4) applies no force; the log swings under the rider only after the attach
// impulse, and the rail (queried through 0x35C698) follows the node.
//
// Ported instruction by instruction (source Z-up centimeters, row vectors p' = p * M):
//   0x3421A0 (+0x34D9B0 base, 0x34E448 channels, 0x351508 start, 0x34E348)  originalAnimTeeterConstruct
//   0x342358 teeter update (entity vtable+0x7C via 0x356198)               originalAnimTeeterUpdate
//   0x342538 apply force (entity vtable+0x15C)                             originalAnimTeeterApplyForce
//   0x3610E0 node matrix (+0x34DC90 = 0x34E348 channels, 0x34DD18 compose) originalAnimNodeMatrix
//   0x351800 channel eval (0x351538 sampler, 0x351A80 segment search, 0x31BE50 sincos)
//   0x35B708 RailModifier ctor math (0x35C0E8 rest inverse, 0x35C040 bounds) originalRailModifierConstruct
//   0x35C4E0 per-tick bounds (0x35C040; + spatial relocation 0x3291E0)      originalRailModifierBounds
//   0x35C5A0 rail transform (0x34FED8 animated node path)                   originalRailModifierTransform
//   0x35C698 type-2 rail query (called by 0x334680 for layer type 2)        originalRailModifierQuery
//
// Per game tick (0x356198 entity update, the world entity pass before the rider like the
// crashbags): 0x342358 (consumes the torque accumulated during the previous tick's rider
// phase), then, dirty, 0x352D20 -> RailModifier 0x35C4E0 for every bound rail.
// Float policy as engine/roller_modifier.hpp: VU lanes chop (terrain_original under
// OriginalRounding), EE add.s/sub.s with the guard bit, EE mul.s chop, EE div.s nearest.
#include "roller_modifier.hpp"
#include "rail_motion.hpp"
#include "collision_scalar.hpp"
#include <optional>
#include <span>
#include <vector>

namespace ssx {

namespace rail_modifier_math {
inline RollerMatrix identity(){return {RollerQuad{1,0,0,0},RollerQuad{0,1,0,0},RollerQuad{0,0,1,0},RollerQuad{0,0,0,1}};}
// VU 4x4 product (vmulax/vmadday/vmaddaz/vmaddw with vf4..7 = b): row j = a_j * b.
inline RollerMatrix product(const RollerMatrix& a,const RollerMatrix& b){RollerMatrix o;for(unsigned j=0;j<4;++j)o[j]=roller_math::transform(b,a[j]);return o;}
// EE mul.s of a translation row by (s,s,s,1) (0x35C0E8 / 0x34DD18 / 0x34FED8).
inline RollerQuad scaleRow(const RollerQuad& t,float s){using terrain_original::mul;return {mul(t[0],s),mul(t[1],s),mul(t[2],s),mul(t[3],1.f)};}
inline RollerMatrix scaledLocal(RollerMatrix m,float s){m[3]=scaleRow(m[3],s);return m;}
}

// ---------------------------------------------------------------------------
// Model animation data (kind-2 model record: +0x08 node table, 16-byte nodes
// {parent, mesh, animation, bind matrix}; +0x14 animation length).
struct OriginalAnimSegment {float a=0,b=0,c=0,d=0,t0=0,t1=0;};   // value = ((a t + b) t + c) t + d on [t0,t1)
struct OriginalAnimTrack {                                        // node +0x08 data
    std::array<float,6> base{};                                  // +0x00 tx,ty,tz, rx,ry,rz (degrees)
    uint32_t mask=0;                                              // +0x18 animated components (bit k -> value k)
    std::vector<std::vector<OriginalAnimSegment>> curves;         // +0x20 one curve per set mask bit
};
struct OriginalAnimNode {int32_t parent=-1;RollerMatrix bind=rail_modifier_math::identity();std::optional<OriginalAnimTrack> track;};
struct OriginalAnimModel {std::vector<OriginalAnimNode> nodes;float length=0;};

// 0xD0-byte channel (vtable 0x490AD0 at +0x84), one per animated node in node order.
struct OriginalAnimChannel {
    std::array<int32_t,16> segment{};   // +0x00 cached segment index per active component
    const OriginalAnimTrack* track=nullptr; // +0x40
    std::array<float,16> value{};       // +0x44 sampled components
    RollerMatrix matrix=rail_modifier_math::identity(); // +0x90 local matrix
};

// builtin6 argument block +0x04..+0x24 (after the 0x445ED0 int->float conversion).
struct OriginalAnimTeeterArgs {
    float gain=.6f,restitution=0,maxSpeed=60,stiffness=.02f,rest=-10,damping=.6f;uint32_t flipMask=0;float minimum=-1,maximum=-1;
};

struct OriginalAnimTeeter {
    float torque=0;        // +0x00 accumulated by 0x342538, cleared by 0x342358
    float step=0;          // +0x04 last parameter step
    float gain=0;          // +0x08 args+4 * 1e-6
    float restitution=0;   // +0x0C args+8
    float minimum=0;       // +0x10
    float maximum=0;       // +0x14
    float maxStep=0;       // +0x18 args+0xC * (1/30) / fps
    float damping=0;       // +0x1C args+0x18
    float stiffness=0;     // +0x20 args+0x10
    float rest=0;          // +0x24 args+0x14 * (1/30)
    float velocity=0;      // +0x28
    uint32_t flipMask=0;   // +0x2C per-node lever sign flip
    float time=0;          // +0x30 teeter parameter (animation time)
    float sampleTime=0;    // +0x34 time the channels are sampled at
    float previous=0;      // +0x38 time before the last update (ctor 1e10)
    float unclamped=0;     // +0x3C time before the range clamp (ctor 1e10)
    int32_t evaluated=0;   // +0x40 set by 0x34E348
    bool dirty=true;       // entity+0x12 bit 0 (+0x56): node matrices stale
    const OriginalAnimModel* model=nullptr;
    RollerMatrix instanceMatrix=rail_modifier_math::identity(); // instance+0x10 (0x356078 base: no primary modifier)
    float instanceScale=1; // instance+0x84
    std::vector<OriginalAnimChannel> channels; // +0x78
    std::vector<RollerMatrix> matrices;        // +0x74 node world matrices (valid while !dirty)
};

namespace anim_original {
// 0x351A80: next cached segment, else the first segment with t < t1 (or the last).
inline size_t findSegment(const std::vector<OriginalAnimSegment>& curve,int32_t& index,float t){
    int32_t next=index+1;index=next;
    if(next<int32_t(curve.size())){const auto& s=curve[size_t(next)];if(s.t0<=t&&t<s.t1)return size_t(next);}
    index=0;size_t k=0;
    if(int32_t(curve.size())-1<=0)return 0;
    for(;;){
        if(t<curve[k].t1)return k;
        index+=1;++k;
        if(!(index<int32_t(curve.size())-1))return k;
    }
}
// 0x351538 sampler.
inline void sample(OriginalAnimChannel& c,float t){
    using terrain_original::mul;OriginalRounding rounding;
    size_t active=0;
    for(unsigned n=0;n<16;++n){
        if(!(c.track->mask>>n&1u))continue;
        const auto& curve=c.track->curves.at(active);int32_t& cache=c.segment[active];++active;
        size_t k=size_t(cache);
        bool inside=k<curve.size()&&curve[k].t0<=t&&t<curve[k].t1;
        if(!inside)k=findSegment(curve,cache,t);
        const auto& s=curve.at(k);
        float v=originalScalarAdd(mul(s.a,t),s.b);v=mul(v,t);v=originalScalarAdd(v,s.c);v=mul(v,t);
        c.value[n]=originalScalarAdd(v,s.d);
    }
}
// 0x351800: sample, then the Euler matrix of -(rx,ry,rz) degrees and translation (tx,ty,tz).
inline void evaluateChannel(OriginalAnimChannel& c,float t){
    using terrain_original::mul;
    sample(c,t);
    const float k=std::bit_cast<float>(0x3c8efa36u); // gp-0x2A90 pi/180
    float ax,ay,az;{OriginalRounding r;ax=mul(-c.value[3],k);ay=mul(-c.value[4],k);az=mul(-c.value[5],k);}
    auto [sx,cx]=collision_scalar::sincos(ax);auto [sy,cy]=collision_scalar::sincos(ay);auto [sz,cz]=collision_scalar::sincos(az);
    OriginalRounding rounding;
    float sxsy=mul(sx,sy),cxsy=mul(cx,sy),cxsz=mul(cx,sz),sxcz=mul(sx,cz),sxsysz=mul(sxsy,sz),cxcz=mul(cx,cz);
    float cxszsy=mul(cxsz,sy),sxsz=mul(sx,sz),cxsycz=mul(cxsy,cz),sxsycz=mul(sxsy,cz),cxcy=mul(cx,cy),msxcy=mul(-sx,cy);
    auto& m=c.matrix;
    m[0]={mul(cy,cz),mul(-cy,sz),sy,0.f};
    m[1]={originalScalarAdd(sxsycz,cxsz),originalScalarSubtract(cxcz,sxsysz),msxcy,0.f};
    m[2]={originalScalarSubtract(sxsz,cxsycz),originalScalarAdd(cxszsy,sxcz),cxcy,0.f};
    m[3]={c.value[0],c.value[1],c.value[2],1.f};
}
// 0x351508: first segment start of the first channel's first curve (0 without one).
inline float startTime(const std::vector<OriginalAnimChannel>& channels){
    if(channels.empty()||channels[0].track->curves.empty()||channels[0].track->curves[0].empty())return 0;
    return channels[0].track->curves[0][0].t0;
}
}

// 0x34E348: evaluate every channel at +0x34.
inline void originalAnimEvaluateChannels(OriginalAnimTeeter& e){
    for(auto& c:e.channels)anim_original::evaluateChannel(c,e.sampleTime);
    e.evaluated=1;
}
// 0x34DD18: node world = local (channel matrix or bind, translation * instance scale) * parent world
// (the instance matrix for roots).
inline void originalAnimComposeNodes(OriginalAnimTeeter& e){
    using namespace rail_modifier_math;OriginalRounding rounding;
    const auto& nodes=e.model->nodes;e.matrices.resize(nodes.size());size_t channel=0;
    for(size_t i=0;i<nodes.size();++i){
        const RollerMatrix& source=nodes[i].track?e.channels.at(channel++).matrix:nodes[i].bind;
        RollerMatrix local=scaledLocal(source,e.instanceScale);
        e.matrices[i]=product(local,nodes[i].parent>=0?e.matrices.at(size_t(nodes[i].parent)):e.instanceMatrix);
    }
}
// 0x3610E0 (entity vtable+0xEC): node world matrix, recomputed through 0x34DC90 when dirty.
inline const RollerMatrix& originalAnimNodeMatrix(OriginalAnimTeeter& e,int32_t node){
    if(e.dirty){originalAnimEvaluateChannels(e);originalAnimComposeNodes(e);e.dirty=false;}
    return e.matrices.at(size_t(node));
}

// builtin6 -> 0x3421A0(object, 1, instance, args) with 0x34D9B0 (base: entity ctor, channels 0x34E448,
// start 0x351508, 0x34E348). fps = world+0x10 (60).
inline OriginalAnimTeeter originalAnimTeeterConstruct(const OriginalAnimModel& model,const RollerMatrix& instanceMatrix,float instanceScale,
        const OriginalAnimTeeterArgs& args,int32_t fps=60){
    using terrain_original::mul;
    OriginalAnimTeeter e;e.model=&model;e.instanceMatrix=instanceMatrix;e.instanceScale=instanceScale;
    for(const auto& n:model.nodes)if(n.track){OriginalAnimChannel c;c.track=&*n.track;for(unsigned k=0;k<6;++k)c.value[k]=n.track->base[k];e.channels.push_back(c);}
    e.matrices.assign(model.nodes.size(),RollerMatrix{});
    float start=anim_original::startTime(e.channels);
    e.sampleTime=start;e.time=start;e.evaluated=1;
    e.previous=e.unclamped=std::bit_cast<float>(0x501502f9u); // gp-0x2AA8 1e10
    originalAnimEvaluateChannels(e);
    OriginalRounding rounding;
    const float third=std::bit_cast<float>(0x3d088889u); // gp-0x2B6C/-0x2B68/-0x2B60 1/30
    e.minimum=0<=args.minimum?mul(args.minimum,third):start;
    e.maximum=args.maximum<0?model.length:mul(args.maximum,third);
    e.restitution=args.restitution;
    e.gain=mul(args.gain,std::bit_cast<float>(0x358637bdu)); // gp-0x2B64 1e-6
    e.maxStep=collision_scalar::divide(mul(args.maxSpeed,third),float(fps));
    e.flipMask=args.flipMask;e.rest=mul(args.rest,third);e.damping=args.damping;e.dirty=true;e.stiffness=args.stiffness;
    e.torque=e.step=e.velocity=0;
    return e;
}

// 0x342358 (entity vtable+0x7C, from 0x356198): damped spring on +0x30, dt = world+0x14.
// Returns 0x34EBA0 (instance+0xC == this entity: always) after setting the dirty bit.
inline bool originalAnimTeeterUpdate(OriginalAnimTeeter& e,float dt){
    using terrain_original::mul;OriginalRounding rounding;
    e.previous=e.time;
    float drive=mul(e.gain,e.torque);if(e.gain<0)drive=-drive;
    float spring=mul(-e.stiffness,originalScalarSubtract(e.time,e.rest));
    float step=mul(e.velocity,dt);
    float damping=mul(-e.damping,e.velocity);
    e.step=step;
    float acceleration=originalScalarAdd(originalScalarAdd(spring,drive),damping);
    if(e.maxStep<std::abs(step))e.step=step<0?-e.maxStep:e.maxStep;
    float time=originalScalarAdd(e.time,e.step);
    float velocity=originalScalarAdd(e.velocity,mul(acceleration,dt));
    e.time=time;e.unclamped=time;e.velocity=velocity;
    const float threshold=std::bit_cast<float>(0x3dcccccdu); // gp-0x2B5C/-0x2B58 0.1
    if(time<e.minimum||e.maximum<time){
        e.time=time<e.minimum?e.minimum:e.maximum;
        if(threshold<std::abs(velocity))e.velocity=mul(velocity,e.restitution);
        else {e.step=0;e.velocity=0;}
    }
    e.torque=0;e.sampleTime=e.time;e.dirty=true;
    return true;
}

// 0x342538 (entity vtable+0x15C): torque += dot(node Y axis, (point - node origin) x force)
// for |force| > 100 on an animated node. point/node = the rider's rail result +0x00/+0x5C.
// Returns false when the call does nothing.
inline bool originalAnimTeeterApplyForce(OriginalAnimTeeter& e,int32_t node,const RollerQuad& point,const RollerQuad& force){
    using namespace roller_math;
    if(node<0)return false;
    if(!e.model->nodes.at(size_t(node)).track)return false;
    const RollerMatrix& m=originalAnimNodeMatrix(e,node);
    OriginalRounding rounding;
    float magnitude=vuSqrt(dot4(force,force));
    if(!(100.f<magnitude))return false;
    RollerQuad lever=vsub(point,m[3]);
    if(e.gain<0)lever=vscale(lever,-1.f);
    if(e.flipMask>>(uint32_t(node)&31u)&1u)lever=vscale(lever,-1.f);
    RollerQuad axis=transform(m,RollerQuad{0,1,0,0}); // 0x4FF150
    RollerQuad moment=cross(lever,force);
    e.torque=originalScalarAdd(e.torque,dot4(axis,moment));
    return true;
}

// ---------------------------------------------------------------------------
struct OriginalRailModifier {
    RollerQuad boundsMin{},boundsMax{}; // +0x10 / +0x20 spatial bounds (layer type 2 in the world tree)
    uint32_t packedId=0;               // +0x30 rail record (rid<<8 | 8)
    int32_t node=-1;                   // +0x34 model node
    uint32_t instance=0;               // +0x40 (the instance resource in the browser)
    RollerMatrix restInverse=rail_modifier_math::identity(); // +0x50 inverse of the node's rest world matrix
};
struct OriginalRailEntityBounds {RollerQuad min{},max{};}; // entity vtable+0x16C when vtable+0x164 != 0

// 0x35C040: entity bounds -/+ (100,100,100,0) when the entity reports them (0x3569D0 -> 0x352B88:
// only with a primary modifier; never for the teeters, whose container holds just the rails).
inline void originalRailModifierBounds(OriginalRailModifier& m,const std::optional<OriginalRailEntityBounds>& entity){
    if(!entity)return;
    OriginalRounding rounding;const RollerQuad margin{100,100,100,0};
    m.boundsMin=roller_math::vsub(entity->min,margin);m.boundsMax=roller_math::vadd(entity->max,margin);
}
// 0x35C0E8: rest inverse of bind(node) * bind(parent) ... * instance (bind translations scaled).
inline RollerMatrix originalRailModifierRestInverse(const OriginalAnimModel& model,int32_t node,const RollerMatrix& instanceMatrix,float instanceScale){
    using namespace rail_modifier_math;OriginalRounding rounding;
    if(node<0)throw std::runtime_error("RailModifier without a node (35C0E8 instance-matrix path) is not ported");
    RollerMatrix world=scaledLocal(model.nodes.at(size_t(node)).bind,instanceScale);
    for(int32_t p=model.nodes.at(size_t(node)).parent;p!=-1;p=model.nodes.at(size_t(p)).parent)
        world=product(world,scaledLocal(model.nodes.at(size_t(p)).bind,instanceScale));
    world=product(world,instanceMatrix);
    RollerQuad t=world[3];
    RollerMatrix inverse=world;
    std::swap(inverse[0][1],inverse[1][0]);std::swap(inverse[0][2],inverse[2][0]);std::swap(inverse[1][2],inverse[2][1]);
    RollerQuad v=roller_math::vscale(t,-1.f);v[3]=0;
    RollerMatrix rows=inverse;rows[3]=t; // row 3 is still the translation while +0x80 is multiplied (w = 0)
    RollerQuad r=roller_math::transform(rows,v);r[3]=1.f;
    inverse[3]=r;
    return inverse;
}
// 0x35C0E8 with node -1 (+0x34 < 0, e.g. the R&B boxcar rails of program 55): +0x50 = instance+0x10, then the same
// in-place inverse as above (3x3 transpose, row 3 = -t * R^T with w 0, then w 1). The transform 0x35C5A0 then takes
// 0x34FED8(instance, -1) = the entity matrix vt+0xC4 (a Spline piece: the Spline matrix).
inline RollerMatrix originalRailModifierRestInverseInstance(const RollerMatrix& instanceMatrix){
    OriginalRounding rounding;
    const RollerMatrix& world=instanceMatrix;RollerQuad t=world[3];
    RollerMatrix inverse=world;
    std::swap(inverse[0][1],inverse[1][0]);std::swap(inverse[0][2],inverse[2][0]);std::swap(inverse[1][2],inverse[2][1]);
    RollerQuad v=roller_math::vscale(t,-1.f);v[3]=0;
    RollerMatrix rows=inverse;rows[3]=t;
    RollerQuad r=roller_math::transform(rows,v);r[3]=1.f;
    inverse[3]=r;
    return inverse;
}
inline OriginalRailModifier originalRailModifierConstructInstance(uint32_t packedId,uint32_t instance,const RollerMatrix& instanceMatrix,
        const std::array<float,3>& instanceBoundsMin,const std::array<float,3>& instanceBoundsMax,const std::optional<OriginalRailEntityBounds>& entity){
    OriginalRailModifier m;m.packedId=packedId;m.node=-1;m.instance=instance;
    if(!entity){
        OriginalRounding rounding;const RollerQuad margin{100,100,100,0};
        m.boundsMin=roller_math::vsub({instanceBoundsMin[0],instanceBoundsMin[1],instanceBoundsMin[2],1},margin);
        m.boundsMax=roller_math::vadd({instanceBoundsMax[0],instanceBoundsMax[1],instanceBoundsMax[2],1},margin);
    }
    m.restInverse=originalRailModifierRestInverseInstance(instanceMatrix);
    originalRailModifierBounds(m,entity);
    return m;
}
// builtin48 -> 0x355E38 -> 0x35B708 (math part). instanceBounds = instance+0x60/+0x6C (w 1).
// The ctor also removes the rail's static segments from the world tree (0x3284B8) and inserts
// itself as a type-2 layer (0x328C20); the destructor 0x35BD70 reverses both.
inline OriginalRailModifier originalRailModifierConstruct(uint32_t packedId,int32_t node,uint32_t instance,const OriginalAnimModel& model,
        const RollerMatrix& instanceMatrix,float instanceScale,const std::array<float,3>& instanceBoundsMin,const std::array<float,3>& instanceBoundsMax,
        const std::optional<OriginalRailEntityBounds>& entity=std::nullopt){
    OriginalRailModifier m;m.packedId=packedId;m.node=node;m.instance=instance;
    if(!entity){
        OriginalRounding rounding;const RollerQuad margin{100,100,100,0};
        m.boundsMin=roller_math::vsub({instanceBoundsMin[0],instanceBoundsMin[1],instanceBoundsMin[2],1},margin);
        m.boundsMax=roller_math::vadd({instanceBoundsMax[0],instanceBoundsMax[1],instanceBoundsMax[2],1},margin);
    }
    m.restInverse=originalRailModifierRestInverse(model,node,instanceMatrix,instanceScale);
    originalRailModifierBounds(m,entity);
    return m;
}
// 0x35C5A0: T = restInverse * current node world (0x34FED8 -> entity vtable+0xEC).
inline RollerMatrix originalRailModifierTransform(const OriginalRailModifier& m,const RollerMatrix& nodeWorld){
    OriginalRounding rounding;return rail_modifier_math::product(m.restInverse,nodeWorld);
}
inline RollerMatrix originalRailModifierTransform(const OriginalRailModifier& m,OriginalAnimTeeter& e){
    const RollerMatrix& world=originalAnimNodeMatrix(e,m.node);return originalRailModifierTransform(m,world);
}

// 0x35C698 result (+0x00 point, +0x10 tangent, +0x20/+0x30 zero, +0x44 kind 2, +0x4C surface,
// +0x50 instance, +0x54 0, +0x58 record, +0x5C node, +0x60/+0x64 -1, +0x68 t, +0x6C/+0x70 0).
struct OriginalRailModifierHit {
    RollerQuad point{},tangent{};float t=0;int32_t surface=-1;int32_t node=-1;uint32_t instance=0;
    const OriginalRailRecord* record=nullptr;size_t segment=0;
};
// Segment rows with their authored w lanes (0,0,0,1).
inline RollerMatrix originalRailSegmentRows(const OriginalRailSegment& s){
    RollerMatrix r;for(unsigned j=0;j<4;++j)r[j]={s.coefficients[j][0],s.coefficients[j][1],s.coefficients[j][2],j==3?1.f:0.f};return r;
}
// 0x35C698(object, box, box, point, mask, &found, &bestDistance, out): every segment of the rail,
// transformed by T (no bounds rejection), five-point chord search, golden section (at most 23
// steps), strictly-closer replacement. Distances are 4-lane (point w matters: pass the caller's).
inline void originalRailModifierQuery(const OriginalRailModifier& m,const OriginalRailRecord& record,std::span<const RollerMatrix> segmentRows,
        const RollerMatrix& transformMatrix,const RollerQuad& q,uint32_t mask,bool& found,float& bestDistance,OriginalRailModifierHit& out){
    using namespace roller_math;
    if(!(record.flags&mask))return;
    OriginalRounding rounding;
    const float c1=std::bit_cast<float>(0x3ec3910cu),c2=std::bit_cast<float>(0x3f1e377au),tolerance=std::bit_cast<float>(0x3a03126fu);
    static constexpr float lowTable[5]={0,0,0,.25f,.25f},highTable[5]={.75f,.75f,1,1,1}; // 0x48E7D8 / 0x48E7F0
    auto distance=[&](const RollerQuad& a,const RollerQuad& b){RollerQuad d=vsub(a,b);return vuSqrt(dot4(d,d));};
    for(size_t si=0;si<segmentRows.size();++si){
        RollerMatrix mt=rail_modifier_math::product(segmentRows[si],transformMatrix);
        RollerQuad previous=transform(mt,{0,0,0,1});
        float best=distance(q,previous);int index=0;
        for(int i=1;i<5;++i){
            float t=terrain_original::mul(float(i),.25f),t2=terrain_original::mul(t,t),t3=terrain_original::mul(t,t2);
            RollerQuad current=transform(mt,{t3,t2,t,1});
            RollerQuad segment=vsub(current,previous),d=vsub(q,previous);
            float fraction=collision_scalar::divide(dot4(d,segment),dot4(segment,segment));
            float clamped=0;if(0<=fraction)clamped=eeMin(fraction,1.f);
            RollerQuad closest=vadd(previous,vscale(segment,clamped));
            float dist=distance(q,closest);
            if(dist<best){best=dist;index=i;}
            previous=current;
        }
        using terrain_original::mul;
        float low=lowTable[index],high=highTable[index];
        auto evaluate=[&](float t,float t2,float t3){return transform(mt,{t3,t2,t,1});};
        float span=originalScalarSubtract(high,low);
        float high2=mul(high,high),low2=mul(low,low),high3=mul(high,high2),a=mul(span,c1),low3=mul(low,low2),b=mul(span,c2);
        float x1=originalScalarAdd(low,a);
        RollerQuad last=evaluate(low,low2,low3);
        float x2=originalScalarAdd(low,b);
        float x1s=mul(x1,x1),x2s=mul(x2,x2),x1c=mul(x1,x1s);
        float chosen=x2,x2c=mul(x2,x2s);
        last=evaluate(x1,x1s,x1c);float d1=distance(last,q);
        last=evaluate(x2,x2s,x2c);float d2=distance(last,q);
        float chosenDistance=d2;
        last=evaluate(high,high2,high3);
        for(int counter=1;counter<24;++counter){
            if(std::abs(originalScalarSubtract(high,low))<=tolerance)break;
            if(d2<d1){
                low=x1;float h=mul(high,c1);x1=x2;float g=mul(x1,c2);d1=d2;x2=originalScalarAdd(g,h);
                float s=mul(x2,x2);chosen=x2;last=evaluate(x2,s,mul(x2,s));d2=distance(last,q);chosenDistance=d2;
            }else{
                high=x2;float h=mul(low,c1);x2=x1;float g=mul(x2,c2);d2=d1;x1=originalScalarAdd(g,h);
                float s=mul(x1,x1);chosen=x1;last=evaluate(x1,s,mul(x1,s));d1=distance(last,q);chosenDistance=d1;
            }
        }
        if(found&&!(chosenDistance<bestDistance))continue;
        found=true;bestDistance=chosenDistance;
        float two=originalScalarAdd(chosen,chosen),three=mul(chosen,mul(chosen,3.f));
        RollerQuad derivative=transform(mt,{three,two,1,0});
        out.point=last;out.tangent=vscale(derivative,vuRsqrt(dot4(derivative,derivative)));out.t=chosen;
        out.surface=record.surface;out.instance=m.instance;out.node=m.node;out.record=&record;out.segment=si;
    }
}
inline void originalRailModifierQuery(const OriginalRailModifier& m,const OriginalRailRecord& record,const RollerMatrix& transformMatrix,
        const RollerQuad& q,uint32_t mask,bool& found,float& bestDistance,OriginalRailModifierHit& out){
    std::vector<RollerMatrix> rows;for(const auto& s:record.segments)rows.push_back(originalRailSegmentRows(s));
    originalRailModifierQuery(m,record,rows,transformMatrix,q,mask,found,bestDistance,out);
}
// Merge into the rail query result the rider code consumes (0x334680 out), after the static layers.
inline void originalRailModifierApply(const OriginalRailModifierHit& hit,float bestDistance,OriginalRailQueryResult& result){
    result.found=true;result.distance=bestDistance;result.point={hit.point[0],hit.point[1],hit.point[2]};
    result.tangent={hit.tangent[0],hit.tangent[1],hit.tangent[2]};result.t=hit.t;result.surface=hit.surface;
    result.record=hit.record;result.segment=hit.record?&hit.record->segments.at(hit.segment):nullptr;
}
}
