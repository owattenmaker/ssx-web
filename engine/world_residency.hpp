#pragma once
// Streamed-location residency of the original world (docs/peak-mountain.md).
//
// The PS2 keeps whole LOCATIONS (ELF location table 0x43E250, SDB track = resource & 0xFF) in the world only
// while their streaming row (table 0x442168, driven by 22CEA8/22D088 from the residency table 0x442488)
// is loaded: a location's terrain patches, instances and splines are in the octree the world queries walk
// only between its load and its unload. The browser keeps every location it has fetched and gates each world
// query on the resident flag of the entry's track instead of rebuilding the lists, so the queries see exactly
// the original's resident set on every tick (web/peak_world.inc sets the flags from the streaming state).
//
// Every track is resident by default: a single-residency race event (web/prepare.py packages) is unchanged.
#include <array>
#include <cstdint>

namespace ssx {
inline std::array<uint8_t,256>& worldTrackResidency(){
    static std::array<uint8_t,256> tracks=[]{std::array<uint8_t,256> a{};a.fill(1);return a;}();
    return tracks;
}
// resource = rid<<8 | track (terrain patches, instances, rail packed ids).
inline bool worldResident(uint32_t resource){return worldTrackResidency()[resource&255u]!=0;}
// Tracks whose instances are in the section-activation octree (0x101B60): from the chunk read completion (resolver
// kind 3 -> 328C20) until the streamer's unload start (row 5 -> 7, 230360 -> 103308 + stage teardown). Only read by
// the section activation of a streamed world (engine/section_streaming.hpp Activation::present).
inline std::array<uint8_t,256>& worldTrackOctree(){
    static std::array<uint8_t,256> tracks=[]{std::array<uint8_t,256> a{};a.fill(1);return a;}();
    return tracks;
}
}
