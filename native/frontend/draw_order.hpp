#pragma once
// The PS2 render list's draw order for the post-fog effects (web/ps2-draw-order.js, docs/visual-parity.md 41.9): the 0x364240 key
// and the three.js renderOrder the page gives each effect from it. Only the event boot's FX handles (renderer +0xF50); the Conquer
// the Mountain table joins with free ride.
#include <array>
#include <cstdint>

namespace ssx_native {

namespace draw_order {

// renderer +0xF50 in PS2 RAM (setpieces-abc1/full tick 2000, every event state)
inline constexpr std::array<int, 80> kEventHandles = {
    1525, -1,   -1,   -1,   1524, 1523, 1522, 1521, 1520, -1,   1519, 1518, -1,   1517, 1516, 1515, 1514, 1513, 1512, 1511,
    1510, 1509, 1508, 1507, 1506, 1505, 1565, 1564, 1563, 1562, 1561, 1560, 1559, -1,   -1,   -1,   -1,   1558, 1557, 1556,
    1555, 1554, 1553, 1552, 1551, 1550, 1549, -1,   -1,   -1,   -1,   1548, 1547, 1546, 1545, 1544, 1653, 1652, 1651, 1650,
    1649, 1648, 1647, 1646, 1645, 1644, 1643, 1642, 1641, -1,   1640, 1639, 1638, 1637, 1636, -1,   -1,   -1,   -1,   -1};
// 0x492010 over word0 bits 6..9
inline constexpr std::array<uint32_t, 9> kModeRank = {0, 4, 5, 3, 1, 6, 2, 2, 1};

inline int fxHandle(int id) {
    return id >= 0 && id < int(kEventHandles.size()) ? kEventHandles[size_t(id)] : -1;
}

struct Spec {
    uint32_t priority = 7;
    uint32_t t0 = 0;
    uint32_t mode = 0;
    int fx = -1;
    int fx2 = -1;
};

// 0x364240: the unsigned 32-bit key
inline uint32_t drawKey(const Spec& s) {
    const int handle = fxHandle(s.fx);
    const int handle2 = fxHandle(s.fx2);
    uint32_t tex = handle >= 0 ? uint32_t(handle) : 0;
    if (handle2 >= 0) {
        tex |= (uint32_t(handle2) & 0xF) << 6;
    }
    const uint32_t rank = s.mode < kModeRank.size() ? kModeRank[s.mode] : 0;
    const uint32_t x = (31 - s.priority) << 26 | (s.t0 & 0x3FF) << 16 | rank << 13 | (tex & 0x3FF) << 3;
    return ~x;
}

// three.js renderOrder for a post-fog effect (priorities 6 / 7 / 8 in [660, 720)); submission orders equal keys
inline double drawOrder(const Spec& s, double submission) {
    return 660 + (double(drawKey(s)) - double(0x98000000u)) / double(0x0C000000u) * 60 + submission * 1e-8;
}

// web/ps2-draw-order.js EFFECT / SUBMIT for the rider effects ported so far
inline Spec snow(int fx) {
    return {7, 0, 4, fx, -1};
}
inline Spec wake() {
    return {7, 0, 3, 56, -1};
}
inline Spec boost(int fx) {
    return {7, 0, 3, fx, -1};
}
inline Spec aura() {
    return {7, 0, 3, 62, -1};
}
inline Spec streamer(int fx) {
    return {8, 1023, 3, fx, -1};
}
inline constexpr double kSubmitWake = 10;
inline constexpr double kSubmitBoost = 13;
inline constexpr double kSubmitAura = 14;
inline constexpr double kSubmitStreamer = 15;
inline constexpr double kSubmitSnow = 18;

}  // namespace draw_order

}  // namespace ssx_native
