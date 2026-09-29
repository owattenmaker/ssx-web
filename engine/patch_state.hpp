#pragma once
#include <array>
#include <cstdint>
namespace ssx {
// Rider state copied from the contacted terrain patch (docs/locations.md "The Junction super pipe").
// rider+0x2D4 = patch+0xA (runtime flags = authored | 0x40), written by 13D1B8 (13D604); rider+0x430 = patch+0x150
// (rid<<8 | track); 1218D0 (per rider from 128AC0) sets rider+0x434 = 22E0E0(track byte of +0x430) when +0x430 != -1.
constexpr uint16_t kOriginalPatchRuntimeFlag=0x40;
constexpr int kOriginalStreamingRows=50;
// Original 22E0E0: the index of the first streaming-table row (0x442168, 16-byte rows) whose +4 (the loaded track
// slot, -1 when the location is not resident) equals track; 50 when none. The index is the ELF location id (0x43E250).
inline int originalStreamingLocationIndex(const std::array<int32_t,kOriginalStreamingRows>& rowTrack,int32_t track){
    for(int k=0;k<kOriginalStreamingRows;k++)if(rowTrack[k]==track)return k;
    return kOriginalStreamingRows;
}
}
