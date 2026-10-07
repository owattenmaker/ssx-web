#pragma once
// RollerModifier sphere-tree world query (collision step 0x35EDC8):
//   query = 0x3303F0(sp+0x80, collider = modifier+0xE0, 1)
//   depth = 0x336850(0x2D1BE0(), query, &packet, modifier+0x2C0 terrain cache)
//   0x3304E8(query, 2)
// Source Z-up centimeters. Every VU operation is an IEEE per-lane op with the
// current (chop) rounding (terrain_original add/sub/mul/div/sqrt); VU dot
// products include the w-lane product (vmul, vadday.x, vmaddaz.x, vmaddw.x).
// EE add.s/sub.s keep one guard bit, div.s/sqrt.s round to nearest. The Q unit
// and DIV.S by zero follow PCSX2 (+-FLT_MAX; VSQRT of |x|), not the recompiled
// lowering (0 / inf): see roller_world_query_detail::zeroExponent.
//
// Ported (instruction by instruction):
//   0x3303F0 query setup (+0 = 0 for a2=1: coarse terrain, +8 = 0: no normal
//            filter, +0x20/+0x30 bounds = centre +- 0x32C590 radius, fresh
//            0x32C508 scratch collider), 0x3304E8 (frees; no state effect)
//   0x336850 selection (0x3306D8 preferred depth -1, metric |depth+1|, ties:
//            instance beats terrain, lower instance+0x78, lower patch+0x150)
//   0x335960 patch (flags 0x41, strict bounds) + 0x32B6E0 COARSE (3x3 cells,
//            stride 3, cached cell first, lazily cached normals, cache write)
//   0x330788 / 0x330828 -> 0x32CA78 / 0x328030 -> 0x32DB40 (+0x32DA40 shell
//            sort), 0x32D470 recursion, 0x32D440 -> 0x32B6A8 (VU0 0x6E0),
//            0x32DF28 mask cache (WorldBodyCollision::sphereTreeMasks)
//   0x335B90 route/bounds, 0x334888 filter 2 (surface -1 nodes skipped) with
//            0x330710 (0x327CC8 + 0x32C770) local transform, 0x330540 node /
//            group bounds, 0x32C0F8 type-1 meshes; 0x330778 restore.
// Not ported (explicit std::runtime_error when reached): type-2 box nodes
// (0x330950 -> 0x32C928/0x32D028/0x329590) and type-3 tree-vs-tree nodes
// (0x3308C8 -> 0x32CB58/0x32CBF8/0x32CDB0) with surface != -1 (none exist in
// Snow Jam: its only real-surface nodes are the type-1 meshes of
// mdl_ARA1_loga_* and mdl_ARA1_snowcrumb_*), the entity vtable+0x134 full
// query override, and 0x340A08 refine (query+0 == 2 only; never here).
//
// Traversal order (0x335D78 root slots 0..7, then per partially overlapping
// node 0x33CCF8: patches, instances, children in Gray order 0,1,3,2,6,7,5,4;
// 0x340FA0's 0..7 order only runs for cells inside the box, which a roller box
// never contains). The original never enforces the 64-packet
// capacity on this path (0x335960 and 0x334888 ignore it; 0x335B90 passes the
// remaining count to 0x334888, which only forwards it to the entity override),
// so more than 64 contacts would overflow 0x336850's stack buffer: the port
// throws instead. Selection ties are broken by identities that are unique per
// patch/instance, so the only order-dependent results are:
//   * packets of one instance (one 0x334888 call, node/triangle order: kept);
//   * the terrain cell cache and the collider scratch (+0x94/+0x9C), written
//     only by terrain patches: they follow the 0x33CCF8 visit order, which is
//     NOT the octree list order of prepareTerrainTraversal (children 0..7) but
//     visits children in Gray order 0,1,3,2,6,7,5,4 (originalRollerSpatialBefore);
//   * the scratch collider scale (+0x90): 0x327CC8 does not copy it and
//     0x32C770 multiplies it by 1/instance-scale for every processed node, so
//     it is the product over all processed surface nodes of the query so far.
//     The port processes instances in WorldBodyCollision::instances order.
//     This equals the octree order unless two processed surface nodes of
//     different instances overlap one query and the earlier has scale != 1
//     (Snow Jam: loga_1001 has scale 0.99999994 and no other surface instance
//     within tens of metres).
// Culling by the octree cells is conservative (items are contained in their
// loose cells), so it removes nothing the per-item strict bounds tests keep.
// World data: the port sees only WorldBodyCollision, which now holds the whole race-event
// world (A_ARA1/ARA1/ARA1_B, tracks 3/8/9: tools/export_event_membership.py); the live
// test finds every octree patch natively and excludes no query.
// The 0x3279D0 grids equal terrain_original::coarseGrid bit for bit.
#include "roller_collider.hpp"
#include "world_body_collision.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace ssx {
namespace roller_world_query_detail {
using Quad=RollerQuad;
using terrain_original::Vector;
inline Quad qadd(Quad a,const Quad& b){for(unsigned k=0;k<4;++k)a[k]=terrain_original::add(a[k],b[k]);return a;}
inline Quad qsub(Quad a,const Quad& b){for(unsigned k=0;k<4;++k)a[k]=terrain_original::sub(a[k],b[k]);return a;}
inline Quad qscale(Quad a,float s){for(auto& x:a)x=terrain_original::mul(x,s);return a;}   // vmulx / vmulq
// vmul, vadday.x, vmaddaz.x, vmaddw.x: the z and w products go through the 1.0 vector as fs (vf6; e.g. 0x32DBA4).
inline float qdot(const Quad& a,const Quad& b){
    using namespace terrain_original;
    float sum=add(mul(a[0],b[0]),mul(a[1],b[1]));
    sum=add(sum,mul(1.f,mul(a[2],b[2])));
    return add(sum,mul(1.f,mul(a[3],b[3])));
}
// vmulax/vmadday/vmaddaz/vmaddw: lane k = ((m[k]x + m[4+k]y) + m[8+k]z) + m[12+k]w.
inline Quad qapply(const collision_transform::Matrix& m,const Quad& v){
    using namespace terrain_original;Quad r;
    for(unsigned k=0;k<4;++k)r[k]=add(add(add(mul(m[k],v[0]),mul(m[4+k],v[1])),mul(m[8+k],v[2])),mul(m[12+k],v[3]));
    return r;
}
// Q unit and DIV.S with PCSX2's zero handling (VUops.cpp _vuDIV/_vuSQRT/_vuRSQRT
// after vuDouble, FPU.cpp checkDivideByZero): an operand with a zero exponent
// gives +-FLT_MAX (sign = xor of the operand signs, fs = vf0w = +1), VSQRT takes
// |x|. The emulator-built savestate grids prove it: zero-area cells hold the
// 32B6E0 degenerate marker (1e11,0,0,0) that only VRSQRT(0) = FLT_MAX produces
// (the recompiled lowering returns 0 there). Nonzero operands are unchanged.
inline bool zeroExponent(float x){return (std::bit_cast<uint32_t>(x)&0x7f800000u)==0;}
inline float signedMax(uint32_t sign){return std::bit_cast<float>((sign&0x80000000u)|0x7f7fffffu);}
inline float vuSqrt(float x){return zeroExponent(x)?0.f:terrain_original::sqrt(std::fabs(x));}
inline float vuReciprocal(float x){return zeroExponent(x)?signedMax(std::bit_cast<uint32_t>(x)):terrain_original::div(1.f,x);}
inline float vuRsqrt(float x){return zeroExponent(x)?signedMax(std::bit_cast<uint32_t>(x)):terrain_original::div(1.f,terrain_original::sqrt(std::fabs(x)));}
inline float divS(float a,float b){
    if(zeroExponent(b))return signedMax(std::bit_cast<uint32_t>(a)^std::bit_cast<uint32_t>(b));
    return originalScalarDivide(a,b);
}
// cvt.w.s under chop rounding (saturating, NaN -> 0 as the recompiled cast).
inline int32_t cvtW(float x){
    if(!(x==x))return 0;
    if(x>=2147483648.f)return INT32_MAX;
    if(x<=-2147483648.f)return INT32_MIN;
    return int32_t(std::trunc(x));
}
inline Quad point4(const Vector& v){return {v[0],v[1],v[2],1.f};}
inline Quad normal4(const Vector& v){return {v[0],v[1],v[2],0.f};}
inline Vector xyz(const Quad& q){return {q[0],q[1],q[2]};}
// Shell-sort gap table at 0x44AF98.
inline constexpr int32_t shellGaps[]={1,4,13,40,121,364,1093,3280,9841,29524,88573,265720,797161,2147483647};
}

// 0x32DA40(n, keys, values): descending shell sort of keys, values follow.
inline void originalRollerShellSort(int32_t n,int32_t* keys,int32_t* values){
    using roller_world_query_detail::shellGaps;
    unsigned start=1;while(shellGaps[start]<n)++start;
    for(int gi=int(start)-1;;--gi) {
        int32_t gap=shellGaps[gi];
        if(gap<n)for(int32_t i=gap;i<n;++i) {
            int32_t value=values[i],key=keys[i],j=i-gap;
            while(j>=0&&keys[j]<key){keys[j+gap]=keys[j];values[j+gap]=values[j];j-=gap;}
            keys[j+gap]=key;values[j+gap]=value;
        }
        if(gap<2)break;
    }
}
// 0x32DB40(collider, normal): child visit order (+0x9C) by descending
// truncated projection of the corner vectors on normal*-1000.
inline void originalRollerChildOrder(OriginalSphereTreeCollider& collider,const RollerQuad& normal){
    using namespace roller_world_query_detail;
    Quad direction=qscale(normal,-1000.f);std::array<int32_t,8> keys{};
    for(int32_t i=0;i<4;++i) {
        int32_t key=cvtW(qdot(collider.corners[i],direction));
        collider.order[i]=i;collider.order[7-i]=7-i;keys[i]=key;keys[7-i]=int32_t(0u-uint32_t(key));
    }
    originalRollerShellSort(8,keys.data(),collider.order.data());
}
namespace roller_world_query_detail {
// 0x32D470 edge test (one of AB, AC, BC): true when the node sphere reaches the edge.
inline bool edgeReach(const Quad& toStart,float toStartSquared,const Quad& edge,float radiusSquared){
    using namespace terrain_original;
    float length=vuSqrt(qdot(edge,edge));
    float along=divS(qdot(toStart,edge),length);
    if(along<0.f)return false;
    float perpendicular=originalScalarSubtract(toStartSquared,mul(along,along));
    if(radiusSquared<perpendicular)return false;
    float near=originalScalarSubtract(along,originalScalarSqrt(originalScalarSubtract(radiusSquared,perpendicular)));
    return !(length<near);
}
struct TriangleQuery {
    OriginalSphereTreeCollider& collider;const std::vector<uint8_t>& masks;
    const Quad &a,&b,&c,&normal;Quad& translation;Quad& point;
};
// 0x32D470(collider, A, B, C, normal, node centre, level, node index).
inline void recurse(const TriangleQuery& q,const Quad& center,unsigned level,uint32_t index){
    using namespace terrain_original;
    const auto& tree=*q.collider.tree;
    if(level>=tree.levels.size())throw std::runtime_error("Original sphere-tree level outside tree");
    float distance=qdot(q.normal,qsub(center,q.a));
    float radius=mul(tree.levels[level].radiusCm,q.collider.scale);
    if(level==0&&distance<0.f)return;
    if(!(distance<originalScalarAdd(radius,q.collider.depth)))return;
    if(index>=q.masks.size())throw std::runtime_error("Original sphere-tree mask index outside tree");
    uint8_t mask=q.masks[index];
    const unsigned depthLevels=unsigned(tree.levels.size()-1); // tree+0xC
    bool leaf=!(level<depthLevels)||!(level<3)||mask==0;
    Quad projected=qsub(center,qscale(q.normal,distance));
    // 0x32D440 -> 0x32B6A8 (VU0 microprogram 0x6E0; tolerance gp-0x2D80 unused).
    bool inside=terrain_original::pointInTriangle(xyz(q.a),xyz(q.b),xyz(q.c),xyz(projected));
    if(!inside) {
        if(leaf)return;
        if(radius<20.f)return;
        float radiusSquared=mul(radius,radius);
        Quad fromA=qsub(center,q.a);float fromASquared=qdot(fromA,fromA);
        bool reach=!(radiusSquared<fromASquared);
        if(!reach)reach=edgeReach(fromA,fromASquared,qsub(q.b,q.a),radiusSquared);
        if(!reach)reach=edgeReach(fromA,fromASquared,qsub(q.c,q.a),radiusSquared);
        if(!reach){Quad fromB=qsub(center,q.b);reach=edgeReach(fromB,qdot(fromB,fromB),qsub(q.c,q.b),radiusSquared);}
        if(!reach)return;
    }
    if(leaf) {
        float depth=originalScalarSubtract(distance,radius);
        if(!(depth<q.collider.depth))return;
        q.translation=qscale(q.normal,-depth);q.point=projected;q.collider.depth=depth;
        return;
    }
    float offset=tree.levels[level+1].childOffsetCm;uint32_t stride=tree.levels[level].stride;
    for(unsigned k=0;k<8;++k) {
        int32_t child=q.collider.order[k];
        if(!((uint32_t(mask)>>(uint32_t(child)&31))&1))continue;
        if(child<0||child>7)throw std::runtime_error("Original sphere-tree child order corrupted");
        Quad next=qadd(center,qscale(q.collider.corners[child],offset));
        recurse(q,next,level+1,index+uint32_t(child+1)*stride);
    }
}
}

// 0x32DF28 for collider.tree: uncompressed trees (tree+8 == 0) keep their own
// stream and never touch the cache; compressed ones go through the shared
// pools of the world (keyed by resource only), or their own stream without one.
inline const std::vector<uint8_t>& originalRollerTreeMasks(const WorldBodyCollision* world,const OriginalSphereTreeCollider& collider){
    if(!collider.tree)throw std::runtime_error("Original roller collider has no sphere tree");
    if(!collider.tree->compressed||!world)return collider.tree->masks;
    return world->sphereTreeMasks(*collider.tree,collider.treeResource);
}
// 0x32CA78(collider, A, B, C, normal, &translation, &point): resolves the masks
// through 0x32DF28 on every call (as the original), writes +0x9C (order) and
// +0x94 (best signed depth; hit when < 0).
inline bool originalRollerSphereTreeTriangle(OriginalSphereTreeCollider& collider,const WorldBodyCollision* world,
        const RollerQuad& a,const RollerQuad& b,const RollerQuad& c,const RollerQuad& normal,RollerQuad& translation,RollerQuad& point){
    if(!collider.tree||collider.tree->levels.empty())throw std::runtime_error("Original roller collider has no sphere tree");
    const std::vector<uint8_t>& masks=originalRollerTreeMasks(world,collider);
    originalRollerChildOrder(collider,normal);
    collider.depth=0;
    roller_world_query_detail::recurse({collider,masks,a,b,c,normal,translation,point},collider.center,0,0);
    return collider.depth<0.f;
}
struct OriginalRollerTriangleHit {bool hit=false;RollerQuad point{},normal{};float depth=0;};
namespace roller_world_query_detail {
// 0x330788/0x330828 epilogue: depth = |translation| (VSQRT), normal = translation*VDIV.
inline OriginalRollerTriangleHit finishTriangle(bool hit,const Quad& translation,const Quad& point){
    OriginalRollerTriangleHit out;if(!hit)return out;
    out.hit=true;out.point=point;out.depth=vuSqrt(qdot(translation,translation));
    out.normal=qscale(translation,vuReciprocal(out.depth));return out;
}
}
// 0x330788 (query vtable+0x24): single-sided triangle callback.
inline OriginalRollerTriangleHit originalRollerTriangle(OriginalSphereTreeCollider& collider,const WorldBodyCollision* world,
        const RollerQuad& a,const RollerQuad& b,const RollerQuad& c,const RollerQuad& normal){
    RollerQuad translation{},point{};
    bool hit=originalRollerSphereTreeTriangle(collider,world,a,b,c,normal,translation,point);
    return roller_world_query_detail::finishTriangle(hit,translation,point);
}
// 0x330828 (query vtable+0x2C) -> 0x328030: double-sided triangle callback.
inline OriginalRollerTriangleHit originalRollerDoubleSidedTriangle(OriginalSphereTreeCollider& collider,const WorldBodyCollision* world,
        const RollerQuad& a,const RollerQuad& b,const RollerQuad& c,const RollerQuad& normal){
    using namespace roller_world_query_detail;
    if(!collider.tree||collider.tree->levels.empty())throw std::runtime_error("Original roller collider has no sphere tree");
    float distance=qdot(normal,qsub(collider.center,a));
    float radius=terrain_original::mul(collider.tree->levels[0].radiusCm,collider.scale); // 0x32C590
    if(radius<std::fabs(distance))return {};
    RollerQuad translation{},point{};bool hit;
    if(distance<0.f){Quad flipped=qscale(normal,-1.f);hit=originalRollerSphereTreeTriangle(collider,world,a,b,c,flipped,translation,point);}
    else hit=originalRollerSphereTreeTriangle(collider,world,a,b,c,normal,translation,point);
    return finishTriangle(hit,translation,point);
}

// One 334888/335960 contact packet (the fields the port models).
struct OriginalRollerWorldPacket {
    RollerQuad point{},normal{};   // +0x00, +0x10
    float depth=0;                 // +0x40
    bool terrain=false;            // +0x54 patch (terrain) vs +0x50 instance
    uint32_t resource=0;           // patch+0x150 / instance+0x78
    unsigned node=0;               // +0x5C instance model node (terrain: 0)
    int32_t triangle=-1;           // 0x32C0F8 packet +0x60 (terrain: -1)
    int surface=-1;                // +0x4C
    float u=0,v=0;                 // terrain +0x6C/+0x70 (cell centre u/v)
};

// 0x32B6E0 coarse mode (query+0 == 0) on one patch grid (10x10 points, w=1).
// Returns the contact of the cached cell or of the first hitting cell/half and
// updates the cache exactly as the original (cleared on a cached miss or a
// degenerate cached triangle, written on a scan hit, untouched on a cached hit).
namespace roller_world_query_detail {
// 0x32B6E0 lazily cached triangle normal: cross(p-q, q-r) * VRSQRT; Q > 10
// marks the triangle degenerate (x = 1e11 = gp-0x2DAC/-0x2DA0/-0x2D90).
inline Quad cellNormal(const Quad& p,const Quad& q,const Quad& r){
    using namespace terrain_original;
    Quad e=qsub(p,q),f=qsub(q,r);
    Quad n{sub(mul(e[1],f[2]),mul(f[1],e[2])),sub(mul(e[2],f[0]),mul(f[2],e[0])),sub(mul(e[0],f[1]),mul(f[0],e[1])),0.f};
    float reciprocal=vuRsqrt(qdot(n,n));
    Quad out=qscale(n,reciprocal);
    if(10.f<reciprocal)out[0]=99999997952.f;
    return out;
}
inline constexpr float degenerateNormal=99999997952.f;
}
struct OriginalRollerQueryBounds {RollerQuad high{},low{};}; // query +0x20 / +0x30
inline std::optional<OriginalRollerWorldPacket> originalRollerTerrainPatch(const WorldCollisionTerrain& patch,const OriginalRollerQueryBounds& bounds,
        OriginalSphereTreeCollider& collider,const WorldBodyCollision* world,OriginalRollerTerrainCache& cache){
    using namespace roller_world_query_detail;using terrain_original::mul;
    constexpr unsigned stride=3,cells=3;constexpr uint32_t detailed=0;
    constexpr float uvScale=0.3333333432674408f,uvOffset=0.1666666716337204f; // gp-0x2DC0/-0x2DBC
    auto P=[&](unsigned u,unsigned v){return point4(patch.grid[u*10+v]);};
    auto contact=[&](const OriginalRollerTriangleHit& hit,unsigned u,unsigned v) {
        OriginalRollerWorldPacket packet;packet.point=hit.point;packet.normal=hit.normal;packet.depth=hit.depth;
        packet.terrain=true;packet.resource=patch.resource;packet.surface=patch.surface;
        packet.u=originalScalarAdd(mul(float(u),uvScale),uvOffset);packet.v=originalScalarAdd(mul(float(v),uvScale),uvOffset);
        return packet;
    };
    if(cache.patch==patch.resource&&cache.detailed==detailed) {
        unsigned u=cache.cellU,v=cache.cellV;
        if(u>=cells||v>=cells)throw std::runtime_error("Original roller terrain cache cell outside the coarse grid");
        Quad a=P(u*stride,v*stride),b=P(u*stride+stride,v*stride),c=P(u*stride,v*stride+stride),d=P(u*stride+stride,v*stride+stride);
        bool valid=true;OriginalRollerTriangleHit hit;
        if(cache.half!=0) {
            Quad n=cellNormal(d,b,c);
            if(n[0]==degenerateNormal)valid=false;else hit=originalRollerTriangle(collider,world,d,b,c,n);
        } else {
            Quad n=cellNormal(c,b,a);
            if(n[0]==degenerateNormal)valid=false;else hit=originalRollerTriangle(collider,world,c,b,a,n);
        }
        if(valid&&hit.hit)return contact(hit,u,v);
        cache={};
    }
    for(unsigned u=0;u<cells;++u)for(unsigned v=0;v<cells;++v) {
        Quad a=P(u*stride,v*stride),b=P(u*stride+stride,v*stride),c=P(u*stride,v*stride+stride),d=P(u*stride+stride,v*stride+stride);
        bool overlap=true;
        for(unsigned k=0;k<3;++k) {
            float low=std::min(std::min(a[k],b[k]),std::min(c[k],d[k])),high=std::max(std::max(a[k],b[k]),std::max(c[k],d[k]));
            overlap&=low<bounds.high[k]&&bounds.low[k]<high;
        }
        if(!overlap)continue;
        Quad n=cellNormal(d,b,c);
        if(n[0]!=degenerateNormal) {
            auto hit=originalRollerTriangle(collider,world,d,b,c,n);
            if(hit.hit){cache={patch.resource,uint16_t(u),uint16_t(v),1u,detailed};return contact(hit,u,v);}
        }
        n=cellNormal(c,b,a);
        if(n[0]!=degenerateNormal) {
            auto hit=originalRollerTriangle(collider,world,c,b,a,n);
            if(hit.hit){cache={patch.resource,uint16_t(u),uint16_t(v),0u,detailed};return contact(hit,u,v);}
        }
    }
    return std::nullopt;
}

// 0x330540 (query vtable+0x4C): collider box (centre +- 0x32C590 radius) against [low, high].
inline bool originalRollerBoundsTest(const OriginalSphereTreeCollider& collider,const terrain_original::Vector& low,const terrain_original::Vector& high){
    using namespace roller_world_query_detail;
    float r=terrain_original::mul(collider.tree->levels[0].radiusCm,collider.scale);
    Quad extent{r,r,r,0.f},top=qadd(collider.center,extent),bottom=qsub(collider.center,extent);
    for(unsigned k=0;k<3;++k) {
        if(top[k]<low[k]&&bottom[k]<low[k])return false;
        if(high[k]<top[k]&&high[k]<bottom[k])return false;
    }
    return true;
}
// 0x330710 (query vtable+0xC): 0x327CC8 copies corners, centre and the tree
// header (NOT +0x90) from the query collider into the scratch collider, then
// 0x32C770 transforms it by the node inverse and scales by 1/instance scale,
// multiplying the scratch's own (stale) +0x90.
inline void originalRollerLocalCollider(OriginalSphereTreeCollider& scratch,const OriginalSphereTreeCollider& source,
        const collision_transform::Matrix& inverse,float reciprocalScale){
    using namespace roller_world_query_detail;
    scratch.corners=source.corners;scratch.center=source.center;scratch.tree=source.tree;scratch.treeResource=source.treeResource;
    for(unsigned i=0;i<4;++i) {
        Quad corner=qscale(qapply(inverse,scratch.corners[i]),reciprocalScale);
        scratch.corners[i]=corner;scratch.corners[7-i]=qscale(corner,-1.f);
    }
    Quad center=qapply(inverse,scratch.center);
    for(unsigned k=0;k<3;++k)center[k]=terrain_original::mul(center[k],reciprocalScale);
    scratch.center=center;scratch.scale=terrain_original::mul(scratch.scale,reciprocalScale);
}
// 0x32C0F8(query, mesh, double-sided, packets, capacity) for the roller query
// type (query+8 == 0: no normal filter). Meshes of 11+ triangles test 10-triangle
// groups first with 0x330540 against the authored group boxes (+8), which are
// the exact per-group vertex min/max (verified); the port derives them.
inline std::vector<OriginalRollerWorldPacket> originalRollerMesh(OriginalSphereTreeCollider& collider,const WorldBodyCollision* world,
        const CollisionTriangleMesh& mesh,bool doubleSided){
    using namespace roller_world_query_detail;std::vector<OriginalRollerWorldPacket> packets;
    const unsigned count=unsigned(mesh.indices.size()/3);
    if(mesh.normals.size()<count)throw std::runtime_error("Original collision mesh normal count");
    auto test=[&](unsigned t) {
        Quad a=point4(mesh.vertices.at(mesh.indices[t*3])),b=point4(mesh.vertices.at(mesh.indices[t*3+1])),c=point4(mesh.vertices.at(mesh.indices[t*3+2]));
        Quad n=normal4(mesh.normals[t]);
        auto hit=doubleSided?originalRollerDoubleSidedTriangle(collider,world,a,b,c,n):originalRollerTriangle(collider,world,a,b,c,n);
        if(!hit.hit)return;
        OriginalRollerWorldPacket packet;packet.point=hit.point;packet.normal=hit.normal;packet.depth=hit.depth;packet.triangle=int32_t(t);
        packets.push_back(packet);
    };
    if(count<11){for(unsigned t=0;t<count;++t)test(t);return packets;}
    for(unsigned group=0;group<(count+9)/10;++group) {
        unsigned first=group*10,last=std::min(first+10,count);
        Vector low=mesh.vertices.at(mesh.indices[first*3]),high=low;
        for(unsigned t=first;t<last;++t)for(unsigned j=0;j<3;++j){const auto& p=mesh.vertices.at(mesh.indices[t*3+j]);for(unsigned k=0;k<3;++k){low[k]=std::min(low[k],p[k]);high[k]=std::max(high[k],p[k]);}}
        if(!originalRollerBoundsTest(collider,low,high))continue;
        for(unsigned t=first;t<last;++t)test(t);
    }
    return packets;
}


// 0x335960 on one patch: runtime flags (+0xA & 0x41) == 0x41 (the browser keeps
// the authored flags; bit 0x40 is set on every loaded patch), strict bounds
// (+0x158/+0x164) against the query box, then 0x32B6E0 coarse.
inline std::optional<OriginalRollerWorldPacket> originalRollerPatchQuery(const WorldCollisionTerrain& patch,const OriginalRollerQueryBounds& bounds,
        OriginalSphereTreeCollider& collider,const WorldBodyCollision* world,OriginalRollerTerrainCache& cache){
    if(!(patch.flags&1))return std::nullopt;
    for(unsigned k=0;k<3;++k)if(!(patch.low[k]<bounds.high[k]&&bounds.low[k]<patch.high[k]))return std::nullopt;
    return originalRollerTerrainPatch(patch,bounds,collider,world,cache);
}
// 0x334888 filter 2 for the roller query type on one instance: every surface
// -1 node is skipped; other nodes run 0x330710 (local scratch collider), then
// type 1: 0x330540 node bounds + 0x32C0F8; contacts are returned to world
// space (normal = M*n, point = M*(p*scale, w)) with the local depth. 334888
// uses the entity matrices whenever instance+0xC is set (entity != nullptr).
// The scratch collider persists across the instances of one query.
inline void originalRollerInstanceQuery(const WorldCollisionInstance& instance,const WorldCollisionEntity* entity,
        const OriginalSphereTreeCollider& collider,OriginalSphereTreeCollider& scratch,const WorldBodyCollision* world,
        std::vector<OriginalRollerWorldPacket>& out){
    using namespace roller_world_query_detail;
    if(!instance.unsupported.empty()&&instance.nodes.empty())
        throw std::runtime_error("Original roller query: unsupported instance ("+instance.unsupported+")");
    for(size_t nodeIndex=0;nodeIndex<instance.nodes.size();++nodeIndex) {
        const auto& node=instance.nodes[nodeIndex];
        if(node.surface==-1)continue; // 0x334D00 filter 2
        if(instance.scale==0.f)throw std::runtime_error("Original roller query: zero-scale instance");
        if(node.type==2)throw std::runtime_error("Original roller query: type-2 box surface node (0x330950) is not ported");
        if(node.type==3)throw std::runtime_error("Original roller query: type-3 sphere-tree surface node (0x3308C8) is not ported");
        if(node.type!=1||!node.triangles)throw std::runtime_error("Original roller query: surface node geometry unavailable");
        const auto& nodeWorld=entity?entity->world.at(nodeIndex):node.world;
        const auto& nodeInverse=entity?entity->inverse.at(nodeIndex):node.inverse;
        float reciprocal=originalScalarDivide(1.f,instance.scale); // 0x334E54 div.s
        originalRollerLocalCollider(scratch,collider,nodeInverse,reciprocal);
        if(!originalRollerBoundsTest(scratch,node.low,node.high))continue;
        auto packets=originalRollerMesh(scratch,world,*node.triangles,node.doubleSided);
        for(auto& packet:packets) {
            packet.normal=qapply(nodeWorld,packet.normal);
            Quad scaled=packet.point;for(unsigned k=0;k<3;++k)scaled[k]=terrain_original::mul(scaled[k],instance.scale);
            scaled[3]=terrain_original::mul(scaled[3],1.f);
            packet.point=qapply(nodeWorld,scaled);
            packet.terrain=false;packet.resource=instance.resource;packet.node=node.index;packet.surface=node.surface;
            out.push_back(packet);
        }
    }
}
// 0x335B90 route and bounds. Runtime flags come from eventRuntimeFlags
// (authored instances load static: 0x20). Static: instance+0x60/+0x6C bounds;
// entity route (0x40): entity bounds (vtable+0x16C; the vtable+0x164
// predicate is taken as true when entity geometry exists). The original skips
// an entity-route instance without an entity; the port only does so when no
// node could contribute (all surfaces -1), otherwise it throws.
inline void originalRollerInstanceRoute(const WorldCollisionInstance& instance,const OriginalRollerQueryBounds& bounds,
        const OriginalSphereTreeCollider& collider,OriginalSphereTreeCollider& scratch,const WorldBodyCollision* world,
        std::vector<OriginalRollerWorldPacket>& out){
    if(instance.type==0)return; // no descriptor / 0x334888 type 0
    const uint32_t flags=instance.eventRuntimeFlags?*instance.eventRuntimeFlags:0x20u;
    const WorldCollisionEntity* entity=instance.entity.get();
    const terrain_original::Vector* low=nullptr,*high=nullptr;
    if(flags&0x20u){low=&instance.low;high=&instance.high;}
    else if(flags&0x40u) {
        if(!entity) {
            for(const auto& node:instance.nodes)if(node.surface!=-1)throw std::runtime_error("Original roller query: entity-route instance without entity geometry");
            return;
        }
        low=&entity->low;high=&entity->high;
    } else return;
    for(unsigned k=0;k<3;++k)if(!((*low)[k]<bounds.high[k]&&bounds.low[k]<(*high)[k]))return;
    originalRollerInstanceQuery(instance,entity,collider,scratch,world,out);
}

struct OriginalRollerWorldQueryResult {
    OriginalRollerWorldContact contact;
    std::vector<OriginalRollerWorldPacket> packets; // 0x335D78 collection (terrain first, then instances)
    int selected=-1;
};
// 0x336850 selection over the collected packets (0x3306D8: preferred -1 for
// query+0 == 0, twice the 0x32C590 radius otherwise).
inline int originalRollerSelect(std::span<const OriginalRollerWorldPacket> packets,float preferred=-1.f){
    if(packets.empty())return -1;
    OriginalRounding rounding;
    auto metric=[&](const OriginalRollerWorldPacket& p){return std::fabs(originalScalarSubtract(p.depth,preferred));};
    int selected=0;float best=metric(packets[0]);
    for(size_t i=1;i<packets.size();++i) {
        float d=metric(packets[i]);
        if(d<best){best=d;selected=int(i);continue;}
        if(!(d==best))continue;
        const auto& candidate=packets[i];const auto& current=packets[size_t(selected)];
        if(!candidate.terrain){if(current.terrain||candidate.resource<current.resource)selected=int(i);}
        else if(current.terrain&&candidate.resource<current.resource)selected=int(i);
    }
    return selected;
}
// 0x3303F0 query box: centre +- 0x32C590 radius (add.s/sub.s), w = 1.
inline OriginalRollerQueryBounds originalRollerQueryBounds(const OriginalSphereTreeCollider& collider){
    OriginalRounding rounding;OriginalRollerQueryBounds bounds;
    float radius=terrain_original::mul(collider.tree->levels[0].radiusCm,collider.scale);
    for(unsigned k=0;k<3;++k){bounds.high[k]=originalScalarAdd(collider.center[k],radius);bounds.low[k]=originalScalarSubtract(collider.center[k],radius);}
    bounds.high[3]=bounds.low[3]=1.f;return bounds;
}

// 0x335D78 visit order of terrain patches for a query box smaller than a
// level-11 loose cell: root slots 0..7 (by coordinate signs), then 0x33CCF8 at
// every partially overlapping node: the node's patch list (head insertion
// order, WorldCollisionTerrain::insertionOrder descending), its instances, then
// the children in Gray order 0,1,3,2,6,7,5,4. (0x340FA0, used only for nodes
// whose loose cell lies inside the box, visits children 0..7 as
// WorldBodyCollision::prepareTerrainTraversal does; the port refuses boxes that
// could contain a cell.) Cells that do not overlap the box contain no
// overlapping patch, so sorting the overlapping patches by this key reproduces
// the original sequence.
inline bool originalRollerSpatialBefore(const OriginalSpatialCell& a,const OriginalSpatialCell& b){
    static constexpr unsigned grayRank[8]={0,1,3,2,7,6,4,5}; // rank of child c in 0,1,3,2,6,7,5,4
    unsigned ar=originalSpatialRoot(a),br=originalSpatialRoot(b);if(ar!=br)return ar<br;
    for(int level=30;level>=std::max(a.level,b.level);--level) {
        auto child=[&](const OriginalSpatialCell& c){unsigned result=0;for(unsigned k=0;k<3;++k)result=(result<<1)|(uint32_t(c.coordinate[k])>>unsigned(level-c.level)&1);return result;};
        unsigned ac=child(a),bc=child(b);if(ac!=bc)return grayRank[ac]<grayRank[bc];
    }
    return a.level>b.level;
}

// Full 0x3303F0 + 0x336850 + 0x3304E8 on the native world.
inline OriginalRollerWorldQueryResult originalRollerWorldQueryDetailed(const WorldBodyCollision& world,OriginalSphereTreeCollider& collider,OriginalRollerTerrainCache& cache){
    OriginalRounding rounding;
    if(!collider.tree||collider.tree->levels.empty())throw std::runtime_error("Original roller collider has no sphere tree");
    OriginalRollerWorldQueryResult result;
    const auto bounds=originalRollerQueryBounds(collider);
    // A level-11 loose cell spans 1.4*2048 cm; a box that wide could be fully inside-tested (0x340FA0 order).
    if(!(originalScalarSubtract(bounds.high[0],bounds.low[0])<2867.2f))throw std::runtime_error("Original roller query box can contain an octree cell (0x340FA0 order not modeled)");
    std::vector<const WorldCollisionTerrain*> patches;
    for(const auto& patch:world.terrain) {
        if(!(patch.flags&1)||!worldResident(patch.resource))continue;bool overlap=true;
        for(unsigned k=0;k<3;++k)overlap&=patch.low[k]<bounds.high[k]&&bounds.low[k]<patch.high[k];
        if(overlap)patches.push_back(&patch);
    }
    std::stable_sort(patches.begin(),patches.end(),[](const WorldCollisionTerrain* a,const WorldCollisionTerrain* b) {
        if(originalRollerSpatialBefore(a->spatial,b->spatial))return true;
        if(originalRollerSpatialBefore(b->spatial,a->spatial))return false;
        return a->insertionOrder>b->insertionOrder; // 0x328660 inserts at the list head
    });
    for(const auto* patch:patches)
        if(auto packet=originalRollerPatchQuery(*patch,bounds,collider,&world,cache))result.packets.push_back(*packet);
    OriginalSphereTreeCollider scratch;scratch.scale=1.f; // fresh 0x32C508 object per query
    for(const auto& instance:world.instances)if(worldResident(instance.resource))originalRollerInstanceRoute(instance,bounds,collider,scratch,&world,result.packets);
    if(result.packets.size()>64)throw std::runtime_error("Original roller query: more than 64 contacts overflow 0x336850");
    result.selected=originalRollerSelect(result.packets);
    if(result.selected>=0) {
        const auto& packet=result.packets[size_t(result.selected)];
        result.contact.hit=true;result.contact.point=packet.point;result.contact.normal=packet.normal;result.contact.depth=packet.depth;
        result.contact.terrain=packet.terrain;result.contact.resource=packet.resource;result.contact.node=packet.node;
    }
    result.contact.contacts=unsigned(result.packets.size());
    return result;
}
// 0x35EDC8's world query: selected packet point/normal/depth (depth -1 and no
// hit when nothing was collected; the original then leaves its packet untouched).
inline OriginalRollerWorldContact originalRollerWorldQuery(const WorldBodyCollision& world,OriginalSphereTreeCollider& collider,OriginalRollerTerrainCache& cache){
    return originalRollerWorldQueryDetailed(world,collider,cache).contact;
}
}
