#pragma once
// UVScroll texture scrolling for the front end (web/uv-scroll.js and its part of web/set-pieces-renderer.js, natively):
// - the per-instance scroller 0x35F7D0 in web/ee-scalar-float.js's arithmetic (MUL / ADD / SUB chop with the EE guard bit,
//   DIV.S nearest);
// - the scroll groups (first-appearance order of distinct initial states, prepare.py's uv_scroll_group ids), each drawn with its
//   representative instance's (u, v);
// - the set-piece clock: one scroller tick per game tick up to set_piece_info()[1], the section log's slot-1 enters
//   (set_piece_sections) restarting an instance after that tick, and the ready state's phases at a race start.
#include "core_api.hpp"
#include "json.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace ssx_native {

namespace ee {
inline uint32_t bits(float x) {
    uint32_t b;
    std::memcpy(&b, &x, 4);
    return b;
}
inline float fromBits(uint32_t b) {
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}
// an exactly representable double rounded toward zero to float32
inline float chop(double exact) {
    float r = float(exact);
    if (r != 0 && std::abs(double(r)) > std::abs(exact)) {
        r = fromBits(bits(r) - 1);
    }
    return r;
}
inline float mul(float a, float b) {
    return chop(double(a) * double(b));
}
inline float addSub(float a, float b, bool subtract) {
    uint32_t x = bits(a);
    uint32_t y = bits(b);
    const int d = int((x >> 23) & 255) - int((y >> 23) & 255);
    if (d >= 25) {
        y &= 0x80000000u;
    } else if (d <= -25) {
        x &= 0x80000000u;
    } else if (d > 0) {
        y &= 0xffffffffu << (d - 1);
    } else if (d < 0) {
        x &= 0xffffffffu << (-d - 1);
    }
    const double l = fromBits(x);
    const double r = fromBits(y);
    return chop(subtract ? l - r : l + r);
}
inline float add(float a, float b) {
    return addSub(a, b, false);
}
inline float sub(float a, float b) {
    return addSub(a, b, true);
}
// the correctly rounded float32 quotient (DIV.S nearest)
inline float div(float a, float b) {
    const double q = double(a) / double(b);
    const float r = float(q);
    if (!std::isfinite(q) || double(r) == q) {
        return r;
    }
    const float other = fromBits(bits(r) + (std::abs(double(r)) > std::abs(q) ? uint32_t(-1) : 1u));
    if ((double(r) + double(other)) / 2 != q) {
        return r;
    }
    const double rest = (double(a) - q * double(b)) / double(b);
    if (rest > 0) {
        return r > other ? r : other;
    }
    if (rest < 0) {
        return r < other ? r : other;
    }
    return (bits(r) & 1) ? other : r;
}
}  // namespace ee

struct UvScrollState {
    int mode = 0;
    float timer = 0, onTime = 0, offTime = 0, angle = 0, spin = 0, u = 0, v = 0, stepU = 0, stepV = 0;
    int active = 0;
};

// 0x35F7D0 (web/uv-scroll.js uvScrollTick)
inline void uvScrollTick(UvScrollState& s, float fps) {
    if (!(0 < s.onTime) && !(0 < s.offTime)) {
        return;
    }
    const float timer = ee::add(s.timer, ee::div(1, fps));
    s.timer = timer;
    if (!s.active) {
        if (!(s.offTime <= timer)) {
            return;
        }
        s.timer = 0;
        if (0 < s.onTime) {
            s.active = 1;
        }
        return;
    }
    if (s.spin != 0) {
        const float twoPi = 6.2831854820251465f;
        const float rate = s.spin;
        s.angle = ee::add(s.angle, rate);
        if (twoPi <= rate) {
            s.spin = ee::sub(rate, twoPi);
        } else if (rate <= twoPi) {
            s.spin = ee::add(rate, twoPi);
        }
    }
    if (s.onTime <= s.timer) {
        s.timer = 0;
        if (0 < s.offTime) {
            s.active = 0;
        }
        if (s.mode == 2 || s.mode == 6) {
            s.stepU = -s.stepU;
            s.stepV = -s.stepV;
        }
    }
    if (!s.active) {
        return;
    }
    if (s.mode != 6) {
        s.u = ee::add(s.u, s.stepU);
        s.v = ee::add(s.v, s.stepV);
    } else {
        const float su = s.stepU;
        const float on = s.onTime;
        const float t = s.timer;
        float k;
        if (su < 0) {
            k = t < ee::mul(on, 0.5f) ? t : ee::sub(on, t);
        } else {
            const float left = ee::sub(on, t);
            k = left < ee::mul(on, 0.5f) ? left : ee::sub(on, left);
        }
        const float inv = ee::div(1, on);
        const float dv = ee::mul(ee::mul(s.stepV, k), inv);
        const float du = ee::mul(ee::mul(su, k), inv);
        s.v = ee::add(s.v, dv);
        s.u = ee::add(s.u, du);
    }
    if (1 < s.u) {
        s.u = ee::sub(s.u, 1);
    } else if (s.u < -1) {
        s.u = ee::add(s.u, 1);
    }
    if (1 < s.v) {
        s.v = ee::sub(s.v, 1);
    } else if (s.v < -1) {
        s.v = ee::add(s.v, 1);
    }
}

class UvScrollSet {
public:
    // data: UVSCROLL/uv-scroll.json; ready: the course's SECTIONS/ready-state.json (null without sections)
    UvScrollSet(const nlohmann::json& data, const nlohmann::json& ready) : data_(data), ready_(ready) {
        fps_ = data.value("fps", 60.0f);
        std::map<std::string, int> groups;
        for (const auto& x : data["instances"]) {
            // sortedJson(initial): nlohmann's std::map keeps the keys sorted
            const std::string key = nlohmann::json(x["initial"]).dump();
            if (!groups.count(key)) {
                const int id = int(groups.size());
                groups[key] = id;
                representative_.push_back(x["resource"].get<uint32_t>());
            }
        }
        reset();
    }
    // web/set-pieces-renderer.js update(): the ticks up to the core's set-piece clock
    void update() {
        const float target = set_piece_info()[1];
        if (target < float(ticks_)) {
            reset();
        }
        const uint32_t* log = set_piece_sections();
        const uint32_t count = log ? log[0] : 0;
        while (float(ticks_) < target) {
            // the section scan at the end of this tick: slot-1 enters restart their scroller
            while (sectionCursor_ < count && log[2 + 6 * sectionCursor_] <= uint32_t(ticks_)) {
                const uint32_t* e = log + 2 + 6 * sectionCursor_;
                ++sectionCursor_;
                if (e[2] == 1) {
                    activate(e[1]);
                }
            }
            for (auto& [resource, s] : states_) {
                uvScrollTick(s, fps_);
            }
            ++ticks_;
        }
    }
    // the group offsets, by uv_scroll_group id
    std::vector<std::array<float, 2>> offsets() const {
        std::vector<std::array<float, 2>> out;
        for (uint32_t r : representative_) {
            auto it = states_.find(r);
            out.push_back(it == states_.end() ? std::array<float, 2>{0, 0} : std::array<float, 2>{it->second.u, it->second.v});
        }
        return out;
    }

private:
    static UvScrollState stateOf(const nlohmann::json& j, const nlohmann::json* bitsOf) {
        UvScrollState s;
        auto f = [&](const char* k) {
            if (bitsOf && bitsOf->contains(k) && (*bitsOf)[k].is_number()) {
                return ee::fromBits((*bitsOf)[k].get<uint32_t>());
            }
            return j.contains(k) && j[k].is_number() ? j[k].get<float>() : 0.f;
        };
        s.mode = j.value("mode", 0);
        s.timer = f("timer");
        s.onTime = f("onTime");
        s.offTime = f("offTime");
        s.angle = f("angle");
        s.spin = f("spin");
        s.u = f("u");
        s.v = f("v");
        s.stepU = f("stepU");
        s.stepV = f("stepV");
        s.active = j.value("active", 0);
        return s;
    }
    void activate(uint32_t resource) {
        for (const auto& x : data_["instances"]) {
            if (x["resource"].get<uint32_t>() == resource) {
                states_[resource] = stateOf(x["initial"], nullptr);
            }
        }
    }
    void reset() {
        ticks_ = 0;
        sectionCursor_ = 0;
        states_.clear();
        for (const auto& x : data_["instances"]) {
            states_[x["resource"].get<uint32_t>()] = stateOf(x["initial"], nullptr);
        }
        // race tick 0 = the ready savestate: the pieces built before the race keep their phase
        if (ready_.is_object() && ready_.contains("uvscroll")) {
            for (const auto& entry : ready_["uvscroll"]) {
                const uint32_t r = entry["resource"].get<uint32_t>();
                if (!states_.count(r)) {
                    continue;
                }
                const nlohmann::json* b = entry.contains("bits") ? &entry["bits"] : nullptr;
                states_[r] = stateOf(entry["state"], b);
            }
        }
    }
    nlohmann::json data_;
    nlohmann::json ready_;
    float fps_ = 60;
    std::vector<uint32_t> representative_;
    std::map<uint32_t, UvScrollState> states_;
    long ticks_ = 0;
    uint32_t sectionCursor_ = 0;
};

}  // namespace ssx_native
