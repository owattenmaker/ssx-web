#pragma once
// Snow Jam's start-gate spark fountains (web/startfire-renderer.js natively; STARTFIRE/startfire.json from tools/export_startfire.py):
// the profile's kernel, the trigger box on the human rider, the gate schedule, and per emitter the snow VU A00 particle equations in
// doubles as the page computes them. The page steps it once per snow update serial (one a tick); its emitter seeds stand in for
// the 0x36CCB8 words (the page's own approximation, kept).
#include "json.hpp"
#include "particles.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ssx_native {

class Startfire {
public:
    explicit Startfire(const nlohmann::json& meta) : meta_(meta) {
        if (!meta.is_object()) {
            return;
        }
        const auto& p = meta["profile"];
        auto g = [&](const char* k) { return p[k].get<double>(); };
        const double d = g("Damp");
        const double half = g("LifeR") * 0.5;
        const double maxLife = g("Life") + half;
        const double minLife = g("Life") - half;
        const double inv = 1 / d;
        auto v = [&](const std::string& k) { return V3{g((k + "X").c_str()), g((k + "Y").c_str()), g((k + "Z").c_str())}; };
        auto c = [&](const std::string& k) { return V4{g((k + "R").c_str()), g((k + "G").c_str()), g((k + "B").c_str()), g((k + "A").c_str())}; };
        auto scale3 = [](V3 a, double s) {
            for (double& x : a) {
                x *= s;
            }
            return a;
        };
        auto scale4 = [](V4 a, double s) {
            for (double& x : a) {
                x *= s;
            }
            return a;
        };
        k_.velocityRanges = {scale3(v("R0V"), inv), scale3(v("R1V"), inv), scale3(v("R2V"), inv)};
        const V4 r0 = scale4(c("R0"), 128), r1 = scale4(c("R1"), 128), start = scale4(c("StartCol"), 128), end = scale4(c("EndCol"), 128);
        const double lifeRange = (maxLife - minLife) * d;
        k_.count = p["NumParticles"].get<int>();
        k_.ageStep = (g("Duration") * d) / g("NumParticles");
        k_.ageRate = d;
        k_.remaining = g("Duration") + g("Life") + g("LifeR") * 0.5;
        k_.lifeBase = minLife * d - lifeRange;
        k_.lifeRange = lifeRange;
        k_.sizeBase = g("Size") - g("SizeR") * 1.5;
        k_.sizeRange = g("SizeR");
        k_.sizeDelta = g("SizeFinal") - g("Size");
        k_.force = scale3(scale3(v("Force"), inv), inv);
        const V3 r0p = v("R0"), r1p = v("R1"), off = v("Off");
        for (size_t a = 0; a < 3; ++a) {
            k_.positionBase[a] = off[a] - (r0p[a] + r1p[a]) * 1.5;
        }
        k_.positionRanges = {r0p, r1p};
        const V3 vel = scale3(v("Vel"), inv);
        for (size_t a = 0; a < 3; ++a) {
            k_.velocityBase[a] = vel[a] - (k_.velocityRanges[0][a] + k_.velocityRanges[1][a] + k_.velocityRanges[2][a]) * 1.5;
        }
        for (size_t a = 0; a < 4; ++a) {
            k_.colourBase[a] = start[a] - (r0[a] + r1[a]) * 1.5;
            k_.colourSlope[a] = (end[a] - start[a]) * (1 / (maxLife * d));
        }
        k_.colourRanges = {r0, r1};
        gateSeconds_ = meta["inferred"]["gate_unit_seconds"].get<double>();
        for (size_t a = 0; a < 3; ++a) {
            box_[0][a] = meta["trigger"]["bounds_min_cm"][a].get<double>();
            box_[1][a] = meta["trigger"]["bounds_max_cm"][a].get<double>();
        }
        loaded_ = true;
    }
    bool loaded() const {
        return loaded_;
    }
    // after a tick: the snow update serial (snow_info()[20]) and the human's rider_state position (m, three space)
    void afterTick(float serial, const float* rider) {
        if (!loaded_) {
            return;
        }
        const double riderCm[3] = {double(rider[0]) * 100, -double(rider[2]) * 100, double(rider[1]) * 100};
        const int steps = hasSerial_ ? int(std::min(std::max(double(serial) - double(serial_), 0.0), 30.0)) : 0;
        serial_ = serial;
        hasSerial_ = true;
        for (int i = 0; i < steps; ++i) {
            step(riderCm);
        }
    }
    // the particles: x, y, z (source cm), size (cm), colour bytes
    std::vector<double> particles() const {
        std::vector<double> out;
        for (const auto& e : emitters_) {
            uint32_t state = e.seed;
            for (int i = 0; i < k_.count; ++i) {
                double r[9];
                for (int n = 0; n < 9; ++n) {
                    state = particles::lfsr(state);
                    r[n] = ee::fromBits(state);
                }
                const double age = e.age - i * k_.ageStep;
                if (age < 0) {
                    continue;
                }
                const double lifetime = k_.lifeBase + k_.lifeRange * r[8];
                if (!(age < lifetime)) {
                    continue;
                }
                const double t = std::min(age, 2.700000047683716);
                const double poly = t * -0.7300000190734863 + t * t * 0.11299999803304672;
                double p[3];
                for (size_t a = 0; a < 3; ++a) {
                    const double velocity = e.kernel.velocityBase[a] + e.kernel.velocityRanges[0][a] * r[2] + e.kernel.velocityRanges[1][a] * r[3] +
                                            e.kernel.velocityRanges[2][a] * r[4];
                    p[a] = e.origin[a] + e.kernel.positionBase[a] + e.kernel.positionRanges[0][a] * r[0] + e.kernel.positionRanges[1][a] * r[1] +
                           e.kernel.force[a] * age + (e.kernel.force[a] - velocity) * poly;
                }
                const double size = std::abs((k_.sizeDelta * age) / lifetime + k_.sizeBase + k_.sizeRange * r[7]);
                out.insert(out.end(), {p[0], p[1], p[2], size});
                for (size_t a = 0; a < 4; ++a) {
                    const double x = k_.colourBase[a] + k_.colourRanges[0][a] * r[5] + k_.colourSlope[a] * age + k_.colourRanges[1][a] * r[6];
                    out.push_back(std::min(255.0, std::trunc(std::max(a < 3 ? std::min(x, 128.0) : x, 0.0))));
                }
            }
        }
        return out;
    }

private:
    using V3 = std::array<double, 3>;
    using V4 = std::array<double, 4>;
    struct Kernel {
        int count = 0;
        double ageStep = 0, ageRate = 0, remaining = 0, lifeBase = 0, lifeRange = 0, sizeBase = 0, sizeRange = 0, sizeDelta = 0;
        V3 force{}, positionBase{}, velocityBase{};
        std::array<V3, 2> positionRanges{};
        std::array<V3, 3> velocityRanges{};
        V4 colourBase{}, colourSlope{};
        std::array<V4, 2> colourRanges{};
    };
    struct Local {
        V3 positionBase{}, velocityBase{}, force{};
        std::array<V3, 2> positionRanges{};
        std::array<V3, 3> velocityRanges{};
    };
    struct Emitter {
        V3 origin{};
        Local kernel;
        double age = 0;
        double remaining = 0;
        uint32_t seed = 0;
    };
    // 0x370058: the instance matrix rotates the local-frame vectors (row-major, w 0)
    static V3 rotate(const nlohmann::json& m, const V3& v) {
        auto e = [&](size_t i) { return m[i].get<double>(); };
        return {v[0] * e(0) + v[1] * e(4) + v[2] * e(8), v[0] * e(1) + v[1] * e(5) + v[2] * e(9), v[0] * e(2) + v[1] * e(6) + v[2] * e(10)};
    }
    void reset() {
        triggered_ = false;
        timer_ = 0;
        fired_ = 0;
        emitters_.clear();
    }
    void step(const double* riderCm) {
        const double tick = 1.0 / 60;
        bool inside = true;
        for (size_t a = 0; a < 3; ++a) {
            inside = inside && riderCm[a] >= box_[0][a] && riderCm[a] <= box_[1][a];
        }
        // a restart uphill of the trigger re-arms the one-shot stage event
        if (triggered_ && riderCm[0] > box_[1][0] + 1000) {
            reset();
        }
        if (!triggered_ && inside) {
            triggered_ = true;
            timer_ = 0;
        }
        for (auto& e : emitters_) {
            e.age += tick * k_.ageRate;
            e.remaining -= tick;
        }
        emitters_.erase(std::remove_if(emitters_.begin(), emitters_.end(), [](const Emitter& e) { return !(e.remaining > 0); }), emitters_.end());
        if (!triggered_) {
            return;
        }
        timer_ += tick;
        const auto& schedule = meta_["schedule"];
        while (fired_ < schedule.size() && timer_ >= schedule[fired_]["gate"].get<double>() * gateSeconds_ - 1e-6) {
            for (const auto& instance : schedule[fired_]["instances"]) {
                const auto& m = instance["matrix"];
                Emitter e;
                e.origin = {m[12].get<double>(), m[13].get<double>(), m[14].get<double>()};
                e.kernel.positionBase = rotate(m, k_.positionBase);
                e.kernel.positionRanges = {rotate(m, k_.positionRanges[0]), rotate(m, k_.positionRanges[1])};
                e.kernel.velocityBase = rotate(m, k_.velocityBase);
                e.kernel.velocityRanges = {rotate(m, k_.velocityRanges[0]), rotate(m, k_.velocityRanges[1]), rotate(m, k_.velocityRanges[2])};
                e.kernel.force = rotate(m, k_.force);
                e.remaining = k_.remaining;
                const uint32_t rid = instance["rid"].get<uint32_t>();
                e.seed = 0x3f800000u | (uint32_t(uint64_t(rid) * 2654435761ull) >> 9);
                emitters_.push_back(e);
            }
            ++fired_;
        }
    }

    nlohmann::json meta_;
    bool loaded_ = false;
    Kernel k_;
    double gateSeconds_ = 0;
    std::array<V3, 2> box_{};
    bool triggered_ = false;
    double timer_ = 0;
    size_t fired_ = 0;
    std::vector<Emitter> emitters_;
    float serial_ = 0;
    bool hasSerial_ = false;
};

}  // namespace ssx_native
