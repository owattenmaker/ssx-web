#pragma once
// The audio world painters (web/audio-painters.js queryAudioPainters, 1:1): MusicTrigger (type 0), Mix (type 1) and Ambience
// (type 2) sampled at the rider's position (source cm) in the region of its last contacted track, through the 2C1CD8 point tree
// (EE float). The retail data's rates are all 0, so the stateless query is the original's result.
#include "audio_common.hpp"
#include "ee_float.hpp"
#include <cmath>

namespace ssx_native {

namespace audio {

struct PainterQuery {
    int mix = -1;
    int triggerA = -1;
    int triggerB = -1;
    int ambience = -1;
};

// 2C1CD8 + 2BAF90 + 2C0A10: the payload index at (x, y), -1 for none
inline int painterLeafPayload(const Json& section, double x, double y) {
    using namespace ee;
    const float ox = section["origin"][0].get<float>();
    const float oy = section["origin"][1].get<float>();
    const float scale = section["scale"].get<float>();
    const float px = mul(sub(float(x), ox), scale);
    const float py = mul(sub(float(y), oy), scale);
    uint32_t p;
    if (!(px > -1 && px < 32768 && py > -1 && py < 32768)) {
        p = uint32_t(section["outside"][1].get<double>());
    } else {
        const auto& n = section["nodes"];
        const size_t count = n.size();
        uint32_t a = (uint32_t(int32_t(std::trunc(px))) << 1) & 0xffff;
        uint32_t b = (uint32_t(int32_t(std::trunc(py))) << 1) & 0xffff;
        size_t i = section["root"].get<size_t>();
        for (size_t steps = 0;; ++steps) {
            if (steps > count / 4) {
                throw std::runtime_error("Cyclic painter tree");
            }
            if (!(n[i * 4].get<uint32_t>() & 1)) {
                break;
            }
            i = n[i * 4 + (((a >> 15) << 1) | (b >> 15))].get<uint32_t>() >> 1;
            a = (a << 1) & 0xfffc;
            b = (b << 1) & 0xfffc;
        }
        p = (n[i * 4 + 2].get<uint32_t>() | (n[i * 4 + 3].get<uint32_t>() << 16));
    }
    if (p == 0xffffffffu) {
        return -1;
    }
    if (p >= section["payloads"].size()) {
        throw std::runtime_error("Painter payload index outside section");
    }
    return int(p);
}

inline PainterQuery queryAudioPainters(const Json& data, double x, double y, const std::string& location) {
    const Json* entry = nullptr;
    if (data.contains("locations")) {
        const auto& locs = data["locations"];
        if (locs.contains(location)) {
            const auto& l = locs[location];
            entry = l.contains("painters") ? &l["painters"] : &l;
        }
    } else if (data.value("location", "") == location) {
        entry = data.contains("painters") ? &data["painters"] : &data;
    }
    auto sample = [&](const char* key) -> const Json* {
        if (!entry || !entry->contains(key) || (*entry)[key].is_null()) {
            return nullptr;
        }
        const Json& s = (*entry)[key];
        const int p = painterLeafPayload(s, x, y);
        return p < 0 ? nullptr : &s["payloads"][size_t(p)]["values"];
    };
    PainterQuery q;
    const Json* mt = sample("musicTrigger");
    const Json* mix = sample("mix");
    const Json* amb = sample("ambience");
    q.mix = mix ? (*mix)[0].get<int>() : -1;
    q.triggerA = mt ? (*mt)[0].get<int>() : -1;
    q.triggerB = mt ? (*mt)[1].get<int>() : -1;
    q.ambience = amb ? (*amb)[0].get<int>() : -1;
    return q;
}

}  // namespace audio

}  // namespace ssx_native
