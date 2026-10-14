#pragma once
// The terrain snow sparkle (web/terrain-sparkle.js natively; docs/presentation.md 14): the patches of terrain-sparkle.bin near the
// camera (0x38D968 / 0x38D690, the level-8 edge test of 0x22C410), each patch's sprites from VU1 program 4 at 0x1460 (the RNG chain
// in the VU's chop arithmetic, P(u, v) in doubles like the page), the twinkle rotation and glint vector from the camera, the
// Surface painter's density (world painter type 10). Output: the sprites (origin-relative metres, alpha / 128) the world pass
// draws at renderOrder 640 (GS ALPHA 0x49: Cd + Cd x As).
#include "ee_float.hpp"
#include "particles.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace ssx_native {

class TerrainSparkle {
public:
    static constexpr size_t kPatchWords = 56;

    TerrainSparkle(const std::string& path, const std::array<double, 3>& origin) : origin_(origin) {
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            return;
        }
        bytes_.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (bytes_.size() < 16 || std::memcmp(bytes_.data(), "SPKL", 4) != 0) {
            throw std::runtime_error("Not a terrain sparkle file");
        }
        const uint32_t version = u32(4);
        if (version != 1 && version != 2) {
            throw std::runtime_error("Not a terrain sparkle file");
        }
        n_ = u32(8);
        words_.resize(n_ * kPatchWords);
        std::memcpy(words_.data(), bytes_.data() + 16, words_.size() * 4);
        corners_.resize(n_ * 12);
        base_.resize(n_);
        counts_.resize(n_);
        for (size_t i = 0; i < n_; ++i) {
            const float* w = words_.data() + i * kPatchWords;
            patchCorners(w, corners_.data() + i * 12);
            base_[i] = countBase(w + 48, w + 51);
        }
        if (version == 2) {
            surfaceTree(u32(12));
        }
        setDensity(1);
        loaded_ = true;
    }
    bool loaded() const {
        return loaded_;
    }

    // per drawn frame: the camera (three's matrixWorld columns right / up / back, eye; projection), fog_info (null: no camera tick)
    // and weather_info()[46] (the reset placements). Returns the sprites (x, y, z origin-relative m, alpha / 128).
    const std::vector<float>& update(const double* world, const double* projection, const float* fogInfo, float resets) {
        out_.clear();
        if (!loaded_) {
            return out_;
        }
        const double p[3] = {world[12], world[13], world[14]};
        eye_[0] = float((p[0] + origin_[0]) * 100);
        eye_[1] = float(-(p[2] + origin_[2]) * 100);
        eye_[2] = float((p[1] + origin_[1]) * 100);
        auto norm = [](double x, double y, double z) {
            const double l = std::sqrt(x * x + y * y + z * z);
            return std::array<double, 3>{x / l, y / l, z / l};
        };
        const auto right = norm(world[0], world[1], world[2]);
        const auto up = norm(world[4], world[5], world[6]);
        const auto back = norm(world[8], world[9], world[10]);
        stepDensity(fogInfo, resets);
        const double sx = 0.25 * projection[0];
        const double sy = 0.21875 * projection[5];
        const auto T = twinkle();
        const double a[4] = {sx * right[1], sy * up[1], -back[1], -back[1]};
        double s[3];
        for (size_t k = 0; k < 3; ++k) {
            s[k] = T[0][k] * a[0] + T[1][k] * a[1] + T[2][k] * (a[2] + a[3]);
        }
        const double lim = 3000.0 * 3000.0 * 1.001;
        for (size_t i = 0; i < n_; ++i) {
            const float* C = corners_.data() + i * 12;
            bool near = false;
            for (size_t q = 0; q < 12 && !near; q += 3) {
                const double dx = double(eye_[0]) - C[q];
                const double dy = double(eye_[1]) - C[q + 1];
                const double dz = double(eye_[2]) - C[q + 2];
                if (dx * dx + dy * dy + dz * dz <= lim) {
                    near = true;
                }
            }
            if (!near || counts_[i] <= 0 || !level8Edge(C)) {
                continue;
            }
            const auto& S = spritesOf(i);
            const size_t end = std::min(S.size(), size_t(counts_[i]) * 6);
            for (size_t k = 0; k < end; k += 6) {
                const double dot = double(S[k + 3]) * s[0] + double(S[k + 4]) * s[1] + double(S[k + 5]) * s[2];
                if (dot < 0) {
                    continue;
                }
                const double alpha = std::trunc(std::min(dot, 1.0) * 128);
                if (alpha <= 0) {
                    continue;
                }
                out_.insert(out_.end(), {float(double(S[k]) / 100 - origin_[0]), float(double(S[k + 2]) / 100 - origin_[1]),
                                         float(-double(S[k + 1]) / 100 - origin_[2]), float(alpha / 128)});
            }
        }
        return out_;
    }

private:
    uint32_t u32(size_t at) const {
        uint32_t v;
        std::memcpy(&v, bytes_.data() + at, 4);
        return v;
    }
    float f32(size_t at) const {
        float v;
        std::memcpy(&v, bytes_.data() + at, 4);
        return v;
    }
    // the four record corners P(0,0), P(0,1), P(1,0), P(1,1) from the rows (row q = coefficient 15 - q)
    static void patchCorners(const float* rows, float* out) {
        auto c = [&](int k, int a) { return double(rows[(15 - k) * 3 + a]); };
        for (int a = 0; a < 3; ++a) {
            double u1 = 0, v1 = 0, all = 0;
            for (int i = 0; i < 4; ++i) {
                u1 += c(i, a);
                v1 += c(4 * i, a);
                for (int j = 0; j < 4; ++j) {
                    all += c(i + 4 * j, a);
                }
            }
            out[a] = float(c(0, a));
            out[3 + a] = float(v1);
            out[6 + a] = float(u1);
            out[9 + a] = float(all);
        }
    }
    static float countBase(const float* lo, const float* hi) {
        using namespace ee;
        const float x = sub(lo[0], hi[0]);
        const float y = sub(lo[1], hi[1]);
        const float z = sub(lo[2], hi[2]);
        return mul(add(add(mul(x, x), mul(y, y)), mul(z, z)), mul(8, fromBits(0x34b2f4fc)));
    }
    void setDensity(float density) {
        if (density == densityApplied_) {
            return;
        }
        for (size_t i = 0; i < n_; ++i) {
            counts_[i] = std::max(0, int(std::trunc(ee::mul(base_[i], density))));
        }
        densityApplied_ = density;
    }
    // 22C1B0 / 0x22C410: an edge is at level 8 when b + k (dA^2 + dB^2) >= 3
    bool level8Edge(const float* C) const {
        using namespace ee;
        static const float twoR0 = add(mul(3000, 3000), mul(3000, 3000));
        static const float k = div(sub(1, 3), sub(add(mul(15000, 15000), mul(15000, 15000)), twoR0));
        static const float b = sub(3, mul(k, twoR0));
        auto dist2 = [&](const float* c) {
            const float x = sub(eye_[0], c[0]);
            const float y = sub(eye_[1], c[1]);
            const float z = sub(eye_[2], c[2]);
            return add(add(mul(x, x), mul(y, y)), mul(z, z));
        };
        const float d0 = dist2(C), d1 = dist2(C + 3), d2 = dist2(C + 6), d3 = dist2(C + 9);
        auto level8 = [&](float a, float bb) { return add(b, mul(k, add(a, bb))) >= 3; };
        return level8(d0, d1) || level8(d2, d3) || level8(d0, d2) || level8(d1, d3);
    }
    // program 4 at 0x1460 without the camera: P(u, v) + 12 cm and the random vector n (a cached chain serves any smaller count)
    const std::vector<float>& spritesOf(size_t i) {
        auto it = cache_.find(i);
        if (it != cache_.end() && it->second.size() >= size_t(counts_[i]) * 6) {
            return it->second;
        }
        return cache_[i] = sprites(words_.data() + i * kPatchWords, counts_[i]);
    }
    static std::vector<float> sprites(const float* rows, int count) {
        using ee::fromBits;
        auto add = [](float a, float b) { return ee::vuAdd(a, b); };
        auto sub = [](float a, float b) { return ee::vuSub(a, b); };
        auto mul = [](float a, float b) { return ee::mul(a, b); };
        auto R = [&](int q, int a) { return rows[q * 3 + a]; };
        const float I1 = fromBits(0x3f85a5a5);
        const float r0[3] = {R(0, 0), R(0, 1), R(0, 2)};
        const float r1[3] = {R(1, 0), R(1, 1), R(1, 2)};
        const float r2[3] = {R(2, 0), R(2, 1), R(2, 2)};
        const float r3z = R(3, 2);
        const float w = 1;
        const float vf14[3] = {add(r1[0], r2[0]), add(r1[1], r2[1]), add(r1[2], r2[2])};
        const float vf17a[3] = {add(r0[0], r1[0]), add(r0[1], r1[1]), add(r0[2], r1[2])};
        float n0 = add(r1[0], w);
        float n1 = add(r2[1], r1[1]);
        float n2 = add(r3z, w);
        const float vf16[3] = {add(vf17a[0], I1), add(vf17a[1], I1), add(vf17a[2], I1)};
        float vf17[3] = {add(vf14[0], I1), add(vf14[1], I1), add(vf14[2], I1)};
        const float u0z = add(vf16[2], vf16[1]);
        vf17[2] = add(vf17[2], vf17[1]);
        n0 = add(n0, I1);
        n1 = add(n1, I1);
        n2 = add(n2, I1);
        float uz = add(u0z, vf16[0]);
        float vz = add(vf17[2], vf17[0]);
        float u3 = 0, u2 = 0, v3 = 0;
        auto seed = [](float x) { return (ee::bits(x) & 0x7fffffu) | 0x3f800000u; };
        std::vector<float> out(size_t(std::max(count, 0)) * 6);
        for (int k = 0; k < count; ++k) {
            n2 = add(n2, u3);
            n0 = add(n0, u2);
            n1 = add(n1, vz);
            float s = add(n2, v3);
            n0 = add(n0, n1);
            n1 = add(n1, n2);
            n2 = fromBits(particles::lfsr(seed(s)));
            const uint32_t rx = particles::lfsr(seed(n0));
            s = add(uz, vz);
            n0 = fromBits(rx);
            n1 = fromBits(particles::lfsr(seed(n1)));
            const float sv = add(vz, s);
            const float ru = fromBits(particles::lfsr(seed(s)));
            const float nx = sub(n0, 1.5f), ny = sub(n1, 1.5f), nz = sub(n2, 1.5f);
            const float rv = fromBits(particles::lfsr(seed(sv)));
            const float u = sub(ru, 1), v = sub(rv, 1);
            u2 = mul(u, u);
            const float v2 = mul(v, v);
            v3 = mul(v2, v);
            u3 = mul(u2, u);
            n0 = nx;
            n1 = ny;
            n2 = nz;
            uz = u;
            vz = v;
            // P(u, v) in doubles (the page's fast path)
            const size_t o = size_t(k) * 6;
            for (int a = 0; a < 3; ++a) {
                auto p = [&](int q) {
                    return ((double(rows[a + q * 3]) * u + rows[a + q * 3 + 3]) * u + rows[a + q * 3 + 6]) * u + rows[a + q * 3 + 9];
                };
                out[o + size_t(a)] = float(((p(0) * v + p(4)) * v + p(8)) * v + p(12) + (a == 2 ? 12 : 0));
            }
            out[o + 3] = nx;
            out[o + 4] = ny;
            out[o + 5] = nz;
        }
        return out;
    }
    // 38B794..38BAF0: the twinkle rotation from the camera position (Rodrigues about the unit axes)
    std::array<std::array<double, 3>, 3> twinkle() const {
        using M3 = std::array<std::array<double, 3>, 3>;
        auto rod = [](int axis, double t) {
            const double c = std::cos(t), s = std::sin(t);
            double a[3] = {0, 0, 0};
            a[axis] = 1;
            const double K[3][3] = {{0, -a[2], a[1]}, {a[2], 0, -a[0]}, {-a[1], a[0], 0}};
            M3 m{};
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    m[size_t(i)][size_t(j)] = c * (i == j ? 1 : 0) + (1 - c) * a[i] * a[j] + s * K[i][j];
                }
            }
            return m;
        };
        auto mm = [](const M3& A, const M3& B) {
            M3 o{};
            for (size_t i = 0; i < 3; ++i) {
                for (size_t j = 0; j < 3; ++j) {
                    o[i][j] = A[i][0] * B[0][j] + A[i][1] * B[1][j] + A[i][2] * B[2][j];
                }
            }
            return o;
        };
        const double twinkle = ee::fromBits(0xbb9374bc);
        M3 M = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
        const int pairs[3][2] = {{0, 1}, {1, 0}, {2, 2}};
        for (const auto& pr : pairs) {
            M = mm(rod(pr[0], double(eye_[size_t(pr[1])]) * twinkle), M);
        }
        return M;
    }

    // ---- world painter type 10, tWPIGD_Surface ----
    void surfaceTree(uint32_t at) {
        if (!at) {
            return;
        }
        tree_.scale = f32(at);
        tree_.origin = {f32(at + 4), f32(at + 8)};
        tree_.root = u32(at + 12);
        const uint32_t nodeCount = u32(at + 16);
        tree_.outside = u32(at + 24);
        const uint32_t payloadCount = u32(at + 28);
        for (uint32_t i = 0; i < nodeCount; ++i) {
            std::array<uint16_t, 4> node{};
            for (int k = 0; k < 4; ++k) {
                uint16_t v;
                std::memcpy(&v, bytes_.data() + at + 32 + i * 8 + k * 2, 2);
                node[size_t(k)] = v;
            }
            tree_.nodes.push_back(node);
        }
        const size_t p = at + 32 + nodeCount * 8;
        for (uint32_t i = 0; i < payloadCount; ++i) {
            tree_.payloads.push_back({f32(p + i * 8), f32(p + i * 8 + 4)});
        }
        hasTree_ = true;
    }
    int surfacePayload(float x, float y) const {
        const float px = float(float(x - tree_.origin[0]) * tree_.scale);
        const float py = float(float(y - tree_.origin[1]) * tree_.scale);
        const std::array<uint16_t, 4>* leaf = nullptr;
        if (px > -1 && px < 32768 && py > -1 && py < 32768) {
            uint32_t a = (uint32_t(int(std::trunc(px))) << 1) & 0xffff;
            uint32_t b = (uint32_t(int(std::trunc(py))) << 1) & 0xffff;
            uint32_t index = tree_.root;
            for (size_t steps = 0; steps <= tree_.nodes.size(); ++steps) {
                if (index >= tree_.nodes.size()) {
                    throw std::runtime_error("Surface painter node outside the tree");
                }
                const auto& node = tree_.nodes[index];
                if (!(node[0] & 1)) {
                    leaf = &node;
                    break;
                }
                index = node[((a >> 15) << 1) | (b >> 15)] >> 1;
                a = (a << 1) & 0xfffc;
                b = (b << 1) & 0xfffc;
            }
            if (!leaf) {
                throw std::runtime_error("Cyclic Surface painter tree");
            }
        }
        const uint32_t value = leaf ? (uint32_t((*leaf)[2]) | (uint32_t((*leaf)[3]) << 16)) : tree_.outside;
        return value == 0xffffffffu || value >= tree_.payloads.size() ? -1 : int(value);
    }
    void painterStep(float x, float y) {
        using namespace ee;
        const bool initial = distance_ == -99999.f;
        if (!initial) {
            const float dx = sub(lastX_, x), dy = sub(lastY_, y);
            distance_ = add(distance_, float(std::sqrt(double(add(mul(dx, dx), mul(dy, dy))))));
        }
        lastX_ = x;
        lastY_ = y;
        auto reset = [&] {
            current_ = 1;
            distance_ = 0;
        };
        if (!hasTree_) {
            reset();
            return;
        }
        const int i = surfacePayload(x, y);
        if (i < 0) {
            reset();
            return;
        }
        const float rate = tree_.payloads[size_t(i)][0];
        const float value = tree_.payloads[size_t(i)][1];
        auto blend = [&](float weight) {
            const float w = mul(weight, weight);
            current_ = add(mul(w, value), mul(sub(1, w), current_));
        };
        if (current_ == value) {
            distance_ = 0;
        }
        if (initial) {
            blend(-1);
            distance_ = 0;
            return;
        }
        if (rate >= 0 && rate <= distance_) {
            blend(1);
            distance_ = 0;
        } else {
            blend(-rate);
        }
    }
    // stepDensity: once per camera tick (the core's Fog driver tick and X / Y)
    void stepDensity(const float* info, float resets) {
        if (resets_ >= 0 && resets != resets_ && distance_ != -99999.f) {
            distance_ = -99999.f;
        }
        resets_ = resets;
        int steps = 0;
        float x = eye_[0], y = eye_[1];
        if (info && info[10]) {
            const float ticks = info[7];
            x = info[8];
            y = info[9];
            if (hasLastTicks_ && ticks < lastTicks_) {
                current_ = 1;
                distance_ = -99999.f;
            }
            steps = !hasLastTicks_ || ticks < lastTicks_ ? 1 : int(std::min(ticks - lastTicks_, 8.f));
            lastTicks_ = ticks;
            hasLastTicks_ = true;
        } else {
            // a camera without core ticks: one step a drawn frame (the page's 60 steps a second)
            steps = 1;
            hasLastTicks_ = false;
        }
        for (int k = 0; k < steps; ++k) {
            painterStep(x, y);
        }
        setDensity(current_);
    }

    struct Tree {
        float scale = 0;
        std::array<float, 2> origin{};
        uint32_t root = 0, outside = 0xffffffffu;
        std::vector<std::array<uint16_t, 4>> nodes;
        std::vector<std::array<float, 2>> payloads;
    };
    std::array<double, 3> origin_;
    std::vector<uint8_t> bytes_;
    size_t n_ = 0;
    bool loaded_ = false;
    std::vector<float> words_, corners_, base_;
    std::vector<int> counts_;
    float densityApplied_ = -1;
    std::map<size_t, std::vector<float>> cache_;
    std::array<float, 3> eye_{};
    Tree tree_;
    bool hasTree_ = false;
    float current_ = 1, distance_ = -99999.f, lastX_ = 0, lastY_ = 0;
    float lastTicks_ = 0;
    bool hasLastTicks_ = false;
    float resets_ = -1;
    std::vector<float> out_;
};

}  // namespace ssx_native
