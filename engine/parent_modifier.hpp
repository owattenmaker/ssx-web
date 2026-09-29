#pragma once
// Original ParentModifier (ELF name "ParentModifier", vtable 0x48F508, 0xB0-byte object) and the
// LiveComp-on-a-modifier node composition (the raven flyby / the Junction blimp).
//
// ParentModifier. Script builtin18 (0x2FDC60; defaults 0x4FB758: key0 parent instance (-1 = the
// script's current instance), key1 child instance, key2 node (int, default 0), keys 3..5 offset
// x/y/z (float, default 0); key types 0x4461C0 = 1 1 1 2 2 2 1): when the parent has an entity
// (vt+0x54(6) == 0) and the child's entity accepts modifiers (vt+0x84), 0x355978 allocates 0xB0
// bytes, runs the ctor 0x357038 and attaches it to the CHILD (0x3554B0):
//   0x357038 -> 0x356EB8 (Position base): +0x30 offset (w 0), +0x50..+0x8F identity (0x4FF1A0) with
//            row 3 = offset, +0x44 dirty = 1; then +0xA0 parent instance, +0x90 node, vtable 0x48F508.
//   update (vt+0x14, 0x3619B8): dirty = 1 (every tick; the matrix is lazily re-evaluated on read,
//            vt+0x94 0x361940 -> vt+0x1C 0x357108).
//   evaluate 0x357108: parent entity = parent instance +0xC; none -> just clean. If the parent
//            entity has animated nodes (vt+0xD0: LiveComp 0x361090 -> 1; Object 0x360990 -> 0) the
//            matrix is the parent's node world matrix (vt+0xE8, LiveComp 0x3610E0: node world with
//            the instance / primary-modifier root), else 0x34FED8(parent instance, node) (static model
//            node path); then row 3 += offset * M (VU: x*r0 + y*r1 + z*r2 + w*r3 with offset w = 0,
//            then vadd.xyzw). dirty = 0.
//   contact (vt+0xB4, 0x357210): forwards to the parent entity's vt+0x154 with the node (not modeled).
// Users (all offsets 0): Metro-City programs 38/39/40 and The Junction programs 28..35 (slot 1 of the
// searchlightbasea_* LiveComps: builtin3 random start, builtin0 on searchlightglowa_*, builtin18(child =
// searchlightglowa, node 3) -> the light cone follows the swinging lamp head), The Junction program 39
// (blimpad_1000/_1001 and blimplights_1000 on blimpa_1000 node 0). Snow Jam has none; no location of
// the three uses the Halo modifier (builtin92).
//
// LiveComp on a primary modifier (program 54 raven: builtin3() then builtin19 Spline; The Junction
// program 39: builtin3(key5 10) then Spline): the LiveComp node matrices (0x361098 -> 0x34DC90 ->
// 0x34DD18) compose onto the entity matrix 0x356078 = primary modifier vt+0x94 (the spline matrix,
// lazily evaluated), so node world = local * ... * spline matrix (translations scaled by the instance
// scale +0x84). The LiveComp still plays its own time (0x341D48, the entity pass): the raven's default
// player loops the 0.6 s model animation at 1/60 s per tick (wings: nodes 1..4 rotate about local Y,
// node 0 bobs along Z); the blimp loops its 2 s animation at 10/30/60 s per tick (body bob, propellers).
// Builtin31 (0x3000A8) in program 54 is audio: 0x28B180 -> 0x297950(audio, instance, 181, 0), a
// positional sound (the raven's caw loop), no visual state.
// Float policy as engine/rail_modifier.hpp; compile with -ffp-contract=off.
#include "livecomp_animation.hpp"

namespace ssx {

struct OriginalParentModifier {
    RollerQuad offset{};               // +0x30 (w 0)
    uint32_t dirty=1;                  // +0x44
    RollerMatrix matrix=rail_modifier_math::identity(); // +0x50..+0x8F
    int32_t node=0;                    // +0x90
    uint32_t parent=0;                 // +0xA0 parent instance (browser: resource)
};

// 0x357038 (+0x356EB8).
inline OriginalParentModifier originalParentConstruct(uint32_t parent,int32_t node,const RollerQuad& offset){
    OriginalParentModifier p;p.offset=offset;p.matrix=rail_modifier_math::identity();p.matrix[3]=offset;p.dirty=1;p.node=node;p.parent=parent;return p;
}
// 0x3619B8 (vt+0x14): every entity update.
inline void originalParentUpdate(OriginalParentModifier& p){p.dirty=1;}
// 0x357108 with the parent's node world matrix (vt+0xE8 / 0x34FED8). parentNode == nullptr: the
// parent instance has no entity (the matrix keeps its value, only the dirty flag clears).
inline void originalParentEvaluate(OriginalParentModifier& p,const RollerMatrix* parentNode){
    if(parentNode){
        p.matrix=*parentNode;
        OriginalRounding rounding;
        const RollerQuad moved=roller_math::transform(p.matrix,p.offset);
        p.matrix[3]=roller_math::vadd(p.matrix[3],moved);
    }
    p.dirty=0;
}
// 0x361940 (vt+0x94): lazy matrix.
template<class ParentNode> inline const RollerMatrix& originalParentMatrix(OriginalParentModifier& p,ParentNode&& parentNode){
    if(p.dirty)originalParentEvaluate(p,parentNode());
    return p.matrix;
}

// LiveComp node world matrices on a primary-modifier root (0x361098 with 0x356078 -> modifier
// vt+0x94). Recomputed only while the entity is dirty (0x341D48 sets it every tick unless the
// player is done), reading the root at that moment.
inline const std::vector<RollerMatrix>& originalLiveCompMatricesOn(OriginalLiveComp& s,const RollerMatrix& root){
    if(s.anim.dirty){s.anim.instanceMatrix=root;originalAnimEvaluateChannels(s.anim);originalAnimComposeNodes(s.anim);s.anim.dirty=false;}
    return s.anim.matrices;
}
// Rest pose of the baked (static) draw: bind matrices composed on the authored instance matrix.
inline std::vector<RollerMatrix> originalAnimRestMatrices(const OriginalAnimModel& model,const RollerMatrix& authored,float scale){
    OriginalAnimTeeter rest;rest.model=&model;rest.instanceMatrix=authored;rest.instanceScale=scale;
    using namespace rail_modifier_math;OriginalRounding rounding;std::vector<RollerMatrix> out(model.nodes.size());
    for(size_t i=0;i<model.nodes.size();++i){
        RollerMatrix local=scaledLocal(model.nodes[i].bind,scale);
        out[i]=product(local,model.nodes[i].parent>=0?out.at(size_t(model.nodes[i].parent)):authored);
    }
    return out;
}
}
