#pragma once
// Arithmetic trace for the matcher (docs/ps2-float.md "Matcher"): a trace build of the core records every helper call
// (its call site, the op, the operands and the result) while enabled. Only make_swap_tree.py --trace builds use it.
#include <bit>
#include <cstdint>
#include <source_location>
#include <string>
#include <unordered_map>
#include <vector>

namespace ssx::ps2trace {

enum Op : uint32_t {
    Add = 1,
    Sub = 2,
    Mul = 3,
    Div = 4,
    Sqrt = 5,
    EeAdd = 6,
    EeSub = 7,
    EeDiv = 8,
    EeSqrt = 9,
    CollisionDiv = 10,
    CollisionSqrt = 11,
};

struct Entry {
    uint32_t site;
    uint32_t op;
    uint32_t a;
    uint32_t b;
    uint32_t result;
};

inline bool enabled = false;
inline std::vector<Entry> entries;
inline std::vector<std::string> sites;
inline std::unordered_map<std::string, uint32_t> siteIds;

inline uint32_t site(const std::source_location& location) {
    std::string key = std::string(location.file_name()) + ":" + std::to_string(location.line()) + ":" + std::to_string(location.column());
    auto found = siteIds.find(key);
    if (found != siteIds.end()) {
        return found->second;
    }
    const uint32_t id = uint32_t(sites.size());
    sites.push_back(key);
    siteIds.emplace(key, id);
    return id;
}

inline float record(Op op, float a, float b, float result, const std::source_location& location) {
    if (enabled) {
        entries.push_back({site(location), op, std::bit_cast<uint32_t>(a), std::bit_cast<uint32_t>(b), std::bit_cast<uint32_t>(result)});
    }
    return result;
}

// A helper as a function object: `auto add = originalScalarAdd;` still works, and the default argument records the
// site of each call (a defaulted parameter on a function would break the places that take its address).
struct TracedBinary {
    float (*impl)(float, float);
    Op op;

    float operator()(float a, float b, std::source_location location = std::source_location::current()) const {
        return record(op, a, b, impl(a, b), location);
    }
};

struct TracedUnary {
    float (*impl)(float);
    Op op;

    float operator()(float a, std::source_location location = std::source_location::current()) const {
        return record(op, a, 0.f, impl(a), location);
    }
};

}
