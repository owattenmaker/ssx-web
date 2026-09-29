#pragma once
#include "terrain_contact_math.hpp"
#include <span>
namespace ssx {
using OriginalWakeNoiseTable=std::array<float,161>;
float originalWakeNoise(const OriginalWakeNoiseTable&,float phase); //2D18B0, nonnegative wake phase
float originalWakeOctaves(const OriginalWakeNoiseTable&,float phase,int octaves=3); //2D1928
struct OriginalWakeProfile {
 int columns=5,capacity=32;float lifetime=1.25f;
 std::array<float,5> verticalIncrement{},textureV{};
};
OriginalWakeProfile originalWakeProfile(bool deviceBound); //2DCF28 configuration/lookup initialization
struct OriginalWakeCursor {int count=0,head=0,capacity=32;float phase=0;};
void originalWakeAdvanceCursor(OriginalWakeCursor&); //2DE058..2DE0D4
struct OriginalWakeRowInput {
 terrain_original::Vector coefficient60{},coefficient70{},direction80{},velocity{},point{},normal{0,0,1};
 float amplitude90=0,alpha94=0,value60=0,phase=0;
 int columns=5; //2DCF28: human5, nonhuman4
};
struct OriginalWakeRow {
 std::array<terrain_original::Vector,5> velocity{};
 terrain_original::Vector drag50{},anchor{}; // anchor is the birth position
 std::array<terrain_original::Vector,5> positions{}; // mutable render-band positions
 float value60=0,age64=0;int32_t alpha=0;
 float textureU=0;std::array<uint32_t,3> rgb{};
};
// Complete2DDD30 row payload and repeated render-anchor/alpha construction.
// Caller owns the allocated ring/vertex buffers and their unchanged RGB/UVs.
OriginalWakeRow originalWakeRow(const OriginalWakeRowInput&,const OriginalWakeNoiseTable&);
}

namespace ssx {
//2DD0B8..2DD2A0 plus its2DD378 expiry branch. Iterate newest to oldest;
// the first expired row truncates the retained count and stops the pass.
void originalWakeAgeRows(OriginalWakeCursor&,std::span<OriginalWakeRow>,int columns,float lifetime,
    const std::array<float,5>& verticalIncrement);
}
