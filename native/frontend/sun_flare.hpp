#pragma once
// The sun glow and lens flare (web/sun-flare.js natively): the tWPIGD_Sun painter (stepped per camera tick like ScreenTint; the
// texture field truncated in the blend), its package (SUN_FLARE/sun-flare.json, the raw GS textures) and the per-frame sprites
// (2F4DB8 / 2F4A08 / 2F4690) the renderer draws after the fog, with the 16 x 16 depth query's visibility.
#include "json.hpp"
#include "painter_tree.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ssx_native {

struct SunSprite {
    // centre and half size in 512 x 448 viewport pixels; uv rectangle; GS vertex RGB bytes and alpha (before the visibility)
    float cx = 0, cy = 0, half = 0;
    std::array<float, 4> uv{0, 0, 1, 1};
    std::array<float, 3> rgb{128, 128, 128};
    float alpha = 0;
};

class SunFlare {
public:
    explicit SunFlare(const std::string& dir) {
        std::ifstream f(dir + "sun-flare.json", std::ios::binary);
        if (!f) {
            throw std::runtime_error("cannot read " + dir + "sun-flare.json");
        }
        pkg_ = nlohmann::json::parse(f);
        if (pkg_["version"] != 1 || pkg_["flares"].size() != 9) {
            throw std::runtime_error("Unsupported sun flare package");
        }
        for (const auto& [name, t] : pkg_["textures"].items()) {
            std::ifstream tf(dir + t["file"].get<std::string>(), std::ios::binary);
            std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(tf)), std::istreambuf_iterator<char>());
            if (bytes.size() != t["width"].get<size_t>() * t["height"].get<size_t>() * 4) {
                throw std::runtime_error("Sun texture extent");
            }
            textures.push_back({name, t["width"].get<uint32_t>(), t["height"].get<uint32_t>(), std::move(bytes)});
        }
        defaults_ = values(pkg_["defaults"]);
        if (pkg_["painter"].is_object()) {
            for (const auto& p : pkg_["painter"]["payloads"]) {
                payloads_.push_back(values(p));
                rates_.push_back(p["rate"].get<double>());
            }
        }
        current_ = defaults_;
    }
    struct Texture {
        std::string name;
        uint32_t width, height;
        std::vector<uint8_t> rgba;
    };
    std::vector<Texture> textures;
    const nlohmann::json& package() const {
        return pkg_;
    }
    // 2C0778 once per camera tick (fog_info: [7] ticks, [8]/[9] camera source X/Y, [10] ready); worldLoads: environment_world_loads
    void tick(const float* info, uint32_t worldLoads) {
        const float ticks = info[7];
        if (!info[10]) {
            return;
        }
        if (ticks < lastTicks_) {
            current_ = defaults_;
            distance_ = -99999.f;
        }
        if (worldLoads != worldLoadsSeen_) {
            worldLoadsSeen_ = worldLoads;
            current_ = defaults_;
            distance_ = 0;
        }
        if (ticks > 0 && ticks != lastTicks_) {
            step(info[8], info[9]);
        }
        lastTicks_ = ticks;
    }
    // the painter's values: elevation, azimuth, r, g, b, glow alpha, texture, flare alpha, size
    const std::array<float, 9>& values() const {
        return current_;
    }

private:
    static std::array<float, 9> values(const nlohmann::json& p) {
        return {p["elevation"].get<float>(), p["azimuth"].get<float>(), p["colour"][0].get<float>(), p["colour"][1].get<float>(), p["colour"][2].get<float>(),
                p["glow_alpha"].get<float>(), p["texture"].get<float>(), p["flare_alpha"].get<float>(), p["size"].get<float>()};
    }
    void blend(const std::array<float, 9>& v, double weight) {
        const float w = float(weight * weight);
        const float c = float(1 - w);
        for (size_t i = 0; i < 9; ++i) {
            const float x = float(float(w * v[i]) + float(c * current_[i]));
            current_[i] = i == 6 ? std::trunc(x) : x;
        }
    }
    void step(float x, float y) {
        const bool initial = distance_ == -99999.f;
        if (!initial) {
            const float dx = float(lastX_ - x);
            const float dy = float(lastY_ - y);
            distance_ = float(distance_ + float(std::sqrt(double(float(float(dx * dx) + float(dy * dy))))));
        }
        lastX_ = x;
        lastY_ = y;
        const int selected = pkg_["painter"].is_object() ? painterPayloadIndex(pkg_["painter"], x, y) : -1;
        if (selected < 0 || size_t(selected) >= payloads_.size()) {
            current_ = defaults_;
            distance_ = 0;
            return;
        }
        const auto& v = payloads_[size_t(selected)];
        const double rate = rates_[size_t(selected)];
        if (v == current_) {
            distance_ = 0;
        }
        if (initial) {
            blend(v, -1);
            distance_ = 0;
            return;
        }
        if (rate >= 0 && rate <= distance_) {
            blend(v, 1);
            distance_ = 0;
        } else {
            blend(v, -rate);
        }
    }
    nlohmann::json pkg_;
    std::array<float, 9> defaults_{};
    std::array<float, 9> current_{};
    std::vector<std::array<float, 9>> payloads_;
    std::vector<double> rates_;
    float distance_ = -99999.f;
    float lastX_ = 0;
    float lastY_ = 0;
    float lastTicks_ = -1;
    uint32_t worldLoadsSeen_ = 0;
};

}  // namespace ssx_native
