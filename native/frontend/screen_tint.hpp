#pragma once
// ScreenTint (tWPIGD_ScreenTint, web/screen-tint.js natively): the quadtree payload under the camera source position, blended
// by the distance travelled, stepped once per camera tick after the core's tick; applied after the fog composite
// (C = C * s >> 7, then C + a; the lightning flash over it). The arithmetic is the JS version's (Math.fround at each step).
#include "json.hpp"
#include "painter_tree.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ssx_native {

struct TintOutput {
    std::array<float, 3> scaleBytes{127, 127, 127};
    std::array<float, 3> addBytes{0, 0, 0};
    // 0 off, 1 scale, 2 scale + add
    int enabled = 0;
    std::array<float, 3> flashRGB{};
    float flashA = 0;
    bool flashOn = false;
};

class ScreenTint {
public:
    explicit ScreenTint(const nlohmann::json& doc) : tint_(doc) {
        for (const auto& p : doc["payloads"]) {
            std::array<float, 6> v{};
            for (int k = 0; k < 3; ++k) {
                v[size_t(k)] = float(p["scale"][size_t(k)].get<double>());
                v[size_t(3 + k)] = float(p["add"][size_t(k)].get<double>());
            }
            payloads_.push_back(v);
            rates_.push_back(p["rate"].get<double>());
        }
        current_ = defaults();
    }
    // fogInfo: the core's fog_info (11 floats); weatherInfo: weather_info (47) or null; lightning: weather_lightning (10) or null;
    // worldLoads: environment_world_loads()
    void step(const float* fogInfo, const float* weatherInfo, const float* lightning, uint32_t worldLoads) {
        const float ticks = fogInfo[7];
        if (weatherInfo) {
            if (painterResets_ >= 0 && weatherInfo[46] != painterResets_ && distance_ != -99999.f) {
                distance_ = -99999.f;
            }
            painterResets_ = weatherInfo[46];
        }
        if (lightning) {
            const float I = lightning[1];
            if (I != 0) {
                for (int k = 0; k < 3; ++k) {
                    out_.flashRGB[size_t(k)] = float(int(std::trunc(float(float(I * lightning[2 + k]) * 255.f))) & 255);
                }
                out_.flashA = float(int(std::trunc(float(I * 255.f))) & 255);
                out_.flashOn = true;
            } else {
                out_.flashOn = false;
            }
        }
        if (!fogInfo[10]) {
            return;
        }
        if (ticks < lastTicks_) {
            current_ = defaults();
            distance_ = -99999.f;
            publish();
        }
        // followWorldLoad: a world load leaves the painter at its class defaults with +0 = 0
        if (worldLoads != worldLoadsSeen_) {
            worldLoadsSeen_ = worldLoads;
            current_ = defaults();
            distance_ = 0;
            publish();
        }
        if (ticks > 0 && ticks != lastTicks_) {
            stepAt(fogInfo[8], fogInfo[9]);
            publish();
        }
        lastTicks_ = ticks;
    }
    const TintOutput& output() const {
        return out_;
    }

private:
    static std::array<float, 6> defaults() {
        return {1, 1, 1, 0, 0, 0};
    }
    int payloadIndex(float x, float y) const {
        return tint_.contains("nodes") ? painterPayloadIndex(tint_, x, y) : -1;
    }
    void blend(const std::array<float, 6>& values, double weight) {
        const float w = float(weight * weight);
        const float c = float(1 - w);
        for (size_t i = 0; i < 6; ++i) {
            current_[i] = float(float(w * values[i]) + float(c * current_[i]));
        }
    }
    void stepAt(float x, float y) {
        const bool initial = distance_ == -99999.f;
        if (!initial) {
            const float dx = float(lastX_ - x);
            const float dy = float(lastY_ - y);
            distance_ = float(distance_ + float(std::sqrt(double(float(float(dx * dx) + float(dy * dy))))));
        }
        lastX_ = x;
        lastY_ = y;
        const int selected = payloadIndex(x, y);
        if (selected < 0 || size_t(selected) >= payloads_.size()) {
            current_ = defaults();
            distance_ = 0;
            return;
        }
        const auto& values = payloads_[size_t(selected)];
        const double rate = rates_[size_t(selected)];
        if (values == current_) {
            distance_ = 0;
        }
        if (initial) {
            blend(values, -1);
            distance_ = 0;
            return;
        }
        if (rate >= 0 && rate <= distance_) {
            blend(values, 1);
            distance_ = 0;
        } else {
            blend(values, -rate);
        }
    }
    void publish() {
        bool any = false;
        for (int k = 0; k < 3; ++k) {
            out_.scaleBytes[size_t(k)] = float(int(std::trunc(float(current_[size_t(k)] * 127.5f))));
            const int a = int(std::trunc(float(current_[size_t(3 + k)] * 127.5f)));
            out_.addBytes[size_t(k)] = float(std::max(0, a));
            any = any || a > 0;
        }
        out_.enabled = current_ == defaults() ? 0 : (any ? 2 : 1);
    }

    nlohmann::json tint_;
    std::vector<std::array<float, 6>> payloads_;
    std::vector<double> rates_;
    std::array<float, 6> current_{};
    float distance_ = -99999.f;
    float lastX_ = 0;
    float lastY_ = 0;
    float lastTicks_ = -1;
    float painterResets_ = -1;
    uint32_t worldLoadsSeen_ = 0;
    TintOutput out_;
};

}  // namespace ssx_native
