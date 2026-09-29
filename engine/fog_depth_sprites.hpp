#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace ssx {
//36B054..36B0E4. Coordinates use GS 12.4 fixed point. columns8 is the
//source width/8 count prepared by36AEBC, not the number of emitted sprites.
inline std::vector<std::array<uint64_t,8>> originalFogDepthSprites(int columns8,uint32_t height,
 uint32_t originX16,uint32_t originY16){
 std::vector<std::array<uint64_t,8>> result;
 for(int column=0;column<columns8;column+=2){
  uint64_t u=8+uint32_t(column)*128u;
  uint64_t x=originX16+128u+uint32_t(column)*128u;
  uint64_t bottom=uint64_t(originY16+(height<<4))<<16;
  result.push_back({u|(uint64_t(8)<<16),3,x|(uint64_t(originY16)<<16),5,
                    (u+128)|((uint64_t(height<<4)+8)<<16),3,(x+128)|bottom,5});
 }
 return result;
}
}
