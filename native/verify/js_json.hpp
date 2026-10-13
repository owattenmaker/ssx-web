#pragma once
// The page's JSON handling, natively (docs/native.md "N2"): the documents the page builds with JSON.parse / JSON.stringify and
// object spreads, made the same way, so the core parses the same values and the page's string comparisons of documents
// (web/ai-racers.js setDocument) come out the same.
// - Key order: a JS object keeps its keys in insertion order, except that integer-like keys ("0", "17") come first in
//   ascending order. The documents are ordered_json (insertion order); jsKeys / jsStringify apply the integer rule.
// - Numbers: nlohmann keeps an integer / float distinction that JS has not; jsStringify writes every number as
//   JSON.stringify would (an integral value as an integer, -0 as 0, NaN and the infinities as null).
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace ssx_native {

using json = nlohmann::ordered_json;

// A canonical array index ("0", "17"; not "01", not 2^32 - 1 or more): JS orders these keys first
inline bool jsIndexKey(const std::string& k, uint32_t& value) {
    if (k.empty() || k.size() > 10 || (k.size() > 1 && k[0] == '0')) {
        return false;
    }
    uint64_t v = 0;
    for (char c : k) {
        if (c < '0' || c > '9') {
            return false;
        }
        v = v * 10 + uint64_t(c - '0');
    }
    if (v >= 4294967295ull) {
        return false;
    }
    value = uint32_t(v);
    return true;
}

// Object.keys order
inline std::vector<std::string> jsKeys(const json& o) {
    std::vector<std::pair<uint32_t, std::string>> indexes;
    std::vector<std::string> rest;
    for (auto it = o.begin(); it != o.end(); ++it) {
        uint32_t v;
        if (jsIndexKey(it.key(), v)) {
            indexes.emplace_back(v, it.key());
        } else {
            rest.push_back(it.key());
        }
    }
    std::sort(indexes.begin(), indexes.end());
    std::vector<std::string> out;
    for (auto& [v, k] : indexes) {
        out.push_back(k);
    }
    out.insert(out.end(), rest.begin(), rest.end());
    return out;
}

// A JS truthy object that is not an array (`v && typeof v === 'object' && !Array.isArray(v)`)
inline bool jsIsObject(const json& v) {
    return v.is_object();
}

// The numbers as JSON.stringify writes them
inline json jsNormalized(const json& v) {
    if (v.is_object()) {
        json out = json::object();
        for (const auto& k : jsKeys(v)) {
            out[k] = jsNormalized(v.at(k));
        }
        return out;
    }
    if (v.is_array()) {
        json out = json::array();
        for (const auto& e : v) {
            out.push_back(jsNormalized(e));
        }
        return out;
    }
    if (v.is_number_float()) {
        const double x = v.get<double>();
        if (!std::isfinite(x)) {
            return nullptr;
        }
        if (x == 0) {
            return 0;
        }
        if (x == std::trunc(x) && std::abs(x) < 9007199254740992.0) {
            return int64_t(x);
        }
        return x;
    }
    return v;
}

inline std::string jsStringify(const json& v) {
    return jsNormalized(v).dump();
}

// JS ToInt32 of a JSON value used as a number (absent / null -> 0 as `x | 0` does for null; NaN -> 0)
inline int32_t jsInt(const json& v, int32_t fallback = 0) {
    if (v.is_number_integer()) {
        return int32_t(uint32_t(v.get<int64_t>()));
    }
    if (v.is_number_float()) {
        const double x = v.get<double>();
        if (!std::isfinite(x)) {
            return 0;
        }
        double m = std::fmod(std::trunc(x), 4294967296.0);
        if (m < 0) {
            m += 4294967296.0;
        }
        return int32_t(uint32_t(m));
    }
    if (v.is_boolean()) {
        return v.get<bool>() ? 1 : 0;
    }
    return fallback;
}

// A JS number of a JSON value (Number(v): absent NaN, null 0, a bool 1 / 0)
inline double jsNumber(const json* v) {
    if (!v) {
        return NAN;
    }
    if (v->is_number()) {
        return v->get<double>();
    }
    if (v->is_null()) {
        return 0;
    }
    if (v->is_boolean()) {
        return v->get<bool>() ? 1 : 0;
    }
    return NAN;
}

inline const json* jsField(const json& o, const char* key) {
    if (!o.is_object()) {
        return nullptr;
    }
    auto it = o.find(key);
    return it == o.end() ? nullptr : &*it;
}

// web/ai-racers.js mergeSettings: one level deep ({...initial[key], ...value} when initial[key] is a plain object)
inline json mergeSettingsShallow(const json& initial, const json& overrides) {
    json out = initial;
    if (!overrides.is_object()) {
        return out;
    }
    for (auto it = overrides.begin(); it != overrides.end(); ++it) {
        const json* base = jsField(initial, it.key().c_str());
        if (base && base->is_object()) {
            json merged = *base;
            const json& value = it.value();
            if (value.is_object()) {
                for (auto j = value.begin(); j != value.end(); ++j) {
                    merged[j.key()] = j.value();
                }
            } else if (value.is_array()) {
                // a spread array adds its indexes as keys
                for (size_t k = 0; k < value.size(); ++k) {
                    merged[std::to_string(k)] = value[k];
                }
            } else if (value.is_string()) {
                const std::string s = value.get<std::string>();
                for (size_t k = 0; k < s.size(); ++k) {
                    merged[std::to_string(k)] = std::string(1, s[k]);
                }
            }
            out[it.key()] = merged;
        } else {
            out[it.key()] = it.value();
        }
    }
    return out;
}

// web/character-roster.js mergeCharacterSettings: objects recurse, arrays and scalars replace
inline json mergeCharacterSettings(const json& base, const json& overrides) {
    if (!jsIsObject(overrides)) {
        return base;
    }
    json out = base.is_object() ? base : json::object();
    for (auto it = overrides.begin(); it != overrides.end(); ++it) {
        const json* b = jsField(base, it.key().c_str());
        if (jsIsObject(it.value()) && b && jsIsObject(*b)) {
            out[it.key()] = mergeCharacterSettings(*b, it.value());
        } else {
            out[it.key()] = it.value();
        }
    }
    return out;
}

// web/lineup.js put: record[path] = clone(value) on a dotted path
inline void putPath(json& record, const std::string& path, const json& value) {
    json* o = &record;
    size_t start = 0;
    for (;;) {
        const size_t dot = path.find('.', start);
        const std::string key = path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (dot == std::string::npos) {
            if (o->is_array()) {
                (*o)[std::stoul(key)] = value;
            } else {
                (*o)[key] = value;
            }
            return;
        }
        o = o->is_array() ? &(*o)[std::stoul(key)] : &(*o)[key];
        start = dot + 1;
    }
}

}  // namespace ssx_native
