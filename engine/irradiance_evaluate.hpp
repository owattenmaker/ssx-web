#pragma once
#include "irradiance.hpp"
namespace ssx {
// Program2 upper04B0 ITOF15 followed by04E0/04E8/04F0. Columns are the
// supplied normal transform, not an inverse-transpose constructed here.
inline std::array<float,3> originalIrradianceTransformNormal(
    const std::array<int32_t,3>& packed,const std::array<std::array<float,3>,3>& columns){
    terrain_original::Rounding rounding;
    std::array<float,3> decoded{},result{};
    for(unsigned i=0;i<3;++i)decoded[i]=terrain_original::mul(float(packed[i]),1.f/32768.f);
    for(unsigned i=0;i<3;++i){
        float value=terrain_original::mul(columns[0][i],decoded[0]);
        value=terrain_original::add(value,terrain_original::mul(columns[1][i],decoded[1]));
        result[i]=terrain_original::add(value,terrain_original::mul(columns[2][i],decoded[2]));
    }
    return result;
}
// Program2 VU entry1150 scales the uploaded coefficients by255. The vertex
// lighting block04F8..05B0 evaluates the normal polynomial in source op order.
// Input normal is already transformed by the vertex path; do not normalize here.
// Output remains float byte-space RGBA, before packing/material/GS operations.
inline std::array<float,4> originalIrradianceEvaluate(
    const OriginalIrradianceCoefficients& coefficients,const std::array<float,3>& normal){
    terrain_original::Rounding rounding;
    const auto bank=originalIrradianceScale(coefficients,255.f);
    const float x=normal[0],y=normal[1],z=normal[2];
    const std::array<float,10> weights={1.f,
        terrain_original::mul(x,x),terrain_original::mul(y,y),terrain_original::mul(z,z),
        terrain_original::mul(x,y),terrain_original::mul(z,x),terrain_original::mul(y,z),x,y,z};
    std::array<float,4> result{};
    for(unsigned lane=0;lane<4;++lane){
        float value=terrain_original::mul(bank[0][lane],1.f);
        for(unsigned row=1;row<10;++row)value=terrain_original::add(value,terrain_original::mul(bank[row][lane],weights[row]));
        value=value>0.f?value:0.f;
        result[lane]=value<255.f?value:255.f;
    }
    return result;
}
// Program2 upper0668 FTOI0 truncates the already clamped lanes for vertex color.
inline std::array<uint8_t,4> originalIrradianceVertexColor(
    const OriginalIrradianceCoefficients& coefficients,const std::array<float,3>& normal){
    const auto value=originalIrradianceEvaluate(coefficients,normal);
    return {uint8_t(value[0]),uint8_t(value[1]),uint8_t(value[2]),uint8_t(value[3])};
}

}
