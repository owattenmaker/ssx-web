#pragma once
// Original SSX3 (PS2 USA, SLUS_207.72) stage builtin 7 "Boost" (0x2FBEC8; entity type 8, 0x50 bytes, vtable 0x4914E0):
// the one-way volumes of the hub connectors (onewayvolume_*). Every call on the disc is
// builtin7(key1 2, key3 0.9, key4 100, key5..7 int 0,1,0) from the volume's section-enter program: mode 2 pushes a
// human rider whose velocity is not already along the volume's +Y (instance row 1) by +Y * 100 km/h * 0.9 * dt, armed
// 60 updates after the entity was built. Port of 0x341388 (ctor), 0x3415D0 (update), 0x341818 / 0x341770 (velocity
// change) and 0x1250A8 (rider velocity add); the rider list rider+0x5B8 (104E70, 0x108C28) and the air predictor
// restart (0x1135B8 when 11FE98 == 1) are the host's.
// Float policy: EE mul.s / cvt: terrain_original::mul + truncation; EE add.s / sub.s: originalScalarAdd/Subtract (guard
// bit); VU lanes: terrain_original add/mul under OriginalRounding (chop); VRSQRT 1/sqrt.
#include "terrain_contact_math.hpp"
#include "original_float.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <optional>

namespace ssx {
using BoostQuad=std::array<float,4>;
struct OriginalBoostArgs {int32_t instance=-1,mode=0;float duration=1.f,gain=0.f,speedKmh=1.f;BoostQuad direction{0,1,0,0};}; // 0x4FB4C8
struct OriginalBoostVolume {BoostQuad direction{};float speed=0,gain=0;int32_t duration=0,countdown=0,mode=0;};
namespace one_way_volume {
inline const float kKmhToCms=std::bit_cast<float>(0x41de38e4u); // gp-0x2B98 27.777779
inline const float kDt=std::bit_cast<float>(0x3c888889u);       // [[gp-0x848]+0x14] 1/60
inline BoostQuad vmul(const BoostQuad& a,float s){BoostQuad r;for(unsigned k=0;k<4;k++)r[k]=terrain_original::mul(a[k],s);return r;}
inline float vdot(const BoostQuad& a,const BoostQuad& b){ // vmul, vadday.x, vmaddaz.x (1*z), vmaddw.x (1*w)
    using namespace terrain_original;
    const float px=mul(a[0],b[0]),py=mul(a[1],b[1]),pz=mul(a[2],b[2]),pw=mul(a[3],b[3]);
    return add(add(add(px,py),mul(1.f,pz)),mul(1.f,pw));
}
inline BoostQuad vnormalize(const BoostQuad& v){ // s = ((x2+y2)+1*z2)+1*w2; Q = VRSQRT(1, s); v * Q
    const float s=vdot(v,v);const float q=s>0?terrain_original::div(1.f,terrain_original::sqrt(s)):std::bit_cast<float>(0x7F7FFFFFu);
    return vmul(v,q);
}
}

// 0x2FBEC8 keyed parse (types 0x445F70 {1,1,2,2,2,2,2,2,1}): (key, word, type 1 int / 2 float).
struct OriginalBoostKey {int32_t key=0;uint32_t word=0;int32_t type=0;};
inline OriginalBoostArgs originalBoostArgs(const OriginalBoostKey* keys,int count){
    std::array<uint32_t,9> f{0xFFFFFFFFu,0u,std::bit_cast<uint32_t>(1.f),0u,std::bit_cast<uint32_t>(1.f),0u,std::bit_cast<uint32_t>(1.f),0u,0u};
    static constexpr int32_t types[9]={1,1,2,2,2,2,2,2,1};
    for(int i=0;i<count;i++){const auto& a=keys[i];if(a.key<0||a.key>8)continue;
        if(a.type==types[a.key]||types[a.key]!=2)f[size_t(a.key)]=a.word;else{OriginalRounding r;f[size_t(a.key)]=std::bit_cast<uint32_t>(float(int32_t(a.word)));}}
    OriginalBoostArgs a;a.instance=int32_t(f[0]);a.mode=int32_t(int16_t(f[1]&0xFFFFu)); // lh key1
    a.duration=std::bit_cast<float>(f[2]);a.gain=std::bit_cast<float>(f[3]);a.speedKmh=std::bit_cast<float>(f[4]);
    a.direction={std::bit_cast<float>(f[5]),std::bit_cast<float>(f[6]),std::bit_cast<float>(f[7]),0.f};return a;
}
// 0x341388 (rows = instance+0x10, the authored unscaled matrix; fps = [[gp+0x2A74]+0x10] = 60).
inline OriginalBoostVolume originalBoostConstruct(const OriginalBoostArgs& a,const std::array<BoostQuad,4>& rows,int32_t fps=60){
    using namespace terrain_original;OriginalRounding rounding;OriginalBoostVolume v;
    v.mode=a.mode;v.gain=a.gain;v.speed=mul(a.speedKmh,one_way_volume::kKmhToCms);
    v.duration=int32_t(mul(a.duration,float(fps)));if(v.mode!=0)v.countdown=v.duration; // mode 0 leaves +0x3C unwritten
    const BoostQuad& d=a.direction;
    if(d[0]==0.f&&d[1]==0.f&&d[2]==0.f)v.direction=v.mode==2?BoostQuad{0,1,0,0}:BoostQuad{0,0,0,0}; // 0x4FF150 / 0x4FF120
    else v.direction=one_way_volume::vnormalize(d);
    if(v.mode==2){ // n = dir * -1; dir = n.x*r0 + n.y*r1 + n.z*r2 + n.w*r3 (VMULAx / VMADDAy / VMADDAz / VMADDw); speed = -speed
        const BoostQuad n=one_way_volume::vmul(v.direction,-1.f);BoostQuad r;
        for(unsigned k=0;k<4;k++){float acc=mul(rows[0][k],n[0]);acc=add(acc,mul(rows[1][k],n[1]));acc=add(acc,mul(rows[2][k],n[2]));r[k]=add(acc,mul(rows[3][k],n[3]));}
        v.direction=r;v.speed=-v.speed;
    }
    return v;
}
// 0x3415D0 head: countdown / keep-alive / whether the rider loop runs this update.
struct OriginalBoostStep {bool runLoop=false,keepAlive=false;};
inline OriginalBoostStep originalBoostBegin(OriginalBoostVolume& v){
    OriginalBoostStep s;s.keepAlive=v.mode!=1;
    if(v.countdown>0){--v.countdown;if(v.countdown>=1)s.keepAlive=true;}
    s.runLoop=!(v.mode!=1&&v.countdown>0);return s;
}
// 0x341818 (gain >= 0) / 0x341770 (gain < 0): the velocity change for a rider in the volume, or none.
inline std::optional<BoostQuad> originalBoostPush(const OriginalBoostVolume& v,const BoostQuad& velocity,float dt=one_way_volume::kDt){
    using namespace terrain_original;OriginalRounding rounding;
    if(v.gain<0.f){const float g=originalScalarAdd(v.gain,1.f);return one_way_volume::vmul(one_way_volume::vmul(velocity,g),dt);}
    BoostQuad d=v.direction;if(d[0]==0.f&&d[1]==0.f&&d[2]==0.f)d=one_way_volume::vnormalize(velocity);
    const float dot=one_way_volume::vdot(velocity,d),f1=originalScalarSubtract(v.speed,dot);
    if(v.mode==2){if(!(0.f<=dot))return std::nullopt;return one_way_volume::vmul(one_way_volume::vmul(one_way_volume::vmul(d,v.speed),v.gain),dt);}
    if(!(0.f<f1))return std::nullopt;
    return one_way_volume::vmul(one_way_volume::vmul(one_way_volume::vmul(d,f1),v.gain),dt);
}
// 0x1250A8: rider+0x1E0 += dv (VU add, 4 lanes).
inline BoostQuad originalBoostAddVelocity(BoostQuad velocity,const BoostQuad& dv){OriginalRounding rounding;for(unsigned k=0;k<4;k++)velocity[k]=terrain_original::add(velocity[k],dv[k]);return velocity;}
}
