#pragma once
// Shared types between the RollerModifier dynamics (engine/roller_modifier.hpp,
// 0x35DA70..0x361D44) and its sphere-tree world query (engine/roller_world_query.hpp,
// 0x3303F0/0x336850). Source Z-up centimeters.
#include "sphere_tree_collision.hpp"
#include <array>
#include <cstdint>

namespace ssx {
using RollerQuad=std::array<float,4>;

// 0x32C508 sphere-tree collider object (RollerModifier+0xE0; 3303F0 also
// allocates a scratch one per query). Layout offsets are the original ones.
struct OriginalSphereTreeCollider {
    std::array<RollerQuad,8> corners{};      // +0x00 32C648: M*(0x4FF640[i]*scale), corners[7-i] = -corners[i]
    RollerQuad center{};                     // +0x80 32C648: M*(tree center*scale, 1)
    float scale=1;                           // +0x90
    float depth=0;                           // +0x94 32CA78 scratch (best signed depth)
    const CollisionSphereTree* tree=nullptr; // +0x98 327CC8 (sphere tree of the hit node)
    uint32_t treeResource=0;                 // 32DF28 mask-cache key of *tree (not an original field)
    std::array<int32_t,8> order{};           // +0x9C 32DB40 scratch (child visit order)
};

// RollerModifier+0x2C0: the 32B6E0 terrain cell cache passed through 336850
// (32B6E0 t3/s6). The cached cell is tried first when +0x0 is the queried patch
// and +0xC equals query+0; a miss (or a degenerate cached triangle) clears all
// five fields to 0 before the full scan, a scan hit writes all five, a cached
// hit leaves them untouched. A cache that names another patch is left as is.
struct OriginalRollerTerrainCache {
    uint32_t patch=0;        // +0x0 patch identity. The original stores the patch pointer; the
                             //      port stores the patch resource id (patch+0x150, never 0).
                             //      0 or any value naming no patch = no cached cell.
    uint16_t cellU=0,cellV=0;// +0x4/+0x6 cell row/column (coarse: 0..2, 9x9 grid rows/3)
    uint32_t half=0;         // +0x8 1: triangle (D,B,C) of the cell, 0: triangle (C,B,A)
    uint32_t detailed=0;     // +0xC query+0 flag the cell was found with (roller: 0 = coarse)
};

// The 336850 result fields the roller reads (packet +0x00 point, +0x10 normal,
// +0x40 depth; depth -1 when there is no contact).
struct OriginalRollerWorldContact {
    bool hit=false;
    RollerQuad point{},normal{};
    float depth=-1;
    // Diagnostics of the selected packet (not read by the roller).
    bool terrain=false;uint32_t resource=0;unsigned node=0; // patch+0x150 or instance+0x78, model node
    unsigned contacts=0;                                      // 0x335D78 packet count
};
}
