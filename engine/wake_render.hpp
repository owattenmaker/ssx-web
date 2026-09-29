#pragma once
#include "wake_row.hpp"
namespace ssx {
struct OriginalWakeVisual {std::array<float,4> environment{};std::array<uint32_t,3> rgb{};float u=0;};
OriginalWakeVisual originalWakeVisual(std::array<float,4> environmentARGB,int32_t counter); //2DE110..2DE348
struct OriginalWakeDrawWindow {int start=0,count=0;float fade=1.25f,step=0;};
OriginalWakeDrawWindow originalWakeDrawWindow(const OriginalWakeCursor&,std::span<const OriginalWakeRow>,float lifetime); //2DDC68..2DDCE4
// VU1 3A08 advances the unclamped fade after every row. Multiplying the
// ordinal by step changes rounding before the integer alpha conversion.
inline std::array<float,32> originalWakeRowFades(const OriginalWakeDrawWindow& window){
 terrain_original::Rounding rounding;std::array<float,32> result{};float fade=window.fade;
 for(int i=0;i<window.count&&i<32;++i){result[i]=fade;fade=terrain_original::add(fade,window.step);}
 return result;
}
}
