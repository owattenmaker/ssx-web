#pragma once
// The world painters' location quadtree (web/screen-tint.js payloadIndex, web/sun-flare.js painterPayloadIndex): the payload of
// the leaf under the camera source position, or the outside word; -1 none. The JS arithmetic (Math.fround at each step).
#include "json.hpp"
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace ssx_native {

inline int painterPayloadIndex(const nlohmann::json& tree, float x, float y) {
    const auto& nodes = tree["nodes"];
    if (nodes.empty()) {
        return -1;
    }
    const float px = float(float(double(x) - tree["origin"][0].get<double>()) * tree["scale"].get<double>());
    const float py = float(float(double(y) - tree["origin"][1].get<double>()) * tree["scale"].get<double>());
    const nlohmann::json* leaf = nullptr;
    if (px > -1 && px < 32768 && py > -1 && py < 32768) {
        uint32_t a = (uint32_t(int32_t(std::trunc(px))) << 1) & 0xffffu;
        uint32_t b = (uint32_t(int32_t(std::trunc(py))) << 1) & 0xffffu;
        int index = tree["root"].get<int>();
        for (size_t steps = 0; steps <= nodes.size(); ++steps) {
            const auto& node = nodes.at(size_t(index));
            if (!(node[0].get<int64_t>() & 1)) {
                leaf = &node;
                break;
            }
            index = int(node[size_t(((a >> 15) << 1) | (b >> 15))].get<int64_t>() >> 1);
            a = (a << 1) & 0xfffcu;
            b = (b << 1) & 0xfffcu;
        }
        if (!leaf) {
            throw std::runtime_error("Cyclic painter tree");
        }
    }
    const uint32_t value = leaf ? uint32_t((*leaf)[2].get<int64_t>() | ((*leaf)[3].get<int64_t>() << 16)) : uint32_t(tree["outside_words"][1].get<int64_t>());
    return value == 0xffffffffu ? -1 : int(value);
}

}  // namespace ssx_native
