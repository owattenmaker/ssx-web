#pragma once
// The set-piece particle sprites (web/set-piece-particle-eval.js natively): the core exports each drawable effect's kernel and
// birth ring (stage_world_particles); the sprites are evaluated with the structure of the original VU1 program 0x439A40 (entries
// 0x000 burst / 0x510 trail) the way the page does: the per-particle accumulators (age, time, fraction, lifetime) in the VU / EE
// single-precision operations, the per-sprite arithmetic in doubles (the page's JS numbers).
#include "ee_float.hpp"
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

extern "C" {
uint32_t* stage_world_particles();
}

namespace ssx_native {

namespace particles {

// x, y, z (source cm), half size (cm), r, g, b, a (GS integers, 128 = 1)
constexpr size_t kSpriteFloats = 8;
constexpr double kTMax = 2.700000047683716;
constexpr double kPolyA = -0.7300000190734863;
constexpr double kPolyB = 0.11299999803304672;

// VU DIV (round toward zero)
inline float vuDiv(float a, float b) {
    const double q = double(a) / double(b);
    float r = float(q);
    if (!std::isfinite(q) || r == 0) {
        return r;
    }
    if (std::abs(double(r) * double(b)) > std::abs(double(a))) {
        r = ee::fromBits(ee::bits(r) - 1);
    }
    return r;
}

// JS ToInt32 (Math.trunc(x) & 255 on a double)
inline uint32_t toInt32(double x) {
    if (!std::isfinite(x)) {
        return 0;
    }
    const double t = std::trunc(x);
    const double m = std::fmod(t, 4294967296.0);
    const double u = m < 0 ? m + 4294967296.0 : m;
    return uint32_t(uint64_t(u));
}

inline double clampByte(double v) {
    return std::trunc(std::min(std::max(v, 0.0), 255.0));
}

// RINIT / RNEXT (23-bit LFSR)
inline uint32_t lfsr(uint32_t s) {
    const uint32_t t = (s & 0x7fffffu) | 0x3f800000u;
    return ((((t << 1) ^ ((t >> 4) & 1) ^ ((t >> 22) & 1)) & 0x7fffffu) | 0x3f800000u);
}

struct Effect {
    uint32_t kind = 0, texture = 0, blend = 0, resource = 0;
    const uint32_t* K = nullptr;
    const float* F = nullptr;
    uint32_t capacity = 0, cursor = 0;
    const float* ringA = nullptr;
    const float* ringB = nullptr;
    const uint32_t* ringBits = nullptr;
};

// web/stage_world.inc stage_world_particles: [count, then per effect: kind, texture, blend, resource, 84 kernel words, and for a
// trail: capacity, cursor, ring A (4 x capacity), ring B (4 x capacity)]
inline std::vector<Effect> readEffects() {
    std::vector<Effect> out;
    const uint32_t* U = stage_world_particles();
    if (!U) {
        return out;
    }
    const uint32_t n = U[0];
    size_t at = 1;
    for (uint32_t k = 0; k < n; ++k) {
        Effect e;
        e.kind = U[at];
        e.texture = U[at + 1];
        e.blend = U[at + 2];
        e.resource = U[at + 3];
        at += 4;
        e.K = U + at;
        e.F = reinterpret_cast<const float*>(U + at);
        at += 84;
        if (e.kind == 1) {
            e.capacity = U[at];
            e.cursor = U[at + 1];
            at += 2;
            e.ringA = reinterpret_cast<const float*>(U + at);
            e.ringB = reinterpret_cast<const float*>(U + at + 4 * e.capacity);
            e.ringBits = U + at + 4 * e.capacity;
            at += 8 * size_t(e.capacity);
        }
        out.push_back(e);
    }
    return out;
}

// entry 0x000 (380518): the burst kernel
inline void burst(const Effect& e, std::vector<double>& out) {
    const uint32_t* K = e.K;
    const float* F = e.F;
    const int slots = int32_t(K[0]);
    const int blur = int32_t(K[1]);
    const float sizeRange = F[4], lifeRange = F[5], lifeBase = F[7];
    const double sizeBase = F[6], sizeDelta = F[8], alphaStep = F[12];
    const float blurStep = F[13];
    float age = F[2];
    const float period = F[3];
    const double fx = F[24], fy = F[25], fz = F[26], vbx = F[28], vby = F[29], vbz = F[30];
    // the nine streams (words 14..18, 20..23)
    std::vector<float> R(size_t(std::max(slots, 0)) * 9);
    for (int k = 0; k < 9; ++k) {
        uint32_t s = K[k < 5 ? 14 + k : 15 + k];
        for (int slot = 0; slot < slots; ++slot) {
            s = lfsr(s);
            R[size_t(slot) * 9 + size_t(k)] = ee::fromBits(s);
        }
    }
    double slotIndex = 0;
    for (int slot = 0; slot < slots; ++slot) {
        const double r38 = R[size_t(slot) * 9], r3C = R[size_t(slot) * 9 + 1], r40 = R[size_t(slot) * 9 + 2], r44 = R[size_t(slot) * 9 + 3],
                     r48 = R[size_t(slot) * 9 + 4], r50 = R[size_t(slot) * 9 + 5], r54 = R[size_t(slot) * 9 + 6], r58 = R[size_t(slot) * 9 + 7];
        const float r5C = R[size_t(slot) * 9 + 8];
        const float life = ee::vuAdd(lifeBase, ee::mul(lifeRange, r5C));
        if (age < life) {
            const double a = age;
            const double ca0 = double(F[60]) + double(F[64]) * r50 + double(F[72]) * a + double(F[68]) * r54;
            const double ca1 = double(F[61]) + double(F[65]) * r50 + double(F[73]) * a + double(F[69]) * r54;
            const double ca2 = double(F[62]) + double(F[66]) * r50 + double(F[74]) * a + double(F[70]) * r54;
            const double ca3 = double(F[63]) + double(F[67]) * r50 + double(F[75]) * a + double(F[71]) * r54;
            const double size = std::abs(sizeDelta * a * double(vuDiv(1, life)) + sizeBase + double(sizeRange) * r58);
            const double px = double(F[44]) + double(F[56]) * slotIndex + double(F[48]) * r38 + double(F[52]) * r3C;
            const double py = double(F[45]) + double(F[57]) * slotIndex + double(F[49]) * r38 + double(F[53]) * r3C;
            const double pz = double(F[46]) + double(F[58]) * slotIndex + double(F[50]) * r38 + double(F[54]) * r3C;
            const double vx = vbx + double(F[32]) * r40 + double(F[36]) * r44 + double(F[40]) * r48;
            const double vy = vby + double(F[33]) * r40 + double(F[37]) * r44 + double(F[41]) * r48;
            const double vz = vbz + double(F[34]) * r40 + double(F[38]) * r44 + double(F[42]) * r48;
            const double r = clampByte(ca0), g = clampByte(ca1), b = clampByte(ca2);
            double fade = 1;
            float t = age;
            for (int count = blur;;) {
                const double t2 = std::min(double(t), kTMax);
                const double poly = t2 * kPolyA + t2 * t2 * kPolyB;
                out.insert(out.end(), {fx * t + px + (fx - vx) * poly, fy * t + py + (fy - vy) * poly, fz * t + pz + (fz - vz) * poly, size, r, g, b,
                                       double(toInt32(ca3 * fade) & 255)});
                fade -= alphaStep;
                t = ee::vuSub(t, blurStep);
                if (t < 0) {
                    break;
                }
                if (--count <= 0) {
                    break;
                }
            }
        }
        age = ee::vuSub(age, period);
        slotIndex += 1;
        if (age < 0) {
            break;
        }
    }
}

// one ring row of the trail kernel: its births from age on; returns the row's age afterwards
inline float trailRow(const float* F, int blur, std::vector<double>& out, uint32_t bits, int per, float age, double px, double py, double pz, double vx,
                      double vy, double vz, double nx, double ny, double nz, double nvx, double nvy, double nvz, float fractionStep) {
    const float ageStep = F[3], lifeRange = F[5], lifeBase = F[7];
    const double alphaStep = F[12], sizeRange = F[4], sizeBase = F[6], sizeDelta = F[8];
    const float blurStep = F[13];
    // the row's LFSR stream (9 draws per birth) and the VU accumulators
    std::vector<float> R(size_t(per) * 9);
    for (size_t q = 0; q < R.size(); ++q) {
        bits = lfsr(bits);
        R[q] = ee::fromBits(bits);
    }
    float fraction = 0;
    for (int i = 0; i < per; ++i) {
        const float* r = R.data() + size_t(i) * 9;
        const float lifetime = ee::vuAdd(lifeBase, ee::mul(lifeRange, r[8]));
        const double inverse = vuDiv(1, lifetime);
        const double a = age;
        const double fr = fraction;
        const double c = 1 - fr;
        const double bvx = vx * c + nvx * fr, bvy = vy * c + nvy * fr, bvz = vz * c + nvz * fr;
        const double bpx = px * c + nx * fr, bpy = py * c + ny * fr, bpz = pz * c + nz * fr;
        const double p0x = bpx + F[44] + double(F[56]) * a + double(F[48]) * r[0] + double(F[52]) * r[1];
        const double p0y = bpy + F[45] + double(F[57]) * a + double(F[49]) * r[0] + double(F[53]) * r[1];
        const double p0z = bpz + F[46] + double(F[58]) * a + double(F[50]) * r[0] + double(F[54]) * r[1];
        if (age < lifetime) {
            const double wx = bvx + F[28] + double(F[32]) * r[2] + double(F[36]) * r[3] + double(F[40]) * r[4];
            const double wy = bvy + F[29] + double(F[33]) * r[2] + double(F[37]) * r[3] + double(F[41]) * r[4];
            const double wz = bvz + F[30] + double(F[34]) * r[2] + double(F[38]) * r[3] + double(F[42]) * r[4];
            const double ca3 = double(F[63]) + double(F[67]) * r[5] + double(F[75]) * a + double(F[71]) * r[6];
            const double cr = clampByte(double(F[60]) + double(F[64]) * r[5] + double(F[72]) * a + double(F[68]) * r[6]);
            const double cg = clampByte(double(F[61]) + double(F[65]) * r[5] + double(F[73]) * a + double(F[69]) * r[6]);
            const double cb = clampByte(double(F[62]) + double(F[66]) * r[5] + double(F[74]) * a + double(F[70]) * r[6]);
            const double fvx = F[24] - wx, fvy = F[25] - wy, fvz = F[26] - wz;
            const double size = std::abs(sizeDelta * a * inverse + sizeBase + sizeRange * r[7]);
            double fade = 1;
            float t = age;
            for (int count = blur;;) {
                const double t2 = std::min(double(t), kTMax);
                const double poly = t2 * kPolyA + t2 * t2 * kPolyB;
                const double alpha = double(toInt32(ca3 * fade) & 255);
                fade -= alphaStep;
                out.insert(out.end(), {double(F[24]) * t + p0x + fvx * poly, double(F[25]) * t + p0y + fvy * poly, double(F[26]) * t + p0z + fvz * poly,
                                       size, cr, cg, cb, alpha});
                t = ee::vuSub(t, blurStep);
                if (t < 0) {
                    break;
                }
                if (--count <= 0) {
                    break;
                }
            }
        }
        age = ee::vuAdd(age, ageStep);
        fraction = ee::vuAdd(fraction, fractionStep);
    }
    return age;
}

// entry 0x510 (3807A0): the trail birth ring (ring A positions, ring B velocities with the seed in w)
inline void trail(const Effect& e, std::vector<double>& out) {
    const int cap = int32_t(e.capacity);
    if (cap <= 0) {
        return;
    }
    const int per = int32_t(e.K[0]) / cap;
    if (per <= 0) {
        return;
    }
    const float* F = e.F;
    const float ageStep = F[3];
    const int blur = int32_t(e.K[1]);
    const float fractionStep = vuDiv(1, float(per));
    const int batches = (cap >> 6) + 1;
    int done = 0;
    const int cursor = int(e.cursor);
    for (int batch = 0; batch < batches; ++batch) {
        int rows64 = cap - done;
        if (rows64 >= 65) {
            rows64 = 64;
        }
        float age = ee::mul(ee::mul(float(per), float(batch << 6)), ageStep);
        const int first = (done + cursor + 1) % cap;
        done += rows64;
        const int neighbour = done < cap ? (done + cursor + 1) % cap : (done - 1 + cursor + 1) % cap;
        for (int j = 0; j <= rows64; ++j) {
            const int slot = j < rows64 ? (first + j) % cap : neighbour;
            const bool extra = j == rows64;
            const int nextSlot = !extra ? (j + 1 < rows64 ? (first + j + 1) % cap : neighbour) : slot;
            const uint32_t seedBits = e.ringBits[slot * 4 + 3];
            const float seed = e.ringB[slot * 4 + 3];
            if (ee::vuSub(seed, 1) < 0) {
                age = ee::vuAdd(age, ee::mul(ageStep, float(per)));
                continue;
            }
            const double px = e.ringA[slot * 4], py = e.ringA[slot * 4 + 1], pz = e.ringA[slot * 4 + 2];
            const double vx = e.ringB[slot * 4], vy = e.ringB[slot * 4 + 1], vz = e.ringB[slot * 4 + 2];
            double nx, ny, nz;
            if (!extra) {
                nx = e.ringA[nextSlot * 4];
                ny = e.ringA[nextSlot * 4 + 1];
                nz = e.ringA[nextSlot * 4 + 2];
            } else if (rows64 == 64) {
                const int s0 = first % cap;
                nx = e.ringB[s0 * 4];
                ny = e.ringB[s0 * 4 + 1];
                nz = e.ringB[s0 * 4 + 2];
            } else {
                nx = px;
                ny = py;
                nz = pz;
            }
            const double nvx = !extra ? double(e.ringB[nextSlot * 4]) : vx;
            const double nvy = !extra ? double(e.ringB[nextSlot * 4 + 1]) : vy;
            const double nvz = !extra ? double(e.ringB[nextSlot * 4 + 2]) : vz;
            age = trailRow(F, blur, out, seedBits, per, age, px, py, pz, vx, vy, vz, nx, ny, nz, nvx, nvy, nvz, fractionStep);
        }
    }
}

using Combo = std::tuple<uint32_t, uint32_t, uint32_t>;

// particleCombinations: every (texture, blend, kind) a course can draw, in the page's order (the renderer's mesh creation order)
inline std::vector<Combo> combinations(const nlohmann::json& doc) {
    std::vector<Combo> out;
    auto add = [&](uint32_t texture, uint32_t blend, uint32_t kind) {
        const Combo c{texture, blend, kind};
        if (std::find(out.begin(), out.end(), c) == out.end()) {
            out.push_back(c);
        }
    };
    for (const auto& b : doc["blocks"]) {
        const auto& w = b["words"];
        const uint32_t frames = std::max<uint32_t>(1, w[52].get<uint32_t>());
        for (uint32_t f = 0; f < frames; ++f) {
            add(w[49].get<uint32_t>() + f, w[50].get<uint32_t>(), b["builtin"].get<int>() == 26 ? 1 : 0);
        }
    }
    for (const auto& e : doc["initial"]["effects"]) {
        if (e["kind"] != "Particle") {
            continue;
        }
        const uint32_t frames = std::max<uint32_t>(1, e["emitter"][0x180 / 4].get<uint32_t>());
        for (uint32_t f = 0; f < frames; ++f) {
            add(e["emitter"][1].get<uint32_t>() + f, e["emitter"][2].get<uint32_t>(), 0);
        }
    }
    if (doc.contains("multi")) {
        for (const auto& g : doc["multi"]) {
            const uint32_t frames = std::max<uint32_t>(1, g["emitter"][0x180 / 4].get<uint32_t>());
            for (uint32_t f = 0; f < frames; ++f) {
                add(g["emitter"][1].get<uint32_t>() + f, g["emitter"][2].get<uint32_t>(), 0);
            }
        }
    }
    return out;
}

}  // namespace particles

}  // namespace ssx_native
