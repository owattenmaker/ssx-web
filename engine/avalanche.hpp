#pragma once
// Original avalanche / rock-slide playback (docs/avalanche.md): retail SSX 3 plays back tumbler motion recorded in the
// location's SSB kind-22 record, bound to the groups the stage defines (builtin 93 -> 0x2D92D0; data exported by
// tools/export_avalanches.py). Ported instruction by instruction:
//   0x2D97A8 trigger (builtin 94)         originalAvalancheTrigger
//   0x2D7E60 tumbler array                 (inside the trigger)
//   0x2D7EF8 per-tick slot update          originalAvalancheSlotTick (t += gp+0x974 = 0.009)
//   0x2D7CA8 tumbler update                originalTumblerUpdate
//   0x2D5778 integration                   originalTumblerIntegrate (position s8 x 2 per sample, axis-angle rotation per
//                                          sample, the lerped last step and colour; the emitter catch-up points are not
//                                          ported: presentation only, they do not touch the tumbler state)
//   0x2D7C00 envelope                      originalTumblerEnvelope
//   0x2D7DD8 release, 0x2D81B0 release all originalTumblerRelease / originalAvalancheSlotRelease
//   0x2D9C00 AvaSpline matrix              originalAvalancheInstanceMatrix (modifier 0x48F338 via 0x357820 -> 0x2D1CF0)
//   0x2D9CB0 / 0x2D9D68 snapshot save / restore (the race replay, cReplay 0x26D818 / 0x26DBF0)
//                                          originalAvalancheSave / originalAvalancheRestore
// Float policy: EE mul.s / add.s / sub.s chop (terrain_original, originalScalarAdd/Subtract), div.s / sqrt.s (sqrt.s reads
// ft) nearest as PCSX2 (collision_scalar), cvt.w.s truncates; VU vadd / vmulx chop; the 4x4 products are
// vmulax/vmadday/vmaddaz/vmaddw chains ((R0*c.x + R1*c.y) + R2*c.z) + R3*c.w. 0x31BE50 = collision_scalar::sincos.
// Row 3 of the per-sample rotation is (0,0,0,1): the original takes it from a stack temp that holds 0x4FF130 there (PS2
// much-2-much-full: every tumbler's +0x50 / +0xA0 row is (0,0,0,1)).
#include "collision_scalar.hpp"
#include "terrain_contact_math.hpp"
#include "set_piece_particles.hpp"
#include "original_random.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

namespace ssx {
using AvaQuad=std::array<float,4>;
using AvaMatrix=std::array<AvaQuad,4>;
constexpr AvaMatrix avalancheIdentity(){return {AvaQuad{1,0,0,0},AvaQuad{0,1,0,0},AvaQuad{0,0,1,0},AvaQuad{0,0,0,1}};}

// Group record 0x120 bytes (0x2D92D0): the words as the ctor stores them.
struct OriginalAvalancheGroup {
    uint32_t resource=0;              // +248 instance (resource)
    uint16_t type=1;                  // +240: 2 persistent (key 5), 0 (key 2) / 1 otherwise
    bool hasEmitter=false;            // +242 (builtin 96 on the instance)
    float duration=0,fadeIn=0,fadeOut=0,speed=1; // +256 dur x 30, +260 fin x 30, +264 fout x 30, +268 speed
    AvaQuad start{0,0,0,1};           // +224
    std::vector<uint8_t> samples;     // +252: trunc(dur) samples of 10 bytes
    bool avaSpline=false;             // a builtin-95 AvaSpline follows it
    std::array<uint32_t,54> emitterWords{}; // +0..+0xD7: the builtin-96 block (builtin-16 particle parameters) when hasEmitter
};
struct OriginalAvalancheSound {uint16_t tick=0,tumbler=0;}; // +272 {u16 tick, u16 tumbler index} (0x2DA028 swaps the record's)
struct OriginalAvalancheDef {int32_t id=0;std::vector<OriginalAvalancheGroup> groups;std::vector<OriginalAvalancheSound> sounds;};

// Tumbler (0x2F0 bytes, pool of 64 at 0x4EE770).
struct OriginalTumbler {
    int32_t sample=0;                  // +0
    AvaQuad integrated{};              // +16
    AvaMatrix rotationIntegrated=avalancheIdentity(); // +32
    AvaQuad position{};                // +96
    AvaMatrix rotation=avalancheIdentity(); // +112
    float scale=1,alpha=0;             // +176 / +180
    AvaQuad colour{1,1,1,1};           // +192
    int32_t avalanche=-1,group=-1;     // +736 group (-1: free)
    int32_t next=-1;                   // +740 list link (index in the pool, -1 end)
    bool emitterActive=false;          // +720
};
struct OriginalAvalancheSlot {
    int32_t avalanche=-1;              // +0 (-1 free)
    int32_t list=-1;                   // +4 tumbler list head (pool index)
    std::vector<int32_t> array;        // +8 tumbler array (list order; -1 released)
    int32_t eventCursor=0,soundCursor=0; // +16 / +20
    float t=0;                         // +24
};
// The trails: every pool tumbler holds a colour dynamic emitter at +0xD0 (0x208 bytes, vtable 0x4930D0, the class of the rider snow;
// constructed once with the pool, 0x3714B8 = 0x370B60 + vtable, +0x200 = +0x204 = 0). It persists with the pool: the kernel
// seeds 1..8, which 0x370058 does not write for a seed >= 0, keep their contents (0 from the construction).
struct OriginalAvalancheEmitter {
    OriginalDynamicEmitter base;                  // +0x000..+0x1FF (rings: base.ringPosition / ringVelocity, capacity +0x178)
    uint32_t word200=0;                           // +0x200: 1 once an emitter group took the tumbler (the trigger's +720)
    std::vector<std::array<uint8_t,4>> colours;   // *(+0x204): the ring's RGBA bytes (0x371548 "DynEmitterData Colours")
    bool constructed=false;                       // 0x3714B8 applied (lazily: the world stays constant-initialisable for RIDER_LOCAL)
};
// 0x3714B8 (0x370B60: +0xC..+0x14 0, capacity -1, enabled 1, not allocated, cursor / rings / active 0, +0x1F4 = -1, +0x1E4..+0x1F3
// = 0; vtable 0x4930D0, +0x200 = +0x204 = 0), once.
inline void originalAvalancheEmitterConstruct(OriginalAvalancheEmitter& e){
    if(e.constructed)return;e.constructed=true;
    using namespace dynamic_emitter;auto& b=e.base;
    b.setU(FlipCount,0);b.setU(FlipPhase,0);b.setU(FlipRate,0);b.setI(Capacity,-1);b.setU(Enabled,1);b.setU(Allocated,0);b.setU(Cursor,0);
    b.setU(RingA,0);b.setU(RingB,0);b.setU(ActiveCount,0);b.setU(Packet+0x10,0xFFFFFFFFu);for(unsigned o=Packet;o<Packet+0x10;o+=4)b.setU(o,0);
    b.setU(Vtable,0x4930D0u);e.word200=0;
}
struct OriginalAvalancheWorld {
    std::vector<OriginalAvalancheDef> defs;     // the list gp+2496 (newest first)
    std::array<OriginalAvalancheSlot,16> slots; // 0x538938
    std::array<OriginalTumbler,64> pool;        // 0x4EE770
    int32_t activeCount=0;                      // *(... +0xA8)+0x708
    // 0x29DEF0(audio, 1 / 0): the loop refcount (audio+0x6040: 0 -> 1 starts the rumble loop, back to 0 stops it). The sound
    // cues of the record (0x29E560 at a tumbler's +96) are dead in retail: 0x29E560 is jr ra (the cursor still advances).
    int32_t loopRefs=0;std::vector<int32_t> loopTransitions; // the refcount after each change (drained by the host; at most 64 kept)
    std::array<OriginalAvalancheEmitter,64> emitters;          // tumbler +0xD0 (parallel to pool; not reset by a trigger)
};

namespace avalanche_detail {
using namespace terrain_original;
inline float sadd(float a,float b){return originalScalarAdd(a,b);}
inline float ssub(float a,float b){return originalScalarSubtract(a,b);}
inline float sdiv(float a,float b){return collision_scalar::divide(a,b);}
inline float ssqrt(float a){return collision_scalar::squareRoot(a);}
inline int32_t trunc(float x){return int32_t(x);} // cvt.w.s (the recorded magnitudes stay far from the saturation)
inline AvaQuad transform(const AvaMatrix& r,const AvaQuad& c){AvaQuad out;for(unsigned k=0;k<4;++k)out[k]=add(add(add(mul(r[0][k],c[0]),mul(r[1][k],c[1])),mul(r[2][k],c[2])),mul(r[3][k],c[3]));return out;}
// 0x2D5C84.. / 0x2D60D0..: the axis-angle matrix (axis (x,y,z), sine s, cosine c), rows 0..2 then (0,0,0,1).
inline AvaMatrix axisAngle(float x,float y,float z,float s,float c){
    const float one=1.f;
    float f1=ssub(one,c);
    const float f2=mul(f1,z),f4=mul(f1,x),f1y=mul(f1,y);
    const float f13=mul(s,z),f12=mul(s,x);
    const float f9=mul(f2,z),f10=mul(f4,z),f5=mul(f2,x),f6=mul(f4,x),f3=mul(f1y,z),f0=mul(f1y,x),f8=mul(s,y),f2y=mul(f2,y),f4y=mul(f4,y),f1yy=mul(f1y,y);
    AvaMatrix m{};
    m[0]={sadd(f6,c),ssub(f0,f13),sadd(f5,f8),0.f};
    m[1]={sadd(f4y,f13),sadd(f1yy,c),ssub(f2y,f12),0.f};
    m[2]={ssub(f10,f8),sadd(f3,f12),sadd(f9,c),0.f};
    m[3]={0.f,0.f,0.f,1.f};
    return m;
}
inline const uint8_t* sampleAt(const OriginalAvalancheGroup& g,int32_t index){return g.samples.data()+size_t(index)*10;}
}

// 0x2D7C00 (t: the group time t x speed).
inline void originalTumblerEnvelope(OriginalTumbler& tu,const OriginalAvalancheGroup& g,float t){
    using namespace avalanche_detail;Rounding rounding;
    const float s=mul(t,30.f);
    if(s<g.fadeIn){tu.scale=1.f;tu.alpha=sdiv(s,g.fadeIn);return;}
    if(s<ssub(g.duration,g.fadeOut)){tu.scale=1.f;tu.alpha=1.f;return;}
    if(g.type==2)return;
    const float a=sdiv(ssub(g.duration,s),g.fadeOut);tu.alpha=a;tu.scale=a;
    if(0.f<a)tu.scale=ssqrt(a);
}

struct OriginalTumblerSample {AvaQuad position{},colour{};AvaMatrix rotation=avalancheIdentity();};
// 0x2D5778: integrate the recorded samples up to trunc(t x 30) (clamped to dur - 2 with fraction 1), then the partial step.
inline OriginalTumblerSample originalTumblerIntegrate(OriginalTumbler& tu,const OriginalAvalancheGroup& g,float t){
    using namespace avalanche_detail;
    originalTumblerEnvelope(tu,g,t);
    Rounding rounding;
    const float x=mul(t,30.f);
    int32_t s=trunc(x);const int32_t n=trunc(g.duration);
    float frac=ssub(x,float(s));
    if(!(s<n-1)){frac=1.f;s=n-2;}
    const float oneMinus=ssub(1.f,frac);
    auto reset=[&]{tu.sample=0;tu.integrated=g.start;tu.rotationIntegrated=avalancheIdentity();};
    if(s!=tu.sample){
        if(s==0)reset();
        else{
            if(s<tu.sample)reset();
            while(tu.sample!=s){ // the recorded steps (the emitter catch-up of +748 is presentation only)
                const uint8_t* p=sampleAt(g,tu.sample);
                const AvaQuad step{sadd(float(int8_t(p[0])),float(int8_t(p[0]))),sadd(float(int8_t(p[1])),float(int8_t(p[1]))),sadd(float(int8_t(p[2])),float(int8_t(p[2]))),0.f};
                for(unsigned k=0;k<4;++k)tu.integrated[k]=add(tu.integrated[k],step[k]); // vadd.xyzw
                const float scaleAxis=std::bit_cast<float>(0x3c010204u);
                float ax=mul(float(int8_t(p[6])),scaleAxis),ay=mul(float(int8_t(p[7])),scaleAxis),az=mul(float(int8_t(p[8])),scaleAxis);
                const float len=ssqrt(sadd(sadd(mul(ax,ax),mul(ay,ay)),mul(az,az)));
                if(!(len==0.f)){const float inv=sdiv(1.f,len);az=mul(az,inv);ax=mul(ax,inv);ay=mul(ay,inv);}
                const float angle=mul(float(p[9]),std::bit_cast<float>(0x3c069124u));
                const auto sc=collision_scalar::sincos(angle);
                const AvaMatrix r=axisAngle(ax,ay,az,sc[0],sc[1]);
                const AvaMatrix old=tu.rotationIntegrated;
                for(unsigned i=0;i<4;++i)tu.rotationIntegrated[i]=transform(r,old[i]);
                ++tu.sample;
            }
        }
    }
    // 0x2D5E2C: the partial step and colour of sample s, the partial rotation
    OriginalTumblerSample out;
    const uint8_t* p=sampleAt(g,s);
    const AvaQuad step{sadd(mul(float(int8_t(p[0])),frac),mul(float(int8_t(p[0])),frac)),sadd(mul(float(int8_t(p[1])),frac),mul(float(int8_t(p[1])),frac)),sadd(mul(float(int8_t(p[2])),frac),mul(float(int8_t(p[2])),frac)),0.f};
    for(unsigned k=0;k<4;++k)out.position[k]=add(step[k],tu.integrated[k]);
    const float inv128=std::bit_cast<float>(0x3c000000u),colourScale=1.f; // gp+0x9B8
    const float r=mul(sadd(mul(float(p[3]),oneMinus),mul(float(p[13]),frac)),inv128);
    const float gg=mul(sadd(mul(float(p[4]),oneMinus),mul(float(p[14]),frac)),inv128);
    const float b=mul(sadd(mul(float(p[5]),oneMinus),mul(float(p[15]),frac)),inv128);
    out.colour={mul(r,colourScale),mul(gg,colourScale),mul(b,colourScale),mul(tu.alpha,colourScale)};
    {
        const float scaleAxis=std::bit_cast<float>(0x3c010204u);
        float ax=mul(float(int8_t(p[6])),scaleAxis),ay=mul(float(int8_t(p[7])),scaleAxis),az=mul(float(int8_t(p[8])),scaleAxis);
        const float len=ssqrt(sadd(sadd(mul(ax,ax),mul(ay,ay)),mul(az,az)));
        if(!(len==0.f)){const float inv=sdiv(1.f,len);az=mul(az,inv);ax=mul(ax,inv);ay=mul(ay,inv);}
        float angle=mul(float(p[9]),std::bit_cast<float>(0x40060a92u));angle=mul(frac,angle);angle=mul(angle,std::bit_cast<float>(0x3b808081u));
        const auto sc=collision_scalar::sincos(angle);
        const AvaMatrix rr=axisAngle(ax,ay,az,sc[0],sc[1]);
        for(unsigned i=0;i<4;++i)out.rotation[i]=transform(rr,tu.rotationIntegrated[i]);
    }
    return out;
}

// ---- The trails emitter (tumbler +0xD0) -------------------------------------------------------------------------------------
// 0x371600 (vt+0x14): 0x370D60 (cursor 0; for each of the capacity slots: both rings zeroed, the active count 0 -- inside the
// loop, so an unallocated emitter keeps its count) then every colour slot zeroed (the static colour gp+0x28A8, 0 once set up).
inline void originalAvalancheEmitterClear(OriginalAvalancheEmitter& e){
    originalAvalancheEmitterConstruct(e);using namespace set_piece_particles;using namespace dynamic_emitter;auto& b=e.base;
    b.setU(Cursor,0);const int32_t n=b.i(Capacity);
    for(int32_t i=0;i<n;++i){if(size_t(i)<b.ringPosition.size())b.ringPosition[size_t(i)]=kZero;if(size_t(i)<b.ringVelocity.size())b.ringVelocity[size_t(i)]=kZero;b.setU(ActiveCount,0);}
    for(int32_t i=0;i<n&&size_t(i)<e.colours.size();++i)e.colours[size_t(i)]={0,0,0,0};
}
// 0x370DC8(emitter, params, seed) on the colour class (its reallocation: 0x3715B0 free, 0x371548 allocate, 0x371600 clear). The same
// body as engine/set_piece_particles.hpp originalDynamicEmitterConstruct's 0x370DC8 part, with the caller's seed: the avalanche
// passes 2.0, so 0x370058 writes kernel seed 0 only and draws nothing; the one visual draw is the flip phase (NumFlipTextures >= 2).
inline void originalAvalancheEmitterSetup(OriginalAvalancheEmitter& e,const OriginalParticleParams& p,float seed,OriginalRandomState& visual){
    originalAvalancheEmitterConstruct(e);using namespace set_piece_particles;using namespace dynamic_emitter;namespace P=particle_param;auto& b=e.base;
    OriginalRounding rounding;
    const int32_t flips=p.i(P::NumFlipTextures);b.setI(FlipCount,flips);b.setF(FlipRate,p.f(P::FlipTextureRate));
    if(flips>=2){const float count=float(flips);const float u=eeSub(fbits((visual.next()&0x7FFFFFu)|0x3F800000u),1.f);b.setF(FlipPhase,eeMul(count,u));}
    else b.setU(FlipPhase,0);
    b.setI(Texture,p.i(P::TextureId));b.setI(Blend,p.i(P::BlendMode));
    const float duration=p.f(P::Duration);
    b.setF(Remaining,duration<0.f?duration:eeAdd(eeAdd(duration,p.f(P::Life)),eeMul(p.f(P::LifeR),.5f)));
    bool reallocate=false;
    if(!(0.f<=duration)){
        const float ticks=eeMul(eeAdd(p.f(P::Life),eeMul(p.f(P::LifeR),.5f)),60.f);
        float t=float(eeTruncate(ticks));if(t<ticks)t=eeAdd(t,1.f);
        const int32_t capacity=eeTruncate(t);
        if(b.i(Capacity)!=capacity){b.setI(Capacity,capacity);reallocate=true;}
    }
    b.setV(LastPosition,kZero);b.setV(LastVelocity,kZero);b.setV(LastColour,kZero);
    const float span=duration<0.f?eeAdd(p.f(P::Life),eeMul(p.f(P::LifeR),.5f)):duration;
    auto k=b.kernel();
    originalParticleKernelInit(k,p.i(P::NumParticles)*b.i(Capacity),p.i(P::NumBlur),p.vec3(P::Force,0.f),span,p.f(P::BlurStep),p.f(P::Damp));
    std::array<float,8> bounds;for(int j=0;j<8;j++)bounds[j]=b.f(BoundsMin+4*j);
    originalParticleKernelPrepare(k,kIdentity,p,bounds,seed,visual);
    for(int j=0;j<8;j++)b.setF(BoundsMin+4*j,bounds[j]);
    if(reallocate){const unsigned n=unsigned(b.i(Capacity));b.ringPosition.assign(n,kZero);b.ringVelocity.assign(n,kZero);e.colours.assign(n,{0,0,0,0});originalAvalancheEmitterClear(e);}
    b.setU(Allocated,1);
    b.setU(Packet+0x0,0x00000000u);b.setU(Packet+0x4,0x00B00294u);b.setU(Packet+0x8,0x00000080u);b.setU(Packet+0xC,0);b.setU(Packet+0x10,0xFFFFFFFFu);
    uint32_t w1=b.u(Packet+0x4);w1=(w1&0xFFBFFFFFu)|0x00400000u;w1&=0xFFCFFFFFu;w1&=0xFFFFFFFCu;w1|=2u;b.setU(Packet+0x4,w1);
    b.setU(Packet+0x8,b.u(Packet+0x8)&0xE00003FFu);
}
// 0x3717C0(emitter, position, velocity, colour, active, dt): +0x1D0 = colour; the ring slot's RGBA bytes = trunc(clamp(c x 255,
// 0, 255)) (a negative or NaN product gives 0); then 0x3710D0 (one visual draw when active).
inline void originalAvalancheEmitterEmit(OriginalAvalancheEmitter& e,const AvaQuad& position,const AvaQuad& colour,bool active,float dt,OriginalRandomState& visual){
    originalAvalancheEmitterConstruct(e);using namespace set_piece_particles;using namespace dynamic_emitter;auto& b=e.base;
    b.setV(LastColour,{colour[0],colour[1],colour[2],colour[3]});
    {
        OriginalRounding rounding;
        auto byte=[](float c){const float x=eeMul(c,255.f);const float y=(0.f<=x)?(x<255.f?x:255.f):0.f;return uint8_t(eeTruncate(y));};
        const unsigned slot=b.u(Cursor);
        if(slot<e.colours.size())e.colours[slot]={byte(colour[0]),byte(colour[1]),byte(colour[2]),byte(colour[3])};
    }
    const Vec4 p{position[0],position[1],position[2],position[3]};
    originalDynamicEmitterBirth(b,&p,&kZero,active,dt,visual);
}
inline OriginalParticleParams originalAvalancheEmitterParams(const OriginalAvalancheGroup& g){
    OriginalParticleParams p;p.builtin=16;for(unsigned k=0;k<54;++k)p.setU(4*k,g.emitterWords[k]);return p;
}

// 0x2D7DD8 (force: 0x2D81B0's release of every tumbler, the type-2 pieces too). Returns the released group's instance
// (0: nothing to release): the host sends entity vt+0x08(3) to it.
inline uint32_t originalTumblerRelease(OriginalAvalancheWorld& w,OriginalTumbler& tu,bool force){
    if(tu.group<0)return 0;
    const auto& g=w.defs[size_t(tu.avalanche)].groups[size_t(tu.group)];
    if(!force&&g.type==2)return 0;
    const uint32_t instance=g.resource;
    originalAvalancheEmitterClear(w.emitters[size_t(&tu-w.pool.data())]); // 0x371600(+0xD0)
    tu.group=-1;tu.avalanche=-1;tu.emitterActive=false; // group +0x114 = 0, +0x2E0 = 0
    return instance;
}
// 0x2D7CA8: returns alive. released: the instance the host hides (entity vt+0x08(3)), 0 none. With a visual stream, a playing
// emitter group (not type 2) sets its emitter's kernel +0x10C (colour range 0 alpha, tumbler +0x1FC) to the tumbler's alpha and
// emits the point (0x3717C0: position +96, zero velocity, colour +192, active = t' < duration, dt = gp-0x3CD4 = 1/60).
inline bool originalTumblerUpdate(OriginalAvalancheWorld& w,OriginalTumbler& tu,float t,uint32_t& released,OriginalRandomState* visual=nullptr){
    using namespace avalanche_detail;
    released=0;if(tu.group<0)return false;
    const auto& g=w.defs[size_t(tu.avalanche)].groups[size_t(tu.group)];
    Rounding rounding;
    const float tt=mul(t,g.speed);
    const auto out=originalTumblerIntegrate(tu,g,tt);
    if(mul(tt,30.f)<g.duration){
        tu.position=out.position;tu.rotation=out.rotation;tu.colour=out.colour;
        if(g.type!=2&&g.hasEmitter&&visual){
            auto& em=w.emitters[size_t(&tu-w.pool.data())];em.base.setF(0x12C,tu.alpha);
            originalAvalancheEmitterEmit(em,tu.position,tu.colour,tt<g.duration,set_piece_particles::fbits(0x3C888889u),*visual);
        }
        return true;
    }
    if(g.type==2)return true;
    released=originalTumblerRelease(w,tu,false);return false;
}
// 0x2D81B0: every tumbler released (forced), the slot freed, the loop stopped. Returns the instances to hide.
inline std::vector<uint32_t> originalAvalancheSlotRelease(OriginalAvalancheWorld& w,OriginalAvalancheSlot& slot){
    std::vector<uint32_t> hidden;slot.array.clear();
    if(slot.avalanche<0)return hidden;
    for(int32_t k=slot.list;k>=0;k=w.pool[size_t(k)].next)if(const uint32_t r=originalTumblerRelease(w,w.pool[size_t(k)],true))hidden.push_back(r);
    slot.avalanche=-1;w.loopRefs=w.loopRefs>0?w.loopRefs-1:0;if(w.loopTransitions.size()<64)w.loopTransitions.push_back(w.loopRefs);if(w.activeCount)--w.activeCount;
    return hidden;
}
// 0x2D97A8 (builtin 94). Returns false when the id is unknown, already playing, or no slot / tumbler is free. An emitter group's
// tumbler gets its trails emitter: 0x371600, 0x370DC8(2.0, the group's builtin-96 block), +720 = 1, and +0x1E0 (the emitter's
// kernel colour base) = (0x2EE7C8, 0x2EE810, 0x2EE858)(0): the human's Weather painter flake R, G, B (block 0 +0x20), w 0.
inline bool originalAvalancheTrigger(OriginalAvalancheWorld& w,int32_t id,OriginalRandomState* visual=nullptr,const AvaQuad& flake=AvaQuad{1,1,1,0}){
    int32_t def=-1;for(size_t k=0;k<w.defs.size();++k)if(w.defs[k].id==id){def=int32_t(k);break;}
    if(def<0)return false;
    for(const auto& s:w.slots)if(s.avalanche==def)return false;
    for(auto& slot:w.slots){
        if(slot.avalanche>=0)continue;
        slot=OriginalAvalancheSlot{};slot.avalanche=def;slot.list=-1;
        const auto& d=w.defs[size_t(def)];size_t at=0;
        for(size_t gi=0;gi<d.groups.size();++gi){
            while(at<w.pool.size()&&w.pool[at].group>=0)++at;
            if(at==w.pool.size()){ // 64 tumblers in use: the ones taken are freed, the slot too
                for(int32_t k=slot.list;k>=0;k=w.pool[size_t(k)].next){w.pool[size_t(k)].group=-1;w.pool[size_t(k)].avalanche=-1;}
                slot.avalanche=-1;return false;}
            auto& tu=w.pool[at];const auto& g=d.groups[gi];
            tu=OriginalTumbler{};tu.avalanche=def;tu.group=int32_t(gi);tu.next=slot.list;slot.list=int32_t(at);
            tu.sample=0;tu.integrated=g.start;tu.rotationIntegrated=avalancheIdentity();
            tu.position={0,0,0,1};tu.rotation=avalancheIdentity();tu.colour={1,1,1,1};
            originalTumblerEnvelope(tu,g,0.f);
            tu.emitterActive=g.hasEmitter;
            if(g.hasEmitter){
                auto& em=w.emitters[at];originalAvalancheEmitterClear(em);
                OriginalRandomState none;originalAvalancheEmitterSetup(em,originalAvalancheEmitterParams(g),2.f,visual?*visual:none);
                em.word200=1;em.base.setV(0x110,{flake[0],flake[1],flake[2],0.f});
            }
        }
        for(int32_t k=slot.list;k>=0;k=w.pool[size_t(k)].next)slot.array.push_back(k); // 0x2D7E60
        ++w.loopRefs;if(w.loopTransitions.size()<64)w.loopTransitions.push_back(w.loopRefs);++w.activeCount;
        return true;
    }
    return false;
}
// The host sets each new tumbler's +96 from its instance (instance +0x40: the authored matrix's translation row).
inline void originalAvalancheSetStart(OriginalAvalancheWorld& w,int32_t slotIndex,const std::function<AvaQuad(uint32_t)>& translation){
    auto& slot=w.slots[size_t(slotIndex)];
    for(int32_t k=slot.list;k>=0;k=w.pool[size_t(k)].next){auto& tu=w.pool[size_t(k)];if(tu.group>=0)tu.position=translation(w.defs[size_t(tu.avalanche)].groups[size_t(tu.group)].resource);}
}
// 0x2D7EF8. released: the instances the host hides this tick.
inline void originalAvalancheSlotTick(OriginalAvalancheWorld& w,OriginalAvalancheSlot& slot,std::vector<uint32_t>& released,OriginalRandomState* visual=nullptr){
    using namespace avalanche_detail;
    if(slot.avalanche<0)return;
    Rounding rounding;
    slot.t=sadd(slot.t,std::bit_cast<float>(0x3c1374bcu)); // gp+0x974
    bool any=false;size_t index=0;
    for(int32_t k=slot.list;k>=0;k=w.pool[size_t(k)].next,++index){
        uint32_t r=0;const bool alive=originalTumblerUpdate(w,w.pool[size_t(k)],slot.t,r,visual);
        if(r)released.push_back(r);
        any|=alive;if(!alive&&index<slot.array.size())slot.array[index]=-1;
    }
    // (the instance events +12 are zeroed by the parse 0x2DA028: dead in retail)
    const auto& d=w.defs[size_t(slot.avalanche)];
    while(slot.soundCursor<int32_t(d.sounds.size())){
        const auto& s=d.sounds[size_t(slot.soundCursor)];
        if(!(float(s.tick)<=mul(slot.t,30.f)))break;
        ++slot.soundCursor; // 0x29E560(audio, tumbler +96) of array[s.tumbler] when it lives: jr ra
    }
    if(!any){auto h=originalAvalancheSlotRelease(w,slot);released.insert(released.end(),h.begin(),h.end());}
}
// 0x2D9CB0 (from the replay snapshot 0x26D818): 0x80 bytes through the stream's vt+0x0C: per slot the playing avalanche's id
// (record +0; -1 free), then per slot its t (+24; 0 free).
inline std::array<uint32_t,32> originalAvalancheSave(const OriginalAvalancheWorld& w){
    std::array<uint32_t,32> out{};
    for(unsigned k=0;k<16;++k){const auto& s=w.slots[k];out[k]=s.avalanche>=0?uint32_t(w.defs[size_t(s.avalanche)].id):0xFFFFFFFFu;out[16+k]=s.avalanche>=0?std::bit_cast<uint32_t>(s.t):0u;}
    return out;
}
// 0x2D9D68 (from the replay restore 0x26DBF0, after the entity groups were emptied (0x355118) and before the move nodes come
// back (0x357D28)): every playing slot released (0x2D81B0; its hidden instances are returned), then per saved slot in order the
// avalanche triggered again (0x2D97A8, into the first free slot and tumblers) and brought to its t:
//   slot t = t; when t != 0, per tumbler of the new list: +0x2EC = 1 when its group has an emitter (the emitter catch-up;
//   presentation), 0x2D7C00(tumbler, t) and 0x2D5778(tumbler, t) -- the group's speed factor is NOT applied (0x2D7CA8 plays
//   t x speed: ABC1's 0.75 groups come back ahead), and +96 / +112 keep the trigger's values (the instance translation, the
//   identity) until the next 0x2D7CA8, so the next 0x2D9C00 (entity group 1, before the tumbler step) reads those;
//   the event cursor advances while event t <= t (no events in retail), the sound cursor while float(sound tick) <= t (not
//   t x 30 as 0x2D7EF8: fewer cues are skipped; the cues are dead in retail).
inline std::vector<uint32_t> originalAvalancheRestore(OriginalAvalancheWorld& w,const std::array<uint32_t,32>& saved,const std::function<AvaQuad(uint32_t)>& translation,OriginalRandomState* visual=nullptr,const AvaQuad& flake=AvaQuad{1,1,1,0}){
    std::vector<uint32_t> hidden;
    for(auto& slot:w.slots)if(slot.avalanche>=0){auto h=originalAvalancheSlotRelease(w,slot);hidden.insert(hidden.end(),h.begin(),h.end());}
    for(unsigned k=0;k<16;++k){
        const int32_t id=int32_t(saved[k]);if(id<0)continue;
        if(!originalAvalancheTrigger(w,id,visual,flake))continue;
        int32_t index=-1;for(int32_t j=0;j<16;++j)if(w.slots[size_t(j)].avalanche>=0&&w.defs[size_t(w.slots[size_t(j)].avalanche)].id==id){index=j;break;}
        if(index<0)continue;
        originalAvalancheSetStart(w,index,translation);
        auto& slot=w.slots[size_t(index)];const float t=std::bit_cast<float>(saved[16+k]);slot.t=t;
        if(t==0.f)continue;
        for(int32_t p=slot.list;p>=0;p=w.pool[size_t(p)].next){
            auto& tu=w.pool[size_t(p)];const auto& g=w.defs[size_t(tu.avalanche)].groups[size_t(tu.group)];
            originalTumblerEnvelope(tu,g,t);(void)originalTumblerIntegrate(tu,g,t);
        }
        const auto& d=w.defs[size_t(slot.avalanche)];
        while(slot.soundCursor<int32_t(d.sounds.size())&&float(d.sounds[size_t(slot.soundCursor)].tick)<=t)++slot.soundCursor;
    }
    return hidden;
}
// 0x2D9C00: the AvaSpline matrix of an instance: rotation x scale, translation = position (nullptr: no live tumbler).
inline bool originalAvalancheInstanceMatrix(const OriginalAvalancheWorld& w,uint32_t resource,AvaMatrix& m){
    using namespace terrain_original;Rounding rounding;
    for(const auto& tu:w.pool){
        if(tu.group<0||w.defs[size_t(tu.avalanche)].groups[size_t(tu.group)].resource!=resource)continue;
        for(unsigned i=0;i<4;++i)for(unsigned k=0;k<4;++k)m[i][k]=mul(tu.rotation[i][k],tu.scale);
        m[3]=tu.position;return true;
    }
    return false;
}
}
