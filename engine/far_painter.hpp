#pragma once
// Original SSX3 (PS2 USA, SLUS_207.72) world painter type 4: the camera far-clip cap, and the
// outer-camera far / texture-chunk streaming range derived from it.
// Data: tools/export_far_painter.py --location L -> web/public/assets/<L>/SECTIONS/far-painter.json.
// Verification: tools/test_far_painter.py (tests/far_painter_replay.cpp) against the
// local/ps2-capture/runs/sections/*-full captures.
//
// Painter object (0x10 bytes; factory 2C0408 case 4 -> ctor 2BC7A0, vtable 485218):
//   +0 driver distance (ctor -99999 = gp-42E8), +4 vtable, +8 current cap, +C last sample
//   (ctor both 30000 = gp-42EC).  Payload (2 floats): +0 rate, +4 far cap (cm).
//   slot 66 (+0x210) blend 2BCEA8: notify, w = weight*weight, current = w*value + (1-w)*current
//                   (EE mul.s, sub.s, mul.s, add.s in that order), sample = value, notify.
//   slot 67 (+0x218) notify 2BDA38: empty.
//   slot 68 (+0x220) compare 2BDB10: current == value (c.eq.s; the sample slot is not read).
//   slot 69 (+0x228) reset 2BE0F8: +0 (distance) = 0, current = 30000 (gp-42A8); sample kept.
//   slot 6  (+0x030) getter 2C14B8: returns +8.
// Environment blocks 4FA370 + view*F0 (view 0..5 = riders, 6/7 = cameras): block+0 is the far
// wrapper (wrapper+0 painter, wrapper+8/+C last X/Y, wrapper+0x10 = 0x404: section 4, type 4).
// 2EE3B8(view) = *(float*)(**(4FA370 + view*F0) + 8) = the current cap.
//
// Per camera frame, 15E668 (outer camera, `o`), after the eye/look-at gather, collision 15EE00,
// shake 15E460 and the final eye store o+0x20:
//   o+0/+4/+8 = weighted blend of the camera algorithms' fov/near/far (DEFAULT_3 alone: far 30000,
//   weight 1 -> exact copy);
//   15EBBC: 2ED490(o+0x18 + 6, x = o+0x20, y = o+0x24, weight -99999 (gp-6C6C)): each property
//   wrapper of camera block 6 runs the transition driver 2C0778 (engine/painter_driver.hpp) at
//   the FINAL camera eye X/Y (source Z-up cm; not the rider).  The region is the global current
//   location gp+0x770 (ARA1 8, BRA2 16, BHP1 15); a location whose painter record has no type-4
//   section takes the reset path every frame (cap 30000).
//   15EBD4: cap = 2EE3B8(o+0x18 + 6)
//   far1 = (o+0x10 <= o+8) ? min(o+8, cap) : o+0x10;   o+8 = far1; gp+0xA78 = far1
//   o+0 = (0 <= fov) ? min(fov, o+0xC) : 0
//   o+4 = (o+0x10 <= near) ? min(near, o+0x14) : o+0x10
//   o+8 = (o+0x10 <= far1) ? min(far1, o+0x14) : o+0x10   (o+0x10 = 30, o+0x14 = 30000)
// Later in the same frame 22E8B8 calls 15EC98(o) for each active camera (count = +0x10 of
// *(*(*(gp-0x848)+0x84)+0x84), 1 in a single-player race):
//   f = (count == 1) ? o+8 : ((0 <= o+8) ? min(o+8, 15000) : 0)
//   W+0x250 + o+0xB0*0x50 = 1 (viewer enabled); 3A9658(W+0x10, o+0xB0, eye = o+0x20, range = f*1.5)
//   (then o+0xB4 = 1 when 3A96E0 reports a chunk distance d with max(15000, min(f*2/3, 30000)) <= d
//   or f <= d; that flag does not feed the range).
// ChunkStreaming::frame (section_streaming.hpp) takes eye = o+0x20 and range = f*1.5 of the same frame.
//
// Timing evidence (sections captures): capture record T holds o+0x20, o+8 and the W+0x250 viewer
// (eye, range) of the same camera frame, and the countdown-anchor savestate's block-6 wrapper
// holds exactly record 0's eye as last X/Y: step(record T eye) -> record T far.
#include "painter_driver.hpp"
#include "painter_tree.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace ssx {
constexpr float kFarPainterDefault=30000.f;        // gp-42EC (ctor) / gp-42A8 (reset)
constexpr float kFarPainterSentinel=-99999.f;      // gp-42E8 (ctor distance), gp-6C6C (automatic weight)
constexpr float kCameraAlgorithmFar=30000.f;       // DEFAULT_3 far (engine/original_camera.hpp)
constexpr float kCameraNearMinimum=30.f,kCameraFarMaximum=30000.f;   // outer+0x10 / +0x14
constexpr float kSplitScreenStreamFar=15000.f;     // gp-6C68
constexpr float kStreamRangeScale=1.5f;            // lui 0x3FC0

struct OriginalFarPainterState {float current=kFarPainterDefault,sample=kFarPainterDefault;};   // +8, +C
// 2BCEA8
inline void originalFarPainterBlend(OriginalFarPainterState& s,float value,float weight){
    using namespace terrain_original;Rounding rounding;
    const float squared=mul(weight,weight),complement=originalScalarSubtract(1.f,squared);
    s.current=originalScalarAdd(mul(squared,value),mul(complement,s.current));s.sample=value;
}
// 2BDB10
inline bool originalFarPainterMatches(const OriginalFarPainterState& s,float value){return s.current==value;}
// 2BE0F8 (distance = painter+0, kept by the driver state in this port)
inline void originalFarPainterReset(OriginalFarPainterState& s,float& distance){distance=0;s.current=kFarPainterDefault;}

inline float originalFarMin(float a,float b){return a<b?a:b;}   // min.s on finite values
// 15EBE0..15EC54: returns o+8.  `gpA78` (optional) receives the intermediate far1 stored at gp+0xA78.
inline float originalCameraFarClip(float rawFar,float cap,float nearMinimum=kCameraNearMinimum,float farMaximum=kCameraFarMaximum,float* gpA78=nullptr){
    const float far1=(nearMinimum<=rawFar)?originalFarMin(rawFar,cap):nearMinimum;
    if(gpA78)*gpA78=far1;
    return (nearMinimum<=far1)?originalFarMin(far1,farMaximum):nearMinimum;
}
// 15EC98: far used for streaming (count = active cameras) and the viewer range 1.5*far (EE mul.s, chop).
inline float originalStreamingFar(float outerFar,uint32_t cameraCount=1){
    if(cameraCount==1)return outerFar;
    return (0.f<=outerFar)?originalFarMin(outerFar,kSplitScreenStreamFar):0.f;
}
inline float originalStreamingRange(float streamingFar){
    using namespace terrain_original;Rounding rounding;return mul(streamingFar,kStreamRangeScale);
}

// One camera's type-4 painter (block 6 for the human camera) with its section of the current location.
struct OriginalFarPainter {
    struct Payload {float rate=0,value=kFarPainterDefault;};
    std::vector<std::array<uint16_t,4>> nodes;std::vector<Payload> payloads;
    OriginalPainterTree tree;uint32_t outside=0xffffffffu;bool present=false;   // present = location authors type 4
    OriginalPainterDriverState driver;OriginalFarPainterState state;int selected=-1;unsigned steps=0;

    // 2BC7A0: the object is rebuilt with the location's painter set (race load); distance -99999.
    void construct(){driver={};state={};selected=-1;steps=0;}
    // Seed from a savestate (block-6 painter +0/+8/+C, wrapper +8/+C).
    void seed(float distance,float current,float sample,float lastX,float lastY){driver.distance=distance;driver.lastX=lastX;driver.lastY=lastY;state.current=current;state.sample=sample;}
    float cap() const{return state.current;}   // 2EE3B8
    // 2ED490 -> 2C0778 for the far wrapper at the final camera eye (source X/Y); returns 2EE3B8.
    float step(float x,float y){
        if(!std::isfinite(x)||!std::isfinite(y))throw std::runtime_error("Nonfinite far painter sample position");
        OriginalPainterDriverAccess access;
        access.sample=[this](float px,float py)->std::optional<OriginalPainterSample>{
            auto id=originalPainterPayload(tree,px,py,outside,payloads.size());selected=id?int(*id):-1;
            if(!id)return {};return OriginalPainterSample{4,payloads[*id].rate};};
        access.matches=[this](){return originalFarPainterMatches(state,payloads.at(size_t(selected)).value);};
        access.blend=[this](float weight){originalFarPainterBlend(state,payloads.at(size_t(selected)).value,weight);};
        access.reset=[this](){originalFarPainterReset(state,driver.distance);};
        originalPainterDriverStep(driver,x,y,4,present?OriginalPainterAvailability::Ready:OriginalPainterAvailability::MissingSection,kFarPainterSentinel,access);
        ++steps;return state.current;
    }
    // Whole camera-frame chain: cap -> outer+0x08 -> streaming range (single camera).
    struct Frame {float cap,far,range;};
    Frame frame(float eyeX,float eyeY,uint32_t cameraCount=1){
        const float c=step(eyeX,eyeY),f=originalCameraFarClip(kCameraAlgorithmFar,c);
        return {c,f,originalStreamingRange(originalStreamingFar(f,cameraCount))};
    }
};

// Loader for nlohmann::json documents written by tools/export_far_painter.py.
template<class Json> inline void loadFarPainter(const Json& doc,OriginalFarPainter& p){
    if(doc.at("version").template get<int>()!=1||doc.at("type").template get<int>()!=4)throw std::runtime_error("Unsupported far painter package");
    p=OriginalFarPainter{};
    const auto& j=doc.at("painter");
    if(j.is_null()){p.construct();return;}
    p.nodes=j.at("nodes").template get<std::vector<std::array<uint16_t,4>>>();
    for(const auto& row:j.at("payloads")){OriginalFarPainter::Payload q{row.at("rate").template get<float>(),row.at("far_cm").template get<float>()};
        if(!std::isfinite(q.rate)||!std::isfinite(q.value))throw std::runtime_error("Invalid far painter payload");p.payloads.push_back(q);}
    const auto origin=j.at("origin").template get<std::array<float,2>>();const float scale=j.at("scale").template get<float>();const unsigned root=j.at("root").template get<unsigned>();
    if(!std::isfinite(scale)||scale<=0||root>=p.nodes.size()||p.nodes.size()>32768||p.payloads.empty())throw std::runtime_error("Invalid far painter tree");
    for(const auto& n:p.nodes){
        if(n[0]&1){for(auto c:n)if((c>>1)>=p.nodes.size())throw std::runtime_error("Invalid far painter child");}
        else{const uint32_t id=uint32_t(n[2])|(uint32_t(n[3])<<16);if(id!=0xffffffffu&&id>=p.payloads.size())throw std::runtime_error("Invalid far painter leaf");}
    }
    const auto outside=j.at("outside_words").template get<std::array<uint32_t,2>>();
    if(outside[0]&1||(outside[1]!=0xffffffffu&&outside[1]>=p.payloads.size()))throw std::runtime_error("Invalid far painter outside leaf");
    p.tree={scale,origin[0],origin[1],uint16_t(root),p.nodes};p.outside=outside[1];p.present=true;p.construct();
}
}
