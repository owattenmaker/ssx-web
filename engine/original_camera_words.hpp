#pragma once
// Word-level view of the DEFAULT_3 algorithm object (0x390 bytes) for seeding from and comparing with PS2 memory
// (engine/original_camera_tests.cpp oracles, web/core.cpp camera_state_words / camera_seed_words).
#include "original_camera.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <type_traits>
namespace ssx::original_camera_words {
inline float f(uint32_t b){return std::bit_cast<float>(b);}
inline uint32_t u(float x){return std::bit_cast<uint32_t>(x);}
// Word-level view of the 0x390-byte algorithm object, used to seed from and
// compare against PCSX2 savestate snapshots. Only decoded fields are mapped.
template<class S,class Fn> inline void visitFields(S& s,Fn&& fn){
    auto quad=[&](unsigned off,auto& v){for(unsigned i=0;i<4;++i)fn(off+4*i,v[i]);};
    fn(0x00,s.fov);fn(0x04,s.near);fn(0x08,s.far);
    quad(0x20,s.lookAt);quad(0x40,s.eye);fn(0x50,s.yaw);fn(0x54,s.pitch);quad(0x60,s.outputEye);
    quad(0x80,s.direction);quad(0xA0,s.headCopy);quad(0xB0,s.savedOffset);quad(0xC0,s.pullBehind);
    quad(0xD0,s.fieldD0);quad(0xE0,s.fieldE0);quad(0xF0,s.fieldF0);quad(0x100,s.velocityFilter);
    quad(0x110,s.swingStartView);quad(0x120,s.swingTargetView);quad(0x130,s.swingAxis);quad(0x140,s.swingRotatedView);
    quad(0x150,s.swingWallProjection);quad(0x160,s.swingCurveProjection);quad(0x170,s.returnView);quad(0x180,s.returnLookDelta);
    quad(0x190,s.field190);quad(0x1A0,s.lockView);quad(0x1B0,s.lastVelocity);quad(0x1C0,s.lastRawVelocity);
    fn(0x1D0,s.distance);fn(0x1D4,s.eyeVerticalOffset);fn(0x1D8,s.filteredPitch);fn(0x1DC,s.fovScale);fn(0x1E0,s.mode5Timer);
    fn(0x1E4,s.lookAtHeight);fn(0x1E8,s.boostBump);fn(0x1EC,s.lastBoost);fn(0x1F0,s.field1F0);fn(0x1F4,s.phaseBClock);
    fn(0x1F8,s.amplitudeGain);fn(0x1FC,s.timeGain);fn(0x200,s.phaseBDuration);fn(0x204,s.heldEyeTerm);fn(0x208,s.heldLookTerm);
    fn(0x20C,s.lookOffsetB);fn(0x210,s.eyeOffsetB);fn(0x214,s.pushFilter);fn(0x218,s.phaseAClock);fn(0x21C,s.lookOffsetA);
    fn(0x220,s.eyeOffsetA);fn(0x224,s.landingDecay);fn(0x228,s.takeoffRamp);fn(0x22C,s.swingElapsed);fn(0x230,s.swingDuration);
    fn(0x234,s.swingClock);fn(0x238,s.swingAngle);fn(0x23C,s.returnTimer);fn(0x240,s.residualTimer);fn(0x244,s.residualStart);
    fn(0x248,s.lockBlend);fn(0x24C,s.field24C);fn(0x250,s.field250);fn(0x2B8,s.extraEyeHeight);fn(0x2BC,s.field2BC);
    fn(0x2C0,s.mode5Frames);fn(0x2C4,s.takeoffCountdown);fn(0x2C8,s.field2C8);fn(0x2CC,s.lastAirTick);fn(0x2D0,s.wallLaunch);
    fn(0x2D4,s.airborneLatch);fn(0x2D8,s.swingInitialised);fn(0x2DC,s.field2DC);fn(0x2E0,s.phaseBActive);fn(0x2E4,s.landed);
    fn(0x2E8,s.phaseAActive);fn(0x2EC,s.field2EC);fn(0x2F0,s.resetPending);fn(0x2F4,s.field2F4);fn(0x2F8,s.field2F8);
    fn(0x2FC,s.field2FC);fn(0x300,s.lockState);
    for(unsigned i=0;i<5;++i){fn(0x340+4*i,s.ringX[i]);fn(0x354+4*i,s.ringY[i]);fn(0x368+4*i,s.ringE[i]);}
    fn(0x37C,s.ringIndex);fn(0x380,s.gainP);fn(0x384,s.gainI);fn(0x388,s.gainD);
}
inline uint32_t splineFlags(const OriginalCubicSpline& sp){
    return (sp.clamped?1u:0u)|(sp.allocated?2u:0u)|(sp.built?4u:0u)|(sp.count<<3)|(sp.segments<<11)|(sp.current<<19);
}
inline void splineFromWords(OriginalCubicSpline& sp,uint32_t flags,uint32_t endWord){
    sp.clamped=flags&1;sp.allocated=flags&2;sp.built=flags&4;sp.count=(flags>>3)&0xFF;sp.segments=(flags>>11)&0xFF;sp.current=(flags>>19)&0xFF;sp.endValue=f(endWord);
}
// Overwrites only the decoded fields (and spline flags/end values) of an existing state.
inline void assignWords(OriginalChaseAlgorithmState& s,const std::array<uint32_t,228>& w){
    visitFields(s,[&](unsigned off,auto& v){
        using T=std::remove_reference_t<decltype(v)>;
        if constexpr(std::is_same_v<T,float>)v=f(w[off/4]);else v=int32_t(w[off/4]);
    });
    OriginalCubicSpline* splines[5]={&s.lookSplineB,&s.eyeSplineB,&s.lookSplineA,&s.eyeSplineA,&s.swingSpline};
    for(unsigned i=0;i<5;++i)splineFromWords(*splines[i],w[(0x304+12*i)/4],w[(0x308+12*i)/4]);
}
inline OriginalChaseAlgorithmState fromWords(const std::array<uint32_t,228>& w){OriginalChaseAlgorithmState s;assignWords(s,w);return s;}
inline std::array<uint32_t,228> toWords(const OriginalChaseAlgorithmState& s){
    std::array<uint32_t,228> w{};
    visitFields(s,[&](unsigned off,const auto& v){
        using T=std::remove_cv_t<std::remove_reference_t<decltype(v)>>;
        if constexpr(std::is_same_v<T,float>)w[off/4]=u(v);else w[off/4]=uint32_t(v);
    });
    const OriginalCubicSpline* splines[5]={&s.lookSplineB,&s.eyeSplineB,&s.lookSplineA,&s.eyeSplineA,&s.swingSpline};
    for(unsigned i=0;i<5;++i){w[(0x304+12*i)/4]=splineFlags(*splines[i]);w[(0x308+12*i)/4]=u(splines[i]->endValue);}
    return w;
}
// Compositor words (outer camera object; the embedded DEFAULT_3 compositor is outer+0xC0): lookAt, eye, lift, last
// probe normal, shake request (index, amplitude, pending, was-pending) and the shake walk (comp+0x254..+0x2B4, +0x2EC).
inline constexpr std::array<uint16_t,43> compositorOffsets{0xE0,0xE4,0xE8,0xEC,0x100,0x104,0x108,0x10C,0x460,0x470,0x474,0x478,0x47C,
    0x450,0x454,0x458,0x45C,0x314,0x318,0x31C,0x320,0x324,0x328,0x32C,0x330,0x334,0x338,0x33C,0x340,0x344,0x348,0x34C,0x350,0x354,
    0x358,0x35C,0x360,0x364,0x368,0x36C,0x370,0x374,0x3AC};
template<class S,class Fn> inline void visitCompositor(S& o,Fn&& fn){
    unsigned k=0;for(unsigned i=0;i<4;++i)fn(k++,o.lookAt[i]);for(unsigned i=0;i<4;++i)fn(k++,o.eye[i]);fn(k++,o.lift);for(unsigned i=0;i<4;++i)fn(k++,o.lastProbeNormal[i]);
    fn(k++,o.shakeIndex);fn(k++,o.shakeAmplitude);fn(k++,o.shakePending);fn(k++,o.shakeWasPending);
    for(auto* a:{&o.shake.period1,&o.shake.target1,&o.shake.previous1,&o.shake.timer1,&o.shake.period2,&o.shake.target2,&o.shake.previous2,&o.shake.timer2})for(unsigned i=0;i<3;++i)fn(k++,(*a)[i]);
    fn(k++,o.shake.fadeTimer);fn(k++,o.shake.active);
}
inline std::array<uint32_t,43> compositorWords(const OriginalCameraCompositorState& o){
    std::array<uint32_t,43> w{};visitCompositor(o,[&](unsigned k,const auto& v){using T=std::remove_cv_t<std::remove_reference_t<decltype(v)>>;if constexpr(std::is_same_v<T,float>)w[k]=u(v);else w[k]=uint32_t(v);});return w;
}
inline void assignCompositorWords(OriginalCameraCompositorState& o,const uint32_t* w,const uint8_t* mask){
    visitCompositor(o,[&](unsigned k,auto& v){if(!mask[k])return;using T=std::remove_reference_t<decltype(v)>;if constexpr(std::is_same_v<T,float>)v=f(w[k]);else v=int32_t(w[k]);});
}
}
