#pragma once
// LiveComp animation players (web/livecomp-animation.js natively; engine/livecomp_animation.hpp in the EE arithmetic of
// ee_float.hpp): the channel curves (0x351A80 / 0x351538 / 0x351800), the node compose (0x34DD18), the constructor / tick /
// crossing (0x341AA0 / 0x341D48 / 0x34EBE0), the node matrices (0x361098) and the native-space delta rest -> animated each mesh of
// a node draws with. Data: LIVECOMP/livecomp.json.
#include "ee_float.hpp"
#include "json.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ssx_native {

namespace livecomp {

using Rows = std::array<std::array<float, 4>, 4>;
using DRows = std::array<std::array<double, 4>, 4>;

inline const float kInvHalfPi = ee::fromBits(0x3f22f983);
inline const float kHalfPi = ee::fromBits(0x3fc90fdb);
inline const float kS7 = ee::fromBits(0x3638ef1f);
inline const float kS5 = ee::fromBits(0xb9500d03);
inline const float kS3 = ee::fromBits(0x3c088889);
inline const float kS1 = ee::fromBits(0xbe2aaaab);
inline const float kDeg = ee::fromBits(0x3c8efa36);
inline const float kThird = ee::fromBits(0x3d088889);
inline const float kBig = ee::fromBits(0x501502f9);

// 0x31BE50 sincos
inline void sincos(float x, float& sinOut, float& cosOut) {
    using namespace ee;
    float scaled = mul(x, kInvHalfPi);
    scaled = x < 0 ? sub(scaled, 0.5f) : add(scaled, 0.5f);
    const int quadrant = int(std::trunc(scaled));
    x = sub(x, mul(float(quadrant), kHalfPi));
    const float square = mul(x, x);
    float v = mul(square, kS7);
    v = add(v, kS5);
    v = mul(v, square);
    v = add(v, kS3);
    v = mul(v, square);
    v = add(v, kS1);
    v = mul(v, square);
    v = add(v, 1);
    const float s = mul(v, x);
    const float c = sqrtNearest(sub(1, mul(s, s)));
    switch (quadrant & 3) {
        case 0:
            sinOut = s;
            cosOut = c;
            return;
        case 1:
            sinOut = c;
            cosOut = -s;
            return;
        case 2:
            sinOut = -s;
            cosOut = -c;
            return;
        default:
            sinOut = -c;
            cosOut = s;
    }
}

// VU row-vector transform: row (c0 .. c3) x m
inline std::array<float, 4> row(const Rows& m, float c0, float c1, float c2, float c3) {
    using namespace ee;
    std::array<float, 4> o{};
    for (size_t k = 0; k < 4; ++k) {
        o[k] = vuAdd(vuAdd(vuAdd(mul(m[0][k], c0), mul(m[1][k], c1)), mul(m[2][k], c2)), mul(m[3][k], c3));
    }
    return o;
}

struct Segment {
    std::array<float, 6> v{};
};
struct Track {
    std::vector<float> base;
    uint32_t mask = 0;
    std::vector<std::vector<Segment>> curves;
};
struct Node {
    int parent = -1;
    Rows bind{};
    std::optional<Track> track;
};
struct Start {
    std::string trigger;
    uint32_t ownerResource = 0;
    bool guard = false;
    std::string guardInstance;
    float guardFrames30 = 0;
    std::array<uint32_t, 11> words{};
};
struct Instance {
    uint32_t resource = 0;
    std::string name;
    float length = 0;
    std::vector<Node> nodes;
    Rows matrix{};
    float scale = 1;
    std::string draw;
    std::vector<Start> starts;
    std::vector<Rows> rest;
    // SI x rest^-1 per node (double, render use)
    std::vector<DRows> restInverse;
};
struct Channel {
    const Track* track = nullptr;
    std::array<int32_t, 16> segment{};
    std::array<float, 16> value{};
    Rows matrix{};
};
struct State {
    const Instance* inst = nullptr;
    std::vector<Channel> channels;
    int mode = 0;
    int enabled = 1;
    int done = 0;
    int delay = 0;
    float previous = kBig;
    float unclamped = kBig;
    float low = 0, high = 0, time = 0, rate = 0, sampleTime = 0;
    bool dirty = true;
    bool advanced = false;
    std::vector<Rows> matrices;
    uint64_t matrixVersion = 0;
    std::vector<std::array<double, 16>> deltas;
    uint64_t deltaVersion = ~uint64_t(0);
};

// 0x351A80 segment search (cache: the channel's per-curve segment)
inline int findSegment(const std::vector<Segment>& curve, std::array<int32_t, 16>& cache, int n, float t) {
    const int next = cache[size_t(n)] + 1;
    cache[size_t(n)] = next;
    if (next < int(curve.size())) {
        const auto& s = curve[size_t(next)].v;
        if (s[4] <= t && t < s[5]) {
            return next;
        }
    }
    cache[size_t(n)] = 0;
    int k = 0;
    if (int(curve.size()) - 1 <= 0) {
        return 0;
    }
    for (;;) {
        if (t < curve[size_t(k)].v[5]) {
            return k;
        }
        cache[size_t(n)] += 1;
        ++k;
        if (!(cache[size_t(n)] < int(curve.size()) - 1)) {
            return k;
        }
    }
}

// 0x351538 / 0x351800: the channel values at t and its matrix
inline void evaluateChannel(Channel& ch, float t) {
    using namespace ee;
    const Track& tr = *ch.track;
    int active = 0;
    for (int n = 0; n < 16; ++n) {
        if (!((tr.mask >> n) & 1)) {
            continue;
        }
        const auto& curve = tr.curves[size_t(active)];
        const int slot = active++;
        int k = ch.segment[size_t(slot)];
        const bool inside = k < int(curve.size()) && curve[size_t(k)].v[4] <= t && t < curve[size_t(k)].v[5];
        if (!inside) {
            k = findSegment(curve, ch.segment, slot, t);
        }
        const auto& s = curve[size_t(k)].v;
        float v = add(mul(s[0], t), s[1]);
        v = mul(v, t);
        v = add(v, s[2]);
        v = mul(v, t);
        ch.value[size_t(n)] = add(v, s[3]);
    }
    float sx, cx, sy, cy, sz, cz;
    sincos(mul(-ch.value[3], kDeg), sx, cx);
    sincos(mul(-ch.value[4], kDeg), sy, cy);
    sincos(mul(-ch.value[5], kDeg), sz, cz);
    const float sxsy = mul(sx, sy), cxsy = mul(cx, sy), cxsz = mul(cx, sz), sxcz = mul(sx, cz), sxsysz = mul(sxsy, sz), cxcz = mul(cx, cz);
    const float cxszsy = mul(cxsz, sy), sxsz = mul(sx, sz), cxsycz = mul(cxsy, cz), sxsycz = mul(sxsy, cz), cxcy = mul(cx, cy), msxcy = mul(-sx, cy);
    auto& m = ch.matrix;
    m[0] = {mul(cy, cz), mul(-cy, sz), sy, 0};
    m[1] = {add(sxsycz, cxsz), sub(cxcz, sxsysz), msxcy, 0};
    m[2] = {sub(sxsz, cxsycz), add(cxszsy, sxcz), cxcy, 0};
    m[3] = {ch.value[0], ch.value[1], ch.value[2], 1};
}

// 0x34DD18: node world = scaledLocal(channel or bind) x parent world (the instance for roots)
inline void compose(const Instance& inst, const std::vector<Channel>* channels, std::vector<Rows>& out) {
    out.resize(inst.nodes.size());
    size_t c = 0;
    for (size_t i = 0; i < inst.nodes.size(); ++i) {
        const Node& n = inst.nodes[i];
        const Rows* source = &n.bind;
        if (n.track) {
            if (channels) {
                source = &(*channels)[c].matrix;
            }
            ++c;
        }
        const Rows& parent = n.parent >= 0 ? out[size_t(n.parent)] : inst.matrix;
        const Rows& s = *source;
        Rows o;
        o[0] = row(parent, s[0][0], s[0][1], s[0][2], s[0][3]);
        o[1] = row(parent, s[1][0], s[1][1], s[1][2], s[1][3]);
        o[2] = row(parent, s[2][0], s[2][1], s[2][2], s[2][3]);
        o[3] = row(parent, ee::mul(s[3][0], inst.scale), ee::mul(s[3][1], inst.scale), ee::mul(s[3][2], inst.scale), ee::mul(s[3][3], 1));
        out[i] = o;
    }
}

// 0x341AA0 constructor (random: 0x317810 words)
inline State construct(const Instance& inst, const std::array<uint32_t, 11>& words, const std::function<uint32_t()>& random, float fps) {
    using namespace ee;
    State s;
    s.inst = &inst;
    for (const auto& n : inst.nodes) {
        if (!n.track) {
            continue;
        }
        Channel ch;
        ch.track = &*n.track;
        for (size_t k = 0; k < n.track->base.size() && k < 16; ++k) {
            ch.value[k] = n.track->base[k];
        }
        s.channels.push_back(ch);
    }
    float start = 0;
    if (!s.channels.empty() && !s.channels[0].track->curves.empty() && !s.channels[0].track->curves[0].empty()) {
        start = s.channels[0].track->curves[0][0].v[4];
    }
    for (auto& ch : s.channels) {
        evaluateChannel(ch, start);
    }
    auto r = [&](int k) { return fromBits(words[size_t(k)]); };
    auto unit = [&](float lo, float hi) { return add(lo, mul(sub(hi, lo), sub(fromBits((random() & 0x7fffffu) | 0x3f800000u), 1))); };
    s.mode = int(int16_t(uint16_t(words[1] & 0xffff)));
    s.low = 0 <= r(3) ? mul(r(3), kThird) : start;
    s.high = r(4) < 0 ? inst.length : mul(r(4), kThird);
    float t = r(7) < 0 ? s.low : mul(r(7), kThird);
    if (t < s.low) {
        t = s.low;
    }
    if (s.high < t) {
        t = s.high;
    }
    if (words[8] != 0) {
        t = unit(s.low, s.high);
    }
    s.time = t;
    if (r(6) == 0) {
        s.rate = div(mul(r(5), kThird), fps);
    } else {
        s.rate = div(add(mul(r(5), kThird), mul(mul(r(6), kThird), unit(-1, 1))), fps);
    }
    if (words[2] == 1) {
        s.rate = -s.rate;
    }
    s.sampleTime = s.time;
    return s;
}

enum class TickResult { Finished, Updated, Done };

// 0x341D48
inline TickResult tick(State& s) {
    using namespace ee;
    s.unclamped = kBig;
    s.previous = kBig;
    if (s.done) {
        return TickResult::Finished;
    }
    if (s.enabled && s.delay >= 0) {
        if (s.delay > 0) {
            s.delay -= 1;
        } else {
            s.previous = s.time;
            float t = s.time;
            if (s.mode == 1) {
                if (0 <= s.rate) {
                    t = add(t, s.rate);
                    s.unclamped = t;
                    if (s.high < t) {
                        t = add(s.low, sub(t, s.high));
                    }
                } else {
                    t = add(t, s.rate);
                    s.unclamped = t;
                    if (t < s.low) {
                        t = sub(s.high, sub(s.low, t));
                    }
                }
            } else if (s.mode == 2) {
                if (0 <= s.rate) {
                    t = add(t, s.rate);
                    s.unclamped = t;
                    if (s.high < t) {
                        const float o = sub(t, s.high);
                        s.rate = -s.rate;
                        t = sub(s.high, o);
                    }
                } else {
                    t = add(t, s.rate);
                    s.unclamped = t;
                    if (t < s.low) {
                        const float u = sub(s.low, t);
                        s.rate = -s.rate;
                        t = add(s.low, u);
                    }
                }
            } else if (0 <= s.rate) {
                if (t == s.high) {
                    s.done = 1;
                }
                t = add(t, s.rate);
                s.unclamped = t;
                if (s.high < t) {
                    t = s.high;
                }
            } else {
                if (t == s.low) {
                    s.done = 1;
                }
                t = add(t, s.rate);
                s.unclamped = t;
                if (t < s.low) {
                    t = s.low;
                }
            }
            s.time = t;
            s.advanced = true;
        }
    }
    s.dirty = true;
    s.sampleTime = s.time;
    return s.done ? TickResult::Done : TickResult::Updated;
}

// 0x34EBE0: did this tick's advance cross x (seconds)?
inline bool crossed(const State& s, float x) {
    const float p = s.previous;
    const float n = s.unclamped;
    if (p == kBig) {
        return false;
    }
    return p < n ? (p <= x && x < n) : (n <= x && x < p);
}

// 0x361098: node world matrices (source rows)
inline const std::vector<Rows>& matrices(State& s) {
    if (s.dirty) {
        for (auto& ch : s.channels) {
            evaluateChannel(ch, s.sampleTime);
        }
        compose(*s.inst, &s.channels, s.matrices);
        s.matrixVersion += 1;
        s.dirty = false;
    }
    return s.matrices;
}

// rows; 3 x 3 inverse + translation (double precision, render use)
inline DRows invertAffine(const Rows& m) {
    const auto& a = m[0];
    const auto& b = m[1];
    const auto& c = m[2];
    const auto& t = m[3];
    const double det = double(a[0]) * (double(b[1]) * c[2] - double(b[2]) * c[1]) - double(a[1]) * (double(b[0]) * c[2] - double(b[2]) * c[0]) +
                       double(a[2]) * (double(b[0]) * c[1] - double(b[1]) * c[0]);
    DRows i{};
    i[0] = {(double(b[1]) * c[2] - double(b[2]) * c[1]) / det, (double(a[2]) * c[1] - double(a[1]) * c[2]) / det,
            (double(a[1]) * b[2] - double(a[2]) * b[1]) / det, 0};
    i[1] = {(double(b[2]) * c[0] - double(b[0]) * c[2]) / det, (double(a[0]) * c[2] - double(a[2]) * c[0]) / det,
            (double(a[2]) * b[0] - double(a[0]) * b[2]) / det, 0};
    i[2] = {(double(b[0]) * c[1] - double(b[1]) * c[0]) / det, (double(a[1]) * c[0] - double(a[0]) * c[1]) / det,
            (double(a[0]) * b[1] - double(a[1]) * b[0]) / det, 0};
    for (size_t k = 0; k < 3; ++k) {
        i[3][k] = -(double(t[0]) * i[0][k] + double(t[1]) * i[1][k] + double(t[2]) * i[2][k]);
    }
    i[3][3] = 1;
    return i;
}

template <typename A, typename B>
inline DRows mulRows(const A& a, const B& b) {
    DRows o{};
    for (size_t r = 0; r < 4; ++r) {
        for (size_t k = 0; k < 4; ++k) {
            o[r][k] = double(a[r][0]) * double(b[0][k]) + double(a[r][1]) * double(b[1][k]) + double(a[r][2]) * double(b[2][k]) + double(a[r][3]) * double(b[3][k]);
        }
    }
    return o;
}

// native = (x, z, -y) / 100 (row vectors)
inline const DRows kS = {{{0.01, 0, 0, 0}, {0, 0, -0.01, 0}, {0, 0.01, 0, 0}, {0, 0, 0, 1}}};
inline const DRows kSI = {{{100, 0, 0, 0}, {0, 0, 100, 0}, {0, -100, 0, 0}, {0, 0, 0, 1}}};

inline Rows rowsOf(const nlohmann::json& flat) {
    Rows m{};
    for (size_t r = 0; r < 4; ++r) {
        for (size_t k = 0; k < 4; ++k) {
            m[r][k] = flat[r * 4 + k].get<float>();
        }
    }
    return m;
}

inline uint32_t number(const nlohmann::json& v) {
    return v.is_string() ? uint32_t(std::stoul(v.get<std::string>())) : v.get<uint32_t>();
}

class Animation {
public:
    explicit Animation(const nlohmann::json& data) {
        fps = data.value("fps", 60.0f);
        for (const auto& x : data["instances"]) {
            Instance inst;
            inst.resource = number(x["resource"]);
            inst.name = x["name"].get<std::string>();
            inst.length = x["length"].is_string() ? std::stof(x["length"].get<std::string>()) : x["length"].get<float>();
            inst.matrix = rowsOf(x["matrix"]);
            inst.scale = x["scale"].is_string() ? std::stof(x["scale"].get<std::string>()) : x["scale"].get<float>();
            inst.draw = x.value("draw", std::string());
            for (const auto& n : x["nodes"]) {
                Node node;
                node.parent = n["parent"].get<int>();
                node.bind = rowsOf(n["bind"]);
                if (n.contains("track") && n["track"].is_object()) {
                    Track t;
                    t.base = n["track"]["base"].get<std::vector<float>>();
                    t.mask = n["track"]["mask"].get<uint32_t>();
                    for (const auto& curve : n["track"]["curves"]) {
                        std::vector<Segment> segs;
                        for (const auto& s : curve) {
                            Segment seg;
                            for (size_t k = 0; k < 6; ++k) {
                                seg.v[k] = s[k].get<float>();
                            }
                            segs.push_back(seg);
                        }
                        t.curves.push_back(std::move(segs));
                    }
                    node.track = std::move(t);
                }
                inst.nodes.push_back(std::move(node));
            }
            for (const auto& st : x["starts"]) {
                Start s;
                s.trigger = st["trigger"].get<std::string>();
                s.ownerResource = st.contains("ownerResource") && !st["ownerResource"].is_null() ? number(st["ownerResource"]) : 0;
                if (st.contains("guard") && st["guard"].is_object()) {
                    s.guard = true;
                    s.guardInstance = st["guard"]["instance"].get<std::string>();
                    s.guardFrames30 = st["guard"]["frames30"].get<float>();
                }
                for (size_t k = 0; k < 11; ++k) {
                    s.words[k] = st["words"][k].get<uint32_t>();
                }
                inst.starts.push_back(s);
            }
            instances.push_back(std::move(inst));
        }
        for (auto& x : instances) {
            compose(x, nullptr, x.rest);
            for (const auto& r : x.rest) {
                x.restInverse.push_back(mulRows(kSI, invertAffine(r)));
            }
            byResource[x.resource] = &x;
        }
    }
    Animation(const Animation&) = delete;

    float fps = 60;
    std::vector<Instance> instances;
    std::map<uint32_t, Instance*> byResource;
    // the 0x317810 words for random starts / rates (the page's stand-in when no word is given)
    std::function<uint32_t()> random = [] { return 0u; };
    // false: the core's stage VM runs the slot-5 programs and its start log drives start()
    bool chainStarts = true;

    State* state(uint32_t resource) {
        for (auto& [r, s] : active) {
            if (r == resource) {
                return &s;
            }
        }
        return nullptr;
    }
    // a JS Map's set: an existing key keeps its place
    State& set(uint32_t resource, State s) {
        if (State* e = state(resource)) {
            *e = std::move(s);
            return *e;
        }
        active.emplace_back(resource, std::move(s));
        return active.back().second;
    }
    void erase(uint32_t resource) {
        for (auto it = active.begin(); it != active.end(); ++it) {
            if (it->first == resource) {
                active.erase(it);
                return;
            }
        }
    }
    State& start(const Instance& inst, const std::array<uint32_t, 11>& words) {
        State s = construct(inst, words, random, fps);
        s.advanced = false;
        return set(inst.resource, std::move(s));
    }
    void fire(const std::string& trigger, uint32_t key = 0) {
        for (const auto& inst : instances) {
            for (const auto& st : inst.starts) {
                if (st.trigger != trigger || st.guard) {
                    continue;
                }
                if (trigger == "section" && inst.resource != key) {
                    continue;
                }
                if (trigger == "contact" && st.ownerResource != key) {
                    continue;
                }
                start(inst, st.words);
            }
        }
    }
    // the entity pass (insertion order); chained starts after it
    void tickAll() {
        std::vector<std::pair<const Instance*, std::array<uint32_t, 11>>> created;
        for (auto& [res, s] : active) {
            livecomp::tick(s);
            if (s.previous == kBig || !chainStarts) {
                continue;
            }
            for (const auto& inst : instances) {
                for (const auto& st : inst.starts) {
                    if (st.trigger == "tick" && st.guard && st.guardInstance == s.inst->name && crossed(s, ee::mul(st.guardFrames30, kThird))) {
                        created.push_back({&inst, st.words});
                    }
                }
            }
        }
        for (const auto& [inst, words] : created) {
            start(*inst, words);
        }
    }
    const std::vector<Rows>* nodeMatrices(uint32_t resource) {
        State* s = state(resource);
        return s ? &matrices(*s) : nullptr;
    }
    // per node: the native-space delta rest -> animated (three.js column-major)
    const std::vector<std::array<double, 16>>* nodeDeltas(uint32_t resource) {
        State* s = state(resource);
        if (!s) {
            return nullptr;
        }
        const auto& w = matrices(*s);
        if (s->deltaVersion == s->matrixVersion && s->deltas.size() == w.size()) {
            return &s->deltas;
        }
        s->deltas.assign(w.size(), {});
        for (size_t i = 0; i < w.size(); ++i) {
            const DRows t = mulRows(s->inst->restInverse[i], w[i]);
            const DRows d = mulRows(t, kS);
            for (size_t r = 0; r < 4; ++r) {
                for (size_t k = 0; k < 4; ++k) {
                    s->deltas[i][r * 4 + k] = d[r][k];
                }
            }
        }
        s->deltaVersion = s->matrixVersion;
        return &s->deltas;
    }

    std::vector<std::pair<uint32_t, State>> active;
};

}  // namespace livecomp

}  // namespace ssx_native
