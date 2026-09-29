#pragma once
#include "terrain_contact_math.hpp"
namespace ssx {
//Program2 upper04C0..04D8: supplied matrix times source float XYZW vertex.
//This precedes projection/perspective divide; no unit conversion belongs here.
inline std::array<float,4> originalSkinVertex(const std::array<float,4>& point,const std::array<float,16>& matrix){
 terrain_original::Rounding rounding;using namespace terrain_original;std::array<float,4> out;
 for(unsigned row=0;row<4;++row){float value=mul(matrix[row],point[0]);for(unsigned col=1;col<4;++col)value=add(value,mul(matrix[col*4+row],point[col]));out[row]=value;}return out;
}
}
