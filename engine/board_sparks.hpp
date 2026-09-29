#pragma once
// Original PS2 SSX3 (SLUS_207.72) rider board sparks: rider FX component RFX+0x470
// (constructor 2DA390 from 10FCD8, reset 2DAA78, per-tick update 2DABC8 from the rider-manager
// pass 128F50 / 111948, draw 2DB478 at layer priority 7). Header-only.
//
// Three effects share the component (docs/terrain-render-fidelity.md "Impact and contact effects"):
//  - sparks: a procedural 0x150-byte particle kernel ("Board Spark Emitter", texture 22 `sprk`)
//    uploaded whole to VU1 program 4 entry 0 by renderer slot 0x290 (380518). 30 slots x up to 8
//    trail sprites, GS ALPHA 0x48 (additive), no depth write, projected half size capped at 64 px.
//    The kernel follows the current emit point every tick (no birth history): the spray appears
//    and vanishes with the gate.
//  - glints: three `sprk` billboards (renderer slot 0x270 = 377CF0, colour 128, no size cap) on
//    about 30% of rail ticks.
//  - grind chunks: a generic snow-class emitter (3714B8, profile cmrender.h GrindParticlesFX with
//    TextureId 14 = tmb1..tmb8) fed on surface 9 (brown chunks) or on rails whose surface record
//    +0x88 is set (none on Snow Jam).
//
// Random streams: the update draws only the visual LCG gp+A0C (0x4A3AFC, OriginalSnowRandom):
// 2 draws when chunks are fed, then 1 glint gate draw (+12 when it passes) on rails. Chunk births
// draw the particle-birth stream 3177F0 (0x4FF018) inside OriginalSnowParticles::emit. The
// kernel seeds advance with the VU0 R register only (36D400), never a CPU stream. The gameplay
// generator 0x317810 is never used.
#include "snow_particles.hpp"
#include "terrain_contact_math.hpp"
#include "original_float.hpp"
#include <array>
#include <bit>
#include <vector>
namespace ssx {
using SparkVector4=std::array<float,4>;
// The 0x150-byte kernel (fields read by 36D428 and VU1 program 4 entry 0).
struct OriginalSparkKernel {
    int32_t slots=30,perSlot=8;                                     // +0x00 / +0x04
    float accumulator=0,period=0;                                   // +0x08 / +0x0C (scaled seconds)
    float sizeRange=0,lifeRange=0,sizeBase=0,lifeBase=0,sizeDelta=0; // +0x10 +0x14 +0x18 +0x1C +0x20
    float alphaStep=0,trailStep=0;                                  // +0x30 (1/perSlot) / +0x34 (sub*damp)
    std::array<uint32_t,9> seeds{};  // +38 +3C +40 +44 +48 +50 +54 +58 +5C
    float damping=0;                                                // +0x4C
    SparkVector4 force{},velocityBase{},velocityRange0{},velocityRange1{},velocityRange2{}; // +60 +70 +80 +90 +A0
    SparkVector4 base{},axisA{},axisB{},drift{};                    // +B0 +C0 +D0 +E0
    SparkVector4 colourBase{},colourRange0{},colourRange1{},colourSlope{}; // +F0 +100 +110 +120 (R,G,B,A)
};
enum OriginalSparkSeed:unsigned {Seed38,Seed3C,Seed40,Seed44,Seed48,Seed50,Seed54,Seed58,Seed5C};
// 36D400: rinit R, x / rnext.x -> the VU R-register LFSR (same step as snow_particles' next()).
inline uint32_t originalSparkSeedNext(uint32_t seed){
    const uint32_t state=(seed&0x7fffffu)|0x3f800000u;
    return (((state<<1)^((state>>4)&1)^((state>>22)&1))&0x7fffffu)|0x3f800000u;
}
// Live kernel words of the Snow Jam glide savestate (local/reference/pcsx2/snow-jam-glide.p2s,
// *(0x146F390+0x470+0x28) = 0x5DD200). The constructor constants (36CBF8/36CE00/36CE28 with
// N=30, per=8, force -3000, life 0.4, sub 0.012, damp 1.5, size 7.5/0/7.5, life 0.1..0.194) are
// taken from the dumped bit patterns so the EE/VU rounding of the setup cannot drift.
inline OriginalSparkKernel originalSparkKernelGlide(){
    OriginalSparkKernel k;
    k.accumulator=std::bit_cast<float>(0x3f170a37u);k.period=std::bit_cast<float>(0x3ca3d70au);
    k.sizeRange=7.5f;k.lifeRange=std::bit_cast<float>(0x3e10624eu);k.sizeBase=-3.75f;k.lifeBase=std::bit_cast<float>(0x3c1374b0u);k.sizeDelta=-7.5f;
    k.alphaStep=.125f;k.trailStep=std::bit_cast<float>(0x3c9374bcu);k.damping=1.5f;
    k.seeds={0x3fc27ae8u,0x3f8c5919u,0x3fa37537u,0x3f93c385u,0x3fcc8f49u,0x3feceb3du,0x3fd44fadu,0x3ffb3fd3u,0x3fe628d5u};
    k.force={0,0,std::bit_cast<float>(0xc4a6aaa9u),0};
    return k;
}
struct OriginalGrindChunkProfile {OriginalSnowParticleProfile particles;int textureId=14,flipFrames=8;float flipRate=27;};
// 2DA390's 0xD8 profile (cmrender.h Render_DynamicSpray_GrindParticlesFX; the ctor overwrites
// TextureId 5 with 14). BlendMode 1 = ALPHA enum 5 (GS 0x44), drawn by 371688(emitter, 7).
inline OriginalGrindChunkProfile originalGrindChunkProfile(){
    OriginalGrindChunkProfile g;auto& p=g.particles;
    p.particlesPerBirth=1;p.damping=1;p.size=6;p.sizeRange=6;p.finalSize=6;p.life=4;p.lifeRange=0;
    p.offset={};p.positionRange0={};p.positionRange1={};p.velocity={0,0,600};
    p.velocityRanges={{{200,0,0},{0,200,0},{0,0,100}}};p.force={0,0,-800};
    p.startColour={1,1,1,1};p.endColour={std::bit_cast<float>(0x3e5cc63fu),std::bit_cast<float>(0x3e3851ecu),std::bit_cast<float>(0x3e0c49bau),0};
    p.colourRange0={};p.colourRange1={};return g;
}
struct OriginalBoardSparksState {
    OriginalSparkKernel kernel=originalSparkKernelGlide();
    bool sparks=false,glints=false,chunks=false;        // +0x70 / +0x74 / +0x78
    SnowVector pointCm{};                                // +0x10
    std::array<SnowVector,3> glintCm{};                  // +0x30 / +0x40 / +0x50
    std::array<float,3> glintHalfCm{};                   // +0x60 / +0x64 / +0x68
    uint32_t unsupportedChunks=0;
};
struct OriginalBoardSparksInput {
    SnowVector velocityCmps{};            // rider+0x1E0
    SnowVector contactCm{};               // rider+0x460 (last board contact point; kept in the air)
    SnowVector rightAxis{};               // rider+0x1A0
    SnowVector boardRow0{},boardRow2{};   // scaled world matrix rows 0 and 2 of bone rider+0x8A4 (geometry+0x34)
    bool rail=false;                      // motion mode 4 (11FE98)
    int surface=0;                        // rider+0x438 (kept in the air)
    bool surfaceSparks=false,surfaceGrind=false; // surface record +0x8C / +0x88
    bool tricky=false;                    // (gp+163C == 0) && (gp+1638 != 0 || rider+0x2F0 > 0)
};
struct OriginalBoardSparksStep {OriginalSnowEmission chunk;};
namespace board_sparks_detail {
inline SnowVector scale3(const SnowVector& v,float s){using namespace terrain_original;return {mul(v[0],s),mul(v[1],s),mul(v[2],s)};}
inline SparkVector4 scale4(const SparkVector4& v,float s){using namespace terrain_original;return {mul(v[0],s),mul(v[1],s),mul(v[2],s),mul(v[3],s)};}
inline SparkVector4 add4(const SparkVector4& a,const SparkVector4& b){using namespace terrain_original;return {add(a[0],b[0]),add(a[1],b[1]),add(a[2],b[2]),add(a[3],b[3])};}
inline SparkVector4 sub4(const SparkVector4& a,const SparkVector4& b){using namespace terrain_original;return {sub(a[0],b[0]),sub(a[1],b[1]),sub(a[2],b[2]),sub(a[3],b[3])};}
inline SparkVector4 v4(const SnowVector& v,float w){return {v[0],v[1],v[2],w};}
}
// 36D428(kernel, dt): age accumulator, slot drift and seed advance.
inline void originalSparkKernelAdvance(OriginalSparkKernel& k,float dt){
    using namespace terrain_original;Rounding rounding;
    // EE FPU mul.s/add.s/sub.s (the scalar adder truncates the aligned operand: originalScalar*).
    k.accumulator=originalScalarAdd(k.accumulator,mul(dt,k.damping)); // 36D3E8
    while(mul(float(k.slots),k.period)<k.accumulator){
        k.accumulator=originalScalarSubtract(k.accumulator,k.period);
        k.base=board_sparks_detail::add4(k.base,k.drift);
        for(auto& s:k.seeds)s=originalSparkSeedNext(s);
    }
}
// 36D318(kernel, start, end, r0 = 0, r1 = 0, life = 0.194): colours on a 0..255 scale, (R,G,B,A).
inline void originalSparkKernelColour(OriginalSparkKernel& k,const SparkVector4& start,const SparkVector4& end,float life){
    using namespace board_sparks_detail;using namespace terrain_original;Rounding rounding;
    k.colourRange0={};k.colourRange1={};
    k.colourBase=sub4(start,scale4(add4(k.colourRange0,k.colourRange1),1.5f));
    const float q=div(1.f,mul(life,k.damping));k.colourSlope=scale4(sub4(end,start),q);
}
// 36D1F0(kernel, v, r0, r1, r2).
inline void originalSparkKernelVelocity(OriginalSparkKernel& k,const SparkVector4& v,const SparkVector4& r0,const SparkVector4& r1,const SparkVector4& r2){
    using namespace board_sparks_detail;using namespace terrain_original;Rounding rounding;
    const float q=div(1.f,k.damping);
    k.velocityRange0=scale4(r0,q);k.velocityRange1=scale4(r1,q);k.velocityRange2=scale4(r2,q);
    const auto sum=add4(add4(k.velocityRange0,k.velocityRange1),k.velocityRange2);
    k.velocityBase=sub4(scale4(v,q),scale4(sum,1.5f));
}
// 36CEF8(kernel, p, a, b): spawn box p +- a +- b.
inline void originalSparkKernelBox(OriginalSparkKernel& k,const SparkVector4& p,const SparkVector4& a,const SparkVector4& b){
    using namespace board_sparks_detail;terrain_original::Rounding rounding;
    k.base=sub4(p,scale4(add4(a,b),3.f));k.axisA=scale4(a,2.f);k.axisB=scale4(b,2.f);
}
// 2DABC8 up to (and including) the glints. The returned chunk request must be passed to the
// grind-chunk emitter (3717C0) by the caller, in this tick, before the next rider FX pass.
inline OriginalBoardSparksStep originalBoardSparksUpdate(OriginalBoardSparksState& s,const OriginalBoardSparksInput& in,OriginalSnowRandom& random){
    using namespace board_sparks_detail;using namespace terrain_original;Rounding rounding;
    OriginalBoardSparksStep out;
    originalSparkKernelAdvance(s.kernel,std::bit_cast<float>(0x3c888889u)); // 1.0f/(float)60 (div.s; 0x3C888888 drifts the age by an ulp)
    float square=mul(in.velocityCmps[0],in.velocityCmps[0]);square=add(square,mul(in.velocityCmps[1],in.velocityCmps[1]));
    square=add(square,mul(in.velocityCmps[2],in.velocityCmps[2]));square=add(square,0.f);const float speed=terrain_original::sqrt(square);
    const bool moving=std::bit_cast<float>(0x438ae38eu)<speed;          // 277.778
    s.sparks=s.glints=s.chunks=false;
    if(moving){
        if(in.surfaceSparks){s.sparks=true;if(in.rail)s.glints=true;}
        if((in.rail&&in.surfaceGrind)||in.surface==9)s.chunks=true;
    }
    const auto step=scale3(in.velocityCmps,std::bit_cast<float>(0x3c23d70au)); // 0.01
    for(unsigned k=0;k<3;k++)s.pointCm[k]=add(in.contactCm[k],step[k]);
    out.chunk.positionCm=s.pointCm;out.chunk.stepSeconds=std::bit_cast<float>(0x3c888889u);
    // Rail chunks on a +0x88 surface take their colour from the environment record
    // 0x4FA398 + rider[0x86C]*0xF0 with cap 0.6; no Snow Jam rail has such a surface, so that
    // path is not ported: it stays inactive and is counted.
    if(s.chunks&&in.surface!=9){s.chunks=false;++s.unsupportedChunks;}
    if(s.chunks){
        const SnowColour colour{std::bit_cast<float>(0x3e5cc63fu),std::bit_cast<float>(0x3e3851ecu),std::bit_cast<float>(0x3e0c49bau),1.f};
        float cap=std::bit_cast<float>(0x3dcccccdu);                         // 0.1 (surface 9)
        const auto sideways=scale3(scale3(in.rightAxis,speed),.25f);
        const float chance=mul(originalScalarDivide(speed,std::bit_cast<float>(0x44d05556u)),std::bit_cast<float>(0x3dcccccdu));
        const auto base=scale3(in.velocityCmps,std::bit_cast<float>(0x3f99999au)); // 1.2
        SnowVector ahead{};for(unsigned k=0;k<3;k++)ahead[k]=add(base[k],sideways[k]);
        const float u=random.next();const float twice=originalScalarAdd(u,u);
        const auto swing=scale3(sideways,twice);SnowVector velocity{};for(unsigned k=0;k<3;k++)velocity[k]=sub(ahead[k],swing[k]);
        if(chance<cap)cap=chance;
        out.chunk.active=random.next()<cap;out.chunk.velocityCmps=velocity;out.chunk.colour=colour;
    }
    if(s.sparks){
        const SparkVector4 start=in.tricky?SparkVector4{255,0,255,153}:SparkVector4{77,77,255,153};
        const SparkVector4 end=in.tricky?SparkVector4{255,0,255,102}:SparkVector4{255,51,51,102};
        originalSparkKernelColour(s.kernel,start,end,std::bit_cast<float>(0x3e46a7f0u));
        originalSparkKernelVelocity(s.kernel,{0,0,400,0},{600,0,0,0},{0,600,0,0},{0,0,600,0});
        const float along=in.rail?10.f:15.f,across=in.rail?10.f:90.f;
        originalSparkKernelBox(s.kernel,v4(s.pointCm,1),scale4(v4(in.boardRow2,0),along),scale4(v4(in.boardRow0,0),across));
    }
    if(s.glints){
        if(!(std::bit_cast<float>(0x3f333333u)<random.next()))s.glints=false;
        else for(unsigned i=0;i<3;i++){
            s.glintHalfCm[i]=originalScalarAdd(mul(random.next(),10.f),20.f);
            SnowVector jitter{};for(unsigned k=0;k<3;k++)jitter[k]=originalScalarAdd(mul(random.next(),8.f),-4.f);
            for(unsigned k=0;k<3;k++)s.glintCm[i][k]=add(s.pointCm[k],jitter[k]);   // vadd
        }
    }
    return out;
}
// One drawn spark sprite before projection. VU program 4 drops a sprite whose centre is outside
// the clip volume and then does not step that slot's alpha fade, so fade stepping is the
// renderer's job: alpha = alpha0 * (1 - drawnInSlot/8), drawnInSlot counting accepted sprites.
struct OriginalSparkSprite {SnowVector positionCm{};float halfExtentCm=0;std::array<float,3> rgb{};float alpha0=0;int slot=0;};
// VU1 program 4 entry 0 (the kernel as uploaded by 380518), evaluated in world space: the
// program transforms every vector by the view-projection first, which is linear.
inline std::vector<OriginalSparkSprite> originalSparkSprites(const OriginalSparkKernel& k){
    using namespace board_sparks_detail;using namespace terrain_original;Rounding rounding;
    std::vector<OriginalSparkSprite> out;
    auto r=k.seeds;auto adv=[&](unsigned i){r[i]=originalSparkSeedNext(r[i]);return std::bit_cast<float>(r[i]);};
    float age=k.accumulator,slotIndex=0;
    const float polyA=std::bit_cast<float>(0xbf3ae148u),polyB=std::bit_cast<float>(0x3de76c8bu),tMax=std::bit_cast<float>(0x402ccccdu);
    for(int slot=0;slot<k.slots;slot++){
        const float s5C=adv(Seed5C),s40=adv(Seed40),s50=adv(Seed50),s44=adv(Seed44),s48=adv(Seed48),s38=adv(Seed38),s3C=adv(Seed3C);
        const float life=add(k.lifeBase,mul(k.lifeRange,s5C));
        SparkVector4 p0=add4(add4(add4(k.base,scale4(k.drift,slotIndex)),scale4(k.axisA,s38)),scale4(k.axisB,s3C));
        SparkVector4 v=add4(add4(add4(k.velocityBase,scale4(k.velocityRange0,s40)),scale4(k.velocityRange1,s44)),scale4(k.velocityRange2,s48));
        const float s54=adv(Seed54),s58=adv(Seed58); // both advance on dead slots too (0x210..0x238, branch delay slot)
        if(age<life){
            SparkVector4 colour=add4(add4(add4(k.colourBase,scale4(k.colourRange0,s50)),scale4(k.colourSlope,age)),scale4(k.colourRange1,s54));
            const auto fv=sub4(k.force,v);
            float size=mul(mul(k.sizeDelta,age),div(1.f,life));size=add(size,k.sizeBase);size=add(size,mul(k.sizeRange,s58));
            std::array<float,3> rgb{};for(unsigned c=0;c<3;c++)rgb[c]=std::min(std::max(colour[c],0.f),255.f);
            float t=age;
            for(int j=0;j<k.perSlot;j++){
                const float t2=std::min(t,tMax),poly=add(mul(t2,polyA),mul(mul(t2,t2),polyB));
                const auto pos=add4(add4(p0,scale4(k.force,t)),scale4(fv,poly));
                out.push_back({{pos[0],pos[1],pos[2]},std::abs(size),rgb,colour[3],slot});
                t=sub(t,k.trailStep);if(t<0)break;
            }
        }
        age=sub(age,k.period);slotIndex=add(slotIndex,1.f);if(age<0)break;
    }
    return out;
}
}
