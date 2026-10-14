#pragma once
// Waving flags / streamers (web/flag-animation.js natively; engine/flag_cloth.hpp in the EE arithmetic of ee_float.hpp): the wind
// (0x34C668), the cloth tick (0x34B818), the vertices (0x34BCA0) and the grid build (0x34B228). Data: FLAGS/flags.json (source cm,
// Z up); the world output is native metres (x, z, -y) / 100.
#include "ee_float.hpp"
#include "json.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace ssx_native {

namespace flags {

inline float unitRandom(uint32_t word) {
    return ee::sub(ee::fromBits((word & 0x7fffffu) | 0x3f800000u), 1);
}

inline const float kTwoPi = ee::fromBits(0x40c90fdb);
inline const float kNegTwoPi = ee::fromBits(0xc0c90fdb);
inline const float kToIndex = ee::fromBits(0x42a2f983);
inline const float kWaveMin = ee::fromBits(0x3c23d70a);
inline const float kUvMin = ee::fromBits(0x3a83126f);

struct Parameters {
    int attachStart = 0, attachEnd = 0, pinFirstRow = 0;
    std::array<float, 4> speed{}, amplitude{}, frequency{};
    std::array<float, 3> gust{}, calm{};
    float minimumStrength = 0;
    std::array<float, 2> uvSpeed{};
};

struct Cloth {
    int cloth = -1;
    Parameters parameters;
    int parity = 0;
    int count = 0;
    std::vector<uint32_t> instances;
    bool mesh = false;
    int width = 0, height = 0;
    float widthSpan = 0, heightSpan = 0;
    std::array<float, 4> phase{};
    std::array<float, 2> uvOffset{};
    std::vector<float> base, uv, colour, vertices;
};

struct Wind {
    float wind = 0, base = 0, delta = 0, timer = 0;
};

// 0x34BCA0
inline void vertices(const Cloth& cloth, float wind, const std::vector<float>& sine, std::vector<float>& out) {
    using namespace ee;
    const Parameters& p = cloth.parameters;
    const int w = cloth.width;
    const int h = cloth.height;
    out.assign(size_t(w * h * 3), 0.f);
    const bool both = p.attachStart && p.attachEnd;
    const bool free = !p.attachStart && !p.attachEnd;
    const float strength = add(p.minimumStrength, mul(wind, sub(1, p.minimumStrength)));
    std::array<float, 4> phase{};
    std::array<float, 4> amp{};
    for (size_t i = 0; i < 4; ++i) {
        phase[i] = mul(cloth.phase[i], kNegTwoPi);
        amp[i] = mul(p.amplitude[i], strength);
    }
    float s = 0;
    float c = 0;
    auto lookup = [&](float angle) {
        const int i = int(std::trunc(mul(angle, kToIndex))) & 0x1ff;
        s = sine[size_t(i)];
        c = sine[size_t(i + 0x80)];
    };
    std::vector<float> column(size_t(w * 3), 0.f);
    float sumA = 0;
    float sumB = 0;
    float colValue = 0;
    const float calmWeight = sub(1, strength);
    for (int a2 = 0; a2 < w; ++a2) {
        int a0 = a2;
        float f3 = div(colValue, cloth.widthSpan);
        const float u = f3;
        if (free) {
            f3 = 1;
        } else if (both) {
            if (0.5f < f3) {
                f3 = sub(1, f3);
            }
        } else if (p.attachEnd == 1) {
            a0 = w - a2 - 1;
        }
        const float f5 = mul(u, kTwoPi);
        lookup(add(phase[0], mul(f5, p.frequency[0])));
        if (!both) {
            sumA = add(sumA, mul(mul(amp[0], f3), c));
        }
        float x = mul(mul(amp[0], f3), s);
        float y = sumA;
        float z = 0;
        lookup(add(phase[1], mul(f5, p.frequency[1])));
        if (!both) {
            sumB = add(sumB, mul(mul(amp[1], f3), c));
        }
        x = add(x, mul(mul(amp[1], f3), s));
        y = add(y, sumB);
        z = add(z, 0);
        float o[3] = {x, y, z};
        for (size_t k = 0; k < 3; ++k) {
            o[k] = add(o[k], add(mul(mul(p.gust[k], f3), strength), mul(mul(p.calm[k], f3), calmWeight)));
        }
        column[size_t(a0 * 3)] = o[0];
        column[size_t(a0 * 3 + 1)] = o[1];
        column[size_t(a0 * 3 + 2)] = o[2];
        colValue = add(colValue, 1);
    }
    int vertex = 0;
    float rowValue = 0;
    const auto& base = cloth.base;
    for (int r = 0; r < h; ++r) {
        float cv = 0;
        for (int col = 0; col < w; ++col) {
            float f8 = div(cv, cloth.widthSpan);
            const float f9 = div(rowValue, cloth.heightSpan);
            float f10 = f8;
            if (both) {
                if (0.5f < f8) {
                    f8 = sub(1, f8);
                }
            } else if (p.attachEnd == 1) {
                f8 = sub(1, f8);
                f10 = f8;
            } else if (free) {
                f8 = 1;
            }
            const size_t v = size_t(vertex * 3);
            if (p.pinFirstRow && f9 == 0) {
                // the original's quirk: no vertex increment
                out[v] = base[v];
                out[v + 1] = base[v + 1];
                out[v + 2] = base[v + 2];
                cv = add(cv, 1);
                continue;
            }
            float rx = add(base[v], column[size_t(col * 3)]);
            float ry = add(base[v + 1], column[size_t(col * 3 + 1)]);
            float rz = add(base[v + 2], column[size_t(col * 3 + 2)]);
            if (0 < f10 && kWaveMin < amp[2]) {
                const float half = mul(add(f10, f9), 0.5f);
                const float mid = mul(add(f8, f9), 0.5f);
                lookup(add(phase[2], mul(half, mul(p.frequency[2], kTwoPi))));
                rx = add(rx, mul(mul(amp[2], f8), s));
                ry = add(ry, mul(mul(amp[2], mid), c));
                rz = add(rz, mul(mul(amp[2], f9), s));
            }
            if (0 < f10 && kWaveMin < amp[3]) {
                const float half = mul(add(f10, f9), 0.5f);
                const float mid = mul(add(f8, f9), 0.5f);
                lookup(add(phase[3], mul(half, mul(p.frequency[3], kTwoPi))));
                rx = add(rx, mul(mul(amp[3], f8), s));
                ry = add(ry, mul(mul(amp[3], mid), c));
                rz = add(rz, mul(mul(amp[3], sub(1, f9)), s));
            }
            out[v] = rx;
            out[v + 1] = ry;
            out[v + 2] = rz;
            ++vertex;
            cv = add(cv, 1);
        }
        rowValue = add(rowValue, 1);
    }
}

// 0x34C668 wind part
inline void windTick(Wind& state, int mode, float fps, const std::function<uint32_t()>& random) {
    using namespace ee;
    const float amplitude = mode == 2 ? fromBits(0x3e99999a) : mode == 3 ? fromBits(0x3ee66666) : fromBits(0x3e19999a);
    const float timer = add(state.timer, div(1, fps));
    state.timer = timer;
    if (1 <= timer) {
        state.timer = sub(timer, 1);
        state.base = add(state.base, state.delta);
        const float low = -amplitude;
        const float r = unitRandom(random());
        state.delta = add(low, mul(sub(amplitude, low), r));
        if (add(state.base, state.delta) < 0) {
            state.delta = -state.base;
        }
        if (1 < add(state.base, state.delta)) {
            state.delta = sub(1, state.base);
        }
    }
    state.wind = add(state.base, mul(state.delta, state.timer));
}

// 0x34B818: true when the grid changed
inline bool clothTick(Cloth& cloth, float wind, int frame, const std::vector<float>& sine) {
    using namespace ee;
    const Parameters& p = cloth.parameters;
    const float strength = add(p.minimumStrength, mul(wind, sub(1, p.minimumStrength)));
    for (size_t i = 0; i < 4; ++i) {
        float x = add(cloth.phase[i], mul(p.speed[i], strength));
        if (1 <= x) {
            x = sub(x, 1);
        }
        cloth.phase[i] = x;
    }
    bool updated = false;
    if (frame % 2 == cloth.parity) {
        vertices(cloth, wind, sine, cloth.vertices);
        updated = true;
    }
    if (kUvMin < p.uvSpeed[0] || kUvMin < p.uvSpeed[1]) {
        for (size_t k = 0; k < 2; ++k) {
            float x = add(cloth.uvOffset[k], p.uvSpeed[k]);
            if (1 < x) {
                x = sub(x, 1);
            }
            cloth.uvOffset[k] = x;
        }
    }
    return updated;
}

// 0x34B228: the grid build (phases from four random words; rest grid, UV, colour bilinear over the group's corners)
inline void build(Cloth& cloth, const nlohmann::json& corners, const std::function<uint32_t()>& random, float wind, const std::vector<float>& sine) {
    using namespace ee;
    for (size_t i = 0; i < 4; ++i) {
        cloth.phase[i] = unitRandom(random());
    }
    cloth.uvOffset = {0, 0};
    const Parameters& p = cloth.parameters;
    cloth.width = 8;
    cloth.height = p.amplitude[2] < kWaveMin && p.amplitude[3] < kWaveMin ? 2 : 5;
    cloth.widthSpan = float(cloth.width - 1);
    cloth.heightSpan = float(cloth.height - 1);
    const size_t n = size_t(cloth.width * cloth.height);
    cloth.base.assign(n * 3, 0.f);
    cloth.uv.assign(n * 2, 0.f);
    cloth.colour.assign(n * 4, 0.f);
    auto bilinear = [&](const char* field, size_t a, float iu, float u, float iv, float v) {
        const auto& k = corners[field];
        const float top = add(mul(k[0][a].get<float>(), iu), mul(k[1][a].get<float>(), u));
        const float bottom = add(mul(k[2][a].get<float>(), iu), mul(k[3][a].get<float>(), u));
        return add(mul(top, iv), mul(bottom, v));
    };
    size_t vertex = 0;
    for (int r = 0; r < cloth.height; ++r) {
        for (int col = 0; col < cloth.width; ++col, ++vertex) {
            const float u = div(float(col), cloth.widthSpan);
            const float v = div(float(r), cloth.heightSpan);
            const float iu = sub(1, u);
            const float iv = sub(1, v);
            for (size_t a = 0; a < 3; ++a) {
                cloth.base[vertex * 3 + a] = bilinear("position", a, iu, u, iv, v);
            }
            for (size_t a = 0; a < 2; ++a) {
                cloth.uv[vertex * 2 + a] = bilinear("uv", a, iu, u, iv, v);
            }
            cloth.colour[vertex * 4] = 1;
            for (size_t a = 1; a < 4; ++a) {
                cloth.colour[vertex * 4 + a] = bilinear("colour", a, iu, u, iv, v);
            }
        }
    }
    cloth.mesh = true;
    vertices(cloth, wind, sine, cloth.vertices);
}

// the h - 1 strips as a triangle list
inline std::vector<uint32_t> triangleIndices(int width, int height) {
    std::vector<uint32_t> out;
    for (int s = 0; s + 1 < height; ++s) {
        for (int c = 0; c + 1 < width; ++c) {
            const uint32_t a = uint32_t(s * width + c);
            const uint32_t b = uint32_t((s + 1) * width + c);
            out.insert(out.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    }
    return out;
}

class Animation {
public:
    explicit Animation(const nlohmann::json& data) : data_(data) {
        mode = data["wind"]["mode"].get<int>();
        fps = data.value("fps", 60.0f);
        sine = data["sine_table"].get<std::vector<float>>();
        const auto& i = data["wind"]["initial"];
        wind = {i["wind"].get<float>(), i["base"].get<float>(), i["delta"].get<float>(), i["timer"].get<float>()};
        for (int k = 0; k < 15; ++k) {
            slots[size_t(k)].parity = k & 1;
        }
        for (const auto& x : data["instances"]) {
            Instance inst;
            inst.resource = x["resource"].get<uint32_t>();
            inst.cloth = x["cloth"].get<int>();
            for (size_t k = 0; k < 16; ++k) {
                inst.matrix[k] = x["matrix"][k].get<double>();
            }
            order.push_back(inst.resource);
            instances[inst.resource] = inst;
        }
    }
    struct Instance {
        uint32_t resource = 0;
        int cloth = 0;
        int slot = -1;
        std::array<double, 16> matrix{};
    };
    int mode = 1;
    float fps = 60;
    std::vector<float> sine;
    Wind wind;
    std::array<Cloth, 15> slots{};
    std::map<uint32_t, Instance> instances;
    std::vector<uint32_t> order;
    std::function<uint32_t()> random = [] { return 0u; };

    // entity construction (the instance's section activates)
    int activate(uint32_t resource) {
        auto it = instances.find(resource);
        if (it == instances.end()) {
            return -1;
        }
        Instance& inst = it->second;
        if (inst.slot >= 0) {
            return inst.slot;
        }
        const auto& cloth = data_["cloths"][size_t(inst.cloth)];
        for (int k = 0; k < 15; ++k) {
            Cloth& s = slots[size_t(k)];
            if (s.count && s.cloth == inst.cloth) {
                s.instances.push_back(resource);
                s.count++;
                inst.slot = k;
                return k;
            }
        }
        for (int k = 0; k < 15; ++k) {
            Cloth& s = slots[size_t(k)];
            if (s.count) {
                continue;
            }
            // the first group listing the resource, else the first group
            const nlohmann::json* group = nullptr;
            for (const auto& g : cloth["groups"]) {
                for (const auto& r : g["resources"]) {
                    if (!group && r.get<uint32_t>() == resource) {
                        group = &g;
                    }
                }
            }
            if (!group) {
                group = &cloth["groups"][0];
            }
            s.cloth = inst.cloth;
            s.parameters = parameters(cloth["parameters"]);
            s.phase = {0, 0, 0, 0};
            build(s, (*group)["corners"], random, wind.wind, sine);
            s.instances = {resource};
            s.count = 1;
            inst.slot = k;
            return k;
        }
        // all 15 slots busy: the original leaves the flag undrawn
        return -1;
    }
    void activateAll() {
        for (uint32_t r : order) {
            activate(r);
        }
    }
    bool deactivate(uint32_t resource) {
        auto it = instances.find(resource);
        if (it == instances.end() || it->second.slot < 0) {
            return false;
        }
        Cloth& s = slots[size_t(it->second.slot)];
        auto k = std::find(s.instances.begin(), s.instances.end(), resource);
        *k = s.instances.back();
        s.instances.pop_back();
        s.count--;
        it->second.slot = -1;
        if (s.count == 0) {
            s.mesh = false;
            s.base.clear();
            s.vertices.clear();
            s.uv.clear();
            s.colour.clear();
        }
        return true;
    }
    // one game tick: the slots whose grid changed
    std::vector<int> tick(int frame) {
        windTick(wind, mode, fps, random);
        std::vector<int> changed;
        for (int k = 0; k < 15; ++k) {
            Cloth& s = slots[size_t(k)];
            if (s.count && s.mesh && clothTick(s, wind.wind, frame, sine)) {
                changed.push_back(k);
            }
        }
        return changed;
    }
    const Cloth* clothOf(uint32_t resource) const {
        auto it = instances.find(resource);
        return it != instances.end() && it->second.slot >= 0 ? &slots[size_t(it->second.slot)] : nullptr;
    }
    // native-space positions (m, not origin-relative) of one instance: world = local x M (row vectors), native = (x, z, -y) / 100
    bool writeWorld(uint32_t resource, std::vector<float>& out) const {
        const Cloth* s = clothOf(resource);
        if (!s) {
            return false;
        }
        const auto& m = instances.at(resource).matrix;
        const size_t n = size_t(s->width * s->height);
        out.resize(n * 3);
        for (size_t i = 0; i < n; ++i) {
            const double x = s->vertices[i * 3];
            const double y = s->vertices[i * 3 + 1];
            const double z = s->vertices[i * 3 + 2];
            const double wx = x * m[0] + y * m[4] + z * m[8] + m[12];
            const double wy = x * m[1] + y * m[5] + z * m[9] + m[13];
            const double wz = x * m[2] + y * m[6] + z * m[10] + m[14];
            out[i * 3] = float(wx / 100);
            out[i * 3 + 1] = float(wz / 100);
            out[i * 3 + 2] = float(-wy / 100);
        }
        return true;
    }

private:
    static Parameters parameters(const nlohmann::json& p) {
        Parameters o;
        o.attachStart = p["attachStart"].get<int>();
        o.attachEnd = p["attachEnd"].get<int>();
        o.pinFirstRow = p["pinFirstRow"].get<int>();
        for (size_t k = 0; k < 4; ++k) {
            o.speed[k] = p["speed"][k].get<float>();
            o.amplitude[k] = p["amplitude"][k].get<float>();
            o.frequency[k] = p["frequency"][k].get<float>();
        }
        for (size_t k = 0; k < 3; ++k) {
            o.gust[k] = p["gust"][k].get<float>();
            o.calm[k] = p["calm"][k].get<float>();
        }
        o.minimumStrength = p["minimumStrength"].get<float>();
        o.uvSpeed = {p["uvSpeed"][0].get<float>(), p["uvSpeed"][1].get<float>()};
        return o;
    }
    nlohmann::json data_;
};

}  // namespace flags

}  // namespace ssx_native
