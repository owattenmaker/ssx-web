#pragma once
// Native translation of the SSX3 PS2 (SLUS_207.72) in-race camera director
// (vtable 0x45B908): the fading algorithm list, the per-frame director update
// 0x161BB8 (transitions 0x161AB0, weight ramps with smoothstep, auto-revert),
// the list operations 0x15C988 / 0x15CA50 / 0x15CB08 / 0x161E58, the factory
// 0x15D078, and the multi-algorithm blend path of the compositor 0x15E668.
// Requests: finish 0x162258 (POST_RACE_1 fade), restore 0x162290, restart
// 0x15DB58, default selection 0x161EF0, outer ctor activation 0x15DC70.
//
// The algorithm objects themselves (DEFAULT_2/3/4 and POST_RACE_1) and the
// compositor stages after the gather live in original_camera.hpp. Numerical
// policy is the same: every entry point runs inside an FE_TOWARDZERO scope.
// Addresses, layout and verification: engine/CAMERA_RECOVERY.md "Director".
#include "original_camera.hpp"
#include <stdexcept>
#include <vector>

namespace ssx {

// One 0x1C-byte list node (0x15C988): +0x00 algorithm, +0x04 raw weight,
// +0x08 fade rate per frame, +0x0C smoothstep weight, +0x10/+0x14 links,
// +0x18 owned flag. nodes[0] of the director is the list head (newest).
struct OriginalCameraDirectorNode {
    OriginalChaseAlgorithmState algorithm;
    float weight=0;     // +0x04: ramps by the head's rate; blends fov/near/far
    float rate=0;       // +0x08
    float smoothed=0;   // +0x0C: 3w^2 - 2w^3 (capped at 1); blends eye/look-at
};

// Director fields +0x08..+0x40 (0x161950 init values).
struct OriginalCameraDirectorState {
    std::vector<OriginalCameraDirectorNode> nodes; // +0x08 list (+0 head, +4 count)
    int32_t currentType=0x3D;       // +0x18 type of the newest algorithm (its +0x0C)
    int32_t guardType=0x3D;         // +0x1C (0x161AB0 forces the default back to 0x3D when not 0x3D)
    int32_t preferredType=0x3D;     // +0x24 player camera select (gp+0x1CE0[slot])
    int32_t overrideType=0x3D;      // +0x28 trigger override type
    int32_t lastRequestedType=0x3D; // +0x2C
    uint32_t flags=0;               // +0x30: bit0 trigger override, bit1 locked (replay), bit2 SPOKE
    int32_t overrideArgument=0;     // +0x34
    float revertTimer=0;            // +0x38 seconds (auto-revert after 10 s)
    int32_t spokeLatch=0;           // +0x3C handplant (motion 5) latch
    int32_t field40=0;              // +0x40
    uint32_t unsupportedRequests=0; // native: requests for algorithms that are not ported (SPOKE 0x42 ...)
    int32_t lastUnsupportedType=0;
};

// Director + outer camera (compositor) state, as stepped by 0x15DF98.
struct OriginalDirectedCameraState {
    OriginalCameraDirectorState director;
    OriginalCameraCompositorState compositor;
};

namespace original_camera_director {
using namespace original_camera;
inline constexpr float finishRate=std::bit_cast<float>(0x3C899268u);     // gp-0x6B10 0.016793445 (~60 frames)
inline constexpr float spokeReturnRate=std::bit_cast<float>(0x3BDA740Fu); // gp-0x6B14 1/150
inline constexpr float speedToKmh=std::bit_cast<float>(0x3D1374BCu);      // gp-0x6B28 0.036
inline constexpr float blendMinimum=std::bit_cast<float>(0x38D1B717u);    // gp-0x6C70 1e-4

// Factory 0x15D078 jump table 0x45B5C0 restricted to the ported algorithms.
// Returns the constructed algorithm's own type (+0x0C), or 0 when not ported.
inline int32_t algorithmForRequest(int32_t type){
    switch(type){
    case 0x2:case 0x3C:return 0x3C;                // DEFAULT_2 Near, ctor 0x176A68
    case 0x1:case 0x3E:return 0x3E;                // DEFAULT_4 Far, ctor 0x177068
    case 0x0:case 0x3:case 0x4:case 0x5:case 0x6:case 0x8:case 0xC:case 0xE:case 0xF:case 0x10:case 0x11:
    case 0x1A:case 0x1B:case 0x1C:case 0x1D:case 0x1E:case 0x1F:case 0x3D:case 0x3F:case 0x40:case 0x41:case 0x4A:
        return 0x3D;                               // DEFAULT_3 Mid, ctor 0x176D68
    case 0x44:return 0x44;                         // POST_RACE_1, ctor 0x1789E8
    default:
        if(type>=0x2A&&type<=0x3B)return 0x3D;     // DEFAULT_3 range of the jump table
        return 0;
    }
}

// 0x15C988 push: the new algorithm's set-target runs (vtable slot 0x20 -> 0x166F28), then the
// node is inserted at the head with weight 1 when the rate is exactly 1, else 0 (+0x0C is not
// initialised; the next director update writes it before the compositor reads it).
template<class SetTarget> inline void push(OriginalCameraDirectorState& d,const OriginalChaseAlgorithmState& algorithm,float rate,SetTarget&& setTargetFn){
    OriginalCameraDirectorNode node;node.algorithm=algorithm;
    setTargetFn(node.algorithm);
    node.rate=rate;node.weight=rate==1.f?1.f:0.f;
    d.nodes.insert(d.nodes.begin(),std::move(node));
}
// 0x161E58: +0x18 = algorithm type; an empty list takes the node at rate 1, rate 1 clears the list
// first (0x15CB08), any other rate fades the new head in over the existing nodes.
template<class SetTarget> inline void insertAlgorithm(OriginalCameraDirectorState& d,const OriginalChaseAlgorithmState& algorithm,float rate,SetTarget&& setTargetFn){
    d.currentType=algorithm.variantType;
    if(d.nodes.empty())push(d,algorithm,1.f,setTargetFn);
    else if(rate==1.f){d.nodes.clear();push(d,algorithm,rate,setTargetFn);}
    else push(d,algorithm,rate,setTargetFn);
}

// 0x15D078 factory + 0x161E58 list insertion (+ the tail set-target of 0x15D628).
inline bool activate(OriginalCameraDirectorState& d,const OriginalCameraInput& in,int32_t type,float rate){
    if(d.flags&2)rate=1;
    const int32_t algorithmType=algorithmForRequest(type);
    if(!algorithmType){++d.unsupportedRequests;d.lastUnsupportedType=type;return false;}
    OriginalChaseAlgorithmState algorithm;
    if(algorithmType==0x44)postRaceConstruct(algorithm,in);
    else {algorithm=OriginalChaseAlgorithmState{};algorithm.variantType=algorithmType;}
    // vtable slot 0x60 (0x162310) stores the target pointer: the input carries it.
    insertAlgorithm(d,algorithm,rate,[&](OriginalChaseAlgorithmState& a){setTarget(a,in);});
    setTarget(d.nodes.front().algorithm,in);                     // 0x15D650: vtable slot 0x20 again
    return true;
}

// 0x162060(director, type, argument) with the caller's rate.
inline void request(OriginalCameraDirectorState& d,const OriginalCameraInput& in,int32_t type,int32_t argument,float rate){
    if(type==0x4C){d.flags&=~1u;type=d.preferredType;}
    else {d.overrideType=type;d.overrideArgument=argument;d.flags|=1u;}
    if((d.flags&2)==0||d.lastRequestedType==0x5D)activate(d,in,type,rate);
}

// 0x161EF0 setDefaultAlgorithm (Pause Options camera select); instant cut unless overridden.
inline void setDefault(OriginalCameraDirectorState& d,const OriginalCameraInput& in,int32_t type){
    d.preferredType=type;
    if((d.flags&7)==0)activate(d,in,type,1.f);
}

// 0x1621A8 SPOKE fade-in (motion 5 handplant) and 0x162218 return fade.
inline void spokeBegin(OriginalCameraDirectorState& d,const OriginalCameraInput& in){
    d.flags|=4u;
    if(d.flags&3)return;
    float rate=emul(in.motionMode==5?std::bit_cast<float>(0x3F99999Au):std::bit_cast<float>(0x3F19999Au),frameSeconds);
    activate(d,in,0x42,rate);
}
inline void spokeEnd(OriginalCameraDirectorState& d,const OriginalCameraInput& in){
    d.flags&=~4u;
    if(d.flags&3)return;
    activate(d,in,d.preferredType,spokeReturnRate);
}

// 0x161AB0 decision: guard reset, SPOKE latch (motion 5) and its release above 35 km/h.
struct TransitionDecision {bool resetDefault=false,spokeBegin=false,spokeEnd=false;int32_t latch=0;};
inline TransitionDecision decideTransitions(int32_t guardType,int32_t currentType,int32_t latch,int32_t motionMode,const Quad& velocity){
    TransitionDecision t;t.latch=latch;
    t.resetDefault=guardType!=0x3D&&currentType!=0x4A;
    float speed=emul(len4(velocity),speedToKmh);
    if(latch!=0){if(35.f<speed){t.latch=0;t.spokeEnd=true;}}
    else if(motionMode==5){t.latch=1;t.spokeBegin=true;}
    return t;
}
inline void transitions(OriginalCameraDirectorState& d,const OriginalCameraInput& in){
    TransitionDecision t=decideTransitions(d.guardType,d.currentType,d.spokeLatch,in.motionMode,in.velocity);
    if(t.resetDefault)setDefault(d,in,0x3D);
    d.guardType=0x3D;
    d.spokeLatch=t.latch;
    if(t.spokeBegin)spokeBegin(d,in);
    if(t.spokeEnd)spokeEnd(d,in);
}

inline float smoothWeight(float w){
    float f1=emul(w,w);float f0=eadd(f1,f1);f1=emul(f1,3.f);f0=emul(f0,w);f1=esub(f1,f0);
    return 1.f<f1?1.f:f1;
}

// Director inputs that are not rider data: game state *(world+0x28)+0 in [1,9] (shared with the
// shake suppression of 0x15E360) and *(world+0x28)+0x484.
struct Frame {bool raceStateActive=false;int32_t managerField484=1;};

// 0x161BB8 node pass: the head steps (vtable slot 0x48, 0x1624E8, a1 = 0) and ramps toward 1 by its
// rate; older nodes ramp toward 0 by the head's rate, type 0x4A is forced to 0, nodes at or below
// zero raw or smoothed weight are unlinked (0x15CA50), the rest step (a1 = 1).
template<class Step> inline void advanceNodes(OriginalCameraDirectorState& d,Step&& step){
    if(d.nodes.empty())throw std::runtime_error("Camera director has no algorithm");
    OriginalCameraDirectorNode& head=d.nodes.front();
    step(head,0);
    const float rate=head.rate;
    float w=head.weight,next;
    if(eadd(rate,1.f)<w)next=esub(w,rate);
    else if(w<esub(1.f,rate))next=eadd(w,rate);
    else next=1.f;
    head.weight=next;head.smoothed=smoothWeight(next);
    for(size_t i=1;i<d.nodes.size();){
        OriginalCameraDirectorNode& node=d.nodes[i];
        float old=node.weight;
        if(rate<old)next=esub(old,rate);
        else {next=0;if(old<-rate)next=eadd(old,rate);}
        node.weight=next;node.smoothed=smoothWeight(next);
        if(node.algorithm.variantType==0x4A)node.weight=0;
        if(node.weight<=0.f||node.smoothed<=0.f){d.nodes.erase(d.nodes.begin()+i);continue;} // 0x15CA50
        step(node,1);
        ++i;
    }
}
// 0x161D80..0x161E30: auto-revert of a 0x5D request after 10 s in game states 1..9. Returns
// true when 0x162060(director, preferred, 0) must run at rate 1.
inline bool advanceRevertTimer(OriginalCameraDirectorState& d,const Frame& frame){
    if(frame.raceStateActive&&frame.managerField484==0)d.revertTimer=0;
    if(!frame.raceStateActive||d.lastRequestedType!=0x5D||d.currentType==d.preferredType){d.revertTimer=0;return false;}
    d.revertTimer=eadd(d.revertTimer,frameSeconds);
    if(10.f<d.revertTimer){d.revertTimer=0;return true;}
    return false;
}
// 0x161BB8 director update (vtable slot 0x10), skipped when *(gp-0x6A0)&1.
inline void update(OriginalCameraDirectorState& d,const OriginalCameraInput& in,const Frame& frame){
    transitions(d,in);
    advanceNodes(d,[&](OriginalCameraDirectorNode& node,int){stepAlgorithm(node.algorithm,in);});
    if(advanceRevertTimer(d,frame))request(d,in,d.preferredType,0,1.f);
}

// 0x15E668 gather: a single node (or a type 0x4A head) is copied; otherwise the eye (+0x60) and
// look-at (+0x20) are running weighted means over nodes whose smoothed weight exceeds 1e-4, and
// fov/near/far are running means over the raw weights (nodes at weight exactly 0 skipped).
// 0x15E6E0..0x15E700: the head alone is copied when the list holds one node or the head is type 0x4A.
inline bool singleAlgorithm(const OriginalCameraDirectorState& d){return d.nodes.size()==1||d.nodes.front().algorithm.variantType==0x4A;}
// 0x15E770..0x15E8D0 multi-node eye/look-at running means.
inline void blendEyeLookAt(const std::vector<OriginalCameraDirectorNode>& nodes,Quad& eye,Quad& lookAt){
    eye=zeroQuad;lookAt=zeroQuad;float sum=0;
    for(const auto& node:nodes){
        float s=node.smoothed;
        if(!(blendMinimum<s))continue;
        sum=eadd(sum,s);float k=ediv(s,sum);float kc=esub(1.f,k);
        eye=qadd(qscale(node.algorithm.outputEye,k),qscale(eye,kc));
        lookAt=qadd(qscale(node.algorithm.lookAt,k),qscale(lookAt,kc));
    }
}
// 0x15EB08..0x15EBB8 fov/near/far running means over raw weights.
inline void blendLens(const std::vector<OriginalCameraDirectorNode>& nodes,float& fov,float& near,float& far){
    fov=0;near=0;far=0;float sum=0;
    for(const auto& node:nodes){
        float w=node.weight;if(w==0.f)continue;
        sum=eadd(sum,w);float k=ediv(w,sum);float kc=esub(1.f,k);
        fov=eadd(emul(k,node.algorithm.fov),emul(kc,fov));
        near=eadd(emul(k,node.algorithm.near),emul(kc,near));
        far=eadd(emul(k,node.algorithm.far),emul(kc,far));
    }
}
inline OriginalCameraOutput composite(OriginalDirectedCameraState& state,const OriginalCameraInput& in){
    OriginalCameraDirectorState& d=state.director;OriginalCameraCompositorState& o=state.compositor;
    const OriginalChaseAlgorithmState& head=d.nodes.front().algorithm;
    // A single node (0x15E72C..0x15E76C): the look-at keeps the algorithm's w (on the console model its blends can leave
    // 0x3F7FFFFF; cam-boost-slow 463), the eye's w is 1. The running means force both w lanes to 1 (0x15E8E4).
    if(singleAlgorithm(d)){o.eye=head.outputEye;o.lookAt=head.lookAt;o.eye[3]=1;}
    else {blendEyeLookAt(d.nodes,o.eye,o.lookAt);o.lookAt[3]=1;o.eye[3]=1;}
    float fov,near,far;blendLens(d.nodes,fov,near,far);
    OriginalCameraOutput out=compositeCamera(o,in,fov,near,far);
    out.algorithmEye=head.outputEye;out.algorithmLookAt=head.lookAt;out.resetFired=head.resetPending!=0;
    return out;
}
}

// 0x161950 director init (+0x24 from the session camera select) followed by the outer camera
// ctor's 0x162060(director, 0x4C, 0, 1.0): the preferred algorithm at full weight, set-target twice.
inline void originalDirectedCameraBegin(OriginalDirectedCameraState& state,const OriginalCameraInput& in,int32_t preferredType=0x3D){
    original_camera::Rounding rounding;
    state.director=OriginalCameraDirectorState{};state.director.preferredType=preferredType;
    state.compositor=OriginalCameraCompositorState{};
    original_camera_director::request(state.director,in,0x4C,0,1.f);
}

// 0x15DF98: director update 0x161BB8, then compositor 0x15E668.
inline OriginalCameraOutput originalDirectedCameraStep(OriginalDirectedCameraState& state,const OriginalCameraInput& in,const original_camera_director::Frame& frame={}){
    original_camera::Rounding rounding;
    original_camera_director::update(state.director,in,frame);
    return original_camera_director::composite(state,in);
}

// 0x162258 (called by the rider finish routine 0x125108 for a human rider): fade to POST_RACE_1
// at 0.016793445 per frame unless a trigger override (flag bit 0) is active.
inline void originalDirectedCameraFinish(OriginalDirectedCameraState& state,const OriginalCameraInput& in){
    original_camera::Rounding rounding;
    if(state.director.flags&1u)return;
    original_camera_director::activate(state.director,in,0x44,original_camera_director::finishRate);
}

// 0x162290: instant return to the preferred algorithm (finish overlay 0x234008).
inline void originalDirectedCameraRestore(OriginalDirectedCameraState& state,const OriginalCameraInput& in){
    original_camera::Rounding rounding;
    original_camera_director::activate(state.director,in,state.director.preferredType,1.f);
}

// 0x161EF0 (Pause Options camera select).
inline void originalDirectedCameraSelect(OriginalDirectedCameraState& state,const OriginalCameraInput& in,int32_t type){
    original_camera::Rounding rounding;
    original_camera_director::setDefault(state.director,in,type);
}

// Race restart 0x230180 -> 0x15DB58: 0x162170 (0x162138 clears the override/lock/SPOKE flags and
// latch, then an instant cut to the preferred algorithm), director vtable slot 0x20 (0x15CCF0:
// set-target on every node), compositor lift +0x460 = 0 and +0x4A4 = 1.
inline void originalDirectedCameraRestart(OriginalDirectedCameraState& state,const OriginalCameraInput& in){
    original_camera::Rounding rounding;
    OriginalCameraDirectorState& d=state.director;
    d.flags&=~7u;d.overrideArgument=0;d.overrideType=d.preferredType;d.spokeLatch=0;d.field40=0;
    original_camera_director::activate(d,in,d.preferredType,1.f);
    for(auto& node:d.nodes)original_camera::setTarget(node.algorithm,in);
    state.compositor.lift=0;state.compositor.firstFrameAfterReset=1;
}

}
