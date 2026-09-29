#pragma once
#include "terrain_contact_math.hpp"
namespace ssx {
struct OriginalRiderQueryBounds {std::array<float,4> minimum{},maximum{},sphere{};};
//11E150: position is the explicit override when one is supplied, otherwise
//rider+110. Axes are physical right/forward/up, not the animated bone frame.
inline OriginalRiderQueryBounds originalRiderQueryBounds(const std::array<float,4>& position,
    const std::array<float,4>& right,const std::array<float,4>& forward,const std::array<float,4>& up){
    terrain_original::Rounding rounding;OriginalRiderQueryBounds result{position,position,position};
    for(unsigned corner=0;corner<8;++corner){
        for(unsigned axis=0;axis<3;++axis){
            const float f=terrain_original::mul(forward[axis],150.f),r=terrain_original::mul(right[axis],100.f);
            const float u=terrain_original::mul(up[axis],corner&1?50.f:250.f);
            float point=corner&4?terrain_original::sub(position[axis],f):terrain_original::add(position[axis],f);
            point=corner&2?terrain_original::sub(point,r):terrain_original::add(point,r);
            point=corner&1?terrain_original::sub(point,u):terrain_original::add(point,u);
            if(point<result.minimum[axis])result.minimum[axis]=point;
            if(result.maximum[axis]<point)result.maximum[axis]=point;
        }
    }
    result.sphere[3]=250.f;return result;
}
}
