#pragma once
// The rider shadows' fit and receiver rows (web/rider-shadow.js natively; docs/terrain-render-fidelity.md "Rider shadows"):
// 1225F0 -> 122898 fits a box around six bones in the straight-down light space (374D00, float32 like the EE); the silhouette goes into
// the rider's 128 x 128 slot of the atlas (rider_renderer.cpp drawShadow) and the terrain subtracts it (world_renderer.cpp terrain:
// Cd = max(Cd - T, 0) within the 30 m slab below the feet). At most six riders, the nearest to the camera whose 3 m sphere is in view.
#include "rider_renderer.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace ssx_native {

namespace shadow {

constexpr int kMax = 6;
constexpr int kBones[6] = {5, 10, 15, 18, 21, 0};
constexpr float kMargin = 70;
constexpr float kValue = 32.f / 255.f;

using V3 = std::array<float, 3>;

inline float f(double x) {
    return float(x);
}
inline float dot3(const V3& a, const V3& b) {
    return f(f(f(a[0] * b[0]) + f(a[1] * b[1])) + f(a[2] * b[2]));
}
inline V3 cross(const V3& a, const V3& b) {
    return {f(double(a[1]) * b[2] - double(a[2]) * b[1]), f(double(a[2]) * b[0] - double(a[0]) * b[2]), f(double(a[0]) * b[1] - double(a[1]) * b[0])};
}
inline void normalize(V3& a) {
    const double n = std::hypot(double(a[0]), double(a[1]), double(a[2]));
    if (n > 0) {
        for (float& c : a) {
            c = f(c / n);
        }
    }
}

struct Fit {
    V3 A{}, B{}, C{};
    float ac = 0, bc = 0, minx = 0, maxx = 0, miny = 0, maxy = 0;
};

// 374D00: bones (head, hands, feet, hips; PS2 cm), up: the physical frame's +Z (rider+0x1C0)
inline std::optional<Fit> fitShadow(const std::array<V3, 6>& bones, const V3& up) {
    Fit out;
    V3 N = {f(up[0]), f(up[1]), f(up[2])};
    normalize(N);
    for (size_t k = 0; k < 3; ++k) {
        out.C[k] = f(f(bones[3][k] + bones[4][k]) * 0.5);
    }
    if (N[0] == 0 && N[1] == 0 && std::abs(N[2]) == 1) {
        N[0] = f(N[0] + 0.01);
        normalize(N);
    }
    const V3 L = {0, 0, -1};
    out.A = cross(N, L);
    normalize(out.A);
    out.B = cross(L, out.A);
    normalize(out.B);
    out.ac = dot3(out.A, out.C);
    out.bc = dot3(out.B, out.C);
    double minx = INFINITY, maxx = -INFINITY, miny = INFINITY, maxy = -INFINITY;
    for (const auto& p : bones) {
        const float x = f(double(dot3(out.A, p)) - out.ac);
        const float y = f(double(dot3(out.B, p)) - out.bc);
        minx = std::min(minx, double(x));
        maxx = std::max(maxx, double(x));
        miny = std::min(miny, double(y));
        maxy = std::max(maxy, double(y));
    }
    out.minx = f(minx - kMargin);
    out.maxx = f(maxx + kMargin);
    out.miny = f(miny - kMargin);
    out.maxy = f(maxy + kMargin);
    return out;
}

// the silhouette rows over the skinned source position (rider_renderer.hpp ShadowSlot)
inline ShadowSlot slotRows(const Fit& fit, int slot, bool grounded) {
    ShadowSlot s;
    const double w = double(fit.maxx) - fit.minx;
    const double h = double(fit.maxy) - fit.miny;
    const double mx = (double(fit.maxx) + fit.minx) / 2;
    const double my = (double(fit.maxy) + fit.miny) / 2;
    s.rowX = {float(fit.A[0] * 2 / w), float(fit.A[1] * 2 / w), float(fit.A[2] * 2 / w), float((-double(fit.ac) - mx) * 2 / w)};
    s.rowY = {float(-fit.B[0] * 2 / h), float(-fit.B[1] * 2 / h), float(-fit.B[2] * 2 / h), float((double(fit.bc) + my) * 2 / h)};
    s.slot = float(slot);
    s.board = grounded ? 0.f : kValue;
    return s;
}

// receiverRows: u, v and the depth below the feet over the scene's origin-relative metres
inline std::array<std::array<float, 4>, 3> receiverRows(const Fit& fit, const std::array<double, 3>& origin) {
    const double ox = origin[0], oy = origin[1], oz = origin[2];
    const auto& A = fit.A;
    const auto& B = fit.B;
    const double w = double(fit.maxx) - fit.minx;
    const double h = double(fit.maxy) - fit.miny;
    const double a0 = 100 * double(A[0]), a1 = 100 * double(A[2]), a2 = -100 * double(A[1]), a3 = 100 * (A[0] * ox - A[1] * oz + A[2] * oy);
    const double b0 = 100 * double(B[0]), b1 = 100 * double(B[2]), b2 = -100 * double(B[1]), b3 = 100 * (B[0] * ox - B[1] * oz + B[2] * oy);
    return {{{float(a0 / w), float(a1 / w), float(a2 / w), float((a3 - fit.ac - fit.minx) / w)},
             {float(-b0 / h), float(-b1 / h), float(-b2 / h), float((fit.maxy - b3 + fit.bc) / h)},
             {0.f, -100.f, 0.f, float(double(fit.C[2]) - 100 * oy)}}};
}

}  // namespace shadow

}  // namespace ssx_native
