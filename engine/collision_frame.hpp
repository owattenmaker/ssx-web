#pragma once
#include <array>
namespace ssx {
// Original pre-impact 11FA10 presentation frame (rider+160..190), before the
// separate body-only presentation lift. This is not a posed bone transform.
struct OriginalCollisionFrame {
    std::array<float,3> right{1,0,0},forward{0,1,0},up{0,0,1},origin{};
};
}
