#pragma once
// Original SSX3 (PS2 USA, SLUS_207.72) spline path follower shared by the
// scripted set-piece modifiers (MultiSplineModifier trams 0x48F168,
// SplineModifier ravens/rockets 0x48F250). The paths are the authored SSB
// kind-8 spline records (the same records as the grind rails, see
// rail_motion.hpp): 144-byte segments linked by +0x60 previous / +0x64 next,
// +0x84 start distance, +0x0C length, cubic rows +0x10..+0x40 and the
// arc-length -> parameter cubic +0x50..+0x5C.
//
//   0x3451C0 bind(path, resource)  +0 resource, +4 cursor index 0, +8 cursor =
//                                   first segment (record +0x24), +0xC length
//   0x3454E8 length                 last.distance + last.length (walks +0x64)
//   0x345248 evaluate(path, d)      wrap d into [0,length), move the cursor
//                                   backwards then forwards, then 0x345048
//   0x345048 segment evaluation     s = d - start; t = ((a s + b) s + c) s + e,
//                                   dt/ds = (3a s + 2b) s + c; outputs
//                                   position M(t^3,t^2,t,1), first derivative
//                                   M(3t^2,2t,1,0) and M(6t,2,0,0) * dt/ds
//                                   (VU0 vmulax/vmadda chains).
// EE scalar arithmetic: MUL.S chop, ADD/SUB.S keep the guard bit, DIV.S rounds
// to nearest (PCSX2), CVT.W.S truncates; VU arithmetic rounds toward zero.
#include "rail_motion.hpp"
#include "original_rounding.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>

namespace ssx {
using SplineQuad=std::array<float,4>;

// Runtime path cursor (the 0x10-byte object at modifier +0x48 for MultiSpline).
struct OriginalSplinePath {
    uint32_t resource=0;       // +0x0 (rid<<8)|track
    int32_t index=0;           // +0x4 cursor segment index (0 = first segment)
    size_t cursor=0;           // +0x8 cursor segment (position in record.segments, chain order)
    float length=0;            // +0xC total length (0x3454E8)
    const OriginalRailRecord* record=nullptr;
};
struct OriginalSplineSample {SplineQuad position{},tangent{},curvature{};}; // a1, a2, a3 outputs of 0x345048

namespace spline_path_math {
// vmulax/vmadday/vmaddaz/vmaddw.xyzw with the segment rows (w components 0,0,0,1).
inline SplineQuad rows(const OriginalRailSegment& s,const SplineQuad& c){
    using namespace terrain_original;SplineQuad out;
    for(unsigned k=0;k<4;++k){
        const float r0=k<3?s.coefficients[0][k]:0.f,r1=k<3?s.coefficients[1][k]:0.f,r2=k<3?s.coefficients[2][k]:0.f,r3=k<3?s.coefficients[3][k]:1.f;
        out[k]=add(add(add(mul(r0,c[0]),mul(r1,c[1])),mul(r2,c[2])),mul(r3,c[3]));
    }
    return out;
}
}

// 0x3454E8: the record's segment chain is stored in order (rail_bridge validates the links).
inline float originalSplineLength(const OriginalRailRecord& record){
    if(record.segments.empty())throw std::runtime_error("Empty original spline");
    const auto& last=record.segments.back();
    OriginalRounding rounding;return originalScalarAdd(last.distance,last.length);
}
// 0x3451C0
inline OriginalSplinePath originalSplineBind(const OriginalRailRecord& record){
    OriginalSplinePath path;path.resource=record.packedId;path.record=&record;path.index=0;path.cursor=0;path.length=originalSplineLength(record);return path;
}
// 0x345048
inline OriginalSplineSample originalSplineSegment(const OriginalRailSegment& seg,float distance){
    using terrain_original::mul;OriginalRounding rounding;
    const float a=seg.arcToParameter[0],b=seg.arcToParameter[1],c=seg.arcToParameter[2],e=seg.arcToParameter[3];
    const float s=originalScalarSubtract(distance,seg.distance);
    const float b2=originalScalarAdd(b,b);
    float t=originalScalarAdd(mul(a,s),b);
    const float s3=mul(s,3.f);
    float slope=originalScalarAdd(mul(a,s3),b2);
    t=mul(t,s);t=originalScalarAdd(t,c);
    slope=mul(slope,s);slope=originalScalarAdd(slope,c);           // dt/ds
    t=mul(t,s);t=originalScalarAdd(t,e);
    const float t6=mul(t,6.f),tt=mul(t,t),t2=originalScalarAdd(t,t);
    OriginalSplineSample out;
    auto second=spline_path_math::rows(seg,{t6,2.f,0.f,0.f});
    for(auto& x:second)x=terrain_original::mul(x,slope);            // vmulx by qmtc2(dt/ds)
    out.curvature=second;
    const float ttt=mul(tt,t),tt3=mul(tt,3.f);
    out.position=spline_path_math::rows(seg,{ttt,tt,t,1.f});
    out.tangent=spline_path_math::rows(seg,{tt3,t2,1.f,0.f});
    return out;
}
// 0x345248
inline OriginalSplineSample originalSplineEvaluate(OriginalSplinePath& path,float distance){
    if(!path.record||path.record->segments.empty())throw std::runtime_error("Unbound original spline");
    const auto& segs=path.record->segments;
    {
        using terrain_original::mul;OriginalRounding rounding;
        const float length=path.length;
        const float q=collision_scalar::divide(distance,length);
        float whole=float(int32_t(q));                               // cvt.w.s (truncate) / cvt.s.w
        if(q<whole)whole=originalScalarSubtract(whole,1.f);
        whole=mul(whole,length);
        distance=originalScalarSubtract(distance,whole);
    }
    // backwards while d < cursor.start and a previous segment exists
    while(distance<segs[path.cursor].distance&&path.cursor>0){--path.cursor;--path.index;}
    // forwards while the next segment starts at or before d
    while(path.cursor+1<segs.size()&&segs[path.cursor+1].distance<=distance){++path.cursor;++path.index;}
    return originalSplineSegment(segs[path.cursor],distance);
}
}
