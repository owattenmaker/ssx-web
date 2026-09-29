#pragma once
// Original SSX3 (PS2 USA, SLUS_207.72) stage-script particle effects: the simulation half of the
// set-piece particle systems (rocket smoke, spin-twin sparks, dragon fire, fire gushes, start/mid
// fire pops, EZ fireworks, snow-crumb puffs, snow wind, river/waterfall spray, Metro-City/Junction
// emitters). The draw half (VU program, sprites, blur copies, textures, blend) consumes the state
// images defined here; every image is laid out exactly like the PS2 object so a draw evaluator can
// use the same offsets as the original renderer (3708C0 / 371380 -> renderer vt+0x294 / +0x29C).
//
// Creation (stage LUN builtins, table 0x441F38):
//   builtin16 0x2FD420 "MakeParticleData": 0xE0-byte param block (defaults 0x4FB640, built once),
//     keyed args (field types 0x4460B0), target +0xD8 (-1 = script instance, context +0x290).
//     Instance without entity: clamp LifeR<=Life, 0x3578A8 new type-13 entity (vtable 0x48EE60) +
//     Particle effect. Instance with entity: 0x2FAE38(entity, 13, block): types 9/6/22 refuse (block
//     freed); already type 13 -> vt+0x12C (no-op 0x3609D8, block freed: re-firing does nothing);
//     otherwise the entity is replaced by a type-13 entity 0x3578A8(obj,1,inst,block,old): 0x355F10
//     moves the old modifier/effect lists (0x356020 -> 0x353398: every moved effect gets vt+0x1C =
//     stop, pushed back) and, if the old matrix is not instance+0x10, freezes it in a PositionModifier
//     (0x355918; the spline -> Position handover of engine/spline_modifier.hpp); NO LifeR clamp here.
//     Then 0x355CB0: Particle effect 0x1F0 bytes (ctor 0x3458C0) pushed to the FRONT of the
//     entity's effect list (container+0x10, 0x345700).
//   builtin26 0x2FEE98 "DynamicParticle": 0xF0-byte block (defaults 0x4FB938, types 0x446380),
//     always clamps LifeR<=Life, needs an entity that accepts modifiers (vt+0x84), +0xDC node,
//     +0xE0..+0xE8 offset (w=1): 0x355D10 -> DynamicParticle effect 0x270 bytes (ctor 0x345C90),
//     pushed to the front of the effect list (node >= instance model node count: block freed).
//   builtin69 0x302490 (instance, mode): mode 0 stops every effect of the entity (vt+0x1C),
//     mode 1 deletes the effects whose vt+0x14 returns 3.  builtin30 = sound (0x28B180/0x2974A0),
//     builtin55 0x3019C8 = timer gate (key1/30 s crossed this tick, 0x34EBE0).
//
// Effects (linked list node: +0 next, +4 prev, +8 vtable, +0xC instance):
//   Particle 0x4912B0 (0x1F0): +0x10 owner matrix (entity vt+0xC4 at creation: modifier matrix or
//     instance+0x10), +0x50 static emitter (0x190, vtable +0x18C 0x493160, init 0x370018/0x3705E0),
//     +0x1E0 block, +0x1E4 dead. vt+0x2C update 0x345B40, vt+0x34 draw 0x345BC8 -> 0x3708C0,
//     vt+0x1C stop = no-op, vt+0x14 = 1.
//   DynamicParticle 0x491268 (0x270): +0x10 node, +0x14 stopped, +0x18 dead, +0x20 offset (w 1),
//     +0x30 world birth position, +0x40 world birth velocity, +0x50 Vel (param Vel, which is then
//     zeroed in the block before the kernel is built), +0x60 dynamic emitter (base class, 0x1F8
//     bytes + vptr 0x491370 at +0x1F8), +0x260 block (aliases emitter+0x200). vt+0x2C update
//     0x345F90, vt+0x34 draw 0x346070 -> 0x371380(emitter, 7), vt+0x1C stop 0x346060, vt+0x14 = 2.
//   Neither effect is ever unlinked by itself: a dead effect stays inert in the list; a type-13
//   entity (update 0x357950) deletes itself when its effect list is EMPTY (never, in practice).
//
// Per game tick (world object pass 0x354F98 group 1 = entities, newest-created FIRST (push-front);
// entity update 0x356198: modifier container 0x352C70, vt+0x7C (LiveComp tick 0x341D48 incl. slot-5
// programs), then 0x352D20: effect list in order (vt+0x2C), then the finished check -> slot 4):
//   Particle 0x345B40: if !dead: 0x370788(emitter, 1/60); if remaining was >= 0 and is now <= 0 -> dead.
//   DynamicParticle 0x345F90: if !dead: 0x345EF8 (node world matrix 0x34FED8 -> +0x30 = offset*M,
//     +0x40 = Vel*M), then not stopped: 0x3710D0(active) (one 0x3177F0 draw per call);
//     stopped: if active count > 0 0x3710D0(inactive) (no draw) else dead.
//
// 0x3177F0 (0x4FF018) draws: 9 per emitter construction (0x36CCB8 seeds, via 0x370058 with seed -1)
// + 1 in 0x370DC8 when NumFlipTextures >= 2 (drawn BEFORE the 9 seeds); 1 per active DynamicParticle
// birth. Static-emitter updates never draw.
//
// Float policy (as engine/spline_modifier.hpp): EE add.s/sub.s -> originalScalarAdd/Subtract,
// mul.s -> terrain_original::mul (chop), div.s -> eeDivide (nearest; zero-exponent divisor ->
// sign|0x7F7FFFFF like PCSX2), cvt.w.s truncation; VU0 macro ops lane-wise terrain_original
// add/sub/mul/div in ACC order (VMULA/VMADD round each product and each sum). Compile with
// -ffp-contract=off.
#include "original_random.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace ssx {
namespace set_piece_particles {
using Vec4=std::array<float,4>;
using Mat4=std::array<Vec4,4>; // row vectors, row 3 = translation (p * M)
inline float fbits(uint32_t b){return std::bit_cast<float>(b);}
inline uint32_t bitsf(float f){return std::bit_cast<uint32_t>(f);}
inline float eeAdd(float a,float b){return originalScalarAdd(a,b);}
inline float eeSub(float a,float b){return originalScalarSubtract(a,b);}
inline float eeMul(float a,float b){return terrain_original::mul(a,b);}
inline float eeDivide(float a,float b){
    const uint32_t y=bitsf(b),x=bitsf(a);
    if((y&0x7F800000u)==0)return fbits(((x^y)&0x80000000u)|0x7F7FFFFFu);
    return originalScalarDivide(a,b);
}
inline int32_t eeTruncate(float f){ // cvt.w.s (PCSX2 clamps out-of-range to INT_MAX/INT_MIN)
    if(!(f<2147483648.f))return 0x7FFFFFFF;if(!(f>=-2147483648.f))return int32_t(0x80000000u);return int32_t(f);}
inline Vec4 vAdd(const Vec4& a,const Vec4& b){Vec4 r;for(int i=0;i<4;i++)r[i]=terrain_original::add(a[i],b[i]);return r;}
inline Vec4 vSub(const Vec4& a,const Vec4& b){Vec4 r;for(int i=0;i<4;i++)r[i]=terrain_original::sub(a[i],b[i]);return r;}
inline Vec4 vMulS(const Vec4& a,float s){Vec4 r;for(int i=0;i<4;i++)r[i]=terrain_original::mul(a[i],s);return r;}
inline float vuDivide(float a,float b){return terrain_original::div(a,b);} // VDIV Q (VU0 chop)
// VMULAx/VMADDAy/VMADDAz/VMADDw: row vector v times M.
inline Vec4 vTransform(const Mat4& m,const Vec4& v){
    using namespace terrain_original;Vec4 r;
    for(int k=0;k<4;k++){float acc=mul(m[0][k],v[0]);acc=add(acc,mul(m[1][k],v[1]));acc=add(acc,mul(m[2][k],v[2]));r[k]=add(acc,mul(m[3][k],v[3]));}
    return r;
}
inline Mat4 mProduct(const Mat4& a,const Mat4& b){Mat4 r;for(int i=0;i<4;i++)r[i]=vTransform(b,a[i]);return r;} // rows of a times b
inline uint32_t lfsrStep(uint32_t s){ // VU RINIT+RNEXT (0x36D400)
    s=(s&0x7FFFFFu)|0x3F800000u;return (((s<<1)^((s>>4)&1u)^((s>>22)&1u))&0x7FFFFFu)|0x3F800000u;}
inline const Mat4 kIdentity{{{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}}}; // 0x4FF1A0
inline const Vec4 kZero{0,0,0,0};                                       // 0x4FF120

// Little-endian word image of an original object (unwritten words keep their initial contents).
template<unsigned Bytes> struct Image {
    std::array<uint32_t,Bytes/4> w{};
    uint32_t u(unsigned o)const{return w[o/4];}
    int32_t i(unsigned o)const{return int32_t(w[o/4]);}
    float f(unsigned o)const{return fbits(w[o/4]);}
    Vec4 v(unsigned o)const{return {f(o),f(o+4),f(o+8),f(o+12)};}
    void setU(unsigned o,uint32_t x){w[o/4]=x;}
    void setI(unsigned o,int32_t x){w[o/4]=uint32_t(x);}
    void setF(unsigned o,float x){w[o/4]=bitsf(x);}
    void setV(unsigned o,const Vec4& x){for(int k=0;k<4;k++)setF(o+4*k,x[k]);}
    const uint8_t* bytes()const{return reinterpret_cast<const uint8_t*>(w.data());}
};
} // namespace set_piece_particles

// ---- Parameter block (builtin16: 0xE0 bytes, builtin26: 0xF0) --------------------------------
// Offsets (FIELDS order of tools/export_snow_assets.py for 0x00..0xCC; colours are stored A,R,G,B
// and used as (R,G,B,A)).
namespace particle_param {
enum : unsigned { NumParticles=0x00, NumBlur=0x04, Duration=0x08, Damp=0x0C, Size=0x10, Life=0x14,
    SizeR=0x18, LifeR=0x1C, BlurStep=0x20, Off=0x24, R0=0x30, R1=0x3C, Vel=0x48, R0V=0x54, R1V=0x60,
    R2V=0x6C, Force=0x78, StartCol=0x84, EndCol=0x94, R0Col=0xA4, R1Col=0xB4, TextureId=0xC4,
    BlendMode=0xC8, SizeFinal=0xCC, NumFlipTextures=0xD0, FlipTextureRate=0xD4, Instance=0xD8,
    Node=0xDC /*builtin26*/, Offset=0xE0 /*builtin26, 3 floats*/ };
}
struct OriginalParticleParams : set_piece_particles::Image<0xF0> {
    int builtin=16;
    set_piece_particles::Vec4 vec3(unsigned o,float w)const{return {f(o),f(o+4),f(o+8),w};}
    set_piece_particles::Vec4 colour(unsigned o)const{return {f(o+4),f(o+8),f(o+12),f(o)};} // (R,G,B,A)
};
// Defaults written on first use (0x2FD420 / 0x2FEE98 prologues).
inline OriginalParticleParams originalParticleDefaults(int builtin){
    using namespace particle_param;OriginalParticleParams p;p.builtin=builtin;
    for(unsigned o=0x14;o<=0x80;o+=4)p.setF(o,0.f);
    p.setI(NumParticles,1);p.setI(NumBlur,0);p.setF(Duration,-1.f);p.setF(Damp,1.f);p.setF(Size,4.f);
    p.setF(StartCol,1.f);p.setF(0x88,0);p.setF(0x8C,0);p.setF(0x90,0);
    for(unsigned o=0x94;o<=0xA4;o+=4)p.setF(o,1.f);
    p.setF(0xA8,0);p.setF(0xAC,0);p.setF(0xB0,0);p.setF(R1Col,1.f);p.setF(0xB8,0);p.setF(0xBC,0);p.setF(0xC0,0);
    p.setI(TextureId,16);p.setI(BlendMode,0);p.setF(SizeFinal,0.f);p.setI(NumFlipTextures,1);p.setF(FlipTextureRate,20.f);
    p.setI(Instance,-1);
    if(builtin==26){p.setI(Node,0);p.setF(0xE0,0);p.setF(0xE4,0);p.setF(0xE8,0);}
    else if(builtin!=16)throw std::runtime_error("particle params: builtin 16 or 26");
    return p;
}
// Field types (1 int, 2 float, 0 other) of 0x4460B0 (builtin16) / 0x446380 (builtin26).
inline int originalParticleFieldType(int builtin,unsigned key){
    static const uint8_t t16[58]={1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,2,1,2,1,0,1,1};
    static const uint8_t t26[62]={1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,2,1,2,1,1,2,2,2,0,1,1};
    if(builtin==16)return key<58?t16[key]:-1;return key<62?t26[key]:-1;
}
// One script argument (VM record: key, raw word, type 1 int / 2 float): stored raw when the types
// agree or the field is not a float; an int given for a float field is converted (cvt.s.w).
inline void originalParticleApplyArgument(OriginalParticleParams& p,unsigned key,int type,uint32_t raw){
    const unsigned limit=p.builtin==16?0xE0/4:0xF0/4;
    if(key>=limit)throw std::runtime_error("particle params: key out of range");
    const int field=originalParticleFieldType(p.builtin,key);
    if(type!=field&&field==2)p.setF(4*key,float(int32_t(raw)));else p.setU(4*key,raw);
}
// if (Life < LifeR) LifeR = Life (builtin26 always; builtin16 only for an instance without entity).
inline void originalParticleClampLifeRange(OriginalParticleParams& p){
    using namespace particle_param;if(p.f(Life)<p.f(LifeR))p.setU(LifeR,p.u(Life));}

// ---- Kernel (0x150 bytes; static emitter +0x10, dynamic emitter +0x20) ------------------------
// +0x00 N (particles in the kernel window; dynamic: NumParticles*capacity) +0x04 NumBlur +0x08 age
// (scaled by Damp) +0x0C age step +0x10 sizeRange +0x14 lifeRange +0x18 sizeBase +0x1C lifeBase
// +0x20 sizeDelta (+0x24..+0x2C never written) +0x30 1/NumBlur (0x7F7FFFFF when 0) +0x34 BlurStep*Damp
// +0x38..+0x48 seeds 0..4, +0x4C Damp, +0x50..+0x5C seeds 5..8 (VU LFSR words, 0x36CCB8)
// +0x60 Force/Damp^2 (w 0) +0x70 velocityBase +0x80/+0x90/+0xA0 R0V/R1V/R2V*M/Damp
// +0xB0 positionBase (Off*M - 1.5(R0+R1)) +0xC0/+0xD0 R0*M/R1*M +0xE0 per-birth position step (0)
// +0xF0 colourBase +0x100/+0x110 colour ranges (*128) +0x120 colour slope +0x130 0 +0x140 (1,1,0,0).
namespace particle_kernel {
enum : unsigned { N=0x00, Blur=0x04, Age=0x08, AgeStep=0x0C, SizeRange=0x10, LifeRange=0x14,
    SizeBase=0x18, LifeBase=0x1C, SizeDelta=0x20, InvBlur=0x30, BlurStep=0x34, Seed0=0x38, Damp=0x4C,
    Seed5=0x50, Force=0x60, VelBase=0x70, VelR0=0x80, VelR1=0x90, VelR2=0xA0, PosBase=0xB0, PosR0=0xC0,
    PosR1=0xD0, PosStep=0xE0, ColBase=0xF0, ColR0=0x100, ColR1=0x110, ColSlope=0x120, Zero130=0x130,
    Ones140=0x140, Bytes=0x150 };
inline constexpr unsigned seedOffsets[9]={0x38,0x3C,0x40,0x44,0x48,0x50,0x54,0x58,0x5C};
}
// Kernel view into an emitter image (base = kernel offset inside the image).
template<class I> struct KernelRef {
    I& e;unsigned base;
    float f(unsigned o)const{return e.f(base+o);} int32_t i(unsigned o)const{return e.i(base+o);}
    set_piece_particles::Vec4 v(unsigned o)const{return e.v(base+o);}
    void setF(unsigned o,float x){e.setF(base+o,x);} void setI(unsigned o,int32_t x){e.setI(base+o,x);}
    void setU(unsigned o,uint32_t x){e.setU(base+o,x);} uint32_t u(unsigned o)const{return e.u(base+o);}
    void setV(unsigned o,const set_piece_particles::Vec4& x){e.setV(base+o,x);}
};

// 0x36CBF8 (kernel, n, blur, force, span, blurStep, damp).
template<class K> inline void originalParticleKernelInit(K k,int32_t n,int32_t blur,const set_piece_particles::Vec4& force,float span,float blurStep,float damp){
    using namespace set_piece_particles;using namespace particle_kernel;OriginalRounding rounding;
    const float count=float(n);k.setI(Blur,blur);
    span=eeMul(span,damp);k.setI(N,n);blurStep=eeMul(blurStep,damp);
    span=eeDivide(span,count);k.setF(AgeStep,span);
    const float q=vuDivide(1.f,damp);Vec4 fq=vMulS(vMulS(force,q),q);
    const float blurCount=float(k.i(Blur));
    k.setF(BlurStep,blurStep);k.setV(Force,fq);k.setF(Damp,damp);k.setF(InvBlur,eeDivide(1.f,blurCount));
    k.setV(Ones140,{1.f,1.f,0.f,0.f});k.setV(Zero130,kZero);
}
// 0x36CCB8: nine 0x3177F0 draws -> ((w & 0x7FFFFF) | 1.0) - 1 + 1.
template<class K> inline void originalParticleKernelSeeds(K k,OriginalRandomState& visual){
    using namespace set_piece_particles;OriginalRounding rounding;
    for(unsigned o:particle_kernel::seedOffsets){
        const float x=fbits((visual.next()&0x7FFFFFu)|0x3F800000u);
        k.setF(o,eeAdd(eeSub(x,1.f),1.f));
    }
}
// Emitter bounds 0x36D500 (a1 = 8 floats: min xyzw, max xyzw). Defined below.
template<class K> void originalParticleKernelBounds(K k,std::array<float,8>& bounds);
// 0x370058 (kernel, matrix, params, bounds, seed): seed < 0 draws 0x36CCB8, else seed -> +0x38 only.
template<class K> inline void originalParticleKernelPrepare(K k,const set_piece_particles::Mat4& m,const OriginalParticleParams& p,std::array<float,8>& bounds,float seed,OriginalRandomState& visual){
    using namespace set_piece_particles;using namespace particle_kernel;namespace P=particle_param;OriginalRounding rounding;
    { // 0x36CE00(size, sizeFinal, sizeR)
        const float size=p.f(P::Size),sizeFinal=p.f(P::SizeFinal),sizeR=p.f(P::SizeR);
        k.setF(SizeRange,sizeR);k.setF(SizeDelta,eeSub(sizeFinal,size));k.setF(SizeBase,eeSub(size,eeMul(sizeR,1.5f)));
    }
    { // 0x36CE28(life - lifeR/2, life + lifeR/2)
        const float half=eeMul(p.f(P::LifeR),.5f),mx=eeAdd(p.f(P::Life),half),mn=eeSub(p.f(P::Life),half);
        const float damp=k.f(Damp);float range=eeSub(mx,mn);const float lo=eeMul(mn,damp);range=eeMul(range,damp);
        k.setF(LifeRange,range);k.setF(LifeBase,eeSub(lo,range));
    }
    if(seed<0.f)originalParticleKernelSeeds(k,visual);else k.setF(Seed0,seed);
    k.setV(PosStep,kZero); // 0x36D008
    const Vec4 pos=vTransform(m,p.vec3(P::Off,1.f)),r0=vTransform(m,p.vec3(P::R0,0.f)),r1=vTransform(m,p.vec3(P::R1,0.f));
    k.setV(PosR0,r0);k.setV(PosR1,r1);k.setV(PosBase,vSub(pos,vMulS(vAdd(r0,r1),1.5f)));
    const float dq=k.f(0x4C); // Damp; VDIV Q = 1/Damp per vector
    auto scaled=[&](unsigned o){const Vec4 t=vTransform(m,p.vec3(o,0.f));return vMulS(t,vuDivide(1.f,p.f(P::Damp)));};
    (void)dq;
    const Vec4 vel=scaled(P::Vel),v0=scaled(P::R0V),v1=scaled(P::R1V),v2=scaled(P::R2V);
    k.setV(VelR0,v0);k.setV(VelR1,v1);k.setV(VelR2,v2);k.setV(VelBase,vSub(vel,vMulS(vAdd(vAdd(v0,v1),v2),1.5f)));
    const Vec4 start=vMulS(p.colour(P::StartCol),128.f),end=vMulS(p.colour(P::EndCol),128.f),c0=vMulS(p.colour(P::R0Col),128.f),c1=vMulS(p.colour(P::R1Col),128.f);
    { // 0x36D318(start,end,r0,r1, life + lifeR/2)
        const float maxLife=eeAdd(p.f(P::Life),eeMul(p.f(P::LifeR),.5f));
        k.setV(ColBase,vSub(start,vMulS(vAdd(c0,c1),1.5f)));
        const Vec4 d=vSub(end,start);const float span=eeMul(maxLife,k.f(Damp));
        k.setV(ColSlope,vMulS(d,vuDivide(1.f,span)));k.setV(ColR0,c0);k.setV(ColR1,c1);
    }
    float start0=0.f; // 0x36D3D8: continuous emitters start full (age = maxLife*Damp)
    if(p.f(P::Duration)<0.f)start0=eeAdd(p.f(P::Life),eeMul(p.f(P::LifeR),.5f));
    k.setF(Age,eeMul(start0,k.f(Damp)));
    originalParticleKernelBounds(k,bounds);
}
// 0x36D3E8: age += dt*Damp.
template<class K> inline void originalParticleKernelAdvance(K k,float dt){
    using namespace set_piece_particles;OriginalRounding rounding;
    k.setF(particle_kernel::Age,eeAdd(k.f(particle_kernel::Age),eeMul(dt,k.f(particle_kernel::Damp))));
}
// 0x36D428 (continuous): advance, then while N*step < age: age -= step, +0xB0 += +0xE0, seeds step.
template<class K> inline void originalParticleKernelAdvanceContinuous(K k,float dt){
    using namespace set_piece_particles;using namespace particle_kernel;
    originalParticleKernelAdvance(k,dt);OriginalRounding rounding;
    for(;;){
        const float window=eeMul(float(k.i(N)),k.f(AgeStep));
        if(!(window<k.f(Age)))break;
        k.setF(Age,eeSub(k.f(Age),k.f(AgeStep)));
        k.setV(PosBase,vAdd(k.v(PosBase),k.v(PosStep)));
        for(unsigned o:seedOffsets)k.setU(o,lfsrStep(k.u(o)));
    }
}

// ---- Static emitter (0x190; Particle effect +0x50) ---------------------------------------------
// +0x000 remaining s (Duration<0: Duration, never counts; else Duration+Life+LifeR/2), +0x004 texture
// id, +0x008 blend mode (table 0x44B420), +0x00C drawn-this-tick flag (set by every update, draw
// requires it), +0x010 kernel, +0x160 bounds min (xyzw) +0x170 max, +0x180 NumFlipTextures,
// +0x184 flip phase, +0x188 FlipTextureRate, +0x18C vtable 0x493160.
// Draw 0x3708C0: texture = texture table[texture + trunc(phase)] (renderer +0xF50), kernel +0x10.
namespace static_emitter { enum : unsigned { Remaining=0x0, Texture=0x4, Blend=0x8, Updated=0xC, Kernel=0x10,
    BoundsMin=0x160, BoundsMax=0x170, FlipCount=0x180, FlipPhase=0x184, FlipRate=0x188, Vtable=0x18C, Bytes=0x190 }; }
struct OriginalStaticEmitter : set_piece_particles::Image<0x190> {
    KernelRef<OriginalStaticEmitter> kernel(){return {*this,static_emitter::Kernel};}
    std::array<float,8> bounds()const{std::array<float,8> b;for(int k=0;k<8;k++)b[k]=f(static_emitter::BoundsMin+4*k);return b;}
};
// 0x370018 + 0x3705E0(emitter, matrix, params). Draws 9 values. `initial` = allocation contents.
inline OriginalStaticEmitter originalStaticEmitterConstruct(const OriginalParticleParams& p,const set_piece_particles::Mat4& m,OriginalRandomState& visual,const OriginalStaticEmitter& initial={}){
    using namespace set_piece_particles;using namespace static_emitter;namespace P=particle_param;
    OriginalStaticEmitter e=initial;
    e.setU(Vtable,0x493160u);e.setU(Updated,0);e.setF(FlipPhase,0.f);          // 0x370018
    e.setI(FlipCount,p.i(P::NumFlipTextures));e.setF(FlipRate,p.f(P::FlipTextureRate));
    e.setI(Texture,p.i(P::TextureId));e.setI(Blend,p.i(P::BlendMode));
    float span;
    {
        OriginalRounding rounding;const float duration=p.f(P::Duration);
        if(duration<0.f)e.setF(Remaining,duration);
        else e.setF(Remaining,eeAdd(eeAdd(duration,p.f(P::Life)),eeMul(p.f(P::LifeR),.5f)));
        span=duration;if(!(0.f<=span))span=eeAdd(p.f(P::Life),eeMul(p.f(P::LifeR),.5f));
    }
    auto k=e.kernel();
    originalParticleKernelInit(k,p.i(P::NumParticles),p.i(P::NumBlur),p.vec3(P::Force,0.f),span,p.f(P::BlurStep),p.f(P::Damp));
    std::array<float,8> b;for(int j=0;j<8;j++)b[j]=e.f(BoundsMin+4*j);
    originalParticleKernelPrepare(k,m,p,b,-1.f,visual);
    for(int j=0;j<8;j++)e.setF(BoundsMin+4*j,b[j]);
    return e;
}
// 0x370788(emitter, dt). fps = *(*(gp+0x2A74)+0x10) = 60 in game.
inline void originalStaticEmitterUpdate(OriginalStaticEmitter& e,float dt,int32_t fps=60){
    using namespace set_piece_particles;using namespace static_emitter;
    const float remaining=e.f(Remaining);
    if(remaining==0.f)return;
    auto k=e.kernel();
    {
        OriginalRounding rounding;
        if(remaining<0.f)originalParticleKernelAdvanceContinuous(k,eeDivide(1.f,float(fps)));
        else{
            originalParticleKernelAdvance(k,eeDivide(1.f,float(fps)));
            float r=eeSub(e.f(Remaining),eeDivide(1.f,float(fps)));
            e.setF(Remaining,r<0.f?0.f:r);
        }
    }
    OriginalRounding rounding;
    const float phase=eeAdd(e.f(FlipPhase),eeMul(e.f(FlipRate),dt));
    e.setF(FlipPhase,phase);if(!(eeTruncate(phase)<e.i(FlipCount)))e.setF(FlipPhase,0.f);
    e.setU(Updated,1);
}

// ---- Particle effect (0x4912B0, 0x1F0) ----------------------------------------------------------
struct OriginalParticleEffect {
    set_piece_particles::Mat4 matrix{}; // +0x10 owner matrix at creation (entity vt+0xC4 or instance+0x10)
    OriginalStaticEmitter emitter;      // +0x50
    uint32_t dead=0;                    // +0x1E4
    OriginalParticleParams params;      // *(+0x1E0) (kept: the draw never reads it)
};
// 0x3458C0 (+ 0x345798 base). Draws 9 values.
inline OriginalParticleEffect originalParticleEffectConstruct(const OriginalParticleParams& p,const set_piece_particles::Mat4& ownerMatrix,OriginalRandomState& visual,const OriginalStaticEmitter& initial={}){
    OriginalParticleEffect x;x.matrix=ownerMatrix;x.params=p;x.emitter=originalStaticEmitterConstruct(p,ownerMatrix,visual,initial);x.dead=0;return x;
}
// vt+0x2C 0x345B40 (dt = gp-0x2B54/-0x2B50 = 1/60).
inline void originalParticleEffectUpdate(OriginalParticleEffect& x,float dt=set_piece_particles::fbits(0x3C888889u)){
    if(x.dead)return;
    const bool infinite=x.emitter.f(static_emitter::Remaining)<0.f;
    originalStaticEmitterUpdate(x.emitter,dt);
    if(!infinite&&x.emitter.f(static_emitter::Remaining)<=0.f)x.dead=1;
}
inline bool originalParticleEffectDrawn(const OriginalParticleEffect& x){return !x.dead&&x.emitter.u(static_emitter::Updated)!=0;} // 0x345BC8/0x3708C0 gates (+ frustum)

// ---- Dynamic emitter (0x200 incl. vptr; DynamicParticle +0x60) ---------------------------------
// +0x000 remaining (Duration or Duration+Life+LifeR/2) +0x004 texture +0x008 blend +0x00C flip count
// +0x010 flip phase (random when count >= 2) +0x014 flip rate +0x020 kernel (N = NumParticles*capacity)
// +0x170 allocated (1) +0x174 enabled (1) +0x178 capacity = ceil((Life+LifeR/2)*60) +0x17C cursor
// (newest-first ring, decrements) +0x180 bounds min/max +0x1A0 ring A (capacity x {pos xyz, 0}),
// +0x1A4 ring B (capacity x {vel xyz, seed}), +0x1B0 last position +0x1C0 last velocity +0x1D0 last
// colour (0) +0x1E0 active births in the ring +0x1E4..+0x1F7 GS packet template/bits (0x371380)
// +0x1F8 vptr 0x491370 (the DynamicParticle overwrites the base 0x493118). +0x200 = DynamicParticle
// block pointer (not an emitter field for this class).
namespace dynamic_emitter { enum : unsigned { Remaining=0x0, Texture=0x4, Blend=0x8, FlipCount=0xC, FlipPhase=0x10,
    FlipRate=0x14, Kernel=0x20, Allocated=0x170, Enabled=0x174, Capacity=0x178, Cursor=0x17C, BoundsMin=0x180,
    BoundsMax=0x190, RingA=0x1A0, RingB=0x1A4, LastPosition=0x1B0, LastVelocity=0x1C0, LastColour=0x1D0,
    ActiveCount=0x1E0, Packet=0x1E4, Vtable=0x1F8, Bytes=0x200 }; }
struct OriginalDynamicEmitter : set_piece_particles::Image<0x200> {
    std::vector<set_piece_particles::Vec4> ringPosition,ringVelocity; // *(+0x1A0), *(+0x1A4); w: 0 / seed
    KernelRef<OriginalDynamicEmitter> kernel(){return {*this,dynamic_emitter::Kernel};}
};
// 0x370B60 + 0x370DC8(emitter, params, seed -1) (+ ring (re)allocation 0x370CF8/0x370D60/0x370C60).
// Draw order: [flip phase draw if NumFlipTextures >= 2] then the 9 kernel seeds.
inline OriginalDynamicEmitter originalDynamicEmitterConstruct(const OriginalParticleParams& p,OriginalRandomState& visual,const OriginalDynamicEmitter& initial={}){
    using namespace set_piece_particles;using namespace dynamic_emitter;namespace P=particle_param;
    OriginalDynamicEmitter e=initial;
    // 0x370B60
    e.setU(FlipCount,0);e.setU(FlipPhase,0);e.setU(FlipRate,0);e.setU(Vtable,0x493118u);e.setI(Capacity,-1);e.setU(Enabled,1);
    e.setU(Allocated,0);e.setU(Cursor,0);e.setU(RingA,0);e.setU(RingB,0);e.setU(ActiveCount,0);
    e.setU(Packet+0x10,(e.u(Packet+0x10)&0u)|0xFFFFFFFFu);for(unsigned o=Packet;o<Packet+0x10;o+=4)e.setU(o,0);
    OriginalRounding rounding;
    // 0x370DC8
    const int32_t flips=p.i(P::NumFlipTextures);e.setI(FlipCount,flips);e.setF(FlipRate,p.f(P::FlipTextureRate));
    if(flips>=2){
        const float count=float(flips);const float u=eeSub(fbits((visual.next()&0x7FFFFFu)|0x3F800000u),1.f);
        e.setF(FlipPhase,eeMul(count,u));
    }else e.setU(FlipPhase,0);
    e.setI(Texture,p.i(P::TextureId));e.setI(Blend,p.i(P::BlendMode));
    const float duration=p.f(P::Duration);
    e.setF(Remaining,duration<0.f?duration:eeAdd(eeAdd(duration,p.f(P::Life)),eeMul(p.f(P::LifeR),.5f)));
    bool reallocate=false;
    if(!(0.f<=duration)){
        const float ticks=eeMul(eeAdd(p.f(P::Life),eeMul(p.f(P::LifeR),.5f)),60.f);
        float t=float(eeTruncate(ticks));if(t<ticks)t=eeAdd(t,1.f);
        const int32_t capacity=eeTruncate(t);
        if(e.i(Capacity)!=capacity){e.setI(Capacity,capacity);reallocate=true;}
    }
    e.setV(LastPosition,kZero);e.setV(LastVelocity,kZero);e.setV(LastColour,kZero);
    const float span=duration<0.f?eeAdd(p.f(P::Life),eeMul(p.f(P::LifeR),.5f)):duration;
    auto k=e.kernel();
    originalParticleKernelInit(k,p.i(P::NumParticles)*e.i(Capacity),p.i(P::NumBlur),p.vec3(P::Force,0.f),span,p.f(P::BlurStep),p.f(P::Damp));
    std::array<float,8> b;for(int j=0;j<8;j++)b[j]=e.f(BoundsMin+4*j);
    originalParticleKernelPrepare(k,kIdentity,p,b,-1.f,visual);
    for(int j=0;j<8;j++)e.setF(BoundsMin+4*j,b[j]);
    if(reallocate){ // vt+0xC (0x370CF8 free), vt+0x2C (0x370C60 allocate), vt+0x14 (0x370D60 clear)
        const unsigned n=unsigned(e.i(Capacity));e.ringPosition.assign(n,kZero);e.ringVelocity.assign(n,kZero);
        e.setU(Cursor,0);e.setU(ActiveCount,0);
    }
    e.setU(Allocated,1);
    // 0x371034..0x3710A8: packet template from 0x501420 then bit edits.
    e.setU(Packet+0x0,0x00000000u);e.setU(Packet+0x4,0x00B00294u);e.setU(Packet+0x8,0x00000080u);e.setU(Packet+0xC,0);e.setU(Packet+0x10,0xFFFFFFFFu);
    uint32_t w1=e.u(Packet+0x4);w1=(w1&0xFFBFFFFFu)|0x00400000u;w1&=0xFFCFFFFFu;w1&=0xFFFFFFFCu;w1|=2u;e.setU(Packet+0x4,w1);
    e.setU(Packet+0x8,e.u(Packet+0x8)&0xE00003FFu);
    return e;
}
// 0x3710D0(emitter, position, velocity, active, dt): one 0x3177F0 draw when active.
inline void originalDynamicEmitterBirth(OriginalDynamicEmitter& e,const set_piece_particles::Vec4* position,const set_piece_particles::Vec4* velocity,bool active,float dt,OriginalRandomState& visual){
    using namespace set_piece_particles;using namespace dynamic_emitter;
    if(!e.u(Enabled))return;
    const unsigned slot=e.u(Cursor);
    // A finite-duration emitter never sizes its ring (0x370DC8 only sets the capacity for Duration < 0), so the original
    // writes through an unallocated ring (+RingA/+RingB 0, capacity -1): nothing is drawn from it. Keep the counters,
    // the cursor and the visual draw, skip the ring (ABC1 DynamicParticle at race tick 7231, docs/backcountry.md).
    const bool ring=slot<e.ringVelocity.size()&&slot<e.ringPosition.size();
    const bool wasActive=ring&&0.f<e.ringVelocity[slot][3];
    if(wasActive){if(!active)e.setI(ActiveCount,e.i(ActiveCount)-1);}
    else if(active)e.setI(ActiveCount,e.i(ActiveCount)+1);
    if(position)e.setV(LastPosition,*position);
    if(velocity)e.setV(LastVelocity,*velocity);
    Vec4 a=e.v(LastPosition);a[3]=0.f;if(ring)e.ringPosition[slot]=a;
    Vec4 b=e.v(LastVelocity);
    OriginalRounding rounding;
    if(active){const float x=fbits((visual.next()&0x7FFFFFu)|0x3F800000u);b[3]=eeAdd(eeSub(x,1.f),1.f);}else b[3]=0.f;
    if(ring)e.ringVelocity[slot]=b;
    int32_t c=e.i(Cursor)-1;if(c<0)c=e.i(Capacity)-1;e.setI(Cursor,c);
    const float phase=eeAdd(e.f(FlipPhase),eeMul(e.f(FlipRate),dt));
    e.setF(FlipPhase,phase);if(!(eeTruncate(phase)<e.i(FlipCount)))e.setU(FlipPhase,0);
}

// ---- DynamicParticle effect (0x491268, 0x270) ---------------------------------------------------
struct OriginalDynamicParticleEffect {
    int32_t node=0;                      // +0x10
    uint32_t stopped=0,dead=0;           // +0x14, +0x18
    set_piece_particles::Vec4 offset{};  // +0x20 (w 1)
    set_piece_particles::Vec4 worldPosition{},worldVelocity{}; // +0x30, +0x40
    set_piece_particles::Vec4 velocity{};// +0x50 (param Vel, w 0)
    OriginalDynamicEmitter emitter;      // +0x60
    OriginalParticleParams params;       // *(+0x260), with Vel zeroed
};
// 0x345EF8 with the carrier node world matrix (0x34FED8 result).
inline void originalDynamicParticleFollow(OriginalDynamicParticleEffect& x,const set_piece_particles::Mat4& node){
    using namespace set_piece_particles;OriginalRounding rounding;
    x.worldPosition=vTransform(node,x.offset);x.worldVelocity=vTransform(node,x.velocity);
}
// 0x345C90(obj, block, instance, node, &offset) + 0x345EF8. Draws [flip] + 9.
inline OriginalDynamicParticleEffect originalDynamicParticleConstruct(const OriginalParticleParams& block,const set_piece_particles::Mat4& nodeMatrix,OriginalRandomState& visual,const OriginalDynamicEmitter& initial={}){
    using namespace particle_param;OriginalDynamicParticleEffect x;x.params=block;
    x.node=block.i(Node);x.offset={block.f(Offset),block.f(Offset+4),block.f(Offset+8),1.f};
    x.stopped=0;x.velocity={block.f(Vel),block.f(Vel+4),block.f(Vel+8),0.f};
    x.params.setF(Vel,0.f);x.params.setF(Vel+4,0.f);x.params.setF(Vel+8,0.f);
    x.emitter=originalDynamicEmitterConstruct(x.params,visual,initial);
    x.emitter.setU(dynamic_emitter::Vtable,0x491370u);
    originalDynamicParticleFollow(x,nodeMatrix);x.dead=0;return x;
}
// vt+0x2C 0x345F90 (dt 1/60). `nodeMatrix` = this tick's carrier node world matrix (after the
// carrier's modifier / LiveComp update of this entity update).
inline void originalDynamicParticleUpdate(OriginalDynamicParticleEffect& x,const set_piece_particles::Mat4& nodeMatrix,OriginalRandomState& visual,float dt=set_piece_particles::fbits(0x3C888889u)){
    if(x.dead)return;
    originalDynamicParticleFollow(x,nodeMatrix);
    auto& e=x.emitter;using namespace dynamic_emitter;
    if(x.stopped){
        if(e.i(ActiveCount)>0)originalDynamicEmitterBirth(e,&x.worldPosition,&x.worldVelocity,false,dt,visual);
        else x.dead=1;
        return;
    }
    const bool infinite=e.f(Remaining)<0.f;
    originalDynamicEmitterBirth(e,&x.worldPosition,&x.worldVelocity,true,dt,visual);
    if(!infinite&&e.f(Remaining)<=0.f)x.dead=1;
}
inline void originalDynamicParticleStop(OriginalDynamicParticleEffect& x){x.stopped=1;} // vt+0x1C 0x346060

// ---- Carrier node matrix, static path of 0x34FED8 --------------------------------------------
// chain = local node matrices from the node up to its root (model node table *(instance+0x80)+8,
// record {parent, ?, ?, matrix*}); scale = instance+0x84; entity = entity vt+0xC4 matrix. Entities
// with animated nodes (LiveComp vt+0xD4 != 0) use vt+0xEC(node) instead (engine/livecomp_animation).
inline set_piece_particles::Mat4 originalParticleNodeMatrix(const std::vector<set_piece_particles::Mat4>& chain,float scale,const set_piece_particles::Mat4& entity){
    using namespace set_piece_particles;OriginalRounding rounding;
    if(chain.empty())return entity;
    auto scaled=[&](Mat4 m){m[3]={eeMul(m[3][0],scale),eeMul(m[3][1],scale),eeMul(m[3][2],scale),eeMul(m[3][3],1.f)};return m;};
    Mat4 out=scaled(chain[0]);
    for(size_t i=1;i<chain.size();i++)out=mProduct(out,scaled(chain[i]));
    return mProduct(out,entity);
}
} // namespace ssx
#include "generated/set_piece_particle_bounds.inc" // tools/test_set_piece_particles_lift.py (from the ELF, git-ignored)
