#include "air_motion.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <bit>
#include <cfenv>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON

namespace ssx {
namespace {
using ScopedRounding=OriginalRounding;
}
OriginalAirState OriginalAirState::fromNative(Vec3 p,Vec3 v) {
    ScopedRounding rounding(FE_TONEAREST);
    return {{{float(p.x*100),float(-p.z*100),float(p.y*100)}},
            {{float(v.x*100),float(-v.z*100),float(v.y*100)}}};
}
Vec3 OriginalAirState::nativePosition() const {return {double(position[0])/100,double(position[2])/100,-double(position[1])/100};}
Vec3 OriginalAirState::nativeVelocity() const {return {double(velocity[0])/100,double(velocity[2])/100,-double(velocity[1])/100};}
bool OriginalAirState::step(float maximumSpeed,float* resultingSpeed) {
    for (float value:position) if (!std::isfinite(value)) throw std::runtime_error("Nonfinite airborne position");
    for (float value:velocity) if (!std::isfinite(value)) throw std::runtime_error("Nonfinite airborne velocity");
    if (!std::isfinite(maximumSpeed)||maximumSpeed<=0) throw std::runtime_error("Invalid airborne speed cap");
    ScopedRounding rounding(FE_TOWARDZERO);
    constexpr float dt=std::bit_cast<float>(0x3c888889u),drag=std::bit_cast<float>(0xbb5a740fu);
    constexpr float down=std::bit_cast<float>(0xc1fd5556u),up=std::bit_cast<float>(0xc162aaabu);
    for (unsigned i=0;i<3;++i) {
        float displacement=terrain_original::mul(velocity[i],dt);
        position[i]=terrain_original::add(position[i],displacement);
    }
    float dx=terrain_original::mul(velocity[0],drag),dy=terrain_original::mul(velocity[1],drag);
    float dz=velocity[2]>0?up:down;
    velocity[0]=terrain_original::add(velocity[0],dx);velocity[1]=terrain_original::add(velocity[1],dy);velocity[2]=terrain_original::add(velocity[2],dz);
    float x2=terrain_original::mul(velocity[0],velocity[0]),y2=terrain_original::mul(velocity[1],velocity[1]),z2=terrain_original::mul(velocity[2],velocity[2]);
    float squared=terrain_original::add(x2,y2);squared=terrain_original::add(squared,z2);
    float speed=terrain_original::sqrt(squared);
    if (speed>maximumSpeed) {
        // Recovered source branch; one live capped trajectory is bit-identical in native tests.
        float scale=originalScalarDivide(maximumSpeed,speed);
        for (float& value:velocity) value=terrain_original::mul(value,scale);
        if(resultingSpeed)*resultingSpeed=maximumSpeed;
        return true;
    }
    if(resultingSpeed)*resultingSpeed=speed;
    return false;
}
}
