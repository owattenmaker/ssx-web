// A logged call stream replayed on the native core (docs/core-migration.md "Native core"): the exact export calls a wasm harness
// made (local/core-migration/qa/native-gate/spy-core.js: web/compare-ps2-capture.mjs on a ps2-captures gate), with the bytes of the
// buffers it passed and the RNG words it wrote between calls, made again in order on the native build. At each probe (before a
// tick's pad call) the human's state, pose, shared and visual RNG are hashed as the wasm run hashed them; the first difference is
// reported. replay_calls CALLS.jsonl
#include "json.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using nlohmann::json;
extern "C" {
float* rider_state();
float* rider_world_state();
float* pose_physical();
uint32_t* animation_rng_words();
uint32_t* visual_rng_words();
uint32_t* visual_lcg_word();
}

namespace {
uint32_t jsToUint32(double x) {
    if (!std::isfinite(x)) {
        return 0;
    }
    double m = std::fmod(std::trunc(x), 4294967296.0);
    if (m < 0) {
        m += 4294967296.0;
    }
    return uint32_t(m);
}
double jsNumber(const json& v) {
    if (v.is_number()) {
        return v.get<double>();
    }
    if (v.is_string()) {
        const std::string s = v.get<std::string>();
        if (s == "Infinity") {
            return INFINITY;
        }
        if (s == "-Infinity") {
            return -INFINITY;
        }
        return NAN;
    }
    return NAN;
}
struct Block {
    std::vector<uint8_t> bytes;
};
std::map<double, Block> blocks;          // wasm malloc pointer -> the native buffer
std::map<double, const void*> returned;  // wasm pointer an export returned -> the native one
std::vector<uint8_t> decode(const std::string& b64) {
    static const std::string table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<uint8_t> out;
    uint32_t acc = 0;
    int bits = 0;
    for (char c : b64) {
        if (c == '=') {
            break;
        }
        const auto p = table.find(c);
        if (p == std::string::npos) {
            continue;
        }
        acc = (acc << 6) | uint32_t(p);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(uint8_t(acc >> bits));
        }
    }
    return out;
}
struct Args {
    const json& a;
    const json* b;
    double raw(unsigned k) const {
        return k < a.size() ? jsNumber(a[k]) : NAN;
    }
    int32_t i32(unsigned k) const {
        return int32_t(jsToUint32(raw(k)));
    }
    float f32(unsigned k) const {
        return float(raw(k));
    }
    double f64(unsigned k) const {
        return raw(k);
    }
    void* ptr(unsigned k) {
        const double v = raw(k);
        if (b && b->contains(std::to_string(k))) {
            auto it = blocks.find(v);
            if (it == blocks.end()) {
                throw std::runtime_error("a buffer argument outside the logged allocations");
            }
            const auto bytes = decode((*b)[std::to_string(k)].get<std::string>());
            std::memcpy(it->second.bytes.data(), bytes.data(), std::min(bytes.size(), it->second.bytes.size()));
            return it->second.bytes.data();
        }
        if (v == 0) {
            return nullptr;
        }
        auto r = returned.find(v);
        if (r != returned.end()) {
            return const_cast<void*>(r->second);
        }
        for (auto& [base, block] : blocks) {
            if (v >= base && v < base + double(block.bytes.size())) {
                return block.bytes.data() + size_t(v - base);
            }
        }
        throw std::runtime_error("an unknown pointer argument");
    }
};
struct Result {
    bool pointer = false;
    const void* p = nullptr;
    void set() {
    }
    void setPtr(const void* q) {
        pointer = true;
        p = q;
    }
    void setNumber(double) {
    }
};
#include "dispatch.inc"
uint32_t fnv(const void* p, unsigned n) {
    const auto* w = static_cast<const uint32_t*>(p);
    uint32_t h = 0x811c9dc5u;
    for (unsigned k = 0; k < n; ++k) {
        h = (h ^ w[k]) * 16777619u;
    }
    return h;
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            throw std::runtime_error("usage: replay_calls CALLS.jsonl");
        }
        std::ifstream in(argv[1]);
        std::string line;
        unsigned calls = 0, probes = 0, line_no = 0;
        while (std::getline(in, line)) {
            ++line_no;
            const json o = json::parse(line);
            if (o.contains("p")) {
                const uint32_t h[5] = {fnv(rider_state(), 16), fnv(rider_world_state(), 20), fnv(pose_physical(), 12), fnv(animation_rng_words(), 6),
                                       fnv(visual_rng_words(), 6)};
                for (unsigned k = 0; k < 5; ++k) {
                    if (h[k] != o["p"][k].get<uint32_t>()) {
                        std::printf("probe %u (line %u): part %u differs (native %u, wasm %u)\n", probes, line_no, k, h[k], o["p"][k].get<uint32_t>());
                        std::fflush(stdout);
                        std::_Exit(2);
                    }
                }
                ++probes;
                continue;
            }
            if (o.contains("w")) {
                const std::string w = o["w"].get<std::string>();
                uint32_t v[6] = {};
                for (unsigned k = 0; k < o["v"].size() && k < 6; ++k) {
                    v[k] = o["v"][k].get<uint32_t>();
                }
                uint32_t* target = w == "rng" ? animation_rng_words() : w == "visual" ? visual_rng_words() : visual_lcg_word();
                std::memcpy(target, v, (w == "lcg" ? 1 : 6) * 4);
                continue;
            }
            const std::string name = o["n"].get<std::string>();
            const json& a = o["a"];
            if (name == "_malloc") {
                blocks[o["r"].get<double>()].bytes.assign(size_t(jsNumber(a[0])), 0);
                continue;
            }
            if (name == "_free") {
                blocks.erase(jsNumber(a[0]));
                continue;
            }
            Args args{a, o.contains("b") ? &o["b"] : nullptr};
            Result r;
            if (!dispatch(name, args, r)) {
                throw std::runtime_error("no native case for " + name);
            }
            if (r.pointer && o["r"].is_number()) {
                returned[o["r"].get<double>()] = r.p;
            }
            ++calls;
        }
        std::printf("replayed %u calls, %u probes identical\n", calls, probes);
        std::fflush(stdout);
        std::_Exit(0);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "replay_calls: %s\n", e.what());
        std::fflush(stderr);
        std::_Exit(1);
    }
}
