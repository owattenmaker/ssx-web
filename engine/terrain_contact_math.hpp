#pragma once
#include <array>
#include "original_float.hpp"
#include <algorithm>
#include <cfenv>
#include <cmath>
#include <stdexcept>

namespace ssx::terrain_original {
using Vector=std::array<float,3>;
using Coefficients=std::array<Vector,16>;
// Development-recovered source-space arithmetic, independent of console runtime.
// Source coordinates are Z-up centimeters. Each helper preserves the separate
// multiply/add instructions in PS2 0x32E9A0 instead of contracting them to FMA.
inline float add(float a,float b){
#if defined(__EMSCRIPTEN__)
#if SSX_PS2_EXACT_FPU
 if(software_float::exactArithmetic)return software_float::add(a,b);
#endif
 if(originalRoundingMode==FE_TOWARDZERO)return software_float::add(a,b);
 return software_float::eeFlush(software_float::eeFlush(a)+software_float::eeFlush(b)); // no excess precision in wasm
#else
#if SSX_PS2_EXACT_FPU
 if(software_float::exactArithmetic)return software_float::add(a,b);
#endif
 volatile float r=software_float::eeFlush(software_float::eeFlush(a)+software_float::eeFlush(b));return software_float::eeFlush(r);
#endif
}
inline float sub(float a,float b){
#if defined(__EMSCRIPTEN__)
#if SSX_PS2_EXACT_FPU
 if(software_float::exactArithmetic)return software_float::sub(a,b);
#endif
 if(originalRoundingMode==FE_TOWARDZERO)return software_float::sub(a,b);
 return software_float::eeFlush(software_float::eeFlush(a)-software_float::eeFlush(b)); // no excess precision in wasm
#else
#if SSX_PS2_EXACT_FPU
 if(software_float::exactArithmetic)return software_float::sub(a,b);
#endif
 volatile float r=software_float::eeFlush(software_float::eeFlush(a)-software_float::eeFlush(b));return software_float::eeFlush(r);
#endif
}
inline float mul(float a,float b){
#if defined(__EMSCRIPTEN__)
#if SSX_PS2_EXACT_FPU
 if(software_float::exactArithmetic)return software_float::mul(a,b);
#endif
 if(originalRoundingMode==FE_TOWARDZERO)return software_float::mul(a,b);
 return software_float::eeFlush(software_float::eeFlush(a)*software_float::eeFlush(b)); // no excess precision in wasm
#else
#if SSX_PS2_EXACT_FPU
 if(software_float::exactArithmetic)return software_float::mul(a,b);
#endif
 volatile float r=software_float::eeFlush(software_float::eeFlush(a)*software_float::eeFlush(b));return software_float::eeFlush(r);
#endif
}
inline float div(float a,float b){
#if defined(__EMSCRIPTEN__)
#if SSX_PS2_EXACT_FPU
 if(software_float::exactArithmetic)return software_float::div(a,b);
#endif
 if(originalRoundingMode==FE_TOWARDZERO)return software_float::div(a,b);
 return software_float::eeFlush(software_float::eeFlush(a)/software_float::eeFlush(b)); // no excess precision in wasm
#else
#if SSX_PS2_EXACT_FPU
 if(software_float::exactArithmetic)return software_float::div(a,b);
#endif
 volatile float r=software_float::eeFlush(software_float::eeFlush(a)/software_float::eeFlush(b));return software_float::eeFlush(r);
#endif
}
inline float sum3(float a,float b,float c){return add(add(a,b),c);}
inline float sqrt(float a){
#if defined(__EMSCRIPTEN__)
#if SSX_PS2_EXACT_FPU
 if(software_float::exactArithmetic)return software_float::sqrt(a);
#endif
 if(originalRoundingMode==FE_TOWARDZERO)return software_float::sqrt(a);
 return std::sqrt(a);
#else
#if SSX_PS2_EXACT_FPU
 if(software_float::exactArithmetic)return software_float::sqrt(a);
#endif
 volatile float r=std::sqrt(a);return r;
#endif
}
using Rounding=OriginalRounding;
struct Evaluation {Vector point,du,dv;};
inline Evaluation evaluate(const Coefficients& c,float u,float v) {
    float u2=mul(u,u),v2=mul(v,v);
    std::array<float,4> up{mul(u,u2),u2,u,1},vp{mul(v,v2),v2,v,1};
    std::array<float,4> dup{mul(u2,3),add(u,u),1,0},dvp{mul(v2,3),add(v,v),1,0};
    auto dot4=[](const std::array<float,4>& a,const std::array<float,4>& b) {
        return add(add(add(mul(a[0],b[0]),mul(a[1],b[1])),mul(a[2],b[2])),mul(a[3],b[3]));
    };
    // The point and partials are VU0 horizontal sums (0x32EB24 style): vmul a x b, vadda x + y, vmadda 1.0 x z, vmadd 1.0 x w.
    // The vmul operand order per output: point = row x vp (0x32EB28), du = vp x derivative (0x32EDA8), dv = dvp x row (0x32EC28).
    // row and derivative stay the plain chain (0x32EA24: vmulax / vmadday / vmaddaz / vmaddw, columns x p).
    auto dot4h=[](const std::array<float,4>& a,const std::array<float,4>& b) {
        return add(add(add(mul(a[0],b[0]),mul(a[1],b[1])),mul(1.f,mul(a[2],b[2]))),mul(1.f,mul(a[3],b[3])));
    };
    Evaluation e;
    for(unsigned k=0;k<3;++k) {
        std::array<float,4> row,derivative;
        for(unsigned j=0;j<4;++j) {
            std::array<float,4> values;
            for(unsigned i=0;i<4;++i)values[i]=c[(3-j)*4+3-i][k];
            row[j]=dot4(values,up);derivative[j]=dot4(values,dup);
        }
        e.point[k]=dot4h(row,vp);e.du[k]=dot4h(vp,derivative);e.dv[k]=dot4h(dvp,row);
    }
    return e;
}
// Original grid basis table at EE 0x43CCB8, used by VU0 program 0..0x270.
// Columns are separately authored/rounded u^3,u^2,u,1, not recomputed powers.
inline std::array<Vector,100> coarseGrid(const Coefficients& c) {
    static constexpr float powers[10][4]={
        {0x0.0p+0f,0x0.0p+0f,0x0.0p+0f,0x1.0000000000000p+0f},
        {0x1.67980e0000000p-10f,0x1.948b100000000p-7f,0x1.c71c720000000p-4f,0x1.0000000000000p+0f},
        {0x1.67980e0000000p-7f,0x1.948b100000000p-5f,0x1.c71c720000000p-3f,0x1.0000000000000p+0f},
        {0x1.2f684c0000000p-5f,0x1.c71c720000000p-4f,0x1.5555540000000p-2f,0x1.0000000000000p+0f},
        {0x1.67980e0000000p-4f,0x1.948b100000000p-3f,0x1.c71c720000000p-2f,0x1.0000000000000p+0f},
        {0x1.5f2a7e0000000p-3f,0x1.3c0ca40000000p-2f,0x1.1c71c60000000p-1f,0x1.0000000000000p+0f},
        {0x1.2f684c0000000p-2f,0x1.c71c720000000p-2f,0x1.5555540000000p-1f,0x1.0000000000000p+0f},
        {0x1.e1ccba0000000p-2f,0x1.35ba780000000p-1f,0x1.8e38e20000000p-1f,0x1.0000000000000p+0f},
        {0x1.67980e0000000p-1f,0x1.948b100000000p-1f,0x1.c71c720000000p-1f,0x1.0000000000000p+0f},
        {0x1.0000000000000p+0f,0x1.0000000000000p+0f,0x1.0000000000000p+0f,0x1.0000000000000p+0f},
    };
    Rounding rounding;std::array<Vector,100> grid;
    for(unsigned u=0;u<10;++u)for(unsigned v=0;v<10;++v)for(unsigned k=0;k<3;++k) {
        std::array<float,4> row;
        for(unsigned j=0;j<4;++j) {
            auto base=(3-j)*4;
            row[j]=add(add(add(mul(c[base+3][k],powers[u][0]),mul(c[base+2][k],powers[u][1])),mul(c[base+1][k],powers[u][2])),mul(c[base][k],powers[u][3]));
        }
        grid[u*10+v][k]=add(add(add(mul(row[0],powers[v][0]),mul(row[1],powers[v][1])),mul(row[2],powers[v][2])),mul(row[3],powers[v][3]));
    }
    return grid;
}

struct RefinedContact {bool valid=false;float u=0,v=0;Vector point{},normal{};};
inline RefinedContact refine(const Coefficients& c,Vector origin,Vector direction,float u,float v) {
    Rounding rounding;
    auto e=evaluate(c,u,v);
    auto scalarSum3=[](float a,float b,float d){return originalScalarAdd(originalScalarAdd(a,b),d);};
    // Original query uses a cell-center seed, then exactly four Newton updates.
    // The third solved component is ray fraction; only U/V are retained.
    for(unsigned step=0;step<4;++step) {
        auto a=e.du,b=e.dv;Vector d{-direction[0],-direction[1],-direction[2]};
        Vector first{originalScalarSubtract(mul(b[1],d[2]),mul(b[2],d[1])),originalScalarSubtract(mul(d[0],b[2]),mul(d[2],b[0])),originalScalarSubtract(mul(b[0],d[1]),mul(b[1],d[0]))};
        Vector second{originalScalarSubtract(mul(d[1],a[2]),mul(d[2],a[1])),originalScalarSubtract(mul(a[0],d[2]),mul(a[2],d[0])),originalScalarSubtract(mul(d[0],a[1]),mul(d[1],a[0]))};
        float thirdX=originalScalarSubtract(mul(a[1],b[2]),mul(a[2],b[1]));
        float inverse=originalScalarDivide(1,scalarSum3(mul(a[0],first[0]),mul(b[0],second[0]),mul(d[0],thirdX)));
        Vector residual;for(unsigned k=0;k<3;++k)residual[k]=sub(origin[k],e.point[k]);
        for(unsigned k=0;k<3;++k){first[k]=mul(first[k],inverse);second[k]=mul(second[k],inverse);}
        u=originalScalarAdd(u,scalarSum3(mul(first[0],residual[0]),mul(first[1],residual[1]),mul(first[2],residual[2])));
        v=originalScalarAdd(v,scalarSum3(mul(second[0],residual[0]),mul(second[1],residual[1]),mul(second[2],residual[2])));
        if(!std::isfinite(u)||!std::isfinite(v)||u<0||u>1||v<0||v>1)return {false,u,v,{},{}};
        e=evaluate(c,u,v);
    }
    // 0x32F5E0..0x32F618: vopmula dv, du then vopmsub du, dv (fs = du), and the horizontal length with 1.0 x z
    Vector n{sub(mul(e.dv[1],e.du[2]),mul(e.du[1],e.dv[2])),sub(mul(e.dv[2],e.du[0]),mul(e.du[2],e.dv[0])),sub(mul(e.dv[0],e.du[1]),mul(e.du[0],e.dv[1]))};
    float length=sqrt(add(add(mul(n[0],n[0]),mul(n[1],n[1])),mul(1.f,mul(n[2],n[2]))));
    float scale=div(1,length);for(auto& x:n)x=mul(x,scale);
    return {true,u,v,e.point,n};
}
inline Vector difference(Vector a,Vector b) {for(unsigned k=0;k<3;++k)a[k]=sub(a[k],b[k]);return a;}
// 0x32BD1C cross: vopmsub's fs is the second vector, so the subtracted products are b x a. 0x32BD30 dot (and the other VU0
// horizontal dots): vadda x + y, then vmadda 1.0 (vf0w) x z.
inline Vector cross(Vector a,Vector b) {return {sub(mul(a[1],b[2]),mul(b[1],a[2])),sub(mul(a[2],b[0]),mul(b[2],a[0])),sub(mul(a[0],b[1]),mul(b[0],a[1]))};}
inline float dot(Vector a,Vector b){return add(add(mul(a[0],b[0]),mul(a[1],b[1])),mul(1.f,mul(a[2],b[2])));}
struct GroundProbe {
    Vector origin{},direction{};
    float preferredFraction=.5f;
    Vector center{};
    // 32E100 computes bounds from the two raw endpoints, before delta rounding.
    Vector explicitEnd{};bool hasExplicitEnd=false;
};
// Cruise ground selector at 0x13D42C prefers a normal whose dot with the
// previous normal is >=0.3 before comparing coarse ray fractions (then RID).
inline bool preferGroundCandidate(bool haveCurrent,bool currentAligned,float currentMetric,unsigned currentResource,
                                  bool aligned,float metric,unsigned resource) {
    if(!haveCurrent)return true;
    if(currentAligned!=aligned)return aligned;
    return metric<currentMetric||(metric==currentMetric&&resource<currentResource);
}
struct CoarseContact {bool hit=false;float fraction=0;Vector point{},normal{};unsigned cellU=0,cellV=0,half=0;};
struct ContactCache {bool valid=false;unsigned resource=0,cellU=0,cellV=0,half=0;};
// Rider query scope (rider+0x860): 120E50 rebuilds it with 332DB8 from the rider query bounds
// rider+0x400/+0x410 (11E150), and the rider's world queries (3342D0: landing probe 13A7B0, ...)
// only visit its lists. 332DB8 keeps a terrain patch (node+0x24 list, box patch+0x158/+0x164)
// or an inline-box instance (3309D8, instance flag 0x20, box +0x60/+0x6C) when
// scope.low <= item.high and item.low <= scope.high on every axis (inclusive c.le.s).
struct RiderScope {
    Vector low{},high{};
    bool admits(const Vector& itemLow,const Vector& itemHigh) const {
        for(unsigned k=0;k<3;++k)if(!(low[k]<=itemHigh[k]&&itemLow[k]<=high[k]))return false;
        return true;
    }
};
inline bool pointInTriangle(Vector a,Vector b,Vector c,Vector point) {
    auto edge1=difference(b,a),edge2=difference(c,a),relative=difference(point,a);
    auto triangleNormal=cross(edge1,edge2),projector=cross(triangleNormal,edge2);
    float area=dot(edge1,projector);
    if(std::abs(area)<=1.000000013351432e-10f)return false;
    float reciprocal=div(1,area);
    float u=mul(dot(relative,projector),reciprocal);
    float v=mul(dot(triangleNormal,cross(relative,edge1)),reciprocal);
    if(u<0||v<0||sub(u,1)>=0||add(sub(u,1),v)>=0)return false;
    return true;
}
// Original 0x32E4D0 ray-plane test and VU0 0x6E0 barycentric test.
inline CoarseContact triangleContact(const GroundProbe& probe,Vector a,Vector b,Vector c) {
    Vector normal=cross(difference(a,b),difference(b,c));
    float inverse=div(1,sqrt(dot(normal,normal)));
    if(!std::isfinite(inverse)||inverse>10)return {};
    for(auto& x:normal)x=mul(x,inverse);
    float denominator=dot(probe.direction,normal);
    if(std::abs(denominator)<1.000000013351432e-10f)return {};
    float fraction=originalScalarDivide(dot(difference(a,probe.origin),normal),denominator);
    if(!std::isfinite(fraction)||fraction<0||fraction>1)return {};
    Vector point;for(unsigned k=0;k<3;++k)point[k]=add(probe.origin[k],mul(probe.direction[k],fraction));
    if(!pointInTriangle(a,b,c,point))return {};
    return {true,fraction,point,normal};
}
inline CoarseContact coarseContact(const std::array<Vector,100>& grid,const GroundProbe& probe,const ContactCache* previous=nullptr) {
    Rounding rounding;
    if(previous&&previous->valid&&previous->cellU<9&&previous->cellV<9) {
        unsigned u=previous->cellU,v=previous->cellV;
        auto a=grid[u*10+v],b=grid[(u+1)*10+v],c=grid[u*10+v+1],d=grid[(u+1)*10+v+1];
        auto hit=previous->half?triangleContact(probe,d,b,c):triangleContact(probe,c,b,a);
        if(hit.hit){hit.cellU=u;hit.cellV=v;hit.half=previous->half;return hit;}
    }
    for(unsigned u=0;u<9;++u)for(unsigned v=0;v<9;++v) {
        auto a=grid[u*10+v],b=grid[(u+1)*10+v],c=grid[u*10+v+1],d=grid[(u+1)*10+v+1];
        bool overlaps=true;
        for(unsigned k=0;k<3;++k) {
            float end=probe.hasExplicitEnd?probe.explicitEnd[k]:add(probe.origin[k],probe.direction[k]);
            float low=std::min(std::min(a[k],b[k]),std::min(c[k],d[k])),high=std::max(std::max(a[k],b[k]),std::max(c[k],d[k]));
            if(!(low<std::max(probe.origin[k],end)&&std::min(probe.origin[k],end)<high))overlaps=false;
        }
        if(!overlaps)continue;
        auto hit=triangleContact(probe,d,b,c);unsigned half=1;
        if(!hit.hit){hit=triangleContact(probe,c,b,a);half=0;}
        if(hit.hit){hit.cellU=u;hit.cellV=v;hit.half=half;return hit;}
    }
    return {};
}
inline GroundProbe groundProbe(Vector position,Vector previousNormal,Vector previousLateral,float filteredTurn,float geometryScale) {
    Rounding rounding;
    float offset=mul(geometryScale,mul(filteredTurn,45));GroundProbe result;
    for(unsigned k=0;k<3;++k) {
        float center=add(position[k],mul(previousLateral[k],offset));result.center[k]=center;
        result.origin[k]=add(center,mul(previousNormal[k],-100));
        float end=add(center,mul(previousNormal[k],200));result.direction[k]=sub(end,result.origin[k]);
    }
    return result;
}
}
