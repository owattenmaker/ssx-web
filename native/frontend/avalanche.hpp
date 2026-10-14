#pragma once
// The avalanches' draw (docs/avalanche.md "Draw" and "Trails"; web/avalanche-state.js and web/avalanche-trails.js natively):
// - the AvaSpline pieces: their matrices come through moving_instances (set_pieces.hpp); a piece hidden at the start shows while
//   its tumbler drives it when its trigger's builtin 0 passes key 2 (ENTITY_DRAWN), until it is released;
// - the trails: each playing tumbler's colour emitter birth ring (avalanche_trails), the snow particle sprites (engine/
//   snow_particles.cpp originalSnowParticle in the EE float operations), fog0, GS 0x44, the 128-pixel size cap.
#include "ee_float.hpp"
#include "json.hpp"
#include "particles.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

extern "C" {
uint32_t* avalanche_trails();
float* avalanche_pieces();
uint32_t* avalanche_info();
}

namespace ssx_native {

class Avalanche {
public:
    // doc: the course's avalanches.json (null: none)
    Avalanche(const std::string& code, const nlohmann::json& doc) {
        if (!doc.is_object()) {
            return;
        }
        for (const auto& a : doc["avalanches"]) {
            for (const auto& g : a["groups"]) {
                // a truthy ava_spline
                const auto& v = g.contains("ava_spline") ? g["ava_spline"] : nlohmann::json();
                const bool spline = v.is_boolean() ? v.get<bool>() : v.is_number() ? v.get<double>() != 0 : !v.is_null();
                if (spline) {
                    followers_.insert(g["resource"].get<uint32_t>());
                }
            }
        }
        static const std::map<std::string, std::vector<uint32_t>> kEntityDrawn = {
            {"ABC1", {8198, 74758, 135942, 231430, 239366, 260614, 303878, 362758, 372230, 441862, 453894, 494598, 544006, 563206, 617222, 620806}},
            {"DRA4", {72992, 102944, 125472, 266784, 292896, 316960, 381984, 416288, 417312, 502048, 503072, 644896, 660768, 671776}},
        };
        auto it = kEntityDrawn.find(code);
        if (it != kEntityDrawn.end()) {
            entityDrawn_.insert(it->second.begin(), it->second.end());
        }
        active_ = !followers_.empty();
    }
    bool active() const {
        return active_;
    }
    bool follower(uint32_t resource) const {
        return followers_.count(resource) > 0;
    }
    // a new race: the released pieces come back
    void reset() {
        released_.clear();
        key_ = ~uint64_t(0);
    }
    // avalancheState: the core's pieces of its current tick (read once per tick: avalanche_pieces drains its released list)
    // (any course, as the page reads it: the audio's rumble loop needs no AvaSpline follower)
    void read() {
        const uint32_t* I = avalanche_info();
        const uint64_t key = uint64_t(I[2]) * 4096 + I[1];
        if (key == key_) {
            return;
        }
        key_ = key;
        playing_.clear();
        tumblers_.clear();
        const float* H = avalanche_pieces();
        size_t at = 0;
        const size_t n = size_t(H[at++]);
        for (size_t k = 0; k < n; ++k, at += 22) {
            playing_.insert(uint32_t(H[at]));
            // the 0x2D9C00 matrix's row 3 (+96, source cm) and the scale (+176)
            tumblers_.push_back({H[at + 16], H[at + 17], H[at + 18], H[at + 21]});
        }
        const size_t r = size_t(H[at++]);
        for (size_t k = 0; k < r; ++k) {
            released_.insert(uint32_t(H[at + k]));
        }
        at += r;
        // 0x29DEF0: the loop refcount, then its changes since the last call, in order
        loop_ = H[at++] > 0;
        const size_t t = size_t(H[at++]);
        for (size_t k = 0; k < t; ++k) {
            loopEvents_.push_back(H[at + k]);
        }
    }
    // the audio's view of the current tick (web/avalanche-state.js avalancheState: loop, tumblers)
    bool loop() const {
        return loop_;
    }
    const std::vector<std::array<double, 4>>& tumblers() const {
        return tumblers_;
    }
    // takeLoopEvents: the refcount changes since the last take
    std::vector<double> takeLoopEvents() {
        std::vector<double> e;
        e.swap(loopEvents_);
        return e;
    }
    // a follower hidden at the start: drawn while its tumbler drives it (ENTITY_DRAWN), until released
    bool hiddenPieceDrawn(uint32_t resource) const {
        return !released_.count(resource) && playing_.count(resource) && entityDrawn_.count(resource);
    }

    // the trails' sprites by texture id: x, y, z (source cm), half extent (cm), r, g, b, a (GS integers)
    std::map<int, std::vector<double>> trails() const {
        std::map<int, std::vector<double>> out;
        if (!active_) {
            return out;
        }
        const uint32_t* U = avalanche_trails();
        if (!U) {
            return out;
        }
        const float* Fv = reinterpret_cast<const float*>(U);
        size_t at = 0;
        const uint32_t count = U[at++];
        for (uint32_t k = 0; k < count; ++k) {
            at += 2;
            const uint32_t* image = U + at;
            const float* imageF = Fv + at;
            at += 130;
            const uint32_t n = U[at++];
            const float* A = Fv + at;
            at += 4 * size_t(n);
            const float* B = Fv + at;
            const uint32_t* bits = U + at;
            at += 4 * size_t(n);
            const uint32_t* colours = U + at;
            at += n;
            const int texture = int32_t(image[0x4 >> 2]);
            const int flipCount = int32_t(image[0xC >> 2]);
            const float flipPhase = imageF[0x10 >> 2];
            const int id = texture + (flipCount >= 2 ? int(std::trunc(flipPhase)) : 0);
            sprites(image, imageF, n, A, B, bits, colours, out[id]);
        }
        return out;
    }

private:
    static void sprites(const uint32_t* image, const float* imageF, uint32_t births, const float* A, const float* B, const uint32_t* bits,
                        const uint32_t* colours, std::vector<double>& out) {
        using namespace ee;
        auto w = [&](int o) { return image[o >> 2]; };
        auto kf = [&](int o) { return imageF[(0x20 + o) >> 2]; };
        if (!w(0x174) || int32_t(w(0x1E0)) <= 0) {
            return;
        }
        const int per = births ? int32_t(w(0x20)) / int(births) : 0;
        if (per <= 0) {
            return;
        }
        auto kv = [&](int o) { return std::array<float, 3>{kf(o), kf(o + 4), kf(o + 8)}; };
        auto kc = [&](int o) { return std::array<float, 4>{kf(o), kf(o + 4), kf(o + 8), kf(o + 12)}; };
        const float ageStep = kf(0x0C), invCount = div(1, float(per)), lifeBase = kf(0x1C), lifeRange = kf(0x14);
        const float sizeBase = kf(0x18), sizeRange = kf(0x10), sizeDelta = kf(0x20);
        const auto force = kv(0x60), velBase = kv(0x70), posBase = kv(0xB0), posR0 = kv(0xC0), posR1 = kv(0xD0);
        const std::array<std::array<float, 3>, 3> velR = {kv(0x80), kv(0x90), kv(0xA0)};
        const auto colBase = kc(0xF0), colR0 = kc(0x100), colR1 = kc(0x110), colSlope = kc(0x120);
        const uint32_t cursor = w(0x17C);
        const float tMax = 2.700000047683716f, p1 = -0.7300000190734863f, p2 = 0.11299999803304672f, inv255 = 0.003921568859368563f;
        auto bytes = [&](uint32_t s, int i) { return float((colours[s] >> (8 * i)) & 255); };
        float age = 0;
        float R[9];
        for (uint32_t group = 0; group < births; ++group) {
            if (group % 43 == 0) {
                age = mul(mul(float(group - (group % 43)), float(per)), ageStep);
            }
            const uint32_t a = (cursor + 1 + group) % births;
            const uint32_t b = (cursor + 1 + std::min(group + 1, births - 1)) % births;
            if (!(B[a * 4 + 3] >= 1)) {
                age = add(age, mul(ageStep, float(per)));
                continue;
            }
            uint32_t random = (bits[a * 4 + 3] & 0x7fffffu) | 0x3f800000u;
            float fraction = 0;
            for (int i = 0; i < per; ++i, age = add(age, ageStep), fraction = add(fraction, invCount)) {
                for (int k = 0; k < 9; ++k) {
                    random = particles::lfsr(random);
                    R[k] = fromBits(random);
                }
                const float lifetime = add(lifeBase, mul(lifeRange, R[8]));
                if (!(age < lifetime)) {
                    continue;
                }
                const float complement = sub(1, fraction);
                const float tt = std::min(age, tMax);
                const float poly = add(mul(tt, p1), mul(mul(tt, tt), p2));
                double o[8];
                for (int c = 0; c < 3; ++c) {
                    float v = add(mul(B[a * 4 + c], complement), mul(B[b * 4 + c], fraction));
                    v = add(add(add(add(v, velBase[c]), mul(velR[0][c], R[2])), mul(velR[1][c], R[3])), mul(velR[2][c], R[4]));
                    float p = add(mul(A[a * 4 + c], complement), mul(A[b * 4 + c], fraction));
                    p = add(add(add(p, posBase[c]), mul(posR0[c], R[0])), mul(posR1[c], R[1]));
                    p = add(mul(force[c], age), p);
                    p = add(p, mul(sub(force[c], v), poly));
                    o[c] = p;
                }
                float size = mul(mul(sizeDelta, age), div(1, lifetime));
                size = add(add(size, sizeBase), mul(sizeRange, R[7]));
                o[3] = std::abs(size);
                for (int c = 0; c < 4; ++c) {
                    float tint = add(mul(bytes(a, c), complement), mul(bytes(b, c), fraction));
                    tint = mul(tint, inv255);
                    float value = add(add(add(mul(colBase[c], tint), mul(colR0[c], R[5])), mul(colSlope[c], age)), mul(colR1[c], R[6]));
                    value = std::max(c < 3 ? std::min(value, 128.f) : value, 0.f);
                    o[4 + c] = std::min(255.0, std::max(0.0, std::trunc(double(value))));
                }
                out.insert(out.end(), o, o + 8);
            }
        }
    }

    bool active_ = false;
    std::set<uint32_t> followers_;
    std::set<uint32_t> entityDrawn_;
    std::set<uint32_t> playing_;
    std::set<uint32_t> released_;
    uint64_t key_ = ~uint64_t(0);
    std::vector<std::array<double, 4>> tumblers_;
    bool loop_ = false;
    std::vector<double> loopEvents_;
};

}  // namespace ssx_native
